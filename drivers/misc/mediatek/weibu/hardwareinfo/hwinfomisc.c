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
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/version.h>
#include <linux/of.h>
#include "hardwareinfo.h"

#if IS_ENABLED(CONFIG_MTK_CHARGER) //Leo 20240122
#include "charger_class.h"
#include "mtk_charger.h"
#endif


#define GIT_INFO "/vendor/etc/gitinfo"
#define TOSTRING(value)           #value
#define STRINGIZE(stringizedName) TOSTRING(stringizedName)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)) //Leo add for gki reboot 20220907
static int hwinfo_fread(char *path,char *buf, unsigned long size)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;

	fp = filp_open(path, O_RDONLY, 0644);
	if (IS_ERR(fp)) {
		pr_err("create file error\n");
		return -1;
	}
	
	fs = get_fs();
	set_fs(KERNEL_DS);
	
	pos = 0;
	kernel_read(fp, buf, size, &pos);
	pr_info("hwinfo read: %s\n", buf);
	
	set_fs(fs);
	filp_close(fp, NULL);
	return 0;
}

void hwinfo_gitinfo(void)
{
	//static char curr_buf[20] = {0};
	char temp[50];          
	memset(temp,0,sizeof(temp));
	
	hwinfo_fread(GIT_INFO, temp, strlen("ap:cabe9e37b5 diff:77ebafc"));
	sprintf(hwinfo_dev[HW_TYPE_GITINFO].name_arr,"%s",temp);
	Hwinfo_update_info(HW_TYPE_GITINFO, hwinfo_dev[HW_TYPE_GITINFO].name_arr);
}
#endif

#if defined(CONFIG_MTK_PLATFORM) | defined(CONFIG_ARCH_MTK_PROJECT) //Leo 20210827

void hwinfo_camera(void)
{
	pr_info("leo \n");
}

void hwinfo_current(void)
{
	pr_info("leo \n");
}

void hwinfo_4in1(void)
{
#if defined(CONFIG_MTK_FM_CHIP) 
	char name[10] = {0};
	int str_shift = 1;
	char *temp=STRINGIZE(CONFIG_MTK_FM_CHIP);
	strncpy(name, temp, strlen("MT6631") + str_shift);
	Hwinfo_update_info_cust(HW_TYPE_WIFI, name + str_shift);
#else
	char *name=STRINGIZE(CONFIG_MTK_PLATFORM);
	Hwinfo_update_info_cust(HW_TYPE_WIFI, name);
#endif
}

#if defined(CONFIG_CUSTOM_KERNEL_LCM) 
void hwinfo_lcm_compat(void)
{
	char name[256] = {0};
	int str_shift = 0;
	char *temp=STRINGIZE(CONFIG_CUSTOM_KERNEL_LCM);
	strncpy(name, temp, HW_MAX_NAME_SIZE + str_shift);
	Hwinfo_update_info_cust(HW_TYPE_LCM_COMPAT, name + str_shift);
}
#endif

#if defined(CONFIG_CUSTOM_KERNEL_IMGSENSOR) 
void hwinfo_cam_compat(void)
{
	char name[256] = {0};
	int str_shift = 0;
	char *temp=STRINGIZE(CONFIG_CUSTOM_KERNEL_IMGSENSOR);
	strncpy(name, temp, HW_MAX_NAME_SIZE + str_shift);
	Hwinfo_update_info_cust(HW_TYPE_CAM_COMPAT, name + str_shift);
}
#endif

#if IS_ENABLED(CONFIG_WB_BOARD_ID_SUPPORT) //Leo 20230417
int hwinfo_get_board_id(struct hwinfo_device *hwinfo)
{
	char name[32] = {0};
	struct device_node *pnode = NULL;

	if (hwinfo == NULL) {
		pr_err("%s :hwinfo is NULL \n",__func__);
		return -ENODEV;
	}

	hwinfo->board_id = -1;
	pnode = of_find_compatible_node(NULL, NULL, "cust,mid_misc");
	if (!pnode) {
		pr_err("%s :failed to get pnode\n",__func__);
		return -ENODEV;
	}

	of_property_read_u32(pnode,
		"board_id", &hwinfo->board_id);

	sprintf(name, "%d", hwinfo->board_id);
	Hwinfo_update_info_cust(HW_TYPE_BOARD_ID, name);

	return hwinfo->board_id;
}
#endif

