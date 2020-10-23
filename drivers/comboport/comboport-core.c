// SPDX-License-Identifier: GPL-2.0
/*
 * Combined port class devices
 *
 * Copyright (C) 2020 Enrico Weigelt, metux IT consult <info@metux.net>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/comboport.h>
#include <linux/kobject.h>

DECLARE_RWSEM(comboport_list_lock);
LIST_HEAD(comboport_list);

static struct class *comboport_class = NULL;

static inline void comboport_lock(struct comboport_classdev *cdev)
{
	mutex_lock(&cdev->lock);
}

static inline void comboport_unlock(struct comboport_classdev *cdev)
{
	mutex_unlock(&cdev->lock);
}

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct comboport_classdev *cp_cdev = dev_get_drvdata(dev);
	unsigned long value;
	ssize_t ret;

	if (!cp_cdev->reset)
		return -EOPNOTSUPP;

	ret = kstrtoul(buf, size, &value);
	if (ret)
		return ret;

	if (!value)
		return size;

	ret = cp_cdev->reset(cp_cdev);
	if (ret)
		return ret;

	return size;
}
DEVICE_ATTR_WO(reset);

static struct attribute *comboport_class_attrs[] = {
	&dev_attr_reset.attr,
	NULL,
};

static const struct attribute_group comboport_attr_group = {
	.attrs = comboport_class_attrs,
};

static const struct attribute_group *comboport_groups[] = {
	&comboport_attr_group,
	NULL,
};

int comboport_classdev_register(struct device *parent,
				struct comboport_classdev *cp_cdev)
{
	mutex_init(&cp_cdev->lock);
	comboport_lock(cp_cdev);

	cp_cdev->dev = device_create_with_groups(comboport_class,
						 parent,
						 0,
						 cp_cdev,
						/* comboport_groups */ NULL,
						 cp_cdev->name);
	if (IS_ERR(cp_cdev->dev)) {
		mutex_unlock(&cp_cdev->lock);
		mutex_destroy(&cp_cdev->lock);
		return PTR_ERR(cp_cdev->dev);
	}

	INIT_LIST_HEAD(&cp_cdev->chans);

	cp_cdev->devlist_kobj = kobject_create_and_add("channels",
						       &cp_cdev->dev->kobj);

	down_write(&comboport_list_lock);
	list_add_tail(&cp_cdev->node, &comboport_list);
	up_write(&comboport_list_lock);

	comboport_unlock(cp_cdev);

	return 0;
}
EXPORT_SYMBOL_GPL(comboport_classdev_register);

void comboport_classdev_unregister(struct comboport_classdev *cp_cdev)
{
	if (IS_ERR_OR_NULL(cp_cdev->dev))
		return;

	device_unregister(cp_cdev->dev);

	down_write(&comboport_list_lock);
	list_del(&cp_cdev->node);
	up_write(&comboport_list_lock);

	if (cp_cdev->free)
		cp_cdev->free(cp_cdev);

	mutex_destroy(&cp_cdev->lock);

	kobject_del(cp_cdev->devlist_kobj);
	kobject_put(cp_cdev->devlist_kobj);
	cp_cdev->devlist_kobj = NULL;
}
EXPORT_SYMBOL_GPL(comboport_classdev_unregister);

int comboport_classdev_addchan(struct comboport_classdev *cp_cdev,
			       struct comboport_chandev *channel)
{
	int ret;

	if (IS_ERR_OR_NULL(channel->dev))
		return -EINVAL;

	comboport_lock(cp_cdev);
	list_add_tail(&channel->node, &cp_cdev->chans);
	ret = sysfs_create_link(cp_cdev->devlist_kobj,
				&(channel->dev->kobj),
				channel->name);
	comboport_unlock(cp_cdev);

	return ret;
}
EXPORT_SYMBOL_GPL(comboport_classdev_addchan);

