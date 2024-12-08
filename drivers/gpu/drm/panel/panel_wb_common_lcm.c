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
#include <linux/string.h>
#include <asm-generic/gpio.h>
#include <linux/proc_fs.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <video/videomode.h>

#include <video/mipi_display.h>
#include <video/of_display_timing.h>

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
#if IS_ENABLED(CONFIG_WB_DRM_LCM_SUPPORT) //Leo 20220427
#include WB_DRM_PAMEL_COMMON_H
#endif

#undef pr_fmt
#define pr_fmt(fmt)   KBUILD_MODNAME ": "fmt

//caozy add for lt9711 20240427
#if IS_ENABLED(CONFIG_WB_LT9711_SUPPROT)
#include "../mediatek/mediatek_v2/mtk_disp_notify.h"
struct notifier_block lt9711_disp_notifier;
int lt9711_bl_delay = 500;
#endif
//caozy add for lt9711 20240427
/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */

struct cust_drm_lcm  *common_lcm;
struct common_panel *g_cm = NULL;
	
struct lcm_size lcm_size_arr[] = {
/*	{5 , , },
	{6 , , },
	{7 , , },*/
	{8, 108, 172},
	{84, 115, 180},
	{101, 137, 217},
	{1036, 135, 226},
	{1050, 141, 226},
	{1095, 143, 238},
	{1260, 169, 272},
	{0xff, 108, 172}, //defualt
};

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230330
static void cust_set_lcm_porch_from_csci(struct drm_display_mode *modes)
{
	int temp;
	int LCM_WIDTH = 0,LCM_HEIGHT = 0, VSA = 0,VBP =0, VFP = 0,HSA = 0,HBP = 0,HFP = 0;
	
	struct drm_display_mode *csci_modes = cust_get_drm_panel_mode();

	if (csci_exist("ro.vendor.lcm.width")) { 
		temp = csci_integer("ro.vendor.lcm.width",0);
		if (temp > 0) {
			LCM_WIDTH = temp;
		} else {
			LCM_WIDTH = modes->hdisplay;
		}
	} else {
		LCM_WIDTH = modes->hdisplay;
	}

	if (csci_exist("ro.vendor.lcm.hsa")) { 
		temp = csci_integer("ro.vendor.lcm.hsa",0);
		if (temp > 0) {
			HSA = temp;
		} else {
			HSA = modes->hsync_end - modes->hsync_start;
		}
	} else {
		HSA = modes->hsync_end - modes->hsync_start;
	}
	
	if (csci_exist("ro.vendor.lcm.hbp")) { 
		temp = csci_integer("ro.vendor.lcm.hbp",0);
		if (temp > 0) {
			HBP = temp;
		} else {
			HBP = modes->htotal - modes->hsync_end;
		}
	} else {
		HBP = modes->htotal - modes->hsync_end;
	}

	if (csci_exist("ro.vendor.lcm.hfp")) { 
		temp = csci_integer("ro.vendor.lcm.hfp",0);
		if (temp > 0) {
			HFP = temp;
		} else {
			HFP =  modes->hsync_start - modes->hdisplay;
		}
	} else {
		HFP =  modes->hsync_start - modes->hdisplay;
	}

	if (csci_exist("ro.vendor.lcm.height")) { 
		temp = csci_integer("ro.vendor.lcm.height",0);
		if (temp > 0) {
			LCM_HEIGHT = temp;
		} else {
			LCM_HEIGHT = modes->vdisplay;
		}
	} else {
		LCM_HEIGHT = modes->vdisplay;
	}

	if (csci_exist("ro.vendor.lcm.vsa")) { 
		temp = csci_integer("ro.vendor.lcm.vsa",0);
		if (temp > 0) {
			VSA = temp;
		} else {
			VSA = modes->vsync_end - modes->vsync_start;
		}
	} else {
		VSA = modes->vsync_end - modes->vsync_start;
	}

	if (csci_exist("ro.vendor.lcm.vbp")) { 
		temp = csci_integer("ro.vendor.lcm.vbp",0);
		if (temp > 0) {
			VBP = temp;
		} else {
			VBP = modes->vtotal - modes->vsync_end;
		}
	} else {
		VBP = modes->vtotal - modes->vsync_end;
	}
	
	if (csci_exist("ro.vendor.lcm.vfp")) { 
		temp = csci_integer("ro.vendor.lcm.vfp",0);
		if (temp > 0) {
			VFP = temp;
		} else {
			HFP =  modes->hsync_start - modes->hdisplay;
		}
	} else {
		VFP =  modes->vsync_start - modes->vdisplay;
	}

	csci_modes->hdisplay = LCM_WIDTH;
	csci_modes->hsync_start = LCM_WIDTH + HFP;
	csci_modes->hsync_end = LCM_WIDTH + HFP + HSA;
	csci_modes->htotal = LCM_WIDTH + HFP + HSA + HBP;
	csci_modes->vdisplay = LCM_HEIGHT;
	csci_modes->vsync_start = LCM_HEIGHT + VFP;
	csci_modes->vsync_end = LCM_HEIGHT + VFP + VSA;
	csci_modes->vtotal = LCM_HEIGHT + VFP + VSA + VBP;
	csci_modes->clock = csci_modes->htotal * csci_modes->vtotal * 60 / 1000;

	pr_info("%s csci wb drm: LCM_WIDTH:%d HSA:%d HBP:%d HFP:%d "
				"LCM_HEIGHT:%d VSA:%d VBP:%d VFP:%d clock:%d \n",
			__func__,LCM_WIDTH,HSA,HBP,HFP, LCM_HEIGHT,VSA,VBP,VFP,csci_modes->clock);
}
#endif

static const struct panel_init_cmd cm_init_cmd[] = {

	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DELAY_CMD(20),
	{}
};

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

	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ZERO);
	mdelay(10);
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
	mdelay(10);

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

static const struct drm_display_mode cm_default_mode = {
	.clock = 74235,
	.hdisplay = 800,
	.hsync_start = 800 + 80,
	.hsync_end = 800 + 80 + 80,
	.htotal = 800 + 80 + 80 + 20,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 12,
	.vtotal = 1280 + 30 + 4 + 12,
};

static const struct panel_desc cm_desc = {

	.modes = &cm_default_mode,
	.bpc = 8,
	.size = {
		.width_mm = 108,
		.height_mm = 172,
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
	.pll_clk = 245,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

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

static int common_panel_add(struct common_panel *cm)
{
	struct device *dev = &cm->dsi->dev;
	struct device_node *backlight;
	int ret;

	//Leo 20220512
	if (common_lcm->p_common_funcs == NULL) {
		drm_panel_init(&cm->base, dev, &common_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	} else {
		drm_panel_init(&cm->base, dev, common_lcm->p_common_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	}

	ret = of_drm_get_panel_orientation(dev->of_node, &cm->orientation);
	if (ret < 0) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, ret);
		return ret;
	}

#ifndef CONFIG_MTK_DISP_NO_LK
	cm->prepared = true;
	cm->enabled = true;
	cm->prepared_power = true;
#endif

	//Leo 20220512
	if (common_lcm->p_common_funcs == NULL) {
		cm->base.funcs = &common_panel_funcs;
	} else {
		cm->base.funcs = common_lcm->p_common_funcs;
	}
	
	cm->base.dev = &cm->dsi->dev;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		cm->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!cm->backlight)
			return -EPROBE_DEFER;
	}

	drm_panel_add(&cm->base);
	return 0;
}

