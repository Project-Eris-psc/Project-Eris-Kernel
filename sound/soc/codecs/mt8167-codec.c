/*
 * mt8167-codec.c  --  MT8167 ALSA SoC codec driver
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

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include "mt8167-codec.h"
#include "mt8167-codec-utils.h"

#ifdef CONFIG_MTK_SPEAKER
#include "mt6392-codec.h"
#endif

#define DMIC_PHASE_NUM		8

enum regmap_module_id {
	REGMAP_AFE = 0,
	REGMAP_APMIXEDSYS,
#ifdef CONFIG_MTK_SPEAKER
	REGMAP_PWRAP,
#endif
	REGMAP_NUMS,
};

enum dmic_wire_mode {
	DMIC_ONE_WIRE = 0,
	DMIC_TWO_WIRE,
};

enum dmic_rate_mode {
	DMIC_RATE_D1P625M = 0,
	DMIC_RATE_D3P25M,
};

enum dmic_ch_mode {
	DMIC_L_CH = 0,
	DMIC_R_CH,
	DMIC_CH_NUM,
};

enum codec_pga_gain_enum_id {
	HP_L_PGA_GAIN = 0,
	HP_R_PGA_GAIN,
	LOUT_PGA_GAIN,
	UL_L_PGA_GAIN,
	UL_R_PGA_GAIN,
	PGA_GAIN_MAX,
};

struct mt8167_codec_priv {
#ifdef CONFIG_MTK_SPEAKER
	struct mt6392_codec_priv mt6392_data;
#endif
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct regmap *regmap_modules[REGMAP_NUMS];
	uint32_t lch_dccomp_val; /* L-ch DC compensation value */
	uint32_t rch_dccomp_val; /* R-ch DC compensation value */
	int32_t lch_dc_offset; /* L-ch DC offset value */
	int32_t rch_dc_offset; /* R-ch DC offset value */
	bool is_lch_dc_calibrated;
	bool is_rch_dc_calibrated;
	uint32_t pga_gain[PGA_GAIN_MAX];
	uint32_t dmic_wire_mode;
	uint32_t dmic_ch_phase[DMIC_CH_NUM];
	uint32_t dmic_rate_mode; /* 0:1.625MHz 1:3.25MHz */
	uint32_t loopback_type;
	bool dl_en;
	struct clk *clk;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};

#define MT8167_CODEC_NAME "mt8167-codec"

static int mt8167_codec_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *codec_dai)
{
	dev_dbg(codec_dai->codec->dev, "%s\n", __func__);
	return 0;
}

static void mt8167_codec_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *codec_dai)
{
	dev_dbg(codec_dai->codec->dev, "%s\n", __func__);
}

struct mt8167_codec_rate {
	unsigned int rate;
	unsigned int regvalue;
};

static const struct mt8167_codec_rate mt8167_codec_ul_rates[] = {
	{ .rate =  8000, .regvalue = 0 },
	{ .rate = 16000, .regvalue = 0 },
	{ .rate = 32000, .regvalue = 0 },
	{ .rate = 48000, .regvalue = 1 },
};

static const struct mt8167_codec_rate mt8167_codec_dl_rates[] = {
	{ .rate =   8000, .regvalue = 0 },
	{ .rate =  11025, .regvalue = 1 },
	{ .rate =  12000, .regvalue = 2 },
	{ .rate =  16000, .regvalue = 3 },
	{ .rate =  22025, .regvalue = 4 },
	{ .rate =  24000, .regvalue = 5 },
	{ .rate =  32000, .regvalue = 6 },
	{ .rate =  44100, .regvalue = 7 },
	{ .rate =  48000, .regvalue = 8 },
};

static int mt8167_codec_ul_rate_to_val(struct mt8167_codec_priv *codec_data,
	int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8167_codec_ul_rates); i++)
		if (mt8167_codec_ul_rates[i].rate == rate)
			return mt8167_codec_ul_rates[i].regvalue;

	dev_err(codec_data->codec->dev, "%s unsupported ul rate %d\n",
			__func__, rate);

	return -EINVAL;
}

static int mt8167_codec_dl_rate_to_val(struct mt8167_codec_priv *codec_data,
	int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8167_codec_dl_rates); i++)
		if (mt8167_codec_dl_rates[i].rate == rate)
			return mt8167_codec_dl_rates[i].regvalue;

	dev_err(codec_data->codec->dev, "%s unsupported dl rate %d\n",
			__func__, rate);

	return -EINVAL;
}

static int mt8167_codec_valid_new_rate(struct mt8167_codec_priv *codec_data)
{
	uint32_t abb_afe_con11_val = 0;

	abb_afe_con11_val = ABB_AFE_CON11_TOP_CTRL;

	/* toggle top_ctrl status */
	if (snd_soc_read(codec_data->codec, ABB_AFE_CON11) &
			ABB_AFE_CON11_TOP_CTRL_STATUS)
		snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON11,
			ABB_AFE_CON11_TOP_CTRL, 0x0);
	else
		snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON11,
			ABB_AFE_CON11_TOP_CTRL, abb_afe_con11_val);

	return 0;
}

static int mt8167_codec_setup_ul_rate(struct snd_soc_dai *codec_dai, int rate)
{
	struct mt8167_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec_dai->codec);
	uint32_t val = 0;

	if (mt8167_codec_ul_rate_to_val(codec_data, rate) < 0) {
		dev_err(codec_dai->codec->dev,
			"%s error to get ul rate\n", __func__);
		return -EINVAL;
	}

	val = mt8167_codec_ul_rate_to_val(codec_data, rate);
	snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON1, (0xF << 4), (val << 4));
	mt8167_codec_valid_new_rate(codec_data);

	return 0;
}

static int mt8167_codec_setup_dl_rate(struct snd_soc_dai *codec_dai, int rate)
{
	struct mt8167_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec_dai->codec);
	uint32_t val = 0;

	if (mt8167_codec_dl_rate_to_val(codec_data, rate) < 0) {
		dev_err(codec_dai->codec->dev,
			"%s error to get dl rate\n", __func__);
		return -EINVAL;
	}

	val = mt8167_codec_dl_rate_to_val(codec_data, rate);
	snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON1, GENMASK(3, 0), val);
	mt8167_codec_valid_new_rate(codec_data);

	return 0;
}

static int mt8167_codec_valid_new_dc_comp(
			struct mt8167_codec_priv *codec_data)
{
	/* toggle DC status */
	if (snd_soc_read(codec_data->codec, ABB_AFE_CON11) &
			ABB_AFE_CON11_DC_CTRL_STATUS)
		snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON11,
			ABB_AFE_CON11_DC_CTRL, 0);
	else
		snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON11,
			ABB_AFE_CON11_DC_CTRL, ABB_AFE_CON11_DC_CTRL);

	return 0;
}

static void mt8167_codec_setup_dc_comp(struct snd_soc_dai *codec_dai)
{
	struct mt8167_codec_priv *codec_data =
			snd_soc_codec_get_drvdata(codec_dai->codec);

	/*  L-ch DC compensation value */
	snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON3, GENMASK(15, 0),
			codec_data->lch_dccomp_val);
	/*  R-ch DC compensation value */
	snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON4, GENMASK(15, 0),
			codec_data->rch_dccomp_val);
	/* DC compensation enable */
	snd_soc_update_bits(codec_data->codec, ABB_AFE_CON10, 0x1, 0x1);
	mt8167_codec_valid_new_dc_comp(codec_data);
}

static int mt8167_codec_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *codec_dai)
{
	int ret = 0;
	int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dev_dbg(codec_dai->codec->dev,
			"%s capture rate = %d\n", __func__, rate);
		ret = mt8167_codec_setup_ul_rate(codec_dai, rate);
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dev_dbg(codec_dai->codec->dev,
			"%s playback rate = %d\n", __func__, rate);
		ret = mt8167_codec_setup_dl_rate(codec_dai, rate);
		mt8167_codec_setup_dc_comp(codec_dai);
	}

	return ret;
}

