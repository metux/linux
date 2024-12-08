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

#if IS_ENABLED(CONFIG_WB_LT8911EXB) //Leo 20231221
//#define _Test_Pattern_ // Output test pattern
//#define  _read_edid_ // read eDP panel EDID
//#define _Msa_Active_Only_

#define _eDP_2G7_

enum {
	hfp = 0,
	hs,
	hbp,
	hact,
	htotal,
	vfp,
	vs,
	vbp,
	vact,
	vtotal,
	pclk_10khz
};

static u8		Read_DPCD010A = 0x00;
static bool	ScrambleMode = 0;
//static bool	flag_mipi_on = 0;

#ifdef _read_edid_ // read eDP panel EDID
static u8		EDID_DATA[128] = { 0 };
static u16		EDID_Timing[11] = { 0 };
static bool	EDID_Reply = 0;
#endif

//////////////////////LT8911EXB Config////////////////////////////////
//#define _1080P_eDP_Panel_
//#define _1366x768_eDP_Panel_
//#define _1280x800_eDP_Panel_
//#define _1600x900_eDP_Panel_
//#define _1920x1200_eDP_Panel_
#define _2496x1664_eDP_Panel_

#define _MIPI_Lane_ 4 // 3 /2 / 1

//#define _eDP_scramble_ // eDP scramble mode

#ifdef _1920x1200_eDP_Panel_

#define eDP_lane		2
#define PCR_PLL_PREDIV	0x40

#define _6bit_
//#define _8bit_

//const struct video_timing video[] =
static int MIPI_Timing[] =
// hfp, hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
 { 48, 	32, 50, 	1920, 	2050, 	3, 		6, 	51, 	1280, 	1340, 	15350 };                     //


/*******************************************************************
   全志平台的屏参 lcd_hbp 是 上面参数 hbp + hs 的和；
   The lcd_hbp of Allwinner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 上面参数 vbp + vs的 和，这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

	1、MTK平台 的 dis_eotp_en 的值，改成 false;   LK和kernel都需要修改,改成false。
	1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.

	2、展讯平台的 tx_eotp 的值置 1 。
	2. The value of tx_eotp of Spreadtrum platform is set to 1.

	3、RK平台 EN_EOTP_TX 置1.
	3. The value of EN_EOTP_TX of RK platform is set to 1.

	4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
	4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;
*******************************************************************/

#endif

#ifdef _1080P_eDP_Panel_

#define eDP_lane		2
#define PCR_PLL_PREDIV	0x40

// 根据前端MIPI信号的Timing，修改以下参数：
//According to the timing of the Mipi signal, modify the following parameters:
static int MIPI_Timing[] =
// hfp,	hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
//{ 88, 44, 148,    1920,   2200,   4,      5,  36,     1080,   1125,   14850 };// VESA
  { 48, 32, 50, 	1920, 	2050, 	3, 		6, 	51, 	1280, 	1340, 	16480 };                     //
  //{  48,   32,   142,   1920,   2142,   3,   6,   11,   1080,   1100,   14140 };
/*******************************************************************
   全志平台的屏参 lcd_hbp 是 上面参数 hbp + hs 的和；
   The lcd_hbp of Allwiner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 上面参数 vbp + vs的 和，这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

	1、MTK平台 的 dis_eotp_en 的值，改成 false;   LK和kernel都需要修改,改成false。
	1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.

	2、展讯平台的 tx_eotp 的值置 1 。
	2. The value of tx_eotp of Spreadtrum platform is set to 1.

	3、RK平台 EN_EOTP_TX 置1.
	3. The value of EN_EOTP_TX of RK platform is set to 1.

	4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
	4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;

*******************************************************************/

//#define _6bit_ // eDP panel Color Depth，262K color
#define _8bit_                                              // eDP panel Color Depth，16.7M color

#endif

//-----------------------------------------

#ifdef _1366x768_eDP_Panel_

#define eDP_lane		1
#define PCR_PLL_PREDIV	0x40

// 根据前端MIPI信号的Timing，修改以下参数：
static int MIPI_Timing[] =
// hfp,	hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
 { 48, 	32, 146, 	1368, 	1592, 	03, 		05, 	22, 	768, 	798, 	7622 };


/*******************************************************************
   全志平台的屏参 lcd_hbp 是 上面参数 hbp + hs 的和；
   The lcd_hbp of Allwiner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 上面参数 vbp + vs的 和，这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

   1、MTK平台 的 dis_eotp_en 的值，改成 false;	 LK和kernel都需要修改,改成false。
   1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.
   
   2、展讯平台的 tx_eotp 的值置 1 。
   2. The value of tx_eotp of Spreadtrum platform is set to 1.
   
   3、RK平台 EN_EOTP_TX 置1.
   3. The value of EN_EOTP_TX of RK platform is set to 1.
   
   4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
   4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;
   
*******************************************************************/
#define _6bit_                                                                      // eDP panel Color Depth，262K color
#endif

#ifdef _2496x1664_eDP_Panel_

#define eDP_lane		2
#define PCR_PLL_PREDIV	0x40

// 根据前端MIPI信号的Timing，修改以下参数：
//According to the timing of the Mipi signal, modify the following parameters:
static int MIPI_Timing[] =
//h_blanking 160  v:48
// hfp,	hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
  { 48, 32, 80, 	2496, 	2656, 	3, 		10, 	35, 	1664, 	1712, 	15350 };                     //36fps
/*******************************************************************
   全志平台的屏参 lcd_hbp 是 上面参数 hbp + hs 的和；
   The lcd_hbp of Allwiner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 上面参数 vbp + vs的 和，这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

	1、MTK平台 的 dis_eotp_en 的值，改成 false;   LK和kernel都需要修改,改成false。
	1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.

	2、展讯平台的 tx_eotp 的值置 1 。
	2. The value of tx_eotp of Spreadtrum platform is set to 1.

	3、RK平台 EN_EOTP_TX 置1.
	3. The value of EN_EOTP_TX of RK platform is set to 1.

	4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
	4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;

*******************************************************************/

