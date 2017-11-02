/*
 * ADC driver for the DAQ/IOT modules for the Medha Railway
 * distributed power control system
 *
 * Copyright (C) 2017 Enrico Weigelt, metux IT consult <info@metux.net>
 *                    JAQUET Technology Group AG
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input/matrix_keypad.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/triggered_buffer.h>

#define MAX_CHAN	3

struct medha_dpc_daq_adc_channel {
	int id;
	int addr;
	int irq_a;
	int irq_b;
};

struct adc_device {
	uint32_t __iomem			*base;
	struct completion			done;
	struct medha_dpc_daq_adc_channel	chan[MAX_CHAN];
	struct platform_device			*pdev;
};

irqreturn_t trigger_handler(int irq, void *dev_id)
{
	printk("medha: irq: %d\n", irq);

	return IRQ_HANDLED;
}

static int adc_read_raw(struct iio_dev *idev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
/*	struct adc_device *st = iio_priv(idev);
	int ret; */

	*val = 23;
	*val2 = 66;

	printk(KERN_INFO "adc_read_raw\n");
	switch (mask) {
		case IIO_CHAN_INFO_RAW:
			printk(KERN_INFO " == IIO_CHAN_INFO_RAW\n");
			return IIO_VAL_INT;
		break;
		case IIO_CHAN_INFO_SCALE:
			printk(KERN_INFO " == IIO_CHAN_INFO_SCALE\n");
			return IIO_VAL_FRACTIONAL_LOG2;
		break;
		default:
			printk(KERN_INFO " == default\n");
		break;
	}

	return -EINVAL;
}

static const struct iio_info adc_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &adc_read_raw,
};

// BIT(IIO_CHAN_INFO_RAW)|BIT(IIO_CHAN_INFO_SCALE)
static const struct iio_chan_spec adc_channels[3] = {
	{
		.type				= IIO_VOLTAGE,
		.info_mask_separate		= BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type	= BIT(IIO_CHAN_INFO_SCALE),
		.indexed			= 1,
		.channel			= 0,
		.scan_index			= 0,
		.scan_type			= {
			.sign		= 'u',
			.realbits	= 24,
			.storagebits	= 32,
		},
	},
	{
		.type				= IIO_VOLTAGE,
		.info_mask_separate		= BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type	= BIT(IIO_CHAN_INFO_SCALE),
		.indexed			= 1,
		.channel			= 1,
		.scan_index			= 1,
		.scan_type			= {
			.sign		= 'u',
			.realbits	= 24,
			.storagebits	= 32,
		},
	},
	{
		.type				= IIO_VOLTAGE,
		.info_mask_separate		= BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type	= BIT(IIO_CHAN_INFO_SCALE),
		.indexed			= 1,
		.channel			= 2,
		.scan_index			= 3,
		.scan_type			= {
			.sign		= 'u',
			.realbits	= 24,
			.storagebits	= 32,
		},
	},
};

#define FIFO_SLEEP_MIN  100000
#define FIFO_SLEEP_MAX  200000

static int cmd_write_op(void *data, u64 value)
{
	struct platform_device *pdev = data;
	struct iio_dev *iiodev = platform_get_drvdata(pdev);
	struct adc_device *priv = iio_priv(iiodev);

	dev_info(&pdev->dev, "debug command: %lld\n", value);

	if (priv->pdev != pdev) {
		dev_err(&pdev->dev, "pdev mismatch\n");
		return -EFAULT;
	}

	switch (value) {
		case 1:
			dev_info(&pdev->dev, "trying looptest\n");
			dev_info(&pdev->dev, "writing to test reg\n");
			priv->base[256] = 0x9988;
			dev_info(&pdev->dev, "done\n");
		break;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cmd_fops, NULL, cmd_write_op, "%llu\n");

static int adc_debugfs_init(struct platform_device *pdev)
{
	struct dentry *dbg_dir = debugfs_create_dir("medha-adc", NULL);
	debugfs_create_file("cmd", 0222, dbg_dir, pdev, &cmd_fops);
	//pdata->debugfs_dentry = dbg_dir;
	return 0;
}

static int adc_driver_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct iio_dev *iiodev;
	struct adc_device *priv;
	struct resource *res;
	void __iomem *base;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "adc_driver_probe() failed to get iomem\n");
		return PTR_ERR(base);
	}

	iiodev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!iiodev) {
		dev_err(&pdev->dev, "failed to allocate iio device\n");
		return -ENOMEM;
	}

	iiodev->info = &adc_info;

	platform_set_drvdata(pdev, iiodev);

	priv = iio_priv(iiodev);
	priv->base = base;
	priv->pdev = pdev;
	priv->chan[0].irq_a = platform_get_irq_byname(pdev, "adc0-0");
	priv->chan[0].irq_b = platform_get_irq_byname(pdev, "adc0-1");
	priv->chan[1].irq_a = platform_get_irq_byname(pdev, "adc1-0");
	priv->chan[1].irq_b = platform_get_irq_byname(pdev, "adc1-1");
	priv->chan[2].irq_a = platform_get_irq_byname(pdev, "adc2-0");
	priv->chan[2].irq_b = platform_get_irq_byname(pdev, "adc2-1");

	dev_info(&pdev->dev, "IRQs: %d %d %d %d %d %d\n",
		priv->chan[0].irq_a,
		priv->chan[0].irq_b,
		priv->chan[1].irq_a,
		priv->chan[1].irq_b,
		priv->chan[2].irq_a,
		priv->chan[2].irq_b);

	iiodev->dev.parent = &pdev->dev;
	iiodev->dev.of_node = pdev->dev.of_node;
	iiodev->name = platform_get_device_id(pdev)->name;
	iiodev->modes = INDIO_DIRECT_MODE /*INDIO_BUFFER_HARDWARE*/;
	iiodev->channels = adc_channels;
	iiodev->num_channels = ARRAY_SIZE(adc_channels);
	iiodev->info = &adc_info;

	if ((ret = iio_triggered_buffer_setup(iiodev, NULL, &trigger_handler, NULL)))
		goto err_buffer;

	if ((ret = iio_device_register(iiodev)))
		goto err_unregister;

	adc_debugfs_init(pdev);

	return 0;

err_unregister:
	iio_triggered_buffer_cleanup(iiodev);
err_buffer:
	return ret;
}

static const struct of_device_id adc_dt_match[] = {
	{ .compatible = "medha,m337decc-02-adc" },
	{ }
};

static struct platform_driver adc_driver = {
	.driver		= {
		.name   = "medha,m337decc-02-adc",
		.of_match_table = of_match_ptr(adc_dt_match),
	},
};
module_platform_driver_probe(adc_driver, adc_driver_probe);

MODULE_AUTHOR("Enrico Weigelt, metux IT consule <info@metux.net>");
MODULE_DESCRIPTION("ADC driver for Medha Railway DPC DAQ/diagnostics module");
MODULE_LICENSE("GPL v3");
MODULE_ALIAS("platform:medha-dpc-daq-adc");
