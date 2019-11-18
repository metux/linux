// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bits.h>
#include <linux/spi/spi.h>

#include "ti-sm-usb-dig.h"

struct packet_spi {
	u8 op;
	u8 chan;		/* IO channel */
	u8 flags;
	u8 num;			/* number of cmd/data bytes */
	u8 cmd_mask[3];		/* cmd mask for bytes 0..7, 8..15, 16..23 */
	u8 data[TI_SM_USB_DIG_PACKET_SIZE - 7];
} __packed;

#define FLAG_CLOCK_HIGH		0xf0
#define FLAG_CLOCK_LOW		0x00

#define FLAG_EDGE_BRE		0x01
#define FLAG_EDGE_AFE		0x02

struct spi_sm_usb_dig {
	struct ti_sm_usb_dig_priv *priv;
	u32 current_mode;
};

static int ti_sm_usb_dig_spi_xfer_one_xfer(struct spi_master *master,
					   struct spi_message *msg,
					   struct spi_transfer *xfer)
{
	int rc;
	struct ti_sm_usb_dig_packet p = { 0 };
	struct packet_spi *packet = (struct packet_spi*)&p;
	struct spi_sm_usb_dig *spi_priv = spi_master_get_devdata(master);

	packet->op = TI_SM_USB_DIG_OP_SPI;

	/* compute xfer flags */
	packet->flags = 0;

	if (spi_priv->current_mode & SPI_CPOL) {
		dev_info(spi_priv->priv->dev, "SPI_CPOL on\n");
		packet->flags |= FLAG_CLOCK_HIGH;
	} else {
		dev_info(spi_priv->priv->dev, "SPI_CPOL off\n");
	}

	// FIXME: what to do with FLAG_EDGE_BRE and FLAG_EDGE_AFE ?
	if (spi_priv->current_mode & SPI_CPHA) {
		dev_info(spi_priv->priv->dev, "SPI_CPHA on\n");
		packet->flags |= FLAG_EDGE_BRE;
	} else {
		dev_info(spi_priv->priv->dev, "SPI_CPHA off\n");
		packet->flags |= FLAG_EDGE_AFE;
	}

	dev_info(spi_priv->priv->dev, "xfer_one: spi xfer flags: %d\n",
		 packet->flags);

	if (xfer->rx_buf && xfer->tx_buf) {
		dev_err(spi_priv->priv->dev, "cant do rx and tx in one xfer\n");
		return -EINVAL;
	}

	/* write */
	if (xfer->tx_buf) {
		dev_info(spi_priv->priv->dev, "TX len=%d\n", xfer->len);
		if (xfer->len > sizeof(packet->data)) {
			dev_err(spi_priv->priv->dev, "TX len larger than bufsz %ld\n",
				sizeof(packet->data));
			return -ENOMEM;
		}
		memcpy(&packet->data, xfer->tx_buf, xfer->len);
		packet->cmd_mask[0] = 0;
		packet->cmd_mask[1] = 0;
		packet->cmd_mask[2] = 0;
		packet->num = xfer->len;
		rc = ti_sm_usb_dig_xfer(spi_priv->priv, &p);
		if (rc) {
			dev_err(spi_priv->priv->dev, "TX: sending SPI packet failed: %d\n", rc);
			return rc;
		}
	}

	if (xfer->rx_buf) {
		dev_info(spi_priv->priv->dev, "RX len=%d\n", xfer->len);

		if (xfer->len > sizeof(packet->data)) {
			dev_err(spi_priv->priv->dev, "TX len larger than bufsz %ld\n",
				sizeof(packet->data));
			return -ENOMEM;
		}
		packet->cmd_mask[0] = 0xff;
		packet->cmd_mask[1] = 0xff;
		packet->cmd_mask[2] = 0xff;
		packet->num = xfer->len;
		rc = ti_sm_usb_dig_xfer(spi_priv->priv, &p);
		if (rc) {
			dev_err(spi_priv->priv->dev, "TX: sending SPI packet failed: %d\n", rc);
			return rc;
		}
		memcpy(xfer->rx_buf, &packet->data, xfer->len);
	}

	return 0;
}

