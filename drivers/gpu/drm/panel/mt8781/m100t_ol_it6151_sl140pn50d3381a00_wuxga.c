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

#if IS_ENABLED(CONFIG_WB_IT6151FN)
#define DP_I2C_ADDR 	(0x5C << 0)
#define MIPI_I2C_ADDR 	(0x6C << 0)

#ifdef BUILD_LK
#define IT6151_BUSNUM   I2C6

static u32 it6151_i2c_write_byte(u8 dev_addr,u8 addr, u8 data)
{
    u32 ret_code = I2C_OK;
    u8 write_data[I2C_FIFO_SIZE], len;
    struct mt_i2c_t i2c;
    
    i2c.id = IT6151_BUSNUM;
    i2c.addr = dev_addr;
    i2c.mode = ST_MODE;
    i2c.speed = 100;

    write_data[0]= addr;
    write_data[1] = data;
    len = 2;

    #ifdef IT6151_DEBUG

    printf("[it6151_i2c_write] dev_addr = 0x%x, write_data[0x%x] = 0x%x \n", dev_addr, write_data[0], write_data[1]);
    #endif
    
    ret_code = i2c_write(&i2c, write_data, len);

    return ret_code;
}

static u32 it6151_i2c_read_byte(u8 dev_addr,u8 addr, u8 *dataBuffer)
{
    u32 ret_code = I2C_OK;
    u8 len;
    struct mt_i2c_t i2c;
    
    *dataBuffer = addr;

    i2c.id = IT6151_BUSNUM;
    i2c.addr = dev_addr;
    i2c.mode = ST_MODE;
    i2c.speed = 100;
    len = 1;

    ret_code = i2c_write_read(&i2c, dataBuffer, len, len);

    #ifdef IT6151_DEBUG
    /* dump write_data for check */
    printf("[it6151_read_byte] dev_addr = 0x%x, read_data[0x%x] = 0x%x \n", dev_addr, addr, *dataBuffer);
    #endif
    
    return ret_code;
}
 
 /******************************************************************************
 *IIC drvier,:protocol type 2 add by chenguangjian end
 ******************************************************************************/
#else
extern int it6151_i2c_read_byte(u8 dev_addr, u8 addr, u8 *returnData);
extern int it6151_i2c_write_byte(u8 dev_addr, u8 addr, u8 writeData);
#endif
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////
///       for it6151 defines start                   ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////


//#define PANEL_RESOLUTION_1280x800_NOUFO
//#define PANEL_RESOLUTION_2048x1536_NOUFO_18B
//#define PANEL_RESOLUTION_2048x1536
// #define PANEL_RESOLUTION_2048x1536_NOUFO // FOR INTEL Platform
#define PANEL_RESOLUTION_1920x1200p60RB
//#define PANEL_RESOLUTION_1920x1080p60
//#define PANEL_RESULUTION_1536x2048

#define MIPI_4_LANE     (3)
#define MIPI_3_LANE     (2)
#define MIPI_2_LANE     (1)
#define MIPI_1_LANE     (0)

// MIPI Packed Pixel Stream
#define RGB_24b         (0x3E)
#define RGB_30b         (0x0D)
#define RGB_36b         (0x1D)
#define RGB_18b_P       (0x1E)
#define RGB_18b_L       (0x2E)
#define YCbCr_16b       (0x2C)
#define YCbCr_20b       (0x0C)
#define YCbCr_24b       (0x1C)

// DPTX reg62[3:0]
#define B_DPTXIN_6Bpp   (0)
#define B_DPTXIN_8Bpp   (1)
#define B_DPTXIN_10Bpp  (2)
#define B_DPTXIN_12Bpp  (3)

#define B_LBR           (1)
#define B_HBR           (0)

#define B_4_LANE        (3)
#define B_2_LANE        (1)
#define B_1_LANE        (0)

#define B_SSC_ENABLE   	(1)
#define B_SSC_DISABLE   (0)

///////////////////////////////////////////////////////////////////////////
//CONFIGURE
///////////////////////////////////////////////////////////////////////////
#define TRAINING_BITRATE    (B_HBR)//(B_LBR)
#define DPTX_SSC_SETTING    (B_SSC_ENABLE)//(B_SSC_DISABLE)
#define HIGH_PCLK           (1)
#define MP_MCLK_INV         (1)
#define MP_CONTINUOUS_CLK   (1)
#define MP_LANE_DESKEW      (1)
#define MP_PCLK_DIV			(2)
#define MP_LANE_SWAP        (0)
#define MP_PN_SWAP          (0)

