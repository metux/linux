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
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFA,0x5A),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFF,0x82,0x05,0x01),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xFF,0x82,0x05),
	_INIT_DCS_CMD(0x00,0x93),
	_INIT_DCS_CMD(0xC5,0x6B),
	_INIT_DCS_CMD(0x00,0x97),
	_INIT_DCS_CMD(0xC5,0x6B),
	_INIT_DCS_CMD(0x00,0x9E),
	_INIT_DCS_CMD(0xC5,0x0A),
	_INIT_DCS_CMD(0x00,0x9A),
	_INIT_DCS_CMD(0xC5,0xCD),
	_INIT_DCS_CMD(0x00,0x9C),
	_INIT_DCS_CMD(0xC5,0xCD),
	_INIT_DCS_CMD(0x00,0xB6),
	_INIT_DCS_CMD(0xC5,0x57,0x57),
	_INIT_DCS_CMD(0x00,0xB8),
	_INIT_DCS_CMD(0xC5,0x43,0x43),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xD8,0xB4,0xB4),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xD9,0x00,0x7A,0x7A),
	_INIT_DCS_CMD(0x00,0x82),
	_INIT_DCS_CMD(0xC5,0x95),
	_INIT_DCS_CMD(0x00,0x83),
	_INIT_DCS_CMD(0xC5,0x07),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xE1,0x00,0x05,0x13,0x22,0x2B,0x35,0x46,0x54,0x56,0x64,0x67,0x7F,0x81,0x6B,0x69,0x5D),
	_INIT_DCS_CMD(0x00,0x10),
	_INIT_DCS_CMD(0xE1,0x54,0x48,0x39,0x2F,0x26,0x1B,0x15,0x14),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xE2,0x00,0x05,0x13,0x22,0x2B,0x35,0x46,0x54,0x56,0x64,0x67,0x7F,0x81,0x6B,0x69,0x5D),
	_INIT_DCS_CMD(0x00,0x10),
	_INIT_DCS_CMD(0xE2,0x54,0x48,0x39,0x2F,0x26,0x16,0x0A,0x07),
	_INIT_DCS_CMD(0x00,0xA1),
	_INIT_DCS_CMD(0xB3,0x04,0xB0),
	_INIT_DCS_CMD(0x00,0xA3),
	_INIT_DCS_CMD(0xB3,0x07,0xd0),
	_INIT_DCS_CMD(0x00,0xA5),
	_INIT_DCS_CMD(0xB3,0x80,0x13),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCB,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C),
	_INIT_DCS_CMD(0x00,0x87),
	_INIT_DCS_CMD(0xCB,0x3C),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xCB,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCB,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C),
	_INIT_DCS_CMD(0x00,0x97),
	_INIT_DCS_CMD(0xCB,0x33),
	_INIT_DCS_CMD(0x00,0x98),
	_INIT_DCS_CMD(0xCB,0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xCB,0x34,0x34,0x34,0x37,0x34,0x34,0x34,0x34),
	_INIT_DCS_CMD(0x00,0xA8),
	_INIT_DCS_CMD(0xCB,0x34,0x34,0x34,0x34,0x34,0x34),
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
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCC,0x2F,0x2F,0x2F,0x2F,0x2F,0x2F,0x31,0x02),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xCC,0x31,0x2C,0x2D,0x1A,0x18,0x16,0x14,0x12),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCC,0x10,0x0E,0x0C,0x04,0x2F,0x2F),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCD,0x2F,0x2F,0x2F,0x2F,0x2F,0x2F,0x31,0x01),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xCD,0x31,0x2C,0x2D,0x19,0x17,0x15,0x13,0x11),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCD,0x0F,0x0D,0x0B,0x03,0x2F,0x2F),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xC2,0x87,0x03,0x00,0x00),	
	_INIT_DCS_CMD(0x00,0x84),
	_INIT_DCS_CMD(0xC2,0x86,0x03,0x00,0x00),						
	_INIT_DCS_CMD(0x00,0xC0),//CK1
	_INIT_DCS_CMD(0xC2,0x83,0x07,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xC7),//CK2
	_INIT_DCS_CMD(0xC2,0x82,0x08,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xD0),//CK3
	_INIT_DCS_CMD(0xC2,0x81,0x09,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xD7),//CK4
	_INIT_DCS_CMD(0xC2,0x80,0x0A,0x00,0x03,0x00,0xBF,0x0F),					
	_INIT_DCS_CMD(0x00,0xE0),//CK5
	_INIT_DCS_CMD(0xC2,0x01,0x0B,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xE7),//CK6
	_INIT_DCS_CMD(0xC2,0x02,0x0C,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xF0),//CK7
	_INIT_DCS_CMD(0xC2,0x03,0x0D,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xF7),//CK8
	_INIT_DCS_CMD(0xC2,0x04,0x0E,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0x80),//CK9
	_INIT_DCS_CMD(0xC3,0x05,0x0F,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0x87),//CK10
	_INIT_DCS_CMD(0xC3,0x06,0x10,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0x90),//CK11
	_INIT_DCS_CMD(0xC3,0x07,0x11,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0x97),//CK12
	_INIT_DCS_CMD(0xC3,0x08,0x12,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xC0),
	_INIT_DCS_CMD(0xCD,0x09,0x13,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xC7),
	_INIT_DCS_CMD(0xCD,0x0A,0x14,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xD0),
	_INIT_DCS_CMD(0xCD,0x0B,0x15,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xD7),
	_INIT_DCS_CMD(0xCD,0x0C,0x16,0x00,0x03,0x00,0xBF,0x0F),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xC3,0x00,0x70,0xD3,0x07,0x00,0x00,0x00,0x00),	
	_INIT_DCS_CMD(0x00,0xA8),
	_INIT_DCS_CMD(0xC3,0x00,0x70,0xD4,0x08,0x00,0x00,0x00,0x00),	
	_INIT_DCS_CMD(0x00,0xF0),
	_INIT_DCS_CMD(0xCC,0x3D,0x88,0x88,0x00,0x00),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xC0,0x00,0xA8,0x00,0xF0,0x00,0x10),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xC0,0x00,0xA8,0x00,0xF0,0x00,0x10),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xC0,0x01,0x0A,0x00,0xF0,0x00,0x10),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xC0,0x00,0xA8,0x00,0xF0,0x10),
	_INIT_DCS_CMD(0x00,0xA3),
	_INIT_DCS_CMD(0xC1,0x00,0x22,0x00,0x22,0x00,0x04),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCE,0x00,0x81,0x0B,0x17,0x00,0x28,0x00,0xA8,0x00,0x28,0x00,0x54),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCE,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x0B,0x17,0x00,0x06,0x00,0x13,0x06),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xCE,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xCE,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xD0),
	_INIT_DCS_CMD(0xCE,0x01,0x00,0x0A,0x01,0x01,0x00,0xCA,0x01),
	_INIT_DCS_CMD(0x00,0xE1),
	_INIT_DCS_CMD(0xCE,0x0A,0x02,0x37,0x02,0x37 ,0x00,0x80),
	_INIT_DCS_CMD(0x00,0xF0),
	_INIT_DCS_CMD(0xCE,0x00,0x00,0x00 ,0x00,0x00,0x00,0x00,0x00,0x01,0x01),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xCF,0x05,0x05,0x00,0x04),
	_INIT_DCS_CMD(0x00,0xB5),
	_INIT_DCS_CMD(0xCF,0x03,0x03,0xC4,0xC8),
	_INIT_DCS_CMD(0x00,0xC0),
	_INIT_DCS_CMD(0xCF,0x07,0x07,0xEC,0xF0),
	_INIT_DCS_CMD(0x00,0xC5),
	_INIT_DCS_CMD(0xCF,0x00,0x07,0x1C,0xD0),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0x1C,0x00),
	_INIT_DCS_CMD(0x00,0xC9),
	_INIT_DCS_CMD(0xCE,0x00),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xC1,0xE0),
	_INIT_DCS_CMD(0x00,0x9F),
	_INIT_DCS_CMD(0xC5,0x00),
	_INIT_DCS_CMD(0x00,0x98),
	_INIT_DCS_CMD(0xC5,0x54),
	_INIT_DCS_CMD(0x00,0x91),
	_INIT_DCS_CMD(0xC5,0x4C),
	_INIT_DCS_CMD(0x00,0x8C),
	_INIT_DCS_CMD(0xCF,0x40,0x40),
	_INIT_DCS_CMD(0x00,0x93),	
	_INIT_DCS_CMD(0xC4,0x90),	
	_INIT_DCS_CMD(0x00,0xD7),	
	_INIT_DCS_CMD(0xC0,0xF0),	
	_INIT_DCS_CMD(0x00,0xA2),	
	_INIT_DCS_CMD(0xF5,0x1F),	
	_INIT_DCS_CMD(0x00,0x90),	
	_INIT_DCS_CMD(0xE9,0x10),	
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xB0,0x07),	
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFA,0x01),	
	_INIT_DCS_CMD(0x00,0xA8),	
	_INIT_DCS_CMD(0xC5,0x99),	
	_INIT_DCS_CMD(0x00,0xB6),
	_INIT_DCS_CMD(0xC5,0x55,0x55),
	_INIT_DCS_CMD(0x00,0xB8),
	_INIT_DCS_CMD(0xC5,0x41,0x41),
	_INIT_DCS_CMD(0x00,0x9A),	    
	_INIT_DCS_CMD(0xF5,0x00),	
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFA,0x5A),	
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xB0,0x01),	
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFF,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xFF,0x00,0x00),
	
	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DELAY_CMD(20),
	{} //it must be here
};

static const struct drm_display_mode common_default_mode = {
	.clock = 168940,
	.hdisplay = 1200,
	.hsync_start = 1200 + 24,//HFP
	.hsync_end = 1200 + 24 + 10,//HSA
	.htotal = 1200 + 24 + 10 + 23,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 232,//VFP
	.vsync_end = 2000 + 232 + 4,//VSA
	.vtotal = 2000 + 232 + 4 + 4,//VBP
};

static const struct panel_desc common_desc = {

	.modes = &common_default_mode,
	.bpc = 11,
	.size = {
		#if 0//101
		.width_mm = 137,
		.height_mm = 217,
		#else//1095
		.width_mm = 143,
		.height_mm = 248,
		#endif
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
	.pll_clk = 566,
	.vfp_low_power = 840,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.data_rate = 1000,
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


struct cust_drm_lcm m100t_ft8205_sq110aq7dz512_wuxga2000 = {
	.name = "m100t_ft8205_sq110aq7dz512_wuxga2000",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
