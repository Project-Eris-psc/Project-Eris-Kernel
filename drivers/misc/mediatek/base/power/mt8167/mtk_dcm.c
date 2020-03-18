/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if defined(__KERNEL__)
#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/bug.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/sync_write.h>
#include "mtk_dcm.h"
#else
#include <sync_write.h>
#include <common.h>
#endif

static void __iomem *topckctrl_base;
static void __iomem *mcucfg_base;
static void __iomem *emi_reg_base;
static void __iomem *infracfg_ao_base;
static phys_addr_t mcucfg_phys_base;

#define MCUCFG_NODE			"mediatek,mt8167-mcucfg"
#define INFRACFG_AO_NODE		"mediatek,mt8167-infracfg"
#define TOPCKGEN_NODE			"mediatek,mt8167-topckgen"
#define EMI_REG_NODE			"mediatek,mt8167-emi"

#undef INFRACFG_AO_BASE
#undef MCUCFG_BASE
#undef TOPCKCTRL_BASE
#define INFRACFG_AO_BASE		(infracfg_ao_base)	/*0x10001000 */
#define MCUCFG_BASE			(mcucfg_base)		/*0x10200000 */
#define TOPCKCTRL_BASE			(topckctrl_base)	/*0x10000000 */
#define EMI_REG_BASE			(emi_reg_base)		/*0x10205000 */

/* Lyra MCUCFG */
#define MP0_CA7L_DBG_PWR_CTRL		(MCUCFG_BASE + 0x0068)
#define CCI_ADB400_DCM_CONFIG		(MCUCFG_BASE + 0x0114)
#define L2C_SRAM_CTRL			(MCUCFG_BASE + 0x0648)
#define CCI_STATUS			(MCUCFG_BASE + 0x0660)
#define MCUSYS_BUS_FABRIC_DCM_CTRL	(MCUCFG_BASE + 0x0668)
#define MCU_MISC_DCM_CTRL		(MCUCFG_BASE + 0x066C)
#define MP_CCI_ADB400_DCM_CONFIG	(MCUCFG_BASE + 0x0740)
#define CCI_BW_PMU_CTL			(MCUCFG_BASE + 0x0800)
#define MP0_RGU_DCM_CONFIG		(MCUCFG_BASE + 0x1C70)

/* INFRASYS_AO */
#define INFRA_TOPCKGEN_DCMDBC		(INFRACFG_AO_BASE + 0x014)	/* 0x10001014 */

/* TOP CLOCK CTRL */
#define TOPBUS_DCMCTL			(TOPCKCTRL_BASE + 0x008)	/* 0x10000008 */
#define TOPEMI_DCMCTL			(TOPCKCTRL_BASE + 0x00C)	/* 0x1000000C */
#define INFRABUS_DCMCTL0		(TOPCKCTRL_BASE + 0x028)	/* 0x10000028 */
#define INFRABUS_DCMCTL1		(TOPCKCTRL_BASE + 0x02C)	/* 0x1000002c */
#define MPLL_FREDIV_EN			(TOPCKCTRL_BASE + 0x030)	/* 0x10000030 */

#define EMI_CONH			(EMI_REG_BASE + 0x38)	/* 0x10205038 */
#define EMI_CONM			(EMI_REG_BASE + 0x60)	/* 0x10205060 */

#define USING_KernelLOG
#define TAG				"[Power/dcm] "

#define dcm_err(fmt, args...)		pr_err(TAG fmt, ##args)
#define dcm_warn(fmt, args...)		pr_warn(TAG fmt, ##args)
#define dcm_info(fmt, args...)		pr_info(TAG fmt, ##args)
#define dcm_ver(fmt, args...)		pr_debug(TAG fmt, ##args)
#define dcm_dbg(fmt, args...)		pr_debug(TAG fmt, ##args)

/** macro **/
#define and(v, a) ((v) & (a))
#define or(v, o) ((v) | (o))
#define aor(v, a, o) (((v) & (a)) | (o))

#define reg_read(addr)		__raw_readl(addr)
#define reg_write(addr, val)	mt_reg_sync_writel(val, addr)

#define MCUSYS_SMC_WRITE(addr, val)	mt_reg_sync_writel(val, addr)

#define DCM_OFF (0)
#define DCM_ON (1)

/** global **/
static DEFINE_MUTEX(dcm_lock);
static unsigned int dcm_initiated;

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
typedef int (*DCM_FUNC) (int);

/** 0x10001014	INFRA_TOPCKGEN_DCMDBC
 * 6	0	topckgen_dcm_dbc_cnt.
 * BUT, this field does not behave as its name.
 * only topckgen_dcm_dbc_cnt[0] is using as ARMPLL DCM mode 1 switch.
 **/

typedef enum {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
} ENUM_ARMCORE_DCM;

