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

#define DM_LED_NAME "dm_led"

struct dm_led_dev {
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int gpio;
	bool active_low;
	bool state;
};

static struct dm_led_dev *g_led;

static void dm_led_set(struct dm_led_dev *led, bool on)
{
	gpio_set_value(led->gpio, led->active_low ? !on : on);
	led->state = on;
}

static int dm_led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_led;
	return 0;
}

static ssize_t dm_led_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct dm_led_dev *led = filp->private_data;
	char val;

	if (*ppos)
		return 0;

	val = led->state ? '1' : '0';
	if (copy_to_user(buf, &val, 1))
		return -EFAULT;

	*ppos = 1;
	return 1;
}

static ssize_t dm_led_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct dm_led_dev *led = filp->private_data;
	char val;

	if (count < 1)
		return -EINVAL;
	if (copy_from_user(&val, buf, 1))
		return -EFAULT;

	if (val == '1')
		dm_led_set(led, true);
	else if (val == '0')
		dm_led_set(led, false);
	else
		return -EINVAL;

	return count;
}

static const struct file_operations dm_led_fops = {
	.owner = THIS_MODULE,
	.open = dm_led_open,
	.read = dm_led_read,
	.write = dm_led_write,
	.llseek = no_llseek,
};

static int dm_led_chrdev_init(struct dm_led_dev *led)
{
	int ret;

	ret = alloc_chrdev_region(&led->devt, 0, 1, DM_LED_NAME);
	if (ret)
		return ret;

	cdev_init(&led->cdev, &dm_led_fops);
	led->cdev.owner = THIS_MODULE;

	ret = cdev_add(&led->cdev, led->devt, 1);
	if (ret)
		goto err_unregister;

	led->class = class_create(THIS_MODULE, DM_LED_NAME);
	if (IS_ERR(led->class)) {
		ret = PTR_ERR(led->class);
		goto err_cdev;
	}

	led->device = device_create(led->class, NULL, led->devt, NULL,
				    DM_LED_NAME);
	if (IS_ERR(led->device)) {
		ret = PTR_ERR(led->device);
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(led->class);
err_cdev:
	cdev_del(&led->cdev);
err_unregister:
	unregister_chrdev_region(led->devt, 1);
	return ret;
}

static void dm_led_chrdev_exit(struct dm_led_dev *led)
{
	device_destroy(led->class, led->devt);
	class_destroy(led->class);
	cdev_del(&led->cdev);
	unregister_chrdev_region(led->devt, 1);
}

static int dm_led_probe(struct platform_device *pdev)
{
	struct dm_led_dev *led;
	enum of_gpio_flags flags;
	int gpio;
	int ret;

	if (g_led)
		return -EBUSY;

	gpio = of_get_named_gpio_flags(pdev->dev.of_node, "led-gpio", 0,
				       &flags);
	if (!gpio_is_valid(gpio))
		return gpio < 0 ? gpio : -EINVAL;

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->gpio = gpio;
	led->active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = devm_gpio_request(&pdev->dev, led->gpio, DM_LED_NAME);
	if (ret)
		return ret;

	ret = gpio_direction_output(led->gpio, led->active_low ? 1 : 0);
	if (ret)
		return ret;

	g_led = led;
	platform_set_drvdata(pdev, led);

	ret = dm_led_chrdev_init(led);
	if (ret) {
		g_led = NULL;
		return ret;
	}

	dm_led_set(led, false);
	dev_info(&pdev->dev, "created /dev/%s on gpio %d\n", DM_LED_NAME,
		 led->gpio);
	return 0;
}

static int dm_led_remove(struct platform_device *pdev)
{
	struct dm_led_dev *led = platform_get_drvdata(pdev);

	dm_led_set(led, false);
	dm_led_chrdev_exit(led);
	g_led = NULL;
	return 0;
}

static const struct of_device_id dm_led_of_match[] = {
	{ .compatible = "dm,led" },
	{ }
};
MODULE_DEVICE_TABLE(of, dm_led_of_match);

static struct platform_driver dm_led_driver = {
	.probe = dm_led_probe,
	.remove = dm_led_remove,
	.driver = {
		.name = DM_LED_NAME,
		.of_match_table = dm_led_of_match,
	},
};

module_platform_driver(dm_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("Data Monitor GPIO LED character device driver");
