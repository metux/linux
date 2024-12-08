// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Xinlei lee <xinlei.lee@mediatek.com>
 */
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_SUPPORT) //Leo 20220421
#include <mt-plat/cust_gpios.h>
#endif


#if !IS_ENABLED(CONFIG_CM_CUST_GPIOS_SUPPORT)
#define cust_gpio_set_value(x,y)	pr_info("__func__ no implement! \n",__func__)
#endif

//Leo 20220421
#include "panel_common.h" 

static const struct panel_init_cmd cm_init_cmd[] = {
//============CMD WR enable=============	
_INIT_DCS_CMD(0x00,0x00),
_INIT_DCS_CMD(0xFF,0x82,0x01,0x01), 

_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xFF,0x82,0x01),

//======voltage set(AVDD/AVEE=+6V/-6V,VGHO/VGLO=14V/-14V,GVDD/NGVDD=5.4V/-5.4V)==========
_INIT_DCS_CMD(0x00,0x93), //VGH_N 15V
_INIT_DCS_CMD(0xC5,0x52), 

_INIT_DCS_CMD(0x00,0x97), //VGH_I 15V
_INIT_DCS_CMD(0xC5,0x52),

_INIT_DCS_CMD(0x00,0x9E), 
_INIT_DCS_CMD(0xC5,0x05),  //2AVDD-AVEE

_INIT_DCS_CMD(0x00,0x9A),  //VGL_N -15V 
_INIT_DCS_CMD(0xC5,0xE1),  //2AVEE-AVDD

_INIT_DCS_CMD(0x00,0x9C),  //VGL_I -15V 
_INIT_DCS_CMD(0xC5,0xE1),  //2AVEE-AVDD

_INIT_DCS_CMD(0x00,0xB6), 
_INIT_DCS_CMD(0xC5,0x43,0x43),   //VGHO1_N_I 14V

_INIT_DCS_CMD(0x00,0xB8), 
_INIT_DCS_CMD(0xC5,0x57,0x57),  //VGLO1_N_I -14V

_INIT_DCS_CMD(0x00,0x00), 
_INIT_DCS_CMD(0xD8,0xBE,0xBE),  //GVDDP/N 5.4V/-5.4V step=0.01V

_INIT_DCS_CMD(0x00,0x00), 
_INIT_DCS_CMD(0xD9,0x00,0xA2,0xA2),  //VCOM(-0.7V) step=0.01V

_INIT_DCS_CMD(0x00,0x82), 
_INIT_DCS_CMD(0xC5,0x95),  //LVD

_INIT_DCS_CMD(0x00,0x83), 
_INIT_DCS_CMD(0xC5,0x07),  //LVD Enable

//tp term VGH pump off CLK gating L
// VGH & VGL PUMP Rate 4line
_INIT_DCS_CMD(0x00,0x94),
_INIT_DCS_CMD(0xC5,0x46),

_INIT_DCS_CMD(0x00,0x98),
_INIT_DCS_CMD(0xC5,0x64),

_INIT_DCS_CMD(0x00,0x9B),
_INIT_DCS_CMD(0xC5,0x65),

_INIT_DCS_CMD(0x00,0x9D),
_INIT_DCS_CMD(0xC5,0x65),

_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xA5,0x04),//GVDD & NGVDD test en

//12bit PWM 29Khz
//_INIT_DCS_CMD(0x00,0xB0),
//_INIT_DCS_CMD(0xCA,0x00,0x00,0x04),

//_INIT_DCS_CMD(0x00,0xB4),
//_INIT_DCS_CMD(0xCA,0x03), //add Dimming Frame

//_INIT_DCS_CMD(0x00,0xE6), 
//_INIT_DCS_CMD(0xC0,0x50),//0x00:Forward Scan;   0x50:Backward Scan
//_INIT_DCS_CMD(0x00,0xA7),
//_INIT_DCS_CMD(0xF3,0x20), 

