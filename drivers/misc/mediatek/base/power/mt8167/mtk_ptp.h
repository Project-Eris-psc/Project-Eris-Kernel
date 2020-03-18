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

#ifndef _MT_PTP_
#define _MT_PTP_

#include <linux/kernel.h>

/* enable/disable PTP-OD (SW).
 * This give us ptp_data[3] to temporarily turn off idle
 */
#define EN_PTP_OD (1)
#define SUPPORT_PTPOD_GPU (0)

extern void __iomem *ptpod_base;
extern u32 get_devinfo_with_index(u32 index);

/* PTP Register Definition */
#define PTP_BASEADDR		(ptpod_base)
#define PTP_REVISIONID		(PTP_BASEADDR + 0x0FC)
#define PTP_DESCHAR		(PTP_BASEADDR + 0x200)
#define PTP_TEMPCHAR		(PTP_BASEADDR + 0x204)
#define PTP_DETCHAR		(PTP_BASEADDR + 0x208)
#define PTP_AGECHAR		(PTP_BASEADDR + 0x20C)
#define PTP_DCCONFIG		(PTP_BASEADDR + 0x210)
#define PTP_AGECONFIG		(PTP_BASEADDR + 0x214)
#define PTP_FREQPCT30		(PTP_BASEADDR + 0x218)
#define PTP_FREQPCT74		(PTP_BASEADDR + 0x21C)
#define PTP_LIMITVALS		(PTP_BASEADDR + 0x220)
#define PTP_VBOOT		(PTP_BASEADDR + 0x224)
#define PTP_DETWINDOW		(PTP_BASEADDR + 0x228)
#define PTP_PTPCONFIG		(PTP_BASEADDR + 0x22C)
#define PTP_TSCALCS		(PTP_BASEADDR + 0x230)
#define PTP_RUNCONFIG		(PTP_BASEADDR + 0x234)
#define PTP_PTPEN		(PTP_BASEADDR + 0x238)
#define PTP_INIT2VALS		(PTP_BASEADDR + 0x23C)
#define PTP_DCVALUES		(PTP_BASEADDR + 0x240)
#define PTP_AGEVALUES		(PTP_BASEADDR + 0x244)
#define PTP_VOP30		(PTP_BASEADDR + 0x248)
#define PTP_VOP74		(PTP_BASEADDR + 0x24C)
#define PTP_TEMP		(PTP_BASEADDR + 0x250)
#define PTP_PTPINTSTS		(PTP_BASEADDR + 0x254)
#define PTP_PTPINTSTSRAW	(PTP_BASEADDR + 0x258)
#define PTP_PTPINTEN		(PTP_BASEADDR + 0x25C)
#define PTP_PTPCHKSHIFT		(PTP_BASEADDR + 0x264)
#define PTP_VDESIGN30		(PTP_BASEADDR + 0x26C)
#define PTP_VDESIGN74		(PTP_BASEADDR + 0x270)
#define PTP_DVT30		(PTP_BASEADDR + 0x274)
#define PTP_DVT74		(PTP_BASEADDR + 0x278)
#define PTP_AGECOUNT		(PTP_BASEADDR + 0x27C)
#define PTP_SMSTATE0		(PTP_BASEADDR + 0x280)
#define PTP_SMSTATE1		(PTP_BASEADDR + 0x284)
#define PTP_PTPCTL0		(PTP_BASEADDR + 0x288)
/* #define PTP_TS3_MUX		31:28
 * #define PTP_TS2_MUX		27:24
 * #define PTP_TS1_MUX		23:20
 * #define PTP_TS0_MUX		19:16
 * #define PTP_TS3_EN		3:3
 * #define PTP_TS2_EN		2:2
 * #define PTP_TS1_EN		1:1
 * #define PTP_TS0_EN		0:0
 */

#define PTP_PTPCORESEL		(PTP_BASEADDR + 0x400)
/* #define SYSTEMCLK_CG_EN		31:31
 * #define PTPODCORE1EN		17:17
 * #define PTPODCORE0EN		16:16
 */
/* #define APBSEL		3:0 write this in mt_ptp.c */

#define PTP_THERMINTST		(PTP_BASEADDR + 0x404)
#define PTP_PTPODINTST		(PTP_BASEADDR + 0x408)
/* #define PTPODINT0		0:0 not used anymore */
/* #define PTPODINT1		1:1 not used anymore */
/* #define PTPODINT2		2:2 not used anymore */
/* #define PTPODINT3		3:3 not used anymore */

#define PTP_THSTAGE0ST		(PTP_BASEADDR + 0x40C)
#define PTP_THSTAGE1ST		(PTP_BASEADDR + 0x410)
#define PTP_THSTAGE2ST		(PTP_BASEADDR + 0x414)
#define PTP_THAHBST0		(PTP_BASEADDR + 0x418)
#define PTP_THAHBST1		(PTP_BASEADDR + 0x41C)
#define PTP_PTPSPARE0		(PTP_BASEADDR + 0x420)
#define PTP_PTPSPARE1		(PTP_BASEADDR + 0x424)
#define PTP_PTPSPARE2		(PTP_BASEADDR + 0x428)
#define PTP_PTPSPARE3		(PTP_BASEADDR + 0x42C)
#define PTP_THSLPEVEB		(PTP_BASEADDR + 0x430)

/**
 * 1: Select VCORE_AO ptpod detector
 * 0: Select VCORE_PDN ptpod detector
 */
#if 0
#define PERI_VCORE_PTPOD_CON0	(PERICFG_BASE + 0x408)
#define VCORE_PTPODSEL		(0:0)
#define SEL_VCORE_AO		1
#define SEL_VCORE_PDN		0
#endif

