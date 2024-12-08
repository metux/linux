#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/list.h>

#include "cust_gpios.h"

#define TAG                  "[CUST][GPIO] :"
#define FUNC_ENTRY           pr_info(TAG"%s entry!\n",__func__);
#define FUNC_EXIT            pr_info(TAG"%s exit!\n",__func__);
#define CUST_GPIO_DEVICE     "cust_gpio"

struct cust_gpio_attr cust_gpios[CUST_GPIO_MAX] = {
       [CUST_GPIO_RLED]         =  {"gpio_rled_ctl"},
       [CUST_GPIO_GLED]         =  {"gpio_gled_ctl"},
       [CUST_GPIO_BLED]         =  {"gpio_bled_ctl"},
       [CUST_GPIO_LCM_3V3]      =  {"gpio_lcm_3v3"},
       [CUST_GPIO_LCM_1V8]      =  {"gpio_lcm_1v8"},
       [CUST_GPIO_LCM_RST]      =  {"gpio_lcm_rst"},
       [CUST_GPIO_OTG_5V_EN]    =  {"gpio_otg_5v_en"},
       [CUST_GPIO_TYPEC_OTG_EN] =  {"gpio_typec_otg_en"},
       [CUST_GPIO_DOCKING_EN]   =  {"gpio_docking_en"},
       [CUST_GPIO_USB_SWITCH]   =  {"gpio_usb_switch"},
       [CUST_GPIO_EXT_3V3_EN]   =  {"gpio_ext_3v3_en"},
       [CUST_GPIO_AUDIO_MI_EN]  =  {"gpio_audio_mic_en"},
       [CUST_GPIO_MCU_3V3]      =  {"gpio_mcu_3v3"},
       [CUST_GPIO_MCU_BOOT]     =  {"gpio_mcu_boot"},
       [CUST_GPIO_PSAM_5V]      =  {"gpio_psam_5v"},
       [CUST_GPIO_PSAM_3V3]     =  {"gpio_psam_3v3"},
       [CUST_GPIO_LCM_BL_EN]    =  {"gpio_lcm_bl_en"}, 
       [CUST_GPIO_TP_RST]       =  {"gpio_tp_rst"},
       [CUST_GPIO_BIAS_ENP]     =  {"gpio_bias_enp"},
       [CUST_GPIO_BIAS_POS]     =  {"gpio_bias_pos"},
       [CUST_GPIO_BIAS_NEG]     =  {"gpio_bias_neg"},
       [CUST_GPIO_IT6112_EN]    =  {"gpio_it6112_en"},
       [CUST_GPIO_BIAS_EN]      =  {"gpio_bias_en"},
       [CUST_GPIO_AUHPR_SPK_SW] =  {"gpio_auhpr_spk_sw"},
       [CUST_GPIO_USB_HP_SW]    =  {"gpio_usb_hp_sw"},
       [CUST_GPIO_EDP_IRQO2]    =  {"gpio_edp_irqo2"},
       [CUST_GPIO_LCM_BL_EN2]    =  {"gpio_lcm_bl_en2"},
       [CUST_GPIO_IT6151_1V2_EN] =  {"gpio_it6151_1V2_en"},
       [CUST_GPIO_IT6151_1V8_EN]  =  {"gpio_it6151_1V8_en"},
       [CUST_GPIO_IT6151_ENPSR]  =  {"gpio_it6151_enpsr" },
       [CUST_GPIO_IT6151_STBY]   =  {"gpio_it6151_stby"  },
       [CUST_GPIO_IT6151_RESET]  =  {"gpio_it6151_reset" },
       [CUST_GPIO_LCM_EDP_EN]    =  {"gpio_lcm_edp_en" },
       [CUST_GPIO_PS_VDD_EN]    =  {"gpio_ps_vdd_en" },
       [CUST_GPIO_CUTOFF_VBUS]    =  {"gpio_cutoff_vbus" },
};

#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_EXT_SUPPORT) //Leo 20230129
LIST_HEAD(cgpio_ext_list);
EXPORT_SYMBOL_GPL(cgpio_ext_list);

