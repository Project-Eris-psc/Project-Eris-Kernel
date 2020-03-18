/*
 * mt8167-codec-utils.c  --  MT8167 codec driver utility functions
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

#include <sound/soc.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include "mt8167-codec.h"
#include "mt8167-codec-utils.h"
#ifdef CONFIG_MTK_SPEAKER
#include "mt6392-codec.h"
#endif
#ifdef CONFIG_MTK_AUXADC
#include "mtk_auxadc.h"
#endif

enum auxadc_channel_id {
	AUXADC_CH_AU_HPL = 7,
	AUXADC_CH_AU_HPR,
};

struct reg_setting {
	uint32_t reg;
	uint32_t mask;
	uint32_t val;
};

static const struct reg_setting mt8167_codec_cali_setup_regs[] = {
	{
		.reg = AUDIO_TOP_CON0,
		.mask = GENMASK(26, 25),
		.val = 0x0,
	},
	{
		.reg = AUDIO_TOP_CON0,
		.mask = BIT(2),
		.val = 0x0,
	},
	{
		.reg = AFE_MEMIF_PBUF_SIZE,
		.mask = GENMASK(17, 16),
		.val = 0x0,
	},
	{
		.reg = AFE_CONN_24BIT,
		.mask = GENMASK(4, 3),
		.val = 0x0,
	},
	{
		.reg = AFE_DAC_CON1,
		.mask = GENMASK(3, 0),
		.val = 0x9,
	},
	{
		.reg = AFE_CONN1,
		.mask = BIT(21),
		.val = BIT(21),
	},
	{
		.reg = AFE_CONN2,
		.mask = BIT(6),
		.val = BIT(6),
	},
	{
		.reg = AFE_DAC_CON0,
		.mask = BIT(1),
		.val = BIT(1),
	},
	{
		.reg = AFE_ADDA_PREDIS_CON0,
		.mask = GENMASK(31, 0),
		.val = 0x0,
	},
	{
		.reg = AFE_ADDA_PREDIS_CON1,
		.mask = GENMASK(31, 0),
		.val = 0x0,
	},
	{
		.reg = AFE_ADDA_DL_SRC2_CON0,
		.mask = GENMASK(31, 0),
		.val = (0x7 << 28) | (0x03 << 24) | (0x03 << 11) | BIT(1),
	},
	{
		.reg = AFE_ADDA_DL_SRC2_CON1,
		.mask = GENMASK(31, 0),
		.val = 0xf74f0000,
	},
	{ /* I2S2_OUT_MODE (44.1khz) */
		.reg = AFE_I2S_CON1,
		.mask = (0x9 << 8),
		.val = (0x9 << 8),
	},
	{
		.reg = AFE_ADDA_DL_SRC2_CON0,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{
		.reg = AFE_I2S_CON1,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{
		.reg = AFE_ADDA_UL_DL_CON0,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{
		.reg = AFE_DAC_CON0,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{ /* ADA_HPLO_TO_AUXADC */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(15),
		.val = BIT(15),
	},
	{ /* ADA_HPRO_TO_AUXADC */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(14),
		.val = BIT(14),
	},
};

static const struct reg_setting mt8167_codec_cali_cleanup_regs[] = {
	{ /* ADA_HPLO_TO_AUXADC */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(15),
		.val = 0x0,
	},
	{ /* ADA_HPRO_TO_AUXADC */
		.reg = AUDIO_CODEC_CON02,
		.mask = 0x0,
		.val = BIT(14),
	},
	{
		.reg = AFE_DAC_CON0,
		.mask = BIT(1),
		.val = 0x0,
	},
	{
		.reg = AFE_ADDA_DL_SRC2_CON0,
		.mask = BIT(0),
		.val = 0x0,
	},
	{
		.reg = AFE_I2S_CON1,
		.mask = BIT(0),
		.val = 0x0,
	},
	{
		.reg = AFE_ADDA_UL_DL_CON0,
		.mask = BIT(0),
		.val = 0x0,
	},
	{
		.reg = AFE_DAC_CON0,
		.mask = BIT(0),
		.val = 0x0,
	},
	{
		.reg = AUDIO_TOP_CON0,
		.mask = BIT(2),
		.val = BIT(2),
	},
	{
		.reg = AUDIO_TOP_CON0,
		.mask = GENMASK(26, 25),
		.val = (0x3 << 25),
	},
};

static const struct reg_setting mt8167_codec_cali_enable_regs[] = {
	{ /* dl_rate (44.1khz) */
		.reg = ABB_AFE_CON1,
		.mask = GENMASK(3, 0),
		.val = 9,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = ABB_AFE_CON11_TOP_CTRL,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = 0x0,
	},
	{
		.reg = ABB_AFE_CON3,
		.mask = GENMASK(31, 0),
		.val = 0x0,
	},
	{
		.reg = ABB_AFE_CON4,
		.mask = GENMASK(31, 0),
		.val = 0x0,
	},
	{
		.reg = ABB_AFE_CON10,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{
		.reg = ABB_AFE_CON0,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(27) | BIT(17) | BIT(16),
		.val = BIT(27) | BIT(17) | BIT(16),
	},
	{
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(19),
		.val = BIT(19),
	},
	{
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(15) | BIT(14) | BIT(13) | BIT(12) | BIT(11),
		.val = BIT(15) | BIT(14) | BIT(13) | BIT(12) | BIT(11),
	},
	{
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(28) | BIT(25) | BIT(22),
		.val = BIT(28),
	},
};

static const struct reg_setting mt8167_codec_cali_disable_regs[] = {
	{
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(28) | BIT(25) | BIT(22),
		.val = BIT(25) | BIT(22),
	},
	{
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(15) | BIT(14) | BIT(13) | BIT(12) | BIT(11),
		.val = 0x0,
	},
	{
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(19),
		.val = 0x0,
	},
	{
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(27) | BIT(17) | BIT(16),
		.val = 0x0,
	},
	{
		.reg = ABB_AFE_CON0,
		.mask = BIT(0),
		.val = 0x0,
	},
};

static int mt8167_codec_apply_reg_setting(struct snd_soc_codec *codec,
	const struct reg_setting regs[], uint32_t reg_nums)
{
	int i;

	for (i = 0 ; i < reg_nums ; i++)
		snd_soc_update_bits(codec,
			regs[i].reg, regs[i].mask, regs[i].val);

	return 0;
}

#define AFIFO_SIZE (48 * 1024)
static int mt8167_codec_setup_cali_path(struct snd_soc_codec *codec,
	struct snd_dma_buffer *buf)
{
	/* enable power */
	snd_soc_update_bits(codec, AUDIO_CODEC_CON01, BIT(20), BIT(20));
	/* enable clock */
	snd_soc_update_bits(codec, AUDIO_CODEC_CON03, BIT(30), BIT(30));
	snd_soc_update_bits(codec, AUDIO_CODEC_CON04, BIT(15), BIT(15));
	/* allocate buffer */
	buf->area = dma_zalloc_coherent(codec->dev, AFIFO_SIZE,
		&buf->addr, GFP_KERNEL);

	snd_soc_update_bits(codec,
			AFE_DL1_BASE,
			GENMASK(31, 0),
			buf->addr);
	snd_soc_update_bits(codec,
			AFE_DL1_END,
			GENMASK(31, 0),
			buf->addr + (AFIFO_SIZE - 1));

	/* setup */
	return mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_cali_setup_regs,
			ARRAY_SIZE(mt8167_codec_cali_setup_regs));
}

static int mt8167_codec_cleanup_cali_path(struct snd_soc_codec *codec,
	struct snd_dma_buffer *buf)
{
	int ret = 0;

	/* cleanup */
	ret = mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_cali_cleanup_regs,
			ARRAY_SIZE(mt8167_codec_cali_cleanup_regs));

	/* free buffer */
	dma_free_coherent(codec->dev, AFIFO_SIZE, buf->area,
		buf->addr);
	/* disable clock */
	snd_soc_update_bits(codec, AUDIO_CODEC_CON04, BIT(15), 0x0);
	snd_soc_update_bits(codec, AUDIO_CODEC_CON03, BIT(30), 0x0);
	/* disable power */
	snd_soc_update_bits(codec, AUDIO_CODEC_CON01, BIT(20), 0x0);

	return ret;
}

static int mt8167_codec_enable_cali_path(struct snd_soc_codec *codec)
{
	/* enable */
	return mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_cali_enable_regs,
			ARRAY_SIZE(mt8167_codec_cali_enable_regs));
}

static int mt8167_codec_disable_cali_path(struct snd_soc_codec *codec)
{
	/* disable */
	return mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_cali_disable_regs,
			ARRAY_SIZE(mt8167_codec_cali_disable_regs));
}