static int of_parse_buildin_modes(struct panel_info *info,
	struct device_node *lcd_node)
{
	int i, rc, num_timings;
	struct device_node *timings_np;


	timings_np = of_get_child_by_name(lcd_node, "display-timings");
	if (!timings_np) {
		pr_err("%s: can not find display-timings node\n",
			lcd_node->name);
		return -ENODEV;
	}

	num_timings = of_get_child_count(timings_np);
	if (num_timings == 0) {
		/* should never happen, as entry was already found above */
		pr_err("%s: no timings specified\n", lcd_node->name);
		goto done;
	}

	info->buildin_modes = kzalloc(sizeof(struct drm_display_mode) *
				num_timings, GFP_KERNEL);

	for (i = 0; i < num_timings; i++) {
		rc = of_get_drm_display_mode(lcd_node,
			&info->buildin_modes[i], NULL, i);
		if (rc) {
			pr_err("get display timing failed\n");
			goto entryfail;
		}

		info->buildin_modes[i].width_mm = info->mode.width_mm;
		info->buildin_modes[i].height_mm = info->mode.height_mm;
	}
	info->num_buildin_modes = num_timings;
	pr_info("info->num_buildin_modes = %d\n", num_timings);
	goto done;

entryfail:
	kfree(info->buildin_modes);
done:
	of_node_put(timings_np);

	return 0;
}

int of_parse_reset_seq(struct device_node *np,
				struct panel_info *info)
{
	struct property *prop;
	int bytes, rc;
	u32 *p;

	prop = of_find_property(np, "cust,power-on-sequence", &bytes);
	if (!prop) {
		pr_err("cust,power-on-sequence property not found\n");
		return -EINVAL;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "cust,power-on-sequence",
					p, bytes / 4);
	if (rc) {
		pr_err("parse cust,power-on-sequence failed\n");
		kfree(p);
		return rc;
	}

	info->power_on_seq.items = bytes / 12;
	info->power_on_seq.timing = (struct gpio_timing *)p;

	prop = of_find_property(np, "cust,power-off-sequence", &bytes);
	if (!prop) {
		pr_err("cust,power-off-sequence property not found\n");
		return -EINVAL;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "cust,power-off-sequence",
					p, bytes / 4);
	if (rc) {
		pr_err("parse cust,power-off-sequence failed\n");
		kfree(p);
		return rc;
	}

	info->power_off_seq.items = bytes / 12;
	info->power_off_seq.timing = (struct gpio_timing *)p;


	prop = of_find_property(np, "cust,power-on-sequence_after_cmd", &bytes);
	if (!prop) {
		pr_err("cust,power-on-sequence_after_cmd property not found\n");
		//return -EINVAL;
		info->power_on_seq_after_cmd.items = 0;
		goto pwr_off_before_cmd;
	}

	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "cust,power-on-sequence_after_cmd",
					p, bytes / 4);
	if (rc) {
		pr_err("parse cust,power-on-sequence_after_cmd failed\n");
		kfree(p);
		return rc;
	}

	info->power_on_seq_after_cmd.items = bytes / 12;
	info->power_on_seq_after_cmd.timing = (struct gpio_timing *)p;

pwr_off_before_cmd:
	prop = of_find_property(np, "cust,power-off-sequence_before_cmd", &bytes);
	if (!prop) {
		pr_err("cust,power-off-sequence_before_cmd property not found\n");
			info->power_off_seq_before_cmd.items = 0;
		return -EINVAL;
	}

	pr_info("power off :%d \n",bytes);
	p = kzalloc(bytes, GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	rc = of_property_read_u32_array(np, "cust,power-off-sequence_before_cmd",
					p, bytes / 4);
	if (rc) {
		pr_err("parse cust,power-off-sequence_before_cmd failed\n");
		kfree(p);
		return rc;
	}

	info->power_off_seq_before_cmd.items = bytes / 12;
	info->power_off_seq_before_cmd.timing = (struct gpio_timing *)p;

	return 0;
}

static void convert_dualmipi_initcmd(struct panel_info *p_info)
{
	int i,j,cmd_name;
	struct panel_info *info = p_info;
	int size = info->cmds_len[CMD_DUALMIPI_INIT];
	const struct dcs_setting_entry_u8 *cmds = info->cmds[CMD_DUALMIPI_INIT];

	info->s_dcs_cmd = kzalloc(info->dualmipi_cmd_count * info->dualmipi_one_cmd_size, GFP_KERNEL);

	printk("%s \n",__func__);

	i = 0;
	while (size > 0) {
		cmd_name = (cmds->cmd_name_h << 8 | cmds->cmd_name_l);

		//printk("0x%x 0x%x 0x%x 0x%x ",cmd_name,cmds->cmd, cmds->data_id, cmds->count);
		info->s_dcs_cmd[i].cmd_name = cmd_name;
		info->s_dcs_cmd[i].cmd = cmds->cmd;
		info->s_dcs_cmd[i].data_id = cmds->data_id;
		info->s_dcs_cmd[i].count = cmds->count;

		for (j = 0; j < cmds->count; j++) {
			//printk("0x%x ",cmds->para_list[j]);
			info->s_dcs_cmd[i].para_list[j] = cmds->para_list[j];
		}

		size -= (cmds->count + 5);
		cmds = (struct dcs_setting_entry_u8 *)(cmds->para_list + cmds->count);
		//printk("\n");
		//printf("\n size:%d %d\n",size,cmds->count + 5);
		i++;
	}
}

int common_panel_parse_lcddtb(struct device_node *lcd_node,
	struct panel_info *p_info)
{
	u32 val;
	struct panel_info *info = p_info;
	int bytes, rc;
	const void *p;
	const char *str;
	int i,lcm_size_index = 0;

	if (!lcd_node) {
		pr_err("Lcd node from dtb is Null\n");
		return -ENODEV;
	}
	info->of_node = lcd_node;

