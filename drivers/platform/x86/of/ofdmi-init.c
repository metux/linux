// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2018 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/kernel.h>
//#include <linux/leds.h>
#include <linux/module.h>
//#include <linux/platform_device.h>
//#include <linux/gpio_keys.h>
//#include <linux/gpio/machine.h>
//#include <linux/input.h>
//#include <linux/platform_data/gpio/gpio-amd-fch.h>
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
	if (!prop) {
		pr_warn("could not find property: \"%s\"\n", name);
	}

	for (walk=of_prop_next_string(prop, NULL); walk;
	     walk=of_prop_next_string(prop, walk)) {
		pr_info("trying: \"%s\"\n", walk);
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
	if (!of_prop_match(node, "dmi-sys-vendor", vendor)) {
		pr_info("vendor does not match\n");
		return 0;
	}
	if (!of_prop_match(node, "dmi-board-name", board)) {
		pr_info("board name does not match\n");
		return 0;
	}
	pr_info("vendor and board matching\n");
	devs = of_get_child_by_name(node, "devices");
	if (IS_ERR_OR_NULL(devs)) {
		pr_warn("board has no devices\n");
		return 0;
	}
	return of_platform_populate(devs, NULL, NULL, NULL);
}

static int ofdmi_init_image(struct fdt_image *image)
{
	struct device_node *node = of_fdt_parse(image);
	struct device_node *child;

	pr_info("parsing image: %s\n", image->name);

	if (IS_ERR_OR_NULL(node)) {
		pr_warn(" failed parsing fdt image %s\n", image->name);
		return PTR_ERR(node);
	}
	pr_info("parsed fdt\n");
	for_each_child_of_node(node, child) {
		pr_info("probing child\n");
		ofdmi_board_probe(child);
	}
	return 0;
}

static int __init ofdmi_init(void)
{
	int x;

	pr_info("ofdmi init: %d\n", ARRAY_SIZE(fdt));
	for (x=0; x<ARRAY_SIZE(fdt); x++)
		ofdmi_init_image(&fdt[x]);

	return 0;
}

static void __exit ofdmi_exit(void)
{
	pr_info("ofdmi_exit\n");
}

module_init(ofdmi_init);
module_exit(ofdmi_exit);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
//MODULE_DESCRIPTION("PC Engines APUv2/APUv3 board GPIO/LEDs/keys driver");
MODULE_LICENSE("GPL");
//MODULE_ALIAS("platform:pcengines-apuv2");
//MODULE_SOFTDEP("pre: platform:" AMD_FCH_GPIO_DRIVER_NAME " platform:leds-gpio platform:gpio_keys_polled");
