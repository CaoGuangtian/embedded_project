#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#define DM_ICM20608_NAME "dm_icm20608"

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

struct dm_icm20608_sample {
	s16 accel_x;
	s16 accel_y;
	s16 accel_z;
	s16 temp;
	s16 gyro_x;
	s16 gyro_y;
	s16 gyro_z;
};

struct dm_icm20608_dev {
	struct spi_device *spi;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct mutex lock;
};

static struct dm_icm20608_dev *g_icm20608;

static int dm_icm20608_read_reg(struct spi_device *spi, u8 reg)
{
	u8 tx = reg | ICM20608_READ_FLAG;
	u8 rx = 0;
	int ret;

	ret = spi_write_then_read(spi, &tx, 1, &rx, 1);
	if (ret)
		return ret;

	return rx;
}

static int dm_icm20608_write_reg(struct spi_device *spi, u8 reg, u8 val)
{
	u8 tx[2] = { (u8)(reg & ~ICM20608_READ_FLAG), val };

	return spi_write(spi, tx, sizeof(tx));
}

static int dm_icm20608_read_bytes(struct spi_device *spi, u8 reg, u8 *buf,
				  size_t len)
{
	u8 tx = reg | ICM20608_READ_FLAG;

	return spi_write_then_read(spi, &tx, 1, buf, len);
}

static s16 dm_be16_to_s16(u8 high, u8 low)
{
	return (s16)(((u16)high << 8) | low);
}

static int dm_icm20608_hw_init(struct dm_icm20608_dev *icm)
{
	int id;
	int ret;

	mutex_lock(&icm->lock);

	ret = dm_icm20608_write_reg(icm->spi, ICM20608_PWR_MGMT_1, 0x80);
	if (ret)
		goto out;
	msleep(50);

	ret = dm_icm20608_write_reg(icm->spi, ICM20608_PWR_MGMT_1, 0x01);
	if (ret)
		goto out;
	msleep(10);

	ret = dm_icm20608_write_reg(icm->spi, ICM20608_PWR_MGMT_2, 0x00);
	if (ret)
		goto out;
	ret = dm_icm20608_write_reg(icm->spi, ICM20608_SMPLRT_DIV, 0x00);
	if (ret)
		goto out;
	ret = dm_icm20608_write_reg(icm->spi, ICM20608_CONFIG, 0x04);
	if (ret)
		goto out;
	ret = dm_icm20608_write_reg(icm->spi, ICM20608_GYRO_CONFIG, 0x18);
	if (ret)
		goto out;
	ret = dm_icm20608_write_reg(icm->spi, ICM20608_ACCEL_CONFIG, 0x18);
	if (ret)
		goto out;
	ret = dm_icm20608_write_reg(icm->spi, ICM20608_ACCEL_CONFIG2, 0x04);
	if (ret)
		goto out;

	id = dm_icm20608_read_reg(icm->spi, ICM20608_WHO_AM_I);
	if (id < 0) {
		ret = id;
		goto out;
	}

	dev_info(&icm->spi->dev, "WHO_AM_I=0x%02x\n", id);

out:
	mutex_unlock(&icm->lock);
	return ret;
}

static int dm_icm20608_read_sample(struct dm_icm20608_dev *icm,
				   struct dm_icm20608_sample *sample)
{
	u8 data[14];
	int ret;

	mutex_lock(&icm->lock);
	ret = dm_icm20608_read_bytes(icm->spi, ICM20608_ACCEL_XOUT_H,
				     data, sizeof(data));
	mutex_unlock(&icm->lock);

	if (ret)
		return ret;

	sample->accel_x = dm_be16_to_s16(data[0], data[1]);
	sample->accel_y = dm_be16_to_s16(data[2], data[3]);
	sample->accel_z = dm_be16_to_s16(data[4], data[5]);
	sample->temp = dm_be16_to_s16(data[6], data[7]);
	sample->gyro_x = dm_be16_to_s16(data[8], data[9]);
	sample->gyro_y = dm_be16_to_s16(data[10], data[11]);
	sample->gyro_z = dm_be16_to_s16(data[12], data[13]);

