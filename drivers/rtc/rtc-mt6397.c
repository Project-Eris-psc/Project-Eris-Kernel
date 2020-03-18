/*
 * Copyright (c) 2014-2015 MediaTek Inc.
 * Author: Tianping.Fang <tianping.fang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/reboot.h>
#include "../misc/mediatek/include/mt-plat/mt8167/include/mach/upmu_hw.h"
#include "../misc/mediatek/include/mt-plat/mt8167/include/mach/mt_rtc_hw.h"

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include "../misc/mediatek/include/mt-plat/mtk_boot_common.h"
#endif

//#define RTC_BBPU		0x0000
//#define RTC_BBPU_CBUSY		BIT(6)

//#define RTC_WRTGR		0x003c

//#define RTC_IRQ_STA		0x0002
//#define RTC_IRQ_STA_AL		BIT(0)
//#define RTC_IRQ_STA_LP		BIT(3)

//#define RTC_IRQ_EN		0x0004
//#define RTC_IRQ_EN_AL		BIT(0)
//#define RTC_IRQ_EN_ONESHOT	BIT(2)
//#define RTC_IRQ_EN_LP		BIT(3)
//#define RTC_IRQ_EN_ONESHOT_AL	(RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

//#define RTC_AL_MASK		0x0008
//#define RTC_AL_MASK_DOW		BIT(4)

//#define RTC_TC_SEC		0x000a
//#define RTC_SPAR1		0x0032
//#define RTC_AL_SEC		0x0018

/* Min, Hour, Dom... register offset to RTC_TC_SEC */
#define RTC_OFFSET_SEC		0
#define RTC_OFFSET_MIN		1
#define RTC_OFFSET_HOUR		2
#define RTC_OFFSET_DOM		3
#define RTC_OFFSET_DOW		4
#define RTC_OFFSET_MTH		5
#define RTC_OFFSET_YEAR		6
#define RTC_OFFSET_COUNT	7

#define RTC_AL_SEC		0x0018

//#define RTC_PDN2		0x002e
//#define RTC_PDN1		0x002c
//#define RTC_SPAR0		0x0030
//#define RTC_PDN2_PWRON_ALARM	BIT(4)
//#define RTC_PDN1_PWRON_TIME	BIT(7)
//#define RTC_PDN2_PWRON_LOGO	BIT(15)
//#define RTC_BBPU_RELOAD	BIT(5)

#define RTC_BBPU_KEY		(0x43 << 8)
#define RTC_PWRON_YEA		RTC_PDN2
#define RTC_PWRON_YEA_MASK	0x7f00
#define RTC_PWRON_YEA_SHIFT	8

//#define RTC_PROT		0x0036
#define RTC_PROT_UNLOCK1	0x586a
#define RTC_PROT_UNLOCK2	0x9136

//#define RTC_POWERKEY1		0x0028
//#define RTC_POWERKEY2		0x002a
#define RTC_POWERKEY1_KEY	0xa357
#define RTC_POWERKEY2_KEY	0x67d2

//#define RTC_OSC32CON		0x0026
#define RTC_OSC32CON_UNLOCK1	0x1a57
#define RTC_OSC32CON_UNLOCK2	0x2b68

#define RTC_PWRON_MTH		RTC_PDN2
#define RTC_PWRON_MTH_MASK	0x000f
#define RTC_PWRON_MTH_SHIFT	0

#define RTC_PWRON_SEC		RTC_SPAR0
#define RTC_PWRON_SEC_MASK	0x003f
#define RTC_PWRON_SEC_SHIFT	0
//#define RTC_SPAR0_ALARM_BOOT	BIT(8)


#define RTC_PWRON_MIN		RTC_SPAR1
#define RTC_PWRON_MIN_MASK	0x003f
#define RTC_PWRON_MIN_SHIFT	0

#define RTC_PWRON_HOU		RTC_SPAR1
#define RTC_PWRON_HOU_MASK	0x07c0
#define RTC_PWRON_HOU_SHIFT	6

#define RTC_PWRON_DOM		RTC_SPAR1
#define RTC_PWRON_DOM_MASK	0xf800
#define RTC_PWRON_DOM_SHIFT	11

#define RTC_MIN_YEAR		1968
#define RTC_BASE_YEAR		1900
#define RTC_NUM_YEARS		128
#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - RTC_BASE_YEAR)

//#define RTC_CON              0x003e
#define RTC_CON_LPEN		(1U << 2)
#define RTC_CON_LPRST		(1U << 3)
#define RTC_CON_CDBO		(1U << 4)
#define RTC_CON_F32KOB		(1U << 5)	/* 0: RTC_GPIO exports 32K */
#define RTC_CON_GPO		(1U << 6)
#define RTC_CON_GOE		(1U << 7)	/* 1: GPO mode, 0: GPI mode */
#define RTC_CON_GSR		(1U << 8)
#define RTC_CON_GSMT		(1U << 9)
#define RTC_CON_GPEN		(1U << 10)
#define RTC_CON_GPU		(1U << 11)
#define RTC_CON_GE4		(1U << 12)
#define RTC_CON_GE8		(1U << 13)
#define RTC_CON_GPI		(1U << 14)
#define RTC_CON_LPSTA_RAW	(1U << 15)	/* 32K was stopped */

//#define MT6392_CHRSTATUS                        0x0142
#define RTC_HW_DET_BYPASS	(8 << MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT)
#define	RTC_HW_XOSC_MODE	(~(2 << MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT))
#define	RTC_HW_DCXO_MODE	(2 << MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT)

#if 1
#define RTC_RELPWR_WHEN_XRST	1   /* BBPU = 0 when xreset_rstb goes low */

#define RTC_GPIO_USER_MASK	(((1U << 13) - 1) & 0xff00)

static bool recovery_flag = false;

static bool Write_trigger(void);
static U16 eosc_cali(void);
static bool rtc_first_boot_init(void);
static U16 get_frequency_meter(U16 val, U16 measureSrc, U16 window_size);
static bool rtc_frequency_meter_check(void);
static void rtc_recovery_flow(void);
static bool rtc_recovery_mode_check(void);
static bool rtc_init_after_recovery(void);
static bool rtc_get_recovery_mode_stat(void);
static bool rtc_gpio_init(void);
static bool rtc_android_init(void);
static bool rtc_lpd_init(void);
static bool Writeif_unlock(void);

extern void enable_26M_clock_to_pmic(void);
extern void disable_26M_clock_to_pmic(void);

#endif

struct mt6397_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	struct regmap		*regmap;
	int			irq;
	u32			addr_base;
};

