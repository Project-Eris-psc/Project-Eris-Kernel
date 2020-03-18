#ifndef __MT_RTC_HW_H__
#define __MT_RTC_HW_H__

/* RTC registers */
#define RTC_BBPU		0x0000
#define RTC_BBPU_PWREN		(1U << 0)	/* BBPU = 1 when alarm occurs */
#define RTC_BBPU_BBPU		(1U << 2)	/* 1: power on, 0: power down */
#define RTC_BBPU_AUTO		(1U << 3)	/* BBPU = 0 when xreset_rstb goes low */
#define RTC_BBPU_RELOAD		(1U << 5)
#define RTC_BBPU_CBUSY		(1U << 6)
#define RTC_BBPU_KEY		(0x43 << 8)

#define RTC_IRQ_STA		0x0002
#define RTC_IRQ_STA_AL		(1U << 0)
#define RTC_IRQ_STA_LP		(1U << 3)

#define RTC_IRQ_EN		0x0004
#define RTC_IRQ_EN_AL		(1U << 0)
#define RTC_IRQ_EN_ONESHOT	(1U << 2)
#define RTC_IRQ_EN_LP		(1U << 3)
#define RTC_IRQ_EN_ONESHOT_AL	(RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_CII_EN		0x0006
#define RTC_CII_SEC		(1U << 0)
#define RTC_CII_MIN		(1U << 1)
#define RTC_CII_HOU		(1U << 2)
#define RTC_CII_DOM		(1U << 3)
#define RTC_CII_DOW		(1U << 4)
#define RTC_CII_MTH		(1U << 5)
#define RTC_CII_YEA		(1U << 6)
#define RTC_CII_1_2_SEC		(1U << 7)
#define RTC_CII_1_4_SEC		(1U << 8)
#define RTC_CII_1_8_SEC		(1U << 9)

#define RTC_AL_MASK		0x0008
#define RTC_AL_MASK_SEC		(1U << 0)
#define RTC_AL_MASK_MIN		(1U << 1)
#define RTC_AL_MASK_HOU		(1U << 2)
#define RTC_AL_MASK_DOM		(1U << 3)
#define RTC_AL_MASK_DOW		(1U << 4)
#define RTC_AL_MASK_MTH		(1U << 5)
#define RTC_AL_MASK_YEA		(1U << 6)

#define RTC_TC_SEC		0x000a
#define RTC_TC_SEC_MASK		0x003f

#define RTC_TC_MIN		0x000c
#define RTC_TC_MIN_MASK		0x003f

#define RTC_TC_HOU		0x000e
#define RTC_TC_HOU_MASK		0x001f

#define RTC_TC_DOM		0x0010
#define RTC_TC_DOM_MASK		0x001f

#define RTC_TC_DOW		0x0012
#define RTC_TC_DOW_MASK		0x0007

#define RTC_TC_MTH		0x0014
#define RTC_TC_MTH_MASK		0x000f

#define RTC_TC_YEA		0x0016
#define RTC_TC_YEA_MASK		0x007f

#define RTC_AL_SEC		0x0018
#define RTC_AL_SEC_MASK		0x003f

#define RTC_AL_MIN		0x001a
#define RTC_AL_MIN_MASK		0x003f

/*
 * RTC_NEW_SPARE0: RTC_AL_HOU bit0~4
 *    bit 8 ~ 14 : Fuel Gauge
 *    bit 15     : reserved bits
 */
#define RTC_AL_HOU		0x001c
#define RTC_NEW_SPARE_FG_MASK	0xff00
#define RTC_NEW_SPARE_FG_SHIFT	8
#define RTC_AL_HOU_MASK		0x001f

/*
 * RTC_NEW_SPARE1: RTC_AL_DOM bit0~4
 *    bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_DOM		0x001e
#define RTC_NEW_SPARE1		0xff00
#define RTC_AL_DOM_MASK		0x001f

/*
 * RTC_NEW_SPARE2: RTC_AL_DOW bit0~2
 *    bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_DOW		0x0020
#define RTC_NEW_SPARE2		0xff00
#define RTC_AL_DOW_MASK		0x0007

/*
 * RTC_NEW_SPARE3: RTC_AL_MTH bit0~3
 *    bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_MTH		0x0022
#define RTC_NEW_SPARE3		0xff00
#define RTC_AL_MTH_MASK		0x000f

#define RTC_AL_YEA		0x0024
#define RTC_AL_YEA_MASK		0x007f

#define RTC_OSC32CON		0x0026
#define RTC_OSC32CON_UNLOCK1	0x1a57
#define RTC_OSC32CON_UNLOCK2	0x2b68
#define RTC_OSC32CON_XOSC32_ENB	(1U << 13)
#define RTC_OSC32CON_LNBUFEN	(1U << 11)	/* ungate 32K to ABB */
#define RTC_OSC32CON_EOSC32_CHOP_EN	(1U << 10)
#define RTC_OSC32CON_EMBCK_SEL		(1U << 7)
#define RTC_OSC32CON_EMBCK_SEL_MODE	(1U << 6)
#define RTC_OSC32CON_XOSCCALI_MASK	0x001f

