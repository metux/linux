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
_INIT_DCS_CMD(0x28,0x00),
_INIT_DELAY_CMD(10),
_INIT_DCS_CMD(0x10, 0x00),
_INIT_DELAY_CMD(120),
	{} //it must be here
};

static const struct panel_init_cmd cm_init_cmd[] = {
_INIT_DCS_CMD(0xB9,0x83,0x10,0x21,0x55,0x00),
_INIT_DCS_CMD(0xB1,0x2C,0xB1,0xB1,0x2D,0xED,0x43,0xE1,0x4D,0x36,0x36,0x36,0x36,0x1A,0x8B,0x11,0x65,0x00,0x88,0xFA,0xFF,0xFF,0x8F,0xFF,0x08,0x9A,0x33),
_INIT_DCS_CMD(0xB2,0x00,0x47,0xB0,0xD0,0x00,0x12,0x76,0x3C,0x5A,0x32,0x30,0x00,0x00,0x88,0xF5),
_INIT_DCS_CMD(0xB4,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x49,0x3F,0x7C,0x70,0x01,0x67),
_INIT_DCS_CMD(0xE9,0xCD),
_INIT_DCS_CMD(0xBA,0x84),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xBC,0x1B,0x04),
_INIT_DCS_CMD(0xBE,0x20),
_INIT_DCS_CMD(0xC0,0x3A,0x3A,0x22,0x11,0x22,0xA0,0x61,0x08,0xF5,0x03),
_INIT_DCS_CMD(0xE9,0xCC),
_INIT_DCS_CMD(0xC7,0x80),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xE9,0xC6),
_INIT_DCS_CMD(0xC8,0x97),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xC9,0x00,0x1E,0x13,0x88,0x01),
_INIT_DCS_CMD(0xCB,0x08,0x13,0x07,0x00,0x09,0xBC),
_INIT_DCS_CMD(0xCC,0x02,0x03,0x4C),
_INIT_DCS_CMD(0xE9,0xC4),
_INIT_DCS_CMD(0xD0,0x03),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xD1,0x37,0x06,0x00,0x02,0x04,0x0C,0xFF),
_INIT_DCS_CMD(0xD3,0x06,0x00,0x00,0x00,0x40,0x04,0x08,0x04,0x08,0x37,0x47,0x64,0x4B,0x11,0x11,0x03,0x03,0x32,0x10,0x0C,0x00,0x0C,0x32,0x10,0x08,0x00,0x08,0x32,0x17,0xE7,0x07,0xE7,0x00,0x00),
_INIT_DCS_CMD(0xD5,0x18,0x18,0x18,0x18,0x1F,0x1F,0x1F,0x1F,0x1E,0x1E,0x1E,0x1E,0x24,0x24,0x24,0x24,0x01,0x00,0x01,0x00,0x03,0x02,0x03,0x02,0x05,0x04,0x05,0x04,0x07,0x06,0x07,0x06,0x21,0x20,0x21,0x20,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18),
_INIT_DCS_CMD(0xD8,0xAF,0xAF,0xAA,0xAA,0xAA,0xA0,0xAF,0xAF,0xAA,0xAA,0xAA,0xA0),
_INIT_DCS_CMD(0xE7,0x24,0x0A,0x0A,0x37,0x37,0x67,0x03,0x3E,0x4C,0x14,0x14,0x00,0x00,0x00,0x00,0x12,0x05,0x02,0x02,0x0A,0x33,0x03,0x84),
_INIT_DCS_CMD(0xE0,0x00,0x05,0x0E,0x14,0x1C,0x30,0x49,0x51,0x59,0x56,0x72,0x79,0x80,0x90,0x8F,0x97,0x9F,0xB1,0xAF,0x55,0x5C,0x66,0x73,0x00,0x05,0x0E,0x14,0x1C,0x30,0x49,0x51,0x59,0x56,0x72,0x79,0x80,0x90,0x8F,0x97,0x9F,0xB1,0xAF,0x55,0x5C,0x66,0x73),
_INIT_DCS_CMD(0xE1,0x11,0x00,0x00,0x89,0x30,0x80,0x07,0xD0,0x02,0x58,0x00,0x14,0x02,0x58,0x02,0x58,0x02,0x00,0x02,0x2C,0x00,0x20,0x02,0x02,0x00,0x08,0x00,0x0C,0x05,0x0E,0x04,0x94,0x18,0x00,0x10,0xF0,0x03,0x0C,0x20,0x00,0x06,0x0B,0x0B,0x33,0x0E),
_INIT_DCS_CMD(0xBD,0x01),
_INIT_DCS_CMD(0xB1,0x01,0xBF,0x11),
_INIT_DCS_CMD(0xCB,0x80),
_INIT_DCS_CMD(0xD2,0xB4),
_INIT_DCS_CMD(0xE9,0xC9),
_INIT_DCS_CMD(0xD3,0x84),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xD8,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0),
_INIT_DCS_CMD(0xE1,0x40,0x09,0xBE,0x19,0xFC,0x19,0xFA,0x19,0xF8,0x1A,0x38,0x1A,0x78,0x1A,0xB6,0x2A,0xF6,0x2B,0x34,0x2B,0x74,0x3B,0x74,0x6B,0x74),
_INIT_DCS_CMD(0xE2,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x49,0x3F,0x7C,0x70,0x04,0x68,0x01,0x69,0x01,0x00,0x20,0x01,0x03,0x00,0x00,0x0F,0x0F,0x0F,0x0F,0x02,0x00,0x00,0x25,0x0E,0x0E,0x38,0x3A,0x68,0x04,0x60,0x4A,0x14,0x14,0x00,0x00,0x8F,0x01,0xDE,0x0D,0x45,0x4E,0x00,0x20,0x69,0x00,0x5A,0x00,0x09,0xBC,0x00),
_INIT_DCS_CMD(0xE7,0x02,0x00,0x90,0x01,0xD2,0x09,0xBF,0x0A,0xA0,0x00,0x00),
_INIT_DCS_CMD(0xBD,0x02),
_INIT_DCS_CMD(0xD8,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0,0xFF,0xFF,0xFF,0xFF,0xFA,0xA0),
_INIT_DCS_CMD(0xE7,0xFE,0x03,0xFE,0x03,0xFE,0x03,0x02,0x02,0x02,0x25,0x00,0x25,0x81,0x02,0x40,0x00,0x20,0x67,0x04,0x03,0x02,0x01,0x00,0x00,0x00,0x00,0x01,0x00),
_INIT_DCS_CMD(0xBD,0x03),
_INIT_DCS_CMD(0xD8,0xAA,0xAF,0xAA,0xAA,0xAA,0xA0,0xAA,0xAF,0xAA,0xAA,0xAA,0xA0,0xAA,0xAF,0xAA,0xAA,0xAA,0xA0,0xAA,0xAF,0xAA,0xAA,0xAA,0xA0,0x55,0x55,0x55,0x55,0x55,0x50,0x55,0x55,0x55,0x55,0x55,0x50),
_INIT_DCS_CMD(0xE1,0x01),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xE9,0xC4),
_INIT_DCS_CMD(0xBA,0x96),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xBD,0x01),
_INIT_DCS_CMD(0xE9,0xC5),
_INIT_DCS_CMD(0xBA,0x4F),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xBD,0x03),
_INIT_DCS_CMD(0xE9,0xC6),
_INIT_DCS_CMD(0xB4,0x03,0xFF,0xF8),
_INIT_DCS_CMD(0xE9,0x3F),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0x35,0x00),

