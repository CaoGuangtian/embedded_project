#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#define ICM20608_WHO_AM_I 0x75
#define ICM20608_PWR_MGMT_1 0x6b
#define ICM20608_PWR_MGMT_2 0x6c
#define ICM20608_SMPLRT_DIV 0x19
#define ICM20608_CONFIG 0x1a
#define ICM20608_GYRO_CONFIG 0x1b
#define ICM20608_ACCEL_CONFIG 0x1c
#define ICM20608_ACCEL_CONFIG2 0x1d
#define ICM20608_ACCEL_XOUT_H 0x3b
#define ICM20608_READ_FLAG 0x80

struct dm_icm20608_state {
	struct spi_device *spi;
	struct mutex lock;
};

static int dm_icm20608_read_sample(struct dm_icm20608_state *st, u8 *data)
{
	u8 tx = ICM20608_ACCEL_XOUT_H | ICM20608_READ_FLAG;
	return spi_write_then_read(st->spi, &tx, 1, data, 14);
}

static int dm_icm20608_read_raw(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					int *val, int *val2, long mask)
{
	struct dm_icm20608_state *st = iio_priv(indio_dev);
	u8 data[14];
	int ret, index;

	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	mutex_lock(&st->lock);
	ret = dm_icm20608_read_sample(st, data);
	mutex_unlock(&st->lock);
	if (ret)
		return ret;

	index = chan->address * 2;
	*val = (s16)(((u16)data[index] << 8) | data[index + 1]);
	return IIO_VAL_INT;
}

static const struct iio_info dm_icm20608_info = {
	.read_raw = dm_icm20608_read_raw,
};

#define ICM_AXIS_CHAN(_type, _mod, _addr) { \
	.type = (_type), .modified = 1, .channel2 = (_mod), \
	.channel = 0, .address = (_addr), .indexed = 1, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.scan_type = { .sign = 's', .realbits = 16, .storagebits = 16 } }

#define ICM_TEMP_CHAN(_addr) { \
	.type = IIO_TEMP, .channel = 0, .address = (_addr), .indexed = 1, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.scan_type = { .sign = 's', .realbits = 16, .storagebits = 16 } }

static const struct iio_chan_spec dm_icm20608_channels[] = {
	ICM_AXIS_CHAN(IIO_ACCEL, IIO_MOD_X, 0),
	ICM_AXIS_CHAN(IIO_ACCEL, IIO_MOD_Y, 1),
	ICM_AXIS_CHAN(IIO_ACCEL, IIO_MOD_Z, 2),
	ICM_TEMP_CHAN(3),
	ICM_AXIS_CHAN(IIO_ANGL_VEL, IIO_MOD_X, 4),
	ICM_AXIS_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, 5),
	ICM_AXIS_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, 6),
};

static int dm_icm20608_write_reg(struct spi_device *spi, u8 reg, u8 value)
{
	u8 tx[2] = { reg & ~ICM20608_READ_FLAG, value };
	return spi_write(spi, tx, sizeof(tx));
}

static int dm_icm20608_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct dm_icm20608_state *st;
	int ret;
	u8 id;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (!indio_dev)
		return -ENOMEM;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	if (!spi->max_speed_hz)
		spi->max_speed_hz = 8000000;
	ret = spi_setup(spi);
	if (ret)
		return ret;

	st = iio_priv(indio_dev);
	st->spi = spi;
	mutex_init(&st->lock);
	spi_set_drvdata(spi, indio_dev);

	ret = dm_icm20608_write_reg(spi, ICM20608_PWR_MGMT_1, 0x80);
	if (ret)
		return ret;
	msleep(50);
	ret = dm_icm20608_write_reg(spi, ICM20608_PWR_MGMT_1, 0x01);
	if (ret)
		return ret;
	ret = dm_icm20608_write_reg(spi, ICM20608_PWR_MGMT_2, 0x00);
	if (ret)
		return ret;
	ret = dm_icm20608_write_reg(spi, ICM20608_SMPLRT_DIV, 0x00);
	if (ret)
		return ret;
	ret = dm_icm20608_write_reg(spi, ICM20608_CONFIG, 0x04);
	if (ret)
		return ret;
	ret = dm_icm20608_write_reg(spi, ICM20608_GYRO_CONFIG, 0x18);
	if (ret)
		return ret;
	ret = dm_icm20608_write_reg(spi, ICM20608_ACCEL_CONFIG, 0x18);
	if (ret)
		return ret;
	ret = dm_icm20608_write_reg(spi, ICM20608_ACCEL_CONFIG2, 0x04);
	if (ret)
		return ret;

	{
		u8 tx = ICM20608_WHO_AM_I | ICM20608_READ_FLAG;
		ret = spi_write_then_read(spi, &tx, 1, &id, 1);
	}
	if (ret)
		return ret;
	dev_info(&spi->dev, "WHO_AM_I=0x%02x\n", id);

	indio_dev->name = "dm-icm20608";
	indio_dev->dev.parent = &spi->dev;
	indio_dev->info = &dm_icm20608_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = dm_icm20608_channels;
	indio_dev->num_channels = ARRAY_SIZE(dm_icm20608_channels);

	return iio_device_register(indio_dev);
}

static int dm_icm20608_remove(struct spi_device *spi)
{
	iio_device_unregister(spi_get_drvdata(spi));
	return 0;
}

static const struct of_device_id dm_icm20608_of_match[] = {
	{ .compatible = "dm,icm20608" }, { }
};
MODULE_DEVICE_TABLE(of, dm_icm20608_of_match);

static const struct spi_device_id dm_icm20608_id[] = {
	{ "dm_icm20608", 0 }, { }
};
MODULE_DEVICE_TABLE(spi, dm_icm20608_id);

static struct spi_driver dm_icm20608_driver = {
	.probe = dm_icm20608_probe,
	.remove = dm_icm20608_remove,
	.id_table = dm_icm20608_id,
	.driver = {
		.name = "dm_icm20608_iio",
		.of_match_table = dm_icm20608_of_match,
	},
};

module_spi_driver(dm_icm20608_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("ICM20608 IIO driver");
