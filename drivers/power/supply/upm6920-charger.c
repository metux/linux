// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Unisemipower. All rights reserved.
 */

#define pr_fmt(fmt)	"[upm6920]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/err.h>
#include <linux/pm_wakeup.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>

#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

#include <linux/extcon.h>
#include <linux/phy/phy.h>

//#include <mt-plat/v1/charger_type.h>
//#include <mt-plat/v1/charger_class.h>
#include "charger_class.h"
#include "mtk_charger.h"

#include "upm6920_reg.h"

#define UPM6920_ERR   (1 << 0)
#define UPM6920_INFO  (1 << 1)
#define UPM6920_DEBUG (1 << 2)

#define UPM6920_MIVR_MAX                       13400000

#define CUSTOM_BC12_ENABLE
//#define CUSTOM_BC12_EFFECT

#ifdef CUSTOM_BC12_ENABLE
#ifndef PHY_MODE_BC11_SET
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
#endif

enum cust_usbsw {
    USBSW_CHG = 0,
    USBSW_USB,
};

enum cust_attach_type {
    ATTACH_TYPE_NONE,
    ATTACH_TYPE_PWR_RDY,
    ATTACH_TYPE_TYPEC,
    ATTACH_TYPE_PD,
    ATTACH_TYPE_PD_SDP,
    ATTACH_TYPE_PD_DCP,
    ATTACH_TYPE_PD_NONSTD,
};
#endif

static int upm6920_chg_dbg_enable = UPM6920_ERR|UPM6920_INFO|UPM6920_DEBUG;
module_param(upm6920_chg_dbg_enable, int, 0644);
MODULE_PARM_DESC(upm6920_chg_dbg_enable, "debug charger upm6920");

