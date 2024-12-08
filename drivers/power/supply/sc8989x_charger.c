/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#define pr_fmt(fmt)	"[sc8989x]:%s: " fmt, __func__

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
#include <linux/extcon.h>
#include <linux/phy/phy.h>

//caozy add begin 20240416
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT)
#include <mt-plat/csci.h>
#endif
//caozy add end 20240416

#include "sc8989x_reg.h"
#include "charger_class.h"
#include "mtk_charger.h"

#if IS_ENABLED(CONFIG_CM_MIDMISC_SUPPORT)
#include <mt-plat/middle_misc.h>
#endif
#if (CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME > 0) //jnier add 20240325
static struct hrtimer battery_fake_full_kthread_timer;
static char battery_fake_full=0;
static int battery_fake_full_times=CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME;
#endif

#if IS_ENABLED(CONFIG_TCPC_FUSB302)
#define EXT_PD_CHARGER 1
extern bool get_other_pd_reset_state(void);
#else
#define EXT_PD_CHARGER 0
#endif

#define SC8989X_MIVR_MAX         13400000

#define FAST_CHARGING_CURR_UA      2000000
#define FAST_CHARGING_NORMAL_UA    1500000

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
#ifndef PHY_MODE_BC11_SET
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
#endif

static int re_try = 5;

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
#endif /*CONFIG_TCPC_HUSB311*/

enum {
    PN_SC8989X, 
};

static int pn_data[] = {
    [PN_SC8989X] = 0x05,
};

struct chg_para{
    int vlim;
    int ilim;

    int vreg;
    int ichg;
};

struct sc8989x_platform_data {
    int iprechg;
    int iterm;

    int boostv;
    int boosti;

    struct chg_para usb;
};

struct sc8989x {
    struct device *dev;
    struct i2c_client *client;

    int part_no;
    int revision;

	bool pp_en;//jnier add 20240905
	u32 mivr;

    const char *chg_dev_name;
    const char *eint_name;

    int irq_gpio;
    int en_gpio;

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    struct delayed_work force_detect_dwork;
    struct delayed_work psy_dwork;
    int force_detect_count;
#endif

#if IS_ENABLED(CONFIG_TCPC_HUSB311) || IS_ENABLED(CONFIG_AW35615_PD)
    int drvbus_gpio;
#endif

    enum power_supply_type chg_type;
    enum power_supply_usb_type psy_usb_type;

    int status;
    int irq;

    struct mutex i2c_rw_lock;
    struct mutex attach_lock;

    bool charge_enabled;	/* Register bit status */
    bool power_good;
    bool vbus_good;
    int input_curr_limit;
    int ichg_limit;
    int pd_input_limit;

    struct sc8989x_platform_data *platform_data;
    struct charger_device *chg_dev;
    struct power_supply_desc psy_desc;
    struct power_supply *psy;
    struct power_supply *bat_psy;

#if EXT_PD_CHARGER
    struct workqueue_struct *usb_charger_wq;
    struct delayed_work     pd_work;
    struct delayed_work     pd_enable_work;	
    struct notifier_block   cable_pd_nb;
    struct extcon_dev       *cable_edev;
    bool pd_charger_enable;
#endif

    atomic_t attach;
};

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
#if 0//IS_ENABLED(CONFIG_WB_DOCKING_SUPPORT) //Leo 20221117
#include <linux/of_platform.h>
#include "extcon-mtk-usb.h"
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/extcon.h>
#include <linux/of_platform.h>

static int cust_chg_enable_bc12(struct sc8989x *ddata, bool en);
static struct mtk_extcon_info *g_extcon = NULL;
static struct sc8989x *g_data = NULL;


int cust_set_bc12_en_sc8989x(int en)
{
	int ret;

	if (!g_data) 
		return -ENODEV;

	ret = cust_chg_enable_bc12(g_data, !!en);
	if (ret) {
		pr_err("%s :failed to set bcl2:%d \n",__func__,!!en);
		return ret;
	}
	
	return 0;
}
EXPORT_SYMBOL(cust_set_bc12_en_sc8989x);

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

static int cust_chg_set_usbsw(struct sc8989x *ddata,
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

int sc8989x_force_dpdm(struct sc8989x *sc);
static int cust_chg_enable_bc12(struct sc8989x *ddata, bool en)
{
    int i, ret = 0, attach;
    static const int max_wait_cnt = 250;

#if 0//IS_ENABLED(CONFIG_TCPC_HUSB311) //Leo 20231123
#if IS_ENABLED(CONFIG_WB_DOCKING_SUPPORT) 
    get_mtk_extcon_info();
    if (get_is_docking() == true) {
        dev_info(ddata->dev, "docling connect , can`t enable bc12,return !\n");
        return 0;
    }
#endif
#endif

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
    ret = cust_chg_set_usbsw(ddata, en ? USBSW_CHG : USBSW_USB);

    return ret;
}
#endif

static const struct charger_properties sc8989x_chg_props = {
    .alias_name = "sc8989x_chg",
};

static int __sc8989x_read_reg(struct sc8989x *sc, u8 reg, u8 *data)
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

static int __sc8989x_write_reg(struct sc8989x *sc, int reg, u8 val)
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

static int sc8989x_read_byte(struct sc8989x *sc, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8989x_read_reg(sc, reg, data);
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int sc8989x_write_byte(struct sc8989x *sc, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8989x_write_reg(sc, reg, data);
    mutex_unlock(&sc->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int sc8989x_update_bits(struct sc8989x *sc, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8989x_read_reg(sc, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

    ret = __sc8989x_write_reg(sc, reg, tmp);
    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&sc->i2c_rw_lock);
    return ret;
}

#if 1
#define SC8989X_REG7D 0x7d
#define SC8989X_REG85 0x85
#define SC8989X_KEY1 0x48
#define SC8989X_KEY2 0x54
#define SC8989X_KEY3 0x53
#define SC8989X_KEY4 0x38

static int sc8989x_set_key1(struct sc8989x *sc)
{
    sc8989x_write_byte(sc, SC8989X_REG7D, SC8989X_KEY1);
    sc8989x_write_byte(sc, SC8989X_REG7D, SC8989X_KEY2);
    sc8989x_write_byte(sc, SC8989X_REG7D, SC8989X_KEY3);

    return sc8989x_write_byte(sc, SC8989X_REG7D, SC8989X_KEY4);
}

static int sc8989x_set_wa(struct sc8989x *sc)
{
    int ret;
    u8 reg_val;
    
    ret = sc8989x_read_byte(sc, SC8989X_REG85, &reg_val);
    if (ret < 0) {
        sc8989x_set_key1(sc);
    }

    ret = sc8989x_read_byte(sc, SC8989X_REG85, &reg_val);
    dev_err(sc->dev, "sc8989x_set_wa 0x85 pre val: %d", reg_val);

    sc8989x_write_byte(sc, SC8989X_REG85, reg_val | 0x06);
    ret = sc8989x_read_byte(sc, SC8989X_REG85, &reg_val);
    dev_err(sc->dev, "sc8989x_set_wa 0x85 cur val: %d", reg_val);
    
    return sc8989x_set_key1(sc);
}
#endif

static int sc8989x_enable_otg(struct sc8989x *sc)
{

    u8 val = SC8989X_OTG_ENABLE << SC8989X_OTG_CONFIG_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_03,
                SC8989X_OTG_CONFIG_MASK, val);
}

static int sc8989x_disable_otg(struct sc8989x *sc)
{
    u8 val = SC8989X_OTG_DISABLE << SC8989X_OTG_CONFIG_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_03,
                SC8989X_OTG_CONFIG_MASK, val);
}

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
static int sc8989x_disable_hvdcp(struct sc8989x *sc)
{
    int ret;
    u8 val = SC8989X_HVDCP_DISABLE << SC8989X_HVDCPEN_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_02, 
                SC8989X_HVDCPEN_MASK, val);
    return ret;
}
#endif

static int sc8989x_enable_charger(struct sc8989x *sc)
{
    int ret;

    u8 val = SC8989X_CHG_ENABLE << SC8989X_CHG_CONFIG_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_03, 
                SC8989X_CHG_CONFIG_MASK, val);

#if defined(M100TB_DG_P3PRO_527) //Leo 20230526
    sc8989x_update_bits(sc, SC8989X_REG_0D, 
                0xFF, 0x80);
#endif

    return ret;
}