#define DP_PN_SWAP          (0)
#define DP_AUX_PN_SWAP      (0)
#define DP_LANE_SWAP        (0) //(0) our convert board need to LANE SWAP for data lane
#define FRAME_RESYNC        (0)
#define LVDS_LANE_SWAP      (0)
#define LVDS_PN_SWAP        (0)
#define LVDS_DC_BALANCE     (0)

#define LVDS_6BIT           (0) // '0' for 8 bit, '1' for 6 bit
#define VESA_MAP            (0) // '0' for JEIDA , '1' for VESA MAP

#define INT_MASK            (3)
#define MIPI_INT_MASK       (0)
#define MIPI_EVENT_MODE		(0)
#define	MIPI_HSYNC_W		(32)
#define MIPI_VSYNC_W		(5)
#define TIMER_CNT           (0x0A)
///////////////////////////////////////////////////////////////////////
// Global Setting
///////////////////////////////////////////////////////////////////////
#ifdef PANEL_RESOLUTION_1280x800_NOUFO
#define PANEL_WIDTH 1280
#define VIC 0
#define MP_HPOL 0
#define MP_VPOL 1
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp
#define EN_UFO 0
#define MIPI_PACKED_FMT     RGB_24b
#define MP_H_RESYNC         1
#define MP_V_RESYNC         0
#endif

#ifdef PANEL_RESOLUTION_1920x1080p60
#define PANEL_WIDTH 1920
#define VIC 0x10
#define MP_HPOL 1
#define MP_VPOL 1
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp // B_DPTXIN_8Bpp // B_DPTXIN_6Bpp
#define EN_UFO 0
#define MIPI_PACKED_FMT     RGB_24b // RGB_24b // RGB_18b_L
#define MP_H_RESYNC     1 //    1
#define MP_V_RESYNC     0 //0
#endif

#ifdef PANEL_RESOLUTION_1920x1200p60RB
#define PANEL_WIDTH 1920
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 1
#define MP_VPOL 0
#define DPTX_LANE_COUNT  B_2_LANE
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp
#define EN_UFO 0
#define MIPI_PACKED_FMT     RGB_24b
#define MP_H_RESYNC         1
#define MP_V_RESYNC         0
#endif

#ifdef PANEL_RESOLUTION_2048x1536
#define PANEL_WIDTH 2048
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp
#define EN_UFO 1
#define MIPI_PACKED_FMT     RGB_24b
#define MP_H_RESYNC         0
#define MP_V_RESYNC         0
#endif

#ifdef PANEL_RESOLUTION_2048x1536_NOUFO
#define PANEL_WIDTH 2048
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp
#define EN_UFO 0
#define MIPI_PACKED_FMT     RGB_24b
#define MP_H_RESYNC         1
#define MP_V_RESYNC         0
#endif

#ifdef PANEL_RESOLUTION_2048x1536_NOUFO_18B
#define PANEL_WIDTH 2048
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp
#define EN_UFO 0
#define MIPI_PACKED_FMT     RGB_18b_P
#define MP_H_RESYNC         1
#define MP_V_RESYNC         0
#endif

#ifdef PANEL_RESULUTION_1536x2048
#define PANEL_WIDTH 1536
#define VIC 0 // non-Zero value for CEA setting, check the given input format.
#define MP_HPOL 0
#define MP_VPOL 1
#define MIPI_LANE_COUNT  MIPI_4_LANE
#define DPTX_LANE_COUNT  B_4_LANE
#define DPTX_BPP         B_DPTXIN_8Bpp
#define EN_UFO 1
#define MIPI_PACKED_FMT     RGB_24b
#define MP_H_RESYNC         1
#define MP_V_RESYNC         0
#endif
#ifdef PANEL_RESOLUTION_1366x768
#define PANEL_WIDTH 1366
#define VIC 0x10
#define MP_HPOL 0
#define MP_VPOL 0
#define DPTX_LANE_COUNT  B_1_LANE
#define MIPI_LANE_COUNT  MIPI_2_LANE
#define DPTX_BPP         B_DPTXIN_6Bpp
#define EN_UFO 0
#define MIPI_PACKED_FMT     RGB_24b
#define MP_H_RESYNC         1
#define MP_V_RESYNC         0
#endif

///////////////////////////////////////////////////////////////////////////

