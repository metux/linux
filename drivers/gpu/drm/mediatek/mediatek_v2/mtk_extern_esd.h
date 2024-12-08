#ifndef __MTK_EXTERN_ESD_H_
#define __MTK_EXTERN_ESD_H_

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

enum extern_esd_type {
	EXTERN_ESD_NONE = 0,
	EXTERN_ESD_FTS_SPI = 1,
	EXTERN_ESD_FTS_I2C = 2,
	EXTERN_ESD_MAX,
};

struct mtk_drm_extern_esd {
	int (*get_esd_status)(void);
};


int mtk_drm_extern_esd_register(enum extern_esd_type type,const struct mtk_drm_extern_esd *ops);
int mtk_drm_get_extern_esd_status(enum extern_esd_type type);
#endif
