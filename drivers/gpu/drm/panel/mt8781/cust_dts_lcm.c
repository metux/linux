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

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20220421
#include <mt-plat/csci.h>
#endif


#if !IS_ENABLED(CONFIG_CM_CUST_GPIOS_SUPPORT)
#define cust_gpio_set_value(x,y)	pr_info("__func__ no implement! \n",__func__)
#endif

//Leo 20220421
#include "panel_common.h" 

#ifndef DTS_SUPPORT //Leo 20230828
#define DTS_SUPPORT
#endif

#if IS_ENABLED(CONFIG_DRM_PANEL_IT6112) //Leo 20230111
extern void it6112_init(void);
extern void it6112_mipi_power_off(void);
extern void it6112_init_v2(const struct dcs_setting_entry *dcs_setting_table, int intcmds_size, enum dcs_cmd_name start, int count);
#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT) //Leo 20230413
extern void it6113_set_bright(unsigned int level);
extern void gm8773c_config_v2(const unsigned char *arr, unsigned long size);
extern void gm8773c_init_v2(struct dcs_setting_entry *dcs_setting_table, int start, int count);
#endif
#endif


/*
#define _INIT_DCS_CMD(...) { \
	.type = INIT_DCS_CMD, \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

*/

static const struct panel_init_cmd cm_init_cmd[] = {
	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DELAY_CMD(20),
	{} //it must be here
};


#define LCM_WIDTH		1200
#define LCM_HEIGHT		2000

#define HFP				6
#define HBP				6
#define HSA				5

#define VFP				255
#define VBP				34
#define VSA				6

#define FPS				60

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
		#if 1//101
		.width_mm = 135,
		.height_mm = 226,
		#else
		.width_mm = 108,
		.height_mm = 172,
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

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	pr_info("Leo %s \n",__func__);

	return 0;
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	//struct drm_display_mode *m = get_mode_by_id_hfp(connector, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d fps:%d\n", __func__, cur_mode, dst_mode);

	return ret;
}


static struct mtk_panel_params ext_params = {
	.pll_clk = 547,
	//.vfp_low_power = 840,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9D,
	},
	.data_rate = 1076,
	.rotate = 1,
	.ssc_enable = 0,
};

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
#if IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT) //Leo 20240419
	it6113_set_bright(level);
#else
	char bl_tb[] = {0x51, 0x07, 0xff};

	bl_tb[1] = (level >> 8) & 0x7;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	//atomic_set(&current_backlight, level);
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
#endif

	//pr_info("%s %d\n", __func__, level);
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
};
#endif

static inline struct common_panel *to_common_panel(struct drm_panel *panel)
{
	return container_of(panel, struct common_panel, base);
}


#ifdef DTS_SUPPORT //Leo 20230825
static int common_panel_send_cmds(struct common_panel *cm,
				const void *data, int size)
{
	struct mipi_dsi_device *dsi = cm->dsi;
	const struct dsi_cmd_desc *cmds = data;
	u16 len;

	if ((cmds == NULL) || (dsi == NULL))
		return -EINVAL;


	while (size > 0) {
		len = cmds->wc_l;
		
			mipi_dsi_dcs_write_buffer(dsi, cmds->payload, len);

		if (cmds->wait)
			msleep(cmds->wait);
		cmds = (const struct dsi_cmd_desc *)(cmds->payload + len);
		size -= (len + 2);
	}

	return 0;
}
#else
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
#endif

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
#ifdef DTS_SUPPORT //Leo 20230828
	struct gpio_timing *timing;
	int items, i;
#endif

	if (!cm->prepared_power)
		return 0;

	pr_notice("Leo [Kernel/LCM] %s enter\n", __func__);

#ifdef DTS_SUPPORT //Leo 20230828
#if IS_ENABLED(CONFIG_DRM_PANEL_IT6112) //Leo 20230111
	if (cm->info.dualmipi_ic_support != DUALMIPI_NONE) {
		it6112_mipi_power_off();
	}
#endif 
	items = cm->info.power_off_seq.items;
	timing = cm->info.power_off_seq.timing;
	for (i = 0; i < items; i++) {
		cust_gpio_set_value(timing[i].gpio, timing[i].level);
		if (timing[i].delay != 0)
			mdelay(timing[i].delay);
	}