//#define DP_I2C_ADDR 0x5C
//#define MIPI_I2C_ADDR 0x6C

/////////////////////////////////////////////////////////////////////
///       for it6151 defines end                   /////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// Function
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void IT6151_DPTX_init(void)
{   
#ifndef BUILD_LK
    printk("[KERNEL/LCM] IT6151_DPTX_init !!!\n");
#else
    printf("[LK/LCM] IT6151_DPTX_init\n");
#endif  
    it6151_i2c_write_byte(DP_I2C_ADDR,0x05,0x29);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x05,0x00);
    
    it6151_i2c_write_byte(DP_I2C_ADDR,0x09,INT_MASK);// Enable HPD_IRQ,HPD_CHG,VIDSTABLE
    it6151_i2c_write_byte(DP_I2C_ADDR,0x0A,0x00);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x0B,0x00);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xC5,0xC1);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xB5,0x00);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xB7,0x80);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xC4,0xF0);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x06,0xFF);// Clear all interrupt
    it6151_i2c_write_byte(DP_I2C_ADDR,0x07,0xFF);// Clear all interrupt
    it6151_i2c_write_byte(DP_I2C_ADDR,0x08,0xFF);// Clear all interrupt
    
    it6151_i2c_write_byte(DP_I2C_ADDR,0x05,0x00);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x0c,0x08);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x21,0x05);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x3a,0x04);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x5f,0x06);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xc9,0xf5);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xca,0x4c);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xcb,0x37);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xce,0x80);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xd3,0x03);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xd4,0x60);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xe8,0x11);
    it6151_i2c_write_byte(DP_I2C_ADDR,0xec,VIC);
    mdelay(5);          

    it6151_i2c_write_byte(DP_I2C_ADDR,0x62,DPTX_BPP);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x23,0x42);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x24,0x07);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x25,0x01);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x26,0x00);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x27,0x10);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x2B,0x05);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x23,0x40);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x22,(DP_AUX_PN_SWAP<<3)|(DP_PN_SWAP<<2)|0x03);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x16,(DPTX_SSC_SETTING<<4)|(DP_LANE_SWAP<<3)|(DPTX_LANE_COUNT<<1)|TRAINING_BITRATE);
    //it6151_i2c_write_byte(DP_I2C_ADDR,0x62,0x00);  //rgb 6bit 
    it6151_i2c_write_byte(DP_I2C_ADDR,0x0f,0x01);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x76,0xa7);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x77,0xaf);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x7e,0x8f);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x7f,0x07);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x80,0xef);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x81,0x5f);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x82,0xef);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x83,0x07);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x88,0x38);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x89,0x1f);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x8a,0x48);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x0f,0x00);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x5c,0xf3);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x17,0x04);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x17,0x01);
    mdelay(5);  
}

