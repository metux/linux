#ifndef _ELINK_TP_LIST_H__
#define _ELINK_TP_LIST_H__

#include <linux/kernel.h>

//-----------------------------------------

//M863P
#include "M863P_DFL_PG811_WXGA_8.h" 
#include "M863P_DFL_PO202106017A_WXGANL_8.h"
#include "M863P_DFL_DP101518_WXGANL_101.h" 
#include "M863P_MJK_PG101_WXGANL_101.h"
#include "M863P_DFL_XC_PG1010_131_WXGANL_101.h"

//M866P
#include "M866_DFL_KINGVINA_848_WXGA_101.h"

//Default
#include "gt9xx_firmware.h"

//--------------------------------------------
//tp_modle
typedef enum {
	m863p_tp_gsl_modle,
	m866p_tp_gsl_modle,
	//default
	default_tp_gsl_modle
}NUM_GTP_MODLE;
//--------------------------------------------

static TP_PROJECT_DATA m863p_tp_data[]={
	{"M863P_DFL_PG811_WXGA_8", {0xff,0xff,0xff,0xff}, M863P_DFL_PG811_WXGA_8,ARRAY_SIZE(M863P_DFL_PG811_WXGA_8), gtxx_default_FW, NULL, ARRAY_SIZE(gtxx_default_FW)},
	{"M863P_DFL_PO202106017A_WXGANL_8", {0xff,0xff,0xff,0xff}, M863P_DFL_PO202106017A_WXGANL_8, ARRAY_SIZE(M863P_DFL_PO202106017A_WXGANL_8),gtxx_default_FW, NULL, ARRAY_SIZE(gtxx_default_FW)},
	{"M863P_DFL_DP101518_WXGANL_101", {0xff,0xff,0xff,0xff}, M863P_DFL_DP101518_WXGANL_101, ARRAY_SIZE(M863P_DFL_DP101518_WXGANL_101),gtxx_default_FW, NULL, ARRAY_SIZE(gtxx_default_FW)},
	{"M863P_MJK_PG101_WXGANL_101", {0xff,0xff,0xff,0xff}, M863P_MJK_PG101_WXGANL_101, ARRAY_SIZE(M863P_MJK_PG101_WXGANL_101),gtxx_default_FW, NULL, ARRAY_SIZE(gtxx_default_FW)},
	{"M863P_DFL_XC_PG1010_131_WXGANL_101", {0xff,0xff,0xff,0xff}, M863P_DFL_XC_PG1010_131_WXGANL_101, ARRAY_SIZE(M863P_DFL_XC_PG1010_131_WXGANL_101),gtxx_default_FW, NULL, ARRAY_SIZE(gtxx_default_FW)},

};

static TP_PROJECT_DATA m866p_tp_data[]={
	{"M866P_DFL_KINGVINA_848_WXGA_8", {0xff,0xff,0xff,0xff}, M866P_DFL_KINGVINA_848_WXGA_8,ARRAY_SIZE(M866P_DFL_KINGVINA_848_WXGA_8),gtxx_default_FW, NULL, ARRAY_SIZE(gtxx_default_FW)},
};

ALL_TP_INFO gtxx_tp_info[] = {
	{"M863P",  m863p_tp_gsl_modle,  m863p_tp_data,  0, ARRAY_SIZE(m863p_tp_data),  0},
	{"M866P",  m866p_tp_gsl_modle,  m866p_tp_data,  0, ARRAY_SIZE(m866p_tp_data),  0}, 
};
#endif