	rc = of_property_read_u32(lcd_node, "cust,dsi-work-mode", &val);
	if (!rc) {
		if (val == MIPI_DSI_MODE_VIDEO_BURST)
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					   MIPI_DSI_MODE_VIDEO_BURST;
		else if (val == MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			info->mode_flags = MIPI_DSI_MODE_VIDEO |
					   MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
		else if (val == MIPI_DSI_MODE_VIDEO)
			info->mode_flags = MIPI_DSI_MODE_VIDEO;
	} else {
		pr_err("dsi work mode is not found! use video mode\n");
		info->mode_flags = MIPI_DSI_MODE_VIDEO |
				   MIPI_DSI_MODE_VIDEO_BURST;
	}

	rc = of_property_read_u32(lcd_node, "cust,mipi_dsi_mode_lpm", &val);
	if (!rc) {
		if (val == 1) {
			info->mode_flags |= MIPI_DSI_MODE_LPM;
		} else {
			pr_err("read cust,mipi_dsi_mode_lpm failed\n");
		}
	}

	rc = of_property_read_u32(lcd_node, "cust,dsi-non-continuous-clock", &val);
	if (!rc) {
		if (val == 1) {
			info->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS;
		} else {
			pr_err("read cust,dsi-non-continuous-clock failed\n");
		}
	}

	rc = of_property_read_u32(lcd_node, "cust,dsi-lane-number", &val);
	if (!rc)
		info->lanes = val;
	else
		info->lanes = 4;

	rc = of_property_read_string(lcd_node, "cust,dsi-color-format", &str);
	if (rc)
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(str, "rgb888"))
		info->format = MIPI_DSI_FMT_RGB888;
	else if (!strcmp(str, "rgb666"))
		info->format = MIPI_DSI_FMT_RGB666;
	else if (!strcmp(str, "rgb666_packed"))
		info->format = MIPI_DSI_FMT_RGB666_PACKED;
	else if (!strcmp(str, "rgb565"))
		info->format = MIPI_DSI_FMT_RGB565;
	else
		pr_err("dsi-color-format (%s) is not supported\n", str);

	rc = of_property_read_u32(lcd_node, "cust,lcm_phy_size", &val);
	if (!rc)
		info->lcm_phy_size = val;
	else 
		info->lcm_phy_size = 8;

	for (i = 0;  i < sizeof(lcm_size_arr)/sizeof(struct lcm_size); i++) {
		if (lcm_size_arr[i].size == info->lcm_phy_size) {
			lcm_size_index = i;
			break;
		}

		lcm_size_index = i;
	}

	rc = of_property_read_u32(lcd_node, "cust,width-mm", &val);
	if (!rc)
		info->mode.width_mm = val;
	else
		info->mode.width_mm = lcm_size_arr[lcm_size_index].width_mm;

	rc = of_property_read_u32(lcd_node, "cust,height-mm", &val);
	if (!rc)
		info->mode.height_mm = val;
	else
		info->mode.height_mm = lcm_size_arr[lcm_size_index].height_mm;

	rc = of_property_read_u32(lcd_node, "cust,esd_check_enable", &val);
	if (!rc)
		info->esd_check_enable = val;
	else 
		info->esd_check_enable = 0;

	rc = of_property_read_u32(lcd_node, "cust,cust_esd_check", &val);
	if (!rc)
		info->cust_esd_check = val;
	else
		info->cust_esd_check = 0;

/*
	rc = of_property_read_u32(lcd_node, "cust,esd-check-period", &val);
	if (!rc)
		info->esd_check_period = val;
	else
		info->esd_check_period = 1000;
*/

	rc = of_property_read_u32(lcd_node, "cust,esd-check-register", &val);
	if (!rc)
		info->esd_check_reg = val;
	else
		info->esd_check_reg = 0x0A;

	rc = of_property_read_u32(lcd_node, "cust,esd-check-value", &val);
	if (!rc)
		info->esd_check_val = val;
	else
		info->esd_check_val = 0x9C;

	rc = of_property_read_u32(lcd_node, "cust,pll_clk", &val);
	if (!rc)
		info->pll_clk = val;
	else
		info->pll_clk = 245; //default 800x1280

	rc = of_property_read_u32(lcd_node, "cust,fps", &val);
	if (!rc)
		info->fps = val;
	else
		info->fps = 60; //default 60 fps

	rc = of_property_read_u32(lcd_node, "cust,data_rate", &val);
	if (!rc)
		info->data_rate = val;
	else
		info->data_rate = info->pll_clk * 2;

	rc = of_property_read_u32(lcd_node, "cust,rotate", &val);
	if (!rc)
		info->rotate = val;
	else
		info->rotate = 0;

	rc = of_property_read_u32(lcd_node, "cust,ssc_enable", &val);
	if (!rc)
		info->ssc_enable = val;
	else
		info->ssc_enable = 0; 

	rc = of_property_read_u32(lcd_node, "cust,bpc", &val);
	if (!rc)
		info->bpc = val;
	else
		info->bpc = 8; 

	rc = of_property_read_u32(lcd_node, "cust,vfp_low_power", &val);
	if (!rc)
		info->vfp_low_power = val;
	else
		info->vfp_low_power = 0;

	rc = of_property_read_u32(lcd_node, "cust,avdd_avee_voltage", &val);
	if (!rc)
		info->avdd_avee_voltage = val;
	else
		info->avdd_avee_voltage = 0;


	rc = of_property_read_u32(lcd_node, "cust,dualmipi_ic_support", &val);
	if (!rc)
		info->dualmipi_ic_support = val;
	else
		info->dualmipi_ic_support = 0;

	if (info->dualmipi_ic_support) {
	//dualmipi
		p = of_get_property(lcd_node, "cust,gm8773c_config", &bytes);
		if (p) {
			info->cmds[CMD_DUALMIPI_CONFIG] = p;
			info->cmds_len[CMD_DUALMIPI_CONFIG] = bytes;
		} else
			pr_err("can't find cust,gm8773c_config property\n");

		p = of_get_property(lcd_node, "cust,dualmipi_initcmd", &bytes);
		if (p) {
			info->cmds[CMD_DUALMIPI_INIT] = p;
			info->cmds_len[CMD_DUALMIPI_INIT] = bytes;
		} else
			pr_err("can't find cust,dualmipi_initcmd property\n");

		rc = of_property_read_u32(lcd_node, "cust,dualmipi_cmd_count", &val);
		if (!rc) {
			info->dualmipi_cmd_count = val;
		} else {
			info->dualmipi_cmd_count = 0;
			pr_err("parse dualmipi_cmd_count failed\n");
		}

		rc = of_property_read_u32(lcd_node, "cust,dualmipi_one_cmd_size", &val);
		if (!rc) {
			info->dualmipi_one_cmd_size = val;
		} else {
			info->dualmipi_one_cmd_size = 0;
			pr_err("parse dualmipi_one_cmd_size failed\n");
		}

		convert_dualmipi_initcmd(info);
	}

	if (of_property_read_bool(lcd_node, "cust,use-dcs-write"))
		info->use_dcs = true;
	else
		info->use_dcs = false;

	rc = of_parse_reset_seq(lcd_node, info);
	if (rc)
		pr_err("parse lcd reset sequence failed\n");

	p = of_get_property(lcd_node, "cust,initial-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_INIT] = p;
		info->cmds_len[CMD_CODE_INIT] = bytes;
	} else
		pr_err("can't find cust,initial-command property\n");

	p = of_get_property(lcd_node, "cust,sleep-in-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_IN] = p;
		info->cmds_len[CMD_CODE_SLEEP_IN] = bytes;
	} else
		pr_err("can't find cust,sleep-in-command property\n");

	p = of_get_property(lcd_node, "cust,sleep-out-command", &bytes);
	if (p) {
		info->cmds[CMD_CODE_SLEEP_OUT] = p;
		info->cmds_len[CMD_CODE_SLEEP_OUT] = bytes;
	} else
		pr_err("can't find cust,sleep-out-command property\n");

	rc = of_property_read_u32(lcd_node, "hactive", &val);
	if (!rc)
		info->vm.hactive = val;
	else
		pr_err("can't find hactive property\n");

