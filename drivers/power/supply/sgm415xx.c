// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
//#include <mt-plat/mtk_boot.h>
//#include <mt-plat/upmu_common.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/extcon.h>
#include <linux/phy/phy.h>
#include "sgm415xx.h"


#if IS_ENABLED(CONFIG_CM_MIDMISC_SUPPORT)
extern int cust_mid_misc_get_vbus(void);
extern bool cust_mid_misc_get_usb_connect_state(void);
#endif
static int re_try = 5;
static int sgm4154x_get_bc12_type(struct sgm4154x_device *sgm, int *type);
static void sgm4154x_vbus_in_func(struct sgm4154x_device *sgm);
static void sgm4154x_vbus_out_func(struct sgm4154x_device *sgm);
static int sgm4154x_set_hiz_en(struct charger_device *chg_dev, bool hiz_en);
static int sgm4154x_disable_vbus(struct regulator_dev *rdev);
static int sgm4154x_set_boost_voltage_limit(struct charger_device*chg_dev, u32 uV);
static int sgm4154x_set_boost_current_limit(struct charger_device *chg_dev, u32 uA);

#define BC12_FORCE_DETECT_COUNT 2

//#include <mt-plat/mtk_boot.h>
#include "charger_class.h"
//#include <mt-plat/v1/charger_type.h>
//#include "mtk_charger_intf.h"
/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/
#define SGM4154x_PSY_ENABLE 1
#define SGM4154x_HVDCP_ENABLE 0

#define SGM4154X_REGULATO_OPS_ENABLE 0

#define SGM4154x_REG_NUM    (0xF)

/* SGM4154x REG06 BOOST_LIM[5:4], uV */
static const unsigned int BOOST_VOLT_LIMIT[] = {
	4850000, 5000000, 5150000, 5300000		
};
 /* SGM4154x REG02 BOOST_LIM[7:7], uA */
#if (defined(__SGM41542_CHIP_ID__) || defined(__SGM41541_CHIP_ID__)|| defined(__SGM41543_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
static const unsigned int BOOST_CURRENT_LIMIT[] = {
	1200000, 2000000
};
#else
static const unsigned int BOOST_CURRENT_LIMIT[] = {
	500000, 1200000
};
#endif

#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};
#endif
#if 1
static enum power_supply_usb_type sgm4154x_usb_type[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};
#endif
static const struct charger_properties sgm4154x_chg_props = {
	.alias_name = SGM4154x_NAME,
};

static int sgm4154x_dump_register(struct charger_device *chg_dev);


enum {
	SGM_DP_DM_VOL_HIZ,
	SGM_DP_DM_VOL_0P0,
	SGM_DP_DM_VOL_0P6,
	SGM_DP_DM_VOL_3P3,
};

enum SGM4154x_QC_VOLT {
	QC_20_5000mV,
	QC_20_9000mV,
	QC_20_12000mV,
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
#if SGM4154x_PSY_ENABLE
static struct power_supply_desc sgm4154x_power_supply_desc;
#endif

static struct charger_device *s_chg_dev_otg;

/**********************************************************
 *
 *   [I2C Function For Read/Write sgm4154x]
 *
 *********************************************************/
static int __sgm4154x_read_byte(struct sgm4154x_device *sgm, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(sgm->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __sgm4154x_write_byte(struct sgm4154x_device *sgm, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
               val, reg, ret);
        return ret;
    }
    return 0;
}

static int sgm4154x_read_reg(struct sgm4154x_device *sgm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4154x_read_byte(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	return ret;
}
#if 0
static int sgm4154x_write_reg(struct sgm4154x_device *sgm, u8 reg, u8 val)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4154x_write_byte(sgm, reg, val);
	mutex_unlock(&sgm->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}
#endif
static int sgm4154x_update_bits(struct sgm4154x_device *sgm, u8 reg,
					u8 mask, u8 val)
{
	int ret;
	u8 tmp;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm4154x_read_byte(sgm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= val & mask;

	ret = __sgm4154x_write_byte(sgm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/

 static int sgm4154x_set_watchdog_timer(struct sgm4154x_device *sgm, int time)
{
	int ret;
	u8 reg_val;

	if (time == 0)
		reg_val = SGM4154x_WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = SGM4154x_WDT_TIMER_40S;
	else if (time == 80)
		reg_val = SGM4154x_WDT_TIMER_80S;
	else
		reg_val = SGM4154x_WDT_TIMER_160S;	

	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
				SGM4154x_WDT_TIMER_MASK, reg_val);

	return ret;
}

 #if 0
 static int sgm4154x_get_term_curr(struct sgm4154x_device *sgm)
{
	int ret;
	u8 reg_val;
	int curr;
	int offset = SGM4154x_TERMCHRG_I_MIN_uA;

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_3, &reg_val);
	if (ret)
		return ret;

	reg_val &= SGM4154x_TERMCHRG_CUR_MASK;
	curr = reg_val * SGM4154x_TERMCHRG_CURRENT_STEP_uA + offset;
	return curr;
}

static int sgm4154x_get_prechrg_curr(struct sgm4154x_device *sgm)
{
	int ret;
	u8 reg_val;
	int curr;
	int offset = SGM4154x_PRECHRG_I_MIN_uA;

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_3, &reg_val);
	if (ret)
		return ret;

	reg_val = (reg_val&SGM4154x_PRECHRG_CUR_MASK)>>4;
	curr = reg_val * SGM4154x_PRECHRG_CURRENT_STEP_uA + offset;
	return curr;
}

static int sgm4154x_get_ichg_curr(struct sgm4154x_device *sgm)
{
	int ret;
	u8 ichg;
    unsigned int curr;
	
	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_2, &ichg);
	if (ret)
		return ret;	

	ichg &= SGM4154x_ICHRG_I_MASK;
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))	
	if (ichg <= 0x8)
		curr = ichg * 5000;
	else if (ichg <= 0xF)
		curr = 40000 + (ichg - 0x8) * 10000;
	else if (ichg <= 0x17)
		curr = 110000 + (ichg - 0xF) * 20000;
	else if (ichg <= 0x20)
		curr = 270000 + (ichg - 0x17) * 30000;
	else if (ichg <= 0x30)
		curr = 540000 + (ichg - 0x20) * 60000;
	else if (ichg <= 0x3C)
		curr = 1500000 + (ichg - 0x30) * 120000;
	else
		curr = 3000000;
#else
	curr = ichg * SGM4154x_ICHRG_I_STEP_uA;
#endif	
	return curr;
}
#endif

static int sgm4154x_set_term_curr(struct sgm4154x_device *sgm, int uA)
{
	u8 reg_val;
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))	
	
	for(reg_val = 1; reg_val < 16 && uA >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
		;
	reg_val--;
#else
	if (uA < SGM4154x_TERMCHRG_I_MIN_uA)
		uA = SGM4154x_TERMCHRG_I_MIN_uA;
	else if (uA > SGM4154x_TERMCHRG_I_MAX_uA)
		uA = SGM4154x_TERMCHRG_I_MAX_uA;
	
	reg_val = (uA - SGM4154x_TERMCHRG_I_MIN_uA) / SGM4154x_TERMCHRG_CURRENT_STEP_uA;
#endif

	return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_3,
				  SGM4154x_TERMCHRG_CUR_MASK, reg_val);
}

static int sgm4154x_set_prechrg_curr(struct sgm4154x_device *sgm, int uA)
{
	u8 reg_val;
	
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
	for(reg_val = 1; reg_val < 16 && uA >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
		;
	reg_val--;
#else
	if (uA < SGM4154x_PRECHRG_I_MIN_uA)
		uA = SGM4154x_PRECHRG_I_MIN_uA;
	else if (uA > SGM4154x_PRECHRG_I_MAX_uA)
		uA = SGM4154x_PRECHRG_I_MAX_uA;

	reg_val = (uA - SGM4154x_PRECHRG_I_MIN_uA) / SGM4154x_PRECHRG_CURRENT_STEP_uA;
#endif
	reg_val = reg_val << 4;
	return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_3,
				  SGM4154x_PRECHRG_CUR_MASK, reg_val);
}

static int sgm4154x_set_ichrg_curr(struct charger_device *chg_dev, unsigned int uA)
{
	int ret;
	u8 reg_val;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	pr_err("sgm4154x_set_ichrg_curr charge curr = %d\n", uA);
	
	if (uA < SGM4154x_ICHRG_I_MIN_uA)
		uA = SGM4154x_ICHRG_I_MIN_uA;
	else if ( uA > sgm->init_data.max_ichg)
		uA = sgm->init_data.max_ichg;
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
	if (uA <= 40000)
		reg_val = uA / 5000;	
	else if (uA <= 110000)
		reg_val = 0x08 + (uA -40000) / 10000;	
	else if (uA <= 270000)
		reg_val = 0x0F + (uA -110000) / 20000;	
	else if (uA <= 540000)
		reg_val = 0x17 + (uA -270000) / 30000;	
	else if (uA <= 1500000)
		reg_val = 0x20 + (uA -540000) / 60000;	
	else if (uA <= 2940000)
		reg_val = 0x30 + (uA -1500000) / 120000;
	else 
		reg_val = 0x3d;
#else

	reg_val = uA / SGM4154x_ICHRG_I_STEP_uA;
#endif
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_2,
				  SGM4154x_ICHRG_I_MASK, reg_val);
	
	return ret;
}

