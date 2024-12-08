/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TP  suspend Control Abstraction
 *
 * Copyright (C) RK Company
 *
 */
#ifndef _RK_TP_SUSPEND_H
#define _RK_TP_SUSPEND_H
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
#include "mtk_disp_notify.h"
#else
#include <linux/fb.h>
#include <linux/notifier.h>
#endif

static int tpd_suspend_flag = 0;

struct  tp_device{
	struct notifier_block fb_notif;
	int(*tp_suspend)(struct  tp_device*);
	int(*tp_resume)(struct  tp_device*);
	struct mutex ops_lock;
};


static inline int fb_notifier_callback(struct notifier_block *self,
				unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	int blank;
	int ret = 0;
	struct tp_device *tp;

	tp = container_of(self, struct tp_device, fb_notif);

	if (action != MTK_DISP_EVENT_BLANK)
		return 0;

	blank = *(int *)data;

	pr_info("fb_notify(blank=%d)\n", blank);
	
	switch (blank) {
	case MTK_DISP_BLANK_UNBLANK:
		pr_info("LCD ON Notify\n");
		if (tpd_suspend_flag == true) {
			ret = tp->tp_suspend(tp);
			tpd_suspend_flag = false;
		}
		break;
	case MTK_DISP_BLANK_POWERDOWN:
		pr_info("LCD OFF Notify\n");
		if (tpd_suspend_flag == false) {
			ret = tp->tp_resume(tp);
			tpd_suspend_flag = true;
		}
		break;
	default:
		break;
	}
	return 0;
#else
	struct tp_device *tp;
	struct fb_event *event = data;
	int blank_mode;
	int ret = 0;

	tp = container_of(self, struct tp_device, fb_notif);

	//printk("%s.....lin=%d tp->status=%x,blank_mode=%x\n",__func__,__LINE__,tp->status,blank_mode);

	mutex_lock(&tp->ops_lock);

	switch (action) {
	case FB_EARLY_EVENT_BLANK:
		blank_mode = *((int *)event->data);
		if (blank_mode != FB_BLANK_UNBLANK)
			ret = tp->tp_suspend(tp);
		break;

	case FB_EVENT_BLANK:
		blank_mode = *((int *)event->data);
		if (blank_mode == FB_BLANK_UNBLANK)
			tp->tp_resume(tp);
		break;

	default:
		break;
	}

	mutex_unlock(&tp->ops_lock);

	if (ret < 0)
	{
		printk("TP_notifier_callback error action=%x,blank_mode=%x\n",(int)action,blank_mode);
		return ret;
	}

	return NOTIFY_OK;
#endif

}

static inline int tp_register_fb(struct tp_device *tp)
{
	memset(&tp->fb_notif, 0, sizeof(tp->fb_notif));
	tp->fb_notif.notifier_call = fb_notifier_callback;
	mutex_init(&tp->ops_lock);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	return mtk_disp_notifier_register("Touch", &tp->fb_notif);
#else
	return fb_register_client(&tp->fb_notif);
#endif
}

static inline void tp_unregister_fb(struct tp_device *tp)
{
#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	mtk_disp_notifier_unregister(&tp->fb_notif);
#else
	fb_unregister_client(&tp->fb_notif);
#endif
}
#endif