#define RTC_XOSCCALI_START		0x0000
#define RTC_XOSCCALI_END		0x001f

#define RTC_EOSC_CALI_LEFT	(RTC_OSC32CON_XOSC32_ENB | RTC_OSC32CON_EOSC32_CHOP_EN | RTC_OSC32CON_EMBCK_SEL | RTC_OSC32CON_EMBCK_SEL_MODE | RTC_XOSCCALI_START)
#define RTC_EOSC_CALI_RIGHT	(RTC_OSC32CON_XOSC32_ENB | RTC_OSC32CON_EOSC32_CHOP_EN | RTC_OSC32CON_EMBCK_SEL | RTC_OSC32CON_EMBCK_SEL_MODE | RTC_XOSCCALI_END)

#define RTC_POWERKEY1		0x0028
#define RTC_POWERKEY2		0x002a
#define RTC_POWERKEY1_KEY	0xa357
#define RTC_POWERKEY2_KEY	0x67d2
/*
 * RTC_PDN1:
 *     bit 0 - 3  : Android bits
 *     bit 4 - 5  : Recovery bits (0x10: factory data reset)
 *     bit 6      : Bypass PWRKEY bit
 *     bit 7      : Power-On Time bit
 *     bit 8      : RTC_GPIO_USER_WIFI bit
 *     bit 9      : RTC_GPIO_USER_GPS bit
 *     bit 10     : RTC_GPIO_USER_BT bit
 *     bit 11     : RTC_GPIO_USER_FM bit
 *     bit 12     : RTC_GPIO_USER_PMIC bit
 *     bit 13     : Fast Boot
 *     bit 14     : Kernel Power Off Charging
 *     bit 15     : Debug bit
 */
#define RTC_PDN1		0x002c
#define RTC_PDN1_ANDROID_MASK	0x000f
#define RTC_PDN1_RECOVERY_MASK	0x0030
#define RTC_PDN1_FAC_RESET	(1U << 4)
#define RTC_PDN1_BYPASS_PWR	(1U << 6)
#define RTC_PDN1_PWRON_TIME	(1U << 7)
#define RTC_PDN1_GPIO_WIFI	(1U << 8)
#define RTC_PDN1_GPIO_GPS	(1U << 9)
#define RTC_PDN1_GPIO_BT	(1U << 10)
#define RTC_PDN1_GPIO_FM	(1U << 11)
#define RTC_PDN1_GPIO_PMIC	(1U << 12)
#define RTC_PDN1_FAST_BOOT	(1U << 13)
#define RTC_PDN1_KPOC		(1U << 14)
#define RTC_PDN1_DEBUG		(1U << 15)

/*
 * RTC_PDN2:
 *     bit 0 - 3 : MTH in power-on time
 *     bit 4     : Power-On Alarm bit
 *     bit 5 - 6 : UART bits
 *     bit 7     : reserved bit
 *     bit 8 - 14: YEA in power-on time
 *     bit 15    : Power-On Logo bit
 */
#define RTC_PDN2			0x002e
#define RTC_PDN2_PWRON_MTH_MASK		0x000f
#define RTC_PDN2_PWRON_MTH_SHIFT	0
#define RTC_PDN2_PWRON_ALARM		(1U << 4)
#define RTC_PDN2_UART_MASK		0x0060
#define RTC_PDN2_UART_SHIFT		5
#define RTC_PDN2_PWRON_YEA_MASK		0x7f00
#define RTC_PDN2_AUTOBOOT		(1U << 7)
#define RTC_PDN2_PWRON_YEA_SHIFT	8
#define RTC_PDN2_PWRON_LOGO		(1U << 15)

/*
 * RTC_SPAR0:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6     : 32K less bit. True:with 32K, False:Without 32K
 *     bit 9 - 15: reserved bits
 */
#define RTC_SPAR0			0x0030
#define RTC_SPAR0_PWRON_SEC_MASK 	0x003f
#define RTC_SPAR0_PWRON_SEC_SHIFT 	0
#define RTC_SPAR0_32K_LESS 		(1U << 6)
#define RTC_SPAR0_LP_DET		(1U << 7)
#define RTC_SPAR0_ALARM_BOOT		(1U << 8)

