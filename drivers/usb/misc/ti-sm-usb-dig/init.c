// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/usb_ids.h>
#include <linux/spi/spi.h>

#include "ti-sm-usb-dig.h"

/* 2DO: GPIO --> not supported by firmware ?
        dut powering via regulator or PM ?
        1w driver
 */

DECLARE_MODULE_USB_TABLE(ti_sm_usb_dig_id_table,
	{ USB_DEVICE(USB_VENDOR_ID_TI, USB_DEVICE_ID_TI_SM_USB_DIG) });

int ti_sm_usb_dig_xfer(const struct ti_sm_usb_dig_priv *priv,
		       struct ti_sm_usb_dig_packet *p)
{
	struct device *dev = &priv->interface->dev;
	int rc = 0;
	int actual_length;
	void *buffer;

	buffer = kzalloc(TI_SM_USB_DIG_PACKET_SIZE, GFP_KERNEL);
	memcpy(buffer, p, TI_SM_USB_DIG_PACKET_SIZE);

	rc = usb_interrupt_msg(priv->usb_dev,
				usb_sndintpipe(priv->usb_dev,
						TI_SM_USB_DIG_ENDPOINT),
				buffer,
				TI_SM_USB_DIG_PACKET_SIZE,
				&actual_length,
				TI_SM_USB_DIG_TIMEOUT_MS);
	if (rc) {
		dev_err(dev, "USB TX transaction failed: %d\n", rc);
		goto out;
	}

	actual_length = 666;

	memset(buffer, 0, TI_SM_USB_DIG_PACKET_SIZE);
	rc = usb_interrupt_msg(priv->usb_dev,
				usb_rcvintpipe(priv->usb_dev,
						TI_SM_USB_DIG_ENDPOINT),
				buffer,
				TI_SM_USB_DIG_PACKET_SIZE,
				&actual_length,
				TI_SM_USB_DIG_TIMEOUT_MS);

	if (rc) {
		dev_err(dev, "USB RX transaction failed: %d\n", rc);
		goto out;
	}

	memcpy(p, buffer, TI_SM_USB_DIG_PACKET_SIZE);

out:
	kfree(buffer);
	return rc;
}

static int ti_sm_usb_dig_probe(struct usb_interface *interface,
			       const struct usb_device_id *usb_id)
{
	struct usb_host_interface *hostif = interface->cur_altsetting;
	struct ti_sm_usb_dig_priv *priv;
	struct device *dev = &interface->dev;
	int rc;

	if (hostif->desc.bInterfaceNumber != 0 ||
	    hostif->desc.bNumEndpoints < 2)
		return -ENODEV;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->usb_dev = usb_get_dev(interface_to_usbdev(interface));
	priv->interface = interface;
	priv->dev = dev;
	usb_set_intfdata(interface, priv);
	dev_set_drvdata(priv->dev, priv);

	/* probe the dongle version */
	rc = ti_sm_usb_dig_version(priv);
	if (rc)
		return rc;

	dev_info(priv->dev, "TI SM-USB-DIG Version: %d.%02d Found\n",
		 priv->version_major, priv->version_minor);

	/* register dut power LED driver */
	rc = ti_sm_usb_dig_dutpwr_init(priv);
	if (rc)
		return rc;

	/* switch on dut power */
	rc = ti_sm_usb_dig_dutpwr_set(priv, 1);
	if (rc)
		return rc;

	dev_info(priv->dev, "Switched on DUT power\n");

	rc = ti_sm_usb_dig_i2c_init(priv);
	if (rc)
		return rc;

	rc = ti_sm_usb_dig_spi_init(priv);
	if (rc)
		return rc;

	return 0;
}

static void ti_sm_usb_dig_disconnect(struct usb_interface *interface)
{
	struct device *dev = &interface->dev;
	dev_info(dev, "disconnect\n");
}

static struct usb_driver ti_sm_usb_dig_driver = {
	.name		= "ti-sm-usb-dig",
	.probe		= ti_sm_usb_dig_probe,
	.disconnect	= ti_sm_usb_dig_disconnect,
	.id_table	= ti_sm_usb_dig_id_table,
};
module_usb_driver(ti_sm_usb_dig_driver);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("TI SM-USB-DIG multi interface driver");
MODULE_LICENSE("GPL v2");
