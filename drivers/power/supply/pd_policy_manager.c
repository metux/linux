// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#define pr_fmt(fmt)	"[SC-USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include "pd_policy_manager.h"
#include "charger_class.h"
#include "mtk_charger.h"
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20220630
#include <mt-plat/csci.h>
#endif

//config battery charge full voltage
#define BATT_MAX_CHG_VOLT           4400

//config fast charge current
#define BATT_FAST_CHG_CURR          6000

//config vbus max voltage
#define	BUS_OVP_THRESHOLD           11000

//config open CP vbus/vbat
#define BUS_VOLT_INIT_UP            210 / 100
#define BUS_VOLT_MIN                207 / 100
#define BUS_VOLT_MAX                215 / 100

//config monitor time (ms)
#define PM_WORK_RUN_INTERVAL        300

#define BAT_VOLT_LOOP_LMT           BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT           BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT           BUS_OVP_THRESHOLD

#ifndef CHG_VBUS_OV_STATUS
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#endif

//#define CONFIG_WB_FUSB302_SUPPORT //Leo 20230610

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) \
    KBUILD_MODNAME "SC manager: [%s:%d] " fmt, __func__, __LINE__

extern void ext_charger_ic_function_control(bool enable);
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230707
extern void charger_dev_set_sw_can_charge(bool status);
int usbpd_get_uisoc(struct usbpd_pm *pdpm);
#endif
#if defined(CONFIG_WB_FUSB302_SUPPORT) //Leo 20230610
extern int fusb302_set_apdo_charging_policy(uint8_t policy, int mv, int ma);
extern uint8_t fusb302_inquire_typec_attach_state(void);
extern int fusb302_dpm_pd_request(int mv, int ma);
int fusb302_inquire_pd_source_apdo(uint8_t apdo_type,
			uint8_t *cap_i, struct tcpm_power_cap_val *cap_val);
extern void fusb32_get_pps_status(void);
#endif

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected);

static unsigned int cust_get_chr_notfy_code(void)
{
	static struct power_supply *chg_psy = NULL;
	static struct mtk_charger *info = NULL;

	if (chg_psy == NULL || IS_ERR(chg_psy)) { 
		chg_psy = power_supply_get_by_name("mtk-master-charger");
		if (chg_psy == NULL || IS_ERR(chg_psy)) {
			pr_err("%s Couldn't get chg_psy\n", __func__);
			return -EINVAL;
		}
	}

	if (info == NULL) {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		if (info == NULL) {
			pr_err("%s get mtk_charger struct failed !\n",__func__);
			return -EINVAL;
		}
	}

	return info->notify_code;
}


enum {
    PM_ALGO_RET_OK,
    PM_ALGO_RET_CHG_DISABLED,
    PM_ALGO_RET_TAPER_DONE,
};

static struct pdpm_config pm_config = {
    .bat_volt_lp_lmt            = BAT_VOLT_LOOP_LMT,
    .bat_curr_lp_lmt            = BAT_CURR_LOOP_LMT,
    .bus_volt_lp_lmt            = BUS_VOLT_LOOP_LMT,
    .bus_curr_lp_lmt            = (BAT_CURR_LOOP_LMT >> 1),

    //config CP to main charger current(ma)
    .fc2_taper_current          = 1000,
    //config adapter voltage step(PPS:1-->20mV)
    .fc2_steps                  = 1,
    //config adapter pps pdo min voltage
    .min_adapter_volt_required  = 11000,
    //config adapter pps pdo min curremt
    .min_adapter_curr_required  = 1000,
    //config CP charge min vbat voltage
    .min_vbat_for_cp            = 3500,
    //config standalone(false) or master+slave(true) CP
    .cp_sec_enable              = false,
    //config CP charging, main charger is disable
    .fc2_disable_sw			    = true,
    .ibus_limit_offset          = 0,
};

static struct usbpd_pm *__pdpm;

static void usbpd_check_charger_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->chg_psy) {
		pdpm->chg_psy = power_supply_get_by_name("charger");
		if (!pdpm->chg_psy) {
			pr_err("chg_psy not found\n");
		}
	}
}

static int usbpd_check_primary_charger(struct usbpd_pm *pdpm)
{
	int ret =0;
	pdpm->charger_device = get_charger_by_name("primary_chg");

	if (pdpm->charger_device){
		pr_err("%s: Found primary charger \n",__func__);
		ret=1;
	}else{
		pr_err("%s: *** Error : can't find primary charger ***\n",__func__);
		ret =0;
	}

	return ret;
}

/****************Charge Pump API*****************/
static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_psy) {
        if (pm_config.cp_sec_enable)
            pdpm->cp_psy = power_supply_get_by_name("sc8551-master");
        else
            pdpm->cp_psy = power_supply_get_by_name("sc8551-standalone");
        if (!pdpm->cp_psy)
            pr_err("cp_psy not found\n");
    }
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_sec_psy) {
        pdpm->cp_sec_psy = power_supply_get_by_name("sc8551-slave");
        if (!pdpm->cp_sec_psy)
            pr_err("cp_sec_psy not found\n");
    }
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_psy(pdpm);

    if (!pdpm->cp_psy)
        return;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->cp.vbus_volt = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp.ibus_curr = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
    if (!ret)
        pdpm->cp.vbat_volt = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibat_volt = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CHARGE_COUNTER, &val);
    if (!ret) {
        pdpm->cp.vbus_err_low = !!(val.intval & 0x20);
        pdpm->cp.vbus_err_high = !!(val.intval & 0x10);
    }

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_TEMP, &val);
    if (!ret)
        pdpm->cp.die_temp = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (!ret)
        pdpm->cp.charge_enabled = val.intval;
}

