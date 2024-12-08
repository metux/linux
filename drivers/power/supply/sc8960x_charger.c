// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#define pr_fmt(fmt)	"[sc8960x]:%s: " fmt, __func__

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
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/of_irq.h>
#include <linux/phy/phy.h>
//#include <mt-plat/v1/charger_type.h>

//#include "mtk_charger_intf.h"
#include "charger_class.h"
#include "mtk_charger.h"
#include "sc8960x_reg.h"
#include "sc8960x.h"
#include <tcpm.h>

#define SC89601D_1P0

#ifndef PHY_MODE_BC11_SET
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
#endif

#define WB_IRQ_HANDLER_DELAY_SUPPORT

#if IS_ENABLED(CONFIG_CM_MIDMISC_SUPPORT) //Leo 20230912
#include <mt-plat/middle_misc.h>
extern int cust_mid_misc_get_boot_mode(void);
#endif

static int sc8960x_set_safety_timer(struct charger_device *chg_dev, bool en);
static int sc8960x_is_charging_enable(struct charger_device *chg_dev, bool *en);

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

enum {
	PN_SC89601D,
};

enum sc8960x_part_no {
	SC89601D = 0x03,
	ETA6963 = 0x07,
};

static int pn_data[] = {
	[PN_SC89601D] = 0x03,
};

static char *pn_str[] = {
	[PN_SC89601D] = "sc89601d",
};

struct sc8960x {
	struct device *dev;
	struct i2c_client *client;

	enum sc8960x_part_no part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	int irq_gpio;

	struct delayed_work psy_dwork;
	struct delayed_work request_irq_dwork;
	struct workqueue_struct *sc8690x_request_irq_wq;

	int input_curr_limit;
	enum power_supply_type chg_type;
	enum power_supply_usb_type psy_usb_type;

	struct power_supply_desc psy_desc;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;

	struct sc8960x_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
	bool vbus_good;
	struct delayed_work force_detect_dwork;
	u8 force_detect_count;
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230625
	struct power_supply *cp_psy;
	struct power_supply *bat_psy;
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	int is_pd_adapter;
#endif
	atomic_t attach;
	bool eta6963_en;
};

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20231019
struct sc8960x *g_sc = NULL;
int sc8960x_get_uisoc(struct sc8960x *sc);
#endif

static const struct charger_properties sc8960x_chg_props = {
	.alias_name = "sc8960x",
};

static int __sc8960x_read_reg(struct sc8960x *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8960x_write_reg(struct sc8960x *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			   val, reg, ret);
		return ret;
	}
	return 0;
}

static int sc8960x_read_byte(struct sc8960x *sc, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_read_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8960x_write_byte(struct sc8960x *sc, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_write_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sc8960x_update_bits(struct sc8960x *sc, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8960x_read_reg(sc, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8960x_write_reg(sc, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

static int sc8960x_set_key1(struct sc8960x *sc)
{
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY1);
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY2);
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY3);
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY4);
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY5);
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY6);
	sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY7);
	return sc8960x_write_byte(sc, SC8960X_REG_7F, REG7F_KEY8);
}

static int sc8960x_set_wa(struct sc8960x *sc)
{
	int ret;
	u8 reg_val;
	
	ret = sc8960x_read_byte(sc, SC8960X_REG_99, &reg_val);
	if (ret) {
		sc8960x_set_key1(sc);
	}
	sc8960x_write_byte(sc, SC8960X_REG_92, REG92_PFM_VAL);
	
	return sc8960x_set_key1(sc);
}

static int sc8960x_set_vtc_vol(struct sc8960x *sc, int vol)
{
	u8 val = vol << REG0E_VTC_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_0E, REG0E_VTC_MASK,
				   val);
}

static int sc8960x_set_ntc(struct sc8960x *sc, int vol)
{
	u8 val = vol << REG0E_NTC_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_0E, REG0E_VTC_MASK,
				   val);
}

static int sc8960x_enable_otg(struct sc8960x *sc)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}

static int sc8960x_disable_otg(struct sc8960x *sc)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}