	rc = of_property_read_u32(lcd_node, "vactive", &val);
	if (!rc)
		info->vm.vactive = val;
	else
		pr_err("can't find vactive property\n");

	rc = of_property_read_u32(lcd_node, "hback-porch", &val);
	if (!rc)
		info->vm.hback_porch = val;
	else
		pr_err("can't find hback-porch property\n");

	rc = of_property_read_u32(lcd_node, "hfront-porch", &val);
	if (!rc)
		info->vm.hfront_porch = val;
	else
		pr_err("can't find hfront-porch property\n");

	rc = of_property_read_u32(lcd_node, "hsync-len", &val);
	if (!rc)
		info->vm.hsync_len = val;
	else
		pr_err("can't find hsync-len property\n");

	rc = of_property_read_u32(lcd_node, "vback-porch", &val);
	if (!rc)
		info->vm.vback_porch = val;
	else
		pr_err("can't find vback-porch property\n");

	rc = of_property_read_u32(lcd_node, "vfront-porch", &val);
	if (!rc)
		info->vm.vfront_porch = val;
	else
		pr_err("can't find vfront-porch property\n");

	rc = of_property_read_u32(lcd_node, "vsync-len", &val);
	if (!rc)
		info->vm.vsync_len = val;
	else
		pr_err("can't find vsync-len property\n");

	info->vm.pixelclock = 153600000;

	rc = of_get_drm_display_mode(lcd_node, &info->mode, 0,
				     OF_USE_NATIVE_MODE);
	if (rc) {
		pr_err("get display timing failed\n");
		return rc;
	}

	if (info->update_from_lk) {
		drm_display_mode_from_videomode(&info->vm, &info->mode);
	}

	info->mode.clock = info->mode.htotal * info->mode.vtotal * info->fps / 1000; //default 60 hz

	of_parse_buildin_modes(info, lcd_node);

	return 0;
}

static int common_panel_parse_dt(struct panel_info *p_info, const char *name)
{
	struct device_node *lcd_node;
	char lcd_path[60];
	int rc;
	int val;


	memset(p_info, 0x00, sizeof(struct panel_info));

	sprintf(lcd_path, "/lcds/%s", "cust_lk_panel");
	lcd_node = of_find_node_by_path(lcd_path);
	if (!lcd_node) {
		pr_err("Leo  could not find %s node\n", "cust_lk_panel");
		p_info->update_from_lk = 0;
	} else {
		rc = of_property_read_u32(lcd_node, "cust,update_from_lk", &val);
		if (!rc)
			p_info->update_from_lk = val;
		else {
			p_info->update_from_lk = 0;
			pr_err("can't find update_from_lk property\n");
		}
	}

	if (!p_info->update_from_lk)  {
		memset(lcd_path, 0x00, sizeof(lcd_path));
		sprintf(lcd_path, "/lcds/%s", name);
		lcd_node = of_find_node_by_path(lcd_path);
		if (!lcd_node) {
			pr_err("Leo  could not find %s node\n", name);
			return -ENODEV;
		}
	}

	rc = common_panel_parse_lcddtb(lcd_node, p_info);
	if (rc) {
		pr_err("could not find %s node\n", name);
		return rc;
	}


	rc = of_property_read_u32(lcd_node, "cust,enable_dts_lcd", &val);
	if (!rc)
		p_info->is_dts_lcm_found = val;
	else {
		p_info->is_dts_lcm_found = 0;
		pr_err("can't find is_dts_lcm_found property\n");
	}

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230829
	if (csci_exist("ro.disable_dts_lcd")){
		p_info->is_dts_lcm_found = false;
	}
#endif

	return 0;
}

//debug part 
static ssize_t common_panel_initcmd_read(struct file *file, char *buf,
								size_t len, loff_t *pos)
{

	int i;
	size_t ret = 0;
	u16 _len;
	int bytes = g_cm->info.debug.cmds_len[CMD_CODE_INIT];
	const struct dsi_cmd_desc *cmds = g_cm->info.debug.cmds[CMD_CODE_INIT];
	int bytes1 = g_cm->info.cmds_len[CMD_CODE_INIT];
	const struct dsi_cmd_desc *cmds1 = g_cm->info.cmds[CMD_CODE_INIT];
	static char *buf_tmp = NULL;
	static bool read_flag = false;
	static int buf_size = 0, read_page = 0;
	static struct file *old_file = NULL;
	
	int cmd_shifts = 0;

	if (old_file == file) {
		read_page++;
		cmd_shifts = read_page * PAGE_SIZE;
	} else {
		read_page = 0;
		cmd_shifts = 0;
	}

	if (!read_flag) {

		if (bytes == 0) {
			buf_size = bytes1 + PAGE_SIZE;

			buf_tmp = kzalloc(buf_size, GFP_KERNEL);
			ret += scnprintf(buf_tmp + ret, buf_size - ret, "debug.cmds maxlen:%d input_cmds_len:%d\n",
								buf_size, g_cm->info.debug.input_cmds_len[CMD_CODE_INIT]);

			if (bytes1 > 0)
				ret += scnprintf(buf_tmp + ret, buf_size - ret, "curr initcmd: \n");
			while (bytes1 > 0) {
				//_len = (cmds->wc_h << 8) | cmds->wc_l;
				_len = cmds1->wc_l;

				ret += scnprintf(buf_tmp + ret, buf_size - ret, "%.2x %.2x ",cmds1->wait,cmds1->wc_l);
				for (i = 0; i < _len; i++) {
					ret += scnprintf(buf_tmp + ret, buf_size - ret, "%.2x ",cmds1->payload[i]);
				}
				ret += scnprintf(buf_tmp + ret, buf_size - ret, "\n");

				cmds1 = (struct dsi_cmd_desc *)(cmds1->payload + _len);
				bytes1 -= (_len + 2);
			}

			ret += scnprintf(buf_tmp + ret, buf_size - ret,
						"usage: \n");
			ret += scnprintf(buf_tmp + ret, buf_size - ret,
						"      1.copy dts lcm init cmd to test.txt and modify the init params.\n");
			ret += scnprintf(buf_tmp + ret, buf_size - ret,
						"      2.adb push test.txt sdcard/ \n");
			ret += scnprintf(buf_tmp + ret, buf_size - ret,
						"      3.adb shell \"cat /sdacrd/test.txt > /proc/cust_panel/initcmd\" \n");
			ret += scnprintf(buf_tmp + ret, buf_size - ret,
						"      4.adb shell \"cat /proc/cust_panel/init_cmd\" . Check if the parameters are correct.\n");
			ret += scnprintf(buf_tmp + ret, buf_size - ret,
						"      5.suspend and resume, let the params take effect.\n");
		} else {
			buf_size = bytes + PAGE_SIZE;
			buf_tmp = kzalloc(buf_size, GFP_KERNEL);

			if (bytes > 0)
				ret += scnprintf(buf_tmp + ret, buf_size - ret, "adb initcmd: \n");
			while (bytes > 0) {
				//_len = (cmds->wc_h << 8) | cmds->wc_l;
				_len = cmds->wc_l;

				ret += scnprintf(buf_tmp + ret, buf_size - ret, "%.2x %.2x ",cmds->wait,cmds->wc_l);
				for (i = 0; i < _len; i++) {
					ret += scnprintf(buf_tmp + ret, buf_size - ret, "%.2x ",cmds->payload[i]);
				}
				ret += scnprintf(buf_tmp + ret, buf_size - ret, "\n");

				cmds = (struct dsi_cmd_desc *)(cmds->payload + _len);
				bytes -= (_len + 2);
			}
		}

		if (copy_to_user(buf, buf_tmp, len)) {
			pr_err("%s, copy_to_user fail: %d\n", __func__, __LINE__);
		}

		buf_size -= PAGE_SIZE;
		read_flag = true;
	} else {
		if (buf_size > 0) {

			len = (buf_size > PAGE_SIZE) ? PAGE_SIZE : buf_size;
			buf_size -= len;
			ret = len;

			if (copy_to_user(buf, buf_tmp + cmd_shifts, len)) {
				pr_err("%s, copy_to_user fail: %d\n", __func__, __LINE__);
			}
		} else {
			read_flag = false;
			kfree(buf_tmp);
		}
	}

	old_file = file;

	return ret;
}