static int sgm4154x_set_chrg_volt(struct charger_device *chg_dev, unsigned int chrg_volt)
{
	int ret;
	u8 reg_val;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	pr_err("sgm4154x_set_chrg_volt charge volt = %d\n", chrg_volt);
	
	if (chrg_volt < SGM4154x_VREG_V_MIN_uV)
		chrg_volt = SGM4154x_VREG_V_MIN_uV;
	else if (chrg_volt > sgm->init_data.max_vreg)
		chrg_volt = sgm->init_data.max_vreg;
	
	
	reg_val = (chrg_volt-SGM4154x_VREG_V_MIN_uV) / SGM4154x_VREG_V_STEP_uV;
	reg_val = reg_val<<3;
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_4,
				  SGM4154x_VREG_V_MASK, reg_val);

	return ret;
}

static int sgm4154x_get_chrg_volt(struct charger_device *chg_dev,unsigned int *volt)
{
	int ret;
	u8 vreg_val;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_4, &vreg_val);
	if (ret)
		return ret;	

	vreg_val = (vreg_val & SGM4154x_VREG_V_MASK)>>3;

	if (15 == vreg_val)
		*volt = 4352000; //default
	else if (vreg_val < 25)	
		*volt = vreg_val*SGM4154x_VREG_V_STEP_uV + SGM4154x_VREG_V_MIN_uV;	

	return 0;
}

static int sgm4154x_get_vindpm_offset_os(struct sgm4154x_device *sgm)
{
	int ret;
	u8 reg_val;

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_f, &reg_val);
	if (ret)
		return ret;	

	reg_val = reg_val & SGM4154x_VINDPM_OS_MASK;	

	return reg_val;
}

static int sgm4154x_set_vindpm_offset_os(struct sgm4154x_device *sgm,u8 offset_os)
{
	int ret;	
	
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_f,
				  SGM4154x_VINDPM_OS_MASK, offset_os);
	
	if (ret){
		pr_err("%s fail\n",__func__);
		return ret;
	}
	
	return ret;
}

static int sgm4154x_set_input_volt_lim(struct sgm4154x_device *sgm, unsigned int vindpm)
{
	int ret;
	unsigned int offset;
	u8 reg_val;
	u8 os_val;

	if (vindpm < SGM4154x_VINDPM_V_MIN_uV ||
	    vindpm > SGM4154x_VINDPM_V_MAX_uV)
 		return -EINVAL;	
	
	if (vindpm < 5900000){
		os_val = 0;
		offset = 3900000;
	}		
	else if (vindpm >= 5900000 && vindpm < 7500000){
		os_val = 1;
		offset = 5900000; //uv
	}		
	else if (vindpm >= 7500000 && vindpm < 10500000){
		os_val = 2;
		offset = 7500000; //uv
	}		
	else{
		os_val = 3;
		offset = 10500000; //uv
	}		
	
	sgm4154x_set_vindpm_offset_os(sgm,os_val);
	reg_val = (vindpm - offset) / SGM4154x_VINDPM_STEP_uV;	

	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_6,
				  SGM4154x_VINDPM_V_MASK, reg_val); 

	return ret;
}

static int sgm4154x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	int ret = 0;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	printk("sgm4154x_set_ivl vindpm volt = %d\n", volt);

	if (!sgm->pp_en) {
		printk("%s: power path is disabled\n", __func__);
		sgm4154x_set_input_volt_lim(sgm, SGM4154x_MIVR_MAX);
		return ret;
	}

	sgm->mivr = volt;

	return sgm4154x_set_input_volt_lim(sgm, volt);
}

static int sgm4154x_get_input_volt_lim(struct sgm4154x_device *sgm)
{
	int ret;
	int offset;
	u8 vlim;
	int temp;

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_6, &vlim);
	if (ret)
		return ret;
	
	temp = sgm4154x_get_vindpm_offset_os(sgm);
	if (0 == temp)
		offset = 3900000; //uv
	else if (1 == temp)
		offset = 5900000;
	else if (2 == temp)
		offset = 7500000;
	else if (3 == temp)
		offset = 10500000;
	else
		offset = 5900000;
		
	
	temp = offset + (vlim & 0x0F) * SGM4154x_VINDPM_STEP_uV;
	return temp;
}


static int sgm4154x_set_input_curr_lim(struct charger_device *chg_dev, unsigned int iindpm)
{
	int ret;
	u8 reg_val;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	pr_err("sgm4154x_set_input_curr_lim indpm curr = %d\n", iindpm);
	
	if (iindpm < SGM4154x_IINDPM_I_MIN_uA ||
			iindpm > SGM4154x_IINDPM_I_MAX_uA)
		return -EINVAL;	

#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
	reg_val = (iindpm-SGM4154x_IINDPM_I_MIN_uA) / SGM4154x_IINDPM_STEP_uA;
#else		
	if (iindpm >= SGM4154x_IINDPM_I_MIN_uA && iindpm <= 3100000)//default
		reg_val = (iindpm-SGM4154x_IINDPM_I_MIN_uA) / SGM4154x_IINDPM_STEP_uA;
	else if (iindpm > 3100000 && iindpm < SGM4154x_IINDPM_I_MAX_uA)
		reg_val = 0x1E;
	else
		reg_val = SGM4154x_IINDPM_I_MASK;
#endif
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_0,
				  SGM4154x_IINDPM_I_MASK, reg_val);
	return ret;
}

static int sgm4154x_get_input_curr_lim(struct charger_device *chg_dev,unsigned int *ilim)
{
	int ret;	
	u8 reg_val;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_0, &reg_val);
	if (ret)
		return ret;	
	if (SGM4154x_IINDPM_I_MASK == (reg_val & SGM4154x_IINDPM_I_MASK))
		*ilim =  SGM4154x_IINDPM_I_MAX_uA;
	else
		*ilim = (reg_val & SGM4154x_IINDPM_I_MASK)*SGM4154x_IINDPM_STEP_uA + SGM4154x_IINDPM_I_MIN_uA;

	return 0;
}


static int32_t sgm4154x_set_dpdm(
	struct sgm4154x_device *sgm, uint8_t dp_val, uint8_t dm_val)
{
	uint8_t data_reg = 0;
	
	uint8_t mask = SGM4154x_DP_VSEL_MASK|SGM4154x_DM_VSEL_MASK;
	
	data_reg  = (dp_val & SGM4154x_DP_VSEL_MASK) << SGM4154x_DP_VOLT_SHIFT;
	data_reg |= (dm_val & SGM4154x_DM_VSEL_MASK) << SGM4154x_DM_VOLT_SHIFT;
	
	
	return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
				  mask, data_reg);

}

static int sgm4154x_charging_set_hvdcp20(
		struct sgm4154x_device *sgm, uint32_t vbus_target)
{
	int32_t ret = 0;

	pr_err("Set vbus target %dv\n", vbus_target);
	switch (vbus_target) {
	case 5:
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P0);
		sgm4154x_set_input_volt_lim(sgm, 4500000);
		break;
	case 9:		
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_0P6);
		sgm4154x_set_input_volt_lim(sgm, 8000000);
		break;
	case 12:		
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
		sgm4154x_set_input_volt_lim(sgm, 11000000);
		break;
	default:
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_HIZ, SGM_DP_DM_VOL_HIZ);
		sgm4154x_set_input_volt_lim(sgm, 4500000);
		break;
	}

	return (ret < 0) ? ret : 0;
}

int32_t sgm4154x_charging_enable_hvdcp30(struct sgm4154x_device *sgm, bool enable)
{
//	sgm4154x_dbg("enable %d\n", enable);
	//enter continuous mode DP 0.6, DM 3.3
	return sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
}

int32_t sgm4154x_charging_set_hvdcp30(struct sgm4154x_device *sgm, bool increase)
{
	int32_t ret = 0;

//	sgm4154x_dbg("increase %d\n", increase);
	if (increase) {
		//DP 3.3, DM 3.3
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		//need test
		msleep(100);
		//DP 0.6, DM 3.3
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		msleep(100);
	} else {
		//DP 0.6, DM 3.3
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
		if (ret < 0)
			return ret;
		//need test
		msleep(100);
		//DP 0.6, DM 3.3
		ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		msleep(100);
	}
	return 0;
}

bool sgm4154x_is_hvdcp(struct sgm4154x_device *sgm,enum SGM4154x_QC_VOLT val)
{	
    int i = 20;	
	int vlim;
	u8 temp;

	vlim = sgm4154x_get_input_volt_lim(sgm);
	
	if (QC_20_9000mV == val)
	{
		sgm4154x_set_input_volt_lim(sgm,8000000); //8v
	}
	else if (QC_20_12000mV == val)
	{
		sgm4154x_set_input_volt_lim(sgm,11000000); //11v
	}
	else
	{
		sgm4154x_set_input_volt_lim(sgm,4500000); //4.5v
		return 0;
	}
	mdelay(1);
	while(i--){		
	 	sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_a, &temp);
		
		if(0 == (temp&0x40)){
			return 1;
		}
		else if(1 == !!(temp&0x40) && i == 1){
			sgm4154x_set_input_volt_lim(sgm,vlim);
			return 0;
		}
		mdelay(10);
	}	
	return 0;
}