static void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pm_config.cp_sec_enable)
        return;

    usbpd_check_cp_sec_psy(pdpm);
    
    if (!pdpm->cp_sec_psy)
        return;

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp_sec.ibus_curr = val.intval; 

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (!ret)
        pdpm->cp_sec.charge_enabled = val.intval;
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_psy(pdpm);

    if (!pdpm->cp_psy)
        return -ENODEV;

    val.intval = enable;
    ret = power_supply_set_property(pdpm->cp_psy, 
            POWER_SUPPLY_PROP_ONLINE, &val);

    return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_sec_psy(pdpm);
    
    if (!pdpm->cp_sec_psy)
        return -ENODEV;

    val.intval = enable;
    ret = power_supply_set_property(pdpm->cp_sec_psy, 
            POWER_SUPPLY_PROP_ONLINE, &val);
    
    return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_psy(pdpm);

    if (!pdpm->cp_psy)
        return -ENODEV;

    ret = power_supply_get_property(pdpm->cp_psy, 
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (!ret)
        pdpm->cp.charge_enabled = !!val.intval;

    return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_sec_psy(pdpm);

    if (!pdpm->cp_sec_psy) 
        return -ENODEV;

    ret = power_supply_get_property(pdpm->cp_sec_psy, 
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (!ret)
        pdpm->cp_sec.charge_enabled = !!val.intval;
    
    return ret;
}

static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool enable) //主充20230616
{
    /*TODO: config main charger enable or disable
    If the main charger needs to take 100mA current when the CC charging, you 
    can set the max current to 100mA when disable
    */

    /*int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("battery");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    if(enable)   val.intval = 3000000;
    else         val.intval = 100000;

    ret = power_supply_set_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);

    return ret;*/

#if IS_ENABLED(CONFIG_CHARGER_SC8960X) //Leo 20231019
    bool is_hz_mode = true;
    int uisoc;
    uisoc = usbpd_get_uisoc(pdpm);
    charger_dev_get_hz_status(pdpm->charger_device, &is_hz_mode);
#endif

    if (enable) {
        printk("LQ >> %s enable main charger \n",__func__);
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230707
        charger_dev_set_sw_can_charge(true);
#endif
#if IS_ENABLED(CONFIG_CHARGER_SC8960X) //Leo 20231019
        if (is_hz_mode == true) {
            charger_dev_enable_hz(pdpm->charger_device, false);
        }
#endif
        ext_charger_ic_function_control(false);
    } else {
        printk("LQ >>> %s disable main charger\n",__func__);
#if IS_ENABLED(CONFIG_CHARGER_SC8851) //Leo 20230707
        charger_dev_set_sw_can_charge(false);
#endif
#if IS_ENABLED(CONFIG_CHARGER_SC8960X) //Leo 20231019
        if (uisoc <= 91) {
            charger_dev_enable_hz(pdpm->charger_device, true);
        }
#endif
        charger_dev_enable(pdpm->charger_device, false);
        ext_charger_ic_function_control(true);
    }
    return 0 ;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
    /*TODO: check main charger enable or disable
    */
    /*int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("battery");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
    if (!ret)
    {
        if(val.intval == 100000)  pdpm->sw.charge_enabled = false;
        else  pdpm->sw.charge_enabled = true;
    }

    return ret;*/
    bool enable = false;
    bool is_hz_mode = true;
    charger_dev_get_hz_status(pdpm->charger_device, &is_hz_mode);

    charger_dev_is_enabled(pdpm->charger_device, &enable);
    if (enable) {
        printk("LQ >>> %s get main charger enable\n",__func__);
        pdpm->sw.charge_enabled = true;
    }else{
        printk("LQ >>> %s get main charger disable\n",__func__);
        pdpm->sw.charge_enabled = false;
        if (is_hz_mode == true) {
            charger_dev_enable_hz(pdpm->charger_device, false);
        }
    }

    return 0;
}

static void usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
    usbpd_pm_check_sw_enabled(pdpm);
}

/***************PD API****************/
static inline int check_typec_attached_snk(struct usbpd_pm *pdpm)
{
#if defined(CONFIG_WB_FUSB302_SUPPORT) //Leo 20230610
    pr_info("fusb302_inquire_typec_attach_state():%d TYPEC_ATTACHED_SNK:%d \n",fusb302_inquire_typec_attach_state(), TYPEC_ATTACHED_SNK);
    if (fusb302_inquire_typec_attach_state() != TYPEC_ATTACHED_SNK) {
        return -EINVAL;
    }
#else
    if (cust_get_chr_notfy_code() != 0) //Leo add for it must call for ensure the tcpc regiter ok 20230815
        return -EINVAL;

    if (tcpm_inquire_typec_attach_state(pdpm->tcpc) != TYPEC_ATTACHED_SNK)
        return -EINVAL;
#endif
    return 0;
}

