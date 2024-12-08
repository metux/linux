// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for the TI hl7015 battery charger.
 *
 * Author: Mark A. Greer <mgreer@animalcreek.com>
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/extcon-provider.h>
#include <linux/of_gpio.h>
#include <linux/extcon.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>

#include "charger_class.h"
#include "mtk_charger.h"



//#define SET_OTG_CURRENT_TO_1A


#define	HL7015_MANUFACTURER	"Texas Instruments"

#define HL7015_REG_ISC		0x00 /* Input Source Control */
#define HL7015_REG_ISC_EN_HIZ_MASK		BIT(7)
#define HL7015_REG_ISC_EN_HIZ_SHIFT		7
#define HL7015_REG_ISC_VINDPM_MASK		(BIT(6) | BIT(5) | BIT(4) | \
						 BIT(3))
#define HL7015_REG_ISC_VINDPM_SHIFT		3
#define HL7015_REG_ISC_IINLIM_MASK		(BIT(2) | BIT(1) | BIT(0))
#define HL7015_REG_ISC_IINLIM_SHIFT		0

#define HL7015_REG_POC		0x01 /* Power-On Configuration */
#define HL7015_REG_POC_RESET_MASK		BIT(7)
#define HL7015_REG_POC_RESET_SHIFT		7
#define HL7015_REG_POC_WDT_RESET_MASK		BIT(6)
#define HL7015_REG_POC_WDT_RESET_SHIFT		6
#define HL7015_REG_POC_CHG_CONFIG_MASK		(BIT(5) | BIT(4))
#define HL7015_REG_POC_CHG_CONFIG_SHIFT	4
#define HL7015_REG_POC_CHG_CONFIG_DISABLE		0x0
#define HL7015_REG_POC_CHG_CONFIG_CHARGE		0x1
#define HL7015_REG_POC_CHG_CONFIG_OTG			0x2
#define HL7015_REG_POC_SYS_MIN_MASK		(BIT(3) | BIT(2) | BIT(1))
#define HL7015_REG_POC_SYS_MIN_SHIFT		1
#define HL7015_REG_POC_SYS_MIN_MIN			3000
#define HL7015_REG_POC_SYS_MIN_MAX			3700
#define HL7015_REG_POC_BOOST_LIM_MASK		BIT(0)
#define HL7015_REG_POC_BOOST_LIM_SHIFT		0

#define HL7015_REG_CCC		0x02 /* Charge Current Control */
#define HL7015_REG_CCC_ICHG_MASK		(BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4) | BIT(3) | BIT(2))
#define HL7015_REG_CCC_ICHG_SHIFT		2
#define HL7015_REG_CCC_FORCE_20PCT_MASK	BIT(0)
#define HL7015_REG_CCC_FORCE_20PCT_SHIFT	0

#define HL7015_REG_PCTCC	0x03 /* Pre-charge/Termination Current Cntl */
#define HL7015_REG_PCTCC_IPRECHG_MASK		(BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4))
#define HL7015_REG_PCTCC_IPRECHG_SHIFT		4
#define HL7015_REG_PCTCC_IPRECHG_MIN			128
#define HL7015_REG_PCTCC_IPRECHG_MAX			2048
#define HL7015_REG_PCTCC_ITERM_MASK		(BIT(3) | BIT(2) | BIT(1) | \
						 BIT(0))
#define HL7015_REG_PCTCC_ITERM_SHIFT		0
#define HL7015_REG_PCTCC_ITERM_MIN			128
#define HL7015_REG_PCTCC_ITERM_MAX			2048

#define HL7015_REG_CVC		0x04 /* Charge Voltage Control */
#define HL7015_REG_CVC_VREG_MASK		(BIT(7) | BIT(6) | BIT(5) | \
						 BIT(4) | BIT(3) | BIT(2))
#define HL7015_REG_CVC_VREG_SHIFT		2
#define HL7015_REG_CVC_BATLOWV_MASK		BIT(1)
#define HL7015_REG_CVC_BATLOWV_SHIFT		1
#define HL7015_REG_CVC_VRECHG_MASK		BIT(0)
#define HL7015_REG_CVC_VRECHG_SHIFT		0

#define HL7015_REG_CTTC	0x05 /* Charge Term/Timer Control */
#define HL7015_REG_CTTC_EN_TERM_MASK		BIT(7)
#define HL7015_REG_CTTC_EN_TERM_SHIFT		7
#define HL7015_REG_CTTC_TERM_STAT_MASK		BIT(6)
#define HL7015_REG_CTTC_TERM_STAT_SHIFT	6
#define HL7015_REG_CTTC_WATCHDOG_MASK		(BIT(5) | BIT(4))
#define HL7015_REG_CTTC_WATCHDOG_SHIFT		4
#define HL7015_REG_CTTC_EN_TIMER_MASK		BIT(3)
#define HL7015_REG_CTTC_EN_TIMER_SHIFT		3
#define HL7015_REG_CTTC_CHG_TIMER_MASK		(BIT(2) | BIT(1))
#define HL7015_REG_CTTC_CHG_TIMER_SHIFT	1
#define HL7015_REG_CTTC_JEITA_ISET_MASK	BIT(0)
#define HL7015_REG_CTTC_JEITA_ISET_SHIFT	0

#define HL7015_REG_ICTRC	0x0C /* IR Comp/Thermal Regulation Control */
#define HL7015_REG_ICTRC_BAT_COMP_MASK		(BIT(7) | BIT(6) | BIT(5))
#define HL7015_REG_ICTRC_BAT_COMP_SHIFT	5
#define HL7015_REG_ICTRC_VCLAMP_MASK		(BIT(4) | BIT(3) | BIT(2))
#define HL7015_REG_ICTRC_VCLAMP_SHIFT		2

#define HL7015_REG_THERMAL	0x06
#define HL7015_REG_ICTRC_TREG_MASK		(BIT(1) | BIT(0))
#define HL7015_REG_ICTRC_TREG_SHIFT		0

#define HL7015_REG_MOC		0x07 /* Misc. Operation Control */
#define HL7015_REG_MOC_DPDM_EN_MASK		BIT(7)
#define HL7015_REG_MOC_DPDM_EN_SHIFT		7
#define HL7015_REG_MOC_TMR2X_EN_MASK		BIT(6)
#define HL7015_REG_MOC_TMR2X_EN_SHIFT		6
#define HL7015_REG_MOC_BATFET_DISABLE_MASK	BIT(5)
#define HL7015_REG_MOC_BATFET_DISABLE_SHIFT	5
#define HL7015_REG_MOC_JEITA_VSET_MASK		BIT(4)
#define HL7015_REG_MOC_JEITA_VSET_SHIFT	4
#define HL7015_REG_MOC_INT_MASK_MASK		(BIT(1) | BIT(0))
#define HL7015_REG_MOC_INT_MASK_SHIFT		0

#define HL7015_REG_SS		0x08 /* System Status */
#define HL7015_REG_SS_VBUS_STAT_MASK		(BIT(7) | BIT(6))
#define HL7015_REG_SS_VBUS_STAT_SHIFT		6
#define HL7015_REG_SS_CHRG_STAT_MASK		(BIT(5) | BIT(4))
#define HL7015_REG_SS_CHRG_STAT_SHIFT		4
#define HL7015_REG_SS_DPM_STAT_MASK		BIT(3)
#define HL7015_REG_SS_DPM_STAT_SHIFT		3
#define HL7015_REG_SS_PG_STAT_MASK		BIT(2)
#define HL7015_REG_SS_PG_STAT_SHIFT		2
#define HL7015_REG_SS_THERM_STAT_MASK		BIT(1)
#define HL7015_REG_SS_THERM_STAT_SHIFT		1
#define HL7015_REG_SS_VSYS_STAT_MASK		BIT(0)
#define HL7015_REG_SS_VSYS_STAT_SHIFT		0

#define HL7015_REG_F		0x09 /* Fault */
#define HL7015_REG_F_WATCHDOG_FAULT_MASK	BIT(7)
#define HL7015_REG_F_WATCHDOG_FAULT_SHIFT	7
#define HL7015_REG_F_BOOST_FAULT_MASK		BIT(6)
#define HL7015_REG_F_BOOST_FAULT_SHIFT		6
#define HL7015_REG_F_CHRG_FAULT_MASK		(BIT(5) | BIT(4))
#define HL7015_REG_F_CHRG_FAULT_SHIFT		4
#define HL7015_REG_F_BAT_FAULT_MASK		BIT(3)
#define HL7015_REG_F_BAT_FAULT_SHIFT		3
#define HL7015_REG_F_NTC_FAULT_MASK		(BIT(2) | BIT(1) | BIT(0))
#define HL7015_REG_F_NTC_FAULT_SHIFT		0

#define HL7015_REG_VPRS	0x0A /* Vendor/Part/Revision Status */
#define HL7015_REG_VPRS_PN_MASK		(BIT(5) | BIT(4) | BIT(3))
#define HL7015_REG_VPRS_PN_SHIFT		3
#define HL7015_REG_VPRS_PN_24190			0x6//0x4
#define HL7015_REG_VPRS_PN_24192			0x5 /* Also 24193, 24196 */
#define HL7015_REG_VPRS_PN_24192I			0x3
#define HL7015_REG_VPRS_TS_PROFILE_MASK	BIT(2)
#define HL7015_REG_VPRS_TS_PROFILE_SHIFT	2
#define HL7015_REG_VPRS_DEV_REG_MASK		(BIT(1) | BIT(0))
#define HL7015_REG_VPRS_DEV_REG_SHIFT		0

#define HL7015_CON0		0x00
#define HL7015_CON1		0x01
#define HL7015_CON2		0x02
#define HL7015_CON3		0x03
#define HL7015_CON4		0x04
#define HL7015_CON5		0x05
#define HL7015_CON6		0x06
#define HL7015_CON7		0x07
#define HL7015_CON8		0x08
#define HL7015_CON9		0x09
#define HL7015_CONA		0x0A
#define HL7015_CONB		0x0B
#define HL7015_CONC		0x0C
#define HL7015_COND		0x0D
#define HL7015_REG_NUM		14

/* CON0 */
#define CON0_EN_HIZ_MASK	0x01
#define CON0_EN_HIZ_SHIFT	7
#define CON0_VINDPM_MASK	0x1F   //edited by selena for I2C error 
#define CON0_VINDPM_SHIFT	3
#define CON0_IINLIM_MASK	0x07
#define CON0_IINLIM_SHIFT	0

/* CON1 */
#define CON1_REG_RESET_MASK			0x01
#define CON1_REG_RESET_SHIFT		7
#define CON1_I2C_WDT_RESET_MASK		0x01
#define CON1_I2C_WDT_RESET_SHIFT	6
#define CON1_CHG_CONFIG_MASK		0x03
#define CON1_CHG_CONFIG_SHIFT		4
#define CON1_SYS_MIN_MASK			0x07
#define CON1_SYS_MIN_SHIFT			1
#define CON1_BOOST_LIM_MASK			0x01
#define CON1_BOOST_LIM_SHIFT		0

/* CON2 */
#define CON2_ICHG_MASK			0x3F
#define CON2_ICHG_SHIFT			2
#define CON2_BCLOD_MASK			0x01
#define CON2_BCLOD_SHIFT		1
#define CON2_FORCE_20PCT_MASK	0x01
#define CON2_FORCE_20PCT_SHIFT	0

/* CON3 */
#define CON3_IPRECHG_MASK	0x0F
#define CON3_IPRECHG_SHIFT	4
#define CON3_ITERM_MASK		0x0F
#define CON3_ITERM_SHIFT	0

/* CON4 */
#define CON4_VREG_MASK		0x3F
#define CON4_VREG_SHIFT		2
#define CON4_BATLOWV_MASK	0x01
#define CON4_BATLOWV_SHIFT	1
#define CON4_VRECHG_MASK	0x01
#define CON4_VRECHG_SHIFT	0

/* CON5 */
#define CON5_EN_TERM_MASK			0x01
#define CON5_EN_TERM_SHIFT			7
#define CON5_TERM_STAT_MASK			0x01
#define CON5_TERM_STAT_SHIFT		6
#define CON5_WATCHDOG_MASK			0x03
#define CON5_WATCHDOG_SHIFT			4
#define CON5_EN_SAFE_TIMER_MASK		0x01
#define CON5_EN_SAFE_TIMER_SHIFT	3
#define CON5_CHG_TIMER_MASK			0x03
#define CON5_CHG_TIMER_SHIFT		1
#define CON5_Reserved_MASK			0x01
#define CON5_Reserved_SHIFT			0

/* CON6 */
#define CON6_BOOSTV_MASK	0x0F
#define CON6_BOOSTV_SHIFT	4
#define CON6_BHOT_MASK		0x03
#define CON6_BHOT_SHIFT		2
#define CON6_TREG_MASK		0x03
#define CON6_TREG_SHIFT		0

/* CON7 */
#define CON7_DPDM_EN_MASK				0x01
#define CON7_DPDM_EN_SHIFT				7
#define CON7_TMR2X_EN_MASK				0x01
#define CON7_TMR2X_EN_SHIFT				6
#define CON7_PPFET_DISABLE_MASK			0x01
#define CON7_PPFET_DISABLE_SHIFT		5
//three bits were reserved
#define CON7_CHRG_FAULT_INT_MASK_MASK	0x01
#define CON7_CHRG_FAULT_INT_MASK_SHIFT	1
#define CON7_BAT_FAULT_INT_MASK_MASK	0x01
#define CON7_BAT_FAULT_INT_MASK_SHIFT	0

/* CON8 */
#define CON8_VIN_STAT_MASK		0x03
#define CON8_VIN_STAT_SHIFT		6
#define CON8_CHRG_STAT_MASK		0x03
#define CON8_CHRG_STAT_SHIFT	4
#define CON8_DPM_STAT_MASK		0x01
#define CON8_DPM_STAT_SHIFT		3
#define CON8_PG_STAT_MASK		0x01
#define CON8_PG_STAT_SHIFT		2
#define CON8_THERM_STAT_MASK	0x01
#define CON8_THERM_STAT_SHIFT	1
#define CON8_VSYS_STAT_MASK		0x01
#define CON8_VSYS_STAT_SHIFT	0

#define CON8_VBUS_STAT_MASK 0xC0
#define CON8_VBUS_STAT_SHIFT 6
#define HL7015_VBUS_TYPE_UNKNOWN 0
#define HL7015_VBUS_TYPE_SDP 1
#define HL7015_VBUS_TYPE_DCP 2


/* CON9 */
#define CON9_WATCHDOG_FAULT_MASK	0x01
#define CON9_WATCHDOG_FAULT_SHIFT	7
#define CON9_OTG_FAULT_MASK			0x01
#define CON9_OTG_FAULT_SHIFT		6
#define CON9_CHRG_FAULT_MASK		0x03
#define CON9_CHRG_FAULT_SHIFT		4
#define CON9_BAT_FAULT_MASK			0x01
#define CON9_BAT_FAULT_SHIFT		3
// one bit was reserved
#define CON9_NTC_FAULT_MASK			0x07
#define CON9_NTC_FAULT_SHIFT		0

/* CONA */
// vender info register

/* CONB */
#define CONB_TSR_MASK				0x03
#define CONB_TSR_SHIFT				6
#define CONB_TRSP_MASK				0x01
#define CONB_TRSP_SHIFT				5
#define CONB_DIS_RECONNECT_MASK		0x01
#define CONB_DIS_RECONNECT_SHIFT	4
#define CONB_DIS_SR_INCHG_MASK		0x01
#define CONB_DIS_SR_INCHG_SHIFT		3
#define CONB_TSHIP_MASK				0x07
#define CONB_TSHIP_SHIFT			0

/* CONC */
#define CONC_BAT_COMP_MASK			0x07
#define CONC_BAT_COMP_SHIFT			5
#define CONC_BAT_VCLAMP_MASK		0x07
#define CONC_BAT_VCLAMP_SHIFT		2
// one bit was reserved
#define CONC_BOOST_9V_EN_MASK		0x01
#define CONC_BOOST_9V_EN_SHIFT		0

