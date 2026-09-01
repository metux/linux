/* SPDX-License-Identifier: MIT */
#ifndef _INTEL_RINGBUFFER_H_
#define _INTEL_RINGBUFFER_H_

#include <drm/drm_util.h>

#include <linux/hashtable.h>
#include <linux/irq_work.h>
#include <linux/random.h>
#include <linux/seqlock.h>

#include "i915_pmu.h"
#include "i915_reg.h"
#include "i915_request.h"
#include "i915_selftest.h"
#include "gt/intel_timeline.h"
#include "intel_engine_types.h"
#include "intel_device_info.h"
#include "intel_gpu_commands.h"
#include "intel_workarounds.h"

struct drm_printer;

/* Early gen2 devices have a cacheline of just 32 bytes, using 64 is overkill,
 * but keeps the logic simple. Indeed, the whole purpose of this macro is just
 * to give some inclination as to some of the magic values used in the various
 * workarounds!
 */
#define CACHELINE_BYTES 64
#define CACHELINE_DWORDS (CACHELINE_BYTES / sizeof(u32))

/*
 * The register defines to be used with the following macros need to accept a
 * base param, e.g:
 *
 * REG_FOO(base) _MMIO((base) + <relative offset>)
 * ENGINE_READ(engine, REG_FOO);
 *
 * register arrays are to be defined and accessed as follows:
 *
 * REG_BAR(base, i) _MMIO((base) + <relative offset> + (i) * <shift>)
 * ENGINE_READ_IDX(engine, REG_BAR, i)
 */

#define __ENGINE_REG_OP(op__, engine__, ...) \
	intel_uncore_##op__((engine__)->uncore, __VA_ARGS__)

#define __ENGINE_READ_OP(op__, engine__, reg__) \
	__ENGINE_REG_OP(op__, (engine__), reg__((engine__)->mmio_base))

#define ENGINE_READ16(...)	__ENGINE_READ_OP(read16, __VA_ARGS__)
#define ENGINE_READ(...)	__ENGINE_READ_OP(read, __VA_ARGS__)
#define ENGINE_READ_FW(...)	__ENGINE_READ_OP(read_fw, __VA_ARGS__)
#define ENGINE_POSTING_READ(...) __ENGINE_READ_OP(posting_read_fw, __VA_ARGS__)
#define ENGINE_POSTING_READ16(...) __ENGINE_READ_OP(posting_read16, __VA_ARGS__)

#define ENGINE_READ64(engine__, lower_reg__, upper_reg__) \
	__ENGINE_REG_OP(read64_2x32, (engine__), \
			lower_reg__((engine__)->mmio_base), \
			upper_reg__((engine__)->mmio_base))

#define ENGINE_READ_IDX(engine__, reg__, idx__) \
	__ENGINE_REG_OP(read, (engine__), reg__((engine__)->mmio_base, (idx__)))

#define __ENGINE_WRITE_OP(op__, engine__, reg__, val__) \
	__ENGINE_REG_OP(op__, (engine__), reg__((engine__)->mmio_base), (val__))

#define ENGINE_WRITE16(...)	__ENGINE_WRITE_OP(write16, __VA_ARGS__)
#define ENGINE_WRITE(...)	__ENGINE_WRITE_OP(write, __VA_ARGS__)
#define ENGINE_WRITE_FW(...)	__ENGINE_WRITE_OP(write_fw, __VA_ARGS__)

#define GEN6_RING_FAULT_REG_READ(engine__) \
	intel_uncore_read((engine__)->uncore, RING_FAULT_REG(engine__))

#define GEN6_RING_FAULT_REG_POSTING_READ(engine__) \
	intel_uncore_posting_read((engine__)->uncore, RING_FAULT_REG(engine__))

#define GEN6_RING_FAULT_REG_RMW(engine__, clear__, set__) \
({ \
	u32 __val; \
\
	__val = intel_uncore_read((engine__)->uncore, \
				  RING_FAULT_REG(engine__)); \
	__val &= ~(clear__); \
	__val |= (set__); \
	intel_uncore_write((engine__)->uncore, RING_FAULT_REG(engine__), \
			   __val); \
})

/* seqno size is actually only a uint32, but since we plan to use MI_FLUSH_DW to
 * do the writes, and that must have qw aligned offsets, simply pretend it's 8b.
 */
enum intel_engine_hangcheck_action {
	ENGINE_IDLE = 0,
	ENGINE_WAIT,
	ENGINE_ACTIVE_SEQNO,
	ENGINE_ACTIVE_HEAD,
	ENGINE_ACTIVE_SUBUNITS,
	ENGINE_WAIT_KICK,
	ENGINE_DEAD,
};

static inline const char *
hangcheck_action_to_str(const enum intel_engine_hangcheck_action a)
{
	switch (a) {
	case ENGINE_IDLE:
		return "idle";
	case ENGINE_WAIT:
		return "wait";
	case ENGINE_ACTIVE_SEQNO:
		return "active seqno";
	case ENGINE_ACTIVE_HEAD:
		return "active head";
	case ENGINE_ACTIVE_SUBUNITS:
		return "active subunits";
	case ENGINE_WAIT_KICK:
		return "wait kick";
	case ENGINE_DEAD:
		return "dead";
	}

	return "unknown";
}


#endif /* _INTEL_RINGBUFFER_H_ */
