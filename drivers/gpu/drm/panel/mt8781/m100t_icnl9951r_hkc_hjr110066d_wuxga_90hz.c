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

static const struct panel_init_cmd cm_sleep_init_cmd[] = {
_INIT_DCS_CMD(0XF0,0X5A,0X59),
_INIT_DCS_CMD(0XF1,0XA5,0XA6),
_INIT_DCS_CMD(0XAC,0X0A),
_INIT_DCS_CMD(0x28,0x00),
_INIT_DELAY_CMD(10),
_INIT_DCS_CMD(0x10, 0x00),
_INIT_DELAY_CMD(120),
_INIT_DCS_CMD(0xCC,0x01),
	{} //it must be here
};

static const struct panel_init_cmd cm_init_cmd[] = {
_INIT_DCS_CMD(0XF0,0X5A,0X59),
_INIT_DCS_CMD(0XF1,0XA5,0XA6),
_INIT_DCS_CMD(0XB2,0X04,0X03,0X94,0X95,0X77,0X77,0X07,0X90,0X88,0X00,0XFF,0X00,0XFF,0X00,0XFF,0X00,0XFF,0X80,0X80,0X80,0X80,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X55,0X11,0X00,0X00),
_INIT_DCS_CMD(0XB3,0XF6,0X01,0X02,0X09,0X8A,0X85,0X00,0X00,0X5C,0X00,0X00,0X11,0X34,0X01,0X01,0X01,0X01,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X03,0X06,0X80,0X80,0X01,0X00,0X00,0X00),
_INIT_DCS_CMD(0XB4,0X0D,0X06,0X1A,0X1D,0X56,0X26,0X01,0XC7,0X33,0X33,0XAA,0X33,0X44,0X26,0X26,0X00,0X02,0X08,0X20,0X30,0X05,0X85,0X23,0X20,0X40,0X11,0X10,0X20,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XB5,0X00,0XA8,0XA9,0X26,0X09,0X25,0X27,0X2D,0X2D,0X1B,0X19,0X17,0X15,0X13,0X11,0X0F,0X0D,0X05,0X23,0X03,0X03,0X03,0X03,0X03,0XC0,0X00,0X01,0X50,0XFF,0XFF,0XE0,0X00),
_INIT_DCS_CMD(0XB6,0X00,0XA8,0XA9,0X26,0X08,0X24,0X27,0X2D,0X2D,0X1A,0X18,0X16,0X14,0X12,0X10,0X0E,0X0C,0X04,0X22,0X03,0X03,0X03,0X03,0X03,0XC0,0X00,0X01,0X50,0XFF,0XFF,0XE0,0X00),
_INIT_DCS_CMD(0XB7,0X0F,0X03,0XC0,0X00,0X01,0X50,0X0F,0X03,0XC0,0X00,0X01,0X50,0X03,0XFC,0X3F,0XFF,0XFD,0X50,0X55,0X55,0X55,0X55,0X55,0X50,0X03,0X03,0XC0,0X00,0X01,0X50,0X00,0X00),
_INIT_DCS_CMD(0XB8,0X0F,0X03,0XC0,0X00,0X01,0X50,0X0F,0X03,0XC0,0X00,0X01,0X50,0X03,0XFC,0X3F,0XFF,0XFD,0X50,0X55,0X55,0X55,0X55,0X55,0X50,0X03,0X03,0XC0,0X00,0X01,0X50,0X00,0X00),
_INIT_DCS_CMD(0XB9,0X01,0X55,0X55,0X55,0X55,0X55,0X50,0X55,0X55,0X55,0X55,0X55,0X50,0XFF,0XFF,0XFF,0XFF,0XFD,0X50,0XFF,0XFF,0XFF,0XFF,0XFD,0X50,0X00,0X00,0X00,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XBB,0X01,0X02,0X03,0X0A,0X04,0X13,0X14,0X12,0X16,0X5C,0X00,0X15,0X16,0X03,0X00),
_INIT_DCS_CMD(0XBC,0XFE,0XF8,0XF0,0X00,0X00,0X00,0X00,0X04,0X00,0X05,0X80,0X02,0X24,0X00,0XB9,0X99,0X99,0X00,0XC4,0X09,0XC3,0X86,0X03,0X2E,0X11,0X00,0X00,0X00,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XBD,0XED,0X23,0X42,0X52,0X52,0X1F,0X00),
_INIT_DCS_CMD(0XBE,0X75,0X43,0X7E,0X43,0X0A,0X88,0X58,0X33,0X33,0X33,0X93,0X03,0X18,0X18,0X00,0X00,0X00,0X00,0XB2,0XAF,0XB2,0XAF,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XBF,0X0C,0X19,0X0C,0X19,0X00,0X11,0X22,0X04,0X58,0X00),
_INIT_DCS_CMD(0XC0,0X40,0X90,0X17,0X41,0X23,0X56,0XF7,0X8A,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0X3F,0XFF,0X00,0XCC,0X02,0X00,0X01,0XB3),
_INIT_DCS_CMD(0XC1,0X01,0X30,0X00,0X1E,0X00,0X1E,0X04,0X40,0X44,0X04,0XC7,0X80,0X05,0X12,0XC0,0X23,0X47,0XC0,0X10,0XFF,0X0F,0XE7,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XC2,0X00),
_INIT_DCS_CMD(0XC3,0X00,0XFF,0X42,0X4D,0X01,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X97,0X10,0X10,0X2A,0X2A,0X2A,0X2A,0X2A,0X2A,0X2A,0X2A,0X40,0X00,0X00,0X00,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XC4,0X0C,0X35,0X28,0X49,0X00,0X3F,0X00,0X50,0X00,0X1F,0X00,0XA3,0XF0,0XE7),
_INIT_DCS_CMD(0XC5,0X03,0X13,0X10,0X57,0X5D,0X37,0X04,0X05,0X04,0X04,0X19,0X00,0XB4,0X2C,0X2B,0X2B,0XBB,0XAE,0X20,0X00,0X02,0X00,0X80,0X1D,0X15,0X06,0X13,0X64,0XFF,0X03,0X20,0XFF),

_INIT_DCS_CMD(0XC7,0X76,0X54,0X32,0X22,0X34,0X56,0X77,0X77,0X20,0X76,0X54,0X32,0X22,0X34,0X56,0X77,0X77,0X20,0X42,0X00,0X21,0XFF,0XFF,0X04,0X04,0X03,0X0E,0X07,0X00),
_INIT_DCS_CMD(0X80,0XE8,0XD5,0XB4,0X99,0X82,0X6D,0X5A,0X4A,0X3A,0X09,0XE3,0XC3,0XA2,0X84,0X69,0X3B,0X08,0XDA,0XAF,0XAC,0X83,0X5A,0X2E,0X00,0XD2,0X9A,0X77,0X49,0X3F,0X38,0X32,0X28),
_INIT_DCS_CMD(0X81,0XE8,0XD2,0XAE,0X92,0X79,0X65,0X53,0X42,0X34,0X03,0XDC,0XBC,0X9B,0X7F,0X65,0X36,0X03,0XD6,0XAC,0XA9,0X81,0X58,0X2C,0XFF,0XD1,0X99,0X77,0X49,0X3F,0X38,0X32,0X28),
_INIT_DCS_CMD(0X82,0XE8,0XD3,0XB1,0X95,0X7D,0X69,0X56,0X46,0X37,0X06,0XDF,0XBF,0X9E,0X81,0X67,0X38,0X05,0XD8,0XAD,0XAA,0X82,0X59,0X2D,0XFF,0XD1,0X99,0X77,0X49,0X3F,0X38,0X32,0X28),
_INIT_DCS_CMD(0X83,0X09,0X1F,0X12,0X04,0X00,0X1F,0X12,0X04,0X00,0X1F,0X12,0X04,0X00,0X28,0X18,0X08,0X00,0X28,0X18,0X08,0X00,0X28,0X18,0X08,0X00),
_INIT_DCS_CMD(0X84,0XFF,0XFF,0XFA,0XAA,0X95,0X55,0X00,0X00,0X00,0XFF,0XFF,0XFA,0XAA,0X95,0X54,0X00,0X00,0X00,0XFF,0XFF,0XFA,0XAA,0X95,0X54,0X00,0X00,0X00),
_INIT_DCS_CMD(0XC8,0X42,0X00,0X48,0XE7,0XE0,0X00,0X23),
_INIT_DCS_CMD(0XD7,0X3F,0X04,0X0A,0X00,0X00,0X06),
_INIT_DCS_CMD(0XD5,0X01,0X30,0X86,0X10),

_INIT_DCS_CMD(0X6D,0X43),
_INIT_DCS_CMD(0XF0,0X5A,0X57),
_INIT_DCS_CMD(0X79,0X41,0X12,0X00,0X00,0X00,0X00,0X97,0X02,0X00,0X20,0X00,0X03,0X02),
_INIT_DCS_CMD(0XF0,0X5A,0X58),
_INIT_DCS_CMD(0X79,0X41,0X12,0X00,0X00,0X00,0X00,0X97,0X02,0X00,0X20,0X00,0X02,0X02),
_INIT_DCS_CMD(0XF0,0X5A,0X59),
_INIT_DCS_CMD(0X7A,0X12,0X00,0X00,0X89,0X30,0X80,0X07,0X80,0X04,0XB0,0X00,0X08,0X02,0X58,0X02,0X58,0X02,0X00,0X02,0X2C,0X00,0X20,0X00,0XC4,0X00,0X08,0X00,0X0C,0X0D,0XB7,0X0B,0X71),
_INIT_DCS_CMD(0X7B,0X18,0X00,0X10,0XF0,0X03,0X0C,0X20,0X00,0X06,0X0B,0X0B,0X33,0X0E,0X1C,0X2A,0X38,0X46,0X54,0X62,0X69,0X70,0X77,0X79,0X7B,0X7D,0X7E,0X01,0X02,0X01,0X00,0X09,0X40),
_INIT_DCS_CMD(0X7C,0X09,0XBE,0X19,0XFC,0X19,0XFA,0X19,0XF8,0X1A,0X38,0X1A,0X78,0X1A,0XB6,0X2A,0XB6,0X2A,0XF4,0X2A,0XF4,0X4B,0X34,0X63,0X74,0X00,0X00,0X00,0X00,0X00,0X00),
_INIT_DCS_CMD(0XE0,0X0C,0X00,0XB0,0X10,0X00,0X0A,0XBE),
_INIT_DCS_CMD(0XF1,0X5A,0X59),
_INIT_DCS_CMD(0XF0,0XA5,0XA6),
_INIT_DCS_CMD(0X35,0X00),

_INIT_DCS_CMD(0x11, 0x00),
_INIT_DELAY_CMD(120),
_INIT_DCS_CMD(0x29, 0x00),
_INIT_DELAY_CMD(20),
_INIT_DCS_CMD(0xAC,0x05),
	{} //it must be here
};

