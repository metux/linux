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

#define FDT_IMAGE_ENT(n)		\
	{ .basename = "ofdrv-" #n,	\
	  .begin = __dtb_##n##_begin,	\
	  .fdt_name = "fdt-" #n,	\
	}

struct fdt_image {
	char *begin;
	const char *basename;
	const char *fdt_name;
};

struct ofdrv_priv {
	struct device_node *root;
	struct fdt_image *pdata;
	void *begin;
	size_t size;
	struct bin_attribute bin_attr;
};

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

// FIXME: make it __init ? will be copied anyways
static struct fdt_image fdt[] = {
#ifdef CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2
	FDT_IMAGE_ENT(apu2x)
#endif
};

struct platform_device *pdevs[ARRAY_SIZE(fdt)];

static bool ofdrv_match_dmi(const struct device_node *node)
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

static void ofdrv_kick_devs(struct device_node *np, const char *bus_name)
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

static void ofdrv_unbind(struct device_node *parent)
{
	struct property *pr;
	struct device_node *np = of_get_child_by_name(parent, "unbind");

	if (IS_ERR_OR_NULL(np))
		return;

	for_each_property_of_node(np, pr)
		ofdrv_kick_devs(np, pr->name);
}

static int ofdrv_board_probe(struct device_node *node)
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

static int ofdrv_probe(struct platform_device *pdev)
{
	struct fdt_image *pdata;
	struct ofdrv_priv *priv;
	struct device_node *child;
	int ret;

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct fdt_image), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pdata = pdata;
	platform_set_drvdata(pdev, priv);

	/* unflatten the fdt */
	priv->size = fdt_totalsize(pdata->begin);
	priv->begin = devm_kmemdup(&pdev->dev, pdata->begin, priv->size,
				   GFP_KERNEL);
	if (!priv->begin)
		return -ENOMEM;

	of_fdt_unflatten_tree(priv->begin, NULL, &priv->root);

	if (IS_ERR_OR_NULL(priv->root)) {
		dev_err(&pdev->dev, "failed unflattening tree: %ld\n",
			PTR_ERR(priv->root));
		return PTR_ERR(priv->root);
	}

	/* initialize sysfs bin file */
	priv->bin_attr.attr.name = pdata->fdt_name;
	priv->bin_attr.attr.mode = S_IRUSR;
	priv->bin_attr.size = priv->size;
	priv->bin_attr.private = priv->begin;
	priv->bin_attr.read = fdt_image_raw_read;
	ret = sysfs_create_bin_file(firmware_kobj, &priv->bin_attr);
	if (ret)
		dev_warn(&pdev->dev, "failed creating sysfs file: %d\n", ret);

	/* attach dt nodes to sysfs */
	__of_attach_node_sysfs(priv->root, priv->pdata->basename);
	for_each_of_allnodes_from(priv->root, child)
		__of_attach_node_sysfs(child, priv->pdata->basename);

	/* do probing */
	for_each_child_of_node(priv->root, child)
		ofdrv_board_probe(child);

	return 0;
}

static int ofdrv_remove(struct platform_device *pdev)
{
	return 0;
}

#define DRIVER_NAME	"ofdrv"

struct platform_driver ofdrv_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = ofdrv_probe,
	.remove = ofdrv_remove,
};

static int __init ofdrv_init(void)
{
	int x;

	platform_driver_register(&ofdrv_driver);

	for (x=0; x<ARRAY_SIZE(fdt); x++) {
		pdevs[x] = platform_device_register_resndata(NULL,
			DRIVER_NAME,
			x,
			NULL,
			0,
			&fdt[x],
			sizeof(struct fdt_image));
	}

	return 0;
}

static void __exit ofdrv_exit(void)
{
	int x;

	for (x=0; x<ARRAY_SIZE(fdt); x++)
		if (!IS_ERR_OR_NULL(pdevs[x]))
			platform_device_unregister(pdevs[x]);
}

module_init(ofdrv_init);
module_exit(ofdrv_exit);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Generic oftree based initialization of custom board devices");
MODULE_LICENSE("GPL");