static struct mt6397_rtc *mt_rtc;
static bool is_xosc;

static u16 RTC_Read(u16 addr)
{
	u32 data;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + (uintptr_t)addr,
				  &data);
	if (ret < 0)
		goto exit;
	return (u16) data;
exit:
	dev_err(mt_rtc->dev, "regmap read error!!!\n");
	return ret;
}

static u16 RTC_Read_PMIC(u16 addr)
{
	u32 data;
	int ret;

	ret = regmap_read(mt_rtc->regmap, (uintptr_t)addr, &data);
	if (ret < 0)
		goto exit;
	return (u16) data;
exit:
	dev_err(mt_rtc->dev, "regmap read error!!!\n");
	return ret;
}

static void RTC_Write(u16 addr, u16 data)
{
	int ret;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + (uintptr_t)addr, data);
	if(ret < 0)
		dev_err(mt_rtc->dev, "regmap write error!!!\n");
}

#if 0
void RTC_Write_PMIC(u16 addr, u16 data)
{
	int ret;
	ret = regmap_write(mt_rtc->regmap, (uintptr_t)addr, data);
	if(ret < 0)
		dev_err(mt_rtc->dev, "regmap write error!!!\n");
}

#endif

static void RTC_Config_PMIC(u16 addr, u16 data, u16 MASK, u16 SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(mt_rtc->regmap, (uintptr_t)addr, &pmic_reg);
	if (return_value != 0)
		pr_err("[RTC] PMIC Reg[%x] read data fail\n", addr);

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (data << SHIFT);

	return_value = regmap_write(mt_rtc->regmap, (uintptr_t)addr, pmic_reg);
	if (return_value != 0)
		pr_err("[RTC] PMIC Reg[%x] write data fail\n", addr);
}


static int mtk_rtc_write_trigger(struct mt6397_rtc *rtc)
{
	unsigned long timeout = jiffies + HZ;
	int ret;
	u32 data;

	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_WRTGR, 1);
	if (ret < 0)
		return ret;

	while (1) {
		ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_BBPU,
				  &data);
		if (ret < 0)
			break;
		if (!(data & RTC_BBPU_CBUSY))
			break;
		if (time_after(jiffies, timeout)) {
			ret = -ETIMEDOUT;
			cpu_relax();
			return 0;
		}
		cpu_relax();
	}

	return 1;
}

static bool Write_trigger(void)
{
	return mtk_rtc_write_trigger(mt_rtc);
}

static void rtc_call_exception(void)
{
	//ASSERT(0);
	BUG();
}

static bool rtc_xosc_check_clock(U16 *result)
{
	///// fix me  loose range for frequency meter result////
	if ((result[0] >= 3  &&result[0] <= 7 ) &&
			(result[1] > 1500 && result[1] < 6000) &&
			(result[2] == 0) &&
			(result[3] == 0))
		return true;
	else
		return false;
}

static bool rtc_eosc_check_clock(U16 *result)
{
	if ((result[0] >= 3  &&result[0] <= 7 )&&
			(result[1] < 500) &&
			(result[2] > 2 && result[2] < 9) &&
			(result[3] > 300 && result[3] < 10400))
		return true;
	else
		return false;
}


static void rtc_xosc_write(U16 val)
{
	RTC_Write(RTC_OSC32CON, RTC_OSC32CON_UNLOCK1);
	mdelay(1);

	RTC_Write(RTC_OSC32CON, RTC_OSC32CON_UNLOCK2);
	mdelay(1);

	RTC_Write(RTC_OSC32CON, val);
	mdelay(1);
}

static U16 get_frequency_meter(U16 val, U16 measureSrc, U16 window_size)
{
	unsigned long timeout = jiffies + HZ;
	U16 ret;

	if(val!=0)
		rtc_xosc_write(val);

	//disable freq. meter, gated src clock
	RTC_Config_PMIC(MT6392_FQMTR_CON0, 0x0000, 0xFFFF, 0);
	//TOP_CKPDN1[5]=1, gated fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKPDN1, 0x1, 0x1, MT6392_PMIC_RG_FQMTR_PDN_SHIFT);
	//TOP_RST_CON[8]=1, FQMTR reset
	RTC_Config_PMIC(MT6392_TOP_RST_CON, 0x1, 0x1, MT6392_PMIC_RG_FQMTR_RST_SHIFT);
	while( !(RTC_Read_PMIC(MT6392_FQMTR_CON2)==0) && (FQMTR_BUSY&RTC_Read_PMIC(MT6392_FQMTR_CON0))==FQMTR_BUSY);
	//TOP_RST_CON[8]=0, FQMTR normal
	RTC_Config_PMIC(MT6392_TOP_RST_CON, 0x0, 0x1, MT6392_PMIC_RG_FQMTR_RST_SHIFT);
	//TOP_CKPDN1[5]=0, turn on fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKPDN1, 0x0, 0x1, MT6392_PMIC_RG_FQMTR_PDN_SHIFT);

	//set freq. meter window value (0=1X32K(fix clock))
	RTC_Config_PMIC(MT6392_FQMTR_CON1, window_size, 0xFFFF, 0);
	//enable freq. meter, set measure clock to 26Mhz
	RTC_Config_PMIC(MT6392_FQMTR_CON0, FQMTR_EN | measureSrc, 0xFFFF, 0);

	mdelay(2);
#if 1
	while ((FQMTR_BUSY & RTC_Read_PMIC(MT6392_FQMTR_CON0)) == FQMTR_BUSY)
	{
		if (time_after(jiffies, timeout))
		{
			printk("get frequency time out\n");
			break;
		}
	};		// FQMTR read until ready
#endif
	//read data should be closed to 26M/32k = 812.5
	ret = RTC_Read_PMIC(MT6392_FQMTR_CON2);

	printk("[RTC] get_frequency_meter: input=0x%x, ouput=%d\n", val, ret);
	return ret;
}