static int sc8960x_enable_charger(struct sc8960x *sc)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
		sc8960x_update_bits(sc, SC8960X_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int sc8960x_disable_charger(struct sc8960x *sc)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
		sc8960x_update_bits(sc, SC8960X_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int sc8960x_set_chargecurrent(struct sc8960x *sc, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
	return sc8960x_update_bits(sc, SC8960X_REG_02, REG02_ICHG_MASK,
				   ichg << REG02_ICHG_SHIFT);

}

int sc8960x_get_term_current(struct sc8960x *sc, int *curr)
{
	u8 reg_val;
	int iterm;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_03, &reg_val);
	if (!ret) {
		iterm = (reg_val & REG03_ITERM_MASK) >> REG03_ITERM_SHIFT;
		iterm = iterm * REG03_ITERM_LSB + REG03_ITERM_BASE;

		*curr = iterm * 1000;
	}
	return ret;
}

int sc8960x_set_term_current(struct sc8960x *sc, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_term_current);

int sc8960x_set_prechg_current(struct sc8960x *sc, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return sc8960x_update_bits(sc, SC8960X_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sc8960x_set_prechg_current);

int sc8960x_get_chargevol(struct sc8960x *sc, int *volt)
{
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}
	return ret;
}

int sc8960x_set_chargevolt(struct sc8960x *sc, int volt)
{
	u8 val;
	u8 vreg_ft;
	int ret;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	if (((volt - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 1) {
		volt -= 8;
		vreg_ft = REG0D_VREG_FT_INC8MV;
	}
	else if (((volt - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 2) {
		volt -= 16;
		vreg_ft = REG0D_VREG_FT_INC16MV;
	}
	else if (((volt - REG04_VREG_BASE) % REG04_VREG_LSB) / 8 == 3) {
		volt -= 24;
		vreg_ft = REG0D_VREG_FT_INC24MV;
	}
	else
		vreg_ft = REG0D_VREG_FT_DEFAULT;

	val = (volt - REG04_VREG_BASE ) / REG04_VREG_LSB;

	ret = sc8960x_update_bits(sc, SC8960X_REG_04, REG04_VREG_MASK,
				   val << REG04_VREG_SHIFT);
	if (ret) {
		dev_err(sc->dev, "%s: failed to set charger volt\n", __func__);
		return ret;
	}

	ret = sc8960x_update_bits(sc, SC8960X_REG_0D, REG0D_VBAT_REG_FT_MASK,
					vreg_ft << REG0D_VBAT_REG_FT_SHIFT);
	if (ret) {
		dev_err(sc->dev, "%s: failed to set charger volt ft\n", __func__);
		return ret;
	}

	return 0;
}

int sc8960x_get_input_volt_limit(struct sc8960x *sc, u32 *volt)
{
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_06, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
		vchg = vchg * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
		*volt = vchg * 1000;
	}
	return ret;
}

int sc8960x_set_input_volt_limit(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_VINDPM_MASK,
				   val << REG06_VINDPM_SHIFT);
}

int sc8960x_get_input_current_limit(struct  sc8960x *sc, u32 *curr)
{
	u8 reg_val;
	int icl;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;
}

int sc8960x_set_input_current_limit(struct sc8960x *sc, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_IINLIM_MASK,
				   val << REG00_IINLIM_SHIFT);
}

int sc8960x_set_watchdog_timer(struct sc8960x *sc, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
			   REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(sc8960x_set_watchdog_timer);

int sc8960x_disable_watchdog_timer(struct sc8960x *sc)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sc8960x_disable_watchdog_timer);

int sc8960x_reset_watchdog_timer(struct sc8960x *sc)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_01, REG01_WDT_RESET_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(sc8960x_reset_watchdog_timer);

int sc8960x_reset_chip(struct sc8960x *sc)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
		sc8960x_update_bits(sc, SC8960X_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(sc8960x_reset_chip);

int sc8960x_enter_hiz_mode(struct sc8960x *sc)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc8960x_enter_hiz_mode);

int sc8960x_exit_hiz_mode(struct sc8960x *sc)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sc8960x_exit_hiz_mode);

static int sc8960x_enable_hz(struct charger_device *chg_dev, bool en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en == true) {
		ret = sc8960x_enter_hiz_mode(sc);
	} else {
		disable_irq_nosync(sc->irq);
		mdelay(5);
		ret = sc8960x_exit_hiz_mode(sc);
		mdelay(20);
		enable_irq(sc->irq);
	}

	return ret;
}

static int sc8960x_get_hz(struct charger_device *chg_dev, bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &reg_val);
	if (!ret) {
		*en = (reg_val & 0x80);
		pr_info("%s get hz status:%d !\n",__func__, *en);
	} else {
		pr_err("%s get hz status failed , set default true !\n");
		*en = true;
	}

	return ret;
}

// static int sc8960x_enable_term(struct sc8960x *sc, bool enable)
// {
// 	u8 val;
// 	int ret;

// 	if (enable)
// 		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
// 	else
// 		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

// 	ret = sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TERM_MASK, val);

// 	return ret;
// }
// EXPORT_SYMBOL_GPL(sc8960x_enable_term);

int sc8960x_set_boost_current(struct sc8960x *sc, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;


	if (curr >= BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return sc8960x_update_bits(sc, SC8960X_REG_02, REG02_BOOST_LIM_MASK,
				   val << REG02_BOOST_LIM_SHIFT);
}

int sc8960x_set_boost_voltage(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt == BOOSTV_4700)
		val = REG06_BOOSTV_4P7V;
	else if (volt == BOOSTV_5100)
		val = REG06_BOOSTV_5P1V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_4P9V;

	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_BOOSTV_MASK,
				   val << REG06_BOOSTV_SHIFT);
}

static int sc8960x_set_acovp_threshold(struct sc8960x *sc, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14200)
		val = REG06_OVP_14P2V;
	else if (volt == VAC_OVP_11000)
		val = REG06_OVP_11V;
	else if (volt == VAC_OVP_6400)
		val = REG06_OVP_6P4V;
	else
		val = REG06_OVP_5P8V;

	return sc8960x_update_bits(sc, SC8960X_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}

