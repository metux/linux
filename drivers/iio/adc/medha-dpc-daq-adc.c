/*
 * ADC driver for the DAQ/IOT modules for the Medha Railway
 * distributed power control system
 *
 * Copyright (C) 2017 Enrico Weigelt, metux IT consult <info@metux.net>
 *                    JAQUET Technology Group AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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
#include <linux/spinlock.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define __TIMESTAMP

#define MAX_CHAN	3
#define CHUNK_SIZE	2048
#define REG_SZ		2

/** FPGA-side register numbers. need to multiply by REG_SZ for cpu-side offset **/
enum {
	REG_MCLK	= 256,
	REG_RESET	= 258,
	REG_TEST	= 512,
	REG_ADC0_PING	= 8,
	REG_ADC0_PONG	= 10,
	REG_ADC1_PING	= 16,
	REG_ADC1_PONG	= 18,
	REG_ADC2_PING	= 24,
	REG_ADC2_PONG	= 26,
};

enum {
	SAMPLE_RATE_50K		= 0,
	SAMPLE_RATE_100K	= 1,
	SAMPLE_RATE_150K	= 2,
	SAMPLE_RATE_200K	= 3,
	SAMPLE_RATE_250K	= 4,
	SAMPLE_RATE_300K	= 5,
	SAMPLE_RATE_350K	= 6,
	SAMPLE_RATE_400K	= 7,
	SAMPLE_RATE_450K	= 8,
	SAMPLE_RATE_500K	= 9,
};

struct m337decc_chan_spec {
	const char* dev_name;
	const char* irq_name_ping;
	const char* irq_name_pong;
	int reg_ping;
	int reg_pong;
};

static const struct m337decc_chan_spec chan_spec[MAX_CHAN] = {
	{
		.irq_name_ping = "adc0-0",
		.irq_name_pong = "adc0-1",
		.dev_name = "m337decc-adc0",
		.reg_ping = REG_ADC0_PING,
		.reg_pong = REG_ADC0_PONG,
	},
	{
		.irq_name_ping = "adc1-0",
		.irq_name_pong = "adc1-1",
		.dev_name = "m337decc-adc1",
		.reg_ping = REG_ADC1_PING,
		.reg_pong = REG_ADC1_PONG,
	},
	{
		.irq_name_ping = "adc2-0",
		.irq_name_pong = "adc2-1",
		.dev_name = "m337decc-adc2",
		.reg_ping = REG_ADC2_PING,
		.reg_pong = REG_ADC2_PONG,
	},
};

struct m337decc_device;

struct m337decc_channel {
	int ch;
	int irq_ping;
	int irq_pong;
	struct m337decc_device *adc_dev;
	s64 timestamp;
	spinlock_t lock;
};

struct m337decc_device {
	void __iomem		*base;
	struct iio_dev		*iio_dev[MAX_CHAN];
	struct m337decc_channel	*adc_chan[MAX_CHAN];
	struct platform_device	*pdev;
	int			enable_count;
	spinlock_t		lock;
};

static const struct iio_chan_spec m337decc_channels[] = {
	{
		.type				= IIO_VOLTAGE,
		.info_mask_separate		= BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type	= BIT(IIO_CHAN_INFO_SCALE) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.indexed			= 1,
		.channel			= 0,
		.scan_index			= 0,
		.scan_type			= {
			.sign		= 's',
			.realbits	= 24,
			.storagebits	= 32,
		},
	},
#ifdef __TIMESTAMP
	IIO_CHAN_SOFT_TIMESTAMP(1),
#endif
};

static inline void m337decc_reg_set(struct m337decc_device *adc, int reg, u16 val)
{
	iowrite16(val, adc->base + reg*REG_SZ);
}

static inline u16 m337decc_reg_get(struct m337decc_device *adc, int reg)
{
	return ioread16(adc->base + reg*REG_SZ);
}

static inline u32 m337decc_reg_get32(struct m337decc_device *adc, int reg)
{
	u32* ptr = adc->base + reg*REG_SZ;
	return *ptr;
//	return ioread32(adc->base + reg*REG_SZ);
}

static inline struct m337decc_channel *iio_m337decc_channel(struct iio_dev *indio_dev)
{
	return iio_priv(indio_dev);
}