static void rtc_measure_four_clock(U16 *result)
{
	U16 window_size;

	//Disable RTC CLK gating
	//TOP_CKPDN1[5]=1, gated fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKPDN1, 0x0, 0x1, MT6392_PMIC_RG_FQMTR_PDN_SHIFT);

	//select 26M as fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKCON1, FQMTR_FIX_CLK_26M, 0x7, MT6392_PMIC_RG_FQMTR_CKSEL_SHIFT);
	window_size = 4;
	mdelay(1);
	//select 26M as target clock
	result[0] = get_frequency_meter(0, FQMTR_AUD26M_CK, window_size);

	//select XOSC32_CK as fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKCON1, FQMTR_FIX_CLK_XOSC_32K_DET, 0x7, MT6392_PMIC_RG_FQMTR_CKSEL_SHIFT);
	window_size = 4;
	mdelay(1);
	//select 26M as target clock
	result[1] = get_frequency_meter(0, FQMTR_AUD26M_CK, window_size);

	//select 26M as fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKCON1, FQMTR_FIX_CLK_26M, 0x7, MT6392_PMIC_RG_FQMTR_CKSEL_SHIFT);
	window_size = 3970;  // (26M / 32K) * 5
	mdelay(1);
	//select XOSC32 as target clock
	result[2] = get_frequency_meter(0, FQMTR_XOSC32_CK, window_size);
	//select DCXO_32 as target clock
	result[2] = get_frequency_meter(0, FQMTR_DCXO_F32K_CK, window_size);

	//select EOSC32_CK as fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKCON1, FQMTR_FIX_CLK_EOSC_32K, 0x7, MT6392_PMIC_RG_FQMTR_CKSEL_SHIFT);
	window_size = 4;
	mdelay(1);
	//select 26M as target clock
	result[3] = get_frequency_meter(0, FQMTR_AUD26M_CK, window_size);
}

static void rtc_switch_mode(bool XOSC, bool recovery)
{
	if (XOSC) {
		if (recovery) {
			/* HW bypass switch mode control and set to XOSC */
			/* RTC_Read_PMIC((MT6392_CHRSTATUS) | RTC_HW_DET_BYPASS) & RTC_HW_XOSC_MODE */
			RTC_Config_PMIC(MT6392_CHRSTATUS, 0x1, 0x1, 3 + MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT);
			RTC_Config_PMIC(MT6392_CHRSTATUS, 0x0, 0x1, 1 + MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT);
		}
		rtc_xosc_write(0x0003);  /* assume crystal exist mode + XOSCCALI = 0x3 */
		if (recovery)
			mdelay(1000);
	} else {
		if (recovery) {
			/* HW bypass switch mode control and set to DCXO */
			/* RTC_Read_PMIC(MT6392_CHRSTATUS) | RTC_HW_DET_BYPASS | RTC_HW_DCXO_MODE */
			RTC_Config_PMIC(MT6392_CHRSTATUS, 0x1, 0x1, 3 + MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT);
			RTC_Config_PMIC(MT6392_CHRSTATUS, 0x1, 0x1, 1 + MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT);
		}
		rtc_xosc_write(0x240F); /*crystal not exist + eosc cali = 0xF*/
		mdelay(10);
	}
}

static void rtc_switch_to_xosc_mode(void)
{
	rtc_switch_mode(true, false);
}

static void rtc_switch_to_dcxo_mode(void)
{
	rtc_switch_mode(false, false);
}

static void rtc_switch_to_xosc_recv_mode(void)
{
	rtc_switch_mode(true, true);
}

static void rtc_switch_to_dcxo_recv_mode(void)
{
	rtc_switch_mode(false, true);
}

/* return 1: 32k  0: 32k-less */
bool rtc_get_xosc_mode(void)
{
	U16 con, xosc_mode;;

	con = RTC_Read(RTC_OSC32CON);

	if((con & 0x0020) == 0)
		xosc_mode = 1;
	else
		xosc_mode = 0;
	return xosc_mode;
}

static bool rtc_frequency_meter_check(void)
{
	U16  result[4];

	if (rtc_get_recovery_mode_stat())
		rtc_switch_to_xosc_recv_mode();

	rtc_measure_four_clock(result);

	if (rtc_xosc_check_clock(result)) {
		pr_info("[RTC] XOSC\n");
		rtc_xosc_write(0x0000);	/* crystal exist mode + XOSCCALI = 0 */
		return true;
	} else {
		if (!rtc_get_recovery_mode_stat()) {
			rtc_switch_to_dcxo_mode();
			pr_info("[RTC] dcxo mode\n");
		} else {
			rtc_switch_to_dcxo_recv_mode();
			pr_info("[RTC] dcxo recv mode\n");
		}
	}

	rtc_measure_four_clock(result);

	if (rtc_eosc_check_clock(result)) {
		U16 val;

		val = eosc_cali();
		pr_info("[RTC] EOSC cali val = 0x%x\n", val);
		//EMB_HW_Mode
		val = (val & 0x001f)|0x2400;
		rtc_xosc_write(val);
		return true;
	} else {
		pr_info("[RTC] freq else return\n");
		return false;
	}
}

static void rtc_set_recovery_mode_stat(bool enable)
{
	recovery_flag = enable;
}

static bool rtc_get_recovery_mode_stat(void)
{
	return recovery_flag;
}

static bool rtc_init_after_recovery(void)
{
	if (!Writeif_unlock())
		return false;
	/* write powerkeys */
	RTC_Write(RTC_POWERKEY1, RTC_POWERKEY1_KEY);
	RTC_Write(RTC_POWERKEY2, RTC_POWERKEY2_KEY);
	if (!Write_trigger())
		return false;

	RTC_Config_PMIC(MT6392_CHRSTATUS, 0x0, 0x1, 3 + MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT);

	if (!rtc_gpio_init())
		return false;
	if (!rtc_android_init())
		return false;
	if (!rtc_lpd_init())
		return false;

	return true;
}
static bool rtc_recovery_mode_check(void)
{
	/////// fix me add return ret for recovery mode check fail
	if (!rtc_frequency_meter_check()) {
		rtc_call_exception();
		return false;
	}
	return true;
}

static void rtc_recovery_flow(void)
{
	u16 count = 0;
	printk("rtc_recovery_flow\n");
	rtc_set_recovery_mode_stat(true);
	while (count < 3) {
		if (rtc_recovery_mode_check()) {
			if (rtc_init_after_recovery())
				break;
		}
		count++;
	}
	rtc_set_recovery_mode_stat(false);
	if (count == 3)
		rtc_call_exception();

}
#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
extern kal_bool kpoc_flag ;
#endif

static bool Writeif_unlock(void)
{
	RTC_Write(RTC_PROT, RTC_PROT_UNLOCK1);
	if (!Write_trigger())
		return false;
	RTC_Write(RTC_PROT, RTC_PROT_UNLOCK2);
	if (!Write_trigger())
		return false;

	return true;
}

