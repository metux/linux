// SPDX-License-Identifier: GPL-2.0
/*
 * port multiplexer dummy driver
 *
 * Copyright (C) 2019 Enrico Weigelt, metux IT consult <info@metux.net>
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/portmux/driver.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

const struct portmux_choice choices[] = {
	{
		.name = "choice-1",
		.description = "choice #1",
	},
	{
		.name = "choice-2",
		.description = "choice #2",
	},
	{
		.name = "choice-3",
		.description = "choice #3",
	},
};

struct privdata {
	struct portmux_classdev portmux;
	int current_choice;
};

static int portmux_dummy_set(struct portmux_classdev *portmux, int idx)
{
	struct privdata *pd = portmux->priv;

	dev_info(portmux->dev, "set() choice %d\n", idx);
	pd->current_choice = idx;
	return 0;
}

static int portmux_dummy_get(struct portmux_classdev *portmux)
{
	struct privdata *pd = portmux->priv;
	return pd->current_choice;
}

static int portmux_dummy_probe(struct platform_device *pdev)
{
	struct privdata *pd = devm_kzalloc(&pdev->dev, sizeof(struct privdata), GFP_KERNEL);

	if (!pd)
		return -ENOMEM;

	pd->portmux.name = "dummy";
	pd->portmux.type = "nothing:void";
	pd->portmux.choices = choices;
	pd->portmux.num_choices = ARRAY_SIZE(choices);
	pd->portmux.set = portmux_dummy_set;
	pd->portmux.get = portmux_dummy_get;
	pd->portmux.priv = pd;
	pd->current_choice = 0;
	return devm_portmux_classdev_register(&pdev->dev, &pd->portmux);
}

static struct platform_driver portmux_dummy_driver = {
	.driver = {
		.name = "portmux-dummy",
	},
	.probe = portmux_dummy_probe,
};

module_platform_driver(portmux_dummy_driver);

MODULE_DESCRIPTION("Port multiplexer dummy driver");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_LICENSE("GPL v2");