static int mt8167_codec_hw_free(struct snd_pcm_substream *substream,
			struct snd_soc_dai *codec_dai)
{
	dev_dbg(codec_dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int mt8167_codec_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *codec_dai)
{
	dev_err(codec_dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int mt8167_codec_dl_ul_enable(struct snd_pcm_substream *substream,
		struct snd_soc_dai *codec_dai)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_update_bits(codec_dai->codec,
			ABB_AFE_CON0, BIT(1), BIT(1));
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_update_bits(codec_dai->codec,
			ABB_AFE_CON0, BIT(0), BIT(0));

	return 0;
}

static int mt8167_codec_dl_ul_disable(struct snd_pcm_substream *substream,
		struct snd_soc_dai *codec_dai)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_update_bits(codec_dai->codec,
			ABB_AFE_CON0, BIT(1), 0x0);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_update_bits(codec_dai->codec,
			ABB_AFE_CON0, BIT(0), 0x0);

	return 0;
}

static int mt8167_codec_trigger(struct snd_pcm_substream *substream,
			int command,
			struct snd_soc_dai *codec_dai)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8167_codec_dl_ul_enable(substream, codec_dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8167_codec_dl_ul_disable(substream, codec_dai);
		break;
	}
	dev_err(codec_dai->codec->dev, "%s command = %d\n ",
			__func__, command);
	return 0;
}

static const struct snd_soc_dai_ops mt8167_codec_aif_dai_ops = {
	.startup = mt8167_codec_startup,
	.shutdown = mt8167_codec_shutdown,
	.hw_params = mt8167_codec_hw_params,
	.hw_free = mt8167_codec_hw_free,
	.prepare = mt8167_codec_prepare,
	.trigger = mt8167_codec_trigger,
};

#define MT8167_CODEC_DL_RATES SNDRV_PCM_RATE_8000_48000
#define MT8167_CODEC_UL_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)

static struct snd_soc_dai_driver mt8167_codec_dai = {
	.name = "mt8167-codec-dai",
	.ops = &mt8167_codec_aif_dai_ops,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MT8167_CODEC_DL_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MT8167_CODEC_UL_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
};

static int mt8167_codec_dmic_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);
	uint32_t abb_afe_con9_val = 0;
	uint32_t audio_codec_con03_val = 0;

	if (codec_data->dmic_wire_mode == DMIC_TWO_WIRE)
		abb_afe_con9_val |=
			ABB_AFE_CON9_TWO_WIRE_EN;

	if (codec_data->dmic_rate_mode == DMIC_RATE_D3P25M)
		abb_afe_con9_val |=
			ABB_AFE_CON9_D3P25M_SEL;

	abb_afe_con9_val |=
		(codec_data->dmic_ch_phase[DMIC_L_CH] << 13);
	abb_afe_con9_val |=
		(codec_data->dmic_ch_phase[DMIC_R_CH] << 10);

	abb_afe_con9_val |=
		ABB_AFE_CON9_DIG_MIC_EN;

	audio_codec_con03_val =
		AUDIO_CODEC_CON03_SLEW_RATE_10 |
		AUDIO_CODEC_CON03_DIG_MIC_EN;

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, ABB_AFE_CON9,
			abb_afe_con9_val, abb_afe_con9_val);
		snd_soc_update_bits(codec, AUDIO_CODEC_CON03,
			audio_codec_con03_val, audio_codec_con03_val);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, AUDIO_CODEC_CON03,
			AUDIO_CODEC_CON03_DIG_MIC_EN, 0x0);
		snd_soc_update_bits(codec, ABB_AFE_CON9,
			ABB_AFE_CON9_DIG_MIC_EN, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt8167_codec_left_audio_amp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* store gain */
		codec_data->pga_gain[HP_L_PGA_GAIN] =
			snd_soc_read(codec, AUDIO_CODEC_CON01) &
				GENMASK(2, 0);
		/* set to small gain for depop sequence */
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON01, GENMASK(2, 0), 0x0);
		/* disable input short (left audio amp only) */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON02,
			AUDIO_CODEC_CON02_ABUF_INSHORT, 0x0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* enable input short to prevent signal leakage from L-DAC */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON02,
			AUDIO_CODEC_CON02_ABUF_INSHORT,
			AUDIO_CODEC_CON02_ABUF_INSHORT);
		/* restore gain */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON01,
				GENMASK(2, 0),
				codec_data->pga_gain[HP_L_PGA_GAIN]);
		break;
	default:
		break;
	}

	return 0;
}

static int mt8167_codec_right_audio_amp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* store gain */
		codec_data->pga_gain[HP_R_PGA_GAIN] =
			(snd_soc_read(codec, AUDIO_CODEC_CON01) &
				GENMASK(5, 3)) >> 3;
		/* set to small gain for depop sequence */
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON01, GENMASK(5, 3), 0x0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* restore gain */
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON01, GENMASK(5, 3),
			(codec_data->pga_gain[HP_R_PGA_GAIN]) << 3);
		break;
	default:
		break;
	}

	return 0;
}

static int mt8167_codec_voice_amp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* store gain */
		codec_data->pga_gain[LOUT_PGA_GAIN] =
			(snd_soc_read(codec, AUDIO_CODEC_CON02) &
				GENMASK(12, 9)) >> 9;
		/* set to small gain for depop sequence */
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON02, GENMASK(12, 9), 0x0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* restore gain (fade in ?) */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON02,
			GENMASK(12, 9),
			(codec_data->pga_gain[LOUT_PGA_GAIN]) << 9);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* store gain */
		codec_data->pga_gain[LOUT_PGA_GAIN] =
			(snd_soc_read(codec, AUDIO_CODEC_CON02) &
				GENMASK(12, 9)) >> 9;
		/* set to small gain for depop sequence (fade out ?) */
		snd_soc_update_bits(codec,
				AUDIO_CODEC_CON02, GENMASK(12, 9), 0x0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* restore gain */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON02,
			GENMASK(12, 9),
			(codec_data->pga_gain[LOUT_PGA_GAIN]) << 9);
		break;
	default:
		break;
	}

	return 0;
}

/* CODEC_CLK */
static int mt8167_codec_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CLKLDO power on */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON01, BIT(20), BIT(20));
		/* Audio Codec CLK on */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON03, BIT(30), BIT(30));
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Audio Codec CLK off */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON03, BIT(30), 0x0);
		/* CLKLDO power off */
		snd_soc_update_bits(codec, AUDIO_CODEC_CON01, BIT(20), 0x0);
		break;
	default:
		break;
	}

	return 0;
}

/* 1.4V common mode voltage */
static int mt8167_codec_vcm14_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULL_VCM14_EN,
			AUDIO_CODEC_CON00_AUDULL_VCM14_EN);
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULR_VCM14_EN,
			AUDIO_CODEC_CON00_AUDULR_VCM14_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULL_VCM14_EN, 0x0);
		snd_soc_update_bits(codec,
			AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULR_VCM14_EN, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

/* Uplink 2.4V differential reference */
static int mt8167_codec_ul_vref24_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULL_VREF24_EN,
			AUDIO_CODEC_CON00_AUDULL_VREF24_EN);
		snd_soc_update_bits(codec, AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULR_VREF24_EN,
			AUDIO_CODEC_CON00_AUDULR_VREF24_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULL_VREF24_EN, 0x0);
		snd_soc_update_bits(codec, AUDIO_CODEC_CON00,
			AUDIO_CODEC_CON00_AUDULR_VREF24_EN, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static void mt8167_codec_hp_depop_setup(
			struct mt8167_codec_priv *codec_data)
{
	/* Set audio DAC bias current */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON01, (0x1F << 6), 0x0);
	/* Set the charge option of depop VCM gen. to "charge type" */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(18), BIT(18));
	/* Set the 22uA current step of depop VCM gen. to charge 22uF cap. */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, GENMASK(20, 19), (0x3 << 19));//47uf
	/* Set the depop VCM voltage of depop VCM gen. to 1.35V. */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(21), 0x0);
	/* Enable the depop VCM generator. */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(22), BIT(22));
	/* Set the series resistor of depop mux of HP drivers to 62.5 Ohm. */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, (0x3 << 23), (0x3 << 23));
	/* Disable audio DAC clock. */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(27), 0x0);
	/* Enable the depop mux of HP drivers. */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(25), BIT(25));
}