static int usbpd_pps_enable_charging(struct usbpd_pm *pdpm, bool en,
                u32 mV, u32 mA)
{
    int ret, cnt = 0;

    if (check_typec_attached_snk(pdpm) < 0)
        return -EINVAL;
    pr_err("en = %d, %dmV, %dmA\n", en, mV, mA);

#if defined(CONFIG_WB_FUSB302_SUPPORT) //Leo 20230610
    do {
        if (en)
            ret = fusb302_set_apdo_charging_policy(DPM_CHARGING_POLICY_PPS, mV, mA);
        else
            ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);
#else
    do {
        if (en)
            ret = tcpm_set_apdo_charging_policy(pdpm->tcpc,
                DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
        else
            ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);
#endif

    if (ret != TCP_DPM_RET_SUCCESS)
        pr_err("fail(%d)\n", ret);
    return ret > 0 ? -ret : ret;
}

static bool usbpd_get_pps_status(struct usbpd_pm *pdpm)
{
    int ret, apdo_idx = -1;
    struct tcpm_power_cap_val apdo_cap = {0};
    u8 cap_idx;

    pr_err("usbpd_get_pps_status ++\n");
    if (check_typec_attached_snk(pdpm) < 0)
        return false;

    if (!pdpm->is_pps_en_unlock) {
        pr_err("pps en is locked\n");
        return false;
    }

   // pr_err("%d mv ~ %d mv, %d ma\n",pdpm->p_vmin, pdpm->p_ma, pdpm->p_vmax);

    /* select TA boundary */
    cap_idx = 0;
    while (1) {
#if defined(CONFIG_WB_FUSB302_SUPPORT) //Leo 20230610
        ret = fusb302_inquire_pd_source_apdo(
                        TCPM_POWER_CAP_APDO_TYPE_PPS,
                        &cap_idx, &apdo_cap);
#else
        ret = tcpm_inquire_pd_source_apdo(pdpm->tcpc,
                        TCPM_POWER_CAP_APDO_TYPE_PPS,
                        &cap_idx, &apdo_cap);
#endif
        if (ret != TCP_DPM_RET_SUCCESS) {
            pr_err("inquire pd apdo fail(%d)\n", ret);
            break;
        }

        pr_err("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
            apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
            apdo_cap.pwr_limit);

        /*
        * !(apdo_cap.min_mv <= data->vcap_min &&
        *   apdo_cap.max_mv >= data->vcap_max &&
        *   apdo_cap.ma >= data->icap_min)
        */
        if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
            apdo_cap.ma < pm_config.min_adapter_curr_required)
            continue;
        if (apdo_idx == -1) {
            apdo_idx = cap_idx;
            pdpm->apdo_max_volt = apdo_cap.max_mv;
            pdpm->apdo_max_curr = apdo_cap.ma;
        } else {
            if (apdo_cap.ma > pdpm->apdo_max_curr) {
                apdo_idx = cap_idx;
                pdpm->apdo_max_volt = apdo_cap.max_mv;
                pdpm->apdo_max_curr = apdo_cap.ma;
            }
        }

        break;
    }

    if (apdo_idx != -1){
        pr_err("select potential cap_idx[%d]\n", cap_idx);
        ret = usbpd_pps_enable_charging(pdpm, true, 5000, 3000);
        if (ret != TCP_DPM_RET_SUCCESS)
            return false;
        return true;
    }
    return false;
}

/*
TCP_DPM_RET_SUCCESS = 0, 
TCP_DPM_RET_SENT = 0, 
TCP_DPM_RET_VDM_ACK = 0, 

TCP_DPM_RET_DENIED_UNKNOWN,
TCP_DPM_RET_DENIED_NOT_READY,
TCP_DPM_RET_DENIED_LOCAL_CAP,
TCP_DPM_RET_DENIED_PARTNER_CAP,
TCP_DPM_RET_DENIED_SAME_ROLE,
TCP_DPM_RET_DENIED_INVALID_REQUEST, //== 6
TCP_DPM_RET_DENIED_REPEAT_REQUEST,
*/
#ifndef TCP_DPM_RET_DENIED_INVALID_REQUEST
#define TCP_DPM_RET_DENIED_INVALID_REQUEST    6
#endif

static int usbpd_select_pdo(struct usbpd_pm *pdpm, u32 mV, u32 mA)
{
    int ret, cnt = 0;

    if (check_typec_attached_snk(pdpm) < 0)
        return -EINVAL;
    pr_err("%dmV, %dmA\n", mV, mA);

    do {
#if defined(CONFIG_WB_FUSB302_SUPPORT) //Leo 20230610
        ret = fusb302_dpm_pd_request(mV, mA);
        msleep(50);
        fusb32_get_pps_status();
        msleep(50);
#else
        ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
#endif
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);

    if (ret != TCP_DPM_RET_SUCCESS)
        pr_err("fail(%d)\n", ret);

//Leo add for after pd hardrest, if current is pd2.0 , start pdo 20240105
    if (ret == TCP_DPM_RET_DENIED_INVALID_REQUEST) {
        usbpd_pps_enable_charging(pdpm, true, mV, mA);
    }
//Leo add end

    return ret > 0 ? -ret : ret;
}

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
                    unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, tcp_nb);
    struct tcp_notify *noti = data;

    switch (event) {
    case TCP_NOTIFY_PD_STATE:
        switch (noti->pd_state.connected) {
        case PD_CONNECT_NONE:
            pr_err("detached\n");
            pdpm->is_pps_en_unlock = false;
            pdpm->hrst_cnt = 0;
            break;
        case PD_CONNECT_HARD_RESET:
            pdpm->hrst_cnt++;
            pr_err("pd hardreset, cnt = %d\n",
                pdpm->hrst_cnt);
            pdpm->is_pps_en_unlock = false;
            break;
        case PD_CONNECT_PE_READY_SNK_APDO:
            if (pdpm->hrst_cnt < 5) {
                pr_err("en unlock\n");
                pdpm->is_pps_en_unlock = true;
            }
            break;
        default:
            break;
        }
    default:
        break;
    }
	
	if (pdpm->usb_psy) {
		power_supply_changed(pdpm->usb_psy);
	}
	
    return NOTIFY_OK;
}

static void usbpd_check_tcpc(struct usbpd_pm *pdpm)
{
    int ret;

    if (!pdpm->tcpc) {
        pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!pdpm->tcpc) {
            pr_err("get tcpc dev fail\n");
            return;
        }
        pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
        ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
                        TCP_NOTIFY_TYPE_USB);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
        }
    }
}

int usbpd_cust_init(struct usbpd_pm *pdpm)
{
    int ret = 0;
    struct power_supply *bat_psy = NULL;

    if (!pdpm->tcpc) {
        pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!pdpm->tcpc) {
            pr_err("get tcpc dev fail\n");
            return -ENODEV;
        }
    }

    bat_psy = pdpm->bat_psy;
    if (!pdpm->bat_psy) {
        pr_notice("%s retry to get bat_psy\n", __func__);
        bat_psy = devm_power_supply_get_by_phandle(pdpm->dev, "gauge");
        pdpm->bat_psy = bat_psy;
    }

    if (IS_ERR_OR_NULL(bat_psy)) {
        pr_err("%s Couldn't get bat_psy\n", __func__);
        ret = -ENODEV;
    }

    pr_info("%s:%d\n", __func__,
        ret);
    return ret;
}