static int sc8960x_set_stat_ctrl(struct sc8960x *sc, int ctrl)
{
	u8 val;

	val = ctrl;

	return sc8960x_update_bits(sc, SC8960X_REG_00, REG00_STAT_CTRL_MASK,
				   val << REG00_STAT_CTRL_SHIFT);
}

static int sc8960x_set_int_mask(struct sc8960x *sc, int mask)
{
	u8 val;

	val = mask;

	return sc8960x_update_bits(sc, SC8960X_REG_0A, REG0A_INT_MASK_MASK,
				   val << REG0A_INT_MASK_SHIFT);
}

// static int sc8960x_enable_batfet(struct sc8960x *sc)
// {
// 	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

// 	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK,
// 				   val);
// }
// EXPORT_SYMBOL_GPL(sc8960x_enable_batfet);

//static int sc8960x_disable_batfet(struct sc8960x *sc)
//{
//	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
//
//	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DIS_MASK,
//				   val);
//}
//EXPORT_SYMBOL_GPL(sc8960x_disable_batfet);

//static int sc8960x_set_batfet_delay(struct sc8960x *sc, uint8_t delay)
//{
//	u8 val;
//
//	if (delay == 0)
//		val = REG07_BATFET_DLY_0S;
//	else
//		val = REG07_BATFET_DLY_10S;
//
//	val <<= REG07_BATFET_DLY_SHIFT;
//
//	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_BATFET_DLY_MASK,
//				   val);
//}
//EXPORT_SYMBOL_GPL(sc8960x_set_batfet_delay);

static int sc8960x_enable_safety_timer(struct sc8960x *sc)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(sc8960x_enable_safety_timer);

static int sc8960x_disable_safety_timer(struct sc8960x *sc)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return sc8960x_update_bits(sc, SC8960X_REG_05, REG05_EN_TIMER_MASK,
				   val);
}

static struct sc8960x_platform_data *sc8960x_parse_dt(struct device_node *np,
							  struct sc8960x *sc)
{
	int ret;
	struct sc8960x_platform_data *pdata;

	pdata = devm_kzalloc(sc->dev, sizeof(struct sc8960x_platform_data),
				 GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
		sc->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &sc->eint_name) < 0) {
		sc->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	sc->chg_det_enable =
		of_property_read_bool(np, "sc,sc8960x,charge-detect-enable");

	sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
	if (sc->irq_gpio < 0){
		pr_err("sc,intr-gpio is not available\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of sc,sc8960x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of sc,sc8960x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of sc,sc8960x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of sc,sc8960x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of sc,sc8960x,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of sc,sc8960x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
			("Failed to read node of sc,sc8960x,termination-current\n");
	}

	ret =
		of_property_read_u32(np, "sc,sc8960x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of sc,sc8960x,boost-voltage\n");
	}

	ret =
		of_property_read_u32(np, "sc,sc8960x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of sc,sc8960x,boost-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8960x,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of sc,sc8960x,vac-ovp-threshold\n");
	}

#if 0//defined(M101TB_DG_PT2_531) //Leo 20230912
#if IS_ENABLED(CONFIG_CM_MIDMISC_SUPPORT) //Leo 20230912
	if ((cust_mid_misc_get_boot_mode() == 8)
			||(cust_mid_misc_get_boot_mode() == 9)) {
		pdata->iterm = 100;
	}
#endif
#endif

	return pdata;
}

static int sc8960x_force_dpdm(struct sc8960x *sc)
{
	return sc8960x_update_bits(sc, SC8960X_REG_07, REG07_FORCE_DPDM_MASK,
						REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT);
}

static int cust_chg_set_usbsw(struct sc8960x *ddata,
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

static int cust_chg_enable_bc12(struct sc8960x *ddata, bool en)
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
				dev_notice(ddata->dev, "%s: change attach:%d, disable bc12\n",
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
	ret = cust_chg_set_usbsw(ddata, en ? USBSW_CHG : USBSW_USB);

	return ret;
}

static void sc8960x_force_detection_dwork_handler(struct work_struct *work)
{
	int ret;
	struct sc8960x *sc = container_of(work, struct sc8960x, force_detect_dwork.work);

	cust_chg_enable_bc12(sc, true);

	ret = sc8960x_force_dpdm(sc);
	if (ret) {
		dev_err(sc->dev, "%s: failed to force detection\n", __func__);
		return;
	}
	sc->power_good = false;

	sc->force_detect_count++;
}

static int sc8960x_get_charger_type(struct sc8960x *sc)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);

	if (ret)
		return ret;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		//sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		//sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		//sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		sc->chg_type = POWER_SUPPLY_TYPE_USB;
		break;
	case REG08_VBUS_TYPE_SDP:
		sc->chg_type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case REG08_VBUS_TYPE_CDP:
		sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case REG08_VBUS_TYPE_DCP:
		sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		if (sc->force_detect_count < 10)
			schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(2000));
		break;
	case REG08_VBUS_TYPE_NON_STD:
		sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	default:
		sc->chg_type = POWER_SUPPLY_TYPE_USB;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	}

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20240108
	if (sc->is_pd_adapter == true) {
		sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
	}