/* Thermal Register Definition */
#define THERMAL_BASE		ptpod_base

#define PTP_TEMPMONCTL0		(THERMAL_BASE + 0x000)
#define PTP_TEMPMONCTL1		(THERMAL_BASE + 0x004)
#define PTP_TEMPMONCTL2		(THERMAL_BASE + 0x008)
#define PTP_TEMPMONINT		(THERMAL_BASE + 0x00C)
#define PTP_TEMPMONINTSTS	(THERMAL_BASE + 0x010)
#define PTP_TEMPMONIDET0	(THERMAL_BASE + 0x014)
#define PTP_TEMPMONIDET1	(THERMAL_BASE + 0x018)
#define PTP_TEMPMONIDET2	(THERMAL_BASE + 0x01C)
#define PTP_TEMPH2NTHRE		(THERMAL_BASE + 0x024)
#define PTP_TEMPHTHRE		(THERMAL_BASE + 0x028)
#define PTP_TEMPCTHRE		(THERMAL_BASE + 0x02C)
#define PTP_TEMPOFFSETH		(THERMAL_BASE + 0x030)
#define PTP_TEMPOFFSETL		(THERMAL_BASE + 0x034)
#define PTP_TEMPMSRCTL0		(THERMAL_BASE + 0x038)
#define PTP_TEMPMSRCTL1		(THERMAL_BASE + 0x03C)
#define PTP_TEMPAHBPOLL		(THERMAL_BASE + 0x040)
#define PTP_TEMPAHBTO		(THERMAL_BASE + 0x044)
#define PTP_TEMPADCPNP0		(THERMAL_BASE + 0x048)
#define PTP_TEMPADCPNP1		(THERMAL_BASE + 0x04C)
#define PTP_TEMPADCPNP2		(THERMAL_BASE + 0x050)
#define PTP_TEMPADCMUX		(THERMAL_BASE + 0x054)
#define PTP_TEMPADCEXT		(THERMAL_BASE + 0x058)
#define PTP_TEMPADCEXT1		(THERMAL_BASE + 0x05C)
#define PTP_TEMPADCEN		(THERMAL_BASE + 0x060)
#define PTP_TEMPPNPMUXADDR	(THERMAL_BASE + 0x064)
#define PTP_TEMPADCMUXADDR	(THERMAL_BASE + 0x068)
#define PTP_TEMPADCEXTADDR	(THERMAL_BASE + 0x06C)
#define PTP_TEMPADCEXT1ADDR	(THERMAL_BASE + 0x070)
#define PTP_TEMPADCENADDR	(THERMAL_BASE + 0x074)
#define PTP_TEMPADCVALIDADDR	(THERMAL_BASE + 0x078)
#define PTP_TEMPADCVOLTADDR	(THERMAL_BASE + 0x07C)
#define PTP_TEMPRDCTRL		(THERMAL_BASE + 0x080)
#define PTP_TEMPADCVALIDMASK	(THERMAL_BASE + 0x084)
#define PTP_TEMPADCVOLTAGESHIFT	(THERMAL_BASE + 0x088)
#define PTP_TEMPADCWRITECTRL	(THERMAL_BASE + 0x08C)
#define PTP_TEMPMSR0		(THERMAL_BASE + 0x090)
#define PTP_TEMPMSR1		(THERMAL_BASE + 0x094)
#define PTP_TEMPMSR2		(THERMAL_BASE + 0x098)
#define PTP_TEMPIMMD0		(THERMAL_BASE + 0x0A0)
#define PTP_TEMPIMMD1		(THERMAL_BASE + 0x0A4)
#define PTP_TEMPIMMD2		(THERMAL_BASE + 0x0A8)
#define PTP_TEMPMONIDET3	(THERMAL_BASE + 0x0B0)
#define PTP_TEMPADCPNP3		(THERMAL_BASE + 0x0B4)
#define PTP_TEMPMSR3		(THERMAL_BASE + 0x0B8)
#define PTP_TEMPIMMD3		(THERMAL_BASE + 0x0BC)
#define PTP_TEMPPROTCTL		(THERMAL_BASE + 0x0C0)
#define PTP_TEMPPROTTA		(THERMAL_BASE + 0x0C4)
#define PTP_TEMPPROTTB		(THERMAL_BASE + 0x0C8)
#define PTP_TEMPPROTTC		(THERMAL_BASE + 0x0CC)
#define PTP_TEMPSPARE0		(THERMAL_BASE + 0x0F0)
#define PTP_TEMPSPARE1		(THERMAL_BASE + 0x0F4)
#define PTP_TEMPSPARE2		(THERMAL_BASE + 0x0F8)
#define PTP_TEMPSPARE3		(THERMAL_BASE + 0x0FC)

enum ptp_ctrl_id {
	PTP_CTRL_MCUSYS = 0,
#if SUPPORT_PTPOD_GPU
	PTP_CTRL_GPUSYS = 1,
#endif
	NR_PTP_CTRL, /* 1 or 2 */
};

enum ptp_det_id {
	PTP_DET_MCUSYS = PTP_CTRL_MCUSYS,
#if SUPPORT_PTPOD_GPU
	PTP_DET_GPUSYS = PTP_CTRL_GPUSYS,
#endif
	NR_PTP_DET,	/* 1 or 2 */
};

/* PTP Extern Function */
extern int is_ptp_initialized_done(void);
unsigned int mt_ptp_get_level(void);
int get_ptpod_status(void);

#endif
