// SPDX-License-Identifier: GPL-2.0-or-later

#include "ti-sm-usb-dig.h"

/* retrieve controller version */
int ti_sm_usb_dig_version(struct ti_sm_usb_dig_priv *priv)
{
	struct ti_sm_usb_dig_packet p = { 0 };
	int ret;

	p.op = TI_SM_USB_DIG_OP_VERSION;

	ret = ti_sm_usb_dig_xfer(priv, &p);
	if (ret)
		return ret;

	priv->version_major = p.op;
	priv->version_minor = p.payload.raw[0];

	return 0;
}