void video_check(void)
{
	u16 hsync_start,vsync_start,vs,vfp,vbp,hs,hbp,hfp,htotal,vtotal;
	u16 hsync_start1,vsync_start1,vs1,vfp1,vbp1,hs1,hbp1,hfp1,htotal1,vtotal1;

	u8 hsync_start2,vsync_start2,vs2,vfp2,vbp2,hs2,hbp2,hfp2,htotal2,vtotal2;
	u8 hsync_start3,vsync_start3,vs3,vfp3,vbp3,hs3,hbp3,hfp3,htotal3,vtotal3;

 	u8 mipi_state;
 
 	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x0D,&mipi_state);
 	printk("\r\n mipi 0x0d is :%x\r\n",mipi_state);
 	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x0c,&mipi_state);
 	printk("\r\n mipi 0x0c is :%x\r\n",mipi_state);

 	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x30,&hsync_start2);
	hsync_start = hsync_start2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x31,&hsync_start2);
 	hsync_start += (hsync_start2 & 0xF)<<8;;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x32,&hs2);
	hs = hs2 ;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x33,&hs2);
	hs += (hs2 & 0xF)<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x34,&hbp2);
	hbp = hbp2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x35,&hbp2);
	hbp += (hbp2 & 0xF)<<8;
 	

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x36,&hfp2);
	hfp = hfp2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x37,&hfp2);
	hfp += (hfp2 & 0xF)<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x38,&htotal2);
	htotal = htotal2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x39,&htotal2);
	htotal += (htotal2 & 0xF)<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x3a,&vsync_start2);
	vsync_start = vsync_start2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x3b,&vsync_start2);
	vsync_start += (vsync_start2 & 0xF)<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x3c,&vs2);
	vs = vs2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x3d,&vs2);
	vs += (vs2 & 0xF)<<8;
	
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x3e,&vbp2);
	vbp = vbp2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x3f,&vbp2);
	vbp += (vbp2 & 0xF)<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x40,&vfp2);
	vfp = vfp2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x41,&vfp2);
	vfp += (vfp2 & 0xF)<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x42,&vtotal2);
	vtotal = vtotal2;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x43,&vtotal2);
	vtotal += (vtotal2 & 0xF)<<8;

 	printk("\r\n hsync_start,hs,hfp,hbp,htotal is:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
 		hsync_start,hs,hbp,hfp,htotal,vsync_start,vs,vbp,vfp,vtotal);

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x50,&hsync_start3);
	hsync_start1 = hsync_start3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x51,&hsync_start3);
	hsync_start1 += hsync_start3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x52,&hs3);
	hs1 = hs3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x53,&hs3);
	hs1 += hs3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x54,&hbp3);
	hbp1 = hbp3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x55,&hbp3);
	hbp1 += hbp3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x56,&hfp3);
	hfp1 = hfp3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x57,&hfp3);
	hfp1 += hfp3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x58,&htotal3);
	htotal1 = htotal3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x59,&htotal3);
	htotal1 += htotal3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x5a,&vsync_start3);
	vsync_start1 = vsync_start3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x5b,&vsync_start3);
	vsync_start1 += vsync_start3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x5c,&vs3);
	vs1 = vs3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x5d,&vs3);
	vs1 += vs3<<8;
	
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x5e,&vbp3);
	vbp1 = vbp3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x5f,&vbp3);
	vbp1 += vbp3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x60,&vfp3);
	vfp1 = vfp3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x61,&vfp3);
	vfp1 += vfp3<<8;

	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x62,&vtotal3);
	vtotal1 = vtotal3;
	it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x63,&vtotal3);
	vtotal1 += vtotal3<<8;
 
 	printk("\r\n hsync_start1,hs1,hbp1,hfp1,htotal1 is:%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
 		hsync_start1,hs1,hbp1,hfp1,htotal1,vsync_start1,vs1,vbp1,vfp1,vtotal1);

}
void dptx_show_vid_info(void)
{
	u8 HSyncPol, VSyncPol, InterLaced;
    u8 HTotal, HDES, HDEW, HFPH, HSYNCW;
    u8 VTotal, VDES, VDEW, VFPH, VSYNCW;
    u8 rddata;
	u8 tmp;
	//unsigned long PCLK = 0;
	//unsigned long sum = 0;
    u8 sysstat,linkstat ;


    it6151_i2c_read_byte(DP_I2C_ADDR,0x0D,&sysstat);
    it6151_i2c_read_byte(DP_I2C_ADDR,0x0E,&linkstat);
    if((linkstat&0x1F) != 0x10)
    {
    	printk("Link Training has something wrong, reg0E = %x\n",linkstat);
    }
    else
    {
	it6151_i2c_read_byte(DP_I2C_ADDR,0x0E,&tmp);
	tmp = tmp & 0x80;
	printk("\r\nreg0E = 0x%X, Link Rate = %s\r\n",linkstat,tmp?"HBR:2.7G":"LBR:1.62G");
    }
	printk("reg0D = 0x%X, %s%s%s\n",sysstat,(sysstat&1)?"Interrupt! ":"",(sysstat&2)?"HPD ":"Unplug " ,(sysstat&4)?"Video Stable":"Video Unstable");

   
    it6151_i2c_read_byte(DP_I2C_ADDR,0xA0,&rddata);
    HSyncPol = rddata&0x01;
    VSyncPol = (rddata&0x04)>>2;
    InterLaced = (rddata&0x10)>>4;

    it6151_i2c_read_byte(DP_I2C_ADDR,0xA1,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xA2,&HTotal);
	HTotal = HTotal&0x1F;
	HTotal = HTotal<<8;
    HTotal = HTotal + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xA3,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xA4,&HDES);
	HDES = HDES&0x1F;
	HDES = HDES<<8;
    HDES = HDES + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xA5,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xA6,&HDEW);
	HDEW = HDEW&0x1F;
	HDEW = HDEW<<8;
    HDEW = HDEW + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xA7,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xA8,&HFPH);
	HFPH = HFPH&0x03;
	HFPH = HFPH<<8;
    HFPH = HFPH + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xA9,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xAA,&HSYNCW);
	HSYNCW = HSYNCW&0x03;
	HSYNCW = HSYNCW<<8;
    HSYNCW = HSYNCW + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xAB,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xAC,&VTotal);
	VTotal = VTotal&0x0F;
	VTotal = VTotal<<8;
    VTotal = VTotal + rddata;

    it6151_i2c_read_byte(DP_I2C_ADDR,0xAD,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xAE,&VDES);
	VDES = VDES&0x01;
	VDES = VDES<<8;
    VDES = VDES + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xAF,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xB0,&VDEW);
	VDEW = VDEW&0x0F;
	VDEW = VDEW<<8;
    VDEW = VDEW + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xB1,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xB2,&VFPH);
	VFPH = VFPH&0x01;
	VFPH = VFPH<<8;
    VFPH = VFPH + rddata;

	it6151_i2c_read_byte(DP_I2C_ADDR,0xB3,&rddata);
	it6151_i2c_read_byte(DP_I2C_ADDR,0xB4,&VSYNCW);
	VSYNCW = VSYNCW&0x01;
	VSYNCW = VSYNCW<<8;
    VSYNCW = VSYNCW + rddata;

	printk("\r\nH_act:%d,V_act:%d\r\n",HDEW,VDEW);
    printk("\r\nHTotal:%d,VTotal:%d\r\n",HTotal,VTotal);
    printk("\r\n hfp:%d,hs:%d,hbp:%d\r\n", HFPH, HSYNCW  , HTotal - HDEW - HFPH - HSYNCW);	
    printk("\r\n vfp:%d,vs:%d,vbp:%d\r\n",VFPH,VSYNCW,VTotal - VDEW - VFPH - VSYNCW);
    printk("\r\n%s\r\n",(InterLaced&0x01)?"initerlaced":"progress");
    printk("\r\nH polarity:%s,V polarity:%s \r\n",(HSyncPol&0x01)?"-":"+",(VSyncPol&0x01)?"-":"+");


    it6151_i2c_read_byte(DP_I2C_ADDR,0x12,&rddata);
    rddata |= 0x80;
    it6151_i2c_write_byte(DP_I2C_ADDR,0x12,rddata) ;
    rddata &= 0x7f;
    mdelay(10);
    it6151_i2c_write_byte(DP_I2C_ADDR,0x12,rddata) ;


}


