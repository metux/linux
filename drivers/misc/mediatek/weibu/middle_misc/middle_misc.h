#ifndef __MIDDLE_MISC_H_
#define __MIDDLE_MISC_H_

enum boot_mode_t {
        NORMAL_BOOT = 0,
        META_BOOT = 1,
        RECOVERY_BOOT = 2,
        SW_REBOOT = 3,
        FACTORY_BOOT = 4,
        ADVMETA_BOOT = 5,
        ATE_FACTORY_BOOT = 6,
        ALARM_BOOT = 7,
        KERNEL_POWER_OFF_CHARGING_BOOT = 8,
        LOW_POWER_OFF_CHARGING_BOOT = 9,
        DONGLE_BOOT = 10, 
        UNKNOWN_BOOT
};

enum midmisc_log_type_t {
	MIDMISC_CM_LOG = 0,
	MIDMISC_TS_LOG = 1,
	MIDMISC_LOG_TYPE_MAX,
};

extern int cust_mid_misc_get_boot_mode(void);
extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);
extern bool cust_midmisc_get_log_en(int type);
void cust_midmisc_get_use_tp_name_from_lk(const char **name);
extern int cust_mid_misc_is_boot_completed(void);
#if IS_ENABLED(CONFIG_WB_BOARD_ID_SUPPORT) //Leo 20230417
extern int cust_midmisc_get_board_id(void);
#endif
extern char *cust_midmisc_get_lcm_name(void);

#define BOOTC_MAX_SIZE			256
#define BOOT_COMPLETED_CHAIN     0x10U
extern int midmisc_register_boot_completed_notifier(struct notifier_block *nb);

#if defined(CONFIG_CM_NOTIFY_EVENT_SUPPORT) //Leo 20240202
#define NE_MAX_SIZE			80
#define NOTIFY_EVENT_CHAIN     0x11U
extern int init_midmisc_notify_event(void);
extern int midmisc_register_notify_event_notifier(struct notifier_block *nb);
#endif



#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
extern int cust_midmisc_get_dualmipi_chipid(void);
#endif









#endif