#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
int hwinfo_get_dualmipi_chip_id(struct hwinfo_device *hwinfo)
{
	struct device_node *pnode = NULL;
	char *dualmipi_ic_name[] = {
		"GM8773C",
		"IT6112",
		"IT6113"
	};

	if (hwinfo == NULL) {
		pr_err("%s :hwinfo is NULL \n",__func__);
		return -ENODEV;
	}

	hwinfo->dualmipi_chipid = -1;
	pnode = of_find_compatible_node(NULL, NULL, "cust,mid_misc");
	if (!pnode) {
		pr_err("%s :failed to get pnode\n",__func__);
		return -ENODEV;
	}

	of_property_read_u32(pnode,
		"dualmipi_chipid", &hwinfo->dualmipi_chipid);

	if ((hwinfo->dualmipi_chipid >= 0) 
		&& (hwinfo->dualmipi_chipid < (sizeof(dualmipi_ic_name) / sizeof(char *)))) {
		//sprintf(name, "%s", hwinfo->dualmipi_chipid);
		Hwinfo_update_info_cust(HW_TYPE_DUALMIPI_ID, 
				dualmipi_ic_name[hwinfo->dualmipi_chipid]);
	}

	return hwinfo->dualmipi_chipid;
}
#endif

#if IS_ENABLED(CONFIG_MTK_CHARGER) //Leo 20240122
void hwinfo_get_charger(void)
{
	char name[256] = {0};
	static int str_shift = 0;
	struct charger_device *chg_dev = NULL;

	if (str_shift != 0) {
		pr_err("%s already initd, just return !\n",__func__);
		return;
	}

	chg_dev = get_charger_by_name("primary_chg");
	if (chg_dev != NULL) {
		sprintf(name, "pri: %s",chg_dev->props.alias_name);
		str_shift = strlen(name);
	}

	chg_dev = get_charger_by_name("secondary_chg");
	if (chg_dev != NULL) {
		sprintf(name + str_shift, " sec: %s",chg_dev->props.alias_name);
		str_shift = strlen(chg_dev->props.alias_name);
	}

	if (str_shift != 0) {
		Hwinfo_update_info_cust(HW_TYPE_CHARGE, name);
	}
}
#endif

#else /*CONFIG_MTK_PLATFORM*/

#define CAMERA_INFO "/sys/devices/virtual/misc/sprd_sensor/camera_sensor_name"
#define CURRENT_INFO "/sys/class/power_supply/battery/current_avg"

static int __init hwinfo_lcm_info(char *str)
{
	hwinfo_dev_init();	

	if (!str)
		return 0;
	
	Hwinfo_update_info(HW_TYPE_LCM, str);

	return 0;
}
__setup("lcd_name=", hwinfo_lcm_info);
	
void hwinfo_camera(void)
{
	//static char cam_buf[256] = {0};
	char *str;
	char *p = hwinfo_dev[HW_TYPE_MAIN_CAM].name_arr;
	static int cam_init_flag = 1;
	
	if (cam_init_flag) {
		char delim[2] = "\n";
		cam_init_flag = 0;	
		hwinfo_fread(CAMERA_INFO, hwinfo_dev[HW_TYPE_MAIN_CAM].name_arr,
				sizeof(hwinfo_dev[HW_TYPE_MAIN_CAM].name_arr)/sizeof(char));
		
		str = strsep(&p, delim);
		Hwinfo_update_info(HW_TYPE_MAIN_CAM, str);
		if (str) {
			str = strsep(&p, delim);
			Hwinfo_update_info(HW_TYPE_SUB_CAM, str);
		}		
	}
}

void hwinfo_current(void)
{
	//static char curr_buf[20] = {0};
	char temp[20];
	char delim[3] = "00";
	char *p=temp;
	char *str;                 
	memset(temp,0,sizeof(temp));
	
	hwinfo_fread(CURRENT_INFO, temp, sizeof(char) * 6);
	str = strsep(&p, delim);
	sprintf(hwinfo_dev[HW_TYPE_CURRENT].name_arr,"%s mA",str);
	Hwinfo_update_info(HW_TYPE_CURRENT, hwinfo_dev[HW_TYPE_CURRENT].name_arr);
}

void hwinfo_4in1(void)
{
	
	const char *str = of_flat_dt_get_cpuinfo_hw();
	
	strcpy(hwinfo_dev[HW_TYPE_WIFI].name_arr, str);
	
	Hwinfo_update_info(HW_TYPE_WIFI, 
		hwinfo_dev[HW_TYPE_WIFI].name_arr + strlen("Unisoc "));
}

#if 0
void hwinfo_strsep(char *src, char *dst, char *delim_c, int sep_index)
{
	
	char sep_buf[HW_MAX_NAME_SIZE];
	char delim[20];
	char *p=sep_buf;        
	int index;

	memset(sep_buf, 0x00, HW_MAX_NAME_SIZE);
	strcpy(sep_buf, src);
	strcpy(delim, delim_c);

	for(index = 0; index < sep_index; index++) {
		dst = strsep(&p, delim);	
	};
}

void hwinfo_strsep_set(char *src)
{
	char delim[3] = "##";
	char dst[HW_MAX_NAME_SIZE];
	
	hwinfo_strsep(src, dst, delim, 1);
	
}
#endif /*CONFIG_MTK_PLATFORM*/
#endif 
