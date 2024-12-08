// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_basic_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include "mtk_charger.h"

#if defined(M101TB_DG_PT2_531) //Leo 20230706
#ifndef CONFIG_WB_DG_CUST_SUPPORT
#define CONFIG_WB_DG_CUST_SUPPORT
#endif
#endif

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20220630
#include <mt-plat/csci.h>
#endif

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			info->setting.cv = info->sw_jeita.cv;
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
}

static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
			rp != 500 &&
			info->chr_type != POWER_SUPPLY_TYPE_USB &&
			info->chr_type != POWER_SUPPLY_TYPE_USB_CDP)
		return true;

	return false;
}

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

	pr_info("%s entry !\n", __func__);

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL) {
			continue;
		};

		if (info->enable_fast_charging_indicator &&
		    ((alg->alg_id & info->fast_charging_indicator) == 0)) {
			continue;
		}

		chg_alg_set_current_limit(alg, &info->setting);
		state = chg_alg_is_algo_ready(alg);
		pr_info("%s %s ret:%s\n", __func__, dev_name(&alg->dev),
			chg_alg_state_to_str(state));

#if IS_ENABLED(CONFIG_TCPC_FUSB302) //Leo 20240801
#if IS_ENABLED(CONFIG_WB_PD_ALGO_SUPPORT)
		pr_info("%s %s get_other_pd_reset_state:%d \n", __func__, dev_name(&alg->dev),get_other_pd_reset_state());
		if (get_other_pd_reset_state() == true) {
			if ((i == PE2_ID) || (i == PE_ID)) {
				state = ALG_NOT_READY;
			}
		}
#endif
#endif

		if (state == ALG_READY || state == ALG_RUNNING) {
			ret = true;
			break;
		}
	}
	return ret;
}

static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2, *pdata_dvchg, *pdata_dvchg2;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;

#if defined(CONFIG_WB_DG_CUST_SUPPORT) //Leo 20230407
	int temp = info->battery_temp;
	pr_info("%s pd charge Leo temp:%d \n",__func__,temp);
#endif

	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
	pdata_dvchg = &info->chg_data[DVCHG1_SETTING];
	pdata_dvchg2 = &info->chg_data[DVCHG2_SETTING];
	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		is_basic = true;
		pr_info("%s info->usb_unlimited == true, goto done \n",__func__);
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		is_basic = true;
		pr_info("%s info->water_detected == true, goto done \n",__func__);
		goto done;
	}

	if (((info->bootmode == 1) ||
	    (info->bootmode == 5)) && info->enable_meta_current_limit != 0) {
		pdata->input_current_limit = 200000; // 200mA
		is_basic = true;
		pr_info("%s info->bootmode == 1/5, goto done \n",__func__);
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		pr_info("%s info->atm_enabled == true, goto done \n",__func__);
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
				pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		/* NONSTANDARD_CHARGER */
		pdata->input_current_limit =
			info->data.usb_charger_current;
		pdata->charging_current_limit =
			info->data.usb_charger_current;
		is_basic = true;
	} else {
		/*chr_type && usb_type cannot match above, set 500mA*/
		pdata->input_current_limit =
				info->data.usb_charger_current;
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	}


	if (support_fast_charging(info)) //Leo 20230407
		is_basic = false;
	else {
		is_basic = true;
		/* AICL */
		if (!info->disable_aicl)
			charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
		if (is_typec_adapter(info)) {
			if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
				== 3000) {
				pdata->input_current_limit = 3000000;
				pdata->charging_current_limit = 3000000;
			} else if (adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL) == 1500) {
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
			} else {
				chr_err("type-C: inquire rp error\n");
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}

			chr_err("type-C:%d current:%d\n",
				info->pd_type,
				adapter_dev_get_property(info->pd_adapter,
					TYPEC_RP_LEVEL));
		}
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB)
			chr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 350000;
			}
		}
	}

	sc_select_charging_current(info, pdata);

#if IS_ENABLED(CONFIG_WB_DG_CUST_SUPPORT) //Leo 20230506
	if ((temp >= 0) && (temp <= 56)) {
		pdata->thermal_charging_current_limit = -1;
		pdata->thermal_input_current_limit = -1;

		pdata2->thermal_charging_current_limit = -1;
		pdata2->thermal_input_current_limit = -1;
	} 
