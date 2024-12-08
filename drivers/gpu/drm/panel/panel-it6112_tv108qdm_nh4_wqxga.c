// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

//static char bl_tb0[] = { 0x51, 0xff };
extern void it6112_init(void);
extern void it6112_mipi_power_off(void);
//TO DO: You have to do that remove macro BYPASSI2C and solve build error
//otherwise voltage will be unstable
#define BYPASSI2C

#ifndef BYPASSI2C
/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
		.compatible = "mediatek,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	pr_debug("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,
		client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

//static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
//{
//	int ret = 0;
//	struct i2c_client *client = _lcm_i2c_client;
//	char write_data[2] = { 0 };
//
//	if (client == NULL) {
//		pr_debug("ERROR!! _lcm_i2c_client is null\n");
//		return 0;
//	}
//
//	write_data[0] = addr;
//	write_data[1] = value;
//	ret = i2c_master_send(client, write_data, 2);
//	if (ret < 0)
//		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");
//
//	return ret;
//}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/***********************************/
#endif

struct jdi {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
    struct gpio_desc *pwr_1v8_gpio;
    struct gpio_desc *pwr_3v3_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	struct gpio_desc *it6112_gpio;
	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int error;
};

#define jdi_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define jdi_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct jdi *panel_to_jdi(struct drm_panel *panel)
{
	return container_of(panel, struct jdi, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int jdi_dcs_read(struct jdi *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void jdi_panel_get_data(struct jdi *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = jdi_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

//static void jdi_dcs_write(struct jdi *ctx, const void *data, size_t len)
//{
//	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
//	ssize_t ret;
//	char *addr;
//
//	if (ctx->error < 0)
//		return;
//
//	addr = (char *)data;
//	if ((int)*addr < 0xB0)
//		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
//	else
//		ret = mipi_dsi_generic_write(dsi, data, len);
//	if (ret < 0) {
//		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
//		ctx->error = ret;
//	}
//}
//
//static void jdi_panel_init(struct jdi *ctx)
//{
//	//jdi_dcs_write_seq_static(ctx, 0x11);
//	//jdi_dcs_write_seq_static(ctx, 0x29);
//    //
//	//jdi_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1]);
//
//	pr_info("%s-\n", __func__);
//}

static int jdi_disable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int jdi_unprepare(struct drm_panel *panel)
{

	struct jdi *ctx = panel_to_jdi(panel);

	if (!ctx->prepared)
		return 0;

    //gpiod_set_value(ctx->it6112_gpio, 0);
    it6112_mipi_power_off();
    msleep(100);
    gpiod_set_value(ctx->reset_gpio, 0);
    msleep(2);
	gpiod_set_value(ctx->bias_neg, 0);
	msleep(2);
	gpiod_set_value(ctx->bias_pos, 0);
    msleep(2);
    gpiod_set_value(ctx->pwr_1v8_gpio, 0);
    
	ctx->prepared = false;
    return 0;
}

static int jdi_prepare(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);
	int ret = 0;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;
    
    gpiod_set_value(ctx->pwr_1v8_gpio, 1);
    msleep(20);
	gpiod_set_value(ctx->bias_pos, 1);
    msleep(10);
	gpiod_set_value(ctx->bias_neg, 1);
    msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
    msleep(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(25);
	gpiod_set_value(ctx->reset_gpio, 1);
    msleep(25);
    gpiod_set_value(ctx->it6112_gpio, 1);
    msleep(5);
	it6112_init();

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	jdi_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int jdi_enable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 271268,
	.hdisplay = 1600,
	.hsync_start = 1600 + 80,//HFP
	.hsync_end = 1600 + 80 + 24,//HSA
	.htotal = 1600 + 80 + 24 + 80,//HBP
	.vdisplay = 2560,
	.vsync_start = 2560 + 16,//VFP
	.vsync_end = 2560 + 16 + 4,//VSA
	.vtotal = 2560 + 16 + 4 + 16,//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 800,
	//.vfp_low_power = 840,
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 1600,
    .rotate = 1,
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

//static int jdi_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
//				 unsigned int level)
//{
//
//	if (level > 255)
//		level = 255;
//	pr_info("%s backlight = -%d\n", __func__, level);
//	bl_tb0[1] = (u8)level;
//
//	if (!cb)
//		return -1;
//
//	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
//	return 0;
//}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct jdi *ctx = panel_to_jdi(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	//.set_backlight_cmdq = jdi_setbacklight_cmdq,
	.ata_check = panel_ata_check,
};
#endif

static int jdi_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs jdi_drm_funcs = {
	.disable = jdi_disable,
	.unprepare = jdi_unprepare,
	.prepare = jdi_prepare,
	.enable = jdi_enable,
	.get_modes = jdi_get_modes,
};

static int jdi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct jdi *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct jdi), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
    
	ctx->pwr_1v8_gpio = devm_gpiod_get(dev, "pwr-1v8", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pwr_1v8_gpio)) {
		dev_info(dev, "cannot get pwr_1v8_gpio %ld\n",
			 PTR_ERR(ctx->pwr_1v8_gpio));
		//return PTR_ERR(ctx->pwr_1v8_gpio);
	}
	devm_gpiod_put(dev, ctx->pwr_1v8_gpio);

	ctx->pwr_3v3_gpio = devm_gpiod_get(dev, "pwr-3v3", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pwr_3v3_gpio)) {
		dev_info(dev, "xxx cannot get pwr_3v3_gpio %ld\n",
			 PTR_ERR(ctx->pwr_3v3_gpio));
		//return PTR_ERR(ctx->pwr_3v3_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
    
	if (ctx->gate_ic == 0) {
		ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_info(dev, "cannot get bias-gpios 0 %ld\n",
				 PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		devm_gpiod_put(dev, ctx->bias_pos);

		ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_info(dev, "cannot get bias-gpios 1 %ld\n",
				 PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		devm_gpiod_put(dev, ctx->bias_neg);
	}
    
    ctx->it6112_gpio = devm_gpiod_get(dev, "it6112_gpio", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->it6112_gpio)) {
		dev_info(dev, "xxx cannot get it6112_gpio %ld\n",
			 PTR_ERR(ctx->it6112_gpio));
		//return PTR_ERR(ctx->it6112_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
    
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &jdi_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- jdi,nt36672e,vdo,60hz\n", __func__);

	return ret;
}

static int jdi_remove(struct mipi_dsi_device *dsi)
{
	struct jdi *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id jdi_of_match[] = {
	{
	    .compatible = "it6112_tv108qdm_nh4_wqxga",
	},
	{}
};

MODULE_DEVICE_TABLE(of, jdi_of_match);

static struct mipi_dsi_driver jdi_driver = {
	.probe = jdi_probe,
	.remove = jdi_remove,
	.driver = {
		.name = "panel-it6112_tv108qdm_nh4_wqxga",
		.owner = THIS_MODULE,
		.of_match_table = jdi_of_match,
	},
};

module_mipi_dsi_driver(jdi_driver);

MODULE_AUTHOR("shaohua deng <shaohua.deng@mediatek.com>");
MODULE_DESCRIPTION("BOE TV108QDM_NH4PANEL Driver");
MODULE_LICENSE("GPL v2");
