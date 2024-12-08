/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* The following software/firmware and/or related documentation ("MediaTek Software")
* have been modified by MediaTek Inc. All revisions are subject to any receiver\'s
* applicable license agreements with MediaTek Inc.
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
#include <mt-plat/csci.h>
/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/
#define DEVICE_NAME "cust_bias"
/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct i2c_client *cust_bias_client;

int cust_bias_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = cust_bias_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		printk("ERROR!! cust_bias_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	printk("bias-->reg :0x%02x  value:0x%02x  ret:%d\n",write_data[0],write_data[1],ret);
	if (ret < 0)
		pr_info("[LCM][ERROR] cust_bias write data fail !!\n");

	return ret;
}
EXPORT_SYMBOL(cust_bias_i2c_write_bytes);
/****************************************************************************/
static int cust_bias_i2c_driver_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	printk("[%s] start!\n", __func__);

	cust_bias_client = client;

	printk("[%s] %s i2c success!\n", __func__, client->name);

	return 0;
}

static const struct i2c_device_id cust_bias_i2c_id[] = {
	{"cust_bias",	0},
};

static const struct of_device_id cust_bias_of_match[] = {
	{.compatible = "cust,cust_bias"},
	{},
};

static struct i2c_driver cust_bias_i2c_driver = {
	  .driver = {
	      .name  = DEVICE_NAME,
	      .of_match_table = cust_bias_of_match,
	  },
	  .probe    = cust_bias_i2c_driver_probe,
	 .id_table  = cust_bias_i2c_id,
};

int cust_bias_init(void)
{
	printk("[%s] init start\n", __func__);

	if (i2c_add_driver(&cust_bias_i2c_driver) != 0)
		printk("[%s] Failed to register i2c driver.\n", __func__);
	else
		printk("[%s] Success to register i2c driver.\n", __func__);

	return 0;
}
EXPORT_SYMBOL(cust_bias_init);

void cust_bias_exit(void)
{
	i2c_del_driver(&cust_bias_i2c_driver);
}
EXPORT_SYMBOL(cust_bias_exit);

//module_init(cust_bias_init_dev);
//module_exit(cust_bias_exit);

MODULE_DESCRIPTION("I2C cust_bias Driver");
MODULE_AUTHOR("caozy");

