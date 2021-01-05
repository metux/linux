// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2018 metux IT consult
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
#include <linux/memblock.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "../../../of/of_private.h"

#define FDT_EXTERN(name) \
	extern char __dtb_##name##_begin[]; \
	extern char __dtb_##name##_end[];

struct fdt_image {
	char *begin;
	char *end;
	const char *name;
	const char *compatible;
};

#define FDT_IMAGE_ENT(n,compat)		\
	{ .name = #n,			\
	  .compatible = compat,		\
	  .begin = __dtb_##n##_begin,	\
	  .end = __dtb_##n##_end }

#ifdef CONFIG_X86_PLATFORM_FDT_PCENGINES_APU2
FDT_EXTERN(apu2x);
#endif

struct fdt_image fdt[] = {
#ifdef CONFIG_X86_PLATFORM_FDT_PCENGINES_APU2
	FDT_IMAGE_ENT(apu2x,"")
#endif
};

struct device_node* of_fdt_parse(struct fdt_image *image)
{
	struct device_node* root;
	void *new_fdt;

	size_t size = fdt_totalsize(image->begin);
	new_fdt = kmemdup(image->begin, size, GFP_KERNEL);
	of_fdt_unflatten_tree(new_fdt, NULL, &root);

	return root;
}

static int of_prop_match(struct device_node *node, const char* name,
			 const char* value)
{
	struct property *prop;
	const char *walk;

	if (!name || !value)
		return 0;

	prop = of_find_property(node, name, NULL);
	if (!prop)
		return 0;

	for (walk=of_prop_next_string(prop, NULL); walk;
	     walk=of_prop_next_string(prop, walk)) {
		if (strcmp(walk, value)==0)
			return 1;
	}
	return 0;
}

static int ofdmi_board_probe(struct device_node *node)
{
	struct device_node *devs;
	const char *board = dmi_get_system_info(DMI_BOARD_NAME);
	const char *vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	pr_info("dmi: vendor=\"%s\" board=\"%s\"\n", vendor, board);

	if (!of_prop_match(node, "dmi-sys-vendor", vendor))
		return 0;

	if (!of_prop_match(node, "dmi-board-name", board))
		return 0;

	pr_info("vendor and board matching\n");
	devs = of_get_child_by_name(node, "devices");
	if (IS_ERR_OR_NULL(devs)) {
		pr_err("board has no devices\n");
		return 0;
	}

	return of_platform_populate(devs, NULL, NULL, NULL);
}

static int ofdmi_init_image(struct fdt_image *image)
{
	struct device_node *node = of_fdt_parse(image);
	struct device_node *child;

	if (IS_ERR_OR_NULL(node)) {
		pr_err("failed parsing fdt image %s\n", image->name);
		return PTR_ERR(node);
	}

	for_each_child_of_node(node, child)
		ofdmi_board_probe(child);

	return 0;
}

static int __init ofdmi_init(void)
{
	int x;

	for (x=0; x<ARRAY_SIZE(fdt); x++)
		ofdmi_init_image(&fdt[x]);

	return 0;
}

module_init(ofdmi_init);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_LICENSE("GPL");