int IT6151_init(void)
{
    unsigned char VenID[2], DevID[2], RevID;
	//u8 temp,i;

    printk("[KERNEL/LCM] IT6151_init\n");
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x00, &VenID[0]); 
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x01, &VenID[1]);
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x02, &DevID[0]);
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x03, &DevID[1]);
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x04, &RevID);
                
#ifndef BUILD_LK    
    printk("Current DPDevID=%02X%02X\n", DevID[1], DevID[0]);
    printk("Current DPVenID=%02X%02X\n", VenID[1], VenID[0]);
    printk("Current DPRevID=%02X\n\n", RevID);  
#endif
                
    if( VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x51 && DevID[1]==0x61 ){

#ifndef BUILD_LK    
        printk("[KERNEL/LCM] ===== qinrq ===== Test 1 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#else
        printf("[LK/LCM] ===== qinrq ===== Test 1 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#endif
      it6151_i2c_write_byte(DP_I2C_ADDR,0x05,0x04);// DP SW Reset
        it6151_i2c_write_byte(DP_I2C_ADDR,0xfd,(MIPI_I2C_ADDR<<1)|1);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x00);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x0c,(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(MIPI_LANE_COUNT<<4)|EN_UFO);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x11,MP_MCLK_INV);

        if(RevID == 0xA1){          
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x19, MP_LANE_DESKEW); 
        }else{
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x19,(MP_CONTINUOUS_CLK<<1) | MP_LANE_DESKEW); 
        }
                
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x27, MIPI_PACKED_FMT);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x28,((PANEL_WIDTH/4-1)>>2)&0xC0);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x29,(PANEL_WIDTH/4-1)&0xFF);
        
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x2e,0x34);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x2f,0x01);
        
        
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x4e,(MP_V_RESYNC<<3)|(MP_H_RESYNC<<2)|(MP_VPOL<<1)|(MP_HPOL));
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x80,(EN_UFO<<5)|MP_PCLK_DIV);
		it6151_i2c_write_byte(MIPI_I2C_ADDR,0x84,0x8f);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x09,MIPI_INT_MASK);
        it6151_i2c_write_byte(MIPI_I2C_ADDR,0x92,TIMER_CNT); 
