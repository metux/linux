// SPDX-License-Identifier: GPL-2.0
/*
 * Combined port bus type
 *
 * Copyright (C) 2020 Enrico Weigelt, metux IT consult <info@metux.net>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/comboport.h>
#include <linux/property.h>

static const char *fw_devices[] = {
	"platform/QEMU0002:00",
//	"acpi/LNXCPU:00",
};

static const struct property_entry fw_properties[] = {
	PROPERTY_ENTRY_STRING("name", "combo-dummy0"),
	PROPERTY_ENTRY_STRING_ARRAY("channels", fw_devices),
	{ }
};

static struct platform_device *comboport_platform_device = NULL;

static int __init comboport_dummy_init(void)
{
	struct platform_device_info pdevinfo = {
		.name = COMBOPORT_GENERIC_DRIVER,
		.id = PLATFORM_DEVID_NONE,
		.fwnode = fwnode_create_software_node(
			fw_properties,
			NULL),
	};

	comboport_platform_device = platform_device_register_full(&pdevinfo);

	if (IS_ERR(comboport_platform_device))
		pr_err(COMBOPORT_GENERIC_DRIVER
		       ": failed registering device: %ld\n",
		       PTR_ERR(comboport_platform_device));

	return PTR_ERR(comboport_platform_device);
}

static void __exit comboport_dummy_exit(void)
{
	platform_device_unregister(comboport_platform_device);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Combined port device class: dummy device");

module_init(comboport_dummy_init);
module_exit(comboport_dummy_exit);