#define _LINEF0     0x0A
#define _LINEF1     0x0D
#define _SPACE      0x20

void filter_hex_char(char *dst, char *str)
{
	int i;
	int len = strlen(str);

	for (i  = 0; i < len; i++)
	{
		if (((*str >= '0' ) && (*str <= '9'))
				|| ((*str >= 'A') && (*str <= 'F'))
				|| ((*str >= 'a') && (*str <= 'f'))) {
			*dst++ = *str++;
		} else {
			str++;
		}
	}
}

static void str_to_int(char *dst, char *src, int len)
{
	char hex[2];
	unsigned int i, char_c;

	if (!dst || !src)
		return;

	for(i = 0; i < len; i+=2) {
		strncpy(hex, src+i, 2);
		sscanf(hex,"%x",&char_c);
		*dst++ = (unsigned char) char_c;
	}
}

static ssize_t common_panel_initcmd_write(struct file *file, const char *buf,
								 size_t len, loff_t *pos)
{
	int bytes, real_len,len_shift;
	int cmds_shift = 0;
	unsigned char *src_buf = NULL;
	unsigned char *dts_buf = NULL;
	unsigned char *buf_cmd = NULL;
	unsigned char *buf_temp = NULL;
	static struct file *old_file = NULL;
	static int wirte_page = 0;
	

	if (old_file == file) { //for size more than one PAGE
		wirte_page++;
		len_shift = PAGE_SIZE*wirte_page;
		cmds_shift = g_cm->info.debug.cmds_len[CMD_CODE_INIT];
		pr_info("Leo write page:%d \n",wirte_page);
	} else {
		wirte_page = 0;
		len_shift = 0;
		cmds_shift = 0;
	}

	real_len = len + len_shift;

	if (g_cm == NULL)
		return -EFAULT;

	src_buf = kzalloc(len * sizeof(char), GFP_KERNEL);
	dts_buf = kzalloc(len * sizeof(char), GFP_KERNEL);
	if (wirte_page != 0)
		buf_temp = kzalloc(cmds_shift * sizeof(char), GFP_KERNEL);

	g_cm->info.debug.input_cmds_len[CMD_CODE_INIT] = real_len;

	if (copy_from_user(src_buf, buf, len)) {
		return -EFAULT;
	}

	filter_hex_char(dts_buf, src_buf);
	bytes = strlen(dts_buf) * sizeof(char) / 2;
	if (strlen(dts_buf) % 2 == 1)
		bytes += 1;

	if (g_cm->info.debug.cmds[CMD_CODE_INIT] != NULL) {
		if (wirte_page != 0)
			memcpy(buf_temp, g_cm->info.debug.cmds[CMD_CODE_INIT], cmds_shift);
		kfree(g_cm->info.debug.cmds[CMD_CODE_INIT]);
		g_cm->info.debug.cmds[CMD_CODE_INIT] = NULL;
	}

	g_cm->info.debug.cmds[CMD_CODE_INIT] = kzalloc(cmds_shift +  bytes, GFP_KERNEL);
	buf_cmd = g_cm->info.debug.cmds[CMD_CODE_INIT];

	if (wirte_page != 0) {
		memcpy(buf_cmd, buf_temp, cmds_shift);
		str_to_int(buf_cmd + cmds_shift, dts_buf, strlen(dts_buf));
	} else {
		str_to_int(buf_cmd, dts_buf, strlen(dts_buf));
	}

	if (wirte_page != 0) {
		g_cm->info.debug.cmds_len[CMD_CODE_INIT] += bytes;
	} else {
		g_cm->info.debug.cmds_len[CMD_CODE_INIT] = bytes;
	}

	old_file=file;

#if 0
	{
		int i;
		pr_info("len:%d buf_cmd:%s \n",len, src_buf);

		printk(" \n\n");
		for (i = 0; i < bytes; i++) {
			printk("0x%.2x \n", *(buf_cmd + cmds_shift + i));
		}
	}
#endif

	kfree(buf_temp);
	kfree(src_buf);
	kfree(dts_buf);
	return len;
}

static ssize_t common_panel_dualmipi_initcmd_read(struct file *file, char *buf,
								size_t len, loff_t *pos)
{
	int i,j,ret=0; 
	const unsigned char *val = g_cm->info.cmds[CMD_DUALMIPI_CONFIG];
	static char *buf_tmp = NULL;
	static bool read_flag = false;
	struct dcs_setting_entry *s_cmd = g_cm->info.s_dcs_cmd;
	int bytes = g_cm->info.cmds_len[CMD_DUALMIPI_INIT];
	int cfg_bytes = g_cm->info.cmds_len[CMD_DUALMIPI_CONFIG];
	static int buf_size = 0, read_page = 0;
	static struct file *old_file = NULL;

	int cmd_shifts = 0;

	if (old_file == file) {
		read_page++;
		cmd_shifts = read_page * PAGE_SIZE;
	} else {
		read_page = 0;
		cmd_shifts = 0;
	}

	if (!read_flag) {
		buf_size = (bytes + cfg_bytes)*5 + PAGE_SIZE;
		buf_tmp = kzalloc(buf_size * sizeof(char), GFP_KERNEL);

		if (cfg_bytes != 0) {
			ret += scnprintf(buf_tmp + ret, buf_size - ret,"gm8773c_config:\n");
			for (i = 0; i < cfg_bytes; i+=2) {
				ret += scnprintf(buf_tmp + ret, buf_size - ret," 0x%.2x,0x%.2x\n",*(val+i), *(val+i+1));
			}
		}

		ret += scnprintf(buf_tmp + ret, buf_size - ret,"dualmipi_init_cmd:\n");
		for (i = 0; i < g_cm->info.dualmipi_cmd_count; i++) {
			ret += scnprintf(buf_tmp + ret, buf_size - ret,"0x%x 0x%.2x 0x%.2x 0x%.2x ",
				s_cmd[i].cmd_name, s_cmd[i].cmd, s_cmd[i].data_id, s_cmd[i].count);

			for (j = 0; j < s_cmd[i].count; j++) {
				ret += scnprintf(buf_tmp + ret, buf_size - ret,"0x%.2x ",s_cmd[i].para_list[j]);
			}
			ret += scnprintf(buf_tmp + ret, buf_size - ret,"\n");
		}

		if (copy_to_user(buf, buf_tmp, len)) {
			pr_err("%s, copy_to_user fail: %d\n", __func__, __LINE__);
		}

		buf_size = ret;
		ret = (ret > PAGE_SIZE) ? PAGE_SIZE : ret;
		buf_size -= PAGE_SIZE;
		read_flag = true;
	} else {
		if (buf_size > 0) {

			len = (buf_size > PAGE_SIZE) ? PAGE_SIZE : buf_size;
			buf_size -= len;
			ret = len;

			if (copy_to_user(buf, buf_tmp + cmd_shifts, len)) {
				pr_err("%s, copy_to_user fail: %d\n", __func__, __LINE__);
			}
		} else {
			read_flag = false;
			kfree(buf_tmp);
		}
	}

	old_file=file;

	return ret;

}

