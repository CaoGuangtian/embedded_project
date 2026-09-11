#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>

#define AP3216C_SYS_CFG 0x00
#define AP3216C_IR_DATA_L 0x0a
#define AP3216C_ALS_DATA_L 0x0c
#define AP3216C_PS_DATA_L 0x0e

struct dm_ap3216c_state {
	struct i2c_client *client;
	struct mutex lock;
};

static int dm_ap3216c_read_raw(struct iio_dev *indio_dev,
				       const struct iio_chan_spec *chan,
				       int *val, int *val2, long mask)
{
	struct dm_ap3216c_state *st = iio_priv(indio_dev);
	int low = -EIO, high = -EIO, value = 0;

	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	mutex_lock(&st->lock);
	switch (chan->address) {
	case 0:
		low = i2c_smbus_read_byte_data(st->client, AP3216C_IR_DATA_L);
		high = i2c_smbus_read_byte_data(st->client, AP3216C_IR_DATA_L + 1);
		if (low >= 0 && high >= 0)
			value = (low & 0x80) ? 0 : ((high & 0x03) << 8) | low;
		break;
	case 1:
		low = i2c_smbus_read_byte_data(st->client, AP3216C_ALS_DATA_L);
		high = i2c_smbus_read_byte_data(st->client, AP3216C_ALS_DATA_L + 1);
		if (low >= 0 && high >= 0)
			value = (high << 8) | low;
		break;
	case 2:
		low = i2c_smbus_read_byte_data(st->client, AP3216C_PS_DATA_L);
		high = i2c_smbus_read_byte_data(st->client, AP3216C_PS_DATA_L + 1);
		if (low >= 0 && high >= 0)
			value = (low & 0x40) ? 0 : ((high & 0x3f) << 4) | (low & 0x0f);
		break;
	default:
		low = -EINVAL;
		break;
	}
	mutex_unlock(&st->lock);

	if (low < 0 || high < 0)
		return -EIO;
	*val = value;
	return IIO_VAL_INT;
}

static const struct iio_info dm_ap3216c_info = {
	.read_raw = dm_ap3216c_read_raw,
};

#define AP3216C_CHAN(_type, _addr) { \
	.type = (_type), .channel = 0, .address = (_addr), .indexed = 1, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.scan_type = { .sign = 'u', .realbits = 16, .storagebits = 16 } }

static const struct iio_chan_spec dm_ap3216c_channels[] = {
	AP3216C_CHAN(IIO_INTENSITY, 0),
	AP3216C_CHAN(IIO_LIGHT, 1),
	AP3216C_CHAN(IIO_PROXIMITY, 2),
};

static int dm_ap3216c_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct iio_dev *indio_dev;
	struct dm_ap3216c_state *st;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EOPNOTSUPP;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*st));
	if (!indio_dev)
		return -ENOMEM;

	st = iio_priv(indio_dev);
	st->client = client;
	mutex_init(&st->lock);
	i2c_set_clientdata(client, indio_dev);

	ret = i2c_smbus_write_byte_data(client, AP3216C_SYS_CFG, 0x04);
	if (ret < 0)
		return ret;
	msleep(20);
	ret = i2c_smbus_write_byte_data(client, AP3216C_SYS_CFG, 0x03);
	if (ret < 0)
		return ret;
	msleep(150);

	indio_dev->name = "dm-ap3216c";
	indio_dev->dev.parent = &client->dev;
	indio_dev->info = &dm_ap3216c_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = dm_ap3216c_channels;
	indio_dev->num_channels = ARRAY_SIZE(dm_ap3216c_channels);

	return iio_device_register(indio_dev);
}

static int dm_ap3216c_remove(struct i2c_client *client)
{
	iio_device_unregister(i2c_get_clientdata(client));
	return 0;
}

static const struct of_device_id dm_ap3216c_of_match[] = {
	{ .compatible = "dm,ap3216c" }, { }
};
MODULE_DEVICE_TABLE(of, dm_ap3216c_of_match);

static const struct i2c_device_id dm_ap3216c_id[] = {
	{ "dm_ap3216c", 0 }, { }
};
MODULE_DEVICE_TABLE(i2c, dm_ap3216c_id);

static struct i2c_driver dm_ap3216c_driver = {
	.probe = dm_ap3216c_probe,
	.remove = dm_ap3216c_remove,
	.id_table = dm_ap3216c_id,
	.driver = {
		.name = "dm_ap3216c_iio",
		.of_match_table = of_match_ptr(dm_ap3216c_of_match),
	},
};

module_i2c_driver(dm_ap3216c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("AP3216C IIO driver");