static int sgm4154x_get_state(struct sgm4154x_device *sgm,
			     struct sgm4154x_state *state)
{
	u8 chrg_stat;
	u8 fault;
	u8 chrg_param_0,chrg_param_1,chrg_param_2;
	int ret;

#if defined(CUSTOM_BC12_ENABLE) //Leo 20240827
	cust_chg_enable_bc12(sgm, true);
#endif

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &chrg_stat);
	if (ret){
		ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &chrg_stat);
		if (ret){
			pr_err("%s read SGM4154x_CHRG_STAT fail\n",__func__);
			return ret;
		}
	}
	state->chrg_type = (chrg_stat & SGM4154x_VBUS_STAT_MASK) >> 5;
	state->chrg_stat = (chrg_stat & SGM4154x_CHG_STAT_MASK)>>3;
	state->online = !!(chrg_stat & SGM4154x_PG_STAT);
	state->therm_stat = !!(chrg_stat & SGM4154x_THERM_STAT);
	state->vsys_stat = !!(chrg_stat & SGM4154x_VSYS_STAT);
	
	pr_err("%s chrg_type =%d,chrg_stat =%d online = %d\n",__func__,state->chrg_type,state->chrg_stat,state->online);
	

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_FAULT, &fault);
	if (ret){
		pr_err("%s read SGM4154x_CHRG_FAULT fail\n",__func__);
		return ret;
	}
	state->chrg_fault = fault;	
	state->ntc_fault = fault & SGM4154x_TEMP_MASK;
	state->health = state->ntc_fault;
	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_0, &chrg_param_0);
	if (ret){
		pr_err("%s read SGM4154x_CHRG_CTRL_0 fail\n",__func__);
		return ret;
	}
	state->hiz_en = !!(chrg_param_0 & SGM4154x_HIZ_EN);
	
	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_5, &chrg_param_1);
	if (ret){
		pr_err("%s read SGM4154x_CHRG_CTRL_5 fail\n",__func__);
		return ret;
	}
	state->term_en = !!(chrg_param_1 & SGM4154x_TERM_EN);
	
	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_a, &chrg_param_2);
	if (ret){
		pr_err("%s read SGM4154x_CHRG_CTRL_a fail\n",__func__);
		return ret;
	}
	state->vbus_gd = !!(chrg_param_2 & SGM4154x_VBUS_GOOD);

#if defined(CUSTOM_BC12_ENABLE) //Leo 20240827
	cust_chg_enable_bc12(sgm, false);
#endif

	return 0;
}
#if 1
static int sgm4154x_set_hiz_en(struct charger_device *chg_dev, bool hiz_en)
{
	u8 reg_val;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	dev_notice(sgm->dev, "%s:%d", __func__, hiz_en);
	reg_val = hiz_en ? SGM4154x_HIZ_EN : 0;

	return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_0,
				  SGM4154x_HIZ_EN, reg_val);
}
#endif
static int sgm4154x_enable_charger(struct sgm4154x_device *sgm)
{
    int ret;
    
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_CHRG_EN,
                     SGM4154x_CHRG_EN);
	
    return ret;
}

static int sgm4154x_disable_charger(struct sgm4154x_device *sgm)
{
    int ret;
    
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_CHRG_EN,
                     0);
    return ret;
}

static int sgm4154x_charging_switch(struct charger_device *chg_dev,bool enable)
{
	int ret;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	int chg_type = 0;
	
	if (enable)
		ret = sgm4154x_enable_charger(sgm);
	else
		ret = sgm4154x_disable_charger(sgm);

	sgm->chg_config = enable;

#if 0
	if(sgm->chg_config){
	    if(sgm->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP && re_try >= 1){
	        re_try--;
	        sgm4154x_get_bc12_type(sgm, &chg_type);
	        if(sgm->psy_usb_type != POWER_SUPPLY_USB_TYPE_SDP){
	            power_supply_changed(sgm->charger);
	        }
	    }
	}
#endif

	pr_err("sgm4154x_charging_switch %s charger %s chg_type=%d\n", enable ? "enable" : "disable",
        !ret ? "successfully" : "failed", chg_type);

	return ret;
}

static int sgm4154x_set_recharge_volt(struct sgm4154x_device *sgm, int mV)
{
	u8 reg_val;
	
	reg_val = (mV - SGM4154x_VRECHRG_OFFSET_mV) / SGM4154x_VRECHRG_STEP_mV;

	return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_4,
				  SGM4154x_VRECHARGE, reg_val);
}

static int sgm4154x_set_wdt_rst(struct sgm4154x_device *sgm, bool is_rst)
{
	u8 val;
	
	if (is_rst)
		val = SGM4154x_WDT_RST_MASK;
	else
		val = 0;
	return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1,
				  SGM4154x_WDT_RST_MASK, val);	
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm4154x_dump_register(struct charger_device *chg_dev)
{

	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned char sgm4154x_reg[SGM4154x_REG_NUM+1] = { 0 }; 
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
		
	for (i = 0; i < SGM4154x_REG_NUM+1; i++) {
		ret = sgm4154x_read_reg(sgm,i, &sgm4154x_reg[i]);
		if (ret != 0) {
			pr_info("[sgm4154x] i2c transfor error\n");
			return 1;
		}
		pr_info("%s,[0x%x]=0x%x ",__func__, i, sgm4154x_reg[i]);
	}
//Leo 20240822
	
	return 0;
}


/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm4154x_hw_chipid_detect(struct sgm4154x_device *sgm)
{
	int ret = 0;
	u8 val = 0;
	ret = sgm4154x_read_reg(sgm,SGM4154x_CHRG_CTRL_b,&val);
	if (ret < 0)
	{
		pr_info("[%s] read SGM4154x_CHRG_CTRL_b fail\n", __func__);
		return ret;
	}		
	val = val & SGM4154x_PN_MASK;
	pr_info("[%s] Reg[0x0B]=0x%x\n", __func__,val);
	
	return val;
}

static int sgm4154x_reset_watch_dog_timer(struct charger_device
		*chg_dev)
{
	int ret;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	pr_info("charging_reset_watch_dog_timer\n");

	ret = sgm4154x_set_wdt_rst(sgm,0x1);	/* RST watchdog */	

	return ret;
}

#if IS_ENABLED(CONFIG_WB_SGM415XX_USE_NEW_CHARGER)
static int sgm4154x_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	int ret;
	union power_supply_propval val;

    ret = power_supply_get_property(sgm->charger, POWER_SUPPLY_PROP_STATUS, &val);
    if (ret < 0){
		return ret;
    }

	printk("%s done =%d\n", __func__,*is_done);

	return *is_done = (val.intval == POWER_SUPPLY_STATUS_FULL);
}
#else
static int sgm4154x_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	//struct sgm4154x_state state;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	//sgm4154x_get_state(sgm, &state);

	if (sgm->state.chrg_stat == 3)
		*is_done = true;
	else
		*is_done = false;

	return 0;
}
#endif

static int sgm4154x_set_en_timer(struct sgm4154x_device *sgm)
{
	int ret;	

	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
				SGM4154x_SAFETY_TIMER_EN, SGM4154x_SAFETY_TIMER_EN);

	return ret;
}

static int sgm4154x_set_disable_timer(struct sgm4154x_device *sgm)
{
	int ret;	

	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
				SGM4154x_SAFETY_TIMER_EN, 0);

	return ret;
}

static int sgm4154x_enable_safetytimer(struct charger_device *chg_dev,bool en)
{
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	int ret = 0;

	if (en)
		ret = sgm4154x_set_en_timer(sgm);
	else
		ret = sgm4154x_set_disable_timer(sgm);
	return ret;
}

static int sgm4154x_get_is_safetytimer_enable(struct charger_device
		*chg_dev,bool *en)
{
	int ret = 0;
	u8 val = 0;
	
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	ret = sgm4154x_read_reg(sgm,SGM4154x_CHRG_CTRL_5,&val);
	if (ret < 0)
	{
		pr_info("[%s] read SGM4154x_CHRG_CTRL_5 fail\n", __func__);
		return ret;
	}
	*en = !!(val & SGM4154x_SAFETY_TIMER_EN);
	return 0;
}

#if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
static int sgm4154x_en_pe_current_partern(struct charger_device
		*chg_dev,bool is_up)
{
	int ret = 0;	
	
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
				SGM4154x_EN_PUMPX, SGM4154x_EN_PUMPX);
	if (ret < 0)
	{
		pr_info("[%s] read SGM4154x_CHRG_CTRL_d fail\n", __func__);
		return ret;
	}
	if (is_up)
		ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
				SGM4154x_PUMPX_UP, SGM4154x_PUMPX_UP);
	else
		ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
				SGM4154x_PUMPX_DN, SGM4154x_PUMPX_DN);
	return ret;
}
#endif

