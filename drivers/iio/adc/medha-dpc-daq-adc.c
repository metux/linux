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
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>


#define MAX_CHAN	3

#define FIFO_SLEEP_MIN	100000
#define FIFO_SLEEP_MAX	200000

/** FPGA-side register numbers. need to multiply by 4 for cpu-side offset **/
enum {
	REG_MCLK	= 128,
	REG_RESET	= 129,
	REG_TEST	= 256,
	REG_ADC0_PING	= 4,
	REG_ADC0_PONG	= 5,
	REG_ADC1_PING	= 8,
	REG_ADC1_PONG	= 9,
	REG_ADC2_PING	= 12,
	REG_ADC2_PONG	= 13,
};

struct _irq_names {
	const char* ping;
	const char* pong;
};

static const struct _irq_names irq_names[MAX_CHAN] = {
	{
		.ping = "adc0-0",
		.pong = "adc0-1",
	},
	{
		.ping = "adc1-0",
		.pong = "adc1-1",
	},
	{
		.ping = "adc2-0",
		.pong = "adc2-1",
	},
};

struct adc_device;

struct adc_channel {
	int ch;
	int irq_ping;
	int irq_pong;
	struct adc_device *adc_dev;
};

struct adc_device {
	void __iomem		*base;
	struct iio_dev		*iio_dev[MAX_CHAN];
	struct platform_device	*pdev;
};

static int adc_read_raw(struct iio_dev *idev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
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
	.driver_module	= THIS_MODULE,
	.read_raw	= &adc_read_raw,
};

// BIT(IIO_CHAN_INFO_RAW)|BIT(IIO_CHAN_INFO_SCALE)
static const struct iio_chan_spec adc_channels = {
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
};

static irqreturn_t irq_handler(int irq, void *private)
{
//	printk(KERN_INFO "medha irq_handler: %d\n", irq);
//	disable_irq(irq);
//	return IRQ_HANDLED;
	return IRQ_WAKE_THREAD;
}

static irqreturn_t irq_worker(int irq, void *private)
{
	struct iio_dev *iiodev = private;
//	struct adc_channel *adc_chan = iio_priv(iiodev);

	dev_info(&iiodev->dev, "IRQ worker:%d\n", irq);

//	int i, k, fifo1count, read;
//	u16 *data = adc_dev->data;

/*
	fifo1count = tiadc_readl(adc_dev, REG_FIFO1CNT);
	for (k = 0; k < fifo1count; k = k + i) {
                for (i = 0; i < (indio_dev->scan_bytes)/2; i++) {
                        read = tiadc_readl(adc_dev, REG_FIFO1);
                        data[i] = read & FIFOREAD_DATA_MASK;
                }
                iio_push_to_buffers(indio_dev, (u8 *) data);
        }
*/
//        tiadc_writel(adc_dev, REG_IRQSTATUS, IRQENB_FIFO1THRES);
//        tiadc_writel(adc_dev, REG_IRQENABLE, IRQENB_FIFO1THRES);

	return IRQ_HANDLED;
}

/*
static int tiadc_buffer_preenable(struct iio_dev *indio_dev)
{
	// Flush FIFO. Needed in corner cases in simultaneous tsc/adc use
	return 0;
}

static int tiadc_buffer_postenable(struct iio_dev *indio_dev)
{
	// 2do: enable sampling
	return 0;
}

static int tiadc_buffer_predisable(struct iio_dev *indio_dev)
{
	// 2do: stop recording
	//adc_dev->buffer_en_ch_steps = 0;
	//adc_dev->total_ch_enabled = 0;

	// Flush FIFO of leftover data in the time it takes to disable adc
	// 2do: send reset signal
	return 0;
}

static int adc_buffer_postdisable(struct iio_dev *indio_dev)
{
	// tiadc_step_config(indio_dev);
	return 0;
}
*/

/*
static const struct iio_buffer_setup_ops adc_buffer_setup_ops = {
	.preenable = &adc_buffer_preenable,
	.postenable = &adc_buffer_postenable,
	.predisable = &adc_buffer_predisable,
	.postdisable = &adc_buffer_postdisable,
};
*/

static inline void adc_reg_set(struct adc_device *adc, int reg, u16 val)
{
	iowrite16(val, adc->base + reg*4);
}

static inline u16 adc_reg_get(struct adc_device *adc, int reg)
{
	return ioread16(adc->base + reg*4);
}

static int looptest(struct platform_device *pdev, u16 val)
{
	struct adc_device *adc = platform_get_drvdata(pdev);
	u16 readback;
	u16 inv = ~val & 0xFFFF;

	adc_reg_set(adc, REG_TEST, val);
	msleep(50);
	readback = adc_reg_get(adc, REG_TEST);
	readback = adc_reg_get(adc, REG_TEST);
	readback = adc_reg_get(adc, REG_TEST);
	readback = adc_reg_get(adc, REG_TEST);

	if (readback == inv)
		dev_info(&pdev->dev, "looptest A: OK   w 0x%04X r 0x%04X\n", val, readback);
	else
		dev_info(&pdev->dev, "looptest A: FAIL w 0x%04X r 0x%04X e 0x%04X\n", val, readback, inv);

	return 0;
}