#define chg_debug(fmt, ...) \
	if (upm6920_chg_dbg_enable & UPM6920_DEBUG ) { \
		printk(KERN_ERR "[upm6920]:%s:"fmt, __func__, ##__VA_ARGS__); \
	} else { \
		printk(KERN_DEBUG "[upm6920]:%s:"fmt, __func__, ##__VA_ARGS__); \
	}

#define chg_info(fmt, ...) \
	if (upm6920_chg_dbg_enable & UPM6920_INFO) { \
            printk(KERN_ERR "[upm6920]:%s:"fmt, __func__, ##__VA_ARGS__); \
    } else { \
            printk(KERN_DEBUG "[upm6920]:%s:"fmt, __func__, ##__VA_ARGS__); \
    }

#define chg_err(fmt, ...) \
	if (upm6920_chg_dbg_enable & UPM6920_ERR) { \
		printk(KERN_ERR "[upm6920]:%s:"fmt, __func__, ##__VA_ARGS__); \
	} else { \
		printk(KERN_DEBUG "[upm6920]:%s:"fmt, __func__, ##__VA_ARGS__); \
	}

enum upm6920_part_no {
	UPM6920_PN = 0x03,
    SC8989X_PN = 0x04,
};

//#ifndef CONFIG_MTK_CHARGER
enum charger_type {
	CHARGER_UNKNOWN = 0,
	STANDARD_HOST,      /* USB : 450mA */
	CHARGING_HOST,
	NONSTANDARD_CHARGER,    /* AC : 450mA~1A */
	STANDARD_CHARGER,   /* AC : ~1A */
	APPLE_2_1A_CHARGER, /* 2.1A apple charger */
	APPLE_1_0A_CHARGER, /* 1A apple charger */
	APPLE_0_5A_CHARGER, /* 0.5A apple charger */
	WIRELESS_CHARGER,
};
//#endif

struct chg_para{
	int vlim;
	int ilim;
	int vreg;
	int ichg;
};

struct upm6920_platform_data {
	int iprechg;
	int iterm;
	int boostv;
	int boosti;
	int cv;
	struct chg_para usb;
	int ircomp_r;
	int ircomp_v;
};

struct upm6920 {
	struct device *dev;
	struct i2c_client *client;
#ifdef CONFIG_TCPC_CLASS
	/*type_c_port0*/
	struct tcpc_device *tcpc;
#endif
	int part_no;
	int revision;

	bool pp_en;//jnier add 20240606
	u32 mivr;

	const char *chg_dev_name;
	const char *eint_name;

	int status;
	int irq;
	int irq_gpio;

	atomic_t charger_suspended;
	bool otg_enable;

	int chg_type;
	enum power_supply_type psy_chg_type;
    enum power_supply_usb_type psy_usb_type;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool vbus_attach;

	int drvbus_gpio;

	struct upm6920_platform_data *platform_data;

	struct charger_device *chg_dev;
    struct power_supply_desc psy_desc;
    struct power_supply *psy;
	struct power_supply *bat_psy;

	struct delayed_work psy_dwork;
	struct delayed_work probe_dwork;
	struct delayed_work hidd_dwork;

	struct task_struct *hvdcp_detect_task;
	wait_queue_head_t hvdcp_wqueue;	
	atomic_t hvdcp_detect_trig;
	struct wakeup_source *hvdcp_detect_ws;
	bool chg_config;

	bool sc8989x_charger;

#ifdef CUSTOM_BC12_ENABLE
	atomic_t attach;
    struct delayed_work force_detect_dwork;
    int force_detect_count;
#endif
};

static int re_try = 5;

//#ifdef CONFIG_MTK_CHARGER
//extern void Charger_Detect_Init(void);
//extern void Charger_Detect_Release(void);
//#endif

#if IS_ENABLED(CONFIG_WB_DOCKING_SUPPORT) //Leo 20221117
#include <linux/of_platform.h>
#include "extcon-mtk-usb.h"
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/extcon.h>
#include <linux/of_platform.h>

static struct mtk_extcon_info *g_extcon = NULL;

static bool get_is_docking(void)
{
	if (g_extcon != NULL) {
		return !gpio_get_value(g_extcon->docking_det_gpio);
	}
	
	return false;
}

static bool get_mtk_extcon_info(void)
{
	struct device_node *pnode = NULL;
	struct platform_device *pdev = NULL;
	struct mtk_extcon_info *extcon = NULL;

	if (g_extcon != NULL) {
		return 0;
	}

	pnode = of_find_compatible_node(NULL, NULL, "mediatek,extcon-usb");
	if (!pnode) {
		pr_err("%s :failed to get pnode\n",__func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(pnode);
	if (WARN_ON(!pdev)) {
		of_node_put(pnode);
		pr_err("%s :failed to get pdev\n",__func__);
		return -ENODEV;
	};

	extcon = (struct mtk_extcon_info *)platform_get_drvdata(pdev);

	if (extcon == NULL) {
		pr_err("%s :failed to get extcon\n",__func__);
		return -ENODEV;
	}

	g_extcon = extcon;
	
	return 0;
}
#endif

#ifdef CUSTOM_BC12_ENABLE
static int cust_chg_set_usbsw(struct upm6920 *ddata,
                enum cust_usbsw usbsw)
{
    struct phy *phy;
    int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
                           PHY_MODE_BC11_CLR;

    dev_info(ddata->dev, "usbsw=%d\n", usbsw);
    
    phy = phy_get(ddata->dev, "usb2-phy");
    if (IS_ERR_OR_NULL(phy)) {
        dev_err(ddata->dev, "failed to get usb2-phy\n");
        return -ENODEV;
    }
    ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
    if (ret)
        dev_err(ddata->dev, "failed to set phy ext mode\n");
    phy_put(ddata->dev, phy);
    return ret;
}

static bool is_usb_rdy(struct device *dev)
{
    bool ready = true;
    struct device_node *node;

    node = of_parse_phandle(dev->of_node, "usb", 0);
    if (node) {
        ready = !of_property_read_bool(node, "cdp-block");
        pr_info("usb ready = %d\n", ready);
    } else
        dev_warn(dev, "usb node missing or invalid\n");
    return ready;
}

static int cust_chg_enable_bc12(struct upm6920 *ddata, bool en)
{
    int i, ret = 0, attach;
    static const int max_wait_cnt = 250;

    dev_info(ddata->dev, "en=%d\n", en);
    if (en) {
        /* CDP port specific process */
        dev_info(ddata->dev, "check CDP block\n");
        for (i = 0; i < max_wait_cnt; i++) {
            if (is_usb_rdy(ddata->dev))
                break;

            attach = atomic_read(&ddata->attach);
            if (attach == ATTACH_TYPE_TYPEC)
                msleep(100);
            else {
                dev_info(ddata->dev, "%s: change attach:%d, disable bc12\n",
                       __func__, attach);
                en = false;
                break;
            }
        }
        if (i == max_wait_cnt)
            dev_notice(ddata->dev, "CDP timeout\n", __func__);
        else
            dev_info(ddata->dev, "CDP free\n", __func__);
    }

#if IS_ENABLED(CONFIG_WB_DOCKING_SUPPORT) //Leo 20230110
    get_mtk_extcon_info();
    if (get_is_docking() == false) {
        ret = cust_chg_set_usbsw(ddata, en ? USBSW_CHG : USBSW_USB);
    }
#else
    ret = cust_chg_set_usbsw(ddata, en ? USBSW_CHG : USBSW_USB);
#endif

    return ret;
}
#endif

static int __upm6920_read_reg(struct upm6920 *upm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(upm->client, reg);
	if (ret < 0) {
		chg_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __upm6920_write_reg(struct upm6920 *upm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(upm->client, reg, val);
	if (ret < 0) {
		chg_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int upm6920_read_byte(struct upm6920 *upm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6920_read_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6920_write_byte(struct upm6920 *upm, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6920_write_reg(upm, reg, data);
	mutex_unlock(&upm->i2c_rw_lock);

	return ret;
}

static int upm6920_update_bits(struct upm6920 *upm, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&upm->i2c_rw_lock);
	ret = __upm6920_read_reg(upm, reg, &tmp);
	if (ret) {
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __upm6920_write_reg(upm, reg, tmp);
	if (ret) {
		chg_err("Failed: reg=%02X, ret=%d\n", reg, ret);
	}
out:
	mutex_unlock(&upm->i2c_rw_lock);
	return ret;
}

int upm6920_enter_hiz_mode(struct upm6920 *upm)
{
	u8 val = 0;

	chg_info(" enter");

	val = UPM6920_HIZ_ENABLE << UPM6920_ENHIZ_SHIFT;
	return  upm6920_update_bits(upm, UPM6920_REG_00, UPM6920_ENHIZ_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6920_enter_hiz_mode);

int upm6920_exit_hiz_mode(struct upm6920 *upm)
{
	u8 val = 0;

	chg_info(" enter");

	val = UPM6920_HIZ_DISABLE << UPM6920_ENHIZ_SHIFT;
	return upm6920_update_bits(upm, UPM6920_REG_00, UPM6920_ENHIZ_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6920_exit_hiz_mode);

int upm6920_set_hiz_mode(struct upm6920 *upm, bool en)
{
    int ret;

    if (en) {
       ret = upm6920_enter_hiz_mode(upm);
    } else {
      	ret = upm6920_exit_hiz_mode(upm);
    }

	return ret;
}


int upm6920_get_hiz_mode(struct upm6920 *upm, u8 *state)
{
	u8 val = 0;
	int ret = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_00, &val);
	if (ret) {
		chg_err("read UPM6920_REG_00 failed, ret:%d\n", ret);
		return ret;
	}
	*state = (val & UPM6920_ENHIZ_MASK) >> UPM6920_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(upm6920_get_hiz_mode);

static int upm6920_enable_ilim_pin(struct upm6920 *upm, bool en)
{
	u8 val = 0;

	chg_info("en:%d", en);

	if (en) {
		val = UPM6920_ENILIM_ENABLE << UPM6920_ENILIM_SHIFT;
	} else {
		val = UPM6920_ENILIM_DISABLE << UPM6920_ENILIM_SHIFT;
	}

	return upm6920_update_bits(upm, UPM6920_REG_00, UPM6920_ENILIM_MASK, val);
}

static int upm6920_set_input_current_limit(struct upm6920 *upm, int curr)
{
	u8 val = 0;

	chg_info("curr = %d", curr);

	if (curr < UPM6920_IINLIM_BASE)
		curr = UPM6920_IINLIM_BASE;

	val = (curr - UPM6920_IINLIM_BASE) / UPM6920_IINLIM_LSB;

	return upm6920_update_bits(upm, UPM6920_REG_00, UPM6920_IINLIM_MASK,
						val << UPM6920_IINLIM_SHIFT);
}

int upm6920_get_input_current_limit(struct upm6920 *upm, u32 *curr)
{
   u8 reg_val;
	int icl;
	int ret;

	ret = upm6920_read_byte(upm, UPM6920_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & UPM6920_IINLIM_MASK) >> UPM6920_IINLIM_SHIFT;
		icl = icl * UPM6920_IINLIM_LSB + UPM6920_IINLIM_BASE;
		*curr = icl * 1000;
	}

    return ret;
}


int upm6920_set_dp_output(struct upm6920 *upm, u8 mode)
{
	chg_info("mode = %d", mode);

	return upm6920_update_bits(upm, UPM6920_REG_01, UPM6920_DPDAC_MASK,
						mode << UPM6920_DPDAC_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6920_set_dp_output);

int upm6920_set_dm_output(struct upm6920 *upm, u8 mode)
{
	chg_info("mode = %d", mode);

	return upm6920_update_bits(upm, UPM6920_REG_01, UPM6920_DMDAC_MASK,
						mode << UPM6920_DMDAC_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6920_set_dm_output);

int upm6920_enable_12v_detect(struct upm6920 *upm, bool en)
{
	u8 val = 0;

	chg_info("en:%d", en);

	if (en) {
		val = UPM6920_ENABLE_12V << UPM6920_EN12V_SHIFT;
	} else {
		val = UPM6920_DISABLE_12V << UPM6920_EN12V_SHIFT;
	}

	return upm6920_update_bits(upm, UPM6920_REG_01, UPM6920_EN12V_MASK,
						val);
}
EXPORT_SYMBOL_GPL(upm6920_enable_12v_detect);

int upm6920_vindpm_offset(struct upm6920 *upm, u8 mode)
{
	u8 val = 0;

	chg_info("mode:%d", mode);

	val = mode << UPM6920_VINDPMOS_SHIFT;

	return upm6920_update_bits(upm, UPM6920_REG_01, UPM6920_VINDPMOS_MASK,
						val);
}
EXPORT_SYMBOL_GPL(upm6920_vindpm_offset);

int upm6920_adc_rate_select(struct upm6920 *upm, u8 mode)
{
	u8 val = 0;

	chg_info("mode:%d", mode);
	val = mode << UPM6920_CONV_RATE_SHIFT;

	return upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_CONV_RATE_MASK,
				val);
}
EXPORT_SYMBOL_GPL(upm6920_adc_rate_select);

//static int upm6920_adc_one_shot_start(struct upm6920 *upm)
//{
//	chg_info("conv start\n");
//
//	return upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_CONV_START_MASK,
//				UPM6920_CONV_START << UPM6920_CONV_START_SHIFT);
//}

int upm6920_enable_ico(struct upm6920* upm, bool en)
{
	u8 val = 0;
	int ret = 0;

	chg_info("en:%d", en);

	if (en) {
		val = UPM6920_ICO_ENABLE << UPM6920_ICOEN_SHIFT;
	} else {
		val = UPM6920_ICO_DISABLE << UPM6920_ICOEN_SHIFT;
	}

	ret = upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_ICOEN_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_enable_ico);

int upm6920_enable_hvdcp(struct upm6920 *upm, bool en)
{
	int ret;
	u8 val = 0;

	chg_info("en:%d", en);

	if (en) {
		val = UPM6920_HVDCP_ENABLE << UPM6920_HVDCPEN_SHIFT;
	} else {
		val = UPM6920_HVDCP_DISABLE << UPM6920_HVDCPEN_SHIFT;
	}

	ret = upm6920_update_bits(upm, UPM6920_REG_02,
				UPM6920_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_enable_hvdcp);

static int upm6920_force_dpdm(struct upm6920 *upm)
{
	int ret = 0;
	u8 val = 0;

	val = UPM6920_FORCE_DPDM << UPM6920_FORCE_DPDM_SHIFT;

	ret = upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_FORCE_DPDM_MASK, val);

	chg_info("Force DPDM %s\n", !ret ?  "successfully" : "failed");
	return ret;

}

static int upm6920_enable_auto_dpdm(struct upm6920* upm, bool en)
{
	u8 val = 0;
	int ret = 0;

	chg_info("en:%d", en);

	if (en) {
		val = UPM6920_AUTO_DPDM_ENABLE << UPM6920_AUTO_DPDM_EN_SHIFT;
	} else {
		val = UPM6920_AUTO_DPDM_DISABLE << UPM6920_AUTO_DPDM_EN_SHIFT;
	}

	ret = upm6920_update_bits(upm, UPM6920_REG_02,
						UPM6920_AUTO_DPDM_EN_MASK, val);

	return ret;
}

static int upm6920_reset_watchdog_timer(struct upm6920 *upm)
{
	u8 val = UPM6920_WDT_RESET << UPM6920_WDT_RESET_SHIFT;

	chg_info("enter\n");
	return upm6920_update_bits(upm, UPM6920_REG_03,
						UPM6920_WDT_RESET_MASK, val);
}

static int upm6920_enable_otg(struct upm6920 *upm)
{
	u8 val = UPM6920_OTG_ENABLE << UPM6920_OTG_CONFIG_SHIFT;

	chg_info("enable\n");
	return upm6920_update_bits(upm, UPM6920_REG_03,
				   UPM6920_OTG_CONFIG_MASK, val);
}

static int upm6920_disable_otg(struct upm6920 *upm)
{
	u8 val = UPM6920_OTG_DISABLE << UPM6920_OTG_CONFIG_SHIFT;

	chg_info("disable\n");
	return upm6920_update_bits(upm, UPM6920_REG_03,
				   UPM6920_OTG_CONFIG_MASK, val);
}

static int upm6920_enable_charger(struct upm6920 *upm)
{
	int ret = 0;
	u8 val = UPM6920_CHG_ENABLE << UPM6920_CHG_CONFIG_SHIFT;

	chg_info("enable\n");
	ret = upm6920_update_bits(upm, UPM6920_REG_03,
				UPM6920_CHG_CONFIG_MASK, val);

	return ret;
}

static int upm6920_disable_charger(struct upm6920 *upm)
{
	int ret = 0;
	u8 val = UPM6920_CHG_DISABLE << UPM6920_CHG_CONFIG_SHIFT;

	chg_info("disable\n");
	ret = upm6920_update_bits(upm, UPM6920_REG_03,
				UPM6920_CHG_CONFIG_MASK, val);

	return ret;
}

int upm6920_vsys_min_limit(struct upm6920 *upm, int sys_min)
{
    u8 val = 0;

	chg_info("sys_min:%d\n", sys_min);

	if (sys_min < UPM6920_SYS_MINV_BASE) {
		sys_min = UPM6920_SYS_MINV_BASE;
	}

	val = (sys_min - UPM6920_SYS_MINV_BASE) / UPM6920_SYS_MINV_LSB;

    return upm6920_update_bits(upm, UPM6920_REG_03,
			UPM6920_SYS_MINV_MASK, val << UPM6920_SYS_MINV_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6920_vsys_min_limit);

static int upm6920_set_chargecurrent(struct upm6920 *upm, int curr)
{
	u8 ichg = 0;

	chg_info("ichg:%d\n", curr);

	if (curr < UPM6920_ICHG_BASE)
		curr = UPM6920_ICHG_BASE;

	ichg = (curr - UPM6920_ICHG_BASE) / UPM6920_ICHG_LSB;
	return upm6920_update_bits(upm, UPM6920_REG_04,
						UPM6920_ICHG_MASK, ichg << UPM6920_ICHG_SHIFT);
}

static int upm6920_set_prechg_current(struct upm6920 *upm, int curr)
{
	u8 iprechg = 0;

	chg_info("curr:%d\n", curr);

	if (curr < UPM6920_IPRECHG_BASE)
		curr = UPM6920_IPRECHG_BASE;
	if (curr > UPM6920_IPRECHG_MAX)
		curr = UPM6920_IPRECHG_MAX;

	iprechg = (curr - UPM6920_IPRECHG_BASE) / UPM6920_IPRECHG_LSB;

	return upm6920_update_bits(upm, UPM6920_REG_05,
						UPM6920_IPRECHG_MASK, iprechg << UPM6920_IPRECHG_SHIFT);
}

static int upm6920_set_term_current(struct upm6920 *upm, int curr)
{
	u8 iterm = 0;

	chg_info("curr:%d\n", curr);

	if (curr < UPM6920_ITERM_BASE)
		curr = UPM6920_ITERM_BASE;
	if (curr > UPM6920_ITERM_MAX)
		curr = UPM6920_ITERM_MAX;

	iterm = (curr - UPM6920_ITERM_BASE) / UPM6920_ITERM_LSB;

	return upm6920_update_bits(upm, UPM6920_REG_05,
						UPM6920_ITERM_MASK, iterm << UPM6920_ITERM_SHIFT);
}

static int upm6920_set_chargevolt(struct upm6920 *upm, int volt)
{
	u8 val = 0;

	chg_info("volt:%d", volt);

	if (volt < UPM6920_VREG_BASE)
		volt = UPM6920_VREG_BASE;
	if (volt > UPM6920_VREG_MAX)
		volt = UPM6920_VREG_MAX;

	val = (volt - UPM6920_VREG_BASE) / UPM6920_VREG_LSB;
	return upm6920_update_bits(upm, UPM6920_REG_06,
						UPM6920_VREG_MASK, val << UPM6920_VREG_SHIFT);
}

int upm6920_get_chargevol(struct upm6920 *upm, int *volt)
{
   	u8 reg_val;
	int vchg;
	int ret;

	ret = upm6920_read_byte(upm, UPM6920_REG_06, &reg_val);
	if (!ret) {
		vchg = (reg_val & UPM6920_VREG_MASK) >> UPM6920_VREG_SHIFT;
		vchg = vchg * UPM6920_VREG_LSB + UPM6920_VREG_BASE;
		*volt = vchg * 1000;
	}
	return ret;
}


int upm6920_enable_term(struct upm6920 *upm, bool enable)
{
	u8 val = 0;
	int ret = 0;

	chg_info("enable:%d\n", enable);

	if (enable)
		val = UPM6920_TERM_ENABLE << UPM6920_EN_TERM_SHIFT;
	else
		val = UPM6920_TERM_DISABLE << UPM6920_EN_TERM_SHIFT;

	ret = upm6920_update_bits(upm, UPM6920_REG_07,
						UPM6920_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_enable_term);

int upm6920_set_watchdog_timer(struct upm6920 *upm, u8 mode)
{
	u8 val = 0;

	chg_info("mode:%d\n", mode);

	val = mode << UPM6920_WDT_SHIFT;

	return upm6920_update_bits(upm, UPM6920_REG_07,
						UPM6920_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6920_set_watchdog_timer);

static int upm6920_disable_watchdog_timer(struct upm6920 *upm)
{
	u8 val = UPM6920_WDT_DISABLE << UPM6920_WDT_SHIFT;

	chg_info("enter\n");

	return upm6920_update_bits(upm, UPM6920_REG_07,
						UPM6920_WDT_MASK, val);
}

static int upm6920_enable_safety_timer(struct upm6920 *upm, bool en)
{
	u8 val = 0;

	chg_info("en:%d\n", en);

	if (en) {
		val = UPM6920_CHG_TIMER_ENABLE << UPM6920_EN_TIMER_SHIFT;
	} else {
		val = UPM6920_CHG_TIMER_DISABLE << UPM6920_EN_TIMER_SHIFT;
	}

	return upm6920_update_bits(upm, UPM6920_REG_07, UPM6920_EN_TIMER_MASK,
				   val);
}

#if 1
static int upm6920_ir_comp_set_resistor(struct upm6920 *upm, int bat_comp)
{
	u8 val = 0;

	chg_info("bat_comp:%d\n", bat_comp);

	if (bat_comp < UPM6920_BAT_COMP_MOHM_BASE)
		bat_comp = UPM6920_BAT_COMP_MOHM_BASE;
	if (bat_comp > UPM6920_BAT_COMP_MOHM_MAX)
		bat_comp = UPM6920_BAT_COMP_MOHM_MAX;

	val = (bat_comp - UPM6920_BAT_COMP_MOHM_BASE) / UPM6920_BAT_COMP_MOHM_LSB;
	return upm6920_update_bits(upm, UPM6920_REG_08,
						UPM6920_BAT_COMP_MASK, val << UPM6920_BAT_COMP_SHIFT);
}

static int upm6920_ir_comp_set_vclamp(struct upm6920 *upm, int vol_clamp)
{
	u8 val = 0;

	chg_info("vol_clamp:%d\n", vol_clamp);

	if (vol_clamp < UPM6920_VCLAMP_MV_BASE)
		vol_clamp = UPM6920_VCLAMP_MV_BASE;
	if (vol_clamp > UPM6920_VCLAMP_MV_MAX)
		vol_clamp = UPM6920_VCLAMP_MV_MAX;

	val = (vol_clamp - UPM6920_VCLAMP_MV_BASE) / UPM6920_VCLAMP_MV_LSB;
	return upm6920_update_bits(upm, UPM6920_REG_08,
						UPM6920_VCLAMP_MASK, val << UPM6920_VCLAMP_SHIFT);
}
#endif

int upm6920_force_start_ico(struct upm6920 *upm, bool en)
{
	u8 val = 0;

	chg_info("en:%d\n", en);

	if (en) {
		val = UPM6920_FORCE_ICO << UPM6920_FORCE_ICO_SHIFT;
	} else {
		val = UPM6920_NOT_FORCE_ICO << UPM6920_FORCE_ICO_SHIFT;
	}

	return upm6920_update_bits(upm, UPM6920_REG_09, UPM6920_FORCE_ICO_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6920_force_start_ico);

int upm6920_enable_shipmode(struct upm6920 *upm, bool en)
{
	u8 val = 0;

	chg_info("en:%d\n", en);

	if (en) {
		val = UPM6920_BATFET_OFF << UPM6920_BATFET_DIS_SHIFT;
	} else {
		val = UPM6920_BATFET_ON << UPM6920_BATFET_DIS_SHIFT;
	}

	return upm6920_update_bits(upm, UPM6920_REG_09, UPM6920_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6920_enable_shipmode);

int upm6920_batfet_delay(struct upm6920 *upm, u8 mode)
{
	u8 val = 0;

	chg_info("mode:%d\n", mode);

	val = mode << UPM6920_BATFET_DLY_SHIFT;

	return upm6920_update_bits(upm, UPM6920_REG_09, UPM6920_BATFET_DLY_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(upm6920_batfet_delay);

int upm6920_set_batfet_rst(struct upm6920 *upm, bool en)
{
	u8 val = 0;

	chg_info("en:%d\n", en);

	if (en) {
		val = UPM6920_BATFET_RST_EN_ENABLE << UPM6920_BATFET_RST_EN_SHIFT;
	} else {
		val = UPM6920_BATFET_RST_EN_DISABLE << UPM6920_BATFET_RST_EN_SHIFT;
	}

	return upm6920_update_bits(upm, UPM6920_REG_09,
				UPM6920_BATFET_RST_EN_MASK, val);
}
EXPORT_SYMBOL_GPL(upm6920_set_batfet_rst);

static int upm6920_set_boost_voltage(struct upm6920 *upm, int volt)
{
	u8 val = 0;

	chg_info("vol:%d\n", volt);

	if (volt < UPM6920_BOOSTV_BASE)
		volt = UPM6920_BOOSTV_BASE;
	if (volt > UPM6920_BOOSTV_MAX)
		volt = UPM6920_BOOSTV_MAX;

	val = ((volt - UPM6920_BOOSTV_BASE) / UPM6920_BOOSTV_LSB)
			<< UPM6920_BOOSTV_SHIFT;

	return upm6920_update_bits(upm, UPM6920_REG_0A,
				UPM6920_BOOSTV_MASK, val);
}

static int upm6920_set_boost_current(struct upm6920 *upm, int curr)
{
	u8 temp = 0;

	chg_info("curr:%d\n", curr);

	if (curr < 750)
		temp = UPM6920_BOOST_LIM_500MA;
	else if (curr < 1200)
		temp = UPM6920_BOOST_LIM_750MA;
	else if (curr < 1400)
		temp = UPM6920_BOOST_LIM_1200MA;
	else if (curr < 1650)
		temp = UPM6920_BOOST_LIM_1400MA;
	else if (curr < 1870)
		temp = UPM6920_BOOST_LIM_1650MA;
	else if (curr < 2150)
		temp = UPM6920_BOOST_LIM_1875MA;
	else if (curr < 2450)
		temp = UPM6920_BOOST_LIM_2150MA;
	else
		temp= UPM6920_BOOST_LIM_2450MA;

	chg_info("boost current reg_val = %d",temp);
	return upm6920_update_bits(upm, UPM6920_REG_0A,
				UPM6920_BOOST_LIM_MASK,
				temp << UPM6920_BOOST_LIM_SHIFT);
}

static int upm6920_get_vbus_stat(struct upm6920 *upm, int *vbus_stat)
{
	int ret = 0;
	u8 reg_val = 0;
	int tmp = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_0B, &reg_val);
	if (ret) {
		return ret;
	}
	tmp = (reg_val & UPM6920_VBUS_STAT_MASK);
	tmp >>= UPM6920_VBUS_STAT_SHIFT;
	*vbus_stat = tmp;

	chg_info("type:%d,reg0B = 0x%x\n", tmp, reg_val);

	return ret;
}

static int upm6920_set_input_volt_limit(struct upm6920 *upm, int volt)
{
	u8 val = 0;

	chg_info(" volt = %d", volt);

	if (volt < UPM6920_VINDPM_MIN)
		volt = UPM6920_VINDPM_MIN;
	if (volt > UPM6920_VINDPM_MAX)
		volt = UPM6920_VINDPM_MAX;

	val = (volt - UPM6920_VINDPM_BASE) / UPM6920_VINDPM_LSB;

	upm6920_update_bits(upm, UPM6920_REG_0D,
						UPM6920_FORCE_VINDPM_MASK, UPM6920_FORCE_VINDPM_ENABLE << UPM6920_FORCE_VINDPM_SHIFT);

	return upm6920_update_bits(upm, UPM6920_REG_0D,
						UPM6920_VINDPM_MASK, val << UPM6920_VINDPM_SHIFT);
}


static int upm6920_get_input_volt_limit(struct upm6920 *upm, u32 *volt)
{
	u8 reg_val;
	int vchg;
	int ret;

	ret = upm6920_read_byte(upm, UPM6920_REG_0D, &reg_val);
	if (!ret) {
		vchg = (reg_val & UPM6920_FORCE_VINDPM_MASK) >> UPM6920_VINDPM_SHIFT;
		vchg = vchg * UPM6920_VINDPM_LSB + UPM6920_VINDPM_BASE;
		*volt = vchg * 1000;
	}

	return ret;

}


int upm6920_adc_read_battery_volt(struct upm6920 *upm, int *bat_vol)
{
	uint8_t val = 0;
	int volt = 0;
	int ret = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_0E, &val);
	if (ret < 0) {
		chg_err("read battery voltage failed :%d\n", ret);
		return ret;
	}

	volt = UPM6920_BATV_BASE + ((val & UPM6920_BATV_MASK) >> UPM6920_BATV_SHIFT) * UPM6920_BATV_LSB;
	*bat_vol = volt;

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_adc_read_battery_volt);

int upm6920_adc_read_sys_volt(struct upm6920 *upm, int *sys_vol)
{
	uint8_t val;
	int volt;
	int ret;

	ret = upm6920_read_byte(upm, UPM6920_REG_0F, &val);
	if (ret < 0) {
		chg_err("read system voltage failed :%d\n", ret);
		return ret;
	}

	volt = UPM6920_SYSV_BASE + ((val & UPM6920_SYSV_MASK) >> UPM6920_SYSV_SHIFT) * UPM6920_SYSV_LSB;
	*sys_vol = volt;

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_adc_read_sys_volt);

int upm6920_adc_read_temperature(struct upm6920 *upm, int *ts_pct)
{
	uint8_t val = 0;
	int temp = 0;
	int ret = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_10, &val);
	if (ret < 0) {
		chg_err("read temperature failed :%d\n", ret);
		return ret;
	}

	temp = UPM6920_TSPCT_BASE + (((val & UPM6920_TSPCT_MASK) >> UPM6920_TSPCT_SHIFT) * UPM6920_TSPCT_LSB / UPM6920_TSPCT_PCS);
	*ts_pct = temp;

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_adc_read_temperature);

int upm6920_adc_start(struct upm6920 *upm, bool oneshot)
{
    u8 val;
    int ret;

	pr_err("%s oneshot= %d\n", __func__,oneshot);
    ret = upm6920_read_byte(upm, UPM6920_REG_02, &val);
    if (ret < 0) {
        dev_err(upm->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
    }

    if (((val & UPM6920_CONV_RATE_MASK) >> UPM6920_CONV_RATE_SHIFT)
            == UPM6920_ADC_CONTINUE_ENABLE)
        return 0;

    if (oneshot) {
        ret = upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_CONV_START_MASK,
                UPM6920_CONV_START << UPM6920_CONV_START_SHIFT);
    }
    else {
        ret = upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_CONV_RATE_MASK,
                UPM6920_ADC_CONTINUE_ENABLE << UPM6920_CONV_RATE_SHIFT);
    }

    return ret;
}
EXPORT_SYMBOL_GPL(upm6920_adc_start);

int upm6920_adc_stop(struct upm6920 *upm)
{
	pr_err("%s\n", __func__);
    return upm6920_update_bits(upm, UPM6920_REG_02, UPM6920_CONV_RATE_MASK,
                UPM6920_ADC_CONTINUE_DISABLE << UPM6920_CONV_RATE_SHIFT);
}
EXPORT_SYMBOL_GPL(upm6920_adc_stop);

int upm6920_adc_read_vbus_volt(struct upm6920 *upm, int *vbus_vol) {
	uint8_t val = 0;
	int volt = 0;
	int ret = 0;
	bool vbus_good;
	u8 reg_val = 0;

    upm6920_adc_start(upm, true);
    msleep(50);

	ret = upm6920_read_byte(upm, UPM6920_REG_11, &val);
		if (ret < 0) {
		chg_err("read vbus voltage failed :%d\n", ret);
		return ret;
	}

	ret = upm6920_read_byte(upm, UPM6920_REG_0B, &reg_val);
	if (ret) {
		printk("upm6920_adc_read_vbus_volt read 0x0B register failed.");
	}
	vbus_good = !!(reg_val & UPM6920_PG_STAT_MASK);

	if(vbus_good){
		volt = UPM6920_VBUSV_BASE + ((val & UPM6920_VBUSV_MASK) >> UPM6920_VBUSV_SHIFT) * UPM6920_VBUSV_LSB;
		*vbus_vol = volt * 1000;
	}else{
		*vbus_vol = 0;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_adc_read_vbus_volt);

int upm6920_adc_read_charge_current(struct upm6920 *upm, int *bat_cur)
{
	uint8_t val = 0;
	int curr = 0;
	int ret = 0;

    upm6920_adc_start(upm, true);
    msleep(50);

	ret = upm6920_read_byte(upm, UPM6920_REG_12, &val);
	if (ret < 0) {
		chg_err("read charge current failed :%d\n", ret);
		return ret;
	}

	curr = (int)(UPM6920_ICHGR_BASE + ((val & UPM6920_ICHGR_MASK) >> UPM6920_ICHGR_SHIFT) * UPM6920_ICHGR_LSB);
	*bat_cur = curr * 1000;

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_adc_read_charge_current);

int upm6920_read_ico_idpm_limit(struct upm6920 *upm, int *icl)
{
	uint8_t val = 0;
	int ret = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_13, &val);
	if (ret < 0) {
		chg_err("read idpm limit while ico failed :%d\n", ret);
		return ret;
	}

	*icl = UPM6920_IDPM_LIM_BASE + ((val & UPM6920_IDPM_LIM_MASK) >> UPM6920_IDPM_LIM_SHIFT) * UPM6920_IDPM_LIM_LSB ;

	return ret;
}
EXPORT_SYMBOL_GPL(upm6920_read_ico_idpm_limit);

static int upm6920_reset_chip(struct upm6920 *upm)
{
	int ret = 0;
	u8 val = UPM6920_RESET << UPM6920_RESET_SHIFT;

	ret = upm6920_update_bits(upm, UPM6920_REG_14,
						UPM6920_RESET_MASK, val);

	chg_info("reset, ret:%d\n", ret);

	return ret;
}

static int upm6920_detect_device(struct upm6920 *upm)
{
	int ret = 0;
	u8 data = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_14, &data);
	if (!ret) {
		upm->part_no = (data & UPM6920_PN_MASK) >> UPM6920_PN_SHIFT;
		upm->revision =
		    (data & UPM6920_DEV_REV_MASK) >> UPM6920_DEV_REV_SHIFT;
	}

	chg_info("upm->part_no = 0x%X revision=0x%X\n", upm->part_no, upm->revision);

	return ret;
}


//static int upm6920_start_hvdcp_detect(struct upm6920 *upm)
//{
//	atomic_set(&upm->hvdcp_detect_trig, 1);
//	wake_up_interruptible(&upm->hvdcp_wqueue);
//
//	return 0;
//}

static int upm6920_get_bc12_type(struct upm6920 *upm, int *type)
{
	int ret = 0;
	int vbus_stat = 0;
	//int upm_chg_type = CHARGER_UNKNOWN;

#ifdef CUSTOM_BC12_ENABLE
	int attach;

	attach = atomic_read(&upm->attach);
	printk("upm6920_get_bc12_type attach=%d\n", attach);
#endif

	ret = upm6920_get_vbus_stat(upm, &vbus_stat);
	if (ret) {
		chg_err("get vbus stat failed, ret:%d\n", ret);
		return ret;
	}

	switch (vbus_stat) {
	case UPM6920_VBUS_TYPE_NONE:
		//upm_chg_type = CHARGER_UNKNOWN;
		if(upm->vbus_attach){
			upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		}else{
			upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		chg_info("charger type: none\n");
		break;
	case UPM6920_VBUS_TYPE_SDP:
		//upm_chg_type = STANDARD_HOST;
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		chg_info("charger type: SDP\n");
		break;
	case UPM6920_VBUS_TYPE_CDP:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB_CDP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		//upm_chg_type = CHARGING_HOST;
		chg_info("charger type: CDP\n");
		break;
	case UPM6920_VBUS_TYPE_DCP:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		//upm_chg_type = STANDARD_CHARGER;
		chg_info("charger type: DCP\n");
		break;
	case UPM6920_VBUS_TYPE_HVDCP:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		//upm_chg_type = STANDARD_CHARGER;
		chg_info("charger type: HVDCP\n");
		break;
	case UPM6920_VBUS_TYPE_UNKNOWN:
		if(upm->vbus_attach){
			upm->psy_usb_type =  POWER_SUPPLY_USB_TYPE_SDP;
		}else{
			upm->psy_usb_type =  POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		//upm_chg_type = NONSTANDARD_CHARGER;
		chg_info("charger type: unknown adapter\n");
#ifdef CUSTOM_BC12_ENABLE
        if (upm->force_detect_count < 10) {
            schedule_delayed_work(&upm->force_detect_dwork, msecs_to_jiffies(2000));
        }
#endif
		break;
	case UPM6920_VBUS_TYPE_NON_STD:
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		//upm_chg_type = NONSTANDARD_CHARGER;
		chg_info("charger type: non-std charger\n");
		break;
	default:
		//upm_chg_type = NONSTANDARD_CHARGER;
		upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		upm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		upm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		chg_info("charger type: invalid value, report non-std\n");
		break;
	}

#if IS_ENABLED(CONFIG_WB_DOCKING_SUPPORT) //Leo 20230110
    get_mtk_extcon_info();
    if (get_is_docking() == true) {
        upm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        upm->psy_chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        upm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
    }
#endif

	//if (upm_chg_type == STANDARD_CHARGER) {
	//	upm6920_start_hvdcp_detect(upm);
	//}

#if 0
	if (upm_chg_type != STANDARD_CHARGER) {
		Charger_Detect_Release();
	}
#endif
	*type = upm->psy_chg_type;

#ifdef CUSTOM_BC12_ENABLE
	cust_chg_enable_bc12(upm, false);
#endif

	return 0;
}

static int upm6920_send_ta20_current_pattern(struct charger_device *chg_dev, u32 uV);
static ssize_t
upm6920_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct upm6920 *upm = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "upm6920 Reg");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = upm6920_read_byte(upm, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	//upm6920_send_ta20_current_pattern(upm->chg_dev, 8500*1000); add for test

	return idx;
}

static ssize_t
upm6920_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct upm6920 *upm = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x14) {
		upm6920_write_byte(upm, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, upm6920_show_registers,
		   upm6920_store_registers);

static struct attribute *upm6920_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group upm6920_attr_group = {
	.attrs = upm6920_attributes,
};

static int upm6920_hvdcp_detect_thread(void *data)
{
	int ret = 0;
	int wait_cnt;
	int vbus_stat = 0;
	struct upm6920 *upm = data;

	chg_debug("enter");

	while (!kthread_should_stop()) {
		wait_event_interruptible(upm->hvdcp_wqueue,
			atomic_read(&upm->hvdcp_detect_trig) || kthread_should_stop());

		if (kthread_should_stop()) {
			chg_info("should stop, break\n");
			break;
		}

		chg_debug("hvdcp detect start\n");
		atomic_set(&upm->hvdcp_detect_trig, 0);
		wait_cnt = 0;
		__pm_stay_awake(upm->hvdcp_detect_ws);
		ret = upm6920_enable_hvdcp(upm, true);
		if (ret) {
			chg_err("enable hvdcp failed, ret:%d\n", ret);
			goto out;
		}

		ret = upm6920_force_dpdm(upm);
		if (ret) {
			chg_err("force dpdm failed, ret:%d\n", ret);
			goto out;
		}

		do {
			msleep(300);

			ret = upm6920_get_vbus_stat(upm, &vbus_stat);
			if (ret) {
				chg_err("get vbus stat failed, ret:%d\n", ret);
				goto out;
			}

			if (vbus_stat == UPM6920_VBUS_TYPE_HVDCP) {
				chg_info("hvdcp detected\n");
				break;
			}
		} while(wait_cnt++ < 10);

out:
		__pm_relax(upm->hvdcp_detect_ws);
		chg_debug("hvdcp detect end\n");
	}

	return 0;
}

static struct upm6920_platform_data *upm6920_parse_dt(struct device_node *np,
						      struct upm6920 *upm)
{
	int ret = 0;
	struct upm6920_platform_data *pdata;

	pdata = devm_kzalloc(upm->dev, sizeof(struct upm6920_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &upm->chg_dev_name) < 0) {
		upm->chg_dev_name = "primary_chg";
		chg_info("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &upm->eint_name) < 0) {
		upm->eint_name = "chr_stat";
		chg_info("no eint name\n");
	}

	ret = of_get_named_gpio(np, "upm,intr_gpio", 0);
	if (ret < 0) {
		chg_err("%s no int gpio(%d)\n", __func__, ret);
	} else {
		upm->irq_gpio = ret;
		pr_info("%s irq_gpio = %u\n", __func__, upm->irq_gpio);
	}

	ret = of_property_read_u32(np, "upm,upm6920,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		chg_err("Failed to read node of upm,upm6920,usb-vlim\n");
	}
	upm->mivr=pdata->usb.vlim;//jnier add 20240606

	ret = of_property_read_u32(np, "upm,upm6920,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		chg_err("Failed to read node of upm,upm6920,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		chg_err("Failed to read node of upm,upm6920,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		chg_err("Failed to read node of upm,upm6920,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 256;
		chg_err("Failed to read node of upm,upm6920,precharge-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 250;
		chg_err("Failed to read node of upm,upm6920,termination-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		chg_err("Failed to read node of upm,upm6920,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		chg_err("Failed to read node of upm,upm6920,boost-current\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,const-voltage",
				 &pdata->cv);
	if (ret) {
		pdata->cv = 4200;
		chg_err("Failed to read node of upm,upm6920,const-voltage\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,ircomp-r",
				 &pdata->ircomp_r);
	if (ret) {
		pdata->ircomp_r = 0;
		chg_err("Failed to read node of upm,upm6920,ircomp-r\n");
	}

	ret = of_property_read_u32(np, "upm,upm6920,ircomp-v",
				 &pdata->ircomp_v);
	if (ret) {
		pdata->ircomp_v = 0;
		chg_err("Failed to read node of upm,upm6920,ircomp-v\n");
	}

    upm->drvbus_gpio = of_get_named_gpio(np, "upm,drvbus-gpio", 0);
    if (upm->drvbus_gpio < 0) {
        chg_err("upm,upm->drvbus_gpio is not available\n");
    } else {
        ret = gpio_request(upm->drvbus_gpio, "upm,drvbus-gpio");
        if (ret < 0) {
            chg_err("gpio_request upm,drvbus-gpio failed!\n");
        } else {
            if (gpio_is_valid(upm->drvbus_gpio)) {
                gpio_direction_output(upm->drvbus_gpio, 0);
                chg_err("upm drvbus_gpio default low\n");
            }
        }
    }

	return pdata;
}


static int upm6920_get_charge_stat(struct upm6920 *upm, int *state)
{
    int ret;
    u8 val;

    ret = upm6920_read_byte(upm, UPM6920_REG_0B, &val);
    if (!ret) {
        if ((val & UPM6920_VBUS_STAT_MASK) >> UPM6920_VBUS_STAT_SHIFT 
                == UPM6920_VBUS_TYPE_OTG) {
            *state = POWER_SUPPLY_STATUS_DISCHARGING;
            return ret;
        }
        val = val & UPM6920_CHRG_STAT_MASK;
        val = val >> UPM6920_CHRG_STAT_SHIFT;
        switch (val)
        {
		case UPM6920_CHRG_STAT_NOTCHG:
			if (upm->chg_config) {
                *state = POWER_SUPPLY_STATUS_CHARGING;
            } else {
                *state = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            break;
        case UPM6920_CHRG_STAT_PRECHG:
        case UPM6920_CHRG_STAT_FASTCHG:
            *state = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case UPM6920_CHRG_STAT_CHGDONE:
            *state = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
            *state = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
        }
    }
    pr_err("%s---->%d  %02x\n", __func__, *state, val);

    return ret;
}

#ifdef CUSTOM_BC12_ENABLE
static void upm6920_force_detection_dwork_handler(struct work_struct *work) {
    int ret;
    struct upm6920 *upm = container_of(work, struct upm6920,
                                        force_detect_dwork.work);

    printk("upm6920_force_detection_dwork_handler begin\n");

    cust_chg_enable_bc12(upm, true);
    
    ret = upm6920_force_dpdm(upm);
    if (ret) {
        dev_err(upm->dev, "%s: force dpdm failed(%d)\n", __func__, ret);
        return;
    }

    upm->power_good = false;

    upm->force_detect_count++;

    printk("upm6920_force_detection_dwork_handler end\n");
}
#endif


static void upm6920_dump_regs(struct upm6920 *upm)
{
	int addr = 0;
	u8 val;
	int ret = 0;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = upm6920_read_byte(upm, addr, &val);
		chg_info("REG[0x%02X]=0x%02X\n", addr, val);
	}

	return;
}

static void upm6920_vbus_in_func(struct upm6920 *upm)
{
	//Charger_Detect_Init();
#ifdef CUSTOM_BC12_EFFECT
	cust_chg_enable_bc12(upm, true);
#endif
}

static void upm6920_vbus_out_func(struct upm6920 *upm)
{
#if 0
	int ret = 0;

	ret = upm6920_enable_hvdcp(upm, false);
	if (ret) {
		chg_err("disable hvdcp failed, ret:%d\n", ret);
	}
#endif
	upm->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
	//schedule_delayed_work(&upm->psy_dwork, 0);
	//Charger_Detect_Release();

#ifdef CUSTOM_BC12_EFFECT
	cust_chg_enable_bc12(upm, false);
#endif
}

static irqreturn_t upm6920_irq_handler(int irq, void *data)
{
	int ret = 0;
	u8 reg_val = 0;
	bool prev_power_good = false;
	bool prev_vbus_attach = false;
	struct upm6920 *upm = (struct upm6920 *)data;

	chg_info("enter\n");

	//upm6920_dump_regs(upm);

	prev_power_good = upm->power_good;
	prev_vbus_attach = upm->vbus_attach;

	ret = upm6920_read_byte(upm, UPM6920_REG_0B, &reg_val);
	if (ret) {
		chg_err("read 0x0B register failed.");
		return IRQ_HANDLED;
	}
	upm->power_good = !!(reg_val & UPM6920_PG_STAT_MASK);
	chg_info("(%d,%d,0x0B=0x%X)\n", prev_power_good, upm->power_good, reg_val);

	ret = upm6920_read_byte(upm, UPM6920_REG_11, &reg_val);
	if (ret) {
		chg_err("read 0x11 register failed.");
		return IRQ_HANDLED;
	}
	upm->vbus_attach = !!(reg_val & UPM6920_VBUS_GD_MASK);
	chg_info("(%d,%d,0x11=0x%X)\n", prev_vbus_attach, upm->vbus_attach, reg_val);

	if (!prev_vbus_attach && upm->vbus_attach) {
		chg_info("adapter/usb inserted\n");
#ifdef CUSTOM_BC12_ENABLE
		upm->force_detect_count = 0;
		//schedule_delayed_work(&upm->psy_dwork, 0);
#endif
		upm6920_set_chargecurrent(upm, 500);
		msleep(30);
		upm6920_set_input_current_limit(upm, 500);
		msleep(30);
		upm6920_vbus_in_func(upm);
		upm->chg_config = true;
		schedule_delayed_work(&upm->hidd_dwork, msecs_to_jiffies(1000));
	} else if (prev_vbus_attach && !upm->vbus_attach) {
		chg_info("adapter/usb removed\n");
		cancel_delayed_work_sync(&upm->hidd_dwork);
		re_try = 5;
        upm->chg_config = false;
		upm6920_set_chargecurrent(upm, 500);
		msleep(30);
		upm6920_set_input_current_limit(upm, 500);
		msleep(30);
		upm6920_vbus_out_func(upm);
		ret = upm6920_enter_hiz_mode(upm);
    	if (ret) {
        	chg_info("Failed to enter hiz mode\n");
		}
		msleep(140);
		ret = upm6920_exit_hiz_mode(upm);
    	if (ret) {
        	chg_info("Failed to exit hiz mode\n");
		}

		ret = upm6920_get_bc12_type(upm, &upm->chg_type);
		power_supply_changed(upm->psy);

#ifdef CUSTOM_BC12_ENABLE
#ifndef CUSTOM_BC12_EFFECT
		cust_chg_enable_bc12(upm, true);
#endif
#endif
		return IRQ_HANDLED;
	}

	if (!prev_power_good && upm->power_good) {
		chg_info("bc1.2 done\n");
		ret = upm6920_get_bc12_type(upm, &upm->chg_type);
		power_supply_changed(upm->psy);
		//schedule_delayed_work(&upm->psy_dwork, 0);
	}

	return IRQ_HANDLED;
}

static int upm6920_register_interrupt(struct upm6920 *upm)
{
	int ret = 0;

	ret = devm_gpio_request_one(upm->dev, upm->irq_gpio, GPIOF_DIR_IN,
			devm_kasprintf(upm->dev, GFP_KERNEL,
			"upm6920_irq_gpio.%s", dev_name(upm->dev)));
	if (ret < 0) {
		chg_err("gpio request fail(%d)\n", ret);
		return ret;
	}

	upm->client->irq = gpio_to_irq(upm->irq_gpio);
	if (upm->client->irq < 0) {
		chg_err("gpio2irq fail(%d)\n", upm->client->irq);
		return upm->client->irq;
	}
	chg_info("irq = %d\n", upm->client->irq);

	ret = devm_request_threaded_irq(upm->dev, upm->client->irq, NULL,
					upm6920_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"upm6920_irq", upm);
	if (ret < 0) {
		chg_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(upm->client->irq);

	return 0;
}

static int upm6920_init_device(struct upm6920 *upm)
{
	int ret = 0;

	ret = upm6920_disable_watchdog_timer(upm);
	if (ret) {
		chg_err("Failed to disable watch dog, ret = %d\n", ret);
	}
	ret = upm6920_set_prechg_current(upm, upm->platform_data->iprechg);
	if (ret) {
		chg_err("Failed to set prechg current, ret = %d\n", ret);
	}
	ret = upm6920_set_chargevolt(upm, upm->platform_data->cv);
	if (ret) {
		chg_err("Failed to set cv, ret = %d\n", ret);
	}
	ret = upm6920_set_term_current(upm, upm->platform_data->iterm);
	if (ret) {
		chg_err("Failed to set termination current, ret = %d\n", ret);
	}
	ret = upm6920_set_boost_voltage(upm, upm->platform_data->boostv);
	if (ret) {
		chg_err("Failed to set boost voltage, ret = %d\n", ret);
	}
	ret = upm6920_set_boost_current(upm, upm->platform_data->boosti);
	if (ret) {
		chg_err("Failed to set boost current, ret = %d\n", ret);
	}
	ret = upm6920_enable_ilim_pin(upm, true);
	if (ret) {
		chg_err("Failed to set enlim, ret = %d\n", ret);
	}
	ret = upm6920_enable_auto_dpdm(upm, true);
	if (ret) {
		chg_err("Failed to set auto dpdm, ret = %d\n", ret);
	}
	ret = upm6920_set_input_volt_limit(upm,   upm->platform_data->usb.vlim);
	if (ret) {
		chg_err("Failed to set input volt limit, ret = %d\n", ret);
	}
	ret = upm6920_enable_hvdcp(upm, false);
	if (ret) {
		chg_err("Failed to disable hvdcp, ret = %d\n", ret);
	}

	ret = upm6920_ir_comp_set_resistor(upm, upm->platform_data->ircomp_r);
	if (ret) {
		chg_err("Failed to set ircomp_r, ret = %d\n", ret);
	}
	ret = upm6920_ir_comp_set_vclamp(upm, upm->platform_data->ircomp_v);
	if (ret) {
		chg_err("Failed to set ircomp_v, ret = %d\n", ret);
	}

	return 0;
}

static void upm6920_inform_probe_dwork_handler(struct work_struct *work)
{
	struct upm6920 *upm = container_of(work, struct upm6920,
								probe_dwork.work);
	//Charger_Detect_Init();
	upm6920_force_dpdm(upm);
	msleep(3000);
	upm6920_irq_handler(upm->irq, (void *) upm);
}

static void upm6920_hidden_mode_work_handler(struct work_struct *work){
	u8 val = 0;
	int temp1 = 0;
	int temp2 = 0;
	struct upm6920 *upm = container_of(work, struct upm6920,
								hidd_dwork.work);
	if(upm->sc8989x_charger){
		return;
	}
	chg_err("hidd work process\n");
	upm6920_write_byte(upm, UPM6920_REG_A9, REGA9_MODE_ENTER);
	
	upm6920_read_byte(upm, UPM6920_REG_A4, &val);
	//printf("rega4:0x%x\n", val);
	temp1 = !!(val & UPM6920_A4_VAL_MASK);
	upm6920_read_byte(upm, UPM6920_REG_A3, &val);
	//printf("rega3:0x%x\n", val);
	temp2 = !!(val & UPM6920_A3_VAL_MASK);

	upm6920_write_byte(upm, UPM6920_REG_A9, REGA9_MODE_EXIT);

	if((temp1 == 0)&&(temp2 == 0)&& (upm->pp_en == true)) {
		upm6920_set_input_volt_limit(upm, 4900);
		return;
	}

	schedule_delayed_work(&upm->hidd_dwork, msecs_to_jiffies(1000));
	return;
}
static void upm6920_inform_psy_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;
	struct upm6920 *upm = container_of(work, struct upm6920,
								psy_dwork.work);

	if (!upm->psy) {
		upm->psy = power_supply_get_by_name("charger");
		if (!upm->psy) {
			printk("Couldn't get psy\n");
			mod_delayed_work(system_wq, &upm->psy_dwork,
					msecs_to_jiffies(2*1000));
			return;
		}
	}

	if (upm->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;

	propval.intval = upm->psy_usb_type;

	ret = power_supply_set_property(upm->psy, POWER_SUPPLY_PROP_USB_TYPE,
					&propval);
	if (ret < 0)
		printk("inform power supply online failed:%d\n", ret);

	propval.intval = upm->psy_desc.type;

	ret = power_supply_set_property(upm->psy,
					POWER_SUPPLY_PROP_TYPE,
					&propval);
	if (ret < 0)
		printk("inform power supply charge type failed:%d\n", ret);

	return;
}

static int upm6920_charging(struct charger_device *chg_dev, bool enable)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val = 0;
	int chg_type;

	if (enable)
		ret = upm6920_enable_charger(upm);
	else
		ret = upm6920_disable_charger(upm);

	chg_debug(" %s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = upm6920_read_byte(upm, UPM6920_REG_03, &val);
	if (!ret)
		upm->charge_enabled = !!(val & UPM6920_CHG_CONFIG_MASK);

	upm->chg_config = enable;


	if(upm->chg_config){
	    if(upm->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP && re_try >= 1){
	        re_try--;
#ifdef CUSTOM_BC12_EFFECT
	        cust_chg_enable_bc12(upm, true);
	        upm6920_force_dpdm(upm);
#endif
	        upm6920_get_bc12_type(upm, &chg_type);
	        if(upm->psy_usb_type != POWER_SUPPLY_USB_TYPE_SDP){
	            power_supply_changed(upm->psy);
	        }
	    }
	}

	return ret;
}

static int upm6920_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;

	ret = upm6920_charging(chg_dev, true);
	
	if (ret) {
		chg_err("Failed to enable charging:%d", ret);
	}

	return ret;
}

static int upm6920_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	ret = upm6920_charging(chg_dev, false);

	if (ret) {
		chg_err("Failed to disable charging:%d", ret);
	}

	ret = upm6920_enter_hiz_mode(upm);
    if (ret) {
        printk("Failed to enter hiz mode\n");
	}
	msleep(140);
	ret = upm6920_exit_hiz_mode(upm);
    if (ret) {
        printk("Failed to exit hiz mode\n");
	}

	return ret;
}

static int upm6920_dump_register(struct charger_device *chg_dev)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	upm6920_dump_regs(upm);

	return 0;
}

static int upm6920_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	*en = upm->charge_enabled;

	return 0;
}

static int _upm6920_get_ichg(struct upm6920 *upm, u32 *curr)
{
	u8 reg_val = 0;
	int ichg = 0;
	int ret = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_04, &reg_val);
	if (!ret) {
		ichg = (reg_val & UPM6920_ICHG_MASK) >> UPM6920_ICHG_SHIFT;
		ichg = ichg * UPM6920_ICHG_LSB + UPM6920_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int upm6920_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
        struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
        int ret;

        ret = _upm6920_get_ichg(upm, curr);
        return ret;
}

static int upm6920_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	chg_debug("charge curr = %d", curr);
	return upm6920_set_chargecurrent(upm, curr / 1000);
}

static int upm6920_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	
	return upm6920_get_input_current_limit(upm, curr);

}

static int upm6920_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	chg_debug("indpm curr = %d", curr);

	return upm6920_set_input_current_limit(upm, curr / 1000);
}

static int upm6920_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

    return upm6920_get_chargevol(upm, volt);
	
}

static int upm6920_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	chg_debug("charge volt = %d", volt);

	return upm6920_set_chargevolt(upm, volt / 1000);
}

static int upm6920_kick_wdt(struct charger_device *chg_dev)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	return upm6920_reset_watchdog_timer(upm);
}

static int upm6920_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	int ret = 0;
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	chg_debug("vindpm volt = %d", volt);
//jnier add start 20240606
	if (!upm->pp_en) {
		dev_info(upm->dev, "%s: power path is disabled\n", __func__);
		upm6920_set_input_volt_limit(upm, UPM6920_MIVR_MAX / 1000);
		return ret;
	}
	upm->mivr=volt; 
//jnier add end 20240606
	return upm6920_set_input_volt_limit(upm, volt / 1000);

}

static int upm6920_get_ivl(struct charger_device *chg_dev, u32 *volt)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	return upm6920_get_input_volt_limit(upm, volt);
}

static int upm6920_get_vbus_adc(struct charger_device *chg_dev, u32 *vbus)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
    
    return upm6920_adc_read_vbus_volt(upm, vbus);
}

static int upm6920_get_ibus_adc(struct charger_device *chg_dev, u32 *ibus)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
    
    return upm6920_adc_read_charge_current(upm, ibus);
}

static int upm6920_set_ieoc(struct charger_device *chg_dev, u32 ieoc)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
    
    return upm6920_set_term_current(upm, ieoc / 1000);
}

static int upm6920_enable_te(struct charger_device *chg_dev, bool en)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

    return upm6920_enable_term(upm, en);
}



static int upm6920_enable_hz(struct charger_device *chg_dev, bool en)
{
   struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

    return upm6920_set_hiz_mode(upm, en);
}

//jnier add start 20240605
static int upm6920_enable_power_path(struct charger_device *chg_dev, bool en)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	upm->pp_en = en;
	dev_info(upm->dev, "%s: en = %d, pp_en = %d\n",__func__, en, upm->pp_en);
	return upm6920_set_ivl(chg_dev, en ? upm->mivr : UPM6920_MIVR_MAX);
}

static int upm6920_is_power_path_enable(struct charger_device *chg_dev, bool *en)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

	dev_info(upm->dev, "%s: pp_en = %d\n",__func__, upm->pp_en);
	*en = upm->pp_en;
	return 0;
}
//jnier add end 20240605

#if 0
static int upm6920_send_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
    int ret;
    int i;

    chg_debug("%s: %s\n", __func__, is_increase ?
            "pumpx up" : "pumpx dn");

    upm6920_set_chargecurrent(upm, 1000); //Leo add for default 20240605
    //pumpx start
    ret = upm6920_set_input_current_limit(upm, 100);
    if (ret)
        return ret;
    msleep(10);

    for (i = 0; i < 6; i++) {
        if (i < 3) {
            upm6920_set_input_current_limit(upm, 800);
            is_increase ? msleep(100) : msleep(300);
        } else {
            upm6920_set_input_current_limit(upm, 800);
            is_increase ? msleep(300) : msleep(100);
        }
        upm6920_set_input_current_limit(upm, 100);
        msleep(100);
    }

    //pumpx stop
    upm6920_set_input_current_limit(upm, 800);
    msleep(500);
    //pumpx wdt, max 240ms
    upm6920_set_input_current_limit(upm, 100);
    msleep(100);

    return upm6920_set_input_current_limit(upm, 1500);
}
#endif

static int upm6920_send_ta20_current_pattern(struct charger_device *chg_dev, u32 uV)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    int i;

    upm6920_enable_ico(upm, 0);//Leo 20240605
    upm6920_set_chargecurrent(upm, 1000); //Leo add for default 20240605

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }
    
    val = (uV - 5500000) / 500000;
    
    chg_debug("%s ta20 vol=%duV, val=%d\n", __func__, uV, val);
    
    upm6920_set_input_current_limit(upm, 100);
    mdelay(150);
    for (i = 4; i >= 0; i--) {
        upm6920_set_input_current_limit(upm, 800);
        (val & (1 << i)) ? msleep(100) : msleep(50);
        upm6920_set_input_current_limit(upm, 100);
        (val & (1 << i)) ? msleep(50) : msleep(100);
    }
    upm6920_set_input_current_limit(upm, 800);
    mdelay(150);
    upm6920_set_input_current_limit(upm, 100);
    mdelay(120);


    //upm6920_enable_ico(upm, 1);//Leo 20240605

    return upm6920_set_input_current_limit(upm, 800);
}

static int upm6920_set_ta20_reset(struct charger_device *chg_dev)
{
    struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
    int curr;
    int ret;

    return 0; //not dynamic voltage 20240605

    ret = upm6920_get_input_current_limit(upm, &curr);

    ret = upm6920_set_input_current_limit(upm, 100);
    msleep(300);
    return upm6920_set_input_current_limit(upm, curr);
}

#if 0
static int upm6920_check_charge_done(struct upm6920 *upm, bool *done)
{
	int ret = 0;
	u8 val = 0;

	ret = upm6920_read_byte(upm, UPM6920_REG_0B, &val);
	if (!ret) {
		val = val & UPM6920_CHRG_STAT_MASK;
		val = val >> UPM6920_CHRG_STAT_SHIFT;
		*done = (val == UPM6920_CHRG_STAT_CHGDONE);
	}

	return ret;
}
#endif

static int upm6920_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

#if 0
	ret = upm6920_check_charge_done(upm, done);
#else
	union power_supply_propval val;

    ret = power_supply_get_property(upm->psy, POWER_SUPPLY_PROP_STATUS, &val);
    if (ret < 0){
		return ret;
    }

    *done = (val.intval == POWER_SUPPLY_STATUS_FULL);
	dev_info(upm->dev, "%s done =%d\n", __func__,*done);
#endif

	return ret;
}

static int upm6920_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 64 * 1000;

	return 0;
}

static int upm6920_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = upm6920_enable_safety_timer(upm, en);

	return ret;
}

static int upm6920_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = upm6920_read_byte(upm, UPM6920_REG_07, &reg_val);

	if (!ret)
		*en = !!(reg_val & UPM6920_EN_TIMER_MASK);

	return ret;
}