int dcm_armcore(ENUM_ARMCORE_DCM mode)
{
	if (mode == ARMCORE_DCM_OFF) {
		/* disable mode 1 */
		reg_write(INFRA_TOPCKGEN_DCMDBC, and(reg_read(INFRA_TOPCKGEN_DCMDBC), ~(1 << 0)));
	} else if (mode == ARMCORE_DCM_MODE1) {
		/* enable mode 1 */
		reg_write(INFRA_TOPCKGEN_DCMDBC, aor(reg_read(INFRA_TOPCKGEN_DCMDBC), ~(1 << 0), (1 << 0)));
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/**************************
 * MCUSYS DCM
 **************************/
typedef enum {
	MCUSYS_DCM_OFF = DCM_OFF,
	MCUSYS_DCM_ON = DCM_ON,
} ENUM_MCUSYS_DCM;

/* Lyra MCUCFG DCM setting */
#define DBGPWRDUP				0x0F
#define CCI_ADB400_DCM_CONFIG_SETTING		0x03
#define L2C_SRAM_DCM_EN				0x01
#define MCU_BUS_DCM_EN				0x100
#define MCUSYS_BUS_FABRIC_DCM_CTRL_SETTING	0xFFF1FFF
#define MP1_CNTVALUEB_DCM_EN			0x02
#define MP_CCI_ADB400_DCM_CONFIG_SETTING	0x45
#define CCI_BW_MON_DCM_DIS			0x100

int dcm_mcusys(ENUM_MCUSYS_DCM on)
{
	if (on == MCUSYS_DCM_OFF) {
		reg_write(CCI_ADB400_DCM_CONFIG, and(reg_read(CCI_ADB400_DCM_CONFIG),
								~CCI_ADB400_DCM_CONFIG_SETTING));
		reg_write(L2C_SRAM_CTRL, and(reg_read(L2C_SRAM_CTRL), ~L2C_SRAM_DCM_EN));
		reg_write(CCI_STATUS, and(reg_read(CCI_STATUS), ~MCU_BUS_DCM_EN));
		reg_write(MCUSYS_BUS_FABRIC_DCM_CTRL, and(reg_read(MCUSYS_BUS_FABRIC_DCM_CTRL),
								~MCUSYS_BUS_FABRIC_DCM_CTRL_SETTING));
		reg_write(MCU_MISC_DCM_CTRL, and(reg_read(MCU_MISC_DCM_CTRL), ~MP1_CNTVALUEB_DCM_EN));
		reg_write(MP_CCI_ADB400_DCM_CONFIG, and(reg_read(MP_CCI_ADB400_DCM_CONFIG),
								~MP_CCI_ADB400_DCM_CONFIG_SETTING));
	} else if (on == MCUSYS_DCM_ON) {
		reg_write(CCI_ADB400_DCM_CONFIG, aor(reg_read(CCI_ADB400_DCM_CONFIG),
						~CCI_ADB400_DCM_CONFIG_SETTING, CCI_ADB400_DCM_CONFIG_SETTING));
		reg_write(L2C_SRAM_CTRL, aor(reg_read(L2C_SRAM_CTRL), ~L2C_SRAM_DCM_EN, L2C_SRAM_DCM_EN));
		reg_write(CCI_STATUS, aor(reg_read(CCI_STATUS), ~MCU_BUS_DCM_EN, MCU_BUS_DCM_EN));
		reg_write(MCUSYS_BUS_FABRIC_DCM_CTRL, aor(reg_read(MCUSYS_BUS_FABRIC_DCM_CTRL),
					~MCUSYS_BUS_FABRIC_DCM_CTRL_SETTING, MCUSYS_BUS_FABRIC_DCM_CTRL_SETTING));
		reg_write(MCU_MISC_DCM_CTRL, aor(reg_read(MCU_MISC_DCM_CTRL),
					~MP1_CNTVALUEB_DCM_EN, MP1_CNTVALUEB_DCM_EN));
		reg_write(MP_CCI_ADB400_DCM_CONFIG, aor(reg_read(MP_CCI_ADB400_DCM_CONFIG),
					~MP_CCI_ADB400_DCM_CONFIG_SETTING, MP_CCI_ADB400_DCM_CONFIG_SETTING));
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}


/** 0x10000010	INFRA_TOPCKGEN_DCMCTL
 * 0	0	infra_dcm_enable
 * this field actually is to activate clock ratio between infra/fast_peri/slow_peri.
 * and need to set when bus clock switch from CLKSQ to PLL.
 * do ASSERT, for each time infra/peri bus dcm setting.
 **/
#define ASSERT_INFRA_DCMCTL() \
	do {      \
		volatile unsigned int dcmctl;                           \
		dcmctl = reg_read(INFRA_TOPCKGEN_DCMCTL);               \
		WARN_ON(!(dcmctl & 1));                                  \
	} while (0)


/** 0x10000028 INFRABUS_DCMCTL0
* 31 31 peridcm_force_on
* 29 29 peridcm_force_clk_off
* 28 28 peridcm_force_clk_slow
* 27 27 peridcm_clkoff_en
* 26 26 peridcm_clkslw_en
* 25 21 peridcm_sfsel
	(0x10000 /1
	 0x01000 /2
	 0x00100 /4
	 0x00010 /8
	 0x00001 /16)
* 20 16 peridcm_fsel
	(0x10000 /1
	 0x01000 /2
	 0x00100 /4
	 0x00010 /8
	 0x00001 /16)
* 15 15 infradcm_force_on
* 13 13 infradcm_force_clk_off
* 12 12 infradcm_force_clk_slow
* 11 11 infradcm_clkoff_en
* 10 10 infradcm_clkslw_en
* 9   5 infradcm_sfsel
	(0x10000 /1
	 0x01000 /2
	 0x00100 /4
	 0x00010 /8
	 0x00001 /16)
* 4  0  infradcm_fsel
	(0x10000 /1
	 0x01000 /2
	 0x00100 /4
	 0x00010 /8
	 0x00001 /16)
**/
#define PERI_DCM_EN_MASK ((1<<27)|(1<<27))
#define PERI_DCM_EN_ON	  ((1<<27)|(1<<26))
#define PERI_DCM_EN_OFF  ((0<<27)|(0<<26))
#define PERI_DCM_SFSEL_MASK (0x1f<<21)
#define PERI_DCM_FSEL_MASK	 (0x1f<<16)

#define INFRA_DCM_EN_MASK ((1<<11)|(1<<10))
#define INFRA_DCM_EN_ON	  ((1<<11)|(1<<10))
#define INFRA_DCM_EN_OFF  ((0<<11)|(0<<10))
#define INFRA_DCM_SFSEL_MASK (0x1f<<5)
#define INFRA_DCM_FSEL_MASK	 (0x1f<<0)


/** 0x1000002c INFRABUS_DCMCTL1
* 31 31 busdcm_pllck_sel
19 15 periDCM_debouce_cnt
14 14 periDCM_debounce_en
13 9  infraDCM_debouce_cnt
8   8    inraDCM_debounce_en
7   3    pmic_sfel
2   2  pmic_spiclkdcm_en
1   1  pmic_bclkdcm_en
0   0  usbdcm_en
**/
#define BUSFCM_PLLCK_SEL_MASK (1<<31)
#define PERI_DCM_DBC_CNT_MASK     (0x1f<<15)
#define PERI_DCM_DBC_EN_MASK      (1<<14)
#define INFRA_DCM_DBC_CNT_MASK	  (0x1f<<9)
#define INFRA_DCM_DBC_EN_MASK     (1<<8)
#define PMIC_SFSEL_MASK    (0x1f<<3)
#define PMIC_SPICLKDCM_EN_MASK (1<<2)
#define PMIC_BCLKDCM_EN_MASK (1<<1)
#define USBDCM_EN_MASK (1<<0)

#define PMIC_DCM_EN_ON   ((1<<2)|(1<<1))
#define PMIC_DCM_EN_OFF  ((0<<2)|(0<<1))

#define USB_DCM_EN_ON  (1<<0)
#define USB_DCM_EN_OFF (0<<0)


typedef enum {
	INFRA_DCM_OFF = DCM_OFF,
	INFRA_DCM_ON = DCM_ON,
} ENUM_INFRA_DCM;


typedef enum {
	INFRA_DCM_DBC_OFF = DCM_OFF,
	INFRA_DCM_DBC_ON = DCM_ON,
} ENUM_INFRA_DCM_DBC;


/* cnt : 0~0x1f */
int dcm_infra_dbc(ENUM_INFRA_DCM_DBC on, int cnt)
{
	int value;

	cnt &= 0x1f;
	on = (on != 0) ? 1 : 0;
	value = (cnt << 9) | (on << 8);

	reg_write(INFRABUS_DCMCTL1, aor(reg_read(INFRABUS_DCMCTL1),
					~(INFRA_DCM_DBC_CNT_MASK | INFRA_DCM_DBC_EN_MASK), value));

	return 0;
}

/**
 * 5'b10000: /1
 * 5'b01000: /2
 * 5'b00100: /4
 * 5'b00010: /8
 * 5'b00001: /16
 * 5'b00000: /32
 */
typedef enum {
	INFRA_DCM_SFSEL_DIV_1 = 0x10,
	INFRA_DCM_SFSEL_DIV_2 = 0x08,
	INFRA_DCM_SFSEL_DIV_4 = 0x04,
	INFRA_DCM_SFSEL_DIV_8 = 0x02,
	INFRA_DCM_SFSEL_DIV_16 = 0x01,
	INFRA_DCM_SFSEL_DIV_32 = 0x00,
} ENUM_INFRA_SFSEL;

int dcm_infra_sfsel(int cnt)
{
	int value;

	cnt &= 0x1f;
	value = (cnt << 5);

	reg_write(INFRABUS_DCMCTL0, aor(reg_read(INFRABUS_DCMCTL0), ~INFRA_DCM_SFSEL_MASK, value));

	return 0;
}


/** input argument
 * 0: 1/1
 * 1: 1/2
 * 2: 1/4
 * 3: 1/8
 * 4: 1/16
 * 5: 1/32
 **/
int dcm_infra_rate(unsigned full, unsigned int sfel)
{


	full = 0x10 >> full;
	sfel = 0x10 >> sfel;

	reg_write(INFRABUS_DCMCTL0, aor(reg_read(INFRABUS_DCMCTL0),
					~(PERI_DCM_SFSEL_MASK | INFRA_DCM_FSEL_MASK),
					(full << 0) | (sfel << 5)));

	return 0;
}

#define TOPBUS_DCMCTL_SETTING		0x17
#define BUSDCM_APB_TOG			0x01

int dcm_infra(ENUM_INFRA_DCM on)
{

	/* ASSERT_INFRA_DCMCTL(); */

	if (on) {
#if 1		/* override the dbc and fsel setting !! */
		dcm_infra_dbc(INFRA_DCM_DBC_ON, 0x7);		/* dbc 8 */
		dcm_infra_sfsel(INFRA_DCM_SFSEL_DIV_32);	/* sfsel /32 */
#endif
		reg_write(INFRABUS_DCMCTL0,
			  aor(reg_read(INFRABUS_DCMCTL0), ~INFRA_DCM_EN_MASK, INFRA_DCM_EN_ON));
		reg_write(INFRABUS_DCMCTL1, aor(reg_read(INFRABUS_DCMCTL1), ~BUSFCM_PLLCK_SEL_MASK,
										BUSFCM_PLLCK_SEL_MASK));
		reg_write(TOPBUS_DCMCTL, aor(reg_read(TOPBUS_DCMCTL), ~TOPBUS_DCMCTL_SETTING,
										TOPBUS_DCMCTL_SETTING));
		reg_write(TOPBUS_DCMCTL, and(reg_read(TOPBUS_DCMCTL), ~BUSDCM_APB_TOG));
		reg_write(TOPBUS_DCMCTL, aor(reg_read(TOPBUS_DCMCTL), ~BUSDCM_APB_TOG, BUSDCM_APB_TOG));
	} else {
#if 1		/* override the dbc and fsel setting !! */
		dcm_infra_dbc(INFRA_DCM_DBC_ON, 0x0);		/* dbc 0 */
		reg_write(INFRABUS_DCMCTL1, and(reg_read(INFRABUS_DCMCTL1),
							~INFRA_DCM_DBC_EN_MASK)); /* Disable DBC_EN */
		dcm_infra_sfsel(INFRA_DCM_SFSEL_DIV_32);	/*sfsel /32 */
#endif
		reg_write(INFRABUS_DCMCTL0,
			  aor(reg_read(INFRABUS_DCMCTL0), ~INFRA_DCM_EN_MASK, INFRA_DCM_EN_OFF));
		reg_write(TOPBUS_DCMCTL, and(reg_read(TOPBUS_DCMCTL), ~TOPBUS_DCMCTL_SETTING));
		reg_write(TOPBUS_DCMCTL, and(reg_read(TOPBUS_DCMCTL), ~BUSDCM_APB_TOG));
		reg_write(TOPBUS_DCMCTL, aor(reg_read(TOPBUS_DCMCTL), ~BUSDCM_APB_TOG, BUSDCM_APB_TOG));
	}

	return 0;
}

typedef enum {
	PERI_DCM_OFF = DCM_OFF,
	PERI_DCM_ON = DCM_ON,
} ENUM_PERI_DCM;


/* cnt: 0~0x1f */
int dcm_peri_dbc(int on, int cnt)
{
	int value;

	WARN_ON(cnt > 0x1f);

	on = (on != 0) ? 1 : 0;
	value = (cnt << 15) | (on << 14);

	reg_write(INFRABUS_DCMCTL1,
		  aor(reg_read(INFRABUS_DCMCTL1),
		      ~(PERI_DCM_DBC_CNT_MASK | PERI_DCM_DBC_EN_MASK), value));

	return 0;
}

/**
 * 5'b10000: /1
 * 5'b01000: /2
 * 5'b00100: /4
 * 5'b00010: /8
 * 5'b00001: /16
 * 5'b00000: /32
 */
typedef enum {
	PERI_DCM_SFSEL_DIV_1 = 0x10,
	PERI_DCM_SFSEL_DIV_2 = 0x08,
	PERI_DCM_SFSEL_DIV_4 = 0x04,
	PERI_DCM_SFSEL_DIV_8 = 0x02,
	PERI_DCM_SFSEL_DIV_16 = 0x01,
	PERI_DCM_SFSEL_DIV_32 = 0x00,
} ENUM_PERI_SFSEL;

int dcm_peri_sfsel(int cnt)
{
	int value;

	cnt &= 0x1f;
	value = (cnt << 21);

	reg_write(INFRABUS_DCMCTL0, aor(reg_read(INFRABUS_DCMCTL0), ~PERI_DCM_SFSEL_MASK, value));

	return 0;
}


/** input argument
 * 0: 1/1
 * 1: 1/2
 * 2: 1/4
 * 3: 1/8
 * 4: 1/16
 * 5: 1/32
 * default: 5, 5, 5
 **/
int dcm_peri_rate(unsigned int full, unsigned int sfel)
{

	full = 0x10 >> full;
	sfel = 0x10 >> sfel;

	reg_write(INFRABUS_DCMCTL0, aor(reg_read(INFRABUS_DCMCTL0),
					~(PERI_DCM_SFSEL_MASK | PERI_DCM_FSEL_MASK),
					(full << 16) | (sfel << 21)));

	return 0;
}


int dcm_peri(ENUM_PERI_DCM on)
{

	if (on) {
#if 1		/* override the dbc and fsel setting !! */
		dcm_peri_dbc(PERI_DCM_ON, 0x7);
		dcm_peri_sfsel(PERI_DCM_SFSEL_DIV_32);
#endif
		reg_write(INFRABUS_DCMCTL0, aor(reg_read(INFRABUS_DCMCTL0),
						~PERI_DCM_EN_MASK, PERI_DCM_EN_ON));
	} else {
#if 1		/* override the dbc and fsel setting !! */
		dcm_peri_dbc(PERI_DCM_OFF, 0x0);
#endif
		reg_write(INFRABUS_DCMCTL0, aor(reg_read(INFRABUS_DCMCTL0),
						~PERI_DCM_EN_ON, PERI_DCM_EN_OFF));
	}

	return 0;
}



typedef enum {
	PMIC_DCM_OFF = DCM_OFF,
	PMIC_DCM_ON = DCM_ON,
} ENUM_PMIC_DCM;

/** input argument
 * 0: 1/1
 * 1: 1/2
 * 2: 1/4
 * 3: 1/8
 * 4: 1/16
 * 5: 1/32
 **/
int dcm_pmic_rate(unsigned int sfel)
{

	sfel = 0x10 >> sfel;

	reg_write(INFRABUS_DCMCTL1, aor(reg_read(INFRABUS_DCMCTL1), ~PMIC_SFSEL_MASK, (sfel << 3)));

	return 0;
}

int dcm_pmic(ENUM_PMIC_DCM on)
{
	if (on) {
		reg_write(INFRABUS_DCMCTL1,
			  aor(reg_read(INFRABUS_DCMCTL1),
			      ~(PMIC_SPICLKDCM_EN_MASK | PMIC_BCLKDCM_EN_MASK), PMIC_DCM_EN_ON));
	} else {
		reg_write(INFRABUS_DCMCTL1,
			  aor(reg_read(INFRABUS_DCMCTL1),
			      ~(PMIC_SPICLKDCM_EN_MASK | PMIC_BCLKDCM_EN_MASK), PMIC_DCM_EN_OFF));
	}

	return 0;
}

typedef enum {
	USB_DCM_OFF = DCM_OFF,
	USB_DCM_ON = DCM_ON,
} ENUM_USB_DCM;

int dcm_usb(ENUM_USB_DCM on)
{
	if (on) {
		reg_write(INFRABUS_DCMCTL1,
			  aor(reg_read(INFRABUS_DCMCTL1), ~USBDCM_EN_MASK, USB_DCM_EN_ON));
	} else {
		reg_write(INFRABUS_DCMCTL1,
			  aor(reg_read(INFRABUS_DCMCTL1), ~USBDCM_EN_MASK, USB_DCM_EN_OFF));
	}

	return 0;
}

/** 0x1000000c	TOPEMI_DCMCTL
 * 27 27 emidcm_clk_off
 * 26 26 emidcm_force_off
 * 25 21 emidcm_idle_fsel( / emidcm_idle_fsel+1)
 * 20 16 emidcm_full_sel
 * 15 9  emidcm_dbc_cnt
 * 8  8  emidcm_dbc_enable
 * 7  7  emidcm_enable
 * 5  1  emidcm_apb_sel
 * 0  0  emidcm_apb_tog
 **/
#define EMIDCM_EN_MASK ((1<<27)|(1<<7))
#define EMIDCM_EN_ON ((1<<27)|(1<<7))
#define EMIDCM_EN_OFF ((0<<27)|(0<<7))

#define EMIDCM_DBC_CNT_MASK (0x7f<<9)
#define EMIDCM_DBC_EN_MASK (1<<8)

#define EMIDCM_TOG_0 (0<<0)
#define EMIDCM_TOG_1 (1<<0)

#define EMIDCM_APB_SEL_FORCE_ON  (1<<1)
#define EMIDCM_APB_SEL_DCM       (1<<2)
#define EMIDCM_APB_SEL_DBC		 (1<<3)
#define EMIDCM_APB_SEL_FSEL		 (1<<4)
#define EMIDCM_APB_SEL_IDLE_FSEL (1<<5)

typedef enum {
	EMI_DCM_OFF = DCM_OFF,
	EMI_DCM_ON = DCM_ON,
} ENUM_EMI_DCM;


/** 0x10205038	EMI_CONH
* 1 0 DCM RATION (00: non dcm, 01: 1/8 slow down, 10: 1/16 slow down, 11: 1/32 slow down)
**/

#define EMIDCM_RATIO_MASK (0x3<<0)

typedef enum {
	EMI_DCM_SLOW_DOWN_RATIO_NON = 0x0,
	EMI_DCM_SLOW_DOWN_RATIO_DIV_8 = 0x1,
	EMI_DCM_SLOW_DOWN_RATIO_DIV_16 = 0x2,
	EMI_DCM_SLOW_DOWN_RATIO_DIV_32 = 0x3,
} ENUM_EMI_DCM_SLOW_DOWN_RATIO;

int dcm_emi_slow_down_ratio(ENUM_EMI_DCM_SLOW_DOWN_RATIO ratio)
{

	int value;

	ratio &= 0x3;
	value = (ratio << 0);
	reg_write(EMI_CONH, aor(reg_read(EMI_CONH), ~EMIDCM_RATIO_MASK, value));
	return 0;
}


/* cnt: 0~0x7f */
int dcm_emi_dbc(int on, int cnt)
{
	int value;

	WARN_ON(cnt > 0x7f);

	on = (on != 0) ? 1 : 0;
	value = (cnt << 9) | (on << 8) | EMIDCM_APB_SEL_DBC;

	reg_write(TOPEMI_DCMCTL,
		  aor(reg_read(TOPEMI_DCMCTL), ~(EMIDCM_DBC_CNT_MASK | EMIDCM_DBC_EN_MASK), value));
	reg_write(TOPEMI_DCMCTL, and(reg_read(TOPEMI_DCMCTL), ~EMIDCM_TOG_1));
	reg_write(TOPEMI_DCMCTL, or(reg_read(TOPEMI_DCMCTL), EMIDCM_TOG_1));

	return 0;
}

#define EMI_DCM_DIS	0xFF000000

int dcm_emi(ENUM_EMI_DCM on)
{
	if (on) {
		dcm_emi_dbc(1, 0xf);
		/* EMI emidcm_idle_fsel default is 0x1f (/32) */
		dcm_emi_slow_down_ratio(EMI_DCM_SLOW_DOWN_RATIO_DIV_32);
		reg_write(EMI_CONM, and(reg_read(EMI_CONM), ~EMI_DCM_DIS));
		reg_write(TOPEMI_DCMCTL, aor(reg_read(TOPEMI_DCMCTL),
					     ~EMIDCM_EN_MASK, EMIDCM_EN_OFF | EMIDCM_APB_SEL_DCM));
	} else {
		dcm_emi_dbc(0, 0);
		dcm_emi_slow_down_ratio(EMI_DCM_SLOW_DOWN_RATIO_DIV_32);
		reg_write(EMI_CONM, aor(reg_read(EMI_CONM), ~EMI_DCM_DIS, EMI_DCM_DIS));
		reg_write(TOPEMI_DCMCTL, aor(reg_read(TOPEMI_DCMCTL),
					     ~EMIDCM_EN_MASK, EMIDCM_EN_OFF | EMIDCM_APB_SEL_DCM));
	}

	reg_write(TOPEMI_DCMCTL, and(reg_read(TOPEMI_DCMCTL), ~EMIDCM_TOG_1));
	reg_write(TOPEMI_DCMCTL, or(reg_read(TOPEMI_DCMCTL), EMIDCM_TOG_1));

	return 0;
}


/*****************************************************/

enum {
	ARMCORE_DCM_TYPE = (1U << 0),
	MCUSYS_DCM_TYPE = (1U << 1),
	INFRA_DCM_TYPE = (1U << 2),
	PERI_DCM_TYPE = (1U << 3),
	EMI_DCM_TYPE = (1U << 4),
	PMIC_DCM_TYPE = (1U << 5),
	USB_DCM_TYPE = (1U << 6),
	NR_DCM_TYPE = 7,
};

#define ALL_DCM_TYPE  (ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | INFRA_DCM_TYPE | PERI_DCM_TYPE |  \
						EMI_DCM_TYPE | PMIC_DCM_TYPE | USB_DCM_TYPE)

typedef struct _dcm {
	int current_state;
	int saved_state;
	int disable_refcnt;
	int default_state;
	DCM_FUNC func;
	int typeid;
	char *name;
} DCM;

static DCM dcm_array[NR_DCM_TYPE] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = (DCM_FUNC) dcm_armcore,
	 .current_state = ARMCORE_DCM_MODE1,
	 .default_state = ARMCORE_DCM_MODE1,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys,
	 .current_state = MCUSYS_DCM_ON,
	 .default_state = MCUSYS_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = (DCM_FUNC) dcm_infra,
	 .current_state = INFRA_DCM_ON,
	 .default_state = INFRA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 .current_state = PERI_DCM_ON,
	 .default_state = PERI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = EMI_DCM_TYPE,
	 .name = "EMI_DCM",
	 .func = (DCM_FUNC) dcm_emi,
	 .current_state = EMI_DCM_ON,
	 .default_state = EMI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PMIC_DCM_TYPE,
	 .name = "PMIC_DCM",
	 .func = (DCM_FUNC) dcm_pmic,
	 .current_state = PMIC_DCM_ON,
	 .default_state = PMIC_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = USB_DCM_TYPE,
	 .name = "USB_DCM",
	 .func = (DCM_FUNC) dcm_usb,
	 .current_state = USB_DCM_ON,
	 .default_state = USB_DCM_ON,
	 .disable_refcnt = 0,
	 },
};