int usbpd_get_uisoc(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval prop = {0};
    struct power_supply *bat_psy = NULL;

    if (!pdpm->tcpc) {
        pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!pdpm->tcpc) {
            pr_err("get tcpc dev fail\n");
            return -ENODEV;
        }
    }

    bat_psy = pdpm->bat_psy;
    if (!pdpm->bat_psy) {
        pr_notice("%s retry to get bat_psy\n", __func__);
        bat_psy = devm_power_supply_get_by_phandle(pdpm->dev, "gauge");
        pdpm->bat_psy = bat_psy;
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

/*******************main charger API********************/
static void usbpd_check_usb_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->usb_psy) { 
        pdpm->usb_psy = power_supply_get_by_name("charger");
        if (!pdpm->usb_psy)
            pr_err("usb psy not found!\n");
    }
}

static int usbpd_update_ibat_curr(struct usbpd_pm *pdpm)
{
    /*TODO: update ibat and vbat by gauge
    */
    /*int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->bms_psy) {
        pdpm->bms_psy = power_supply_get_by_name("bms");
        if (!pdpm->bms_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->sw.ibat_curr= -(int)(val.intval/1000);

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->sw.vbat_volt = (int)(val.intval/1000);

    return ret;*/
    int vbat=5000000, ibat=500;
	
    //charger_dev_get_vbat(pdpm->charger_device, &vbat);
	//charger_dev_get_ibat(pdpm->charger_device, &ibat);
	pdpm->sw.ibat_curr = (ibat/1000);
	pdpm->sw.vbat_volt = (vbat/1000);

	printk("LQ >>> %s  ibat=%d,vbat=%d\n",__func__,pdpm->sw.ibat_curr,pdpm->sw.vbat_volt);

     return 0;
}


static int usbpd_update_ibus_curr(struct usbpd_pm *pdpm)
{
    /*TODO: update ibus of main charger
    */
    /*int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("usb");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val);
    if (!ret)
        pdpm->sw.ibus_curr = (int)(val.intval/1000);

    return ret;*/
	int ibus;
		
	charger_dev_get_ibus(pdpm->charger_device, &ibus);
	pdpm->sw.ibus_curr = (ibus/1000);
	printk("LQ >>> %s  ibus=%d\n",__func__,(ibus/1000));
		
	return 0;

}

static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
    bool retValue;

    retValue = usbpd_get_pps_status(pdpm);
    if (retValue)
        pdpm->pps_supported = true;

    
    if (pdpm->pps_supported)
        pr_info("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
                pdpm->apdo_selected_pdo,
                pdpm->apdo_max_volt,
                pdpm->apdo_max_curr);
    else
        pr_info("Not qualified PPS adapter\n");
}

#define TAPER_TIMEOUT	(5000 / PM_WORK_RUN_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (500 / PM_WORK_RUN_INTERVAL)
static int usbpd_pm_fc2_charge_algo(struct usbpd_pm *pdpm) //Leo  20230610
{
    int steps;
    int step_vbat = 0;
    int step_ibus = 0;
    int step_ibat = 0;
    int ibus_total = 0;
    int ibat_limit = 0;
    static int ibus_limit;
#if 1
    int uisoc = 0;
#endif

    if (ibus_limit == 0)
        ibus_limit = pm_config.bus_curr_lp_lmt;

    /* reduce bus current in cv loop */
    if (pdpm->vbat_now > pm_config.bat_volt_lp_lmt - 50) {
        if (pdpm->ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
            pdpm->ibus_lmt_change_timer = 0;
            ibus_limit = pm_config.bus_curr_lp_lmt;
        }
    } else if (pdpm->vbat_now < pm_config.bat_volt_lp_lmt - 250) {
        ibus_limit = pm_config.bus_curr_lp_lmt;
        pdpm->ibus_lmt_change_timer = 0;
    } else {
        pdpm->ibus_lmt_change_timer = 0;
    }

    ibus_limit = min(pdpm->apdo_max_curr, ibus_limit);
#if defined(M101TB_DG_PT2_531) //Leo 20231019
    if (ibus_limit == 3000)
         ibus_limit += pm_config.ibus_limit_offset;
    pr_info("usbpd charger Leo ibus_limit:%d %d\n",ibus_limit, pm_config.ibus_limit_offset);
#endif

    /* battery voltage loop*/
    if (pdpm->vbat_now > pm_config.bat_volt_lp_lmt) //Leo 
        step_vbat = -pm_config.fc2_steps;
    else if (pdpm->vbat_now < pm_config.bat_volt_lp_lmt - 7)
        step_vbat = pm_config.fc2_steps;


    /* battery charge current loop*/
    /*TODO: thermal contrl
    * bat_limit = min(pm_config.bat_curr_lp_lmt, fcc_curr);
    */
    ibat_limit = pm_config.bat_curr_lp_lmt;

    if (pdpm->ibat_now < ibat_limit)
        step_ibat = pm_config.fc2_steps;
    else if (pdpm->sw.ibat_curr > ibat_limit + 100)
        step_ibat = -pm_config.fc2_steps;

    /* bus current loop*/
    ibus_total = pdpm->cp.ibus_curr + pdpm->sw.ibus_curr;

    if (pm_config.cp_sec_enable)
        ibus_total += pdpm->cp_sec.ibus_curr;

    if (ibus_total < ibus_limit - 50)
        step_ibus = pm_config.fc2_steps;
    else if (ibus_total > ibus_limit)
        step_ibus = -pm_config.fc2_steps;

    steps = min(min(step_vbat, step_ibus), step_ibat);

    /* check if cp disabled due to other reason*/
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }

    if (!pdpm->cp.charge_enabled || (pm_config.cp_sec_enable && 
        !pdpm->cp_sec_stopped && !pdpm->cp_sec.charge_enabled)) {
        pr_notice("cp.charge_enabled:%d  %d  %d\n",
                pdpm->cp.charge_enabled, pdpm->cp.vbus_err_low, pdpm->cp.vbus_err_high);
        return PM_ALGO_RET_CHG_DISABLED;
    }

    /* charge pump taper charge */
    if (pdpm->vbat_now > pm_config.bat_volt_lp_lmt - 50 
            && pdpm->ibat_now < pm_config.fc2_taper_current) {
        if (pdpm->fc2_taper_timer++ > TAPER_TIMEOUT) {
            pr_notice("charge pump taper charging done\n");
            pdpm->fc2_taper_timer = 0;
            return PM_ALGO_RET_TAPER_DONE;
        }
    } else {
        pdpm->fc2_taper_timer = 0;
    }

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20230711
	if (csci_exist("ro.vendor.exit.cp.charge")) { //just for debug
		int temp = 0;
		temp = csci_integer("ro.vendor.exit.cp.charge",0);
		if (temp > 0) {
			pdpm->fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	}
#endif

#if 1 //Leo 20230621
    //here to get ui_soc
    uisoc = usbpd_get_uisoc(pdpm);
    if (uisoc > 90) {
        pdpm->fc2_taper_timer = 0;
        return PM_ALGO_RET_TAPER_DONE;
    }
#endif

    /*TODO: customer can add hook here to check system level 
        * thermal mitigation*/

    pr_err("%s %d %d %d all %d\n", __func__,  
            step_vbat, step_ibat, step_ibus, steps);

    pdpm->request_voltage += steps * 20;

#if defined(M101TB_DG_PT2_531) //Leo 20231019
    if (pdpm->request_voltage > pdpm->apdo_max_volt - 500)
        pdpm->request_voltage = pdpm->apdo_max_volt - 500;
#else
    if (pdpm->request_voltage > pdpm->apdo_max_volt - 1000)
        pdpm->request_voltage = pdpm->apdo_max_volt - 1000;
#endif


    return PM_ALGO_RET_OK;
}