/* COND */
// one bit was reserved
#define COND_DISABLE_TS_MASK		0x01
#define COND_DISABLE_TS_SHIFT		6
#define COND_VINDPM_OFFSET_MASK		0x01
#define COND_VINDPM_OFFSET_SHIFT	5
// five bits were reserved

#if IS_ENABLED(CONFIG_TCPC_FUSB302)
#define EXT_PD_CHARGER 1
extern bool get_other_pd_reset_state(void);
extern int get_pd_input_current(void);
extern void set_pd_input_current(int input_current);
#else
#define EXT_PD_CHARGER 0
#endif

struct hl7015_platform_data {
	const struct regulator_init_data *regulator_init_data;
};


/*
 * The FAULT register is latched by the hl7015 (except for NTC_FAULT)
 * so the first read after a fault returns the latched value and subsequent
 * reads return the current value.  In order to return the fault status
 * to the user, have the interrupt handler save the reg's value and retrieve
 * it in the appropriate health/status routine.
 */
struct hl7015_dev_info {
	struct i2c_client		*client;
	struct device			*dev;
	struct extcon_dev		*edev;
	struct power_supply		*charger;
	#if 0
	struct power_supply		*battery;
	#endif
	struct delayed_work		input_current_limit_work;
	char				model_name[I2C_NAME_SIZE];
	bool				initialized;
	bool				irq_event;
	u16				sys_min;
	u16				iprechg;
	u16				iterm;
	struct mutex			f_reg_lock;
	u8				f_reg;
	u8				ss_reg;
	u8				watchdog;
	int psy_usb_type;
	struct power_supply_desc psy_desc;
	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct charger_properties chg_props;
	struct alarm otg_kthread_gtimer;
	struct workqueue_struct *otg_boost_workq;
	struct work_struct kick_work;
	unsigned int polling_interval;
	bool polling_enabled;

	struct delayed_work read_byte_work;

	const char *chg_dev_name;
	const char *eint_name;
	u8 power_good;
	int irq;
	
#if EXT_PD_CHARGER
    struct workqueue_struct *usb_charger_wq;
    struct delayed_work     pd_work;
    struct delayed_work     pd_enable_work;	
    struct notifier_block   cable_pd_nb;
    struct extcon_dev       *cable_edev;
    bool pd_charger_enable;

    int fast_input_cur;
#endif
};

static struct i2c_client *new_client;
static int is_otg_mode = 0;
static int chg_en_gpio;
static int drvbus_gpio;
static struct hl7015_dev_info *g_bdi;
void hl7015_set_i2cwatchdog(unsigned int val);
void hl7015_set_ichg(unsigned int val);
static struct wakeup_source *hl7015_wake_lock;
void hl7015_set_chg_config(unsigned int val);
static const unsigned int hl7015_usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_NONE,
};
struct power_supply_desc hl7015_charger_desc;
static DEFINE_MUTEX(hl7015_i2c_access);
static DEFINE_MUTEX(hl7015_access_lock);
/*
 * The tables below provide a 2-way mapping for the value that goes in
 * the register field and the real-world value that it represents.
 * The index of the array is the value that goes in the register; the
 * number at that index in the array is the real-world value that it
 * represents.
 */

/* REG06[1:0] (TREG) in tenths of degrees Celsius */
static const int hl7015_ictrc_treg_values[] = {
	600, 800, 1000, 1200
};

const unsigned int VBAT_CVTH[] = {
	3504000, 3520000, 3536000, 3552000,
	3568000, 3584000, 3600000, 3616000,
	3632000, 3648000, 3664000, 3680000,
	3696000, 3712000, 3728000, 3744000,
	3760000, 3776000, 3792000, 3808000,
	3824000, 3840000, 3856000, 3872000,
	3888000, 3904000, 3920000, 3936000,
	3952000, 3968000, 3984000, 4000000,
	4016000, 4032000, 4048000, 4064000,
	4080000, 4096000, 4112000, 4128000,
	4144000, 4160000, 4176000, 4192000,
	4208000, 4224000, 4240000, 4256000,
	4272000, 4288000, 4304000, 4320000,
	4336000, 4352000, 4368000, 4386000,
	4400000, 4416000, 4432000, 4448000,
	4464000, 4480000, 4496000, 4512000
};

const unsigned int VINDPM_NORMAL_CVTH[] = {
	3880000, 3970000, 4030000, 4120000,
	4200000, 4290000, 4350000, 4440000,
	4520000, 4610000, 4670000, 4760000,
	4840000, 4930000, 4990000, 5080000
};

const unsigned int VINDPM_HIGH_CVTH[] = {
	8320000, 8500000, 8640000, 8820000,
	9010000, 9190000, 9330000, 9510000,
	9690000, 9870000, 10010000, 10190000,
	10380000, 10560000, 10700000, 10880000
};

const unsigned int SYS_MIN_CVTH[] = {
	3000000, 3100000, 3200000, 3300000,
	3400000, 3500000, 3600000, 3700000
};

const unsigned int BOOSTV_CVTH[] = {
	4550000, 4614000, 4678000, 4742000,
	4806000, 4870000, 4934000, 4998000,
	5062000, 5126000, 5190000, 5254000,
	5318000, 5382000, 5446000, 5510000
};

const unsigned int CSTH[] = {
	512000, 576000, 640000, 704000,
	768000, 832000, 896000, 960000,
	1024000, 1088000, 1152000, 1216000,
	1280000, 1344000, 1408000, 1472000,
	1536000, 1600000, 1664000, 1728000,
	1792000, 1856000, 1920000, 1984000,
	2048000, 2112000, 2176000, 2240000,
	2304000, 2368000, 2432000, 2496000, 
	2560000, 2624000, 2688000, 2752000,
	2816000, 2880000, 2944000, 3008000,
	3072000, 3200000, 3264000, 3328000, 
	3392000, 3456000, 3520000, 3584000,
	3648000, 3712000, 3776000, 3840000,
	3904000, 3968000, 4032000, 4096000, 
	4160000, 4224000, 4288000, 4352000,
	4416000, 4480000, 4544000
};

/*HL7015 REG00 IINLIM[5:0]*/
const unsigned int INPUT_CSTH[] = {
	100000, 150000, 500000, 900000,
	1000000, 1500000, 2000000, 3000000
};

const unsigned int IPRE_CSTH[] = {
	128000, 256000, 384000, 512000,
	640000, 768000, 896000, 1024000,
	1152000, 1280000, 1408000, 1536000,
	1664000, 1792000, 1920000, 2048000
};

const unsigned int ITERM_CSTH[] = {
	128000, 256000, 384000, 512000,
	640000, 768000, 896000, 1024000,
	1152000, 1280000, 1408000, 1536000,
	1664000, 1792000, 1920000, 2048000
};

unsigned int charging_value_to_parameter_hl7015(const unsigned int *parameter, const unsigned int array_size,
					const unsigned int val)
{
	if (val < array_size)
		return parameter[val];
	pr_err("[hl7015] Can't find the parameter\n");
	return parameter[0];
}

unsigned int charging_parameter_to_value_hl7015(const unsigned int *parameter, const unsigned int array_size,
					const unsigned int val)
{
	unsigned int i;

	pr_err("[hl7015] array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_err("[hl7015] NO register value match\n");
	/* TODO: ASSERT(0);	// not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number,
					 unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		for (i = (number - 1); i != 0; i--) {	/* max value in the last element */
			if (pList[i] <= level) {
				pr_err("[hl7019d] zzf_%d<=%d, i=%d\n", pList[i], level, i);
				return pList[i];
			}
		}
		pr_err("[hl7019d] Can't find closest level\n");
		return pList[0];
		/* return 000; */
	} else {
		for (i = 0; i < number; i++) {	/* max value in the first element */
			if (pList[i] <= level)
				return pList[i];
		}
		pr_err("[hl7019d] Can't find closest level\n");
		return pList[number - 1];
		/* return 000; */
	}
}

/*
 * Return the index in 'tbl' of greatest value that is less than or equal to
 * 'val'.  The index range returned is 0 to 'tbl_size' - 1.  Assumes that
 * the values in 'tbl' are sorted from smallest to largest and 'tbl_size'
 * is less than 2^8.
 */
static u8 hl7015_find_idx(const int tbl[], int tbl_size, int v)
{
	int i;

	for (i = 1; i < tbl_size; i++)
		if (v < tbl[i])
			break;

	return i - 1;
}

/* Basic driver I/O routines */