//#define LCM_5
//#define LCM_6
//#define LCM_7
//#define LCM_8
//#define LCM_101
//#define LCM_1036
//#define LCM_105
#define LCM_1095

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


#define LCM_WIDTH		1200
#define LCM_HEIGHT		1920

#define VSA				4
#define VBP				30
#define VFP				1450
#define VFP_90			315

#define HSA				4
#define HBP				40
#define HFP				40

#define FPS				60
#define FPS_90			90

#define PLL_CLK			329

//htotal * vtotal * vrefresh / 1000
#define HTOTAL		(LCM_WIDTH + HFP + HBP + HSA)
#define VTOTAL		(LCM_HEIGHT + VFP + VBP + VSA)
#define PIXEL		(HTOTAL * VTOTAL * FPS ) / 1000
#define VTOTAL_90	(LCM_HEIGHT + VFP_90 + VBP + VSA)
#define PIXEL_90	(HTOTAL * VTOTAL_90 * FPS_90 ) / 1000

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

static const struct drm_display_mode common_mode_90 = {
	.clock = PIXEL_90,
	.hdisplay = LCM_WIDTH,
	.hsync_start = LCM_WIDTH + HFP, //hfp
	.hsync_end = LCM_WIDTH + HFP + HSA, //hsa
	.htotal = LCM_WIDTH + HFP + HSA + HBP, //hsp
	.vdisplay = LCM_HEIGHT,
	.vsync_start = LCM_HEIGHT + VFP_90, //vfp
	.vsync_end = LCM_HEIGHT + VFP_90 + VSA, //vsa
	.vtotal = LCM_HEIGHT + VFP_90 + VSA + VBP, //vsp
};