struct rw_semaphore cgpio_list_mtx;
DECLARE_RWSEM(cgpio_list_mtx);
EXPORT_SYMBOL_GPL(cgpio_list_mtx);

int cust_gpio_ext_register(struct cust_gpio_ext *cgpio_ext)
{
	down_write(&cgpio_list_mtx);
	list_add_tail(&cgpio_ext->node, &cgpio_ext_list);
	up_write(&cgpio_list_mtx);
	
	return 0;
}
EXPORT_SYMBOL(cust_gpio_ext_register);

int cust_gpio_get_ext_value(char *name) 
{
	struct cust_gpio_ext *cgpio_ext;

	list_for_each_entry(cgpio_ext, &cgpio_ext_list, node) {
		if (!strcmp(name, cgpio_ext->name)) {
			if (gpio_is_valid(cgpio_ext->gpio)) {
				return gpio_get_value(cgpio_ext->gpio);
			} else {
				pr_err(TAG"%s gpio:%s is unvalid :%d \n",
						__func__, cgpio_ext->name, cgpio_ext->gpio);
				return -ENODEV;
			}
		}
	}

	pr_err(TAG"%s gpio:%s is no found! \n",__func__, cgpio_ext->name);

	return -ENODEV;
}
EXPORT_SYMBOL(cust_gpio_get_ext_value);

int cust_gpio_get_num_value(char *name) 
{
	struct cust_gpio_ext *cgpio_ext;

	list_for_each_entry(cgpio_ext, &cgpio_ext_list, node) {
		if (!strcmp(name, cgpio_ext->name)) {
			if (gpio_is_valid(cgpio_ext->gpio)) {
				return cgpio_ext->gpio;
			} else {
				pr_err(TAG"%s gpio:%s is unvalid :%d \n",
						__func__, cgpio_ext->name, cgpio_ext->gpio);
				return -ENODEV;
			}
		}
	}

	pr_err(TAG"%s gpio:%s is no found! \n",__func__, cgpio_ext->name);

	return -ENODEV;
}
EXPORT_SYMBOL(cust_gpio_get_num_value);

int cust_gpio_set_ext_value(char *name, int value)
{
	struct cust_gpio_ext *cgpio_ext;

	list_for_each_entry(cgpio_ext, &cgpio_ext_list, node) {
		if (!strcmp(name, cgpio_ext->name)) {
			if (gpio_is_valid(cgpio_ext->gpio)) {
				return gpio_direction_output(cgpio_ext->gpio, !!value);
			} else {
				pr_err(TAG"%s gpio:%s is unvalid :%d \n",
						__func__, cgpio_ext->name, cgpio_ext->gpio);
				return -ENODEV;
			}
		}
	}

	pr_err(TAG"%s gpio:%s is no found! \n",__func__, cgpio_ext->name);

	return 0;
}
EXPORT_SYMBOL(cust_gpio_set_ext_value);

int cust_gpio_ext_init(const char *name, int gpio)
{
	struct cust_gpio_ext *p_cgpio_ext;

	if (!gpio_is_valid(gpio)) {
		pr_err(TAG"%s register gpio num error! \n",__func__);
		return -ENODEV;
	}

	p_cgpio_ext = kzalloc(sizeof(struct cust_gpio_ext), GFP_KERNEL);
	if (!p_cgpio_ext) {
		pr_err("[CUST][GPIO] %s kzalloc mem failed!\n",name);
		return -ENOMEM;
	}

	p_cgpio_ext->gpio = gpio;
	strcpy(p_cgpio_ext->name, name);
	INIT_LIST_HEAD(&p_cgpio_ext->node);
	cust_gpio_ext_register(p_cgpio_ext);
	
	return 0;
}
EXPORT_SYMBOL(cust_gpio_ext_init);
#endif

struct cust_gpio_attr *cust_get_gpio(enum cust_gpio_type type)
{
	return 	&cust_gpios[type];
}
EXPORT_SYMBOL(cust_get_gpio);

