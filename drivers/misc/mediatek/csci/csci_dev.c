/***************************************************************************** 
** Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** Description: Infotm board configuration system.
**
** Author:
**     warits <warits.wang@infotmic.com.cn>
**      
** Revision History: 
** ----------------- 
** 1.1  04/09/2012
*****************************************************************************/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/of_reserved_mem.h>

#include <asm/io.h>
#include <asm/irq.h>
//#include <mach/hardware.h>
#include <mt-plat/csci.h>

#define CSCI_COMPATIBLE    "mediatek,csci_get_load_addr"

static void __iomem *csci_dtra = 0;
extern int read_list(char *buf, int len);
extern void update_string(struct imap_csci *t);
extern int csci_real_count;
extern struct imap_csci csci[CSCI_MAX_COUNT];
u64 csci_start;
u64 csci_size;


static int csci_map(struct device_node *np)
{
	int ret = 0;
	char *csci;
	unsigned long* va = NULL;
	struct device_node *csci_mnode;
	struct reserved_mem *csci_mem;

	csci_mnode = of_find_compatible_node(NULL, NULL, CSCI_COMPATIBLE);
	if (!csci_mnode) {
		pr_info("[csci] no node for reserved memory\n");
		ret = -1;
		goto use_csci_dev;
		//return -EINVAL;
	}

	csci_mem = of_reserved_mem_lookup(csci_mnode);
	if (!csci_mem) {
		pr_info("[csci] cannot lookup reserved memory, use device node cust,csci_dev\n");

		ret = -1;
		//return -EINVAL;
	} else {
		pr_info("[csci] phys:0x%llx - 0x%llx (0x%llx)\n",
			(unsigned long long)csci_mem->base,
			(unsigned long long)csci_mem->base + (unsigned long long)csci_mem->size,
			(unsigned long long)csci_mem->size);
		csci_start = csci_mem->base;
		csci_size = csci_mem->size;
	}

use_csci_dev:
	if (ret < 0) {
		ret = of_property_read_u64(np, "csci_start", &csci_start);
		if (ret) {
			pr_err("[%s]: of_property_read_u32 csci_start failed: %d\n",
					__func__, ret);
			return ret;
		}

		ret = of_property_read_u64(np, "csci_size", &csci_size);
		if (ret) {
			pr_err("[%s]: of_property_read_u32 csci_size failed: %d\n",
					__func__, ret);
			return ret;
		}
	}

	printk("csci: csci_map  csci_start = 0x%llx ,csci_size = 0x%llx\n",csci_start, csci_size);
	va =  ioremap((phys_addr_t)csci_start, csci_size);
	printk("csci va=0x%08lx pa=%0x\n",(long unsigned int)va,(u32)csci_start);

	csci = (char*)va;
	csci_init(csci,CSCI_SIZE_NORMAL);
	csci_list(NULL);
	return 0;
}

static ssize_t csci_show(struct device_driver *ddri, char *buf)
{
	int i;
	int len = 0;

	for (i = 0; i < csci_real_count; i++) {
		if (csci[i].key[0] != '\0') {
			len += snprintf(buf + len, PAGE_SIZE-len,"%s %s\n",
							csci[i].key, csci[i].string);
		}
	}

	return len;
}
static DRIVER_ATTR_RO(csci);

static long csci_dev_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)                       
{
	struct csci_query qt;
	int ret;
	unsigned long init_vals[2] = {0, 0};
	if(_IOC_TYPE(cmd) != CSCI_MAGIC
	   || _IOC_NR(cmd) > CSCI_IOCMAX)
	  return -ENOTTY;

	/* take the return value just to make gcc happy */
	if(cmd != CSCI_REINIT && cmd != CSCI_GETTR)
	    ret = copy_from_user(&qt, (struct csci_query *)arg,
		    sizeof(struct csci_query));

	switch(cmd)
	{
		case CSCI_EXIST:
			ret = csci_exist(qt.key);
			//printk(KERN_ERR "csci: exist: key=%s, ret=%d\n", qt.key, ret);
			return ret;
		case CSCI_INTEGER:
			ret = csci_integer(qt.key, qt.section);
			//printk(KERN_ERR "csci: integer: key=%s, ret=%d\n", qt.key, ret);
			return ret;
		case CSCI_EQUAL:
			ret = csci_equal(qt.key, qt.value, qt.section);
			//printk(KERN_ERR "csci: equal: key=%s, v=%s, sec=%d, ret=%d\n", qt.key, qt.value, qt.section, ret);
			return ret;
		case CSCI_CSCI:
			ret = csci_string_csci(qt.fb, qt.key, qt.section);
			//printk(KERN_ERR "csci: csci: fb=%s, key=%s, sec=%d, ret=%d\n", qt.fb, qt.key, qt.section, ret);
			if(ret) goto error;
			else break ;
		case CSCI_STRING:
			ret = csci_string(qt.fb, qt.key, qt.section);
			//printk(KERN_ERR "csci: string: fb=%s, key=%s, sec=%d, ret=%d\n", qt.fb, qt.key, qt.section, ret);
			if(ret) goto error;
			else break ;
		case CSCI_REINIT:
		//	ret = copy_from_user((char *)init_vals  arg , 8);
			ret = csci_init((char *)init_vals[0] , init_vals[1]);
			//printk(KERN_ERR "reinit csci at: 0x%8x, len:0x%x\n", init_vals[0], init_vals[1]);
			if(ret) goto error;
			break;
		case CSCI_GETTR:
			//copy_to_user((char *)arg, csci_dtra, 0x1000);
			return 0;
		default:
			goto error;
	}

	return copy_to_user((void *)arg, &qt, sizeof(struct csci_query));
error:
	return -EFAULT;
}