//#define _6bit_ // eDP panel Color Depth，262K color
#define _8bit_                                              // eDP panel Color Depth，16.7M color

#endif

enum
{
	_Level1_ = 0,   // 15mA     0x82/0x00
	_Level2_,       // 12.8mA   0x81/0x00
	_Level3_,       // 11.2mA   0x80/0xe0
	_Level4_,       // 9.6mA    0x80/0xc0
	_Level5_,       // 8mA      0x80/0xa0
	_Level6_        // 6mA      0x80/0x80
};

static u8	Swing_Setting1[] = { 0x82, 0x81, 0x80, 0x80, 0x80, 0x80 };
static u8	Swing_Setting2[] = { 0x00, 0x00, 0xe0, 0xc0, 0xa0, 0x80 };

static u8	Level = _Level1_;

static void LT8911EXB_MIPI_Video_Timing( void )                                    // ( struct video_timing *video_format )
{
	pr_info("%s %d start \n",__func__,__LINE__);
	LT8911EXB_IIC_Write_byte( 0xff, 0xd0 );
	LT8911EXB_IIC_Write_byte( 0x0d, (u8)( MIPI_Timing[vtotal] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0e, (u8)( MIPI_Timing[vtotal] % 256 ) );    //vtotal
	LT8911EXB_IIC_Write_byte( 0x0f, (u8)( MIPI_Timing[vact] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x10, (u8)( MIPI_Timing[vact] % 256 ) );      //vactive

	LT8911EXB_IIC_Write_byte( 0x11, (u8)( MIPI_Timing[htotal] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x12, (u8)( MIPI_Timing[htotal] % 256 ) );    //htotal
	LT8911EXB_IIC_Write_byte( 0x13, (u8)( MIPI_Timing[hact] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x14, (u8)( MIPI_Timing[hact] % 256 ) );      //hactive

	LT8911EXB_IIC_Write_byte( 0x15, (u8)( MIPI_Timing[vs] % 256 ) );        //vsa
	LT8911EXB_IIC_Write_byte( 0x16, (u8)( MIPI_Timing[hs] % 256 ) );        //hsa
	LT8911EXB_IIC_Write_byte( 0x17, (u8)( MIPI_Timing[vfp] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x18, (u8)( MIPI_Timing[vfp] % 256 ) );       //vfp

	LT8911EXB_IIC_Write_byte( 0x19, (u8)( MIPI_Timing[hfp] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x1a, (u8)( MIPI_Timing[hfp] % 256 ) );       //hfp
}

static void DpcdWrite( u32 Address, u8 Data )
{
	/***************************
	   注意大小端的问题!
	   这里默认是大端模式

	   Pay attention to the Big-Endian and Little-Endian!
	   The default mode is Big-Endian here.

	 ****************************/
	u8	AddressH   = 0x0f & ( Address >> 16 );
	u8	AddressM   = 0xff & ( Address >> 8 );
	u8	AddressL   = 0xff & Address;

	u8	reg;
	pr_info("%s %d start \n",__func__,__LINE__);
	LT8911EXB_IIC_Write_byte( 0xff, 0xa6 );
	LT8911EXB_IIC_Write_byte( 0x2b, ( 0x80 | AddressH ) );  //CMD
	LT8911EXB_IIC_Write_byte( 0x2b, AddressM );             //addr[15:8]
	LT8911EXB_IIC_Write_byte( 0x2b, AddressL );             //addr[7:0]
	LT8911EXB_IIC_Write_byte( 0x2b, 0x00 );                 //data lenth
	LT8911EXB_IIC_Write_byte( 0x2b, Data );                 //data
	LT8911EXB_IIC_Write_byte( 0x2c, 0x00 );                 //start Aux

	mdelay(20);                                         //more than 10ms
	reg = LT8911EXB_IIC_Read_byte( 0x25 );

	if( ( reg & 0x0f ) == 0x0c )
	{
		return;
	}
}

#if 0
static void LT8911EXB_read_edid( void )
{
#ifdef _read_edid_
	u8 reg, i, j;
//	bool	aux_reply, aux_ack, aux_nack, aux_defer;
	LT8911EXB_IIC_Write_byte( 0xff, 0xac );
	LT8911EXB_IIC_Write_byte( 0x00, 0x20 ); //Soft Link train
	LT8911EXB_IIC_Write_byte( 0xff, 0xa6 );
	LT8911EXB_IIC_Write_byte( 0x2a, 0x01 );

	/*set edid offset addr*/
	LT8911EXB_IIC_Write_byte( 0x2b, 0x40 ); //CMD
	LT8911EXB_IIC_Write_byte( 0x2b, 0x00 ); //addr[15:8]
	LT8911EXB_IIC_Write_byte( 0x2b, 0x50 ); //addr[7:0]
	LT8911EXB_IIC_Write_byte( 0x2b, 0x00 ); //data lenth
	LT8911EXB_IIC_Write_byte( 0x2b, 0x00 ); //data lenth
	LT8911EXB_IIC_Write_byte( 0x2c, 0x00 ); //start Aux read edid

#ifdef _uart_debug_
	pr_info("\r\n[LK/LCM]" );
	pr_info("\r\n[LK/LCM]Read eDP EDID......" );
#endif
	pr_info("%s %d start \n",__func__,__LINE__);
	mdelay(20);                         //more than 10ms
	reg = LT8911EXB_IIC_Read_byte( 0x25 );
	if( ( reg & 0x0f ) == 0x0c )
	{
		for( j = 0; j < 8; j++ )
		{
			if( j == 7 )
			{
				LT8911EXB_IIC_Write_byte( 0x2b, 0x10 ); //MOT
			}else
			{
				LT8911EXB_IIC_Write_byte( 0x2b, 0x50 );
			}

			LT8911EXB_IIC_Write_byte( 0x2b, 0x00 );
			LT8911EXB_IIC_Write_byte( 0x2b, 0x50 );
			LT8911EXB_IIC_Write_byte( 0x2b, 0x0f );
			LT8911EXB_IIC_Write_byte( 0x2c, 0x00 ); //start Aux read edid
			mdelay(50);                         //more than 50ms

			if( LT8911EXB_IIC_Read_byte( 0x39 ) == 0x31 )
			{
				LT8911EXB_IIC_Read_byte( 0x2b );
				for( i = 0; i < 16; i++ )
				{
					EDID_DATA[j * 16 + i] = LT8911EXB_IIC_Read_byte( 0x2b );
				}

				EDID_Reply = 1;
			}else
			{
				EDID_Reply = 0;
#ifdef _uart_debug_
				pr_info("\r\nno_reply" );
				pr_info("\r\n" );
#endif
				//		print("\r\n*************End***************");
				return;
			}
		}

#ifdef _uart_debug_

		for( i = 0; i < 128; i++ ) //print edid data
		{
			if( ( i % 16 ) == 0 )
			{
				pr_info("\r\n" );
			}
			pr_info(", ", EDID_DATA[i] );
		}

		pr_info("\r\n" );
		pr_info("\r\n[LK/LCM]eDP Timing = { H_FP / H_pluse / H_BP / H_act / H_tol / V_FP / V_pluse / V_BP / V_act / V_tol / D_CLK };" );
		pr_info("\r\n[LK/LCM]eDP Timing = { " );
		EDID_Timing[hfp] = ( ( EDID_DATA[0x41] & 0xC0 ) * 4 + EDID_DATA[0x3e] );
		//Debug_DispNum( (u32)EDID_Timing[hfp] );         // HFB
		pr_info(", " );

		EDID_Timing[hs] = ( ( EDID_DATA[0x41] & 0x30 ) * 16 + EDID_DATA[0x3f] );
		//Debug_DispNum( (u32)EDID_Timing[hs] );          // Hsync Wid
		pr_info(", " );

		EDID_Timing[hbp] = ( ( ( EDID_DATA[0x3a] & 0x0f ) * 0x100 + EDID_DATA[0x39] ) - ( ( EDID_DATA[0x41] & 0x30 ) * 16 + EDID_DATA[0x3f] ) - ( ( EDID_DATA[0x41] & 0xC0 ) * 4 + EDID_DATA[0x3e] ) );
		//Debug_DispNum( (u32)EDID_Timing[hbp] );         // HBP
		pr_info(", " );

		EDID_Timing[hact] = ( ( EDID_DATA[0x3a] & 0xf0 ) * 16 + EDID_DATA[0x38] );
		//Debug_DispNum( (u32)EDID_Timing[hact] );        // H active
		pr_info(", " );

		EDID_Timing[htotal] = ( ( EDID_DATA[0x3a] & 0xf0 ) * 16 + EDID_DATA[0x38] + ( ( EDID_DATA[0x3a] & 0x0f ) * 0x100 + EDID_DATA[0x39] ) );
		//Debug_DispNum( (u32)EDID_Timing[htotal] );      // H total
		pr_info(", " );

		EDID_Timing[vfp] = ( ( EDID_DATA[0x41] & 0x0c ) * 4 + ( EDID_DATA[0x40] & 0xf0 ) / 16 );
		//Debug_DispNum( (u32)EDID_Timing[vfp] );         // VFB
		pr_info(", " );

		EDID_Timing[vs] = ( ( EDID_DATA[0x41] & 0x03 ) * 16 + EDID_DATA[0x40] & 0x0f );
		//Debug_DispNum( (u32)EDID_Timing[vs] );          // Vsync Wid
		pr_info(", " );

		EDID_Timing[vbp] = ( ( ( EDID_DATA[0x3d] & 0x03 ) * 0x100 + EDID_DATA[0x3c] ) - ( ( EDID_DATA[0x41] & 0x03 ) * 16 + EDID_DATA[0x40] & 0x0f ) - ( ( EDID_DATA[0x41] & 0x0c ) * 4 + ( EDID_DATA[0x40] & 0xf0 ) / 16 ) );
		//Debug_DispNum( (u32)EDID_Timing[vbp] );         // VBP
		pr_info(", " );

		EDID_Timing[vact] = ( ( EDID_DATA[0x3d] & 0xf0 ) * 16 + EDID_DATA[0x3b] );
		//Debug_DispNum( (u32)EDID_Timing[vact] );        // V active
		pr_info(", " );

		EDID_Timing[vtotal] = ( ( EDID_DATA[0x3d] & 0xf0 ) * 16 + EDID_DATA[0x3b] + ( ( EDID_DATA[0x3d] & 0x03 ) * 0x100 + EDID_DATA[0x3c] ) );
		//Debug_DispNum( (u32)EDID_Timing[vtotal] );      // V total
		pr_info(", " );

		EDID_Timing[pclk_10khz] = ( EDID_DATA[0x37] * 0x100 + EDID_DATA[0x36] );
		//Debug_DispNum( (u32)EDID_Timing[pclk_10khz] );  // CLK
		pr_info("};" );
		pr_info("\r\n" );
#endif
	}

	return;

#endif
}
#endif

static void LT8911EXB_init( void )
{
	u8	i;
	u8	pcr_pll_postdiv;
	u8	pcr_m;
	u16 Temp16;

	/* init */
	LT8911EXB_IIC_Write_byte( 0xff, 0x81 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x08, 0x7f ); // i2c over aux issue
	LT8911EXB_IIC_Write_byte( 0x49, 0xff ); // enable 0x87xx

	LT8911EXB_IIC_Write_byte( 0xff, 0x82 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x5a, 0x0e ); // GPIO test output

	//for power consumption//
	LT8911EXB_IIC_Write_byte( 0xff, 0x81 );
	LT8911EXB_IIC_Write_byte( 0x05, 0x06 );
	LT8911EXB_IIC_Write_byte( 0x43, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x44, 0x1f );
	LT8911EXB_IIC_Write_byte( 0x45, 0xf7 );
	LT8911EXB_IIC_Write_byte( 0x46, 0xf6 );
	LT8911EXB_IIC_Write_byte( 0x49, 0x7f );

	LT8911EXB_IIC_Write_byte( 0xff, 0x82 );
	LT8911EXB_IIC_Write_byte( 0x12, 0x33 );

	/* mipi Rx analog */
	LT8911EXB_IIC_Write_byte( 0xff, 0x82 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x32, 0x51 );
	LT8911EXB_IIC_Write_byte( 0x35, 0x22 ); //EQ current
	LT8911EXB_IIC_Write_byte( 0x3a, 0x77 ); //EQ 12.5db
	LT8911EXB_IIC_Write_byte( 0x3b, 0x77 ); //EQ 12.5db

	LT8911EXB_IIC_Write_byte( 0x4c, 0x0c );
	LT8911EXB_IIC_Write_byte( 0x4d, 0x00 );

	/* dessc_pcr  pll analog */
	LT8911EXB_IIC_Write_byte( 0xff, 0x82 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x6a, 0x40 );
	LT8911EXB_IIC_Write_byte( 0x6b, PCR_PLL_PREDIV );

	Temp16 = MIPI_Timing[pclk_10khz];

	if( MIPI_Timing[pclk_10khz] < 8800 )
	{
		LT8911EXB_IIC_Write_byte( 0x6e, 0x82 ); //0x44:pre-div = 2 ,pixel_clk=44~ 88MHz
		pcr_pll_postdiv = 0x08;
	}else
	{
		LT8911EXB_IIC_Write_byte( 0x6e, 0x81 ); //0x40:pre-div = 1, pixel_clk =88~176MHz
		pcr_pll_postdiv = 0x04;
	}

	pcr_m = (u8)( Temp16 * pcr_pll_postdiv / 25 / 100 );

	/* dessc pll digital */
	LT8911EXB_IIC_Write_byte( 0xff, 0x85 );     // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0xa9, 0x31 );
	LT8911EXB_IIC_Write_byte( 0xaa, 0x17 );
	LT8911EXB_IIC_Write_byte( 0xab, 0xba );
	LT8911EXB_IIC_Write_byte( 0xac, 0xe1 );
	LT8911EXB_IIC_Write_byte( 0xad, 0x47 );
	LT8911EXB_IIC_Write_byte( 0xae, 0x01 );
	LT8911EXB_IIC_Write_byte( 0xae, 0x11 );

	/* Digital Top */
	LT8911EXB_IIC_Write_byte( 0xff, 0x85 );             // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0xc0, 0x01 );             //select mipi Rx
#ifdef _6bit_
	LT8911EXB_IIC_Write_byte( 0xb0, 0xd0 );             //enable dither
#else
	LT8911EXB_IIC_Write_byte( 0xb0, 0x00 );             // disable dither
#endif

	/* mipi Rx Digital */
	LT8911EXB_IIC_Write_byte( 0xff, 0xd0 );             // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x00, _MIPI_Lane_ % 4 );  // 0: 4 Lane / 1: 1 Lane / 2 : 2 Lane / 3: 3 Lane
	LT8911EXB_IIC_Write_byte( 0x02, 0x08 );             //settle
	LT8911EXB_IIC_Write_byte( 0x08, 0x00 );
//	LT8911EXB_IIC_Write_byte( 0x0a, 0x12 );               //pcr mode

	LT8911EXB_IIC_Write_byte( 0x0c, 0x80 );             //fifo position
	LT8911EXB_IIC_Write_byte( 0x1c, 0x80 );             //fifo position

	//	hs mode:MIPI行采样；vs mode:MIPI帧采样
	LT8911EXB_IIC_Write_byte( 0x24, 0x70 );             // 0x30  [3:0]  line limit	  //pcr mode( de hs vs)

	LT8911EXB_IIC_Write_byte( 0x31, 0x0a );

	/*stage1 hs mode*/
	LT8911EXB_IIC_Write_byte( 0x25, 0x90 );             // 0x80		   // line limit
	LT8911EXB_IIC_Write_byte( 0x2a, 0x3a );             // 0x04		   // step in limit
	LT8911EXB_IIC_Write_byte( 0x21, 0x4f );             // hs_step
	LT8911EXB_IIC_Write_byte( 0x22, 0xff );

	/*stage2 de mode*/
	LT8911EXB_IIC_Write_byte( 0x0a, 0x02 );             //de adjust pre line
	LT8911EXB_IIC_Write_byte( 0x38, 0x02 );             //de_threshold 1
	LT8911EXB_IIC_Write_byte( 0x39, 0x04 );             //de_threshold 2
	LT8911EXB_IIC_Write_byte( 0x3a, 0x08 );             //de_threshold 3
	LT8911EXB_IIC_Write_byte( 0x3b, 0x10 );             //de_threshold 4

	LT8911EXB_IIC_Write_byte( 0x3f, 0x04 );             //de_step 1
	LT8911EXB_IIC_Write_byte( 0x40, 0x08 );             //de_step 2
	LT8911EXB_IIC_Write_byte( 0x41, 0x10 );             //de_step 3
	LT8911EXB_IIC_Write_byte( 0x42, 0x60 );             //de_step 4

	/*stage2 hs mode*/
	LT8911EXB_IIC_Write_byte( 0x1e, 0x01 );             // 0x11
	LT8911EXB_IIC_Write_byte( 0x23, 0xf0 );             // 0x80			   //

	LT8911EXB_IIC_Write_byte( 0x2b, 0x80 );             // 0xa0

#ifdef _Test_Pattern_
	LT8911EXB_IIC_Write_byte( 0x26, ( pcr_m | 0x80 ) );
#else

//	LT8911EXB_IIC_Write_byte( 0x26, pcr_m  );
	LT8911EXB_IIC_Write_byte( 0x26, 0x15 );

	LT8911EXB_IIC_Write_byte( 0x27, 0xb7 );
	LT8911EXB_IIC_Write_byte( 0x28, 0xb0 );
#endif

	LT8911EXB_MIPI_Video_Timing( );         //defualt setting is 1080P

	LT8911EXB_IIC_Write_byte( 0xff, 0x81 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x03, 0x7b ); //PCR reset
	LT8911EXB_IIC_Write_byte( 0x03, 0xff );

#ifdef _eDP_2G7_
	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x19, 0x31 );
	LT8911EXB_IIC_Write_byte( 0x1a, 0x36 ); // sync m
	LT8911EXB_IIC_Write_byte( 0x1b, 0x00 ); // sync_k [7:0]
	LT8911EXB_IIC_Write_byte( 0x1c, 0x00 ); // sync_k [13:8]

	// txpll Analog
	LT8911EXB_IIC_Write_byte( 0xff, 0x82 );
//	LT8911EXB_IIC_Write_byte( 0x01, 0x18 );// default : 0x18
	LT8911EXB_IIC_Write_byte( 0x02, 0x42 );
	LT8911EXB_IIC_Write_byte( 0x03, 0x00 ); // txpll en = 0
	LT8911EXB_IIC_Write_byte( 0x03, 0x01 ); // txpll en = 1
//	LT8911EXB_IIC_Write_byte( 0x04, 0x3a );// default : 0x3A

	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x0c, 0x10 ); // cal en = 0

	LT8911EXB_IIC_Write_byte( 0xff, 0x81 );
	LT8911EXB_IIC_Write_byte( 0x09, 0xfc );
	LT8911EXB_IIC_Write_byte( 0x09, 0xfd );

	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x0c, 0x11 ); // cal en = 1

	// ssc
	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x13, 0x83 );
	LT8911EXB_IIC_Write_byte( 0x14, 0x41 );
	LT8911EXB_IIC_Write_byte( 0x16, 0x0a );
	LT8911EXB_IIC_Write_byte( 0x18, 0x0a );
	LT8911EXB_IIC_Write_byte( 0x19, 0x33 );
#endif

#ifdef _eDP_1G62_
	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x19, 0x31 );
	LT8911EXB_IIC_Write_byte( 0x1a, 0x20 ); // sync m
	LT8911EXB_IIC_Write_byte( 0x1b, 0x19 ); // sync_k [7:0]
	LT8911EXB_IIC_Write_byte( 0x1c, 0x99 ); // sync_k [13:8]

	// txpll Analog
	LT8911EXB_IIC_Write_byte( 0xff, 0x82 );
	//	LT8911EXB_IIC_Write_byte( 0x01, 0x18 );// default : 0x18
	LT8911EXB_IIC_Write_byte( 0x02, 0x42 );
	LT8911EXB_IIC_Write_byte( 0x03, 0x00 ); // txpll en = 0
	LT8911EXB_IIC_Write_byte( 0x03, 0x01 ); // txpll en = 1
	//	LT8911EXB_IIC_Write_byte( 0x04, 0x3a );// default : 0x3A

	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x0c, 0x10 ); // cal en = 0

	LT8911EXB_IIC_Write_byte( 0xff, 0x81 );
	LT8911EXB_IIC_Write_byte( 0x09, 0xfc );
	LT8911EXB_IIC_Write_byte( 0x09, 0xfd );

	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x0c, 0x11 ); // cal en = 1

	//ssc
	LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
	LT8911EXB_IIC_Write_byte( 0x13, 0x83 );
	LT8911EXB_IIC_Write_byte( 0x14, 0x41 );
	LT8911EXB_IIC_Write_byte( 0x16, 0x0a );
	LT8911EXB_IIC_Write_byte( 0x18, 0x0a );
	LT8911EXB_IIC_Write_byte( 0x19, 0x33 );