static int hl7015_read(struct hl7015_dev_info *bdi, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(bdi->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int hl7015_write(struct hl7015_dev_info *bdi, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(bdi->client, reg, data);
}

static int hl7015_read_mask(struct hl7015_dev_info *bdi, u8 reg,
		u8 mask, u8 shift, u8 *data)
{
	u8 v;
	int ret;

	ret = hl7015_read(bdi, reg, &v);
	if (ret < 0)
		return ret;

	v &= mask;
	v >>= shift;
	*data = v;

//	printk("[charging]------hl7015_read_mask--reg[%d]---data=%d\n",reg,*data);

	return 0;
}

static int hl7015_write_mask(struct hl7015_dev_info *bdi, u8 reg,
		u8 mask, u8 shift, u8 data)
{
	u8 v;
	int ret;

	ret = hl7015_read(bdi, reg, &v);
	if (ret < 0)
		return ret;
	
//	printk("[charging]------hl7015_write_mask——read--reg[%d]--data=%d\n",reg,v);
	
	v &= ~mask;
	v |= ((data << shift) & mask);
	
//	printk("[charging]------hl7015_write_mask-reg[%d]=---data=%d\n",reg,v);

	return hl7015_write(bdi, reg, v);
}

static int hl7015_get_field_val(struct hl7015_dev_info *bdi,
		u8 reg, u8 mask, u8 shift,
		const int tbl[], int tbl_size,
		int *val)
{
	u8 v;
	int ret;

	ret = hl7015_read_mask(bdi, reg, mask, shift, &v);
	if (ret < 0)
		return ret;

	v = (v >= tbl_size) ? (tbl_size - 1) : v;
	*val = tbl[v];

	return 0;
}

static int hl7015_set_field_val(struct hl7015_dev_info *bdi,
		u8 reg, u8 mask, u8 shift,
		const int tbl[], int tbl_size,
		int val)
{
	u8 idx;

	idx = hl7015_find_idx(tbl, tbl_size, val);

	return hl7015_write_mask(bdi, reg, mask, shift, idx);
}

static int __hl7015_read_byte(u8 reg_addr, u8 *rd_buf, int rd_len)
{
#if 0
	int ret = 0;
	struct i2c_adapter *adap = new_client->adapter;
	struct i2c_msg msg[2];
	u8 *w_buf = NULL;
	u8 *r_buf = NULL;

	memset(msg, 0, 2 * sizeof(struct i2c_msg));

	w_buf = kzalloc(1, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;
	r_buf = kzalloc(rd_len, GFP_KERNEL);
	if (r_buf == NULL)
		return -1;

	*w_buf = reg_addr;

	msg[0].addr = new_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = w_buf;

	msg[1].addr = new_client->addr;
	msg[1].flags = 1;
	msg[1].len = rd_len;
	msg[1].buf = r_buf;

	ret = i2c_transfer(adap, msg, 2);

	memcpy(rd_buf, r_buf, rd_len);

	kfree(w_buf);
	kfree(r_buf);
	return ret;
#endif
	int ret;

	ret = hl7015_read(g_bdi, reg_addr, rd_buf);
	return ret;
}

static int hl7015_read_byte(u8 reg_addr, u8 *rd_buf, int rd_len)
{
	int ret = 0;
	mutex_lock(&hl7015_i2c_access);
	ret = __hl7015_read_byte(reg_addr, rd_buf, rd_len);
	mutex_unlock(&hl7015_i2c_access);
	return ret;
}

int __hl7015_write_byte(unsigned char reg_num, u8 *wr_buf, int wr_len)
{
#if 0
	int ret = 0;
	struct i2c_adapter *adap = new_client->adapter;
	struct i2c_msg msg;
	u8 *w_buf = NULL;

	memset(&msg, 0, sizeof(struct i2c_msg));

	w_buf = kzalloc(wr_len, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;

	w_buf[0] = reg_num;
	memcpy(w_buf + 1, wr_buf, wr_len);

	msg.addr = new_client->addr;
	msg.flags = 0;
	msg.len = wr_len;
	msg.buf = w_buf;

	ret = i2c_transfer(adap, &msg, 1);

	kfree(w_buf);
	return ret;
#endif
	int ret = 0;
	ret = hl7015_write(g_bdi, reg_num, *wr_buf);
	return ret;
}

int hl7015_write_byte(unsigned char reg_num, u8 *wr_buf, int wr_len)
{
	int ret = 0;
	mutex_lock(&hl7015_i2c_access);
	ret = __hl7015_write_byte(reg_num, wr_buf, wr_len);
	mutex_unlock(&hl7015_i2c_access);
	return ret;
}

unsigned int hl7015_read_interface(unsigned char reg_num, unsigned char *val, unsigned char MASK,
				unsigned char SHIFT)
{
	unsigned char hl7015_reg = 0;
	unsigned int ret = 0;

	ret = hl7015_read_byte(reg_num, &hl7015_reg, 1);
	//pr_err("[hl7015_read_interface] Reg[%x]=0x%x\n", reg_num, hl7015_reg);
	hl7015_reg &= (MASK << SHIFT);
	*val = (hl7015_reg >> SHIFT);
	//pr_err("[hl7015_read_interface] val=0x%x\n", *val);

	return ret;
}

unsigned int hl7015_config_interface(unsigned char reg_num, unsigned char val, unsigned char MASK,
					unsigned char SHIFT)
{
	unsigned char hl7015_reg = 0;
	unsigned char hl7015_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&hl7015_access_lock);
	ret = hl7015_read_byte(reg_num, &hl7015_reg, 1);
	hl7015_reg_ori = hl7015_reg;
	hl7015_reg &= ~(MASK << SHIFT);
	hl7015_reg |= (val << SHIFT);
	//if (reg_num == HL7015_CON4)
		//hl7015_reg &= ~(1 << CON4_RESET_SHIFT);

	ret = hl7015_write_byte(reg_num, &hl7015_reg, 2);
	mutex_unlock(&hl7015_access_lock);
	pr_err("[hl7015_config_interface] write Reg[%x]=0x%x from 0x%x\n", reg_num,
			hl7015_reg, hl7015_reg_ori);
	/* Check */
	/* hl7015_read_byte(reg_num, &hl7015_reg, 1); */
	/* pr_err("[hl7015_config_interface] Check Reg[%x]=0x%x\n", reg_num, hl7015_reg); */

	return ret;
}

/* write one register directly */
unsigned int hl7015_reg_config_interface(unsigned char reg_num, unsigned char val)
{
	unsigned char hl7015_reg = val;
	
	pr_err("[hl7015_reg__config_interface] write Reg[%x]=0x%x\n", reg_num, hl7015_reg);

	return hl7015_write_byte(reg_num, &hl7015_reg, 2);
}



#ifdef CONFIG_SYSFS
/*
 * There are a numerous options that are configurable on the hl7015
 * that go well beyond what the power_supply properties provide access to.
 * Provide sysfs access to them so they can be examined and possibly modified
 * on the fly.  They will be provided for the charger power_supply object only
 * and will be prefixed by 'f_' to make them easier to recognize.
 */

#define HL7015_SYSFS_FIELD(_name, r, f, m, store)			\
{									\
	.attr	= __ATTR(f_##_name, m, hl7015_sysfs_show, store),	\
	.reg	= HL7015_REG_##r,					\
	.mask	= HL7015_REG_##r##_##f##_MASK,				\
	.shift	= HL7015_REG_##r##_##f##_SHIFT,			\
}

#define HL7015_SYSFS_FIELD_RW(_name, r, f)				\
		HL7015_SYSFS_FIELD(_name, r, f, S_IWUSR | S_IRUGO,	\
				hl7015_sysfs_store)

#define HL7015_SYSFS_FIELD_RO(_name, r, f)				\
		HL7015_SYSFS_FIELD(_name, r, f, S_IRUGO, NULL)

static ssize_t hl7015_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t hl7015_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

struct hl7015_sysfs_field_info {
	struct device_attribute	attr;
	u8	reg;
	u8	mask;
	u8	shift;
};

/* On i386 ptrace-abi.h defines SS that breaks the macro calls below. */
#undef SS

static struct hl7015_sysfs_field_info hl7015_sysfs_field_tbl[] = {
			/*	sysfs name	reg	field in reg */
	HL7015_SYSFS_FIELD_RW(en_hiz,		ISC,	EN_HIZ),
	HL7015_SYSFS_FIELD_RW(vindpm,		ISC,	VINDPM),
	HL7015_SYSFS_FIELD_RW(iinlim,		ISC,	IINLIM),
	HL7015_SYSFS_FIELD_RW(chg_config,	POC,	CHG_CONFIG),
	HL7015_SYSFS_FIELD_RW(sys_min,		POC,	SYS_MIN),
	HL7015_SYSFS_FIELD_RW(boost_lim,	POC,	BOOST_LIM),
	HL7015_SYSFS_FIELD_RW(ichg,		CCC,	ICHG),
	HL7015_SYSFS_FIELD_RW(force_20_pct,	CCC,	FORCE_20PCT),
	HL7015_SYSFS_FIELD_RW(iprechg,		PCTCC,	IPRECHG),
	HL7015_SYSFS_FIELD_RW(iterm,		PCTCC,	ITERM),
	HL7015_SYSFS_FIELD_RW(vreg,		CVC,	VREG),
	HL7015_SYSFS_FIELD_RW(batlowv,		CVC,	BATLOWV),
	HL7015_SYSFS_FIELD_RW(vrechg,		CVC,	VRECHG),
	HL7015_SYSFS_FIELD_RW(en_term,		CTTC,	EN_TERM),
	HL7015_SYSFS_FIELD_RW(term_stat,	CTTC,	TERM_STAT),
	HL7015_SYSFS_FIELD_RO(watchdog,	CTTC,	WATCHDOG),
	HL7015_SYSFS_FIELD_RW(en_timer,	CTTC,	EN_TIMER),
	HL7015_SYSFS_FIELD_RW(chg_timer,	CTTC,	CHG_TIMER),
	HL7015_SYSFS_FIELD_RW(jeta_iset,	CTTC,	JEITA_ISET),
	HL7015_SYSFS_FIELD_RW(bat_comp,	ICTRC,	BAT_COMP),
	HL7015_SYSFS_FIELD_RW(vclamp,		ICTRC,	VCLAMP),
	HL7015_SYSFS_FIELD_RW(treg,		ICTRC,	TREG),
	HL7015_SYSFS_FIELD_RW(dpdm_en,		MOC,	DPDM_EN),
	HL7015_SYSFS_FIELD_RW(tmr2x_en,	MOC,	TMR2X_EN),
	HL7015_SYSFS_FIELD_RW(batfet_disable,	MOC,	BATFET_DISABLE),
	HL7015_SYSFS_FIELD_RW(jeita_vset,	MOC,	JEITA_VSET),
	HL7015_SYSFS_FIELD_RO(int_mask,	MOC,	INT_MASK),
	HL7015_SYSFS_FIELD_RO(vbus_stat,	SS,	VBUS_STAT),
	HL7015_SYSFS_FIELD_RO(chrg_stat,	SS,	CHRG_STAT),
	HL7015_SYSFS_FIELD_RO(dpm_stat,	SS,	DPM_STAT),
	HL7015_SYSFS_FIELD_RO(pg_stat,		SS,	PG_STAT),
	HL7015_SYSFS_FIELD_RO(therm_stat,	SS,	THERM_STAT),
	HL7015_SYSFS_FIELD_RO(vsys_stat,	SS,	VSYS_STAT),
	HL7015_SYSFS_FIELD_RO(watchdog_fault,	F,	WATCHDOG_FAULT),
	HL7015_SYSFS_FIELD_RO(boost_fault,	F,	BOOST_FAULT),
	HL7015_SYSFS_FIELD_RO(chrg_fault,	F,	CHRG_FAULT),
	HL7015_SYSFS_FIELD_RO(bat_fault,	F,	BAT_FAULT),
	HL7015_SYSFS_FIELD_RO(ntc_fault,	F,	NTC_FAULT),
	HL7015_SYSFS_FIELD_RO(pn,		VPRS,	PN),
	HL7015_SYSFS_FIELD_RO(ts_profile,	VPRS,	TS_PROFILE),
	HL7015_SYSFS_FIELD_RO(dev_reg,		VPRS,	DEV_REG),
};

static struct attribute *
	hl7015_sysfs_attrs[ARRAY_SIZE(hl7015_sysfs_field_tbl) + 1];

ATTRIBUTE_GROUPS(hl7015_sysfs);

static void hl7015_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(hl7015_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		hl7015_sysfs_attrs[i] = &hl7015_sysfs_field_tbl[i].attr.attr;

	hl7015_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static struct hl7015_sysfs_field_info *hl7015_sysfs_field_lookup(
		const char *name)
{
	int i, limit = ARRAY_SIZE(hl7015_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		if (!strcmp(name, hl7015_sysfs_field_tbl[i].attr.attr.name))
			break;

	if (i >= limit)
		return NULL;

	return &hl7015_sysfs_field_tbl[i];
}

static ssize_t hl7015_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct hl7015_dev_info *bdi = power_supply_get_drvdata(psy);
	struct hl7015_sysfs_field_info *info;
	ssize_t count;
	int ret;
	u8 v;

	info = hl7015_sysfs_field_lookup(attr->attr.name);
	if (!info)
		return -EINVAL;


	ret = hl7015_read_mask(bdi, info->reg, info->mask, info->shift, &v);
	if (ret)
		count = ret;
	else
		count = scnprintf(buf, PAGE_SIZE, "%hhx\n", v);



	return count;
}

static ssize_t hl7015_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct hl7015_dev_info *bdi = power_supply_get_drvdata(psy);
	struct hl7015_sysfs_field_info *info;
	int ret;
	u8 v;

	info = hl7015_sysfs_field_lookup(attr->attr.name);
	if (!info)
		return -EINVAL;

	ret = kstrtou8(buf, 0, &v);
	if (ret < 0)
		return ret;



	ret = hl7015_write_mask(bdi, info->reg, info->mask, info->shift, v);
	if (ret)
		count = ret;


	return count;
}
#endif

void hl7015_set_iinlim(unsigned int val)
{
	if(val > 7) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON0),
				(unsigned char)(val),
				(unsigned char)(CON0_IINLIM_MASK),
				(unsigned char)(CON0_IINLIM_SHIFT)
				);
	/*			
	val = val & 0x07;			
	val = (val|0x30) ;  
	hl7015_reg_config_interface(0x00,val);*/
}
/* CON0 */

static int hl7015_set_en_hiz(struct charger_device *chg_dev, bool en)
{
	if(!chg_dev)
		return -EINVAL;

	pr_err("[%s] en=%d\n", __func__,en);
	hl7015_set_i2cwatchdog(0x0);
//	hl7015_set_ichg(0x7);
	if (!en) {
		//hl7015_set_chg_config(0x1);   //edited by selena for I2C error 
		hl7015_reg_config_interface(0x01,0x1b);
	} else {
		hl7015_set_chg_config(0x0);
	}

#if 0
	hl7015_config_interface((unsigned char)(HL7015_CON0),
				(unsigned char)(en),
				(unsigned char)(CON0_EN_HIZ_MASK),
				(unsigned char)(CON0_EN_HIZ_SHIFT)
				);

	if(en)
		gpio_direction_output(chg_en_gpio,1);
	else
		gpio_direction_output(chg_en_gpio,0);
#endif
	return 0;
}
#if 0
static int hl7015_set_en_hiz_en(bool en)
{


	pr_err("[%s] en=%d\n", __func__,en);
	hl7015_set_i2cwatchdog(0x0);
//	hl7015_set_ichg(0x7);
	if (!en) {
		//hl7015_set_chg_config(0x1);   //edited by selena for I2C error 
		hl7015_reg_config_interface(0x01,0x1b);
	} else {
		hl7015_set_chg_config(0x0);
	}

#if 1
	hl7015_config_interface((unsigned char)(HL7015_CON0),
				(unsigned char)(en),
				(unsigned char)(CON0_EN_HIZ_MASK),
				(unsigned char)(CON0_EN_HIZ_SHIFT)
				);

	if(en)
		gpio_direction_output(chg_en_gpio,1);
	else
		gpio_direction_output(chg_en_gpio,0);
#endif
	return 0;
}
#endif
void hl7015_set_vindpm(unsigned int val)
{
	if(val > 15) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

/*	hl7015_config_interface((unsigned char)(HL7015_CON0),
				(unsigned char)(val),
				(unsigned char)(CON0_VINDPM_MASK),
				(unsigned char)(CON0_VINDPM_SHIFT)
				); */
}


/* CON1 */
void hl7015_set_register_reset(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_REG_RESET_MASK),
				(unsigned char)(CON1_REG_RESET_SHIFT)
				);
}

void hl7015_set_i2cwatchdog_timer_reset(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_I2C_WDT_RESET_MASK),
				(unsigned char)(CON1_I2C_WDT_RESET_SHIFT)
				);
}

void hl7015_set_chg_config(unsigned int val)
{
	if(val > 3) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_CHG_CONFIG_MASK),
				(unsigned char)(CON1_CHG_CONFIG_SHIFT)
				);
}

void hl7015_set_sys_min(unsigned int val)
{
	if(val > 7) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_SYS_MIN_MASK),
				(unsigned char)(CON1_SYS_MIN_SHIFT)
				);
}

void hl7015_set_boost_lim(unsigned int val)
{
	if(val > 7) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_BOOST_LIM_MASK),
				(unsigned char)(CON1_BOOST_LIM_SHIFT)
				);
}

/* CON2 */
void hl7015_set_ichg(unsigned int val)
{
	if(val > 62) { //hhl modify
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_ICHG_MASK),
				(unsigned char)(CON2_ICHG_SHIFT)
				);
}

void hl7015_set_bcold(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_BCLOD_MASK),
				(unsigned char)(CON2_BCLOD_SHIFT)
				);
}

void hl7015_set_force_20pct(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_FORCE_20PCT_MASK),
				(unsigned char)(CON2_FORCE_20PCT_SHIFT)
				);
}

/* CON3 */
void hl7015_set_iprechg(unsigned int val)
{
	if(val > 15) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON3),
				(unsigned char)(val),
				(unsigned char)(CON3_IPRECHG_MASK),
				(unsigned char)(CON3_IPRECHG_SHIFT)
				);
}

void hl7015_set_iterm(unsigned int val)
{
	if(val > 15) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON3),
				(unsigned char)(val),
				(unsigned char)(CON3_ITERM_MASK),
				(unsigned char)(CON3_ITERM_SHIFT)
				);
}

/* CON4 */
void hl7015_set_vreg(unsigned int val)
{
	if(val > 63) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_VREG_MASK),
				(unsigned char)(CON4_VREG_SHIFT)
				);
}

void hl7015_set_batlowv(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}
	
	hl7015_config_interface((unsigned char)(HL7015_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_BATLOWV_MASK),
				(unsigned char)(CON4_BATLOWV_SHIFT)
				);
}

void hl7015_set_vrechg(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_VRECHG_MASK),
				(unsigned char)(CON4_VRECHG_SHIFT)
				);
}

/* CON5 */
void hl7015_set_en_term(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_EN_TERM_MASK),
				(unsigned char)(CON5_EN_TERM_SHIFT)
				);
}

void hl7015_set_term_stat(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_TERM_STAT_MASK),
				(unsigned char)(CON5_TERM_STAT_SHIFT)
				);
}

void hl7015_set_i2cwatchdog(unsigned int val)
{
	if(val > 3) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_WATCHDOG_MASK),
				(unsigned char)(CON5_WATCHDOG_SHIFT)
				);
}

void hl7015_set_en_safty_timer(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_EN_SAFE_TIMER_MASK),
				(unsigned char)(CON5_EN_SAFE_TIMER_SHIFT)
				);
}

void hl7015_set_charge_timer(unsigned int val)
{
	if(val > 3) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_CHG_TIMER_MASK),
				(unsigned char)(CON5_CHG_TIMER_SHIFT)
				);
}

/* CON6 */
void hl7015_set_boostv(unsigned int val)
{
	if(val > 15) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_BOOSTV_MASK),
				(unsigned char)(CON6_BOOSTV_SHIFT)
				);
}

void hl7015_set_bhot(unsigned int val)
{
	if(val > 3) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_BHOT_MASK),
				(unsigned char)(CON6_BHOT_SHIFT)
				);
}

void hl7015_set_treg(unsigned int val)
{
	if(val > 3) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_TREG_MASK),
				(unsigned char)(CON6_TREG_SHIFT)
				);
}

/* CON7 */
void hl7015_set_dpdm_en(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON7),
				(unsigned char)(val),
				(unsigned char)(CON7_DPDM_EN_MASK),
				(unsigned char)(CON7_DPDM_EN_SHIFT)
				);
}

void hl7015_set_tmr2x_en(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON7),
				(unsigned char)(val),
				(unsigned char)(CON7_TMR2X_EN_MASK),
				(unsigned char)(CON7_TMR2X_EN_SHIFT)
				);
}

void hl7015_set_ppfet_disable(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON7),
				(unsigned char)(val),
				(unsigned char)(CON7_PPFET_DISABLE_MASK),
				(unsigned char)(CON7_PPFET_DISABLE_MASK)
				);
}

void hl7015_set_chrgfault_int_mask(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON7),
				(unsigned char)(val),
				(unsigned char)(CON7_CHRG_FAULT_INT_MASK_MASK),
				(unsigned char)(CON7_CHRG_FAULT_INT_MASK_SHIFT)
				);
}

void hl7015_set_batfault_int_mask(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CON7),
				(unsigned char)(val),
				(unsigned char)(CON7_BAT_FAULT_INT_MASK_MASK),
				(unsigned char)(CON7_BAT_FAULT_INT_MASK_SHIFT)
				);
}

/* CON8 */
static unsigned int hl7015_get_vin_status(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON8),
			&val,
			(unsigned char)(CON8_VIN_STAT_MASK),
			(unsigned char)(CON8_VIN_STAT_SHIFT)
			);

	return val;
}

static void hl7015_vin_status_dump(void)
{
	unsigned int vin_status;

	vin_status = hl7015_get_vin_status();

	switch(vin_status)
	{
		case 0:
			pr_err("[hl7015] no input or dpm detection incomplete.\n");
			break;
		case 1:
			pr_err("[hl7015] USB host inserted.\n");
			break;
		case 2:
			pr_err("[hl7015] Adapter inserted.\n");
			break;
		case 3:
			pr_err("[hl7015] OTG device inserted.\n");
			break;
		default:
			pr_err("[hl7015] wrong vin status.\n");
			break;
	}
}

static unsigned int hl7015_get_chrg_status(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON8),
			&val,
			(unsigned char)(CON8_CHRG_STAT_MASK),
			(unsigned char)(CON8_CHRG_STAT_SHIFT)
			);

	return val;
}

static void hl7015_chrg_status_dump(void)
{
	unsigned int chrg_status;

	chrg_status = hl7015_get_chrg_status();

	switch(chrg_status)
	{
		case 0:
			pr_err("[hl7015] not charging.\n");
			break;
		case 1:
			pr_err("[hl7015] precharging mode.\n");
			break;
		case 2:
			pr_err("[hl7015] fast charging mode.\n");
			break;
		case 3:
			pr_err("[hl7015] charge termination done.\n");
			break;
		default:
			pr_err("[hl7015] wrong charge status.\n");
			break;
	}
}

static unsigned int hl7015_get_dpm_status(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON8),
			&val,
			(unsigned char)(CON8_DPM_STAT_MASK),
			(unsigned char)(CON8_DPM_STAT_SHIFT)
			);

	return val;
}

static void hl7015_dpm_status_dump(void)
{
	unsigned int dpm_status;

	dpm_status = hl7015_get_dpm_status();

	if(0x0 == dpm_status)
		pr_err("[hl7015] not in dpm.\n");
	else
		pr_err("[hl7015] in vindpm or ilimdpm.\n");
}

static unsigned int hl7015_get_pg_status(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON8),
			&val,
			(unsigned char)(CON8_PG_STAT_MASK),
			(unsigned char)(CON8_PG_STAT_SHIFT)
			);

	return val;
}