static bool rtc_android_init(void)
{
	U16 irqsta;

	RTC_Write(RTC_IRQ_EN, 0);
	RTC_Write(RTC_CII_EN, 0);
	RTC_Write(RTC_AL_MASK, 0);

	RTC_Write(RTC_AL_YEA, 1970 - RTC_MIN_YEAR);
	RTC_Write(RTC_AL_MTH, 1);
	RTC_Write(RTC_AL_DOM, 1);
	RTC_Write(RTC_AL_DOW, 1);
	RTC_Write(RTC_AL_HOU, 0);
	RTC_Write(RTC_AL_MIN, 0);
	RTC_Write(RTC_AL_SEC, 0);

	RTC_Write(RTC_PDN1, RTC_PDN1_DEBUG);   /* set Debug bit */
	RTC_Write(RTC_PDN2, ((1970 - RTC_MIN_YEAR) << RTC_PDN2_PWRON_YEA_SHIFT) | 1);
	RTC_Write(RTC_SPAR0, 0);
	RTC_Write(RTC_SPAR1, (1 << RTC_SPAR1_PWRON_DOM_SHIFT));

	RTC_Write(RTC_DIFF, 0);
	RTC_Write(RTC_CALI, 0);

	if (!Write_trigger())
		return false;

	irqsta = RTC_Read(RTC_IRQ_STA);	/* read clear */

	/* init time counters after resetting RTC_DIFF and RTC_CALI */
	RTC_Write(RTC_TC_YEA, 2010 - RTC_MIN_YEAR);
	RTC_Write(RTC_TC_MTH, 1);
	RTC_Write(RTC_TC_DOM, 1);
	RTC_Write(RTC_TC_DOW, 1);
	RTC_Write(RTC_TC_HOU, 0);
	RTC_Write(RTC_TC_MIN, 0);
	RTC_Write(RTC_TC_SEC, 0);
	if(!Write_trigger())
		return false;

	return true;
}

static bool rtc_gpio_init(void)
{
	U16 con;

	/* GPI mode and pull enable + pull down */
	con = RTC_Read(RTC_CON) & (RTC_CON_LPSTA_RAW | RTC_CON_LPRST | RTC_CON_LPEN);
	con &= ~RTC_CON_GPU;
	con |= RTC_CON_GPEN | RTC_CON_F32KOB | RTC_CON_GOE;
	RTC_Write(RTC_CON, con);
	if (Write_trigger())
		return true;
	else
		return false;
}

static U16 eosc_cali(void)
{
	U16 val = 0;
	U16 diff;
	int middle;
	int left = RTC_EOSC_CALI_LEFT, right = RTC_EOSC_CALI_RIGHT;

	//fix clock = eosc 32K
	RTC_Config_PMIC(MT6392_TOP_CKCON1, FQMTR_FIX_CLK_EOSC_32K, 0x7, MT6392_PMIC_RG_FQMTR_CKSEL_SHIFT);
	printk("[RTC] EOSC_Cali: TOP_CKCON1=0x%x\n", RTC_Read_PMIC(MT6392_TOP_CKCON1));
	while (left<=(right)) {
		middle = (right + left) / 2;
		if(middle == left)
			break;

		val = get_frequency_meter(middle, FQMTR_AUD26M_CK, 0);
		if ((val>RTC_FQMTR_LOW_BASE) && (val<RTC_FQMTR_LOW_BASE))
			break;
		if (val > RTC_FQMTR_LOW_BASE)
			right = middle;
		else
			left = middle;
	}

	if ((val>RTC_FQMTR_LOW_BASE) && (val<RTC_FQMTR_HIGH_BASE))
		return middle;

	val=get_frequency_meter(left, FQMTR_AUD26M_CK, 0);
	diff=RTC_FQMTR_LOW_BASE-val;
	val=get_frequency_meter(right, FQMTR_AUD26M_CK, 0);
	if(diff<(val-RTC_FQMTR_LOW_BASE))
		return left;
	else
		return right;
}

static void rtc_lpd_state_clr(void)
{
	U16 spar0;

	spar0 = RTC_Read(RTC_SPAR0);

	RTC_Write(RTC_SPAR0, spar0 & (~0x0080) ); //bit 7 for low power detected in preloader
	Write_trigger();
	spar0 = RTC_Read(RTC_SPAR0);
	printk("[RTC] RTC_SPAR0=0x%x \n", spar0);
}

static void rtc_osc_init(void)
{
	/* disable 32K export if there are no RTC_GPIO users */
	if (!(RTC_Read(RTC_PDN1) & RTC_GPIO_USER_MASK))
		rtc_gpio_init();

	if(rtc_get_xosc_mode())
	{
		U16 con;
		con = RTC_Read(RTC_OSC32CON);
		if ((con & RTC_OSC32CON_XOSCCALI_MASK) != 0x0) {	/* check XOSCCALI */
			rtc_xosc_write(0x0003);  /* crystal exist mode + XOSCCALI = 0x3 */
			//gpt_busy_wait_us(200);
			udelay(200);
		}

		rtc_xosc_write(0x0000);  /* crystal exist mode + XOSCCALI = 0x0 */
	}
	else
	{
		U16 val;
		val = eosc_cali();
		printk("[RTC] EOSC cali val = 0x%x\n", val);
		//EMB_HW_Mode
		val = (val & RTC_OSC32CON_XOSCCALI_MASK) | RTC_OSC32CON_XOSC32_ENB | RTC_OSC32CON_EOSC32_CHOP_EN;
		rtc_xosc_write(val);
	}

	rtc_lpd_state_clr();
}

static bool rtc_check_lpd(void)
{
	U16 con;

	con = RTC_Read(RTC_CON);
	if (con & RTC_CON_LPSTA_RAW
		|| ((con & RTC_CON_LPEN)!=RTC_CON_LPEN)
		|| con & RTC_CON_LPRST) {
	    return true;
	} else {
		return false;
	}
}
static bool rtc_lpd_init(void)
{
	U16 con;

	con = RTC_Read(RTC_CON) | RTC_CON_LPEN;
	con &= ~RTC_CON_LPRST;
	RTC_Write(RTC_CON, con);
	if (!Write_trigger())
		return false;

	con |= RTC_CON_LPRST;
	RTC_Write(RTC_CON, con);
	if (!Write_trigger())
		return false;

	con &= ~RTC_CON_LPRST;
	RTC_Write(RTC_CON, con);
	if (!Write_trigger())
		return false;

	RTC_Write(RTC_SPAR0, RTC_Read(RTC_SPAR0) | 0x0080 ); //bit 7 for low power detected in preloader
	if (!Write_trigger() || rtc_check_lpd())
		return false;
	return true;
}

