/* this file function is display all devices name */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
//#include <linux/mtd/mtd.h>
//#include <linux/mtd/nand.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/of.h>

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230712
#include <mt-plat/csci.h>
#if !IS_ENABLED(CONFIG_WB_TYPEC_EARPIECE) //Leo 20230110
#define CONFIG_WB_TYPEC_EARPIECE
#endif
#endif

#include "hardwareinfo.h"

#define MID_MISC_DEVICE		"mid_misc"
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

struct hwinfo_device *hwinfo;
struct HW_DEV_NAME hwinfo_dev[HW_TYPE_MAX];
struct HW_DEV_NAME hwinfo_dev_no[HW_TYPE_MAX] = {
	[HW_TYPE_ZERO]		= {"Begin: "},
	[HW_TYPE_LCM]		 = {"LCM: "},
	[HW_TYPE_TP]		  = {"TP: "},
	[HW_TYPE_MAIN_CAM]	= {"MAIN_CAM: "},
	[HW_TYPE_SUB_CAM]	 = {"SUB_CAM: "},
	[HW_TYPE_MAIN2_CAM]   = {"MAIN2_CAM: "},
	[HW_TYPE_MAIN3_CAM]   = {"MAIN3_CAM: "},
	[HW_TYPE_GSENSOR]	 = {"Gsenosr: "},
	[HW_TYPE_MSENSOR]	 = {"Msenosr: "},
	[HW_TYPE_GYROSENSOR]  = {"Gyrosenosr: "},
	[HW_TYPE_ALSPS]	   = {"Alsps: "},
	[HW_TYPE_WIFI]		= {"Wifi/BT/GPS/FM: "},
	[HW_TYPE_FINGERPRINT] = {"Fingerprint: "},
	[HW_TYPE_CURRENT]	 = {"Current: "},
#if defined(HW_GITINFO_SUPPORT) //Leo 20221124
	[HW_TYPE_GITINFO]	 = {"GitInfo: "},
#endif
	[HW_TYPE_LCM_COMPAT]  = {"LCM_COMPAT: "},
	[HW_TYPE_CAM_COMPAT]  = {"CAM_COMPAT: "},
	[HW_TYPE_DM_VERITY]   = {"dm_verity: "},
	[HW_TYPE_CHARGE]	  = {"Charge: "},
	[HW_TYPE_OTP]	  = {"Otp Info: "},
	[HW_TYPE_UFS]	  = {"UFS_INFO: "},
	[HW_TYPE_CC]	  = {"CC: "},
	[HW_TYPE_BOARD_ID]	  = {"Board_ID: "},
	[HW_TYPE_DUALMIPI_ID]	  = {"Dualmipi_Chipid: "},
	[HW_TYPE_BARSENSOR]     = {"Barsensor: "},
	[HW_TYPE_BUILD]     = {"Build: "},
};

void hwinfo_dev_init(void)
{
	int i;
	for (i = HW_TYPE_ZERO; i < HW_TYPE_MAX; i++) {
		hwinfo_dev[i].name = NULL;
		memset(hwinfo_dev[i].name_arr,0x00,HW_MAX_NAME_SIZE*sizeof(char));
	}
}

static ssize_t show_hwinfo(struct device *dev,struct device_attribute *attr, char *buf)
{
	int i;
	int len = 0;
	
	hwinfo_camera();
	hwinfo_current();
	hwinfo_4in1();
#if defined(HW_GITINFO_SUPPORT) //Leo 20221124
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)) //Leo add for gki reboot 20220907
	hwinfo_gitinfo();
#endif
#endif
	#if defined(CONFIG_SPRD_SENSOR_HUB) //Leo 20210719
	hwinfo_getsensorhub();
	#endif
	#if defined(CONFIG_CUSTOM_KERNEL_LCM) 
	hwinfo_lcm_compat();
	#endif
	#if defined(CONFIG_CUSTOM_KERNEL_IMGSENSOR) 
	hwinfo_cam_compat();
	#endif
	#if IS_ENABLED(CONFIG_MTK_CHARGER)
	hwinfo_get_charger();
	#endif

	#ifdef CUST_BUILD_PROJECT
	Hwinfo_update_info_cust(HW_TYPE_BUILD,CUST_BUILD_PROJECT);
	#endif

	len += snprintf(buf+len, PAGE_SIZE-len, "\r\n"); 
	for (i = HW_TYPE_LCM; i < HW_TYPE_MAX; i++) {
		if (hwinfo_dev[i].name) {
			len += snprintf(buf + len, PAGE_SIZE-len, "%s %s\n",
							hwinfo_dev_no[i].name, hwinfo_dev[i].name);
		} else {
			len += snprintf(buf + len, PAGE_SIZE-len, "%s no found!\n",hwinfo_dev_no[i].name);
		}
	}
	
	return len;
}