static void hl7015_pg_status_dump(void)
{
	unsigned int pg_status;

	pg_status = hl7015_get_pg_status();

	if(0x0 == pg_status)
		pr_err("[hl7015] power is not good.\n");
	else
		pr_err("[hl7015] power is good.\n");
}

static unsigned int hl7015_get_therm_status(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON8),
			&val,
			(unsigned char)(CON8_THERM_STAT_MASK),
			(unsigned char)(CON8_THERM_STAT_SHIFT)
			);

	return val;
}

static void hl7015_therm_status_dump(void)
{
	unsigned int therm_status;

	therm_status = hl7015_get_therm_status();

	if(0x0 == therm_status)
		pr_err("[hl7015] ic's thermal status is in normal.\n");
	else
		pr_err("[hl7015] ic is in thermal regulation.\n");
}

static unsigned int hl7015_get_vsys_status(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON8),
			&val,
			(unsigned char)(CON8_VSYS_STAT_MASK),
			(unsigned char)(CON8_VSYS_STAT_SHIFT)
			);

	return val;
}

static void hl7015_vsys_status_dump(void)
{
	unsigned int vsys_status;

	vsys_status = hl7015_get_vsys_status();

	if(0x0 == vsys_status)
		pr_err("[hl7015] ic is not in vsysmin regulation(BAT > VSYSMIN).\n");
	else
		pr_err("[hl7015] ic is in vsysmin regulation(BAT < VSYSMIN).\n");
}

static void hl7015_charger_system_status(void)
{
	hl7015_vin_status_dump();
	hl7015_chrg_status_dump();
	hl7015_dpm_status_dump();
	hl7015_pg_status_dump();
	hl7015_therm_status_dump();
	hl7015_vsys_status_dump();
}

/* CON9 */
static unsigned int hl7015_get_watchdog_fault(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON9),
			&val,
			(unsigned char)(CON9_WATCHDOG_FAULT_MASK),
			(unsigned char)(CON9_WATCHDOG_FAULT_SHIFT)
			);

	return val;
}

static void hl7015_watchdog_fault_dump(void)
{
	unsigned int wtd_fault;

	wtd_fault = hl7015_get_watchdog_fault();

	if(0x0 == wtd_fault)
		pr_err("[hl7015] i2c watchdog is normal.\n");
	else
		pr_err("[hl7015] i2c watchdog timer is expirate.\n");
}

static unsigned int hl7015_get_otg_fault(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON9),
			&val,
			(unsigned char)(CON9_OTG_FAULT_MASK),
			(unsigned char)(CON9_OTG_FAULT_SHIFT)
			);

	return val;
}

static void hl7015_otg_fault_dump(void)
{
	unsigned int otg_fault;

	otg_fault = hl7015_get_otg_fault();

	if(0x0 == otg_fault)
		pr_err("[hl7015] the OTG function is fine.\n");
	else
		pr_err("[hl7015] the OTG function is error.\n");
}

static unsigned int hl7015_get_chrg_fault(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON9),
			&val,
			(unsigned char)(CON9_CHRG_FAULT_MASK),
			(unsigned char)(CON9_CHRG_FAULT_SHIFT)
			);

	return val;
}

static void hl7015_chrg_fault_dump(void)
{
	unsigned int chrg_fault;

	chrg_fault = hl7015_get_chrg_fault();
	switch(chrg_fault)
	{
		case 0:
			pr_err("[hl7015] the ic charging status is normal.\n");
			break;
		case 1:
			pr_err("[hl7015] the ic's input is fault.\n");
			break;
		case 2:
			pr_err("[hl7015] the ic's thermal is shutdown.\n");
			break;
		case 3:
			pr_err("[hl7015] the ic's charge safety timer is expirate.\n");
			break;
		default:
			pr_err("[hl7015] the ic's charge fault status is unkown.\n");
			break;
	}
}
#if 0
static unsigned int hl7015_get_bat_fault(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON9),
			&val,
			(unsigned char)(CON9_BAT_FAULT_MASK),
			(unsigned char)(CON9_BAT_FAULT_SHIFT)
			);

	return val;
}

static void hl7015_bat_fault_dump(void)
{
	unsigned int bat_fault;

	bat_fault = hl7015_get_bat_fault();

	if(0x0 == bat_fault)
		pr_err("[hl7015] battery is normal.\n");
	else
		pr_err("[hl7015] battery is in OVP.\n");
}
#endif
static unsigned int hl7015_get_ntc_fault(void)
{
	unsigned int ret;
	unsigned char val;

	ret = hl7015_read_interface((unsigned char)(HL7015_CON9),
			&val,
			(unsigned char)(CON9_NTC_FAULT_MASK),
			(unsigned char)(CON9_NTC_FAULT_SHIFT)
			);

	return val;
}

static void hl7015_ntc_fault_dump(void)
{
	unsigned int ntc_fault;

	ntc_fault = hl7015_get_ntc_fault();
	switch(ntc_fault)
	{
		case 0:
			pr_err("[hl7015] the ic's body temperature is normal.\n");
			break;
		case 5:
			pr_err("[hl7015] the ic's body temperature is cold.\n");
			break;
		case 6:
			pr_err("[hl7015] the ic's body temperature is hot.\n");
			break;
		default:
			pr_err("[hl7015] the ic's body temperature is unknow.\n");
			break;
	}
}

static void hl7015_charger_fault_status(void)
{
	hl7015_watchdog_fault_dump();
	hl7015_otg_fault_dump();
	hl7015_chrg_fault_dump();
	#if 0
	hl7015_bat_fault_dump();
	#endif
	hl7015_ntc_fault_dump();
}

/* CONA */
//vender info register

/* CONB */
void hl7015_set_tsr(unsigned int val)
{
	if(val > 3) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONB),
				(unsigned char)(val),
				(unsigned char)(CONB_TSR_MASK),
				(unsigned char)(CONB_TSR_SHIFT)
				);
}

void hl7015_set_tsrp(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONB),
				(unsigned char)(val),
				(unsigned char)(CONB_TRSP_MASK),
				(unsigned char)(CONB_TRSP_SHIFT)
				);
}

void hl7015_set_dis_connect(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONB),
				(unsigned char)(val),
				(unsigned char)(CONB_DIS_RECONNECT_MASK),
				(unsigned char)(CONB_DIS_RECONNECT_SHIFT)
				);
}

void hl7015_set_dis_sr_inchg(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONB),
				(unsigned char)(val),
				(unsigned char)(CONB_DIS_SR_INCHG_MASK),
				(unsigned char)(CONB_DIS_SR_INCHG_SHIFT)
				);
}

void hl7015_set_tship(unsigned int val)
{
	if(val > 7) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONB),
				(unsigned char)(val),
				(unsigned char)(CONB_TSHIP_MASK),
				(unsigned char)(CONB_TSHIP_SHIFT)
				);
}

/* CONC */
void hl7015_set_bat_comp(unsigned int val)
{
	if(val > 7) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONC),
				(unsigned char)(val),
				(unsigned char)(CONC_BAT_COMP_MASK),
				(unsigned char)(CONC_BAT_COMP_SHIFT)
				);
}

void hl7015_set_bat_vclamp(unsigned int val)
{
	if(val > 7) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONC),
				(unsigned char)(val),
				(unsigned char)(CONC_BAT_VCLAMP_MASK),
				(unsigned char)(CONC_BAT_VCLAMP_SHIFT)
				);
}

void hl7015_set_boost_9v_en(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_CONC),
				(unsigned char)(val),
				(unsigned char)(CONC_BOOST_9V_EN_MASK),
				(unsigned char)(CONC_BOOST_9V_EN_SHIFT)
				);
}

/* COND */
void hl7015_set_disable_ts(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_COND),
				(unsigned char)(val),
				(unsigned char)(COND_DISABLE_TS_MASK),
				(unsigned char)(COND_DISABLE_TS_SHIFT)
				);
}

void hl7015_set_vindpm_offset(unsigned int val)
{
	if(val > 1) {
		pr_err("[%s] parameter error.\n", __func__);
		return;
	}

	hl7015_config_interface((unsigned char)(HL7015_COND),
				(unsigned char)(val),
				(unsigned char)(COND_VINDPM_OFFSET_MASK),
				(unsigned char)(COND_VINDPM_OFFSET_SHIFT)
				);
}
static int hl7015_enable_charging(struct charger_device *chg_dev, bool en)
{
	unsigned int status = 0;

	if(!chg_dev)
		return -EINVAL;

	if (is_otg_mode)
	{
		pr_err("[hl7015]%s: OTG mode,do not setting charging\n", __func__);
		return status;
	}

	if (en) {
		//hl7015_set_chg_config(0x1);   //edited by selena for I2C error 
		hl7015_reg_config_interface(0x01,0x1b);
		
	} else {
		hl7015_set_chg_config(0x0);
	}

	return status;
}

static int hl7015_get_current(struct charger_device *chg_dev, u32 *ichg)
{
	int status = 0;
	unsigned int array_size;
	unsigned char reg_value;

	if(!chg_dev)
		return -EINVAL;

	array_size = ARRAY_SIZE(CSTH);
	hl7015_read_interface(HL7015_CON2, &reg_value, CON2_ICHG_MASK, CON2_ICHG_SHIFT);	/* charge current */
	*ichg = charging_value_to_parameter_hl7015(CSTH, array_size, reg_value);

	return status;
}

static int hl7015_check_force_ts1(void)
{
	u8 ts1, read_val, set_val;
	
	hl7015_read_byte(0xd, &read_val, 1);
	ts1 = !(read_val & 0x40);
	if (ts1){
		set_val = 0x50;
		hl7015_write_byte(0xd, &set_val, 2);
		hl7015_read_byte(0xd, &read_val, 1);
    	pr_err("ts1 endy,%s()-[0xd]=0x%x, ts1=%d\n",__func__,read_val, ts1);
	}else{
    	pr_err("!ts1 endy,%s()-[0xd]=0x%x, ts1=%d\n",__func__,read_val, ts1);
	}
	return 0;
}


static int hl7015_check_force_20pct(u8 value)
{
	u8 force_20pct, val;
	
	force_20pct = !!(value & CON2_FORCE_20PCT_MASK);
	if (force_20pct){
		hl7015_set_force_20pct(0);
		hl7015_read_byte(2, &val, 1);
    	pr_err("endy,%s()-[0x2]=0x%x, force_20pct=%d\n",__func__,val, force_20pct);
	}else{
    	pr_err("endy,%s()-[0x2]=0x%x, force_20pct=%d\n",__func__,value, force_20pct);
	}
	return 0;
}

static int hl7015_set_current(struct charger_device *chg_dev, u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned char register_value;
	u8 reg_val;
	struct hl7015_dev_info *bdi = charger_get_data(chg_dev);
    
    pr_err("endy,%s()++\n",__func__);
    
    pr_err("endy,%s()current_value = %d.\n",__func__,current_value);


	if(!chg_dev)
		return -EINVAL;

	if (is_otg_mode)
	{
		pr_err("[hl7015]%s: OTG mode,do not setting charging\n", __func__);
		return status;
	}

	pr_err("[hl7015] charge current setting value: %d.\n", current_value);
	if (current_value <= 500000) {
		//hl7015_set_ichg(0x0);    //edited by selena for I2C error 
		hl7015_reg_config_interface(0x02,0x60);
		
	} else {
		array_size = ARRAY_SIZE(CSTH);
		set_chr_current = bmt_find_closest_level(CSTH, array_size, current_value);
		set_chr_current = 4032000;

#ifdef EXT_PD_CHARGER
		if(bdi->pd_charger_enable){
			set_chr_current = 4352000;

			pr_err("[hl7015] pd connect set_chr_current=%d\n", set_chr_current);
		}
#endif
		pr_err("[hl7015] charge current finally setting value: %d.\n", set_chr_current);
		register_value = charging_parameter_to_value_hl7015(CSTH, array_size, set_chr_current);
		//hl7015_set_ichg(register_value);  //edited by selena for I2C error 
		register_value = register_value << 2;
		hl7015_reg_config_interface(0x02,register_value);
	}
	
	hl7015_read_byte(2, &reg_val, 1);
    pr_err("endy,%s()-[0x2]=0x%x- bdi->power_good=%d set_chr_current=%d\n",__func__,reg_val, bdi->power_good, set_chr_current);

	hl7015_check_force_20pct(reg_val);  //edited by selena for I2C error 

	return status;
}


static int hl7015_get_cv_voltage(struct charger_device *chg_dev, u32 *cv)
{
	int status = 0;
	unsigned int array_size;
	unsigned char reg_value;

	if(!chg_dev)
		return -EINVAL;

	array_size = ARRAY_SIZE(VBAT_CVTH);
	hl7015_read_interface(HL7015_CON4, &reg_value, CON4_VREG_MASK, CON4_VREG_SHIFT);
	*cv = charging_value_to_parameter_hl7015(VBAT_CVTH, array_size, reg_value);

	return status;
}

static int hl7015_set_cv_voltage(struct charger_device *chg_dev, u32 cv)
{
	int status = 0;
	unsigned short int array_size;
	unsigned int set_cv_voltage;
	unsigned char  register_value;

	if(!chg_dev)
		return -EINVAL;

	if (is_otg_mode)
	{
		pr_err("[hl7015]%s: OTG mode,do not setting charging\n", __func__);
		return status;
	}

	pr_err("[hl7015] charge voltage setting value: %d.\n", cv);
	/*static kal_int16 pre_register_value; */
	array_size = ARRAY_SIZE(VBAT_CVTH);
	/*pre_register_value = -1; */
	set_cv_voltage = bmt_find_closest_level(VBAT_CVTH, array_size, cv);

	register_value =
	charging_parameter_to_value_hl7015(VBAT_CVTH, array_size, set_cv_voltage);
	pr_err("[hl7015] charging_set_cv_voltage register_value=0x%x %d %d\n",
	 register_value, cv, set_cv_voltage);
	//hl7015_set_vreg(register_value);    //edited by selena for I2C error 
	register_value = (register_value << 2)|0x2;
	hl7015_reg_config_interface(0x04,register_value);

	return status;
}

static int hl7015_get_input_current(struct charger_device *chg_dev, u32 *aicr)
{
	unsigned int status = 0;
	unsigned int array_size;
	unsigned char register_value;
	
	if(!chg_dev)
		return -EINVAL;

	array_size = ARRAY_SIZE(INPUT_CSTH);
	hl7015_read_interface(HL7015_CON0, &register_value, CON0_IINLIM_MASK, CON0_IINLIM_SHIFT);
	*aicr = charging_parameter_to_value_hl7015(INPUT_CSTH, array_size, register_value);

	return status;
}

static int hl7015_set_input_current(struct charger_device *chg_dev, u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned char register_value;
	struct hl7015_dev_info *bdi = charger_get_data(chg_dev);
	
	pr_err("%s(),input current = %d.\n",__func__,current_value);

	if(!chg_dev)
		return -EINVAL;

	if (is_otg_mode)
	{
		pr_err("[hl7015]%s: OTG mode,do not setting charging\n", __func__);
		return status;
	}

#ifdef EXT_PD_CHARGER
	if(bdi->pd_charger_enable){
		current_value = bdi->fast_input_cur*1000;

		pr_err("[hl7015]%s: pd connect current_value=bdi->fast_input_cur==%d\n", __func__,bdi->fast_input_cur);
	}
#endif

	if (current_value < 100000) {
		register_value = 0x0;
	} else {
		array_size = ARRAY_SIZE(INPUT_CSTH);
		set_chr_current = bmt_find_closest_level(INPUT_CSTH, array_size, current_value);
		pr_err("[hl7015] charge input current finally setting value: %d.\n", set_chr_current);
		register_value = charging_parameter_to_value_hl7015(INPUT_CSTH, array_size, set_chr_current);
	}

	hl7015_set_iinlim(register_value);
    pr_err("endy,%s()-- bdi->power_good=%d current_value=%d\n",__func__,bdi->power_good,current_value);

	return status;
}