static bool rtc_first_boot_init(void)
{
	printk("rtc_first_boot_init\n");

	if (!Writeif_unlock())
		return false;

	if (!rtc_gpio_init()) {
		pr_info("[RTC] GPIO init fail!\n");
		return false;
	}
	rtc_switch_to_xosc_mode();
	/* write powerkeys */
	RTC_Write(RTC_POWERKEY1, RTC_POWERKEY1_KEY);
	RTC_Write(RTC_POWERKEY2, RTC_POWERKEY2_KEY);
	if (!Write_trigger()) {
		pr_info("[RTC] Write trigger fail!\n");
		return false;
	}
	mdelay(1000);

	if (!rtc_frequency_meter_check()) {
		pr_info("[RTC] frequency meter fail!\n");
		return false;
	}
	if (!rtc_android_init()) {
		pr_info("[RTC] Android init fail!\n");
		return false;
	}
	if (!rtc_lpd_init()) {
		pr_info("[RTC] lpd init fail!\n");
		return false;
	}

	return true;
}

static void rtc_bbpu_power_down(void)
{
	U16 bbpu;

	/* pull PWRBB low */
	bbpu = RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_PWREN;
	Writeif_unlock();
	RTC_Write(RTC_BBPU, bbpu);
	Write_trigger();
}

void rtc_bbpu_power_on(void)
{
	U16 bbpu;

	/* pull PWRBB high */
#if RTC_RELPWR_WHEN_XRST
	bbpu = RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_BBPU | RTC_BBPU_PWREN;
#else
	bbpu = RTC_BBPU_KEY | RTC_BBPU_BBPU | RTC_BBPU_PWREN;
#endif
	RTC_Write(RTC_BBPU, bbpu);
	Write_trigger();
	printk("[RTC] rtc_bbpu_power_on done and bbpu = 0x%x\n", RTC_Read(RTC_BBPU));

	RTC_Write(RTC_CALI, RTC_Read(RTC_CALI) & ~RTC_CALI_BBPU_2SEC_EN);

}

void rtc_mark_bypass_pwrkey(void)
{
	U16 pdn1;

	pdn1 = RTC_Read(RTC_PDN1) | RTC_PDN1_BYPASS_PWR;
	RTC_Write(RTC_PDN1, pdn1);
	Write_trigger();
}

static void rtc_clean_mark(void)
{
	U16 pdn1, pdn2;

	pdn1 = RTC_Read(RTC_PDN1) & ~(RTC_PDN1_DEBUG | RTC_PDN1_BYPASS_PWR);   /* also clear Debug bit */
	pdn2 = RTC_Read(RTC_PDN2) & ~RTC_PDN1_FAC_RESET;
	RTC_Write(RTC_PDN1, pdn1);
	RTC_Write(RTC_PDN2, pdn2);
	Write_trigger();
}

U16 rtc_rdwr_uart_bits(U16 *val)
{
	U16 pdn2;

	if (RTC_Read(RTC_CON) & RTC_CON_LPSTA_RAW)
		return 3;   /* UART bits are invalid due to RTC uninit */

	if (val) {
		pdn2 = RTC_Read(RTC_PDN2) & ~RTC_PDN2_UART_MASK;
		pdn2 |= (*val & (RTC_PDN2_UART_MASK >> RTC_PDN2_UART_SHIFT)) << RTC_PDN2_UART_SHIFT;
		RTC_Write(RTC_PDN2, pdn2);
		Write_trigger();
	}

	return (RTC_Read(RTC_PDN2) & RTC_PDN2_UART_MASK) >> RTC_PDN2_UART_SHIFT;
}