static ssize_t common_panel_dualmipi_initcmd_write(struct file *file, const char *buf,
								 size_t len, loff_t *pos)
{
	return len;
}

static ssize_t common_panel_timing_read(struct file *file, char *buf,
								size_t len, loff_t *pos)
{
	int ret =0;
	char *buf_tmp = NULL;
	static bool read_flag = false;
	struct mtk_panel_ext *ext = g_cm->info.debug.ext;
	struct videomode vm = {0};
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 202300330
	const struct drm_display_mode *csci_mode = 
		g_cm->info.is_dts_lcm_found ? &g_cm->info.mode: cust_get_drm_panel_mode();
#endif

	if (ext == NULL) {
		g_cm->info.debug.ext = find_panel_ext(&g_cm->base);
		ext = g_cm->info.debug.ext;
	}

	if (!read_flag) {
		buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);

		drm_display_mode_to_videomode(csci_mode, &vm);
		ret += scnprintf(buf_tmp + ret, len - ret, "hfp:old:%d set:%d\n",
										vm.hfront_porch, ext->params->dyn.hfp);
		ret += scnprintf(buf_tmp + ret, len - ret, "hbp:old:%d set:%d\n",
										vm.hback_porch, ext->params->dyn.hbp);
		ret += scnprintf(buf_tmp + ret, len - ret, "hsa:old:%d set:%d\n",
										vm.hsync_len, ext->params->dyn.hsa);
		ret += scnprintf(buf_tmp + ret, len - ret, "vfp:old:%d set:%d\n",
										vm.vfront_porch, ext->params->dyn.vfp);
		ret += scnprintf(buf_tmp + ret, len - ret, "vbp:old:%d set:%d\n",
										vm.vback_porch, ext->params->dyn.vbp);
		ret += scnprintf(buf_tmp + ret, len - ret, "vsa:old:%d set:%d\n",
										vm.vsync_len, ext->params->dyn.vsa);
		ret += scnprintf(buf_tmp + ret, len - ret, "switch_en:%d \n",ext->params->dyn.switch_en);
		ret += scnprintf(buf_tmp + ret, len - ret, "mipi_hopping_sta:%d \n",ext->params->dyn.mipi_hopping_sta);
		ret += scnprintf(buf_tmp + ret, len - ret, "pll_clk:%d \n",ext->params->pll_clk);


		ret += scnprintf(buf_tmp + ret, len - ret, "usage: \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo hfp:20 > /proc/cust_panel/timing \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo hbp:20 > /proc/cust_panel/timing \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo hsa:20 > /proc/cust_panel/timing \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo vfp:20 > /proc/cust_panel/timing \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo vbp:20 > /proc/cust_panel/timing \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo vsa:20 > /proc/cust_panel/timing \n");
		//ret += scnprintf(buf_tmp + ret, len - ret, "echo switch_en:1 > /proc/cust_panel/timing \n");
		ret += scnprintf(buf_tmp + ret, len - ret, "echo pll_clk:245 > /proc/cust_panel/timing \n");
		//ret += scnprintf(buf_tmp + ret, len - ret, "echo mipi_ccci:1 > /sys/kernel/debug/mtkfb \n");

		if (copy_to_user(buf, buf_tmp, len)) {
			pr_err("%s, copy_to_user fail: %d\n", __func__, __LINE__);
		}

		read_flag = true;
	} else {
		read_flag = false;
		kfree(buf_tmp);
	}

	return ret;
}

static ssize_t common_panel_timing_write(struct file *file, const char *buf,
								 size_t len, loff_t *pos)
{
	unsigned char *src_buf = NULL;
	struct mtk_panel_ext *ext = g_cm->info.debug.ext;
	unsigned int temp= 0;
	struct dynamic_mipi_params *dyn = &ext->params->dyn;

	src_buf = kzalloc(len * sizeof(char), GFP_KERNEL);
	if (copy_from_user(src_buf, buf, len)) {
		return -EFAULT;
	}

	if (!strncmp(src_buf, "hfp:", 4)) {
		if (sscanf(src_buf, "hfp:%d\n", &temp) == 1)
			ext->params->dyn.hfp = temp;
	} else if (!strncmp(src_buf, "hbp:", 4)) {
		if (sscanf(src_buf, "hbp:%d\n", &temp) == 1)
				ext->params->dyn.hbp = temp;
	} else if (!strncmp(src_buf, "hsa:", 4)) {
		if (sscanf(src_buf, "hsa:%d\n", &temp) == 1)
			ext->params->dyn.hsa = temp;
	} else if (!strncmp(src_buf, "vfp:", 4)) {
		if (sscanf(src_buf, "vfp:%d\n", &temp) == 1)
			ext->params->dyn.vfp = temp;
	} else if (!strncmp(src_buf, "vbp:", 4)) {
		if (sscanf(src_buf, "vbp:%d\n", &temp) == 1)
			ext->params->dyn.vbp = temp;
	} else if (!strncmp(src_buf, "vsa:", 4)) {
		if (sscanf(src_buf, "vsa:%d\n", &temp) == 1)
			ext->params->dyn.vsa = temp;
	//} else if (!strncmp(src_buf, "switch_en:", 10)) {
	//	if (sscanf(src_buf, "switch_en:%d\n", &temp) == 1)
	//		ext->params->dyn.switch_en = temp;
	} else if (!strncmp(src_buf, "pll_clk:", 7)) {
		if (sscanf(src_buf, "pll_clk:%d\n", &temp) == 1) {
			ext->params->pll_clk = temp;
			ext->params->data_rate = temp * 2;
		}
	}

	if (dyn->hfp | dyn->hbp | dyn->hsa 
			| dyn->vfp | dyn->hbp | dyn->vsa) {
		dyn->switch_en = true;
		dyn->mipi_hopping_sta = true;
	} else {
		dyn->switch_en = false;
		dyn->mipi_hopping_sta = false;
	}

	if (src_buf != NULL)
		kfree(src_buf);
	return len;
}