// FIXME:
// add chipselect: cmd flat set, data=0x01 CS low, data=0x02 CS hi
// read: cmd flag set, data=0xff
static int ti_sm_usb_dig_spi_xfer_one_msg(struct spi_master *master,
					  struct spi_message *msg)
{
	struct spi_sm_usb_dig *spi_priv = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	int rc;

	if (msg->spi->master == master)
		dev_info(spi_priv->priv->dev, "masters equal\n");
	else
		dev_info(spi_priv->priv->dev, "masters differing\n");

	msg->status = 0;
	msg->actual_length = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
//		trace_spi_transfer_start(msg, xfer);

		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer);
		if (rc)
			goto msg_done;

//		trace_spi_transfer_stop(msg, xfer);
	}

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		dev_dbg(&msg->spi->dev,
			"  xfer %p: len %u tx %p/%pad rx %p/%pad\n",
			xfer, xfer->len,
			xfer->tx_buf, &xfer->tx_dma,
			xfer->rx_buf, &xfer->rx_dma);
	}

msg_done:
//	msg->status = as->done_status;
	spi_finalize_current_message(msg->spi->master);

	return rc;
}

static int ti_sm_usb_dig_spi_setup(struct spi_device *spi)
{
	struct spi_sm_usb_dig *spi_priv =
		spi_master_get_devdata(spi->controller);

	spi_priv->current_mode = spi->mode;

	dev_info(spi_priv->priv->dev, "setup() mode: %d\n", spi->mode);

	return 0;
}

static void ti_sm_usb_dig_spi_cleanup(struct spi_device *spi)
{
	struct spi_sm_usb_dig *spi_priv =
		spi_master_get_devdata(spi->controller);

	dev_info(spi_priv->priv->dev, "cleanup()\n");
}

// FIXME: usgly hack
static struct spi_board_info ads1118_board_info = {
	.modalias	= "ads1118",
	.max_speed_hz	= 48000000, //48 Mbps
	.bus_num	= 3,
	.chip_select	= 0,
	.mode		= SPI_MODE_1,
};

int ti_sm_usb_dig_spi_init(struct ti_sm_usb_dig_priv *priv)
{
	int rc;
	struct spi_sm_usb_dig *spi_priv;
	struct spi_master *master;
	struct spi_device *spi_device;

	master = spi_alloc_master(priv->dev, sizeof(*spi_priv));
	if (!master) {
		dev_err(priv->dev, "spi_alloc_master() failed\n");
		return -ENOMEM;
	}

	spi_priv = spi_master_get_devdata(master);
	spi_priv->priv = priv;

	master->num_chipselect = 16;
	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_3WIRE;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	master->flags = SPI_MASTER_HALF_DUPLEX;
	master->dev.of_node = priv->dev->of_node;

	master->transfer_one_message = ti_sm_usb_dig_spi_xfer_one_msg;
	master->setup = ti_sm_usb_dig_spi_setup;
	master->cleanup = ti_sm_usb_dig_spi_cleanup;

	rc = devm_spi_register_master(priv->dev, master);
	if (rc < 0) {
		spi_master_put(master);
		dev_err(priv->dev, "devm_spi_register_master() failed: %d\n",
			rc);
		return rc;
	}

	dev_info(priv->dev, "added SPI master\n");

	spi_device = spi_new_device(master, &ads1118_board_info);
	if ((spi_device == NULL) || IS_ERR(spi_device)) {
		dev_err(priv->dev, "failed registering ads1118 spi device: %ld\n", PTR_ERR(spi_device));
	}

	dev_info(priv->dev, "added SPI device: %pK\n", spi_device);

	if (spi_device->dev.driver == NULL) {
		dev_err(priv->dev, "spi device has no driver\n");
	} else {
		if (spi_device->dev.driver->owner == NULL) {
			dev_err(priv->dev, "module owner is NULL !\n");
		}
		else {
			dev_info(priv->dev, "module has owner\n");
	//		if (!try_module_get(spi_device->dev.driver->owner)) {
	//			dev_err(priv->dev, "failed getting driver module for ads1118\n");
	//		}
		}
	}

	dev_info(priv->dev, "registered ads1118 spi device\n");

	return 0;
}

// FIXME: missing cleanup (ungesister spi device, etc