static int32_t mt8167_codec_get_hp_cali_val(struct snd_soc_codec *codec,
	uint32_t auxadc_channel)
{
	int32_t cali_val = 0;
	struct snd_dma_buffer dma_buf;
#ifdef CONFIG_MTK_AUXADC
	int32_t auxadc_on_val = 0;
	int32_t auxadc_off_val = 0;
	int32_t auxadc_val_sum = 0;
	int32_t auxadc_val_avg = 0;
	int32_t count = 0;
	int32_t countlimit = 5;
#endif

	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_setup_cali_path(codec, &dma_buf);

	usleep_range(10 * 1000, 15 * 1000);

#ifdef CONFIG_MTK_AUXADC
	IMM_GetOneChannelValue_Cali(auxadc_channel, &auxadc_off_val);
	dev_dbg(codec->dev, "%s auxadc_off_val: %d\n",
		__func__, auxadc_off_val);
#endif

	mt8167_codec_enable_cali_path(codec);

	usleep_range(10 * 1000, 15 * 1000);

#ifdef CONFIG_MTK_AUXADC
	for (count = 0 ; count < countlimit ; count++) {
		IMM_GetOneChannelValue_Cali(auxadc_channel, &auxadc_on_val);
		auxadc_val_sum += auxadc_on_val;
		dev_dbg(codec->dev, "%s auxadc_on_val: %d, auxadc_val_sum: %d\n",
			__func__, auxadc_on_val, auxadc_val_sum);
	}

	auxadc_val_avg = auxadc_val_sum / countlimit;

	cali_val = auxadc_off_val - auxadc_val_avg;

	cali_val = cali_val/1000; /* mV */
#endif

	mt8167_codec_disable_cali_path(codec);
	mt8167_codec_cleanup_cali_path(codec, &dma_buf);

	return cali_val;
}