//debug part 
static ssize_t common_panel_dump_read(struct file *file, char *buf,
								size_t len, loff_t *pos)
{

	size_t ret = 0;
	struct panel_info *info = &g_cm->info;
	static char *buf_tmp = NULL;
	static bool read_flag = false;

	buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);

	if (!read_flag) {
		ret += scnprintf(buf_tmp + ret, len - ret, "mode_flags:             0x%x\n",info->mode_flags);
		ret += scnprintf(buf_tmp + ret, len - ret, "lanes:                  %d \n",info->lanes);
		ret += scnprintf(buf_tmp + ret, len - ret, "color-format:           %d \n",info->format);
		ret += scnprintf(buf_tmp + ret, len - ret, "pll_clk:                %d \n",info->pll_clk);
		ret += scnprintf(buf_tmp + ret, len - ret, "data_rate:              %d \n",info->data_rate);
		ret += scnprintf(buf_tmp + ret, len - ret, "dts_fps:                %d \n",info->fps);
		ret += scnprintf(buf_tmp + ret, len - ret, "rotate:                 %d \n",info->rotate);
		ret += scnprintf(buf_tmp + ret, len - ret, "ssc_enable:             %d \n",info->ssc_enable);
		ret += scnprintf(buf_tmp + ret, len - ret, "bpc:                    %d \n",info->bpc);
		ret += scnprintf(buf_tmp + ret, len - ret, "lcm_phy_size:           %d \n",info->lcm_phy_size);
		ret += scnprintf(buf_tmp + ret, len - ret, "width-mm:               %d \n",info->mode.width_mm);
		ret += scnprintf(buf_tmp + ret, len - ret, "height-mm:              %d \n",info->mode.height_mm);
		ret += scnprintf(buf_tmp + ret, len - ret, "vfp_low_power:          %d \n",info->vfp_low_power);
		ret += scnprintf(buf_tmp + ret, len - ret, "esd_check_enable:       %d \n",info->esd_check_enable);
		ret += scnprintf(buf_tmp + ret, len - ret, "cust_esd_check:         %d \n",info->cust_esd_check);
		ret += scnprintf(buf_tmp + ret, len - ret, "esd_check_reg:          0x%x \n",info->esd_check_reg);
		ret += scnprintf(buf_tmp + ret, len - ret, "esd_check_val:          0x%x \n",info->esd_check_val);
		ret += scnprintf(buf_tmp + ret, len - ret, "dualmipi_ic_support:    %d \n",info->dualmipi_ic_support);
		ret += scnprintf(buf_tmp + ret, len - ret, "dualmipi_cmd_count:     %d \n",info->dualmipi_cmd_count);
		ret += scnprintf(buf_tmp + ret, len - ret, "dualmipi_one_cmd_size:  %d \n",info->dualmipi_one_cmd_size);
		ret += scnprintf(buf_tmp + ret, len - ret, "avdd_avee_voltage:      0x%02x \n",info->avdd_avee_voltage);

		if (copy_to_user(buf, buf_tmp, len)) {
			pr_err("%s, copy_to_user fail: %d\n", __func__, __LINE__);
		}

		read_flag = true;
	} else {
		read_flag = false;
	}

	if (buf_tmp != NULL)
		kfree(buf_tmp);
	return ret;
}


static const struct proc_ops common_panel_proc_initcmd_ops = {
	.proc_read = common_panel_initcmd_read,
	.proc_write = common_panel_initcmd_write,
};

static const struct proc_ops common_panel_proc_dualmipi_initcmd_ops = {
	.proc_read = common_panel_dualmipi_initcmd_read,
	.proc_write = common_panel_dualmipi_initcmd_write,
};

static const struct proc_ops common_panel_proc_timing_ops = {
	.proc_read = common_panel_timing_read,
	.proc_write = common_panel_timing_write,
};

static const struct proc_ops common_panel_proc_dump_ops = {
	.proc_read = common_panel_dump_read,
	//.proc_write = common_panel_dump_write,
};

