/*
 * mt8167s-som.c  --  MT8167S-SOM ALSA SoC machine driver
 *
 * Copyright (c) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <sound/soc.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <sound/pcm_params.h>
#include "../../codecs/tlv320wn.h"


#define ENUM_TO_STR(enum) #enum

#define  TLV320_MCLK_SOURCE 0
#define  TLV320_BCLK_SOURCE 1


enum PINCTRL_PIN_STATE {
	PIN_STATE_DEFAULT = 0,
	PIN_STATE_MAX
};

enum mtkfile_pcm_state {
	MTKFILE_PCM_STATE_UNKNOWN = 0,
	MTKFILE_PCM_STATE_OPEN,
	MTKFILE_PCM_STATE_HW_PARAMS,
	MTKFILE_PCM_STATE_PREPARE,
	MTKFILE_PCM_STATE_START,
	MTKFILE_PCM_STATE_PAUSE,
	MTKFILE_PCM_STATE_RESUME,
	MTKFILE_PCM_STATE_DRAIN,
	MTKFILE_PCM_STATE_STOP,
	MTKFILE_PCM_STATE_HW_FREE,
	MTKFILE_PCM_STATE_CLOSE,
	MTKFILE_PCM_STATE_NUM,
};

static const char *const pcm_state_func[] = {
	ENUM_TO_STR(MTKFILE_PCM_STATE_UNKNOWN),
	ENUM_TO_STR(MTKFILE_PCM_STATE_OPEN),
	ENUM_TO_STR(MTKFILE_PCM_STATE_HW_PARAMS),
	ENUM_TO_STR(MTKFILE_PCM_STATE_PREPARE),
	ENUM_TO_STR(MTKFILE_PCM_STATE_START),
	ENUM_TO_STR(MTKFILE_PCM_STATE_PAUSE),
	ENUM_TO_STR(MTKFILE_PCM_STATE_RESUME),
	ENUM_TO_STR(MTKFILE_PCM_STATE_DRAIN),
	ENUM_TO_STR(MTKFILE_PCM_STATE_STOP),
	ENUM_TO_STR(MTKFILE_PCM_STATE_HW_FREE),
	ENUM_TO_STR(MTKFILE_PCM_STATE_CLOSE),
};

static const char * const nfy_ctl_names[] = {
	"Master Volume",
	"Master Volume X",
	"Master Switch",
	"Master Switch X",
	"PCM State",
	"PCM State X",
};

enum {
	MASTER_VOLUME_ID = 0,
	MASTER_VOLUMEX_ID,
	MASTER_SWITCH_ID,
	MASTER_SWITCHX_ID,
	PCM_STATE_ID,
	PCM_STATEX_ID,
	CTRL_NOTIFY_NUM,
	CTRL_NOTIFY_INVAL = 0xFFFF,
};
struct soc_ctlx_res {
	int master_volume;
	int master_switch;
	int pcm_state;
	struct snd_ctl_elem_id nfy_ids[CTRL_NOTIFY_NUM];
	struct mutex res_mutex;
	spinlock_t res_lock;
};

struct mt8167s_som_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
	//int tdmadc_rst_gpio;
	//struct regulator *extamp_supply;
	//struct regulator *tdmadc_supply;
	//struct regulator *amp_rst_supply;
	struct regulator *tdmadc_1p8_supply;
	struct regulator *tdmadc_3p3_supply;
	//struct regulator *tdmadc_micbias_supply;
	//struct regulator *adc_1p8_supply;
	//struct regulator *adc_3p3_supply;
	struct soc_ctlx_res ctlx_res;
};

static const char * const mt8167s_som_pinctrl_pin_str[PIN_STATE_MAX] = {
	"default",

};

static SOC_ENUM_SINGLE_EXT_DECL(pcm_state_enums, pcm_state_func);

/* ctrl resource manager */
static inline int soc_ctlx_init(struct soc_ctlx_res *ctlx_res, struct snd_soc_card *soc_card)
{
	int i;
	struct snd_card *card = soc_card->snd_card;
	struct snd_kcontrol *control;

	ctlx_res->master_volume = 100;
	ctlx_res->master_switch = 1;
	ctlx_res->pcm_state = MTKFILE_PCM_STATE_UNKNOWN;
	mutex_init(&ctlx_res->res_mutex);
	spin_lock_init(&ctlx_res->res_lock);

	for (i = 0; i < CTRL_NOTIFY_NUM; i++) {
		list_for_each_entry(control, &card->controls, list) {
			if (strncmp(control->id.name, nfy_ctl_names[i], sizeof(control->id.name)))
				continue;
			ctlx_res->nfy_ids[i] = control->id;
		}
	}

	return 0;
}