void rtc_boot_check(void)
{
	U16 irqsta, pdn1, pdn2, spar0, spar1;
	U16 result[4];
	bool check_mode_flag = false;

	//Enable XO to PMIC 26M
	enable_26M_clock_to_pmic();

	//Enable 26M in PMIC
	RTC_Config_PMIC(MT6392_TOP_CKPDN0, 0x1, 0x1, 1);

	//Disable RTC CLK gating
	//TOP_CKPDN1[5]=0, gated fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKPDN1, 0x0, 0x1, MT6392_PMIC_RG_FQMTR_PDN_SHIFT);

	// check clock source are match with 32K exist
	rtc_measure_four_clock(result);
	if (!rtc_xosc_check_clock(result) && !rtc_eosc_check_clock(result))
	{
		printk("[RTC] RTC 32K mode setting wrong. Enter first boot/recovery. \n");
		check_mode_flag = true;
	}

	printk("[RTC] bbpu = 0x%x, con = 0x%x\n", RTC_Read(RTC_BBPU), RTC_Read(RTC_CON));
	if (rtc_check_lpd() || check_mode_flag)
	{
		if (!rtc_first_boot_init()) {
			rtc_recovery_flow();
		}
		RTC_Write(RTC_BBPU, RTC_Read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
		Write_trigger();
	}
	else
	{
	/* normally HW reload is done in BROM but check again here */
		printk("[RTC] powerkey1 = 0x%x, powerkey2 = 0x%x\n",
		RTC_Read(RTC_POWERKEY1), RTC_Read(RTC_POWERKEY2));
		RTC_Write(RTC_BBPU, RTC_Read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
		if (!Write_trigger())
		{
			rtc_recovery_flow();
		}else
		{
			if (!Writeif_unlock())
			{
				rtc_recovery_flow();
			}else
			{
				printk("Writeif_unlock\n");
				if (RTC_Read(RTC_POWERKEY1) != RTC_POWERKEY1_KEY ||
					RTC_Read(RTC_POWERKEY2) != RTC_POWERKEY2_KEY)
				{
					printk("[RTC] powerkey1 = 0x%x, powerkey2 = 0x%x\n",
						RTC_Read(RTC_POWERKEY1), RTC_Read(RTC_POWERKEY2));
					if (!rtc_first_boot_init()) {
						rtc_recovery_flow();
					}
				} else
				{
					rtc_osc_init();
				}
			}
		}
		// make sure RTC get the latest register info. //
		RTC_Write(RTC_BBPU, RTC_Read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
		Write_trigger();
	}

	//TOP_CKTST2[4]=1, RTC_CLK32K_1V8 source sel RTC 32K. Otherwise 75K
	RTC_Config_PMIC(MT6392_TOP_CKTST2, 0x1, 0x1, 4);
	printk("[RTC] MT6392_TOP_CKTST2 = 0x%x\n", RTC_Read_PMIC(MT6392_TOP_CKTST2));

	//TOP_CKPDN1[5]=1, gated fixed clock
	RTC_Config_PMIC(MT6392_TOP_CKPDN1, 0x1, 0x1, MT6392_PMIC_RG_FQMTR_PDN_SHIFT);

	rtc_clean_mark();
	//set register to let MD know 32k status
	spar0 = RTC_Read(RTC_SPAR0);
	if(rtc_get_xosc_mode() == 0)
	{
		RTC_Write(RTC_SPAR0, (spar0 | RTC_SPAR0_32K_LESS) );
	}
	else
	{
		RTC_Write(RTC_SPAR0, (spar0 & ~RTC_SPAR0_32K_LESS) );
	}
	Write_trigger();
	RTC_Write(RTC_BBPU, RTC_Read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	Write_trigger();
	irqsta = RTC_Read(RTC_IRQ_STA);	/* Read clear */
	pdn1 = RTC_Read(RTC_PDN1);
	pdn2 = RTC_Read(RTC_PDN2);
	spar0 = RTC_Read(RTC_SPAR0);
	spar1 = RTC_Read(RTC_SPAR1);
	printk("[RTC] irqsta = 0x%x, pdn1 = 0x%x, pdn2 = 0x%x, spar0 = 0x%x, spar1 = 0x%x\n",
		  irqsta, pdn1, pdn2, spar0, spar1);
	printk("[RTC] new_spare0 = 0x%x, new_spare1 = 0x%x, new_spare2 = 0x%x, new_spare3 = 0x%x\n",
		  RTC_Read(RTC_AL_HOU), RTC_Read(RTC_AL_DOM), RTC_Read(RTC_AL_DOW), RTC_Read(RTC_AL_MTH));
	printk("[RTC] bbpu = 0x%x, con = 0x%x, cali = 0x%x\n", RTC_Read(RTC_BBPU), RTC_Read(RTC_CON), RTC_Read(RTC_CALI));
	RTC_Write(RTC_SPAR0, spar0 & ~RTC_SPAR0_ALARM_BOOT);
	Write_trigger();
	printk("[RTC] spar0 = 0x%x\n", RTC_Read(RTC_SPAR0));

	//Disable 26M in PMIC
	RTC_Config_PMIC(MT6392_TOP_CKPDN0, 0x0, 0x1, 1);

	//Disable XO to PMIC 26M
	disable_26M_clock_to_pmic();
}

void pl_power_off(void)
{
	printk("[RTC] pl_power_off\n");

	rtc_bbpu_power_down();

	while (1);
}

static void _mtk_rtc_save_pwron_alarm(void)
{
	u32 pdn1, pdn2;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	pdn1 &= ~RTC_PDN1_PWRON_TIME;
	pdn2 |= RTC_PDN2_PWRON_ALARM;
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN1, pdn1);
	if (ret < 0)
		goto exit;

	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, pdn2);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

exit:
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");
}

static void _mtk_rtc_set_alarm(struct rtc_time *tm)
{
	u16 data[RTC_OFFSET_COUNT];
	int ret;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	dev_err(mt_rtc->dev, "set al time = %04d/%02d/%02d %02d:%02d:%02d\n",
		  tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);

	ret = regmap_bulk_write(mt_rtc->regmap,
				mt_rtc->addr_base + RTC_AL_SEC,
				data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_AL_MASK,
				   RTC_AL_MASK_DOW);
	if (ret < 0)
		goto exit;
	ret = regmap_update_bits(mt_rtc->regmap,
				mt_rtc->addr_base + RTC_IRQ_EN,
				RTC_IRQ_EN_ONESHOT_AL,
				RTC_IRQ_EN_ONESHOT_AL);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");
}

static void _rtc_get_tick(struct rtc_time *tm)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	ret = regmap_bulk_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];

	return;
exit:
	dev_err(mt_rtc->dev, "_rtc_get_tick regmap write/read error!!!\n");
}

static void _mtk_rtc_get_tick_time(struct rtc_time *tm)
{
	u32 bbpu, sec;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_BBPU, &bbpu);
	if (ret < 0)
		goto exit;

	bbpu |= RTC_BBPU_KEY | RTC_BBPU_RELOAD;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_BBPU, bbpu);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	_rtc_get_tick(tm);
	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_TC_SEC, &sec);
	if (ret < 0)
		goto exit;
	if (sec < tm->tm_sec) {	/* SEC has carried */
		_rtc_get_tick(tm);
	}

	return;

exit:
	dev_err(mt_rtc->dev, "_mtk_rtc_get_tick_time regmap write/read error!!!\n");
}

static void _mtk_rtc_get_pwron_alarm_time(struct rtc_time *tm)
{

	u32 spar1, pdn2, spar0;
	int ret;

	/*RTC_PWRON_SEC == SPAR0 */
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, &spar0);
	if (ret < 0)
		goto exit;
	/*RTC_PWRON_DOM == RTC_PWRON_HOU */
	/*== RTC_PWRON_MIN == RTC_SPAR1*/
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	/*RTC_PWRON_MTH == RTC_PWRON_YEAR== SPAR0 */
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;
	dev_err(mt_rtc->dev, "spar0=0x%x, spar1=0x%x, pdn2=0x%x!!!\n", spar0, spar1, pdn2);

	tm->tm_sec = (spar0 & RTC_PWRON_SEC_MASK) >> RTC_PWRON_SEC_SHIFT;
	tm->tm_min = (spar1 & RTC_PWRON_MIN_MASK) >> RTC_PWRON_MIN_SHIFT;
	tm->tm_hour = (spar1 & RTC_PWRON_HOU_MASK) >> RTC_PWRON_HOU_SHIFT;
	tm->tm_mday = (spar1 & RTC_PWRON_DOM_MASK) >> RTC_PWRON_DOM_SHIFT;
	tm->tm_mon = (pdn2 & RTC_PWRON_MTH_MASK) >> RTC_PWRON_MTH_SHIFT;
	tm->tm_year = (pdn2 & RTC_PWRON_YEA_MASK) >> RTC_PWRON_YEA_SHIFT;
	dev_err(mt_rtc->dev, "year=0x%x,mon=0x%x,mday =0x%x hou=0x%x,min=0x%x,sec=0x%x\n",
	  tm->tm_year, tm->tm_mon, tm->tm_mday,
	  tm->tm_hour, tm->tm_min, tm->tm_sec);

	return;

exit:
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");
}

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static void _mtk_rtc_set_alarm_boot(void)
{
	u32 spar0;
	int ret;

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, &spar0);
	if (ret < 0)
		goto exit;

	spar0 |= RTC_SPAR0_ALARM_BOOT;
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, spar0);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");
}
#endif

