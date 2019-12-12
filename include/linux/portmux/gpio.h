/* SPDX-License-Identifier: GPL-2.0 */
/*
 * portmux/gpio.h - definitions for the gpio portmux driver
 *
 * Copyright (C) 2020 metux IT consult
 *
 * Author: Enrico Weigelt <info@metux.net>
 */

#ifndef _LINUX_PORTMUX_GPIO_H
#define _LINUX_PORTMUX_GPIO_H

#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/portmux/driver.h>

/**
 * struct portmux_gpio_pdata - 	gpio portmux driver platform data
 * @num_choices:		number of choices
 * @choices:			array of portmux choice structures. may not
				be freed as long as the device lives.
 * @name:			device name
 */
struct portmux_gpio_pdata {
	int num_choices;
	const struct portmux_choice *choices;
	const char *name;
	const char *type;
};

#define PORTMUX_GPIO_MAX_LINES	BITS_PER_TYPE(uint64_t)

#endif /* _LINUX_PORTMUX_GPIO_H */