static int soc_ctlx_get(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8167s_som_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	int type;

	for (type = 0; type < CTRL_NOTIFY_NUM; type++) {
		if (kctl->id.numid == res_mgr->nfy_ids[type].numid)
			break;
	}
	if (type == CTRL_NOTIFY_NUM) {
		pr_err("invalid mixer control(numid:%d)\n", kctl->id.numid);
		return -EINVAL;
	}

	mutex_lock(&res_mgr->res_mutex);
	switch (type) {
	case MASTER_VOLUME_ID:
	case MASTER_VOLUMEX_ID:
		ucontrol->value.integer.value[0] = res_mgr->master_volume;
		break;
	case MASTER_SWITCH_ID:
	case MASTER_SWITCHX_ID:
		ucontrol->value.integer.value[0] = res_mgr->master_switch;
		break;
	default:
		break;
	}
	mutex_unlock(&res_mgr->res_mutex);
	pr_notice("get mixer control(%s) value is:%ld\n", kctl->id.name, ucontrol->value.integer.value[0]);

	return 0;
}

static int soc_ctlx_put(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8167s_som_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	int type;
	int nfy_type;
	int need_notify_self = 0;
	int *value = NULL;

	for (type = 0; type < CTRL_NOTIFY_NUM; type++) {
		if (kctl->id.numid == res_mgr->nfy_ids[type].numid)
			break;
	}
	if (type == CTRL_NOTIFY_NUM) {
		pr_err("invalid mixer control(numid:%d)\n", kctl->id.numid);
		return -EINVAL;
	}

	mutex_lock(&res_mgr->res_mutex);
	switch (type) {
	case MASTER_VOLUME_ID:
		if ((res_mgr->master_switch == 1) ||
			(ucontrol->value.integer.value[0] != 0)) {
			nfy_type = MASTER_VOLUMEX_ID;
			value = &res_mgr->master_volume;
			need_notify_self = 1;
		}
		break;
	case MASTER_VOLUMEX_ID:
		nfy_type = MASTER_VOLUME_ID;
		value = &res_mgr->master_volume;
		break;
	case MASTER_SWITCH_ID:
		nfy_type = MASTER_SWITCHX_ID;
		value = &res_mgr->master_switch;
		need_notify_self = 1;
		break;
	case MASTER_SWITCHX_ID:
		nfy_type = MASTER_SWITCH_ID;
		value = &res_mgr->master_switch;
		break;
	default:
		break;
	}
	if (value != NULL) {
		*value = ucontrol->value.integer.value[0];
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE, &(res_mgr->nfy_ids[nfy_type]));
	} else {
		nfy_type = CTRL_NOTIFY_INVAL;
	}
	if (need_notify_self) {
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE, &(kctl->id));
	}
	mutex_unlock(&res_mgr->res_mutex);
	pr_notice("set mixer control(%s) value is:%ld, notify id:%x, notify self:%d\n",
						kctl->id.name,
						ucontrol->value.integer.value[0],
						nfy_type,
						need_notify_self);

	return 0;
}

static int soc_pcm_state_get(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8167s_som_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	unsigned long flags;

	spin_lock_irqsave(&res_mgr->res_lock, flags);
	ucontrol->value.integer.value[0] = res_mgr->pcm_state;
	spin_unlock_irqrestore(&res_mgr->res_lock, flags);
	pr_notice("get mixer control(%s) value is:%ld\n", kctl->id.name, ucontrol->value.integer.value[0]);

	return 0;
}

