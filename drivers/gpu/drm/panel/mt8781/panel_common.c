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

#include "panel_common.h"
#include "panel_list.h"

#define LCM_COMPILE_ASSERT(condition) \
	LCM_COMPILE_ASSERT_X(condition, __LINE__)
#define LCM_COMPILE_ASSERT_X(condition, line) \
	LCM_COMPILE_ASSERT_XX(condition, line)
#define LCM_COMPILE_ASSERT_XX(condition, line) \
	char assertion_failed_at_line_##line[(condition) ? 1 : -1]

unsigned int lcm_count =
	sizeof(lcm_driver_list) / sizeof(struct cust_drm_lcm *);
LCM_COMPILE_ASSERT(sizeof(lcm_driver_list) / sizeof(struct cust_drm_lcm *) != 0);

extern struct cust_drm_lcm *lcm_driver_list[];
#if IS_ENABLED(CONFIG_WB_IT6151FN) //Shiyh 20240604
extern int _IT6151_driver_init(void);
extern int _IT6151_driver_exit(void);
#endif
static int drm_lcm_index = -1;

static const struct panel_init_cmd default_init_cmd[] = {
	_INIT_DCS_CMD(0x11, 0x00),
	_INIT_DELAY_CMD(120),
	_INIT_DCS_CMD(0x29, 0x00),
	_INIT_DELAY_CMD(20),
	_INIT_DCS_CMD(0x35, 0x00),
	{}
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

static struct mtk_panel_params default_ext_params = {
	.pll_clk = 245,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
};

static struct mtk_panel_funcs default_ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif


static const struct drm_display_mode default_display_mode = {
	
	.clock = 74235,
	.hdisplay = 800,
	.hsync_start = 800 + 80,
	.hsync_end = 800 + 80 + 80,
	.htotal = 800 + 80 + 80 + 20,
	.vdisplay = 1280,
	.vsync_start = 1280 + 20,
	.vsync_end = 1280 + 20 + 12,
	.vtotal = 1280 + 20 + 4 + 12,
};

static const struct panel_desc default_desc = {

	.modes = &default_display_mode,
	.bpc = 8,
	.size = {
		.width_mm = 108,
		.height_mm = 172,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
				  MIPI_DSI_MODE_LPM,
	.init_cmds = default_init_cmd,
	.discharge_on_disable = false,

};


struct cust_drm_lcm default_lcm = {
	.name = "default_drm",
	.p_panel_desc = &default_desc,
	.p_ext_funcs = &default_ext_funcs,
	.p_ext_params = &default_ext_params,
};

#if 0 //Leo 20240308
int cust_add_drm_lcm(struct cust_drm_lcm *cust_lcm)
{
	static int i = 1;

	if (!strcmp(cust_lcm->name, "default_drm")) {
		drm_lcm_list[0].name = cust_lcm->name;
		drm_lcm_list[0].p_panel_desc = cust_lcm->p_panel_desc;
		drm_lcm_list[0].p_ext_funcs = cust_lcm->p_ext_funcs;
		drm_lcm_list[0].p_ext_params = cust_lcm->p_ext_params;
		return 0;
	}

	if (i >= MAX_LCM_SUPPORT) {
		pr_info("%s:cust_lcm: i:%d > MAX_LCM_SUPPORT:%d, Error!",
									__func__, i, MAX_LCM_SUPPORT);
		return -1;
	}

	if (drm_lcm_list[i].name == NULL) {
		drm_lcm_list[i].name = cust_lcm->name;
		drm_lcm_list[i].p_panel_desc = cust_lcm->p_panel_desc;
		drm_lcm_list[i].p_ext_funcs = cust_lcm->p_ext_funcs;
		drm_lcm_list[i].p_ext_params = cust_lcm->p_ext_params;
		drm_lcm_list[i].p_common_funcs = cust_lcm->p_common_funcs;
		i++;
	}
	
	return 0;
}
EXPORT_SYMBOL(cust_add_drm_lcm);
#endif

struct cust_drm_lcm *cust_get_drm_lcm(const char *name) 
{
	int i = 0;
	
	if ((drm_lcm_index >= 0) && (drm_lcm_index < lcm_count)){
		return lcm_driver_list[drm_lcm_index];
	}

	if (name == NULL) {
		drm_lcm_index = 0;
		goto End;
	}

	for (i = 0; i < lcm_count; i++) {
		if (lcm_driver_list[i] == NULL) {
			drm_lcm_index = 0;
			goto End;
		}

		if (!strcmp(lcm_driver_list[i]->name, name)) {
			drm_lcm_index = i;
			break;
		}
	}

End:
	pr_info("Leo drm index:%d name:%s \n",drm_lcm_index, lcm_driver_list[drm_lcm_index]->name);
	return lcm_driver_list[drm_lcm_index];
}
EXPORT_SYMBOL(cust_get_drm_lcm);

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) 
#include <mt-plat/csci.h>
static struct drm_display_mode csci_disp_mode;
static char csci_lcm_name[128];
const char *cust_get_lcm_form_csci(void)
{
	memset(csci_lcm_name, 0x00, sizeof(csci_lcm_name));
	if(csci_exist("lcm.driver.name"))
		csci_string(csci_lcm_name,"lcm.driver.name",0);
	else {
		pr_err("[%s] <lcm.driver.namee> not exist in csci.ini.\n", __func__);
		return NULL;
	}

	return csci_lcm_name;
}
EXPORT_SYMBOL(cust_get_lcm_form_csci);

struct drm_display_mode *cust_get_drm_panel_mode(void) 
{
	return &csci_disp_mode;
}
EXPORT_SYMBOL(cust_get_drm_panel_mode);

void drm_mode_copy_1(struct drm_display_mode *dst,struct drm_display_mode *src)
{
	struct list_head head = dst->head;

	*dst = *src;
	dst->head = head;
}
EXPORT_SYMBOL(drm_mode_copy_1);
#endif

#if 0 //Leo 20240308
static void cust_add_array_lcm(void)
{
	int i;

	pr_info("cust_add_array_lcm. %d \n",lcm_count);

	for (i = 0; i < lcm_count; i++) {
		if (lcm_driver_list[i] != NULL) {
			cust_add_drm_lcm(lcm_driver_list[i]);
		}
	}
}
#endif

static void cust_i2c_driver_register(void) 
{
	pr_info("%s endtry !\n",__func__);

#if IS_ENABLED(CONFIG_WB_IT6112_I2C_SUSPEND_LOW) || IS_ENABLED(CONFIG_WB_TOUCH_I2C_SUSPEND_LOW)//Leo 20240329
	cust_i2c_init();
#endif

#if IS_ENABLED(CONFIG_WB_LT8911EXB) //Leo 20231221
	_LT8911EXB_driver_init();
#endif
#if IS_ENABLED(CONFIG_WB_IT6151FN) //Shiyh 20240604
	_IT6151_driver_init();
#endif
#if IS_ENABLED(CONFIG_WB_BIAS_SUPPORT) //Leo 20240507
	cust_bias_init();
#endif

}

static void cust_i2c_driver_unregister(void) 
{
	pr_info("%s endtry !\n",__func__);

#if IS_ENABLED(CONFIG_WB_IT6112_I2C_SUSPEND_LOW) || IS_ENABLED(CONFIG_WB_TOUCH_I2C_SUSPEND_LOW)
	cust_i2c_exit();
#endif
#if IS_ENABLED(CONFIG_WB_IT6151FN) //Shiyh 20240604
	_IT6151_driver_exit();
#endif
#if IS_ENABLED(CONFIG_WB_LT8911EXB) //Leo 20231221
	_LT8911EXB_driver_exit();
#endif

#if IS_ENABLED(CONFIG_WB_BIAS_SUPPORT) //Leo 20240507
	cust_bias_exit();
#endif
}

static int __init cust_drm_init(void)
{
	pr_info("cust_drm_init.\n");

#if 0 //Leo 20240308
	cust_add_drm_lcm(&default_lcm);
	cust_add_array_lcm();
#endif
	cust_i2c_driver_register();
	return 0;
}

static void __exit cust_drm_exit(void)
{
	pr_info("cust_drm_exit.\n");

	cust_i2c_driver_unregister();
}

module_init(cust_drm_init);
module_exit(cust_drm_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cust Drm Driver");
MODULE_AUTHOR("Leo Zhang<zhangwenlong@weibu.com>");