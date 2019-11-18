// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bits.h>
#include <linux/i2c.h>

#include "ti-sm-usb-dig.h"

/* (data size - start condition - address - ACK) / ACK after data byte */
#define TI_SM_USB_DIG_I2C_MAX_MSG ((TI_SM_USB_DIG_DATA_SIZE - 3) / 2)

#define DECLARE_I2C_PACKET(name)				\
	struct ti_sm_usb_dig_packet name = {			\
		.op = TI_SM_USB_DIG_OP_I2C,			\
		.payload = {					\
			.spi = {				\
				.chan = 0x01,			\
			},					\
		},						\
	}

/* add an command field to packet buffer */
static inline void ti_sm_usb_dig_i2c_cmd(struct ti_sm_usb_dig_packet *p,
					 u8 cmd)
{
	u8 pos = p->payload.spi.num;

	p->payload.i2c.data[pos] = cmd;
	p->payload.i2c.cmd_mask[pos / 8] |= BIT(7 - (pos % 8));
	p->payload.i2c.num++;
}

/* add an data field to packet buffer */
static inline void ti_sm_usb_dig_i2c_dat(struct ti_sm_usb_dig_packet *p,
					 u8 data)
{
	p->payload.i2c.data[p->payload.spi.num] = data;
	p->payload.i2c.num++;
}

/* terminate I2C transaction */
static int ti_sm_usb_dig_i2c_stop(struct ti_sm_usb_dig_priv *priv)
{
	DECLARE_I2C_PACKET(p);

	ti_sm_usb_dig_i2c_cmd(&p, TI_SM_USB_DIG_I2C_STOP);

	return ti_sm_usb_dig_xfer(priv, &p);
}

/* send I2C message */
static int ti_sm_usb_dig_i2c_msg(struct ti_sm_usb_dig_priv *priv,
				 struct i2c_msg *msg)
{
	DECLARE_I2C_PACKET(p);
	int x, y, rc;
	int addr = (msg->addr << 1) | ((msg->flags & I2C_M_RD) ? BIT(0) : 0);

	ti_sm_usb_dig_i2c_cmd(&p, TI_SM_USB_DIG_I2C_START);
	ti_sm_usb_dig_i2c_dat(&p, addr);

	ti_sm_usb_dig_i2c_cmd(&p, TI_SM_USB_DIG_I2C_ACKS);

	if (msg->flags & I2C_M_RD) {
		for (x=0; x<msg->len; x++) {
			ti_sm_usb_dig_i2c_dat(&p, 0xff);
			ti_sm_usb_dig_i2c_cmd(&p, TI_SM_USB_DIG_I2C_ACKM);
		}
	} else {
		for (x=0; x<msg->len; x++) {
			ti_sm_usb_dig_i2c_dat(&p, msg->buf[x]);
			ti_sm_usb_dig_i2c_cmd(&p, TI_SM_USB_DIG_I2C_ACKS);
		}
	}

	rc = ti_sm_usb_dig_xfer(priv, &p);
	if (rc)
		return rc;

	/*
	 * now we read in any data we got during read MSGs
	 * and check ACKS
	 */
	if (((u8 *)&p)[2])
		return -EPROTO;

	for (x=0, y=3; x<msg->len; x++, y += 2) {
		if (msg->flags & I2C_M_RD) {
			msg->buf[x] = ((u8 *)&p)[y];
		} else if (((u8 *)&p)[y + 1]) {
			return -EPROTO;
		}
	}

	return 0;
}

/* I2C transfer */
static int ti_sm_usb_dig_i2c_xfer(struct i2c_adapter *adapter,
				  struct i2c_msg *msgs,
				  int num)
{
	struct ti_sm_usb_dig_priv *priv = i2c_get_adapdata(adapter);
	int x, rc;

	for (x=0; x<num; x++) {
		rc = ti_sm_usb_dig_i2c_msg(priv, &msgs[x]);
		if (rc)
			goto stop;
	}

stop:
	rc = ti_sm_usb_dig_i2c_stop(priv);
	if (rc)
		return rc;

	return num;
}

static u32 ti_sm_usb_dig_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm ti_sm_usb_dig_i2c_algo = {
	.master_xfer	= ti_sm_usb_dig_i2c_xfer,
	.functionality	= ti_sm_usb_dig_i2c_func,
};

static const struct i2c_adapter_quirks ti_sm_usb_dig_i2c_quirks = {
	.max_read_len	= TI_SM_USB_DIG_I2C_MAX_MSG,
	.max_write_len	= TI_SM_USB_DIG_I2C_MAX_MSG,
};

int ti_sm_usb_dig_i2c_init(struct ti_sm_usb_dig_priv *priv)
{
	int rc;

	priv->i2c_adapter.quirks = &ti_sm_usb_dig_i2c_quirks;
	priv->i2c_adapter.owner = THIS_MODULE;
	priv->i2c_adapter.class = I2C_CLASS_HWMON;
	priv->i2c_adapter.algo = &ti_sm_usb_dig_i2c_algo;
	priv->i2c_adapter.dev.parent = priv->dev;
	priv->i2c_adapter.dev.of_node = priv->dev->of_node;

	strlcpy(priv->i2c_adapter.name, dev_name(priv->dev),
		sizeof(priv->i2c_adapter.name));

	rc = devm_i2c_add_adapter(priv->dev, &priv->i2c_adapter, priv);
	if (rc) {
		dev_err(priv->dev, "unable to add I2C adapter\n");
		return rc;
	}

	dev_info(priv->dev, "added I2C adapter\n");

	return 0;
}
