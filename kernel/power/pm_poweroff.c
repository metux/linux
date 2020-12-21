// SPDX-License-Identifier: GPL-2.0-only
/*
 * pm_poweroff.c - notifiers for platform specific poweroff handlers
 *
 * archs, machs and platform drivers can register themselves here,
 * in order to be called when the machine shall be powered off.
 */

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/pm.h>

/*
 * platform specific power off handlers
 */
static ATOMIC_NOTIFIER_HEAD(power_off_nh);

int register_pm_power_off(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&power_off_nh, nb);
}
EXPORT_SYMBOL_GPL(register_pm_power_off);

int unregister_pm_power_off(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&power_off_nh, nb);
}
EXPORT_SYMBOL_GPL(unregister_pm_power_off);

int devm_register_pm_power_off(struct device *dev, struct notifier_block *nb)
{
	struct notifier_block **rcnb;
	int ret;

	rcnb = devres_alloc(devm_unregister_pm_power_off, sizeof(*rcnb),
			    GFP_KERNEL);
	if (!rcnb)
		return -ENOMEM;

	ret = register_pm_power_off(nb);
	if (!ret) {
		*rcnb = nb;
		devres_add(dev, rcnb);
	} else {
		devres_free(rcnb);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_register_pm_power_off);

static void devm_unregister_pm_power_off(struct device *dev, void *res)
{
	WARN_ON(unregister_pm_power_off(*(struct notifier_block**)res));
}

int call_pm_power_off(void)
{
	return atomic_notifier_call_chain(&power_off_nh, 0, NULL);
}
EXPORT_SYMBOL_GPL(call_pm_power_off);

static ATOMIC_NOTIFIER_HEAD(power_off_prepare_nh);

int register_pm_power_off_prepare(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&power_off_prepare_nh, nb);
}
EXPORT_SYMBOL_GPL(register_pm_power_off_prepare);

int unregister_pm_power_off_prepare(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&power_off_prepare_nh, nb);
}
EXPORT_SYMBOL_GPL(unregister_pm_power_off);

int devm_register_pm_power_off_prepare(struct device *dev, struct notifier_block *nb)
{
	struct notifier_block **rcnb;
	int ret;

	rcnb = devres_alloc(devm_unregister_pm_power_off_prepare, sizeof(*rcnb),
			    GFP_KERNEL);
	if (!rcnb)
		return -ENOMEM;

	ret = register_pm_power_off(nb);
	if (!ret) {
		*rcnb = nb;
		devres_add(dev, rcnb);
	} else {
		devres_free(rcnb);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_register_pm_power_off_prepare);

static void devm_unregister_pm_power_off_prepare(struct device *dev, void *res)
{
	WARN_ON(unregister_pm_power_off_prepare(*(struct notifier_block**)res));
}

int call_pm_power_off_prepare(void)
{
	return atomic_notifier_call_chain(&power_off_prepare_nh, 0, NULL);
}
EXPORT_SYMBOL_GPL(call_pm_power_off_prepare);
