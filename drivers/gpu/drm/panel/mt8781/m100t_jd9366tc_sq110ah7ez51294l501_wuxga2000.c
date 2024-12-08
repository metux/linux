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
    _INIT_DCS_CMD(0x30,0x01),
    _INIT_DCS_CMD(0x78,0x49,0x61,0x02,0x00),
    _INIT_DCS_CMD(0x30,0x02),
    _INIT_DCS_CMD(0x31,0x11),
    _INIT_DCS_CMD(0x32,0x08),
    _INIT_DCS_CMD(0x33,0x24),
    _INIT_DCS_CMD(0x3D,0x42),
    _INIT_DCS_CMD(0x3E,0xFF),
    _INIT_DCS_CMD(0x3F,0x30),
    _INIT_DCS_CMD(0x42,0x81),
    _INIT_DCS_CMD(0x43,0x30),
    _INIT_DCS_CMD(0x44,0x21),
    _INIT_DCS_CMD(0x46,0x07),
    _INIT_DCS_CMD(0x49,0xC3),
    _INIT_DCS_CMD(0x4A,0x3F),
    _INIT_DCS_CMD(0x41,0x00,0x00,0x10,0x10,0x00,0x00,0x10,0x10,0x00,0x00,0x00,0x00),
    _INIT_DCS_CMD(0x5A,0x23,0x23,0x23,0x23,0x23,0x22,0x23,0x20,0x21,0x08,0x0A),
    _INIT_DCS_CMD(0x5B,0x0C,0x0E,0x00,0x18,0x23,0x23,0x23,0x23,0x23,0x23,0x23),
    _INIT_DCS_CMD(0x5C,0x23,0x23,0x23,0x23,0x23,0x22,0x23,0x20,0x21,0x09,0x0B),
    _INIT_DCS_CMD(0x5D,0x0D,0x0F,0x01,0x19,0x23,0x23,0x23,0x23,0x23,0x23,0x23),
    _INIT_DCS_CMD(0x5E,0x23,0x23,0x23,0x23,0x23,0x23,0x22,0x20,0x21,0x0F,0x0D),
    _INIT_DCS_CMD(0x5F,0x0B,0x09,0x19,0x01,0x23,0x23,0x23,0x23,0x23,0x23,0x23),
    _INIT_DCS_CMD(0x60,0x23,0x23,0x23,0x23,0x23,0x23,0x22,0x20,0x21,0x0E,0x0C),
    _INIT_DCS_CMD(0x61,0x0A,0x08,0x18,0x00,0x23,0x23,0x23,0x23,0x23,0x23,0x23),
    _INIT_DCS_CMD(0x4C,0xA0,0xA0),
    _INIT_DCS_CMD(0x68,0x00),
    _INIT_DCS_CMD(0x4E,0xFF,0xFF),
    _INIT_DCS_CMD(0x50,0x00,0x00),
    _INIT_DCS_CMD(0x55,0xFF,0xFF),
    _INIT_DCS_CMD(0x56,0x00,0x00),
    _INIT_DCS_CMD(0x66,0xFF,0xFF),
    _INIT_DCS_CMD(0x67,0x00,0x00),
    _INIT_DCS_CMD(0x64,0xE0,0x7F,0x00),
    _INIT_DCS_CMD(0x65,0xE0,0x7F,0x00),
    _INIT_DCS_CMD(0x6F,0x20,0x01,0x00),
    _INIT_DCS_CMD(0x70,0x20,0x01,0x00),
    _INIT_DCS_CMD(0x71,0xE0,0x7F,0x00),
    _INIT_DCS_CMD(0x72,0xE0,0x7F,0x00),
    _INIT_DCS_CMD(0x30,0x05),
    _INIT_DCS_CMD(0x31,0x00),
    _INIT_DCS_CMD(0x40,0x6F,0x39,0x39),
    _INIT_DCS_CMD(0x30,0x07),
    _INIT_DCS_CMD(0x31,0xC0),
    _INIT_DCS_CMD(0x30,0x02),
    _INIT_DCS_CMD(0x62,0x00),
    _INIT_DCS_CMD(0x63,0x00),
    _INIT_DCS_CMD(0x30,0x08),
    _INIT_DCS_CMD(0x31,0x34),
    _INIT_DCS_CMD(0x33,0x05),
    _INIT_DCS_CMD(0x40,0x46),
    _INIT_DCS_CMD(0x41,0x31),
    _INIT_DCS_CMD(0x42,0x1C),
    _INIT_DCS_CMD(0x47,0x02),
    _INIT_DCS_CMD(0x48,0x0C),
    _INIT_DCS_CMD(0x4A,0x0C),
    _INIT_DCS_CMD(0x4B,0x02),
    _INIT_DCS_CMD(0x4C,0xA0),
    _INIT_DCS_CMD(0x4E,0xA0),
    _INIT_DCS_CMD(0x50,0x0A),
    _INIT_DCS_CMD(0x52,0x83),
    _INIT_DCS_CMD(0x5A,0xB0),
    _INIT_DCS_CMD(0x5B,0xD0),
    _INIT_DCS_CMD(0x5C,0x74),
    _INIT_DCS_CMD(0x62,0x05),
    _INIT_DCS_CMD(0x64,0xE0),
    _INIT_DCS_CMD(0x65,0x5F),
    _INIT_DCS_CMD(0x7E,0x2A),
    _INIT_DCS_CMD(0x5e,0x00),
    _INIT_DCS_CMD(0x73,0x01),
    _INIT_DCS_CMD(0x30,0x0A),
    _INIT_DCS_CMD(0x32,0x3F),
    _INIT_DCS_CMD(0x33,0x30),
    _INIT_DCS_CMD(0x3f,0x50),
    _INIT_DCS_CMD(0x42,0x31),
    _INIT_DCS_CMD(0x43,0x00),
    _INIT_DCS_CMD(0x49,0x03),
    _INIT_DCS_CMD(0x30,0x0b),
    _INIT_DCS_CMD(0x33,0x00,0x39),
    _INIT_DCS_CMD(0x3c,0x00,0x39),
    _INIT_DCS_CMD(0x43,0xAD),
    _INIT_DCS_CMD(0x44,0x2D),
    _INIT_DCS_CMD(0x3E,0x08,0x0D,0x17,0x1E,0x25),
    _INIT_DCS_CMD(0x3F,0x42,0x5B,0x60,0x66,0x60,0x76,0x78,0x79,0x85,0x81,0x89,0x92,0xA6,0xA5),
    _INIT_DCS_CMD(0x40,0x51,0x58,0x61,0x64,0x08,0x0D,0x17,0x1E,0x25),
    _INIT_DCS_CMD(0x41,0x42,0x5B,0x60,0x66,0x60,0x76,0x78,0x79,0x85,0x81,0x89,0x92,0xA6,0xA5),
    _INIT_DCS_CMD(0x42,0x51,0x58,0x61,0x64),
    _INIT_DCS_CMD(0x45,0x6B),
    _INIT_DCS_CMD(0x46,0x30),
    _INIT_DCS_CMD(0x48,0x3D),
    _INIT_DCS_CMD(0x49,0x14),
    _INIT_DCS_CMD(0x4A,0x29),
    _INIT_DCS_CMD(0x30,0x0C),
    _INIT_DCS_CMD(0x32,0x62),
    _INIT_DCS_CMD(0x42,0x34),
    _INIT_DCS_CMD(0x5E,0x00),
    _INIT_DCS_CMD(0x6A,0x7f),
    _INIT_DCS_CMD(0x62,0x41),
    _INIT_DCS_CMD(0x30,0x0d),
    _INIT_DCS_CMD(0x4C,0x74),
    _INIT_DCS_CMD(0x30,0x08),
    _INIT_DCS_CMD(0x52,0xC3),
    _INIT_DCS_CMD(0x54,0x20),
    _INIT_DCS_CMD(0x30,0x08),
    _INIT_DCS_CMD(0x30,0x03),
    _INIT_DCS_CMD(0x31,0x1F),
    _INIT_DCS_CMD(0x3C,0xE8),
    _INIT_DCS_CMD(0x30,0x00),
    _INIT_DCS_CMD(0x11,0x00),
    _INIT_DELAY_CMD(120),
    _INIT_DCS_CMD(0x29,0x00),
    _INIT_DELAY_CMD(50),
    _INIT_DCS_CMD(0x35,0x00),
	{} //it must be here
};