static int sc8989x_disable_charger(struct sc8989x *sc)
{
    int ret;

    u8 val = SC8989X_CHG_DISABLE << SC8989X_CHG_CONFIG_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_03, 
                SC8989X_CHG_CONFIG_MASK, val);
    return ret;
}

static int sc8989x_adc_start(struct sc8989x *sc, bool oneshot)
{
    u8 val;
    int ret;
    
	pr_err("%s oneshot= %d\n", __func__,oneshot);	
    ret = sc8989x_read_byte(sc, SC8989X_REG_02, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
    }
    
    if (((val & SC8989X_CONV_RATE_MASK) >> SC8989X_CONV_RATE_SHIFT) 
            == SC8989X_ADC_CONTINUE_ENABLE)
        return 0;

    if (oneshot) {
        ret = sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_CONV_START_MASK,
                SC8989X_CONV_START << SC8989X_CONV_START_SHIFT);
    }
    else {
        ret = sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_CONV_RATE_MASK,
                SC8989X_ADC_CONTINUE_ENABLE << SC8989X_CONV_RATE_SHIFT);
    }
    
    return ret;
}

static int sc8989x_adc_stop(struct sc8989x *sc)
{
	pr_err("%s\n", __func__);	
    return sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_CONV_RATE_MASK,
                SC8989X_ADC_CONTINUE_DISABLE << SC8989X_CONV_RATE_SHIFT);
}

int sc8989x_set_chargecurrent(struct sc8989x *sc, int curr)
{
    u8 ichg;

    if (curr < SC8989X_ICHG_BASE)
        curr = SC8989X_ICHG_BASE;

    ichg = (curr - SC8989X_ICHG_BASE)/SC8989X_ICHG_LSB;
	pr_err("%s curr ichg = [%d %d]\n", __func__,curr,ichg);
    return sc8989x_update_bits(sc, SC8989X_REG_04, 
                        SC8989X_ICHG_MASK, ichg << SC8989X_ICHG_SHIFT);

}

int sc8989x_set_term_current(struct sc8989x *sc, int curr)
{
    u8 iterm;

    if (curr < SC8989X_ITERM_BASE)
        curr = SC8989X_ITERM_BASE;

    iterm = (curr - SC8989X_ITERM_BASE) / SC8989X_ITERM_LSB;

    return sc8989x_update_bits(sc, SC8989X_REG_05, 
                        SC8989X_ITERM_MASK, iterm << SC8989X_ITERM_SHIFT);

}

int sc8989x_get_term_current(struct sc8989x *sc, int *curr)
{
    u8 reg_val;
    int iterm;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_05, &reg_val);
    if (!ret) {
        iterm = (reg_val & SC8989X_ITERM_MASK) >> SC8989X_ITERM_SHIFT;
        iterm = iterm * SC8989X_ITERM_LSB + SC8989X_ITERM_BASE;

        *curr = iterm * 1000;
    }
    return ret;
}

int sc8989x_set_prechg_current(struct sc8989x *sc, int curr)
{
    u8 iprechg;

    if (curr < SC8989X_IPRECHG_BASE)
        curr = SC8989X_IPRECHG_BASE;

    iprechg = (curr - SC8989X_IPRECHG_BASE) / SC8989X_IPRECHG_LSB;

    return sc8989x_update_bits(sc, SC8989X_REG_05, 
                SC8989X_IPRECHG_MASK, iprechg << SC8989X_IPRECHG_SHIFT);

}

int sc8989x_set_chargevolt(struct sc8989x *sc, int volt)
{
    u8 val;

    if (volt < SC8989X_VREG_BASE)
        volt = SC8989X_VREG_BASE;

    val = (volt - SC8989X_VREG_BASE)/SC8989X_VREG_LSB;
    return sc8989x_update_bits(sc, SC8989X_REG_06, 
                        SC8989X_VREG_MASK, val << SC8989X_VREG_SHIFT);
}

int sc8989x_get_chargevol(struct sc8989x *sc, int *volt)
{
    u8 reg_val;
    int vchg;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_06, &reg_val);
    if (!ret) {
        vchg = (reg_val & SC8989X_VREG_MASK) >> SC8989X_VREG_SHIFT;
        vchg = vchg * SC8989X_VREG_LSB + SC8989X_VREG_BASE;
        *volt = vchg * 1000;
    }
    return ret;
}

int sc8989x_adc_read_vbus_volt(struct sc8989x *sc, u32 *vol)
{
    uint8_t val;
    int volt;
    int ret;

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
#else
    sc8989x_adc_start(sc, true);
    msleep(50);
#endif

    ret = sc8989x_read_byte(sc, SC8989X_REG_11, &val);
    if (ret < 0) {
        dev_err(sc->dev, "read vbus voltage failed :%d\n", ret);
    } else{
        if(val & SC8989X_VBUS_GD_MASK){
            volt = SC8989X_VBUSV_BASE + ((val & SC8989X_VBUSV_MASK) >> 
                SC8989X_VBUSV_SHIFT) * SC8989X_VBUSV_LSB;
            *vol = volt * 1000;
        }else{
           *vol = 0; 
        }
    }

    return ret;
}

int sc8989x_adc_read_charge_current(struct sc8989x *sc, u32 *cur)
{
    uint8_t val;
    int curr;
    int ret;

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
#else
    sc8989x_adc_start(sc, true);
    msleep(50);
#endif

    ret = sc8989x_read_byte(sc, SC8989X_REG_12, &val);
    if (ret < 0) {
        dev_err(sc->dev, "read charge current failed :%d\n", ret);
    } else{
        curr = (int)(SC8989X_ICHGR_BASE + ((val & SC8989X_ICHGR_MASK) >> 
            SC8989X_ICHGR_SHIFT) * SC8989X_ICHGR_LSB) ;
        *cur = curr * 1000; 
    }

    return ret;
}

int sc8989x_set_force_vindpm(struct sc8989x *sc, bool en)
{
    u8 val;

#if defined(M100TB_DG_P3PRO_527) //Leo 20230526
	return 0;
#endif

    if (en)
        val = SC8989X_FORCE_VINDPM_ENABLE;
    else
        val = SC8989X_FORCE_VINDPM_DISABLE;

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
#else
    sc->power_good = false;
#endif
    return sc8989x_update_bits(sc, SC8989X_REG_0D, 
                SC8989X_FORCE_VINDPM_MASK, val << SC8989X_FORCE_VINDPM_SHIFT);
}

int sc8989x_set_input_volt_limit(struct sc8989x *sc, int volt)
{
    u8 val;

#if defined(M100TB_DG_P3PRO_527) //Leo 20230526
	return 0;
#endif

    if (volt < SC8989X_VINDPM_BASE)
        volt = SC8989X_VINDPM_BASE;
    
    sc8989x_set_force_vindpm(sc, true);

    val = (volt - SC8989X_VINDPM_BASE) / SC8989X_VINDPM_LSB;
    return sc8989x_update_bits(sc, SC8989X_REG_0D, 
                        SC8989X_VINDPM_MASK, val << SC8989X_VINDPM_SHIFT);
}

int sc8989x_set_input_current_limit(struct sc8989x *sc, int curr)
{
    u8 val;

#if IS_ENABLED(CONFIG_WB_DG_CUST_SUPPORT) //Leo 20230721
    if (curr > sc->input_curr_limit)
        curr = sc->input_curr_limit;
#else
    if (curr > 3000)
        curr = 3000;
#endif

    if (curr < SC8989X_IINLIM_BASE)
        curr = SC8989X_IINLIM_BASE;

    val = (curr - SC8989X_IINLIM_BASE) / SC8989X_IINLIM_LSB;
	pr_err("sc8989x_set_input_current_limit curr = %d val = %d\n", curr,val);
    return sc8989x_update_bits(sc, SC8989X_REG_00, SC8989X_IINLIM_MASK, 
                        val << SC8989X_IINLIM_SHIFT);
}

