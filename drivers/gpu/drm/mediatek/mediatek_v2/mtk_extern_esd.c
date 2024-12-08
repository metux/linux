// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <uapi/linux/sched/types.h>
#include <linux/pinctrl/consumer.h>
#include <linux/printk.h>

#include "mtk_extern_esd.h"

static const struct mtk_drm_extern_esd *ext_esd_ops[EXTERN_ESD_MAX] = {NULL};

int mtk_drm_extern_esd_register(enum extern_esd_type type,const struct mtk_drm_extern_esd *ops)
{
	if ((type <= EXTERN_ESD_NONE) || (type >= EXTERN_ESD_MAX)) {
		pr_err("%s err extern esd type:%d EXTERN_ESD[%d ~ %d]\n",
					__func__, type, EXTERN_ESD_NONE+1, EXTERN_ESD_MAX-1);
		return -ENODEV;
	}

	ext_esd_ops[type] = ops;

	return 0;
}

EXPORT_SYMBOL_GPL(mtk_drm_extern_esd_register);

int mtk_drm_get_extern_esd_status(enum extern_esd_type type)
{
	if (ext_esd_ops[type] == NULL) {
		pr_err("%s err extern esd type:%d ops is NULL\n",__func__, type);
		return 0;
	}
	
	return ext_esd_ops[type]->get_esd_status();
}
EXPORT_SYMBOL_GPL(mtk_drm_get_extern_esd_status);