//_INIT_DCS_CMD(0x00,0xC1), 
//_INIT_DCS_CMD(0xC0,0x11),//PowerOn&Off BlankFrame Number

//==========gamma==============
//Analog Gamma 
_INIT_DCS_CMD(0x00,0x00),
_INIT_DCS_CMD(0xE1,0x05,0x0B,0x19,0x28,0x31,0x3B,0x4D,0x5A,0x5D,0x6B,0x6F,0x87,0x7A,0x65,0x64,0x58),
_INIT_DCS_CMD(0x00,0x10),
_INIT_DCS_CMD(0xE1,0x50,0x44,0x36,0x2D,0x24,0x15,0x01,0x00),
_INIT_DCS_CMD(0x00,0x00),
_INIT_DCS_CMD(0xE2,0x05,0x0B,0x19,0x28,0x31,0x3B,0x4D,0x5A,0x5D,0x6B,0x6F,0x87,0x7A,0x65,0x64,0x58),
_INIT_DCS_CMD(0x00,0x10),
_INIT_DCS_CMD(0xE2,0x50,0x44,0x36,0x2D,0x24,0x15,0x01,0x00),
//reg_sd_sap
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xA4,0x8C),

//OSC Auto calibration
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xF3,0x10),

//############################################################################################################
//===FT8201AB===

// B3A1~B3A4 
// B3A5=0x00 Source Resolution B3A5[7:5]
//B3A6=0x13  [4]:reg_panel_sd_resol, [1]:reg_panel_size, [0]:reg_panel_zigzag
_INIT_DCS_CMD(0x00,0xA1),
_INIT_DCS_CMD(0xB3,0x04,0xB0),	//X=1200
_INIT_DCS_CMD(0x00,0xA3),
_INIT_DCS_CMD(0xB3,0x07,0x80),	//Y=1920
_INIT_DCS_CMD(0x00,0xA5),
_INIT_DCS_CMD(0xB3,0x80,0x13),	//600RGB

// DMA Setting C1D0=0x30 (Single Chip) / 0xb0 (Multiple Drop)
_INIT_DCS_CMD(0x00,0xD0),
_INIT_DCS_CMD(0xC1,0xB0),  

// ## GOA Timing ##

//SKIP LVD Power-OFF0
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xCB,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F),
_INIT_DCS_CMD(0x00,0x87),
_INIT_DCS_CMD(0xCB,0x3F),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xCB,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xCB,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F),
_INIT_DCS_CMD(0x00,0x97),
_INIT_DCS_CMD(0xCB,0x3F),


//Power-OFF NORM
_INIT_DCS_CMD(0x00,0x98),
_INIT_DCS_CMD(0xCB,0xD4,0xD4,0xD4,0xD4,0xD4,0xD4,0xD4,0xD4),
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xCB,0xD4,0xD4,0xDB,0xD8,0xD4,0xD4,0xD4,0xD4),
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xCB,0xD4,0xD4,0xD4,0xD4,0xD4,0xD4,0xD4,0xD4),


//Power-On
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xB7),
_INIT_DCS_CMD(0xCB,0x00),
_INIT_DCS_CMD(0x00,0xB8),
_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xC0),
_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xC7),
_INIT_DCS_CMD(0xCB,0x00),

//GND Function
//_INIT_DCS_CMD(0x00,0xD0),
//_INIT_DCS_CMD(0xCB,0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x30,0x00,0x30),

//U2D						
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xCC,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xCC,0x2B,0x22,0x20,0x35,0x34,0x1A,0x18,0x16),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xCC,0x14,0x12,0x10,0x08,0x38,0x38),	
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xCD,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xCD,0x2B,0x21,0x1F,0x35,0x34,0x19,0x17,0x15),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xCD,0x13,0x11,0x0F,0x07,0x38,0x38),