int sc8989x_get_input_volt_limit(struct sc8989x *sc, u32 *volt)
{
    u8 reg_val;
    int vchg;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0D, &reg_val);
    if (!ret) {
        vchg = (reg_val & SC8989X_VINDPM_MASK) >> SC8989X_VINDPM_SHIFT;
        vchg = vchg * SC8989X_VINDPM_LSB + SC8989X_VINDPM_BASE;
        *volt = vchg * 1000;
    }
    return ret;
}

int sc8989x_get_input_current_limit(struct  sc8989x *sc, u32 *curr)
{
    u8 reg_val;
    int icl;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_00, &reg_val);
    if (!ret) {
        icl = (reg_val & SC8989X_IINLIM_MASK) >> SC8989X_IINLIM_SHIFT;
        icl = icl * SC8989X_IINLIM_LSB + SC8989X_IINLIM_BASE;
        *curr = icl * 1000;
    }

    return ret;
}

int sc8989x_set_watchdog_timer(struct sc8989x *sc, u8 timeout)
{
    u8 val;

    val = (timeout - SC8989X_WDT_BASE) / SC8989X_WDT_LSB;
    val <<= SC8989X_WDT_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, 
                        SC8989X_WDT_MASK, val); 
}

int sc8989x_disable_watchdog_timer(struct sc8989x *sc)
{
    u8 val = SC8989X_WDT_DISABLE << SC8989X_WDT_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, 
                        SC8989X_WDT_MASK, val);
}

int sc8989x_reset_watchdog_timer(struct sc8989x *sc)
{
    u8 val = SC8989X_WDT_RESET << SC8989X_WDT_RESET_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_03, 
                        SC8989X_WDT_RESET_MASK, val);
}

int sc8989x_force_dpdm(struct sc8989x *sc)
{
    int ret;
    u8 val = SC8989X_FORCE_DPDM << SC8989X_FORCE_DPDM_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_02, 
                        SC8989X_FORCE_DPDM_MASK, val);

    pr_info("Force DPDM %s\n", !ret ? "successfully" : "failed");
    
    return ret;

}

int sc8989x_reset_chip(struct sc8989x *sc)
{
    int ret;
    u8 val = SC8989X_RESET << SC8989X_RESET_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_14, 
                        SC8989X_RESET_MASK, val);
    return ret;
}

int sc8989x_enable_hiz_mode(struct sc8989x *sc, bool en)
{
    u8 val;

    if (en) {
        val = SC8989X_HIZ_ENABLE << SC8989X_ENHIZ_SHIFT;
    } else {
        val = SC8989X_HIZ_DISABLE << SC8989X_ENHIZ_SHIFT;
    }

    return sc8989x_update_bits(sc, SC8989X_REG_00, 
                        SC8989X_ENHIZ_MASK, val);

}

int sc8989x_exit_hiz_mode(struct sc8989x *sc)
{

    u8 val = SC8989X_HIZ_DISABLE << SC8989X_ENHIZ_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_00, 
                        SC8989X_ENHIZ_MASK, val);

}

int sc8989x_get_hiz_mode(struct sc8989x *sc, u8 *state)
{
    u8 val;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_00, &val);
    if (ret)
        return ret;
    *state = (val & SC8989X_ENHIZ_MASK) >> SC8989X_ENHIZ_SHIFT;

    return 0;
}

static int sc8989x_do_event(struct charger_device *chgdev, u32 event, u32 args)
{
	struct sc8989x *ddata = dev_get_drvdata(&chgdev->dev);

	switch (event) {
	case EVENT_FULL:
			charger_dev_notify(chgdev, CHARGER_DEV_NOTIFY_EOC);
			//sc8989x_disable_charger(ddata);
			break;
	case EVENT_RECHARGE:
			charger_dev_notify(chgdev, CHARGER_DEV_NOTIFY_RECHG);
			//sc8989x_enable_charger(ddata);
			break;
	case EVENT_DISCHARGE:
			break;
	default:
			break;
	}
	power_supply_changed(ddata->psy);
	return 0;
}

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
static int sc8989x_set_auto_dpdm(struct sc8989x *sc, bool enable)
{
    u8 val = enable ? SC8989X_AUTO_DPDM_ENABLE : SC8989X_AUTO_DPDM_DISABLE;

    return sc8989x_update_bits(sc, SC8989X_REG_02, 
                        SC8989X_AUTO_DPDM_EN_MASK, val << SC8989X_AUTO_DPDM_EN_SHIFT);
}
#endif

static int sc8989x_enable_term(struct sc8989x *sc, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = SC8989X_TERM_ENABLE << SC8989X_EN_TERM_SHIFT;
    else
        val = SC8989X_TERM_DISABLE << SC8989X_EN_TERM_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_07, 
                        SC8989X_EN_TERM_MASK, val);

    return ret;
}

int sc8989x_set_boost_current(struct sc8989x *sc, int curr)
{
    u8 temp;

    if (curr == 500)
        temp = SC8989X_BOOST_LIM_500MA;
    else if (curr == 750)
        temp = SC8989X_BOOST_LIM_750MA;
    else if (curr == 1200)
        temp = SC8989X_BOOST_LIM_1200MA;
    else
        temp = SC8989X_BOOST_LIM_1400MA;

    return sc8989x_update_bits(sc, SC8989X_REG_0A, 
                SC8989X_BOOST_LIM_MASK, 
                temp << SC8989X_BOOST_LIM_SHIFT);

}

int sc8989x_set_boost_voltage(struct sc8989x *sc, int volt)
{
    u8 val = 0;

    if (volt < SC8989X_BOOSTV_BASE)
        volt = SC8989X_BOOSTV_BASE;
    if (volt > SC8989X_BOOSTV_BASE 
            + (SC8989X_BOOSTV_MASK >> SC8989X_BOOSTV_SHIFT) 
            * SC8989X_BOOSTV_LSB)
        volt = SC8989X_BOOSTV_BASE 
            + (SC8989X_BOOSTV_MASK >> SC8989X_BOOSTV_SHIFT) 
            * SC8989X_BOOSTV_LSB;

    val = ((volt - SC8989X_BOOSTV_BASE) / SC8989X_BOOSTV_LSB) 
            << SC8989X_BOOSTV_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_0A, 
                SC8989X_BOOSTV_MASK, val);


}

static int sc8989x_enable_ico(struct sc8989x* sc, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = SC8989X_ICO_ENABLE << SC8989X_ICOEN_SHIFT;
    else
        val = SC8989X_ICO_DISABLE << SC8989X_ICOEN_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_ICOEN_MASK, val);

    return ret;

}

static int sc8989x_enable_safety_timer(struct sc8989x *sc)
{
    const u8 val = SC8989X_CHG_TIMER_ENABLE << SC8989X_EN_TIMER_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, SC8989X_EN_TIMER_MASK,
                val);
}

static int sc8989x_disable_safety_timer(struct sc8989x *sc)
{
    const u8 val = SC8989X_CHG_TIMER_DISABLE << SC8989X_EN_TIMER_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, SC8989X_EN_TIMER_MASK,
                val);

}

static struct sc8989x_platform_data *sc8989x_parse_dt(struct device_node *np,
                            struct sc8989x *sc)
{
    int ret;
    struct sc8989x_platform_data *pdata;
    pdata = devm_kzalloc(sc->dev, sizeof(struct sc8989x_platform_data),
                GFP_KERNEL);
    if (!pdata)
        return NULL;

