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

#define FLAG_CLOCK_HIGH		0x00
#define FLAG_CLOCK_LOW		0xf0

#define FLAG_EDGE_BRE		0x01
#define FLAG_EDGE_AFE		0x02

struct spi_sm_usb_dig {
	struct ti_sm_usb_dig_priv *priv;
	u32 current_mode;
};

static int ti_sm_usb_dig_spi_xfer_one_xfer(struct spi_master *master,
					   struct spi_message *msg,
					   struct spi_transfer *xfer,
					   char chan, char cs)
{
	int rc;
	struct ti_sm_usb_dig_packet p = { 0 };
	struct packet_spi *packet = (struct packet_spi*)&p;
	struct spi_sm_usb_dig *spi_priv = spi_master_get_devdata(master);

	packet->op = TI_SM_USB_DIG_OP_SPI;

	/* compute xfer flags */
	packet->flags = 0;
	packet->chan = chan;

	if (spi_priv->current_mode & SPI_CPOL)
		packet->flags |= FLAG_CLOCK_LOW;
	else
		packet->flags |= FLAG_CLOCK_HIGH;

	if (spi_priv->current_mode & SPI_CPHA)
		packet->flags |= FLAG_EDGE_AFE;
	else
		packet->flags |= FLAG_EDGE_BRE;

	dev_info(spi_priv->priv->dev, "xfer_one: spi xfer flags: %02x\n",
		 packet->flags);

	dev_info(spi_priv->priv->dev, "xfer_one: chip_select=%d\n",
		 msg->spi->chip_select);

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
			dev_err(spi_priv->priv->dev, "RX len larger than bufsz %ld\n",
				sizeof(packet->data));
			return -ENOMEM;
		}

		packet->cmd_mask[0] = 0x80;
		packet->cmd_mask[1] = 0;
		packet->cmd_mask[2] = 0;

		// xfer size is 1 control byte plus xfer->len payload
		packet->num = xfer->len+1;

		memset(packet->data, 0xff, packet->num);

		// set CS = LOW (0x02 for HI)
		if (cs)
			packet->data[0] = 0x02;
		else
			packet->data[0] = 0x01;

		dev_info(spi_priv->priv->dev, "BUF before: %02x:%02x:%02x:%02x:%02x\n",
			 packet->data[0], packet->data[1],
			 packet->data[2], packet->data[3], packet->data[4]);

		rc = ti_sm_usb_dig_xfer(spi_priv->priv, &p);
		if (rc) {
			dev_err(spi_priv->priv->dev, "RX: sending SPI packet failed: %d\n", rc);
			return rc;
		}
		memcpy(xfer->rx_buf, &packet->data, xfer->len);
	}

	dev_info(spi_priv->priv->dev, "BUF after:  %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x::%02x:\n",
		 packet->data[0], packet->data[1], packet->data[2],
		 packet->data[3], packet->data[4], packet->data[5],
		 packet->data[6], packet->data[7], packet->data[9]);

	def_info(spi_priv->priv->dev, "OP=%02x CHAN=%02x Flags=%02x num=%02x\n",
		packet->op, packet->chan, packet->flags, packet->num);

	return 0;
}

static int ti_sm_usb_dig_spi_xfer_one_msg(struct spi_master *master,
					  struct spi_message *msg)
{
	struct spi_sm_usb_dig *spi_priv = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	int rc;

	msg->status = 0;
	msg->actual_length = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		// brute force all four modes with both channels
//		spi_priv->current_mode = SPI_MODE_0;

//		dev_info(&msg->spi->dev, "MODE0 CH0 CS0\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 0);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE0 CH1 CS0\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 0);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE0 CH0 CS1\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 1);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE0 CH1 CS1\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 1);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		spi_priv->current_mode = SPI_MODE_1;

		dev_info(&msg->spi->dev, "MODE1 CH0 CS0\n");
		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 0);
		dev_info(&msg->spi->dev, "result = %d\n", rc);

		dev_info(&msg->spi->dev, "MODE1 CH1 CS0\n");
		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 0);
		dev_info(&msg->spi->dev, "result = %d\n", rc);

		dev_info(&msg->spi->dev, "MODE1 CH0 CS1\n");
		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 1);
		dev_info(&msg->spi->dev, "result = %d\n", rc);

		dev_info(&msg->spi->dev, "MODE1 CH1 CS1\n");
		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 1);
		dev_info(&msg->spi->dev, "result = %d\n", rc);

//		spi_priv->current_mode = SPI_MODE_2;

//		dev_info(&msg->spi->dev, "MODE2 CH0 CS0\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 0);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE2 CH1 CS0\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 0);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE2 CH0 CS1\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 1);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE2 CH1 CS1\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 1);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		spi_priv->current_mode = SPI_MODE_3;

//		dev_info(&msg->spi->dev, "MODE3 CH0 CS0\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 0);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE3 CH1 CS0\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 0);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE3 CH0 CS1\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 0, 1);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

//		dev_info(&msg->spi->dev, "MODE3 CH1 CS1\n");
//		rc = ti_sm_usb_dig_spi_xfer_one_xfer(master, msg, xfer, 1, 1);
//		dev_info(&msg->spi->dev, "result = %ld\n", rc);

		if (rc) {
			dev_err(&msg->spi->dev, "xfer error: %d\n", rc);
			goto msg_done;
		}
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

	dev_info(spi_priv->priv->dev, "setup() mode: %d %s%s\n",
		spi->mode,
		(spi->mode & SPI_CPOL) ? "CPOL " : "",
		(spi->mode & SPI_CPHA) ? "CPHA" : "");

	return 0;
}

static void ti_sm_usb_dig_spi_cleanup(struct spi_device *spi)
{
	struct spi_sm_usb_dig *spi_priv =
		spi_master_get_devdata(spi->controller);

	dev_info(spi_priv->priv->dev, "cleanup()\n");
}

// FIXME: ugly hack
static struct spi_board_info dut_board_info[] = {
	{
		.modalias	= "ads1118",
		.max_speed_hz	= 48000000,
		.bus_num	= 1,
		.chip_select	= 0,
		.mode		= SPI_MODE_1,
	},
	{
		.modalias	= "ads1118",
		.max_speed_hz	= 48000000,
		.bus_num	= 1,
		.chip_select	= 1,
		.mode		= SPI_MODE_1,
	},
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

	master->num_chipselect = 1;
	master->mode_bits = SPI_CPHA | SPI_CPOL | SPI_3WIRE;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	master->flags = 0;
	master->dev.of_node = priv->dev->of_node;

	master->transfer_one_message = ti_sm_usb_dig_spi_xfer_one_msg;
	master->setup = ti_sm_usb_dig_spi_setup;
	master->cleanup = ti_sm_usb_dig_spi_cleanup;

	rc = devm_spi_register_master(priv->dev, master);
	if (rc < 0) {
		spi_master_put(master);
		dev_err(priv->dev, "failed registering spi master: %d\n", rc);
		return rc;
	}

	spi_device = spi_new_device(master, &dut_board_info[0]);
	if ((spi_device == NULL) || IS_ERR(spi_device)) {
		dev_err(priv->dev, "failed registering spi dut device CS0: %ld\n", PTR_ERR(spi_device));
	}

	spi_device = spi_new_device(master, &dut_board_info[1]);
	if ((spi_device == NULL) || IS_ERR(spi_device)) {
		dev_err(priv->dev, "failed registering spi dut device CS1: %ld\n", PTR_ERR(spi_device));
	}

	dev_info(priv->dev, "registered spi dut device\n");

	return 0;
}

// FIXME: missing cleanup (ungesister spi device, etc
