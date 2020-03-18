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


#ifndef __hdmictrl_h__
#define __hdmictrl_h__
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
/* #include <linux/earlysuspend.h> */
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "hdmitable.h"
#include "hdmi_drv.h"
/* #include "mt_boot_common.h" */
/* #define HDMIDRV_BASE  (0xF4015000) */
/* #define HDMISYS_BASE  (0xF4000000) */
/* #define HDMIPLL_BASE  (0xF0209000)  pll */
/* #define HDMICKGEN_BASE  (0xF0000000) */
/* #define HDMIPAD_BASE  (0xF0005000) */
#define FALSE                       0
#define TRUE                        1


enum HDMI_REG_ENUM {
	HDMI_SHELL,
	HDMI_DDC,
	HDMI_CEC,
	HDMI_REG_NUM,
};

enum HDMI_REF_MODULE_ENUM {
	AP_CCIF0,		/*  PLL/TVD */
	TOPCK_GEN,
	INFRA_SYS,
	MMSYS_CONFIG,
	GPIO_REG,
	DPI1_REG,
	GIC_REG,
	HDMI_REF_REG_NUM,
};

enum HDMI_REF_CLOCK_ENUM {
/*	MMSYS_POWER,	 Must be first,  power of the mmsys */
/*	PERI_DDC,*/
/*	MMSYS_HDMI_HDCP, */
	INFRA_SYS_CLK_DDC,	/* DDC Clock */
	INFRA_SYS_CEC_26M,
	INFRA_SYS_CEC_32K,
	MMSYS_HDMI_PLL,
	MMSYS_HDMI_PIXEL,
	MMSYS_HDMI_AUDIO,
	MMSYS_HDMI_SPIDIF,
	/* TOP_HDMI_SEL, */
	HDMI_SEL_CLOCK_NUM,
};

extern unsigned long hdmi_reg[HDMI_REG_NUM];
extern unsigned long hdmi_ref_reg[HDMI_REF_REG_NUM];
extern struct clk *hdmi_ref_clock[HDMI_SEL_CLOCK_NUM];
extern unsigned char hdmi_dpi_output;

#define HDMIDRV_BASE   (hdmi_reg[HDMI_SHELL])
#define HDMIDDC_BASE   (hdmi_reg[HDMI_DDC])
#define HDMICEC_BASE   (hdmi_reg[HDMI_CEC])

#define HDMISYS_BASE   (hdmi_ref_reg[MMSYS_CONFIG])
#define HDMIPLL_BASE   (hdmi_ref_reg[AP_CCIF0])	/* pll */
#define HDMICKGEN_BASE (hdmi_ref_reg[TOPCK_GEN])
#define HDMIPAD_BASE   (hdmi_ref_reg[GPIO_REG])
#define HDMI_INFRA_SYS (hdmi_ref_reg[INFRA_SYS])

/* ////////////////////////////////////// */
#define PORD_MODE     (1<<0)
#define HOTPLUG_MODE  (1<<1)

#define AV_INFO_HD_ITU709           0x80
#define AV_INFO_SD_ITU601           0x40
#define AV_INFO_4_3_OUTPUT          0x10
#define AV_INFO_16_9_OUTPUT         0x20

/* AVI Info Frame */
#define AVI_TYPE            0x82
#define AVI_VERS            0x02
#define AVI_LEN             0x0d

/* Audio Info Frame */
#define AUDIO_TYPE             0x84
#define AUDIO_VERS             0x01
#define AUDIO_LEN              0x0A

#define VS_TYPE            0x81
#define VS_VERS            0x01
#define VS_LEN             0x05
#define VS_PB_LEN          0x0b	/* VS_LEN+1 include checksum */
/* GAMUT Data */
#define GAMUT_TYPE             0x0A
#define GAMUT_PROFILE          0x81
#define GAMUT_SEQ              0x31

/*  ACP Info Frame */
#define ACP_TYPE            0x04
/* #define ACP_VERS            0x02 */
#define ACP_LEN             0x00

/* ISRC1 Info Frame */
#define ISRC1_TYPE            0x05
/* #define ACP_VERS            0x02 */
#define ISRC1_LEN             0x00

/* SPD Info Frame */
#define SPD_TYPE            0x83
#define SPD_VERS            0x01
#define SPD_LEN             0x19


#define SV_ON        (unsigned short)(1)
#define SV_OFF       (unsigned short)(0)

#define TMDS_CLK_X1 1
#define TMDS_CLK_X1_25  2
#define TMDS_CLK_X1_5  3
#define TMDS_CLK_X2  4

#define RJT_24BIT 0
#define RJT_16BIT 1
#define LJT_24BIT 2
#define LJT_16BIT 3
#define I2S_24BIT 4
#define I2S_16BIT 5

#define GRL_INT              0x14
#define INT_MDI            (0x1 << 0)
#define INT_HDCP           (0x1 << 1)
#define INT_FIFO_O         (0x1 << 2)
#define INT_FIFO_U         (0x1 << 3)
#define INT_IFM_ERR        (0x1 << 4)
#define INT_INF_DONE       (0x1 << 5)
#define INT_NCTS_DONE      (0x1 << 6)
#define INT_CTRL_PKT_DONE  (0x1 << 7)
#define GRL_INT_MASK              0x18
#define GRL_CTRL             0x1C
#define CTRL_GEN_EN        (0x1 << 2)
#define CTRL_SPD_EN        (0x1 << 3)
#define CTRL_MPEG_EN       (0x1 << 4)
#define CTRL_AUDIO_EN      (0x1 << 5)
#define CTRL_AVI_EN        (0x1 << 6)
#define CTRL_AVMUTE        (0x1 << 7)
#define GRL_STATUS           0x20
#define STATUS_HTPLG       (0x1 << 0)
#define STATUS_PORD        (0x1 << 1)