uint32_t mt8167_codec_conv_dc_offset_to_comp_val(int32_t dc_offset)
{
	uint32_t dccomp_val = 0;
	bool invert = false;

	if (!dc_offset)
		return 0;

	if (dc_offset > 0)
		invert = true;
	else
		dc_offset *= (-1);

	/* transform to 1.8V scale */
	dc_offset = (dc_offset * 18) / 10;

	/*
	 * ABB_AFE_CON3, 1 step is (1/32768) * 1800 mV = 0.0549
	 * dccomp_val = dc_cali_value/0.0549 = dc_cali_value * 18
	 */
	dccomp_val = dc_offset * 18;

	/* two's complement to change the direction of dc compensation value */
	if (invert)
		dccomp_val = 0xFFFFFFFF - dccomp_val + 1;

	return dccomp_val;
}
EXPORT_SYMBOL_GPL(mt8167_codec_conv_dc_offset_to_comp_val);

int mt8167_codec_get_hpl_cali_val(struct snd_soc_codec *codec,
	uint32_t *dccomp_val, int32_t *dc_offset)
{
	*dc_offset = mt8167_codec_get_hp_cali_val(codec, AUXADC_CH_AU_HPL);
	*dccomp_val = mt8167_codec_conv_dc_offset_to_comp_val(*dc_offset);

	dev_dbg(codec->dev, "%s dc_offset %d, dccomp_val %d\n",
		__func__, *dc_offset, *dccomp_val);
	return 0;
}
EXPORT_SYMBOL_GPL(mt8167_codec_get_hpl_cali_val);

int mt8167_codec_get_hpr_cali_val(struct snd_soc_codec *codec,
	uint32_t *dccomp_val, int32_t *dc_offset)
{
	*dc_offset = mt8167_codec_get_hp_cali_val(codec, AUXADC_CH_AU_HPR);
	*dccomp_val = mt8167_codec_conv_dc_offset_to_comp_val(*dc_offset);

	dev_dbg(codec->dev, "%s dc_offset %d, dccomp_val %d\n",
		__func__, *dc_offset, *dccomp_val);
	return 0;
}
EXPORT_SYMBOL_GPL(mt8167_codec_get_hpr_cali_val);

