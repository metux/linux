// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2021 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 */

#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/of_fdt.h>

#include "ofboard.h"

static bool __init ofboard_match_dmi(struct device *dev)
{
	return true;
#ifdef CONFIG_DMI
	const char *board = dmi_get_system_info(DMI_BOARD_NAME);
	const char *vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	const struct device_node *node = dev->of_node;

	if (!of_match_string(node, "dmi-sys-vendor", vendor))
		return false;

	if (!of_match_string(node, "dmi-board-name", board))
		return false;

	dev_info(dev, "matched dmi: vendor=\"%s\" board=\"%s\"\n", vendor,
		 board);

	return true;
#else
	return false;
#endif
}

static void __init ofboard_kick_devs(struct device *dev,
				     struct device_node *np,
				     const char *bus_name)
{
	struct property *prop;
	const char *walk;
	struct bus_type *bus;
	int ret;

	if (strcmp(bus_name, "name")==0)
		return;

	bus = find_bus(bus_name);
	if (!bus) {
		dev_warn(dev, "cant find bus \"%s\"\n", bus_name);
		return;
	}

	of_property_for_each_string(np, bus_name, prop, walk) {
		ret = bus_unregister_device_by_name(bus, walk);
		if (ret)
			dev_warn(dev, "failed removing device \"%s\" on bus "
				 "\"%s\": %d\n", walk, bus_name, ret);
		else
			dev_info(dev, "removed device \"%s\" from bus "
				 "\"%s\"\n", walk, bus_name);
	}

	bus_put(bus);
}

static void __init ofboard_unbind(struct device *dev)
{
	struct property *pr;
	struct device_node *np = of_get_child_by_name(dev->of_node, "unbind");

	if (!IS_ERR_OR_NULL(np))
		for_each_property_of_node(np, pr)
			ofboard_kick_devs(dev, np, pr->name);
}

static int ofboard_populate(struct device *dev)
{
	int ret = 0;
	int id = 0;

	ret = of_overlay_apply(dev_get_platdata(dev), dev->of_node, &id);
	if (ret < 0) {
		dev_err(dev, "failed to apply overlay\n");
		return ret;
	}

	dev_info(dev, "applied overlay: %d\n", id);
	return ret;
}

static int ofboard_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *devnode;

	dev_info(dev, "probing: %s ...\n", dev_name(&pdev->dev));

	of_fdt_unflatten_tree(dev_get_platdata(dev), NULL, &devnode);
	if (!devnode) {
		dev_err(&pdev->dev, "failed unflattening fdt\n");
		return -EINVAL;
	}

	dev->of_node = devnode;

	if (!ofboard_match_dmi(dev)) {
		dev_info(dev, "dmi info doesnt match\n");
		return -EINVAL;
	}

	ofboard_unbind(&pdev->dev);

	return ofboard_populate(dev);
}

struct platform_driver ofboard_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = ofboard_probe,
};
