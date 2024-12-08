// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
//#include <mt-plat/mtk_boot_common.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20211210
#include <mt-plat/csci.h>
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
#include "mtk_disp_notify.h"
#endif

/* for magnify velocity******************************************** */
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC, 0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC, 1)
#define TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, 2, struct tpd_filter_t)
#ifdef CONFIG_COMPAT
#define COMPAT_TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, \
						2, struct tpd_filter_t)
#endif
struct tpd_filter_t tpd_filter;
struct tpd_dts_info tpd_dts_data;
struct pinctrl *pinctrl1;
struct pinctrl_state *pins_default;
struct pinctrl_state *eint_as_int, *eint_output0,
		*eint_output1, *rst_output0, *rst_output1;


const struct of_device_id touch_of_match[] = {
	{ .compatible = "mediatek,touch", },
	{ .compatible = "mediatek,mt8167-touch", },
	{},
};

void tpd_get_dts_info(void)
{
	struct device_node *node1 = NULL;
	int key_dim_local[16] = {0}, i = 0;

	node1 = of_find_matching_node(node1, touch_of_match);
	if (node1) {
		of_property_read_u32(node1,
			"tpd-max-touch-num", &tpd_dts_data.touch_max_num);
		of_property_read_u32(node1,
			"use-tpd-button", &tpd_dts_data.use_tpd_button);
		pr_debug("[tpd]use-tpd-button = %d\n",
			tpd_dts_data.use_tpd_button);
		if (of_property_read_u32_array(node1, "tpd-resolution",
			tpd_dts_data.tpd_resolution,
			ARRAY_SIZE(tpd_dts_data.tpd_resolution))) {
			pr_info("[tpd] resulution is %d %d",
				tpd_dts_data.tpd_resolution[0],
				tpd_dts_data.tpd_resolution[1]);
		}

		if (tpd_dts_data.use_tpd_button) {
			of_property_read_u32(node1,
				"tpd-key-num", &tpd_dts_data.tpd_key_num);
			if (of_property_read_u32_array(node1, "tpd-key-local",
				tpd_dts_data.tpd_key_local,
				ARRAY_SIZE(tpd_dts_data.tpd_key_local)))
				pr_debug("tpd-key-local: %d %d %d %d",
					tpd_dts_data.tpd_key_local[0],
					tpd_dts_data.tpd_key_local[1],
					tpd_dts_data.tpd_key_local[2],
					tpd_dts_data.tpd_key_local[3]);
			if (of_property_read_u32_array(node1,
				"tpd-key-dim-local",
				key_dim_local, ARRAY_SIZE(key_dim_local))) {
				memcpy(tpd_dts_data.tpd_key_dim_local,
					key_dim_local, sizeof(key_dim_local));
				for (i = 0; i < 4; i++) {
					pr_debug("[tpd]key[%d].key_x = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_x);
					pr_debug("[tpd]key[%d].key_y = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_y);
					pr_debug("[tpd]key[%d].key_W = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_width);
					pr_debug("[tpd]key[%d].key_H = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_height);
				}
			}
		}
		of_property_read_u32(node1, "tpd-filter-enable",
			&tpd_dts_data.touch_filter.enable);
		if (tpd_dts_data.touch_filter.enable) {
			of_property_read_u32(node1,
				"tpd-filter-pixel-density",
				&tpd_dts_data.touch_filter.pixel_density);
			if (of_property_read_u32_array(node1,
				"tpd-filter-custom-prameters",
				(u32 *)tpd_dts_data.touch_filter.W_W,
				ARRAY_SIZE(tpd_dts_data.touch_filter.W_W)))
				pr_debug("get tpd-filter-custom-parameters");
			if (of_property_read_u32_array(node1,
				"tpd-filter-custom-speed",
				tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD,
				ARRAY_SIZE(tpd_dts_data
						.touch_filter
						.VECLOCITY_THRESHOLD)))
				pr_debug("get tpd-filter-custom-speed");
		}
		memcpy(&tpd_filter,
			&tpd_dts_data.touch_filter, sizeof(tpd_filter));
		pr_debug("[tpd]tpd-filter-enable = %d, pixel_density = %d\n",
				tpd_filter.enable, tpd_filter.pixel_density);
		tpd_dts_data.tpd_use_ext_gpio =
			of_property_read_bool(node1, "tpd-use-ext-gpio");
		of_property_read_u32(node1,
			"tpd-rst-ext-gpio-num",
			&tpd_dts_data.rst_ext_gpio_num);

	} else {
		TPD_DMESG("can't find touch compatible custom node\n");
	}
}

static DEFINE_MUTEX(tpd_set_gpio_mutex);
void tpd_gpio_as_int(int pin)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DEBUG("[tpd] %s\n", __func__);
	if (pin == 1)
		pinctrl_select_state(pinctrl1, eint_as_int);
	mutex_unlock(&tpd_set_gpio_mutex);
}

void tpd_gpio_output(int pin, int level)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DEBUG("%s pin = %d, level = %d\n", __func__, pin, level);
	if (pin == 1) {
		if (level)
			pinctrl_select_state(pinctrl1, eint_output1);
		else
			pinctrl_select_state(pinctrl1, eint_output0);
	} else {
		if (tpd_dts_data.tpd_use_ext_gpio) {
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
			mt6306_set_gpio_dir(
				tpd_dts_data.rst_ext_gpio_num, 1);
			mt6306_set_gpio_out(
				tpd_dts_data.rst_ext_gpio_num, level);
#endif
		} else {
			if (level)
				pinctrl_select_state(pinctrl1, rst_output1);
			else
				pinctrl_select_state(pinctrl1, rst_output0);
		}
	}
	mutex_unlock(&tpd_set_gpio_mutex);
}
int tpd_get_gpio_info(struct platform_device *pdev)
{
	int ret;

	TPD_DEBUG("[tpd %d] mt_tpd_pinctrl+++++++++++++++++\n", pdev->id);
	pinctrl1 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl1)) {
		ret = PTR_ERR(pinctrl1);
		dev_info(&pdev->dev, "fwq Cannot find pinctrl1!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		TPD_DMESG("Cannot find pinctrl default %d!\n", ret);
	}
	eint_as_int = pinctrl_lookup_state(pinctrl1, "state_eint_as_int");
	if (IS_ERR(eint_as_int)) {
		ret = PTR_ERR(eint_as_int);
		TPD_DMESG("Cannot find pinctrl state_eint_as_int!\n");
		return ret;
	}


	eint_output0 = pinctrl_lookup_state(pinctrl1, "state_eint_output0");
	if (IS_ERR(eint_output0)) {
		ret = PTR_ERR(eint_output0);
		TPD_DMESG("Cannot find pinctrl state_eint_output0!\n");
		return ret;
	}
	eint_output1 = pinctrl_lookup_state(pinctrl1, "state_eint_output1");
	if (IS_ERR(eint_output1)) {
		ret = PTR_ERR(eint_output1);
		TPD_DMESG("Cannot find pinctrl state_eint_output1!\n");
		return ret;
	}
	if (tpd_dts_data.tpd_use_ext_gpio == false) {
		rst_output0 =
			pinctrl_lookup_state(pinctrl1, "state_rst_output0");
		if (IS_ERR(rst_output0)) {
			ret = PTR_ERR(rst_output0);
			TPD_DMESG("Cannot find pinctrl state_rst_output0!\n");
			return ret;
		}
		rst_output1 =
			pinctrl_lookup_state(pinctrl1, "state_rst_output1");
		if (IS_ERR(rst_output1)) {
			ret = PTR_ERR(rst_output1);
			TPD_DMESG("Cannot find pinctrl state_rst_output1!\n");
			return ret;
		}
	}
	
	
	TPD_DEBUG("[tpd%d] mt_tpd_pinctrl----------\n", pdev->id);
	return 0;
}

static int tpd_misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int tpd_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long tpd_compat_ioctl(
			struct file *file, unsigned int cmd,
			unsigned long arg)
{
	long ret;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_TPD_GET_FILTER_PARA:
		if (arg32 == NULL) {
			pr_info("invalid argument.");
			return -EINVAL;
		}
		ret = file->f_op->unlocked_ioctl(file, TPD_GET_FILTER_PARA,
					   (unsigned long)arg32);
		if (ret) {
			pr_info("TPD_GET_FILTER_PARA unlocked_ioctl failed.");
			return ret;
		}
		break;
	default:
		pr_info("tpd: unknown IOCTL: 0x%08x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
static long tpd_unlocked_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	/* char strbuf[256]; */
	void __user *data;

	long err = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	if (_IOC_DIR(cmd) & _IOC_READ)
			err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
			err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
#else
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
			(void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
#endif
	if (err) {
		pr_info("tpd: access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case TPD_GET_VELOCITY_CUSTOM_X:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd_v_magnify_x, sizeof(tpd_v_magnify_x))) {
			err = -EFAULT;
			break;
		}

		break;

	case TPD_GET_VELOCITY_CUSTOM_Y:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd_v_magnify_y, sizeof(tpd_v_magnify_y))) {
			err = -EFAULT;
			break;
		}

		break;
	case TPD_GET_FILTER_PARA:
			data = (void __user *) arg;

			if (data == NULL) {
				err = -EINVAL;
				TPD_DMESG("GET_FILTER_PARA: data is null\n");
				break;
			}

			if (copy_to_user(data, &tpd_filter,
					sizeof(struct tpd_filter_t))) {
				TPD_DMESG("GET_FILTER_PARA: copy data error\n");
				err = -EFAULT;
				break;
			}
			break;
	default:
		pr_info("tpd: unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}
static struct work_struct touch_resume_work;
static struct workqueue_struct *touch_resume_workqueue;
static const struct file_operations tpd_fops = {
/* .owner = THIS_MODULE, */
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tpd_compat_ioctl,
#endif
};

/*---------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};

/* ********************************************** */
/* #endif */


/* function definitions */
static int __init tpd_device_init(void);
static void __exit tpd_device_exit(void);
static int tpd_probe(struct platform_device *pdev);
static int tpd_remove(struct platform_device *pdev);
static struct work_struct tpd_init_work;
static struct workqueue_struct *tpd_init_workqueue;
static int tpd_suspend_flag;
int tpd_register_flag;
/* global variable definitions */
struct tpd_device *tpd;
static struct tpd_driver_t tpd_driver_list[TP_DRV_MAX_COUNT];	/* = {0}; */

struct platform_device tpd_device = {
	.name		= TPD_DEVICE,
	.id			= -1,
};
const struct dev_pm_ops tpd_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
};
static struct platform_driver tpd_driver = {
	.remove = tpd_remove,
	.shutdown = NULL,
	.probe = tpd_probe,
	.driver = {
			.name = TPD_DEVICE,
			.pm = &tpd_pm_ops,
			.owner = THIS_MODULE,
			.of_match_table = touch_of_match,
	},
};
static struct tpd_driver_t *g_tpd_drv;
/* hh: use fb_notifier */
static struct notifier_block tpd_fb_notifier;
/* use fb_notifier */
static void touch_resume_workqueue_callback(struct work_struct *work)
{
	TPD_DEBUG("GTP %s\n", __func__);
	g_tpd_drv->resume(NULL);
	tpd_suspend_flag = 0;
}

static int tpd_fb_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *data)
{
#if !IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	struct fb_event *evdata = NULL;
#endif
	int blank;
	int err = 0;

	TPD_DEBUG("%s\n", __func__);

#if !IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	evdata = data;
#endif
	/* If we aren't interested in this event, skip it immediately ... */
#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	if (event != MTK_DISP_EVENT_BLANK)
		return 0;
#else
	if (event != FB_EVENT_BLANK)
		return 0;
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	blank = *(int *)data;
#else
	blank = *(int *)evdata->data;
#endif

	TPD_DMESG("fb_notify(blank=%d)\n", blank);
	
	switch (blank) {
	case FB_BLANK_UNBLANK:
		TPD_DMESG("LCD ON Notify\n");
		if (g_tpd_drv && tpd_suspend_flag) {
			err = queue_work(touch_resume_workqueue,
						&touch_resume_work);
			if (!err) {
				TPD_DMESG("start resume_workqueue failed\n");
				return err;
			}
		}
		break;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	case MTK_DISP_BLANK_POWERDOWN:
#else
	case FB_BLANK_POWERDOWN:
#endif
		TPD_DMESG("LCD OFF Notify\n");
		if (g_tpd_drv && !tpd_suspend_flag) {
			err = cancel_work_sync(&touch_resume_work);
			if (!err)
				TPD_DMESG("cancel resume_workqueue failed\n");
			g_tpd_drv->suspend(NULL);
		}
		tpd_suspend_flag = 1;
		break;
	default:
		break;
	}
	return 0;
}
/* Add driver: if find TPD_TYPE_CAPACITIVE driver successfully, loading it */
int tpd_driver_add(struct tpd_driver_t *tpd_drv)
{
	int i;

	if (g_tpd_drv != NULL) {
		TPD_DMESG("touch driver exist\n");
		return -1;
	}
	/* check parameter */
	if (tpd_drv == NULL)
		return -1;
	tpd_drv->tpd_have_button = tpd_dts_data.use_tpd_button;
	/* R-touch */
	if (strcmp(tpd_drv->tpd_device_name, "generic") == 0) {
		tpd_driver_list[0].tpd_device_name = tpd_drv->tpd_device_name;
		tpd_driver_list[0].tpd_local_init = tpd_drv->tpd_local_init;
		tpd_driver_list[0].suspend = tpd_drv->suspend;
		tpd_driver_list[0].resume = tpd_drv->resume;
		tpd_driver_list[0].tpd_have_button = tpd_drv->tpd_have_button;
		return 0;
	}
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name == NULL) {
			tpd_driver_list[i].tpd_device_name =
				tpd_drv->tpd_device_name;
			tpd_driver_list[i].tpd_local_init =
				tpd_drv->tpd_local_init;
			tpd_driver_list[i].suspend = tpd_drv->suspend;
			tpd_driver_list[i].resume = tpd_drv->resume;
			tpd_driver_list[i].tpd_have_button =
				tpd_drv->tpd_have_button;
			tpd_driver_list[i].attrs = tpd_drv->attrs;
			break;
		}
		if (strcmp(tpd_driver_list[i].tpd_device_name,
			tpd_drv->tpd_device_name) == 0)
			return 1;	/* driver exist */
	}

	return 0;
}