#if SGM4154x_PSY_ENABLE
static enum power_supply_property sgm4154x_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	//POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_PRESENT
};

static int sgm4154x_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	//case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		return true;
	default:
		return false;
	}
}
static int sgm4154x_charger_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct sgm4154x_device *sgm = power_supply_get_drvdata(psy);
	int ret = -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CUSTOM_BC12_ENABLE) //Leo 20240827
		atomic_set(&sgm->attach, val->intval);	
#endif
		break;
    case POWER_SUPPLY_PROP_STATUS:
        if (val->intval) {
            ret = sgm4154x_enable_charger(sgm);
        } else {
            ret = sgm4154x_disable_charger(sgm);
        }
        break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm4154x_set_input_curr_lim(s_chg_dev_otg, val->intval);
		break;
/*	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		sgm4154x_charging_switch(s_chg_dev_otg,val->intval);		
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm4154x_set_input_volt_lim(s_chg_dev_otg, val->intval);
		break;*/
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		printk("LQ >>> %s  val->intval=%d\n",__func__,val->intval);
		ret = sgm4154x_set_chrg_volt(sgm->chg_dev, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_WB_SGM415XX_USE_NEW_CHARGER)
static int sgm4154x_get_charge_stat(struct sgm4154x_device *sgm, int *state)
{
    int ret,chrg_type,chrg_stat;
    u8 val;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &val);
	chrg_type = (val & SGM4154x_VBUS_STAT_MASK) >> 5;
	chrg_stat = (val & SGM4154x_CHG_STAT_MASK) >> 3;

    if (!ret) {
        if ((chrg_type == SGM4154x_OTG_MODE) || (!sgm->pp_en)) {
            *state = POWER_SUPPLY_STATUS_DISCHARGING;
            return ret;
        }

        switch (chrg_stat)
        {
		case SGM4154X_CHRG_STAT_NOTCHG:
			if (sgm->chg_config) {
                *state = POWER_SUPPLY_STATUS_CHARGING;
            } else {
                *state = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            break;
        case SGM4154X_CHRG_STAT_PRECHG:
        case SGM4154X_CHRG_STAT_FASTCHG:
            *state = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case SGM4154X_TERM_CHRG:
            *state = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
            *state = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
        }
    }

    pr_err("%s---->state=%d chrg_type=%d chrg_stat=%d pp_en=%d\n", __func__, *state, chrg_type, chrg_stat, sgm->pp_en);

    return ret;
}

static int sgm4154x_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sgm4154x_device *sgm = power_supply_get_drvdata(psy);
	//struct sgm4154x_state state;
	int ret = 0;
	int data;

	int c_vol = 5000;
	u8 reg_val;
	int chrg_stat;
	int chrg_fault;
	int health;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
        ret = sgm4154x_get_charge_stat(sgm, &data);
        val->intval = data;
        //sgm4154x_dump_register(sgm->chg_dev);
        pr_info("%s POWER_SUPPLY_PROP_STATUS val->intval:%d \n",__func__,val->intval);
        break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &reg_val);
		chrg_stat = (reg_val & SGM4154x_CHG_STAT_MASK) >> 3;
		switch (chrg_stat) {		
		case SGM4154x_PRECHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM4154x_FAST_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;		
		case SGM4154x_TERM_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM4154x_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SGM4154x_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sgm4154x_get_chrg_volt(sgm->chg_dev,&data);
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = SGM4154x_NAME;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = sgm->vbus_attach;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sgm->power_good;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = sgm->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sgm->psy_desc.type;
		pr_info("%s val->intval:%d \n",__func__,sgm->psy_desc.type);
		break;	
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_FAULT, &reg_val);
		chrg_fault = reg_val & 0xF8;
		health = reg_val & SGM4154x_TEMP_MASK;
		if (chrg_fault)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;

		switch (health) {
		case SGM4154x_TEMP_HOT:
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case SGM4154x_TEMP_WARM:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM4154x_TEMP_COOL:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM4154x_TEMP_COLD:
			val->intval = POWER_SUPPLY_HEALTH_COLD;
			break;
		}
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		//val->intval = state.vbus_adc;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		//val->intval = state.ibus_adc;
		break;

    case POWER_SUPPLY_PROP_CURRENT_MAX:
		c_vol = cust_mid_misc_get_vbus();
		if (c_vol > 8000) {
			val->intval = 2000000;
		}else{
			val->intval = 500000;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:		
		break;

	default:
		return -EINVAL;
	}

	return ret;
}
#else
static int get_charger_type(struct sgm4154x_state state)
{
		int type=0;
		switch(state.chrg_type) {
		case SGM4154x_USB_SDP:
			printk("SGM4154x charger type: SDP\n");
			type = POWER_SUPPLY_USB_TYPE_SDP;//STANDARD_HOST;
			break;

		case SGM4154x_USB_CDP:
			printk("SGM4154x charger type: CDP\n");
			type = POWER_SUPPLY_USB_TYPE_CDP;//STANDARD_CHARGER;
			break;

		case SGM4154x_USB_DCP:
			printk("SGM4154x charger type: DCP\n");
			type = POWER_SUPPLY_USB_TYPE_DCP;//STANDARD_CHARGER;
			break;

		case SGM4154x_UNKNOWN:
			printk("SGM4154x charger type: UNKNOWN\n");
			type = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
			break;	
		case SGM4154x_NON_STANDARD:
			printk("SGM4154x charger type: NON_STANDARD\n");
			type = POWER_SUPPLY_USB_TYPE_CDP;//STANDARD_HOST;
			break;
		case SGM4154x_OTG_MODE:
			type = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
			break;

		default:
			type = 0;
			break;
			
	}
	//printk("LQ >>> %s  type=%d\n",__func__,type);	
	return type;
}

static int sgm4154x_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sgm4154x_device *sgm = power_supply_get_drvdata(psy);
	struct sgm4154x_state state;
	int ret = 0;
	u32 data;

	mutex_lock(&sgm->lock);
	ret = sgm4154x_get_state(sgm, &state);
	//state = sgm->state;
	mutex_unlock(&sgm->lock);
	//printk("LQ >>> %s  chrg_type=%d,online=%d,chrg_stat=%d\n",__func__,state.chrg_type,state.online,state.chrg_stat);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.chrg_type || (state.chrg_type == SGM4154x_OTG_MODE))
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (!state.chrg_stat)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_stat == 3)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
			//add by LQ start
			val->intval = get_charger_type(state);
			//add by LQ end
			break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {		
		case SGM4154x_PRECHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM4154x_FAST_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;		
		case SGM4154x_TERM_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM4154x_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SGM4154x_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sgm4154x_get_chrg_volt(sgm->chg_dev,&data);
		if (ret < 0)
				break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = SGM4154x_NAME;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = state.vbus_gd;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sgm->psy_desc.type;//sgm4154x_power_supply_desc.type;
		pr_info("%s val->intval:%d \n",__func__,sgm->psy_desc.type);
		break;	

	case POWER_SUPPLY_PROP_HEALTH:
		if (state.chrg_fault & 0xF8)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;

		switch (state.health) {
		case SGM4154x_TEMP_HOT:
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case SGM4154x_TEMP_WARM:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM4154x_TEMP_COOL:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case SGM4154x_TEMP_COLD:
			val->intval = POWER_SUPPLY_HEALTH_COLD;
			break;
		}
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		//val->intval = state.vbus_adc;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		//val->intval = state.ibus_adc;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;

/*	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm4154x_get_input_volt_lim(sgm);
		if (ret < 0)
			return ret;

		val->intval = ret;
		break;*/

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:		
		break;
#if 0
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = !state.hiz_en;
		break;
#endif
	default:
		return -EINVAL;
	}

	return ret;
}
#endif
#endif

#if 0
static bool sgm4154x_state_changed(struct sgm4154x_device *sgm,
				  struct sgm4154x_state *new_state)
{
	struct sgm4154x_state old_state;

	mutex_lock(&sgm->lock);
	old_state = sgm->state;
	mutex_unlock(&sgm->lock);

	return (old_state.chrg_type != new_state->chrg_type ||
		old_state.chrg_stat != new_state->chrg_stat     ||		
		old_state.online != new_state->online		    ||
		old_state.therm_stat != new_state->therm_stat	||		
		old_state.vsys_stat != new_state->vsys_stat 	||
		old_state.chrg_fault != new_state->chrg_fault	
		);
}
#endif

static bool sgm4154x_dpdm_detect_is_done(struct sgm4154x_device * sgm)
{
	u8 chrg_stat;
	int ret;

	bool dpdm_detect_done;

	ret = sgm4154x_read_reg(sgm, SGM4154x_INPUT_DET, &chrg_stat);
	if(ret) {
		dev_err(sgm->dev, "Check DPDM detecte error\n");
	}
	
	dpdm_detect_done = (chrg_stat&SGM4154x_DPDM_ONGOING);

	printk("sgm4154x_dpdm_detect_is_done dpdm_detect_done=%d\n", dpdm_detect_done);

	return dpdm_detect_done;
}

