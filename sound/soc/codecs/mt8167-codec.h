/*
 * mt8167-codec.h  --  MT8167 ALSA SoC codec driver
 *
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT8167_CODEC_H__
#define __MT8167_CODEC_H__

enum mt8167_codec_loopback_type {
	CODEC_LOOPBACK_NONE = 0,
	CODEC_LOOPBACK_AMIC_TO_SPK,
	CODEC_LOOPBACK_AMIC_TO_HP,
	CODEC_LOOPBACK_DMIC_TO_SPK,
	CODEC_LOOPBACK_DMIC_TO_HP,
	CODEC_LOOPBACK_HEADSET_MIC_TO_SPK,
	CODEC_LOOPBACK_HEADSET_MIC_TO_HP,
};

#define AFE_OFFSET               (0xA0000000)
#define APMIXED_OFFSET           (0x0B000000)

#define AFE_REG(reg)             (reg | AFE_OFFSET)
#define APMIXED_REG(reg)         (reg | APMIXED_OFFSET)

/* audio_top_sys */
#define AUDIO_TOP_CON0           AFE_REG(0x0000)
#define AFE_DAC_CON0             AFE_REG(0x0010)
#define AFE_DAC_CON1             AFE_REG(0x0014)
#define AFE_CONN1                AFE_REG(0x0024)
#define AFE_CONN2                AFE_REG(0x0028)
#define AFE_I2S_CON1             AFE_REG(0x0034)
#define AFE_DL1_BASE             AFE_REG(0x0040)
#define AFE_DL1_END              AFE_REG(0x0048)
#define AFE_CONN_24BIT           AFE_REG(0x006C)
#define AFE_ADDA_DL_SRC2_CON0    AFE_REG(0x0108)
#define AFE_ADDA_DL_SRC2_CON1    AFE_REG(0x010C)
#define AFE_ADDA_UL_DL_CON0      AFE_REG(0x0124)
#define AFE_ADDA_PREDIS_CON0     AFE_REG(0x0260)
#define AFE_ADDA_PREDIS_CON1     AFE_REG(0x0264)
#define AFE_MEMIF_PBUF_SIZE      AFE_REG(0x03D8)
#define ABB_AFE_CON0             AFE_REG(0x0F00)
#define ABB_AFE_CON1             AFE_REG(0x0F04)
#define ABB_AFE_CON2             AFE_REG(0x0F08)
#define ABB_AFE_CON3             AFE_REG(0x0F0C)
#define ABB_AFE_CON4             AFE_REG(0x0F10)
#define ABB_AFE_CON5             AFE_REG(0x0F14)
#define ABB_AFE_CON6             AFE_REG(0x0F18)
#define ABB_AFE_CON7             AFE_REG(0x0F1C)
#define ABB_AFE_CON8             AFE_REG(0x0F20)
#define ABB_AFE_CON9             AFE_REG(0x0F24)
#define ABB_AFE_CON10            AFE_REG(0x0F28)
#define ABB_AFE_CON11            AFE_REG(0x0F2C)
#define ABB_AFE_STA0             AFE_REG(0x0F30)
#define ABB_AFE_STA1             AFE_REG(0x0F34)
#define ABB_AFE_STA2             AFE_REG(0x0F38)
#define AFE_MON_DEBUG0           AFE_REG(0x0F44)
#define AFE_MON_DEBUG1           AFE_REG(0x0F48)
#define ABB_AFE_SDM_TEST         AFE_REG(0x0F4C)

/* apmixedsys */
#define AUDIO_CODEC_CON00        APMIXED_REG(0x0700)
#define AUDIO_CODEC_CON01        APMIXED_REG(0x0704)
#define AUDIO_CODEC_CON02        APMIXED_REG(0x0708)
#define AUDIO_CODEC_CON03        APMIXED_REG(0x070C)
#define AUDIO_CODEC_CON04        APMIXED_REG(0x0710)

/* ABB_AFE_CON9 */
#define ABB_AFE_CON9_TWO_WIRE_EN BIT(8)
#define ABB_AFE_CON9_DIG_MIC_EN  BIT(4)
#define ABB_AFE_CON9_D3P25M_SEL  BIT(0)

/* ABB_AFE_CON11 */
#define ABB_AFE_CON11_DC_CTRL         BIT(9)
#define ABB_AFE_CON11_TOP_CTRL        BIT(8)
#define ABB_AFE_CON11_DC_CTRL_STATUS  BIT(1)
#define ABB_AFE_CON11_TOP_CTRL_STATUS BIT(0)

/* AUDIO_CODEC_CON00 */
#define AUDIO_CODEC_CON00_AUDULL_VREF24_EN BIT(20)
#define AUDIO_CODEC_CON00_AUDULL_VCM14_EN  BIT(19)
#define AUDIO_CODEC_CON00_AUDULR_VREF24_EN BIT(2)
#define AUDIO_CODEC_CON00_AUDULR_VCM14_EN  BIT(1)

/* AUDIO_CODEC_CON02 */
#define AUDIO_CODEC_CON02_ABUF_INSHORT BIT(29)

/* AUDIO_CODEC_CON03 */
#define AUDIO_CODEC_CON03_DIG_MIC_EN   BIT(28)
#define AUDIO_CODEC_CON03_SLEW_RATE_11 (0x3 << 22)
#define AUDIO_CODEC_CON03_SLEW_RATE_10 (0x2 << 22)
#define AUDIO_CODEC_CON03_SLEW_RATE_01 (0x1 << 22)
#define AUDIO_CODEC_CON03_SLEW_RATE_00 (0x0 << 22)

#endif