static bool ul_is_builtin_mic(uint32_t type)
{
	bool ret = false;

	switch (type) {
	case CODEC_LOOPBACK_AMIC_TO_SPK:
	case CODEC_LOOPBACK_AMIC_TO_HP:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool ul_is_headset_mic(uint32_t type)
{
	bool ret = false;

	switch (type) {
	case CODEC_LOOPBACK_HEADSET_MIC_TO_SPK:
	case CODEC_LOOPBACK_HEADSET_MIC_TO_HP:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool ul_is_amic(uint32_t type)
{
	return ul_is_builtin_mic(type) ||
			ul_is_headset_mic(type);
}

static bool ul_is_dmic(uint32_t type)
{
	bool ret = false;

	switch (type) {
	case CODEC_LOOPBACK_DMIC_TO_SPK:
	case CODEC_LOOPBACK_DMIC_TO_HP:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool dl_is_spk(uint32_t type)
{
	bool ret = false;

	switch (type) {
	case CODEC_LOOPBACK_AMIC_TO_SPK:
	case CODEC_LOOPBACK_DMIC_TO_SPK:
	case CODEC_LOOPBACK_HEADSET_MIC_TO_SPK:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool dl_is_hp(uint32_t type)
{
	bool ret = false;

	switch (type) {
	case CODEC_LOOPBACK_AMIC_TO_HP:
	case CODEC_LOOPBACK_DMIC_TO_HP:
	case CODEC_LOOPBACK_HEADSET_MIC_TO_HP:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static const struct reg_setting mt8167_codec_ul_rate_32000_setup_regs[] = {
	{ /* ul_rate (32khz) */
		.reg = ABB_AFE_CON1,
		.mask = GENMASK(7, 4),
		.val = 0x0,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = ABB_AFE_CON11_TOP_CTRL,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_ul_rate_48000_setup_regs[] = {
	{ /* ul_rate (48khz) */
		.reg = ABB_AFE_CON1,
		.mask = GENMASK(7, 4),
		.val = (0x1 << 4),
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = ABB_AFE_CON11_TOP_CTRL,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_dl_rate_32000_setup_regs[] = {
	{ /* dl_rate (32khz) */
		.reg = ABB_AFE_CON1,
		.mask = GENMASK(3, 0),
		.val = 8,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = ABB_AFE_CON11_TOP_CTRL,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_dl_rate_48000_setup_regs[] = {
	{ /* dl_rate (48khz) */
		.reg = ABB_AFE_CON1,
		.mask = GENMASK(3, 0),
		.val = 10,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = ABB_AFE_CON11_TOP_CTRL,
	},
	{ /* toggle top_ctrl */
		.reg = ABB_AFE_CON11,
		.mask = ABB_AFE_CON11_TOP_CTRL,
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_builtin_amic_enable_regs[] = {
	{ /* micbias0 */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(17),
		.val = BIT(17),
	},
	{ /* left pga mux <- AU_VIN0 */
		.reg = AUDIO_CODEC_CON00,
		.mask = GENMASK(31, 28),
		.val = 0x0,
	},
	{ /* right pga mux <- AU_VIN2 */
		.reg = AUDIO_CODEC_CON00,
		.mask = GENMASK(13, 10),
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_headset_amic_enable_regs[] = {
	{ /* micbias1 */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(21),
		.val = BIT(21),
	},
	{ /* left pga mux <- AU_VIN1 */
		.reg = AUDIO_CODEC_CON00,
		.mask = GENMASK(31, 28),
		.val = BIT(28),
	},
	{ /* right pga mux <- AU_VIN1 */
		.reg = AUDIO_CODEC_CON00,
		.mask = GENMASK(13, 10),
		.val = BIT(10),
	},
};

static const struct reg_setting mt8167_codec_amic_enable_regs[] = {
	{ /* CLKLDO power on */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(20),
		.val = BIT(20),
	},
	{ /* Audio Codec CLK on */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(30),
		.val = BIT(30),
	},
	{ /* UL CLK on */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(21),
		.val = BIT(21),
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
		.val = AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
		.val = AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
	},
	{ /* vref24 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULL_VREF24_EN,
		.val = AUDIO_CODEC_CON00_AUDULL_VREF24_EN,
	},
	{ /* vref24 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULR_VREF24_EN,
		.val = AUDIO_CODEC_CON00_AUDULR_VREF24_EN,
	},
	{ /* ul_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(1),
		.val = BIT(1),
	},
	{ /* left pga */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(24),
		.val = BIT(24),
	},
	{ /* right pga */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(6),
		.val = BIT(6),
	},
	{ /* left adc */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(23),
		.val = BIT(23),
	},
	{ /* right adc */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(5),
		.val = BIT(5),
	},
};

static const struct reg_setting mt8167_codec_amic_disable_regs[] = {
	{ /* left adc */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(23),
		.val = 0x0,
	},
	{ /* right adc */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(5),
		.val = 0x0,
	},
	{ /* left pga */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(24),
		.val = 0x0,
	},
	{ /* right pga */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(6),
		.val = 0x0,
	},
	{ /* micbias0 */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(17),
		.val = 0x0,
	},
	{ /* micbias1 */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(21),
		.val = 0x0,
	},
	{ /* left pga mux <- OPEN */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(28),
		.val = (0x2 << 28),
	},
	{ /* right pga mux <- OPEN */
		.reg = AUDIO_CODEC_CON00,
		.mask = BIT(10),
		.val = (0x2 << 10),
	},
	{ /* ul_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(1),
		.val = 0x0,
	},
	{ /* vref24 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULL_VREF24_EN,
		.val = 0x0,
	},
	{ /* vref24 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULR_VREF24_EN,
		.val = 0x0,
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
		.val = 0x0,
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
		.val = 0x0,
	},
	{ /* UL CLK off */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(21),
		.val = 0x0,
	},
	{ /* Audio Codec CLK off */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(30),
		.val = 0x0,
	},
	{ /* CLKLDO power off */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(20),
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_dmic_enable_regs[] = {
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
		.val = AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
		.val = AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
	},
	{ /* micbias0 */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(17),
		.val = BIT(17),
	},
	{ /* ul_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(1),
		.val = BIT(1),
	},
	{ /* dig_mic_en (one-wire) */
		.reg = ABB_AFE_CON9,
		.mask = ABB_AFE_CON9_DIG_MIC_EN,
		.val = ABB_AFE_CON9_DIG_MIC_EN,
	},
	{ /* Digital microphone enable */
		.reg = AUDIO_CODEC_CON03,
		.mask = AUDIO_CODEC_CON03_DIG_MIC_EN,
		.val = AUDIO_CODEC_CON03_DIG_MIC_EN,
	},
};

static const struct reg_setting mt8167_codec_dmic_disable_regs[] = {
	{ /* Digital microphone enable */
		.reg = AUDIO_CODEC_CON03,
		.mask = AUDIO_CODEC_CON03_DIG_MIC_EN,
		.val = 0x0,
	},
	{ /* dig_mic_en (one-wire) */
		.reg = ABB_AFE_CON9,
		.mask = ABB_AFE_CON9_DIG_MIC_EN,
		.val = 0x0,
	},
	{ /* micbias0 */
		.reg = AUDIO_CODEC_CON03,
		.mask = BIT(17),
		.val = 0x0,
	},
	{ /* ul_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(1),
		.val = 0x0,
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
		.val = 0x0,
	},
	{ /* vcm14 */
		.reg = AUDIO_CODEC_CON00,
		.mask = AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_spk_enable_regs[] = {
	{ /* DL CLK on */
		.reg = AUDIO_CODEC_CON04,
		.mask = BIT(15),
		.val = BIT(15),
	},
	{ /* dl_vef24 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(16),
		.val = BIT(16),
	},
	{ /* dac_clk */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(27),
		.val = BIT(27),
	},
	{ /* dl_vcm1 */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(13),
		.val = BIT(13),
	},
	{ /* dl_vcm2 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(17),
		.val = BIT(17),
	},
	{ /* dl_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{ /* left dac */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(15),
		.val = BIT(15),
	},
	{ /* voice amp */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(8),
		.val = BIT(8),
	},
};

static const struct reg_setting mt8167_codec_spk_disable_regs[] = {
	{ /* voice amp */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(8),
		.val = 0x0,
	},
	{ /* left dac */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(15),
		.val = 0x0,
	},
	{ /* dl_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(0),
		.val = 0x0,
	},
	{ /* dl_vcm2 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(17),
		.val = 0x0,
	},
	{ /* dl_vcm1 */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(13),
		.val = 0x0,
	},
	{ /* dac_clk */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(27),
		.val = 0x0,
	},
	{ /* dl_vef24 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(16),
		.val = 0x0,
	},
	{ /* DL CLK off */
		.reg = AUDIO_CODEC_CON04,
		.mask = BIT(15),
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_hp_enable_regs[] = {
	{ /* DL CLK on */
		.reg = AUDIO_CODEC_CON04,
		.mask = BIT(15),
		.val = BIT(15),
	},
	{ /* dl_vef24 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(16),
		.val = BIT(16),
	},
	{ /* dac_clk */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(27),
		.val = BIT(27),
	},
	{ /* dl_vcm1 */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(13),
		.val = BIT(13),
	},
	{ /* dl_vcm2 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(17),
		.val = BIT(17),
	},
	{ /* dl_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(0),
		.val = BIT(0),
	},
	{ /* left dac */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(15),
		.val = BIT(15),
	},
	{ /* right dac */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(14),
		.val = BIT(14),
	},
	{ /* left audio amp */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(12),
		.val = BIT(12),
	},
	{ /* right audio amp */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(11),
		.val = BIT(11),
	},
	{ /* HP Pre-charge function release */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(28),
		.val = BIT(28),
	},
	{ /* Disable the depop mux of HP drivers */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(25),
		.val = 0x0,
	},
	{ /* Disable the depop VCM gen */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(22),
		.val = 0x0,
	},
};

static const struct reg_setting mt8167_codec_hp_disable_regs[] = {
	{ /* Reset HP Pre-charge function */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(28),
		.val = 0x0,
	},
	{ /* Enable the depop mux of HP drivers */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(25),
		.val = BIT(25),
	},
	{ /* Enable depop VCM gen */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(22),
		.val = BIT(22),
	},
	{ /* left audio amp */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(12),
		.val = 0x0,
	},
	{ /* right audio amp */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(11),
		.val = 0x0,
	},
	{ /* left dac */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(15),
		.val = 0x0,
	},
	{ /* right dac */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(14),
		.val = 0x0,
	},
	{ /* dl_en */
		.reg = ABB_AFE_CON0,
		.mask = BIT(0),
		.val = 0x0,
	},
	{ /* dl_vcm2 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(17),
		.val = 0x0,
	},
	{ /* dl_vcm1 */
		.reg = AUDIO_CODEC_CON01,
		.mask = BIT(13),
		.val = 0x0,
	},
	{ /* dac_clk */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(27),
		.val = 0x0,
	},
	{ /* dl_vef24 */
		.reg = AUDIO_CODEC_CON02,
		.mask = BIT(16),
		.val = 0x0,
	},
	{ /* DL CLK off */
		.reg = AUDIO_CODEC_CON04,
		.mask = BIT(15),
		.val = 0x0,
	},
};

static void mt8167_codec_turn_on_ul_amic_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	dev_dbg(codec->dev, "%s %s\n", __func__,
		ul_is_builtin_mic(lpbk_type) ? "built-in mic" : "headset mic");

	if (ul_is_builtin_mic(lpbk_type))
		mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_builtin_amic_enable_regs,
			ARRAY_SIZE(mt8167_codec_builtin_amic_enable_regs));
	else if (ul_is_headset_mic(lpbk_type))
		mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_headset_amic_enable_regs,
			ARRAY_SIZE(mt8167_codec_headset_amic_enable_regs));

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_amic_enable_regs,
		ARRAY_SIZE(mt8167_codec_amic_enable_regs));
}

static void mt8167_codec_turn_on_ul_dmic_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_dmic_enable_regs,
		ARRAY_SIZE(mt8167_codec_dmic_enable_regs));
}

static void mt8167_codec_turn_off_ul_amic_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_amic_disable_regs,
		ARRAY_SIZE(mt8167_codec_amic_disable_regs));
}

static void mt8167_codec_turn_off_ul_dmic_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_dmic_disable_regs,
		ARRAY_SIZE(mt8167_codec_dmic_disable_regs));
}

static void mt8167_codec_turn_on_dl_spk_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_spk_enable_regs,
		ARRAY_SIZE(mt8167_codec_spk_enable_regs));
}

static void mt8167_codec_turn_on_dl_hp_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_hp_enable_regs,
		ARRAY_SIZE(mt8167_codec_hp_enable_regs));
}

static void mt8167_codec_turn_off_dl_spk_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_spk_disable_regs,
		ARRAY_SIZE(mt8167_codec_spk_disable_regs));
}

static void mt8167_codec_turn_off_dl_hp_path(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);

	mt8167_codec_apply_reg_setting(codec,
		mt8167_codec_hp_disable_regs,
		ARRAY_SIZE(mt8167_codec_hp_disable_regs));
}

static void mt8167_codec_setup_ul_rate(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	if (ul_is_amic(lpbk_type))
		mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_ul_rate_48000_setup_regs,
			ARRAY_SIZE(mt8167_codec_ul_rate_48000_setup_regs));
	else if (ul_is_dmic(lpbk_type))
		mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_ul_rate_32000_setup_regs,
			ARRAY_SIZE(mt8167_codec_ul_rate_32000_setup_regs));
}

static void mt8167_codec_turn_on_ul_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	mt8167_codec_setup_ul_rate(codec, lpbk_type);
	if (ul_is_amic(lpbk_type))
		mt8167_codec_turn_on_ul_amic_path(codec, lpbk_type);
	else if (ul_is_dmic(lpbk_type))
		mt8167_codec_turn_on_ul_dmic_path(codec);
}

static void mt8167_codec_turn_off_ul_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	if (ul_is_amic(lpbk_type))
		mt8167_codec_turn_off_ul_amic_path(codec);
	else if (ul_is_dmic(lpbk_type))
		mt8167_codec_turn_off_ul_dmic_path(codec);
}

static void mt8167_codec_setup_dl_rate(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	if (ul_is_amic(lpbk_type))
		mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_dl_rate_48000_setup_regs,
			ARRAY_SIZE(mt8167_codec_dl_rate_48000_setup_regs));
	else if (ul_is_dmic(lpbk_type))
		mt8167_codec_apply_reg_setting(codec,
			mt8167_codec_dl_rate_32000_setup_regs,
			ARRAY_SIZE(mt8167_codec_dl_rate_32000_setup_regs));
}

static void mt8167_codec_turn_on_dl_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	mt8167_codec_setup_dl_rate(codec, lpbk_type);
	if (dl_is_spk(lpbk_type)) {
		mt8167_codec_turn_on_dl_spk_path(codec);
#ifdef CONFIG_MTK_SPEAKER
		mt6392_int_spk_turn_on(codec);
#endif
	} else if (dl_is_hp(lpbk_type))
		mt8167_codec_turn_on_dl_hp_path(codec);
}

static void mt8167_codec_turn_off_dl_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	if (dl_is_spk(lpbk_type)) {
#ifdef CONFIG_MTK_SPEAKER
		mt6392_int_spk_turn_off(codec);
#endif
		mt8167_codec_turn_off_dl_spk_path(codec);
	} else if (dl_is_hp(lpbk_type))
		mt8167_codec_turn_off_dl_hp_path(codec);
}

void mt8167_codec_turn_on_lpbk_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	mt8167_codec_turn_on_ul_path(codec, lpbk_type);
	mt8167_codec_turn_on_dl_path(codec, lpbk_type);
}
EXPORT_SYMBOL_GPL(mt8167_codec_turn_on_lpbk_path);

void mt8167_codec_turn_off_lpbk_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type)
{
	mt8167_codec_turn_off_dl_path(codec, lpbk_type);
	mt8167_codec_turn_off_ul_path(codec, lpbk_type);
}
EXPORT_SYMBOL_GPL(mt8167_codec_turn_off_lpbk_path);