#if defined(CUSTOM_BC12_ENABLE) //Leo 20240827
static int cust_chg_set_usbsw(struct sgm4154x_device *ddata,
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

static int cust_chg_enable_bc12(struct sgm4154x_device *ddata, bool en)
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
#endif

static void sgm4154x_chg_type_hvdcp_work(struct work_struct *work)
{
	
	struct sgm4154x_device * sgm =
		container_of(work, struct sgm4154x_device, hvdcp_work.work);
		
	sgm4154x_charging_set_hvdcp20(sgm,9);	
	pr_err("%s HVDCP",__func__);
	
}

static void charger_monitor_work_func(struct work_struct *work)
{
	int ret = 0;
	struct sgm4154x_device * sgm = NULL;
	struct delayed_work *charge_monitor_work = NULL;
	//static u8 last_chg_method = 0;
	struct sgm4154x_state state;

	charge_monitor_work = container_of(work, struct delayed_work, work);
	if(charge_monitor_work == NULL) {
		pr_err("Cann't get charge_monitor_work\n");
		return ;
	}
	sgm = container_of(charge_monitor_work, struct sgm4154x_device, charge_monitor_work);
	if(sgm == NULL) {
		pr_err("Cann't get sgm \n");
		return ;
	}

	ret = sgm4154x_get_state(sgm, &state);
	mutex_lock(&sgm->lock);
	sgm->state = state;
	mutex_unlock(&sgm->lock);

	if(!sgm->state.vbus_gd) {
		dev_err(sgm->dev, "Vbus not present, disable charge\n");
		sgm4154x_disable_charger(sgm);
		goto OUT;
	}
	if(!state.online)
	{
		dev_err(sgm->dev, "Vbus not online\n");		
		goto OUT;
	}
	
	sgm4154x_dump_register(sgm->chg_dev);
	pr_err("%s\n",__func__);
OUT:	
	schedule_delayed_work(&sgm->charge_monitor_work, 10*HZ);
}

static void charger_detect_work_func(struct work_struct *work)
{
	struct delayed_work *charge_detect_delayed_work = NULL;
	struct sgm4154x_device * sgm = NULL;
	union power_supply_propval online_propval = {0};
	union power_supply_propval psy_usb_type = {0};
//	union power_supply_propval chr_type = {0};
	//static int charge_type_old = 0;
#if 1
	int curr_in_limit = 0;
#endif
	struct sgm4154x_state state;
	int ret;
	//int boot_mode = 0;

	//boot_mode = get_boot_mode();
	
	charge_detect_delayed_work = container_of(work, struct delayed_work, work);
	if(charge_detect_delayed_work == NULL) {
		pr_err("Cann't get charge_detect_delayed_work\n");
		return ;
	}
	sgm = container_of(charge_detect_delayed_work, struct sgm4154x_device, charge_detect_delayed_work);
	if(sgm == NULL) {
		pr_err("Cann't get sgm4154x_device\n");
		return ;
	}

	//if (!sgm->charger_wakelock->active)
		//__pm_stay_awake(sgm->charger_wakelock);

	ret = sgm4154x_get_state(sgm, &state);
	mutex_lock(&sgm->lock);
	sgm->state = state;
	mutex_unlock(&sgm->lock);

	online_propval.intval = state.online;
	
	if(!sgm->state.vbus_gd) {
		dev_err(sgm->dev, "Vbus not present, disable charge\n");
		sgm4154x_disable_charger(sgm);

		psy_usb_type.intval = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
		online_propval.intval = 0;
		goto err;
	}

	if(!state.online){
		dev_err(sgm->dev, "Vbus not online\n");

		psy_usb_type.intval = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
		online_propval.intval = 0;
		goto err;
	}

	if(!sgm4154x_dpdm_detect_is_done(sgm)) {
		dev_err(sgm->dev, "DPDM detecte not done, disable charge\n");
		sgm4154x_dump_register(sgm->chg_dev);
		//return;
	}
#if 1
	switch(sgm->state.chrg_type) {
		case SGM4154x_USB_SDP:
			printk("%s SGM4154x charger type: SDP\n",__func__);
			curr_in_limit = 500000;
			psy_usb_type.intval = POWER_SUPPLY_USB_TYPE_SDP;//STANDARD_HOST;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			online_propval.intval = 1;
			break;

		case SGM4154x_USB_CDP:
			printk("%s SGM4154x charger type: CDP\n",__func__);
			curr_in_limit = 1500000;
			psy_usb_type.intval = POWER_SUPPLY_USB_TYPE_CDP;//STANDARD_CHARGER;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			online_propval.intval = 1;
			break;

		case SGM4154x_USB_DCP:
			printk("%s SGM4154x charger type: DCP\n",__func__);
#if SGM4154x_HVDCP_ENABLE
			//sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_HIZ);
			//schedule_delayed_work(&sgm->hvdcp_work, msecs_to_jiffies(1400));
#endif
			curr_in_limit = 2000000;
			psy_usb_type.intval = POWER_SUPPLY_USB_TYPE_DCP;//STANDARD_CHARGER;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			online_propval.intval = 1;
			break;

		case SGM4154x_UNKNOWN:
			printk("%s SGM4154x charger type: UNKNOWN\n",__func__);
			psy_usb_type.intval = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			online_propval.intval = 0;
			break;	
		case SGM4154x_NON_STANDARD:
			printk("%s SGM4154x charger type: NON_STANDARD\n",__func__);
			curr_in_limit = 2000000;//500000;
			psy_usb_type.intval = POWER_SUPPLY_USB_TYPE_DCP;//STANDARD_HOST;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			online_propval.intval = 1;
			break;
		case SGM4154x_OTG_MODE:
			printk("%s SGM4154x charger type: OTG_MODE\n",__func__);
			psy_usb_type.intval = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			online_propval.intval = 0;
			break;
		default:
			printk("%s SGM4154x charger type: default\n",__func__);
			psy_usb_type.intval = 0;//POWER_SUPPLY_TYPE_UNKNOWN;
			sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			online_propval.intval = 0;
			break;
	}

	//if(boot_mode == FACTORY_BOOT || boot_mode == META_BOOT){
	//	printk("SGM4154x chrg_type=%d\n", sgm->state.chrg_type);
	//	curr_in_limit = 500000;
	//	psy_usb_type.intval = STANDARD_HOST;
	//	online_propval.intval = 1;
	//}

	//set charge parameters
	dev_err(sgm->dev, "Update: curr_in_limit = %d\n", curr_in_limit);
	sgm4154x_set_input_curr_lim(sgm->chg_dev, curr_in_limit);
#endif
	//enable charge
	//printk("LQ >>> %s  enable charge intval:%d\n",__func__,online_propval.intval);
	if(online_propval.intval){
		sgm4154x_enable_charger(sgm);
	}
	sgm4154x_dump_register(sgm->chg_dev);

	power_supply_changed(sgm->charger);
	
err:
	//release wakelock

	if (!sgm->charger) {
		sgm->charger = power_supply_get_by_name("charger");
	}

	ret = power_supply_set_property(sgm->charger, POWER_SUPPLY_PROP_ONLINE, &online_propval);
	if (ret < 0)
		dev_info(sgm->dev,"inform power supply online failed:%d\n", ret);

	//ret = power_supply_set_property(sgm->ac, POWER_SUPPLY_PROP_CHARGE_TYPE, &psy_usb_type);
	//if (ret < 0)
		//printk("inform power supply charge type failed:%d\n", ret);
	ret = power_supply_set_property(sgm->charger, POWER_SUPPLY_PROP_TYPE, &psy_usb_type);
	if (ret < 0)
		dev_info(sgm->dev,"inform power supply usb type failed:%d\n", ret);

#if SGM4154x_PSY_ENABLE
	power_supply_changed(sgm->charger);
#endif
	dev_err(sgm->dev, "Relax wakelock\n");
	__pm_relax(sgm->charger_wakelock);

	return;
}

static irqreturn_t sgm4154x_irq_handler_thread(int irq, void *private)
{
	struct sgm4154x_device *sgm = private;

	//lock wakelock
	pr_err("%s entry\n",__func__);
    if (!sgm->charger_wakelock->active)
		__pm_stay_awake(sgm->charger_wakelock);
	schedule_delayed_work(&sgm->charge_detect_delayed_work, 100);
	//power_supply_changed(sgm->charger);
	
	return IRQ_HANDLED;
}

static void sgm4154x_vbus_in_func(struct sgm4154x_device *sgm)
{
#ifdef CUSTOM_BC12_ENABLE
	cust_chg_enable_bc12(sgm, true);
#endif
}

static void sgm4154x_vbus_out_func(struct sgm4154x_device *sgm)
{
#ifdef CUSTOM_BC12_ENABLE
	cust_chg_enable_bc12(sgm, false);
#endif
}

#ifdef CUSTOM_BC12_ENABLE
static void sgm4154x_force_detection_dwork_handler(struct work_struct *work) {
    struct sgm4154x_device *sgm = container_of(work, struct sgm4154x_device,
                                        force_detect_dwork.work);

    printk("sgm4154x_force_detection_dwork_handler begin\n");

    sgm->force_detect_count++;

    if(!cust_mid_misc_get_usb_connect_state()){

    printk("sgm4154x_force_detection_dwork_handler usb not connected\n");

    sgm4154x_vbus_in_func(sgm);
    sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_7, SGM4154x_IINDET_EN, SGM4154x_IINDET_EN);
    msleep(50);

    sgm4154x_set_input_curr_lim(sgm->chg_dev, 500000);
    msleep(100);

    sgm4154x_dump_register(sgm->chg_dev);

    sgm4154x_get_bc12_type(sgm, &sgm->chg_type);

	sgm4154x_vbus_out_func(sgm);

    //if(sgm->psy_usb_type != POWER_SUPPLY_USB_TYPE_SDP){
    	power_supply_changed(sgm->charger);
    //}

    sgm4154x_dump_register(sgm->chg_dev);
    }

    printk("sgm4154x_force_detection_dwork_handler end\n");
}
#endif

