/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>

#include "flashlight.h"
#include "flashlight-dt.h"

/* device tree should be defined in flashlight-dt.h */
#ifndef LM3643_DTNAME
#define LM3643_DTNAME "mediatek,flashlights_lm3643"
#endif
#ifndef LM3643_DTNAME_I2C
#define LM3643_DTNAME_I2C "mediatek,flashlights_lm3643_i2c"
#endif

#define LM3643_NAME "flashlights-lm3643"

/* define registers */
#define LM3643_REG_ENABLE (0x01)
#define LM3643_MASK_ENABLE_LED1 (0x01)
#define LM3643_MASK_ENABLE_LED2 (0x02)
#define LM3643_DISABLE (0x00)
#define LM3643_ENABLE_LED1 (0x01)
#define LM3643_ENABLE_LED1_TORCH (0x09)
#define LM3643_ENABLE_LED1_FLASH (0x0D)
#define LM3643_ENABLE_LED2 (0x02)
#define LM3643_ENABLE_LED2_TORCH (0x0A)
#define LM3643_ENABLE_LED2_FLASH (0x0E)

#define LM3643_REG_TORCH_LEVEL_LED1 (0x05)
#define LM3643_REG_FLASH_LEVEL_LED1 (0x03)
#define LM3643_REG_TORCH_LEVEL_LED2 (0x06)
#define LM3643_REG_FLASH_LEVEL_LED2 (0x04)

#define LM3643_REG_TIMING_CONF (0x08)
#define LM3643_TORCH_RAMP_TIME (0x10)
#define LM3643_FLASH_TIMEOUT   (0x0F)

/* define ct, level */
#define LM3643_CT_NUM 2
#define LM3643_CT_HT 0
#define LM3643_CT_LT 1

#define LM3643_LEVEL_NUM 26
#define LM3643_LEVEL_TORCH 7

/* define mutex and work queue */
static DEFINE_MUTEX(lm3643_mutex);
static struct work_struct lm3643_work_ht;
static struct work_struct lm3643_work_lt;

/* define pinctrl */
#define LM3643_PINCTRL_PIN_HWEN 0
#define LM3643_PINCTRL_PINSTATE_LOW 0
#define LM3643_PINCTRL_PINSTATE_HIGH 1
#define LM3643_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define LM3643_PINCTRL_STATE_HWEN_LOW  "hwen_low"
static struct pinctrl *lm3643_pinctrl;
static struct pinctrl_state *lm3643_hwen_high;
static struct pinctrl_state *lm3643_hwen_low;

/* define usage count */
static int use_count;

/* define i2c */
static struct i2c_client *lm3643_i2c_client;

/* platform data */
struct lm3643_platform_data {
	u8 torch_pin_enable;         /* 1: TX1/TORCH pin isa hardware TORCH enable */
	u8 pam_sync_pin_enable;      /* 1: TX2 Mode The ENVM/TX2 is a PAM Sync. on input */
	u8 thermal_comp_mode_enable; /* 1: LEDI/NTC pin in Thermal Comparator Mode */
	u8 strobe_pin_disable;       /* 1: STROBE Input disabled */
	u8 vout_mode_enable;         /* 1: Voltage Out Mode enable */
};

/* lm3643 chip data */
struct lm3643_chip_data {
	struct i2c_client *client;
	struct lm3643_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};


