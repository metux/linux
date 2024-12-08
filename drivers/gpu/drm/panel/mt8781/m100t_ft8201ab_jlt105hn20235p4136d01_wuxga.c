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

//reload setting
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xF5,0x4A,0xD9,0x4A,0xD9,0x2A,0x2A,0x2B,0x0B,0x0C,0x14,0x15,0x39,0x34,0x83),

_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xF5,0x87,0x16,0x18,0x16,0x1F,0x97,0x77,0x77,0x46,0xC0,0xD9,0xE0),

_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xC5,0x84,0x0C,0x01,0x66,0x42,0x02,0x01,0x6B,0x27,0x01,0xCD,0x25,0xCD,0x25,0x05,0x40),

_INIT_DCS_CMD(0x00,0xD8),
_INIT_DCS_CMD(0xCD,0x00,0x00,0x0F,0xFF,0x00,0x00,0x00),

_INIT_DCS_CMD(0x00,0xC4),
_INIT_DCS_CMD(0xCF,0x00),

_INIT_DCS_CMD(0x00,0xC9),
_INIT_DCS_CMD(0xCF,0x00),
//======voltage set==========
_INIT_DCS_CMD(0x00,0x93), //VGH_N 19V
_INIT_DCS_CMD(0xC5,0x7A), 

_INIT_DCS_CMD(0x00,0x97), //VGH_I 19V
_INIT_DCS_CMD(0xC5,0x7A),

_INIT_DCS_CMD(0x00,0x9E), //VGH pump
_INIT_DCS_CMD(0xC5,0x0A),  //2AVDD-2AVEE

_INIT_DCS_CMD(0x00,0x9A),  //VGL_N -11V & AVEE-AVDD
_INIT_DCS_CMD(0xC5,0x39),  

_INIT_DCS_CMD(0x00,0x9C),  //VGL_I -11V & AVEE-AVDD
_INIT_DCS_CMD(0xC5,0x39), 

_INIT_DCS_CMD(0x00,0xB6), 
_INIT_DCS_CMD(0xC5,0x6B,0x6B),   //VGHO1_N_I 18V

_INIT_DCS_CMD(0x00,0xB8), 
_INIT_DCS_CMD(0xC5,0x2F,0x2F),  //VGLO1_N_I -10V

_INIT_DCS_CMD(0x00,0x00), 
_INIT_DCS_CMD(0xD8,0xCD,0xCD),  //GVDDP/N CD==5.55V

//_INIT_DCS_CMD(0x00,0x00), 
//_INIT_DCS_CMD(0xD9,0x00,0x75,0x75),  //VCOM(-1V) 

_INIT_DCS_CMD(0x00,0x82), 
_INIT_DCS_CMD(0xC5,0x95),  //LVD

_INIT_DCS_CMD(0x00,0x83), 
_INIT_DCS_CMD(0xC5,0x07),  //LVD Enable

//==========gamma==============
//Analog Gamma 
_INIT_DCS_CMD(0x00,0x00),
_INIT_DCS_CMD(0xE1,0x05,0x10,0x28,0x3C,0x47,0x53,0x65,0x72,0x74,0x80,0x7F,0x91,0x75,0x65,0x66,0x5B),
_INIT_DCS_CMD(0x00,0x10),
_INIT_DCS_CMD(0xE1,0x53,0x47,0x38,0x2E,0x24,0x0E,0x01,0x00),
_INIT_DCS_CMD(0x00,0x00),
_INIT_DCS_CMD(0xE2,0x05,0x10,0x28,0x3C,0x47,0x53,0x65,0x72,0x74,0x80,0x7F,0x91,0x75,0x65,0x66,0x5B),
_INIT_DCS_CMD(0x00,0x10),
_INIT_DCS_CMD(0xE2,0x53,0x47,0x38,0x2E,0x24,0x0E,0x01,0x00),

//reg_sd_sap
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xA4,0x0C),

//GAP=1.5uA & AP=1uA
_INIT_DCS_CMD(0x00,0x84),
_INIT_DCS_CMD(0xC5,0x00,0x00),

//OSC Auto calibration
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xF3,0x10),

//############################################################################################################
//===FT8201AB===
// B3A1~B3A4 砞﹚秆猂	//done
// B3A5=0x00 Source Resolution B3A5[7:5]
//B3A6=0x13  [4]:reg_panel_sd_resol, [1]:reg_panel_size, [0]:reg_panel_zigzag
_INIT_DCS_CMD(0x00,0xA1),
_INIT_DCS_CMD(0xB3,0x04,0xB0),	//X=1200
_INIT_DCS_CMD(0x00,0xA3),
_INIT_DCS_CMD(0xB3,0x07,0x80),	//Y=1920
_INIT_DCS_CMD(0x00,0xA5),
_INIT_DCS_CMD(0xB3,0x80,0x13),

