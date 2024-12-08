#ifndef BUILD_LK
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#define VALUE_ON        1
#define VALUE_OFF       0

#if defined(CGPIO_MISCDEV_SUPPORT) //Leo 20200824
#define CGPIO_IOC_MAGIC                   'c'
#define  CGPIO_IOC_RLED_SET              _IOW(CGPIO_IOC_MAGIC, 1, int)
#define  CGPIO_IOC_RLED_GET              _IOW(CGPIO_IOC_MAGIC, 2, int)
#define  CGPIO_IOC_GLED_SET              _IOW(CGPIO_IOC_MAGIC, 3, int)
#define  CGPIO_IOC_GLED_GET              _IOW(CGPIO_IOC_MAGIC, 4, int)
#define  CGPIO_IOC_BLED_SET              _IOW(CGPIO_IOC_MAGIC, 5, int)
#define  CGPIO_IOC_BLED_GET              _IOW(CGPIO_IOC_MAGIC, 6, int)
#define  CGPIO_IOC_RF24_RSV_SET          _IOW(CGPIO_IOC_MAGIC, 7, int)
#define  CGPIO_IOC_RF24_RSV_GET          _IOW(CGPIO_IOC_MAGIC, 8, int)
#define  CGPIO_IOC_WD_CLK_SET            _IOW(CGPIO_IOC_MAGIC, 9, int)
#define  CGPIO_IOC_WD_CLK_GET            _IOW(CGPIO_IOC_MAGIC, 10, int)
#endif

struct cust_gpio_attr {
	const char *name;
	int pin;
	bool exist;
};
#endif

#ifndef HIGH
#define HIGH        1
#define LOW       0
#endif

enum cust_gpio_type {
	CUST_GPIO_RLED = 0,
	CUST_GPIO_GLED,
	CUST_GPIO_BLED,
	CUST_GPIO_LCM_3V3,
	CUST_GPIO_LCM_1V8,
	CUST_GPIO_LCM_RST,
	CUST_GPIO_OTG_5V_EN,
	CUST_GPIO_TYPEC_OTG_EN,
	CUST_GPIO_DOCKING_EN,
	CUST_GPIO_USB_SWITCH,
	CUST_GPIO_EXT_3V3_EN,
	CUST_GPIO_AUDIO_MI_EN,
	CUST_GPIO_MCU_3V3 = 12,
	CUST_GPIO_MCU_BOOT= 13,	
	CUST_GPIO_PSAM_5V= 14,
	CUST_GPIO_PSAM_3V3= 15,
	CUST_GPIO_LCM_BL_EN = 16,
	CUST_GPIO_TP_RST = 17,
	CUST_GPIO_BIAS_ENP = 18,
	CUST_GPIO_BIAS_POS = 19,
	CUST_GPIO_BIAS_NEG = 20,
	CUST_GPIO_IT6112_EN = 21,
	CUST_GPIO_BIAS_EN = 22,
	CUST_GPIO_AUHPR_SPK_SW = 23,
	CUST_GPIO_USB_HP_SW = 24,
	CUST_GPIO_EDP_IRQO2 = 25,
	CUST_GPIO_LCM_BL_EN2 = 26,
	CUST_GPIO_IT6151_1V2_EN = 27,
	CUST_GPIO_IT6151_1V8_EN = 28,
	CUST_GPIO_IT6151_ENPSR = 29,
	CUST_GPIO_IT6151_STBY = 30,
	CUST_GPIO_IT6151_RESET = 31,
	CUST_GPIO_LCM_EDP_EN = 32,
	CUST_GPIO_PS_VDD_EN = 33,
	CUST_GPIO_CUTOFF_VBUS = 34,
	CUST_GPIO_MAX
};

#ifndef BUILD_LK
#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_EXT_SUPPORT) //Leo 20230129
struct cust_gpio_ext {
	struct list_head    node; //it should be first .
	char name[32];
	int gpio;
};

extern int cust_gpio_ext_register(struct cust_gpio_ext *cgpio_ext);
extern int cust_gpio_get_ext_value(char *name);
extern int cust_gpio_set_ext_value(char *name, int value);
extern int cust_gpio_ext_init(const char *name, int gpio);
extern int cust_gpio_get_num_value(char *name);
extern struct list_head leds_list;
#endif

extern int cust_gpio_set_value(enum cust_gpio_type type,int value);
extern int cust_gpio_get_value(enum cust_gpio_type type);
extern struct cust_gpio_attr *cust_get_gpio(enum cust_gpio_type type);
#endif
