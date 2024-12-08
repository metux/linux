#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>

#include "middle_misc.h"

#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20231010
#include <mt-plat/csci.h>
#endif

#if IS_ENABLED(CONFIG_CM_CUST_GPIOS_SUPPORT) 
#include <mt-plat/cust_gpios.h>
#endif


#if !IS_ENABLED(CONFIG_CM_CUST_GPIOS_SUPPORT)
#define cust_gpio_set_value(x,y)	pr_info("__func__ no implement! \n",__func__)
#endif

#define MID_MISC_DEVICE		"mid_misc"
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

//caozy add begin 20230418
#if IS_ENABLED(CONFIG_TCPC_FUSB302)
#if IS_ENABLED(CONFIG_WB_TD_CUST_SUPPORT)||IS_ENABLED(CONFIG_WB_PD_ALGO_SUPPORT)
static bool other_pd_reset = false;
#endif
#endif
//caozy add end 20230418

#define WB_MIDMISC_BOOTCOMPLETED_SUPPORT
#define WB_MIDMISC_DISP_SCREEN_SUPPORT
#define WB_MIDMISC_LCM_NAME

struct tag_bootmode {
	u32 size;
	u32 tag; 
	u32 bootmode;
	u32 boottype;
};

struct disp_screen_t {
	u32 height;
	u32 width;
};

struct middle_log_type {
	bool log_en;
};

struct middle_misc_t {
	int version;
	u32 boot_mode;
	struct disp_screen_t disp_screen;
	int board_id;
	int boot_completed;
	int dualmipi_chipid;
	
	struct middle_log_type *log_type;
};

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1]; /* this is the minimum size */
};

struct middle_log_type log_type[MIDMISC_LOG_TYPE_MAX] = {
	[MIDMISC_CM_LOG]         =  {0},
	[MIDMISC_TS_LOG]         =  {0},
};

#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
const char *dualmipi_ic_name[] = {
	"GM8773C",
	"IT6112",
	"IT6113"
};
#endif

static bool is_init_done = false;
static struct device *g_dev = NULL;

static struct middle_misc_t middle_misc = {
	.version = 0x01,
	.boot_mode = -1,
	.disp_screen = {
		.height = 1280,
		.width = 800,
	},
	.board_id = -1,
	.boot_completed = false,
	.dualmipi_chipid = -1,
	.log_type = log_type,
};

static bool tp_gesture_en = false;
void cust_mid_set_tp_gesture(bool en)
{
	tp_gesture_en = en;
}
EXPORT_SYMBOL(cust_mid_set_tp_gesture);

bool cust_mid_get_tp_gesture_state(void)
{
	return tp_gesture_en;
}
EXPORT_SYMBOL(cust_mid_get_tp_gesture_state);

static int md_vbus = 0;
void cust_mid_misc_set_vbus(int vbus)
{
	md_vbus = vbus;
}
EXPORT_SYMBOL(cust_mid_misc_set_vbus);

int cust_mid_misc_get_vbus(void)
{
	return md_vbus;
}
EXPORT_SYMBOL(cust_mid_misc_get_vbus);

static bool usb_connected = false;
void cust_mid_misc_set_usb_connect_state(bool state)
{
	usb_connected = state;
	printk("cust_mid_misc_set_usb_connect_state usb_connected=%d", usb_connected);
}
EXPORT_SYMBOL(cust_mid_misc_set_usb_connect_state);

bool cust_mid_misc_get_usb_connect_state(void)
{
	printk("cust_mid_misc_get_usb_connect_state usb_connected=%d", usb_connected);
	return usb_connected;
}
EXPORT_SYMBOL(cust_mid_misc_get_usb_connect_state);

int cust_mid_misc_get_boot_mode(void)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (is_init_done == false)
		return -ENODEV;

	if ((middle_misc.boot_mode != -1) && (middle_misc.boot_mode < UNKNOWN_BOOT)) {
		return middle_misc.boot_mode;
	}

	boot_node = of_parse_phandle(g_dev->of_node, "bootmode", 0);
	if (!boot_node)
		pr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			pr_err("%s: failed to get atag,boot\n", __func__);
		else {
			pr_info("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);

			middle_misc.boot_mode = tag->bootmode;
			return tag->bootmode;
		}
	}
	
	return 0;
}
EXPORT_SYMBOL(cust_mid_misc_get_boot_mode);