// DMA Setting C1D0=0x30 (Single Chip) / 0xb0 (Multiple Drop)
_INIT_DCS_CMD(0x00,0xD0),
_INIT_DCS_CMD(0xC1,0xB0),  

// ## GOA Timing ##

//SKIP LVD Power-OFF0
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xCB,0x33,0x33,0x30,0x33,0x30,0x33,0x30),
_INIT_DCS_CMD(0x00,0x87),
_INIT_DCS_CMD(0xCB,0x33),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xCB,0x33,0x33,0x55,0x55,0x33,0x30,0x33,0x33),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xCB,0x30,0x33,0x33,0x33,0x30,0x30,0x33),
_INIT_DCS_CMD(0x00,0x97),
_INIT_DCS_CMD(0xCB,0x33),
//Power-OFF NORM
_INIT_DCS_CMD(0x00,0x98),
_INIT_DCS_CMD(0xCB,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14),
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xCB,0x14,0x14,0x55,0x55,0x14,0x14,0x14,0x14),
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xCB,0x14,0x55,0x55,0x14,0x14,0x14,0x14,0x14),
//Power-On
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xB7),
_INIT_DCS_CMD(0xCB,0x00),
_INIT_DCS_CMD(0x00,0xB8),
_INIT_DCS_CMD(0xCB,0x00,0x00,0x55,0x55,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xC0),
_INIT_DCS_CMD(0xCB,0x00,0x55,0x55,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xC7),
_INIT_DCS_CMD(0xCB,0x00),

//U2D
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xCC,0x00,0x00,0x00,0x00,0x00,0x29,0x2A,0x34),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xCC,0x35,0x13,0x14,0x15,0x16,0x07,0x1F,0x00),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xCC,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xCD,0x00,0x00,0x00,0x00,0x00,0x29,0x2A,0x34),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xCD,0x35,0x17,0x18,0x19,0x1A,0x08,0x20,0x00),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xCD,0x00,0x00,0x00,0x00,0x00,0x00),
//D2U
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xCC,0x00,0x00,0x00,0x00,0x00,0x29,0x2A,0x34),
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xCC,0x35,0x1A,0x19,0x18,0x17,0x20,0x08,0x00),
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xCC,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xCD,0x00,0x00,0x00,0x00,0x00,0x29,0x2A,0x34),
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xCD,0x35,0x16,0x15,0x14,0x13,0x1F,0x07,0x00),
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xCD,0x00,0x00,0x00,0x00,0x00,0x00),

//goa_vstx_shift_cnt_sel (20200812)
_INIT_DCS_CMD(0x00,0x81),
_INIT_DCS_CMD(0xC2,0x40),	//C281[6]=1

//VST5	(20200812)
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xC2,0x84,0x02,0x78,0x9D),

//VST6	(20200812)
_INIT_DCS_CMD(0x00,0x94),
_INIT_DCS_CMD(0xC2,0x83,0x02,0x78,0x9D), 

//VEND5	(20200812)
_INIT_DCS_CMD(0x00,0xB0),
_INIT_DCS_CMD(0xC2,0x00,0x02,0x78,0x9D),

//VEND6	(20200812)
_INIT_DCS_CMD(0x00,0xB4),
_INIT_DCS_CMD(0xC2,0x01,0x02,0x78,0x9D),

//CLKC
_INIT_DCS_CMD(0x00,0x80),
_INIT_DCS_CMD(0xC3,0x82,0x87,0x02,0x70,0x9D,0x00,0x02,0x07),
_INIT_DCS_CMD(0x00,0x88),
_INIT_DCS_CMD(0xC3,0x00,0x85,0x02,0x70,0x9D,0x00,0x02,0x07),
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xC3,0x02,0x83,0x02,0x70,0x9D,0x00,0x02,0x07),
_INIT_DCS_CMD(0x00,0x98),
_INIT_DCS_CMD(0xC3,0x04,0x81,0x02,0x70,0x9D,0x00,0x02,0x07),

//CLKD
_INIT_DCS_CMD(0x00,0xC0),
_INIT_DCS_CMD(0xCD,0x81,0x86,0x02,0x70,0x9D,0x00,0x02,0x07),
_INIT_DCS_CMD(0x00,0xC8),
_INIT_DCS_CMD(0xCD,0x01,0x84,0x02,0x70,0x9D,0x00,0x02,0x07),
_INIT_DCS_CMD(0x00,0xD0),
_INIT_DCS_CMD(0xCD,0x03,0x82,0x02,0x70,0x9D,0x00,0x02,0x07),
_INIT_DCS_CMD(0x00,0xD8),
_INIT_DCS_CMD(0xCD,0x05,0x80,0x02,0x70,0x9D,0x00,0x02,0x07), 