static void mt8167_codec_hp_depop_cleanup(
			struct mt8167_codec_priv *codec_data)
{
	/* Set the charge option of depop VCM gen. to "dis-charge type" */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(18), 0x0);
	/* Set the 33uF cap current step of depop VCM gen to dis-charge */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, (0x2 << 19), (0x2 << 19));
	/* Disable the depop VCM generator */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, BIT(22), 0x0);
}

/* PRE_PMD */
static void mt8167_codec_hp_depop_enable(
			struct mt8167_codec_priv *codec_data)
{
	/* store gain */
	codec_data->pga_gain[HP_L_PGA_GAIN] =
		snd_soc_read(codec_data->codec, AUDIO_CODEC_CON01) &
			GENMASK(2, 0);
	codec_data->pga_gain[HP_R_PGA_GAIN] =
		(snd_soc_read(codec_data->codec, AUDIO_CODEC_CON01) &
			GENMASK(5, 3)) >> 3;
	/* set to small gain for depop sequence */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON01, GENMASK(2, 0), 0x0);
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON01, GENMASK(5, 3), 0x0);
	/* Reset HP Pre-charge function */
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			BIT(28), 0x0);
	/* Enable the depop mux of HP drivers */
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			BIT(25), BIT(25));
	/* Enable depop VCM gen */
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			BIT(22), BIT(22));
}

/* POST_PMU */
static void mt8167_codec_hp_depop_disable(
			struct mt8167_codec_priv *codec_data)
{
	/* HP Pre-charge function release */
	usleep_range(10000, 11000);
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			BIT(28), BIT(28));
	/* Disable the depop mux of HP drivers */
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			BIT(25), 0x0);
	/* Disable the depop VCM gen */
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			BIT(22), 0x0);
	/* restore gain */
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON01,
			GENMASK(2, 0),
			codec_data->pga_gain[HP_L_PGA_GAIN]);
	snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON01,
			GENMASK(5, 3),
			(codec_data->pga_gain[HP_R_PGA_GAIN]) << 3);
}

static int mt8167_codec_depop_vcm_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mt8167_codec_hp_depop_disable(codec_data);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mt8167_codec_hp_depop_enable(codec_data);
		break;
	default:
		break;
	}

	return 0;
}

/* AIF DL_UL loopback Switch */
static int mt8167_codec_aif_dl_ul_lpbk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* Enable downlink data loopback to uplink */
		snd_soc_update_bits(codec, ABB_AFE_CON2, BIT(3), BIT(3));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Disable downlink data loopback to uplink */
		snd_soc_update_bits(codec, ABB_AFE_CON2, BIT(3), 0x0);
		break;
	default:
		break;
	}

	return 0;
}

/* DMIC Data Gen Switch */
static int mt8167_codec_dmic_data_gen_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* ul_dmic_debug_ch1/2 data 1 */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				GENMASK(13, 12), (0x3 << 12));
		/* ul_dmic_debug_en (enable) */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				BIT(8), BIT(8));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* ul_dmic_debug_ch1/2 data 0 */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				GENMASK(13, 12), 0x0);
		/* ul_dmic_debug_en (disable) */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				BIT(8), 0x0);
		break;
	default:
		break;
	}

	return 0;
}

/* AMIC Data Gen Switch */
static int mt8167_codec_amic_data_gen_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* ul_amic_debug_ch1/2 data 1 */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				GENMASK(15, 14), (0x3 << 14));
		/* ul_amic_debug_en (enable) */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				BIT(9), BIT(9));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* ul_amic_debug_ch1/2 data 0 */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				GENMASK(15, 14), 0x0);
		/* ul_amic_debug_en (disable) */
		snd_soc_update_bits(codec, ABB_AFE_CON1,
				BIT(9), 0x0);
		break;
	default:
		break;
	}

	return 0;
}

/* SDM Tone Gen Switch */
static int mt8167_codec_sdm_tone_gen_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* tri_amp_div */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				GENMASK(14, 12), (7 << 12));
		/* tri_freq_div */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				GENMASK(9, 4), (1 << 4));
		/* abb_sdm_src_sel_ctl (Triangular tone) */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				BIT(2), BIT(2));
		/* tri_mute_sw (Unmute trigen) */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				BIT(1), 0x0);
		/* tri_dac_en (Enable trigen) */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				BIT(0), BIT(0));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* tri_mute_sw (Mute trigen) */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				BIT(1), BIT(1));
		/* tri_dac_en (Disable trigen) */
		snd_soc_update_bits(codec, ABB_AFE_SDM_TEST,
				BIT(0), 0x0);
		break;
	default:
		break;
	}

	return 0;
}

/* Audio Amp Playback Volume
 * {-2, 0, +2, +4, +6, +8, +10, +12} dB
 */
static const unsigned int dl_audio_amp_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 7, TLV_DB_SCALE_ITEM(-200, 200, 0),
};

/* Voice Amp Playback Volume
 * {-18, -16, -14, -12, -10, ..., +12} dB
 */
static const unsigned int dl_voice_amp_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 15, TLV_DB_SCALE_ITEM(-1800, 200, 0),
};

/* PGA Capture Volume
 * {-6, 0, +6, +12, +18, +24} dB
 */
static const unsigned int ul_pga_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 5, TLV_DB_SCALE_ITEM(-600, 600, 0),
};

/* Headset_PGAL_GAIN
 * Headset_PGAR_GAIN
 * {-2, 0, +2, +4, +6, +8, +10, +12} dB
 */
static const char *const headset_pga_gain_text[] = {
	"-2dB", "+0dB", "+2dB", "+4dB",
	"+6dB", "+8dB", "+10dB", "+12dB",
};

/* Lineout_PGA_GAIN
 * {-18, -16, -14, -12, -10, ..., +12} dB
 */
static const char *const lineout_pga_gain_text[] = {
	"-18dB", "-16dB", "-14dB", "-12dB",
	"-10dB", "-8dB", "-6dB", "-4dB",
	"-2dB", "+0dB", "+2dB", "+4dB",
	"+6dB", "+8dB", "+10dB", "+12dB",
};

/* Audio_PGA1_Setting
 * Audio_PGA2_Setting
 * {-6, 0, +6, +12, +18, +24} dB
 */
static const char *const ul_pga_gain_text[] = {
	"-6dB", "+0dB", "+6dB", "+12dB", "+18dB", "+24dB"
};

static const struct soc_enum mt8167_codec_pga_gain_enums[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(headset_pga_gain_text),
		headset_pga_gain_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(headset_pga_gain_text),
		headset_pga_gain_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(lineout_pga_gain_text),
		lineout_pga_gain_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ul_pga_gain_text),
		ul_pga_gain_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ul_pga_gain_text),
		ul_pga_gain_text),
};