#if defined(WB_MIDMISC_DISP_SCREEN_SUPPORT) //Leo 20230315
static int DISP_Screen_size_init(void)
{
	struct device_node *pnode = NULL;

	middle_misc.disp_screen.width = 0;
	middle_misc.disp_screen.height = 0;

	pnode = of_find_compatible_node(NULL, NULL, "mediatek,touch");
	if (!pnode) {
			pr_err("%s :failed to get pnode\n",__func__);
			return -ENODEV;
	}

	of_property_read_u32(pnode,
		"drm_width", &middle_misc.disp_screen.width);
	of_property_read_u32(pnode,
		"drm_height", &middle_misc.disp_screen.height);
	pr_info("Leo resulution is %d %d",
		middle_misc.disp_screen.width,
		middle_misc.disp_screen.height);
	
	return 0;
}

unsigned int DISP_GetScreenHeight(void)
{
	return (middle_misc.disp_screen.height > 0 ? middle_misc.disp_screen.height : 800);
}
EXPORT_SYMBOL(DISP_GetScreenHeight);

unsigned int DISP_GetScreenWidth(void)
{
	return (middle_misc.disp_screen.width > 0 ? middle_misc.disp_screen.width : 800);
}
EXPORT_SYMBOL(DISP_GetScreenWidth);

void cust_midmisc_get_use_tp_name_from_lk(const char **name)
{
	int rc;
	struct device_node *lcd_node;

	lcd_node = of_find_node_by_path("/lcds/cust_lk_panel"); //only support it
	if (!lcd_node) {
		pr_err("Leo  could not find %s node\n", "cust_lk_panel");
	} else {
		rc = of_property_read_string(lcd_node, "cust,use_tp_name", (const char **)name);
		if (!rc) {
			pr_err("can't find cust,use_tp_name property\n");
		}
	}
}
EXPORT_SYMBOL(cust_midmisc_get_use_tp_name_from_lk);
#endif

//jnier add begin 20231221
#if IS_ENABLED(CONFIG_TCPC_FUSB302)
#if IS_ENABLED(CONFIG_WB_TD_CUST_SUPPORT)||IS_ENABLED(CONFIG_WB_PD_ALGO_SUPPORT)
void set_other_pd_reset_state(bool state)
{
	other_pd_reset = state;
}
EXPORT_SYMBOL(set_other_pd_reset_state);

bool get_other_pd_reset_state(void)
{
	return other_pd_reset;
}
EXPORT_SYMBOL(get_other_pd_reset_state);
#endif
#endif
//jnier add end 20231221

#if defined(WB_MIDMISC_BOOTCOMPLETED_SUPPORT) //Leo 20230324
enum mid_misc_boot {
	MIDMISC_V_BOOTOK = 1,
	MIDMISC_V_MAX,
};

static RAW_NOTIFIER_HEAD(boot_completed_chain);

static void boot_completed_notifiers(unsigned long val, void *v)
{
	static bool is_init = false;
	if (is_init == false) {
		raw_notifier_call_chain(&boot_completed_chain, val, v);
		is_init = true;
	}
}

int midmisc_register_boot_completed_notifier(struct notifier_block *nb)
{
	int err;
	err = raw_notifier_chain_register(&boot_completed_chain, nb);
	if(err) {
		pr_err("boot_completed_chain register error! \n");
		goto out;
	}
out:
	return err;
}
EXPORT_SYMBOL(midmisc_register_boot_completed_notifier);

static int midmisc_v_boot_parse(int param)
{
	pr_info("%s cmd:%d \n",__func__,param);
	
	switch (param) {
		case MIDMISC_V_BOOTOK:
			middle_misc.boot_completed = true;
			boot_completed_notifiers(BOOT_COMPLETED_CHAIN, NULL);
		break;
		default:
			pr_info("%s %d: error: cmd:%d no found!",__func__,__LINE__,param);
		break;
	}
	
	return 0;
}

static ssize_t
midmisc_v_boot_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[BOOTC_MAX_SIZE];
	size_t copy_size = cnt;

	if (cnt >= sizeof(buf))
		copy_size = BOOTC_MAX_SIZE - 1;

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;

	if (cnt == 1 && buf[0] == '1') {
		midmisc_v_boot_parse(1);
		return 1;
	}

	return cnt;
}

static int midmisc_v_boot_show(struct seq_file *m, void *v)
{
	seq_puts(m, "----------------------------------------\n");
	return 0;
}

static int midmisc_v_boot_open(struct inode *inode, struct file *file)
{
	return single_open(file, midmisc_v_boot_show, inode->i_private);
}