#if IS_ENABLED(CONFIG_WB_IT6112_I2C_SUSPEND_LOW) //Leo 20240329
	if (cm->info.dualmipi_ic_support != DUALMIPI_NONE) {
		cust_i2c_pull_down();
	}
#endif
#else /*DTS_SUPPORT*/
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ZERO);
	mdelay(10);
#endif

	cm->prepared_power = false;

	return 0;
}

static int common_panel_unprepare(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);
	int ret;
#ifdef DTS_SUPPORT //Leo 20230828
	struct gpio_timing *timing;
	int items, i;
#endif


	if (!cm->prepared)
		return 0;

#ifdef DTS_SUPPORT //Leo 20230828
	items = cm->info.power_off_seq_before_cmd.items;
	timing = cm->info.power_off_seq_before_cmd.timing;
	for (i = 0; i < items; i++) {
		cust_gpio_set_value(timing[i].gpio, !!timing[i].level);
		if (timing[i].delay != 0)
			mdelay(timing[i].delay);
	}
#endif

#if IS_ENABLED(CONFIG_DRM_PANEL_IT6112) //Leo 20230111
	if (cm->info.dualmipi_ic_support == DUALMIPI_NONE) {
		if (!cm->desc->discharge_on_disable) {
			ret = common_panel_enter_sleep_mode(cm);
			if (ret < 0) {
				pr_notice("failed to set panel off: %d\n",
					ret);
				return ret;
			}
		}
	}
#else 
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
#ifdef DTS_SUPPORT //Leo 20230828
	struct gpio_timing *timing;
	int items, i;
#endif
#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT) //Leo 20230413
	const unsigned char *cfg;
#endif

	if (cm->prepared_power)
		return 0;
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

#ifdef DTS_SUPPORT //Leo 20230828
#if IS_ENABLED(CONFIG_WB_IT6112_I2C_SUSPEND_LOW) //Leo 20240329
	if (cm->info.dualmipi_ic_support != DUALMIPI_NONE) {
		cust_i2c_pull_up();
	}
#endif
	items = cm->info.power_on_seq.items;
	timing = cm->info.power_on_seq.timing;
	for (i = 0; i < items; i++) {
		cust_gpio_set_value(timing[i].gpio, timing[i].level);
		if (timing[i].delay != 0)
			mdelay(timing[i].delay);
#if IS_ENABLED(CONFIG_WB_BIAS_SUPPORT) //Leo 20240507
		if (CUST_GPIO_BIAS_NEG == timing[i].gpio) {
			if (cm->info.avdd_avee_voltage > 0) {
				cust_bias_i2c_write_bytes(0x00, cm->info.avdd_avee_voltage);
			}
		}

		if (CUST_GPIO_BIAS_POS == timing[i].gpio) {
			if (cm->info.avdd_avee_voltage > 0) {
				cust_bias_i2c_write_bytes(0x01, cm->info.avdd_avee_voltage);
			}
		}
#endif
	}
#if IS_ENABLED(CONFIG_DRM_PANEL_IT6112) //Leo 20230111
	if (cm->info.dualmipi_ic_support != DUALMIPI_NONE) {
		if (cm->info.dualmipi_ic_support != DUALMIPI_GM8733C) {
			it6112_init_v2(cm->info.s_dcs_cmd, cm->info.dualmipi_cmd_count, 0, cm->info.dualmipi_cmd_count);
#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT) //Leo 20230413
		} else {
			cfg = cm->info.cmds[CMD_DUALMIPI_CONFIG];
			gm8773c_config_v2(cfg, cm->info.cmds_len[CMD_DUALMIPI_CONFIG]);
			gm8773c_init_v2(cm->info.s_dcs_cmd, 0, cm->info.dualmipi_cmd_count);
#endif
		}
		//it6112_init();

		mdelay(150);
	}
#endif
#else
	cust_gpio_set_value(CUST_GPIO_LCM_1V8, GPIO_OUT_ONE);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(10);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(2);
	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(5);
#endif

	cm->prepared_power = true;

	return 0;
}