static int mt8167_codec_get_gain_enum_id(const char *name)
{
	if (!strcmp(name, "Headset_PGAL_GAIN"))
		return HP_L_PGA_GAIN;
	if (!strcmp(name, "Headset_PGAR_GAIN"))
		return HP_R_PGA_GAIN;
	if (!strcmp(name, "Lineout_PGA_GAIN"))
		return LOUT_PGA_GAIN;
	if (!strcmp(name, "Audio_PGA1_Setting"))
		return UL_L_PGA_GAIN;
	if (!strcmp(name, "Audio_PGA2_Setting"))
		return UL_R_PGA_GAIN;
	return -EINVAL;
}

static int mt8167_codec_pga_gain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	int id = mt8167_codec_get_gain_enum_id(kcontrol->id.name);
	uint32_t value = 0;

	switch (id) {
	case HP_L_PGA_GAIN:
	case HP_R_PGA_GAIN:
	case LOUT_PGA_GAIN:
	case UL_L_PGA_GAIN:
	case UL_R_PGA_GAIN:
		value = codec_data->pga_gain[id];
		break;
	default:
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mt8167_codec_pga_gain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int id = mt8167_codec_get_gain_enum_id(kcontrol->id.name);
	uint32_t value = ucontrol->value.integer.value[0];

	if (value >= e->items)
		return -EINVAL;

	dev_dbg(codec_data->codec->dev,
		"%s id %d, value %u\n", __func__, id, value);

	switch (id) {
	case HP_L_PGA_GAIN:
		snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON01,
			GENMASK(2, 0), value);
		break;
	case HP_R_PGA_GAIN:
		snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON01,
			GENMASK(5, 3), value << 3);
		break;
	case LOUT_PGA_GAIN:
		snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON02,
			GENMASK(12, 9), value << 9);
		break;
	case UL_L_PGA_GAIN:
		snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON00,
			GENMASK(27, 25), value << 25);
		break;
	case UL_R_PGA_GAIN:
		snd_soc_update_bits(codec_data->codec, AUDIO_CODEC_CON00,
			GENMASK(9, 7), value << 7);
		break;
	default:
		return -EINVAL;
	}

	codec_data->pga_gain[id] = value;

	return 0;
}

/* HPL Calibration */
static int mt8167_codec_hpl_dc_comp_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);

	if (!codec_data->is_lch_dc_calibrated) {
		mt8167_codec_get_hpl_cali_val(codec_data->codec,
			&codec_data->lch_dccomp_val,
			&codec_data->lch_dc_offset);
		codec_data->is_lch_dc_calibrated = true;
	}
	ucontrol->value.integer.value[0] = codec_data->lch_dc_offset;
	return 0;
}

static int mt8167_codec_hpl_dc_comp_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);

	codec_data->lch_dc_offset = ucontrol->value.integer.value[0];
	codec_data->lch_dccomp_val =
		mt8167_codec_conv_dc_offset_to_comp_val(
			codec_data->lch_dc_offset);
	return 0;
}

/* HPR Calibration */
static int mt8167_codec_hpr_dc_comp_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);
	if (!codec_data->is_rch_dc_calibrated) {
		mt8167_codec_get_hpr_cali_val(codec_data->codec,
			&codec_data->rch_dccomp_val,
			&codec_data->rch_dc_offset);
		codec_data->is_rch_dc_calibrated = true;
	}
	ucontrol->value.integer.value[0] = codec_data->rch_dc_offset;
	return 0;
}

static int mt8167_codec_hpr_dc_comp_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);

	codec_data->rch_dc_offset = ucontrol->value.integer.value[0];
	codec_data->rch_dccomp_val =
		mt8167_codec_conv_dc_offset_to_comp_val(
			codec_data->rch_dc_offset);

	return 0;
}

/* UL to DL loopback (Codec_Loopback_Select) */
#define ENUM_TO_STR(enum) #enum
static const char * const mt8167_codec_loopback_text[] = {
	ENUM_TO_STR(CODEC_LOOPBACK_NONE),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_DMIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_DMIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_HP),
};

static const struct soc_enum mt8167_codec_loopback_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8167_codec_loopback_text),
		mt8167_codec_loopback_text);

static int mt8167_codec_loopback_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);
	ucontrol->value.integer.value[0] = codec_data->loopback_type;
	return 0;
}

static int mt8167_codec_loopback_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t prev_lpbk_type = codec_data->loopback_type;
	uint32_t next_lpbk_type = ucontrol->value.integer.value[0];

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);

	if (next_lpbk_type == prev_lpbk_type) {
		dev_dbg(codec_data->codec->dev, "%s dummy action\n", __func__);
		return 0;
	}

	if (prev_lpbk_type != CODEC_LOOPBACK_NONE)
		mt8167_codec_turn_off_lpbk_path(
			codec_data->codec, prev_lpbk_type);
	if (next_lpbk_type != CODEC_LOOPBACK_NONE)
		mt8167_codec_turn_on_lpbk_path(
			codec_data->codec, next_lpbk_type);

	codec_data->loopback_type = ucontrol->value.integer.value[0];
	return 0;
}

/* Codec_DL_Switch */
static const char * const mt8167_codec_dl_switch_text[] = { "Off", "On" };

static const struct soc_enum mt8167_codec_dl_switch_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8167_codec_dl_switch_text),
		mt8167_codec_dl_switch_text);

static int mt8167_codec_dl_switch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);
	ucontrol->value.integer.value[0] = codec_data->dl_en;
	return 0;
}

static int mt8167_codec_dl_switch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t prev_dl_en = codec_data->dl_en;
	uint32_t next_dl_en = ucontrol->value.integer.value[0];

	dev_dbg(codec_data->codec->dev, "%s\n", __func__);

	if (next_dl_en == prev_dl_en) {
		dev_dbg(codec_data->codec->dev, "%s dummy action\n", __func__);
		return 0;
	}

	if (prev_dl_en)
		/* turn off */
		snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON0, BIT(0), 0x0);
	else
		/* turn on */
		snd_soc_update_bits(codec_data->codec,
			ABB_AFE_CON0, BIT(0), BIT(0));

	codec_data->dl_en = next_dl_en;
	return 0;
}

static int mt8167_codec_dmic_ch_phase_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->dmic_ch_phase[DMIC_L_CH];
	ucontrol->value.integer.value[1] = codec_data->dmic_ch_phase[DMIC_R_CH];
	return 0;
}

static int mt8167_codec_dmic_ch_phase_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	codec_data->dmic_ch_phase[DMIC_L_CH] = ucontrol->value.integer.value[0];
	codec_data->dmic_ch_phase[DMIC_R_CH] = ucontrol->value.integer.value[1];
	return 0;
}

static int mt8167_codec_dmic_rate_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->dmic_rate_mode;
	return 0;
}