	return 0;
}

static int dm_icm20608_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_icm20608;
	return 0;
}

static ssize_t dm_icm20608_read(struct file *filp, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct dm_icm20608_dev *icm = filp->private_data;
	struct dm_icm20608_sample sample;
	int ret;

	if (count < sizeof(sample))
		return -EINVAL;

	ret = dm_icm20608_read_sample(icm, &sample);
	if (ret)
		return ret;

	if (copy_to_user(buf, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

static const struct file_operations dm_icm20608_fops = {
	.owner = THIS_MODULE,
	.open = dm_icm20608_open,
	.read = dm_icm20608_read,
	.llseek = no_llseek,
};

static int dm_icm20608_chrdev_init(struct dm_icm20608_dev *icm)
{
	int ret;

	ret = alloc_chrdev_region(&icm->devt, 0, 1, DM_ICM20608_NAME);
	if (ret)
		return ret;

	cdev_init(&icm->cdev, &dm_icm20608_fops);
	icm->cdev.owner = THIS_MODULE;

	ret = cdev_add(&icm->cdev, icm->devt, 1);
	if (ret)
		goto err_unregister;

	icm->class = class_create(THIS_MODULE, DM_ICM20608_NAME);
	if (IS_ERR(icm->class)) {
		ret = PTR_ERR(icm->class);
		goto err_cdev;
	}

	icm->device = device_create(icm->class, NULL, icm->devt, NULL,
				    DM_ICM20608_NAME);
	if (IS_ERR(icm->device)) {
		ret = PTR_ERR(icm->device);
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(icm->class);
err_cdev:
	cdev_del(&icm->cdev);
err_unregister:
	unregister_chrdev_region(icm->devt, 1);
	return ret;
}

static void dm_icm20608_chrdev_exit(struct dm_icm20608_dev *icm)
{
	device_destroy(icm->class, icm->devt);
	class_destroy(icm->class);
	cdev_del(&icm->cdev);
	unregister_chrdev_region(icm->devt, 1);
}

static int dm_icm20608_probe(struct spi_device *spi)
{
	struct dm_icm20608_dev *icm;
	int ret;

	if (g_icm20608)
		return -EBUSY;

	icm = devm_kzalloc(&spi->dev, sizeof(*icm), GFP_KERNEL);
	if (!icm)
		return -ENOMEM;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	if (!spi->max_speed_hz)
		spi->max_speed_hz = 8000000;

	ret = spi_setup(spi);
	if (ret)
		return ret;

	icm->spi = spi;
	mutex_init(&icm->lock);
	spi_set_drvdata(spi, icm);

	ret = dm_icm20608_hw_init(icm);
	if (ret)
		return ret;

	g_icm20608 = icm;

	ret = dm_icm20608_chrdev_init(icm);
	if (ret) {
		g_icm20608 = NULL;
		return ret;
	}

	dev_info(&spi->dev, "created /dev/%s\n", DM_ICM20608_NAME);
	return 0;
}

static int dm_icm20608_remove(struct spi_device *spi)
{
	struct dm_icm20608_dev *icm = spi_get_drvdata(spi);

	dm_icm20608_chrdev_exit(icm);
	g_icm20608 = NULL;
	return 0;
}

static const struct of_device_id dm_icm20608_of_match[] = {
	{ .compatible = "dm,icm20608" },
	{ .compatible = "alientek,icm20608" },
	{ }
};
MODULE_DEVICE_TABLE(of, dm_icm20608_of_match);

static const struct spi_device_id dm_icm20608_id[] = {
	{ "dm_icm20608", 0 },
	{ "icm20608", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, dm_icm20608_id);

static struct spi_driver dm_icm20608_driver = {
	.probe = dm_icm20608_probe,
	.remove = dm_icm20608_remove,
	.id_table = dm_icm20608_id,
	.driver = {
		.name = DM_ICM20608_NAME,
		.of_match_table = dm_icm20608_of_match,
	},
};

module_spi_driver(dm_icm20608_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("Data Monitor ICM20608 SPI character device driver");
