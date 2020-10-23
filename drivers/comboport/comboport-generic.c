// SPDX-License-Identifier: GPL-2.0
/*
 * Combined port bus type
 *
 * Copyright (C) 2020 Enrico Weigelt, metux IT consult <info@metux.net>
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/comboport.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/property.h>

static int generic_reset(struct comboport_classdev *cdev)
{
	dev_info(cdev->dev, "issueing port reset\n");
	return 0;
}

static int comboport_generic_probe(struct platform_device *pdev)
{
	struct comboport_generic *cpdev;
	int ret = 0;
	int x = 0;
	const char *chan_names[32];
	int chnum;

	cpdev = devm_kzalloc(&pdev->dev, sizeof(struct comboport_generic),
			     GFP_KERNEL);

	platform_set_drvdata(pdev, cpdev);

	ret = device_property_read_string(&pdev->dev,
					  "name",
					  &cpdev->comboport.name);
	if (ret) {
		dev_err(&pdev->dev, "missing name property\n");
		return -ENOENT;
	}

	dev_info(&pdev->dev, "probing ...\n");

	cpdev->comboport.dev = &pdev->dev;
	cpdev->comboport.reset = generic_reset;

	chnum = device_property_read_string_array(&pdev->dev,
						  "channels",
						  chan_names,
						  ARRAY_SIZE(chan_names));
	if (chnum < 0) {
		dev_err(&pdev->dev, "failed reading channel names: %d\n", ret);
		return chnum;
	}

	ret = comboport_classdev_register(&pdev->dev, &cpdev->comboport);
	if (ret) {
		dev_err(&pdev->dev, "failed registering comboport classdev\n");
		return ret;
	}

	for (x=0; x<chnum; x++) {
		dev_info(&pdev->dev, "channel #%d: %s\n", x, chan_names[x]);
		comboport_classdev_probe_channel(&cpdev->comboport, chan_names[x]);
	}

	return 0;
}

static struct platform_driver comboport_generic_driver = {
	.driver = {
		.name = COMBOPORT_GENERIC_DRIVER,
	},
	.probe = comboport_generic_probe,
};

module_platform_driver(comboport_generic_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("Combined port device class: generic device");