static const struct drm_display_mode common_default_mode = {
	.clock = 168940,
	.hdisplay = 1200,
	.hsync_start = 1200 + 40,//HFP
	.hsync_end = 1200 + 40 + 8,//HSA
	.htotal = 1200 + 40 + 8 + 28,//HBP
	.vdisplay = 2000,
	.vsync_start = 2000 + 44,//VFP
	.vsync_end = 2000 + 44 + 8,//VSA
	.vtotal = 2000 + 44 + 8 + 38,//VBP
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
	.pll_clk = 510,//480,
	.vfp_low_power = 840,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.data_rate = 1020,//960,
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
//add by LQ start
static const struct panel_init_cmd temp_cmd[] = {
	_INIT_DCS_CMD(0x30,0x0B),
	_INIT_DCS_CMD(0x31,0x01),
	_INIT_DCS_CMD(0x30,0x00),
	{}
};
//add by LQ end

static int common_panel_enter_sleep_mode(struct common_panel *cm)
{
	struct mipi_dsi_device *dsi = cm->dsi;
	int ret,err,i;
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;

	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	msleep(120);

	//add  by LQ start
	for (i = 0; temp_cmd[i].len != 0; i++) {
			const struct panel_init_cmd *cmd = &temp_cmd[i];

			switch (cmd->type) {
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
				printk("LQ >>> %s failed to write command %u\n",__func__, i);
				//return err;
			}
		}
	msleep(20);//LQ
//add by LQ end

	return 0;
}

static int common_panel_unprepare_power(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);

	if (!cm->prepared_power)
		return 0;

	printk("LQ  [Kernel/LCM] %s enter\n", __func__);
#if 0//ndef CONFIG_WB_TP_INCELL_SUPPORT
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(5);
	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ZERO);
	mdelay(5);
	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ZERO);
	mdelay(5);
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ZERO);
	mdelay(5);
#endif
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
	printk("LQ [Kernel/LCM] %s enter\n", __func__);
#ifndef CONFIG_WB_TP_INCELL_SUPPORT
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ONE);
	//cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ONE);
	msleep(10);

	cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ONE);
	usleep_range(2000, 2001);
	cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ONE);

	msleep(20);

	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(20);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(20);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(120);
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


struct cust_drm_lcm m100t_jd9366tc_sq110ah7ez51294l501_wuxga2000 = {
	.name = "m100t_jd9366tc_sq110ah7ez51294l501_wuxga2000",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
