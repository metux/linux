// SPDX-License-Identifier: GPL-2.0
/*
 * port multiplexer subsystem
 *
 * Copyright (C) 2019 Enrico Weigelt, metux IT consult <info@metux.net>
 */

#define pr_fmt(fmt) "portmux-core: " fmt

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/portmux/driver.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

static inline void portmux_lock(struct portmux_classdev *portmux)
{
	spin_lock(&portmux->spinlock);
}

static inline void portmux_unlock(struct portmux_classdev *portmux)
{
	spin_unlock(&portmux->spinlock);
}

/* sysfs attribute "name" */
static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct portmux_classdev *portmux = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", portmux->name);
}
DEVICE_ATTR_RO(name);

/* sysfs attribute "type" */
static ssize_t type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct portmux_classdev *portmux = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", portmux->type);
}
DEVICE_ATTR_RO(type);

/* sysfs attribute "choices" */
static ssize_t choices_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int x;
	int len = 0;
	int l;

	struct portmux_classdev *portmux = dev_get_drvdata(dev);

	for (x=0; x<portmux->num_choices; x++) {
		l = sprintf(buf, "%s\n", portmux->choices[x].name);
		len += l;
		buf += l;
	}

	return len;
}
DEVICE_ATTR_RO(choices);

/* sysfs attribute "active" */
static ssize_t active_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct portmux_classdev *portmux = dev_get_drvdata(dev);
	int rc;

	rc = portmux_classdev_get(portmux);
	if (rc < 0)
		goto out;

	rc = sprintf(buf, "%s", portmux->choices[rc].name);

out:
	return rc;
}
static ssize_t active_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct portmux_classdev *portmux = dev_get_drvdata(dev);
	int x;
	int rc = -EINVAL;

	for (x=0; x<portmux->num_choices; x++) {
		if (sysfs_streq(buf, portmux->choices[x].name)) {
			dev_info(dev, "setting choice: %s\n", portmux->choices[x].name);
			rc = portmux_classdev_set(portmux, x);
			if (rc)
				goto out;

			rc = strlen(buf);
			goto out;
		}
	}

out:
	return rc;
}
DEVICE_ATTR_RW(active);

static struct attribute *portmux_classdev_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_type.attr,
	&dev_attr_active.attr,
	&dev_attr_choices.attr,
	NULL,
};

static const struct attribute_group portmux_classdev_group = {
	.attrs = portmux_classdev_attrs,
};

static const struct attribute_group *portmux_classdev_groups[] = {
	&portmux_classdev_group,
	NULL,
};

void portmux_classdev_unregister(struct portmux_classdev *portmux)
{
	if ((!portmux->dev) || (IS_ERR(portmux->dev))) {
		pr_err("tried unregistering a not registered portmux classdev\n");
		return;
	}

	portmux_lock(portmux);

	if (portmux->release)
		portmux->release(portmux);

	device_unregister(portmux->dev);
	portmux->dev = NULL;

	portmux_unlock(portmux);
}
EXPORT_SYMBOL_GPL(portmux_classdev_unregister);

int portmux_classdev_get(struct portmux_classdev *portmux)
{
	int rc;

	portmux_lock(portmux);
	rc = portmux->get(portmux);
	portmux_unlock(portmux);

	if (rc >= portmux->num_choices)
		return -ERANGE;

	return rc;
}
EXPORT_SYMBOL_GPL(portmux_classdev_get);

int portmux_classdev_set(struct portmux_classdev *portmux, int idx)
{
	int rc;

	if ((idx < 0) || (idx >= portmux->num_choices))
		return -ERANGE;

	portmux_lock(portmux);
	rc = portmux->set(portmux, idx);
	portmux_unlock(portmux);

	return rc;
}
EXPORT_SYMBOL_GPL(portmux_classdev_set);

static struct class portmux_class = {
	.name = "portmux",
	.owner = THIS_MODULE,
	.dev_groups = portmux_classdev_groups,
};

int portmux_classdev_register(struct device *parent,
			      struct portmux_classdev *portmux)
{
	int rc = 0;

	spin_lock_init(&portmux->spinlock);
	portmux_lock(portmux);

	portmux->parent = parent;

	BUG_ON(!portmux->set);
	BUG_ON(!portmux->get);

	portmux->dev = device_create(&portmux_class,
				     parent,
				     0,
				     portmux,
				     "%s",
				     portmux->name);

	dev_set_drvdata(portmux->dev, portmux);

	if (IS_ERR(portmux->dev)) {
		pr_err(
			"device_create_with_groups failed: %ld\n",
			PTR_ERR(portmux->dev));
		rc = PTR_ERR(portmux->dev);
		goto out;
	}

	dev_info(portmux->dev, "portmux device registered\n");

out:
	portmux_unlock(portmux);
	return rc;
}
EXPORT_SYMBOL_GPL(portmux_classdev_register);

/* --- devm wrappers --- */
static void __devm_portmux_classdev_release(struct device *dev, void *res)
{
	portmux_classdev_unregister(*(struct portmux_classdev**)res);
}

int devm_portmux_classdev_register(struct device *parent,
				   struct portmux_classdev *portmux)
{
	int rc;

	rc = portmux_classdev_register(parent, portmux);
	if (rc)
		return rc;

	return devres_add_auto_ptr(parent, __devm_portmux_classdev_release, portmux);
}
EXPORT_SYMBOL_GPL(devm_portmux_classdev_register);

static int __init portmux_init(void)
{
	return class_register(&portmux_class);
}

static void __exit portmux_exit(void)
{
	class_unregister(&portmux_class);
}

subsys_initcall(portmux_init);
module_exit(portmux_exit);

MODULE_DESCRIPTION("Port multiplexer subsystem");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_LICENSE("GPL v2");
