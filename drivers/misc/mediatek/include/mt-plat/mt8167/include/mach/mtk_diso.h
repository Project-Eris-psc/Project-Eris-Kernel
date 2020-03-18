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

#ifndef _MTK_DISO_H_
#define _MTK_DISO_H_

/*#define MTK_AUXADC_IRQ_SUPPORT*/
/*#define MTK_LOAD_SWITCH_FPF3040*/
#define MTK_DISCRETE_SWITCH
#define AP_AUXADC_DISO_VDC_CHANNEL 9
#define AP_AUXADC_DISO_VUSB_CHANNEL 12
#define R_DISO_DC_PULL_UP 10000
#define R_DISO_DC_PULL_DOWN 1000
#define R_DISO_VBUS_PULL_UP 5100
#define R_DISO_VBUS_PULL_DOWN 1000
#define VDC_MIN_VOLTAGE  BATTERY_VOLT_04_440000_V /*FPF3040 min value is 4000, set larger for PMIC detect*/
#define VDC_MAX_VOLTAGE BATTERY_VOLT_10_500000_V
#define VBUS_MIN_VOLTAGE  BATTERY_VOLT_04_440000_V /*FPF3040 min value is 4000, set larger for PMIC detect*/
#define VBUS_MAX_VOLTAGE BATTERY_VOLT_07_000000_V
#define SWITCH_RISING_TIMING 105
#define SWITCH_FALLING_TIMING 105
#define LOAD_SWITCH_TIMING_MARGIN 30
#define AUXADC_CHANNEL_DEBOUNCE 0x2
#define AUXADC_CHANNEL_DELAY_PERIOD 0x5
#ifdef MTK_LOAD_SWITCH_FPF3040
#define VIN_SEL_FLAG
#define VIN_SEL_FLAG_DEFAULT_LOW
#define CUST_GPIO_VIN_SEL 18
#elif defined(MTK_DISCRETE_SWITCH)
#ifdef MTK_DSC_USE_EINT
#define VIN_SEL_FLAG
#endif
#define CUST_GPIO_VIN_SEL 20
#define CUST_EINT_VDC_NUM 42
#define CUST_EINT_VUSB_NUM 43
#define CUST_EINT_VDC_DEBOUNCE_CN 1
#define CUST_EINT_VUSB_DEBOUNCE_CN 1
#endif
#endif