//D2U						
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xCC,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38),
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xCC,0x2B,0x21,0x07,0x35,0x34,0x0F,0x11,0x13),
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xCC,0x15,0x17,0x19,0x1F,0x38,0x38),
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xCD,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38),
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xCD,0x2B,0x22,0x08,0x35,0x34,0x10,0x12,0x14),
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xCD,0x16,0x18,0x1A,0x20,0x38,0x38),

//goa_vstx_shift_cnt_sel (20200812)
_INIT_DCS_CMD(0x00,0x81),
_INIT_DCS_CMD(0xC2,0x40),	//C281[6]=1

//reg_goa_select_dg_rtn
_INIT_DCS_CMD(0x00,0xFD),
_INIT_DCS_CMD(0xCB,0x82),	

//VST5					
_INIT_DCS_CMD(0x00,0x90),//STV1_ODD
_INIT_DCS_CMD(0xC2,0x85,0x03,0x1F,0x9F),				

//VST6	
_INIT_DCS_CMD(0x00,0x94),//STV1_EVEN
_INIT_DCS_CMD(0xC2,0x84,0x03,0x1F,0x9F), 

//VEND5					
_INIT_DCS_CMD(0x00,0xB0),//STV2_ODD
_INIT_DCS_CMD(0xC2,0x03,0x43,0x1F,0x9F),				

//VEND6	
_INIT_DCS_CMD(0x00,0xB4),//STV2_EVEN
_INIT_DCS_CMD(0xC2,0x02,0x43,0x1F,0x9F),

//VEND7					
_INIT_DCS_CMD(0x00,0xB8),//RESET_ODD
_INIT_DCS_CMD(0xC2,0x01,0x03,0x1F,0x9F),				

//VEND8	
_INIT_DCS_CMD(0x00,0xBC),//RESET_EVEN
_INIT_DCS_CMD(0xC2,0x02,0x03,0x1F,0x9F),

//CLKB					
_INIT_DCS_CMD(0x00,0xE0),
_INIT_DCS_CMD(0xC2,0x83,0x01,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0xE8),
_INIT_DCS_CMD(0xC2,0x82,0x02,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0xF0),
_INIT_DCS_CMD(0xC2,0x81,0x03,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0xF8),
_INIT_DCS_CMD(0xC2,0x00,0x04,0x03,0x1F,0x9F,0x00,0x00,0x0B),	

//CLKC						
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xC3,0x01,0x05,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xC3,0x02,0x06,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xC3,0x03,0x07,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0x98),
_INIT_DCS_CMD(0xC3,0x04,0x08,0x03,0x1F,0x9F,0x00,0x00,0x0B),	
			
//CLKD
_INIT_DCS_CMD(0x00,0xC0),
_INIT_DCS_CMD(0xCD,0x05,0x09,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0xC8),
_INIT_DCS_CMD(0xCD,0x06,0x0A,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0xD0),
_INIT_DCS_CMD(0xCD,0x07,0x0B,0x03,0x1F,0x9F,0x00,0x00,0x0B),
_INIT_DCS_CMD(0x00,0xD8),
_INIT_DCS_CMD(0xCD,0x08,0x0C,0x03,0x1F,0x9F,0x00,0x00,0x0B), 

				
//ECLK 
_INIT_DCS_CMD(0x00,0xF0),//VDD1&VDD2=1s Switch
_INIT_DCS_CMD(0xCC,0x3D,0x88,0x88,0x18,0x18,0x80),

//=============== ## TCON Timing START ##	===============//
_INIT_DCS_CMD(0x00, 0x80),
_INIT_DCS_CMD(0xC0, 0x00 ,0x96 ,0x01 ,0x0B ,0x00 ,0x17),

_INIT_DCS_CMD(0x00, 0x90),
_INIT_DCS_CMD(0xC0, 0x00 ,0x96 ,0x01 ,0x0B ,0x00 ,0x17),

_INIT_DCS_CMD(0x00, 0xA0),
_INIT_DCS_CMD(0xC0, 0x01 ,0x27 ,0x01 ,0x12 ,0x00 ,0x10),