#define GRL_CFG0             0x24
#define CFG0_I2S_MODE_RTJ  0x1
#define CFG0_I2S_MODE_LTJ  0x0
#define CFG0_I2S_MODE_I2S  0x2
#define CFG0_I2S_MODE_24Bit 0x00
#define CFG0_I2S_MODE_16Bit 0x10

#define GRL_CFG1             0x28
#define CFG1_EDG_SEL       (0x1 << 0)
#define CFG1_SPDIF         (0x1 << 1)
#define CFG1_DVI           (0x1 << 2)
#define CFG1_HDCP_DEBUG    (0x1 << 3)
#define GRL_CFG2             0x2c
#define CFG2_NOTICE_EN     (0x1 << 6)
#define MHL_DE_SEL         (0x1<<3)
#define GRL_CFG3             0x30
#define CFG3_AES_KEY_INDEX_MASK    0x3f
#define CFG3_CONTROL_PACKET_DELAY  (0x1 << 6)
#define CFG3_KSV_LOAD_START        (0x1 << 7)
#define GRL_CFG4             0x34
#define CFG4_AES_KEY_LOAD  (0x1 << 4)
#define CFG4_AV_UNMUTE_EN  (0x1 << 5)
#define CFG4_AV_UNMUTE_SET (0x1 << 6)
#define CFG_MHL_MODE (0x1<<7)
#define GRL_CFG5             0x38
#define CFG5_CD_RATIO_MASK 0x8F
#define CFG5_FS128         (0x1 << 4)
#define CFG5_FS256         (0x2 << 4)
#define CFG5_FS384         (0x3 << 4)
#define CFG5_FS512         (0x4 << 4)
#define CFG5_FS768         (0x6 << 4)
#define GRL_WR_BKSV0         0x40
#define GRL_WR_AN0           0x54
#define GRL_RD_AKSV0         0x74
#define GRL_RI_0             0x88
#define GRL_KEY_PORT         0x90
#define GRL_KSVLIST          0x94
#define GRL_HDCP_STA         0xB8
#define HDCP_STA_RI_RDY    (0x1 << 2)
#define HDCP_STA_V_MATCH   (0x1 << 3)
#define HDCP_STA_V_RDY     (0x1 << 4)


#define GRL_HDCP_CTL         0xBC
#define HDCP_CTL_ENC_EN    (0x1 << 0)
#define HDCP_CTL_AUTHEN_EN (0x1 << 1)
#define HDCP_CTL_CP_RSTB   (0x1 << 2)
#define HDCP_CTL_AN_STOP   (0x1 << 3)
#define HDCP_CTRL_RX_RPTR  (0x1 << 4)
#define HDCP_CTL_HOST_KEY  (0x1 << 6)
#define HDCP_CTL_SHA_EN    (0x1 << 7)

#define GRL_REPEATER_HASH    0xC0
#define GRL_I2S_C_STA0             0x140
#define GRL_I2S_C_STA1             0x144
#define GRL_I2S_C_STA2             0x148
#define GRL_I2S_C_STA3             0x14C	/* including sampling frequency information. */
#define GRL_I2S_C_STA4             0x150
#define GRL_I2S_UV           0x154
#define GRL_ACP_ISRC_CTRL    0x158
#define VS_EN              (0x01<<0)
#define ACP_EN             (0x01<<1)
#define ISRC1_EN           (0x01<<2)
#define ISRC2_EN           (0x01<<3)
#define GAMUT_EN           (0x01<<4)
#define GRL_CTS_CTRL         0x160
#define CTS_CTRL_SOFT      (0x1 << 0)

#define GRL_CTS0             0x164
#define GRL_CTS1             0x168
#define GRL_CTS2             0x16c

#define GRL_DIVN             0x170
#define NCTS_WRI_ANYTIME   (0x01<<6)

#define GRL_DIV_RESET        0x178
#define SWAP_YC  (0x01 << 0)
#define UNSWAP_YC  (0x00 << 0)

#define GRL_AUDIO_CFG        0x17C
#define AUDIO_ZERO          (0x01<<0)
#define HIGH_BIT_RATE      (0x01<<1)
#define SACD_DST           (0x01<<2)
#define DST_NORMAL_DOUBLE  (0x01<<3)
#define DSD_INV            (0x01<<4)
#define LR_INV             (0x01<<5)
#define LR_MIX             (0x01<<6)
#define SACD_SEL           (0x01<<7)

#define GRL_NCTS             0x184

#define GRL_IFM_PORT         0x188
#define GRL_CH_SW0           0x18C
#define GRL_CH_SW1           0x190
#define GRL_CH_SW2           0x194
#define GRL_CH_SWAP          0x198
#define LR_SWAP             (0x01<<0)
#define LFE_CC_SWAP         (0x01<<1)
#define LSRS_SWAP           (0x01<<2)
#define RLS_RRS_SWAP        (0x01<<3)
#define LR_STATUS_SWAP      (0x01<<4)