int csci_dev_open(struct inode *fi, struct file *fp)
{
//	printk(KERN_ERR "csci opened.\n");
    /* reset export pos */
    read_list(NULL, 0);
    return 0;
}

#if 0
static DEFINE_SPINLOCK(ddf_lock);
extern void __ddf(void *, void *, uint32_t, uint32_t);
#endif
static ssize_t csci_dev_read(struct file *filp,
   char __user *buf, size_t length, loff_t *offset)
{
#if 0
	unsigned long flags;
	void (*ddf)(void *, void *, uint32_t, uint32_t)
		= ioremap_nocache(IMAP_IRAM_BASE + 0x18000, 0x2000);

	printk(KERN_ERR "prepare function to GRAM: Gx%p, Vx%p\n", ddf, __ddf);
	memcpy((void *)ddf, (void *)__ddf, 0x2000);
	printk(KERN_ERR "reset DDR to 333M\n");

	spin_lock_init(&ddf_lock);

	spin_lock_irqsave(&ddf_lock, flags);
	__ddf((IMAP_EMIF_BASE), (SYSMGR_CLKGEN_BASE)
			+ 3 * 0x18, 0x21, 0x1036);
	spin_unlock_irqrestore(&ddf_lock, flags);

	return 0;
#endif
	return read_list(buf, length);
}

static ssize_t csci_dev_write(struct file *filp,
 const  char __user *buf, size_t length, loff_t *offset)
{
	struct imap_csci t;
	char src_buf[CSCI_MAX_LEN*2] = {0};

	if (copy_from_user(src_buf, buf, length)) {
		pr_err("%s copy_from_user failed \n");
		return -EFAULT;;
	}

	if ((sscanf(src_buf, "%s %s", t.key, t.string) == 2)) {
		update_string(&t);
	}

	return length;
}

static const struct file_operations csci_dev_fops = {
	.read = csci_dev_read,
	.write = csci_dev_write,
	.open = csci_dev_open,
	.unlocked_ioctl = csci_dev_ioctl,
};

#ifdef CONFIG_OF
static const struct of_device_id csci_of_ids[] = {
	{.compatible = "cust,csci_dev",},
	{},
};
#endif

static int csci_probe(struct platform_device *pdev)
{
	struct class *cls;
	int ret;

	printk(KERN_INFO "csci driver (c) 2009, 2014 InfoTM\n");

	/* create gps dev node */
	ret = csci_map(pdev->dev.of_node);
	if (ret < 0) {
		printk(KERN_ERR "csci_map failed.\n");
		return -EFAULT;
	}
	
	ret = register_chrdev(CSCI_MAJOR, CSCI_NAME, &csci_dev_fops);
	if(ret < 0)
	{
		printk(KERN_ERR "Register char device for csci failed.\n");
		return -EFAULT;
	}

	cls = class_create(THIS_MODULE, CSCI_NAME);
	if(!cls)
	{
		printk(KERN_ERR "Can not register class for csci .\n");
		return -EFAULT;
	}

	/* create device node */
	device_create(cls, NULL, MKDEV(CSCI_MAJOR, 0), NULL, CSCI_NAME);

	csci_dtra = kmalloc(0x1000, GFP_KERNEL);
	if(!csci_dtra)
		printk(KERN_ERR "Alloc csci_dtra buffer failed.\n");
	else {
	 //   void *__b = rbget("dramdtra");
	  //  if(__b) memcpy(csci_dtra, __b, 0x1000);
	}

	ret = driver_create_file(pdev->dev.driver, &driver_attr_csci);
	if (ret != 0) { 
		pr_info("driver_create_file (%s) = %d\n",
				driver_attr_csci.attr.name, ret);
	}

	return 0;
}

static int csci_remove(struct platform_device *pdev)   
{
	return 0;
}

static struct platform_driver csci_driver = {
	.driver = {
		.name = "csci_dev",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(csci_of_ids),
	},
	.probe = csci_probe,
	.remove = csci_remove,
};

static int __init csci_dev_init(void)
{

	if (platform_driver_register(&csci_driver)) {
		pr_err("unable to register csci_driver.\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit csci_dev_exit(void) {}

#if defined(CONFIG_MID_CSCI_SUPPORT) //Leo 20220511
rootfs_initcall(csci_dev_init);
#else
module_init(csci_dev_init);
#endif
module_exit(csci_dev_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("warits <warits.wang@infotmic.com.cn>");
MODULE_DESCRIPTION("csci interface device export");