static const struct proc_ops midmisc_v_bootcompleted = {
	.proc_open = midmisc_v_boot_open,
	.proc_write = midmisc_v_boot_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init init_midmisc_v_bootc(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("midmisc_bootc", 0664, NULL, &midmisc_v_bootcompleted);
	if (!pe)
		return -ENOMEM;
	return 0;
}

int cust_mid_misc_is_boot_completed(void)
{
	return middle_misc.boot_completed;
}
EXPORT_SYMBOL(cust_mid_misc_is_boot_completed);
#endif


#if IS_ENABLED(CONFIG_WB_BOARD_ID_SUPPORT) //Leo 20230417
int cust_midmisc_get_board_id(void)
{
	struct device_node *pnode = NULL;

	if (middle_misc.board_id >= 0) 
		return middle_misc.board_id;

	pnode = of_find_compatible_node(NULL, NULL, "cust,mid_misc");
	if (!pnode) {
			pr_err("%s :failed to get pnode\n",__func__);
			return -ENODEV;
	}

	of_property_read_u32(pnode,
		"board_id", &middle_misc.board_id);
	
	return middle_misc.board_id;
}
EXPORT_SYMBOL(cust_midmisc_get_board_id);
#endif

#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
int cust_midmisc_get_dualmipi_chipid(void)
{
	struct device_node *pnode = NULL;

	if (middle_misc.dualmipi_chipid >= 0) 
		return middle_misc.dualmipi_chipid;

	pnode = of_find_compatible_node(NULL, NULL, "cust,mid_misc");
	if (!pnode) {
			pr_err("%s :failed to get pnode\n",__func__);
			return -ENODEV;
	}

	of_property_read_u32(pnode,
		"dualmipi_chipid", &middle_misc.dualmipi_chipid);
	
	return middle_misc.dualmipi_chipid;
}
EXPORT_SYMBOL(cust_midmisc_get_dualmipi_chipid);
#endif

bool cust_midmisc_get_log_en(int type)
{
	if ((type < 0) || (type > MIDMISC_LOG_TYPE_MAX)) {
		pr_info("erro type !\n");
		return 0;
	}

	return middle_misc.log_type[type].log_en;
}
EXPORT_SYMBOL(cust_midmisc_get_log_en);

char *cust_midmisc_get_lcm_name(void)
{
	struct device_node *lcm_name;
	struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;

	lcm_name = of_find_node_by_path("/chosen");
	if (lcm_name) {
		videolfb_tag = (struct tag_videolfb *)of_get_property(lcm_name,"atag,videolfb",(int *)&size);
		if (!videolfb_tag)
			pr_info("LQ >>> %s Invalid lcm name\n",__func__);

		pr_info("%s read lcm name %s\n",__func__,videolfb_tag->lcmname);
		return videolfb_tag->lcmname;
	} else {
		return NULL;
	}
}
EXPORT_SYMBOL(cust_midmisc_get_lcm_name);

//debug part
static ssize_t mid_misc_debug_show(struct device* cd,
					  struct device_attribute *attr, char* buf)
{
	int len = 0;
#if 1//defined(WB_MIDMISC_BOOTCOMPLETED_SUPPORT) //Leo 20230324
	len += snprintf(buf+len, PAGE_SIZE-len, "boot_mode:%d \r\n",
						cust_mid_misc_get_boot_mode());
#endif
#if defined(WB_MIDMISC_BOOTCOMPLETED_SUPPORT) //Leo 20230324
	len += snprintf(buf+len, PAGE_SIZE-len, "boot_completed:%d \r\n",
						cust_mid_misc_is_boot_completed());
#endif
#if defined(WB_MIDMISC_DISP_SCREEN_SUPPORT)
	len += snprintf(buf+len, PAGE_SIZE-len, "disp_screen:width:%d height:%d\r\n",
						DISP_GetScreenWidth(),DISP_GetScreenHeight());
	{
		const char *name = "None";
		cust_midmisc_get_use_tp_name_from_lk((const char **)&name);
		len += snprintf(buf+len, PAGE_SIZE-len, "use_tp_name_from_lk:%s \r\n",name);
	}
#endif

#if IS_ENABLED(CONFIG_WB_BOARD_ID_SUPPORT) //Leo 20230417
	len += snprintf(buf+len, PAGE_SIZE-len, "board_id:%d \r\n",
						cust_midmisc_get_board_id());
#endif
#if IS_ENABLED(CONFIG_IT6112_GM87553C_SUPPORT) || IS_ENABLED(CONFIG_IT6113_GM87553C_SUPPORT)
	{
		int chip_id = cust_midmisc_get_dualmipi_chipid();
		if ((chip_id > 0) && (chip_id < sizeof(dualmipi_ic_name) / sizeof (char *))) {
			len += snprintf(buf+len, PAGE_SIZE-len, "dualmipi_chipid:%d :%s \r\n",
								chip_id, dualmipi_ic_name[chip_id]);
		}
	}
#endif

	len += snprintf(buf+len, PAGE_SIZE-len, "lcm_name:%s \r\n",
						cust_midmisc_get_lcm_name());

	len += snprintf(buf+len, PAGE_SIZE-len, "cm_log_en:%d ts_log_en:%d\r\n",
				cust_midmisc_get_log_en(MIDMISC_CM_LOG),cust_midmisc_get_log_en(MIDMISC_TS_LOG));

	return len;
}

static ssize_t mid_misc_debug_store(struct device* cd,
				   struct device_attribute *attr,const char* buf, size_t len)
{

	unsigned int temp = 0;


	if (!strncmp(buf, "cm_log_en:", sizeof("cm_log_en:") - 1)) {
		if (sscanf(buf, "cm_log_en:%d\n", &temp) == 1)
			middle_misc.log_type[MIDMISC_CM_LOG].log_en= temp;
	} else if (!strncmp(buf, "ts_log_en:", sizeof("ts_log_en:") - 1)) {
		if (sscanf(buf, "ts_log_en:%d\n", &temp) == 1)
			middle_misc.log_type[MIDMISC_TS_LOG].log_en= temp;
	}

	return len;
}

static DEVICE_ATTR(mid_misc, 0664, mid_misc_debug_show, mid_misc_debug_store);

static struct attribute *mid_misc_attrs[] = {
	&dev_attr_mid_misc.attr,
	NULL,
};

static struct attribute_group mid_misc_group = {
	.attrs = mid_misc_attrs
};


static int misc_mid_probe(struct platform_device *pdev)
{
	int ret;

	g_dev = &pdev->dev;

	ret = sysfs_create_group(&pdev->dev.kobj, &mid_misc_group);
	if (ret < 0) {
		pr_err("[CUST][GPIO] unable to create mid_misc_group attribute file\n");
		return ret;
	}

	//middle_misc.version = 0x01;
	//middle_misc.log_type = log_type;

#if defined(WB_MIDMISC_DISP_SCREEN_SUPPORT) //Leo 20230315
	DISP_Screen_size_init();
#endif

#if defined(WB_MIDMISC_BOOTCOMPLETED_SUPPORT) //Leo 20230324
	ret = init_midmisc_v_bootc();
	if (ret < 0) {
		pr_err("unable init_midmisc_v_bootc !\n");
		return ret;
	}
#endif

#if defined(CONFIG_CM_NOTIFY_EVENT_SUPPORT) //Leo 20230324
	ret = init_midmisc_notify_event();
	if (ret < 0) {
		pr_err("unable init_midmisc_notify_event !\n");
		return ret;
	}
#endif


#if IS_ENABLED(CONFIG_MID_CSCI_SUPPORT) //Leo 20231010
	if (csci_exist("ro.cm_log_en")) {
		middle_misc.log_type[MIDMISC_CM_LOG].log_en = true;
	}

	if (csci_exist("ro.ts_log_en")) {
		middle_misc.log_type[MIDMISC_TS_LOG].log_en = true;
	}
#endif

	is_init_done = true;

//init interface
	middle_misc.boot_mode = cust_mid_misc_get_boot_mode();

	return 0;
}

static int misc_mid_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mid_misc_of_ids[] = {
	{.compatible = "cust,mid_misc",},
	{},
};
MODULE_DEVICE_TABLE(of, mid_misc_of_ids);
#endif

static struct platform_driver mid_misc_driver = {
	.driver = {
		.name = MID_MISC_DEVICE,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mid_misc_of_ids),
	},
	.probe = misc_mid_probe,
	.remove = misc_mid_remove,
};

static int __init _mid_misc_init(void)
{
	if (platform_driver_register(&mid_misc_driver) != 0) {
		pr_err("failed to register mid_misc_driver.\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit _mid_misc_exit(void)
{
	platform_driver_unregister(&mid_misc_driver);
}

module_init(_mid_misc_init);
module_exit(_mid_misc_exit);
MODULE_AUTHOR("<Leo.zhang@default.com>");
MODULE_LICENSE("GPL");
