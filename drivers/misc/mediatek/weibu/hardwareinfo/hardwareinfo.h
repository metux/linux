#ifndef __HARDWAREINFO_H_
#define __HARDWAREINFO_H_

#ifndef CONFIG_MTK_PLATFORM
#define CONFIG_MTK_PLATFORM  "hwinfo"
#endif

#define HW_MAX_NAME_SIZE	256


enum HW_TYPE {
	HW_TYPE_NONE = -1,
	HW_TYPE_ZERO = 0,
	HW_TYPE_LCM,
	HW_TYPE_TP,
	HW_TYPE_MAIN_CAM,
	HW_TYPE_SUB_CAM,
	HW_TYPE_MAIN2_CAM,
	HW_TYPE_MAIN3_CAM,
	HW_TYPE_GSENSOR, //7
	HW_TYPE_MSENSOR,
	HW_TYPE_GYROSENSOR,
	HW_TYPE_ALSPS,
	HW_TYPE_WIFI,
	HW_TYPE_FINGERPRINT,
	HW_TYPE_CURRENT,
#if defined(HW_GITINFO_SUPPORT) //Leo 20221124
	HW_TYPE_GITINFO,
#endif
	HW_TYPE_LCM_COMPAT,
	HW_TYPE_CAM_COMPAT,
	HW_TYPE_DM_VERITY,
	HW_TYPE_CHARGE,
	HW_TYPE_OTP,//19
	HW_TYPE_UFS,
	HW_TYPE_CC,
	HW_TYPE_BOARD_ID,
	HW_TYPE_DUALMIPI_ID,
	HW_TYPE_BARSENSOR,
	HW_TYPE_BUILD,
	HW_TYPE_MAX,
};

struct HW_DEV_NAME {
	char *name;
	char name_arr[HW_MAX_NAME_SIZE];
};

struct hwinfo_device {
	struct device *dev;
	struct device_node *node;
	int board_id;
	int dualmipi_chipid;
};

extern void Hwinfo_update_info_cust(int hw_type, char *name);
extern void Hwinfo_update_info(int hw_type, char *name);
extern void hwinfo_dev_init(void);
extern void hwinfo_camera(void);
extern void hwinfo_current(void);
extern void hwinfo_4in1(void);
#if defined(HW_GITINFO_SUPPORT) //Leo 20221124
extern void hwinfo_gitinfo(void);
#endif
extern void hwinfo_strsep(char *src, char *dst, char *delim_c, int sep_index);
extern struct HW_DEV_NAME hwinfo_dev[HW_TYPE_MAX];
#if defined(CONFIG_SPRD_SENSOR_HUB) //Leo add for sprd sensorHub sensorinfo 20210719
extern void hwinfo_getsensorhub(void);
#endif
#if defined(CONFIG_CUSTOM_KERNEL_LCM) 
extern void hwinfo_lcm_compat(void);
#endif
#if defined(CONFIG_CUSTOM_KERNEL_IMGSENSOR) 
extern void hwinfo_cam_compat(void);
#endif
#if IS_ENABLED(CONFIG_WB_BOARD_ID_SUPPORT) //Leo 20230417
int hwinfo_get_board_id(struct hwinfo_device *hwinfo);
#endif
#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
int hwinfo_get_dualmipi_chip_id(struct hwinfo_device *hwinfo);
#endif
#if IS_ENABLED(CONFIG_MTK_CHARGER) //Leo 20240122
void hwinfo_get_charger(void);
#endif
#endif