/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to recovery CURRENT_STATE before any power-off reset.
 * 2. dcm_set_default(type) to reset as cold-power-on init state.
 * 3. dcm_disable(type) to disable all dcm.
 * 4. dcm_set_state(type) to set dcm state.
 * 5. dcm_dump_state(type) to show CURRENT_STATE.
 * 6. /sys/power/dcm_state interface:  'restore', 'disable', 'dump', 'set'. 4 commands.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/

void dcm_set_default(unsigned int type)
{
	int i;
	DCM *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	/* dcm_info("[%s]type:0x%08x\n", __func__, type); */

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm->saved_state = dcm->current_state = dcm->default_state;
			dcm->disable_refcnt = 0;
			dcm->func(dcm->current_state);

			/* dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
			 * dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);
			 */
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
	int i;
	DCM *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("[%s]type:0x%08x, set:%d\n", __func__, type, state);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->saved_state = state;
			if (dcm->disable_refcnt == 0) {
				dcm->current_state = state;
				dcm->func(dcm->current_state);
			}

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);
}


void dcm_disable(unsigned int type)
{
	int i;
	DCM *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->current_state = DCM_OFF;
			dcm->disable_refcnt++;
			dcm->func(dcm->current_state);

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);

}

void dcm_restore(unsigned int type)
{
	int i;
	DCM *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->disable_refcnt > 0)
				dcm->disable_refcnt--;
			if (dcm->disable_refcnt == 0) {
				dcm->current_state = dcm->saved_state;
				dcm->func(dcm->current_state);
			}

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);
}


