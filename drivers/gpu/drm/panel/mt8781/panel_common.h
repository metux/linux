#ifndef __PANEL_COMMON_
#define __PANEL_COMMON_

#include <linux/fb.h>
#include <video/videomode.h>
#include "dualmipi.h"

#if IS_ENABLED(CONFIG_WB_LT8911EXB) //Leo 20231221
#include "lcm_lt8911exb_i2c.h"
#endif

#if IS_ENABLED(CONFIG_WB_IT6112_I2C_SUSPEND_LOW) || IS_ENABLED(CONFIG_WB_TOUCH_I2C_SUSPEND_LOW)//Leo 20240329
#include "cust_i2c.h"
#endif

#if IS_ENABLED(CONFIG_WB_BIAS_SUPPORT) //Leo 20240507
#include "cust_i2c_bias.h"
#endif

#define GPIO_OUT_ONE		1
#define GPIO_OUT_ZERO		0

#define REGFLAG_DELAY				0xFEE
#define REGFLAG_END_OF_TABLE		0x00

#define _INIT_DCS_CMD(...) { \
	.type = INIT_DCS_CMD, \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

#define _INIT_DELAY_CMD(...) { \
	.type = DELAY_CMD,\
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

#ifndef DTS_SUPPORT
#define DTS_SUPPORT
#endif

#ifdef DTS_SUPPORT
enum {
	CMD_CODE_INIT = 0,
	CMD_CODE_SLEEP_IN,
	CMD_CODE_SLEEP_OUT,
	CMD_OLED_BRIGHTNESS,
	CMD_OLED_REG_LOCK,
	CMD_OLED_REG_UNLOCK,
	CMD_DUALMIPI_CONFIG,
	CMD_DUALMIPI_INIT,
	CMD_CODE_RESERVED2,
	CMD_CODE_RESERVED3,
	CMD_CODE_RESERVED4,
	CMD_CODE_RESERVED5,
	CMD_CODE_MAX,
};

enum {
	SPRD_DSI_MODE_CMD = 0,
	SPRD_DSI_MODE_VIDEO_BURST,
	SPRD_DSI_MODE_VIDEO_SYNC_PULSE,
	SPRD_DSI_MODE_VIDEO_SYNC_EVENT,
};

enum {
	ESD_MODE_REG_CHECK,
	ESD_MODE_TE_CHECK,
};

struct dsi_cmd_desc {
	/*u8 data_type;*/
	u8 wait;
	//u8 wc_h;
	u8 wc_l;
	u8 payload[];
};

struct gpio_timing {
	u32 gpio;
	u32 level;
	u32 delay;
};

struct reset_sequence {
	u32 items;
	struct gpio_timing *timing;
};
#endif

enum {
	DUALMIPI_NONE = 0,
	DUALMIPI_IT6112,
	DUALMIPI_IT6113,
	DUALMIPI_GM8733C
};

struct panel_desc {
	const struct drm_display_mode *modes;
	const struct drm_display_mode *modes_90;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
	bool discharge_on_disable;
};

struct lcm_size {
	int size;
	int width_mm;
	int height_mm;
};

struct panel_debug {
	struct reset_sequence power_on_seq;
	struct reset_sequence power_off_seq;
	void *cmds[CMD_CODE_MAX];
	int cmds_len[CMD_CODE_MAX];
	int input_cmds_len[CMD_CODE_MAX];
	struct mtk_panel_ext *ext;
};

struct panel_info {
	/* common parameters */
	struct device_node *of_node;
	struct drm_display_mode mode;
	struct drm_display_mode *buildin_modes;
	int num_buildin_modes;
	struct reset_sequence power_on_seq;
	struct reset_sequence power_on_seq_after_cmd;
	struct reset_sequence power_off_seq;
	struct reset_sequence power_off_seq_before_cmd;
	const void *cmds[CMD_CODE_MAX];
	int cmds_len[CMD_CODE_MAX];
	struct dcs_setting_entry *s_dcs_cmd;
	struct panel_debug debug;

	/* esd check parameters*/
	bool esd_check_enable;
	u8 cust_esd_check;
	u16 esd_check_period;
	u32 esd_check_reg;
	u8 esd_check_val;

	/* MIPI DSI specific parameters */
	u32 format;
	u32 lanes;
	u32 mode_flags;
	u32 pll_clk;
	u32 fps;
	u32 ssc_enable;
	u32 data_rate;
	u32 rotate;
	u32 lcm_phy_size;
	u32 bpc;
	u32 vfp_low_power;
	u32 avdd_avee_voltage;
	struct videomode vm;
	u32 update_from_lk;
	u32 dualmipi_ic_support;
	u32 dualmipi_cmd_count;
	u32 dualmipi_one_cmd_size;
	bool is_dts_lcm_found;
	bool use_dcs;
};

struct common_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct device *dev;
	const struct panel_desc *desc;
	struct backlight_device *backlight;
	bool enabled;

	enum drm_panel_orientation orientation;

	bool prepared_power;
	bool prepared;

#ifdef DTS_SUPPORT //Leo 20230825
	struct panel_info info;
#endif
};

enum dsi_cmd_type {
	INIT_DCS_CMD,
	DELAY_CMD,
};

struct panel_init_cmd {
	enum dsi_cmd_type type;
	size_t len;
	const char *data;
};

struct cust_drm_lcm {
	const char *name;
	const struct panel_desc *p_panel_desc;
	struct mtk_panel_funcs *p_ext_funcs;
	struct mtk_panel_params *p_ext_params;
	const struct drm_panel_funcs *p_common_funcs;
};

extern struct cust_drm_lcm *cust_get_drm_lcm(const char *name);
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) 
extern struct drm_display_mode *cust_get_drm_panel_mode(void) ;
extern void drm_mode_copy_1(struct drm_display_mode *dst,struct drm_display_mode *src);
extern const char *cust_get_lcm_form_csci(void);
#endif
#endif