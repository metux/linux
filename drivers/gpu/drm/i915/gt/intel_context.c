/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2019 Intel Corporation
 */

#include "gem/i915_gem_context.h"
#include "gem/i915_gem_pm.h"

#include "i915_drv.h"
#include "i915_globals.h"

#include "intel_context.h"
#include "intel_engine.h"
#include "intel_engine_pm.h"

static struct i915_global_context {
	struct i915_global base;
	struct kmem_cache *slab_ce;
} global;

struct intel_context *intel_context_alloc(void)
{
	return kmem_cache_zalloc(global.slab_ce, GFP_KERNEL);
}

void intel_context_free(struct intel_context *ce)
{
	kmem_cache_free(global.slab_ce, ce);
}

struct intel_context *
intel_context_lookup(struct i915_gem_context *ctx,
		     struct intel_engine_cs *engine)
{
	struct intel_context *ce = NULL;
	struct rb_node *p;

	spin_lock(&ctx->hw_contexts_lock);
	p = ctx->hw_contexts.rb_node;
	while (p) {
		struct intel_context *this =
			rb_entry(p, struct intel_context, node);

		if (this->engine == engine) {
			GEM_BUG_ON(this->gem_context != ctx);
			ce = this;
			break;
		}

		if (this->engine < engine)
			p = p->rb_right;
		else
			p = p->rb_left;
	}
	spin_unlock(&ctx->hw_contexts_lock);

	return ce;
}

struct intel_context *
__intel_context_insert(struct i915_gem_context *ctx,
		       struct intel_engine_cs *engine,
		       struct intel_context *ce)
{
	struct rb_node **p, *parent;
	int err = 0;

	spin_lock(&ctx->hw_contexts_lock);

	parent = NULL;
	p = &ctx->hw_contexts.rb_node;
	while (*p) {
		struct intel_context *this;

		parent = *p;
		this = rb_entry(parent, struct intel_context, node);

		if (this->engine == engine) {
			err = -EEXIST;
			ce = this;
			break;
		}

		if (this->engine < engine)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	if (!err) {
		rb_link_node(&ce->node, parent, p);
		rb_insert_color(&ce->node, &ctx->hw_contexts);
	}

	spin_unlock(&ctx->hw_contexts_lock);

	return ce;
}

void __intel_context_remove(struct intel_context *ce)
{
	struct i915_gem_context *ctx = ce->gem_context;

	spin_lock(&ctx->hw_contexts_lock);
	rb_erase(&ce->node, &ctx->hw_contexts);
	spin_unlock(&ctx->hw_contexts_lock);
}

static struct intel_context *
intel_context_instance(struct i915_gem_context *ctx,
		       struct intel_engine_cs *engine)
{
	struct intel_context *ce, *pos;

	ce = intel_context_lookup(ctx, engine);
	if (likely(ce))
		return ce;

	ce = intel_context_alloc();
	if (!ce)
		return ERR_PTR(-ENOMEM);

	intel_context_init(ce, ctx, engine);

	pos = __intel_context_insert(ctx, engine, ce);
	if (unlikely(pos != ce)) /* Beaten! Use their HW context instead */
		intel_context_free(ce);

	GEM_BUG_ON(intel_context_lookup(ctx, engine) != pos);
	return pos;
}

struct intel_context *
intel_context_pin_lock(struct i915_gem_context *ctx,
		       struct intel_engine_cs *engine)
	__acquires(ce->pin_mutex)
{
	struct intel_context *ce;

	ce = intel_context_instance(ctx, engine);
	if (IS_ERR(ce))
		return ce;

	if (mutex_lock_interruptible(&ce->pin_mutex))
		return ERR_PTR(-EINTR);

	return ce;
}

struct intel_context *
intel_context_pin(struct i915_gem_context *ctx,
		  struct intel_engine_cs *engine)
{
	struct intel_context *ce;
	int err;

	ce = intel_context_instance(ctx, engine);
	if (IS_ERR(ce))
		return ce;

	if (likely(atomic_inc_not_zero(&ce->pin_count)))
		return ce;

	if (mutex_lock_interruptible(&ce->pin_mutex))
		return ERR_PTR(-EINTR);

	if (likely(!atomic_read(&ce->pin_count))) {
		intel_wakeref_t wakeref;

		err = 0;
		with_intel_runtime_pm(ce->engine->i915, wakeref)
			err = ce->ops->pin(ce);
		if (err)
			goto err;

		i915_gem_context_get(ctx);
		GEM_BUG_ON(ce->gem_context != ctx);

		mutex_lock(&ctx->mutex);
		list_add(&ce->active_link, &ctx->active_engines);
		mutex_unlock(&ctx->mutex);

		smp_mb__before_atomic(); /* flush pin before it is visible */
	}

