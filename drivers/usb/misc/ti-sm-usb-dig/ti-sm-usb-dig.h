// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __TI_SM_USB_DIG_H
#define __TI_SM_USB_DIG_H

#include <linux/i2c.h>
#include <linux/leds.h>

#define USB_DEVICE_ID_TI_SM_USB_DIG	0x2f90

#define TI_SM_USB_DIG_ENDPOINT		1
#define TI_SM_USB_DIG_TIMEOUT_MS	1000
#define TI_SM_USB_DIG_PACKET_SIZE	32
#define TI_SM_USB_DIG_DATA_SIZE		(TI_SM_USB_DIG_PACKET_SIZE - 7)

#define TI_SM_USB_DIG_OP_SPI		0x01
#define TI_SM_USB_DIG_OP_I2C		0x02
#define TI_SM_USB_DIG_OP_1W		0x03
#define TI_SM_USB_DIG_OP_COMMAND	0x04
#define TI_SM_USB_DIG_OP_VECTOR1	0x05
#define TI_SM_USB_DIG_OP_VECTOR2	0x06
#define TI_SM_USB_DIG_OP_VERSION	0x07

#define TI_SM_USB_DIG_CMD_DUT_POWERON	0x01
#define TI_SM_USB_DIG_CMD_DUT_POWEROFF	0x02

#define TI_SM_USB_DIG_I2C_START		0x03
#define TI_SM_USB_DIG_I2C_STOP		0x04
#define TI_SM_USB_DIG_I2C_ACKM		0x05
#define TI_SM_USB_DIG_I2C_ACKS		0x06

struct ti_sm_usb_dig_priv {
	struct usb_device *usb_dev;
	struct usb_interface *interface;
	struct i2c_adapter i2c_adapter;
	struct led_classdev dutpwr;
	struct spi_master *spi_master;
	struct device *dev;
	u8 version_major;
	u8 version_minor;
};

struct ti_sm_usb_dig_packet {
	u8 op;			/* operation: I2C, SPI, ... */
	union {
		/* raw data */
		u8 raw[TI_SM_USB_DIG_PACKET_SIZE - 1];

		/* I2C */
		struct {
			u8 chan;		/* IO channel */
			u8 unused;
			u8 num;			/* number of cmd/data bytes */
			u8 cmd_mask[3];		/* bitmask of used data fields */
			u8 data[TI_SM_USB_DIG_PACKET_SIZE - 7];
		} __packed i2c;

		/* SPI */
		struct {
			u8 chan;		/* IO channel */
			u8 flags;
			u8 num;			/* number of cmd/data bytes */
			u8 cmd0;
			u8 cmd1;
			u8 cmd2;
			u8 data[TI_SM_USB_DIG_PACKET_SIZE - 6];
		} __packed spi;
	} payload;
} __packed;	/* padded to TI_SM_USB_DIG_PACKET_SIZE */

int ti_sm_usb_dig_xfer(const struct ti_sm_usb_dig_priv *priv,
		       struct ti_sm_usb_dig_packet *packet);

int ti_sm_usb_dig_version(struct ti_sm_usb_dig_priv *priv);

int ti_sm_usb_dig_cmd(const struct ti_sm_usb_dig_priv *priv, u8 cmd);
int ti_sm_usb_dig_dut_power(const struct ti_sm_usb_dig_priv *priv, int onoff);

int ti_sm_usb_dig_i2c_init(struct ti_sm_usb_dig_priv *priv);

int ti_sm_usb_dig_spi_init(struct ti_sm_usb_dig_priv *priv);
int ti_sm_usb_dig_spi_fini(struct ti_sm_usb_dig_priv *priv);

int ti_sm_usb_dig_dutpwr_init(struct ti_sm_usb_dig_priv *priv);
int ti_sm_usb_dig_dutpwr_set(const struct ti_sm_usb_dig_priv *priv, int onoff);

#endif /* __TI_SM_USB_DIG_H */