_INIT_DCS_CMD(0xB9,0x83,0x10,0x21,0x55,0x00),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xE2,0x50),

_INIT_DCS_CMD(0xB9,0x00,0x00,0x00,0x00,0x00),

_INIT_DCS_CMD(0x35,0x00),

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
//#define LCM_105
//#define LCM_1095
#define LCM_1197

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

#ifdef LCM_1197
#define WIDTH_MM    156
#define HEIGHT_MM   261 
#endif

#ifndef WIDTH_MM
#define WIDTH_MM   137
#endif

#ifndef HEIGHT_MM
#define HEIGHT_MM  217
#endif 


#define LCM_WIDTH		1200
#define LCM_HEIGHT		2000

#define VSA				8
#define VBP				12
#define VFP				1190
#define VFP_90			120

#define HSA				20
#define HBP				40
#define HFP				57

#define FPS				60
#define FPS_90			90

#define PLL_CLK			329

//htotal * vtotal * vrefresh / 1000
#define HTOTAL		(LCM_WIDTH + HFP + HBP + HSA)
#define VTOTAL		(LCM_HEIGHT + VFP + VBP + VSA)
#define VTOTAL_90		(LCM_HEIGHT + VFP_90 + VBP + VSA)
#define PIXEL		(HTOTAL * VTOTAL * FPS ) / 1000
#define PIXEL_90		(HTOTAL * VTOTAL_90 * FPS_90 ) / 1000

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
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	//.data_rate = PLL_CLK*2,
	.rotate = 1,
	.ssc_enable = 0,

	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