#define GRL_INFOFRM_VER      0x19C
#define GRL_INFOFRM_TYPE     0x1A0
#define GRL_INFOFRM_LNG      0x1A4
#define GRL_SHIFT_R2         0x1B0
#define AUDIO_PACKET_OFF      (0x01 << 6)
#define GRL_MIX_CTRL         0x1B4
#define MIX_CTRL_SRC_EN    (0x1 << 0)
#define BYPASS_VOLUME    (0x1 << 1)
#define MIX_CTRL_FLAT      (0x1 << 7)
#define GRL_IIR_FILTER       0x1B8
#define GRL_SHIFT_L1         0x1C0
#define GRL_AOUT_BNUM_SEL    0x1C4
#define AOUT_24BIT         0x00
#define AOUT_20BIT         0x02
#define AOUT_16BIT         0x03
#define HIGH_BIT_RATE_PACKET_ALIGN (0x3 << 6)

#define GRL_L_STATUS_0        0x200
#define GRL_L_STATUS_1        0x204
#define GRL_L_STATUS_2        0x208
#define GRL_L_STATUS_3        0x20c
#define GRL_L_STATUS_4        0x210
#define GRL_L_STATUS_5        0x214
#define GRL_L_STATUS_6        0x218
#define GRL_L_STATUS_7        0x21c
#define GRL_L_STATUS_8        0x220
#define GRL_L_STATUS_9        0x224
#define GRL_L_STATUS_10        0x228
#define GRL_L_STATUS_11        0x22c
#define GRL_L_STATUS_12        0x230
#define GRL_L_STATUS_13        0x234
#define GRL_L_STATUS_14        0x238
#define GRL_L_STATUS_15        0x23c
#define GRL_L_STATUS_16        0x240
#define GRL_L_STATUS_17        0x244
#define GRL_L_STATUS_18        0x248
#define GRL_L_STATUS_19        0x24c
#define GRL_L_STATUS_20        0x250
#define GRL_L_STATUS_21        0x254
#define GRL_L_STATUS_22        0x258
#define GRL_L_STATUS_23        0x25c
#define GRL_R_STATUS_0        0x260
#define GRL_R_STATUS_1        0x264
#define GRL_R_STATUS_2        0x268
#define GRL_R_STATUS_3        0x26c
#define GRL_R_STATUS_4        0x270
#define GRL_R_STATUS_5        0x274
#define GRL_R_STATUS_6        0x278
#define GRL_R_STATUS_7        0x27c
#define GRL_R_STATUS_8        0x280
#define GRL_R_STATUS_9        0x284
#define GRL_R_STATUS_10        0x288
#define GRL_R_STATUS_11        0x28c
#define GRL_R_STATUS_12        0x290
#define GRL_R_STATUS_13        0x294
#define GRL_R_STATUS_14        0x298
#define GRL_R_STATUS_15        0x29c
#define GRL_R_STATUS_16        0x2a0
#define GRL_R_STATUS_17        0x2a4
#define GRL_R_STATUS_18        0x2a8
#define GRL_R_STATUS_19        0x2ac
#define GRL_R_STATUS_20        0x2b0
#define GRL_R_STATUS_21        0x2b4
#define GRL_R_STATUS_22        0x2b8
#define GRL_R_STATUS_23        0x2bc

#define DUMMY_304    0x304
#define CHMO_SEL  (0x3<<2)
#define CHM1_SEL  (0x3<<4)
#define CHM2_SEL  (0x3<<6)
#define AUDIO_I2S_NCTS_SEL   (1<<1)
#define AUDIO_I2S_NCTS_SEL_64   (1<<1)
#define AUDIO_I2S_NCTS_SEL_128  (0<<1)
#define NEW_GCP_CTRL (1<<0)
#define NEW_GCP_CTRL_MERGE (1<<0)
#define NEW_GCP_CTRL_ORIG  (0<<0)

#define CRC_CTRL 0x310
#define clr_crc_result (1<<1)
#define init_crc  (1<<0)

#define CRC_RESULT_L 0x314

#define CRC_RESULT_H 0x318

#define VIDEO_CFG_0 0x380
#define VIDEO_CFG_1 0x384
#define VIDEO_CFG_2 0x388
#define VIDEO_CFG_3 0x38c
#define VIDEO_CFG_4 0x390
#define VIDEO_SOURCE_SEL (1<<7)
#define NORMAL_PATH (1<<7)
#define GEN_RGB (0<<7)
#define HDMICLK_CFG_4	0x80
#define DPI1_MUX_CK (0x0<<24)
#define DPI1_H_CK (0x1<<24)
#define DPI1_D2  (0x2<<24)
#define DPI1_D4  (0x3<<24)

#define HDMICLK_CFG_5	0x90
#define CLK_HDMIPLL_SEL (0x3<<8)
#define HDMIPLL_MUX_CK (0x0<<8)
#define HDMIPLL_CTS (0x1<<8)
#define HDMIPLL_D2  (0x2<<8)
#define HDMIPLL_D4  (0x3<<8)

#define HDCP_STATUS_RESET		0x398
#define HDCP_STATUS_RESET0	(0x1 << 0)
#define VSYNC_RESET_SELECT  (0x1 << 1)
#define RG_N_DIV2			(0x1 << 2)
#define RG_SPD_IIS_SEL		(0x1 << 3)

#define CLK_MUX_SEL8     0x40
#define RG_FDPI1_MUX_SEL_MASK (0x7 << 15)
#define RG_FDPI1_MUX_SEL_SHIFT 15
#define DPI1_26M_CLK_SEL     (0x0 << 15)
#define DPI1_297M_CLK_SEL	 (0x1 << 15)
#define DPI1_148M_CLK_SEL	 (0x2 << 15)
#define DPI1_78M_CLK_SEL     (0x3 << 15)
#define DPI1_37M_CLK_SEL      (0x4 << 15)

