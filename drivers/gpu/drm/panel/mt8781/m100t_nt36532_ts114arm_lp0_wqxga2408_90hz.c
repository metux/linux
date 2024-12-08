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
	_INIT_DCS_CMD(0x10,0x00),
	_INIT_DELAY_CMD(120),
	{} //it must be here
};
static const struct panel_init_cmd cm_init_cmd[] = {
	_INIT_DCS_CMD(0xFF,0x20),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x05,0x81),
	_INIT_DCS_CMD(0x07,0x4B),
	_INIT_DCS_CMD(0x08,0x23),
	_INIT_DCS_CMD(0x0D,0x43),
	_INIT_DCS_CMD(0x0F,0x96),
	_INIT_DCS_CMD(0x10,0x46),
	_INIT_DCS_CMD(0x1F,0x55,0x55,0x55),
	_INIT_DCS_CMD(0x32,0x72),
	_INIT_DCS_CMD(0x65,0xDD),
	_INIT_DCS_CMD(0x69,0xDD),
	_INIT_DCS_CMD(0x6D,0xDD),
	_INIT_DCS_CMD(0x78,0x93),
	_INIT_DCS_CMD(0xB0,0x00,0x00,0x00,0x23,0x00,0x59,0x00,0x7E,0x00,0x9C,0x00,0xB7,0x00,0xCD,0x00,0xE1),
	_INIT_DCS_CMD(0xB1,0x00,0xF3,0x01,0x2D,0x01,0x59,0x01,0x9B,0x01,0xCC,0x02,0x14,0x02,0x4F,0x02,0x50),
	_INIT_DCS_CMD(0xB2,0x02,0x89,0x02,0xC8,0x02,0xEE,0x03,0x23,0x03,0x44,0x03,0x6F,0x03,0x7C,0x03,0x8A),
	_INIT_DCS_CMD(0xB3,0x03,0x9A,0x03,0xA9,0x03,0xC6,0x03,0xE9,0x03,0xFD,0x03,0xFF,0x00,0x00),
	_INIT_DCS_CMD(0xB4,0x00,0x00,0x00,0x23,0x00,0x59,0x00,0x7E,0x00,0x9C,0x00,0xB7,0x00,0xCD,0x00,0xE1),
	_INIT_DCS_CMD(0xB5,0x00,0xF4,0x01,0x2D,0x01,0x59,0x01,0x9B,0x01,0xCC,0x02,0x14,0x02,0x4F,0x02,0x50),
	_INIT_DCS_CMD(0xB6,0x02,0x89,0x02,0xC8,0x02,0xEE,0x03,0x23,0x03,0x44,0x03,0x6F,0x03,0x7C,0x03,0x8A),
	_INIT_DCS_CMD(0xB7,0x03,0x9A,0x03,0xA9,0x03,0xC6,0x03,0xE9,0x03,0xFD,0x03,0xFF,0x00,0x00),
	_INIT_DCS_CMD(0xB8,0x00,0x00,0x00,0x23,0x00,0x59,0x00,0x7E,0x00,0x9C,0x00,0xB7,0x00,0xCD,0x00,0xE1),
	_INIT_DCS_CMD(0xB9,0x00,0xF3,0x01,0x2D,0x01,0x59,0x01,0x9B,0x01,0xCC,0x02,0x14,0x02,0x4F,0x02,0x50),
	_INIT_DCS_CMD(0xBA,0x02,0x89,0x02,0xC8,0x02,0xEE,0x03,0x23,0x03,0x44,0x03,0x6F,0x03,0x7C,0x03,0x8A),
	_INIT_DCS_CMD(0xBB,0x03,0x9A,0x03,0xA9,0x03,0xC6,0x03,0xE9,0x03,0xFD,0x03,0xFF,0x00,0x00),
	_INIT_DCS_CMD(0xC6,0x32),
	_INIT_DCS_CMD(0xC7,0x20),
	_INIT_DCS_CMD(0xC8,0xF5),
	_INIT_DCS_CMD(0xC9,0x00),
	_INIT_DCS_CMD(0xCA,0x00),
	_INIT_DCS_CMD(0xCB,0x31),
	_INIT_DCS_CMD(0xCC,0x00),
	_INIT_DCS_CMD(0xCD,0x86),
	_INIT_DCS_CMD(0xCE,0x31),
	_INIT_DCS_CMD(0xCF,0x52),
	_INIT_DCS_CMD(0xD0,0x32),
	_INIT_DCS_CMD(0xD1,0x74),
	_INIT_DCS_CMD(0xD2,0x32),
	_INIT_DCS_CMD(0xD3,0x20),
	_INIT_DCS_CMD(0xD4,0xF5),
	_INIT_DCS_CMD(0xD5,0x00),
	_INIT_DCS_CMD(0xD6,0x00),
	_INIT_DCS_CMD(0xD7,0x31),
	_INIT_DCS_CMD(0xD8,0x00),
	_INIT_DCS_CMD(0xD9,0x86),
	_INIT_DCS_CMD(0xDA,0x31),
	_INIT_DCS_CMD(0xDB,0x52),
	_INIT_DCS_CMD(0xDC,0x32),
	_INIT_DCS_CMD(0xDD,0x74),
	_INIT_DCS_CMD(0xDE,0x32),
	_INIT_DCS_CMD(0xDF,0x20),
	_INIT_DCS_CMD(0xE0,0xF5),
	_INIT_DCS_CMD(0xE1,0x00),
	_INIT_DCS_CMD(0xE2,0x00),
	_INIT_DCS_CMD(0xE3,0x31),
	_INIT_DCS_CMD(0xE4,0x00),
	_INIT_DCS_CMD(0xE5,0x86),
	_INIT_DCS_CMD(0xE6,0x31),
	_INIT_DCS_CMD(0xE7,0x52),
	_INIT_DCS_CMD(0xE8,0x32),
	_INIT_DCS_CMD(0xE9,0x74),
	_INIT_DCS_CMD(0xFF,0x21),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0xB0,0x00,0x00,0x00,0x23,0x00,0x58,0x00,0x7F,0x00,0x9D,0x00,0xB8,0x00,0xCE,0x00,0xE2),
	_INIT_DCS_CMD(0xB1,0x00,0xF4,0x01,0x2D,0x01,0x59,0x01,0x9B,0x01,0xCC,0x02,0x16,0x02,0x4F,0x02,0x50),
	_INIT_DCS_CMD(0xB2,0x02,0x89,0x02,0xC8,0x02,0xEE,0x03,0x23,0x03,0x44,0x03,0x6F,0x03,0x7C,0x03,0x8A),
	_INIT_DCS_CMD(0xB3,0x03,0x9A,0x03,0xA9,0x03,0xC6,0x03,0xE9,0x03,0xFD,0x03,0xFF,0x00,0x00),
	_INIT_DCS_CMD(0xB4,0x00,0x00,0x00,0x23,0x00,0x58,0x00,0x7F,0x00,0x9D,0x00,0xB8,0x00,0xCE,0x00,0xE2),
	_INIT_DCS_CMD(0xB5,0x00,0xF4,0x01,0x2D,0x01,0x59,0x01,0x9B,0x01,0xCC,0x02,0x16,0x02,0x4F,0x02,0x50),
	_INIT_DCS_CMD(0xB6,0x02,0x89,0x02,0xC8,0x02,0xEE,0x03,0x23,0x03,0x44,0x03,0x6F,0x03,0x7C,0x03,0x8A),
	_INIT_DCS_CMD(0xB7,0x03,0x9A,0x03,0xA9,0x03,0xC6,0x03,0xE9,0x03,0xFD,0x03,0xFF,0x00,0x00),
	_INIT_DCS_CMD(0xB8,0x00,0x00,0x00,0x23,0x00,0x58,0x00,0x7F,0x00,0x9D,0x00,0xB8,0x00,0xCE,0x00,0xE2),
	_INIT_DCS_CMD(0xB9,0x00,0xF4,0x01,0x2D,0x01,0x59,0x01,0x9B,0x01,0xCC,0x02,0x16,0x02,0x4F,0x02,0x50),
	_INIT_DCS_CMD(0xBA,0x02,0x89,0x02,0xC8,0x02,0xEE,0x03,0x23,0x03,0x44,0x03,0x6F,0x03,0x7C,0x03,0x8A),
	_INIT_DCS_CMD(0xBB,0x03,0x9A,0x03,0xA9,0x03,0xC6,0x03,0xE9,0x03,0xFD,0x03,0xFF,0x00,0x00),
	_INIT_DCS_CMD(0xFF,0x22),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x01,0x45),
	_INIT_DCS_CMD(0xFF,0x23),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x76,0x01,0x02),
	_INIT_DCS_CMD(0x77,0x01,0x02),
	_INIT_DCS_CMD(0x78,0x01,0x02),
	_INIT_DCS_CMD(0x7A,0x63,0x63),
	_INIT_DCS_CMD(0x7B,0xB3,0xB3),
	_INIT_DCS_CMD(0x7C,0x2D,0x32),
	_INIT_DCS_CMD(0x7E,0x10,0x10),
	_INIT_DCS_CMD(0xBA,0x1B,0x00),
	_INIT_DCS_CMD(0xBC,0x04,0x00,0x00),
	_INIT_DCS_CMD(0xFF,0x24),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x00,0x01,0x01,0x01,0x01,0x27,0x0D,0x0C,0x0F,0x0E,0x28,0x28,0x26,0x26,0x3A,0x04,0x05),
	_INIT_DCS_CMD(0x01,0x28,0x3B,0x3B,0x3B,0x08,0x30,0x2E,0x2C),
	_INIT_DCS_CMD(0x02,0x01,0x01,0x01,0x01,0x27,0x0D,0x0C,0x0F,0x0E,0x28,0x28,0x26,0x26,0x3A,0x04,0x05),
	_INIT_DCS_CMD(0x03,0x28,0x3B,0x3B,0x3B,0x08,0x30,0x2E,0x2C),
	_INIT_DCS_CMD(0x17,0x41,0x25,0x63,0xA7,0x8B,0xC9,0x41,0x25,0x63,0xA7,0x8B,0xC9),
	_INIT_DCS_CMD(0x1C,0x80),
	_INIT_DCS_CMD(0x2F,0x02),
	_INIT_DCS_CMD(0x30,0x01),
	_INIT_DCS_CMD(0x31,0x12),
	_INIT_DCS_CMD(0x33,0x06),
	_INIT_DCS_CMD(0x34,0x07),
	_INIT_DCS_CMD(0x35,0x12),
	_INIT_DCS_CMD(0x37,0x22),
	_INIT_DCS_CMD(0x3B,0x76),
	_INIT_DCS_CMD(0x3D,0x02),
	_INIT_DCS_CMD(0x3F,0x0B),
	_INIT_DCS_CMD(0x47,0x24),
	_INIT_DCS_CMD(0x4B,0x51),
	_INIT_DCS_CMD(0x4C,0x01),
	_INIT_DCS_CMD(0x50,0x87),
	_INIT_DCS_CMD(0x51,0x21),
	_INIT_DCS_CMD(0x52,0x43),
	_INIT_DCS_CMD(0x53,0x65),
	_INIT_DCS_CMD(0x54,0x87),
	_INIT_DCS_CMD(0x55,0x0A,0x01),
	_INIT_DCS_CMD(0x56,0x04),
	_INIT_DCS_CMD(0x58,0x10),
	_INIT_DCS_CMD(0x59,0x10),
	_INIT_DCS_CMD(0x5B,0x71),
	_INIT_DCS_CMD(0x5C,0x0F),
	_INIT_DCS_CMD(0x5E,0x00,0x0A),
	_INIT_DCS_CMD(0x60,0x91,0xAE),
	_INIT_DCS_CMD(0x61,0x68),
	_INIT_DCS_CMD(0x7E,0x20),
	_INIT_DCS_CMD(0x7F,0xE1),
	_INIT_DCS_CMD(0x92,0x7D,0x00,0xF5),
	_INIT_DCS_CMD(0x93,0x1E,0x00),
	_INIT_DCS_CMD(0x94,0x0A,0x00),
	_INIT_DCS_CMD(0x95,0x01),
	_INIT_DCS_CMD(0x96,0x88,0xAA,0x80),
	_INIT_DCS_CMD(0x9A,0x0B),
	_INIT_DCS_CMD(0xA5,0x00),
	_INIT_DCS_CMD(0xAA,0xA3,0xA3,0x28),
	_INIT_DCS_CMD(0xAB,0x22),
	_INIT_DCS_CMD(0xC2,0xCC,0x00,0x01),
	_INIT_DCS_CMD(0xD4,0x03),
	_INIT_DCS_CMD(0xD6,0x46),
	_INIT_DCS_CMD(0xD7,0x35),
	_INIT_DCS_CMD(0xD8,0x25),
	_INIT_DCS_CMD(0xDB,0x09),
	_INIT_DCS_CMD(0xDD,0x33),
	_INIT_DCS_CMD(0xDF,0x09),
	_INIT_DCS_CMD(0xE1,0x09),
	_INIT_DCS_CMD(0xE3,0x09),
	_INIT_DCS_CMD(0xE5,0x09),
	_INIT_DCS_CMD(0xE9,0x09),
	_INIT_DCS_CMD(0xEB,0x09),
	_INIT_DCS_CMD(0xEF,0x09),
	_INIT_DCS_CMD(0xF1,0x23),
	_INIT_DCS_CMD(0xF2,0x23),
	_INIT_DCS_CMD(0xF3,0x23),
	_INIT_DCS_CMD(0xF4,0x22),
	_INIT_DCS_CMD(0xF5,0x23),
	_INIT_DCS_CMD(0xF6,0x23),
	_INIT_DCS_CMD(0xF7,0x24),
	_INIT_DCS_CMD(0xFF,0x25),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x05,0x00),
	_INIT_DCS_CMD(0x0F,0x20),
	_INIT_DCS_CMD(0x14,0x8D),
	_INIT_DCS_CMD(0x16,0xD4),
	_INIT_DCS_CMD(0x20,0x71),
	_INIT_DCS_CMD(0x23,0x05),
	_INIT_DCS_CMD(0x24,0x1D),
	_INIT_DCS_CMD(0x27,0x71),
	_INIT_DCS_CMD(0x2A,0x05),
	_INIT_DCS_CMD(0x2B,0x1D),
	_INIT_DCS_CMD(0x34,0x71),
	_INIT_DCS_CMD(0x37,0x05),
	_INIT_DCS_CMD(0x38,0x1D),
	_INIT_DCS_CMD(0x39,0x08),
	_INIT_DCS_CMD(0x3B,0x10),
	_INIT_DCS_CMD(0x3F,0x20),
	_INIT_DCS_CMD(0x40,0x00),
	_INIT_DCS_CMD(0x42,0x04),
	_INIT_DCS_CMD(0x45,0x76),
	_INIT_DCS_CMD(0x47,0x51),
	_INIT_DCS_CMD(0x49,0x71),
	_INIT_DCS_CMD(0x4C,0x05),
	_INIT_DCS_CMD(0x4D,0x1D),
	_INIT_DCS_CMD(0x4E,0x08),
	_INIT_DCS_CMD(0x51,0x71),
	_INIT_DCS_CMD(0x54,0x05),
	_INIT_DCS_CMD(0x55,0x1D),
	_INIT_DCS_CMD(0x56,0x08),
	_INIT_DCS_CMD(0x5B,0x80),
	_INIT_DCS_CMD(0x5E,0x76),
	_INIT_DCS_CMD(0x60,0x51),
	_INIT_DCS_CMD(0x62,0x71),
	_INIT_DCS_CMD(0x65,0x05),
	_INIT_DCS_CMD(0x66,0x1D),
	_INIT_DCS_CMD(0x67,0x08),
	_INIT_DCS_CMD(0x68,0x04),
	_INIT_DCS_CMD(0x6B,0x04),
	_INIT_DCS_CMD(0x6C,0x0E),
	_INIT_DCS_CMD(0x6D,0x0E),
	_INIT_DCS_CMD(0x6E,0x12),
	_INIT_DCS_CMD(0x6F,0x12),
	_INIT_DCS_CMD(0xC1,0x46),
	_INIT_DCS_CMD(0xC5,0x1F),
	_INIT_DCS_CMD(0xC6,0x10),
	_INIT_DCS_CMD(0xDC,0xE9),
	_INIT_DCS_CMD(0xDD,0x04),
	_INIT_DCS_CMD(0xDE,0x6F),
	_INIT_DCS_CMD(0xFF,0x26),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x04,0x50),
	_INIT_DCS_CMD(0x0A,0x04),
	_INIT_DCS_CMD(0x0C,0x09),
	_INIT_DCS_CMD(0x0D,0x00),
	_INIT_DCS_CMD(0x0F,0x05),
	_INIT_DCS_CMD(0x13,0xF1),
	_INIT_DCS_CMD(0x14,0xF0),
	_INIT_DCS_CMD(0x19,0x1D,0x1C,0x1C,0x1C),
	_INIT_DCS_CMD(0x1A,0x12,0xED,0xED,0xED),
	_INIT_DCS_CMD(0x1B,0x1C,0x1C,0x1C,0x1C),
	_INIT_DCS_CMD(0x1C,0x32,0x0D,0x0D,0x0D),
	_INIT_DCS_CMD(0x1D,0x03),
	_INIT_DCS_CMD(0x1E,0x9F),
	_INIT_DCS_CMD(0x1F,0x7D),
	_INIT_DCS_CMD(0x24,0x01),
	_INIT_DCS_CMD(0x25,0x55),
	_INIT_DCS_CMD(0x2A,0x1D,0x1C,0x1C,0x1C),
	_INIT_DCS_CMD(0x2B,0x0A,0xE5,0xE5,0xE5),
	_INIT_DCS_CMD(0x2D,0x00,0x00,0x00,0x0E,0x00,0x00,0x0F,0x04,0x00),
	_INIT_DCS_CMD(0x2F,0x0B),
	_INIT_DCS_CMD(0x30,0x7D),
	_INIT_DCS_CMD(0x32,0x9F),
	_INIT_DCS_CMD(0x33,0x22),
	_INIT_DCS_CMD(0x34,0x92),
	_INIT_DCS_CMD(0x35,0x78),
	_INIT_DCS_CMD(0x36,0x96),
	_INIT_DCS_CMD(0x37,0x78),
	_INIT_DCS_CMD(0x38,0x06),
	_INIT_DCS_CMD(0x39,0x02),
	_INIT_DCS_CMD(0x3A,0x7D),
	_INIT_DCS_CMD(0x3D,0x00,0x00,0xA0,0x00,0x00,0x20),
	_INIT_DCS_CMD(0x3F,0x43),
	_INIT_DCS_CMD(0x40,0xCA),
	_INIT_DCS_CMD(0x41,0x1A),
	_INIT_DCS_CMD(0x42,0xCA),
	_INIT_DCS_CMD(0x43,0x01),
	_INIT_DCS_CMD(0x44,0x72),
	_INIT_DCS_CMD(0x45,0x0B),
	_INIT_DCS_CMD(0x46,0xCA),
	_INIT_DCS_CMD(0x48,0xCA),
	_INIT_DCS_CMD(0x49,0x02),
	_INIT_DCS_CMD(0x4A,0xCA),
	_INIT_DCS_CMD(0x4D,0x02),
	_INIT_DCS_CMD(0x4E,0xB3),
	_INIT_DCS_CMD(0x4F,0x02),
	_INIT_DCS_CMD(0x50,0xB3),
	_INIT_DCS_CMD(0x51,0x02),
	_INIT_DCS_CMD(0x52,0xBD),
	_INIT_DCS_CMD(0x53,0x02),
	_INIT_DCS_CMD(0x54,0x72),
	_INIT_DCS_CMD(0x56,0x02),
	_INIT_DCS_CMD(0x58,0xBD),
	_INIT_DCS_CMD(0x5B,0x02),
	_INIT_DCS_CMD(0x5C,0xBD),
	_INIT_DCS_CMD(0x61,0xBD),
	_INIT_DCS_CMD(0x65,0xBD),
	_INIT_DCS_CMD(0x69,0x02),
	_INIT_DCS_CMD(0x6A,0xBD),
	_INIT_DCS_CMD(0x6E,0x02),
	_INIT_DCS_CMD(0x6F,0xB3),
	_INIT_DCS_CMD(0x70,0x02),
	_INIT_DCS_CMD(0x71,0xB3),
	_INIT_DCS_CMD(0x72,0x02),
	_INIT_DCS_CMD(0x73,0xBD),
	_INIT_DCS_CMD(0x74,0x02),
	_INIT_DCS_CMD(0x75,0x72),
	_INIT_DCS_CMD(0x7B,0x02),
	_INIT_DCS_CMD(0x7C,0xB3),
	_INIT_DCS_CMD(0x7E,0x02),
	_INIT_DCS_CMD(0x7F,0xB3),
	_INIT_DCS_CMD(0x80,0xBB),
	_INIT_DCS_CMD(0x81,0xBB),
	_INIT_DCS_CMD(0x82,0x00,0x64,0x64,0x64),
	_INIT_DCS_CMD(0x84,0x34,0x34,0x34),
	_INIT_DCS_CMD(0x8B,0x36),
	_INIT_DCS_CMD(0x8C,0x09),
	_INIT_DCS_CMD(0x8D,0x00),
	_INIT_DCS_CMD(0x8F,0x05),
	_INIT_DCS_CMD(0x93,0xF1),
	_INIT_DCS_CMD(0x94,0xF0),
	_INIT_DCS_CMD(0x97,0x00,0x00),
	_INIT_DCS_CMD(0x99,0x1D,0x1D,0x1D,0x1D),
	_INIT_DCS_CMD(0x9A,0xE1,0xB8,0xB8,0xB8),
	_INIT_DCS_CMD(0x9B,0x1D,0x1C,0x1C,0x1C),
	_INIT_DCS_CMD(0x9C,0x01,0xD8,0xD8,0xD8),
	_INIT_DCS_CMD(0x9D,0x1D,0x1D,0x1D,0x1D),
	_INIT_DCS_CMD(0x9E,0xD9,0xB0,0xB0,0xB0),
	_INIT_DCS_CMD(0xC9,0x00),
	_INIT_DCS_CMD(0xCD,0x00,0x00),
	_INIT_DCS_CMD(0xCE,0x00,0x00),
	_INIT_DCS_CMD(0xCF,0x01,0x00,0x00),
	_INIT_DCS_CMD(0xD0,0x00,0x00),
	_INIT_DCS_CMD(0xD1,0x00,0x00),
	_INIT_DCS_CMD(0xD2,0x2C),
	_INIT_DCS_CMD(0xFF,0x27),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x13,0x06),
	_INIT_DCS_CMD(0x50,0x00),
	_INIT_DCS_CMD(0x58,0x80),
	_INIT_DCS_CMD(0x59,0xBF),
	_INIT_DCS_CMD(0x5A,0x00),
	_INIT_DCS_CMD(0x5B,0x01),
	_INIT_DCS_CMD(0x5C,0x00),
	_INIT_DCS_CMD(0x5D,0x04),
	_INIT_DCS_CMD(0x5E,0x17),
	_INIT_DCS_CMD(0x5F,0x02),
	_INIT_DCS_CMD(0x60,0x00,0x00),
	_INIT_DCS_CMD(0x61,0x25,0x00),
	_INIT_DCS_CMD(0x62,0xE5,0x00),
	_INIT_DCS_CMD(0x63,0x13,0x00),
	_INIT_DCS_CMD(0x64,0x41,0x00),
	_INIT_DCS_CMD(0x65,0x21,0x00),
	_INIT_DCS_CMD(0x66,0xE5,0x00),
	_INIT_DCS_CMD(0x67,0x13,0x00),
	_INIT_DCS_CMD(0x68,0x41,0x00),
	_INIT_DCS_CMD(0x77,0xAA,0xA0),
	_INIT_DCS_CMD(0x78,0x80),
	_INIT_DCS_CMD(0x79,0xA3),
	_INIT_DCS_CMD(0x7A,0x00),
	_INIT_DCS_CMD(0x7B,0x01),
	_INIT_DCS_CMD(0x7D,0x04),
	_INIT_DCS_CMD(0x7E,0x22),
	_INIT_DCS_CMD(0x7F,0x01),
	_INIT_DCS_CMD(0x80,0x62,0x0B),
	_INIT_DCS_CMD(0x81,0x19,0x00),
	_INIT_DCS_CMD(0x82,0xE5,0x00),
	_INIT_DCS_CMD(0x83,0x0B,0x00),
	_INIT_DCS_CMD(0x84,0x27,0x00),
	_INIT_DCS_CMD(0x85,0x16,0x00),
	_INIT_DCS_CMD(0x86,0xE5,0x00),
	_INIT_DCS_CMD(0x87,0x0B,0x00),
	_INIT_DCS_CMD(0x88,0x27,0x00),
	_INIT_DCS_CMD(0x97,0xAA,0xA0),
	_INIT_DCS_CMD(0xD1,0x54),
	_INIT_DCS_CMD(0xE2,0xFF),
	_INIT_DCS_CMD(0xFF,0x2A),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x00,0x23),
	_INIT_DCS_CMD(0x01,0x0A),
	_INIT_DCS_CMD(0x02,0xAA),
	_INIT_DCS_CMD(0x03,0x0A),
	_INIT_DCS_CMD(0x05,0x0A),
	_INIT_DCS_CMD(0x08,0x0A),
	_INIT_DCS_CMD(0x0A,0x0A),
	_INIT_DCS_CMD(0x0C,0x0A),
	_INIT_DCS_CMD(0x0E,0x23),
	_INIT_DCS_CMD(0x0F,0x23),
	_INIT_DCS_CMD(0x10,0x23),
	_INIT_DCS_CMD(0x11,0x23),
	_INIT_DCS_CMD(0x12,0x23),
	_INIT_DCS_CMD(0x13,0x23),
	_INIT_DCS_CMD(0x14,0x07),
	_INIT_DCS_CMD(0x15,0x07),
	_INIT_DCS_CMD(0x16,0x34),
	_INIT_DCS_CMD(0x17,0x07),
	_INIT_DCS_CMD(0x18,0x34),
	_INIT_DCS_CMD(0x19,0x07),
	_INIT_DCS_CMD(0x1A,0x34),
	_INIT_DCS_CMD(0x1B,0x07),
	_INIT_DCS_CMD(0x1C,0x34),
	_INIT_DCS_CMD(0x1D,0x06),
	_INIT_DCS_CMD(0x1E,0x07),
	_INIT_DCS_CMD(0x1F,0x07),
	_INIT_DCS_CMD(0x25,0x15),
	_INIT_DCS_CMD(0x27,0x5F),
	_INIT_DCS_CMD(0x28,0x78),
	_INIT_DCS_CMD(0x30,0x03),
	_INIT_DCS_CMD(0x32,0xC9),
	_INIT_DCS_CMD(0x33,0xFE),
	_INIT_DCS_CMD(0x35,0x0C),
	_INIT_DCS_CMD(0x4B,0x00,0x0F,0x0F,0x00,0x00,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x64,0x96),
	_INIT_DCS_CMD(0x67,0x9E),
	_INIT_DCS_CMD(0x68,0x00),
	_INIT_DCS_CMD(0x69,0x00),
	_INIT_DCS_CMD(0x6A,0x9E),
	_INIT_DCS_CMD(0x6B,0x00),
	_INIT_DCS_CMD(0x6C,0x00),
	_INIT_DCS_CMD(0x70,0x96),
	_INIT_DCS_CMD(0x7F,0x96),
	_INIT_DCS_CMD(0x82,0x96),
	_INIT_DCS_CMD(0x85,0x96),
	_INIT_DCS_CMD(0x88,0x16),
	_INIT_DCS_CMD(0x89,0x00),
	_INIT_DCS_CMD(0x8A,0x00),
	_INIT_DCS_CMD(0x8B,0x16),
	_INIT_DCS_CMD(0x8C,0x00),
	_INIT_DCS_CMD(0x8D,0x00),
	_INIT_DCS_CMD(0x8E,0x16),
	_INIT_DCS_CMD(0x8F,0x00),
	_INIT_DCS_CMD(0x90,0x00),
	_INIT_DCS_CMD(0x92,0x00),
	_INIT_DCS_CMD(0x93,0x00),
	_INIT_DCS_CMD(0x94,0x06),
	_INIT_DCS_CMD(0x99,0x91),
	_INIT_DCS_CMD(0x9A,0x0A),
	_INIT_DCS_CMD(0xA0,0x9E,0x00,0x00),
	_INIT_DCS_CMD(0xA2,0x3F),
	_INIT_DCS_CMD(0xA4,0xFF),
	_INIT_DCS_CMD(0xA5,0xC0),
	_INIT_DCS_CMD(0xA7,0x30),
	_INIT_DCS_CMD(0xA9,0x21),
	_INIT_DCS_CMD(0xB1,0x00,0x01),
	_INIT_DCS_CMD(0xB3,0x16,0x00,0x00),
	_INIT_DCS_CMD(0xB7,0xFF),
	_INIT_DCS_CMD(0xB8,0x40),
	_INIT_DCS_CMD(0xB9,0x98,0x00,0xAD,0x00,0x5A,0xD6,0x69,0x5F,0x41,0x02,0x00),
	_INIT_DCS_CMD(0xBA,0xAA,0xAA,0xA5,0xA5,0xE1,0xE1,0xFF,0xFF),
	_INIT_DCS_CMD(0xBB,0xAA,0xAA,0xAA,0xAA,0xAA,0x55,0xAA,0x55,0x38,0xC7,0x38,0xC7,0xFF,0xFF,0xFF,0xFF),
	_INIT_DCS_CMD(0xBC,0x66,0x06),
	_INIT_DCS_CMD(0xC4,0x02),
	_INIT_DCS_CMD(0xC5,0x05),
	_INIT_DCS_CMD(0xC6,0x1D),
	_INIT_DCS_CMD(0xC8,0x06),
	_INIT_DCS_CMD(0xCA,0x01),
	_INIT_DCS_CMD(0xCB,0x11,0x00,0x00,0x00),
	_INIT_DCS_CMD(0xCC,0xF5,0xAA,0x5F,0xF5,0xAA,0x5F),
	_INIT_DCS_CMD(0xD0,0x04),
	_INIT_DCS_CMD(0xD1,0x01),
	_INIT_DCS_CMD(0xDF,0x01),
	_INIT_DCS_CMD(0xE2,0x01),
	_INIT_DCS_CMD(0xF4,0x91),
	_INIT_DCS_CMD(0xF5,0x0A),
	_INIT_DCS_CMD(0xFF,0x20),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x44,0x83),
	_INIT_DCS_CMD(0x45,0x83),
	_INIT_DCS_CMD(0x46,0x83),
	_INIT_DCS_CMD(0xFF,0x10),
	_INIT_DCS_CMD(0xFB,0x01),
	_INIT_DCS_CMD(0x3B,0x03,0x0A,0x1E,0x0A,0x0A,0x00),
	_INIT_DCS_CMD(0x90,0x03),
	_INIT_DCS_CMD(0x91,0x89,0xA8,0x00,0x08,0xD2,0x00,0x00,0x00,0x00,0xD8,0x00,0x0B,0x0E,0xDC,0x07,0xEB),
	_INIT_DCS_CMD(0x92,0x10,0xE0),
	_INIT_DCS_CMD(0x9D,0x01),
	_INIT_DCS_CMD(0xB3,0x00),//90hz
	//_INIT_DCS_CMD(0xB3,0x40),//60hz

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
//#define LCM_120
//#define LCM_114

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

