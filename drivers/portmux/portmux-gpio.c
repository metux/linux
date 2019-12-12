// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO-controlled port multiplexer driver
 *
 * Copyright (C) 2019 Enrico Weigelt, metux IT consult <info@metux.net>
 */

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/portmux/driver.h>
#include <linux/portmux/gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/property.h>

struct privdata {
	struct portmux_classdev portmux;
	struct gpio_descs *gpios;
};

static int portmux_gpio_set(struct portmux_classdev *portmux, int choice)
{
	struct privdata *pd = portmux->priv;
	int i;
	uint64_t mask = (uint64_t) portmux->choices[choice].data;

	for (i=0; i<pd->gpios->ndescs; i++)
		gpiod_set_value(pd->gpios->desc[i], (mask >> i) & 1);

	return 0;
}

static int portmux_gpio_get(struct portmux_classdev *portmux)
{
	struct privdata *pd = portmux->priv;
	int i;
	uint64_t mask = 0;

	for (i=0; i<pd->gpios->ndescs; i++) {
		if (gpiod_get_value(pd->gpios->desc[i])) {
			dev_info(portmux->dev, "line %d enabled\n", i);
			mask |= BIT(i);
		} else {
			dev_info(portmux->dev, "line %d disabled\n", i);
		}
	}

	dev_info(portmux->dev, "mask: %lld\n", mask);

	// now look for the matching choice
	for (i=0; i<portmux->num_choices; i++) {
		if ((uint64_t)portmux->choices[i].data == mask) {
			dev_info(portmux->dev, "matching choice: %d\n", i);
			return i;
		}
	}

	dev_err(portmux->dev, "no matching choice for mask %lld\n", mask);

	return -EINVAL;
}

static const struct of_device_id portmux_gpio_dt_ids[] = {
	{ .compatible = "gpio-portmux", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, portmux_gpio_dt_ids);

static int portmux_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct portmux_gpio_pdata *pdata;
	struct privdata *pd = devm_kzalloc(dev, sizeof(struct privdata), GFP_KERNEL);
	int pins;

	dev_info(&pdev->dev, "probing portmux-gpio ...\n");

	if (!pd)
		return -ENOMEM;

	pdata = dev_get_platdata(dev);
	if (!pdata) {
		dev_err(dev, "missing platform data\n");
		return -ENOENT;
	}

	pins = gpiod_count(dev, NULL);
	if (pins < 0) {
		dev_err(dev, "invalid pin count: %d\n", pins);
		return -EINVAL;
	}

	if (pins > PORTMUX_GPIO_MAX_LINES) {
		dev_err(dev, "too many lines: %d (max: %ld)\n", pins, PORTMUX_GPIO_MAX_LINES);
		return -ERANGE;
	}

	pd->portmux.name = devm_kstrdup(dev, pdata->name, GFP_KERNEL);
	pd->portmux.type = devm_kstrdup(dev, pdata->type, GFP_KERNEL);
	pd->portmux.choices = pdata->choices;
	pd->portmux.num_choices = pdata->num_choices;
	pd->portmux.set = portmux_gpio_set;
	pd->portmux.get = portmux_gpio_get;
	pd->portmux.priv = pd;

	pd->gpios = devm_gpiod_get_array(dev, NULL, GPIOD_OUT_LOW);
	if (IS_ERR(pd->gpios))
		return PTR_ERR(pd->gpios);

	return devm_portmux_classdev_register(dev, &pd->portmux);
}

static struct platform_driver portmux_gpio_driver = {
	.driver = {
		.name = "portmux-gpio",
		.of_match_table = of_match_ptr(portmux_gpio_dt_ids),
	},
	.probe = portmux_gpio_probe,
};
module_platform_driver(portmux_gpio_driver);

MODULE_DESCRIPTION("GPIO-controlled port multiplexer driver");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_LICENSE("GPL v2");
