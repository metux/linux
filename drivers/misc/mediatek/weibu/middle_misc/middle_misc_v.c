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

#include "middle_misc_v.h"


#define MID_MISC_V_DEVICE		"mid_misc_v"
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#define WB_MIDMISC_BOOTCOMPLETED_SUPPORT
#define WB_MIDMISC_DISP_SCREEN_SUPPORT

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

struct middle_misc_t {
	int version;
	struct tag_bootmode bootmode;
	struct disp_screen_t disp_screen;
};

static bool is_init_done = false;
static struct device *g_dev = NULL;
static struct middle_misc_t middle_misc;

int cust_mid_misc_v_get_boot_mode(void)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (is_init_done == false)
		return -ENODEV;

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
			return tag->bootmode;
		}
	}
	
	return 0;
}
EXPORT_SYMBOL(cust_mid_misc_v_get_boot_mode);


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

unsigned int DISP_GetScreenHeight_v(void)
{
	return (middle_misc.disp_screen.height > 0 ? middle_misc.disp_screen.height : 800);
}
EXPORT_SYMBOL(DISP_GetScreenHeight_v);

unsigned int DISP_GetScreenWidth_v(void)
{
	return (middle_misc.disp_screen.width > 0 ? middle_misc.disp_screen.width : 800);
}
EXPORT_SYMBOL(DISP_GetScreenWidth_v);
#endif

#if defined(WB_MIDMISC_BOOTCOMPLETED_SUPPORT) //Leo 20230324
enum mid_misc_v_boot {
	MIDMISC_V_BOOTOK = 1,
	MIDMISC_V_MAX,
};

static RAW_NOTIFIER_HEAD(boot_completed_chain_v);

static void boot_completed_notifiers(unsigned long val, void *v)
{
	static bool is_init = false;
	if (is_init == false) {
		raw_notifier_call_chain(&boot_completed_chain_v, val, v);
		is_init = true;
	}
}

int midmisc_v_register_boot_completed_notifier(struct notifier_block *nb)
{
	int err;
	err = raw_notifier_chain_register(&boot_completed_chain_v, nb);
	if(err) {
		pr_err("boot_completed_chain register error! \n");
		goto out;
	}
out:
	return err;
}
EXPORT_SYMBOL(midmisc_v_register_boot_completed_notifier);

static int midmisc_v_boot_parse(int param)
{
	pr_info("%s cmd:%d \n",__func__,param);
	
	switch (param) {
		case MIDMISC_V_BOOTOK:
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

	pe = proc_create("midmisc_v_bootc", 0664, NULL, &midmisc_v_bootcompleted);
	if (!pe)
		return -ENOMEM;
	return 0;
}
#endif


//debug part
static ssize_t mid_misc_v_debug_show(struct device* cd,
                      struct device_attribute *attr, char* buf)
{
	int len = 0;
	len += snprintf(buf+len, PAGE_SIZE-len, "boot_mode:%d \r\n",
						cust_mid_misc_v_get_boot_mode());
#if defined(WB_MIDMISC_DISP_SCREEN_SUPPORT)
	len += snprintf(buf+len, PAGE_SIZE-len, "disp_screen:width:%d height:%d\r\n",
						DISP_GetScreenWidth_v(),DISP_GetScreenHeight_v());
#endif


	return len;
}

static ssize_t mid_misc_v_debug_store(struct device* cd,
                   struct device_attribute *attr,const char* buf, size_t len)
{
	unsigned int databuf[2];
	if (2 == sscanf(buf,"%d %d",&databuf[0], &databuf[1])) {
	}
	return len;
}

static DEVICE_ATTR(mid_misc_v, 0664, mid_misc_v_debug_show, mid_misc_v_debug_store);

static struct attribute *mid_misc_v_attrs[] = {
	&dev_attr_mid_misc_v.attr,
	NULL,
};

static struct attribute_group mid_misc_v_group = {
	.attrs = mid_misc_v_attrs
};


static int misc_mid_probe(struct platform_device *pdev)
{
	int ret;

	g_dev = &pdev->dev;

	ret = sysfs_create_group(&pdev->dev.kobj, &mid_misc_v_group);
	if (ret < 0) {
		pr_err("unable to create mid_misc_v_group attribute file\n");
		return ret;
	}
	middle_misc.version = 0x01;

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

	is_init_done = true;

	return 0;
}

static int misc_mid_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mid_misc_v_of_ids[] = {
	{.compatible = "cust,mid_misc_v",},
	{},
};
MODULE_DEVICE_TABLE(of, mid_misc_v_of_ids);
#endif

static struct platform_driver mid_misc_v_driver = {
	.driver = {
		.name = MID_MISC_V_DEVICE,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mid_misc_v_of_ids),
	},
	.probe = misc_mid_probe,
	.remove = misc_mid_remove,
};

static int __init _mid_misc_v_init(void)
{
	if (platform_driver_register(&mid_misc_v_driver) != 0) {
		pr_err("failed to register mid_misc_v_driver.\n");
		return -ENODEV;
	}

	return 0;
}


static void __exit _mid_misc_v_exit(void)
{
	platform_driver_unregister(&mid_misc_v_driver);
}

module_init(_mid_misc_v_init);
module_exit(_mid_misc_v_exit);
MODULE_AUTHOR("<Leo.zhang@default.com>");
MODULE_LICENSE("GPL");