void dcm_dump_state(int type)
{
	int i;
	DCM *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm_info("[%-16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);
		}
	}
}

#define REG_DUMP(addr) { dcm_info("%-30s(0x%p): 0x%08X\n", #addr, addr, reg_read(addr)); }


void dcm_dump_regs(void)
{
	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("\n******** dcm dump register *********\n");
	/* MCUSYS */
	REG_DUMP(INFRA_TOPCKGEN_DCMDBC);
	REG_DUMP(MP0_CA7L_DBG_PWR_CTRL);
	REG_DUMP(CCI_ADB400_DCM_CONFIG);
	REG_DUMP(L2C_SRAM_CTRL);
	REG_DUMP(CCI_STATUS);
	REG_DUMP(MCUSYS_BUS_FABRIC_DCM_CTRL);
	REG_DUMP(MCU_MISC_DCM_CTRL);
	REG_DUMP(MP_CCI_ADB400_DCM_CONFIG);
	REG_DUMP(CCI_BW_PMU_CTL);
	REG_DUMP(MP0_RGU_DCM_CONFIG);

	/* TOPBUS - INFRA,PERI,EMI,PMIC,USB */
	REG_DUMP(TOPBUS_DCMCTL);

	/* EMI */
	REG_DUMP(TOPEMI_DCMCTL);

	/* INFRA,PERI,PMIC,USB */
	REG_DUMP(INFRABUS_DCMCTL0);
	REG_DUMP(INFRABUS_DCMCTL1);

	/* EMI CONH */
	REG_DUMP(EMI_CONH);
	REG_DUMP(EMI_CONM);
}