#ifdef LCM_120
#define WIDTH_MM    156
#define HEIGHT_MM   260 
#endif

#ifdef LCM_114
#define LCM_WIDTH    156	//wait debug
#define LCM_HEIGHT   260 	//
#endif

#ifndef WIDTH_MM
#define WIDTH_MM   137
#endif

#ifndef HEIGHT_MM
#define HEIGHT_MM  217
#endif 


#define LCM_WIDTH		1720
#define LCM_HEIGHT		2408

#define VSA				2
#define VBP				8
#define VFP				1290
#define VFP_90			30

#define HSA				4
#define HBP				30
#define HFP				30

#define FPS				60
#define FPS_90			90

#define PLL_CLK			472

//htotal * vtotal * vrefresh / 1000
#define HTOTAL		(LCM_WIDTH + HFP + HBP + HSA)
#define VTOTAL		(LCM_HEIGHT + VFP + VBP + VSA)
#define PIXEL		(HTOTAL * VTOTAL * FPS ) / 1000
#define VTOTAL_90		(LCM_HEIGHT + VFP_90 + VBP + VSA)//2256
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
	//.data_rate = PLL_CLK*2,
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
		.pic_height = 2408,
		.pic_width = 1720,
		.slice_height = 8,
		.slice_width = 860,
		.chunk_size = 860,
		.xmit_delay = 512,
		.dec_delay = 723,
		.scale_value = 32,
		.increment_interval = 216,
		.decrement_interval = 11,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 3804,
		.slice_bpg_offset = 2027,
		.initial_offset = 6144,
		.final_offset = 4320,
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
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,

		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1720,
		.slice_height = 8,
		.slice_width = 860,
		.chunk_size = 860,
		.xmit_delay = 512,
		.dec_delay = 723,
		.scale_value = 32,
		.increment_interval = 216,
		.decrement_interval = 11,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 3804,
		.slice_bpg_offset = 2027,
		.initial_offset = 6144,
		.final_offset = 4320,
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

	cust_gpio_set_value(CUST_GPIO_BIAS_EN, GPIO_OUT_ZERO);
	mdelay(10);
#if IS_ENABLED(CONFIG_WB_TP_INCELL_SUPPORT) //Leo 20230131
    cust_gpio_set_value(CUST_GPIO_TP_RST, GPIO_OUT_ZERO);
    mdelay(5);
#endif
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


struct cust_drm_lcm m100t_nt36532_ts114arm_lp0_wqxga2408_90hz = {
	.name = "m100t_nt36532_ts114arm_lp0_wqxga2408_90hz",
	.p_panel_desc = &common_desc,
	.p_ext_funcs = &ext_funcs,
	.p_ext_params = &ext_params,
	.p_common_funcs = &common_panel_funcs,
};