static int soc_pcm_state_put(struct snd_kcontrol *kctl,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kctl);
	struct mt8167s_som_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	unsigned long flags;

	spin_lock_irqsave(&res_mgr->res_lock, flags);
	if (ucontrol->value.integer.value[0] != res_mgr->pcm_state) {
		res_mgr->pcm_state = ucontrol->value.integer.value[0];
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE, &(res_mgr->nfy_ids[PCM_STATEX_ID]));
	}
	spin_unlock_irqrestore(&res_mgr->res_lock, flags);
	pr_notice("set mixer control(%s) value is:%ld\n",
						kctl->id.name,
						ucontrol->value.integer.value[0]);

	return 0;
}


static const struct snd_kcontrol_new mt8167s_som_soc_controls[] = {
	/* for third party app use */
	SOC_SINGLE_EXT("Master Volume",
			    0,
			    0,
			    100,
			    0,
			    soc_ctlx_get,
			    soc_ctlx_put),
	SOC_SINGLE_EXT("Master Volume X",
			    0,
			    0,
			    100,
			    0,
			    soc_ctlx_get,
			    soc_ctlx_put),
	SOC_SINGLE_BOOL_EXT("Master Switch",
			    0,
			    soc_ctlx_get,
			    soc_ctlx_put),
	SOC_SINGLE_BOOL_EXT("Master Switch X",
			    0,
			    soc_ctlx_get,
			    soc_ctlx_put),
	SOC_ENUM_EXT("PCM State",
		     pcm_state_enums,
		     soc_pcm_state_get,
		     soc_pcm_state_put),
	SOC_ENUM_EXT("PCM State X",
		     pcm_state_enums,
		     soc_pcm_state_get,
		     0),
};

static int i2s_8ch_playback_state_set(struct snd_pcm_substream *substream, int state)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mt8167s_som_priv *card_data = snd_soc_card_get_drvdata(card);
	struct soc_ctlx_res *res_mgr = &card_data->ctlx_res;
	int nfy_type;
	unsigned long flags;

	nfy_type = PCM_STATEX_ID;
	spin_lock_irqsave(&res_mgr->res_lock, flags);
	if (res_mgr->pcm_state != state) {
		res_mgr->pcm_state = state;
		snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_VALUE, &(res_mgr->nfy_ids[nfy_type]));
	} else {
		nfy_type = CTRL_NOTIFY_INVAL;
	}
	spin_unlock_irqrestore(&res_mgr->res_lock, flags);

	return 0;
}

static int i2s_8ch_playback_startup(struct snd_pcm_substream *substream)
{
	i2s_8ch_playback_state_set(substream, MTKFILE_PCM_STATE_OPEN);
	return 0;
}

static void i2s_8ch_playback_shutdown(struct snd_pcm_substream *substream)
{
	i2s_8ch_playback_state_set(substream, MTKFILE_PCM_STATE_CLOSE);
}

static int i2s_8ch_playback_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	i2s_8ch_playback_state_set(substream, MTKFILE_PCM_STATE_HW_PARAMS);
	return 0;
}

static int i2s_8ch_playback_hw_free(struct snd_pcm_substream *substream)
{
	i2s_8ch_playback_state_set(substream, MTKFILE_PCM_STATE_HW_FREE);
	return 0;
}

static int i2s_8ch_playback_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		i2s_8ch_playback_state_set(substream, MTKFILE_PCM_STATE_START);
		break;
	default:
		break;
	}

	return 0;
}

static struct snd_soc_ops i2s_8ch_playback_ops = {
	.startup = i2s_8ch_playback_startup,
	.shutdown = i2s_8ch_playback_shutdown,
	.hw_params = i2s_8ch_playback_hw_params,
	.hw_free = i2s_8ch_playback_hw_free,
	.trigger = i2s_8ch_playback_trigger,
};

static int tlv3101_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;


	printk("aic:%s\n", __func__);
	if (substream == NULL) {
		pr_err("invalid stream parameter\n");
		return -EINVAL;
	}

	rtd = substream->private_data;
	if (rtd == NULL) {
		pr_err("invalid runtime parameter\n");
		return -EINVAL;
	}

	codec_dai = rtd->codec_dai;
	if (codec_dai == NULL) {
		pr_err("invalid dai parameter\n");
		return -EINVAL;
	}


	snd_soc_dai_set_pll(codec_dai, 0,TLV320_MCLK_SOURCE,
			256 * params_rate(params), params_rate(params));

	return 0;
}