static int mt8167_codec_dmic_rate_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8167_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	codec_data->dmic_rate_mode = ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new mt8167_codec_controls[] = {
	/* DL Audio amplifier gain adjustment */
	SOC_DOUBLE_TLV("Audio Amp Playback Volume",
		AUDIO_CODEC_CON01, 0, 3, 7, 0,
		dl_audio_amp_gain_tlv),
	/* DL Voice amplifier gain adjustment */
	SOC_SINGLE_TLV("Voice Amp Playback Volume",
		AUDIO_CODEC_CON02, 9, 15, 0,
		dl_voice_amp_gain_tlv),
	/* UL PGA gain adjustment */
	SOC_DOUBLE_TLV("PGA Capture Volume",
		AUDIO_CODEC_CON00, 25, 7, 5, 0,
		ul_pga_gain_tlv),
	/* Headset_PGAL_GAIN */
	SOC_ENUM_EXT("Headset_PGAL_GAIN",
		mt8167_codec_pga_gain_enums[HP_L_PGA_GAIN],
		mt8167_codec_pga_gain_get,
		mt8167_codec_pga_gain_put),
	/* Headset_PGAR_GAIN */
	SOC_ENUM_EXT("Headset_PGAR_GAIN",
		mt8167_codec_pga_gain_enums[HP_R_PGA_GAIN],
		mt8167_codec_pga_gain_get,
		mt8167_codec_pga_gain_put),
	/* Lineout_PGA_GAIN */
	SOC_ENUM_EXT("Lineout_PGA_GAIN",
		mt8167_codec_pga_gain_enums[LOUT_PGA_GAIN],
		mt8167_codec_pga_gain_get,
		mt8167_codec_pga_gain_put),
	/* Audio_PGA1_Setting */
	SOC_ENUM_EXT("Audio_PGA1_Setting",
		mt8167_codec_pga_gain_enums[UL_L_PGA_GAIN],
		mt8167_codec_pga_gain_get,
		mt8167_codec_pga_gain_put),
	/* Audio_PGA2_Setting */
	SOC_ENUM_EXT("Audio_PGA2_Setting",
		mt8167_codec_pga_gain_enums[UL_R_PGA_GAIN],
		mt8167_codec_pga_gain_get,
		mt8167_codec_pga_gain_put),
	/* HP calibration */
	SOC_SINGLE_EXT("Audio HPL Offset",
		SND_SOC_NOPM, 0, 0x8000, 0,
		mt8167_codec_hpl_dc_comp_get,
		mt8167_codec_hpl_dc_comp_put),
	SOC_SINGLE_EXT("Audio HPR Offset",
		SND_SOC_NOPM, 0, 0x8000, 0,
		mt8167_codec_hpr_dc_comp_get,
		mt8167_codec_hpr_dc_comp_put),
	/* UL to DL loopback */
	SOC_ENUM_EXT("Codec_Loopback_Select",
		mt8167_codec_loopback_enum,
		mt8167_codec_loopback_get,
		mt8167_codec_loopback_put),
	/* for factory usage */
	SOC_ENUM_EXT("Codec_DL_Switch",
		mt8167_codec_dl_switch_enum,
		mt8167_codec_dl_switch_get,
		mt8167_codec_dl_switch_put),
	/* for dmic phase debug */
	SOC_DOUBLE_EXT("Dmic Ch Phase",
		SND_SOC_NOPM, 0, 1, 7, 0,
		mt8167_codec_dmic_ch_phase_get,
		mt8167_codec_dmic_ch_phase_put),
	/* for dmic rate mode debug */
	SOC_SINGLE_EXT("Dmic Rate Mode",
		SND_SOC_NOPM, 0, 1, 0,
		mt8167_codec_dmic_rate_mode_get,
		mt8167_codec_dmic_rate_mode_put),
};

/* Left PGA Mux/Right PGA Mux */
static const char * const pga_mux_text[] = {
	"CH0", "CH1", "OPEN",
};

static SOC_ENUM_SINGLE_DECL(mt8167_codec_left_pga_mux_enum,
	AUDIO_CODEC_CON00, 28, pga_mux_text);

static SOC_ENUM_SINGLE_DECL(mt8167_codec_right_pga_mux_enum,
	AUDIO_CODEC_CON00, 10, pga_mux_text);

static const struct snd_kcontrol_new mt8167_codec_left_pga_mux =
	SOC_DAPM_ENUM("Left PGA Mux", mt8167_codec_left_pga_mux_enum);

static const struct snd_kcontrol_new mt8167_codec_right_pga_mux =
	SOC_DAPM_ENUM("Right PGA Mux", mt8167_codec_right_pga_mux_enum);

/* AIF TX Mux */
static const char * const aif_tx_mux_text[] = {
	"Analog MIC", "Digital MIC", "Aif Rx"
};

static SOC_ENUM_SINGLE_DECL(mt8167_codec_aif_tx_mux_enum,
	SND_SOC_NOPM, 0, aif_tx_mux_text);

static const struct snd_kcontrol_new mt8167_codec_aif_tx_mux =
	SOC_DAPM_ENUM("AIF TX Mux", mt8167_codec_aif_tx_mux_enum);

/* HPOUT Mux */
static const char * const hp_out_mux_text[] = {
	"OPEN", "AUDIO_AMP",
};

static SOC_ENUM_SINGLE_DECL(mt8167_codec_hp_out_mux_enum,
	SND_SOC_NOPM, 0, hp_out_mux_text);

static const struct snd_kcontrol_new mt8167_codec_hp_out_mux =
	SOC_DAPM_ENUM("HPOUT Mux",
			mt8167_codec_hp_out_mux_enum);

/* LINEOUT Mux  */
static const char * const line_out_mux_text[] = {
	"OPEN", "VOICE_AMP",
};

static SOC_ENUM_SINGLE_DECL(mt8167_codec_line_out_mux_enum,
	SND_SOC_NOPM, 0, line_out_mux_text);

static const struct snd_kcontrol_new mt8167_codec_line_out_mux =
	SOC_DAPM_ENUM("LINEOUT Mux",
		mt8167_codec_line_out_mux_enum);

/* AIF DL_UL loopback Switch */
static const struct snd_kcontrol_new mt8167_codec_aif_dl_ul_lpbk_ctrl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

/* DMIC Data Gen Switch */
static const struct snd_kcontrol_new mt8167_codec_dmic_data_gen_ctrl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

/* AMIC Data Gen Switch */
static const struct snd_kcontrol_new mt8167_codec_amic_data_gen_ctrl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