#define CLK_GATING_CTRL8   0x70
#define RG_FDPI1_SW_CG	(0x1 << 5)
#define RG_FDPI1_SW_CG_CLR (0x0 << 5)
/* This infrasys register should be removed */
#define INFRA_RST0	0x30
#define CEC_SOFT_RST (0x1<<10)

#define INFRA_PDN0	0x40
#define CEC_PDN0 (0x1<<18)

#define INFRA_PDN1	0x44
#define CEC_PDN1 (0x1<<18)

#define PREICFG_PDN_SET	0x3008
#define DDC_PDN_SET (0x1<<25)

#define PREICFG_PDN_CLR	0x10
#define DDC_PDN_CLR (0x1<<25)

   /***********************************************/
#define CLK_CFG_UPDATE 0x04
#define hdmi_ck_update (0x1 << 11)
#define dpi1_ck_update (0x1 << 6)

#define GPIO_MODE4 0x5630
#define HDMISCK (0x1 << 12)
#define HDMI2SCK (0x2 << 12)
#define SCKMASK (0x7 << 12)
#define GPIO_MODE5 0x5640
#define HDMISDA (0x1 << 0)
#define HDMI2SDA (0x2 << 0)
#define SDAMASK (0x7 << 0)

   /***********************************************/
#define MMSYS_CG_CON1   0x110
#define DISP_PWM_MM_CG	         (0x1 << 0)
#define DPI1_ENGINE_CLK_CG       (0x1 << 16)
#define DPI1_ENGINE_CLK_CG_CLR   (0x0 << 16)
#define DPI1_PIXEL_CLK_CG        (0x1 << 17)
#define DPI1_PIXEL_CLK_CG_CLR    (0x0 << 17)
#define HDMI_PIXEL_CLK_CG	 (0x1 << 18)
#define HDMI_PIXEL_CLK_CG_CLR	 (0x0 << 18)
#define HDMI_SPDIF_CLK_CG	 (0x1 << 19)
#define HDMI_SPDIF_CLK_CG_CLR	 (0x0 << 19)
#define HDMI_ADSP_BCK_CG	 (0x1 << 20)
#define HDMI_ADSP_BCK_CG_CLR     (0x0 << 20)
#define HDMI_PLL_CK_CG		 (0x1 << 21)
#define HDMI_PLL_CK_CG_CLR	 (0x0 << 21)

#define DISP_HDMI_SYS_CFG_00   0x900
#define HDMI_SPIDIF_ON		(0x01 << 0)
#define HDMI_RST           (0x01 << 1)
#define ANLG_ON           (0x01 << 2)
#define CFG10_DVI          (0x01 << 3)
#define TEST_HDMI_TX_REG   (0x01 << 4)
#define HDMI_TEST_SEL		 (0x7 << 5)
#define SYS_KEYMASK1       (0xff << 8)
#define SYS_KEYMASK2       (0xff << 16)
#define AUD_OUTSYNC_EN     (((unsigned int)1) << 24)
#define AUD_OUTSYNC_PRE_EN (((unsigned int)1) << 25)
#define I2CM_ON            (((unsigned int)1) << 26)
#define E2PROM_TYPE_8BIT   (((unsigned int)1) << 27)
#define MCM_E2PROM_ON      (((unsigned int)1) << 28)
#define EXT_E2PROM_ON      (((unsigned int)1) << 29)
#define HTPLG_PIN_SEL_OFF  (((unsigned int)1) << 30)
#define AES_EFUSE_ENABLE   (((unsigned int)1) << 31)

#define DISP_HDMI_SYS_CFG_01   0x904
#define DEEP_COLOR_MODE_MASK (0x3 << 1)
#define COLOR_8BIT_MODE		(0 << 1)
#define COLOR_10BIT_MODE	(1 << 1)
#define COLOR_12BIT_MODE	 (2 << 1)
#define COLOR_16BIT_MODE	 (3 << 1)
#define DEEP_COLOR_EN		 (1 << 0)
#define HDMI_AUDIO_TEST_SEL	(0x01 << 8)
#define HDMI_PSECURE_EN           (0x0 << 15)
#define HDMI_PSECURE_EN_MASK	    (0x1 << 15)
#define HDMI_OUT_FIFO_EN		  (0x01 << 16)
#define HDMI_OUT_FIFO_CLK_INV	  (0x01 << 17)
#define MHL_MODE_ON		(0x01 << 28)
#define HDMI_PP_MODE		(0x01 << 29)
#define MHL_SYNC_AUTO_EN		  (0x01 << 30)
#define HDMI_PCLK_FREE_RUN	  (0x01 << 31)

#define HDMI_SYS_CFG24   0x24
#define HDMI_SYS_CFG28   0x28
#define HDMI_SYS_FMETER   0x4c
#define TRI_CAL (1 << 0)
#define CLK_EXC (1 << 1)
#define CAL_OK (1 << 2)
#define CALSEL (2 << 3)
#define CAL_CNT (0xffff << 16)

#define HDMI_SYS_PWR_RST_B  0x100
#define hdmi_pwr_sys_sw_reset (0 << 0)
#define hdmi_pwr_sys_sw_unreset (1 << 0)

#define HDMI_PWR_CTRL  0x104
#define hdmi_iso_dis (0 << 0)
#define hdmi_iso_en (1 << 0)
#define hdmi_power_turnoff (0 << 1)
#define hdmi_power_turnon (1 << 1)
#define hdmi_clock_on (0 << 2)
#define hdmi_clock_off (1 << 2)