static struct snd_soc_ops tlv3101_machine_ops = {
	.hw_params = tlv3101_hw_params,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8167s_som_dais[] = {
	/* Front End DAI links */
	{
		.name = "I2S 8CH Playback",
		.stream_name = "I2S8CH Playback",
		.cpu_dai_name = "HDMI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		.ops = &i2s_8ch_playback_ops,
	},
	{
		.name = "TDM Capture",
		.stream_name = "TDM_Capture",
		.cpu_dai_name = "TDM_IN",
		.codec_name = "tlv320aic3101.1-001b", //i2c1 addr 0x1b
		.codec_dai_name = "tlv320aic3101-codec",
		.dai_fmt = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ops = &tlv3101_machine_ops,
	},
	{
		.name = "DMIC Capture",
		.stream_name = "DMIC_Capture",
		.cpu_dai_name = "VUL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "AWB Capture",
		.stream_name = "AWB_Record",
		.cpu_dai_name = "AWB",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
#ifdef CONFIG_MTK_BTCVSD_ALSA
	{
		.name = "BTCVSD_RX",
		.stream_name = "BTCVSD_Capture",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "mt-soc-btcvsd-rx-pcm",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "BTCVSD_TX",
		.stream_name = "BTCVSD_Playback",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "mt-soc-btcvsd-tx-pcm",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
	{
		.name = "DL1 Playback",
		.stream_name = "DL1_Playback",
		.cpu_dai_name = "DL1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Ref In Capture",
		.stream_name = "DL1_AWB_Record",
		.cpu_dai_name = "AWB",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "DAI Capture",
		.stream_name = "VOIP_Call_BT_Capture",
		.cpu_dai_name = "DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
	},

	/* Backend End DAI links */
	{
		.name = "HDMI BE",
		.cpu_dai_name = "HDMIO",
		.no_pcm = 1,
		.codec_name = "hdmi-audio-codec",
		.codec_dai_name = "i2s-hifi",
		//.codec_name = "tas5782m",
		//.codec_dai_name = "tas5782m-i2s",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
	},
	{
		.name = "2ND EXT Codec",
		.cpu_dai_name = "2ND I2S",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_capture = 1,
	},
	{
		.name = "MTK Codec",
		.cpu_dai_name = "INT ADDA",
		.no_pcm = 1,
		.codec_name = "mt8167-codec",
		.codec_dai_name = "mt8167-codec-dai",
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "DMIC BE",
		.cpu_dai_name = "INT ADDA",
		.no_pcm = 1,
		.codec_name = "mt8167-codec",
		.codec_dai_name = "mt8167-codec-dai",
		.dpcm_capture = 1,
	},
	{
		.name = "HW Gain1 BE",
		.cpu_dai_name = "HW_GAIN1",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_capture = 1,
	},
	{
		.name = "TDM IN BE",
		.cpu_dai_name = "TDM_IN_IO",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_capture = 1,
	},
	{
		.name = "I2S BE",
		.cpu_dai_name = "I2S",
		.no_pcm = 1,
		//.codec_name = "snd-soc-dummy",
		//.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "tas5782m",
		.codec_dai_name = "tas5782m-i2s",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,

	},
	{
		.name = "DL BE",
		.cpu_dai_name = "DL Input",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_capture = 1,
	},
	{
		.name = "MRG BT BE",
		.cpu_dai_name = "MRG BT",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

static const struct snd_soc_dapm_widget mt8167s_som_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("External Line In"),
	SND_SOC_DAPM_OUTPUT("External I2S out"),
	SND_SOC_DAPM_INPUT("External Line In2"),

};

static const struct snd_soc_dapm_route mt8167s_som_audio_map[] = {
	{"2ND I2S Capture", NULL, "External Line In"},
	{"I2S Capture", NULL, "External Line In2"},
	{"External I2S out", NULL, "I2S Playback"},

};

static int mt8167s_som_suspend_post(struct snd_soc_card *card)
{
	struct mt8167s_som_priv *card_data;

	card_data = snd_soc_card_get_drvdata(card);

	/* tdm adc power down */
	if (!IS_ERR(card_data->tdmadc_3p3_supply))
		regulator_disable(card_data->tdmadc_3p3_supply);
	if (!IS_ERR(card_data->tdmadc_1p8_supply))
		regulator_disable(card_data->tdmadc_1p8_supply);
	return 0;
}

static int mt8167s_som_resume_pre(struct snd_soc_card *card)
{
	struct mt8167s_som_priv *card_data;
	int ret;

	card_data = snd_soc_card_get_drvdata(card);

	/* tdm adc power down */
	if (!IS_ERR(card_data->tdmadc_3p3_supply)) {
		ret = regulator_enable(card_data->tdmadc_3p3_supply);
		if (ret != 0)
			dev_err(card->dev, "%s failed to enable tdm 3p3 supply %d!\n", __func__, ret);
	}
	if (!IS_ERR(card_data->tdmadc_1p8_supply)) {
		ret = regulator_enable(card_data->tdmadc_1p8_supply);
		if (ret != 0)
			dev_err(card->dev, "%s failed to enable tdm 1p8 supply %d!\n", __func__, ret);
	}
	return 0;
}
static struct snd_soc_card mt8167s_som_card = {
	.name = "mt-snd-card",
	.owner = THIS_MODULE,
	.dai_link = mt8167s_som_dais,
	.num_links = ARRAY_SIZE(mt8167s_som_dais),
	.controls = mt8167s_som_soc_controls,
	.num_controls = ARRAY_SIZE(mt8167s_som_soc_controls),
	.dapm_widgets = mt8167s_som_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8167s_som_dapm_widgets),
	.dapm_routes = mt8167s_som_audio_map,
	.num_dapm_routes = ARRAY_SIZE(mt8167s_som_audio_map),
	.suspend_post = mt8167s_som_suspend_post,
	.resume_pre = mt8167s_som_resume_pre,
};

static int mt8167s_som_gpio_probe(struct snd_soc_card *card)
{
	struct mt8167s_som_priv *card_data;
	int ret = 0;
	int i;
	printk("mt8167s_som_gpio_probe!!\r\n");

	card_data = snd_soc_card_get_drvdata(card);

	card_data->pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(card_data->pinctrl)) {
		ret = PTR_ERR(card_data->pinctrl);
		dev_err(card->dev, "%s pinctrl_get failed %d\n",
			__func__, ret);
		goto exit;
	}

	for (i = 0 ; i < PIN_STATE_MAX ; i++) {
		card_data->pin_states[i] =
			pinctrl_lookup_state(card_data->pinctrl,
				mt8167s_som_pinctrl_pin_str[i]);
		if (IS_ERR(card_data->pin_states[i])) {
			ret = PTR_ERR(card_data->pin_states[i]);
			dev_warn(card->dev, "%s Can't find pinctrl state %s %d\n",
				__func__, mt8167s_som_pinctrl_pin_str[i], ret);
		}
	}
	/* default state */
	if (!IS_ERR(card_data->pin_states[PIN_STATE_DEFAULT])) {
		ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_DEFAULT]);
		if (ret) {
			dev_err(card->dev, "%s failed to select state %d\n",
				__func__, ret);
			goto exit;
		}
	}
exit:
	return ret;
}

