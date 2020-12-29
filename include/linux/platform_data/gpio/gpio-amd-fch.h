/* SPDX-License-Identifier: GPL-2.0+ */

/*
 * AMD FCH gpio driver platform-data
 *
 * Copyright (C) 2018 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 *
 */

#ifndef __LINUX_PLATFORM_DATA_GPIO_AMD_FCH_H
#define __LINUX_PLATFORM_DATA_GPIO_AMD_FCH_H

#include <dt-bindings/gpio/amd-fch-gpio.h>

#define AMD_FCH_GPIO_DRIVER_NAME "gpio_amd_fch"

/*
 * struct amd_fch_gpio_pdata - GPIO chip platform data
 * @gpio_num: number of entries
 * @gpio_reg: array of gpio registers
 * @gpio_names: array of gpio names
 */
struct amd_fch_gpio_pdata {
	int			gpio_num;
	int			*gpio_reg;
	const char * const	*gpio_names;
};

#endif /* __LINUX_PLATFORM_DATA_GPIO_AMD_FCH_H */
