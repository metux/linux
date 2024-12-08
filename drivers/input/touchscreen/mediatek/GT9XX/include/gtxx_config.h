#ifndef _GTXX_CONFIG_H__
#define _GTXX_CONFIG_H__

#include <linux/kernel.h>

#define TP_NAME_LEN_GT9XX 64

typedef struct tp_project_data_gt9xx {
   char name[TP_NAME_LEN_GT9XX];
   unsigned char tp_id[4];
   unsigned char *p_gtxx_config_data_id;
   unsigned int tp_data_id_len;
   unsigned char *p_fw;
   int (*tpd_keys)[4];
   unsigned int tp_source_len;
}TP_PROJECT_DATA_GT9XX;


typedef struct all_tp_info_gt9xx {
   char name[TP_NAME_LEN_GT9XX];
   unsigned int tp_gtxx_modle;
   TP_PROJECT_DATA_GT9XX *pro_tp_data;
   int HAVE_KEY;
   unsigned int tp_num;
   int gpio_led_pin;
}ALL_TP_INFO_GT9XX;


int gtxx_tp_list_config(void);
int gtxx_control_input_orientation(int *x, int *y, int screenwidth, int screenheight);
int gtxx_config_tp(void);
int gtxx_get_tp_info_index(void);
int gtxx_get_project_data_index(void);
int gtxx_get_tp_is_vaild(void);
#endif