_INIT_DCS_CMD(0x00, 0xB0),
_INIT_DCS_CMD(0xC0, 0x00 ,0x96 ,0x01 ,0x0B ,0x17),

_INIT_DCS_CMD(0x00, 0xA3),
_INIT_DCS_CMD(0xC1, 0x22, 0x22, 0x04),

_INIT_DCS_CMD(0x00, 0x80),
_INIT_DCS_CMD(0xCE, 0x00 ),

_INIT_DCS_CMD(0x00, 0x90),
_INIT_DCS_CMD(0xCE, 0x00 ),

_INIT_DCS_CMD(0x00, 0xD0),
_INIT_DCS_CMD(0xCE, 0x01 ,0x00 ,0x18 ,0x01 ,0x01 ,0x00 ,0xEF ,0x01),

_INIT_DCS_CMD(0x00, 0xE0),
_INIT_DCS_CMD(0xCE, 0x00),

_INIT_DCS_CMD(0x00, 0xF0),
_INIT_DCS_CMD(0xCE, 0x00),

_INIT_DCS_CMD(0x00, 0xB0),
_INIT_DCS_CMD(0xCF, 0x06 ,0x06 ,0x4C ,0x50),

_INIT_DCS_CMD(0x00, 0xB5),
_INIT_DCS_CMD(0xCF, 0x03 ,0x03 ,0xBC ,0xC0),

_INIT_DCS_CMD(0x00, 0xC0),
_INIT_DCS_CMD(0xCF, 0x07 ,0x07 ,0x58 ,0x5C),

_INIT_DCS_CMD(0x00, 0xC5),
_INIT_DCS_CMD(0xCF, 0x00 ,0x07 ,0x08 ,0x80),

//=============== ## TCON Timing END ##	===============//

_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xC4,0x88),//CLK Source delay

_INIT_DCS_CMD(0x00,0x92),
_INIT_DCS_CMD(0xC4,0xC0),//CLK allon

//_INIT_DCS_CMD(0x00,0xC5),
//_INIT_DCS_CMD(0xC3,0x0E,0x00,0x10),//CLKA&CLKD Non Notch: EQ

//_INIT_DCS_CMD(0x00,0xC8),
//_INIT_DCS_CMD(0xC3,0x0E,0x00,0x10),//CLKC&CLKD Non Notch: EQ

_INIT_DCS_CMD(0x00,0xC9),
_INIT_DCS_CMD(0xCE,0x00),//tp_term_sync_vb

_INIT_DCS_CMD(0x1C,0x00),//FIFO mode

_INIT_DCS_CMD(0x00, 0xD6),
_INIT_DCS_CMD(0xC1, 0x00),//M&S PLL from the same 4M 

_INIT_DCS_CMD(0x00,0xD5),
_INIT_DCS_CMD(0xC0,0xF0),//Hsync & Vsync delay for cascade

_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xC1,0xE0),//Vsync sync with mipi

_INIT_DCS_CMD(0x00,0x9F),
_INIT_DCS_CMD(0xC5,0x00),//pump VGH afd off

_INIT_DCS_CMD(0x00,0x91),
_INIT_DCS_CMD(0xC5,0x4C),//PUMP  VGH CLK all on disable

_INIT_DCS_CMD(0x00,0xD7),
_INIT_DCS_CMD(0xCE,0x01),//tcon_tp_term2_vb_ln

_INIT_DCS_CMD(0x00,0x82),
_INIT_DCS_CMD(0xA5,0x01),//Sync gamma enable

_INIT_DCS_CMD(0x00,0x8C),
_INIT_DCS_CMD(0xCF,0x40,0x40),//VGH pump AFD sequence

_INIT_DCS_CMD(0x00,0x9A),
_INIT_DCS_CMD(0xF5,0x35),//en_Vcom can't be gating by LVD