static int common_panel_debug_init(struct common_panel *cm)
{
	struct proc_dir_entry *common_pane_proc_dir;
	static struct proc_dir_entry *cm_debug_file;


	cm->info.debug.ext = find_panel_ext(&cm->base);

	common_pane_proc_dir = proc_mkdir("cust_panel", NULL);
	cm_debug_file = proc_create("initcmd", (S_IWUSR | S_IRUGO),
										common_pane_proc_dir, &common_panel_proc_initcmd_ops);
	if (cm_debug_file == NULL) {
		pr_err(" %s: proc initcmd file create failed!\n", __func__);
		return -ENOMEM;
	}

	cm_debug_file = proc_create("dualmipi_initcmd", (S_IWUSR | S_IRUGO),
										common_pane_proc_dir, &common_panel_proc_dualmipi_initcmd_ops);
	if (cm_debug_file == NULL) {
		pr_err(" %s: proc initcmd file create failed!\n", __func__);
		return -ENOMEM;
	}

	cm_debug_file = proc_create("timing", (S_IWUSR | S_IRUGO),
										common_pane_proc_dir, &common_panel_proc_timing_ops);
	if (cm_debug_file == NULL) {
		pr_err(" %s: proc porch file create failed!\n", __func__);
		return -ENOMEM;
	}

	cm_debug_file = proc_create("dumpinfo", (S_IWUSR | S_IRUGO),
										common_pane_proc_dir, &common_panel_proc_dump_ops);
	if (cm_debug_file == NULL) {
		pr_err(" %s: proc porch file create failed!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

//caozy add for lt9711 20240427
#if IS_ENABLED(CONFIG_WB_LT9711_SUPPROT)
static void lt9711_power_init(){
	cust_gpio_set_value(CUST_GPIO_LCM_3V3, GPIO_OUT_ONE);
    msleep(5);

    cust_gpio_set_value(CUST_GPIO_BIAS_POS, GPIO_OUT_ONE);//edp_3v3
    msleep(10);

    cust_gpio_set_value(CUST_GPIO_BIAS_NEG, GPIO_OUT_ONE);//edp_1v2
    msleep(10);

    cust_gpio_set_value(CUST_GPIO_IT6112_EN, GPIO_OUT_ONE);//edp_1v8
    msleep(50);

	cust_gpio_set_value(CUST_GPIO_LCM_RST, GPIO_OUT_ONE);
	mdelay(lt9711_bl_delay);

    cust_gpio_set_value(CUST_GPIO_LCM_BL_EN, GPIO_OUT_ONE);
}

static int lt9711_disp_notifier_callback(struct notifier_block *nb, unsigned long value, void *v)
{
	int *data = (int *)v;

	if (v) {
		printk("lt9711_disp_notifier_callback success!!!\n");
		if (value == MTK_DISP_EARLY_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_POWERDOWN) {
				printk("lt9711 suspend but do nothing\n");
			}
		} else if (value == MTK_DISP_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_UNBLANK) {
				printk("lt9711 resume\n");
				lt9711_power_init();
			}
		}
	} else {
		printk("lt9711_disp_notifier_callback do nothing!!!\n");
		return -1;
	}

	return 0;
}
#endif
//caozy add for lt9711 20240427

static int common_panel_probe(struct mipi_dsi_device *dsi)
{
	struct common_panel *cm;
	struct device *dev = &dsi->dev;
	int ret;
	const struct panel_desc *desc;
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230302
	int temp = 0;
#endif
	struct drm_display_mode *modes;

	const char *dtbo_lcm_name = "default_drm";
	const char *dts_lcm_name = "dts_lcm_panel";

	cm = devm_kzalloc(&dsi->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

#if 0//IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20220421
	common_lcm = cust_get_drm_lcm(cust_get_lcm_form_csci());
	desc = common_lcm->p_panel_desc;
#else 
	ret = of_property_read_string(dev->of_node, "lcm_name", (const char **)&dtbo_lcm_name);
	if (ret) {
		pr_err("[%s]: of_property_read_string lcm_name failed: %d\n",
				__func__, ret);
	}

	common_panel_parse_dt(&cm->info, dtbo_lcm_name);
	if (cm->info.is_dts_lcm_found == true) {
		ret = of_property_read_string(cm->info.of_node, "use_drm_panel", (const char **)&dts_lcm_name);
		if (ret) {
			pr_err("[%s]: of_property_read_string use_drm_panel failed: %d\n",
					__func__, ret);
		}
	}

	if (cm->info.is_dts_lcm_found == true) {
		common_lcm = cust_get_drm_lcm(dts_lcm_name);
	} else {
		common_lcm = cust_get_drm_lcm(dtbo_lcm_name);
	}

	if (common_lcm->p_panel_desc != NULL) {
		desc = common_lcm->p_panel_desc;
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230330
		modes = (cm->info.is_dts_lcm_found == true) 
					? &cm->info.mode : (struct drm_display_mode *)desc->modes;
		cust_set_lcm_porch_from_csci(modes);
#endif
	} else {
		pr_err("%s: %s p_panel_desc is NULL \n", __func__, dtbo_lcm_name);
	}

	pr_info("[%s]: dtbo_lcm_name: %s \n",__func__, dtbo_lcm_name);
#endif

#if IS_ENABLED(CONFIG_CM_HARDWAREINFO_SUPPORT) //Leo 20210827
	{
		char lcm_name[128] = {0};
		char temp[24] = {0};
		extern void Hwinfo_update_info_cust(int hw_type, char *name);
		#define HW_TYPE_LCM    1
		sprintf(temp,"%s%s",cm->info.is_dts_lcm_found ? ":dts": " ",
								(cm->info.update_from_lk && cm->info.is_dts_lcm_found) ? ":lk": " ");
		sprintf(lcm_name,"%s%s",dtbo_lcm_name, temp);
		Hwinfo_update_info_cust(HW_TYPE_LCM, lcm_name);
	}
#endif

	cm->dev = dev;
	if (common_lcm->p_panel_desc == NULL) {
		pr_info("%s: %s p_panel_desc is NULL, use default desc !\n", __func__, dtbo_lcm_name);
		desc = of_device_get_match_data(&dsi->dev);
	}

	if (cm->info.is_dts_lcm_found == true) {
		dsi->lanes = cm->info.lanes;
		dsi->format = cm->info.format;
		dsi->mode_flags = cm->info.mode_flags;
	} else {
		dsi->lanes = desc->lanes;
		dsi->format = desc->format;
		dsi->mode_flags = desc->mode_flags;
	}

	cm->desc = desc;
	cm->dsi = dsi;
	ret = common_panel_add(cm);
	if (ret < 0)
		return ret;

	mipi_dsi_set_drvdata(dsi, cm);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&cm->base);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&cm->base);
	if ((common_lcm->p_ext_params == NULL) | (common_lcm->p_ext_funcs == NULL)) {
		ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &cm->base);
	} else {
		if (cm->info.is_dts_lcm_found == true) {
			common_lcm->p_ext_params->pll_clk = cm->info.pll_clk;
			common_lcm->p_ext_params->rotate = cm->info.rotate;
			common_lcm->p_ext_params->data_rate = cm->info.data_rate;
			common_lcm->p_ext_params->ssc_enable = cm->info.ssc_enable;
			common_lcm->p_ext_params->vfp_low_power = cm->info.vfp_low_power;
			common_lcm->p_ext_params->cust_esd_check = cm->info.cust_esd_check;
			common_lcm->p_ext_params->esd_check_enable = cm->info.esd_check_enable;
			common_lcm->p_ext_params->lcm_esd_check_table[0].cmd = cm->info.esd_check_reg;
			common_lcm->p_ext_params->lcm_esd_check_table[0].count = 1; //default
			common_lcm->p_ext_params->lcm_esd_check_table[0].para_list[0] = (unsigned char)cm->info.esd_check_val;
		}


#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230302
	if (csci_exist("ro.vendor.lcm.orientation_180") && (cm->info.update_from_lk == 0)) {
		temp = csci_integer("ro.vendor.lcm.orientation_180",0);
		if (temp == 1) {
			common_lcm->p_ext_params->rotate = !common_lcm->p_ext_params->rotate;
		}
	}
	if (csci_exist("ro.vendor.lcm.pll.clk")) { //just for debug
		temp = csci_integer("ro.vendor.lcm.pll.clk",0);
		if (temp > 0) {
			common_lcm->p_ext_params->pll_clk = temp;
			common_lcm->p_ext_params->data_rate = temp*2;
		}
	}
#endif
		ret = mtk_panel_ext_create(dev, common_lcm->p_ext_params, common_lcm->p_ext_funcs, &cm->base);
	}
	if (ret < 0)
		return ret;
#endif

	g_cm = cm;
	common_panel_debug_init(g_cm);

//caozy add for lt9711 20240427
#if IS_ENABLED(CONFIG_WB_LT9711_SUPPROT)
	lt9711_disp_notifier.notifier_call = lt9711_disp_notifier_callback;
	mtk_disp_notifier_register("LT9711_DISP", &lt9711_disp_notifier);
	if (csci_exist("lt9711.bl.delay_time")) {
		lt9711_bl_delay = csci_integer("lt9711.bl.delay_time",0);
	}
#endif
//caozy add for lt9711 20240427

	return ret;
}

static void common_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct common_panel *cm = mipi_dsi_get_drvdata(dsi);

	drm_panel_disable(&cm->base);
	drm_panel_unprepare(&cm->base);
}

static int common_panel_remove(struct mipi_dsi_device *dsi)
{
	struct common_panel *cm = mipi_dsi_get_drvdata(dsi);
	int ret;

	common_panel_shutdown(dsi);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	if (cm->base.dev)
		drm_panel_remove(&cm->base);

	return 0;
}

static const struct of_device_id cm_of_match[] = {
	{ 
	  .compatible = "wb,drm_lcm",
	  .data = &cm_desc
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, cm_of_match);

static struct mipi_dsi_driver common_panel_driver = {
	.driver = {
		.name = "panel_wb_common_lcm",
		.of_match_table = cm_of_match,
	},
	.probe = common_panel_probe,
	.remove = common_panel_remove,
	.shutdown = common_panel_shutdown,
};
module_mipi_dsi_driver(common_panel_driver);

MODULE_AUTHOR("Xinlei Lee <xinlei.lee@mediatek.com>");
MODULE_DESCRIPTION("common 800x1280 video mode panel driver");
MODULE_LICENSE("GPL v2");