static int m337decc_start(struct m337decc_device *adc_dev)
{
	int cnt;

	dev_info(&adc_dev->pdev->dev, "starting ADC\n");
	spin_lock_bh(&adc_dev->lock);

	if (unlikely(adc_dev->enable_count < 0))
		adc_dev->enable_count = 0;

	if (!adc_dev->enable_count) {
		m337decc_reg_set(adc_dev, REG_RESET, 1);
	}

	cnt = adc_dev->enable_count++;

	spin_unlock_bh(&adc_dev->lock);

	dev_info(&adc_dev->pdev->dev, "start: new enable_count=%d\n", cnt);
	return 0;
}

static int m337decc_stop(struct m337decc_device *adc_dev)
{
	int cnt;
	dev_info(&adc_dev->pdev->dev, "stopping ADC\n");

	spin_lock_bh(&adc_dev->lock);

	if (unlikely(adc_dev->enable_count < 0))
		adc_dev->enable_count = 0;

	if (adc_dev->enable_count) {
		m337decc_reg_set(adc_dev, REG_RESET, 0);
	}

	cnt = adc_dev->enable_count--;

	spin_unlock_bh(&adc_dev->lock);

	dev_info(&adc_dev->pdev->dev, "stop: new enable count=%d\n", cnt);

	return 0;
}

static inline struct m337decc_device *iio_m337decc_device(struct iio_dev *indio_dev)
{
	return iio_m337decc_channel(indio_dev)->adc_dev;
}

static inline struct m337decc_device *pdev_to_adc(struct platform_device *pdev)
{
	return platform_get_drvdata(pdev);
}

static int m337decc_set_sample_rate(struct m337decc_device *adc, int rate)
{
	u16 r;
	dev_info(&adc->pdev->dev, "setting sample rate: %d\n", rate);
	switch (rate) {
		case  50000:	r=0;	break;
		case 100000:	r=1;	break;
		case 150000:	r=2;	break;
		case 200000:	r=3;	break;
		case 250000:	r=4;	break;
		case 300000:	r=5;	break;
		case 350000:	r=6;	break;
		case 400000:	r=7;	break;
		case 450000:	r=8;	break;
		case 500000:	r=9;	break;
		default:
			dev_err(&adc->pdev->dev, "unsupported sampling rate: %d\n", rate);
			return -EINVAL;
	}
	m337decc_reg_set(adc, REG_MCLK, r);
	return 0;
}

static int m337decc_get_sample_rate(struct m337decc_device *adc)
{
	u16 r = m337decc_reg_get(adc, REG_MCLK);
	switch (r) {
		case 0:	return  50000;
		case 1:	return 100000;
		case 2:	return 150000;
		case 3:	return 200000;
		case 4:	return 250000;
		case 5:	return 300000;
		case 6:	return 350000;
		case 7:	return 400000;
		case 8:	return 450000;
		case 9:	return 500000;
	}
	dev_err(&adc->pdev->dev, "illegal sampling rate: %d\n", r);
	return 0;
}

static ssize_t m337decc_write_frequency(struct device *dev,
				        struct device_attribute *attr,
				        const char *buf,
				        size_t len)
{
	struct m337decc_device *adc_dev = iio_m337decc_device(dev_to_iio_dev(dev));
	int ret;
	u16 val;

	if ((ret = kstrtou16(buf, 10, &val)))
		return ret;

	if ((ret = m337decc_set_sample_rate(adc_dev, val)))
		return ret;

	return len;
}

static ssize_t m337decc_read_frequency(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct m337decc_device *adc_dev = iio_m337decc_device(dev_to_iio_dev(dev));
	return sprintf(buf, "%d\n", m337decc_get_sample_rate(adc_dev));
}

static IIO_CONST_ATTR(sampling_frequency_available,"50000 100000 150000 200000 250000 300000 350000 400000 450000 500000");
static IIO_DEV_ATTR_SAMP_FREQ(0644, m337decc_read_frequency, m337decc_write_frequency);

static struct attribute *m337decc_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	NULL
};

static const struct attribute_group m337decc_attribute_group = {
	.attrs = m337decc_attributes
};