#endif
	pr_info("%s %d start \n",__func__,__LINE__);
	for( i = 0; i < 5; i++ )      //Check Tx PLL
	{
		mdelay(5);
		if( LT8911EXB_IIC_Read_byte( 0x37 ) & 0x02 )
		{
			pr_info("\r\nLT8911 tx pll locked" );
			break;
		}else
		{
			pr_info("\r\nLT8911 tx pll unlocked" );
			LT8911EXB_IIC_Write_byte( 0xff, 0x81 );
			LT8911EXB_IIC_Write_byte( 0x09, 0xfc );
			LT8911EXB_IIC_Write_byte( 0x09, 0xfd );

			LT8911EXB_IIC_Write_byte( 0xff, 0x87 );
			LT8911EXB_IIC_Write_byte( 0x0c, 0x10 );
			LT8911EXB_IIC_Write_byte( 0x0c, 0x11 );
		}
	}
	/* tx phy */
	LT8911EXB_IIC_Write_byte( 0xff, 0x82 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x11, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x13, 0x10 );
	LT8911EXB_IIC_Write_byte( 0x14, 0x0c );
	LT8911EXB_IIC_Write_byte( 0x14, 0x08 );
	LT8911EXB_IIC_Write_byte( 0x13, 0x20 );

	LT8911EXB_IIC_Write_byte( 0xff, 0x82 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x0e, 0x35 );
