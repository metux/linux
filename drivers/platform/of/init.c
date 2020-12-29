// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2021 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "../../of/of_private.h"

#define DECLARE_FDT_EXTERN(n) \
	extern char __dtb_##n##_begin[]; \
	static struct bin_attribute __dtb_##n##_attr = { \
		.attr = { .name = "fdt-" #n, .mode = S_IRUSR }, \
		.private = __dtb_##n##_begin, \
		.read = fdt_image_raw_read, \
	};

struct fdt_image {
	char *begin;
	size_t size;
	const char *basename;
	struct bin_attribute *bin_attr;
	struct device_node *root;
};

#define FDT_IMAGE_ENT(n)		\
	{ .basename = "ofdrv-" #n,	\
	  .begin = __dtb_##n##_begin,	\
	  .bin_attr = &__dtb_##n##_attr }

static ssize_t fdt_image_raw_read(struct file *filep, struct kobject *kobj,
				  struct bin_attribute *bin_attr, char *buf,
				  loff_t off, size_t count)
{
	memcpy(buf, bin_attr->private + off, count);
	return count;
}

#ifdef CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2
DECLARE_FDT_EXTERN(apu2x);
#endif

static struct fdt_image fdt[] = {
#ifdef CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2
	FDT_IMAGE_ENT(apu2x)
#endif
};

static int __init ofdrv_parse_image(struct fdt_image *image)
{
	struct device_node* root;
	void *new_fdt;

	image->size = fdt_totalsize(image->begin);
	new_fdt = kmemdup(image->begin, image->size, GFP_KERNEL);
	if (!new_fdt)
		return -ENOMEM;

	image->begin = new_fdt;
	of_fdt_unflatten_tree(new_fdt, NULL, &root);

	if (IS_ERR_OR_NULL(root))
		return PTR_ERR(root);

	image->root = root;

	return 0;
}

static bool __init ofdrv_match_dmi(const struct device_node *node)
{
#ifdef CONFIG_DMI
	const char *board = dmi_get_system_info(DMI_BOARD_NAME);
	const char *vendor = dmi_get_system_info(DMI_SYS_VENDOR);

	if (!of_match_string(node, "dmi-sys-vendor", vendor))
		return false;

	if (!of_match_string(node, "dmi-board-name", board))
		return false;

	pr_info("matched dmi: vendor=\"%s\" board=\"%s\"\n", vendor, board);
	return true;
#else
	return false;
#endif
}

static void __init ofdrv_kick_devs(struct device_node *np, const char *bus_name)
{
	struct property *prop;
	const char *walk;
	struct bus_type *bus;
	int ret;

	if (strcmp(bus_name, "name")==0)
		return;

	bus = find_bus(bus_name);
	if (!bus) {
		pr_warn("cant find bus \"%s\"\n", bus_name);
		return;
	}

	of_property_for_each_string(np, bus_name, prop, walk) {
		ret = bus_unregister_device_by_name(bus, walk);
		if (ret)
			pr_warn("failed removing device %s on bus %s: %d\n",
				walk, bus_name, ret);
		else
			pr_info("removed device: %s/%s\n", bus_name, walk);
	}

	bus_put(bus);
}

static void __init ofdrv_unbind(struct device_node *parent)
{
	struct property *pr;
	struct device_node *np = of_get_child_by_name(parent, "unbind");

	if (IS_ERR_OR_NULL(np))
		return;

	for_each_property_of_node(np, pr)
		ofdrv_kick_devs(np, pr->name);
}

static int __init ofdrv_board_probe(struct device_node *node)
{
	struct device_node *devs;

	/* check whether we have the right board for this DT */
	if (!of_device_is_compatible(node, "virtual,dmi-board"))
		return -EINVAL;

	if (!ofdrv_match_dmi(node))
		return -EINVAL;

	ofdrv_unbind(node);

	/* instantiate devices */
	devs = of_get_child_by_name(node, "devices");
	if (IS_ERR_OR_NULL(devs)) {
		pr_warn("board oftree has no devices\n");
		return -ENOENT;
	}

	return of_platform_populate(devs, NULL, NULL, NULL);
}

static int __init ofdrv_init_sysfs(struct fdt_image *image)
{
	struct device_node *np;
	int ret;

	image->bin_attr->size = image->size;
	image->bin_attr->private = image->begin;

	ret = sysfs_create_bin_file(firmware_kobj, image->bin_attr);
	if (ret)
		pr_warn("failed creating sysfs bin file: %d\n", ret);

	__of_attach_node_sysfs(image->root, image->basename);
	for_each_of_allnodes_from(image->root, np)
		__of_attach_node_sysfs(np, image->basename);

	return 0;
}

static int __init ofdrv_init_image(struct fdt_image *image)
{
	struct device_node *child;
	int ret;

	ret = ofdrv_parse_image(image);
	if (ret)
		return ret;

	ofdrv_init_sysfs(image);

	for_each_child_of_node(image->root, child)
		ofdrv_board_probe(child);

	return 0;
}

static int __init ofdrv_init(void)
{
	int x;

	for (x=0; x<ARRAY_SIZE(fdt); x++)
		ofdrv_init_image(&fdt[x]);

	return 0;
}

module_init(ofdrv_init);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Generic oftree based initialization of custom board devices");
MODULE_LICENSE("GPL");
