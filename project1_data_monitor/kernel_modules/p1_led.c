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

#define P1_LED_NAME "p1_led"

struct p1_led_dev {
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int gpio;
	bool active_low;
	bool state;
};

static struct p1_led_dev *g_led;

static void p1_led_set(struct p1_led_dev *led, bool on)
{
	gpio_set_value(led->gpio, led->active_low ? !on : on);
	led->state = on;
}

static int p1_led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_led;
	return 0;
}

static ssize_t p1_led_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct p1_led_dev *led = filp->private_data;
	char val;

	if (*ppos)
		return 0;

	val = led->state ? '1' : '0';
	if (copy_to_user(buf, &val, 1))
		return -EFAULT;

	*ppos = 1;
	return 1;
}

static ssize_t p1_led_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct p1_led_dev *led = filp->private_data;
	char val;

	if (count < 1)
		return -EINVAL;
	if (copy_from_user(&val, buf, 1))
		return -EFAULT;

	if (val == '1')
		p1_led_set(led, true);
	else if (val == '0')
		p1_led_set(led, false);
	else
		return -EINVAL;

	return count;
}

static const struct file_operations p1_led_fops = {
	.owner = THIS_MODULE,
	.open = p1_led_open,
	.read = p1_led_read,
	.write = p1_led_write,
	.llseek = no_llseek,
};

static int p1_led_chrdev_init(struct p1_led_dev *led)
{
	int ret;

	ret = alloc_chrdev_region(&led->devt, 0, 1, P1_LED_NAME);
	if (ret)
		return ret;

	cdev_init(&led->cdev, &p1_led_fops);
	led->cdev.owner = THIS_MODULE;

	ret = cdev_add(&led->cdev, led->devt, 1);
	if (ret)
		goto err_unregister;

	led->class = class_create(THIS_MODULE, P1_LED_NAME);
	if (IS_ERR(led->class)) {
		ret = PTR_ERR(led->class);
		goto err_cdev;
	}

	led->device = device_create(led->class, NULL, led->devt, NULL,
				    P1_LED_NAME);
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

static void p1_led_chrdev_exit(struct p1_led_dev *led)
{
	device_destroy(led->class, led->devt);
	class_destroy(led->class);
	cdev_del(&led->cdev);
	unregister_chrdev_region(led->devt, 1);
}

static int p1_led_probe(struct platform_device *pdev)
{
	struct p1_led_dev *led;
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

	ret = devm_gpio_request(&pdev->dev, led->gpio, P1_LED_NAME);
	if (ret)
		return ret;

	ret = gpio_direction_output(led->gpio, led->active_low ? 1 : 0);
	if (ret)
		return ret;

	g_led = led;
	platform_set_drvdata(pdev, led);

	ret = p1_led_chrdev_init(led);
	if (ret) {
		g_led = NULL;
		return ret;
	}

	p1_led_set(led, false);
	dev_info(&pdev->dev, "created /dev/%s on gpio %d\n", P1_LED_NAME,
		 led->gpio);
	return 0;
}

static int p1_led_remove(struct platform_device *pdev)
{
	struct p1_led_dev *led = platform_get_drvdata(pdev);

	p1_led_set(led, false);
	p1_led_chrdev_exit(led);
	g_led = NULL;
	return 0;
}

static const struct of_device_id p1_led_of_match[] = {
	{ .compatible = "p1,myled" },
	{ }
};
MODULE_DEVICE_TABLE(of, p1_led_of_match);

static struct platform_driver p1_led_driver = {
	.probe = p1_led_probe,
	.remove = p1_led_remove,
	.driver = {
		.name = P1_LED_NAME,
		.of_match_table = p1_led_of_match,
	},
};

module_platform_driver(p1_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Project1");
MODULE_DESCRIPTION("Project1 GPIO LED character device driver");