#if defined(CONFIG_PM)
static ssize_t dcm_state_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	int i;
	DCM *dcm;

	/* dcm_dump_state(ALL_DCM_TYPE); */
	len = snprintf(buf, PAGE_SIZE, "\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		len += snprintf(buf+len, PAGE_SIZE-len, "[%-16s 0x%08x] current state:%d (%d)\n",
					dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "\n********** dcm_state help *********\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "set:           echo set [mask] [mode] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "disable:       echo disable [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "restore:       echo restore [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "dump:          echo dump [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "***** [mask] is hexl bit mask of dcm;\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "***** [mode] is type of DCM to set and retained\n");

	return len;
}

static ssize_t dcm_state_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	char cmd[16];
	unsigned int mask;

	if (sscanf(buf, "%15s %x", cmd, &mask) == 2) {
		mask &= ALL_DCM_TYPE;

		if (!strcmp(cmd, "restore")) {
			/* dcm_dump_regs(); */
			dcm_restore(mask);
			/* dcm_dump_regs(); */
		} else if (!strcmp(cmd, "disable")) {
			/* dcm_dump_regs(); */
			dcm_disable(mask);
			/* dcm_dump_regs(); */
		} else if (!strcmp(cmd, "dump")) {
			dcm_dump_state(mask);
			dcm_dump_regs();
		} else if (!strcmp(cmd, "set")) {
			int mode;

			if (sscanf(buf, "%15s %x %d", cmd, &mask, &mode) == 3) {
				mask &= ALL_DCM_TYPE;

				dcm_set_state(mask, mode);
			}
		} else {
			dcm_info("SORRY, do not support your command: %s\n", cmd);
			return -EINVAL;
		}
		return n;
	}

	dcm_info("SORRY, do not support your command.\n");

	return -EINVAL;
}