_INIT_DCS_CMD(0x00,0xA2),
_INIT_DCS_CMD(0xF5,0x1F),//Source pull low setting

//vgho&vglo_p1_chg_slpout
_INIT_DCS_CMD(0x00,0xB2),
_INIT_DCS_CMD(0xC5,0x15),
_INIT_DCS_CMD(0x00,0xB5),
_INIT_DCS_CMD(0xC5,0x05),

//temp compensation
_INIT_DCS_CMD(0x00,0xD2),
_INIT_DCS_CMD(0xC5,0xD1),//temp comp en

//H & L temp setting 
//H=60C, L=-5C 
_INIT_DCS_CMD(0x00,0xE0),
_INIT_DCS_CMD(0xC5,0x3C,0x3C,0xFB,0xFB),

//VGH_H=-63  VGH_L=+20
//VGH_H_step=-15, VGH_L_step=+15
_INIT_DCS_CMD(0x00,0xE8),
_INIT_DCS_CMD(0xC5,0xC1,0x14,0x09,0x07),

//VGL_H=0  VGL_L=0
//VGL_H_step=0, VGL_L_step=0
_INIT_DCS_CMD(0x00,0xEC),
_INIT_DCS_CMD(0xC5,0x00,0x00,0x00,0x00),

//HT LT pump ratio
_INIT_DCS_CMD(0x00,0xF4),
_INIT_DCS_CMD(0xC5,0x00,0x01,0x01),

_INIT_DCS_CMD(0x00,0xFB),
_INIT_DCS_CMD(0xC5,0x01,0x01,0x01),

//pump ratio set temp threshold
_INIT_DCS_CMD(0x00,0xF0),
_INIT_DCS_CMD(0xC5,0x3C,0x3C,0xFB,0xFB),

_INIT_DCS_CMD(0x00,0xF7),
_INIT_DCS_CMD(0xC5,0x3C,0x3C,0xFB,0xFB),




//### For dual chip only ###	//

//for dual chip master
_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//for master only mode
_INIT_DCS_CMD(0xFA,0x02),	

_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xC5,0x09),//VGHO1 sink OFF

_INIT_DCS_CMD(0x00,0xCB),
_INIT_DCS_CMD(0xC5,0x01),//VGLO1 sink ON

_INIT_DCS_CMD(0x00,0xB6), 
_INIT_DCS_CMD(0xC5,0x43,0x43), //M_VGHO1_N_I 14V

_INIT_DCS_CMD(0x00,0xB8), 
_INIT_DCS_CMD(0xC5,0x57,0x57), //M_VGLO1_N_I -14V

_INIT_DCS_CMD(0x00,0x91), 
_INIT_DCS_CMD(0xA5,0x00), //master vcom on

_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//Return to normal
_INIT_DCS_CMD(0xFA,0x5A),	

//for dual chip slave
_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//for slave only mode
_INIT_DCS_CMD(0xFA,0x01),	

_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xC5,0x09),//VGHO1 sink OFF

_INIT_DCS_CMD(0x00,0xCB),
_INIT_DCS_CMD(0xC5,0x09),//VGLO1 sink OFF

_INIT_DCS_CMD(0x00,0xB6), //VGHO1-2step
_INIT_DCS_CMD(0xC5,0x41,0x41),   //S_VGHO1_N_I 13.8V

_INIT_DCS_CMD(0x00,0xB8), //VGLO1-2step
_INIT_DCS_CMD(0xC5,0x55,0x55),  //S_VGLO1_N_I -13.8V

_INIT_DCS_CMD(0x00,0x91), 
_INIT_DCS_CMD(0xA5,0x40),//slave vcom off

_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//Return to normal
_INIT_DCS_CMD(0xFA,0x5A), 
//============CMD WR disable=============	
_INIT_DCS_CMD(0x00,0x00),
_INIT_DCS_CMD(0xFF,0x00,0x00,0x00), 