static irqreturn_t m337decc_irq_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct m337decc_channel *adc_chan = iio_m337decc_channel(indio_dev);
	adc_chan->timestamp = iio_get_time_ns(indio_dev);
	return IRQ_WAKE_THREAD;
}

/*
   scale: -2.5 .. +2.5V
   1 step = 2.5V / 2^23 ~ 298nV
*/
#define SCALE_INTEGER	0
#define SCALE_NANO	298

static int m337decc_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int *val,
			     int *val2,
			     long mask)
{
	struct m337decc_device *adc_dev = iio_m337decc_device(indio_dev);

	switch (mask) {
		case IIO_CHAN_INFO_SCALE:
			*val = SCALE_INTEGER;
			*val2 = SCALE_NANO;
			dev_info(&indio_dev->dev, "read_raw: IIO_CHAN_INFO_SCALE: %d:%d\n", *val, *val2);
			return IIO_VAL_INT_PLUS_NANO;

		case IIO_CHAN_INFO_SAMP_FREQ:
			dev_info(&indio_dev->dev, "read_raw: IIO_CHAN_INFO_SAMP_FREQ\n");
			*val = m337decc_get_sample_rate(adc_dev);
			*val2 = 0;
			return IIO_VAL_INT;
	}

	dev_info(&indio_dev->dev, "read_raw: unhandled mask: %ld\n", mask);
	return -EINVAL;
}

// FIXME: that affects all devices !
static int m337decc_write_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int val,
			      int val2,
			      long mask)
{
	struct m337decc_device *adc_dev = iio_m337decc_device(indio_dev);

	switch (mask) {
		case IIO_CHAN_INFO_SAMP_FREQ:
			dev_info(&indio_dev->dev, "setting sample freq at %d: val=%d val2=%d\n", -1, val, val2);
			m337decc_set_sample_rate(adc_dev, val);
			return 0;

		case IIO_CHAN_INFO_SCALE:
			dev_info(&indio_dev->dev, "setting scale %d: val=%d val2=%d\n", -1, val, val2);
			return 0;
	}

	return -EINVAL;
}

static irqreturn_t m337decc_irq_worker(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct m337decc_channel *adc_chan = iio_m337decc_channel(indio_dev);
	int reg = 0;
	int x;
	int ch = adc_chan->ch;
	char *bufn = "<none>";
//	u32 *dump_buf;

	if (adc_chan == NULL) {
		dev_err(&indio_dev->dev, "adc_chan IS NULL !\n");
		return IRQ_HANDLED;
	}

//	dump_buf = devm_kzalloc(&indio_dev->dev,
//				sizeof(u32) * CHUNK_SIZE,
//				GFP_KERNEL);

	spin_lock_bh(&adc_chan->lock);

	/* find out for which ADC / buf we're acting */
	if (irq == adc_chan->irq_ping) {
		dev_dbg(&indio_dev->dev, "chan %d ping buffer\n", ch);
		reg = chan_spec[ch].reg_ping;
		bufn = "ping";
	}
	else if (irq == adc_chan->irq_pong) {
		dev_dbg(&indio_dev->dev, "chan %d pong buffer\n", ch);
		reg = chan_spec[ch].reg_pong;
		bufn = "pong";
	}
	else {
		dev_err(&indio_dev->dev, " unknown irq/buffer\n");
		return IRQ_HANDLED;
	}

	for (x=0; x<CHUNK_SIZE; x++) {
		u32 sample1 = m337decc_reg_get32(adc_chan->adc_dev, reg);
//		dump_buf[x] = sample1;
//		msleep(50);
//		dev_info(&indio_dev->dev, "CH %d retrieved: %8X\n", ch, sample1);
#ifdef __TIMESTAMP
		iio_push_to_buffers_with_timestamp(indio_dev,
						   &sample1,
						   iio_get_time_ns(indio_dev));
#else
		iio_push_to_buffers(indio_dev, &sample1);
#endif
	}

	spin_unlock_bh(&adc_chan->lock);

	dev_info(&indio_dev->dev, "chan %d finished %s\n", ch, bufn);
//	char textbuf[128];
//	snprintf(textbuf, sizeof(textbuf), "BUF %d %s: ", ch, bufn);
//	print_hex_dump_bytes(textbuf, DUMP_PREFIX_OFFSET, dump_buf, sizeof(u32)*CHUNK_SIZE);
//	devm_kfree(&indio_dev->dev, dump_buf);

	return IRQ_HANDLED;
}

