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
	_INIT_DCS_CMD(0xFF,0x98,0x81,0x03),
	_INIT_DCS_CMD(0x01,0x00),
	_INIT_DCS_CMD(0x02,0x00),
	_INIT_DCS_CMD(0x03,0x53),
	_INIT_DCS_CMD(0x04,0xD3),
	_INIT_DCS_CMD(0x05,0x00),
	_INIT_DCS_CMD(0x06,0x0D),
	_INIT_DCS_CMD(0x07,0x08),
	_INIT_DCS_CMD(0x08,0x00),
	_INIT_DCS_CMD(0x09,0x00),
	_INIT_DCS_CMD(0x0a,0x00),
	_INIT_DCS_CMD(0x0b,0x00),
	_INIT_DCS_CMD(0x0c,0x00),
	_INIT_DCS_CMD(0x0d,0x00),
	_INIT_DCS_CMD(0x0e,0x00),
	_INIT_DCS_CMD(0x0f,0x28),
	_INIT_DCS_CMD(0x10,0x28),
	_INIT_DCS_CMD(0x11,0x00),
	_INIT_DCS_CMD(0x12,0x00),
	_INIT_DCS_CMD(0x13,0x00),
	_INIT_DCS_CMD(0x14,0x00),
	_INIT_DCS_CMD(0x15,0x00),
	_INIT_DCS_CMD(0x16,0x00),
	_INIT_DCS_CMD(0x17,0x00),
	_INIT_DCS_CMD(0x18,0x00),
	_INIT_DCS_CMD(0x19,0x00),
	_INIT_DCS_CMD(0x1a,0x00),
	_INIT_DCS_CMD(0x1b,0x00),
	_INIT_DCS_CMD(0x1c,0x00),
	_INIT_DCS_CMD(0x1d,0x00),
	_INIT_DCS_CMD(0x1e,0x40),
	_INIT_DCS_CMD(0x1f,0x80),
	_INIT_DCS_CMD(0x20,0x06),
	_INIT_DCS_CMD(0x21,0x01),
	_INIT_DCS_CMD(0x22,0x00),
	_INIT_DCS_CMD(0x23,0x00),
	_INIT_DCS_CMD(0x24,0x00),
	_INIT_DCS_CMD(0x25,0x00),
	_INIT_DCS_CMD(0x26,0x00),
	_INIT_DCS_CMD(0x27,0x00),
	_INIT_DCS_CMD(0x28,0x33),
	_INIT_DCS_CMD(0x29,0x33),
	_INIT_DCS_CMD(0x2a,0x00),
	_INIT_DCS_CMD(0x2b,0x00),
	_INIT_DCS_CMD(0x2c,0x00),
	_INIT_DCS_CMD(0x2d,0x00),
	_INIT_DCS_CMD(0x2e,0x00),
	_INIT_DCS_CMD(0x2f,0x00),
	_INIT_DCS_CMD(0x30,0x00),
	_INIT_DCS_CMD(0x31,0x00),
	_INIT_DCS_CMD(0x32,0x00),
	_INIT_DCS_CMD(0x33,0x00),
	_INIT_DCS_CMD(0x34,0x03),
	_INIT_DCS_CMD(0x35,0x00),
	_INIT_DCS_CMD(0x36,0x00),
	_INIT_DCS_CMD(0x37,0x00),
	_INIT_DCS_CMD(0x38,0x96),
	_INIT_DCS_CMD(0x39,0x00),
	_INIT_DCS_CMD(0x3a,0x00),
	_INIT_DCS_CMD(0x3b,0x00),
	_INIT_DCS_CMD(0x3c,0x00),
	_INIT_DCS_CMD(0x3d,0x00),
	_INIT_DCS_CMD(0x3e,0x00),
	_INIT_DCS_CMD(0x3f,0x00),
	_INIT_DCS_CMD(0x40,0x00),
	_INIT_DCS_CMD(0x41,0x00),
	_INIT_DCS_CMD(0x42,0x00),
	_INIT_DCS_CMD(0x43,0x00),
	_INIT_DCS_CMD(0x44,0x00),
	_INIT_DCS_CMD(0x50,0x00),
	_INIT_DCS_CMD(0x51,0x23),
	_INIT_DCS_CMD(0x52,0x45),
	_INIT_DCS_CMD(0x53,0x67),
	_INIT_DCS_CMD(0x54,0x89),
	_INIT_DCS_CMD(0x55,0xAB),
	_INIT_DCS_CMD(0x56,0x01),
	_INIT_DCS_CMD(0x57,0x23),
	_INIT_DCS_CMD(0x58,0x45),
	_INIT_DCS_CMD(0x59,0x67),
	_INIT_DCS_CMD(0x5a,0x89),
	_INIT_DCS_CMD(0x5b,0xAB),
	_INIT_DCS_CMD(0x5c,0xCD),
	_INIT_DCS_CMD(0x5d,0xEF),
	_INIT_DCS_CMD(0x5e,0x00),
	_INIT_DCS_CMD(0x5f,0x08),
	_INIT_DCS_CMD(0x60,0x08),
	_INIT_DCS_CMD(0x61,0x06),
	_INIT_DCS_CMD(0x62,0x06),
	_INIT_DCS_CMD(0x63,0x01),
	_INIT_DCS_CMD(0x64,0x01),
	_INIT_DCS_CMD(0x65,0x00),
	_INIT_DCS_CMD(0x66,0x00),
	_INIT_DCS_CMD(0x67,0x02),
	_INIT_DCS_CMD(0x68,0x15),
	_INIT_DCS_CMD(0x69,0x15),
	_INIT_DCS_CMD(0x6a,0x14),
	_INIT_DCS_CMD(0x6b,0x14),
	_INIT_DCS_CMD(0x6c,0x0D),
	_INIT_DCS_CMD(0x6d,0x0D),
	_INIT_DCS_CMD(0x6e,0x0C),
	_INIT_DCS_CMD(0x6f,0x0C),
	_INIT_DCS_CMD(0x70,0x0F),
	_INIT_DCS_CMD(0x71,0x0F),
	_INIT_DCS_CMD(0x72,0x0E),
	_INIT_DCS_CMD(0x73,0x0E),
	_INIT_DCS_CMD(0x74,0x02),
	_INIT_DCS_CMD(0x75,0x08),
	_INIT_DCS_CMD(0x76,0x08),
	_INIT_DCS_CMD(0x77,0x06),
	_INIT_DCS_CMD(0x78,0x06),
	_INIT_DCS_CMD(0x79,0x01),
	_INIT_DCS_CMD(0x7a,0x01),
	_INIT_DCS_CMD(0x7b,0x00),
	_INIT_DCS_CMD(0x7c,0x00),
	_INIT_DCS_CMD(0x7d,0x02),
	_INIT_DCS_CMD(0x7e,0x15),
	_INIT_DCS_CMD(0x7f,0x15),
	_INIT_DCS_CMD(0x80,0x14),
	_INIT_DCS_CMD(0x81,0x14),
	_INIT_DCS_CMD(0x82,0x0D),
	_INIT_DCS_CMD(0x83,0x0D),
	_INIT_DCS_CMD(0x84,0x0C),
	_INIT_DCS_CMD(0x85,0x0C),
	_INIT_DCS_CMD(0x86,0x0F),
	_INIT_DCS_CMD(0x87,0x0F),
	_INIT_DCS_CMD(0x88,0x0E),
	_INIT_DCS_CMD(0x89,0x0E),
	_INIT_DCS_CMD(0x8A,0x02),
	_INIT_DCS_CMD(0xFF,0x98,0x81,0x04),
	_INIT_DCS_CMD(0xC5,0x3A),
	_INIT_DCS_CMD(0x6E,0x2B),
	_INIT_DCS_CMD(0x6F,0x37),
	_INIT_DCS_CMD(0x3A,0x24),
	_INIT_DCS_CMD(0x8D,0x1A),
	_INIT_DCS_CMD(0x87,0xBA),
	_INIT_DCS_CMD(0xB2,0xD1),
	_INIT_DCS_CMD(0x88,0x0B),
	_INIT_DCS_CMD(0x38,0x01),
	_INIT_DCS_CMD(0x39,0x00),
	_INIT_DCS_CMD(0xB5,0x02),
	_INIT_DCS_CMD(0x31,0x25),
	_INIT_DCS_CMD(0x3B,0x98),
	_INIT_DCS_CMD(0xFF,0x98,0x81,0x01),
	_INIT_DCS_CMD(0x22,0x0A),
	_INIT_DCS_CMD(0x31,0x00),
	_INIT_DCS_CMD(0x53,0x4D),
	_INIT_DCS_CMD(0x55,0x3D),
	_INIT_DCS_CMD(0x50,0x85),
	_INIT_DCS_CMD(0x51,0x80),
	_INIT_DCS_CMD(0x60,0x06),
	_INIT_DCS_CMD(0x62,0x20),
	_INIT_DCS_CMD(0xA0,0x00),
	_INIT_DCS_CMD(0xA1,0x21),
	_INIT_DCS_CMD(0xA2,0x35),
	_INIT_DCS_CMD(0xA3,0x19),
	_INIT_DCS_CMD(0xA4,0x1E),
	_INIT_DCS_CMD(0xA5,0x33),
	_INIT_DCS_CMD(0xA6,0x27),
	_INIT_DCS_CMD(0xA7,0x26),
	_INIT_DCS_CMD(0xA8,0xAF),
	_INIT_DCS_CMD(0xA9,0x1B),
	_INIT_DCS_CMD(0xAA,0x27),
	_INIT_DCS_CMD(0xAB,0x8D),
	_INIT_DCS_CMD(0xAC,0x1A),
	_INIT_DCS_CMD(0xAD,0x1B),
	_INIT_DCS_CMD(0xAE,0x50),
	_INIT_DCS_CMD(0xAF,0x26),
	_INIT_DCS_CMD(0xB0,0x2B),
	_INIT_DCS_CMD(0xB1,0x54),
	_INIT_DCS_CMD(0xB2,0x5E),
	_INIT_DCS_CMD(0xB3,0x23),
	_INIT_DCS_CMD(0xC0,0x00),
	_INIT_DCS_CMD(0xC1,0x21),
	_INIT_DCS_CMD(0xC2,0x35),
	_INIT_DCS_CMD(0xC3,0x19),
	_INIT_DCS_CMD(0xC4,0x1E),
	_INIT_DCS_CMD(0xC5,0x33),
	_INIT_DCS_CMD(0xC6,0x27),
	_INIT_DCS_CMD(0xC7,0x26),
	_INIT_DCS_CMD(0xC8,0xAF),
	_INIT_DCS_CMD(0xC9,0x1B),
	_INIT_DCS_CMD(0xCA,0x27),
	_INIT_DCS_CMD(0xCB,0x8D),
	_INIT_DCS_CMD(0xCC,0x1A),
	_INIT_DCS_CMD(0xCD,0x1B),
	_INIT_DCS_CMD(0xCE,0x50),
	_INIT_DCS_CMD(0xCF,0x26),
	_INIT_DCS_CMD(0xD0,0x2B),
	_INIT_DCS_CMD(0xD1,0x54),
	_INIT_DCS_CMD(0xD2,0x5E),
	_INIT_DCS_CMD(0xD3,0x23), 		
	_INIT_DCS_CMD(0xFF,0x98,0x81,0x00),
	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
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


#define LCM_WIDTH		800
#define LCM_HEIGHT		1280

#define VSA				4
#define VBP				20
#define VFP				20

#define HSA				20
#define HBP				20
#define HFP				40

#define FPS				60

#define PLL_CLK			218

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
	.vfp_low_power = 840,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.data_rate = PLL_CLK*2,
	.rotate = 1,
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
	cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ZERO);
	mdelay(2);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ZERO);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ZERO);
	mdelay(2);
	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ZERO);

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
	cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ONE);
	mdelay(2);
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

	cust_gpio_set_value(CUST_GPIO_BIAS_EN, GPIO_OUT_ONE);

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


struct cust_drm_lcm m100t_ili9881c_bns101b40hd981d113e_wxga = {
	.name = "m100t_ili9881c_bns101b40hd981d113e_wxga",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