_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xFF,0x00,0x00),

//----------------------LCD initial code End----------------------//

	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DELAY_CMD(200),
	{} //it must be here
};

//#define LCM_5
//#define LCM_6
//#define LCM_7
//#define LCM_8
//#define LCM_101
//#define LCM_1036
#define LCM_1051
//#define LCM_1095

#ifdef LCM_8
#define WIDTH_MM    108
#define HEIGHT_MM   172
#endif

#ifdef LCM_101
#define WIDTH_MM    137
#define HEIGHT_MM   217 
#endif

#ifdef LCM_1036
#define WIDTH_MM    135
#define HEIGHT_MM   226 
#endif

#ifdef LCM_1051
#define WIDTH_MM    141
#define HEIGHT_MM   226 
#endif

#ifdef LCM_1095
#define WIDTH_MM    144
#define HEIGHT_MM   238 
#endif

#ifndef WIDTH_MM
#define WIDTH_MM   137
#endif

#ifndef HEIGHT_MM
#define HEIGHT_MM  217
#endif 


#define LCM_WIDTH		1200
#define LCM_HEIGHT		1920

#define VSA				8
#define VBP				40
#define VFP				224

#define HSA				6
#define HBP				28
#define HFP				76

#define FPS				60

#define PLL_CLK			505

//htotal * vtotal * vrefresh / 1000
#define HTOTAL		(LCM_WIDTH + HFP + HBP + HSA)
#define VTOTAL		(LCM_HEIGHT + VFP + VBP + VSA)
#define PIXEL		(HTOTAL * VTOTAL * FPS ) / 1000
static const struct drm_display_mode common_default_mode = {
	.clock = PIXEL, 
	.hdisplay = LCM_WIDTH,
	.hsync_start = LCM_WIDTH + HFP, //hfp
	.hsync_end = LCM_WIDTH + HFP + HSA, //hsa
	.htotal = LCM_WIDTH + HFP + HSA + HBP, //hsp
	.vdisplay = LCM_HEIGHT,
	.vsync_start = LCM_HEIGHT + VFP, //vfp
	.vsync_end = LCM_HEIGHT + VFP + VSA, //vsa
	.vtotal = LCM_HEIGHT + VFP + VSA + VBP, //vsp
};

static const struct panel_desc common_desc = {

	.modes = &common_default_mode,
	.bpc = 8,
	.size = {
		.width_mm = WIDTH_MM,
		.height_mm = HEIGHT_MM,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
				  MIPI_DSI_MODE_LPM,
	.init_cmds = cm_init_cmd,
	.discharge_on_disable = false,

};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 1;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = PLL_CLK,
	//.vfp_low_power = 840,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.data_rate = PLL_CLK*2,
	.rotate = 1,
	.ssc_enable = 0,
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

static inline struct common_panel *to_common_panel(struct drm_panel *panel)
{
	return container_of(panel, struct common_panel, base);
}

static int common_panel_init_dcs_cmd(struct common_panel *cm)
{
	struct mipi_dsi_device *dsi = cm->dsi;
	int i, err = 0;

	if (cm->desc->init_cmds) {
		const struct panel_init_cmd *init_cmds = cm->desc->init_cmds;

		for (i = 0; init_cmds[i].len != 0; i++) {
			const struct panel_init_cmd *cmd = &init_cmds[i];

			switch (cmd->type) {
			case DELAY_CMD:
				msleep(cmd->data[0]);
				err = 0;
				break;

			case INIT_DCS_CMD:
				err = mipi_dsi_dcs_write(dsi, cmd->data[0],
							 cmd->len <= 1 ? NULL :
							 &cmd->data[1],
							 cmd->len - 1);
				break;

			default:
				err = -EINVAL;
			}

			if (err < 0) {
				pr_notice("failed to write command %u\n", i);
				return err;
			}
		}
	}
	return 0;
}

static int common_panel_enter_sleep_mode(struct common_panel *cm)
{
	struct mipi_dsi_device *dsi = cm->dsi;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	return 0;
}

static int common_panel_unprepare_power(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (!cm->prepared_power)
		return 0;

	pr_notice("Leo [Kernel/LCM] %s enter\n", __func__);

	mdelay(100);

#if IS_ENABLED(CONFIG_WB_TP_INCELL_SUPPORT) //Leo 20230131
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ZERO);
    mdelay(5);
#endif
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(5);

	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ZERO);
	mdelay(5);
	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ZERO);
	mdelay(5);
	//cust_gpio_set_value(CUST_GPIO_BIAS_EN, GPIO_OUT_ZERO);
	//mdelay(5);

	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ZERO);
	mdelay(10);

	cm->prepared_power = false;

	return 0;
}

