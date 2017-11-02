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


#define MAX_CHAN	3

struct medha_dpc_daq_adc_channel {
	int id;
	int addr;
	int irq;
};

struct medha_dpc_daq_adc_device {
	struct completion			done;
	spinlock_t				spi_lock;		// FIXME: init
	struct medha_dpc_daq_adc_channel	chan[MAX_CHAN];
	struct spi_device			*spi;
};

static int medha_dpc_daq_adc_spicall(struct medha_dpc_daq_adc_device *dev, struct spi_message *msg)
{
	unsigned long lockflags = 0;
	struct spi_device *spidev = to_spi_device(dev);

	spin_lock_irqsave(&dev->spi_lock, lockflags);
	spi_sync(spidev, msg);
	spin_unlock_irqrestore(&dev->spi_lock, lockflags);

	return 0;
}

static u16 medha_dpc_daq_adc_read16(struct medha_dpc_daq_adc_device *dev, u8 reg)
{
	u16 val = 0;

	struct spi_transfer tr0 = { .tx_buf = &reg, .len = sizeof(reg) };
	struct spi_transfer tr1 = { .rx_buf = &val, .len = sizeof(val) };

	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(&t0, &msg);
	spi_message_add_tail(&t1, &msg);

	return medha_dpc_daq_adc_spicall(dev, &msg);
}

static int medha_dpc_daq_adc_write8(struct medha_dpc_daq_adc_device *dev, u8 reg, u8 val)
{
	struct spi_transfer tr0 = { .tx_buf = &reg, .len = sizeof(reg) };
	struct spi_transfer tr1 = { .tx_buf = &val, .len = sizeof(val) };

	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(&t0, &msg);
	spi_message_add_tail(&t1, &msg);

	return medha_dpc_daq_adc_spicall(dev, &msg);
}

static int medha_dpc_daq_adc_write16(struct medha_dpc_daq_adc_device *dev, u8 reg, u16 val)
{
	struct spi_transfer tr0 = { .tx_buf = &reg, .len = sizeof(reg) };
	struct spi_transfer tr1 = { .tx_buf = &val, .len = sizeof(val) };

	struct spi_message msg;
	spi_message_init(&msg);
	spi_message_add_tail(&t0, &msg);
	spi_message_add_tail(&t1, &msg);

	return medha_dpc_daq_adc_spicall(dev, &msg);
}

irqreturn_t trigger_handler(int irq, void *dev_id)
{
	printk("medha: irq: %d\n", irq);

	return IRQ_HANDLED;
}

static int medha_dpc_daq_adc_probe(struct spi_device *spi)
{
	int ret = 0;
	struct iio_dev *iiodev;
	struct medha_dpc_daq_adc_device *priv;

	iiodev = devm_iio_device_alloc(&spi->dev, sizeof(*priv));
	if (!iiodev)
		return -ENOMEM;

	priv = iio_priv(iiodev);
	spi_set_drvdata(spi, iio_dev);

	priv->spi = spi;

	iiodev->dev.parent = &spi->dev;
	iiodev->dev.of_node = spi->dev.of_node;
	iiodev->name = spi_get_device_id(spi)->name;
	iiodev->modes = INDIO_DIRECT_MODE;
	iiodev->channels = MAX_CHAN;

	if (ret = iio_triggered_buffer_setup(iiodev, NULL, &trigger_handler, NULL))
		goto err_buffer;

	if (ret = iio_device_register(iiodev))
		goto err_unregister;

	return 0;

err_unregister:
	iio_triggered_buffer_cleanup(iiodev);
err_buffer:
	return ret;
}

static int medha_dpc_daq_adc_remove(struct spi_device *spi)
{
	struct iio_dev *iiodev = spi_get_drvdata(spi);
	struct medha_dpc_daq_adc_device *priv;

	iio_device_unregister(iiodev);
	iio_triggered_buffer_cleanup(iiodev);

	return 0;
}

static const struct of_device_id medha_dpc_daq_adc_dt_match[] = {
	{ .compatible = "medha,dpc-daq-adc" },
	{ }
};

static struct spi_driver medha_dpc_daq_adc_driver = {
	.probe		= medha_dpc_daq_adc_probe,
	.remove		= medha_dpc_daq_adc_probe,
	.driver		= {
		.name   = "medha-dpc-daq-adc",
		.of_match_table = of_match_ptr(medha_dpc_daq_adc_dt_match),
	},
};
module_spi_driver(medha_dpc_daq_adc_driver);

MODULE_AUTHOR("Enrico Weigelt, metux IT consule <info@metux.net>");
MODULE_DESCRIPTION("ADC driver for Medha Railway DPC DAQ/diagnostics module");
MODULE_LICENSE("GPL v3");
MODULE_ALIAS("platform:medha-dpc-daq-adc");