static int sgm4154x_get_bc12_type(struct sgm4154x_device *sgm, int *type)
{
	int ret = 0;
	u8 chrg_stat;
	int vbus_stat = 0;

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &chrg_stat);
	if (ret){
		ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &chrg_stat);
		if (ret){
			pr_err("%s read SGM4154x_CHRG_STAT fail\n",__func__);
			return ret;
		}
	}
	vbus_stat = (chrg_stat & SGM4154x_VBUS_STAT_MASK) >> 5;

	switch (vbus_stat) {
	case SGM4154x_NOT_CHRGING:
		if(sgm->vbus_attach){
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		}else{
			sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		sgm->psy_desc.type= POWER_SUPPLY_TYPE_USB;
		pr_err("sgm4154x_get_bc12_type charger type: none\n");
#ifdef CUSTOM_BC12_ENABLE
		if (sgm->vbus_attach) {
			if(sgm->force_detect_count <= BC12_FORCE_DETECT_COUNT){
				schedule_delayed_work(&sgm->force_detect_dwork, msecs_to_jiffies(3500));
			}
		}
#endif
		break;
	case SGM4154x_USB_SDP:
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		sgm->psy_desc.type= POWER_SUPPLY_TYPE_USB;
		pr_err("sgm4154x_get_bc12_type charger type: SDP\n");
#ifdef CUSTOM_BC12_ENABLE
        //if (!sgm4154x_dpdm_detect_is_done(sgm)) {
        	if(sgm->force_detect_count <= BC12_FORCE_DETECT_COUNT){
				schedule_delayed_work(&sgm->force_detect_dwork, msecs_to_jiffies(3500));
        	}
        //}
#endif
		break;
	case SGM4154x_USB_CDP:
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB_CDP;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		pr_err("sgm4154x_get_bc12_type charger type: CDP\n");
		break;
	case SGM4154x_USB_DCP:
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		pr_err("sgm4154x_get_bc12_type charger type: DCP\n");
		break;
	case SGM4154x_UNKNOWN:
		if(sgm->vbus_attach){
			sgm->psy_usb_type =  POWER_SUPPLY_USB_TYPE_SDP;
		}else{
			sgm->psy_usb_type =  POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		pr_err("sgm4154x_get_bc12_type charger type: unknown adapter\n");
#ifdef CUSTOM_BC12_ENABLE
        //if (!sgm4154x_dpdm_detect_is_done(sgm)) {
        	if(sgm->force_detect_count <= BC12_FORCE_DETECT_COUNT){
				schedule_delayed_work(&sgm->force_detect_dwork, msecs_to_jiffies(3500));
        	}
        //}
#endif
		break;
	case SGM4154x_NON_STANDARD:
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		pr_err("sgm4154x_get_bc12_type charger type: non-std charger\n");
		break;
	case SGM4154x_OTG_MODE:
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		pr_err("sgm4154x_get_bc12_type charger type: otg mode\n");
		if(!sgm->otg_mode){
			sgm4154x_disable_vbus(NULL);
		}
		break;
	default:
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		pr_err("sgm4154x_get_bc12_type charger type: invalid value, report non-std\n");
		break;
	}

	*type = sgm->psy_chg_type;

#ifdef CUSTOM_BC12_ENABLE
	//sgm4154x_vbus_out_func(sgm);
#endif

	return 0;
}

static irqreturn_t cus_sgm4154x_irq_handler_thread(int irq, void *private)
{
	struct sgm4154x_device *sgm = private;
	int ret = 0;
	u8 reg_val;
	bool prev_power_good = false;
	bool prev_vbus_attach = false;

	pr_err("%s entry\n",__func__);

	prev_power_good = sgm->power_good;
	prev_vbus_attach = sgm->vbus_attach;

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &reg_val);
	if (ret){
		pr_err("%s read SGM4154x_CHRG_STAT fail\n",__func__);
		return IRQ_HANDLED;
	}
	sgm->power_good = !!(reg_val & SGM4154x_PG_STAT);

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_a, &reg_val);
	if (ret){
		pr_err("%s read SGM4154x_CHRG_CTRL_a fail\n",__func__);
		return IRQ_HANDLED;
	}
	sgm->vbus_attach = !!(reg_val & SGM4154x_VBUS_GOOD);

	pr_err("%s 000 prev_power_good=%d sgm->power_good=%d\n",__func__,prev_power_good, sgm->power_good);

	if (!prev_vbus_attach && sgm->vbus_attach) {
		pr_err("%s adapter/usb inserted\n",__func__);
		//sgm->pp_en = true;
		sgm->force_detect_count = 1;
		sgm4154x_set_input_curr_lim(sgm->chg_dev, 500000);
		msleep(30);

		//sgm4154x_vbus_in_func(sgm);
		//msleep(30);
		sgm->chg_config = true;
	} else if (prev_vbus_attach && !sgm->vbus_attach) {
		pr_err("%s adapter/usb removed\n",__func__);
		sgm->power_good = false;
		sgm->vbus_attach = false;
		cancel_delayed_work_sync(&sgm->force_detect_dwork);
		re_try = 5;
		sgm4154x_set_input_curr_lim(sgm->chg_dev, 500000);
		msleep(30);

		//sgm4154x_vbus_out_func(sgm);
		//msleep(30);

		ret = sgm4154x_set_hiz_en(sgm->chg_dev, 1);
		if (ret) {
			pr_err("Failed to enter hiz mode\n");
		}
		msleep(140);

		ret = sgm4154x_set_hiz_en(sgm->chg_dev, 0);
		if (ret) {
			pr_err("Failed to enter hiz mode\n");
		}

		//ret = sgm4154x_get_bc12_type(sgm, &sgm->chg_type);
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->psy_chg_type = POWER_SUPPLY_TYPE_USB;
		sgm->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		power_supply_changed(sgm->charger);

		sgm4154x_dump_register(sgm->chg_dev);

		//sgm4154x_vbus_out_func(sgm);
		sgm->chg_config = false;
		return IRQ_HANDLED;
	}

	sgm4154x_dump_register(sgm->chg_dev);
	if(!sgm->power_good){
		ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &reg_val);
		if (ret){
			pr_err("%s read SGM4154x_CHRG_STAT fail\n",__func__);
			return IRQ_HANDLED;
		}
		sgm->power_good = !!(reg_val & SGM4154x_PG_STAT);
		pr_err("%s 111 prev_power_good=%d sgm->power_good=%d\n",__func__,prev_power_good, sgm->power_good);
	}

	if (!prev_power_good && sgm->power_good) {
		pr_err("%s bc1.2 done\n",__func__);
		ret = sgm4154x_get_bc12_type(sgm, &sgm->chg_type);
		power_supply_changed(sgm->charger);
	}

	return IRQ_HANDLED;
}

static void cus_charger_detect_work_func(struct work_struct *work)
{
	struct sgm4154x_device *sgm = container_of(work, struct sgm4154x_device, charge_detect_delayed_work.work);

	msleep(3000);
	cus_sgm4154x_irq_handler_thread(sgm->client->irq, (void *) sgm);
}

#if SGM4154x_PSY_ENABLE
static char *sgm4154x_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static struct power_supply_desc sgm4154x_power_supply_desc = {
	.name = "sgm4154x-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sgm4154x_usb_type,
	.num_usb_types = ARRAY_SIZE(sgm4154x_usb_type),
	.properties = sgm4154x_power_supply_props,
	.num_properties = ARRAY_SIZE(sgm4154x_power_supply_props),
	.get_property = sgm4154x_charger_get_property,
	.set_property = sgm4154x_charger_set_property,
	.property_is_writeable = sgm4154x_property_is_writeable,
};

static int sgm4154x_power_supply_init(struct sgm4154x_device *sgm,
							struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = sgm,
						.of_node = dev->of_node, };

	psy_cfg.supplied_to = sgm4154x_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sgm4154x_charger_supplied_to);


	memcpy(&sgm->psy_desc, &sgm4154x_power_supply_desc, sizeof(sgm->psy_desc));
	sgm->charger = devm_power_supply_register(sgm->dev,
						 &sgm->psy_desc,
						 &psy_cfg);
	if (IS_ERR(sgm->charger))
		return -EINVAL;
	
	return 0;
}
#endif