static int common_panel_unprepare(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);
	int ret;

	if (!cm->prepared)
		return 0;

	if (!cm->desc->discharge_on_disable) {
		ret = common_panel_enter_sleep_mode(cm);
		if (ret < 0) {
			pr_notice("failed to set panel off: %d\n",
				ret);
			return ret;
		}
	}

	common_panel_unprepare_power(panel);

	cm->prepared = false;

	return 0;
}

static int common_panel_prepare_power(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (cm->prepared_power)
		return 0;
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ONE);
	msleep(20);

	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ONE);
	usleep_range(2000, 2001);
	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ONE);

	msleep(20);

#if IS_ENABLED(CONFIG_WB_TP_INCELL_SUPPORT) //Leo 20230131
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ONE);
    mdelay(5);
#endif

	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(2);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(5);

	cm->prepared_power = true;

	return 0;
}

static int common_panel_prepare(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);
	int ret = 0;

	if (cm->prepared)
		return 0;

	common_panel_prepare_power(panel);

	ret = common_panel_init_dcs_cmd(cm);
	if (ret < 0) {
		pr_notice("failed to init panel: %d\n", ret);
		return ret;
	}

	cm->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

	//cust_gpio_set_value(CUST_GPIO_BIAS_EN, GPIO_OUT_ONE);

	return 0;
}

static int common_panel_enable(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (cm->enabled)
		return 0;

	msleep(130);

	if (cm->backlight) {
		cm->backlight->props.state &= ~BL_CORE_FBBLANK;
		cm->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(cm->backlight);
	}

	cm->enabled = true;
	return 0;
}

static int common_panel_disable(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (!cm->enabled)
		return 0;

	if (cm->backlight) {
		cm->backlight->props.power = FB_BLANK_POWERDOWN;
		cm->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(cm->backlight);
	}


	cm->enabled = false;

	return 0;
}

static int common_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct common_panel *cm = to_common_panel(panel);
	const struct drm_display_mode *m = cm->desc->modes;
	struct drm_display_mode *mode;
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 202300330
	struct drm_display_mode *csci_mode = cust_get_drm_panel_mode();
#endif

	mode = drm_mode_duplicate(connector->dev, m);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		return -ENOMEM;
	}

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230329
	drm_mode_copy_1(mode, csci_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u\n",
			csci_mode->hdisplay, csci_mode->vdisplay);
		return -ENOMEM;
	}
#endif

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = cm->desc->size.width_mm;
	connector->display_info.height_mm = cm->desc->size.height_mm;
	connector->display_info.bpc = cm->desc->bpc;
	drm_connector_set_panel_orientation(connector, cm->orientation);

	return 1;
}

static const struct drm_panel_funcs common_panel_funcs = {
	.unprepare = common_panel_unprepare,
	.prepare = common_panel_prepare,
	.disable = common_panel_disable,
	.enable = common_panel_enable,
	.get_modes = common_panel_get_modes,
};


struct cust_drm_lcm m100t_ft8201ab_c00zj105201a801_wuxga = {
	.name = "m100t_ft8201ab_c00zj105201a801_wuxga",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