static int cust_gpio_init(struct platform_device *pdev, 
                 struct cust_gpio_attr gpios[], int size)
{
	int ret = 0, i = 0;

	for(i = 0; i < size; i++) {
		gpios[i].exist = 1; //init exist
		if (gpios[i].name != NULL) {
			gpios[i].pin = of_get_named_gpio(pdev->dev.of_node, gpios[i].name, 0);
			if (gpios[i].pin < 0) {
				gpios[i].exist = 0;
				pr_info("[CUST][GPIO] %s = %d\n",gpios[i].name,gpios[i].pin);
			}
		} else {
			gpios[i].exist = 0; 
			pr_err("[CUST][GPIO]*** error :pin idx:%d no apply the name!\n",gpios[i].pin);
		}

		if (gpios[i].exist) {
			ret = gpio_request(gpios[i].pin, gpios[i].name);
			if (ret < 0) {
				gpios[i].exist = 0;
				pr_info("[CUST][GPIO] %s gpio_request failed!\n",gpios[i].name);
			} 
#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_EXT_SUPPORT) //Leo 20230129
			else
			{
				cust_gpio_ext_init(gpios[i].name, gpios[i].pin);
			}
#endif
		}
		pr_info("[CUST][GPIO] func:%s name:%s pin:%d \n",__func__,
											gpios[i].name,gpios[i].pin);
	}

	return ret;
}

int cust_gpio_set_value(enum cust_gpio_type type,int value)
{
	if (type >= CUST_GPIO_MAX) {
		pr_err("[CUST][GPIO] func:%s fatel err type:%d no exist!\n",__func__,type);
        return -ENODEV;
	}	
	pr_info("[CUST][GPIO] set pin:%s pin:%d value:%d!\n", 
				cust_gpios[type].name, cust_gpios[type].pin,value);

	if (cust_gpios[type].exist) {
		gpio_direction_output(cust_gpios[type].pin, !!value);
		return 0;
	} else {
		pr_debug("[CUST][GPIO] pin %s no found in dts!\n",cust_gpios[type].name);
		return -ENODEV;
	}
}
EXPORT_SYMBOL(cust_gpio_set_value);

int cust_gpio_get_value(enum cust_gpio_type type)
{
	if (type >= CUST_GPIO_MAX) {
		pr_err("[CUST][GPIO] func:%s fatel err type:%d no exist!\n",__func__,type);
		return -ENODEV;
	}

	if (cust_gpios[type].exist) {
		pr_info("[CUST][GPIO] pin:%s val:%d \n",cust_gpios[type].name
									,gpio_get_value(cust_gpios[type].pin));
		return  gpio_get_value(cust_gpios[type].pin);
	} else {
		pr_err("[CUST][GPIO] pin %s no found in dts!\n",cust_gpios[type].name);
		return -ENODEV;
	}
}
EXPORT_SYMBOL(cust_gpio_get_value);

int cust_gpio_set_output(enum cust_gpio_type type,int value)
{
	if (type >= CUST_GPIO_MAX) {
		pr_err("[CUST][GPIO] func:%s fatel err type:%d no exist!\n",__func__,type);
		return -ENODEV;
	}
	pr_info("[CUST][GPIO] set pin:%s pin:%d value:%d!\n",
			cust_gpios[type].name,cust_gpios[type].pin,value);

	if (cust_gpios[type].exist) {
		gpio_direction_output(cust_gpios[type].pin, !!value);
		return 0;
	} else {
		pr_err("[CUST][GPIO] pin %s no found in dts!\n",cust_gpios[type].name);
		return -ENODEV;
	}
}
EXPORT_SYMBOL(cust_gpio_set_output);

int cust_gpio_set_input(enum cust_gpio_type type)
{
	if (type >= CUST_GPIO_MAX) {
		pr_err("[CUST][GPIO] func:%s fatel err type:%d no exist!\n",__func__,type);
		return -ENODEV;
	}
	pr_info("[CUST][GPIO] set pin:%s pin:%d !\n",
			cust_gpios[type].name,cust_gpios[type].pin);

	if (cust_gpios[type].exist) {
		gpio_direction_input(cust_gpios[type].pin);
		return 0;
	} else {
		pr_err("[CUST][GPIO] pin %s no found in dts!\n",cust_gpios[type].name);
		return -ENODEV;
	}
}
EXPORT_SYMBOL(cust_gpio_set_input);