static int hl7015_get_termination_curr(struct charger_device *chg_dev, u32 *term_curr)
{
	unsigned int status = 0;
	unsigned int array_size;
	unsigned char register_value;

	if(!chg_dev)
		return -EINVAL;

	array_size = ARRAY_SIZE(ITERM_CSTH);
	hl7015_read_interface(HL7015_CON3, &register_value, CON3_ITERM_MASK, CON3_ITERM_SHIFT);
	*term_curr = charging_parameter_to_value_hl7015(ITERM_CSTH, array_size, register_value);

	return status;
}

static int hl7015_set_termination_curr(struct charger_device *chg_dev, u32 term_curr)
{
	unsigned int status = 0;
	unsigned int set_term_current;
	unsigned int array_size;
	unsigned int register_value = 0;

	if(!chg_dev)
		return -EINVAL;

	pr_err("[hl7015] charge termination current setting value: %d.\n", term_curr);
	if(term_curr < 100000) {
		hl7015_set_iterm(0x0);
	} else {
		array_size = ARRAY_SIZE(ITERM_CSTH);
		set_term_current = bmt_find_closest_level(ITERM_CSTH, array_size, term_curr);
		pr_err("[hl7015] charge termination current finally setting value: %d.\n", set_term_current);
		register_value = charging_parameter_to_value_hl7015(ITERM_CSTH, array_size, set_term_current);
	}

	hl7015_set_iterm(register_value);

	return status;
}

/* charger operation's functions */
static int hl7015_dump_register(struct charger_device *chg_dev)
{
	unsigned int status = 0;
	int i;
	unsigned char reg_val;

	if(!chg_dev)
		return -EINVAL;

	//pr_err("[hl7015] hl7015_dump info:\n");
	for(i = 0; i < HL7015_REG_NUM + 1; i++)
	{
		hl7015_read_byte(i, &reg_val, 1);
		pr_info("hl7015_dump: [0x%x] = 0x%x\n", i, reg_val);
	}
	hl7015_check_force_ts1();	

	return status;
}

static int hl7015_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	unsigned int status = 0;

	if(!chg_dev)
		return -EINVAL;

	hl7015_set_i2cwatchdog_timer_reset(0x1);
	/* charger status polling */
	hl7015_charger_system_status();
	hl7015_charger_fault_status();

	return status;
}

static int hl7015_get_vbus_adc(struct charger_device *chgdev, u32 *vbus)
{
    struct hl7015_dev_info *bdi = dev_get_drvdata(&chgdev->dev);

    if(bdi->power_good){
    	*vbus = 5000*1000;
    }else{
    	*vbus = 0;
    }

    return 0;
}

static int hl7015_do_event(struct charger_device *chg_dev, unsigned int event, unsigned int args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_err("[hl7015] %s: event = %d\n", __func__, event);

	switch (event) {
	//case EVENT_EOC:
	case EVENT_FULL:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}
//Antaiui <AI_BSP_CHG> <hehl> <2021-05-11> add reset ta begin
static int hl7015_reset_ta(struct charger_device *chg_dev)
{
	pr_err("%s\n", __func__);
	//hl7015_set_vindpm(1);
	hl7015_set_chg_config(0x0);
	msleep(300);
	//hl7015_set_vindpm(0);
	hl7015_set_chg_config(0x1);

	return 0;
}
//Antaiui <AI_BSP_CHG> <hehl> <2021-05-11> add reset ta end
static int hl7015_set_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
	unsigned int status = 0;

	if(!chg_dev)
		return -EINVAL;

	if(true == is_increase) {
		hl7015_set_iinlim(0x0); /* 100mA */
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_increase() on 1");
		msleep(85);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_increase() off 1");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_increase() on 2");
		msleep(85);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_increase() off 2");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_increase() on 3");
		msleep(281);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_increase() off 3");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_increase() on 4");
		msleep(281);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_increase() off 4");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_increase() on 5");
		msleep(281);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_increase() off 5");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_increase() on 6");
		msleep(485);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_increase() off 6");
		msleep(50);

		pr_err("[hl7015] mtk_ta_increase() end\n");

		hl7015_set_iinlim(0x2); /* 500mA */
		msleep(200);
	} else {
		hl7015_set_iinlim(0x0); /* 100mA */
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_decrease() on 1");
		msleep(281);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_decrease() off 1");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_decrease() on 2");
		msleep(281);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_decrease() off 2");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_decrease() on 3");
		msleep(281);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_decrease() off 3");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_decrease() on 4");
		msleep(85);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_decrease() off 4");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_decrease() on 5");
		msleep(85);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_decrease() off 5");
		msleep(85);

		hl7015_set_iinlim(0x2); /* 500mA */
		pr_err("[hl7015] mtk_ta_decrease() on 6");
		msleep(485);

		hl7015_set_iinlim(0x0); /* 100mA */
		pr_err("[hl7015] mtk_ta_decrease() off 6");
		msleep(50);

		pr_err("[hl7015] mtk_ta_decrease() end\n");

		hl7015_set_iinlim(0x2); /* 500mA */
	}

	return status;
}

static int hl7015_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	unsigned int status = 0;
	unsigned char register_value = 0;

	if(!chg_dev)
		return -EINVAL;

	hl7015_read_interface(HL7015_CON0, &register_value, CON0_VINDPM_MASK, CON0_VINDPM_SHIFT);
	
	if(0xf == register_value)
		*en = false;
	else if (0x7 == register_value)
		*en = true;

	return status;
}

static int hl7015_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	unsigned int status = 0;

	if(!chg_dev)
		return -EINVAL;
	pr_err("[hl7015] hl7015_enable_powerpath: %d.-------------\n", en);

	if(true == en)
		hl7015_set_vindpm(0x7); // 4.44V
	else
		hl7015_set_vindpm(0x7); // 5.08V

	return status;
}

static int hl7015_get_vindpm_voltage(struct charger_device *chg_dev, bool *in_loop)
{
	unsigned int status = 0; 
	unsigned char register_value = 0;

	if(!chg_dev)
		return -EINVAL;
	
	hl7015_read_interface(HL7015_CON8, &register_value, CON8_DPM_STAT_MASK, CON8_DPM_STAT_SHIFT);

	if(0 == register_value)
		*in_loop = true;
	else
		*in_loop = false;

	return status;
}

static int hl7015_set_vindpm_voltage(struct charger_device *chg_dev, u32 vindpm_vol)
{
	unsigned int status = 0;
	unsigned int array_size;
	unsigned int set_vindpm_vol;
	unsigned char register_value;
	unsigned char vindpm_status = 0;

	if(!chg_dev)
		return -EINVAL;
	
	hl7015_read_interface(HL7015_COND, &vindpm_status, COND_VINDPM_OFFSET_MASK, COND_VINDPM_OFFSET_SHIFT);

	if(1 == vindpm_status) {
		pr_err("[hl7015] vindpm voltage setting value: %d.-------------111\n", vindpm_vol);

		if(vindpm_vol < 8300000) {
			pr_err("[hl7015] enter high vindpm mode.--------111\n");
			hl7015_set_vindpm(0x0);
		} else {
			array_size = ARRAY_SIZE(VINDPM_HIGH_CVTH);
			set_vindpm_vol = bmt_find_closest_level(VINDPM_HIGH_CVTH, array_size, vindpm_vol);
			pr_err("[hl7015] vindpm voltage finally setting value: %d.-------------111\n", set_vindpm_vol);
			register_value = charging_parameter_to_value_hl7015(VINDPM_HIGH_CVTH, array_size, set_vindpm_vol);
			hl7015_set_vindpm(register_value);
		}	
	} else {
		pr_err("[hl7015] vindpm voltage setting value: %d.\n", vindpm_vol);

		if(vindpm_vol < 3500000) {
			pr_err("[hl7015] enter high vindpm mode.\n");
			hl7015_set_vindpm(0x0);
		} else {
			array_size = ARRAY_SIZE(VINDPM_NORMAL_CVTH);
			set_vindpm_vol = bmt_find_closest_level(VINDPM_NORMAL_CVTH, array_size, vindpm_vol);
			pr_err("[hl7015] vindpm voltage finally setting value: %d.\n", set_vindpm_vol);
			register_value = charging_parameter_to_value_hl7015(VINDPM_NORMAL_CVTH, array_size, set_vindpm_vol);
			hl7015_set_vindpm(register_value);
		}	
	}

	return status;
}

static int hl7015_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	unsigned int status = 0; 
	unsigned char register_value = 0;

	if(!chg_dev)
		return -EINVAL;
	
	hl7015_read_interface(HL7015_CON5, &register_value, CON5_CHG_TIMER_MASK, CON5_CHG_TIMER_SHIFT);

	if(1 == register_value)
		*en = true;
	else
		*en = false;

	return status;
}

static int hl7015_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	unsigned int status = 0; 

	if(!chg_dev)
		return -EINVAL;

	if(true == en)
		hl7015_set_en_safty_timer(0x1);
	else
		hl7015_set_en_safty_timer(0x0);

	return status;
}

static int yft_hl7015_dump_register(struct hl7015_dev_info *bdi);
static int hl7015_set_force_dpdm(struct hl7015_dev_info *bdi,int en);

int wtzn_hl7015_charger_enable_otg(bool en)
{
	printk("wtzn_hl7015_charger_enable_otg:%d\n",en);

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    if(gpio_is_valid(drvbus_gpio)){
        gpio_direction_output(drvbus_gpio, en);
    }
#endif

	if(en){
		hl7015_set_chg_config(0x2);
		hl7015_set_boostv(10); //set OTG output 5.2v
		//enable_boost_polling(true);
		#ifdef SET_OTG_CURRENT_TO_1A
		hl7015_set_boost_lim(0); //set OTG output 1A. 0:1A, 1:2.1A
		#endif
		is_otg_mode = 1;
		hl7015_set_i2cwatchdog(0x0);
	}else{
		hl7015_set_chg_config(0x1);
		//enable_boost_polling(false);
		is_otg_mode = 0;
		hl7015_set_i2cwatchdog(0x0);
	}
	return en;
}
EXPORT_SYMBOL(wtzn_hl7015_charger_enable_otg);
static int hl7015_charger_enable_otg(struct charger_device *chg_dev, bool en)
{
	unsigned int status = 0;

	if(!chg_dev)
		return -EINVAL;
	
	pr_err("[hl7015] enable otg %d.\n", en);
	
	if(true == en) {
		if (!hl7015_wake_lock->active) {
			__pm_stay_awake(hl7015_wake_lock);
		}
		hl7015_set_chg_config(0x2);
		hl7015_set_boostv(10); //set OTG output 5.2v
		//enable_boost_polling(true);
		#ifdef SET_OTG_CURRENT_TO_1A
		hl7015_set_boost_lim(0); //set OTG output 1A. 0:1A, 1:2.1A
		#endif
		is_otg_mode = 1;
		hl7015_set_i2cwatchdog(0x0);
	}else {
		if (hl7015_wake_lock->active) {
			__pm_relax(hl7015_wake_lock);
		}
		hl7015_set_chg_config(0x1);
		//enable_boost_polling(false);
		is_otg_mode = 0;
		hl7015_set_i2cwatchdog(0x0);
	}

	yft_hl7015_dump_register(g_bdi);
	return status;
}

static int hl7015_set_boost_current_limit(struct charger_device *chg_dev, u32 boost_curr)
{
	unsigned int status = 0;

	if(!chg_dev)
		return -EINVAL;

#ifdef SET_OTG_CURRENT_TO_1A
	hl7015_set_boost_lim(0x0);
#else
	if(boost_curr < 1000000)
		hl7015_set_boost_lim(0x0);
	else
		hl7015_set_boost_lim(0x1);
#endif

	return status;
}

static int hl7015_get_charging_status(struct charger_device *chg_dev, bool *is_done)
{
	unsigned int status = 0;
	unsigned int register_val;

	if(!chg_dev)
		return -EINVAL;

	register_val = hl7015_get_chrg_status();

	if (0x3 == register_val)
		*is_done = true;
	else
		*is_done = false;

	return status;
}

#ifdef CONFIG_REGULATOR
static int hl7015_set_charge_mode(struct regulator_dev *dev, u8 val)
{
	struct hl7015_dev_info *bdi = rdev_get_drvdata(dev);
	int ret;


	ret = hl7015_write_mask(bdi, HL7015_REG_POC,
				 HL7015_REG_POC_CHG_CONFIG_MASK,
				 HL7015_REG_POC_CHG_CONFIG_SHIFT, val);


	return ret;
}

static int hl7015_vbus_enable(struct regulator_dev *dev)
{
	return hl7015_set_charge_mode(dev, HL7015_REG_POC_CHG_CONFIG_OTG);
}

static int hl7015_vbus_disable(struct regulator_dev *dev)
{
	return hl7015_set_charge_mode(dev, HL7015_REG_POC_CHG_CONFIG_CHARGE);
}

static int hl7015_vbus_is_enabled(struct regulator_dev *dev)
{
	struct hl7015_dev_info *bdi = rdev_get_drvdata(dev);
	int ret;
	u8 val;


	ret = hl7015_read_mask(bdi, HL7015_REG_POC,
				HL7015_REG_POC_CHG_CONFIG_MASK,
				HL7015_REG_POC_CHG_CONFIG_SHIFT, &val);



	return ret ? ret : val == HL7015_REG_POC_CHG_CONFIG_OTG;
}

static const struct regulator_ops hl7015_vbus_ops = {
	.enable = hl7015_vbus_enable,
	.disable = hl7015_vbus_disable,
	.is_enabled = hl7015_vbus_is_enabled,
};

static const struct regulator_desc hl7015_vbus_desc = {
	.name = "usb_otg_vbus",
	.of_match = "usb-otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &hl7015_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static const struct regulator_init_data hl7015_vbus_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
};

static int hl7015_register_vbus_regulator(struct hl7015_dev_info *bdi)
{
	struct hl7015_platform_data *pdata = bdi->dev->platform_data;
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = bdi->dev;
	if (pdata && pdata->regulator_init_data)
		cfg.init_data = pdata->regulator_init_data;
	else
		cfg.init_data = &hl7015_vbus_init_data;
	cfg.driver_data = bdi;
	reg = devm_regulator_register(bdi->dev, &hl7015_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(bdi->dev, "Can't register regulator: %d\n", ret);
	}

	return ret;
}
#else
static int hl7015_register_vbus_regulator(struct hl7015_dev_info *bdi)
{
	return 0;
}
#endif

#if 0
static int hl7015_set_config(struct hl7015_dev_info *bdi)
{
	int ret;
	u8 v;
	printk("%s:carrot start \n", __func__);
	ret = hl7015_read(bdi, HL7015_REG_CTTC, &v);
	if (ret < 0)
		return ret;

	bdi->watchdog = ((v & HL7015_REG_CTTC_WATCHDOG_MASK) >>
					HL7015_REG_CTTC_WATCHDOG_SHIFT);

	/*
	 * According to the "Host Mode and default Mode" section of the
	 * manual, a write to any register causes the hl7015 to switch
	 * from default mode to host mode.  It will switch back to default
	 * mode after a WDT timeout unless the WDT is turned off as well.
	 * So, by simply turning off the WDT, we accomplish both with the
	 * same write.
	 */
	v &= ~HL7015_REG_CTTC_WATCHDOG_MASK;

	ret = hl7015_write(bdi, HL7015_REG_CTTC, v);
	if (ret < 0)
		return ret;

	if (bdi->sys_min) {
		v = bdi->sys_min / 100 - 30; // manual section 9.5.1.2, table 9
		ret = hl7015_write_mask(bdi, HL7015_REG_POC,
					 HL7015_REG_POC_SYS_MIN_MASK,
					 HL7015_REG_POC_SYS_MIN_SHIFT,
					 v);
		if (ret < 0)
			return ret;
	}

	if (bdi->iprechg) {
		v = bdi->iprechg / 128 - 1; // manual section 9.5.1.4, table 11
		ret = hl7015_write_mask(bdi, HL7015_REG_PCTCC,
					 HL7015_REG_PCTCC_IPRECHG_MASK,
					 HL7015_REG_PCTCC_IPRECHG_SHIFT,
					 v);
		if (ret < 0)
			return ret;
	}

	if (bdi->iterm) {
		v = bdi->iterm / 128 - 1; // manual section 9.5.1.4, table 11
		ret = hl7015_write_mask(bdi, HL7015_REG_PCTCC,
					 HL7015_REG_PCTCC_ITERM_MASK,
					 HL7015_REG_PCTCC_ITERM_SHIFT,
					 v);
		if (ret < 0)
			return ret;
	}
	printk("%s:carrot end \n", __func__);
	return 0;
}
#endif

