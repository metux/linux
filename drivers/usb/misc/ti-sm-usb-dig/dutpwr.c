// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/leds.h>

#include "ti-sm-usb-dig.h"

struct packet_cmd {
	u8 op;
	u8 cmd;
} __packed;

int ti_sm_usb_dig_dutpwr_set(const struct ti_sm_usb_dig_priv *priv,
			     int onoff)
{
	struct ti_sm_usb_dig_packet p = { 0 };

	struct packet_cmd *payload = (struct packet_cmd*)&p;

	payload->op = TI_SM_USB_DIG_OP_COMMAND;
	payload->cmd = (onoff ? TI_SM_USB_DIG_CMD_DUT_POWERON
			      : TI_SM_USB_DIG_CMD_DUT_POWEROFF);

	return ti_sm_usb_dig_xfer(priv, &p);
}

static void ti_sm_usb_dig_led_set(struct led_classdev *led_cdev,
				  enum led_brightness brightness)
{
	struct ti_sm_usb_dig_priv *priv;

	priv = container_of(led_cdev, struct ti_sm_usb_dig_priv, dutpwr);

	ti_sm_usb_dig_dutpwr_set(priv, brightness);
}

int ti_sm_usb_dig_dutpwr_init(struct ti_sm_usb_dig_priv *priv)
{
	int rc;

	priv->dutpwr.name = "sm-usb-dig::dut-power";
	priv->dutpwr.brightness_set = ti_sm_usb_dig_led_set;

	rc = devm_led_classdev_register(priv->dev, &priv->dutpwr);
	if (rc) {
		dev_err(priv->dev, "failed registering dutpwr LED: %d\n", rc);
		return rc;
	}

	dev_info(priv->dev, "registered dutpwr LED\n");

	return 0;
}