/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int lm3643_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	/* get pinctrl */
	lm3643_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(lm3643_pinctrl)) {
		fl_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(lm3643_pinctrl);
	}

	/* Flashlight HWEN pin initialization */
	lm3643_hwen_high = pinctrl_lookup_state(lm3643_pinctrl, LM3643_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(lm3643_hwen_high)) {
		fl_err("Failed to init (%s)\n", LM3643_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(lm3643_hwen_high);
	}
	lm3643_hwen_low = pinctrl_lookup_state(lm3643_pinctrl, LM3643_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(lm3643_hwen_low)) {
		fl_err("Failed to init (%s)\n", LM3643_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(lm3643_hwen_low);
	}

	return ret;
}

static int lm3643_pinctrl_set(int pin, int state)
{
	int ret = 0;

	if (IS_ERR(lm3643_pinctrl)) {
		fl_err("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case LM3643_PINCTRL_PIN_HWEN:
		if (state == LM3643_PINCTRL_PINSTATE_LOW && !IS_ERR(lm3643_hwen_low))
			pinctrl_select_state(lm3643_pinctrl, lm3643_hwen_low);
		else if (state == LM3643_PINCTRL_PINSTATE_HIGH && !IS_ERR(lm3643_hwen_high))
			pinctrl_select_state(lm3643_pinctrl, lm3643_hwen_high);
		else
			fl_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		fl_err("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	fl_dbg("pin(%d) state(%d)\n", pin, state);

	return ret;
}


/******************************************************************************
 * lm3643 operations
 *****************************************************************************/
static const unsigned char lm3643_torch_level[LM3643_LEVEL_NUM] = {
	0x0F, 0x20, 0x31, 0x42, 0x52, 0x63, 0x74, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char lm3643_flash_level[LM3643_LEVEL_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x10, 0x14, 0x19,
	0x1D, 0x21, 0x25, 0x2A, 0x2E, 0x32, 0x37, 0x3B, 0x3F, 0x43,
	0x48, 0x4C, 0x50, 0x54, 0x59, 0x5D};

static volatile unsigned char lm3643_reg_enable;
static volatile int lm3643_level_ht = -1;
static volatile int lm3643_level_lt = -1;

static int lm3643_is_torch(int level)
{
	if (level >= LM3643_LEVEL_TORCH)
		return -1;

	return 0;
}

static int lm3643_verify_level(int level)
{
	if (level < 0)
		level = 0;
	else if (level >= LM3643_LEVEL_NUM)
		level = LM3643_LEVEL_NUM - 1;

	return level;
}

/* i2c wrapper function */
static int lm3643_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	struct lm3643_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		fl_err("failed writing at 0x%02x\n", reg);

	return ret;
}

/* flashlight enable function */
static int lm3643_enable_ht(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (!lm3643_is_torch(lm3643_level_ht)) {
		/* torch mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED1_TORCH;
	} else {
		/* flash mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED1_FLASH;
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_enable_lt(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (!lm3643_is_torch(lm3643_level_lt)) {
		/* torch mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED2_TORCH;
	} else {
		/* flash mode */
		lm3643_reg_enable |= LM3643_ENABLE_LED2_FLASH;
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_enable(int ct_index)
{
	if (ct_index == LM3643_CT_HT)
		lm3643_enable_ht();
	else if (ct_index == LM3643_CT_LT)
		lm3643_enable_lt();
	else {
		fl_err("Error ct index\n");
		return -1;
	}

	return 0;
}

/* flashlight disable function */
static int lm3643_disable_ht(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (lm3643_reg_enable & LM3643_MASK_ENABLE_LED2) {
		/* if LED 2 is enable, disable LED 1 */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED1);
	} else {
		/* if LED 2 is enable, disable LED 1 and clear mode */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED1_FLASH);
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_disable_lt(void)
{
	unsigned char reg, val;

	reg = LM3643_REG_ENABLE;
	if (lm3643_reg_enable & LM3643_MASK_ENABLE_LED1) {
		/* if LED 1 is enable, disable LED 2 */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED2);
	} else {
		/* if LED 1 is enable, disable LED 2 and clear mode */
		lm3643_reg_enable &= (~LM3643_ENABLE_LED2_FLASH);
	}
	val = lm3643_reg_enable;

	return lm3643_write_reg(lm3643_i2c_client, reg, val);
}

static int lm3643_disable(int ct_index)
{
	if (ct_index == LM3643_CT_HT)
		lm3643_disable_ht();
	else if (ct_index == LM3643_CT_LT)
		lm3643_disable_lt();
	else {
		fl_err("Error ct index\n");
		return -1;
	}

	return 0;
}

/* set flashlight level */
static int lm3643_set_level_ht(int level)
{
	int ret;
	unsigned char reg, val;

	level = lm3643_verify_level(level);

	/* set torch brightness level */
	reg = LM3643_REG_TORCH_LEVEL_LED1;
	val = lm3643_torch_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	lm3643_level_ht = level;

	/* set flash brightness level */
	reg = LM3643_REG_FLASH_LEVEL_LED1;
	val = lm3643_flash_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	return ret;
}

int lm3643_set_level_lt(int level)
{
	int ret;
	unsigned char reg, val;

	level = lm3643_verify_level(level);

	/* set torch brightness level */
	reg = LM3643_REG_TORCH_LEVEL_LED2;
	val = lm3643_torch_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	lm3643_level_lt = level;

	/* set flash brightness level */
	reg = LM3643_REG_FLASH_LEVEL_LED2;
	val = lm3643_flash_level[level];
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	return ret;
}

static int lm3643_set_level(int ct_index, int level)
{
	if (ct_index == LM3643_CT_HT)
		lm3643_set_level_ht(level);
	else if (ct_index == LM3643_CT_LT)
		lm3643_set_level_lt(level);
	else {
		fl_err("Error ct index\n");
		return -1;
	}

	return 0;
}

/* flashlight init */
int lm3643_init(void)
{
	int ret;
	unsigned char reg, val;

	lm3643_pinctrl_set(LM3643_PINCTRL_PIN_HWEN, LM3643_PINCTRL_PINSTATE_HIGH);

	/* clear enable register */
	reg = LM3643_REG_ENABLE;
	val = LM3643_DISABLE;
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	lm3643_reg_enable = val;

	/* set torch current ramp time and flash timeout */
	reg = LM3643_REG_TIMING_CONF;
	val = LM3643_TORCH_RAMP_TIME | LM3643_FLASH_TIMEOUT;
	ret = lm3643_write_reg(lm3643_i2c_client, reg, val);

	return ret;
}

/* flashlight uninit */
int lm3643_uninit(void)
{
	lm3643_disable(LM3643_CT_HT);
	lm3643_disable(LM3643_CT_LT);
	lm3643_pinctrl_set(LM3643_PINCTRL_PIN_HWEN, LM3643_PINCTRL_PINSTATE_LOW);

	return 0;
}


/******************************************************************************
 * Timer and work queue
 *****************************************************************************/
static struct hrtimer fl_timer_ht;
static struct hrtimer fl_timer_lt;
static unsigned int fl_timeout_ms[LM3643_CT_NUM];

static void lm3643_work_disable_ht(struct work_struct *data)
{
	fl_dbg("ht work queue callback\n");
	lm3643_disable_ht();
}

static void lm3643_work_disable_lt(struct work_struct *data)
{
	fl_dbg("lt work queue callback\n");
	lm3643_disable_lt();
}

static enum hrtimer_restart fl_timer_func_ht(struct hrtimer *timer)
{
	schedule_work(&lm3643_work_ht);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart fl_timer_func_lt(struct hrtimer *timer)
{
	schedule_work(&lm3643_work_lt);
	return HRTIMER_NORESTART;
}

int lm3643_timer_start(int ct_index, ktime_t ktime)
{
	if (ct_index == LM3643_CT_HT)
		hrtimer_start(&fl_timer_ht, ktime, HRTIMER_MODE_REL);
	else if (ct_index == LM3643_CT_LT)
		hrtimer_start(&fl_timer_lt, ktime, HRTIMER_MODE_REL);
	else {
		fl_err("Error ct index\n");
		return -1;
	}

	return 0;
}

int lm3643_timer_cancel(int ct_index)
{
	if (ct_index == LM3643_CT_HT)
		hrtimer_cancel(&fl_timer_ht);
	else if (ct_index == LM3643_CT_LT)
		hrtimer_cancel(&fl_timer_lt);
	else {
		fl_err("Error ct index\n");
		return -1;
	}

	return 0;

}


/******************************************************************************
 * Flashlight operations
 *****************************************************************************/
static int lm3643_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_user_arg *fl_arg;
	int ct_index;
	ktime_t ktime;

	fl_arg = (struct flashlight_user_arg *)arg;
	ct_index = fl_get_cl_index(fl_arg->ct_id);

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		fl_dbg("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n", (int)fl_arg->arg);
		fl_timeout_ms[ct_index] = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		fl_dbg("FLASH_IOC_SET_DUTY: %d\n", (int)fl_arg->arg);
		lm3643_set_level(ct_index, fl_arg->arg);
		break;

	case FLASH_IOC_SET_STEP:
		fl_dbg("FLASH_IOC_SET_STEP: %d\n", (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		fl_dbg("FLASH_IOC_SET_ONOFF: %d\n", (int)fl_arg->arg);
		if (fl_arg->arg == 1) {
			if (fl_timeout_ms[ct_index]) {
				ktime = ktime_set(fl_timeout_ms[ct_index] / 1000,
						(fl_timeout_ms[ct_index] % 1000) * 1000000);
				lm3643_timer_start(ct_index, ktime);
			}
			lm3643_enable(ct_index);
		} else {
			lm3643_disable(ct_index);
			lm3643_timer_cancel(ct_index);
		}
		break;
	default:
		fl_info("No such command and arg: (%d, %d)\n", _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int lm3643_open(void *pArg)
{
	fl_dbg("Open start\n");

	/* Actual behavior move to set driver function since power saving issue */

	fl_dbg("Open done.\n");

	return 0;
}

static int lm3643_release(void *pArg)
{
	fl_dbg("Release start.\n");

	/* uninit chip and clear usage count */
	mutex_lock(&lm3643_mutex);
	use_count--;
	if (!use_count)
		lm3643_uninit();
	if (use_count < 0)
		use_count = 0;
	mutex_unlock(&lm3643_mutex);

	fl_dbg("Release done. (%d)\n", use_count);

	return 0;
}

static int lm3643_set_driver(void)
{
	fl_dbg("Set driver start\n");

	/* init chip and set usage count */
	mutex_lock(&lm3643_mutex);
	if (!use_count)
		lm3643_init();
	use_count++;
	mutex_unlock(&lm3643_mutex);

	fl_dbg("Set driver done. (%d)\n", use_count);

	return 0;
}

static ssize_t lm3643_strobe_store(struct flashlight_arg arg)
{
	lm3643_set_driver();
	lm3643_set_level(arg.ct, arg.level);
	lm3643_enable(arg.ct);
	msleep(arg.dur);
	lm3643_disable(arg.ct);
	lm3643_release(NULL);

	return 0;
}

static struct flashlight_operations lm3643_ops = {
	lm3643_open,
	lm3643_release,
	lm3643_ioctl,
	lm3643_strobe_store,
	lm3643_set_driver
};


/******************************************************************************
 * I2C device and driver
 *****************************************************************************/
static int lm3643_chip_init(struct lm3643_chip_data *chip)
{
	/* NOTE: Chip initialication move to "set driver" operation for power saving issue.
	 * lm3643_init();
	 */

	return 0;
}

static int lm3643_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lm3643_chip_data *chip;
	struct lm3643_platform_data *pdata = client->dev.platform_data;
	int err;

	fl_dbg("Probe start.\n");

	/* check i2c */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		fl_err("Failed to check i2c functionality.\n");
		err = -ENODEV;
		goto err_out;
	}

	/* init chip private data */
	chip = kzalloc(sizeof(struct lm3643_chip_data), GFP_KERNEL);
	if (!chip) {
		err = -ENOMEM;
		goto err_out;
	}
	chip->client = client;

	/* init platform data */
	if (!pdata) {
		fl_dbg("Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct lm3643_platform_data), GFP_KERNEL);
		if (!pdata) {
			fl_err("Failed to allocate memory.\n");
			err = -ENOMEM;
			goto err_init_pdata;
		}
		chip->no_pdata = 1;
	}
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
	lm3643_i2c_client = client;

	/* init mutex and spinlock */
	mutex_init(&chip->lock);

	/* init work queue */
	INIT_WORK(&lm3643_work_ht, lm3643_work_disable_ht);
	INIT_WORK(&lm3643_work_lt, lm3643_work_disable_lt);

	/* init timer */
	hrtimer_init(&fl_timer_ht, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	fl_timer_ht.function = fl_timer_func_ht;
	hrtimer_init(&fl_timer_lt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	fl_timer_lt.function = fl_timer_func_lt;
	fl_timeout_ms[LM3643_CT_HT] = 100;
	fl_timeout_ms[LM3643_CT_LT] = 100;

	/* init chip hw */
	lm3643_chip_init(chip);

	/* register flashlight operations */
	if (flashlight_dev_register(LM3643_NAME, &lm3643_ops)) {
		fl_err("Failed to register flashlight device.\n");
		err = -EFAULT;
		goto err_free;
	}

	/* clear usage count */
	use_count = 0;

	fl_dbg("Probe done.\n");

	return 0;

err_free:
	kfree(chip->pdata);
err_init_pdata:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
err_out:
	return err;
}

static int lm3643_i2c_remove(struct i2c_client *client)
{
	struct lm3643_chip_data *chip = i2c_get_clientdata(client);

	fl_dbg("Remove start.\n");

	/* flush work queue */
	flush_work(&lm3643_work_ht);
	flush_work(&lm3643_work_lt);

	/* unregister flashlight operations */
	flashlight_dev_unregister(LM3643_NAME);

	/* free resource */
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);

	fl_dbg("Remove done.\n");

	return 0;
}

static const struct i2c_device_id lm3643_i2c_id[] = {
	{LM3643_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id lm3643_i2c_of_match[] = {
	{.compatible = LM3643_DTNAME_I2C},
	{},
};
#endif

static struct i2c_driver lm3643_i2c_driver = {
	.driver = {
		   .name = LM3643_NAME,
#ifdef CONFIG_OF
		   .of_match_table = lm3643_i2c_of_match,
#endif
		   },
	.probe = lm3643_i2c_probe,
	.remove = lm3643_i2c_remove,
	.id_table = lm3643_i2c_id,
};


/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int lm3643_probe(struct platform_device *dev)
{
	fl_dbg("Probe start.\n");

	/* init pinctrl */
	if (lm3643_pinctrl_init(dev)) {
		fl_dbg("Failed to init pinctrl.\n");
		return -1;
	}

	if (i2c_add_driver(&lm3643_i2c_driver)) {
		fl_dbg("Failed to add i2c driver.\n");
		return -1;
	}

	fl_dbg("Probe done.\n");

	return 0;
}

static int lm3643_remove(struct platform_device *dev)
{
	fl_dbg("Remove start.\n");

	i2c_del_driver(&lm3643_i2c_driver);

	fl_dbg("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm3643_of_match[] = {
	{.compatible = LM3643_DTNAME},
	{},
};
MODULE_DEVICE_TABLE(of, lm3643_of_match);
#else
static struct platform_device lm3643_platform_device[] = {
	{
		.name = LM3643_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, lm3643_platform_device);
#endif

static struct platform_driver lm3643_platform_driver = {
	.probe = lm3643_probe,
	.remove = lm3643_remove,
	.driver = {
		.name = LM3643_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lm3643_of_match,
#endif
	},
};

static int __init flashlight_lm3643_init(void)
{
	int ret;

	fl_dbg("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&lm3643_platform_device);
	if (ret) {
		fl_err("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&lm3643_platform_driver);
	if (ret) {
		fl_err("Failed to register platform driver\n");
		return ret;
	}

	fl_dbg("Init done.\n");

	return 0;
}

static void __exit flashlight_lm3643_exit(void)
{
	fl_dbg("Exit start.\n");

	platform_driver_unregister(&lm3643_platform_driver);

	fl_dbg("Exit done.\n");
}

module_init(flashlight_lm3643_init);
module_exit(flashlight_lm3643_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Simon Wang <Simon-TCH.Wang@mediatek.com>");
MODULE_DESCRIPTION("MTK Flashlight LM3643 Driver");