/*
 * RTC_SPAR1:
 *     bit 0 - 5  : MIN in power-on time
 *     bit 6 - 10 : HOU in power-on time
 *     bit 11 - 15: DOM in power-on time
 */
#define RTC_SPAR1			0x0032
#define RTC_SPAR1_PWRON_MIN_MASK	0x003f
#define RTC_SPAR1_PWRON_MIN_SHIFT	0
#define RTC_SPAR1_PWRON_HOU_MASK	0x07c0
#define RTC_SPAR1_PWRON_HOU_SHIFT	6
#define RTC_SPAR1_PWRON_DOM_MASK	0xf800
#define RTC_SPAR1_PWRON_DOM_SHIFT	11

#define RTC_PROT			0x0036
#define RTC_PROT_UNLOCK1		0x586a
#define RTC_PROT_UNLOCK2		0x9136

#define RTC_DIFF			0x0038
#define RTC_CALI			0x003a
#define RTC_WRTGR			0x003c

#define RTC_CON				0x003e
#define RTC_CON_LPEN			(1U << 2)
#define RTC_CON_LPRST			(1U << 3)
#define RTC_CON_CDBO			(1U << 4)
#define RTC_CON_F32KOB			(1U << 5)	/* 0: RTC_GPIO exports 32K */
#define RTC_CON_GPO			(1U << 6)
#define RTC_CON_GOE			(1U << 7)	/* 1: GPO mode, 0: GPI mode */
#define RTC_CON_GSR			(1U << 8)
#define RTC_CON_GSMT			(1U << 9)
#define RTC_CON_GPEN			(1U << 10)
#define RTC_CON_GPU			(1U << 11)
#define RTC_CON_GE4			(1U << 12)
#define RTC_CON_GE8			(1U << 13)
#define RTC_CON_GPI			(1U << 14)
#define RTC_CON_LPSTA_RAW		(1U << 15)	/* 32K was stopped */

#define RTC_CALI_BBPU_2SEC_EN		(1U << 8)
#define RTC_CALI_BBPU_2SEC_MODE_SHIFT	9
#define RTC_CALI_BBPU_2SEC_MODE_MSK	(3U << RTC_CALI_BBPU_2SEC_MODE_SHIFT)
#define RTC_CALI_BBPU_2SEC_STAT		(1U << 11)
#define RTC_FQMTR_LOW_BASE		(794 - 2)
#define RTC_FQMTR_HIGH_BASE		(794 + 2)

#define FQMTR_FIX_CLK_26M		(0)
#define FQMTR_FIX_CLK_XOSC_32K_DET	(2)
#define FQMTR_FIX_CLK_EOSC_32K		(3)
#define FQMTR_FIX_CLK_PMU_75K		(5)
#define RG_FQMTR_RST			(1U << MT6392_PMIC_RG_FQMTR_RST_SHIFT)
#define RG_FQMTR_PDN			(1U << MT6392_PMIC_RG_FQMTR_PDN_SHIFT)
#define FQMTR_EN			(1U << MT6392_PMIC_FQMTR_EN_SHIFT)
#define FQMTR_BUSY			(1U << MT6392_PMIC_FQMTR_BUSY_SHIFT)
#define FQMTR_TCKSEL_MSK		MT6392_PMIC_FQMTR_TCKSEL_MASK
#define FQMTR_XOSC32_CK			0
#define FQMTR_DCXO_F32K_CK		1
#define FQMTR_EOSC32_CK			2
#define FQMTR_XOSC32_CK_DETECTON	3
#define FQMTR_AUD26M_CK			4
#define FQMTR_WINSET			0x0000
#define CLKSQ_EN			(1U << MT6392_PMIC_RG_CLKSQ_EN_SHIFT)

#define RTC_HW_DET_BYPASS		(8 << MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT)
#define RTC_HW_XOSC_MODE		(~(2 << MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT))
#define RTC_HW_DCXO_MODE		(2 << MT6392_PMIC_RTC_XTAL_DET_RSV_SHIFT)

/* we map HW YEA 0 (2000) to 1968 not 1970 because 2000 is the leap year */
#define RTC_MIN_YEAR			1968
#define RTC_NUM_YEARS			128
//#define RTC_MAX_YEAR          (RTC_MIN_YEAR + RTC_NUM_YEARS - 1)
#define U16  u16

extern void rtc_bbpu_power_on(void);

extern void rtc_mark_bypass_pwrkey(void);

extern U16 rtc_rdwr_uart_bits(U16 *val);

extern void rtc_boot_check(void);

extern void pl_power_off(void);

extern bool rtc_2sec_reboot_check(void);

#endif /* __MT_RTC_HW_H__ */

