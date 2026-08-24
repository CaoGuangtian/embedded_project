#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define DM_KEY_NAME "dm_key"
#define DM_KEY_DEBOUNCE_MS 15
#define DM_KEY_PRESS 1
#define DM_KEY_RELEASE 0

struct dm_key_dev {
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int gpio;
	int irq;
	bool active_low;
	struct timer_list timer;
	wait_queue_head_t waitq;
	spinlock_t lock;
	unsigned char event;
	bool event_ready;
};

static struct dm_key_dev *g_key;

static int dm_key_hw_pressed(struct dm_key_dev *key)
{
	int value = gpio_get_value(key->gpio);

	return key->active_low ? !value : value;
}

static void dm_key_timer_fn(unsigned long data)
{
	struct dm_key_dev *key = (struct dm_key_dev *)data;
	unsigned long flags;
	unsigned char event = dm_key_hw_pressed(key) ? DM_KEY_PRESS :
			      DM_KEY_RELEASE;

	spin_lock_irqsave(&key->lock, flags);
	key->event = event;
	key->event_ready = true;
	spin_unlock_irqrestore(&key->lock, flags);

	wake_up_interruptible(&key->waitq);
}

static irqreturn_t dm_key_irq_handler(int irq, void *dev_id)
{
	struct dm_key_dev *key = dev_id;

	mod_timer(&key->timer,
		  jiffies + msecs_to_jiffies(DM_KEY_DEBOUNCE_MS));
	return IRQ_HANDLED;
}

static int dm_key_open(struct inode *inode, struct file *filp)
{
	filp->private_data = g_key;
	return 0;
}

static ssize_t dm_key_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct dm_key_dev *key = filp->private_data;
	unsigned long flags;
	unsigned char event;
	int ret;

	if (count < 1)
		return -EINVAL;

	if (filp->f_flags & O_NONBLOCK) {
		if (!key->event_ready)
			return -EAGAIN;
	} else {
		ret = wait_event_interruptible(key->waitq, key->event_ready);
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&key->lock, flags);
	event = key->event;
	key->event_ready = false;
	spin_unlock_irqrestore(&key->lock, flags);

	if (copy_to_user(buf, &event, 1))
		return -EFAULT;

	return 1;
}

static unsigned int dm_key_poll(struct file *filp, poll_table *wait)
{
	struct dm_key_dev *key = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &key->waitq, wait);
	if (key->event_ready)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations dm_key_fops = {
	.owner = THIS_MODULE,
	.open = dm_key_open,
	.read = dm_key_read,
	.poll = dm_key_poll,
	.llseek = no_llseek,
};

static int dm_key_chrdev_init(struct dm_key_dev *key)
{
	int ret;

	ret = alloc_chrdev_region(&key->devt, 0, 1, DM_KEY_NAME);
	if (ret)
		return ret;

	cdev_init(&key->cdev, &dm_key_fops);
	key->cdev.owner = THIS_MODULE;

	ret = cdev_add(&key->cdev, key->devt, 1);
	if (ret)
		goto err_unregister;

	key->class = class_create(THIS_MODULE, DM_KEY_NAME);
	if (IS_ERR(key->class)) {
		ret = PTR_ERR(key->class);
		goto err_cdev;
	}

	key->device = device_create(key->class, NULL, key->devt, NULL,
				    DM_KEY_NAME);
	if (IS_ERR(key->device)) {
		ret = PTR_ERR(key->device);
		goto err_class;
	}

	return 0;

err_class:
	class_destroy(key->class);
err_cdev:
	cdev_del(&key->cdev);
err_unregister:
	unregister_chrdev_region(key->devt, 1);
	return ret;
}

static void dm_key_chrdev_exit(struct dm_key_dev *key)
{
	device_destroy(key->class, key->devt);
	class_destroy(key->class);
	cdev_del(&key->cdev);
	unregister_chrdev_region(key->devt, 1);
}

static int dm_key_probe(struct platform_device *pdev)
{
	struct dm_key_dev *key;
	enum of_gpio_flags flags;
	int gpio;
	int ret;

	if (g_key)
		return -EBUSY;

	gpio = of_get_named_gpio_flags(pdev->dev.of_node, "key-gpio", 0,
				       &flags);
	if (!gpio_is_valid(gpio))
		return gpio < 0 ? gpio : -EINVAL;

	key = devm_kzalloc(&pdev->dev, sizeof(*key), GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	key->gpio = gpio;
	key->active_low = flags & OF_GPIO_ACTIVE_LOW;
	spin_lock_init(&key->lock);
	init_waitqueue_head(&key->waitq);

	ret = devm_gpio_request(&pdev->dev, key->gpio, DM_KEY_NAME);
	if (ret)
		return ret;

	ret = gpio_direction_input(key->gpio);
	if (ret)
		return ret;

	key->irq = gpio_to_irq(key->gpio);
	if (key->irq < 0)
		return key->irq;

	setup_timer(&key->timer, dm_key_timer_fn, (unsigned long)key);

	ret = devm_request_irq(&pdev->dev, key->irq, dm_key_irq_handler,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			       DM_KEY_NAME, key);
	if (ret) {
		del_timer_sync(&key->timer);
		return ret;
	}

	g_key = key;
	platform_set_drvdata(pdev, key);

	ret = dm_key_chrdev_init(key);
	if (ret) {
		g_key = NULL;
		del_timer_sync(&key->timer);
		return ret;
	}

	dev_info(&pdev->dev, "created /dev/%s on gpio %d irq %d\n",
		 DM_KEY_NAME, key->gpio, key->irq);
	return 0;
}

static int dm_key_remove(struct platform_device *pdev)
{
	struct dm_key_dev *key = platform_get_drvdata(pdev);

	del_timer_sync(&key->timer);
	dm_key_chrdev_exit(key);
	g_key = NULL;
	return 0;
}

static const struct of_device_id dm_key_of_match[] = {
	{ .compatible = "dm,key" },
	{ }
};
MODULE_DEVICE_TABLE(of, dm_key_of_match);

static struct platform_driver dm_key_driver = {
	.probe = dm_key_probe,
	.remove = dm_key_remove,
	.driver = {
		.name = DM_KEY_NAME,
		.of_match_table = dm_key_of_match,
	},
};

module_platform_driver(dm_key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DataMonitor");
MODULE_DESCRIPTION("Data Monitor GPIO key interrupt character device driver");