#define HDMI_SYS_AMPCTRL  0x328
#define CK_TXAMP_ENB             0x00000008
#define D0_TXAMP_ENB             0x00000080
#define D1_TXAMP_ENB             0x00000800
#define D2_TXAMP_ENB             0x00008000
#define RSET_DTXST_OFF           0x00080000
#define SET_DTXST_ON             0x00800000
#define ENTXTST_ENB              0x08000000
#define CKFIFONEG                0x80000000
#define RG_SET_DTXST           (1 << 23)

#define HDMI_SYS_AMPCTRL1  0x32c

#define HDMI_SYS_PLLCTRL1  0x330
#define PRE_AMP_ENB           0x00000080
#define INV_CLCK              0x80000000
#define RG_ENCKST   (1 << 2)

#define HDMI_SYS_PLLCTRL2  0x334
#define POW_HDMITX    (0x01 << 20)
#define POW_PLL_L     (0x01 << 21)


#define HDMI_SYS_PLLCTRL3  0x338
#define PLLCTRL3_MASK 0xffffffff
#define RG_N3_MASK             0x0000001f
#define N3_POS              0
#define RG_N4_MASK             (3 << 5)
#define N4_POS              5

#define RG_BAND_MASK_0         (1 << 7)
#define BAND_MASK_0_POS     7
#define RG_BAND_MASK_1         (1 << 11)
#define BAND_MASK_1_POS     11


#define HDMI_SYS_PLLCTRL4  0x33c
#define PLLCTRL4_MASK 0xffffffff
#define RG_ENRST_CALIB  (1 << 21)

#define HDMI_SYS_PLLCTRL5  0x340
#define PLLCTRL5_MASK 0xffffffff

#define TURN_ON_RSEN_RESISTANCE     (1 << 7)
#define RG_N5_MASK             0x0000001f
#define N5_POS              0
#define RG_N6_MASK             (3 << 5)
#define N6_POS              5

#define RG_N1_MASK             0x00001f00
#define N1_POS              8


#define HDMI_SYS_PLLCTRL6  0x344
#define PLLCTRL6_MASK 0xffffffff
#define RG_CPLL1_VCOCAL_EN (1 << 25)
#define ABIST_MODE_SET (0x5C << 24)
#define ABIST_MODE_SET_MSK (0xFF << 24)
#define ABIST_MODE_EN (1 << 19)
#define ABIST_LV_EN (1 << 16)
#define RG_DISC_TH  (0x3 << 0)
#define RG_CK148M_EN (1 << 4)

#define HDMI_SYS_PLLCTRL7  0x348
#define PLLCTRL7_MASK  0xffffffff
#define TX_DRV_ENABLE (0xFF << 16)
#define TX_DRV_ENABLE_MSK (0xFF << 16)

#define HDMI_CON0	0x300
#define RG_HDMITX_DRV_IBIAS				(0)
#define RG_HDMITX_DRV_IBIAS_MASK		(0x3F << 0)
#define RG_HDMITX_SER_PASS_SEL			(6)
#define RG_HDMITX_SER_PASS_SEL_MASK		(0x03 << 6)
#define RG_HDMITX_EN_SER_ABEDG			(0x01 << 8)
#define RG_HDMITX_EN_SER_ABIST			(0x01 << 9)	/* NC */
#define RG_HDMITX_EN_SER_PEM			(0x01 << 10)
#define RG_HDMITX_EN_DIN_BIST			(0x01 << 11)
#define RG_HDMITX_EN_SER				(12)
#define RG_HDMITX_EN_SER_MASK			(0x0F << 12)
#define RG_HDMITX_EN_SLDO				(16)
#define RG_HDMITX_EN_SLDO_MASK			(0x0F << 16)
#define RG_HDMITX_EN_PRED				(20)
#define RG_HDMITX_EN_PRED_MASK			(0x0F << 20)
#define RG_HDMITX_EN_IMP				(24)
#define RG_HDMITX_EN_IMP_MASK			(0x0F << 24)
#define RG_HDMITX_EN_DRV				(28)
#define RG_HDMITX_EN_DRV_MASK			(0x0F << 28)

#define HDMI_CON1	0x304
#define RG_HDMITX_SER_DIN				(4)	/* SHIFT  VALUE */
#define RG_HDMITX_SER_DIN_MASK			(0x3FF << 4)
#define RG_HDMITX_SLDO_LVROD			(14)
#define RG_HDMITX_SLDO_LVROD_MASK		(0x03 << 14)
#define RG_HDMITX_CKLDO_LVROD			(16)
#define RG_HDMITX_CKLDO_LVROD_MASK		(0x03 << 16)
#define RG_HDMITX_PRED_IBIAS			(18)	/* set Predivirver bias current */
#define RG_HDMITX_PRED_IBIAS_MASK		(0x0F << 18)
#define RG_HDMITX_PRED_IMP				(0x01 << 22)	/* mask */
#define RG_HDMITX_PRED_IMP_SHIFT		(22)
#define RG_HDMITX_SER_DIN_SEL			(0x01 << 23)
#define RG_HDMITX_SER_CLKDIG_INV		(0x01 << 24)
#define RG_HDMITX_SER_BIST_TOG			(0x01 << 25)
#define RG_HDMITX_DRV_IMP				(26)	/* shift value */
#define RG_HDMITX_DRV_IMP_MASK			(0x3F << 26)