static int mt8167s_som_regulator_probe(struct snd_soc_card *card)
{
	struct mt8167s_som_priv *card_data;
	int isenable, vol, ret;

	printk("mt8167s_som_regulator_probe!!\r\n");

	card_data = snd_soc_card_get_drvdata(card);
	card_data->tdmadc_3p3_supply = devm_regulator_get(card->dev, "tdmadc-3p3v");
	if (IS_ERR(card_data->tdmadc_3p3_supply)) {
		ret = PTR_ERR(card_data->tdmadc_3p3_supply);
		dev_err(card->dev, "%s failed to get tdmadc-3p3v regulator %d\n", __func__, ret);
		return ret;
	}

	ret = regulator_set_voltage(card_data->tdmadc_3p3_supply, 3300000, 3300000);
	if (ret != 0) {
		dev_err(card->dev, "%s failed to set tdmadc-3p3v supply to 3.3v %d\n", __func__, ret);
		return ret;
	}

	ret = regulator_enable(card_data->tdmadc_3p3_supply);
	if (ret != 0) {
		dev_err(card->dev, "%s failed to enable tdmadc 3p3 supply %d!\n", __func__, ret);
		return ret;
	}

	isenable = regulator_is_enabled(card_data->tdmadc_3p3_supply);
	if (isenable != 1)
		dev_err(card->dev, "%s tdmadc 3.3V supply is not enabled\n", __func__);


	vol = regulator_get_voltage(card_data->tdmadc_3p3_supply);
	if (vol != 3300000)
		dev_err(card->dev, "%s tdmadc 3p3 supply != 3.3V (%d)\n", __func__, vol);


	card_data->tdmadc_1p8_supply = devm_regulator_get(card->dev, "tdmadc-1p8v");
	if (IS_ERR(card_data->tdmadc_1p8_supply)) {
		ret = PTR_ERR(card_data->tdmadc_1p8_supply);
		dev_err(card->dev, "%s failed to get tdmadc-1p8v regulator %d\n", __func__, ret);
		return ret;
	}

	ret = regulator_set_voltage(card_data->tdmadc_1p8_supply, 1800000, 1800000);
	if (ret != 0) {
		dev_err(card->dev, "%s failed to set tdmadc-1p8v supply to 1.8v %d\n", __func__, ret);
		return ret;
	}

	ret = regulator_enable(card_data->tdmadc_1p8_supply);
	if (ret != 0) {
		dev_err(card->dev, "%s failed to enable tdmadc 1p8 supply %d!\n", __func__, ret);
		return ret;
	}

	isenable = regulator_is_enabled(card_data->tdmadc_1p8_supply);
	if (isenable != 1)
		dev_err(card->dev, "%s tdmadc 1.8V supply is not enabled\n", __func__);


	vol = regulator_get_voltage(card_data->tdmadc_1p8_supply);
	if (vol != 1800000)
		dev_err(card->dev, "%s tdmadc 1p8 supply != 1.8V (%d)\n", __func__, vol);
	return 0;
}