static bool _mtk_rtc_is_pwron_alarm(struct rtc_time *nowtm, struct rtc_time *tm)
{
	u32 pdn1, sec;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	dev_err(mt_rtc->dev, "pdn1 = 0x%x!!!\n", pdn1);
	/* power-on time is available */
	if (pdn1 & RTC_PDN1_PWRON_TIME) {
		_mtk_rtc_get_tick_time(nowtm);
		ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_TC_SEC, &sec);
		if (ret < 0)
			goto exit;
		if (sec < nowtm->tm_sec) {	/* SEC has carried */
			_mtk_rtc_get_tick_time(nowtm);
		}
		_mtk_rtc_get_pwron_alarm_time(tm);
		return true;
	}
	return false;

exit:
	dev_err(mt_rtc->dev, "_mtk_rtc_is_pwron_alarm regmap write/read error!!!\n");
	return false;
}

static irqreturn_t mtk_rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6397_rtc *rtc = data;
	u32 irqsta, irqen;
	int ret;
	bool pwron_alm = false, pwron_alarm = false;
	struct rtc_time nowtm;
	struct rtc_time tm = { 0 };

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if ((ret >= 0) && (irqsta & RTC_IRQ_STA_AL)) {
		pwron_alarm = _mtk_rtc_is_pwron_alarm(&nowtm, &tm);
		nowtm.tm_year += RTC_MIN_YEAR;
		tm.tm_year += RTC_MIN_YEAR;
		if (pwron_alarm) {
			unsigned long now_time, time;

			now_time = mktime(nowtm.tm_year,
							nowtm.tm_mon,
							nowtm.tm_mday,
							nowtm.tm_hour,
							nowtm.tm_min,
							nowtm.tm_sec);
			time = mktime(tm.tm_year,
						tm.tm_mon,
						tm.tm_mday,
						tm.tm_hour,
						tm.tm_min,
						tm.tm_sec);
			/* power on */
			if (now_time >= time - 1 && now_time <= time + 4) {
				#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
				if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
					|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
					dev_err(mt_rtc->dev, "KPOC alarm!!!\n");
					time += 1;
					rtc_time_to_tm(time, &tm);
					tm.tm_year -= RTC_MIN_YEAR_OFFSET;
					tm.tm_mon += 1;
					/* tm.tm_sec += 1; */
					_mtk_rtc_set_alarm(&tm);
					_mtk_rtc_set_alarm_boot();
					mutex_unlock(&rtc->lock);
					machine_restart(NULL);
				} else {
					_mtk_rtc_save_pwron_alarm();
					pwron_alm = true;
				}
				#else
				_mtk_rtc_save_pwron_alarm();
				pwron_alm = true;
				#endif
			} else if (now_time < time) { /* set power-on alarm */
				dev_err(mt_rtc->dev, "KPOC alarm again!!!\n");
				if (tm.tm_sec == 0) {
					tm.tm_sec = 59;
					tm.tm_min -= 1;
				} else {
					tm.tm_sec -= 1;
				}
			_mtk_rtc_set_alarm(&tm);
			}
		}
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
		irqen = irqsta & ~RTC_IRQ_EN_AL;
		mutex_lock(&rtc->lock);
		if (regmap_write(rtc->regmap, rtc->addr_base + RTC_IRQ_EN,
				 irqen) < 0)
			mtk_rtc_write_trigger(rtc);
		mutex_unlock(&rtc->lock);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int __mtk_rtc_read_time(struct mt6397_rtc *rtc,
			       struct rtc_time *tm, int *sec)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC, sec);
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

void rtc_read_pwron_alarm(struct rtc_wkalrm *alm)
{
	struct rtc_time *tm;
	u32 pdn1, pdn2;
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	if (alm == NULL)
		return;

	dev_err(mt_rtc->dev, "rtc_read_pwron_alarm!!!\n");
	tm = &alm->time;
	mutex_lock(&mt_rtc->lock);
	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	ret = regmap_bulk_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;
	alm->enabled = (pdn1 & RTC_PDN1_PWRON_TIME ? (pdn2 & RTC_PDN2_PWRON_LOGO ? 3 : 2) : 0);
	/* return Power-On Alarm bit */
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];
	mutex_unlock(&mt_rtc->lock);
	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;
	dev_err(mt_rtc->dev, "power-on = %04d/%02d/%02d %02d:%02d:%02d (%d)(%d)\n",
			  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled, alm->pending);
	return;

exit:
	mutex_unlock(&mt_rtc->lock);
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");
}

static void _rtc_set_pwron_alarm_time(struct rtc_time *tm)
{
	u32 spar1, pdn2, spar0;
	int ret;
	u32 tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec;

	dev_err(mt_rtc->dev, "_rtc_save_pwron_time!!!\n");
	/*RTC_PWRON_YEAR == RTC_PWRON_MTH==PDN2 */
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	/*RTC_PWRON_DOM == RTC_PWRON_HOU */
	/*== RTC_PWRON_MIN == RTC_SPAR1*/
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, &spar0);
	if (ret < 0)
		goto exit;

	tm_year = (tm->tm_year << RTC_PWRON_YEA_SHIFT) & RTC_PWRON_YEA_MASK;
	tm_mon = (tm->tm_mon << RTC_PWRON_MTH_SHIFT) & RTC_PWRON_MTH_MASK;
	tm_mday = (tm->tm_mday << RTC_PWRON_DOM_SHIFT) & RTC_PWRON_DOM_MASK;
	tm_hour = (tm->tm_hour << RTC_PWRON_HOU_SHIFT) & RTC_PWRON_HOU_MASK;
	tm_min = (tm->tm_min << RTC_PWRON_MIN_SHIFT) & RTC_PWRON_MIN_MASK;
	tm_sec = (tm->tm_sec << RTC_PWRON_SEC_SHIFT) & RTC_PWRON_SEC_MASK;

	tm_year |= pdn2 & ~(RTC_PWRON_YEA_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, tm_year);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	tm_mon |= pdn2 & ~(RTC_PWRON_MTH_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, tm_mon);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	tm_mday |= spar1 & ~(RTC_PWRON_DOM_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, tm_mday);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	tm_hour |= spar1 & ~(RTC_PWRON_HOU_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, tm_hour);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	tm_min |= spar1 & ~(RTC_PWRON_MIN_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, tm_min);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);


	tm_sec |= spar0 & ~(RTC_PWRON_SEC_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, tm_sec);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	mutex_unlock(&mt_rtc->lock);
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");

}