/* SDM Tone Gen Switch */
static const struct snd_kcontrol_new mt8167_codec_sdm_tone_gen_ctrl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_soc_dapm_widget mt8167_codec_dapm_widgets[] = {
	/* stream domain */
	SND_SOC_DAPM_AIF_OUT("AIF TX", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF RX", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("Left ADC", NULL, AUDIO_CODEC_CON00, 23, 0),
	SND_SOC_DAPM_ADC("Right ADC", NULL, AUDIO_CODEC_CON00, 5, 0),
	SND_SOC_DAPM_DAC("Left DAC", NULL, AUDIO_CODEC_CON01, 15, 0),
	SND_SOC_DAPM_DAC("Right DAC", NULL, AUDIO_CODEC_CON01, 14, 0),

	/* path domain */
	SND_SOC_DAPM_MUX("Left PGA Mux", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_left_pga_mux),
	SND_SOC_DAPM_MUX("Right PGA Mux", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_right_pga_mux),
	SND_SOC_DAPM_MUX("AIF TX Mux", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_aif_tx_mux),
	SND_SOC_DAPM_PGA("Left PGA", AUDIO_CODEC_CON00, 24, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right PGA", AUDIO_CODEC_CON00, 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Left DMIC", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Right DMIC", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DMIC", 2, SND_SOC_NOPM, 0, 0,
			mt8167_codec_dmic_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("Left Audio Amp", 1,
			AUDIO_CODEC_CON01, 12, 0,
			mt8167_codec_left_audio_amp_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Right Audio Amp", 1,
			AUDIO_CODEC_CON01, 11, 0,
			mt8167_codec_right_audio_amp_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HP Depop VCM", 2, SND_SOC_NOPM, 0, 0,
		mt8167_codec_depop_vcm_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX("HPOUT Mux", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_hp_out_mux),
	SND_SOC_DAPM_PGA_E("Voice Amp", AUDIO_CODEC_CON02, 8, 0, NULL, 0,
			mt8167_codec_voice_amp_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX("LINEOUT Mux", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_line_out_mux),
	SND_SOC_DAPM_SWITCH_E("AIF DL_UL loopback", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_aif_dl_ul_lpbk_ctrl,
			mt8167_codec_aif_dl_ul_lpbk_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH_E("DMIC Data Gen", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_dmic_data_gen_ctrl,
			mt8167_codec_dmic_data_gen_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH_E("AMIC Data Gen", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_amic_data_gen_ctrl,
			mt8167_codec_amic_data_gen_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH_E("SDM Tone Gen", SND_SOC_NOPM, 0, 0,
			&mt8167_codec_sdm_tone_gen_ctrl,
			mt8167_codec_sdm_tone_gen_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	/* generic widgets */
	SND_SOC_DAPM_SUPPLY_S("CODEC_CLK", 1, SND_SOC_NOPM, 0, 0,
			mt8167_codec_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("UL_CLK", 2, AUDIO_CODEC_CON03, 21, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DL_CLK", 2, AUDIO_CODEC_CON04, 15, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("Vcm14", 3, SND_SOC_NOPM, 0, 0,
			mt8167_codec_vcm14_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("UL_Vref24", 4, SND_SOC_NOPM, 0, 0,
			mt8167_codec_ul_vref24_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DL_Vref24", 4,
			AUDIO_CODEC_CON02, 16, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC_CLK", 4, AUDIO_CODEC_CON02, 27, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DL_VCM1", 4, AUDIO_CODEC_CON01, 13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DL_VCM2", 4, AUDIO_CODEC_CON02, 17, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AU_MICBIAS0", 5,
			AUDIO_CODEC_CON03, 17, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AU_MICBIAS1", 5,
			AUDIO_CODEC_CON01, 21, 0, NULL, 0),

	/* platform domain */
	SND_SOC_DAPM_INPUT("AU_VIN0"),
	SND_SOC_DAPM_INPUT("AU_VIN1"),
	SND_SOC_DAPM_INPUT("AU_VIN2"),

	SND_SOC_DAPM_OUTPUT("AU_HPL"),
	SND_SOC_DAPM_OUTPUT("AU_HPR"),

	SND_SOC_DAPM_OUTPUT("AU_LOL"),

	SND_SOC_DAPM_SIGGEN("SDM Tone"),
	SND_SOC_DAPM_SIGGEN("DMIC Data"),
	SND_SOC_DAPM_SIGGEN("AMIC Data"),
};

static const struct snd_soc_dapm_route mt8167_codec_dapm_routes[] = {
	/* Power */
	{"AIF TX", NULL, "CODEC_CLK"},
	{"AIF RX", NULL, "CODEC_CLK"},

	{"AIF TX", NULL, "UL_CLK"},
	{"AIF RX", NULL, "DL_CLK"},

	/* DL to UL loopback */
	{"AIF DL_UL loopback", "Switch", "AIF RX"},
	{"AIF TX Mux", "Aif Rx", "AIF DL_UL loopback"},

	/* UL */
	{"AIF TX", NULL, "AIF TX Mux"},

	{"AU_VIN0", NULL, "AU_MICBIAS0"},
	{"AU_VIN2", NULL, "AU_MICBIAS0"},

	{"AU_VIN1", NULL, "AU_MICBIAS1"},

	/* UL - Analog MIC Path */
	{"AIF TX Mux", "Analog MIC", "Left ADC"},
	{"AIF TX Mux", "Analog MIC", "Right ADC"},

	{"Left ADC", NULL, "Left PGA"},
	{"Right ADC", NULL, "Right PGA"},

	{"Left PGA", NULL, "Left PGA Mux"},
	{"Left PGA", NULL, "Left PGA Mux"},

	{"Right PGA", NULL, "Right PGA Mux"},
	{"Right PGA", NULL, "Right PGA Mux"},

	{"Left PGA Mux", "CH0", "AU_VIN0"},
	{"Left PGA Mux", "CH1", "AU_VIN1"},

	{"Right PGA Mux", "CH1", "AU_VIN1"},
	{"Right PGA Mux", "CH0", "AU_VIN2"},

	{"Left PGA", NULL, "Vcm14"},
	{"Right PGA", NULL, "Vcm14"},

	{"Left ADC", NULL, "UL_Vref24"},
	{"Right ADC", NULL, "UL_Vref24"},

	/* UL - Digital MIC Path */
	{"AIF TX Mux", "Digital MIC", "DMIC"},

	{"DMIC", NULL, "Left DMIC"},
	{"DMIC", NULL, "Right DMIC"},

	{"Left DMIC", NULL, "AU_VIN0"},
	{"Right DMIC", NULL, "AU_VIN2"},

	{"Left DMIC", NULL, "Vcm14"},
	{"Right DMIC", NULL, "Vcm14"},

	/* UL - Debug Path (DMIC Data Generator) */
	{"DMIC Data Gen", "Switch", "DMIC Data"},
	{"AIF TX Mux", "Digital MIC", "DMIC Data Gen"},

	/* UL - Debug Path (AMIC Data Generator) */
	{"AMIC Data Gen", "Switch", "AMIC Data"},
	{"AIF TX Mux", "Analog MIC", "AMIC Data Gen"},

	/* DL */
	{"AIF RX", NULL, "Vcm14"},

	{"Left DAC", NULL, "AIF RX"},
	{"Right DAC", NULL, "AIF RX"},

	{"Left DAC", NULL, "DL_Vref24"},
	{"Right DAC", NULL, "DL_Vref24"},

	{"Left DAC", NULL, "DAC_CLK"},
	{"Right DAC", NULL, "DAC_CLK"},

	{"Left DAC", NULL, "DL_VCM1"},
	{"Right DAC", NULL, "DL_VCM1"},

	/* DL - Audio Amp Path */
	{"Left Audio Amp", NULL, "Left DAC"},
	{"Right Audio Amp", NULL, "Right DAC"},

	{"Left Audio Amp", NULL, "DL_VCM2"},
	{"Right Audio Amp", NULL, "DL_VCM2"},

	{"HP Depop VCM", NULL, "Left Audio Amp"},
	{"HP Depop VCM", NULL, "Right Audio Amp"},

	{"HPOUT Mux", "AUDIO_AMP", "HP Depop VCM"},

	{"AU_HPL", NULL, "HPOUT Mux"},
	{"AU_HPR", NULL, "HPOUT Mux"},

	/* DL - Voice Amp Path */
	{"Voice Amp", NULL, "Left DAC"},

	{"Voice Amp", NULL, "DL_VCM2"},
	{"Voice Amp", NULL, "DL_VCM2"},

	{"LINEOUT Mux", "VOICE_AMP", "Voice Amp"},

	{"AU_LOL", NULL, "LINEOUT Mux"},

	/* DL - Debug Path (Triangular Tone Generator) */
	{"SDM Tone Gen", "Switch", "SDM Tone"},
	{"AIF RX", NULL, "SDM Tone Gen"},
};

static int afe_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct mt8167_codec_priv *codec_data =
			(struct mt8167_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap_modules[REGMAP_AFE]))
		return -1;

	dev_dbg(codec_data->codec->dev, "%s reg 0x%x\n",
		__func__, reg);

	ret = regmap_read(codec_data->regmap_modules[REGMAP_AFE],
			(reg & (~AFE_OFFSET)), val);
	return ret;
}

static int afe_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct mt8167_codec_priv *codec_data =
			(struct mt8167_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap_modules[REGMAP_AFE]))
		return -1;

	dev_dbg(codec_data->codec->dev, "%s reg 0x%x, val 0x%x\n",
		__func__, reg, val);

	ret = regmap_write(codec_data->regmap_modules[REGMAP_AFE],
			(reg & (~AFE_OFFSET)), val);
	return ret;
}

static bool reg_is_in_afe(unsigned int reg)
{
	if (reg & AFE_OFFSET)
		return true;
	else
		return false;
}

static int apmixedsys_reg_read(void *context,
			unsigned int reg, unsigned int *val)
{
	struct mt8167_codec_priv *codec_data =
			(struct mt8167_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap_modules[REGMAP_APMIXEDSYS]))
		return -1;
	ret = regmap_read(codec_data->regmap_modules[REGMAP_APMIXEDSYS],
			(reg & (~APMIXED_OFFSET)), val);
	return ret;
}

static int apmixedsys_reg_write(void *context,
			unsigned int reg, unsigned int val)
{
	struct mt8167_codec_priv *codec_data =
			(struct mt8167_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap_modules[REGMAP_APMIXEDSYS]))
		return -1;
	ret = regmap_write(codec_data->regmap_modules[REGMAP_APMIXEDSYS],
			(reg & (~APMIXED_OFFSET)), val);
	return ret;
}

static bool reg_is_in_apmixedsys(unsigned int reg)
{
	if (reg & APMIXED_OFFSET)
		return true;
	else
		return false;
}

#ifdef CONFIG_MTK_SPEAKER
static int pwrap_reg_read(void *context,
			unsigned int reg, unsigned int *val)
{
	struct mt8167_codec_priv *codec_data =
			(struct mt8167_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap_modules[REGMAP_PWRAP]))
		return -1;

	dev_dbg(codec_data->codec->dev, "%s reg 0x%x\n",
		__func__, reg);

	ret = regmap_read(codec_data->regmap_modules[REGMAP_PWRAP],
			(reg & (~PMIC_OFFSET)), val);
	return ret;
}

static int pwrap_reg_write(void *context,
			unsigned int reg, unsigned int val)
{
	struct mt8167_codec_priv *codec_data =
			(struct mt8167_codec_priv *) context;
	int ret = 0;

	if (!(codec_data && codec_data->regmap_modules[REGMAP_PWRAP]))
		return -1;

	dev_dbg(codec_data->codec->dev, "%s reg 0x%x, val 0x%x\n",
		__func__, reg, val);

	ret = regmap_write(codec_data->regmap_modules[REGMAP_PWRAP],
			(reg & (~PMIC_OFFSET)), val);
	return ret;
}

static bool reg_is_in_pmic(unsigned int reg)
{
	if (reg & PMIC_OFFSET)
		return true;
	else
		return false;
}
#endif

/* regmap functions */
static int codec_reg_read(void *context,
		unsigned int reg, unsigned int *val)
{
	int ret = 0;

	if (reg_is_in_afe(reg))
		ret = afe_reg_read(context, reg, val);
	else if (reg_is_in_apmixedsys(reg))
		ret = apmixedsys_reg_read(context, reg, val);
#ifdef CONFIG_MTK_SPEAKER
	else if (reg_is_in_pmic(reg))
		ret = pwrap_reg_read(context, reg, val);
#endif
	else
		ret = -1;
	return ret;
}

static int codec_reg_write(void *context,
			unsigned int reg, unsigned int val)
{
	int ret = 0;

	if (reg_is_in_afe(reg))
		ret = afe_reg_write(context, reg, val);
	else if (reg_is_in_apmixedsys(reg))
		ret = apmixedsys_reg_write(context, reg, val);
#ifdef CONFIG_MTK_SPEAKER
	else if (reg_is_in_pmic(reg))
		ret = pwrap_reg_write(context, reg, val);
#endif
	else
		ret = -1;
	return ret;
}

static void codec_regmap_lock(void *lock_arg)
{
}

static void codec_regmap_unlock(void *lock_arg)
{
}

static struct regmap_config mt8167_codec_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_read = codec_reg_read,
	.reg_write = codec_reg_write,
	.lock = codec_regmap_lock,
	.unlock = codec_regmap_unlock,
	.cache_type = REGCACHE_NONE,
};

#ifdef CONFIG_DEBUG_FS
struct mt8167_codec_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct mt8167_codec_reg_attr mt8167_codec_dump_reg_list[] = {
	/* audio_top_sys */
	DUMP_REG_ENTRY(ABB_AFE_CON0),
	DUMP_REG_ENTRY(ABB_AFE_CON1),
	DUMP_REG_ENTRY(ABB_AFE_CON2),
	DUMP_REG_ENTRY(ABB_AFE_CON3),
	DUMP_REG_ENTRY(ABB_AFE_CON4),
	DUMP_REG_ENTRY(ABB_AFE_CON5),
	DUMP_REG_ENTRY(ABB_AFE_CON6),
	DUMP_REG_ENTRY(ABB_AFE_CON7),
	DUMP_REG_ENTRY(ABB_AFE_CON8),
	DUMP_REG_ENTRY(ABB_AFE_CON9),
	DUMP_REG_ENTRY(ABB_AFE_CON10),
	DUMP_REG_ENTRY(ABB_AFE_CON11),
	DUMP_REG_ENTRY(ABB_AFE_STA0),
	DUMP_REG_ENTRY(ABB_AFE_STA1),
	DUMP_REG_ENTRY(ABB_AFE_STA2),
	DUMP_REG_ENTRY(AFE_MON_DEBUG0),
	DUMP_REG_ENTRY(AFE_MON_DEBUG1),
	DUMP_REG_ENTRY(ABB_AFE_SDM_TEST),

	/* apmixedsys */
	DUMP_REG_ENTRY(AUDIO_CODEC_CON00),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON01),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON02),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON03),
	DUMP_REG_ENTRY(AUDIO_CODEC_CON04),
};

