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

#include "ofboard.h"

static bool __init ofboard_match_dmi(struct device *dev)
{
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
	int ret;
	struct device_node *of_node = dev->of_node;
	struct device_node *np = of_get_child_by_name(of_node, "devices");

	if (IS_ERR_OR_NULL(np)) {
		dev_info(dev, "board oftree has no devices\n");
		return -ENOENT;
	}

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed probing of childs: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ofboard_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (!ofboard_match_dmi(dev))
		return -EINVAL;

	ofboard_unbind(&pdev->dev);

	return ofboard_populate(dev);
}

static const struct of_device_id ofboard_of_match[] = {
	{ .compatible = "virtual,dmi-board" },
	{}
};

struct platform_driver ofboard_driver = {
	.driver = {
		.name = "ofboard",
		.of_match_table = ofboard_of_match,
	},
	.probe = ofboard_probe,
};