    if (of_property_read_string(np, "chg_name", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "primary_chg";
        pr_warn("no charger name\n");
    }

    if (of_property_read_string(np, "eint_name", &sc->eint_name) < 0) {
        sc->eint_name = "chr_stat";
        pr_warn("no eint name\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of sc,sc8989x,usb-vlim\n");
    }

    sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
    if (sc->irq_gpio < 0)
        pr_err("sc,intr-gpio is not available\n");

    ret = of_property_read_u32(np, "sc,sc8989x,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 500;
        pr_err("Failed to read node of sc,sc8989x,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of sc,sc8989x,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 500;
        pr_err("Failed to read node of sc,sc8989x,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,precharge-current",
                &pdata->iprechg);
    if (ret) {
        pdata->iprechg = 180;
        pr_err("Failed to read node of sc,sc8989x,precharge-current\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,termination-current",
                &pdata->iterm);
    if (ret) {
        pdata->iterm = 180;
        pr_err
            ("Failed to read node of sc,sc8989x,termination-current\n");
    }

    ret =
        of_property_read_u32(np, "sc,sc8989x,boost-voltage",
                &pdata->boostv);
    if (ret) {
        pdata->boostv = 5000;
        pr_err("Failed to read node of sc,sc8989x,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "sc,sc8989x,boost-current",
                &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of sc,sc8989x,boost-current\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,ichg_limit", &sc->ichg_limit);
    if (ret) {
        sc->ichg_limit = 3600;
        pr_err("Failed to read node of sc,sc8989x,ichg_limit\n");
    }

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    ret = of_property_read_u32(np, "sc,sc8989x,pd_input_limit", &sc->pd_input_limit);
    if (ret) {
        sc->pd_input_limit = 2050;
        pr_err("Failed to read node of sc,sc8989x,pd_input_limit\n");
    }
#endif

    ret = of_property_read_u32(np, "sc,sc8989x,input_current", &sc->input_curr_limit);
    if (ret) {
        sc->input_curr_limit = 3000;
        pr_err("Failed to read node of sc,sc8989x,input_current\n");
    }

    sc->en_gpio = of_get_named_gpio(np, "sc,en-gpio", 0);
    if (sc->en_gpio < 0) {
        pr_err("sc,sc->en_gpio is not available\n");
    } else {
        ret = gpio_request(sc->en_gpio, "sc8989,en-gpio");
        if (ret < 0) {
            pr_info(" %s gpio_request sc8989,en-gpio failed!\n",__func__);
        } else {
            if (gpio_is_valid(sc->en_gpio)) {
                gpio_direction_output(sc->en_gpio, 0); //enable default
                pr_info("sc8989 charge en gpio enable\n");
            }
        }
    }

#if IS_ENABLED(CONFIG_TCPC_HUSB311) || IS_ENABLED(CONFIG_AW35615_PD)
    sc->drvbus_gpio = of_get_named_gpio(np, "sc,drvbus-gpio", 0);
    if (sc->drvbus_gpio < 0) {
        pr_err("sc,sc->drvbus_gpio is not available\n");
    } else {
        ret = gpio_request(sc->drvbus_gpio, "sc8989,drvbus-gpio");
        if (ret < 0) {
            pr_info(" %s gpio_request sc,drvbus-gpio failed!\n",__func__);
        } else {
            if (gpio_is_valid(sc->drvbus_gpio)) {
                gpio_direction_output(sc->drvbus_gpio, 0);
                pr_info("sc8989 drvbus_gpio default low\n");
            }
        }
    }
#endif

//caozy add begin 20240416
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT)
    if (csci_exist("sc8989x_iterm")) {
        if (csci_integer("sc8989x_iterm", 0) > 0) {
            pdata->iterm = csci_integer("sc8989x_iterm", 0);
            printk("sc8989x pdata->iterm=%d\n",pdata->iterm);
        }
    }
#endif
//caozy add end 20240416

    return pdata;
}

static int sc8989x_get_charge_stat(struct sc8989x *sc, int *state)
{
    int ret;
    u8 val;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &val);
    if (!ret) {
        if ((val & SC8989X_VBUS_STAT_MASK) >> SC8989X_VBUS_STAT_SHIFT 
                == SC8989X_VBUS_TYPE_OTG) {
            *state = POWER_SUPPLY_STATUS_DISCHARGING;
            return ret;
        }
        val = val & SC8989X_CHRG_STAT_MASK;
        val = val >> SC8989X_CHRG_STAT_SHIFT;
        switch (val)
        {
        case SC8989X_CHRG_STAT_IDLE:
            ret = sc8989x_read_byte(sc, SC8989X_REG_11, &val);
            if (val & SC8989X_VBUS_GD_MASK) {
                *state = POWER_SUPPLY_STATUS_CHARGING;
            } else {
                *state = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            break;
        case SC8989X_CHRG_STAT_PRECHG:
        case SC8989X_CHRG_STAT_FASTCHG:
            *state = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case SC8989X_CHRG_STAT_CHGDONE:
            *state = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
            *state = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
        }
    }
	if(!sc->charge_enabled) {
		*state = POWER_SUPPLY_STATUS_NOT_CHARGING;  //jnier add 20231221
	}
    pr_err("%s---->%d  %02x\n", __func__, *state, val);

    return ret;
}

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
static void sc8989x_force_detection_dwork_handler(struct work_struct *work) {
    int ret;
    struct sc8989x *sc = container_of(work, struct sc8989x,
                                        force_detect_dwork.work);

    cust_chg_enable_bc12(sc, true);
    
    ret = sc8989x_force_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: force dpdm failed(%d)\n", __func__, ret);
        return;
    }

    sc->power_good = false;

    sc->force_detect_count++;
}
#endif

static int sc8989x_get_charger_type(struct sc8989x *sc)
{
    int ret;

    u8 reg_val = 0;
    int vbus_stat = 0;

    int attach;

    //mutex_lock(&sc->attach_lock);

    attach = atomic_read(&sc->attach);
    printk("sc8989x_get_charger_type attach=%d\n", attach);

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &reg_val);

    if (ret)
        return ret;

    vbus_stat = (reg_val & SC8989X_VBUS_STAT_MASK);
    vbus_stat >>= SC8989X_VBUS_STAT_SHIFT;

    switch (vbus_stat) {

    case SC8989X_VBUS_TYPE_NONE:
		if(sc->vbus_good){
#if EXT_PD_CHARGER
            if(sc->pd_charger_enable){
                sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
                sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
                sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
            } else {
                sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
                sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
                sc->chg_type = POWER_SUPPLY_TYPE_USB;
            }
#else
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
#endif
		}else{
        	sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        	sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
            sc->chg_type = POWER_SUPPLY_TYPE_USB;
		}
        break;
    case SC8989X_VBUS_TYPE_SDP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        break;
    case SC8989X_VBUS_TYPE_CDP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        break;
    case SC8989X_VBUS_TYPE_DCP:
    case SC8989X_VBUS_TYPE_HVDCP:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    case SC8989X_VBUS_TYPE_UNKNOWN:
		if(sc->vbus_good){
#if EXT_PD_CHARGER
            if(sc->pd_charger_enable){
                sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
                sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
                sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
            }else{
                sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
                sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
                sc->chg_type = POWER_SUPPLY_TYPE_USB;
            }
#else
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
            sc->chg_type = POWER_SUPPLY_TYPE_USB;
#endif
		}else{
        	sc->psy_usb_type =  POWER_SUPPLY_USB_TYPE_UNKNOWN;
        	sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
            sc->chg_type = POWER_SUPPLY_TYPE_USB;
		}
#if IS_ENABLED(CONFIG_TCPC_HUSB311)
        if (sc->force_detect_count < 10) {
            schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(2000));
        }
#endif
        break;
    case SC8989X_VBUS_TYPE_NON_STD:
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    default://Leo modified for plug in usb ,no charger icon
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        break;
    }

    //mutex_unlock(&sc->attach_lock);
    
#if EXT_PD_CHARGER
    pr_err("%s ---->0x%02x  chg type %d  usb type %d  pd_charger_enable=%d\n", __func__, reg_val, 
        sc->psy_desc.type, sc->psy_usb_type, sc->pd_charger_enable);
#else
    pr_err("%s ---->0x%02x  chg type %d  usb type %d\n", __func__, reg_val, 
        sc->psy_desc.type, sc->psy_usb_type);
#endif

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    cust_chg_enable_bc12(sc, false);
#endif

    return 0;
}

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
static void sc8989x_inserted_irq(struct sc8989x *sc)
{
    dev_info(sc->dev, "%s: adapter/usb inserted\n", __func__);

    sc->force_detect_count = 0;

    // schedule_delayed_work(&sc->force_detect_dwork,
    //                     msecs_to_jiffies(80));
    schedule_delayed_work(&sc->psy_dwork, 0);

#if EXT_PD_CHARGER
    schedule_delayed_work(&sc->pd_enable_work, msecs_to_jiffies(250));
    //mdelay(250);
    //sc->pd_charger_enable = get_other_pd_reset_state();
#endif
}

static void sc8989x_removed_irq(struct sc8989x *sc)
{
    dev_info(sc->dev, "%s: adapter/usb removed\n", __func__);
    //cancel_delayed_work_sync(&sc->force_detect_dwork);
    sc8989x_set_input_current_limit(sc, 500);
    sc8989x_set_chargecurrent(sc, 500);

#if EXT_PD_CHARGER
    schedule_delayed_work(&sc->pd_enable_work, msecs_to_jiffies(250));
    //mdelay(250);
    //sc->pd_charger_enable = get_other_pd_reset_state();
#endif
}

static void sc8989x_inform_psy_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    union power_supply_propval propval;
    union power_supply_propval prop_type = {0};
    union power_supply_propval prop_usb_type = {0};

    struct sc8989x *sc = container_of(work, struct sc8989x,
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

    propval.intval = sc->psy_usb_type;

    ret = power_supply_set_property(sc->psy,
                POWER_SUPPLY_PROP_USB_TYPE, &prop_usb_type);

    propval.intval = sc->chg_type;
    ret = power_supply_set_property(sc->psy,
                POWER_SUPPLY_PROP_TYPE, &prop_type);

    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);
    
}
#endif

static void sc8989x_dump_regs(struct sc8989x *sc);
static irqreturn_t sc8989x_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val;
    bool prev_pg;
    bool prev_vbus_pg;
    struct sc8989x *sc = data;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &reg_val);
    if (ret)
        return IRQ_HANDLED;
    prev_pg = sc->power_good;
    sc->power_good = !!(reg_val & SC8989X_PG_STAT_MASK);

    ret = sc8989x_read_byte(sc, SC8989X_REG_11, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_vbus_pg = sc->vbus_good;

    sc->vbus_good = !!(reg_val & SC8989X_VBUS_GD_MASK);

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    if (!prev_vbus_pg && sc->vbus_good) {
        sc8989x_inserted_irq(sc);
    } else if (prev_vbus_pg && !sc->vbus_good) {
        sc8989x_removed_irq(sc);
        power_supply_changed(sc->psy);
        cust_chg_enable_bc12(sc, true);
        power_supply_changed(sc->psy);
        re_try = 5;
        return IRQ_HANDLED;
    }

    if (!prev_pg && sc->power_good) {
        sc8989x_get_charger_type(sc);
        power_supply_changed(sc->psy);
    }
#else
    if (!prev_vbus_pg && sc->vbus_good){
        sc->input_curr_limit = 500;
        pr_err("adapter/usb inserted\n");
		sc8989x_set_input_current_limit(sc, 500);
		msleep(250);
        sc8989x_get_charger_type(sc);
#if EXT_PD_CHARGER
        sc->pd_charger_enable = get_other_pd_reset_state();
#endif
    } else if (prev_vbus_pg && !sc->vbus_good) {
        pr_err("adapter/usb removed\n");
		msleep(250);
#if EXT_PD_CHARGER
        sc->pd_charger_enable = get_other_pd_reset_state();
#endif
        sc8989x_get_charger_type(sc);
    }

    if (!prev_pg && sc->power_good){
        sc8989x_get_charger_type(sc);
    }
    
    //sc8989x_dump_regs(sc);

    power_supply_changed(sc->psy);
#endif


    return IRQ_HANDLED;
}

