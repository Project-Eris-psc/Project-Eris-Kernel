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

#ifndef _MTK_BATTERY_METER_H
#define _MTK_BATTERY_METER_H


/* ============================================================*/
/* define*/
/* ============================================================*/
/*#define SOC_BY_AUXADC*/
/*fg auxadc*/
/*#define SOC_BY_HW_FG*/
#define SOC_BY_SW_FG /*oam*/

/*#define CONFIG_DIS_CHECK_BATTERY*/
/*#define FIXED_TBAT_25*/

#if defined(CONFIG_MTK_BQ24296_SUPPORT) || defined(CONFIG_MTK_BQ24196_SUPPORT)
#define BAT_VOL_USE_ISENSE
#define SWCHR_POWER_PATH
#endif

#if defined(CONFIG_MTK_FAN5402_SUPPORT) \
	 || defined(CONFIG_MTK_FAN5405_SUPPORT) \
	  || defined(CONFIG_MTK_BQ24158_SUPPORT) \
	   || defined(CONFIG_MTK_BQ24196_SUPPORT) \
	    || defined(CONFIG_MTK_BQ24296_SUPPORT) \
	     || defined(CONFIG_MTK_NCP1851_SUPPORT) \
	      || defined(CONFIG_MTK_NCP1854_SUPPORT) \
	       || defined(CONFIG_MTK_BQ24160_SUPPORT) \
			|| defined(CONFIG_MTK_BQ24157_SUPPORT) \
			|| defined(CONFIG_MTK_BQ24250_SUPPORT) \
			|| defined(CONFIG_MTK_BQ24261_SUPPORT)
#define EXTERNAL_SWCHR_SUPPORT
#endif

/* ADC Channel Number */
#define CUST_TABT_NUMBER 17
#ifdef BAT_VOL_USE_ISENSE
#define VBAT_CHANNEL_NUMBER      1 /* w/ powerpath, battery voltage is ISENSE(1)*/
#define ISENSE_CHANNEL_NUMBER	 0 /* w/ powerpath, system voltage is VSENSE(0)*/
#else
#define VBAT_CHANNEL_NUMBER      0
#define ISENSE_CHANNEL_NUMBER	 1
#endif
#define VCHARGER_CHANNEL_NUMBER  2
#define VBATTEMP_CHANNEL_NUMBER  4


/* ADC resistor  */
#define R_BAT_SENSE 4
#define R_I_SENSE 4
#define R_CHARGER_1 330
#define R_CHARGER_2 39

#define TEMPERATURE_T0             110
#define TEMPERATURE_T1             0
#define TEMPERATURE_T2             25
#define TEMPERATURE_T3             50
#define TEMPERATURE_T              255 /*This should be fixed, never change the value*/

#define FG_METER_RESISTANCE		0

/* Qmax for battery  */
#define Q_MAX_POS_50	3009
#define Q_MAX_POS_25	3009
#define Q_MAX_POS_0		3009
#define Q_MAX_NEG_10	3009

#define Q_MAX_POS_50_H_CURRENT  2975
#define Q_MAX_POS_25_H_CURRENT	2975
#define Q_MAX_POS_0_H_CURRENT	2975
#define Q_MAX_NEG_10_H_CURRENT	2975


/* Discharge Percentage */
#define OAM_D5		 0		/*  1 : D5,   0: D2*/


/* battery meter parameter */
#define CUST_TRACKING_POINT  14
#define CUST_R_SENSE         200
#define CUST_HW_CC			0
#define AGING_TUNING_VALUE   100
#define CUST_R_FG_OFFSET    0

#define OCV_BOARD_COMPESATE	0 /*mV*/
#define R_FG_BOARD_BASE		1000
#define R_FG_BOARD_SLOPE	1000 /*slope*/
#define CAR_TUNE_VALUE		89


/* HW Fuel gague  */
#define CURRENT_DETECT_R_FG	(-1)  /*turn off auto detect for auxadc gauge*/
#define MinErrorOffset       1000
#define FG_VBAT_AVERAGE_SIZE 18
#define R_FG_VALUE			0	/*mOhm, base is 20*/

#define CUST_POWERON_DELTA_CAPACITY_TOLRANCE	80
#define CUST_POWERON_LOW_CAPACITY_TOLRANCE		2
#define CUST_POWERON_MAX_VBAT_TOLRANCE			70
#define CUST_POWERON_DELTA_VBAT_TOLRANCE		60

/* Disable Battery check for HQA */
#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
#define FIXED_TBAT_25
#endif

/* Dynamic change wake up period of battery thread when suspend*/
#define VBAT_NORMAL_WAKEUP		3600		/*3.6V*/
#define VBAT_LOW_POWER_WAKEUP		3500		/*3.5v*/
#define NORMAL_WAKEUP_PERIOD		5400	/*90 * 60 = 90 min*/
#define LOW_POWER_WAKEUP_PERIOD		300		/*5 * 60 = 5 min*/
#define CLOSE_POWEROFF_WAKEUP_PERIOD	30	/*30 s*/

#endif	/*#ifndef _MTK_BATTERY_METER_H*/

