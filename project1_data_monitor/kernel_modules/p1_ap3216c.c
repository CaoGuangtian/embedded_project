#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define P1_AP3216C_NAME "p1_ap3216c"

#define AP3216C_SYS_CFG 0x00
#define AP3216C_IR_DATA_L 0x0a
#define AP3216C_IR_DATA_H 0x0b
#define AP3216C_ALS_DATA_L 0x0c
#define AP3216C_ALS_DATA_H 0x0d
#define AP3216C_PS_DATA_L 0x0e
#define AP3216C_PS_DATA_H 0x0f

struct p1_ap3216c_sample {
	u16 ir;
	u16 als;
	u16 ps;
};

struct p1_ap3216c_dev {
	struct i2c_client *client;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct mutex lock;
};

static struct p1_ap3216c_dev *g_ap3216c;

static int p1_ap3216c_read_reg(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int p1_ap3216c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(client, reg, val);
}

static int p1_ap3216c_read_sample(struct p1_ap3216c_dev *ap,
				  struct p1_ap3216c_sample *sample)
{
	int ir_l, ir_h, als_l, als_h, ps_l, ps_h;

	mutex_lock(&ap->lock);

	ir_l = p1_ap3216c_read_reg(ap->client, AP3216C_IR_DATA_L);
	ir_h = p1_ap3216c_read_reg(ap->client, AP3216C_IR_DATA_H);
	als_l = p1_ap3216c_read_reg(ap->client, AP3216C_ALS_DATA_L);
	als_h = p1_ap3216c_read_reg(ap->client, AP3216C_ALS_DATA_H);
	ps_l = p1_ap3216c_read_reg(ap->client, AP3216C_PS_DATA_L);
	ps_h = p1_ap3216c_read_reg(ap->client, AP3216C_PS_DATA_H);

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

static int p1_ap3216c_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_ap3216c;
	return 0;
}

static ssize_t p1_ap3216c_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct p1_ap3216c_dev *ap = filp->private_data;
	struct p1_ap3216c_sample sample;
	int ret;

	if (count < sizeof(sample))
		return -EINVAL;

	ret = p1_ap3216c_read_sample(ap, &sample);
	if (ret)
		return ret;

	if (copy_to_user(buf, &sample, sizeof(sample)))
		return -EFAULT;

	return sizeof(sample);
}

static const struct file_operations p1_ap3216c_fops = {
	.owner = THIS_MODULE,
	.open = p1_ap3216c_open,
	.read = p1_ap3216c_read,
	.llseek = no_llseek,
};

static int p1_ap3216c_chrdev_init(struct p1_ap3216c_dev *ap)
{
	int ret;

	ret = alloc_chrdev_region(&ap->devt, 0, 1, P1_AP3216C_NAME);
	if (ret)
		return ret;

	cdev_init(&ap->cdev, &p1_ap3216c_fops);
	ap->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ap->cdev, ap->devt, 1);
	if (ret)
		goto err_unregister;

	ap->class = class_create(THIS_MODULE, P1_AP3216C_NAME);
	if (IS_ERR(ap->class)) {
		ret = PTR_ERR(ap->class);
		goto err_cdev;
	}

	ap->device = device_create(ap->class, NULL, ap->devt, NULL,
				   P1_AP3216C_NAME);
	if (IS_ERR(ap->device)) {
		ret = PTR_ERR(ap->device);
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(ap->class);
err_cdev:
	cdev_del(&ap->cdev);
err_unregister:
	unregister_chrdev_region(ap->devt, 1);
	return ret;
}

static void p1_ap3216c_chrdev_exit(struct p1_ap3216c_dev *ap)
{
	device_destroy(ap->class, ap->devt);
	class_destroy(ap->class);
	cdev_del(&ap->cdev);
	unregister_chrdev_region(ap->devt, 1);
}

static int p1_ap3216c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct p1_ap3216c_dev *ap;
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

	ret = p1_ap3216c_write_reg(client, AP3216C_SYS_CFG, 0x04);
	if (ret < 0)
		return ret;
	msleep(20);

	ret = p1_ap3216c_write_reg(client, AP3216C_SYS_CFG, 0x03);
	if (ret < 0)
		return ret;
	msleep(150);

	g_ap3216c = ap;

	ret = p1_ap3216c_chrdev_init(ap);
	if (ret) {
		g_ap3216c = NULL;
		return ret;
	}

	dev_info(&client->dev, "created /dev/%s at addr 0x%02x\n",
		 P1_AP3216C_NAME, client->addr);
	return 0;
}

static int p1_ap3216c_remove(struct i2c_client *client)
{
	struct p1_ap3216c_dev *ap = i2c_get_clientdata(client);

	p1_ap3216c_chrdev_exit(ap);
	g_ap3216c = NULL;
	return 0;
}

static const struct of_device_id p1_ap3216c_of_match[] = {
	{ .compatible = "p1,ap3216c" },
	{ .compatible = "ap3216c" },
	{ }
};
MODULE_DEVICE_TABLE(of, p1_ap3216c_of_match);

static const struct i2c_device_id p1_ap3216c_id[] = {
	{ "p1_ap3216c", 0 },
	{ "ap3216c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, p1_ap3216c_id);

static struct i2c_driver p1_ap3216c_driver = {
	.probe = p1_ap3216c_probe,
	.remove = p1_ap3216c_remove,
	.id_table = p1_ap3216c_id,
	.driver = {
		.name = P1_AP3216C_NAME,
		.of_match_table = p1_ap3216c_of_match,
	},
};

module_i2c_driver(p1_ap3216c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Project1");
MODULE_DESCRIPTION("Project1 AP3216C I2C character device driver");