#if (MIPI_EVENT_MODE == 1)
		it6151_i2c_write_byte(MIPI_I2C_ADDR,0x33,0x80 | MIPI_HSYNC_W >> 8);
		it6151_i2c_write_byte(MIPI_I2C_ADDR,0x32,MIPI_HSYNC_W & 0xFF);
		it6151_i2c_write_byte(MIPI_I2C_ADDR,0x3D,0x80 | MIPI_VSYNC_W >> 8);
		it6151_i2c_write_byte(MIPI_I2C_ADDR,0x3C,MIPI_VSYNC_W & 0xFF);
#endif		       
        IT6151_DPTX_init();
		#if 1
		dptx_show_vid_info();
		video_check();
		#endif
//test
/*
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x0d, &temp);
	printk(" ======= Reg0x0D = 0x%x \r\n",temp);
    it6151_i2c_read_byte(DP_I2C_ADDR, 0x0e, &temp);
	printk(" ======= Reg0x0E = 0x%x \r\n",temp);
    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x0d, &temp);
	printk(" ======= Reg0x0D = 0x%x \r\n",temp);
	for(i=0x30;i<=0x43;i++){
		it6151_i2c_read_byte(MIPI_I2C_ADDR, i, &temp);
		printk(" ======= Reg0x%x = 0x%x \r\n",i,temp);
	}
	for(i=0x50;i<=0x57;i++){
		it6151_i2c_read_byte(MIPI_I2C_ADDR, i, &temp);
		printk(" ======= Reg0x%x = 0x%x \r\n",i,temp);
	}*/
//test end
        return 0;
    }

#ifndef BUILD_LK    
    printk(" Test 2 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#endif

    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x00, &VenID[0]);
    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x01, &VenID[1]);
    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x02, &DevID[0]);
    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x03, &DevID[1]);
    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x04, &RevID);

#ifndef BUILD_LK
    printk("Current MPDevID=%02X%02X\n", DevID[1], DevID[0]);
    printk("Current MPVenID=%02X%02X\n", VenID[1], VenID[0]);
    printk("Current MPRevID=%02X\n\n", RevID);