int tpd_driver_remove(struct tpd_driver_t *tpd_drv)
{
	int i = 0;
	/* check parameter */
	if (tpd_drv == NULL)
		return -1;
	for (i = 0; i < TP_DRV_MAX_COUNT; i++) {
		/* find it */
		if (strcmp(tpd_driver_list[i].tpd_device_name,
				tpd_drv->tpd_device_name) == 0) {
			memset(&tpd_driver_list[i], 0,
				sizeof(struct tpd_driver_t));
			break;
		}
	}
	return 0;
}

static void tpd_create_attributes(struct device *dev, struct tpd_attrs *attrs)
{
	int num = attrs->num;

	for (; num > 0;) {
		if (device_create_file(dev, attrs->attr[--num]))
			pr_info("mtk_tpd: tpd create attributes file failed\n");
	}
}

#if IS_ENABLED(CONFIG_CM_HARDWAREINFO_SUPPORT) && IS_ENABLED(CONFIG_MID_CSCI_SUPPORT)
#include <mt-plat/csci.h>

static char tpd_fw_name[64] = {"no found"};

struct tpd_ic_fw {
	char *ic_name;
	char *csci_fw_name;
};

static struct tpd_ic_fw wb_tpd_info[] = {
	{"gslX680", "touchpanel.gsl.firmware"},
	{"gt9xx", "touchpanel.gtxx.firmware"},
	{"GT9XX", "touchpanel.gtxx.firmware"},
	{"None", "None"},
};