//	LT8911EXB_IIC_Write_byte( 0x12, 0xff );
//	LT8911EXB_IIC_Write_byte( 0xff, 0x80 );
//	LT8911EXB_IIC_Write_byte( 0x40, 0x22 );

	/*eDP Tx Digital */
	LT8911EXB_IIC_Write_byte( 0xff, 0xa8 ); // Change Reg bank
#ifdef _Test_Pattern_
	LT8911EXB_IIC_Write_byte( 0x24, 0x50 ); // bit2 ~ bit 0 : test panttern image mode
	LT8911EXB_IIC_Write_byte( 0x25, 0x70 ); // bit6 ~ bit 4 : test Pattern color
	LT8911EXB_IIC_Write_byte( 0x27, 0x50 ); //0x50:Pattern; 0x10:mipi video
#else
	LT8911EXB_IIC_Write_byte( 0x27, 0x10 ); //0x50:Pattern; 0x10:mipi video
#endif

#ifdef _6bit_
	LT8911EXB_IIC_Write_byte( 0x17, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x18, 0x00 );
#else
	// _8bit_
	LT8911EXB_IIC_Write_byte( 0x17, 0x10 );
	LT8911EXB_IIC_Write_byte( 0x18, 0x20 );
#endif

	LT8911EXB_IIC_Write_byte( 0xff, 0xa0 ); // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x00, 0x00 );// 0x08
	LT8911EXB_IIC_Write_byte( 0x01, 0x80 );// 0x00
}