#endif
    if( VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x51 && DevID[1]==0x61 ){
    
#ifndef BUILD_LK    
            printk(" Test 1 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#else
            printf("[LK/LCM] Test 1 DP_I2C_ADDR=0x%x, MIPI_I2C_ADDR=0x%x\n", DP_I2C_ADDR, MIPI_I2C_ADDR);
#endif
            it6151_i2c_write_byte(DP_I2C_ADDR,0x05,0x04);// DP SW Reset
            it6151_i2c_write_byte(DP_I2C_ADDR,0xfd,(MIPI_I2C_ADDR<<1)|1);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x00);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x0c,(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(MIPI_LANE_COUNT<<4)|EN_UFO);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x11,MP_MCLK_INV);
    
            if(RevID == 0xA1){          
                it6151_i2c_write_byte(MIPI_I2C_ADDR,0x19, MP_LANE_DESKEW); 
            }else{
                it6151_i2c_write_byte(MIPI_I2C_ADDR,0x19,(MP_CONTINUOUS_CLK<<1) | MP_LANE_DESKEW); 
            }
                    
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x27, MIPI_PACKED_FMT);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x28,((PANEL_WIDTH/4-1)>>2)&0xC0);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x29,(PANEL_WIDTH/4-1)&0xFF);
            
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x2e,0x34);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x2f,0x01);
            
            
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x4e,(MP_V_RESYNC<<3)|(MP_H_RESYNC<<2)|(MP_VPOL<<1)|(MP_HPOL));
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x80,(EN_UFO<<5)|MP_PCLK_DIV);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x84,0x8f);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x09,MIPI_INT_MASK);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x92,TIMER_CNT);        
            IT6151_DPTX_init();
    
            return 0;
        }

    if(VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x21 && DevID[1]==0x61 ){
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x33);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x40);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x05,0x00);
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x0c,(MP_LANE_SWAP<<7)|(MP_PN_SWAP<<6)|(MIPI_LANE_COUNT<<4));
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x11, MP_MCLK_INV); 
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x19,(MP_CONTINUOUS_CLK<<1) | MP_LANE_DESKEW);  
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x4B,(FRAME_RESYNC<<4));
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x4E,(MP_V_RESYNC<<3)|(MP_H_RESYNC<<2)|(MP_VPOL<<1)|(MP_HPOL));      
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x72,0x01); 
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x73,0x03); 
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0x80,MP_PCLK_DIV); 
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0xC0,(HIGH_PCLK<< 4) | 0x0F);   
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0xC1,0x01);  
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0xC2,0x47);  
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0xC3,0x67);  
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0xC4,0x04);  
            it6151_i2c_write_byte(MIPI_I2C_ADDR,0xCB,(LVDS_PN_SWAP<<5)|(LVDS_LANE_SWAP<<4)|(LVDS_6BIT<<2)|(LVDS_DC_BALANCE<<1)| VESA_MAP);  
           return 1;
  } 
    return -1;
}
EXPORT_SYMBOL(IT6151_init);
#endif //end of CONFIG_WB_IT6151FN
//static unsigned int IT6151_ESD_Check(void)
//{
//#ifndef BUILD_LK
//    static  unsigned char ucIsIT6151=0xFF;
//    unsigned char ucReg, ucStat;
// // unsigned char cmdBuffer;
//    
//  //return FALSE;
//
//    if(ucIsIT6151==0xFF){
//        unsigned char VenID[2], DevID[2];
//                
//#ifndef BUILD_LK
//        printk("\nIT6151 1st IRQ !!!\n");
//#endif
//                
//        it6151_i2c_read_byte(DP_I2C_ADDR, 0x00, &VenID[0]);
//        it6151_i2c_read_byte(DP_I2C_ADDR, 0x01, &VenID[1]);
//        it6151_i2c_read_byte(DP_I2C_ADDR, 0x02, &DevID[0]);
//        it6151_i2c_read_byte(DP_I2C_ADDR, 0x03, &DevID[1]);
//    
//#ifndef BUILD_LK
//        printk("Current DevID=%02X%02X\n", DevID[1], DevID[0]);
//        printk("Current VenID=%02X%02X\n", VenID[1], VenID[0]);
//            #endif
//                    
//        if( VenID[0]==0x54 && VenID[1]==0x49 && DevID[0]==0x51 && DevID[1]==0x61){
//                ucIsIT6151 = 1;
//        }else{
//                ucIsIT6151 = 0;
//    }
//  }
//    if(ucIsIT6151==1){
//        it6151_i2c_read_byte(DP_I2C_ADDR, 0x0D, &ucReg);
//#ifndef BUILD_LK            
//        printk("\nIT6151 Reg0x0D=0x%x !!!\n", ucReg);
//#endif
//        if(ucReg & 0x80){
//            it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x06, &ucStat);
//            if(ucStat & 0x01){
//                it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x06, ucStat); 
//
//                it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x0D, &ucStat);
//                if(ucStat & 0x10){
//                    //disable timer
//                    it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x0B, 0x00);
//                    it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x08, 0x40);
//                }else{
//                    //enable timer
//                    it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x0B, 0x40);               
//                }                               
//            }
//            it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x08, &ucStat);
//            if(ucStat & 0x40){
//                if(ucStat & 0x20){
//                    //disable timer
//                    it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x0B, 0x00);
//                    it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x08, 0x40);
//                }else{
//                    return TRUE;
//                }
//            }       
//        }
//        if(ucReg & 0x01){   //DP_IRQ
//            it6151_i2c_read_byte(DP_I2C_ADDR, 0x21, &ucStat);
//            if(ucStat & 0x02){
//                it6151_i2c_write_byte(DP_I2C_ADDR, 0x21, ucStat);
//            }   
//            it6151_i2c_read_byte(DP_I2C_ADDR, 0x06, &ucReg);
//            it6151_i2c_read_byte(DP_I2C_ADDR, 0x0D, &ucStat);
//            if(ucReg & 0x03){
//                if(ucStat & 0x02){
//                    return TRUE;
//                }
//            }                                       
//        }           
//    }
//    return FALSE;
//    #endif
//}
//
//static void IT6151_ESD_Recover(void)
//{
//    unsigned char ucStat;
//
//  #ifndef BUILD_LK
//    printk("\nIT6151_ESD_Recover\n");
//    #endif
//    it6151_i2c_read_byte(MIPI_I2C_ADDR, 0x08, &ucStat);
//    if(ucStat & 0x40){
//        it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x0B, 0x00);
//        it6151_i2c_write_byte(MIPI_I2C_ADDR, 0x08, 0x40);
//        #ifndef BUILD_LK
//        IT6151_init();
//    #endif
//    }else{
//        IT6151_DPTX_init();
//    }
//}