static int hl7015_register_reset(struct hl7015_dev_info *bdi)
{
	int ret;
	//u8 v;
	//int limit = 100
	/*
	 * This prop. can be passed on device instantiation from platform code:
	 * struct property_entry pe[] =
	 *   { PROPERTY_ENTRY_BOOL("disable-reset"), ... };
	 * struct i2c_board_info bi =
	 *   { .type = "hl7015", .addr = 0x6b, .properties = pe, .irq = irq };
	 * struct i2c_adapter ad = { ... };
	 * i2c_add_adapter(&ad);
	 * i2c_new_client_device(&ad, &bi);
	 */

	printk("%s:carrot start \n", __func__);

	if (device_property_read_bool(bdi->dev, "disable-reset"))
		return 0;
	
	ret = hl7015_write_mask(bdi, HL7015_REG_ISC,
			HL7015_REG_ISC_EN_HIZ_MASK,
			HL7015_REG_ISC_EN_HIZ_SHIFT,
			0);
			
	/* Reset the registers */
	ret = hl7015_write_mask(bdi, HL7015_REG_POC,
			HL7015_REG_POC_RESET_MASK,
			HL7015_REG_POC_RESET_SHIFT,
			0x1);
	printk("%s:carrot ret=%d \n", __func__,ret);
	if (ret < 0)
		return ret;
#if 0 //carrot_drv fix
	/* Reset bit will be cleared by hardware so poll until it is */
	do {
		ret = hl7015_read_mask(bdi, HL7015_REG_POC,
				HL7015_REG_POC_RESET_MASK,
				HL7015_REG_POC_RESET_SHIFT,
				&v);
		if (ret < 0)
			return ret;

		if (v == 0)
			return 0;

		usleep_range(100, 200);
	} while (--limit);

	return -EIO;
#endif
	return 0;
}

/* Charger power supply property routines */

static int hl7015_charger_get_charge_type(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	u8 v;
	int type, ret;
	printk("%s:carrot start \n", __func__);
	ret = hl7015_read_mask(bdi, HL7015_REG_POC,
			HL7015_REG_POC_CHG_CONFIG_MASK,
			HL7015_REG_POC_CHG_CONFIG_SHIFT,
			&v);
	if (ret < 0)
		return ret;

	/* If POC[CHG_CONFIG] (REG01[5:4]) == 0, charge is disabled */
	if (!v) {
		type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	} else {
		ret = hl7015_read_mask(bdi, HL7015_REG_CCC,
				HL7015_REG_CCC_FORCE_20PCT_MASK,
				HL7015_REG_CCC_FORCE_20PCT_SHIFT,
				&v);
		if (ret < 0)
			return ret;

		type = (v) ? POWER_SUPPLY_CHARGE_TYPE_TRICKLE :
			     POWER_SUPPLY_CHARGE_TYPE_FAST;
	}
	printk("%s:carrot start ,val->intval = %d\n", __func__, type);

	val->intval = type;

	return 0;
}

static int hl7015_charger_set_charge_type(struct hl7015_dev_info *bdi,
		const union power_supply_propval *val)
{
	u8 chg_config, force_20pct, en_term;
	int ret;

	/*
	 * According to the "Termination when REG02[0] = 1" section of
	 * the hl7015 manual, the trickle charge could be less than the
	 * termination current so it recommends turning off the termination
	 * function.
	 *
	 * Note: AFAICT from the datasheet, the user will have to manually
	 * turn off the charging when in 20% mode.  If its not turned off,
	 * there could be battery damage.  So, use this mode at your own risk.
	 */
	switch (val->intval) {
	case POWER_SUPPLY_CHARGE_TYPE_NONE:
		chg_config = 0x0;
		break;
	case POWER_SUPPLY_CHARGE_TYPE_TRICKLE:
		chg_config = 0x1;
		force_20pct = 0x1;
		en_term = 0x0;
		break;
	case POWER_SUPPLY_CHARGE_TYPE_FAST:
		chg_config = 0x1;
		force_20pct = 0x0;
		en_term = 0x1;
		break;
	default:
		return -EINVAL;
	}

	if (chg_config) { /* Enabling the charger */
		ret = hl7015_write_mask(bdi, HL7015_REG_CCC,
				HL7015_REG_CCC_FORCE_20PCT_MASK,
				HL7015_REG_CCC_FORCE_20PCT_SHIFT,
				force_20pct);
		if (ret < 0)
			return ret;

		ret = hl7015_write_mask(bdi, HL7015_REG_CTTC,
				HL7015_REG_CTTC_EN_TERM_MASK,
				HL7015_REG_CTTC_EN_TERM_SHIFT,
				en_term);
		if (ret < 0)
			return ret;
	}

	return hl7015_write_mask(bdi, HL7015_REG_POC,
			HL7015_REG_POC_CHG_CONFIG_MASK,
			HL7015_REG_POC_CHG_CONFIG_SHIFT, chg_config);
}