static void tpd_update_ic_fw_name(char *ic_name) 
{
	int i;
	memset(tpd_fw_name,0x00,sizeof(tpd_fw_name)/sizeof(char));
	
	for (i = 0; i < sizeof(wb_tpd_info)/sizeof(struct tpd_ic_fw); i++) 
	{
		if (!strcmp(ic_name, wb_tpd_info[i].ic_name)
			  || !strcmp("None", wb_tpd_info[i].ic_name)) {
				  
			if(csci_exist(wb_tpd_info[i].csci_fw_name)) {
				csci_string(tpd_fw_name, wb_tpd_info[i].csci_fw_name, 0);
			} else {
				sprintf(tpd_fw_name,"%s","no found!");
			}
			break;
		}
	}
}
#endif

/* touch panel probe */
static int tpd_probe(struct platform_device *pdev)
{
	int touch_type = 1;	/* 0:R-touch, 1: Cap-touch */
	int i = 0;
	#if 0//IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20211210
	int phy_orien;
	#endif

	TPD_DMESG("enter %s, %d\n", __func__, __LINE__);

	if (misc_register(&tpd_misc_device))
		pr_info("mtk_tpd: tpd_misc_device register failed\n");
	tpd_get_gpio_info(pdev);
	tpd = kmalloc(sizeof(struct tpd_device), GFP_KERNEL);
	if (tpd == NULL)
		return -ENOMEM;
	memset(tpd, 0, sizeof(struct tpd_device));

	/* allocate input device */
	tpd->dev = input_allocate_device();
	if (tpd->dev == NULL) {
		kfree(tpd);
		return -ENOMEM;
	}

	tpd_mode = TPD_MODE_NORMAL;
	tpd_mode_axis = 0;
	tpd_mode_min = tpd_dts_data.tpd_resolution[1] / 2;
	tpd_mode_max = tpd_dts_data.tpd_resolution[1];
	tpd_mode_keypad_tolerance = tpd_dts_data.tpd_resolution[0] *
		tpd_dts_data.tpd_resolution[0] / 1600;
	/* save dev for regulator_get() before tpd_local_init() */
	tpd->tpd_dev = &pdev->dev;
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name != NULL) {
			tpd_driver_list[i].tpd_local_init();
			/* msleep(1); */
			if (tpd_load_status == 1) {
				#if IS_ENABLED(CONFIG_CM_HARDWAREINFO_SUPPORT)//Leo 20210827
				{
					char name[128];
					extern void Hwinfo_update_info_cust(int hw_type, char *name);
					#define HW_TYPE_TP    2
					#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT)
					tpd_update_ic_fw_name(tpd_driver_list[i].tpd_device_name);
					#endif
					sprintf(name,"%s Fw: %s",tpd_driver_list[i].tpd_device_name, tpd_fw_name);
					Hwinfo_update_info_cust(HW_TYPE_TP, name);
				}
				#endif
				TPD_DMESG("%s, tpd_driver_name=%s\n", __func__,
					  tpd_driver_list[i].tpd_device_name);
				g_tpd_drv = &tpd_driver_list[i];
				break;
			}
		}
	}
	if (g_tpd_drv == NULL) {
		if (tpd_driver_list[0].tpd_device_name != NULL) {
			g_tpd_drv = &tpd_driver_list[0];
			/* touch_type:0: r-touch, 1: C-touch */
			touch_type = 0;
			g_tpd_drv->tpd_local_init();
			TPD_DMESG("Generic touch panel driver\n");
		} else {
			TPD_DMESG("no touch driver is loaded!!\n");
			return 0;
		}
	}
	touch_resume_workqueue = create_singlethread_workqueue("touch_resume");
	INIT_WORK(&touch_resume_work, touch_resume_workqueue_callback);
	/* use fb_notifier */
	tpd_fb_notifier.notifier_call = tpd_fb_notifier_callback;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	if (mtk_disp_notifier_register("Touch", &tpd_fb_notifier))
		TPD_DMESG("register mtk_disp_notifier_register fail!\n");