static ssize_t mt8167_codec_debug_read(struct file *file,
			char __user *user_buf,
			size_t count, loff_t *pos)
{
	struct mt8167_codec_priv *codec_data = file->private_data;
	ssize_t ret, i;
	char *buf;
	int n = 0;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(mt8167_codec_dump_reg_list); i++) {
		n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
			mt8167_codec_dump_reg_list[i].name,
			snd_soc_read(codec_data->codec,
				mt8167_codec_dump_reg_list[i].offset));
	}

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}

static const struct file_operations mt8167_codec_debug_ops = {
	.open = simple_open,
	.read = mt8167_codec_debug_read,
	.llseek = default_llseek,
};
#endif

static void mt8167_codec_init_regs(struct mt8167_codec_priv *codec_data)
{
	dev_dbg(codec_data->codec->dev, "%s\n", __func__);

	/* disable chopper of uplink */
	snd_soc_update_bits(codec_data->codec,
		AUDIO_CODEC_CON00, BIT(17), 0x0);
	snd_soc_update_bits(codec_data->codec,
		AUDIO_CODEC_CON01, BIT(31), 0x0);

	/* Audio buffer quasi-current  */
	snd_soc_update_bits(codec_data->codec,
			AUDIO_CODEC_CON02, GENMASK(31, 30), 0x0);

	/* setup default gain */
	/* +4dB for voice buf gain */
	codec_data->pga_gain[LOUT_PGA_GAIN] = 0xB;
	snd_soc_update_bits(codec_data->codec,
		AUDIO_CODEC_CON02, GENMASK(12, 9),
		(codec_data->pga_gain[LOUT_PGA_GAIN]) << 9);

	mt8167_codec_hp_depop_setup(codec_data);
}

static struct regmap *mt8167_codec_get_regmap_from_dt(const char *phandle_name,
		struct mt8167_codec_priv *codec_data)
{
	struct device_node *self_node = NULL, *node = NULL;
	struct platform_device *platdev = NULL;
	struct device *dev = codec_data->codec->dev;
	struct regmap *regmap = NULL;

	self_node = of_find_compatible_node(NULL, NULL,
		"mediatek," MT8167_CODEC_NAME);
	if (!self_node) {
		dev_err(dev, "%s failed to find %s node\n",
			__func__, MT8167_CODEC_NAME);
		return NULL;
	}
	dev_dbg(dev, "%s found %s node\n", __func__, MT8167_CODEC_NAME);

	node = of_parse_phandle(self_node, phandle_name, 0);
	if (!node) {
		dev_err(dev, "%s failed to find %s node\n",
			__func__, phandle_name);
		return NULL;
	}
	dev_dbg(dev, "%s found %s\n", __func__, phandle_name);