static int sc8989x_register_interrupt(struct sc8989x *sc)
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
                    sc8989x_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "sc8989x_irq", sc);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }else{
        pr_err("request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
    }

    device_init_wakeup(sc->dev, true);

    return 0;
}

static int sc8989x_init_device(struct sc8989x *sc)
{
    int ret;

    sc8989x_disable_watchdog_timer(sc);
    sc8989x_enable_ico(sc, false);
#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    sc8989x_disable_hvdcp(sc);
#endif

    ret = sc8989x_set_prechg_current(sc, sc->platform_data->iprechg);
    if (ret)
        pr_err("Failed to set prechg current, ret = %d\n", ret);

    ret = sc8989x_set_term_current(sc, sc->platform_data->iterm);
    if (ret)
        pr_err("Failed to set termination current, ret = %d\n", ret);

    ret = sc8989x_set_boost_voltage(sc, sc->platform_data->boostv);
    if (ret)
        pr_err("Failed to set boost voltage, ret = %d\n", ret);

    ret = sc8989x_set_boost_current(sc, sc->platform_data->boosti);
    if (ret)
        pr_err("Failed to set boost current, ret = %d\n", ret);

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    ret = sc8989x_set_auto_dpdm(sc, true);

	ret = sc8989x_adc_start(sc, false);
#endif
	
	ret = sc8989x_set_wa(sc);

    return 0;
}

static void determine_initial_status(struct sc8989x *sc)
{
    sc8989x_irq_handler(sc->irq, (void *) sc);
}

static int sc8989x_detect_device(struct sc8989x *sc)
{
    int ret;
    u8 data;

    ret = sc8989x_read_byte(sc, SC8989X_REG_14, &data);
    if (!ret) {
        sc->part_no = (data & SC8989X_PN_MASK) >> SC8989X_PN_SHIFT;
        sc->revision =
            (data & SC8989X_DEV_REV_MASK) >> SC8989X_DEV_REV_SHIFT;
    }

    return ret;
}

static void sc8989x_dump_regs(struct sc8989x *sc)
{
    int addr;
    u8 val;
    int ret;

    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = sc8989x_read_byte(sc, addr, &val);
        if (ret == 0)
            pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
    }
}

static ssize_t
sc8989x_show_registers(struct device *dev, struct device_attribute *attr,
            char *buf)
{
    struct sc8989x *sc = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[200];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8989x Reg");
    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = sc8989x_read_byte(sc, addr, &val);
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
sc8989x_store_registers(struct device *dev,
            struct device_attribute *attr, const char *buf,
            size_t count)
{
    struct sc8989x *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < 0x14) {
        sc8989x_write_byte(sc, (unsigned char) reg,
                (unsigned char) val);
    }

    return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8989x_show_registers,
        sc8989x_store_registers);

static struct attribute *sc8989x_attributes[] = {
    &dev_attr_registers.attr,
    NULL,
};

static const struct attribute_group sc8989x_attr_group = {
    .attrs = sc8989x_attributes,
};

static int sc8989x_charging(struct charger_device *chg_dev, bool enable)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u8 val;

    if (enable)
        ret = sc8989x_enable_charger(sc);
    else
        ret = sc8989x_disable_charger(sc);

    pr_err("%s charger %s\n", enable ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    ret = sc8989x_read_byte(sc, SC8989X_REG_03, &val);

    if (!ret) {
        sc->charge_enabled = !!(val & SC8989X_CHG_CONFIG_MASK);
	}
	sc->charge_enabled = enable;  //jnier add 20231221
#if IS_ENABLED(CONFIG_TCPC_HUSB311)
#if EXT_PD_CHARGER
    sc->pd_charger_enable = get_other_pd_reset_state();
    if(sc->pd_charger_enable){
        if(sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP){
            sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
            power_supply_changed(sc->psy);
        }
    }else{
        if(enable){
            if(sc->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP && re_try >= 1){
                re_try--;
                sc8989x_get_charger_type(sc);
                if(sc->psy_usb_type != POWER_SUPPLY_USB_TYPE_SDP){
                    power_supply_changed(sc->psy);
                }
            }
        }
    }
#endif
#endif

    return ret;
}

static int sc8989x_plug_in(struct charger_device *chg_dev)
{
    int ret;
    
    ret = sc8989x_charging(chg_dev, true);
    if (ret)
        pr_err("Failed to enable charging:%d\n", ret);

    return ret;
}

static int sc8989x_plug_out(struct charger_device *chg_dev)
{
    int ret;

    ret = sc8989x_charging(chg_dev, false);

    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    return ret;
}

static int sc8989x_dump_register(struct charger_device *chg_dev)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    sc8989x_dump_regs(sc);

    return 0;
}

static int sc8989x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    *en = sc->charge_enabled;

    return 0;
}

static int sc8989x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
#if 0
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &val);
    if (!ret) {
        val = val & SC8989X_CHRG_STAT_MASK;
        val = val >> SC8989X_CHRG_STAT_SHIFT;
        *done = (val == SC8989X_CHRG_STAT_CHGDONE);
        dev_info(sc->dev, "%s done =%d\n", __func__,*done);
    }