#define HDMI_CON2	0x308
#define RG_HDMITX_EN_TX_CKLDO			(0x01 << 0)	/* MASK */
#define RG_HDMITX_EN_TX_POSDIV			(0x01 << 1)
#define RG_HDMITX_REFEXT_SEL			(0x01 << 2)
#define RG_HDMITX_TX_POSDIV				(3)	/* shift value */
#define RG_HDMITX_TX_POSDIV_MASK		(0x03 << 3)
#define RG_HDMITX_TX_POSDIV_SEL			(0x01 << 5)
#define RG_HDMITX_EN_MBIAS				(0x01 << 6)
#define RG_HDMITX_MBIAS_LPF_EN			(0x01 << 7)
#define RG_HDMITX_EN_MBIAS_LOX			(0x01 << 8)
#define RG_HDMITX_EN_MBIAS_LOI			(0x01 << 9)
#define RG_HDMITX_MBIAS_LOXBI			(10)
#define RG_HDMITX_MBIAS_LOXBI_MASK		(0x03 << 10)
#define RG_HDMITX_MBIAS_LOIBI			(12)
#define RG_HDMITX_MBIAS_LOIBI_MASK		(0x03 << 12)
#define RG_HDMITX_DATA_CLKCH			(14)
#define RG_HDMITX_DATA_CLKCH_MASK		(0x3FF << 14)
#define RG_HDMITX_SER_TEST_SEL			(24)
#define RG_HDMITX_SER_TEST_SEL_MASK		(0xFF << 24)

#define HDMI_CON3	0x30c
#define RG_HDMITX_TEST_SEL				(16)
#define RG_HDMITX_TEST_SEL_MASK			(0x3F << 16)
#define RG_HDMITX_SER_EIN_SEL_CKCH		(0x01 << 22)
#define RG_HDMITX_SER_BIST_TOG_CKCH		(0x01 << 23)
#define RG_HDMITX_TEST_EN				(24)
#define RG_HDMITX_TEST_EN_MASK			(0x0F << 24)
#define RG_HDMITX_TEST_SEL_TOP			(28)
#define RG_HDMITX_TEST_SEL_TOP_MASK		(0x03 << 28)
#define RG_HDMITX_TEST_DIV_SEL			(30)
#define RG_HDMITX_TEST_DIV_SEL_MASK		(0x02 << 30)

#define HDMI_CON4	0x310
#define RG_HDMITX_RESERVE			(0)
#define RG_HDMITX_D0_IMP_SHIFT		(0)
#define RG_HDMITX_D0_IMP_MASK		(0x3f << 0)
#define RG_HDMITX_D1_IMP_SHIFT		(8)
#define RG_HDMITX_D1_IMP_MASK		(0x3f << 8)
#define RG_HDMITX_D2_IMP_SHIFT		(16)
#define RG_HDMITX_D2_IMP_MASK		(0x3f << 16)
#define RG_HDMITX_RESERVE_MASK			(0xFFFFFFFF << 0)

#define HDMI_CON5	0x314
#define RGS_HDMITX_CAL_STATUS			(4)
#define RGS_HDMITX_CAL_STATUS_MASK		(0xFF << 4)
#define RGS_HDMITX_ABIST_21LEV			(12)
#define RGS_HDMITX_ABIST_21LEV_MASK		(0x0F << 12)
#define RGS_HDMITX_ABIST_21EDG			(16)
#define RGS_HDMITX_ABIST_21EDG_MASK		(0x0F << 16)
#define RGS_HDMITX_ABIST_51LEV			(20)
#define RGS_HDMITX_ABIST_51LEV_MASK		(0x0F << 20)
#define RGS_HDMITX_ABIST_51EDG			(24)
#define RGS_HDMITX_ABIST_51EDG_MASK		(0x0F << 24)
#define RGS_HDMITX_PLUG_TST				(0x01 << 31)

#define HDMI_CON6	0x318
#define RG_HTPLL_BR						(0)
#define RG_HTPLL_BR_MASK				(0x03 << 0)
#define RG_HTPLL_BC						(2)
#define RG_HTPLL_BC_MASK				(0x03 << 2)
#define RG_HTPLL_BP						(4)
#define RG_HTPLL_BP_MASK				(0x0F << 4)
#define RG_HTPLL_IR						(8)
#define RG_HTPLL_IR_MASK				(0x0F << 8)
#define RG_HTPLL_IC						(12)
#define RG_HTPLL_IC_MASK				(0x0F << 12)
#define RG_HTPLL_POSDIV					(16)
#define RG_HTPLL_POSDIV_MASK			(0x03 << 16)
#define RG_HTPLL_PREDIV					(18)
#define RG_HTPLL_PREDIV_MASK			(0x03 << 18)
#define RG_HTPLL_FBKSEL					(20)
#define RG_HTPLL_FBKSEL_MASK			(0x03 << 20)
#define RG_HTPLL_RLH_EN					(0x01 << 22)
#define RG_HTPLL_DDSFBK_EN				(0x01 << 23)
#define RG_HTPLL_FBKDIV					(24)
#define RG_HTPLL_FBKDIV_MASK			(0x7F << 24)
#define RG_HTPLL_EN						(0x01 << 31)

