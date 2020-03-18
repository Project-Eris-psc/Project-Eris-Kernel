/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <mach/upmu_sw.h>
#include <mt-plat/mtk_pmic_wrap.h>
#include <linux/time.h>

static DEFINE_MUTEX(mt6392_adc_mutex);

#define VOLTAGE_FULL_RANGE     1800
static int count_time_out = 100;

static const u32 mt6392_auxadc_regs[] = {
	MT6392_AUXADC_ADC0, MT6392_AUXADC_ADC1, MT6392_AUXADC_ADC2,
	MT6392_AUXADC_ADC3, MT6392_AUXADC_ADC4, MT6392_AUXADC_ADC5,
	MT6392_AUXADC_ADC6, MT6392_AUXADC_ADC7, MT6392_AUXADC_ADC8,
	MT6392_AUXADC_ADC9, MT6392_AUXADC_ADC10, MT6392_AUXADC_ADC11,
	MT6392_AUXADC_ADC12, MT6392_AUXADC_ADC12, MT6392_AUXADC_ADC12,
	MT6392_AUXADC_ADC12,
};

/**
 * PMIC_IMM_GetOneChannelValue() - Get voltage from a auxadc channel
 *
 * @dwChannel: Auxadc channel to choose
 *             0 : BATSNS	2.5~4.5V	~1.668ms data ready
 *             0 : ISENSE	2.5~4.5V	~1.668ms data ready
 *             2 : VCDT	0~1.5V	~0.108ms data ready
 *             3 : BATON	0.1~1.8V	~0.108ms data ready
 *             4 : PMIC_TEMP		~0.108ms data ready
 *             6 : TYPE-C	0~3.3V	~0.108ms data ready
 *             8 : DP/DM	0~3.6V	~0.108ms data ready
 *             9-11 :
 * @deCount: Count time to be averaged
 * @trimd: Auxadc value from trimmed register or not.
 *         mt6392 auxadc has no trimmed register, reserve it to align with mt6397
 *
 * This returns the current voltage of the specified channel in mV,
 * zero for not supported channel, a negative number on error.
 */
int PMIC_IMM_GetOneChannelValue(unsigned int dwChannel, int deCount, int trimd)
{
	int ret, adc_rdy, reg_val;
	int raw_data, adc_result;
	int adc_div = 4096, raw_mask = 0, r_val_temp = 0;
	int count = 0;
	int raw_data_sum = 0, raw_data_avg = 0;
	int u4Sample_times = 0;
	unsigned int adc_channel;

	adc_channel = dwChannel;
	do {
		/*defined channel 5 for TYEPC_CC1, channel 6 for TYPEC_CC2*/
		/*defined channel 7 for CHG_DP, channel 8 for CHG_DM*/
		mutex_lock(&mt6392_adc_mutex);
		if ((dwChannel == 5) || (dwChannel == 6)) {
			ret = pmic_config_interface(MT6392_TYPE_C_CTRL, 0x0, 0xffff, 0);
			if (ret < 0) {
				mutex_unlock(&mt6392_adc_mutex);
				return ret;
			}

			ret = pmic_config_interface(MT6392_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0, 0x5B8, 0xffff, 0);
			if (ret < 0) {
				mutex_unlock(&mt6392_adc_mutex);
				return ret;
			}

			ret = pmic_config_interface(MT6392_TYPE_C_CC_SW_FORCE_MODE_VAL_0, 0x0100, 0xffff, 0);
			if (ret < 0) {
				mutex_unlock(&mt6392_adc_mutex);
				return ret;
			}
			udelay(1000);

			if (dwChannel == 5) {
				ret = pmic_config_interface(MT6392_TYPE_C_CC_SW_FORCE_MODE_VAL_0, 0x1f40, 0xffff, 0);
				if (ret < 0) {
					mutex_unlock(&mt6392_adc_mutex);
					return ret;
				}
				/* AUXADC_RQST0_SET is bit 6*/
				adc_channel = 6;
			} else {
				ret = pmic_config_interface(MT6392_TYPE_C_CC_SW_FORCE_MODE_VAL_0, 0x1fc0, 0xffff, 0);
				if (ret < 0) {
					mutex_unlock(&mt6392_adc_mutex);
					return ret;
				}
			}
			ret = pmic_config_interface(MT6392_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0, 0x05fc, 0xffff, 0);
			if (ret < 0) {
				mutex_unlock(&mt6392_adc_mutex);
				return ret;
			}
			mdelay(10);

		}

		if ((dwChannel == 7) || (dwChannel == 8)) {
			if (dwChannel == 7) {
				/*DP channel*/
				ret = pmic_config_interface(MT6392_CHR_CON18, 0x0700, 0x0f00, 0);
				if (ret < 0) {
					mutex_unlock(&mt6392_adc_mutex);
					return ret;
				}
				/* AUXADC_RQST0_SET is bit 8*/
				adc_channel = 8;
			} else {
				/*DM channel*/
				ret = pmic_config_interface(MT6392_CHR_CON18, 0x0b00, 0x0f00, 0);
				if (ret < 0) {
					mutex_unlock(&mt6392_adc_mutex);
					return ret;
				}
			}
		}

		/* AUXADC_RQST0 SET  */
		ret = pmic_config_interface(MT6392_AUXADC_RQST0_SET, 0x1, 0xffff, adc_channel);
		if (ret < 0) {
			mutex_unlock(&mt6392_adc_mutex);
			return ret;
		}

		if ((adc_channel == 0) || (adc_channel == 1))
			udelay(1500);
		else
			udelay(100);

		/* check auxadc is ready */
		do {
			ret = pmic_read_interface(mt6392_auxadc_regs[adc_channel], &reg_val, 0xffff, 0);
			if (ret < 0) {
				mutex_unlock(&mt6392_adc_mutex);
				return ret;
			}
			udelay(100);
			adc_rdy = (reg_val >> 15) & 1;
			if (adc_rdy != 0)
				break;
			count++;
		} while (count < count_time_out);

		if (adc_rdy != 1) {
			mutex_unlock(&mt6392_adc_mutex);
			pr_err("PMIC_IMM_GetOneChannelValue adc get ready Fail\n");
			return -ETIMEDOUT;
		}
		count = 0;

		/* get the raw data and calculate the adc result of adc */
		ret = pmic_read_interface(mt6392_auxadc_regs[adc_channel], &reg_val, 0xffff, 0);
		if (ret < 0) {
			mutex_unlock(&mt6392_adc_mutex);
			return ret;
		}

		mutex_unlock(&mt6392_adc_mutex);

		switch (adc_channel) {
		case 0:
		case 1:
			r_val_temp = 3;
			raw_mask = 0x7fff;
			adc_div = 32768;
			break;

		case 2:
		case 3:
		case 4:
			r_val_temp = 1;
			raw_mask = 0xfff;
			adc_div = 4096;
			break;

		case 5:
		case 6:
		case 7:
		case 8:
			r_val_temp = 2;
			raw_mask = 0xfff;
			adc_div = 4096;
			break;

		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			r_val_temp = 1;
			raw_mask = 0xfff;
			adc_div = 4096;
			break;
		}

		/* get auxadc raw data */
		raw_data = reg_val & raw_mask;
		raw_data_sum += raw_data;
		u4Sample_times++;
	} while (u4Sample_times < deCount);

	raw_data_avg = raw_data_sum / deCount;

	/* get auxadc real result*/
	adc_result = (raw_data_avg * r_val_temp * VOLTAGE_FULL_RANGE) / adc_div;

	return adc_result;
}