#else
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    union power_supply_propval val;
    int ret;

    ret = power_supply_get_property(sc->psy, POWER_SUPPLY_PROP_STATUS,
                    &val);
    if (ret < 0)
        return ret;

    *done = (val.intval == POWER_SUPPLY_STATUS_FULL);
	dev_info(sc->dev, "%s done =%d\n", __func__,*done);
#endif

    return ret;
}

static int sc8989x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

#if EXT_PD_CHARGER
    if(sc->pd_charger_enable){
        pr_err("pd charge curr =%d\n", sc->ichg_limit * 1000);
        return sc8989x_set_chargecurrent(sc, sc->ichg_limit);
    }
#endif

    pr_err("charge curr = %d\n", curr);

    return sc8989x_set_chargecurrent(sc, curr / 1000);
}

static int sc8989x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_04, &reg_val);
    if (!ret) {
        ichg = (reg_val & SC8989X_ICHG_MASK) >> SC8989X_ICHG_SHIFT;
        ichg = ichg * SC8989X_ICHG_LSB + SC8989X_ICHG_BASE;
        *curr = ichg * 1000;
    }

    return ret;
}

static int sc8989x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    *curr = 60 * 1000;

    return 0;
}

static int sc8989x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge volt = %d\n", volt);

    return sc8989x_set_chargevolt(sc, volt / 1000);
}

static int sc8989x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_get_chargevol(sc, volt);
}

static int sc8989x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	int ret = 0;
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

#if 0
    if(sc->pd_charger_enable){
        return 0;
    }
#endif

    pr_err("vindpm volt = %d\n", volt);
//jnier add start 20240905
	if (!sc->pp_en) {
		dev_info(sc->dev, "%s: power path is disabled\n", __func__);
		sc8989x_set_input_volt_limit(sc, SC8989X_MIVR_MAX / 1000);
		return ret;
	}
	sc->mivr=volt; 
//jnier add end 20240905
    return sc8989x_set_input_volt_limit(sc, volt / 1000);
}

static int sc8989x_get_ivl(struct charger_device *chgdev, u32 *volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);

    return sc8989x_get_input_volt_limit(sc, volt);
}

static int sc8989x_get_vbus_adc(struct charger_device *chgdev, u32 *vbus)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8989x_adc_read_vbus_volt(sc, vbus);
}

static int sc8989x_get_ibus_adc(struct charger_device *chgdev, u32 *ibus)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8989x_adc_read_charge_current(sc, ibus);
}

static int sc8989x_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

#if EXT_PD_CHARGER
    if(sc->pd_charger_enable){
        pr_err("pd indpm curr = %d\n", sc->pd_input_limit*1000);
        return sc8989x_set_input_current_limit(sc, sc->pd_input_limit);
	}
#endif

    pr_err("indpm curr = %d\n", curr);

    return sc8989x_set_input_current_limit(sc, curr / 1000);
}

static int sc8989x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    
    return sc8989x_get_input_current_limit(sc, curr);
}

static int sc8989x_kick_wdt(struct charger_device *chg_dev)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_reset_watchdog_timer(sc);
}

static int sc8989x_set_ieoc(struct charger_device *chgdev, u32 ieoc)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8989x_set_term_current(sc, ieoc / 1000);
}

static int sc8989x_enable_te(struct charger_device *chgdev, bool en)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);

    return sc8989x_enable_term(sc, en);
}

static int sc8989x_enable_hz(struct charger_device *chgdev, bool en)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);

    return sc8989x_enable_hiz_mode(sc, en);
}

//jnier add start 20240905
static int sc8989x_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

	sc->pp_en = en;
	dev_info(sc->dev, "%s: en = %d, pp_en = %d\n",__func__, en, sc->pp_en);
	return sc8989x_set_ivl(chg_dev, en ? sc->mivr : SC8989X_MIVR_MAX);
}

static int sc8989x_is_power_path_enable(struct charger_device *chg_dev, bool *en)
{
	struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

	dev_info(sc->dev, "%s: pp_en = %d\n",__func__, sc->pp_en);
	*en = sc->pp_en;
	return 0;
}
//jnier add end 20240905

static int sc8989x_set_otg(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

#if IS_ENABLED(CONFIG_WB_BOARD_M307TCR110)	//Leo 20240929
#if defined(CONFIG_WB_OTG_VBUS_USE_EXT_LDO)
	pr_info("%s usb extern ldo for otg, skip !\n",__func__);
	return 0;
#endif
#endif

#if IS_ENABLED(CONFIG_TCPC_HUSB311) || IS_ENABLED(CONFIG_AW35615_PD)
    if(gpio_is_valid(sc->drvbus_gpio)){
        gpio_direction_output(sc->drvbus_gpio, en);
    }
#endif

    if (en) {
        ret = sc8989x_disable_charger(sc);
        ret = sc8989x_enable_otg(sc);
    }
    else {
        ret = sc8989x_disable_otg(sc);
        ret = sc8989x_enable_charger(sc);
    }

    pr_err("%s OTG %s\n", en ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    dev_info(sc->dev, "%s = %d\n", __func__,en);
    if (en)
        ret = sc8989x_enable_safety_timer(sc);
    else
        ret = sc8989x_disable_safety_timer(sc);

    return ret;
}

static int sc8989x_is_safety_timer_enabled(struct charger_device *chg_dev,
                    bool *en)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 reg_val;

    ret = sc8989x_read_byte(sc, SC8989X_REG_07, &reg_val);

    if (!ret)
        *en = !!(reg_val & SC8989X_EN_TIMER_MASK);

    return ret;
}

static int sc8989x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = sc8989x_set_boost_current(sc, curr / 1000);

    return ret;
}

