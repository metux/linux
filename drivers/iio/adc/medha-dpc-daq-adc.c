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

struct medha_dpc_daq_adc_channel {
	int id;
	int addr;
};

struct medha_dpc_daq_adc_device {


};

static int medha_dpc_daq_adc_probe(struct platform_device *pdev)
{
	return -EINVAL; // not implemented yet
}

static int medha_dpc_daq_adc_remove(struct platform_device *pdev)
{
	return 0; // not implemented yet
}

static const struct of_device_id medha_dpc_daq_adc_dt_match[] = {
	{ .compatible = "medha,dpc-daq-adc" },
	{ }
};

static struct platform_driver medha_dpc_daq_adc_driver = {
	.probe		= medha_dpc_daq_adc_probe,
	.remove		= medha_dpc_daq_adc_probe,
	.driver		= {
		.name   = "medha-dpc-daq-adc",
		.of_match_table = of_match_ptr(medha_dpc_daq_adc_dt_match),
	},
};
module_platform_driver(medha_dpc_daq_adc_driver);

MODULE_AUTHOR("Enrico Weigelt, metux IT consule <info@metux.net>");
MODULE_DESCRIPTION("ADC driver for Medha Railway DPC DAQ/diagnostics module");
MODULE_LICENSE("GPL v3");
MODULE_ALIAS("platform:medha-dpc-daq-adc");
