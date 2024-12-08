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



#define IT6151_EDP_SLAVE_ADDR_WRITE						(0x5C << 0)
#define	IT6151_MIPIRX_SLAVE_ADDR_WRITE 				(0x6C << 0)
#define IT6151_DRVNAME "IT6151"
//#define _uart_debug_

/*****************************************************************************
 *** STATIC FUNCTION
 *****************************************************************************/
void IT6151_IIC_Write_byte(u8 cmd, u8 data);
u8 IT6151_IIC_Read_byte(u8 cmd);


static struct i2c_client *it6151_mipirx = NULL;
static struct i2c_client *it6151_edp = NULL;

static const struct i2c_device_id it6151_i2c_id[] = 
{
    {"it6151_mipirx",	1},
	{"it6151_edp",		1},

};

int it6151_i2c_read_byte(u8 dev_addr, u8 addr, u8 *returnData)
{
    unsigned char pBuff;
	unsigned char puSendCmd[1];
	puSendCmd[0] = addr;

    if(dev_addr == IT6151_MIPIRX_SLAVE_ADDR_WRITE){
        if ( i2c_master_send(it6151_mipirx, puSendCmd, 1) < 0 ) {
            printk("it6151_i2c_read_byte 6c - send failed!!\n");
            return -1;
        }
    
        if ( i2c_master_recv(it6151_mipirx, &pBuff, 1) < 0 ) {
            printk("it6151_i2c_read_byte 6c - recv failed!!\n");
            return -1;
        }
    }else{
        if ( i2c_master_send(it6151_edp, puSendCmd, 1) < 0 ) {
            printk("it6151_i2c_read_byte 5c - send failed!!\n");
            return -1;
        }
    
        if ( i2c_master_recv(it6151_edp, &pBuff, 1) < 0 ) {
            printk("it6151_i2c_read_byte  - recv failed!!\n");
            return -1;
        }
    }    
	*returnData = pBuff;
    return 0;
	
}

int it6151_i2c_write_byte(u8 dev_addr, u8 addr, u8 writeData)
{
unsigned char write_buf[2] = {addr, writeData};

#if 1
  /* dump write_data for check */
	printk("[KE/it6151_i2c_write] dev_addr = 0x%x, write_data[0x%x] = 0x%x \n", dev_addr, addr, writeData);
#endif
    if(dev_addr == IT6151_MIPIRX_SLAVE_ADDR_WRITE){		
        if (i2c_master_send(it6151_mipirx, write_buf, 2) < 0 ) {
            printk("%s %d error \n",__func__,__LINE__);
            return -1;
        }    
    }else  if(dev_addr == IT6151_EDP_SLAVE_ADDR_WRITE){
        if (i2c_master_send(it6151_edp, write_buf, 2) < 0 ) {
            printk("%s %d error \n",__func__,__LINE__);
            return -1;
        }   
    }else{
        printk("it6151_i2c_write_byte i2c_transfer error\n");
        return 1;
    }	

    return 1;
}

static int match_id(const struct i2c_client *client, const struct i2c_device_id *id)
{
	if (strcmp(client->name, id->name) == 0)
		return true;
	else
		return false;
}

static int IT6151_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err=0; 

	printk("[it6151_i2c_driver_probe] start!\n");

	if(match_id(client, &it6151_i2c_id[0]))
	{
	  if (!(it6151_mipirx = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) 
		{
	    err = -ENOMEM;
	    goto exit;
	  }
		
	  memset(it6151_mipirx, 0, sizeof(struct i2c_client));

	  it6151_mipirx = client;    
	}
	else if(match_id(client, &it6151_i2c_id[1]))
	{
		if (!(it6151_edp = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) 
		{
			err = -ENOMEM;
			goto exit;
		}
		
		memset(it6151_edp, 0, sizeof(struct i2c_client));
	
		it6151_edp = client; 	 
	}
	else
	{
		printk("[it6151_i2c_driver_probe] error!\n");

		err = -EIO;
		goto exit;
	}

	printk("[it6151_i2c_driver_probe] %s i2c sucess!\n", client->name);
	
  return 0;

exit:
  return err;
}

static int IT6151_i2c_remove(struct i2c_client *client)
{
	return 0;
}


static const struct of_device_id IT6151_of_match[] = {
    {.compatible = "mediatek,it6151_edp"}, 
    {.compatible = "mediatek,it6151_mipirx"},
};

static struct i2c_driver IT6151_i2c_driver = {
    .probe = IT6151_i2c_probe,
    .remove = IT6151_i2c_remove,
	.driver = {
		   .name = IT6151_DRVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = IT6151_of_match,
#endif
		   },
    .id_table = it6151_i2c_id,
};

int _IT6151_driver_init(void)
{
        int ret;
        printk("IT6151_driver_init enter\n");
        ret=i2c_add_driver(&IT6151_i2c_driver);
        return ret; 
}

void _IT6151_driver_exit(void)
{
        printk("it6151exb_exit enter\n");
        i2c_del_driver(&IT6151_i2c_driver);
        return ;
}


/*----------------------------------------------------------------------------*/
//module_init(_IT6151_driver_init);
//module_exit(_IT6151_driver_exit);
/*----------------------------------------------------------------------------*/
//MODULE_DESCRIPTION("IT6151 Driver");
//MODULE_AUTHOR("Along");
//MODULE_LICENSE("GPL");