static const unsigned char *pm_str[] = {
    "PD_PM_STATE_ENTRY",
    "PD_PM_STATE_FC2_ENTRY",
    "PD_PM_STATE_FC2_ENTRY_1",
    "PD_PM_STATE_FC2_ENTRY_2",
    "PD_PM_STATE_FC2_ENTRY_3",
    "PD_PM_STATE_FC2_TUNE",
    "PD_PM_STATE_FC2_EXIT",
};

static void usbpd_pm_move_state(struct usbpd_pm *pdpm, enum pm_state state)
{
    pr_err("state change:%s -> %s\n", 
        pm_str[pdpm->state], pm_str[state]);
    pdpm->state = state;
}

//Leo 20230613
static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
    int ret;
    int rc = 0;
    static int tune_vbus_retry;
    static bool stop_sw;
    static bool recover;
#if IS_ENABLED(CONFIG_CHARGER_SC8960X) //Leo 20231019
    bool sw_chr_en;
    bool is_hz_mode = true;
#endif

    pr_err("state phase :%d\n", pdpm->state);
    /*[4785-3952-1-6]*/
    pr_err("cp vbus-vbat-ibus-ibat [%d-%d-%d-%d] cp-s ibus:%d\n",pdpm->cp.vbus_volt,pdpm->cp.vbat_volt,pdpm->cp.ibus_curr,pdpm->cp.ibat_volt,pdpm->cp_sec.ibus_curr);
    pr_err("sw vbat-ibus [%d-%d]\n",pdpm->sw.vbat_volt,pdpm->sw.ibus_curr);

    pdpm->vbat_now = pdpm->cp.vbat_volt;
    pdpm->ibat_now = pdpm->cp.ibat_volt;
    
    switch (pdpm->state) {
    case PD_PM_STATE_ENTRY:
        stop_sw = false;
        recover = false;
        if (pdpm->vbat_now < pm_config.min_vbat_for_cp) {
            pr_notice("batt_volt-%d, waiting...\n", pdpm->vbat_now);
        } else if (pdpm->vbat_now > pm_config.bat_volt_lp_lmt - 100) {
            pr_notice("batt_volt-%d is too high for cp,\
                    charging with switch charger\n", 
                    pdpm->vbat_now);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        } else {
            pr_notice("batt_volt-%d is ok, start flash charging\n", 
                    pdpm->vbat_now);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
        }
        break;

    case PD_PM_STATE_FC2_ENTRY:
        if (pm_config.fc2_disable_sw) {
#if IS_ENABLED(CONFIG_CHARGER_SC8960X) //Leo 20231019
            charger_dev_is_enabled(pdpm->charger_device, &sw_chr_en);
            charger_dev_get_hz_status(pdpm->charger_device, &is_hz_mode);
            pr_info("%s sw_chr_en:%d is_hz_mode:%d \n",__func__,sw_chr_en, is_hz_mode);
            if ((sw_chr_en == true) || (is_hz_mode ==  false)) {
                usbpd_pm_enable_sw(pdpm, false);
                usbpd_pm_check_sw_enabled(pdpm);
            }
#else
            if (pdpm->sw.charge_enabled) {
                usbpd_pm_enable_sw(pdpm, false);
                usbpd_pm_check_sw_enabled(pdpm);
            }
#endif

            if (!pdpm->sw.charge_enabled)
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
        } else {
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
        }
        break;

    case PD_PM_STATE_FC2_ENTRY_1:
        if (pm_config.cp_sec_enable)
            pdpm->request_voltage = pdpm->vbat_now * BUS_VOLT_INIT_UP + 200;
        else {
            if (pdpm->vbat_now < 3600) { //Leo 20230707
                  pdpm->request_voltage = pdpm->vbat_now * BUS_VOLT_INIT_UP + 200;
            } else {
                  pdpm->request_voltage = pdpm->vbat_now * BUS_VOLT_INIT_UP;
            }
        }
            //pdpm->request_voltage = pdpm->vbat_now * BUS_VOLT_INIT_UP; //Leo modified 20230703

        pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
        usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
        //pr_err("request_voltage:%d, request_current:%d\n",
                //pdpm->request_voltage, pdpm->request_current);

        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
        tune_vbus_retry = 0;
        break;

    case PD_PM_STATE_FC2_ENTRY_2:
        pr_err("tune_vbus_retry %d\n", tune_vbus_retry);
        if (pdpm->cp.vbus_err_low || (pdpm->cp.vbus_volt < 
                pdpm->vbat_now * BUS_VOLT_MIN)) {
            tune_vbus_retry++;
            if (pdpm->request_voltage <= 10500 - 20) //Leo 20240105
                pdpm->request_voltage += 20;

            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("vbus_err_low request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        } else if (pdpm->cp.vbus_err_high || (pdpm->cp.vbus_volt > 
                pdpm->vbat_now * BUS_VOLT_MAX)) {
            tune_vbus_retry++;

            if (pdpm->request_voltage >= 6000) //Leo 20240105
                 pdpm->request_voltage -= 20;
            
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("vbus_err_high request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        } else {
            pr_notice("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
            break;
        }
        
        if (tune_vbus_retry > 100) {
            pr_notice("Failed to tune adapter volt into valid range, \
                    charge with switching charger\n");
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        }
        break;
    case PD_PM_STATE_FC2_ENTRY_3:
        usbpd_pm_check_cp_enabled(pdpm);
        if (!pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, true);
            msleep(100);
            usbpd_pm_check_cp_enabled(pdpm);
        }

        if (pm_config.cp_sec_enable) {
            usbpd_pm_check_cp_sec_enabled(pdpm);
            if(!pdpm->cp_sec.charge_enabled) {
                usbpd_pm_enable_cp_sec(pdpm, true);
                msleep(100);
                usbpd_pm_check_cp_sec_enabled(pdpm);
            }
        }

        if (pdpm->cp.charge_enabled) {
            if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
                    || !pm_config.cp_sec_enable) {
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
                pdpm->ibus_lmt_change_timer = 0;
                pdpm->fc2_taper_timer = 0;
            }
        }
        break;

    case PD_PM_STATE_FC2_TUNE:
        ret = usbpd_pm_fc2_charge_algo(pdpm);
        if (ret == PM_ALGO_RET_TAPER_DONE) {
            pr_notice("Move to switch charging:%d\n", ret);
            stop_sw = false;
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else if (ret == PM_ALGO_RET_CHG_DISABLED) {
            pr_notice("Move to switch charging, will try to recover \
                    flash charging:%d\n", ret);
            recover = true;
            stop_sw = false;
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else {
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        }
        
        /*stop second charge pump if either of ibus is lower than 750ma during CV*/
        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled 
                && pdpm->vbat_now > pm_config.bat_volt_lp_lmt - 50
                && (pdpm->cp.ibus_curr < 750 || pdpm->cp_sec.ibus_curr < 750)) {
            pr_notice("second cp is disabled due to ibus < 750mA\n");
            usbpd_pm_enable_cp_sec(pdpm, false);
            usbpd_pm_check_cp_sec_enabled(pdpm);
            pdpm->cp_sec_stopped = true;
        }
        break;

    case PD_PM_STATE_FC2_EXIT:
        /* select default 5V*/
#if 1//IS_ENABLED(CONFIG_MTK_CHARGER) //Leo 20230711
        if (!recover) {
            ret = tcpm_set_pd_charging_policy(pdpm->tcpc,
                DPM_CHARGING_POLICY_VSAFE5V, NULL);
        }
        ret = tcpm_dpm_pd_request(pdpm->tcpc, 5000, 3000, NULL);
#else
        usbpd_select_pdo(pdpm, 5000, 3000);
#endif

        if (pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, false);
            usbpd_pm_check_cp_enabled(pdpm);
        }

        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
            usbpd_pm_enable_cp_sec(pdpm, false);
            usbpd_pm_check_cp_sec_enabled(pdpm);
        }

        pr_err(">>>sw state %d   %d\n", stop_sw, pdpm->sw.charge_enabled);
        if (stop_sw && pdpm->sw.charge_enabled)
            usbpd_pm_enable_sw(pdpm, false);
        else if (!stop_sw && !pdpm->sw.charge_enabled)
            usbpd_pm_enable_sw(pdpm, true); 

        usbpd_pm_check_sw_enabled(pdpm);

        if (recover)
            usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
        else
            rc = 1;
        
        break;
    }

    return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pm_work.work);

    usbpd_pm_update_sw_status(pdpm);
    usbpd_update_ibus_curr(pdpm);
    usbpd_pm_update_cp_status(pdpm);
    usbpd_pm_update_cp_sec_status(pdpm);
    usbpd_update_ibat_curr(pdpm);

    if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
        schedule_delayed_work(&pdpm->pm_work,
            msecs_to_jiffies(PM_WORK_RUN_INTERVAL));

}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
    usbpd_pm_enable_cp(pdpm, false);
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_enable_cp_sec(pdpm, false);
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }
    cancel_delayed_work_sync(&pdpm->pm_work);

    if (!pdpm->sw.charge_enabled) {
        usbpd_pm_enable_sw(pdpm, true);
        usbpd_pm_check_sw_enabled(pdpm);
    }

    pdpm->pps_supported = false;
    pdpm->apdo_selected_pdo = 0;

    usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}