#if defined(CGPIO_MISCDEV_SUPPORT) //Leo 20200824
static int cgpio_open(struct inode *inode, struct file *filp)
{
	FUNC_ENTRY
	
	return 0;
}

static int cgpio_close(struct inode *inode, struct file *filp)
{
	FUNC_ENTRY
	
	return 0;
}

static ssize_t cgpio_read(struct file *file,
			char __user *buf, size_t count, loff_t *ppos)
{
	FUNC_ENTRY
	
	return count;
}

static ssize_t cgpio_write(struct file *file, 
			const char __user *buf, size_t count, loff_t *ppos)
{
	#if 0 
	int s_value = 1;
	FUNC_ENTRY

	copy_from_user(&s_value, buf, sizeof(s_value));
	pr_info(TAG"s_value:%d\n",s_value);
	cust_gpio_set_output(CUST_GPIO_RF24_RSV, !!s_value);
	#endif
	
	return count;
}

static long
cgpio_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int status = 0;
	int s_value = -1;
	int g_value = -1;
	void __user *argp = (void __user *)arg;

	FUNC_ENTRY
	
	switch (cmd) {
	#if 0 //Leo 20200824
	case CGPIO_IOC_WD_CLK_SET:
		pr_info(TAG"CGPIO_IOC_WD_CLK_SET entry!\n");
		copy_from_user(&s_value, argp, sizeof(s_value));
		pr_info(TAG"s_value:%d \n",s_value);
		status = y140_feed_wd_clk(s_value);
		if (status < 0)
			pr_err(TAG"%s CGPIO_IOC_WD_CLK_SET failed!\n",__func__); 
		break;
	case CGPIO_IOC_WD_CLK_GET:
		pr_info(TAG"CGPIO_IOC_WD_CLK_GET entry!\n");
		g_value = cust_gpio_get_value(CUST_GPIO_WTD_CLK);
		if (g_value < 0) {
			pr_err(TAG"%s CGPIO_IOC_WD_CLK_GET failed!\n",__func__); 
		} else {
			if (copy_to_user(argp, &g_value, sizeof(g_value))){
				pr_err(TAG "copy_to_user failed\n");
				status = -EFAULT;
			}
		}
		pr_info(TAG"g_value:%d \n",g_value);
		break;
	#endif
	default:
		pr_err(TAG"%s %d cmd no found\n",__func__, (unsigned int)argp); 
		status = -EINVAL;
		break;
	}

	pr_info(TAG"s_value:%d, g_value:%d",s_value, g_value);
	
	return status;
}

#ifdef CONFIG_COMPAT
static long
cgpio_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	FUNC_ENTRY
	
	return 0;
}
#endif

static const struct file_operations cgpio_fops = {
	.owner     = THIS_MODULE,
	.open      = cgpio_open,
	.read      = cgpio_read,
	.write     = cgpio_write,
	.release   = cgpio_close,
	.unlocked_ioctl = cgpio_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl   = cgpio_compat_ioctl,
	#endif
};

struct miscdevice cgpio_device = {
	.name = "cgpio_device",
	.fops = &cgpio_fops,
	.minor = MISC_DYNAMIC_MINOR,
};
#endif

//debug part
static ssize_t cust_get_value_show(struct device* cd,
                      struct device_attribute *attr, char* buf)
{
	int i = 0, len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "\r\n"); 
	for (i = 0; i < CUST_GPIO_MAX; i++) {
	if (cust_gpios[i].exist) {
		len += snprintf(buf+len, PAGE_SIZE-len, "name:%s index:%d value:%d\n",
				cust_gpios[i].name, i, gpio_get_value(cust_gpios[i].pin));
		}
	}	
	
	return len;
}