static void LT8911EXB_eDP_Video_cfg( void )                                        // ( struct video_timing *video_format )
{
	LT8911EXB_IIC_Write_byte( 0xff, 0xa8 );
	LT8911EXB_IIC_Write_byte( 0x2d, 0x88 );                                 // MSA from register
	pr_info("%s %d start \n",__func__,__LINE__);
#ifdef _Msa_Active_Only_
	LT8911EXB_IIC_Write_byte( 0x05, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x06, 0x00 );                                 //htotal
	LT8911EXB_IIC_Write_byte( 0x07, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x08, 0x00 );                                 //h_start
	LT8911EXB_IIC_Write_byte( 0x09, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x0a, 0x00 );                                 //hsa
	LT8911EXB_IIC_Write_byte( 0x0b, (u8)( MIPI_Timing[hact] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0c, (u8)( MIPI_Timing[hact] % 256 ) );      //hactive
	LT8911EXB_IIC_Write_byte( 0x0d, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x0e, 0x00 );                                 //vtotal
	LT8911EXB_IIC_Write_byte( 0x11, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x12, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x14, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x15, (u8)( MIPI_Timing[vact] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x16, (u8)( MIPI_Timing[vact] % 256 ) );      //vactive
	pr_info("%s %d 1111 \n",__func__,__LINE__);
#else

	LT8911EXB_IIC_Write_byte( 0x05, (u8)( MIPI_Timing[htotal] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x06, (u8)( MIPI_Timing[htotal] % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x07, (u8)( ( MIPI_Timing[hs] + MIPI_Timing[hbp] ) / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x08, (u8)( ( MIPI_Timing[hs] + MIPI_Timing[hbp] ) % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x09, (u8)( MIPI_Timing[hs] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0a, (u8)( MIPI_Timing[hs] % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0b, (u8)( MIPI_Timing[hact] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0c, (u8)( MIPI_Timing[hact] % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0d, (u8)( MIPI_Timing[vtotal] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x0e, (u8)( MIPI_Timing[vtotal] % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x11, (u8)( ( MIPI_Timing[vs] + MIPI_Timing[vbp] ) / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x12, (u8)( ( MIPI_Timing[vs] + MIPI_Timing[vbp] ) % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x14, (u8)( MIPI_Timing[vs] % 256 ) );
	LT8911EXB_IIC_Write_byte( 0x15, (u8)( MIPI_Timing[vact] / 256 ) );
	LT8911EXB_IIC_Write_byte( 0x16, (u8)( MIPI_Timing[vact] % 256 ) );
		pr_info("%s %d 2222 \n",__func__,__LINE__);
#endif
}

static void LT8911EX_ChipID( void )                                                        // read Chip ID
{
	//pr_info("liwy  %s\n", __func__);
//	//pr_info("\r\n###################start#####################" );
	LT8911EXB_IIC_Write_byte( 0xff, 0x81 );                                         //register bank
	LT8911EXB_IIC_Write_byte( 0x08, 0x7f );
	pr_info("%s %d start \n",__func__,__LINE__);
#ifdef _uart_debug_
	pr_info("LT8911EXB chip ID: 0x%x", LT8911EXB_IIC_Read_byte( 0x00 ) );  // 0x17
	pr_info(",  ID: 0x%x", LT8911EXB_IIC_Read_byte( 0x01 ) );                      // 0x05
	pr_info(",  ID: 0x%x\n", LT8911EXB_IIC_Read_byte( 0x02 ) );                      // 0xE0
#endif
}

static u8 DpcdRead( u32 Address )
{
	/***************************
	   注意大小端的问题!
	   这里默认是大端模式

	   Pay attention to the Big-Endian and Little-Endian!
	   The default mode is Big-Endian here.

	 ****************************/

	u8	DpcdValue  = 0x00;
	u8	AddressH   = 0x0f & ( Address >> 16 );
	u8	AddressM   = 0xff & ( Address >> 8 );
	u8	AddressL   = 0xff & Address;
	u8	reg;

	LT8911EXB_IIC_Write_byte( 0xff, 0xac );
	LT8911EXB_IIC_Write_byte( 0x00, 0x20 );                 //Soft Link train
	LT8911EXB_IIC_Write_byte( 0xff, 0xa6 );
	LT8911EXB_IIC_Write_byte( 0x2a, 0x01 );
	pr_info("%s %d start \n",__func__,__LINE__);
	LT8911EXB_IIC_Write_byte( 0xff, 0xa6 );
	LT8911EXB_IIC_Write_byte( 0x2b, ( 0x90 | AddressH ) );  //CMD
	LT8911EXB_IIC_Write_byte( 0x2b, AddressM );             //addr[15:8]
	LT8911EXB_IIC_Write_byte( 0x2b, AddressL );             //addr[7:0]
	LT8911EXB_IIC_Write_byte( 0x2b, 0x00 );                 //data lenth
	LT8911EXB_IIC_Write_byte( 0x2c, 0x00 );                 //start Aux read edid

	mdelay(50);                                         //more than 10ms
	reg = LT8911EXB_IIC_Read_byte( 0x25 );
	if( ( reg & 0x0f ) == 0x0c )
	{
		if( LT8911EXB_IIC_Read_byte( 0x39 ) == 0x22 )
		{
			LT8911EXB_IIC_Read_byte( 0x2b );
			DpcdValue = LT8911EXB_IIC_Read_byte( 0x2b );
		}
		/*
		   else
		   {
		   //	goto no_reply;
		   //	DpcdValue = 0xff;
		   return DpcdValue;
		   }//*/
	}

	return DpcdValue;
}

static void LT8911EX_link_train( void )
{
	LT8911EXB_IIC_Write_byte( 0xff, 0x85 );
	pr_info("%s %d start \n",__func__,__LINE__);
//#ifdef _eDP_scramble_
	if( ScrambleMode )
	{
		LT8911EXB_IIC_Write_byte( 0xa1, 0x82 ); // eDP scramble mode;

		/* Aux operater init */
		LT8911EXB_IIC_Write_byte( 0xff, 0xac );
		LT8911EXB_IIC_Write_byte( 0x00, 0x20 ); //Soft Link train
		LT8911EXB_IIC_Write_byte( 0xff, 0xa6 );
		LT8911EXB_IIC_Write_byte( 0x2a, 0x01 );

		DpcdWrite( 0x010a, 0x01 );
		mdelay(10);
		DpcdWrite( 0x0102, 0x00 );
		mdelay(10);
		DpcdWrite( 0x010a, 0x01 );

		//mdelay(200); //jnier del 20230815
	}
//#else
	else
	{
		LT8911EXB_IIC_Write_byte( 0xa1, 0x02 ); // DP scramble mode;
	}
//#endif

	/* Aux setup */
	LT8911EXB_IIC_Write_byte( 0xff, 0xac );
	LT8911EXB_IIC_Write_byte( 0x00, 0x60 );     //Soft Link train
	LT8911EXB_IIC_Write_byte( 0xff, 0xa6 );
	LT8911EXB_IIC_Write_byte( 0x2a, 0x00 );

	LT8911EXB_IIC_Write_byte( 0xff, 0x81 );
	LT8911EXB_IIC_Write_byte( 0x07, 0xfe );
	LT8911EXB_IIC_Write_byte( 0x07, 0xff );
	LT8911EXB_IIC_Write_byte( 0x0a, 0xfc );
	LT8911EXB_IIC_Write_byte( 0x0a, 0xfe );

	/* link train */

	LT8911EXB_IIC_Write_byte( 0xff, 0x85 );
	LT8911EXB_IIC_Write_byte( 0x1a, eDP_lane );

	LT8911EXB_IIC_Write_byte( 0xff, 0xac );
	LT8911EXB_IIC_Write_byte( 0x00, 0x64 );
	LT8911EXB_IIC_Write_byte( 0x01, 0x0a );
	LT8911EXB_IIC_Write_byte( 0x0c, 0x85 );
	LT8911EXB_IIC_Write_byte( 0x0c, 0xc5 );
//	mdelay(500);
}

static void LT8911EX_link_train_result( void )
{
	u8 i, reg;
	LT8911EXB_IIC_Write_byte( 0xff, 0xac );
	for( i = 0; i < 10; i++ )
	{
		reg = LT8911EXB_IIC_Read_byte( 0x82 );
		//  pr_info("\r\n0x82 = ", reg );
		if( reg & 0x20 )
		{
			if( ( reg & 0x1f ) == 0x1e )
			{
				pr_info("\r\nLink train success, 0x82 = ", reg );
			} else
			{
				pr_info("\r\nLink train fail, 0x82 = ", reg );
			}

			pr_info("\r\n panel link rate: ", LT8911EXB_IIC_Read_byte( 0x83 ) );
			pr_info("\r\n panel link count: ", LT8911EXB_IIC_Read_byte( 0x84 ) );
			return;
		}else
		{
			pr_info("\r\n  link trian on going..." );
		}
		//mdelay(100); //jnier del 20230815
	}
}

static void LT8911EX_TxSwingPreSet( void )
{
	pr_info("%s %d start \n",__func__,__LINE__);
	LT8911EXB_IIC_Write_byte( 0xFF, 0x82 );
	LT8911EXB_IIC_Write_byte( 0x22, Swing_Setting1[Level] ); //lane 0 tap0
	LT8911EXB_IIC_Write_byte( 0x23, Swing_Setting2[Level] );
	LT8911EXB_IIC_Write_byte( 0x24, 0x80 );                  //lane 0 tap1
	LT8911EXB_IIC_Write_byte( 0x25, 0x00 );
	LT8911EXB_IIC_Write_byte( 0x26, Swing_Setting1[Level] ); //lane 1 tap0
	LT8911EXB_IIC_Write_byte( 0x27, Swing_Setting2[Level] );
	LT8911EXB_IIC_Write_byte( 0x28, 0x80 );                  //lane 1 tap1
	LT8911EXB_IIC_Write_byte( 0x29, 0x00 );
}

static void LT8911EXB_video_check( void )
{
	//u8	temp;
	u32 reg = 0x00;
	/* mipi byte clk check*/
	LT8911EXB_IIC_Write_byte( 0xff, 0x85 );     // Change Reg bank
	LT8911EXB_IIC_Write_byte( 0x1d, 0x00 );     //FM select byte clk
	LT8911EXB_IIC_Write_byte( 0x40, 0xf7 );
	LT8911EXB_IIC_Write_byte( 0x41, 0x30 );
	pr_info("%s %d start \n",__func__,__LINE__);
	//#ifdef _eDP_scramble_
	if( ScrambleMode )
	{
		LT8911EXB_IIC_Write_byte( 0xa1, 0x82 ); //eDP scramble mode;
	}
	//#else
	else
	{
		LT8911EXB_IIC_Write_byte( 0xa1, 0x02 ); // DP scramble mode;
	}
	//#endif

//	LT8911EXB_IIC_Write_byte( 0x17, 0xf0 ); // 0xf0:Close scramble; 0xD0 : Open scramble

	LT8911EXB_IIC_Write_byte( 0xff, 0x81 );
	LT8911EXB_IIC_Write_byte( 0x09, 0x7d );
	LT8911EXB_IIC_Write_byte( 0x09, 0xfd );

	LT8911EXB_IIC_Write_byte( 0xff, 0x85 );
	//mdelay(200);  //jnier del 20230815
	if( LT8911EXB_IIC_Read_byte( 0x50 ) == 0x03 )
	{
		reg	   = LT8911EXB_IIC_Read_byte( 0x4d );
		reg	   = reg * 256 + LT8911EXB_IIC_Read_byte( 0x4e );
		reg	   = reg * 256 + LT8911EXB_IIC_Read_byte( 0x4f );

		pr_info("video check: mipi byteclk =  %d \n", reg * 1000); // mipi byteclk = reg * 1000
		//Debug_DispNum( reg );
	}else
	{
		pr_info("\r\n video check: mipi clk unstable" );
	}

	/* mipi vtotal check*/
	reg	   = LT8911EXB_IIC_Read_byte( 0x76 );
	reg	   = reg * 256 + LT8911EXB_IIC_Read_byte( 0x77 );

	pr_info("\r\n  video check: Vtotal =  0x%x" , reg);
	//Debug_DispNum( reg );

	/* mipi word count check*/
	LT8911EXB_IIC_Write_byte( 0xff, 0xd0 );
	reg	   = LT8911EXB_IIC_Read_byte( 0x82 );
	reg	   = reg * 256 + LT8911EXB_IIC_Read_byte( 0x83 );
	reg	   = reg / 3;

	pr_info("\r\n  video check: Hact(word counter) = 0x%x " , reg );
	//Debug_DispNum( reg );

	/* mipi Vact check*/
	reg	   = LT8911EXB_IIC_Read_byte( 0x85 );
	reg	   = reg * 256 + LT8911EXB_IIC_Read_byte( 0x86 );

	pr_info("\r\n  video check: Vact =  0x%x" , reg );
	//Debug_DispNum( reg );
}

static void PCR_Status( void )                     // for debug
{
#ifdef _uart_debug_
	u8 reg;

	LT8911EXB_IIC_Write_byte( 0xff, 0xd0 );
	reg = LT8911EXB_IIC_Read_byte( 0x87 );

	pr_info("Reg0xD087 = 0x%x \n", reg);

	if( reg & 0x10 )
	{
		pr_info("PCR Clock stable\n" );
	}else
	{
		pr_info("PCR Clock unstable\n" );
	}
#endif
}

static void LT8911EXB_config(void)
{
	pr_info("LT8911EXB_config enter\n");
	pr_info("%s %d start \n",__func__,__LINE__);
//	Reset_LT8911EXB();     // 先Reset LT8911EXB ,用GPIO 先拉低LT8911EXB的复位脚 100ms左右，再拉高，保持100ms。
	LT8911EX_ChipID();     // read Chip ID
	LT8911EXB_eDP_Video_cfg();
	LT8911EXB_init();
//	LT8911EXB_read_edid(); // for debug
	Read_DPCD010A = DpcdRead( 0x010A ) & 0x01;

#ifdef _uart_debug_
	//pr_info("\r\n" );
	//pr_info("\r\nDPCD010Ah:%x,", Read_DPCD010A );
#endif

	if( Read_DPCD010A )
	{
		ScrambleMode = 1;
	}else
	{
		ScrambleMode = 0;
	}

	LT8911EX_link_train();
	LT8911EX_link_train_result();  // for debug
	LT8911EX_TxSwingPreSet();
	LT8911EXB_video_check();       // just for Check MIPI Input
	PCR_Status();                  // just for Check PCR CLK
	pr_info("%s %d end \n",__func__,__LINE__);
/*
   while( 1 )
   {
   // 循环检测MIPI信号，如果有断续，会出现reset PCR
   //Loop detection of Mipi signal. If there is interruption, reset PCR will appear
   LT8911_MainLoop( );

   mdelay(1000);
   }
   //*/
}
#endif /*CONFIG_WB_LT8911EXB*/

static const struct panel_init_cmd cm_init_cmd[] = {
	
	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DELAY_CMD(20),
	{} //it must be here
};

//#define LCM_5
//#define LCM_6
//#define LCM_7
//#define LCM_8
//#define LCM_101
//#define LCM_1036
#define LCM_105
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

#ifdef LCM_105
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


#define LCM_WIDTH		2496
#define LCM_HEIGHT		1664

#define VSA				10
#define VBP				35
#define VFP				3

#define HSA				32
#define HBP				80
#define HFP				48

#define FPS				60

#define PLL_CLK			450


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
	.bpc = 11,
	.size = {
		.width_mm = WIDTH_MM,
		.height_mm = HEIGHT_MM,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
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
	.ssc_enable = 0,
	.data_rate = PLL_CLK*2,
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
	cust_gpio_set_value(CUST_GPIO_LCM_RST, 0); 
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, 0);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_LCM_3V3, 0); //3.3
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_EDP_IRQO2, 0); 
	mdelay(1); 

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

	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, 1);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_3V3, 1); //3.3
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_EDP_IRQO2, 1); 
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, 1); 
	mdelay(70);

#if IS_ENABLED(CONFIG_WB_LT8911EXB) //Leo 20231221
	LT8911EXB_config();
#endif
	mdelay(250);

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

	if (0) {
		ret = common_panel_init_dcs_cmd(cm);
		if (ret < 0) {
			pr_notice("failed to init panel: %d\n", ret);
			return ret;
		}
	}

	cm->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

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


struct cust_drm_lcm m100t_sharp_lt8911exb_lq150p1jx01_wqxga2496 = {
	.name = "m100t_sharp_lt8911exb_lq150p1jx01_wqxga2496",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
