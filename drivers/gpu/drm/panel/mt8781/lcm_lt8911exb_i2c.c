// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <linux/kobject.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>


#include "lcm_lt8911exb_i2c.h"

#define LT8911EXB_SLAVE_ADDR 		0x52
#define LT8911EXB_DRVNAME "LT8911EXB_COM"
//#define _uart_debug_

/*****************************************************************************
 *** STATIC FUNCTION
 *****************************************************************************/
void LT8911EXB_IIC_Write_byte(u8 cmd, u8 data);
u8 LT8911EXB_IIC_Read_byte(u8 cmd);

static struct i2c_client *g_LT8911EXB_i2cClient;

static int LT8911EXB_i2c_write(u8 cmd, u8 data)
{
    //int ret = 0;
	unsigned char write_buf[2] = {cmd, data};
	//printk("%s %d \n",__func__,__LINE__);

    if ( i2c_master_send(g_LT8911EXB_i2cClient, write_buf, 2) < 0 ) {
    //    printk("LT8911EXB_i2c_write send failed!!\n");
         	printk("%s %d error \n",__func__,__LINE__);
        return -1;
    }
    return 0;
}

static int LT8911EXB_i2c_read(u8 cmd, u8 *data)
{
    //int ret = 0;
    unsigned char pBuff;
	unsigned char puSendCmd[1];
	puSendCmd[0] = cmd;

 	//printk("%s %d \n",__func__,__LINE__);
	if ( i2c_master_send(g_LT8911EXB_i2cClient, puSendCmd, 1) < 0 ) {
		printk("LT8911EXB_i2c_read  - send failed!!\n");
		return -1;
	}

	if ( i2c_master_recv(g_LT8911EXB_i2cClient, &pBuff, 1) < 0 ) {
		printk("LT8911EXB_i2c_read  - recv failed!!\n");
		return -1;
	}
	*data = pBuff;
    return 0;
}


void LT8911EXB_IIC_Write_byte(u8 cmd, u8 data)
{
	LT8911EXB_i2c_write(cmd, data);
}

u8 LT8911EXB_IIC_Read_byte(u8 cmd)
{
	u8 data = 0;
	
	LT8911EXB_i2c_read(cmd, &data);
	
	return data;
}

static int LT8911EXB_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	client->addr = (LT8911EXB_SLAVE_ADDR>>1);  
	g_LT8911EXB_i2cClient = client;
	printk("[LT8911EXB_i2c_probe] client->addr=0x%x", client->addr);
	if (g_LT8911EXB_i2cClient->addr)
		printk("[LT8911EXB_i2c_probe] g_LT8911EXB_i2cClient->addr=0x%x", g_LT8911EXB_i2cClient->addr);
	return 0;
}

static int LT8911EXB_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id LT8911EXB_i2c_id[] = {{LT8911EXB_DRVNAME, 0}, {} };

static const struct of_device_id LT8911EXB_of_match[] = {
    {.compatible = "mediatek,lt8911exb_com"}, {},
};

static struct i2c_driver LT8911EXB_i2c_driver = {
    .probe = LT8911EXB_i2c_probe,
    .remove = LT8911EXB_i2c_remove,
	.driver = {
		   .name = LT8911EXB_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = LT8911EXB_of_match,
#endif
		   },
    .id_table = LT8911EXB_i2c_id,
};

int _LT8911EXB_driver_init(void)
{
        int ret;
        printk("LT8911EXB_driver_init enter\n");
        ret=i2c_add_driver(&LT8911EXB_i2c_driver);
        return ret; 
}

void _LT8911EXB_driver_exit(void)
{
        printk("lt8911exb_exit enter\n");
        i2c_del_driver(&LT8911EXB_i2c_driver);
        return ;
}


/*----------------------------------------------------------------------------*/
//module_init(_LT8911EXB_driver_init);
//module_exit(_LT8911EXB_driver_exit);
/*----------------------------------------------------------------------------*/
//MODULE_DESCRIPTION("LT8911EXB Driver");
//MODULE_AUTHOR("Along");
//MODULE_LICENSE("GPL");