#endif
#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT)  //jnier add 20240115
	if(csci_exist("charge.thermal.current_limit.disable"))
			if(csci_integer("charge.thermal.current_limit.disable",0) == 1) {
					pdata->thermal_charging_current_limit = -1;
					pdata->thermal_input_current_limit = -1;
					printk("jnier test charge.thermal.current_limit.disable = 1 \n");
			}
#endif

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <=
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		}
		pdata->thermal_throttle_record = true;
	} else
		info->setting.charging_current_limit1 = info->sc.sc_ibat;

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <=
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
		pdata->thermal_throttle_record = true;
	} else
		info->setting.input_current_limit1 = -1;

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <=
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	} else
		info->setting.charging_current_limit2 = info->sc.sc_ibat;

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <=
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	} else
		info->setting.input_current_limit2 = -1;

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1
		&& !info->charger_unlimited
		&& !info->disable_aicl) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
	info->setting.input_current_limit_dvchg1 =
		pdata_dvchg->thermal_input_current_limit;

//caozy add begin 20230418
#if IS_ENABLED(CONFIG_TCPC_FUSB302)
    if(get_other_pd_reset_state()){
#if IS_ENABLED(CONFIG_WB_TD_CUST_SUPPORT)||IS_ENABLED(CONFIG_WB_PD_ALGO_SUPPORT)
		pr_info("---get_other_pd_reset_state = true---\n");
		info->setting.input_current_limit1 = -1;
		info->setting.charging_current_limit1 = -1;
		pdata->input_current_limit = info->setting.input_current_limit1;
		pdata->charging_current_limit = info->setting.charging_current_limit1;
		info->setting.input_current_limit2 = -1;
		info->setting.charging_current_limit2 = -1;
		pdata2->input_current_limit = -1;
		pdata2->charging_current_limit = -1;
#else
		pr_info("---get_other_pd_reset_state = true---\n");
		info->setting.input_current_limit1 = info->data.ac_charger_input_current;
		info->setting.charging_current_limit1 = info->data.ac_charger_current;
		pdata->input_current_limit = info->setting.input_current_limit1;
		pdata->charging_current_limit = info->setting.charging_current_limit1;
		info->setting.input_current_limit2 = 0;
		info->setting.charging_current_limit2 = 0;
		pdata2->input_current_limit = 0;
		pdata2->charging_current_limit = 0;
#endif
    }
#endif

#if IS_ENABLED(CONFIG_WB_DG_CUST_SUPPORT) //Leo 20230407
	if ((temp >= -10) && (temp < 0)) {
		pr_info("%s charge Leo(temp >= -10) && (temp < 0) \n",__func__);
		info->setting.input_current_limit1 = 700000;
		info->setting.charging_current_limit1 = 700000;
		pdata->input_current_limit = info->setting.input_current_limit1;
		pdata->charging_current_limit = info->setting.charging_current_limit1;
		info->setting.input_current_limit2 = 0;
		info->setting.charging_current_limit2 = 0;
		pdata2->input_current_limit = 0;
		pdata2->charging_current_limit = 0;
	} else if ((temp > 56) && (temp <= 60)) {
		pr_info("%s charge Leo (temp > 56) && (temp <= 60) \n",__func__);
		info->setting.input_current_limit1 = 1600000;
		info->setting.charging_current_limit1 = 1600000;
		pdata->input_current_limit = info->setting.input_current_limit1;
		pdata->charging_current_limit = info->setting.charging_current_limit1;
		info->setting.input_current_limit2 = 0;
		info->setting.charging_current_limit2 = 0;
		pdata2->input_current_limit = 0;
		pdata2->charging_current_limit = 0;
	} else if ((temp > 60) || (temp < -10)) {
		pr_info("%s charge Leo(temp > 60) || (temp < -10) \n",__func__);
		info->setting.input_current_limit1 = 0;
		info->setting.charging_current_limit1 = 0;
		pdata->input_current_limit = info->setting.input_current_limit1;
		pdata->charging_current_limit = info->setting.charging_current_limit1;
		info->setting.input_current_limit2 = 0;
		info->setting.charging_current_limit2 = 0;
		pdata2->input_current_limit = 0;
		pdata2->charging_current_limit = 0;
	}
#endif


done:

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -EOPNOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -EOPNOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	/* For TC_018, pleasae don't modify the format */
	/*m:0 chg1:-1,-1,2050,2050 chg2:-1,-1,0,0 dvchg1:-1 sc:2050000 -1 0 type:5:0 usb_unlimited:0 usbif:0 usbsm:0 aicl:-1 atm:0 bm:0 b:1*/
	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d dvchg1:%d sc:%d %d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		_uA_to_mA(pdata_dvchg->thermal_input_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);
	return is_basic;
}