static struct kobj_attribute dcm_state_attr = {
	.attr = {
		 .name = "dcm_state",
		 .mode = 0644,
		 },
	.show = dcm_state_show,
	.store = dcm_state_store,
};


#endif /*#if defined (CONFIG_PM) */


#if defined(CONFIG_OF)
static int mt_dcm_dts_map(void)
{
	struct device_node *node;
	struct resource r;

	/* topckgen */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_NODE);
	if (!node) {
		dcm_info("error: cannot find node " TOPCKGEN_NODE);
		WARN_ON(1);
		goto fail;
	}
	topckctrl_base = of_iomap(node, 0);
	if (!topckctrl_base) {
		dcm_info("error: cannot iomap " TOPCKGEN_NODE);
		WARN_ON(1);
		goto fail;
	}

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node) {
		dcm_info("error: cannot find node " MCUCFG_NODE);
		WARN_ON(1);
		goto fail;
	}
	if (of_address_to_resource(node, 0, &r)) {
		dcm_info("error: cannot get phys addr" MCUCFG_NODE);
		WARN_ON(1);
		goto fail;
	}
	mcucfg_phys_base = r.start;

	mcucfg_base = of_iomap(node, 0);
	if (!mcucfg_base) {
		dcm_info("error: cannot iomap " MCUCFG_NODE);
		WARN_ON(1);
		goto fail;
	}

	/* emi_reg */
	node = of_find_compatible_node(NULL, NULL, EMI_REG_NODE);
	if (!node) {
		dcm_info("error: cannot find node " EMI_REG_NODE);
		WARN_ON(1);
		goto fail;
	}
	emi_reg_base = of_iomap(node, 0);
	if (!emi_reg_base) {
		dcm_info("error: cannot iomap " EMI_REG_NODE);
		WARN_ON(1);
		goto fail;
	}

	/* infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_NODE);
	if (!node) {
		dcm_info("error: cannot find node " INFRACFG_AO_NODE);
		WARN_ON(1);
		goto fail;
	}
	infracfg_ao_base = of_iomap(node, 0);
	if (!infracfg_ao_base) {
		dcm_info("error: cannot iomap " INFRACFG_AO_NODE);
		WARN_ON(1);
		goto fail;
	}

	dcm_initiated = 1;

	return 0;
fail:
	return -1;
}
#else
static int mt_dcm_dts_map(void)
{
	return 0;
}
#endif


int mt_dcm_init(void)
{
	int ret, err = 0;

	ret = mt_dcm_dts_map();
	if (ret != 0) {
		dcm_err("Fail to initialized DCM\n");
		return 0;
	}

	/* enable all dcm */
	dcm_set_default(ALL_DCM_TYPE);
	/* dcm_set_state(ALL_DCM_TYPE, DCM_OFF); disable all DCM by default */

	err = sysfs_create_file(power_kobj, &dcm_state_attr.attr);
	if (err)
		dcm_err("[%s]: fail to create sysfs\n", __func__);

	return 0;
}
late_initcall(mt_dcm_init);


/**** public APIs *****/
void mt_dcm_disable(void)
{
	dcm_disable(ALL_DCM_TYPE);
}
EXPORT_SYMBOL(mt_dcm_disable);

void mt_dcm_restore(void)
{
	dcm_restore(ALL_DCM_TYPE);
}
EXPORT_SYMBOL(mt_dcm_restore);
