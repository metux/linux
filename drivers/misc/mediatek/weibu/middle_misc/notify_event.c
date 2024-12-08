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

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

enum mid_ne_type {
	MM_NE_REMOVE_USB_KEYBOARD = 0,
	MM_NE_ADD_USB_KEYBOARD = 1,
	MIDMISC_V_MAX,
};

static RAW_NOTIFIER_HEAD(mm_notify_event_chain);

static void mm_notify_event_notifiers(unsigned long val, void *v)
{
	raw_notifier_call_chain(&mm_notify_event_chain, val, v);
}

int midmisc_register_notify_event_notifier(struct notifier_block *nb)
{
	int err;
	err = raw_notifier_chain_register(&mm_notify_event_chain, nb);
	if(err) {
		pr_err("notify_event_chain register error! \n");
		goto out;
	}
out:
	return err;
}
EXPORT_SYMBOL(midmisc_register_notify_event_notifier);

static int midmisc_notify_event_parse(int param)
{
	int val = param;
	pr_info("%s cmd:%d \n",__func__,param);
	
	switch (param) {
		case MM_NE_ADD_USB_KEYBOARD:
		case MM_NE_REMOVE_USB_KEYBOARD:
			mm_notify_event_notifiers(NOTIFY_EVENT_CHAIN, (void *)&val);
		break;
		default:
			pr_info("%s %d: error: cmd:%d no found!",__func__,__LINE__,param);
		break;
	}
	
	return 0;
}

static ssize_t
midmisc_notify_event_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[NE_MAX_SIZE];
	size_t copy_size = cnt;

	if (cnt >= sizeof(buf))
		copy_size = BOOTC_MAX_SIZE - 1;

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;
	
	pr_info("%s : %s \n", __func__, buf);

	if (cnt == 1 && buf[0] == '1') {
		midmisc_notify_event_parse(1);
	} else if (cnt == 1 && buf[0] == '0') {
		midmisc_notify_event_parse(0);
	}

	return cnt;
}

static int midmisc_notify_event_show(struct seq_file *m, void *v)
{
	seq_puts(m, "----------------------------------------\n");
	return 0;
}

static int midmisc_notify_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, midmisc_notify_event_show, inode->i_private);
}

static const struct proc_ops midmisc_notify_event_completed = {
	.proc_open = midmisc_notify_event_open,
	.proc_write = midmisc_notify_event_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

int init_midmisc_notify_event(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("mm_notify_event", 0664, NULL, &midmisc_notify_event_completed);
	if (!pe)
		return -ENOMEM;
	return 0;
}