static int m337decc_buffer_preenable(struct iio_dev *indio_dev)
{
	dev_info(&indio_dev->dev, "m337decc_buffer_preenable\n");
	// FIXME: fixup broken sampling rate ?
	// FIXME: do a loop test ?
	return 0;
}

static int m337decc_buffer_postenable(struct iio_dev *indio_dev)
{
	struct m337decc_device *adc_dev = iio_m337decc_device(indio_dev);
	dev_info(&indio_dev->dev, "m337decc_buffer_postenable\n");
	m337decc_start(adc_dev);
	return 0;
}

static int m337decc_buffer_predisable(struct iio_dev *indio_dev)
{
	struct m337decc_device *adc_dev = iio_m337decc_device(indio_dev);
	dev_info(&indio_dev->dev, "m337decc_buffer_predisable\n");
	m337decc_stop(adc_dev);
	return 0;
}

static int m337decc_buffer_postdisable(struct iio_dev *indio_dev)
{
	dev_info(&indio_dev->dev, "m337decc_buffer_postdisable\n");
	return 0;
}

static const struct iio_buffer_setup_ops m337decc_buffer_setup_ops = {
	.preenable	= &m337decc_buffer_preenable,
	.postenable	= &m337decc_buffer_postenable,
	.predisable	= &m337decc_buffer_predisable,
	.postdisable	= &m337decc_buffer_postdisable,
};

static int m337decc_looptest(struct platform_device *pdev, u16 val)
{
	struct m337decc_device *adc_dev = pdev_to_adc(pdev);
	u16 readback;
	u16 inv = ~val & 0xFFFF;

	m337decc_reg_set(adc_dev, REG_TEST, val);
	msleep(50);
	readback = m337decc_reg_get(adc_dev, REG_TEST);
	readback = m337decc_reg_get(adc_dev, REG_TEST);
	readback = m337decc_reg_get(adc_dev, REG_TEST);
	readback = m337decc_reg_get(adc_dev, REG_TEST);

	if (readback == inv)
		dev_info(&pdev->dev, "looptest A: OK   w 0x%04X r 0x%04X\n", val, readback);
	else
		dev_info(&pdev->dev, "looptest A: FAIL w 0x%04X r 0x%04X e 0x%04X\n", val, readback, inv);

	return 0;
}

static int loop_write_op(void *data, u64 value)
{
	struct platform_device *pdev = data;
	return m337decc_looptest(pdev, value & 0xFFFF);
}