static ssize_t store_hwinfo(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	int str_shift = 4;
	int hw_type = 0;
	char temp[2];
	
	if (count > 2) {
		strncpy(temp, buf, sizeof(char)*2);
		sscanf(temp, "%d", &hw_type);
		if (hw_type != 0) {
			strcpy(hwinfo_dev[hw_type].name_arr, buf+str_shift);
			Hwinfo_update_info(hw_type, hwinfo_dev[hw_type].name_arr);
		}
	}

	pr_info("store_hwinfo: buf[0]:0x%x "
				"buf[1]:0x%x hw_type:%d bufstr:%s \n", buf[0], buf[1], hw_type, buf);

	return count;
};

#if IS_ENABLED(CONFIG_WB_TYPEC_EARPIECE) || defined(CONFIG_WB_TYPEC_EARPIECE) //Leo 20230621
static ssize_t store_typec_headset(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count)
{
	return count;
}
static ssize_t show_typec_headset(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE-len, "%d\n",1);

	return len;
}
#endif


static DEVICE_ATTR(hwinfo, 0664, show_hwinfo, store_hwinfo);
#if IS_ENABLED(CONFIG_WB_TYPEC_EARPIECE) || defined(CONFIG_WB_TYPEC_EARPIECE) //Leo 20230621
static DEVICE_ATTR(typec_headset, 0664, show_typec_headset, store_typec_headset);
#endif

void Hwinfo_update_info(int hw_type, char *name)
{
	if ((hw_type > HW_TYPE_MAX) || (hw_type <= HW_TYPE_ZERO)) {
		pr_err("Hwinfo_update_info :%s hw_type is wrong !\n");
		return;
	}
	
	if (name) {
		hwinfo_dev[hw_type].name = name;
	} 
}
EXPORT_SYMBOL(Hwinfo_update_info);

void Hwinfo_update_info_arr(int hw_type, char *name)
{
	if ((hw_type > HW_TYPE_MAX) || (hw_type <= HW_TYPE_ZERO)) {
		pr_err("Hwinfo_update_info :%s hw_type is wrong !\n");
		return;
	}
	
	if (name) {
		strcpy(hwinfo_dev[hw_type].name_arr, name);
	} 
}
EXPORT_SYMBOL(Hwinfo_update_info_arr);

void Hwinfo_get_info_arr(int hw_type, char **arr)
{
	if ((hw_type > HW_TYPE_MAX) || (hw_type <= HW_TYPE_ZERO)) {
		pr_err("Hwinfo_update_info :%s hw_type is wrong !\n");
		return;
	}
		
	*arr = hwinfo_dev[hw_type].name_arr;
}
EXPORT_SYMBOL(Hwinfo_get_info_arr);

void Hwinfo_update_info_cust(int hw_type, char *name)
{
	
	char *arr = NULL;
	
	if (strlen(name) >= HW_MAX_NAME_SIZE) {
		pr_info("%s failed:name size large then %d",__func__,HW_MAX_NAME_SIZE);
		return;
	}

	Hwinfo_get_info_arr(hw_type, &arr);
	strcpy(arr, name);
	Hwinfo_update_info(hw_type, arr);
}
EXPORT_SYMBOL(Hwinfo_update_info_cust);

