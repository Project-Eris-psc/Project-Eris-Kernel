#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#define EPPROM_MAX_ADD 0x7ff /*16Kbit 2Kbyte*/
#define EPPROM_PAGE_SIZE 16

struct eeprom_dev {
	struct i2c_client* i2cclient;
	struct mutex lock;
};

static struct i2c_client* br24eep_client;
static int wc_gpio;
struct eeprom_dev* eeprom_devp;

static int br24eep_open(struct inode *inode, struct file *file)
{
	if(eeprom_devp != NULL){
		pr_err("%s() error: br24eep dev is opened\n", __func__);
		return EPERM;
	}

	eeprom_devp = kmalloc(sizeof(struct eeprom_dev),GFP_KERNEL);
	if(!eeprom_devp)	{
		return -ENOMEM;
	}
	eeprom_devp->i2cclient = br24eep_client;
	mutex_init(&eeprom_devp->lock);
	file->private_data = eeprom_devp;

	pr_err("%s()br24eep dev opened:i2cclient address:[0x%x],gpio_num[%d]\n", __func__,br24eep_client->addr,wc_gpio);

    return 0;
}


static int br24eep_close(struct inode *inode, struct file *file)
{
	kfree(eeprom_devp);
	eeprom_devp = NULL;

    return 0;
}

static loff_t br24eep_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret=0;
	struct eeprom_dev* devp= filp->private_data;

	mutex_lock(&devp->lock);
	switch(orig){
		case 0: /*opposite the head of file*/
			if (offset < 0){
				ret = - EINVAL;
				break;
			}
			if (offset > EPPROM_MAX_ADD){
				ret = - EINVAL;
				break;
			}
			filp->f_pos = (unsigned int)offset;
			ret = filp->f_pos;
			break;
		case 1:/*opposite the current offset*/
			if ((filp->f_pos + offset) > EPPROM_MAX_ADD){
				ret = - EINVAL;
				break;
			}
			if((filp->f_pos + offset) < 0){
				ret = - EINVAL;
				break;
			}
			filp->f_pos += offset;
			ret = filp->f_pos;
			break;
		}
	mutex_unlock(&devp->lock);

    return ret;
}

static ssize_t br24eep_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
    int ret, i;
	unsigned int data_addr = *off;
	unsigned char data_addr_h, data_addr_l;
    unsigned char *readbuf = NULL;
    struct i2c_msg msg[2];
	struct eeprom_dev* devp= file->private_data;

	if (data_addr + count - 1 >= EPPROM_MAX_ADD){
        printk("%s invalid count\n", __func__);
        return -EINVAL;
    }

	if (data_addr > EPPROM_MAX_ADD){
        printk("%s invalid count\n", __func__);
	}

	mutex_lock(&devp->lock);

    readbuf = kzalloc(count, GFP_KERNEL);

    if (count == 0) {
        return 0;
    } else {
    	for (i = 0; i < count;i++) {
			data_addr_h = (((data_addr + i) >> 8) & 0x7);
			data_addr_l = ((data_addr + i) & 0xFF);

	        msg[0].addr  = devp->i2cclient->addr | data_addr_h;    /* device addr */
	        msg[0].buf   = &data_addr_l;                           /* source*/
	        msg[0].len   = 1;                                      /* addr 1 byte */
	        msg[0].flags = 0;                                      /* write */

	        msg[1].addr  = devp->i2cclient->addr | data_addr_h;    /* device addr*/
	        msg[1].buf   = readbuf + i;                           /* source*/
	        msg[1].len   = 1;                             	      /* data len*/
	        msg[1].flags = 1;                		              /* read*/
	        ret = i2c_transfer(devp->i2cclient->adapter, msg, 2);

	        if (ret != 2){
				kfree(readbuf);
				mutex_unlock(&devp->lock);
	            printk("%s i2c_transfer error:%d \n", __func__,ret);
	            return -EINVAL;
	        }
	    }
    }

    ret = copy_to_user(buf, readbuf, count);
    if (ret < 0){
		kfree(readbuf);
		mutex_unlock(&devp->lock);
        printk("%s copy_from_user error\n", __func__);
    }

    kfree(readbuf);
	mutex_unlock(&devp->lock);

    return count;
}

