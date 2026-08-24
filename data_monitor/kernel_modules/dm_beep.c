#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DM_BEEP_NAME "dm_beep"

struct dm_beep_dev {
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int gpio;
	bool active_low;
	bool state;
};

static struct dm_beep_dev *g_beep;

static void dm_beep_set(struct dm_beep_dev *beep, bool on)
{
	gpio_set_value(beep->gpio, beep->active_low ? !on : on);
	beep->state = on;
}

static int dm_beep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_beep;
	return 0;
}

static ssize_t dm_beep_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct dm_beep_dev *beep = filp->private_data;
	char val;

	if (*ppos)
		return 0;

	val = beep->state ? '1' : '0';
	if (copy_to_user(buf, &val, 1))
		return -EFAULT;

	*ppos = 1;
	return 1;
}

static ssize_t dm_beep_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct dm_beep_dev *beep = filp->private_data;
	char val;

	if (count < 1)
		return -EINVAL;
	if (copy_from_user(&val, buf, 1))
		return -EFAULT;

	if (val == '1')
		dm_beep_set(beep, true);
	else if (val == '0')
		dm_beep_set(beep, false);
	else
		return -EINVAL;

	return count;
}

static const struct file_operations dm_beep_fops = {
	.owner = THIS_MODULE,
	.open = dm_beep_open,
	.read = dm_beep_read,
	.write = dm_beep_write,
	.llseek = no_llseek,
};

static int dm_beep_chrdev_init(struct dm_beep_dev *beep)
{
	int ret;

	ret = alloc_chrdev_region(&beep->devt, 0, 1, DM_BEEP_NAME);
	if (ret)
		return ret;

	cdev_init(&beep->cdev, &dm_beep_fops);
	beep->cdev.owner = THIS_MODULE;

	ret = cdev_add(&beep->cdev, beep->devt, 1);
	if (ret)
		goto err_unregister;

	beep->class = class_create(THIS_MODULE, DM_BEEP_NAME);
	if (IS_ERR(beep->class)) {
		ret = PTR_ERR(beep->class);
		goto err_cdev;
	}

	beep->device = device_create(beep->class, NULL, beep->devt, NULL,
				     DM_BEEP_NAME);
	if (IS_ERR(beep->device)) {
		ret = PTR_ERR(beep->device);
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(beep->class);
err_cdev:
	cdev_del(&beep->cdev);
err_unregister:
	unregister_chrdev_region(beep->devt, 1);
	return ret;
}

static void dm_beep_chrdev_exit(struct dm_beep_dev *beep)
{
	device_destroy(beep->class, beep->devt);
	class_destroy(beep->class);
	cdev_del(&beep->cdev);
	unregister_chrdev_region(beep->devt, 1);
}

static int dm_beep_probe(struct platform_device *pdev)
{
	struct dm_beep_dev *beep;
	enum of_gpio_flags flags;
	int gpio;
	int ret;

	if (g_beep)
		return -EBUSY;

	gpio = of_get_named_gpio_flags(pdev->dev.of_node, "beep-gpio", 0,
				       &flags);
	if (!gpio_is_valid(gpio))
		return gpio < 0 ? gpio : -EINVAL;

	beep = devm_kzalloc(&pdev->dev, sizeof(*beep), GFP_KERNEL);
	if (!beep)
		return -ENOMEM;

	beep->gpio = gpio;
	beep->active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = devm_gpio_request(&pdev->dev, beep->gpio, DM_BEEP_NAME);
	if (ret)
		return ret;

	ret = gpio_direction_output(beep->gpio, beep->active_low ? 1 : 0);
	if (ret)
		return ret;

	g_beep = beep;
	platform_set_drvdata(pdev, beep);

	ret = dm_beep_chrdev_init(beep);
	if (ret) {
		g_beep = NULL;
		return ret;
	}

	dm_beep_set(beep, false);
	dev_info(&pdev->dev, "created /dev/%s on gpio %d\n", DM_BEEP_NAME,
		 beep->gpio);
	return 0;
}

static int dm_beep_remove(struct platform_device *pdev)
{
	struct dm_beep_dev *beep = platform_get_drvdata(pdev);

	dm_beep_set(beep, false);
	dm_beep_chrdev_exit(beep);
	g_beep = NULL;
	return 0;
}

static const struct of_device_id dm_beep_of_match[] = {
	{ .compatible = "dm,beep" },
	{ }
};
MODULE_DEVICE_TABLE(of, dm_beep_of_match);

static struct platform_driver dm_beep_driver = {
	.probe = dm_beep_probe,
	.remove = dm_beep_remove,
	.driver = {
		.name = DM_BEEP_NAME,
		.of_match_table = dm_beep_of_match,
	},
};

module_platform_driver(dm_beep_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("Data Monitor GPIO beep character device driver");