static int sgm4154x_hw_init(struct sgm4154x_device *sgm)
{
	int ret = 0;	
	struct power_supply_battery_info bat_info = { };	

	bat_info.constant_charge_current_max_ua =
			SGM4154x_ICHRG_I_DEF_uA;

	bat_info.constant_charge_voltage_max_uv =
			SGM4154x_VREG_V_DEF_uV;

	bat_info.precharge_current_ua =
			SGM4154x_PRECHRG_I_DEF_uA;

	bat_info.charge_term_current_ua =
			SGM4154x_TERMCHRG_I_DEF_uA;

	sgm->init_data.max_ichg =
			SGM4154x_ICHRG_I_MAX_uA;

	sgm->init_data.max_vreg =
			SGM4154x_VREG_V_MAX_uV;
			
	sgm4154x_set_watchdog_timer(sgm,0);

	ret = sgm4154x_set_ichrg_curr(s_chg_dev_otg,
				bat_info.constant_charge_current_max_ua);
	if (ret)
		goto err_out;

	ret = sgm4154x_set_prechrg_curr(sgm, bat_info.precharge_current_ua);
	if (ret)
		goto err_out;

	ret = sgm4154x_set_chrg_volt(s_chg_dev_otg,
				bat_info.constant_charge_voltage_max_uv);
	if (ret)
		goto err_out;

	ret = sgm4154x_set_term_curr(sgm, bat_info.charge_term_current_ua);
	if (ret)
		goto err_out;

	/*ret = sgm4154x_set_input_volt_lim(sgm, sgm->init_data.vlim);
	if (ret)
		goto err_out;*/

	ret = sgm4154x_set_input_curr_lim(s_chg_dev_otg, sgm->init_data.ilim);
	if (ret)
		goto err_out;
	#if 0
	ret = sgm4154x_set_vac_ovp(sgm);//14V
	if (ret)
		goto err_out;	
	#endif
	ret = sgm4154x_set_recharge_volt(sgm, 200);//100~200mv
	if (ret)
		goto err_out;

	sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_a, 0x3, 0x3);

	//add by LQ OVP Setting Init  
	sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_6,SGM4154x_VAC_OVP_MASK,3<<6);//00:5.5v 01:6.5v 10:10.5v  11:14v 
	
	dev_notice(sgm->dev, "ichrg_curr:%d prechrg_curr:%d chrg_vol:%d"
		" term_curr:%d input_curr_lim:%d",
		bat_info.constant_charge_current_max_ua,
		bat_info.precharge_current_ua,
		bat_info.constant_charge_voltage_max_uv,
		bat_info.charge_term_current_ua,
		sgm->init_data.ilim);

	return 0;

err_out:
	return ret;

}

static int sgm4154x_parse_dt(struct device_node *np, struct sgm4154x_device *sgm)
{
	int ret;	
	int irq_gpio = 0, irqn = 0;	
	int chg_en_gpio = 0;	

#if 0
	ret = device_property_read_u32(sgm->dev,
				       "input-voltage-limit-microvolt",
				       &sgm->init_data.vlim);
	if (ret)
		sgm->init_data.vlim = SGM4154x_VINDPM_DEF_uV;

	if (sgm->init_data.vlim > SGM4154x_VINDPM_V_MAX_uV ||
	    sgm->init_data.vlim < SGM4154x_VINDPM_V_MIN_uV)
		return -EINVAL;

	ret = device_property_read_u32(sgm->dev,
				       "input-current-limit-microamp",
				       &sgm->init_data.ilim);
	if (ret)
		sgm->init_data.ilim = SGM4154x_IINDPM_DEF_uA;

	if (sgm->init_data.ilim > SGM4154x_IINDPM_I_MAX_uA ||
	    sgm->init_data.ilim < SGM4154x_IINDPM_I_MIN_uA)
		return -EINVAL;
#endif
	sgm->init_data.vlim = SGM4154x_VINDPM_DEF_uV;
	sgm->init_data.ilim = SGM4154x_IINDPM_DEF_uA;


	if (of_property_read_string(np, "charger_name", &sgm->chg_dev_name) < 0) {
		sgm->chg_dev_name = "primary_chg";
		dev_err(sgm->dev, "no charger name\n");
	}

	irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio))
	{
		dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, irq_gpio);
		return -EINVAL;
	}
	ret = gpio_request(irq_gpio, "sgm4154x_irq_pin");
	if (ret) {
		dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, irq_gpio);
		return ret;
	}
	gpio_direction_input(irq_gpio);
	irqn = gpio_to_irq(irq_gpio);
	if (irqn < 0) {
		dev_err(sgm->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		return irqn;
	}
	sgm->client->irq = irqn;
	
	chg_en_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,chg-en-gpio", 0);
	if (!gpio_is_valid(chg_en_gpio))
	{
		dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
		return -EINVAL;
	}
	ret = gpio_request(chg_en_gpio, "sgm_chg_en_pin");
	if (ret) {
		dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
		return ret;
	}
	gpio_direction_output(chg_en_gpio,0);//default enable charge
	return 0;
}

static int sgm4154x_enable_vbus(struct regulator_dev *rdev)
{	
	int ret = 0;
	struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);
	
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_OTG_EN,
                     SGM4154x_OTG_EN);
	return ret;
}

static int sgm4154x_disable_vbus(struct regulator_dev *rdev)
{
	int ret = 0;
	struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);	

	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_OTG_EN,
                     0);

	return ret;
}

#if SGM4154X_REGULATO_OPS_ENABLE
static int sgm4154x_is_enabled_vbus(struct regulator_dev *rdev)
{
	u8 temp = 0;
	int ret = 0;
	struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);	

	ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_1, &temp);
	return (temp&SGM4154x_OTG_EN)? 1 : 0;
}
#endif

static int sgm4154x_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;

	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	pr_info("%s en = %d\n", __func__, en);

	sgm->otg_mode = en;

	sgm4154x_set_boost_voltage_limit(chg_dev, 5150000);
	sgm4154x_set_boost_current_limit(chg_dev, 1200000);

	if (en) {
		ret = sgm4154x_enable_vbus(NULL);
	} else {
		ret = sgm4154x_disable_vbus(NULL);
	}
	return ret;
}

#if 1
static int sgm4154x_set_boost_voltage_limit(struct charger_device
		*chg_dev, u32 uV)
{	
	int ret = 0;
	char reg_val = -1;
	int i = 0;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	while(i<4){
		if (uV == BOOST_VOLT_LIMIT[i]){
			reg_val = i;
			break;
		}
		i++;
	}
	if (reg_val < 0)
		return reg_val;
	reg_val = reg_val << 4;
	ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_6,
				  SGM4154x_BOOSTV, reg_val);

	return ret;
}
#endif

static int sgm4154x_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{	
	int ret = 0;
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	
	if (uA == BOOST_CURRENT_LIMIT[0]){
		ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_2, SGM4154x_BOOST_LIM,
                     0); 
	}
		
	else if (uA == BOOST_CURRENT_LIMIT[1]){
		ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_2, SGM4154x_BOOST_LIM,
                     BIT(7)); 
	}
	return ret;
}

static int sgm4154x_enable_power_path(struct charger_device *chgdev, bool en)
{
    struct sgm4154x_device *sgm = charger_get_data(chgdev);
	
    printk("%s  en=%d---pp_en=%d\n",__func__,en, sgm->pp_en);

    if(en){
        sgm4154x_set_hiz_en(chgdev, !en);
    }

    //if(en == sgm->pp_en){
    //	return 0;
    //}

    sgm->pp_en = en;

    sgm4154x_set_ivl(chgdev, en ? sgm->mivr : SGM4154x_MIVR_MAX);

    return 0;
}

static int sgm4154x_get_is_power_path_enable(struct charger_device *chgdev, bool *en)
{
    struct sgm4154x_device *sgm = charger_get_data(chgdev);

    *en = sgm->pp_en;

    printk("%s en=%d---pp_en=%d\n",__func__,*en, sgm->pp_en);

    return 0;
}