#endif

	printk("sc8960x chg_type=%d  psy_usb_type=%d\n", sc->chg_type, sc->psy_usb_type);
	cust_chg_enable_bc12(sc, false);
	
	return 0;
}

static int sc8960x_get_charge_stat(struct sc8960x *sc, int *state)
{
	int ret;
	u8 val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;

		switch (val)
		{
		case REG08_CHRG_STAT_IDLE:
			if (sc->vbus_good) { //Leo 20230817
				*state = POWER_SUPPLY_STATUS_CHARGING;
			} else {
				*state = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
		case REG08_CHRG_STAT_PRECHG:
		case REG08_CHRG_STAT_FASTCHG:
			*state = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case REG08_CHRG_STAT_CHGDONE:
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

static void sc8960x_inform_psy_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;
	union power_supply_propval prop_type = {0};
	union power_supply_propval prop_usb_type = {0};

	struct sc8960x *sc = container_of(work, struct sc8960x,
								psy_dwork.work);

	if (!sc->psy) {
		sc->psy = power_supply_get_by_name("charger");
		if (!sc->psy) {
			pr_err("%s get power supply fail\n", __func__);
			mod_delayed_work(system_wq, &sc->psy_dwork, 
					msecs_to_jiffies(2000));
			return ;
		}
	}

	if (sc->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;
	
	ret = power_supply_set_property(sc->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	
	if (ret < 0)
		pr_notice("inform power supply online failed:%d\n", ret);

	prop_usb_type.intval = sc->psy_usb_type;
	ret = power_supply_set_property(sc->psy,
				POWER_SUPPLY_PROP_USB_TYPE, &prop_usb_type);

	prop_type.intval = sc->chg_type;
	ret = power_supply_set_property(sc->psy,
				POWER_SUPPLY_PROP_TYPE, &prop_type);

	if (ret < 0)
		pr_notice("inform power supply charge type failed:%d\n", ret);
	
}

#if defined(WB_IRQ_HANDLER_DELAY_SUPPORT) //Leo 20230915
static void sc8960x_work_irq_handler(struct work_struct *work)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;
	struct sc8960x *sc = container_of(work, struct sc8960x, request_irq_dwork.work);

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);
	if (ret) {
		enable_irq(sc->irq);
		return;
	}

	prev_pg = sc->power_good;

	sc->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	ret = sc8960x_read_byte(sc, SC8960X_REG_0A, &reg_val);
	if (ret) {
		enable_irq(sc->irq);
		return;
	}

	prev_vbus_gd = sc->vbus_good;

	//pr_info("reg:0x0A: val:0x%.2x \n",reg_val);
	sc->vbus_good = !!(reg_val & REG0A_VBUS_GD_MASK);

	if (!prev_vbus_gd && sc->vbus_good) {
		sc->force_detect_count = 0;
		pr_notice("adapter/usb inserted\n");
	} else if (prev_vbus_gd && !sc->vbus_good) {
		pr_notice("adapter/usb removed\n");
		sc8960x_get_charger_type(sc);
		if (0) {
			schedule_delayed_work(&sc->psy_dwork, 0);
		}
		cust_chg_enable_bc12(sc, true);
		power_supply_changed(sc->psy);
		enable_irq(sc->irq);
		return;
	}

	if (!prev_pg && sc->power_good) {
		sc8960x_get_charger_type(sc);
		if (0) {
			schedule_delayed_work(&sc->psy_dwork, 0);
		}
		power_supply_changed(sc->psy);
	}

	enable_irq(sc->irq);
}

static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
	struct sc8960x *sc = (struct sc8960x *)data;

#if IS_ENABLED(CONFIG_CM_MIDMISC_SUPPORT) //Leo 20231116
	disable_irq_nosync(irq);
	if (cust_mid_misc_get_boot_mode() == NORMAL_BOOT) {
		queue_delayed_work(sc->sc8690x_request_irq_wq, &sc->request_irq_dwork, msecs_to_jiffies(300));
	} else {
		sc8960x_work_irq_handler(&sc->request_irq_dwork.work);
	}
	return IRQ_HANDLED;
#else
	disable_irq_nosync(irq);
	queue_delayed_work(sc->sc8690x_request_irq_wq, &sc->request_irq_dwork, msecs_to_jiffies(300));
	return IRQ_HANDLED;
#endif
}
#else
static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_gd;
	struct sc8960x *sc = (struct sc8960x *)data;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = sc->power_good;

	sc->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	ret = sc8960x_read_byte(sc, SC8960X_REG_0A, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_vbus_gd = sc->vbus_good;

	//pr_info("reg:0x0A: val:0x%.2x \n",reg_val);
	sc->vbus_good = !!(reg_val & REG0A_VBUS_GD_MASK);

	if (!prev_vbus_gd && sc->vbus_good) {
		sc->force_detect_count = 0;
		pr_notice("adapter/usb inserted\n");
	} else if (prev_vbus_gd && !sc->vbus_good) {
		pr_notice("adapter/usb removed\n");
		sc8960x_get_charger_type(sc);
		if (0) {
			schedule_delayed_work(&sc->psy_dwork, 0);
		}
		cust_chg_enable_bc12(sc, true);
		power_supply_changed(sc->psy);
		return IRQ_HANDLED;
	}

	if (!prev_pg && sc->power_good) {
		sc8960x_get_charger_type(sc);
		if (0) {
			schedule_delayed_work(&sc->psy_dwork, 0);
		}
		power_supply_changed(sc->psy);
	}

	return IRQ_HANDLED;
}
#endif

static int sc8960x_register_interrupt(struct sc8960x *sc)
{
	int ret = 0;

	ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
	if (ret < 0) {
		pr_err("failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	ret = gpio_direction_input(sc->irq_gpio);
	if (ret < 0) {
		pr_err("failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	sc->irq = gpio_to_irq(sc->irq_gpio);
	if (ret < 0) {
		pr_err("failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
		return ret;
	}

	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc8960x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"sc8960x_irq", sc);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	device_init_wakeup(sc->dev, true);

	return 0;
}

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20240108
static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct sc8960x *sc;
	sc = container_of(pnb, struct sc8960x, pd_nb);

	pr_notice("SC8960x PD charger event:%d %d\n", (int)event,
		(int)noti->pd_state.connected);

	if ((event == 14) && (noti->pd_state.connected == 7)) {
		sc->is_pd_adapter = true;;
	} else if ((event == 14) && (noti->pd_state.connected == 5)) {
		sc->is_pd_adapter = false;
	}

	pr_info("%s: is_pd_adapter:%d \n", __func__,sc->is_pd_adapter);

	return 0;
}

static int sc8960x_chg_init_pd_notifier(struct sc8960x *sc)
{
	int ret;

	sc->is_pd_adapter = -1;
	sc->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (sc->tcpc == NULL) {
		pr_info("%s: failed to get tcpc device\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	sc->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(sc->tcpc, &sc->pd_nb,
				TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC |
				TCP_NOTIFY_TYPE_VBUS);
	if (ret < 0) {
		pr_info("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	return ret;
}
#endif

static void sc8960x_request_irq_handler(struct work_struct *work)
{
	struct sc8960x *sc = container_of(work, struct sc8960x, request_irq_dwork.work);

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20240108
	sc8960x_chg_init_pd_notifier(sc);
#endif

#if defined(WB_IRQ_HANDLER_DELAY_SUPPORT)
	INIT_DELAYED_WORK(&sc->request_irq_dwork, sc8960x_work_irq_handler);
#endif

	sc8960x_register_interrupt(sc);
}

static int sc8960x_init_device(struct sc8960x *sc)
{
	int ret;

	sc8960x_disable_watchdog_timer(sc);

	ret = sc8960x_set_stat_ctrl(sc, sc->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = sc8960x_set_prechg_current(sc, sc->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = sc8960x_set_term_current(sc, sc->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = sc8960x_set_boost_voltage(sc, sc->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = sc8960x_set_boost_current(sc, sc->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = sc8960x_set_acovp_threshold(sc, sc->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret = sc8960x_set_int_mask(sc,
				   REG0A_IINDPM_INT_MASK |
				   REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	ret = sc8960x_set_wa(sc);
	if (ret)
		pr_err("Failed to set private\n");

	ret = sc8960x_set_vtc_vol(sc, REG0E_VTC_3V0);
	if (ret)
		pr_err("Failed to set vtc voltage\n");

	ret = sc8960x_set_ntc(sc, REG0E_NTC_DISABLE);
	if (ret)
		pr_err("Failed to set vtc voltage\n");

	return 0;
}

static void determine_initial_status(struct sc8960x *sc)
{
	sc8960x_irq_handler(sc->irq, (void *) sc);
}

static int sc8960x_detect_device(struct sc8960x *sc)
{
	int ret;
	u8 data;

	ret = sc8960x_read_byte(sc, SC8960X_REG_0B, &data);
	if (!ret) {
		sc->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		sc->revision =
			(data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void sc8960x_dump_regs(struct sc8960x *sc)
{
	int addr;
	u8 val;
	int ret;

	int dump_reg = 0x0E;

	if(sc->eta6963_en){
		dump_reg = 0x0B;
	}

	for (addr = 0x0; addr <= dump_reg; addr++) {
		ret = sc8960x_read_byte(sc, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
sc8960x_show_registers(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sc8960x *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8960x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sc8960x_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					   "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
sc8960x_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sc8960x *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		sc8960x_write_byte(sc, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8960x_show_registers,
		   sc8960x_store_registers);

static struct attribute *sc8960x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sc8960x_attr_group = {
	.attrs = sc8960x_attributes,
};

static int sc8960x_charging(struct charger_device *chg_dev, bool enable)
{

	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = sc8960x_enable_charger(sc);
	else
		ret = sc8960x_disable_charger(sc);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
		   !ret ? "successfully" : "failed");

	ret = sc8960x_read_byte(sc, SC8960X_REG_01, &val);


	if (!ret)
		sc->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int sc8960x_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = sc8960x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int sc8960x_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = sc8960x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int sc8960x_dump_register(struct charger_device *chg_dev)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20240105
	bool is_chr_en, is_hz_en;
#endif

	sc8960x_dump_regs(sc);

#if defined(M101TB_DG_PT2_531) //Leo: 10hous is no enough to charge 20000ma battery 20231016
	sc8960x_set_safety_timer(sc->chg_dev, false);
#endif

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20240105
	sc8960x_is_charging_enable(sc->chg_dev, &is_chr_en);
	sc8960x_get_hz(sc->chg_dev, &is_hz_en);
	if ((is_chr_en == true) && (is_hz_en == true)) {
		sc8960x_enable_hz(sc->chg_dev, false);
		pr_info("sc8960 sw is enabled, so we should exit hiz mode! \n");
	}
#endif

	return 0;
}

static int sc8960x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	*en = sc->charge_enabled;

	return 0;
}

static int sc8960x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int sc8960x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return sc8960x_set_chargecurrent(sc, curr / 1000);
}

static int sc8960x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_02, &reg_val);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int sc8960x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int sc8960x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return sc8960x_set_chargevolt(sc, volt / 1000);
}

static int sc8960x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int sc8960x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return sc8960x_set_input_volt_limit(sc, volt / 1000);

}

static int sc8960x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return sc8960x_set_input_current_limit(sc, curr / 1000);
}

static int sc8960x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int sc8960x_kick_wdt(struct charger_device *chg_dev)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8960x_reset_watchdog_timer(sc);
}

static int sc8960x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		ret = sc8960x_disable_charger(sc);
		ret = sc8960x_enable_otg(sc);
	}
	else {
		ret = sc8960x_disable_otg(sc);
		ret = sc8960x_enable_charger(sc);
	}

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
		   !ret ? "successfully" : "failed");

	return ret;
}

static int sc8960x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

#if defined(M101TB_DG_PT2_531) //Leo 20231016
	en = false;
#endif

	if (en)
		ret = sc8960x_enable_safety_timer(sc);
	else
		ret = sc8960x_disable_safety_timer(sc);

	return ret;
}

static int sc8960x_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sc8960x_read_byte(sc, SC8960X_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int sc8960x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = sc8960x_set_boost_current(sc, curr / 1000);

	return ret;
}

static int sc8960x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);

	if (!sc->psy) {
		dev_notice(sc->dev, "%s: cannot get psy \n", __func__);
		return -ENODEV;
	}

	switch (event) {
#if 1 //Leo 20240105
		case EVENT_FULL:
		case EVENT_RECHARGE:
			power_supply_changed(sc->psy);
			break;
		case EVENT_DISCHARGE:
			power_supply_changed(sc->psy);
			break;
#else
		case EVENT_FULL:
			charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
			break;
		case EVENT_RECHARGE:
			charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
			break;
#endif
		default:
			break;
	}

	return 0;
}

//static int sc8960x_get_hiz_mode(struct charger_device *chg_dev)
//{
//	u8 val;
//	int ret;
//
//	struct sc8960x *sc = dev_get_drvdata(&chg_dev->dev);
//
//	ret = sc8960x_read_byte(sc, SC8960X_REG_00, &val);
//	if (ret)
//		return ret;
//
//	val = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
//
//	pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");
//
//	return val;
//}





static enum power_supply_usb_type sc8960x_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property sc8960x_chg_psy_properties[] = {
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

static int sc8960x_chg_property_is_writeable(struct power_supply *psy,
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

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230625
int sc8960x_get_uisoc(struct sc8960x *sc)
{
	int ret;
	union power_supply_propval prop = {0};
	struct power_supply *bat_psy = NULL;

	bat_psy = sc->bat_psy;
	if (!sc->bat_psy) {
		pr_notice("%s retry to get bat_psy\n", __func__);
		bat_psy = power_supply_get_by_phandle(sc->client->dev.of_node, "gauge");
		sc->bat_psy = bat_psy;
	}

	if (IS_ERR_OR_NULL(bat_psy)) {
		pr_err("%s Couldn't get bat_psy\n", __func__);
		ret = 50;
	} else {
		ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
		ret = prop.intval;
	}

	pr_info("%s:%d\n", __func__,
		ret);
	return ret;
}
#endif

static int sc8960x_chg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;
	u32 _val;
	int data;
	struct sc8960x *sc = power_supply_get_drvdata(psy);
	u8 reg_val = 0;
	int attach;

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230625
	union power_supply_propval online;
	if (IS_ERR_OR_NULL(sc->cp_psy)) {
		sc->cp_psy = power_supply_get_by_phandle(sc->client->dev.of_node, "cp_charger");
		if (IS_ERR_OR_NULL(sc->cp_psy)) {
			pr_err("%s Couldn't get sc->cp_psy\n", __func__);
			sc->cp_psy = power_supply_get_by_name("sc,sc8551-standalone");
			if (IS_ERR_OR_NULL(sc->cp_psy)) {
				pr_err("%s Couldn't get sc,sc8551-standalone sc->cp_psy\n", __func__);
			}
		} else {
			pr_info("%s Get cp_psy successfully! \n",__func__);
		}
	}
#endif

	ret = sc8960x_read_byte(sc, SC8960X_REG_0A, &reg_val);
	if (ret) {
		pr_info("error:%s: read SC8960X_REG_0A failed \n", __func__);
	} else {
		sc->vbus_good = !!(reg_val & REG0A_VBUS_GD_MASK);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "SouthChip";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		//val->intval = sc->vbus_good;
/*
		attach = atomic_read(&sc->attach);
		if (attach == ATTACH_TYPE_TYPEC) {
			sc->vbus_good = true;
		} else {
			sc->vbus_good = false;
		}
*/

		val->intval = sc->vbus_good;

#if 1 //Leo20240108
		attach = atomic_read(&sc->attach);
		//pr_info("Leo attach :%d \n", attach);
		if (attach == 5) {
			val->intval = true;
		}
#endif

		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = sc8960x_get_charge_stat(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230625
		if (sc8960x_get_uisoc(sc) < 95) {
			if (!IS_ERR_OR_NULL(sc->cp_psy)) {
				ret = power_supply_get_property(sc->cp_psy,
					POWER_SUPPLY_PROP_ONLINE, &online);
				if (online.intval) {
					val->intval = POWER_SUPPLY_STATUS_CHARGING;
					pr_info("%s cp_charger work online,set status to charge \n",__func__);
				}
			}
		}

#if 1 //Leo20240108
		attach = atomic_read(&sc->attach);
		//pr_info("Leo attach :%d \n", attach);
		if (attach == 5) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
#endif
#endif
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8960x_get_chargevol(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc8960x_get_input_current_limit(sc, &_val);
		if (ret < 0)
			break;
		val->intval = _val;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sc8960x_get_input_volt_limit(sc, &_val);
		if (ret < 0)
			break;
		val->intval = _val;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8960x_get_term_current(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = sc->psy_usb_type;
#if 1 //Leo20240108
		attach = atomic_read(&sc->attach);
		if (attach == 5) {
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
		}
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (sc->chg_type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (sc->chg_type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sc->chg_type;
#if 1 //Leo20240108
		attach = atomic_read(&sc->attach);
		pr_info("Leo attach :%d \n", attach);
		if (attach == 5) {
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
		}
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sc8960x_chg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;
	struct sc8960x *sc = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		atomic_set(&sc->attach, val->intval);
		sc8960x_force_dpdm(sc);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval) {
			ret = sc8960x_enable_charger(sc);
		} else {
			ret = sc8960x_disable_charger(sc);
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc8960x_set_chargecurrent(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8960x_set_chargevolt(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sc->input_curr_limit = val->intval / 1000;
		ret = sc8960x_set_input_current_limit(sc, sc->input_curr_limit);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sc8960x_set_input_volt_limit(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8960x_set_term_current(sc, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static char *sc8960x_psy_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static const struct power_supply_desc sc8960x_psy_desc = {
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sc8960x_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(sc8960x_chg_psy_usb_types),
	.properties = sc8960x_chg_psy_properties,
	.num_properties = ARRAY_SIZE(sc8960x_chg_psy_properties),
	.property_is_writeable = sc8960x_chg_property_is_writeable,
	.get_property = sc8960x_chg_get_property,
	.set_property = sc8960x_chg_set_property,
};

static int sc8960x_chg_init_psy(struct sc8960x *sc){
	struct power_supply_config cfg = {
		.drv_data = sc,
		.of_node = sc->dev->of_node,
		.supplied_to = sc8960x_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(sc8960x_psy_supplied_to),
	};

	memcpy(&sc->psy_desc, &sc8960x_psy_desc, sizeof(sc->psy_desc));
	sc->psy_desc.name = "charger";//dev_name(sc->dev);
	sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc,
						&cfg);
	return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}

static struct charger_ops sc8960x_chg_ops = {
	/* cable plug in/out */
	.plug_in = sc8960x_plug_in,
	.plug_out = sc8960x_plug_out,
	/* enable */
	.enable = sc8960x_charging,
	.is_enabled = sc8960x_is_charging_enable,
	/* charging current */
	.get_charging_current = sc8960x_get_ichg,
	.set_charging_current = sc8960x_set_ichg,
	/* input current limit */
	.get_input_current = sc8960x_get_icl,
	.set_input_current = sc8960x_set_icl,
	 /* charging voltage */
	.get_constant_voltage = sc8960x_get_vchg,
	.set_constant_voltage = sc8960x_set_vchg,
	.kick_wdt = sc8960x_kick_wdt,
	/* MIVR */
	.set_mivr = sc8960x_set_ivl,
	.is_charging_done = sc8960x_is_charging_done,
	.get_min_charging_current = sc8960x_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = sc8960x_set_safety_timer,
	.is_safety_timer_enabled = sc8960x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = sc8960x_set_otg,
	.set_boost_current_limit = sc8960x_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.event = sc8960x_do_event,

	.enable_hz = sc8960x_enable_hz,
	.get_hz_status = sc8960x_get_hz,

	//.set_hiz_mode = sc8960x_set_hiz_mode,
	//.get_hiz_mode = sc8960x_get_hiz_mode,
	.dump_registers = sc8960x_dump_register,
};

static struct of_device_id sc8960x_charger_match_table[] = {
	{
	 .compatible = "sc,sc89601d",
	 .data = &pn_data[PN_SC89601D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, sc8960x_charger_match_table);

static int sc8960x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct sc8960x *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	int ret = 0;
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20231116
	u8 reg_val = 0;
#endif

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8960x), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	client->addr = 0x6B;
	sc->dev = &client->dev;
	sc->client = client;

	i2c_set_clientdata(client, sc);

	mutex_init(&sc->i2c_rw_lock);

	ret = sc8960x_detect_device(sc);
	if (ret) {
		pr_err("No sc8960x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sc8960x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	if (sc->part_no != SC89601D && sc->part_no != ETA6963){
		pr_info("part no mismatch, hw:%s, devicetree:%s\n",
			pn_str[sc->part_no], pn_str[*(int *) match->data]);
		return -1;
	}

	if(sc->part_no == ETA6963){
		sc->eta6963_en = true;
		pr_err("charger ic is ETA6963\n");
	}

	sc->platform_data = sc8960x_parse_dt(node, sc);

	if (!sc->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = sc8960x_chg_init_psy(sc);
	if (ret < 0) {
		dev_err(sc->dev, "failed to init power supply\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230625
	g_sc = sc;
	sc->cp_psy = power_supply_get_by_phandle(node, "cp_charger");
	if (IS_ERR_OR_NULL(sc->cp_psy)) {
		pr_err("%s Couldn't get sc->cp_psy\n", __func__);
		sc->cp_psy = power_supply_get_by_name("sc,sc8551-standalone");
		if (IS_ERR_OR_NULL(sc->cp_psy)) {
			pr_err("%s Couldn't get sc,sc8551-standalone sc->cp_psy\n", __func__);
		}
	} else {
		pr_info("%s Get cp_psy successfully! \n",__func__);
	}
#endif

	atomic_set(&sc->attach, 0);

	ret = sc8960x_init_device(sc);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	INIT_DELAYED_WORK(&sc->psy_dwork, sc8960x_inform_psy_dwork_handler);
	INIT_DELAYED_WORK(&sc->force_detect_dwork, sc8960x_force_detection_dwork_handler);
	schedule_delayed_work(&sc->psy_dwork, msecs_to_jiffies(5000));

	sc->sc8690x_request_irq_wq = create_singlethread_workqueue("sc8960x_request_irq_wq");
	INIT_DELAYED_WORK(&sc->request_irq_dwork, sc8960x_request_irq_handler);
	queue_delayed_work(sc->sc8690x_request_irq_wq, &sc->request_irq_dwork, msecs_to_jiffies(5000));
	//sc8960x_register_interrupt(sc);

	sc->chg_dev = charger_device_register(sc->chg_dev_name,
						  &client->dev, sc,
						  &sc8960x_chg_ops,
						  &sc8960x_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		return ret;
	}

#if defined(M101TB_DG_PT2_531) //Leo: 10hous is no enough to charge 20000ma battery 20231016
	sc8960x_set_safety_timer(sc->chg_dev, false);
#endif

#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20231116
	ret = sc8960x_read_byte(sc, SC8960X_REG_01, &reg_val);
	if (!ret)
		sc->charge_enabled = !!(reg_val & REG01_CHG_CONFIG_MASK);
#endif

	ret = sysfs_create_group(&sc->dev->kobj, &sc8960x_attr_group);
	if (ret)
		dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(sc);

	pr_err("sc8960x probe successfully, Part Num:%d, Revision:%d\n!",
		   sc->part_no, sc->revision);

	return 0;
}

static int sc8960x_charger_remove(struct i2c_client *client)
{
	struct sc8960x *sc = i2c_get_clientdata(client);

	mutex_destroy(&sc->i2c_rw_lock);

	sysfs_remove_group(&sc->dev->kobj, &sc8960x_attr_group);

	return 0;
}

static void sc8960x_charger_shutdown(struct i2c_client *client)
{

}

static int sc8960x_suspend(struct device *dev)
{
	struct sc8960x *sc = dev_get_drvdata(dev);

	pr_err("%s\n", __func__);

	if (device_may_wakeup(dev))
			enable_irq_wake(sc->irq);

	disable_irq(sc->irq);

	return 0;
}

static int sc8960x_resume(struct device *dev)
{
	struct sc8960x *sc = dev_get_drvdata(dev);

	pr_err("%s\n", __func__);
	enable_irq(sc->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(sc->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sc8960x_pm_ops, sc8960x_suspend, sc8960x_resume);

static struct i2c_driver sc8960x_charger_driver = {
	.driver = {
		   .name = "sc8960x-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = sc8960x_charger_match_table,
		   .pm = &sc8960x_pm_ops,
		   },

	.probe = sc8960x_charger_probe,
	.remove = sc8960x_charger_remove,
	.shutdown = sc8960x_charger_shutdown,

};

module_i2c_driver(sc8960x_charger_driver);

MODULE_DESCRIPTION("SC SC8960x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");