/*bdg dsc params*/
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,

		.bit_per_pixel = 128,
		.pic_height = 2000,
		.pic_width = 1200,
		.slice_height = 20,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 514,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1172,
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
		.rc_range_parameters[10].range_max_qp = 11,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 5,
		.rc_range_parameters[11].range_max_qp = 12,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 5,
		.rc_range_parameters[12].range_max_qp = 13,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 7,
		.rc_range_parameters[13].range_max_qp = 13,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 13,
		.rc_range_parameters[14].range_max_qp = 13,
		.rc_range_parameters[14].range_bpg_offset = -12,
	},
	.dyn_fps = {
		.switch_en = 1,
		//.data_rate = PLL_CLK*2,
		.vact_timing_fps = FPS,
	},
	.dyn = {
		.switch_en = 1,
		//.data_rate = PLL_CLK*2,
		.hfp = HFP,
		.vfp = VFP,
		.vsa = VSA,
	},
};

static struct mtk_panel_params ext_params_90 = {
	.pll_clk = PLL_CLK,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	//.data_rate = PLL_CLK*2,
	.rotate = 1,
	.ssc_enable = 0,

	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
/*bdg dsc params*/
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,

		.bit_per_pixel = 128,
		.pic_height = 2000,
		.pic_width = 1200,
		.slice_height = 20,
		.slice_width = 600,
		.chunk_size = 600,
		.xmit_delay = 512,
		.dec_delay = 556,
		.scale_value = 32,
		.increment_interval = 514,
		.decrement_interval = 8,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1172,
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
		.rc_range_parameters[10].range_max_qp = 11,
		.rc_range_parameters[10].range_bpg_offset = -10,
		.rc_range_parameters[11].range_min_qp = 5,
		.rc_range_parameters[11].range_max_qp = 12,
		.rc_range_parameters[11].range_bpg_offset = -12,
		.rc_range_parameters[12].range_min_qp = 5,
		.rc_range_parameters[12].range_max_qp = 13,
		.rc_range_parameters[12].range_bpg_offset = -12,
		.rc_range_parameters[13].range_min_qp = 7,
		.rc_range_parameters[13].range_max_qp = 13,
		.rc_range_parameters[13].range_bpg_offset = -12,
		.rc_range_parameters[14].range_min_qp = 13,
		.rc_range_parameters[14].range_max_qp = 13,
		.rc_range_parameters[14].range_bpg_offset = -12,
	},
	.dyn_fps = {
		.switch_en = 1,
		//.data_rate = PLL_CLK*2,
		.vact_timing_fps = FPS_90,
	},
	.dyn = {
		.switch_en = 1,
		//.data_rate = PLL_CLK*2,
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
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ZERO);
    mdelay(5);
#endif
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(5);

	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ZERO);
	mdelay(1);
	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ZERO);
	mdelay(20);

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
	msleep(5);
#if IS_ENABLED(CONFIG_WB_TP_INCELL_SUPPORT) //Leo 20230131
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ONE);
    mdelay(5);
#endif

	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ONE);
	mdelay(5);
	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ONE);
	msleep(20);

	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(120);

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

struct cust_drm_lcm m100t_hx83102_jt120wul001a03_wuxga2000_90hz = {
	.name = "m100t_hx83102_jt120wul001a03_wuxga2000_90hz",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
