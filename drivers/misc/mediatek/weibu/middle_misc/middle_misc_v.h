#ifndef __MIDDLE_MISC_V_H_
#define __MIDDLE_MISC_V_H_

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



extern int cust_mid_misc_v_get_boot_mode(void);
extern unsigned int DISP_GetScreenHeight_v(void);
extern unsigned int DISP_GetScreenWidth_v(void);

#define BOOTC_MAX_SIZE			256
#define BOOT_COMPLETED_CHAIN     0x10U
extern int midmisc_v_register_boot_completed_notifier(struct notifier_block *nb);
#endif
