// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2021 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/libfdt.h>
#include <linux/slab.h>

#include "ofboard.h"

#define DECLARE_FDT_EXTERN(n) \
	extern char __dtb_##n##_begin[]; \

#define DECLARE_OFIMG(n) \
	{					\
		.data = __dtb_##n##_begin,	\
		.name = "ofboard-" #n,		\
	}

struct ofimg {
	char *name;
	void *data;
};

#ifdef CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2
DECLARE_FDT_EXTERN(testing);
#endif

static struct ofimg of_images[] = {
#ifdef CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2
	DECLARE_OFIMG(testing),
#endif
};

static int __init ofdrv_init(void)
{
	int x;

	init_oftree();

	platform_driver_register(&ofboard_driver);

	for (x=0; x<ARRAY_SIZE(of_images); x++) {
		struct platform_device_info pdevinfo = {
			.name = of_images[x].name,
			.driver_override = DRIVER_NAME,
			.id = PLATFORM_DEVID_NONE,
			.data = of_images[x].data,
			.size_data = fdt_totalsize(of_images[x].data),
		};
		pr_info("populating ofimage: %s\n", of_images[x].name);
		platform_device_register_full(&pdevinfo);
	}

	pr_info("ofdrv done\n");

	return 0;
}

module_init(ofdrv_init);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Generic oftree based initialization of custom board devices");
MODULE_LICENSE("GPL");