static enum power_supply_usb_type sc8989x_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property sc8989x_chg_psy_properties[] = {
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

static int sc8989x_chg_property_is_writeable(struct power_supply *psy,
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

#if defined(M100TBR210_KJ_965) //Leo 20231212
int is_pd_adapter = 0;
EXPORT_SYMBOL(is_pd_adapter);
#endif

#if (CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME > 0) //jnier add 20240325
enum hrtimer_restart battery_fake_full_kthread_hrtimer_func(struct hrtimer *timer)
{
	pr_err("%s\n", __func__);
	battery_fake_full=1;
	return HRTIMER_NORESTART;
}
void battery_fake_full_kthread_hrtimer_init(void)
{
	hrtimer_init(&battery_fake_full_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_fake_full_kthread_timer.function = battery_fake_full_kthread_hrtimer_func;
}
#endif

static int sc8989x_chg_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    int ret = 0;
    u32 _val;
    int data;
    //caozy add begin
    u8 reg_val;
    bool sc8989x_online;
    //caozy add end
    struct sc8989x *sc = power_supply_get_drvdata(psy);
#if (CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME > 0)
	ktime_t ktime;
	union power_supply_propval bat_capacity;
#endif
	//printk("%s==========psp:%d\n",__func__,psp);
    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = "SouthChip";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        val->intval = sc->vbus_good;
        break;
    case POWER_SUPPLY_PROP_STATUS:
        //caozy add begin
        ret = sc8989x_read_byte(sc, SC8989X_REG_11, &reg_val);
        sc8989x_online = !!(reg_val & SC8989X_VBUS_GD_MASK);
        if(!sc8989x_online){
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
#if (CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME > 0)
			battery_fake_full=0;
#endif
            break;
        }
        //caozy add end
        ret = sc8989x_get_charge_stat(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
#if (CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME > 0)
		sc->bat_psy = power_supply_get_by_name("battery");
		if (!sc->bat_psy) {
			pr_err("%s get power supply fail\n", __func__);
		} else {
			ret = power_supply_get_property(sc->bat_psy, POWER_SUPPLY_PROP_CAPACITY,
							&bat_capacity);
			if((bat_capacity.intval >99)&&(val->intval == POWER_SUPPLY_STATUS_CHARGING)){
				if(battery_fake_full==1){
					val->intval = POWER_SUPPLY_STATUS_FULL;
					pr_err("%s battery_fake val->intval %d\n",__func__,val->intval);
				} else {
					battery_fake_full=0;
					ktime = ktime_set(battery_fake_full_times*60, 0);
					if(!hrtimer_active(&battery_fake_full_kthread_timer)) {
						pr_err("%s battery_fake hrtimer_start val->intval %d\n",__func__,val->intval);
						hrtimer_start(&battery_fake_full_kthread_timer, ktime, HRTIMER_MODE_REL);
					}
				}
			}
		}
#endif
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_get_chargevol(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = sc8989x_get_input_current_limit(sc, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_get_input_volt_limit(sc, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_get_term_current(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        //mutex_lock(&sc->attach_lock);
        val->intval = sc->psy_usb_type;
        //mutex_unlock(&sc->attach_lock);
        break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
        if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB) {
            #if defined(M100TBR210_KJ_965) //Leo 20231212
            if (is_pd_adapter == true) {
                val->intval = FAST_CHARGING_CURR_UA;
            } else {
                val->intval = 500000;
            }
            #else
            val->intval = 500000;
            #endif
        } else if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB_DCP)
#if EXT_PD_CHARGER
        {
            if (sc->pd_charger_enable) {
                val->intval = FAST_CHARGING_CURR_UA;
            } else {
            #if defined(M100TBR210_KJ_965) //Leo 20231215
                if (is_pd_adapter == true) {
                    val->intval = FAST_CHARGING_CURR_UA;
                } else {
                    val->intval = FAST_CHARGING_NORMAL_UA;
                }
            #else
                val->intval = FAST_CHARGING_NORMAL_UA;
            #endif
            }
        }
#else
        {
            val->intval = FAST_CHARGING_NORMAL_UA;
        }
#endif
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        //if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB) {
            val->intval = 5000000;
        //}
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = sc->psy_desc.type;
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int sc8989x_chg_set_property(struct power_supply *psy,
                enum power_supply_property psp,
                const union power_supply_propval *val)
{
    int ret = 0;
    struct sc8989x *sc = power_supply_get_drvdata(psy);

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
        atomic_set(&sc->attach, val->intval);
        sc8989x_force_dpdm(sc);
        break;
    case POWER_SUPPLY_PROP_STATUS:
        if (val->intval) {
            ret = sc8989x_enable_charger(sc);
        } else {
            ret = sc8989x_disable_charger(sc);
        }
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
#if EXT_PD_CHARGER
        if(sc->pd_charger_enable){
            ret = sc8989x_set_chargecurrent(sc, sc->ichg_limit);
            break;
        }
#endif
        ret = sc8989x_set_chargecurrent(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = sc8989x_set_chargevolt(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
#if EXT_PD_CHARGER
        if(sc->pd_charger_enable){
            ret = sc8989x_set_input_current_limit(sc, sc->pd_input_limit);
            break;
        }
#endif
        sc->input_curr_limit = val->intval / 1000;
        ret = sc8989x_set_input_current_limit(sc, sc->input_curr_limit);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
        ret = sc8989x_set_input_volt_limit(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        ret = sc8989x_set_term_current(sc, val->intval / 1000);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static char *sc8989x_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc sc8989x_psy_desc = {
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = sc8989x_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(sc8989x_chg_psy_usb_types),
    .properties = sc8989x_chg_psy_properties,
    .num_properties = ARRAY_SIZE(sc8989x_chg_psy_properties),
    .property_is_writeable = sc8989x_chg_property_is_writeable,
    .get_property = sc8989x_chg_get_property,
    .set_property = sc8989x_chg_set_property,
};

static int sc8989x_chg_init_psy(struct sc8989x *sc)
{
    struct power_supply_config cfg = {
        .drv_data = sc,
        .of_node = sc->dev->of_node,
        .supplied_to = sc8989x_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(sc8989x_psy_supplied_to),
    };

    memcpy(&sc->psy_desc, &sc8989x_psy_desc, sizeof(sc->psy_desc));
    sc->psy_desc.name = dev_name(sc->dev);//"charger";//dev_name(sc->dev);
    sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc,
                        &cfg);
    return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}


static int sc8989x_send_ta20_current_pattern(struct charger_device *chg_dev, u32 uV)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;
    int i;

    sc8989x_enable_ico(sc, 0);//Leo 20240605
    sc8989x_set_chargecurrent(sc, 1000); //Leo add for default 20240605

    if (uV < 5500000) {
        uV = 5500000;
    } else if (uV > 15000000) {
        uV = 15000000;
    }
    
    val = (uV - 5500000) / 500000;
    
    pr_info("%s ta20 vol=%duV, val=%d\n", __func__, uV, val);
    
    sc8989x_set_input_current_limit(sc, 100);
    mdelay(150);
    for (i = 4; i >= 0; i--) {
        sc8989x_set_input_current_limit(sc, 800);
        (val & (1 << i)) ? msleep(100) : msleep(50);
        sc8989x_set_input_current_limit(sc, 100);
        (val & (1 << i)) ? msleep(50) : msleep(100);
    }
    sc8989x_set_input_current_limit(sc, 800);
    mdelay(150);
    sc8989x_set_input_current_limit(sc, 100);
    mdelay(120);


    //upm6920_enable_ico(upm, 1);//Leo 20240605

    return sc8989x_set_input_current_limit(sc, 800);
}

static int sc8989x_set_ta20_reset(struct charger_device *chg_dev)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int curr;
    int ret;

    return 0; //not dynamic voltage 20240605

    ret = sc8989x_get_input_current_limit(sc, &curr);

    ret = sc8989x_set_input_current_limit(sc, 100);
    msleep(300);
    return sc8989x_set_input_current_limit(sc, curr);
}



static struct charger_ops sc8989x_chg_ops = {
    /* cable plug in/out */
    .plug_in = sc8989x_plug_in,
    .plug_out = sc8989x_plug_out,
    /* enable */
    .enable = sc8989x_charging,
    .is_enabled = sc8989x_is_charging_enable,
    /* charging current */
    .set_charging_current = sc8989x_set_ichg,
    .get_charging_current = sc8989x_get_ichg,
    .get_min_charging_current = sc8989x_get_min_ichg,
    /* charging voltage */
    .set_constant_voltage = sc8989x_set_vchg,
    .get_constant_voltage = sc8989x_get_vchg,
    /* input current limit */
    .set_input_current = sc8989x_set_icl,
    .get_input_current = sc8989x_get_icl,
    .get_min_input_current = NULL,
    /* MIVR */
    .set_mivr = sc8989x_set_ivl,
    .get_mivr = sc8989x_get_ivl,
    .get_mivr_state = NULL,
    /* ADC */
    .get_adc = NULL,
    .get_vbus_adc = sc8989x_get_vbus_adc,
    .get_ibus_adc = sc8989x_get_ibus_adc,
    .get_ibat_adc = NULL,
    .get_tchg_adc = NULL,
    .get_zcv = NULL,
    /* charing termination */
    .set_eoc_current = sc8989x_set_ieoc,
    .enable_termination = sc8989x_enable_te,
    .reset_eoc_state = NULL,
    .safety_check = NULL,
    .is_charging_done = sc8989x_is_charging_done,
    /* power path */
    .enable_powerpath = sc8989x_enable_powerpath,
    .is_powerpath_enabled = sc8989x_is_power_path_enable,
    /* timer */
    .enable_safety_timer = sc8989x_set_safety_timer,
    .is_safety_timer_enabled = sc8989x_is_safety_timer_enabled,
    .kick_wdt = sc8989x_kick_wdt,
    /* AICL */
    .run_aicl = NULL,
    /* PE+/PE+20 */
    .send_ta_current_pattern = NULL,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = sc8989x_send_ta20_current_pattern,
    .reset_ta = sc8989x_set_ta20_reset,
    .enable_cable_drop_comp = NULL,
    /* OTG */
    .set_boost_current_limit = sc8989x_set_boost_ilmt,
    .enable_otg = sc8989x_set_otg,
    .enable_discharge = sc8989x_set_otg,
    /* charger type detection */
    .enable_chg_type_det = NULL,
    /* misc */
    .dump_registers = sc8989x_dump_register,
    .enable_hz = sc8989x_enable_hz,
    /* event */
    .event = sc8989x_do_event,
    /* 6pin battery */
    .enable_6pin_battery_charging = NULL,
};

static struct of_device_id sc8989x_charger_match_table[] = {
    {
    .compatible = "sc,sc8989x_charger",
    .data = &pn_data[PN_SC8989X],
    },
    {},
};
MODULE_DEVICE_TABLE(of, sc8989x_charger_match_table);


#if EXT_PD_CHARGER
static void sc8989x_pd_enable(struct work_struct *work) {
    struct sc8989x *sc = container_of(work,struct sc8989x, pd_enable_work.work);	
	sc->pd_charger_enable = get_other_pd_reset_state();
	pr_err("sc->pd_charger_enable=%d\n",sc->pd_charger_enable);	
}

static void sc8989x_pd_connect(struct sc8989x *sc,struct extcon_dev *edev)
{
    union extcon_property_value prop_val;
    //struct bq25700_state state;
    int ret;
    int vol, cur;
    //int vol_idx, cur_idx;
    //int i;

    if (extcon_get_state(edev, EXTCON_CHG_USB_FAST) > 0) {
        ret = extcon_get_property(edev, EXTCON_CHG_USB_FAST,
                      EXTCON_PROP_USB_TYPEC_POLARITY,
                      &prop_val);
        pr_err("usb pd charge...\n");
        vol = prop_val.intval & 0xffff;
        cur = prop_val.intval >> 15;
        if (ret == 0) {
#if EXT_PD_CHARGER
            //sc->pd_charger_enable = true;
            schedule_delayed_work(&sc->pd_enable_work,msecs_to_jiffies(1000));
            pr_err("vol==%d cur==%d sc->pd_charger_enable=%d\n",vol, cur, sc->pd_charger_enable);
            //pd_power=5*ichg_limit/1000
#endif
            //sc8989x_set_chargecurrent(sc, sc->ichg_limit);
            msleep(150);
            sc->pd_input_limit = cur;
            sc8989x_set_input_current_limit(sc, sc->pd_input_limit);
        }

        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        power_supply_changed(sc->psy);
    }
}

static void sc8989x_pd_evt_worker(struct work_struct *work)
{
    struct sc8989x *sc = container_of(work,struct sc8989x, pd_work.work);

    struct extcon_dev *edev = sc->cable_edev;

    sc8989x_pd_connect(sc, edev);
}

static int sc8989x_pd_evt_notifier(struct notifier_block *nb,
                   unsigned long event,
                   void *ptr)
{
    struct sc8989x *sc = container_of(nb, struct sc8989x, cable_pd_nb);

    queue_delayed_work(sc->usb_charger_wq, &sc->pd_work,msecs_to_jiffies(10));

    return NOTIFY_DONE;
}

static int sc8989x_register_pd_nb(struct sc8989x *sc)
{
    if (sc->cable_edev) {
        INIT_DELAYED_WORK(&sc->pd_work, sc8989x_pd_evt_worker);
        sc->cable_pd_nb.notifier_call = sc8989x_pd_evt_notifier;
        extcon_register_notifier(sc->cable_edev,
                     EXTCON_CHG_USB_FAST,
                     &sc->cable_pd_nb);
		//INIT_DELAYED_WORK(&sc->pd_enable_work, sc8989x_pd_enable);//jnier add 20230609
    }

    return 0;
}

static long sc8989x_init_usb(struct sc8989x *sc)
{
    struct extcon_dev *edev;
    struct device *dev = sc->dev;

    sc->usb_charger_wq = create_singlethread_workqueue("sc8989x-usb-wq");

    /* type-C */
    edev = extcon_get_edev_by_phandle(dev, 0);
    if (IS_ERR(edev)) {
        if (PTR_ERR(edev) != -EPROBE_DEFER)
            dev_err(dev, "Invalid or missing extcon dev0\n");
        sc->cable_edev = NULL;
    } else {
        sc->cable_edev = edev;
    }
    
    sc8989x_register_pd_nb(sc);

    if (sc->cable_edev) {
        schedule_delayed_work(&sc->pd_work, 0);
    }

    return 0;
}
#endif

static int sc8989x_charger_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct sc8989x *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;

    int ret = 0;

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8989x), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->dev = &client->dev;
    sc->client = client;

    i2c_set_clientdata(client, sc);
	sc->pp_en = true;//jnier add 20240905

    mutex_init(&sc->i2c_rw_lock);
    mutex_init(&sc->attach_lock);

    ret = sc8989x_detect_device(sc);
    if (ret) {
        pr_err("No sc8989x device found!\n");
        return -ENODEV;
    }

    match = of_match_node(sc8989x_charger_match_table, node);
    if (match == NULL) {
        pr_err("device tree match not found\n");
        return -EINVAL;
    }

    sc->platform_data = sc8989x_parse_dt(node, sc);

    if (!sc->platform_data) {
        pr_err("No platform data provided.\n");
        return -EINVAL;
    }

    ret = sc8989x_chg_init_psy(sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to init power supply\n");
        return -EINVAL;
    }

    ret = sc8989x_init_device(sc);
    if (ret) {
        pr_err("Failed to init device\n");
        return ret;
    }

    if (strcmp(sc->chg_dev_name, "secondary_chg") != 0) {
        sc8989x_register_interrupt(sc);
    }

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    INIT_DELAYED_WORK(&sc->force_detect_dwork, sc8989x_force_detection_dwork_handler);
    INIT_DELAYED_WORK(&sc->psy_dwork, sc8989x_inform_psy_dwork_handler);
#endif

    sc->chg_dev = charger_device_register(sc->chg_dev_name,
                        &client->dev, sc,
                        &sc8989x_chg_ops,
                        &sc8989x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        return ret;
    }

#if EXT_PD_CHARGER
    sc8989x_init_usb(sc);
    INIT_DELAYED_WORK(&sc->pd_enable_work, sc8989x_pd_enable);
#endif

    ret = sysfs_create_group(&sc->dev->kobj, &sc8989x_attr_group);
    if (ret)
        dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);


//Leo 20230426
    if (!strcmp(sc->chg_dev_name, "secondary_chg")) {
        sc8989x_enable_hz(sc->chg_dev, 0);
    } else {
        determine_initial_status(sc);
    }

#if (CONFIG_WB_CHARGER_FAKE_FULL_STATE_TIME > 0)
	battery_fake_full_kthread_hrtimer_init();
#endif

    pr_err("sc8989x probe successfully, Part Num:%d, Revision:%d\n!",
        sc->part_no, sc->revision);

    return 0;
}

static int sc8989x_charger_remove(struct i2c_client *client)
{
    struct sc8989x *sc = i2c_get_clientdata(client);

    sysfs_remove_group(&sc->dev->kobj, &sc8989x_attr_group);

    return 0;
}

static void sc8989x_charger_shutdown(struct i2c_client *client)
{
    struct sc8989x *sc = i2c_get_clientdata(client);
    pr_err("%s\n", __func__);		
	sc8989x_adc_stop(sc);
}

static int sc8989x_suspend(struct device *dev)
{
    struct sc8989x *sc = dev_get_drvdata(dev);

    pr_err("%s\n", __func__);

    if (device_may_wakeup(dev)){
        enable_irq_wake(sc->irq);
    }

    disable_irq(sc->irq);

    return 0;
}

static int sc8989x_resume(struct device *dev)
{
    struct sc8989x *sc = dev_get_drvdata(dev);

    pr_err("%s\n", __func__);
    enable_irq(sc->irq);
    if (device_may_wakeup(dev)){
        disable_irq_wake(sc->irq);
    }

    return 0;
}

static SIMPLE_DEV_PM_OPS(sc8989x_pm_ops, sc8989x_suspend, sc8989x_resume);

static struct i2c_driver sc8989x_charger_driver = {
    .driver = {
        .name = "sc8989x-charger",
        .owner = THIS_MODULE,
        .of_match_table = sc8989x_charger_match_table,
        .pm = &sc8989x_pm_ops,
    },

    .probe = sc8989x_charger_probe,
    .remove = sc8989x_charger_remove,
    .shutdown = sc8989x_charger_shutdown,

};

module_i2c_driver(sc8989x_charger_driver);

MODULE_DESCRIPTION("SC SC8989X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");