static const struct panel_init_cmd cm_init_cmd[] = {
	_INIT_DELAY_CMD(20),
	{} //it must be here
};

//#define LCM_5
//#define LCM_6
//#define LCM_7
//#define LCM_8
//#define LCM_101
//#define LCM_1036
//#define LCM_105
//#define LCM_1095
#define LCM_14

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

#ifdef LCM_14
#define WIDTH_MM    188
#define HEIGHT_MM   302
#endif

#ifndef WIDTH_MM
#define WIDTH_MM   137
#endif

#ifndef HEIGHT_MM
#define HEIGHT_MM  217
#endif 


#define LCM_WIDTH		1920
#define LCM_HEIGHT		1200

#define VSA				12
#define VBP				10
#define VFP				10

#define HSA				27
#define HBP				50
#define HFP				200

#define FPS				60

#define PLL_CLK			488


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
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE ,
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
    //.rotate = 1,    
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

#if 0
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
#endif

static int common_panel_unprepare_power(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (!cm->prepared_power)
		return 0;

	pr_notice("111 Leo [Kernel/LCM] %s enter\n", __func__);
    cust_gpio_set_value(CUST_GPIO_LCM_BL_EN, 0); 
    mdelay(100);
    cust_gpio_set_value(CUST_GPIO_LCM_BL_EN2, 0); 
    mdelay(10);
    cust_gpio_set_value(CUST_GPIO_LCM_3V3, 0); 
    mdelay(10);
	cust_gpio_set_value(CUST_GPIO_IT6151_1V8_EN, 0);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_IT6151_1V2_EN, 0); //3.3
	mdelay(1);
	//cust_gpio_set_value(CUST_GPIO_IT6151_ENPSR, 1); 
	//mdelay(1);
	//cust_gpio_set_value(CUST_GPIO_IT6151_STBY, 1); 
	//mdelay(1);
    cust_gpio_set_value(CUST_GPIO_IT6151_RESET, 0); 
    
	cm->prepared_power = false;

	return 0;
}

static int common_panel_unprepare(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);
	//int ret;

	if (!cm->prepared)
		return 0;

#if 0
	if (!cm->desc->discharge_on_disable) {
		ret = common_panel_enter_sleep_mode(cm);
		if (ret < 0) {
			pr_notice("failed to set panel off: %d\n",
				ret);
			return ret;
		}
	}
#endif

	common_panel_unprepare_power(panel);

	cm->prepared = false;

	return 0;
}

static int common_panel_prepare_power(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (cm->prepared_power)
		return 0;
	pr_notice("111 [Kernel/LCM] %s enter\n", __func__);

    cust_gpio_set_value(CUST_GPIO_LCM_3V3, 1); 
    mdelay(20);
    cust_gpio_set_value(CUST_GPIO_IT6151_1V8_EN, 1);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_IT6151_1V2_EN, 1);
	mdelay(5);
	//cust_gpio_set_value(CUST_GPIO_IT6151_ENPSR, 0); 
	//mdelay(5);
	//cust_gpio_set_value(CUST_GPIO_IT6151_STBY, 0); 
	//mdelay(5);
    cust_gpio_set_value(CUST_GPIO_IT6151_RESET, 1); 
    mdelay(100);
    cust_gpio_set_value(CUST_GPIO_LCM_BL_EN2, 1); 
    mdelay(5);
    cust_gpio_set_value(CUST_GPIO_LCM_EDP_EN, 1); 
    mdelay(100);
    cust_gpio_set_value(CUST_GPIO_LCM_BL_EN, 1); 
    mdelay(10);
    //cust_gpio_set_value(CUST_GPIO_LCM_RST, 1); 
    //mdelay(10);
    //cust_gpio_set_value(CUST_GPIO_LCM_RST, 0); 
    //mdelay(10);
    //cust_gpio_set_value(CUST_GPIO_LCM_RST, 1); 
#if IS_ENABLED(CONFIG_WB_IT6151FN)    
    IT6151_init();
#endif

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

	return 0;
}

static int common_panel_enable(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (cm->enabled)
		return 0;


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


struct cust_drm_lcm m100t_ol_it6151_sl140pn50d3381a00_wuxga = {
	.name = "m100t_ol_it6151_sl140pn50d3381a00_wuxga",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
