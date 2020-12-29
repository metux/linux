/* SPDX-License-Identifier: GPL-2.0+ */

/*
 * AMD FCH gpio platform data definitions
 *
 * Copyright (C) 2020 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 *
 */

#ifndef __DT_BINDINGS_GPIO_AMD_FCH_REGS_H
#define __DT_BINDINGS_GPIO_AMD_FCH_REGS_H

/*
 * gpio registers addresses
 *
 * these regs need to be assigned by board setup, since they're wired
 * in very board specifici was, rarely documented, this should not be
 * available to users.
 */
#define AMD_FCH_GPIO_REG_GPIO49		0x40
#define AMD_FCH_GPIO_REG_GPIO50		0x41
#define AMD_FCH_GPIO_REG_GPIO51		0x42
#define AMD_FCH_GPIO_REG_GPIO55_DEVSLP0	0x43
#define AMD_FCH_GPIO_REG_GPIO57		0x44
#define AMD_FCH_GPIO_REG_GPIO58		0x45
#define AMD_FCH_GPIO_REG_GPIO59_DEVSLP1	0x46
#define AMD_FCH_GPIO_REG_GPIO64		0x47
#define AMD_FCH_GPIO_REG_GPIO68		0x48
#define AMD_FCH_GPIO_REG_GPIO66_SPKR	0x5B
#define AMD_FCH_GPIO_REG_GPIO71		0x4D
#define AMD_FCH_GPIO_REG_GPIO32_GE1	0x59
#define AMD_FCH_GPIO_REG_GPIO33_GE2	0x5A
#define AMT_FCH_GPIO_REG_GEVT22		0x09

#endif /* __DT_BINDINGS_GPIO_AMD_FCH_REGS_H */