static ssize_t br24eep_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
    int ret, i;
    unsigned char pagebuf[2];
	unsigned char *data = NULL;
	unsigned char data_addr_h, data_addr_l;
	unsigned int data_addr = *off;
    struct i2c_msg msg;
	struct eeprom_dev* devp= file->private_data;

    if (data_addr > EPPROM_MAX_ADD){
        printk("%s invalid address \n", __func__);
        return -EINVAL;
    }
	if(data_addr + count > EPPROM_MAX_ADD){
        printk("%s invalid size \n", __func__);
        return -EINVAL;
	}

	mutex_lock(&devp->lock);

    data = kzalloc(count, GFP_KERNEL);
    ret = copy_from_user(data, buf, count);
    if (ret < 0){
        printk("%s copy_from_user error\n", __func__);
    }

	gpio_set_value(wc_gpio, 0);
	ndelay(1000);

	for(i = 0;i < count;i++) {
		data_addr_h = (((data_addr + i) >> 8) & 0x7);
		data_addr_l = (data_addr & 0xFF);

		pagebuf[0] = (data_addr+i) & 0xFF;
		pagebuf[1] = data[i];

		msg.addr  = (devp->i2cclient->addr | data_addr_h);				/* device addr */
		msg.buf   = pagebuf;											/* source */
		msg.len   = 2;													/* addr + data byte */
		msg.flags = 0;													/* wirte flag*/

		ret = i2c_transfer(devp->i2cclient->adapter,&msg,1);
		mdelay(5);

		if (ret != 1) {
			gpio_set_value(wc_gpio, 1);
			kfree(data);
			mutex_unlock(&devp->lock);
			printk("%s i2c_transfer error ret:%d line[%d] \n", __func__,ret,__LINE__);
			return -EINVAL;
		}
	}

	gpio_set_value(wc_gpio, 1);
    kfree(data);
	mutex_unlock(&devp->lock);

    return count;
}

static const struct file_operations br24eep_fops = {
	.owner = THIS_MODULE,
	.open = br24eep_open,
	.release = br24eep_close,
	.llseek = br24eep_llseek,
	.write = br24eep_write,
	.read = br24eep_read,
	.flush = NULL,
	.fasync = NULL,
	.mmap = NULL
};

static struct miscdevice br24eep_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "br24eep",
	.fops = &br24eep_fops,
};
static int br24_i2c_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
	int ret;

	br24eep_client = client;

	wc_gpio = of_get_named_gpio(br24eep_client->dev.of_node, "wc-gpio", 0);
	if (!gpio_is_valid(wc_gpio)) {
		pr_err("%s get invalid wc gpio %d\n", __func__, wc_gpio);
		ret = -EINVAL;
	}
	gpio_request(wc_gpio,"eepro-wc");

	ret = gpio_direction_output(wc_gpio,1);
	if (ret < 0)
		return ret;

	ret = misc_register(&br24eep_device);
	if (ret) {
		pr_err("%s() error: misc_register br24eep failed %d\n", __func__, ret);
		return ret;
	}
    return 0;
}

static int br24_i2c_remove(struct i2c_client *client)
{
	gpio_free(wc_gpio);
	misc_deregister(&br24eep_device);

    return 0;
}

static const struct of_device_id br24_of_match[] = {
	{ .compatible = "br24_eeprom", },
};
MODULE_DEVICE_TABLE(of, br24_of_match);


static const struct i2c_device_id br24_id[] = {
	{"br24_eeprom", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cs35l32_id);

static struct i2c_driver br24_i2c_driver = {
	.driver = {
		   .name = "br24_eeprom",
		   .of_match_table = br24_of_match,
		   },
	.id_table = br24_id,
	.probe = br24_i2c_probe,
	.remove = br24_i2c_remove,
};

module_i2c_driver(br24_i2c_driver);

MODULE_DESCRIPTION("EEPROM br24t16fj driver");
MODULE_AUTHOR("damon.liu, <damon.liu@mediatek.com>");
MODULE_LICENSE("GPL");