//GCH & GCL
_INIT_DCS_CMD(0x00,0xE2),
_INIT_DCS_CMD(0xCC,0x08,0x7C,0x00,0x08),

//ECLK (20200812)
_INIT_DCS_CMD(0x00,0xF0),
_INIT_DCS_CMD(0xCC,0x3D,0x88,0x88,0xC0,0x00),

//reg_goa_select_dg_rtn
_INIT_DCS_CMD(0x00,0xFD),
_INIT_DCS_CMD(0xCB,0x82),

//=============== ## TCON Timing START ##	===============//20220929
_INIT_DCS_CMD(0x00, 0x80),
_INIT_DCS_CMD(0xC0, 0x00 ,0x99 ,0x00 ,0xF2 ,0x00 ,0x10),

_INIT_DCS_CMD(0x00, 0x90),
_INIT_DCS_CMD(0xC0, 0x00 ,0x99 ,0x00 ,0xF2 ,0x00 ,0x10),

_INIT_DCS_CMD(0x00, 0xA0),
_INIT_DCS_CMD(0xC0, 0x01 ,0x30 ,0x00 ,0xF2 ,0x00 ,0x10),

_INIT_DCS_CMD(0x00, 0xB0),
_INIT_DCS_CMD(0xC0, 0x00 ,0x99 ,0x00 ,0xF2 ,0x10),

_INIT_DCS_CMD(0x00, 0xA3),
_INIT_DCS_CMD(0xC1, 0x28, 0x28, 0x04),

_INIT_DCS_CMD(0x00, 0x80),
_INIT_DCS_CMD(0xCE, 0x00 ),

_INIT_DCS_CMD(0x00, 0x90),
_INIT_DCS_CMD(0xCE, 0x00 ),

_INIT_DCS_CMD(0x00, 0xD0),
_INIT_DCS_CMD(0xCE, 0x01 ,0x00 ,0x0A ,0x01 ,0x01 ,0x00 ,0xEA ,0x01),

_INIT_DCS_CMD(0x00, 0xE0),
_INIT_DCS_CMD(0xCE, 0x00),

_INIT_DCS_CMD(0x00, 0xF0),
_INIT_DCS_CMD(0xCE, 0x00),

_INIT_DCS_CMD(0x00, 0xB0),
_INIT_DCS_CMD(0xCF, 0x06 ,0x06 ,0x56 ,0x5A),

_INIT_DCS_CMD(0x00, 0xB5),
_INIT_DCS_CMD(0xCF, 0x03 ,0x03 ,0xD8 ,0xDC),

_INIT_DCS_CMD(0x00, 0xC0),
_INIT_DCS_CMD(0xCF, 0x07 ,0x07 ,0x58 ,0x5C),

_INIT_DCS_CMD(0x00, 0xC5),
_INIT_DCS_CMD(0xCF, 0x00 ,0x07 ,0x08 ,0x80),

//=============== ## TCON Timing END ##	===============//
//CLK Source delay	(20200812)
_INIT_DCS_CMD(0x00,0x90),
_INIT_DCS_CMD(0xC4,0x88),	//CLK Source delay

//CLK allon
_INIT_DCS_CMD(0x00,0x92),
_INIT_DCS_CMD(0xC4,0xC0),

//CLK EQ
_INIT_DCS_CMD(0x00,0xC8),
_INIT_DCS_CMD(0xC3,0xFF,0xF0,0x00),

//gate slew rate code	
_INIT_DCS_CMD(0x00,0xC0),
_INIT_DCS_CMD(0xC5,0x00,0x00,0x00,0x00,0x0F,0x0F,0x0F,0x0F,0x00,0x0C,0x00), //gate slew enable
//tp_term_sync_vb
_INIT_DCS_CMD(0x00,0xC9),
_INIT_DCS_CMD(0xCE,0x00),

_INIT_DCS_CMD(0x1C,0x00),	//FIFO mode

//M&S PLL from the same 4M (CCLEE 20200828-1)
_INIT_DCS_CMD(0x00, 0xD6),
_INIT_DCS_CMD(0xC1, 0x00),

//Hsync & Vsync delay for cascade
_INIT_DCS_CMD(0x00,0xD5),
_INIT_DCS_CMD(0xC0,0xF0),