static int sgm4154x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	switch (event) {
	case 0:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case 1:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

#if SGM4154X_REGULATO_OPS_ENABLE
static struct regulator_ops sgm4154x_vbus_ops = {
	.enable = sgm4154x_enable_vbus,
	.disable = sgm4154x_disable_vbus,
	.is_enabled = sgm4154x_is_enabled_vbus,
};

static const struct regulator_desc sgm4154x_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &sgm4154x_vbus_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sgm4154x_vbus_regulator_register(struct sgm4154x_device *sgm)
{
	struct regulator_config config = {};
	int ret = 0;
	/* otg regulator */
	config.dev = sgm->dev;
	config.driver_data = sgm;
	sgm->otg_rdev = devm_regulator_register(sgm->dev,
						&sgm4154x_otg_rdesc, &config);
	sgm->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	if (IS_ERR(sgm->otg_rdev)) {
		ret = PTR_ERR(sgm->otg_rdev);
		pr_info("%s: register otg regulator failed (%d)\n", __func__, ret);
	}
	return ret;
}
#endif

static struct charger_ops sgm4154x_chg_ops = {
	.enable_hz = sgm4154x_set_hiz_en,

	/* Normal charging */
	.dump_registers = sgm4154x_dump_register,
	.enable = sgm4154x_charging_switch,
	.get_charging_current = NULL,
	.set_charging_current = sgm4154x_set_ichrg_curr,
	.get_input_current = sgm4154x_get_input_curr_lim,
	.set_input_current = sgm4154x_set_input_curr_lim,
	.get_constant_voltage = sgm4154x_get_chrg_volt,
	.set_constant_voltage = sgm4154x_set_chrg_volt,
	.kick_wdt = sgm4154x_reset_watch_dog_timer,
	.set_mivr = sgm4154x_set_ivl,
	.is_charging_done = sgm4154x_get_charging_status,

	/* Safety timer */
	.enable_safety_timer = sgm4154x_enable_safetytimer,
	.is_safety_timer_enabled = sgm4154x_get_is_safetytimer_enable,


	/* Power path */
	.enable_powerpath = sgm4154x_enable_power_path, 
	.is_powerpath_enabled = sgm4154x_get_is_power_path_enable,


	/* OTG */
	.enable_otg = sgm4154x_enable_otg,	
	.set_boost_current_limit = sgm4154x_set_boost_current_limit,
	.event = sgm4154x_do_event,
	
	/* PE+/PE+20 */
#if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
	.send_ta_current_pattern = sgm4154x_en_pe_current_partern,
#else
	.send_ta_current_pattern = NULL,
#endif
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
//	.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,
};

static void sgm4154x_request_irq_handler(struct work_struct *work)
{
	struct delayed_work *sgm4154x_irq_work = NULL;
	struct sgm4154x_device * sgm = NULL;
	int ret;

	printk("--sgm4154x_request_irq_handler begin--\n");

	sgm4154x_irq_work = container_of(work, struct delayed_work, work);
	if(sgm4154x_irq_work == NULL) {
		pr_err("Cann't get sgm4154x_irq_work\n");
		return ;
	}
	sgm = container_of(sgm4154x_irq_work, struct sgm4154x_device, sgm4154x_irq_work);
	
	if (sgm->client->irq) {
		if(sgm->new_charger){
		ret = devm_request_threaded_irq(sgm->dev, sgm->client->irq, NULL,
						cus_sgm4154x_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,"sgm4154x-int",sgm);
						//dev_name(&sgm->client->dev), sgm);
		}else{
		ret = devm_request_threaded_irq(sgm->dev, sgm->client->irq, NULL,
						sgm4154x_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,"sgm4154x-int",sgm);
						//dev_name(&sgm->client->dev), sgm);
		}
		if (ret){
			printk("--sgm4154x_request_irq_handler failed--\n");
			return;
		}
		enable_irq_wake(sgm->client->irq);
	}

	printk("--sgm4154x_request_irq_handler end--\n");
}

static int sgm4154x_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct sgm4154x_device *sgm;
	struct device_node *node = client->dev.of_node;

    char *name = NULL;
	
	pr_info("[%s being]\n", __func__);

	sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
	if (!sgm)
		return -ENOMEM;

	sgm->client = client;
	sgm->dev = dev;	
	
	mutex_init(&sgm->lock);
	mutex_init(&sgm->i2c_rw_lock);
	
	i2c_set_clientdata(client, sgm);
	
	ret = sgm4154x_parse_dt(node, sgm);
	if (ret)
		return ret;
	
	ret = sgm4154x_hw_chipid_detect(sgm);
	if (ret != SGM4154x_PN_ID){
		pr_info("[%s] device not found !!!\n", __func__);
		return ret;
	}	

	sgm->pp_en = true;

#if IS_ENABLED(CONFIG_WB_SGM415XX_USE_NEW_CHARGER)
	sgm->new_charger = true;
#endif
	
	name = devm_kasprintf(sgm->dev, GFP_KERNEL, "%s","sgm4154x suspend wakelock");
	sgm->charger_wakelock = wakeup_source_register(NULL,name);
	
	/* Register charger device */
	sgm->chg_dev = charger_device_register(sgm->chg_dev_name,
						&client->dev, sgm,
						&sgm4154x_chg_ops,
						&sgm4154x_chg_props);
	if (IS_ERR_OR_NULL(sgm->chg_dev)) {
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(sgm->chg_dev);
		return ret;
	}    
	
	/* otg regulator */
	s_chg_dev_otg=sgm->chg_dev;
		
	if(sgm->new_charger){
		INIT_DELAYED_WORK(&sgm->charge_detect_delayed_work, cus_charger_detect_work_func);
	}else{
		INIT_DELAYED_WORK(&sgm->charge_detect_delayed_work, charger_detect_work_func);
	}
	INIT_DELAYED_WORK(&sgm->charge_monitor_work, charger_monitor_work_func);
	INIT_DELAYED_WORK(&sgm->hvdcp_work, sgm4154x_chg_type_hvdcp_work);

	INIT_DELAYED_WORK(&sgm->sgm4154x_irq_work, sgm4154x_request_irq_handler);
	schedule_delayed_work(&sgm->sgm4154x_irq_work, msecs_to_jiffies(1500));

#ifdef CUSTOM_BC12_ENABLE
	if(sgm->new_charger){
		INIT_DELAYED_WORK(&sgm->force_detect_dwork, sgm4154x_force_detection_dwork_handler);
	}
#endif
#if 0
	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						sgm4154x_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						dev_name(&client->dev), sgm);
		if (ret)
			return ret;
		enable_irq_wake(client->irq);
	}
#endif

#if SGM4154x_PSY_ENABLE	
	ret = sgm4154x_power_supply_init(sgm, dev);
	if (ret) {
		pr_err("Failed to register power supply\n");
		return ret;
	}
#endif

	ret = sgm4154x_hw_init(sgm);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		return ret;
	}
	
#if defined(CUSTOM_BC12_ENABLE) //Leo 20240827
	atomic_set(&sgm->attach, 0);
#endif
	//OTG setting
	//sgm4154x_set_otg_voltage(s_chg_dev_otg, 5000000); //5V
	//sgm4154x_set_otg_current(s_chg_dev_otg, 1200000); //1.2A

#if SGM4154X_REGULATO_OPS_ENABLE
	ret = sgm4154x_vbus_regulator_register(sgm);
#endif
	
	//schedule_delayed_work(&sgm->charge_monitor_work,100);
	schedule_delayed_work(&sgm->charge_detect_delayed_work, msecs_to_jiffies(3000));

	pr_info("[%s end]\n", __func__);
	
	return ret;

}

static int sgm4154x_charger_remove(struct i2c_client *client)
{
    struct sgm4154x_device *sgm = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&sgm->charge_monitor_work);

    regulator_unregister(sgm->otg_rdev);

#if SGM4154x_PSY_ENABLE
    power_supply_unregister(sgm->charger);
#endif
	
	mutex_destroy(&sgm->lock);
    mutex_destroy(&sgm->i2c_rw_lock);       

    return 0;
}

static void sgm4154x_charger_shutdown(struct i2c_client *client)
{
    int ret = 0;
	
	struct sgm4154x_device *sgm = i2c_get_clientdata(client);
    ret = sgm4154x_disable_charger(sgm);
    if (ret) {
        pr_err("Failed to disable charger, ret = %d\n", ret);
    }
    pr_info("sgm4154x_charger_shutdown\n");
}

static const struct i2c_device_id sgm4154x_i2c_ids[] = {
	{ "sgm41541", 0 },
	{ "sgm41542", 1 },
	{ "sgm41543", 2 },
	{ "sgm41543D", 3 },
	{ "sgm41513", 4 },
	{ "sgm41513A", 5 },
	{ "sgm41513D", 6 },
	{ "sgm41516", 7 },
	{ "sgm41516D", 8 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm4154x_i2c_ids);

static const struct of_device_id sgm4154x_of_match[] = {
	{ .compatible = "sgm,sgm4154x", },
	{ .compatible = "sgm,sgm41541", },
	{ .compatible = "sgm,sgm41542", },
	{ .compatible = "sgm,sgm41543", },
	{ .compatible = "sgm,sgm41543D", },
	{ .compatible = "sgm,sgm41513", },
	{ .compatible = "sgm,sgm41513A", },
	{ .compatible = "sgm,sgm41513D", },
	{ .compatible = "sgm,sgm41516", },
	{ .compatible = "sgm,sgm41516D", },
	{ },
};
MODULE_DEVICE_TABLE(of, sgm4154x_of_match);


static struct i2c_driver sgm4154x_driver = {
	.driver = {
		.name = "sgm4154x-charger",
		.of_match_table = sgm4154x_of_match,		
	},
	.probe = sgm4154x_driver_probe,
	.remove = sgm4154x_charger_remove,
	.shutdown = sgm4154x_charger_shutdown,
	.id_table = sgm4154x_i2c_ids,
};
module_i2c_driver(sgm4154x_driver);

MODULE_AUTHOR(" qhq <Allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm4154x charger driver");
MODULE_LICENSE("GPL v2");