#else
	if (fb_register_client(&tpd_fb_notifier))
		TPD_DMESG("register fb_notifier fail!\n");
#endif
	/* TPD_TYPE_CAPACITIVE handle */
	if (touch_type == 1) {
		/* struct input_dev dev initialization and registration */
		tpd->dev->name = TPD_INPUT_DEVICE;
		set_bit(EV_SYN, tpd->dev->evbit);
		set_bit(EV_ABS, tpd->dev->evbit);
		set_bit(EV_KEY, tpd->dev->evbit);
		set_bit(BTN_TOUCH, tpd->dev->keybit);
		set_bit(BTN_TOOL_FINGER, tpd->dev->keybit);
		set_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MINOR, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
		set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);
		input_set_abs_params(tpd->dev,
			ABS_MT_POSITION_X, 0,
			tpd_dts_data.tpd_resolution[0], 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_POSITION_Y, 0,
			tpd_dts_data.tpd_resolution[1], 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
		TPD_DEBUG("Cap touch panel driver\n");
	}

	if (input_register_device(tpd->dev))
		TPD_DMESG("input_register_device failed.(tpd)\n");
	else
		tpd_register_flag = 1;
	if (g_tpd_drv->tpd_have_button)
		tpd_button_init();

	if (g_tpd_drv->attrs.num)
		tpd_create_attributes(&pdev->dev, &g_tpd_drv->attrs);

	return 0;
}
static int tpd_remove(struct platform_device *pdev)
{
	input_unregister_device(tpd->dev);
	return 0;
}