static int hl7015_charger_get_health(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	u8 v;
	int health;
	printk("%s:carrot start \n", __func__);
	mutex_lock(&bdi->f_reg_lock);
	v = bdi->f_reg;
	mutex_unlock(&bdi->f_reg_lock);

	if (v & HL7015_REG_F_NTC_FAULT_MASK) {
		switch (v >> HL7015_REG_F_NTC_FAULT_SHIFT & 0x7) {
		case 0x1: /* TS1  Cold */
		case 0x3: /* TS2  Cold */
		case 0x5: /* Both Cold */
			health = POWER_SUPPLY_HEALTH_COLD;
			break;
		case 0x2: /* TS1  Hot */
		case 0x4: /* TS2  Hot */
		case 0x6: /* Both Hot */
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		default:
			health = POWER_SUPPLY_HEALTH_UNKNOWN;
		}
	} else if (v & HL7015_REG_F_BAT_FAULT_MASK) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (v & HL7015_REG_F_CHRG_FAULT_MASK) {
		switch (v >> HL7015_REG_F_CHRG_FAULT_SHIFT & 0x3) {
		case 0x1: /* Input Fault (VBUS OVP or VBAT<VBUS<3.8V) */
			/*
			 * This could be over-voltage or under-voltage
			 * and there's no way to tell which.  Instead
			 * of looking foolish and returning 'OVERVOLTAGE'
			 * when its really under-voltage, just return
			 * 'UNSPEC_FAILURE'.
			 */
			health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			break;
		case 0x2: /* Thermal Shutdown */
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case 0x3: /* Charge Safety Timer Expiration */
			health = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
			break;
		default:  /* prevent compiler warning */
			health = -1;
		}
	} else if (v & HL7015_REG_F_BOOST_FAULT_MASK) {
		/*
		 * This could be over-current or over-voltage but there's
		 * no way to tell which.  Return 'OVERVOLTAGE' since there
		 * isn't an 'OVERCURRENT' value defined that we can return
		 * even if it was over-current.
		 */
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else {
		health = POWER_SUPPLY_HEALTH_GOOD;
	}

	val->intval = health;

	return 0;
}
#if 0
static int hl7015_charger_get_online(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	u8 pg_stat, batfet_disable;
	int ret;

	ret = hl7015_read_mask(bdi, HL7015_REG_SS,
			HL7015_REG_SS_PG_STAT_MASK,
			HL7015_REG_SS_PG_STAT_SHIFT, &pg_stat);
	if (ret < 0)
		return ret;

	ret = hl7015_read_mask(bdi, HL7015_REG_MOC,
			HL7015_REG_MOC_BATFET_DISABLE_MASK,
			HL7015_REG_MOC_BATFET_DISABLE_SHIFT, &batfet_disable);
	if (ret < 0)
		return ret;

	val->intval = pg_stat && !batfet_disable;

	return 0;
}
#endif

#if 0
static int hl7015_battery_set_online(struct hl7015_dev_info *bdi,
				      const union power_supply_propval *val);
static int hl7015_battery_get_status(struct hl7015_dev_info *bdi,
				      union power_supply_propval *val);
static int hl7015_battery_get_temp_alert_max(struct hl7015_dev_info *bdi,
					      union power_supply_propval *val);
static int hl7015_battery_set_temp_alert_max(struct hl7015_dev_info *bdi,
					      const union power_supply_propval *val);

static int hl7015_charger_set_online(struct hl7015_dev_info *bdi,
				      const union power_supply_propval *val)
{
	return hl7015_battery_set_online(bdi, val);
}

static int hl7015_charger_get_status(struct hl7015_dev_info *bdi,
				      union power_supply_propval *val)
{
	return hl7015_battery_get_status(bdi, val);
}

static int hl7015_charger_get_temp_alert_max(struct hl7015_dev_info *bdi,
					      union power_supply_propval *val)
{
	return hl7015_battery_get_temp_alert_max(bdi, val);
}

static int hl7015_charger_set_temp_alert_max(struct hl7015_dev_info *bdi,
					      const union power_supply_propval *val)
{
	return hl7015_battery_set_temp_alert_max(bdi, val);
}
#endif
static int hl7015_charger_get_precharge(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	u8 v;
	int ret;

	ret = hl7015_read_mask(bdi, HL7015_REG_PCTCC,
			HL7015_REG_PCTCC_IPRECHG_MASK,
			HL7015_REG_PCTCC_IPRECHG_SHIFT, &v);
	if (ret < 0)
		return ret;

	val->intval = ++v * 128 * 1000;
	return 0;
}

static int hl7015_charger_get_charge_term(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	u8 v;
	int ret;

	ret = hl7015_read_mask(bdi, HL7015_REG_PCTCC,
			HL7015_REG_PCTCC_ITERM_MASK,
			HL7015_REG_PCTCC_ITERM_SHIFT, &v);
	if (ret < 0)
		return ret;

	val->intval = ++v * 128 * 1000;
	return 0;
}

static int hl7015_charger_get_current(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	u8 v;
	int curr, ret;

	ret = hl7015_get_field_val(bdi, HL7015_REG_CCC,
			HL7015_REG_CCC_ICHG_MASK, HL7015_REG_CCC_ICHG_SHIFT,
			CSTH,
			ARRAY_SIZE(CSTH), &curr);
	if (ret < 0)
		return ret;

	ret = hl7015_read_mask(bdi, HL7015_REG_CCC,
			HL7015_REG_CCC_FORCE_20PCT_MASK,
			HL7015_REG_CCC_FORCE_20PCT_SHIFT, &v);
	if (ret < 0)
		return ret;

	/* If FORCE_20PCT is enabled, then current is 20% of ICHG value */
	if (v)
		curr /= 5;

	val->intval = curr;
	return 0;
}

static int hl7015_charger_get_current_max(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	int idx = ARRAY_SIZE(CSTH) - 1;

	val->intval = CSTH[idx];
	return 0;
}

static int hl7015_charger_set_current(struct hl7015_dev_info *bdi,
		const union power_supply_propval *val)
{
	u8 v;
	int ret, curr = val->intval;

	ret = hl7015_read_mask(bdi, HL7015_REG_CCC,
			HL7015_REG_CCC_FORCE_20PCT_MASK,
			HL7015_REG_CCC_FORCE_20PCT_SHIFT, &v);
	if (ret < 0)
		return ret;

	/* If FORCE_20PCT is enabled, have to multiply value passed in by 5 */
	if (v)
		curr *= 5;

	return hl7015_set_field_val(bdi, HL7015_REG_CCC,
			HL7015_REG_CCC_ICHG_MASK, HL7015_REG_CCC_ICHG_SHIFT,
			CSTH,
			ARRAY_SIZE(CSTH), curr);
}

static int hl7015_charger_get_voltage(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	int voltage, ret;

	ret = hl7015_get_field_val(bdi, HL7015_REG_CVC,
			HL7015_REG_CVC_VREG_MASK, HL7015_REG_CVC_VREG_SHIFT,
			VBAT_CVTH,
			ARRAY_SIZE(VBAT_CVTH), &voltage);
	if (ret < 0)
		return ret;

	val->intval = voltage;
	return 0;
}

static int hl7015_charger_get_voltage_max(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	int idx = ARRAY_SIZE(VBAT_CVTH) - 1;

	val->intval = VBAT_CVTH[idx];
	return 0;
}

static int hl7015_charger_set_voltage(struct hl7015_dev_info *bdi,
		const union power_supply_propval *val)
{
	return hl7015_set_field_val(bdi, HL7015_REG_CVC,
			HL7015_REG_CVC_VREG_MASK, HL7015_REG_CVC_VREG_SHIFT,
			VBAT_CVTH,
			ARRAY_SIZE(VBAT_CVTH), val->intval);
}

static int hl7015_charger_get_iinlimit(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	int iinlimit, ret;

	ret = hl7015_get_field_val(bdi, HL7015_REG_ISC,
			HL7015_REG_ISC_IINLIM_MASK,
			HL7015_REG_ISC_IINLIM_SHIFT,
			INPUT_CSTH,
			ARRAY_SIZE(INPUT_CSTH), &iinlimit);
	if (ret < 0)
		return ret;

	val->intval = iinlimit;
	return 0;
}

#if 0//zzt
static int hl7015_charger_get_usb_type(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	int usb_type, ret;
	u8 ss_reg;

	ret = hl7015_read(bdi, HL7015_REG_SS, &ss_reg);
	if (ret < 0) {
		dev_err(bdi->dev, "Can't read SS reg: %d\n", ret);
		return ret;
	}

	if(ss_reg & 0x40)
		usb_type = POWER_SUPPLY_USB_TYPE_SDP;	//USB
	else if(ss_reg & 0x80)
		usb_type = POWER_SUPPLY_USB_TYPE_DCP;	//AC
	else
		usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	printk("%s:carrot start addr[0x%x]=0x%x, usb_type=%d \n", __func__,HL7015_REG_SS, ss_reg, usb_type);
	val->intval = usb_type;
	return 0;
}

static int hl7015_charger_get_chg_type(struct hl7015_dev_info *bdi,
		union power_supply_propval *val)
{
	int chg_type, ret;
	u8 ss_reg;

	ret = hl7015_read(bdi, HL7015_REG_SS, &ss_reg);
	if (ret < 0) {
		dev_err(bdi->dev, "Can't read SS reg: %d\n", ret);
		return ret;
	}

	if(ss_reg & 0x40)
		chg_type = POWER_SUPPLY_TYPE_USB;	//USB
	else if(ss_reg & 0x80)
		chg_type = POWER_SUPPLY_TYPE_USB_DCP;	//AC
	else
		chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
	
	
	printk("%s:carrot start addr[0x%x]=0x%x, chg_type=%d \n", __func__,HL7015_REG_SS, ss_reg, chg_type);
	val->intval = chg_type;
	return 0;
}
#endif

static int hl7015_get_charger_type(struct hl7015_dev_info *bdi)
{
	int ret;
	u8 ss_reg;
	int vbus_stat = 0;

	ret = hl7015_read(bdi, HL7015_REG_SS, &ss_reg);
	if (ret < 0) {
		dev_err(bdi->dev, "Can't read SS reg: %d\n", ret);
		return 0;
	}

	vbus_stat = (ss_reg & CON8_VBUS_STAT_MASK);
	vbus_stat >>= CON8_VBUS_STAT_SHIFT;

	dev_err(bdi->dev, "hl7015_get_charger_type vbus_stat=%d\n", vbus_stat);

	switch (vbus_stat) {
		case HL7015_VBUS_TYPE_UNKNOWN:
			bdi->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			bdi->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case HL7015_VBUS_TYPE_SDP:
			bdi->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;//USB
			bdi->psy_desc.type = POWER_SUPPLY_TYPE_USB;//USB
			break;
		case HL7015_VBUS_TYPE_DCP:
			bdi->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;//AC
			bdi->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;//AC
			break;
		default:
			bdi->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			bdi->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
	}

	return 0;
}

static int hl7015_charger_set_iinlimit(struct hl7015_dev_info *bdi,
		const union power_supply_propval *val)
{
	return hl7015_set_field_val(bdi, HL7015_REG_ISC,
			HL7015_REG_ISC_IINLIM_MASK,
			HL7015_REG_ISC_IINLIM_SHIFT,
			INPUT_CSTH,
			ARRAY_SIZE(INPUT_CSTH), val->intval);
}

static int hl7015_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct hl7015_dev_info *bdi = power_supply_get_drvdata(psy);
	int ret;

    //printk("hl7015_charger_get_property psp=%d\n", psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = hl7015_charger_get_charge_type(bdi, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = hl7015_charger_get_health(bdi, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		//ret = hl7015_charger_get_online(bdi, val);
		val->intval = bdi->power_good;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
	#if 0
		ret = hl7015_charger_get_status(bdi, val);
	#endif
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		ret = hl7015_charger_get_precharge(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = hl7015_charger_get_charge_term(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = hl7015_charger_get_current(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = hl7015_charger_get_current_max(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = hl7015_charger_get_voltage(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = hl7015_charger_get_voltage_max(bdi, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = hl7015_charger_get_iinlimit(bdi, val);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bdi->model_name;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = HL7015_MANUFACTURER;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		//dump_stack();
		val->intval = bdi->psy_usb_type;
		//ret = hl7015_charger_get_usb_type(bdi, val);
		//val->intval = bdi->psy_usb_type;
		//printk("%s:carrot prop: %d, POWER_SUPPLY_PROP_USB_TYPE:%d, bdi->psy_usb_type=%d, val=%d\n", __func__, psp, POWER_SUPPLY_PROP_USB_TYPE, bdi->psy_usb_type,*val);
		//power_supply_set_property(bdi->charger,POWER_SUPPLY_PROP_USB_TYPE, val);
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = bdi->psy_desc.type;
		//ret = hl7015_charger_get_chg_type(bdi, val);
		//printk("%s:carrot POWER_SUPPLY_PROP_TYPE:  bdi->psy_desc.type=%d,val->intval=%d\n", __func__,  bdi->psy_desc.type, val->intval);
    	//power_supply_set_property(bdi->charger,POWER_SUPPLY_PROP_TYPE, val);
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 1000000;
		ret = 0;
		break;

	default:
		ret = -ENODATA;
	}


	return ret;
}

static int hl7015_charger_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct hl7015_dev_info *bdi = power_supply_get_drvdata(psy);
	int ret;

//	printk("%s:carrot prop: %d\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	    #if 0
		ret = hl7015_charger_set_online(bdi, val);
		#endif
		bdi->power_good = val->intval;
		printk("%s:carrot  POWER_SUPPLY_PROP_ONLINE , bdi->power_good=%d, val->intval=%d\n", __func__,  bdi->power_good, val->intval);
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = hl7015_charger_set_charge_type(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = hl7015_charger_set_current(bdi, val);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = hl7015_charger_set_voltage(bdi, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = hl7015_charger_set_iinlimit(bdi, val);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		bdi->psy_desc.type = val->intval;
		hl7015_charger_desc.type = val->intval;
	//	printk("%s:carrot  POWER_SUPPLY_PROP_TYPE , bdi->psy_desc.type=%d, val->intval=%d, bdi->charger->type=%d\n", __func__,  bdi->psy_desc.type, val->intval, hl7015_charger_desc.type);
		ret = 0;
		break;
	default:
		ret = -EINVAL;
	}



	return ret;
}

static int hl7015_charger_property_is_writeable(struct power_supply *psy,
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
}
#if 0
static void hl7015_input_current_limit_work(struct work_struct *work)
{
	struct hl7015_dev_info *bdi =
		container_of(work, struct hl7015_dev_info,
			     input_current_limit_work.work);

	power_supply_set_input_current_limit_from_supplier(bdi->charger);
}
#endif

/* Sync the input-current-limit with our parent supply (if we have one) */
#if 0
static void hl7015_charger_external_power_changed(struct power_supply *psy)
{
	struct hl7015_dev_info *bdi = power_supply_get_drvdata(psy);

	/*
	 * The Power-Good detection may take up to 220ms, sometimes
	 * the external charger detection is quicker, and the hl7015 will
	 * reset to iinlim based on its own charger detection (which is not
	 * hooked up when using external charger detection) resulting in a
	 * too low default 500mA iinlim. Delay setting the input-current-limit
	 * for 300ms to avoid this.
	 */
	queue_delayed_work(system_wq, &bdi->input_current_limit_work,
			   msecs_to_jiffies(300));
}
#endif
static enum power_supply_property hl7015_charger_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
#if 1
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_MODEL_NAME,
#endif
};

static enum power_supply_usb_type hl7015_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};

static char *hl7015_charger_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

struct power_supply_desc hl7015_charger_desc = {
	.name			         = "hl7015-charger",
	.type			         = POWER_SUPPLY_TYPE_USB,
	.usb_types               = hl7015_chg_psy_usb_types,
	.num_usb_types           = ARRAY_SIZE(hl7015_chg_psy_usb_types),
	.properties		         = hl7015_charger_properties,
	.num_properties		    = ARRAY_SIZE(hl7015_charger_properties),
	.get_property		    = hl7015_charger_get_property,
	.set_property		    = hl7015_charger_set_property,
	.property_is_writeable	= hl7015_charger_property_is_writeable,
};

static int hl7015_configure_usb_otg(struct hl7015_dev_info *bdi, u8 ss_reg)
{
	bool otg_enabled;
	int ret;

	otg_enabled = !!(ss_reg & HL7015_REG_SS_VBUS_STAT_MASK);
	ret = extcon_set_state_sync(bdi->edev, EXTCON_USB, otg_enabled);
	if (ret < 0)
		dev_err(bdi->dev, "Can't set extcon state to %d: %d\n",
			otg_enabled, ret);

	return ret;
}

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

int hl7015_set_force_dpdm_to_detect(int en)
{
	int  ret;

	hl7015_config_interface((unsigned char)(HL7015_CON0),
				(unsigned char)(0),
				(unsigned char)(CON0_EN_HIZ_MASK),
				(unsigned char)(CON0_EN_HIZ_SHIFT)
				);

	ret = hl7015_set_force_dpdm(g_bdi, 1);
	if (ret < 0)
		return ret;
	mdelay(5);
	ret = hl7015_set_force_dpdm(g_bdi, 0);
	if (ret < 0)
		return ret;
	mdelay(5);

	hl7015_set_i2cwatchdog(0x0);
	mdelay(150);

	return 0;
}
EXPORT_SYMBOL_GPL(hl7015_set_force_dpdm_to_detect);


static void hl7015_inserted_irq(struct hl7015_dev_info *bdi)
{
    hl7015_set_force_dpdm_to_detect(true);
    hl7015_get_charger_type(bdi);
    power_supply_changed(bdi->charger);
#if EXT_PD_CHARGER
    schedule_delayed_work(&bdi->pd_enable_work, msecs_to_jiffies(250));
#endif
}

static void hl7015_removed_irq(struct hl7015_dev_info *bdi)
{
    msleep(100);
    hl7015_get_charger_type(bdi);
    power_supply_changed(bdi->charger);
#if EXT_PD_CHARGER
    schedule_delayed_work(&bdi->pd_enable_work, msecs_to_jiffies(50));
#endif
}

static void hl7015_read_byte_work(struct work_struct *work)
{
	const u8 battery_mask_ss = HL7015_REG_SS_CHRG_STAT_MASK;
	const u8 battery_mask_f = HL7015_REG_F_BAT_FAULT_MASK
				| HL7015_REG_F_NTC_FAULT_MASK;

	struct hl7015_dev_info *bdi = container_of(work, struct hl7015_dev_info , read_byte_work.work);

    bool alert_charger = false, alert_battery = false;
	u8 ss_reg = 0, f_reg = 0;
	int i, ret;

	ret = hl7015_read(bdi, HL7015_REG_SS, &ss_reg);
	printk("%s:carrot start addr[0x%x]=0x%x \n", __func__,HL7015_REG_SS, ss_reg);
	if (ret < 0) {
 		return;
	}

	bdi->power_good = !!(ss_reg & HL7015_REG_SS_PG_STAT_MASK);
	hl7015_set_i2cwatchdog(0x0);
	msleep(50);
    if(bdi->power_good){
    	hl7015_inserted_irq(bdi);
    }else{
    	hl7015_removed_irq(bdi);
    }

    printk("%s:inserted addr[0x%x]=0x%x psy_usb_type=%d ,bdi->psy_desc.type=%d,power_good=%d \n", __func__,HL7015_REG_SS, ss_reg, bdi->psy_usb_type, bdi->psy_desc.type, bdi->power_good);

	i = 0;
	do {
		ret = hl7015_read(bdi, HL7015_REG_F, &f_reg);
		if (ret < 0) {
			dev_err(bdi->dev, "Can't read F reg: %d\n", ret);
			return;
		}
	} while (f_reg && ++i < 2);

	/* ignore over/under voltage fault after disconnect */
	if (f_reg == (1 << HL7015_REG_F_CHRG_FAULT_SHIFT) &&
	    !(ss_reg & HL7015_REG_SS_PG_STAT_MASK))
		f_reg = 0;

	if (f_reg != bdi->f_reg) {
		dev_warn(bdi->dev,
			"Fault: boost %d, charge %d, battery %d, ntc %d\n",
			!!(f_reg & HL7015_REG_F_BOOST_FAULT_MASK),
			!!(f_reg & HL7015_REG_F_CHRG_FAULT_MASK),
			!!(f_reg & HL7015_REG_F_BAT_FAULT_MASK),
			!!(f_reg & HL7015_REG_F_NTC_FAULT_MASK));

		mutex_lock(&bdi->f_reg_lock);
		if ((bdi->f_reg & battery_mask_f) != (f_reg & battery_mask_f))
			alert_battery = true;
		if ((bdi->f_reg & ~battery_mask_f) != (f_reg & ~battery_mask_f))
			alert_charger = true;
		bdi->f_reg = f_reg;
		mutex_unlock(&bdi->f_reg_lock);
	}

	if (ss_reg != bdi->ss_reg) {
		/*
		 * The device is in host mode so when PG_STAT goes from 1->0
		 * (i.e., power removed) HIZ needs to be disabled.
		 */
		if ((bdi->ss_reg & HL7015_REG_SS_PG_STAT_MASK) &&
				!(ss_reg & HL7015_REG_SS_PG_STAT_MASK)) {
			ret = hl7015_write_mask(bdi, HL7015_REG_ISC,
					HL7015_REG_ISC_EN_HIZ_MASK,
					HL7015_REG_ISC_EN_HIZ_SHIFT,
					0);
			if (ret < 0)
				dev_err(bdi->dev, "Can't access ISC reg: %d\n",
					ret);
		}

		if ((bdi->ss_reg & battery_mask_ss) != (ss_reg & battery_mask_ss))
			alert_battery = true;
		if ((bdi->ss_reg & ~battery_mask_ss) != (ss_reg & ~battery_mask_ss))
			alert_charger = true;
		bdi->ss_reg = ss_reg;
	}

	printk("%s:carrot ss_reg: 0x%02x, f_reg: 0x%02x\n",__func__, ss_reg, f_reg);
}

static int yft_hl7015_dump_register(struct hl7015_dev_info *bdi)
{

	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned char hl7015_reg[14] = { 0 }; 
		
	for (i = 0; i < (10+4); i++) {
		ret = hl7015_read(bdi, i, &hl7015_reg[i]);
		if (ret < 0) {
			pr_info("[hl7015] i2c transfor error,ret=%d\n",ret);
			return ret;
		}
		pr_info("%s,[0x%x]=0x%x ",__func__, i, hl7015_reg[i]);
	}
	
	return 0;
}

static irqreturn_t hl7015_irq_handler_thread(int irq, void *data)
{
	struct hl7015_dev_info *bdi = data;
	u8 ss_reg = 0;
	int ret;
    printk("%s-------->begin\n", __func__);

    ret = hl7015_read(bdi, HL7015_REG_SS, &ss_reg);
	printk("%s:carrot start addr[0x%x]=0x%x \n", __func__,HL7015_REG_SS, ss_reg);
	if (ret < 0) {
 		return IRQ_HANDLED;
	}

	bdi->power_good = !!(ss_reg & HL7015_REG_SS_PG_STAT_MASK);
	hl7015_set_i2cwatchdog(0x0);
	msleep(50);
    if(bdi->power_good){
    	printk("%s: hl7015 adapter/usb inserted");
    	hl7015_inserted_irq(bdi);
    }else{
    	printk("%s: hl7015 adapter/usb remove");
    	hl7015_removed_irq(bdi);
    }

    printk("%s:inserted addr[0x%x]=0x%x psy_usb_type=%d ,bdi->psy_desc.type=%d,power_good=%d \n", __func__,HL7015_REG_SS, ss_reg, bdi->psy_usb_type, bdi->psy_desc.type, bdi->power_good);
	printk("%s-------->end\n", __func__);
	return IRQ_HANDLED;
}

static int hl7015_set_force_dpdm(struct hl7015_dev_info *bdi,int en)
{
	int ret = 0;
	/* force_dpdm */
	ret = hl7015_write_mask(bdi, 0x07,	HL7015_REG_POC_RESET_MASK,	7, 0);

	if (ret < 0)
		dev_err(bdi->dev, "hl7015_set_force_dpdm write faile %d\n",	ret);
	printk("%s:carrot ret=%d, en=%d \n", __func__,ret, en);
	return ret;
}

#if 0
static int hl7015_hw_init(struct hl7015_dev_info *bdi)
{
	u8 v;
	int ret;
	printk("%s:carrot start \n", __func__);
	/* First check that the device really is what its supposed to be */
	ret = hl7015_read_mask(bdi, HL7015_REG_VPRS,
			HL7015_REG_VPRS_PN_MASK,
			HL7015_REG_VPRS_PN_SHIFT,
			&v);
	if (ret < 0)
		return ret;

	switch (v) {
	case HL7015_REG_VPRS_PN_24190:
	case HL7015_REG_VPRS_PN_24192:
	case HL7015_REG_VPRS_PN_24192I:
		break;
	default:
		dev_err(bdi->dev, "Error unknown model: 0x%02x\n", v);
		return -ENODEV;
	}

	ret = hl7015_register_reset(bdi);
	if (ret < 0)
		return ret;

	ret = hl7015_set_config(bdi);
	if (ret < 0)
		return ret;

#if 0
	ret = hl7015_set_force_dpdm(bdi,1);
	if (ret < 0)
		return ret;
#endif

	ret =  hl7015_set_en_hiz_en(0);
	return hl7015_read(bdi, HL7015_REG_SS, &bdi->ss_reg);
}

#endif

static int hl7015_parse_dt(struct hl7015_dev_info *bdi)
{
	int ret;	
	int irq_gpio = 0, irqn = 0;	

	pr_info("[%s] 222  \n", __func__);	
	irq_gpio = of_get_named_gpio(bdi->dev->of_node, "irq-gpio", 0);
	if (!gpio_is_valid(irq_gpio))
	{
		dev_err(bdi->dev, "%s: %d gpio irq-gpio get failed\n", __func__, irq_gpio);
		return -EINVAL;
	}
	ret = gpio_request(irq_gpio, "hl7015 irq pin");
	if (ret) {
		dev_err(bdi->dev, "%s: %d gpio request failed\n", __func__, irq_gpio);
		return ret;
	}
	gpio_direction_input(irq_gpio);
	irqn = gpio_to_irq(irq_gpio);
	if (irqn < 0) {
		dev_err(bdi->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
		return irqn;
	}
	bdi->client->irq = irqn;
	pr_info("[%s] irq = %d  \n", __func__, bdi->client->irq);	

	chg_en_gpio = of_get_named_gpio(bdi->dev->of_node, "chg-en-gpio", 0);
	if (!gpio_is_valid(chg_en_gpio))
	{
		dev_err(bdi->dev, "%s: %d gpio chg-en-gpio get failed\n", __func__, chg_en_gpio);
		return -EINVAL;
	}
	ret = gpio_request(chg_en_gpio, "hl7015 chg en pin");
	if (ret) {
		dev_err(bdi->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
		return ret;
	}
	gpio_direction_output(chg_en_gpio,0);//default enable charge
	printk("%s: chip-enable-gpio = (%d) gpio_state = (%d) gpio_get_value end. \n", __func__,chg_en_gpio, gpio_get_value(chg_en_gpio));

	if (of_property_read_string(bdi->dev->of_node, "alias_name", &(bdi->chg_props.alias_name)) < 0) {
		bdi->chg_props.alias_name = "hl7015";
		dev_err(bdi->dev, "[hl7015] %s: no alias name\n", __func__);
	}

#if IS_ENABLED(CONFIG_TCPC_HUSB311)
    drvbus_gpio = of_get_named_gpio(np, "h1.h17015,drvbus-gpio", 0);
    if (drvbus_gpio < 0) {
        pr_err("h1.h17015,drvbus-gpio is not available\n");
    } else {
        ret = gpio_request(drvbus_gpio, "h1.h17015,drvbus-gpio");
        if (ret < 0) {
            pr_info(" %s gpio_request h1.h17015,drvbus-gpio failed!\n",__func__);
        } else {
            if (gpio_is_valid(drvbus_gpio)) {
                gpio_direction_output(drvbus_gpio, 0);
                pr_info("hl7015 drvbus_gpio default low\n");
            }
        }
    }
#endif

	pr_info("[%s] end \n", __func__);	
	return 0;
}

static int hl7015_get_config(struct hl7015_dev_info *bdi)
{
	const char * const s = "ti,system-minimum-microvolt";
	struct power_supply_battery_info info = {};
	int v;

	if (device_property_read_u32(bdi->dev, s, &v) == 0) {
		v /= 1000;
		if (v >= HL7015_REG_POC_SYS_MIN_MIN
		 && v <= HL7015_REG_POC_SYS_MIN_MAX)
			bdi->sys_min = v;
		else
			dev_warn(bdi->dev, "invalid value for %s: %u\n", s, v);
	}

	if (bdi->dev->of_node &&
	    !power_supply_get_battery_info(bdi->charger, &info)) {
		v = info.precharge_current_ua / 1000;
		if (v >= HL7015_REG_PCTCC_IPRECHG_MIN
		 && v <= HL7015_REG_PCTCC_IPRECHG_MAX)
			bdi->iprechg = v;
		else
			dev_warn(bdi->dev, "invalid value for battery:precharge-current-microamp: %d\n",
				 v);

		v = info.charge_term_current_ua / 1000;
		if (v >= HL7015_REG_PCTCC_ITERM_MIN
		 && v <= HL7015_REG_PCTCC_ITERM_MAX)
			bdi->iterm = v;
		else
			dev_warn(bdi->dev, "invalid value for battery:charge-term-current-microamp: %d\n",
				 v);
	}

	return 0;
}

static int hl7015_plug_in(struct charger_device *chg_dev)
{
	struct hl7015_dev_info *bdi = charger_get_data(chg_dev);
	u8 ss_reg;
	printk("%s:carrot start \n", __func__);

	hl7015_enable_charging(chg_dev, true);
	
	hl7015_read(bdi, HL7015_REG_SS, &ss_reg);
    printk("%s: addr[0x%x]=0x%x\n", __func__,HL7015_REG_SS, ss_reg );
	if(ss_reg & 0x40){
		hl7015_write_mask(bdi, 0x07, HL7015_REG_POC_RESET_MASK,	7, 1);
	}
	
	if (!hl7015_wake_lock->active) {
		__pm_stay_awake(hl7015_wake_lock);
	}
	return 0;
}

static int hl7015_plug_out(struct charger_device *chg_dev)
{
	struct hl7015_dev_info *bdi = charger_get_data(chg_dev);
	bdi->power_good = 0;
	printk("%s:carrot start \n", __func__);
	hl7015_enable_charging(chg_dev, false);

	if (hl7015_wake_lock->active) {
		__pm_relax(hl7015_wake_lock);
	}
	return 0;
}

static struct charger_ops hl7015_chg_ops = {
	/* cable plug in/out */
	.plug_in = hl7015_plug_in,
	.plug_out = hl7015_plug_out,

	/* enable charging */
	.enable = hl7015_enable_charging,

	/* charge current stuff */
	.get_charging_current = hl7015_get_current,
	.set_charging_current = hl7015_set_current,

	/* charge voltage stuff */
	.get_constant_voltage = hl7015_get_cv_voltage,
	.set_constant_voltage = hl7015_set_cv_voltage,

	/* ic watch dog */
	.kick_wdt = hl7015_reset_watch_dog_timer,

	/* input charge current stuff */
	.get_input_current = hl7015_get_input_current,
	.set_input_current = hl7015_set_input_current,

	/* vindpm stuff */
	.get_mivr_state = hl7015_get_vindpm_voltage,
	.set_mivr = hl7015_set_vindpm_voltage,

	/* charging over */
	.is_charging_done = hl7015_get_charging_status,
	/* safety timer stuff */

	.is_safety_timer_enabled = hl7015_is_safety_timer_enabled,
	.enable_safety_timer = hl7015_enable_safety_timer,

	/* powerpath stuff */
	.is_powerpath_enabled = hl7015_is_powerpath_enabled,
	.enable_powerpath = hl7015_enable_powerpath,     //drv_fix   2_reboot

	/* OTG */
	.enable_otg = hl7015_charger_enable_otg,
	.set_boost_current_limit = hl7015_set_boost_current_limit,

	/* termination current stuff */
	.get_eoc_current = hl7015_get_termination_curr,
	.set_eoc_current = hl7015_set_termination_curr,

    /* ADC */
    //.get_adc = NULL,
    .get_vbus_adc = hl7015_get_vbus_adc,
    //.get_ibus_adc = NULL,
    //.get_ibat_adc = NULL,
    //.get_tchg_adc = NULL,
    //.get_zcv = NULL,
	.event = hl7015_do_event,

	/* pe/pe+ */
	.send_ta_current_pattern = hl7015_set_ta_current_pattern,
	.reset_ta = hl7015_reset_ta,

	.enable_hz = hl7015_set_en_hiz,

	/* dump info */
	.dump_registers = hl7015_dump_register,
};

#if EXT_PD_CHARGER
static void hl7015_pd_enable(struct work_struct *work) 
{
    struct hl7015_dev_info *bdi = container_of(work,struct hl7015_dev_info, pd_enable_work.work);	

	bdi->pd_charger_enable = get_other_pd_reset_state();
	pr_err("bdi->pd_charger_enable=%d\n",bdi->pd_charger_enable);	
}

static void hl7015_pd_connect(struct hl7015_dev_info *bdi,struct extcon_dev *edev)
{
    union extcon_property_value prop_val;
    int ret;
    int vol, cur;

    if (extcon_get_state(edev, EXTCON_CHG_USB_FAST) > 0) {
        ret = extcon_get_property(edev, EXTCON_CHG_USB_FAST,
                      EXTCON_PROP_USB_TYPEC_POLARITY,
                      &prop_val);
        pr_err("usb pd charge...\n");
        vol = prop_val.intval & 0xffff;
        cur = prop_val.intval >> 15;
        if (ret == 0) {
			schedule_delayed_work(&bdi->pd_enable_work,msecs_to_jiffies(500));

            bdi->fast_input_cur = cur;
            pr_err("vol==%d cur==%d fast_input_cur==%d\n",vol, cur, bdi->fast_input_cur);

            bdi->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        	bdi->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        	power_supply_changed(bdi->charger);
        }
    }
}

static void hl7015_pd_evt_worker(struct work_struct *work)
{
    struct hl7015_dev_info *bdi = container_of(work,struct hl7015_dev_info, pd_work.work);

    struct extcon_dev *edev = bdi->cable_edev;

    hl7015_pd_connect(bdi, edev);
}

static int hl7015_pd_evt_notifier(struct notifier_block *nb,
                   unsigned long event,
                   void *ptr)
{
    struct hl7015_dev_info *bdi = container_of(nb, struct hl7015_dev_info, cable_pd_nb);

    queue_delayed_work(bdi->usb_charger_wq, &bdi->pd_work,msecs_to_jiffies(10));

    return NOTIFY_DONE;
}

static int hl7015_register_pd_nb(struct hl7015_dev_info *bdi)
{
    if (bdi->cable_edev) {
        INIT_DELAYED_WORK(&bdi->pd_work, hl7015_pd_evt_worker);
        bdi->cable_pd_nb.notifier_call = hl7015_pd_evt_notifier;
        extcon_register_notifier(bdi->cable_edev,
                     EXTCON_CHG_USB_FAST,
                     &bdi->cable_pd_nb);
    }

    return 0;
}

static long hl7015_init_usb(struct hl7015_dev_info *bdi)
{
    struct extcon_dev *edev;
    struct device *dev = bdi->dev;

    bdi->usb_charger_wq = create_singlethread_workqueue("hl7015-usb-wq");

    /* type-C */
    edev = extcon_get_edev_by_phandle(dev, 0);
    if (IS_ERR(edev)) {
        if (PTR_ERR(edev) != -EPROBE_DEFER)
            dev_err(dev, "Invalid or missing extcon dev0\n");
        bdi->cable_edev = NULL;
    } else {
        bdi->cable_edev = edev;
    }
    
    hl7015_register_pd_nb(bdi);

    if (bdi->cable_edev) {
        schedule_delayed_work(&bdi->pd_work, 0);
    }

    return 0;
}
#endif

static int hl7015_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = {};
	struct hl7015_dev_info *bdi;
	int ret;
	u8 ss_reg_temp, first_type;

	printk("%s:carrot start \n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	bdi = devm_kzalloc(dev, sizeof(*bdi), GFP_KERNEL);
	if (!bdi) {
		dev_err(dev, "Can't alloc bdi struct\n");
		return -ENOMEM;
	}
	printk("%s:carrot 111 \n", __func__);
	bdi->client = client;
	bdi->dev = dev;
	strncpy(bdi->model_name, id->name, I2C_NAME_SIZE);
	mutex_init(&bdi->f_reg_lock);
	bdi->f_reg = 0;
	bdi->ss_reg = HL7015_REG_SS_VBUS_STAT_MASK; /* impossible state */
#if 0
	INIT_DELAYED_WORK(&bdi->input_current_limit_work,
			  hl7015_input_current_limit_work);
#endif
	i2c_set_clientdata(client, bdi);

	ret = hl7015_parse_dt(bdi);
	if (ret)
		return ret;

	new_client = client;

	printk("%s:carrot 222 \n", __func__);
	if (bdi->client->irq <= 0) {
		dev_err(dev, "Can't get irq info\n");
		return -EINVAL;
	}

	bdi->edev = devm_extcon_dev_allocate(dev, hl7015_usb_extcon_cable);
	if (IS_ERR(bdi->edev))
		return PTR_ERR(bdi->edev);

	ret = devm_extcon_dev_register(dev, bdi->edev);
	if (ret < 0)
		return ret;

	printk("%s:carrot 333 \n", __func__);
#ifdef CONFIG_SYSFS
	hl7015_sysfs_init_attrs();
	charger_cfg.attr_grp = hl7015_sysfs_groups;
#endif

	charger_cfg.drv_data = bdi;
	charger_cfg.of_node = dev->of_node;
	charger_cfg.supplied_to = hl7015_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(hl7015_charger_supplied_to),
	memcpy(&bdi->psy_desc, &hl7015_charger_desc, sizeof(bdi->psy_desc));
	//bdi->charger = power_supply_register(dev, &hl7015_charger_desc,
	//					&charger_cfg);
	bdi->psy_desc.name = dev_name(bdi->dev);//"charger";//dev_name(sc->dev);
    bdi->charger = devm_power_supply_register(bdi->dev, &bdi->psy_desc,
                        &charger_cfg);
	if (IS_ERR(bdi->charger)) {
		dev_err(dev, "Can't register charger\n");
		ret = PTR_ERR(bdi->charger);
		goto out_pmrt;
	}
	#if 0
	/* the battery class is deprecated and will be removed. */
	/* in the interim, this property hides it.              */
	if (!device_property_read_bool(dev, "omit-battery-class")) {
		battery_cfg.drv_data = bdi;
		bdi->battery = power_supply_register(dev, &hl7015_battery_desc,
						     &battery_cfg);
		if (IS_ERR(bdi->battery)) {
			dev_err(dev, "Can't register battery\n");
			ret = PTR_ERR(bdi->battery);
			goto out_charger;
		}
	}
	#endif
	printk("%s:carrot 444 \n", __func__);
	ret = hl7015_get_config(bdi);
	if (ret < 0) {
		dev_err(dev, "Can't get devicetree config\n");
		goto out_charger;
	}
#if 0
	ret = hl7015_hw_init(bdi);
	if (ret < 0) {
		dev_err(dev, "Hardware init failed\n");
		goto out_charger;
	}
#endif
	/* Register charger device */
	bdi->chg_dev = charger_device_register("primary_chg",
		&client->dev, bdi, &hl7015_chg_ops, &bdi->chg_props);

	ret = hl7015_configure_usb_otg(bdi, bdi->ss_reg);
	if (ret < 0)
		goto out_charger;

	bdi->initialized = true;
	g_bdi = bdi;
	ret = devm_request_threaded_irq(dev, bdi->client->irq, NULL,
			hl7015_irq_handler_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"hl7015-charger", bdi);
	if (ret < 0) {
		dev_err(dev, "Can't set up irq handler\n");
		goto out_charger;
	}
	printk("%s:carrot 555 \n", __func__);
	ret = hl7015_register_vbus_regulator(bdi);
	if (ret < 0)
		goto out_charger;

	enable_irq_wake(bdi->client->irq);
	hl7015_wake_lock = wakeup_source_register(NULL, "hl7015_wake_lock");

	INIT_DELAYED_WORK(&bdi->read_byte_work, hl7015_read_byte_work);

	printk("%s:carrot end \n", __func__);

#if EXT_PD_CHARGER
    hl7015_init_usb(bdi);
    INIT_DELAYED_WORK(&bdi->pd_enable_work, hl7015_pd_enable);
#endif

	ret = hl7015_read(bdi, HL7015_REG_SS, &ss_reg_temp);
	printk("%s:carrot start addr[0x%x]=0x%x \n", __func__,HL7015_REG_SS, ss_reg_temp);
	if (ret < 0) {
		dev_err(bdi->dev, "Can't read SS reg: %d\n", ret);
		return -1;
	}
	bdi->power_good = !!(ss_reg_temp & HL7015_REG_SS_PG_STAT_MASK);
	first_type = ss_reg_temp & 0xc0;

	if(bdi->power_good && ((0x80 == first_type) || (0x40 == first_type) ))
		schedule_delayed_work(&bdi->read_byte_work, msecs_to_jiffies(3000));

	hl7015_set_boost_9v_en(0);
	hl7015_set_boostv(10);
	hl7015_set_charge_timer(0x2);
	yft_hl7015_dump_register(bdi);
	if (ret < 0) {
		dev_err(dev, "yft_hl7015_dump_register failed\n");
		goto out_charger;
	}

	ss_reg_temp = 0x50;
	hl7015_write_byte(0xd, &ss_reg_temp, 2);

	return 0;

out_charger:
	power_supply_unregister(bdi->charger);

out_pmrt:

	return ret;
}

static int hl7015_remove(struct i2c_client *client)
{
	struct hl7015_dev_info *bdi = i2c_get_clientdata(client);

	hl7015_register_reset(bdi);
	power_supply_unregister(bdi->charger);

	return 0;
}

static const struct i2c_device_id hl7015_i2c_ids[] = {
	{ "hl7015" },
	{ },
};
MODULE_DEVICE_TABLE(i2c, hl7015_i2c_ids);

static const struct of_device_id hl7015_of_match[] = {
	{ .compatible = "mtk,hl7015", },
	{ },
};
MODULE_DEVICE_TABLE(of, hl7015_of_match);

static struct i2c_driver hl7015_driver = {
	.probe		= hl7015_probe,
	.remove		= hl7015_remove,
	.id_table	= hl7015_i2c_ids,
	.driver = {
		.name		= "hl7015-charger",
		.of_match_table	= of_match_ptr(hl7015_of_match),
	},
};
module_i2c_driver(hl7015_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark A. Greer <mgreer@animalcreek.com>");
MODULE_DESCRIPTION("TI HL7015 Charger Driver");