static int cmd_write_op(void *data, u64 value)
{
	struct platform_device *pdev = data;
	struct m337decc_device *adc_dev = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "debug command: %lld\n", value);

	switch (value) {
		case 1:	return m337decc_looptest(pdev, 0x9988);
		case 3: m337decc_start(adc_dev); break;
		case 4: m337decc_stop(adc_dev); break;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_cmd_fops,  NULL, cmd_write_op,  "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(dbg_loop_fops, NULL, loop_write_op, "%llu\n");

static int m337decc_debugfs_init(struct platform_device *pdev)
{
	struct dentry *dbg_dir = debugfs_create_dir("medha-adc", NULL);
	debugfs_create_file("cmd",  0222, dbg_dir, pdev, &dbg_cmd_fops);
	debugfs_create_file("loop", 0222, dbg_dir, pdev, &dbg_loop_fops);
	//pdata->debugfs_dentry = dbg_dir;
	return 0;
}

static const struct iio_info m337decc_info = {
	.driver_module	= THIS_MODULE,
	.read_raw	= &m337decc_read_raw,
	.write_raw	= &m337decc_write_raw,
	.attrs		= &m337decc_attribute_group
};

static int m337decc_setup_device(struct platform_device *pdev, struct m337decc_device *adc_dev, int ch)
{
	int ret = 0;
	struct iio_dev *indio_dev;
	struct m337decc_channel *ch_priv;
	struct iio_buffer *fifo;

	if (ch >= MAX_CHAN) {
		dev_err(&pdev->dev, "m337decc_setup_device() invalid channel number: %d\n", ch);
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "initializing channel %d\n", ch);

	if (!(indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct m337decc_channel)))) {
		dev_err(&pdev->dev, "failed to allocate iio device\n");
		return -ENOMEM;
	}

	ch_priv = iio_m337decc_channel(indio_dev);

	ch_priv->ch = ch;
	ch_priv->adc_dev = adc_dev;
	ch_priv->irq_ping = platform_get_irq_byname(pdev, chan_spec[ch].irq_name_ping);
	ch_priv->irq_pong = platform_get_irq_byname(pdev, chan_spec[ch].irq_name_pong);
	spin_lock_init(&ch_priv->lock);
	dev_dbg(&indio_dev->dev, "IRQs: CH %d -> %d %d\n", ch, ch_priv->irq_ping, ch_priv->irq_pong);

	adc_dev->adc_chan[ch] = ch_priv;

	indio_dev->info = &m337decc_info;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->name = chan_spec[ch].dev_name;
	indio_dev->modes = INDIO_DIRECT_MODE /*INDIO_BUFFER_HARDWARE*/;
	indio_dev->channels = m337decc_channels;
	indio_dev->num_channels = ARRAY_SIZE(m337decc_channels);
	indio_dev->info = &m337decc_info;
	indio_dev->setup_ops = &m337decc_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	if (!(fifo = devm_iio_kfifo_allocate(&indio_dev->dev))) {
		ret = -ENOMEM;
		goto err_free_dev;
	}

	iio_device_attach_buffer(indio_dev, fifo);

	if ((ret = iio_device_register(indio_dev)))
		goto err_free_dev;

	if ((ret = devm_request_threaded_irq(&indio_dev->dev,
					     ch_priv->irq_ping,
					     m337decc_irq_handler,
					     m337decc_irq_worker,
					     IRQF_ONESHOT,
					     indio_dev->name,
					     indio_dev)))
		goto err_free_dev;

	if ((ret = devm_request_threaded_irq(&indio_dev->dev,
					     ch_priv->irq_pong,
					     m337decc_irq_handler,
					     m337decc_irq_worker,
					     IRQF_ONESHOT,
					     indio_dev->name,
					     indio_dev)))
		goto err_free_dev;

	adc_dev->iio_dev[ch] = indio_dev;

	dev_info(&indio_dev->dev, "registered channel: %d\n", ch);

	return 0;

err_free_dev:
	iio_device_free(indio_dev);

	return ret;
}

static int m337decc_driver_probe(struct platform_device *pdev)
{
	struct m337decc_device *adc_dev = devm_kzalloc(&pdev->dev,
						       sizeof(struct m337decc_device),
						       GFP_KERNEL);
	if (adc_dev == NULL)
		return -ENOMEM;

	adc_dev->pdev = pdev;
	adc_dev->base = devm_ioremap_resource(
		&pdev->dev,
		platform_get_resource(pdev, IORESOURCE_MEM, 0));
	adc_dev->enable_count = 0;
	spin_lock_init(&adc_dev->lock);

	if (IS_ERR(adc_dev->base)) {
		dev_err(&pdev->dev, "m337decc_driver_probe() failed to get iomem\n");
		return PTR_ERR(adc_dev->base);
	}

	platform_set_drvdata(pdev, adc_dev);

	m337decc_setup_device(pdev, adc_dev, 0);
	m337decc_setup_device(pdev, adc_dev, 1);
	m337decc_setup_device(pdev, adc_dev, 2);

	m337decc_debugfs_init(pdev);

	return 0;
}

static const struct of_device_id m337decc_dt_match[] = {
	{ .compatible = "medha,m337decc-02-adc" },
	{ }
};

static struct platform_driver m337decc_driver = {
	.driver		= {
		.name		= "m337decc-02-adc",
		.of_match_table	= of_match_ptr(m337decc_dt_match),
	},
};
module_platform_driver_probe(m337decc_driver, m337decc_driver_probe);

MODULE_AUTHOR("Enrico Weigelt, metux IT consule <info@metux.net>");
MODULE_DESCRIPTION("ADC driver for Medha Railway DPC DAQ/diagnostics module");
MODULE_LICENSE("GPL v3");
MODULE_ALIAS("platform:m337decc-02-adc");