int comboport_classdev_rmchan(struct comboport_classdev *cp_cdev,
			      struct comboport_chandev *channel)
{
	if (IS_ERR_OR_NULL(channel->dev))
		return -EINVAL;

	comboport_lock(cp_cdev);
	sysfs_remove_link(&cp_cdev->dev->kobj, channel->name);
	list_del(&channel->node);
	comboport_unlock(cp_cdev);

	return 0;
}
EXPORT_SYMBOL_GPL(comboport_classdev_rmchan);

int comboport_classdev_addchan_dev(struct comboport_classdev *cp_cdev,
				   const char* chan_name,
				   struct device *chan_dev)
{
	struct comboport_chandev *chandev;

	chandev = devm_kzalloc(cp_cdev->dev,
			       sizeof(struct comboport_chandev),
			       GFP_KERNEL);
	if (!chandev)
		return -ENOMEM;

	chandev->name = chan_name;
	chandev->dev = chan_dev;
	return comboport_classdev_addchan(cp_cdev, chandev);
}
EXPORT_SYMBOL_GPL(comboport_classdev_addchan_dev);

extern struct bus_type pci_bus_type;
extern struct bus_type i2c_bus_type;
extern struct bus_type spi_bus_type;
extern struct bus_type usb_bus_type;

/* workaround for dropped find_bus() */
static struct bus_type *comboport_get_bus_type_by_name(const char* name)
{
	static struct bus_type *bus_types[] = {
		&platform_bus_type,
#ifdef CONFIG_PCI
		&pci_bus_type,
#endif
#ifdef CONFIG_I2C
		&i2c_bus_type,
#endif
#ifdef CONFIG_SPI_MASTER
		&spi_bus_type,
#endif
#ifdef CONFIG_USB
		&usb_bus_type,
#endif
	};

	int x;
	for (x=0; x<ARRAY_SIZE(bus_types); x++)
		if (strcmp(name, bus_types[x]->name)==0)
			return bus_types[x];
	return NULL;
}

static struct device *combport_get_dev_by_bus_and_name(const char *bus_name,
						       const char *dev_name)
{
	struct bus_type *bus_type = comboport_get_bus_type_by_name(bus_name);

	if (!bus_type)
		return NULL;

	return bus_find_device_by_name(bus_type, NULL, dev_name);
}

int comboport_classdev_probe_channel(struct comboport_classdev *cdev,
				     const char *name)
{
	const char *dev_name = strchr(name, '/');
	struct device *bus_dev;
	char *bus_name;
	char *chan_name;
	int name_len;
	int ret;

	if (!dev_name)
		return -EINVAL;

	bus_name = kstrndup(name, (dev_name-name), GFP_KERNEL);
	dev_name++;

	bus_dev = combport_get_dev_by_bus_and_name(bus_name, dev_name);
	if (IS_ERR_OR_NULL(bus_dev)) {
		dev_err(cdev->dev, "cant find bus device %s->%s\n",
			dev_name, bus_name);
		goto noent;
	}

	name_len = strlen(name)+1;
	chan_name = devm_kzalloc(cdev->dev, name_len, GFP_KERNEL);
	if (!chan_name) {
		ret = -ENOMEM;
		goto out;
	}

	snprintf(chan_name, name_len, "%s,%s", bus_name, dev_name);

	ret = comboport_classdev_addchan_dev(cdev, chan_name, bus_dev);

	goto out;

noent:
	ret = -ENOENT;

out:
	if (bus_name)
		kfree(bus_name);
	return 0;
}
EXPORT_SYMBOL_GPL(comboport_classdev_probe_channel);

static int __init comboport_init(void)
{
	comboport_class = class_create(THIS_MODULE, "comboport");
	if (IS_ERR(comboport_class))
		return PTR_ERR(comboport_class);

	comboport_class->dev_groups = comboport_groups;

	return 0;
}

static void __exit comboport_exit(void)
{
	class_destroy(comboport_class);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Combined port device class core");

subsys_initcall(comboport_init);
module_exit(comboport_exit);