	platdev = of_find_device_by_node(node);
	if (!platdev) {
		dev_err(dev, "%s failed to get platform device of %s\n",
			__func__, phandle_name);
		return NULL;
	}
	dev_dbg(dev, "%s found platform device of %s\n",
		__func__, phandle_name);

	regmap = dev_get_regmap(&platdev->dev, NULL);
	if (regmap) {
		dev_dbg(dev, "%s found regmap of %s\n", __func__, phandle_name);
		return regmap;
	}

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, phandle_name);
	if (!IS_ERR(regmap)) {
		dev_dbg(dev, "%s found regmap of syscon node %s\n",
			__func__, phandle_name);
		return regmap;
	}
	dev_err(dev, "%s failed to get regmap of syscon node %s\n",
		__func__, phandle_name);

	return NULL;
}

static const char * const modules_dt_regmap_str[REGMAP_NUMS] = {
	"mediatek,afe-regmap",
	"mediatek,apmixedsys-regmap",
#ifdef CONFIG_MTK_SPEAKER
	"mediatek,pwrap-regmap",
#endif
};

static int mt8167_codec_parse_dt(struct mt8167_codec_priv *codec_data)
{
	struct device *dev = codec_data->codec->dev;
	int ret = 0;
	int i;

	for (i = 0 ; i < REGMAP_NUMS ; i++) {
		codec_data->regmap_modules[i] = mt8167_codec_get_regmap_from_dt(
				modules_dt_regmap_str[i],
				codec_data);
		if (!codec_data->regmap_modules[i]) {
			dev_err(dev, "%s failed to get %s\n",
				__func__, modules_dt_regmap_str[i]);
			devm_kfree(dev, codec_data);
			ret = -EPROBE_DEFER;
			break;
		}
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,dmic-wire-mode",
				&codec_data->dmic_wire_mode);
	if (ret) {
		dev_warn(dev, "%s fail to read dmic-wire-mode in node %s\n",
			__func__, dev->of_node->full_name);
		codec_data->dmic_wire_mode = DMIC_ONE_WIRE;
	} else if ((codec_data->dmic_wire_mode != DMIC_ONE_WIRE) &&
		codec_data->dmic_wire_mode != DMIC_TWO_WIRE) {
		codec_data->dmic_wire_mode = DMIC_ONE_WIRE;
	}

	if (of_property_read_u32_array(dev->of_node, "mediatek,dmic-ch-phase",
		codec_data->dmic_ch_phase, ARRAY_SIZE(codec_data->dmic_ch_phase))) {
		for (i = 0; i < ARRAY_SIZE(codec_data->dmic_ch_phase); i++)
			codec_data->dmic_ch_phase[i] = 0;
	}
	for (i = 0; i < ARRAY_SIZE(codec_data->dmic_ch_phase); i++) {
		if (codec_data->dmic_ch_phase[i] >= DMIC_PHASE_NUM)
			codec_data->dmic_ch_phase[i] = 0;
	}

	if (of_property_read_u32(dev->of_node, "mediatek,dmic-rate-mode",
				&codec_data->dmic_rate_mode))
		codec_data->dmic_rate_mode = DMIC_RATE_D1P625M;
	else if ((codec_data->dmic_rate_mode != DMIC_RATE_D1P625M) &&
		(codec_data->dmic_rate_mode != DMIC_RATE_D3P25M))
		codec_data->dmic_rate_mode = DMIC_RATE_D1P625M;
	return ret;
}

static int mt8167_codec_probe(struct snd_soc_codec *codec)
{
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	codec_data->codec = codec;

	ret = mt8167_codec_parse_dt(codec_data);
	if (ret < 0)
		return ret;

	codec_data->clk = devm_clk_get(codec->dev, "bus");
	if (IS_ERR(codec_data->clk)) {
		dev_err(codec->dev, "%s devm_clk_get %s fail\n",
			__func__, "bus");
		return PTR_ERR(codec_data->clk);
	}

	ret = clk_prepare_enable(codec_data->clk);
	if (ret)
		return ret;

	mt8167_codec_init_regs(codec_data);
#ifdef CONFIG_DEBUG_FS
	codec_data->debugfs = debugfs_create_file("mt8167_codec_regs",
			S_IFREG | S_IRUGO,
			NULL, codec_data, &mt8167_codec_debug_ops);
#endif
#ifdef CONFIG_MTK_SPEAKER
	ret = mt6392_codec_probe(codec);
	if (ret < 0)
		clk_disable_unprepare(codec_data->clk);
#endif
	return ret;
}

static int mt8167_codec_remove(struct snd_soc_codec *codec)
{
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	clk_disable_unprepare(codec_data->clk);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(codec_data->debugfs);
#endif
#ifdef CONFIG_MTK_SPEAKER
	mt6392_codec_remove(codec);
#endif
	return 0;
}

static int mt8167_codec_suspend(struct snd_soc_codec *codec)
{
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	clk_disable_unprepare(codec_data->clk);
	return 0;
}

static int mt8167_codec_resume(struct snd_soc_codec *codec)
{
	struct mt8167_codec_priv *codec_data = snd_soc_codec_get_drvdata(codec);

	return clk_prepare_enable(codec_data->clk);
}

static int mt8167_codec_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	dev_dbg(codec->dev, "%s curr bias_level %d, set bias_level: %d\n",
		__func__, snd_soc_codec_get_bias_level(codec), level);

	switch (snd_soc_codec_get_bias_level(codec)) {
	case SND_SOC_BIAS_OFF:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_ON:
		break;
	default:
		break;
	}

	return 0;
}

static struct snd_soc_codec_driver mt8167_codec_driver = {
	.probe = mt8167_codec_probe,
	.remove = mt8167_codec_remove,
	.suspend = mt8167_codec_suspend,
	.resume = mt8167_codec_resume,
	.set_bias_level = mt8167_codec_set_bias_level,
	.controls = mt8167_codec_controls,
	.num_controls = ARRAY_SIZE(mt8167_codec_controls),
	.dapm_widgets = mt8167_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8167_codec_dapm_widgets),
	.dapm_routes = mt8167_codec_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8167_codec_dapm_routes),
};

static int mt8167_codec_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt8167_codec_priv *codec_data = NULL;

	dev_dbg(dev, "%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT8167_CODEC_NAME);
		dev_dbg(dev, "%s set dev name %s\n", __func__, dev_name(dev));
	}

	codec_data = devm_kzalloc(dev,
			sizeof(struct mt8167_codec_priv), GFP_KERNEL);
	if (!codec_data)
		return -ENOMEM;

	dev_set_drvdata(dev, codec_data);

	/* get regmap of codec */
	codec_data->regmap = devm_regmap_init(dev, NULL, codec_data,
		&mt8167_codec_regmap_config);
	if (IS_ERR(codec_data->regmap)) {
		dev_err(dev, "%s failed to get regmap of codec\n", __func__);
		devm_kfree(dev, codec_data);
		codec_data->regmap = NULL;
		return -EINVAL;
	}

	return snd_soc_register_codec(dev,
			&mt8167_codec_driver, &mt8167_codec_dai, 1);
}

static int mt8167_codec_dev_remove(struct platform_device *pdev)
{
	struct mt8167_codec_priv *codec_data = dev_get_drvdata(&pdev->dev);

	mt8167_codec_hp_depop_cleanup(codec_data);

	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id mt8167_codec_dt_match[] = {
	{.compatible = "mediatek," MT8167_CODEC_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8167_codec_dt_match);

static struct platform_driver mt8167_codec_device_driver = {
	.driver = {
		   .name = MT8167_CODEC_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt8167_codec_dt_match,
		   },
	.probe = mt8167_codec_dev_probe,
	.remove = mt8167_codec_dev_remove,
};

module_platform_driver(mt8167_codec_device_driver);

/* Module information */
MODULE_DESCRIPTION("ASoC MT8167 driver");
MODULE_LICENSE("GPL v2");