static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false;
	int i;
	int ret;
	int val = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting); //Leo 20230407

	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			info->polling_interval = CHARGING_FULL_INTERVAL;
			chr_err("%s battery full\n", __func__);
		} else {
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			info->polling_interval = CHARGING_INTERVAL;
			chr_err("%s battery recharge\n", __func__);
		}
	}

	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (info->enable_fast_charging_indicator &&
			    ((alg->alg_id & info->fast_charging_indicator) == 0))
				continue;
#if IS_ENABLED(CONFIG_WB_TD_CUST_SUPPORT)||IS_ENABLED(CONFIG_WB_PD_ALGO_SUPPORT)//jnier 20240116
			chr_err("%s: %d %d %d %d \n", __func__,info->enable_hv_charging,
					pdata->input_current_limit,  pdata->charging_current_limit);
#else
			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}
#endif
			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting); //Leo 20230314
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING || ret == ALG_DONE ||
						ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				chg_alg_start_algo(alg); //Leo 20230314
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true ||
		    pdata->charging_current_limit == 0 ||
		    pdata->input_current_limit == 0) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
		if(info->wb_stop_charging_current) { //jnier add 20240829 for bypass mode
			pdata->charging_current_limit=0;
		}
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);

		chr_debug("%s:old_cv=%d,cv=%d, vbat_mon_en=%d\n",
			__func__,
			info->old_cv,
			info->setting.cv,
			info->setting.vbat_mon_en);
		if (info->old_cv == 0 || (info->old_cv != info->setting.cv)
		    || info->setting.vbat_mon_en == 0) {
			charger_dev_enable_6pin_battery_charging(
				info->chg1_dev, false);
			charger_dev_set_constant_voltage(info->chg1_dev,
				info->setting.cv);
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1)
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			info->old_cv = info->setting.cv;
		} else {
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1) {
				info->stop_6pin_re_en = 1;
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			}
		}
	}

	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0)
		charger_dev_enable(info->chg1_dev, false);
	else {
		alg = get_chg_alg_by_name("pe5");
		ret = chg_alg_is_algo_ready(alg);
		if (!(ret == ALG_READY || ret == ALG_RUNNING))
			charger_dev_enable(info->chg1_dev, true);
	}

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	return 0;
}

static int enable_charging(struct mtk_charger *info,
						bool en)
{
	int i;
	struct chg_alg_device *alg;


	chr_err("%s %d\n", __func__, en);

	if (en == false) {
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;
			chg_alg_stop_algo(alg);
		}
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	} else {
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}

static int charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	struct mtk_charger *info =
			container_of(nb, struct mtk_charger, chg1_nb);
	struct chgdev_notify *data = v;
	int i;

	chr_err("%s %lu\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		info->stop_6pin_re_en = 1;
		notify.evt = EVT_FULL;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	case CHARGER_DEV_NOTIFY_BATPRO_DONE:
		info->batpro_done = true;
		info->setting.vbat_mon_en = 0;
		notify.evt = EVT_BATPRO_DONE;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}
		pr_info("%s: batpro_done = %d\n", __func__, info->batpro_done);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int to_alg_notify_evt(unsigned long evt)
{
	switch (evt) {
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		return EVT_VBUSOVP;
	case CHARGER_DEV_NOTIFY_IBUSOCP:
		return EVT_IBUSOCP;
	case CHARGER_DEV_NOTIFY_IBUSUCP_FALL:
		return EVT_IBUSUCP_FALL;
	case CHARGER_DEV_NOTIFY_BAT_OVP:
		return EVT_VBATOVP;
	case CHARGER_DEV_NOTIFY_IBATOCP:
		return EVT_IBATOCP;
	case CHARGER_DEV_NOTIFY_VBATOVP_ALARM:
		return EVT_VBATOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VBUSOVP_ALARM:
		return EVT_VBUSOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VOUTOVP:
		return EVT_VOUTOVP;
	case CHARGER_DEV_NOTIFY_VDROVP:
		return EVT_VDROVP;
	default:
		return -EINVAL;
	}
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}


int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	info->algo.do_event = charger_dev_event;
	info->algo.do_dvchg1_event = dvchg1_dev_event;
	info->algo.do_dvchg2_event = dvchg2_dev_event;
	//info->change_current_setting = mtk_basic_charging_current;
	return 0;
}
