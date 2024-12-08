#include "tpd.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fb.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include  "include/gtxx_config.h"
#include "tp_list_gt9xx.h"

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include <mt-plat/csci.h>

char gtxx_name_tp_mode[64];

struct gtxx_tp_coordinate_info {
	int sf_hwrotation;
	int xy_swap;
	int x_reverse;
	int y_reverse;
	int xy_deal;
	int xy_deal_minor;
};

struct gtxx_tp_list_info {
	int project_data_index;
	int tp_info_index;
	int gtxx_tp_is_vaild;
};

struct gtxx_tp_coordinate_info gtxx_coordinate;
struct gtxx_tp_list_info gtxx_list;

#define GTXX_DEBUG

#ifdef GTXX_DEBUG 
#define print_info(fmt, args...)   \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_info(fmt, args...)
#endif


#define STRSPLICE(dest, a, b, c) \
	do{ \
		strcpy(dest, a); \
		strcat(dest, b); \
		strcat(dest, c); \
	} while(0)

int gtxx_get_tp_info_index(void) 
{
	return gtxx_list.tp_info_index;
}

int gtxx_get_project_data_index(void)
{
	return gtxx_list.project_data_index;
}

int gtxx_get_tp_is_vaild(void)
{
	return gtxx_list.gtxx_tp_is_vaild;
}

int gtxx_control_input_orientation(int *x, int *y, int screenwidth, int screenheight)
{
	int tpd_x_res=0, tpd_y_res=0;
	int z;

	if(90 == gtxx_coordinate.sf_hwrotation || 270 == gtxx_coordinate.sf_hwrotation)
	{
		tpd_y_res = screenwidth;
		tpd_x_res = screenheight;
	} else {
		tpd_x_res = screenwidth;
		tpd_y_res = screenheight;
	}

	if(1 == gtxx_coordinate.xy_swap)
	{	
		z=*y; *y=*x; *x=z;
	}

	if(1 == gtxx_coordinate.x_reverse)
		*x=tpd_x_res-*x;

	if(1 == gtxx_coordinate.y_reverse)	
		*y=tpd_y_res-*y;

	if(1 == gtxx_coordinate.xy_deal)
	{
		*x=(*x)*tpd_y_res/tpd_x_res;
		*y=(*y)*tpd_x_res/tpd_y_res; 
	}

	if(1 == gtxx_coordinate.xy_deal_minor)
	{
		*x=(*x)*tpd_x_res/tpd_y_res;
		*y=(*y)*tpd_y_res/tpd_x_res; 
	}

	return 0;
}

int gtxx_tp_list_config(void)
{
	int i;

	for (i = 0; i < sizeof(gtxx_tp_info) / sizeof(ALL_TP_INFO_GT9XX); i++){
		if(0 == strncmp(gtxx_name_tp_mode, gtxx_tp_info[i].name, strlen(gtxx_tp_info[i].name))) {
			gtxx_list.tp_info_index = i;
			break;
		}
	}

	print_info("\n[%s] gtxx_name_tp_mode =%s, gtxx_tp_info[%d].name = %s\n",
										__func__, gtxx_name_tp_mode, i, gtxx_tp_info[i].name);

	if (i == sizeof(gtxx_tp_info) / sizeof(ALL_TP_INFO_GT9XX)) {
		print_info("\n[%s] list of designated tp isn't exist gtxx_name_tp_mode =%s\n",__func__, gtxx_name_tp_mode);
		gtxx_list.tp_info_index = 0;
		gtxx_list.project_data_index = 0;
		gtxx_list.gtxx_tp_is_vaild = 1;
		return -1;
	}

	if(gtxx_tp_info[gtxx_list.tp_info_index].tp_num > 1) {
		for(i=0; i < gtxx_tp_info[gtxx_list.tp_info_index].tp_num; i++) { 
			if (0 == strcmp(gtxx_name_tp_mode, gtxx_tp_info[gtxx_list.tp_info_index].pro_tp_data[i].name)) { 
				gtxx_list.project_data_index = i; 
				break; 
			}
		}
		
		if(i == gtxx_tp_info[gtxx_list.tp_info_index].tp_num) {
			gtxx_list.project_data_index = i - 1;
		}
	} else {
		gtxx_list.project_data_index = 0;
	}
	
	gtxx_list.gtxx_tp_is_vaild = 1;

	return 0;
}

int tp_gtxx_model_get(void)
{
	if(csci_exist("touchpanel.gtxx.firmware"))
		csci_string(gtxx_name_tp_mode,"touchpanel.gtxx.firmware",0);
	else {
		print_info("[%s] <touchpanel.gtxx.firmware> not exist in csci.ini.\n", __func__);
		strcpy(gtxx_name_tp_mode, gtxx_tp_info[0].pro_tp_data[0].name); //Leo add for default 20220111
		return -1;
	}

	return 0;
}

int gtxx_tp_input_orientation_config(void)
{

	if(csci_exist("ro.vendor.sf.hwrotation"))
		gtxx_coordinate.sf_hwrotation = csci_integer("ro.vendor.sf.hwrotation",0);
	else
		gtxx_coordinate.sf_hwrotation = -1;

	if(csci_exist("touchpanel.gsl.xy.swap"))
		gtxx_coordinate.xy_swap = csci_integer("touchpanel.gsl.xy.swap",0);
	else
		gtxx_coordinate.xy_swap = -1;

	if(csci_exist("touchpanel.gsl.x.reverse"))
		gtxx_coordinate.x_reverse = csci_integer("touchpanel.gsl.x.reverse",0);
	else
		gtxx_coordinate.x_reverse = -1;

	if(csci_exist("touchpanel.gsl.y.reverse"))
		gtxx_coordinate.y_reverse = csci_integer("touchpanel.gsl.y.reverse",0);
	else
		gtxx_coordinate.y_reverse = -1;

	if(csci_exist("touchpanel.gsl.xy.deal"))
		gtxx_coordinate.xy_deal = csci_integer("touchpanel.gsl.xy.deal",0);
	else
		gtxx_coordinate.xy_deal = -1;

	if(csci_exist("touchpanel.gsl.xy.deal.minor"))
		gtxx_coordinate.xy_deal_minor = csci_integer("touchpanel.gsl.xy.deal.minor",0);
	else
		gtxx_coordinate.xy_deal_minor = -1;

	return 0;
}


int gtxx_config_tp(void)
{
	print_info("[%s] in.\n", __func__);

	tp_gtxx_model_get();

	gtxx_tp_input_orientation_config();

	return 0;
}