void mtk_rtc_save_pwron_time(bool enable, struct rtc_time *tm, bool logo)
{
	u32 pdn1, pdn2;
	int ret;

	dev_err(mt_rtc->dev, "mtk_rtc_save_pwron_time!!!\n");
	_rtc_set_pwron_alarm_time(tm);

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;
	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	if (logo)
		pdn2 |= RTC_PDN2_PWRON_LOGO;
	else
		pdn2 &= ~RTC_PDN2_PWRON_LOGO;

	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, pdn2);
	if (ret < 0)
		goto exit;

	if (enable)
		pdn1 |= RTC_PDN1_PWRON_TIME;
	else
		pdn1 &= ~RTC_PDN1_PWRON_TIME;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, pdn1);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	mutex_unlock(&mt_rtc->lock);
	dev_err(mt_rtc->dev, "regmap write/read error!!!\n");

}

static int mtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	time64_t time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int days, sec, ret;

	do {
		ret = __mtk_rtc_read_time(rtc, tm, &sec);
		if (ret < 0)
			goto exit;
	} while (sec < tm->tm_sec);

	/* HW register use 7 bits to store year data, minus
	 * RTC_MIN_YEAR_OFFSET before write year data to register, and plus
	 * RTC_MIN_YEAR_OFFSET back after read year from register
	 */
	tm->tm_year += RTC_MIN_YEAR_OFFSET;

	/* HW register start mon from one, but tm_mon start from zero. */
	tm->tm_mon--;
	time = rtc_tm_to_time64(tm);

	/* rtc_tm_to_time64 covert Gregorian date to seconds since
	 * 01-01-1970 00:00:00, and this date is Thursday.
	 */
	days = div_s64(time, 86400);
	tm->tm_wday = (days + 4) % 7;

exit:
	return ret;
}

static int mtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_write(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
				data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	/* Time register write to hardware after call trigger function */
	ret = mtk_rtc_write_trigger(rtc);

exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	u32 irqen, pdn2;
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	mutex_lock(&rtc->lock);
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_EN, &irqen);
	if (ret < 0)
		goto err_exit;
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto err_exit;

	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto err_exit;

	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);
	mutex_unlock(&rtc->lock);

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

	return 0;
err_exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	dev_err(rtc->dev, "set al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		  tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	mutex_lock(&rtc->lock);
	switch (alm->enabled) {
	case 2:
		/* enable power-on alarm */
		mtk_rtc_save_pwron_time(true, tm, false);
		break;
	case 3:
		/* enable power-on alarm with logo */
		mtk_rtc_save_pwron_time(true, tm, true);
		break;
	case 4:
		/* disable power-on alarm */
		mtk_rtc_save_pwron_time(false, tm, false);
		break;
	default:
		break;
	}
	if (alm->enabled) {
		ret = regmap_bulk_write(rtc->regmap,
					rtc->addr_base + RTC_AL_SEC,
					data, RTC_OFFSET_COUNT);
		if (ret < 0)
			goto exit;
		ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_AL_MASK,
				   RTC_AL_MASK_DOW);
		if (ret < 0)
			goto exit;
		ret = regmap_update_bits(rtc->regmap,
					 rtc->addr_base + RTC_IRQ_EN,
					 RTC_IRQ_EN_ONESHOT_AL,
					 RTC_IRQ_EN_ONESHOT_AL);
		if (ret < 0)
			goto exit;
	} else {
		ret = regmap_update_bits(rtc->regmap,
					 rtc->addr_base + RTC_IRQ_EN,
					 RTC_IRQ_EN_ONESHOT_AL, 0);
		if (ret < 0)
			goto exit;
	}

	/* All alarm time register write to hardware after calling
	 * mtk_rtc_write_trigger. This can avoid race condition if alarm
	 * occur happen during writing alarm time register.
	 */
	ret = (int)mtk_rtc_write_trigger(rtc);
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static struct rtc_class_ops mtk_rtc_ops = {
	.read_time  = mtk_rtc_read_time,
	.set_time   = mtk_rtc_set_time,
	.read_alarm = mtk_rtc_read_alarm,
	.set_alarm  = mtk_rtc_set_alarm,
};

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_rtc *rtc;
	int ret;
	u32 data;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6397_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->addr_base = res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	rtc->irq = irq_create_mapping(mt6397_chip->irq_domain, res->start);
	if (rtc->irq <= 0)
		return -EINVAL;

	rtc->regmap = mt6397_chip->regmap;
	rtc->dev = &pdev->dev;
	mutex_init(&rtc->lock);

	mt_rtc = rtc;
	is_xosc = false;
	rtc_boot_check();
	rtc_bbpu_power_on();
#if 1
	ret = regmap_read(mt_rtc->regmap, 0x0402,
			  &data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read chip id: %d\n", ret);
		return ret;
	}

	data &= 0xF7FF;
	data |= 0x1;
	ret = regmap_write(mt_rtc->regmap, 0x0402, data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read chip id: %d\n", ret);
		return ret;
	}
#endif
	platform_set_drvdata(pdev, rtc);

	ret = request_threaded_irq(rtc->irq, NULL,
				   mtk_rtc_irq_handler_thread,
				   IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				   "mt6397-rtc", rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		goto out_dispose_irq;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc_dev = rtc_device_register("mt6397-rtc", &pdev->dev,
					   &mtk_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		dev_err(&pdev->dev, "register rtc device failed\n");
		ret = PTR_ERR(rtc->rtc_dev);
		goto out_free_irq;
	}

	return 0;

out_free_irq:
	free_irq(rtc->irq, rtc->rtc_dev);
out_dispose_irq:
	irq_dispose_mapping(rtc->irq);
	return ret;
}

static int mtk_rtc_remove(struct platform_device *pdev)
{
	struct mt6397_rtc *rtc = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc->rtc_dev);
	free_irq(rtc->irq, rtc->rtc_dev);
	irq_dispose_mapping(rtc->irq);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt6397_rtc_suspend(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(rtc->irq);

	return 0;
}

static int mt6397_rtc_resume(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(rtc->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mt6397_pm_ops, mt6397_rtc_suspend,
			mt6397_rtc_resume);

static const struct of_device_id mt6397_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6397-rtc", },
	{ .compatible = "mediatek,mt6323-rtc", },
	{ .compatible = "mediatek,mt6392-rtc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_rtc_of_match);

static struct platform_driver mtk_rtc_driver = {
	.driver = {
		.name = "mt6397-rtc",
		.of_match_table = mt6397_rtc_of_match,
		.pm = &mt6397_pm_ops,
	},
	.probe	= mtk_rtc_probe,
	.remove = mtk_rtc_remove,
};

module_platform_driver(mtk_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6397 PMIC");
MODULE_ALIAS("platform:mt6397-rtc");