static int HardwareInfo_driver_probe(struct platform_device *dev)
{	
	int ret_device_file = 0;
	const char *name;
	char temp[128];
	uint32_t wb_dm_verity_status = 0;
	
	pr_info("*** HardwareInfo_driver_probe!!! ***\n" );

	if((ret_device_file = 
		device_create_file(&(dev->dev), &dev_attr_hwinfo)) != 0) goto exit_error;

	hwinfo = (struct hwinfo_device *)kzalloc(sizeof(struct hwinfo_device), GFP_KERNEL);
	if (hwinfo == NULL) {
		pr_info("failed to allocated memoryhwinfo\n");
		return -ENOMEM;
	}

	hwinfo->node = of_find_compatible_node(NULL, NULL, "cust,hwinfo");
	if (hwinfo->node) {
		of_property_read_string(hwinfo->node, "ufs_id", &name);
		if (strlen(name) != 0) {
			sprintf(temp,"%s",name);
			Hwinfo_update_info_cust(HW_TYPE_UFS, temp);
		} else {
			pr_err("ufs_id node no found !\n");
		}
	} else {
		pr_err("cust,hwinfo node no found !\n");
	}
//jnier add >>>>
	/*
	wb_dm_verity_status == 1    user版本开机出现dm_verity error提示，LK软件上跳过异常
	wb_dm_verity_status == 0    user版本开机没有dm_verity error提示
	*/
    of_property_read_u32(hwinfo->node,"wb_dm_verity_status", &wb_dm_verity_status);
	if(wb_dm_verity_status == 1) {
		Hwinfo_update_info_cust(HW_TYPE_DM_VERITY, " ture");	
	}else if(wb_dm_verity_status == 0){
		Hwinfo_update_info_cust(HW_TYPE_DM_VERITY, " false");		
	}
//jnier add <<<<

#if IS_ENABLED(CONFIG_ACER_CUST_SUPPORT) //Leo 20221130
	Hwinfo_update_info_cust(HW_TYPE_TP, "gslX680_0x0A4D2D63");
#endif

#if IS_ENABLED(CONFIG_WB_TYPEC_EARPIECE) || defined(CONFIG_WB_TYPEC_EARPIECE) //Leo 20230621
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230712
	if (csci_exist("accdet.typecearpiece.disable")) { //just for debug
		int temp = 0;
		temp = csci_integer("accdet.typecearpiece.disable",0);
		if (temp == 0) {
			if((ret_device_file = 
				device_create_file(&(dev->dev), &dev_attr_typec_headset)) != 0) goto exit_error;
		}
	} else {
#if IS_ENABLED(CONFIG_WB_TYPEC_EARPIECE) //Leo 20230621
		if((ret_device_file = 
			device_create_file(&(dev->dev), &dev_attr_typec_headset)) != 0) goto exit_error;
#endif
	}
#else /*IS_ENABLED(CONFIG_MID_CSCI_SUPPORT)*/
	if((ret_device_file = 
		device_create_file(&(dev->dev), &dev_attr_typec_headset)) != 0) goto exit_error;
#endif
#endif

#if IS_ENABLED(CONFIG_WB_BOARD_ID_SUPPORT) //Leo 20230417
	hwinfo_get_board_id(hwinfo);
#endif

#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
	hwinfo_get_dualmipi_chip_id(hwinfo);
#endif

exit_error:	

	return ret_device_file;
}

static int HardwareInfo_driver_remove(struct platform_device *dev)
{
	printk("*** HardwareInfo_drvier_remove!!! ***");
	
	device_remove_file(&(dev->dev), &dev_attr_hwinfo);

#if IS_ENABLED(CONFIG_WB_TYPEC_EARPIECE) || defined(CONFIG_WB_TYPEC_EARPIECE) //Leo 20230621
	device_remove_file(&(dev->dev), &dev_attr_typec_headset);
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mid_misc_of_ids[] = {
	{.compatible = "cust,HardwareInfo",},
	{},
};
MODULE_DEVICE_TABLE(of, mid_misc_of_ids);
#endif


static struct platform_driver HardwareInfo_driver = {
	.probe	= HardwareInfo_driver_probe,
	.remove = HardwareInfo_driver_remove,
	.driver = {
		.name = "HardwareInfo",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(mid_misc_of_ids),
#endif
	},
};

#ifndef CONFIG_OF
static struct platform_device HardwareInfo_device = {
	.name   = "HardwareInfo",
	.id		= -1,
};
#endif

static int __init HardwareInfo_mod_init(void)
{
	int ret = 0;

#ifndef CONFIG_OF
	ret = platform_device_register(&HardwareInfo_device);
	if (ret) {
		pr_info("*** HardwareInfo_mod_init  Unable to driver register(%d)!!!\n", ret);
		goto fail_2;
	} 
#endif

	ret = platform_driver_register(&HardwareInfo_driver);
	if (ret) {
		pr_info("*** HardwareInfo_mod_init  Unable to driver register(%d)!!!\n", ret);
		goto fail_1;
	}

	goto ok_result;

fail_1:
	platform_driver_unregister(&HardwareInfo_driver);

#ifndef CONFIG_OF
fail_2:
	platform_device_unregister(&HardwareInfo_device);
#endif

ok_result:

	return ret;
}

static void __exit HardwareInfo_mod_exit(void)
{
	#if defined(CONFIG_MTK_PLATFORM) || defined(CONFIG_ARCH_MTK_PROJECT)
	hwinfo_dev_init();
	#endif
	
	platform_driver_unregister(&HardwareInfo_driver);
#ifndef CONFIG_OF
	platform_device_unregister(&HardwareInfo_device);
#endif
}

/*****************************************************************************/
module_init(HardwareInfo_mod_init);
module_exit(HardwareInfo_mod_exit);
/*****************************************************************************/

MODULE_AUTHOR("<hello@world.com>");
MODULE_DESCRIPTION("MTK Hareware Info driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);