static int mt8167s_som_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8167s_som_card;
	struct device_node *platform_node;
	struct device_node *codec_node;
	int ret, i;
	struct mt8167s_som_priv *card_data;

	printk("mt8167s_som_dev_probe!!\r\n");

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}
	codec_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev, "Property 'audio-codec' missing or invalid\n");
		return -EINVAL;
	}
	for (i = 0; i < card->num_links; i++) {
		if (mt8167s_som_dais[i].platform_name)
			continue;
		mt8167s_som_dais[i].platform_of_node = platform_node;
	}
	for (i = 0; i < card->num_links; i++) {
		if (mt8167s_som_dais[i].codec_name)
			continue;
		mt8167s_som_dais[i].codec_of_node = codec_node;
	}
	card->dev = &pdev->dev;

	card_data = devm_kzalloc(&pdev->dev,
		sizeof(struct mt8167s_som_priv), GFP_KERNEL);
	if (!card_data) {
		ret = -ENOMEM;
		dev_err(&pdev->dev,
			"%s allocate card private data fail %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_card_set_drvdata(card, card_data);

	mt8167s_som_regulator_probe(card);
	mt8167s_som_gpio_probe(card);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
		__func__, ret);
		return ret;
	}
	soc_ctlx_init(&card_data->ctlx_res, card);

	return ret;
}

static const struct of_device_id mt8167s_som_dt_match[] = {
	{ .compatible = "mediatek,mt8167s-som", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8167s_som_dt_match);

static struct platform_driver mt8167s_som_mach_driver = {
	.driver = {
		   .name = "mt8167s-som",
		   .of_match_table = mt8167s_som_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8167s_som_dev_probe,
};

module_platform_driver(mt8167s_som_mach_driver);

/* Module information */
MODULE_DESCRIPTION("MT8167SOM ALSA SoC machine driver");
MODULE_LICENSE("GPL v1");
MODULE_ALIAS("platform:mt8167s-som");