static ssize_t cust_set_value_store(struct device* cd,
                   struct device_attribute *attr,const char* buf, size_t len)
{
	unsigned int databuf[2];
	if (2 == sscanf(buf,"%d %d",&databuf[0], &databuf[1])) {
		if (cust_gpios[databuf[0]].exist) {
			pr_info("pin_index:%d value:%d", cust_gpios[databuf[0]].pin, databuf[1]);
			gpio_direction_output(cust_gpios[databuf[0]].pin, databuf[1]);
		}
	}
	return len;
}

#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_EXT_SUPPORT)
static ssize_t cust_set_ext_value_store(struct device* cd,
                   struct device_attribute *attr,const char* buf, size_t len)
{
	char name[32] = {0};
	unsigned int databuf;

	if (2 == sscanf(buf,"%s %d", name, &databuf)) {
		if (gpio_is_valid(databuf)) {
			pr_info("name:%s value:%d \n", name, databuf);
			cust_gpio_set_ext_value(name, databuf);
		}
	}
	return len;
}
static ssize_t cust_get_ext_value_show(struct device* cd,
                      struct device_attribute *attr, char* buf)
{

	struct cust_gpio_ext *cgpio_ext;
	int len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "\r\n"); 

	list_for_each_entry(cgpio_ext, &cgpio_ext_list, node) {
		len += snprintf(buf+len, PAGE_SIZE-len, "get ext_name:%s  value:%d\n",
				cgpio_ext->name, cust_gpio_get_ext_value(cgpio_ext->name));
	}

	return len;
}
#endif

static DEVICE_ATTR(custgpio, 0664, cust_get_value_show, cust_set_value_store);
#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_EXT_SUPPORT) //Leo 20230129
static DEVICE_ATTR(custgpioext, 0664, cust_get_ext_value_show, cust_set_ext_value_store);
#endif

static struct attribute *custgpio_attrs[] = {
	&dev_attr_custgpio.attr,
#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_EXT_SUPPORT) //Leo 20230129
	&dev_attr_custgpioext.attr,
#endif
	NULL,
};

static struct attribute_group custgpio_group = {
	.attrs = custgpio_attrs
};


#ifdef CONFIG_OF
static const struct of_device_id _cust_gpio_of_ids[] = {
	{.compatible = "cust,cust_gpios",},
	{},
};
MODULE_DEVICE_TABLE(of, _cust_gpio_of_ids);
#endif

static void cust_misc_init(void)
{
	cust_gpio_set_value(CUST_GPIO_PS_VDD_EN, 1);//chench 2024829 
}

static int _cust_gpio_probe(struct platform_device *pdev)
{
	int ret;
	
	cust_gpio_init(pdev, cust_gpios, CUST_GPIO_MAX);

	cust_misc_init();

	ret = sysfs_create_group(&pdev->dev.kobj, &custgpio_group);
	if (ret < 0) {
		pr_err("[CUST][GPIO] unable to create custgpio attribute file\n");
		return ret;
	}

	#if defined(CGPIO_MISCDEV_SUPPORT) //Leo 20200824
	ret = misc_register(&cgpio_device);
	if (ret < 0) {
		pr_err(TAG"misc_register failed\n");
		return ret;
	}
	#endif

	return 0;
}

static int _cust_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver _cust_gpio_driver = {
	.driver = {
		.name = CUST_GPIO_DEVICE,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(_cust_gpio_of_ids),
	},
	.probe = _cust_gpio_probe,
	.remove = _cust_gpio_remove,
};

static int __init _cust_gpio_init(void)
{
	pr_info("CUST GPIO driver init\n");
	if (platform_driver_register(&_cust_gpio_driver) != 0) {
		pr_err("unable to register CUST GPIO driver.\n");
		return -1;
	}
	return 0;
}

/* should never be called */
static void __exit _cust_gpio_exit(void)
{
	pr_info("CUST GPIO driver exit\n");
	platform_driver_unregister(&_cust_gpio_driver);
}

module_init(_cust_gpio_init);
module_exit(_cust_gpio_exit);
MODULE_LICENSE("GPL");