static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
    pdpm->pd_active = connected;
    pr_err("[SC manager] >> pd_active %d\n", pdpm->pd_active);
    if (connected) {
        msleep(10);
        usbpd_pm_evaluate_src_caps(pdpm);
        pr_err("[SC manager] >>start cp charging pps support %d\n", 
            pdpm->pps_supported);
        if (pdpm->pps_supported)
            schedule_delayed_work(&pdpm->pm_work, 0); 
        else
            pdpm->pd_active = false;
    } else {
        usbpd_pm_disconnect(pdpm);
    }
}

static void cp_psy_change_work(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
                    cp_psy_change_work);

    pdpm->psy_change_running = false;
}

static void usb_psy_change_work(struct work_struct *work)
{
    int ret;
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
                    usb_psy_change_work);
    union power_supply_propval val = {0,};

    pr_err("[SC manager] >> usb change work\n"); //Leo 20230612

    if (check_typec_attached_snk(pdpm) < 0) {
        if (pdpm->pd_active) {
            usbpd_pd_contact(pdpm, false);
        }
        goto out;
    }
#if 0
    ret = power_supply_get_property(pdpm->usb_psy,
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (ret) {
        pr_err("Failed to get usb pd active state\n");
        goto out;
    }
#else
    ret = power_supply_get_property(pdpm->chg_psy, POWER_SUPPLY_PROP_ONLINE, &val);
    if (ret) {
        pr_err("[SC manager] Failed to get charger pd active state\n");
        goto out;
    }
#endif
    val.intval = 1; //Leo 20230612 add for test 
    pr_err("[SC manager] >> pd_active %d,  val.intval %d\n",
            pdpm->pd_active, val.intval);


    if (!pdpm->pd_active && val.intval){ // 0 0
        usbpd_pd_contact(pdpm, true);
    } else if (pdpm->pd_active && !val.intval) {
        usbpd_pd_contact(pdpm, false);
    }
out:
    pdpm->psy_change_running = false;
#if 0//EXT_PD_CHARGER
    pdpm->pd_charger_enable = false;
    pdpm->is_pps_en_unlock = false;
    pdpm->p_vmax = 0;
    pdpm->p_vmin = 0;
    pdpm->p_ma = 0;
#endif
}