static const struct panel_desc common_desc = {

	.modes = &common_default_mode,
	.modes_90 = &common_mode_90,
	.bpc = 8,
	.size = {
		.width_mm = WIDTH_MM,
		.height_mm = HEIGHT_MM,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE,
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
	.cust_esd_check = 0,
	.esd_check_enable = 0,
    //.vfp_low_power = VFP,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.data_rate = PLL_CLK*2,
	.rotate = 1,
	.ssc_enable = 0,

	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
/*bdg dsc params*/
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1920,
		.pic_width = 1200,
		.slice_height = 8,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 196,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 2929,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.dyn_fps = {
		.switch_en = 1,
		.data_rate = PLL_CLK*2,
		.vact_timing_fps = FPS,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = PLL_CLK*2,
		.hfp = HFP,
		.vfp = VFP,
		.vsa = VSA,
	},
};

static struct mtk_panel_params ext_params_90 = {
	.pll_clk = PLL_CLK,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	//.vfp_low_power = VFP,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.data_rate = PLL_CLK*2,
	.rotate = 1,
	.ssc_enable = 0,

	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
/*bdg dsc params*/
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1920,
		.pic_width = 1200,
		.slice_height = 8,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 196,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 2929,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.rc_buf_thresh[0] = 14,
		.rc_buf_thresh[1] = 28,
		.rc_buf_thresh[2] = 42,
		.rc_buf_thresh[3] = 56,
		.rc_buf_thresh[4] = 70,
		.rc_buf_thresh[5] = 84,
		.rc_buf_thresh[6] = 98,
		.rc_buf_thresh[7] = 105,
		.rc_buf_thresh[8] = 112,
		.rc_buf_thresh[9] = 119,
		.rc_buf_thresh[10] = 121,
		.rc_buf_thresh[11] = 123,
		.rc_buf_thresh[12] = 125,
		.rc_buf_thresh[13] = 126,
		.rc_range_parameters[0].range_min_qp = 0,
		.rc_range_parameters[0].range_max_qp = 4,
		.rc_range_parameters[0].range_bpg_offset = 2,
		.rc_range_parameters[1].range_min_qp = 0,
		.rc_range_parameters[1].range_max_qp = 4,
		.rc_range_parameters[1].range_bpg_offset = 0,
		.rc_range_parameters[2].range_min_qp = 1,
		.rc_range_parameters[2].range_max_qp = 5,
		.rc_range_parameters[2].range_bpg_offset = 0,
		.rc_range_parameters[3].range_min_qp = 1,
		.rc_range_parameters[3].range_max_qp = 6,
		.rc_range_parameters[3].range_bpg_offset = -2,
		.rc_range_parameters[4].range_min_qp = 3,
		.rc_range_parameters[4].range_max_qp = 7,
		.rc_range_parameters[4].range_bpg_offset = -4,
		.rc_range_parameters[5].range_min_qp = 3,
		.rc_range_parameters[5].range_max_qp = 7,
		.rc_range_parameters[5].range_bpg_offset = -6,
		.rc_range_parameters[6].range_min_qp = 3,
		.rc_range_parameters[6].range_max_qp = 7,
		.rc_range_parameters[6].range_bpg_offset = -8,
		.rc_range_parameters[7].range_min_qp = 3,
		.rc_range_parameters[7].range_max_qp = 8,
		.rc_range_parameters[7].range_bpg_offset = -8,
		.rc_range_parameters[8].range_min_qp = 3,
		.rc_range_parameters[8].range_max_qp = 9,
		.rc_range_parameters[8].range_bpg_offset = -8,
		.rc_range_parameters[9].range_min_qp = 3,
		.rc_range_parameters[9].range_max_qp = 10,
		.rc_range_parameters[9].range_bpg_offset = -10,
		.rc_range_parameters[10].range_min_qp = 5,
		.rc_range_parameters[10].range_max_qp = 10,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 5,
		.rc_range_parameters[11].range_max_qp = 11,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 5,
		.rc_range_parameters[12].range_max_qp = 11,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 9,
		.rc_range_parameters[13].range_max_qp = 12,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 12,
		.rc_range_parameters[14].range_max_qp = 13,
		.rc_range_parameters[14].range_bpg_offset = -12,
	},
	.dyn_fps = {
		.switch_en = 1,
		.data_rate = PLL_CLK*2,
		.vact_timing_fps = FPS_90,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = PLL_CLK*2,
		.hfp = HFP,
		.vfp = VFP_90,
		.vsa = VSA,
	},
};

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_info("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}
	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90;
	else
		ret = 1;

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
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

static int common_panel_sleep_init_dcs_cmd(struct common_panel *cm)
{
	struct mipi_dsi_device *dsi = cm->dsi;
	int i, err = 0;

	for (i = 0; cm_sleep_init_cmd[i].len != 0; i++) {
		const struct panel_init_cmd *cmd = &cm_sleep_init_cmd[i];

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

	return 0;
}

static int common_panel_enter_sleep_mode(struct common_panel *cm)
{
	struct mipi_dsi_device *dsi = cm->dsi;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = common_panel_sleep_init_dcs_cmd(cm);
	if (ret < 0) {
		pr_notice("failed to sleep init panel: %d\n", ret);
		return ret;
	}

#if 0
	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;

	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	msleep(120);
#endif
	return 0;
}

static int common_panel_unprepare_power(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (!cm->prepared_power)
		return 0;

	pr_notice("Leo [Kernel/LCM] %s enter\n", __func__);

#if IS_ENABLED(CONFIG_WB_TP_INCELL_SUPPORT) //Leo 20230131 
    //cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ZERO);
    //mdelay(5);
#endif
	//cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	//mdelay(5);

	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ZERO);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ZERO);
	mdelay(20);

	//cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ZERO);
	//mdelay(10);

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

	//cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ONE);
	//msleep(1);

	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ONE);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ONE);
	msleep(5);

    cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(1);
#if IS_ENABLED(CONFIG_WB_TP_INCELL_SUPPORT) //Leo 20230131
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ONE);
	msleep(1);
	cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ZERO);
    mdelay(1);
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ONE);
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
	const struct drm_display_mode *m_90 = cm->desc->modes_90;
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_90;
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
	csci_mode->clock = PIXEL;
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

	mode_90 = drm_mode_duplicate(connector->dev, m_90);
	if (!mode_90) {
		dev_err(panel->dev, "failed to add mode_90 %ux%u@%u\n",
			m_90->hdisplay, m_90->vdisplay, drm_mode_vrefresh(m_90));
		return -ENOMEM;
	}

	mode_90->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode_90);
	drm_mode_probed_add(connector, mode_90);

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


struct cust_drm_lcm m100t_icnl9951r_hkc_hjr110066d_wuxga_90hz = {
	.name = "m100t_icnl9951r_hkc_hjr110066d_wuxga_90hz",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
