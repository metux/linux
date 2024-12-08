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
_INIT_DCS_CMD(0xB9,0x83,0x10,0x2E),
_INIT_DCS_CMD(0xE1,0x00,0x00),
_INIT_DCS_CMD(0xE2,0x03),
_INIT_DCS_CMD(0xE9,0xCD),
_INIT_DCS_CMD(0xBB,0x01),
_INIT_DCS_CMD(0xE9,0x00),
_INIT_DCS_CMD(0xD1,0x67,0x0C,0xFF,0x05),
_INIT_DCS_CMD(0xBE,0x90,0x96,0xD2),
_INIT_DCS_CMD(0xD9,0x04,0x03,0xBF),
_INIT_DCS_CMD(0xB1,0x10,0xFA,0xAF,0xAF,0x21,0x21,0xB2,0x5F,0x3B,0x21,0x36,0x36,0x36,0x22,0x21,0x15,0x00),
_INIT_DCS_CMD(0xB2,0x00,0xB0,0x47,0x80,0x00,0x26,0xE0,0x29,0x13,0x00,0x00,0x00,0x15,0x20,0xD7,0x00),
_INIT_DCS_CMD(0xB4,0x6A,0x70,0x6A,0x70,0x6A,0x90,0x6A,0x90,0x01,0x9F,0x01,0x58,0x00,0xFF,0x00,0xFF),
_INIT_DCS_CMD(0xD2,0x21,0x21),
_INIT_DCS_CMD(0xB6,0x53,0x53,0xE3),//OTPVCOM后去掉此行
_INIT_DCS_CMD(0xBF,0xFC,0x85,0x80,0x9C,0x36,0x00,0x04),
_INIT_DCS_CMD(0xD3,0x00,0x00,0x00,0x00,0x3C,0xE8,0x00,0x00,0x00,0x37,0x37,0x33,0x0F,0x26,0x26,0x04,0x00,0x32,0x10,0x24,0x00,0x24,0x32,0x17,0xA8,0x07,0xA8,0x32,0x10,0x08,0x00,0x00,0x00,0x00,0x2B,0x50,0xB6,0xC8,0x2C,0x4F,0xB6,0xC8,0x0F),
_INIT_DCS_CMD(0xE0,0x00,0x02,0x07,0x0C,0x11,0x16,0x2B,0x31,0x38,0x36,0x53,0x5C,0x65,0x75,0x73,0x7A,0x83,0x97,0x99,0x4D,0x57,0x65,0x73,0x00,0x02,0x07,0x0C,0x11,0x16,0x2B,0x31,0x38,0x36,0x53,0x5C,0x65,0x75,0x73,0x7A,0x83,0x97,0x99,0x4D,0x57,0x65,0x73),
_INIT_DCS_CMD(0xBD,0x01),
_INIT_DCS_CMD(0xCB,0x80,0x36,0x12,0x16,0xC0,0x28,0x54,0x84,0x02,0x34),
_INIT_DCS_CMD(0xD3,0x01,0x00,0xFC,0x00,0x00,0x11,0x10,0x00,0x0A,0x00,0x01),
_INIT_DCS_CMD(0xBD,0x02),
_INIT_DCS_CMD(0xB4,0x4E,0x00,0x33,0x11,0x33,0x88),
_INIT_DCS_CMD(0xBF,0xF2,0x00,0x02),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xC0,0x33,0x33,0x22,0x11,0xA2,0x17,0x00,0x80,0x00,0x00,0x08,0x00,0x63,0x63),
_INIT_DCS_CMD(0xD5,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x18,0x18,0x1A,0x1A,0x1B,0x1B,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x20,0x21,0x24,0x25,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18),
_INIT_DCS_CMD(0xD6,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,0x25,0x24,0x21,0x20,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18),
_INIT_DCS_CMD(0xD0,0x07,0x04,0x05),
_INIT_DCS_CMD(0xE7,0x12,0x13,0x02,0x02,0x55,0x00,0x0E,0x0E,0x00,0x26,0x29,0x72,0x1C,0x72,0x01,0x27,0x00,0x00,0x00,0x00,0x17,0x00,0x68),
_INIT_DCS_CMD(0xBD,0x01),
_INIT_DCS_CMD(0xE7,0x01,0x30,0x01,0x94,0x0D,0xB5,0x0E),
_INIT_DCS_CMD(0xBD,0x02),
_INIT_DCS_CMD(0xE7,0xFF,0x01,0xFD,0x01,0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x81,0x00,0x02,0x40),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xD8,0xAA,0xAB,0xEA,0xAA,0xAA,0xA0,0xAA,0xAB,0xEA,0xAA,0xAA,0xA0),
_INIT_DCS_CMD(0xBD,0x01),
_INIT_DCS_CMD(0xD8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
_INIT_DCS_CMD(0xBD,0x02),
_INIT_DCS_CMD(0xD8,0xFF,0xFF,0xBF,0xFF,0xFF,0xF0,0xFF,0xFF,0xBF,0xFF,0xFF,0xF0),
_INIT_DCS_CMD(0xBD,0x03),
_INIT_DCS_CMD(0xD8,0x55,0x55,0x55,0x55,0x55,0x50,0x55,0x55,0x55,0x55,0x55,0x50,0x55,0x55,0x55,0x55,0x55,0x50,0x55,0x55,0x55,0x55,0x55,0x50),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xCC,0x02),
_INIT_DCS_CMD(0xBA,0x70,0x03,0xA8,0x83,0xF2,0x80,0xC0,0x0D),
_INIT_DCS_CMD(0xBD,0x03),
_INIT_DCS_CMD(0xB2,0x80),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xC8,0x00,0x04,0x04,0x00,0x00,0x82),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xCB,0x00,0x13,0x08,0x02,0x25),
_INIT_DCS_CMD(0xBD,0x02),
_INIT_DCS_CMD(0xCB,0x00,0x03,0x00,0x01,0x7E),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xC7,0x30,0xB0,0xC2,0x08,0x10,0x42,0x04),
_INIT_DCS_CMD(0xBD,0x00),
_INIT_DCS_CMD(0xD1,0x67,0x0C,0x0F),

	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DCS_CMD(0x35,0x00),
	_INIT_DELAY_CMD(200),
	{} //it must be here
};

//#define LCM_5
//#define LCM_6
//#define LCM_7
//#define LCM_8
//#define LCM_101
//#define LCM_1036
//#define LCM_1051
//#define LCM_1095
#define LCM_1295

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

#ifdef LCM_1295
#define WIDTH_MM    174
#define HEIGHT_MM   279
#endif

#ifndef WIDTH_MM
#define WIDTH_MM   137
#endif

#ifndef HEIGHT_MM
#define HEIGHT_MM  217
#endif 


#define LCM_WIDTH		1200
#define LCM_HEIGHT		1920

#define VSA				12
#define VBP				28
#define VFP				244

#define HSA				12
#define HBP				16
#define HFP				40

#define FPS				60

#define PLL_CLK			530

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


struct cust_drm_lcm m100t_hx83102e_sat129hs51im0007_wuxga = {
	.name = "m100t_hx83102e_sat129hs51im0007_wuxga",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