static int usbpd_psy_notifier_cb(struct notifier_block *nb, 
            unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
    struct power_supply *psy = data;
    unsigned long flags;

    if (event != PSY_EVENT_PROP_CHANGED)
        return NOTIFY_OK;

    usbpd_check_charger_psy(pdpm);
    usbpd_check_cp_psy(pdpm);
    usbpd_check_usb_psy(pdpm);
    usbpd_check_tcpc(pdpm);
    usbpd_cust_init(pdpm);

    if (!pdpm->cp_psy || !pdpm->usb_psy)
        return NOTIFY_OK;

    if (psy == pdpm->cp_psy || psy == pdpm->usb_psy) {
        spin_lock_irqsave(&pdpm->psy_change_lock, flags);
        pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
        if (!pdpm->psy_change_running) {
            pdpm->psy_change_running = true;
            if (psy == pdpm->cp_psy)
                schedule_work(&pdpm->cp_psy_change_work);
            else
                schedule_work(&pdpm->usb_psy_change_work);
        }
        spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
    }

    return NOTIFY_OK;
}

#if 0//EXT_PD_CHARGER
static void pd_connect(struct usbpd_pm *pdpm,struct extcon_dev *edev)
{
#if 1 //Leo 20230613
	pr_info("do nothings! \n");
#else
    union extcon_property_value prop_val;
    union extcon_property_value pps_prop_val;
    //struct bq25700_state state;
    int ret;
    int vol, cur;
    //int vol_idx, cur_idx;
    //int i;

    if (extcon_get_state(edev, EXTCON_CHG_USB_FAST) > 0) {
        ret = extcon_get_property(edev, EXTCON_CHG_USB_FAST,
                      EXTCON_PROP_USB_TYPEC_POLARITY,
                      &prop_val);
        ret = extcon_get_property(edev, EXTCON_CHG_USB_FAST,
                      EXTCON_PROP_USB_SS,
                      &pps_prop_val);
        pr_err("pd_connect......\n");
        vol = prop_val.intval & 0xffff;
        cur = prop_val.intval >> 15;
        pdpm->is_pps_en_unlock = pps_prop_val.intval;
        if (ret == 0) {
            pdpm->pd_charger_enable = true;
            pr_err("vol==%d cur==%d pdpm->pd_charger_enable=%d pdpm->is_pps_en_unlock=%d\n",vol, cur, pdpm->pd_charger_enable, pdpm->is_pps_en_unlock);
        }
    }
#endif
}

static void pd_evt_worker(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work,struct usbpd_pm, pd_work.work);

    struct extcon_dev *edev = pdpm->cable_edev;

    pd_connect(pdpm, edev);
}

static int pd_evt_notifier(struct notifier_block *nb,
                   unsigned long event,
                   void *ptr)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, cable_pd_nb);

    queue_delayed_work(pdpm->usb_charger_wq, &pdpm->pd_work,msecs_to_jiffies(10));

    return NOTIFY_DONE;
}

static int pd_register_pd_nb(struct usbpd_pm *pdpm)
{
    if (pdpm->cable_edev) {
        INIT_DELAYED_WORK(&pdpm->pd_work, pd_evt_worker);
        pdpm->cable_pd_nb.notifier_call = pd_evt_notifier;
        extcon_register_notifier(pdpm->cable_edev,
                     EXTCON_CHG_USB_FAST,
                     &pdpm->cable_pd_nb);
    }

    return 0;
}

static void pps_connect(struct usbpd_pm *pdpm,struct extcon_dev *edev)
{
    union extcon_property_value prop_val;

    int ret;
    int p_vmin, p_vmax, p_ma;

    if (extcon_get_state(edev, EXTCON_CHG_USB_PD) > 0) {
        ret = extcon_get_property(edev, EXTCON_CHG_USB_PD,
                      EXTCON_PROP_USB_TYPEC_POLARITY,
                      &prop_val);
        if(ret == 0){
            pr_err("pps_connect......n");
            p_vmax = prop_val.intval & 0xffff;
            p_vmin = prop_val.intval >> 15;
            pdpm->is_pps_en_unlock = true;
            pr_err("p_vmax==%d p_vmin = %d pdpm->pd_charger_enable=%d pdpm->is_pps_en_unlock=%d\n",p_vmax, p_vmin, pdpm->pd_charger_enable, pdpm->is_pps_en_unlock);
        }
    }

    if(extcon_get_state(edev, EXTCON_DISP_DP) > 0){
        ret = extcon_get_property(edev, EXTCON_DISP_DP,
              EXTCON_PROP_USB_TYPEC_POLARITY,
              &prop_val);
        if(ret == 0){
            p_ma = prop_val.intval;
            pdpm->is_pps_en_unlock = true;
            pr_err("p_ma==%d p_vmax==%d p_vmin = %d pdpm->pd_charger_enable=%d pdpm->is_pps_en_unlock=%d\n",p_ma, p_vmax, p_vmin, pdpm->pd_charger_enable, pdpm->is_pps_en_unlock);
        }
    }
}

static void pps_evt_worker(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work,struct usbpd_pm, pps_work.work);

    struct extcon_dev *edev = pdpm->cable_edev;

    pps_connect(pdpm, edev);
}