static int upm6920_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);

#if IS_ENABLED(CONFIG_WB_BOARD_M307TCR110)	//Leo 20240929
#if defined(CONFIG_WB_OTG_VBUS_USE_EXT_LDO)
	pr_info("%s usb extern ldo for otg, skip !\n",__func__);
	return 0;
#endif
#endif

    if(gpio_is_valid(upm->drvbus_gpio)){
        gpio_direction_output(upm->drvbus_gpio, en);
    }

	if (en) {
		upm6920_disable_charger(upm);
		ret = upm6920_enable_otg(upm);
	} else {
		ret = upm6920_disable_otg(upm);
		upm6920_enable_charger(upm);
	}
	if (!ret)
		upm->otg_enable = en;

	chg_debug(" %s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	return ret;
}

static int upm6920_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct upm6920 *upm = dev_get_drvdata(&chg_dev->dev);
	int ret;

	chg_debug("otg curr = %d", curr);

	ret = upm6920_set_boost_current(upm, curr / 1000);

	return ret;
}

static int upm6920_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct upm6920 *upm = dev_get_drvdata(&chgdev->dev);

	switch (event) {
	case EVENT_FULL:
			charger_dev_notify(chgdev, CHARGER_DEV_NOTIFY_EOC);
			break;
	case EVENT_RECHARGE:
			charger_dev_notify(chgdev, CHARGER_DEV_NOTIFY_RECHG);
			break;
	case EVENT_DISCHARGE:
			break;
	default:
			break;
	}
	power_supply_changed(upm->psy);
	return 0;
}

