/*
 * GPIO driver for the AMD G series FCH (eg. GX-412TC)
 *
 * Copyright (C) 2018 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 *
 * SPDX-License-Identifier: GPL+
 */

// FIXME: add spinlocks

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>
#include <linux/platform_data/x86/amd-fch-gpio-pdata.h>


#define GPIO_BIT_DIR		23
#define GPIO_BIT_WRITE		22
#define GPIO_BIT_READ		16


struct amd_fch_gpio_priv {
	struct platform_device		*pdev;
	struct gpio_chip		gc;
	void __iomem			*base;
	struct amd_fch_gpio_pdata	*pdata;
};

static uint32_t *amd_fch_gpio_addr(struct gpio_chip *gc, unsigned gpio)
{
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);

	if (gpio > priv->pdata->gpio_num) {
		dev_err(&priv->pdev->dev, "gpio number %d out of range\n", gpio);
		return NULL;
	}

	return priv->base + priv->pdata->gpio_reg[gpio].reg*sizeof(u32);
}

static int amd_fch_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	volatile uint32_t *ptr = amd_fch_gpio_addr(gc, offset);
	if (!ptr) return -EINVAL;

	*ptr &= ~(1 << GPIO_BIT_DIR);
	return 0;
}

static int amd_fch_gpio_direction_output(struct gpio_chip *gc, unsigned gpio, int value)
{
	volatile uint32_t *ptr = amd_fch_gpio_addr(gc, gpio);
	if (!ptr) return -EINVAL;

	*ptr |= (1 << GPIO_BIT_DIR);
	return 0;
}

static int amd_fch_gpio_get_direction(struct gpio_chip *gc, unsigned gpio)
{
	volatile uint32_t *ptr = amd_fch_gpio_addr(gc, gpio);
	if (!ptr) return -EINVAL;

	return (*ptr >> GPIO_BIT_DIR) & 1;
}

static void amd_fch_gpio_set(struct gpio_chip *gc, unsigned gpio, int value)
{
	volatile uint32_t *ptr = amd_fch_gpio_addr(gc, gpio);
	if (!ptr) return;

	if (value)
		*ptr |= (1 << GPIO_BIT_WRITE);
	else
		*ptr &= ~(1 << GPIO_BIT_WRITE);
}

static int amd_fch_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	volatile uint32_t *ptr = amd_fch_gpio_addr(gc, offset);
	if (!ptr) return -EINVAL;

	return ((*ptr) >> GPIO_BIT_READ) & 1;
}

static void amd_fch_gpio_dbg_show(struct seq_file *s, struct gpio_chip *gc)
{
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);
	(void)priv;

	seq_printf(s, "debug info not implemented yet\n");
}

static int amd_fch_gpio_request(struct gpio_chip *chip, unsigned gpio_pin)
{
	if (gpio_pin < chip->ngpio)
		return 0;

	return -EINVAL;
}

static int amd_fch_gpio_probe(struct platform_device *pdev)
{
	struct amd_fch_gpio_priv *priv;
	struct amd_fch_gpio_pdata *pdata = pdev->dev.platform_data;
	int err;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data\n");
		return -ENOENT;
	}

	if (!(priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL))) {
		dev_err(&pdev->dev, "failed to allocate priv struct\n");
		return -ENOMEM;
	}

	priv->pdata	= pdata;
	priv->pdev	= pdev;

	priv->gc.owner			= THIS_MODULE;
	priv->gc.parent			= &pdev->dev;
	priv->gc.label			= dev_name(&pdev->dev);
	priv->gc.base			= priv->pdata->gpio_base;
	priv->gc.ngpio			= priv->pdata->gpio_num;
	priv->gc.request		= amd_fch_gpio_request;
	priv->gc.direction_input	= amd_fch_gpio_direction_input;
	priv->gc.direction_output	= amd_fch_gpio_direction_output;
	priv->gc.get_direction		= amd_fch_gpio_get_direction;
	priv->gc.get			= amd_fch_gpio_get;
	priv->gc.set			= amd_fch_gpio_set;

	spin_lock_init(&priv->gc.bgpio_lock);

	if (IS_ERR(priv->base = devm_ioremap_resource(&pdev->dev, &priv->pdata->res))) {
		dev_err(&pdev->dev, "failed to map iomem\n");
		return -ENXIO;
	}

	dev_info(&pdev->dev, "initializing on my own II\n");

	if (IS_ENABLED(CONFIG_DEBUG_FS)) {
		dev_info(&pdev->dev, "enabling debugfs\n");
		priv->gc.dbg_show = amd_fch_gpio_dbg_show;
	}

	platform_set_drvdata(pdev, priv);

	err = devm_gpiochip_add_data(&pdev->dev, &priv->gc, priv);
	dev_info(&pdev->dev, "probe finished\n");
	return err;
}

static struct platform_driver amd_fch_gpio_driver = {
	.driver = {
		.name = AMD_FCH_GPIO_DRIVER_NAME,
	},
	.probe = amd_fch_gpio_probe,
};

module_platform_driver(amd_fch_gpio_driver);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("AMD G-series FCH GPIO driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio_amd_fch");
