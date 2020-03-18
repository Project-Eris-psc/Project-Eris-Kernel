#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/i2c.h>
#include <linux/input.h>


struct device *dev = NULL;
char * s_c[2];
static ssize_t send(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	s_c[0] = "NULL";
	s_c[1] = NULL;
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, s_c);
	return count;
}
void send_event(int oc_status)
{
	if (oc_status)
		s_c[0] = "oc_happened";
	else
		s_c[0] = "oc_normal";

	s_c[1] = NULL;
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, s_c);
}

static DEVICE_ATTR(S, S_IRUGO|S_IWUSR, NULL, send);


static const struct attribute *hdmi_oc_event_attr[] = {
        &dev_attr_S.attr,
        NULL,
};


static const struct attribute_group hdmi_oc_event_attr_group = {
        .attrs = (struct attribute **) hdmi_oc_event_attr,
};


static struct class hdmi_oc_event_class = {
        .name =         "hdmi_oc_event",
        .owner =        THIS_MODULE,
};


static int __init hdmi_oc_uevent_init(void)
{
	int ret = 0;

	pr_err("hdmi_oc_uevent_init start\n");
	ret = class_register(&hdmi_oc_event_class);
	if (ret < 0) {
		pr_err(KERN_ERR "[HDMI] hdmi_oc_event: class_register fail\n");
		return ret;
	}

	dev = device_create(&hdmi_oc_event_class, NULL, MKDEV(0, 0), NULL, "hdmi_oc_event");
	if (dev) {
		ret = sysfs_create_group(&dev->kobj, &hdmi_oc_event_attr_group);
		if(ret < 0) {
			pr_err(KERN_ERR "[HDMI] hdmi_oc_event:sysfs_create_group fail\r\n");
			return ret;
		}
	} else {
		pr_err(KERN_ERR "[HDMI] hdmi_oc_event:device_create fail\r\n");
		ret = -1;
		return ret;
	}

	return 0;
}
module_init(hdmi_oc_uevent_init);
MODULE_AUTHOR("<fanning.hui@mediatek.com>");
MODULE_DESCRIPTION("hdmi_oc event detect driver");
MODULE_LICENSE("GPL");
