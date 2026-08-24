#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DM_AP3216C_NAME "dm_ap3216c"

#define AP3216C_SYS_CFG 0x00
#define AP3216C_IR_DATA_L 0x0a
#define AP3216C_IR_DATA_H 0x0b
#define AP3216C_ALS_DATA_L 0x0c
#define AP3216C_ALS_DATA_H 0x0d
#define AP3216C_PS_DATA_L 0x0e
#define AP3216C_PS_DATA_H 0x0f

struct dm_ap3216c_sample {
	u16 ir;
	u16 als;
	u16 ps;
};

struct dm_ap3216c_dev {
	struct i2c_client *client;
	struct miscdevice misc_dev;
	struct mutex lock;
};

static struct dm_ap3216c_dev *g_ap3216c;

static int dm_ap3216c_read_reg(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int dm_ap3216c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(client, reg, val);
}

static int dm_ap3216c_read_sample(struct dm_ap3216c_dev *ap,
				  struct dm_ap3216c_sample *sample)
{
	int ir_l, ir_h, als_l, als_h, ps_l, ps_h;

	mutex_lock(&ap->lock);

	ir_l = dm_ap3216c_read_reg(ap->client, AP3216C_IR_DATA_L);
	ir_h = dm_ap3216c_read_reg(ap->client, AP3216C_IR_DATA_H);
	als_l = dm_ap3216c_read_reg(ap->client, AP3216C_ALS_DATA_L);
	als_h = dm_ap3216c_read_reg(ap->client, AP3216C_ALS_DATA_H);
	ps_l = dm_ap3216c_read_reg(ap->client, AP3216C_PS_DATA_L);
	ps_h = dm_ap3216c_read_reg(ap->client, AP3216C_PS_DATA_H);

	mutex_unlock(&ap->lock);

	if (ir_l < 0 || ir_h < 0 || als_l < 0 || als_h < 0 ||
	    ps_l < 0 || ps_h < 0)
		return -EIO;

	if (ir_l & 0x80)
		sample->ir = 0;
	else
		sample->ir = ((ir_h & 0x03) << 8) | ir_l;

	sample->als = (als_h << 8) | als_l;

	if (ps_l & 0x40)
		sample->ps = 0;
	else
		sample->ps = ((ps_h & 0x3f) << 4) | (ps_l & 0x0f);

	return 0;
}

static int dm_ap3216c_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_ap3216c;
	return 0;
}

static ssize_t dm_ap3216c_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct dm_ap3216c_dev *ap = filp->private_data;
	struct dm_ap3216c_sample sample;
	int ret;

	if (count < sizeof(sample))
		return -EINVAL;

	ret = dm_ap3216c_read_sample(ap, &sample);
	if (ret)
		return ret;

	if (copy_to_user(buf, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

static const struct file_operations dm_ap3216c_fops = {
	.owner = THIS_MODULE,
	.open = dm_ap3216c_open,
	.read = dm_ap3216c_read,
	.llseek = no_llseek,
};

/* Sysfs Show Callbacks for direct Shell debugging */
static ssize_t show_ir(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dm_ap3216c_dev *ap = dev_get_drvdata(dev);
	struct dm_ap3216c_sample sample;

	if (!ap || dm_ap3216c_read_sample(ap, &sample) < 0)
		return sprintf(buf, "-1\n");

	return sprintf(buf, "%u\n", sample.ir);
}

static ssize_t show_als(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dm_ap3216c_dev *ap = dev_get_drvdata(dev);
	struct dm_ap3216c_sample sample;

	if (!ap || dm_ap3216c_read_sample(ap, &sample) < 0)
		return sprintf(buf, "-1\n");

	return sprintf(buf, "%u\n", sample.als);
}

static ssize_t show_ps(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dm_ap3216c_dev *ap = dev_get_drvdata(dev);
	struct dm_ap3216c_sample sample;

	if (!ap || dm_ap3216c_read_sample(ap, &sample) < 0)
		return sprintf(buf, "-1\n");

	return sprintf(buf, "%u\n", sample.ps);
}

static DEVICE_ATTR(ir, 0444, show_ir, NULL);
static DEVICE_ATTR(als, 0444, show_als, NULL);
static DEVICE_ATTR(ps, 0444, show_ps, NULL);

static struct attribute *dm_ap3216c_attrs[] = {
	&dev_attr_ir.attr,
	&dev_attr_als.attr,
	&dev_attr_ps.attr,
	NULL
};

ATTRIBUTE_GROUPS(dm_ap3216c);

static int dm_ap3216c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct dm_ap3216c_dev *ap;
	int ret;

	if (g_ap3216c)
		return -EBUSY;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EOPNOTSUPP;

	ap = devm_kzalloc(&client->dev, sizeof(*ap), GFP_KERNEL);
	if (!ap)
		return -ENOMEM;

	ap->client = client;
	mutex_init(&ap->lock);
	i2c_set_clientdata(client, ap);

	/* 1. Software reset sensor and wait for internal PLL logic stability */
	ret = dm_ap3216c_write_reg(client, AP3216C_SYS_CFG, 0x04);
	if (ret < 0)
		return ret;
	msleep(20);

	/* 2. Enable IR, ALS, and PS measurement mode */
	ret = dm_ap3216c_write_reg(client, AP3216C_SYS_CFG, 0x03);
	if (ret < 0)
		return ret;
	msleep(150);

	/* 3. Register as a miscdevice (automatic /dev/dm_ap3216c node creation) */
	ap->misc_dev.minor = MISC_DYNAMIC_MINOR;
	ap->misc_dev.name = DM_AP3216C_NAME;
	ap->misc_dev.fops = &dm_ap3216c_fops;
	ap->misc_dev.groups = dm_ap3216c_groups;
	ap->misc_dev.parent = &client->dev;

	ret = misc_register(&ap->misc_dev);
	if (ret)
		return ret;

	g_ap3216c = ap;

	dev_info(&client->dev, "created /dev/%s via miscdevice at addr 0x%02x\n",
		 DM_AP3216C_NAME, client->addr);
	return 0;
}

static int dm_ap3216c_remove(struct i2c_client *client)
{
	struct dm_ap3216c_dev *ap = i2c_get_clientdata(client);

	misc_deregister(&ap->misc_dev);
	g_ap3216c = NULL;
	return 0;
}

static const struct of_device_id dm_ap3216c_of_match[] = {
	{ .compatible = "dm,ap3216c" },
	{ .compatible = "ap3216c" },
	{ }
};
MODULE_DEVICE_TABLE(of, dm_ap3216c_of_match);

static const struct i2c_device_id dm_ap3216c_id[] = {
	{ "dm_ap3216c", 0 },
	{ "ap3216c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dm_ap3216c_id);

static struct i2c_driver dm_ap3216c_driver = {
	.probe = dm_ap3216c_probe,
	.remove = dm_ap3216c_remove,
	.id_table = dm_ap3216c_id,
	.driver = {
		.name = DM_AP3216C_NAME,
		.of_match_table = of_match_ptr(dm_ap3216c_of_match),
	},
};

module_i2c_driver(dm_ap3216c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("Data Monitor AP3216C I2C Driver (MiscDevice + Sysfs + SMBus)");