//Vsync sync with mipi
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xC1,0xE0),

//pump VGH afd off
_INIT_DCS_CMD(0x00,0x9F),
_INIT_DCS_CMD(0xC5,0x00),

//PUMP  VGH CLK all on disable
_INIT_DCS_CMD(0x00,0x91),
_INIT_DCS_CMD(0xC5,0x4C),

//tcon_tp_term2_vb_ln
_INIT_DCS_CMD(0x00,0xD7),
_INIT_DCS_CMD(0xCE,0x01),

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
//Sync gamma enable
_INIT_DCS_CMD(0x00,0x82),
_INIT_DCS_CMD(0xA5,0x01),

//VGH pump AFD sequence
_INIT_DCS_CMD(0x00,0x8C),
_INIT_DCS_CMD(0xCF,0x40,0x40),

//en_Vcom can't be gating by LVD
_INIT_DCS_CMD(0x00,0x9A),
_INIT_DCS_CMD(0xF5,0x35),

//Source pull low setting
_INIT_DCS_CMD(0x00,0xA2),
_INIT_DCS_CMD(0xF5,0x1F),
//tcon M&S delay 2 MCLK 
_INIT_DCS_CMD(0x00,0xB6),
_INIT_DCS_CMD(0xC0,0x02),

//gamma Voltage test mode en
_INIT_DCS_CMD(0x00,0xA0),
_INIT_DCS_CMD(0xA5,0x04),

//power off blank num
_INIT_DCS_CMD(0x00,0xC1),
_INIT_DCS_CMD(0xC0,0x11),
//### For dual chip only ###	//(20200812)

//for dual chip master
_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//for master only mode
_INIT_DCS_CMD(0xFA,0x02),	

//VGHO1 sink OFF
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xC5,0x09),

//VGLO1 sink ON
_INIT_DCS_CMD(0x00,0xCB),
_INIT_DCS_CMD(0xC5,0x01),

//VGHO1 & VGLO1 
_INIT_DCS_CMD(0x00,0xB6), 
_INIT_DCS_CMD(0xC5,0x6B,0x6B),   //M_VGHO1_N_I 18V
_INIT_DCS_CMD(0x00,0xB8), 
_INIT_DCS_CMD(0xC5,0x2F,0x2F),  //M_VGLO1_N_I -10V

//master vcom on
_INIT_DCS_CMD(0x00,0x91), 
_INIT_DCS_CMD(0xA5,0x00),
_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//Return to normal
_INIT_DCS_CMD(0xFA,0x5A),	

//for dual chip slave
_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//for slave only mode
_INIT_DCS_CMD(0xFA,0x01),	

//VGHO1 sink OFF
_INIT_DCS_CMD(0x00,0xA8),
_INIT_DCS_CMD(0xC5,0x09),

//VGLO1 sink OFF
_INIT_DCS_CMD(0x00,0xCB),
_INIT_DCS_CMD(0xC5,0x09),

//VGHO1 & VGLO1 -2step
_INIT_DCS_CMD(0x00,0xB6), 
_INIT_DCS_CMD(0xC5,0x69,0x69),   //S_VGHO1_N_I 17.8V

_INIT_DCS_CMD(0x00,0xB8), 
_INIT_DCS_CMD(0xC5,0x2D,0x2D),  //S_VGLO1_N_I -9.8V

//slave vcom off
_INIT_DCS_CMD(0x00,0x91), 
_INIT_DCS_CMD(0xA5,0x40),

_INIT_DCS_CMD(0x00,0xA4),
_INIT_DCS_CMD(0xF3,0x0B),
_INIT_DCS_CMD(0x00,0x00),//Return to normal
_INIT_DCS_CMD(0xFA,0x5A), 
//----------------------LCD initial code End----------------------//
//BIST enable
//_INIT_DCS_CMD(0x00,0xA9),
//_INIT_DCS_CMD(0xF6,0x00),
//_INIT_DCS_CMD(0x00,0x87),
//_INIT_DCS_CMD(0xF6,0x5A),

////deep standby
//_INIT_DCS_CMD(0x00,0x00),
//_INIT_DCS_CMD(0xF7,0x5A,0xA5,0x95,0x27),

////tp stop
//_INIT_DCS_CMD(0x00,0x80),
//_INIT_DCS_CMD(0xCE,0x00),
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

#define VSA				6
#define VBP				28
#define VFP				224

#define HSA				5
#define HBP				6
#define HFP				6

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


struct cust_drm_lcm m100t_ft8201ab_jlt105hn20235p4136d01_wuxga = {
	.name = "m100t_ft8201ab_jlt105hn20235p4136d01_wuxga",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
