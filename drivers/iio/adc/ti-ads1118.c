// SPDX-License-Identifier: GPL-2.0+
/*
 * ADS81118 16-bit 8-Channel ADC driver
 *
 * Author: Enrico Weigelt, metux IT consult <info@metux.net>
 *
 * Datasheet: http://www.ti.com/lit/ds/symlink/ads1118.pdf
 */

#include <linux/delay.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

/* OLD */
#define ADS1118_START BIT(7)
#define ADS1118_SINGLE_END BIT(2)
#define ADS1118_CHANNEL(channel) ((channel) << 4)
#define ADS1118_CLOCK_INTERNAL 0x2 /* PD1 = 1 and PD0 = 0 */

/* MTX */
#define CF_RESERVED	BIT(0)
#define CF_NOP0		BIT(1)
#define CF_NOP1		BIT(2)
#define CF_PULL_UP_EN	BIT(3)
#define CF_TS_MODE	BIT(4)
#define CF_DR0		BIT(5)
#define CF_DR1		BIT(6)
#define CF_DR2		BIT(7)
#define CF_MODE		BIT(8)
#define CF_PGA0		BIT(9)
#define CF_PGA1		BIT(10)
#define CF_PGA2		BIT(11)
#define CF_MUX0		BIT(12)
#define CF_MUX1		BIT(13)
#define CF_MUX2		BIT(14)
#define CF_SINGLESHOT	BIT(15)

/* Px --> AINP=AINx, Nx --> AINN=x, G ---> GND */
#define MUX_P0_N1	(           0           )
#define MUX_P0_N3	(   0   |   0   |CF_MUX0)
#define MUX_P1_N3	(   0   |CF_MUX1|   0   )
#define MUX_P2_N3	(   0   |CF_MUX1|CF_MUX0)
#define MUX_P0_NG	(CF_MUX2|   0   |   0   )
#define MUX_P1_NG	(CF_MUX2|   0   |CF_MUX0)
#define MUX_P2_NG	(CF_MUX2|CF_MUX1|   0   )
#define MUX_P3_NG	(CF_MUX2|CF_MUX1|CF_MUX0)

/* Programmable gain amplifier configuration: FSR is +/- x mV */
#define PGA_6144	(           0           )
#define PGA_4096	(                CF_PGA0)
#define PGA_2048	(        CF_PGA1        )
#define PGA_1024	(        CF_PGA1|CF_PGA0)
#define PGA_0512	(CF_PGA2                ) // for >= 101

// CPOL = 0 and CPHA = 1
// DRDY falling edge
// temperature sensor:	set TS_MODE bit = 1 in config register
// 			14 bit, left-justified with 16bit
//			MSB first, first 14 bits are temperature
// conversion needs to be enabled explicitly via config register
// MODE bit = 1 --> oneshot --> conversion when 1 written to SS bit

// 32bit transactions
// read: 	DATA-MSB,   DATA-LSB,   CONFIG-MSB, CONFIG-LSB
// write:	CONFIG-MSB, CONFIG-KSB, CONFIG-MSB, CONFIG-LSB

/* OLD */
struct ads1118 {
	struct spi_device *spi;
	struct regulator *reg;
	/*
	 * Lock protecting access to adc->tx_buff and rx_buff,
	 * especially from concurrent read on sysfs file.
	 */
	struct mutex lock;

	u8 tx_buf ____cacheline_aligned;
	u16 rx_buf;
	u8 rx_buffer[4];
};

#define ADS1118_VOLTAGE_CHANNEL(chan, si)				\
	{								\
		.type = IIO_VOLTAGE,					\
		.indexed = 1,						\
		.channel = chan,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	}

#define ADS1118_TEMP_CHANNEL(chan, si)					\
	{								\
		.type = IIO_TEMP,					\
		.indexed = 1,						\
		.channel = chan,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	}

#define ADS1118_VOLTAGE_CHANNEL_DIFF(chan1, chan2, si)			\
	{								\
		.type = IIO_VOLTAGE,					\
		.indexed = 1,						\
		.channel = (chan1),					\
		.channel2 = (chan2),					\
		.differential = 1,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	}