static int pps_evt_notifier(struct notifier_block *nb,
                   unsigned long event,
                   void *ptr)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, cable_pps_nb);

    queue_delayed_work(pdpm->usb_charger_wq, &pdpm->pps_work,msecs_to_jiffies(10));

    return NOTIFY_DONE;
}


static int pd_register_pps_nb(struct usbpd_pm *pdpm)
{
    if (pdpm->cable_edev) {
        INIT_DELAYED_WORK(&pdpm->pps_work, pps_evt_worker);
        pdpm->cable_pps_nb.notifier_call = pps_evt_notifier;
        extcon_register_notifier(pdpm->cable_edev,
                     EXTCON_CHG_USB_PD,
                     &pdpm->cable_pps_nb);
        extcon_register_notifier(pdpm->cable_edev,
                     EXTCON_DISP_DP,
                     &pdpm->cable_pps_nb);
    }

    return 0;
}

static long pd_init_usb(struct usbpd_pm *pdpm)
{
    struct extcon_dev *edev;
    struct device *dev = pdpm->dev;

    pdpm->usb_charger_wq = create_singlethread_workqueue("typec-pd-wq");

    /* type-C */
    edev = extcon_get_edev_by_phandle(dev, 0);
    if (IS_ERR(edev)) {
        if (PTR_ERR(edev) != -EPROBE_DEFER)
            dev_err(dev, "Invalid or missing extcon dev0\n");
        pdpm->cable_edev = NULL;
    } else {
        pdpm->cable_edev = edev;
    }
    
    pd_register_pd_nb(pdpm);//Leo 20230608
    pd_register_pps_nb(pdpm);

    if (pdpm->cable_edev) {
        schedule_delayed_work(&pdpm->pd_work, 0);
    }

    return 0;
}
#endif

static int cust_cp_parse_dts(struct device_node *np, struct pdpm_config *pdata)
{
	int ret;
	ret = of_property_read_u32(np, "batt_max_chg_volt", &pdata->bat_volt_lp_lmt);
	if (ret) {
		pr_err("Failed to read node of bat_volt_lp_lmt\n");
	}

	ret = of_property_read_u32(np, "batt_fast_chg_curr", &pdata->bat_curr_lp_lmt);
	if (ret) {
		pr_err("Failed to read node of bat_curr_lp_lmt\n");
	}

	ret = of_property_read_u32(np, "bus_volt_lp_lmt", &pdata->bus_volt_lp_lmt);
	if (ret) {
		pr_err("Failed to read node of bus_volt_lp_lmt\n");
	}

	ret = of_property_read_u32(np, "bus_curr_lp_lmt", &pdata->bus_curr_lp_lmt);
	if (ret) {
		pr_err("Failed to read node of bus_curr_lp_lmt\n");
	}

	ret = of_property_read_u32(np, "bus_curr_lp_lmt", &pdata->bus_curr_lp_lmt);
	if (ret) {
		pr_err("Failed to read node of bus_curr_lp_lmt\n");
	}

	ret = of_property_read_u32(np, "ibus_limit_offset", &pdata->ibus_limit_offset);
	if (ret) {
		pr_err("Failed to read node of bus_curr_lp_lmt\n");
	}

	return 0;
}


static int usbpd_pm_probe(struct platform_device *pdev)
{
    struct usbpd_pm *pdpm;
    int ret;

    printk("usbpd_pm_probe begin\n");
    pdpm = kzalloc(sizeof(*pdpm), GFP_KERNEL);
    if (!pdpm)
        return -ENOMEM;

    __pdpm = pdpm;

    pdpm->dev = &pdev->dev;

    INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
    INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);

    spin_lock_init(&pdpm->psy_change_lock);

    usbpd_check_cp_psy(pdpm);
    usbpd_check_cp_sec_psy(pdpm);
    usbpd_check_usb_psy(pdpm);
    usbpd_check_tcpc(pdpm);
    usbpd_cust_init(pdpm);
    ret = usbpd_check_primary_charger(pdpm);
    if(!ret){
        printk("LQ >>> %s get charger failed\n",__func__);
        return -1;
     }

#if 0//EXT_PD_CHARGER //Leo 20230608
    pd_init_usb(pdpm);
#endif

#if 1 //Leo 20230718
    cust_cp_parse_dts(pdev->dev.of_node, &pm_config);
    pdpm->sw.charge_enabled = true; //Leo add for init default for sw charge to en_hz_mode,when start cp charge first.
#endif

    INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);

    pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
    power_supply_reg_notifier(&pdpm->nb);

#if 0
	rpmd->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(rpmd->tcpc, &rpmd->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		dev_notice(rpmd->dev, "%s register tcpc notifier fail(%d)\n",
				      __func__, ret);
		goto err_reg_tcpc_notifier;
	}
#endif

    printk("usbpd_pm_probe end\n");

    return 0;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
    power_supply_unreg_notifier(&__pdpm->nb);
    cancel_delayed_work_sync(&__pdpm->pm_work);
    cancel_work_sync(&__pdpm->cp_psy_change_work);
    cancel_work_sync(&__pdpm->usb_psy_change_work);

    return 0;
}

static const struct of_device_id usbpd_pm_of_ids[] = {
    {.compatible = "usbpd_pm",},
    {},
};
MODULE_DEVICE_TABLE(of, usbpd_pm_of_ids);

static struct platform_driver usbpd_pm_driver = {
    .driver = {
        .name = "usbpd_pm",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(usbpd_pm_of_ids),
    },
    .probe = usbpd_pm_probe,
    .remove = usbpd_pm_remove,
};

static int __init _usbpd_pm_init(void)
{
    printk("_usbpd_pm_init\n");
    if (platform_driver_register(&usbpd_pm_driver) != 0) {
        pr_err("failed to register usbpd_pm_driver.\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit _usbpd_pm_exit(void)
{
    printk("_usbpd_pm_exit\n");
    platform_driver_unregister(&usbpd_pm_driver);
}

module_init(_usbpd_pm_init);
module_exit(_usbpd_pm_exit);

MODULE_DESCRIPTION("SC Charge Pump Policy Manager");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