static int loop_write_op(void *data, u64 value)
{
	struct platform_device *pdev = data;
	return looptest(pdev, value & 0xFFFF);
}

static int cmd_write_op(void *data, u64 value)
{
	struct platform_device *pdev = data;
	struct iio_dev *iiodev = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "debug command: %lld\n", value);

	switch (value) {
		case 1:	return looptest(pdev, 0x9988);
		case 2: irq_worker(666, iiodev); break;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_cmd_fops,  NULL, cmd_write_op,  "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(dbg_loop_fops, NULL, loop_write_op, "%llu\n");

static int adc_debugfs_init(struct platform_device *pdev)
{
	struct dentry *dbg_dir = debugfs_create_dir("medha-adc", NULL);
	debugfs_create_file("cmd",  0222, dbg_dir, pdev, &dbg_cmd_fops);
	debugfs_create_file("loop", 0222, dbg_dir, pdev, &dbg_loop_fops);
	//pdata->debugfs_dentry = dbg_dir;
	return 0;
}

static int setup_device(struct platform_device *pdev, struct adc_device *adc_dev, int ch)
{
	int ret = 0;
	struct iio_dev *iiodev;
	struct adc_channel *ch_priv;
	struct iio_buffer *fifo;

	if (ch >= MAX_CHAN) {
		dev_err(&pdev->dev, "setup_device() invalid channel number: %d\n", ch);
		return -EINVAL;
	}

	dev_info(&pdev->dev, "initializing channel %d\n", ch);

	if (!(iiodev = devm_iio_device_alloc(&pdev->dev, sizeof(struct adc_channel)))) {
		dev_err(&pdev->dev, "failed to allocate iio device\n");
		return -ENOMEM;
	}

	ch_priv = iio_priv(iiodev);

	ch_priv->ch = ch;
	ch_priv->irq_ping = platform_get_irq_byname(pdev, irq_names[ch].ping);
//	ch_priv->irq_pong = platform_get_irq_byname(pdev, irq_names[ch].pong);
	dev_info(&iiodev->dev, "IRQs: CH %d -> %d %d\n", ch, ch_priv->irq_ping, ch_priv->irq_pong);

	iiodev->info = &adc_info;
	iiodev->dev.parent = &pdev->dev;
	iiodev->dev.of_node = pdev->dev.of_node;
	iiodev->name = platform_get_device_id(pdev)->name;		// FIXME
	iiodev->modes = INDIO_DIRECT_MODE /*INDIO_BUFFER_HARDWARE*/;
	iiodev->channels = &adc_channels;
	iiodev->num_channels = 1;
	iiodev->info = &adc_info;
//	iiodev->setup_ops = setup_ops;
	iiodev->modes |= INDIO_BUFFER_SOFTWARE;

	if (!(fifo = devm_iio_kfifo_allocate(&iiodev->dev))) {
		ret = -ENOMEM;
		goto err_free_dev;
	}

	iio_device_attach_buffer(iiodev, fifo);

	if ((ret = iio_device_register(iiodev)))
		goto err_free_dev;

	if ((ret = devm_request_threaded_irq(
				   &iiodev->dev,
				   ch_priv->irq_ping,
				   irq_handler /* NULL */,
				   irq_worker,
				   IRQF_ONESHOT,
				   iiodev->name,
				   iiodev)))
		goto err_free_dev;

	adc_dev->iio_dev[ch] = iiodev;

	dev_info(&iiodev->dev, "registered channel: %d\n", ch);

	return 0;

err_free_dev:
	iio_device_free(iiodev);

	return ret;
}

static int adc_driver_probe(struct platform_device *pdev)
{
	struct adc_device *adc_dev = devm_kzalloc(&pdev->dev, sizeof(struct adc_device), GFP_KERNEL);
	if (adc_dev == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	adc_dev->pdev = pdev;
	adc_dev->base = devm_ioremap_resource(
		&pdev->dev,
		platform_get_resource(pdev, IORESOURCE_MEM, 0));

	if (IS_ERR(adc_dev->base)) {
		dev_err(&pdev->dev, "adc_driver_probe() failed to get iomem\n");
		return PTR_ERR(adc_dev->base);
	}

	platform_set_drvdata(pdev, adc_dev);

	setup_device(pdev, adc_dev, 0);
	setup_device(pdev, adc_dev, 1);
	setup_device(pdev, adc_dev, 2);

	adc_debugfs_init(pdev);

	return 0;
}

static const struct of_device_id adc_dt_match[] = {
	{ .compatible = "medha,m337decc-02-adc" },
	{ }
};

static struct platform_driver adc_driver = {
	.driver		= {
		.name   = "m337decc-02-adc",
		.of_match_table = of_match_ptr(adc_dt_match),
	},
};
module_platform_driver_probe(adc_driver, adc_driver_probe);

MODULE_AUTHOR("Enrico Weigelt, metux IT consule <info@metux.net>");
MODULE_DESCRIPTION("ADC driver for Medha Railway DPC DAQ/diagnostics module");
MODULE_LICENSE("GPL v3");
MODULE_ALIAS("platform:m337decc-02-adc");