#define HDMI_CON7	0x31c
#define RG_HTPLL_RESERVE				(0)
#define RG_HTPLL_RESERVE_MASK			(0xFF << 0)
#define RG_HTPLL_BIAS_EN				(0x01 << 8)
#define RG_HTPLL_BIAS_LPF_EN			(0x01 << 9)
#define RG_HTPLL_OSC_RST				(0x01 << 10)
#define RG_HTPLL_DET_EN					(0x01 << 11)
#define RG_HTPLL_VOD_EN					(0x01 << 12)
#define RG_HTPLL_MONREF_EN				(0x01 << 13)
#define RG_HTPLL_MONVC_EN				(0x01 << 14)
#define RG_HTPLL_MONCK_EN				(0x01 << 15)
#define RG_HTPLL_BAND					(16)
#define RG_HTPLL_BAND_MASK				(0x7F << 16)
#define RG_HTPLL_AUTOK_EN				(0x01 << 23)
#define RG_HTPLL_AUTOK_KF				(24)
#define RG_HTPLL_AUTOK_KF_MASK			(0x03 << 24)
#define RG_HTPLL_AUTOK_KS				(26)
#define RG_HTPLL_AUTOK_KS_MASK			(0x03 << 26)
#define RG_HTPLL_DIVEN					(28)
#define RG_HTPLL_DIVEN_MASK				(0x07 << 28)
#define RG_HTPLL_AUTOK_LOAD				(0x01 << 31)

#define HDMI_CON8	0x320
#define RGS_HTPLL_AUTOK_PASS			(0x01 << 15)
#define RGS_HTPLL_AUTOK_BAND			(0x01 << 23)
#define RGS_HTPLL_AUTOK_FAIL			(25)
#define RGS_HTPLL_AUTOK_FAIL_MASK		(0x7F << 25)


#define IO_PAD_PD 0x670
#define IO_PAD_HOT_PLUG_PD (0x1<<10)
#define IO_PAD_EN 0x570
#define IO_PAD_HOT_PLUG_EN (0x1<<10)

#define TVDPLL_CON0 0x1c0
#define  TVDPLL_EN (0x1 < 0)

#define TVDPLL_CON1	0x1c4
#define RG_TVDPLL_SDM_PCW				(0)
#define RG_TVDPLL_SDM_PCW_MASK			(0x1FFFFF)	/* */
#define TVDPLL_SDM_PCW_CHG        (1 << 31)
#define TVDPLL_SDM_PCW_F        (1<<23)
#define TVDPLL_POSDIV			24	/* default value 010, div = /4 */
#define TVDPLL_PISDIV_MASK      (0x7 << 24)

#define TVDPLL_PWR_CON0	 0x1d0
#define TVDPLL_PWR_ON		(1 << 0)
#define TVDPLL_SDM_ISO_EN	(1 << 1)


enum AUD_CH_NUM_T {
	AUD_INPUT_1_0 = 0,
	AUD_INPUT_1_1,
	AUD_INPUT_2_0,
	AUD_INPUT_2_1,
	AUD_INPUT_3_0,		/* C,L,R */
	AUD_INPUT_3_1,		/* C,L,R */
	AUD_INPUT_4_0,		/* L,R,RR,RL */
	AUD_INPUT_4_1,		/* L,R,RR,RL */
	AUD_INPUT_5_0,
	AUD_INPUT_5_1,
	AUD_INPUT_6_0,
	AUD_INPUT_6_1,
	AUD_INPUT_7_0,
	AUD_INPUT_7_1,
	AUD_INPUT_3_0_LRS,	/* LRS */
	AUD_INPUT_3_1_LRS,	/* LRS */
	AUD_INPUT_4_0_CLRS,	/* C,L,R,S */
	AUD_INPUT_4_1_CLRS,	/* C,L,R,S */
	/* new layout added for DTS */
	AUD_INPUT_6_1_Cs,
	AUD_INPUT_6_1_Ch,
	AUD_INPUT_6_1_Oh,
	AUD_INPUT_6_1_Chr,
	AUD_INPUT_7_1_Lh_Rh,
	AUD_INPUT_7_1_Lsr_Rsr,
	AUD_INPUT_7_1_Lc_Rc,
	AUD_INPUT_7_1_Lw_Rw,
	AUD_INPUT_7_1_Lsd_Rsd,
	AUD_INPUT_7_1_Lss_Rss,
	AUD_INPUT_7_1_Lhs_Rhs,
	AUD_INPUT_7_1_Cs_Ch,
	AUD_INPUT_7_1_Cs_Oh,
	AUD_INPUT_7_1_Cs_Chr,
	AUD_INPUT_7_1_Ch_Oh,
	AUD_INPUT_7_1_Ch_Chr,
	AUD_INPUT_7_1_Oh_Chr,
	AUD_INPUT_7_1_Lss_Rss_Lsr_Rsr,
	AUD_INPUT_6_0_Cs,
	AUD_INPUT_6_0_Ch,
	AUD_INPUT_6_0_Oh,
	AUD_INPUT_6_0_Chr,
	AUD_INPUT_7_0_Lh_Rh,
	AUD_INPUT_7_0_Lsr_Rsr,
	AUD_INPUT_7_0_Lc_Rc,
	AUD_INPUT_7_0_Lw_Rw,
	AUD_INPUT_7_0_Lsd_Rsd,
	AUD_INPUT_7_0_Lss_Rss,
	AUD_INPUT_7_0_Lhs_Rhs,
	AUD_INPUT_7_0_Cs_Ch,
	AUD_INPUT_7_0_Cs_Oh,
	AUD_INPUT_7_0_Cs_Chr,
	AUD_INPUT_7_0_Ch_Oh,
	AUD_INPUT_7_0_Ch_Chr,
	AUD_INPUT_7_0_Oh_Chr,
	AUD_INPUT_7_0_Lss_Rss_Lsr_Rsr,
	AUD_INPUT_8_0_Lh_Rh_Cs,
	AUD_INPUT_UNKNOWN = 0xFF
};

