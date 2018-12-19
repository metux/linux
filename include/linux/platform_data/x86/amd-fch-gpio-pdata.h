/*
 * AMD FCH gpio driver platform-data
 *
 * Copyright (C) 2018 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 *
 * SPDX-License-Identifier: GPL
 */

#ifndef AMD_FCH_PDATA_H
#define AMD_FCH_PDATA_H


#include <linux/ioport.h>

#define AMD_FCH_GPIO_DRIVER_NAME "gpio_amd_fch"

/*
 * struct amd_fch_gpio_reg - GPIO register definition
 * @reg: register index
 * @name: signal name
 */
struct amd_fch_gpio_reg {
    int         reg;
    const char* name;
};

/*
 * struct amd_fch_gpio_pdata - GPIO chip platform data
 * @resource: iomem range
 * @gpio_reg: array of gpio registers
 * @gpio_num: number of entries
 */
struct amd_fch_gpio_pdata {
    struct resource          res;
    int                      gpio_num;
    struct amd_fch_gpio_reg *gpio_reg;
    int                      gpio_base;
};

#endif /* AMD_FCH_PDATA_H */
