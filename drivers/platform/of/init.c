// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2021 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include "../../of/of_private.h"

#include "ofboard.h"

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
	char *basename;
	struct bin_attribute *bin_attr;
	struct device_node *root;
};

#define FDT_IMAGE_ENT(n)			\
	{					\
		.begin = __dtb_##n##_begin,	\
		.bin_attr = &__dtb_##n##_attr,	\
		.basename = "ofboard-" #n	\
	}

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

DECLARE_FDT_EXTERN(testing);

static struct fdt_image fdt[] = {
#ifdef CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2
	FDT_IMAGE_ENT(apu2x)
#endif
};

static int __init ofdrv_init_sysfs(struct fdt_image *image)
{
	image->bin_attr->size = image->size;
	image->bin_attr->private = image->begin;

	if (sysfs_create_bin_file(firmware_kobj, image->bin_attr))
		pr_warn("failed creating sysfs bin_file\n");

	of_attach_tree_sysfs(image->root, image->basename);

	return 0;
}

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

static void populate_fdt(const char* name, char *ptr)
{
	struct platform_device_info pdevinfo = {
		.name = name,
		.driver_override = DRIVER_NAME,
		.id = PLATFORM_DEVID_NONE,
		.data = ptr,
		.size_data = fdt_totalsize(ptr),
	};
	platform_device_register_full(&pdevinfo);
}

struct ofimg {
	char *name;
	void *data;
};


#define DECLARE_OFIMG(n) \
	{					\
		.data = __dtb_##n##_begin,	\
		.name = "ofboard-" #n,		\
	}

static struct ofimg of_images[] = {
	DECLARE_OFIMG(testing),
};

static int __init ofdrv_init(void)
{
	int x;

	init_oftree();

	platform_driver_register(&ofboard_driver);

	for (x=0; x<ARRAY_SIZE(of_images); x++) {
		pr_info("populating ofimage: %s\n", of_images[x].name);
		populate_fdt(of_images[x].name, of_images[x].data);
	}

	pr_info("ofdrv done\n");

	return 0;
}

module_init(ofdrv_init);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Generic oftree based initialization of custom board devices");
MODULE_LICENSE("GPL");