	atomic_inc(&ce->pin_count);
	GEM_BUG_ON(!intel_context_is_pinned(ce)); /* no overflow! */

	mutex_unlock(&ce->pin_mutex);
	return ce;

err:
	mutex_unlock(&ce->pin_mutex);
	return ERR_PTR(err);
}

void intel_context_unpin(struct intel_context *ce)
{
	if (likely(atomic_add_unless(&ce->pin_count, -1, 1)))
		return;

	/* We may be called from inside intel_context_pin() to evict another */
	intel_context_get(ce);
	mutex_lock_nested(&ce->pin_mutex, SINGLE_DEPTH_NESTING);

	if (likely(atomic_dec_and_test(&ce->pin_count))) {
		ce->ops->unpin(ce);

		mutex_lock(&ce->gem_context->mutex);
		list_del(&ce->active_link);
		mutex_unlock(&ce->gem_context->mutex);

		i915_gem_context_put(ce->gem_context);
		intel_context_active_release(ce);
	}

	mutex_unlock(&ce->pin_mutex);
	intel_context_put(ce);
}

static int __context_pin_state(struct i915_vma *vma, unsigned long flags)
{
	int err;

	err = i915_vma_pin(vma, 0, 0, flags | PIN_GLOBAL);
	if (err)
		return err;

	/*
	 * And mark it as a globally pinned object to let the shrinker know
	 * it cannot reclaim the object until we release it.
	 */
	vma->obj->pin_global++;
	vma->obj->mm.dirty = true;

	return 0;
}

static void __context_unpin_state(struct i915_vma *vma)
{
	vma->obj->pin_global--;
	__i915_vma_unpin(vma);
}

static void intel_context_retire(struct i915_active *active)
{
	struct intel_context *ce = container_of(active, typeof(*ce), active);

	if (ce->state)
		__context_unpin_state(ce->state);

	intel_context_put(ce);
}

void
intel_context_init(struct intel_context *ce,
		   struct i915_gem_context *ctx,
		   struct intel_engine_cs *engine)
{
	kref_init(&ce->ref);

	ce->gem_context = ctx;
	ce->engine = engine;
	ce->ops = engine->cops;
	ce->sseu = engine->sseu;
	ce->saturated = 0;

	INIT_LIST_HEAD(&ce->signal_link);
	INIT_LIST_HEAD(&ce->signals);

	mutex_init(&ce->pin_mutex);

	i915_active_init(ctx->i915, &ce->active, intel_context_retire);
}

int intel_context_active_acquire(struct intel_context *ce, unsigned long flags)
{
	int err;

	if (!i915_active_acquire(&ce->active))
		return 0;

	intel_context_get(ce);

	if (!ce->state)
		return 0;

	err = __context_pin_state(ce->state, flags);
	if (err) {
		i915_active_cancel(&ce->active);
		intel_context_put(ce);
		return err;
	}

	/* Preallocate tracking nodes */
	if (!i915_gem_context_is_kernel(ce->gem_context)) {
		err = i915_active_acquire_preallocate_barrier(&ce->active,
							      ce->engine);
		if (err) {
			i915_active_release(&ce->active);
			return err;
		}
	}

	return 0;
}

void intel_context_active_release(struct intel_context *ce)
{
	/* Nodes preallocated in intel_context_active() */
	i915_active_acquire_barrier(&ce->active);
	i915_active_release(&ce->active);
}

static void i915_global_context_shrink(void)
{
	kmem_cache_shrink(global.slab_ce);
}

static void i915_global_context_exit(void)
{
	kmem_cache_destroy(global.slab_ce);
}

static struct i915_global_context global = { {
	.shrink = i915_global_context_shrink,
	.exit = i915_global_context_exit,
} };

int __init i915_global_context_init(void)
{
	global.slab_ce = KMEM_CACHE(intel_context, SLAB_HWCACHE_ALIGN);
	if (!global.slab_ce)
		return -ENOMEM;

	i915_global_register(&global.base);
	return 0;
}

void intel_context_enter_engine(struct intel_context *ce)
{
	intel_engine_pm_get(ce->engine);
}

void intel_context_exit_engine(struct intel_context *ce)
{
	ce->saturated = 0;
	intel_engine_pm_put(ce->engine);
}