/* called when loaded into kernel */
static void tpd_init_work_callback(struct work_struct *work)
{
	TPD_DEBUG("MediaTek touch panel driver init\n");
	if (platform_driver_register(&tpd_driver) != 0)
		TPD_DMESG("unable to register touch panel driver.\n");
}
#if IS_ENABLED(CONFIG_TOUCHSCREEN_MTK_GT9XX)
extern int tpd_driver_init_GT9XX(void);
extern void tpd_driver_exit_GT9XX(void);
#endif

static int __init tpd_device_init(void)
{
	int res = 0;
	#if IS_ENABLED(CONFIG_TOUCHSCREEN_MTK_GT9XX)
	tpd_driver_init_GT9XX();
	#endif

	tpd_init_workqueue = create_singlethread_workqueue("mtk-tpd");
	INIT_WORK(&tpd_init_work, tpd_init_work_callback);

	res = queue_work(tpd_init_workqueue, &tpd_init_work);
	if (!res)
		pr_info("tpd : touch device init failed res:%d\n", res);
	return 0;
}
/* should never be called */
static void __exit tpd_device_exit(void)
{
	TPD_DMESG("MediaTek touch panel driver exit\n");
	#if IS_ENABLED(CONFIG_TOUCHSCREEN_MTK_GT9XX)
	tpd_driver_exit_GT9XX();
	#endif

	misc_deregister(&tpd_misc_device);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK) //Leo 20220505
	mtk_disp_notifier_unregister(&tpd_fb_notifier);
#else
	fb_unregister_client(&tpd_fb_notifier);
#endif
	//gt1x_driver_exit();
	cancel_work_sync(&tpd_init_work);
	destroy_workqueue(tpd_init_workqueue);
	platform_driver_unregister(&tpd_driver);
	TPD_DEBUG("Touch exit done\n");
}

late_initcall(tpd_device_init);
module_exit(tpd_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