extern struct HDMI_AV_INFO_T _stAvdAVInfo;
extern unsigned char _bflagvideomute;
extern unsigned char _bflagaudiomute;
extern unsigned char _bsvpvideomute;
extern unsigned char _bsvpaudiomute;

extern unsigned char _bHdcpOff;
extern struct HDMI_SINK_AV_CAP_T _HdmiSinkAvCap;
extern unsigned int HDMI_INTERRUPT_FLAG;

extern unsigned int hdmi_drv_read(unsigned short u2Reg);
extern void hdmi_drv_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_sys_read(unsigned short u2Reg);
extern void hdmi_sys_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_pll_read(unsigned short u2Reg);
extern void hdmi_pll_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_pad_read(unsigned short u2Reg);
extern void hdmi_pad_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_hdmitopck_read(unsigned short u2Reg);
extern void hdmi_hdmitopck_write(unsigned short u2Reg, unsigned int u4Data);

extern void hdmi_infrasys_write(unsigned short u2Reg, unsigned int u4Data);
extern unsigned int hdmi_infrasys_read(unsigned short u2Reg);
extern void hdmi_perisys_write(unsigned short u2Reg, unsigned int u4Data);

#define vWriteByteHdmiGRL(dAddr, dVal)  (hdmi_drv_write(dAddr, dVal))
#define bReadByteHdmiGRL(bAddr)         (hdmi_drv_read(bAddr))
#define vWriteHdmiGRLMsk(dAddr, dVal, dMsk) \
(vWriteByteHdmiGRL((dAddr), (bReadByteHdmiGRL(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiSYS(dAddr, dVal)  (hdmi_sys_write(dAddr, dVal))
#define dReadHdmiSYS(dAddr)         (hdmi_sys_read(dAddr))
#define vWriteHdmiSYSMsk(dAddr, dVal, dMsk)\
(vWriteHdmiSYS((dAddr), (dReadHdmiSYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiTOPCK(dAddr, dVal)  (hdmi_hdmitopck_write(dAddr, dVal))
#define dReadHdmiTOPCK(dAddr)         (hdmi_hdmitopck_read(dAddr))
#define vWriteHdmiTOPCKMsk(dAddr, dVal, dMsk)\
(vWriteHdmiTOPCK((dAddr), (dReadHdmiTOPCK(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))
#define vWriteHdmiTOPCKUnMsk(dAddr, dMsk)(vWriteHdmiTOPCK((dAddr), (dReadHdmiTOPCK(dAddr) & (~(dMsk)))))

#define vWriteIoPll(dAddr, dVal)  (hdmi_pll_write(dAddr, dVal))
#define dReadIoPll(dAddr)         (hdmi_pll_read(dAddr))
#define vWriteIoPllMsk(dAddr, dVal, dMsk) vWriteIoPll((dAddr), (dReadIoPll(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))

#define vWriteIoPad(dAddr, dVal)  (hdmi_pad_write(dAddr, dVal))
#define dReadIoPad(dAddr)         (hdmi_pad_read(dAddr))
#define vWriteIoPadMsk(dAddr, dVal, dMsk) vWriteIoPad((dAddr), (dReadIoPad(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))


#define vWriteINFRASYS(dAddr, dVal)  (hdmi_infrasys_write(dAddr, dVal))
#define dReadINFRASYS(dAddr)         (hdmi_infrasys_read(dAddr))
#define vWriteINFRASYSMsk(dAddr, dVal, dMsk)\
(vWriteINFRASYS((dAddr), (dReadINFRASYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))
#define vWriteINFRASYSUnMsk(dAddr, dMsk)(vWriteINFRASYS((dAddr), (dReadINFRASYS(dAddr) & (~(dMsk)))))


#define vWritePeriSYS(dAddr, dVal)  (hdmi_perisys_write(dAddr, dVal))
#define dReadPeriSYS(dAddr)         (hdmi_perisys_read(dAddr))
#define vWritePeriSYSMsk(dAddr, dVal, dMsk)\
(vWriteINFRASYS((dAddr), (dReadINFRASYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))


extern void internal_hdmi_read(unsigned long u4Reg, unsigned int *p4Data);

extern void internal_hdmi_write(unsigned long u4Reg, unsigned int u4data);

extern void vSetCTL0BeZero(unsigned char fgBeZero);
extern void vWriteHdmiIntMask(unsigned char bMask);
extern void vHDMIAVUnMute(void);
extern void vHDMIAVMute(void);
extern void vTmdsOnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable);
extern void vChangeVpll(unsigned char bRes, unsigned char bdeepmode);
extern void vChgHDMIVideoResolution(unsigned char ui1resindex, unsigned char ui1colorspace,
				    unsigned char ui1hdmifs, unsigned char bdeepmode);
extern void vChgHDMIAudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex,
				unsigned char bdeepmode);
extern void vChgtoSoftNCTS(unsigned char ui1resindex, unsigned char ui1audiosoft,
			   unsigned char ui1hdmifs, unsigned char bdeepmode);
extern void vSendAVIInfoFrame(unsigned char ui1resindex, unsigned char ui1colorspace);
extern void hdmi_hdmistatus(void);
extern void vTxSignalOnOff(unsigned char bOn);
extern unsigned char bCheckPordHotPlug(unsigned char bMode);
extern void vHotPlugPinInit(struct platform_device *pdev);
extern void vBlackHDMIOnly(void);
extern void vUnBlackHDMIOnly(void);
extern void UnMuteHDMIAudio(void);
extern void MuteHDMIAudio(void);
extern unsigned char hdmi_get_port_hpd_value(void);
extern unsigned char vIsDviMode(void);

#endif
#endif
