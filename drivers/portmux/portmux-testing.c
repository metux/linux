// SPDX-License-Identifier: GPL-2.0
/*
 * port multiplexer dummy driver
 *
 * Copyright (C) 2019 Enrico Weigelt, metux IT consult <info@metux.net>
 */

#define pr_fmt(fmt) "portmux-core: " fmt

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/portmux/driver.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

static struct platform_device *dummy_pdev = NULL;

static struct platform_device pdev_info = {
	.name = "portmux-dummy",
	.id = PLATFORM_DEVID_NONE,
};

static int __init portmux_testing_init(void)
{
	int rc = 0;

	rc = platform_device_register(&pdev_info);
	if (rc)
		pr_err("failed registering portmux-dummy: %d\n", rc);

	return rc;
}

static void __exit portmux_testing_exit(void)
{
	platform_device_unregister(dummy_pdev);
}

module_init(portmux_testing_init);
module_exit(portmux_testing_exit);

MODULE_DESCRIPTION("Port multiplexer testing driver");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_LICENSE("GPL v2");