static int common_panel_prepare(struct drm_panel *panel)
{
	struct common_panel *cm = to_common_panel(panel);
	int ret = 0;
#ifdef DTS_SUPPORT //Leo 20230828
	struct gpio_timing *timing;
	int items, i;
#endif

	if (cm->prepared)
		return 0;

	common_panel_prepare_power(panel);

#ifdef DTS_SUPPORT //Leo 20230825
	if (cm->info.debug.cmds[CMD_CODE_INIT]) {
		ret = common_panel_send_cmds(cm,
				cm->info.debug.cmds[CMD_CODE_INIT],
				cm->info.debug.cmds_len[CMD_CODE_INIT]);
	} else {
		ret = common_panel_send_cmds(cm,
					cm->info.cmds[CMD_CODE_INIT],
					cm->info.cmds_len[CMD_CODE_INIT]);
	}
#else
	ret = common_panel_init_dcs_cmd(cm);
#endif
	if (ret < 0) {
		pr_notice("failed to init panel: %d\n", ret);
		return ret;
	}

#ifdef DTS_SUPPORT //Leo 20230828
	items = cm->info.power_on_seq_after_cmd.items;
	timing = cm->info.power_on_seq_after_cmd.timing;
	for (i = 0; i < items; i++) {
		cust_gpio_set_value(timing[i].gpio, timing[i].level);
		if (timing[i].delay != 0)
			mdelay(timing[i].delay);
	}
#endif


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
#ifdef DTS_SUPPORT //Leo 20230825
	int ret;
#endif

	if (!cm->enabled)
		return 0;

	if (cm->backlight) {
		cm->backlight->props.power = FB_BLANK_POWERDOWN;
		cm->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(cm->backlight);
	}

#ifdef DTS_SUPPORT //Leo 20230825
	ret = common_panel_send_cmds(cm,
				cm->info.cmds[CMD_CODE_SLEEP_IN],
				cm->info.cmds_len[CMD_CODE_SLEEP_IN]);
	if (ret < 0) {
		pr_notice("failed to init CMD_CODE_SLEEP_IN panel: %d\n", ret);
		return ret;
	}
#endif

	cm->enabled = false;

	return 0;
}

static int common_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct common_panel *cm = to_common_panel(panel);
#ifdef DTS_SUPPORT //Leo 20230825
	const struct drm_display_mode *m = &cm->info.mode;
#else 
	const struct drm_display_mode *m = cm->desc->modes;
#endif
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 202300330
	struct drm_display_mode *csci_mode = cust_get_drm_panel_mode();
#endif

	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, m);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		return -ENOMEM;
	}


#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230329

	pr_info("%s before common_panel_get_modes csci wb drm: LCM_WIDTH:%d HFP:%d HSA:%d HBP:%d "
				"LCM_HEIGHT:%d VFB:%d VSA:%d VBP:%d clock:%d \n",
	__func__,
	mode->hdisplay,
	mode->hsync_start,
	mode->hsync_end,
	mode->htotal,
	mode->vdisplay,
	mode->vsync_start,
	mode->vsync_end,
	mode->vtotal,
	mode->clock);

	drm_mode_copy_1(mode, csci_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u\n",
			csci_mode->hdisplay, csci_mode->vdisplay);
		return -ENOMEM;
	}

	pr_info("%s after common_panel_get_modes csci wb drm: LCM_WIDTH:%d HFP:%d HSA:%d HBP:%d "
				"LCM_HEIGHT:%d VFB:%d VSA:%d VBP:%d clock:%d \n",
	__func__,
	mode->hdisplay,
	mode->hsync_start,
	mode->hsync_end,
	mode->htotal,
	mode->vdisplay,
	mode->vsync_start,
	mode->vsync_end,
	mode->vtotal,
	mode->clock);
#endif

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

#ifdef DTS_SUPPORT //Leo 20230825
	connector->display_info.width_mm = cm->info.mode.width_mm;
	connector->display_info.height_mm = cm->info.mode.height_mm;
	connector->display_info.bpc = cm->info.bpc;
#else
	connector->display_info.width_mm = cm->desc->size.width_mm;
	connector->display_info.height_mm = cm->desc->size.height_mm;
	connector->display_info.bpc = cm->desc->bpc;
#endif

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


struct cust_drm_lcm dts_lcm_panel = {
	.name = "dts_lcm_panel",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