static enum power_supply_usb_type upm6920_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property upm6920_chg_psy_properties[] = {
    POWER_SUPPLY_PROP_MANUFACTURER,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
    POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_CURRENT_MAX,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int upm6920_chg_property_is_writeable(struct power_supply *psy,
                        enum power_supply_property psp)
{
    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_ENERGY_EMPTY:
        return 1;
    default:
        return 0;
    }
    return 0;
}

static int upm6920_chg_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    int ret = 0;
    u32 _val;
    int data;
    struct upm6920 *upm = power_supply_get_drvdata(psy);
    u8 reg_val;
    bool upm6920_online;
    int adc_vbus_vol;

    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = "upm6920";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
		val->intval = upm->vbus_attach;
        break;
    case POWER_SUPPLY_PROP_STATUS:
        ret = upm6920_read_byte(upm, UPM6920_REG_11, &reg_val);
        upm6920_online = !!(reg_val & UPM6920_VBUS_GD_MASK);
        if(!upm6920_online){
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
        }
        ret = upm6920_get_charge_stat(upm, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = upm6920_get_chargevol(upm, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = upm6920_get_input_current_limit(upm, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = upm6920_get_input_volt_limit(upm, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = upm->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = upm->psy_desc.type;
        break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
		upm6920_adc_read_vbus_volt(upm, &adc_vbus_vol);
		//printk("----POWER_SUPPLY_PROP_CURRENT_MAX adc_vbus_vol=%d---\n",adc_vbus_vol);
		if (adc_vbus_vol/1000 > 7000) {
    		val->intval = 2000000;
    	}else{
    		val->intval = 500000;
    	}
    	break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
    	val->intval = 5000000;
    	break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int upm6920_chg_set_property(struct power_supply *psy,
                enum power_supply_property psp,
                const union power_supply_propval *val)
{
    int ret = 0;
    struct upm6920 *upm = power_supply_get_drvdata(psy);

    switch (psp) {
#ifdef CUSTOM_BC12_ENABLE
    case POWER_SUPPLY_PROP_ONLINE:
        atomic_set(&upm->attach, val->intval);
        upm6920_force_dpdm(upm);
        break;
#endif
    case POWER_SUPPLY_PROP_STATUS:
        if (val->intval) {
            ret = upm6920_enable_charger(upm);
        } else {
            ret = upm6920_disable_charger(upm);
        }
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        ret = upm6920_set_chargecurrent(upm, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = upm6920_set_chargevolt(upm, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = upm6920_set_input_current_limit(upm, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = upm6920_set_input_volt_limit(upm, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = upm6920_set_term_current(upm, val->intval / 1000);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static const struct charger_properties upm6920_chg_props = {
	.alias_name = "upm6920",
};

static const struct charger_properties sc8989x_chg_props = {
	.alias_name = "sc8989x",
};

static char *upm6920_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc upm6920_psy_desc = {
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = upm6920_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(upm6920_chg_psy_usb_types),
    .properties = upm6920_chg_psy_properties,
    .num_properties = ARRAY_SIZE(upm6920_chg_psy_properties),
    .property_is_writeable = upm6920_chg_property_is_writeable,
    .get_property = upm6920_chg_get_property,
    .set_property = upm6920_chg_set_property,
};

static int upm6920_chg_init_psy(struct upm6920 *upm)
{
    struct power_supply_config cfg = {
        .drv_data = upm,
        .of_node = upm->dev->of_node,
        .supplied_to = upm6920_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(upm6920_psy_supplied_to),
    };

    memcpy(&upm->psy_desc, &upm6920_psy_desc, sizeof(upm->psy_desc));
    upm->psy_desc.name = dev_name(upm->dev);//"charger";//dev_name(upm->dev);
    upm->psy = devm_power_supply_register(upm->dev, &upm->psy_desc,
                        &cfg);
    return IS_ERR(upm->psy) ? PTR_ERR(upm->psy) : 0;
}



static struct charger_ops upm6920_chg_ops = {
    /* cable plug in/out */
    .plug_in = upm6920_plug_in,
    .plug_out = upm6920_plug_out,
    /* enable */
    .enable = upm6920_charging,
    .is_enabled = upm6920_is_charging_enable,
    /* charging current */
    .set_charging_current = upm6920_set_ichg,
    .get_charging_current = upm6920_get_ichg,
    .get_min_charging_current = upm6920_get_min_ichg,
    /* charging voltage */
    .set_constant_voltage = upm6920_set_vchg,
    .get_constant_voltage = upm6920_get_vchg,
    /* input current limit */
    .set_input_current = upm6920_set_icl,
    .get_input_current = upm6920_get_icl,
    .get_min_input_current = NULL,
    /* MIVR */
    .set_mivr = upm6920_set_ivl,
    .get_mivr = upm6920_get_ivl,
    .get_mivr_state = NULL,
    /* ADC */
    .get_adc = NULL,
    .get_vbus_adc = upm6920_get_vbus_adc,
    .get_ibus_adc = upm6920_get_ibus_adc,
    .get_ibat_adc = NULL,
    .get_tchg_adc = NULL,
    .get_zcv = NULL,
    /* charing termination */
    .set_eoc_current = upm6920_set_ieoc,
    .enable_termination = upm6920_enable_te,
    .reset_eoc_state = NULL,
    .safety_check = NULL,
    .is_charging_done = upm6920_is_charging_done,
    /* power path */
    .enable_powerpath = upm6920_enable_power_path,
    .is_powerpath_enabled = upm6920_is_power_path_enable,
    /* timer */
    .enable_safety_timer = upm6920_set_safety_timer,
    .is_safety_timer_enabled = upm6920_is_safety_timer_enabled,
    .kick_wdt = upm6920_kick_wdt,
    /* AICL */
    .run_aicl = NULL,
    /* PE+/PE+20 */
    .send_ta_current_pattern = NULL,//upm6920_send_ta_current_pattern,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = upm6920_send_ta20_current_pattern,
    .reset_ta = upm6920_set_ta20_reset,
    .enable_cable_drop_comp = NULL,
    /* OTG */
    .set_boost_current_limit = upm6920_set_boost_ilmt,
    .enable_otg = upm6920_set_otg,
    .enable_discharge = upm6920_set_otg,
    /* charger type detection */
    .enable_chg_type_det = NULL,
    /* misc */
    .dump_registers = upm6920_dump_register,
    .enable_hz = upm6920_enable_hz,
    /* event */
    .event = upm6920_do_event,
};

static struct of_device_id upm6920_charger_match_table[] = {
	{.compatible = "ti,bq25890h",},
	{.compatible = "upm,upm6920",},
	{},
};
MODULE_DEVICE_TABLE(of, upm6920_charger_match_table);

static int upm6920_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct upm6920 *upm;
	struct device_node *node = client->dev.of_node;
	int ret = 0;

	chg_debug("upm6920 probe enter");
	upm = devm_kzalloc(&client->dev, sizeof(struct upm6920), GFP_KERNEL);
	if (!upm)
		return -ENOMEM;

	upm->dev = &client->dev;
	upm->client = client;
	i2c_set_clientdata(client, upm);
	upm->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
	upm->pp_en = true;//jnier add 20240606
	atomic_set(&upm->charger_suspended, 0);

	mutex_init(&upm->i2c_rw_lock);

	ret = upm6920_detect_device(upm);
	if (ret) {
		chg_err("No upm6920 device found!\n");
		ret = -ENODEV;
		goto err_nodev;
	}

	upm->platform_data = upm6920_parse_dt(node, upm);
	if (!upm->platform_data) {
		chg_err("No platform data provided.\n");
		ret = -EINVAL;
		goto err_parse_dt;
	}

    ret = upm6920_chg_init_psy(upm);
    if (ret < 0) {
        printk("failed to init power supply\n");
        return -EINVAL;
    }

	INIT_DELAYED_WORK(&upm->psy_dwork, upm6920_inform_psy_dwork_handler);
	INIT_DELAYED_WORK(&upm->probe_dwork, upm6920_inform_probe_dwork_handler);
	INIT_DELAYED_WORK(&upm->hidd_dwork, upm6920_hidden_mode_work_handler);

#ifdef CUSTOM_BC12_ENABLE
	INIT_DELAYED_WORK(&upm->force_detect_dwork, upm6920_force_detection_dwork_handler);
#endif

	init_waitqueue_head(&upm->hvdcp_wqueue);
	atomic_set(&upm->hvdcp_detect_trig, 0);
	upm->hvdcp_detect_ws = wakeup_source_register(upm->dev, "upm6920_hvdcp_detect_ws");
	upm->hvdcp_detect_task =
		kthread_run(upm6920_hvdcp_detect_thread, upm, "upm6920_hvdcp_detect_thread");
		if (IS_ERR_OR_NULL(upm->hvdcp_detect_task)) {
			ret = PTR_ERR(upm->hvdcp_detect_task);
			chg_err("create kthread fail, ret:%d\n", ret);
			goto err_kthread_run;
		}

	ret = upm6920_init_device(upm);
	if (ret) {
		chg_err("Failed to init device\n");
		goto err_init;
	}

	ret = upm6920_register_interrupt(upm);
	if (ret) {
		chg_err("Failed to register irq ret=%d\n", ret);
		goto err_irq;
	}

	if(upm->part_no == UPM6920_PN){
		printk("upm6920 detected!!!\n");
		upm->sc8989x_charger = false;
	}else if(upm->part_no == SC8989X_PN){
		printk("sc8989x detected!!!\n");
		upm->sc8989x_charger = true;
	}


//#ifdef CONFIG_MTK_CHARGER
	if(upm->sc8989x_charger){
		upm->chg_dev = charger_device_register(upm->chg_dev_name,
						      &client->dev, upm,
						      &upm6920_chg_ops,
						      &sc8989x_chg_props);
	}else{
		upm->chg_dev = charger_device_register(upm->chg_dev_name,
						      &client->dev, upm,
						      &upm6920_chg_ops,
						      &upm6920_chg_props);
	}

	if (IS_ERR_OR_NULL(upm->chg_dev)) {
		ret = PTR_ERR(upm->chg_dev);
		goto err_device_register;
	}

	if (ret) {
		chg_err("failed to register sysfs. err: %d\n", ret);
		goto err_sysfs_create;
	}
//#endif

	ret = sysfs_create_group(&upm->dev->kobj, &upm6920_attr_group);
	if (ret) {
		chg_err("failed to register sysfs. err: %d\n", ret);
	}
	mod_delayed_work(system_wq, &upm->probe_dwork,
					msecs_to_jiffies(2*1000));

	chg_err("upm6920 probe success, Part Num:%d, Revision:%d\n",
		upm->part_no, upm->revision);

	return 0;

//#ifdef CONFIG_MTK_CHARGER
err_sysfs_create:
	charger_device_unregister(upm->chg_dev);
err_device_register:
//#endif

err_irq:
err_init:
	if (upm->hvdcp_detect_task) {
		kthread_stop(upm->hvdcp_detect_task);
	}
err_kthread_run:
	wakeup_source_unregister(upm->hvdcp_detect_ws);
err_parse_dt:
err_nodev:
	mutex_destroy(&upm->i2c_rw_lock);
	devm_kfree(upm->dev, upm);
	return ret;

}

#ifdef CONFIG_PM_SLEEP
static int upm6920_pm_suspend(struct device *dev)
{
	struct upm6920 *upm = NULL;
	struct i2c_client *client = to_i2c_client(dev);

	chg_info(" suspend start \n");
	if (client) {
		upm = i2c_get_clientdata(client);
		if (upm) {
			chg_err(" set charger_suspended as 1\n");
			atomic_set(&upm->charger_suspended, 1);
		}
	}
	return 0;
}

static int upm6920_pm_resume(struct device *dev)
{
	struct upm6920 *upm = NULL;
	struct i2c_client *client = to_i2c_client(dev);

	chg_info(" suspend stop \n");
	if (client) {
		upm = i2c_get_clientdata(client);
		if (upm) {
			chg_err(" set charger_suspended as 0\n");
			atomic_set(&upm->charger_suspended, 0);
		}
	}
	return 0; 
}

static const struct dev_pm_ops upm6920_pm_ops = {
        .resume                 = upm6920_pm_resume,
        .suspend                = upm6920_pm_suspend,
};
#endif /*CONFIG_PM_SLEEP*/

static int upm6920_charger_remove(struct i2c_client *client)
{
	struct upm6920 *upm = i2c_get_clientdata(client);

	if (upm->hvdcp_detect_task) {
		kthread_stop(upm->hvdcp_detect_task);
	}

	wakeup_source_unregister(upm->hvdcp_detect_ws);
	upm6920_enter_hiz_mode(upm);
	msleep(50);
	upm6920_reset_chip(upm);
	mutex_destroy(&upm->i2c_rw_lock);
	sysfs_remove_group(&upm->dev->kobj, &upm6920_attr_group);

	return 0;
}

static void upm6920_charger_shutdown(struct i2c_client *client)
{

	struct upm6920 *upm = i2c_get_clientdata(client);
#if 1 //Leo add for reset PE20
	int curr;
	int ret;

	disable_irq(upm->client->irq);
	pr_info("Leo %s \n",__func__);

	ret = upm6920_get_input_current_limit(upm, &curr);
	
	ret = upm6920_set_input_current_limit(upm, 100);
	msleep(300);
	upm6920_set_input_current_limit(upm, curr);
	msleep(500);
#endif

	upm6920_enter_hiz_mode(upm);
	msleep(30);
	upm6920_exit_hiz_mode(upm);
	msleep(30);
	upm6920_adc_stop(upm);

}

static const struct i2c_device_id upm6920_i2c_device_id[] = {
	{ "bq25890h", 0x03 },
	{ "upm6920", 0x03 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, upm6920_i2c_device_id);

static struct i2c_driver upm6920_charger_driver = {
	.driver = {
	   .name = "upm6920-charger",
	   .owner = THIS_MODULE,
	   .of_match_table = upm6920_charger_match_table,
	   .pm = &upm6920_pm_ops,
	},

	.probe = upm6920_charger_probe,
	.remove = upm6920_charger_remove,
	.shutdown = upm6920_charger_shutdown,
	.id_table = upm6920_i2c_device_id,

};

module_i2c_driver(upm6920_charger_driver);


MODULE_DESCRIPTION("Unisemi UPM6920 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Unisemipower");