static const struct iio_chan_spec ads1118_channels[] = {
	ADS1118_VOLTAGE_CHANNEL(0, 0),
	ADS1118_VOLTAGE_CHANNEL(1, 4),
	ADS1118_VOLTAGE_CHANNEL(2, 1),
	ADS1118_VOLTAGE_CHANNEL(3, 5),
	ADS1118_VOLTAGE_CHANNEL(4, 2),
	ADS1118_VOLTAGE_CHANNEL(5, 6),
	ADS1118_VOLTAGE_CHANNEL(6, 3),
	ADS1118_VOLTAGE_CHANNEL(7, 7),
	ADS1118_VOLTAGE_CHANNEL_DIFF(0, 1, 8),
	ADS1118_VOLTAGE_CHANNEL_DIFF(2, 3, 9),
	ADS1118_VOLTAGE_CHANNEL_DIFF(4, 5, 10),
	ADS1118_VOLTAGE_CHANNEL_DIFF(6, 7, 11),
	ADS1118_VOLTAGE_CHANNEL_DIFF(1, 0, 12),
	ADS1118_VOLTAGE_CHANNEL_DIFF(3, 2, 13),
	ADS1118_VOLTAGE_CHANNEL_DIFF(5, 4, 14),
	ADS1118_VOLTAGE_CHANNEL_DIFF(7, 6, 15),
};

static int ads1118_adc_conversion(struct ads1118 *adc, int channel,
				  bool differential)
{
	struct spi_device *spi = adc->spi;
	int ret;

//	adc->tx_buf = ADS1118_START;
//	if (!differential)
//		adc->tx_buf |= ADS1118_SINGLE_END;
//	adc->tx_buf |= ADS1118_CHANNEL(channel);
//	adc->tx_buf |= ADS1118_CLOCK_INTERNAL;

//	ret = spi_write(spi, &adc->tx_buf, 1);
//	if (ret)
//		return ret;

	udelay(9);

//	ret = spi_read(spi, &adc->rx_buf, 2);
	ret = spi_read(spi, &adc->rx_buffer, 4);

	if (ret) {
		pr_err("ads1118: err=%d\n", ret);
		return ret;
	}

	pr_info("ads1118 received bytes: %d:%d:%d:%d\n",
		adc->rx_buffer[0],
		adc->rx_buffer[1],
		adc->rx_buffer[2],
		adc->rx_buffer[3]);

	return adc->rx_buf;
}

static int ads1118_read_raw(struct iio_dev *iio,
			    struct iio_chan_spec const *channel, int *value,
			    int *shift, long mask)
{
	struct ads1118 *adc = iio_priv(iio);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&adc->lock);
		*value = ads1118_adc_conversion(adc, channel->scan_index,
						channel->differential);
		mutex_unlock(&adc->lock);
		if (*value < 0)
			return *value;

		return IIO_VAL_INT;
//	case IIO_CHAN_INFO_SCALE:
//		*value = regulator_get_voltage(adc->reg);
//		if (*value < 0)
//			return *value;
//
//		/* convert regulator output voltage to mV */
//		*value /= 1000;
//		*shift = 16;
//
//		return IIO_VAL_FRACTIONAL_LOG2;
	default:
		return -EINVAL;
	}
}

static const struct iio_info ads1118_info = {
	.read_raw = ads1118_read_raw,
};

static int ads1118_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct ads1118 *adc;
	int ret;

	dev_info(&spi->dev, "ads1118 probing\n");

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*adc));
	if (!indio_dev) {
		dev_err(&spi->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	adc = iio_priv(indio_dev);
	adc->spi = spi;
	mutex_init(&adc->lock);

	indio_dev->name = dev_name(&spi->dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->dev.of_node = spi->dev.of_node;
	indio_dev->info = &ads1118_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = ads1118_channels;
	indio_dev->num_channels = ARRAY_SIZE(ads1118_channels);

//	adc->reg = devm_regulator_get(&spi->dev, "vref");
//	if (IS_ERR(adc->reg))
//		return PTR_ERR(adc->reg);
//
//	ret = regulator_enable(adc->reg);
//	if (ret)
//		return ret;

	spi_set_drvdata(spi, indio_dev);

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_info(&spi->dev, "failed to register iio device\n");
//		regulator_disable(adc->reg);
//		return ret;
	}

	/* read configuration register */

	dev_info(&spi->dev, "ads1118 initialized\n");

	return 0;
}

static int ads1118_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct ads1118 *adc = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
//	regulator_disable(adc->reg);

	return 0;
}

static const struct of_device_id ads1118_of_match[] = {
	{ .compatible = "ti,ads1118", },
	{}
};
MODULE_DEVICE_TABLE(of, ads1118_of_match);

static struct spi_driver ads1118_driver = {
	.driver = {
		.name = "ads1118",
		.of_match_table = ads1118_of_match,
		.owner = THIS_MODULE,
	},
	.probe = ads1118_probe,
	.remove = ads1118_remove,
};
module_spi_driver(ads1118_driver);

MODULE_ALIAS("spi:ads1118");
MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("ADS1118 driver");
MODULE_LICENSE("GPL");
