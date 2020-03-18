/*
 * Driver for the TAS5782M Audio Amplifier
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include "tas5782m.h"

#define TAS5872M_DRV_NAME    "tas5782m"

#define TAS5872M_RATES	     (SNDRV_PCM_RATE_8000_96000)
#define TAS5782M_FORMATS     (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5782M_REG_00      (0x00)
#define TAS5782M_REG_02      (0x02)
#define TAS5782M_REG_03      (0x03)
#define TAS5782M_REG_09      (0x09)

#define TAS5782M_REG_7F      (0x7F)
#define TAS5782M_REG_14      (0x14)
#define TAS5782M_REG_15      (0x15)
#define TAS5782M_REG_16      (0x16)
#define TAS5782M_REG_17      (0x17)

#define TAS5782M_REG_2A      (0x2A)
#define TAS5782M_REG_28      (0x28)
#define TAS5782M_REG_3C      (0x3C)
#define TAS5782M_REG_3D      (0x3D)
#define TAS5782M_REG_3E      (0x3E)

#define TAS5782M_REG_44      (0x44)
#define TAS5782M_REG_45      (0x45)
#define TAS5782M_REG_46      (0x46)
#define TAS5782M_REG_47      (0x47)
#define TAS5782M_REG_48      (0x48)
#define TAS5782M_REG_49      (0x49)
#define TAS5782M_REG_4A      (0x4A)
#define TAS5782M_REG_4B      (0x4B)

#define TAS5782M_PAGE_00     (0x00)
#define TAS5782M_PAGE_8C     (0x8C)
#define TAS5782M_PAGE_1E     (0x1E)
#define TAS5782M_PAGE_23     (0x23)


#define TAS5782M_FMT_LJ	     (0x3 << 4)
#define TAS5782M_FMT_I2S     (0x0 << 4)

#define TAS5782M_FMT_NB_NF   (0x0 << 5)
#define TAS5782M_FMT_IB_IF   (0x1 << 5)

#define TAS5782M_FMT_32BIT    (0x3 << 0)
#define TAS5782M_FMT_24BIT    (0x2 << 0)
#define TAS5782M_FMT_16BIT    (0x0 << 0)



struct tas5782m_priv {
	struct regmap *regmap;
	unsigned int mode; /* The mode (I2S or left-justified) */
	unsigned int invert_mode; /* The mode (bck/lrck invert or not) */
	int reset_gpio;
	int mute_gpio;
	int volume_db;
};

const struct regmap_config tas5782m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};


static int tas5782m_vol_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5782m->volume_db;

	return 0;
}

/*0x30 0db,0x3c -6db*/
static int tas5782m_vol_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct tas5782m_priv *tas5782m = snd_soc_component_get_drvdata(component);
	unsigned short db;
        int ret,value;

	value = ucontrol->value.integer.value[0];
		if (value == 0)
			db = 0xFF;
		else
			db = 0x30 + (100 - value)*3/2;


	printk("db setting is :0x%x\n", db);

	ret = regmap_write(tas5782m->regmap, TAS5782M_REG_3C, 0x01);
	if (ret < 0) {
		printk("%s failed(%d) to set left/right volume control\n",
			    __func__, ret);
	}

        /*left channle*/
	ret = regmap_write(tas5782m->regmap, TAS5782M_REG_3D, db);
	if (ret < 0) {
		printk("%s failed(%d) to set left volume control\n",
		       __func__, ret);
	}
	tas5782m->volume_db = db;

	return 0;
}

static int tas5782m_regr_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5782m_regr_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int reg = ucontrol->value.integer.value[0];
	unsigned short d2regvalue;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	d2regvalue = snd_soc_read(codec, reg);
	printk("[0x%x]regvalue = 0x%x\n",reg,d2regvalue);

	return 0;
}

static int tas5782m_mute_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5782m_mute_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int mute;
	u8 reg3_value = 0;

	mute = ucontrol->value.integer.value[0];

	if (mute)
		reg3_value = 0x11;

	snd_soc_write(codec, TAS5782M_REG_00, 0x00);
	snd_soc_write(codec, TAS5782M_REG_7F, 0x00);
	snd_soc_write(codec, TAS5782M_REG_03, reg3_value);

	return 0;
}


static int tas5782m_suspend_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5782m_suspend_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int standby ;

	standby = ucontrol->value.integer.value[0];

	if (standby) {
	 	snd_soc_write(codec, TAS5782M_REG_00, 0x00);
	 	snd_soc_write(codec, TAS5782M_REG_7F, 0x00);
	 	snd_soc_write(codec, 0x2, 0x11);
	}
	else {
		snd_soc_write(codec, TAS5782M_REG_00, 0x00);
	 	snd_soc_write(codec, TAS5782M_REG_7F, 0x00);
	 	snd_soc_write(codec, 0x2, 0x0);
	}

	return 0;
}

static const struct snd_kcontrol_new tas5782m_controls[] = {
	SOC_SINGLE_EXT("TAS5782M DAC Master Volume",
			0,
			0,
			100,
			0,
			tas5782m_vol_get,
			tas5782m_vol_put),

	SOC_SINGLE_EXT("TAS5782M DAC Mute/Unmute",
			    0,
			    0,
			    100,
			    0,
			    tas5782m_mute_get,
			    tas5782m_mute_put),

	SOC_SINGLE_EXT("TAS5782M read value",
			0,
			0,
			500,
			0,
			tas5782m_regr_get,
			tas5782m_regr_put),

	SOC_SINGLE_EXT("TAS5782M suspend/resume",
			0,
			0,
			100,
			0,
			tas5782m_suspend_get,
			tas5782m_suspend_put),
};

static const struct snd_soc_dapm_widget  tas5782m_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("DAC_OUTA"),
	SND_SOC_DAPM_OUTPUT("DAC_OUTB"),

	SND_SOC_DAPM_OUTPUT("DAC_OUTA1"),
	SND_SOC_DAPM_OUTPUT("DAC_OUTB1"),
};

static const struct snd_soc_dapm_route  tas5782m_routes[] = {
	{ "DACL",  NULL, "Playback" },
	{ "DACR",  NULL, "Playback" },

	{ "DAC_OUTA",  NULL, "DACL" },
	{ "DAC_OUTA1", NULL, "DACL" },
	{ "DAC_OUTB",  NULL, "DACR" },
	{ "DAC_OUTB1", NULL, "DACR" },
};


static int tas5782m_soc_suspend(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, TAS5782M_REG_00, 0x00);
	snd_soc_write(codec, TAS5782M_REG_7F, 0x00);
	snd_soc_write(codec, 0x2, 0x11);

	return 0;
}

static int tas5782m_soc_resume(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, TAS5782M_REG_00, 0x00);
	snd_soc_write(codec, TAS5782M_REG_7F, 0x00);
	snd_soc_write(codec, 0x2, 0x0);

	return 0;
}

static int tas5782m_soc_enable(struct snd_soc_codec *codec)
{
	printk("[%s]\n",__func__);

	snd_soc_write(codec,TAS5782M_REG_00, 0x0);
	snd_soc_write(codec,TAS5782M_REG_7F,0x0);
	snd_soc_write(codec,TAS5782M_REG_02, 0x0);
	snd_soc_write(codec,TAS5782M_REG_03, 0x0);
	snd_soc_write(codec,TAS5782M_REG_2A,0x11);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_tas5782m = {
        .suspend =            tas5782m_soc_suspend,
	.resume  =            tas5782m_soc_resume,
	.controls =		tas5782m_controls,
	.num_controls =		ARRAY_SIZE(tas5782m_controls),
	.dapm_widgets		= tas5782m_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tas5782m_dapm_widgets),
	.dapm_routes		= tas5782m_routes,
	.num_dapm_routes	= ARRAY_SIZE(tas5782m_routes),
};

static int tas5782m_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);
	int val = 0,ret;

	printk("[%s]mode[%d],invert[%d],bitdepth[%d]\n",
		        __func__,tas5782m->mode,tas5782m->invert_mode,params_width(params));

        regmap_update_bits(tas5782m->regmap,TAS5782M_REG_3C,0xFF,0x01);
	regmap_update_bits(tas5782m->regmap,TAS5782M_REG_3D,0xFF,0x4E);

		/* Set the DAI format */
	switch (tas5782m->mode) {
	case SND_SOC_DAIFMT_I2S:
		val = TAS5782M_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val = TAS5782M_FMT_LJ;
		break;
	default:
		dev_err(codec->dev, "unknown dai format\n");
		return -EINVAL;
	}

       switch (params_width(params)) {
       	case 32:
		val |= TAS5782M_FMT_32BIT;
		break;
	#if 0
	case 24:
		val |= TAS5782M_FMT_24BIT;
		break;
	#endif
	case 16:
		val |= TAS5782M_FMT_16BIT;
		break;
	default:
		dev_err(codec->dev, "unknown bit depth \n");
		return -EINVAL;
       	}

	ret = regmap_update_bits(tas5782m->regmap, TAS5782M_REG_28, 0xFF, val);
	if (ret < 0) {
		dev_err(codec->dev, "i2c write fmt failed(%d)\n", ret);
		return ret;
	}

        switch (tas5782m->invert_mode) {
	case SND_SOC_DAIFMT_NB_NF:
		val = TAS5782M_FMT_NB_NF;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		val = TAS5782M_FMT_IB_IF;
		break;
	default:
		dev_err(codec->dev, "unknown dai invert mode\n");
		return -EINVAL;
        }
	ret = regmap_update_bits(tas5782m->regmap, TAS5782M_REG_09, 0xFF, val);
	if (ret < 0) {
		dev_err(codec->dev, "i2c write invert mode failed(%d)\n", ret);
		return ret;
	}

	tas5782m_soc_enable(codec);

	return 0;
}

 /*
 * Currently, this function only supports SND_SOC_DAIFMT_I2S and
 * SND_SOC_DAIFMT_LEFT_J.  The tas5782m codec also supports right-justified
 * data for playback only, but ASoC currently does not support different
 * formats for playback vs. record.
 */
static int tas5782m_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int format)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas5782m_priv *tas5782m = snd_soc_codec_get_drvdata(codec);

	printk("[%s]format = 0x%x\n",__func__,format);

	/* set DAI format */
	switch (format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_LEFT_J:
		tas5782m->mode = format & SND_SOC_DAIFMT_FORMAT_MASK;
		break;
	default:
		dev_err(codec->dev, "invalid dai format\n");
		return -EINVAL;
	}
	/* set DAI format */
	switch (format & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			tas5782m->invert_mode = format & SND_SOC_DAIFMT_INV_MASK;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			tas5782m->invert_mode = format & SND_SOC_DAIFMT_INV_MASK;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			tas5782m->invert_mode = format & SND_SOC_DAIFMT_INV_MASK;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			tas5782m->invert_mode = format & SND_SOC_DAIFMT_INV_MASK;
			break;
		default:
			dev_err(codec->dev, "invalid dai invert_mode\n");
			return -EINVAL;
		}


	printk("[%s]mode[%d],invert[%d]\n",__func__,tas5782m->mode,tas5782m->invert_mode);

	return 0;
}



static const struct snd_soc_dai_ops tas5782m_dai_ops = {
	.hw_params	= tas5782m_hw_params,
	.set_fmt	= tas5782m_set_dai_fmt,
};


static struct snd_soc_dai_driver tas5782m_dai = {
	.name		= "tas5782m-i2s",
	.playback 	= {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= TAS5872M_RATES,
		.formats	= TAS5782M_FORMATS,
	},
	.ops = &tas5782m_dai_ops,
};

static int tas5782m_probe(struct device *dev, struct regmap *regmap)
{
	struct tas5782m_priv *tas5782m;
	struct device_node *np = dev->of_node;
	int  ret = 0;

	tas5782m = devm_kzalloc(dev, sizeof(struct tas5782m_priv), GFP_KERNEL);
	if (!tas5782m)
		return -ENOMEM;

	dev_set_drvdata(dev, tas5782m);

	tas5782m->regmap = regmap;

	if (!dev->of_node) {
		dev_err(dev, "%s, cannot find dts node!\n", __func__);
		return ret;
	}

	dev_set_name(dev, "%s", TAS5872M_DRV_NAME);

	tas5782m->reset_gpio = of_get_named_gpio(np, "rst-gpio", 0);
	if (!gpio_is_valid(tas5782m->reset_gpio)) {
		printk("%s get invalid extamp-rst-gpio %d\n", __func__, tas5782m->reset_gpio);
		ret = -EINVAL;
	}

	ret = devm_gpio_request_one(dev, tas5782m->reset_gpio, GPIOF_OUT_INIT_LOW, "tas5782m reset");
	if (ret < 0)
		return ret;

	mdelay(120);

	if (gpio_is_valid(tas5782m->reset_gpio))
			gpio_set_value(tas5782m->reset_gpio, 1);

	tas5782m->mute_gpio = of_get_named_gpio(np, "mute-gpio", 0);

	if (!gpio_is_valid(tas5782m->mute_gpio)) {
		printk("%s get invalid extamp-mute-gpio %d\n", __func__, tas5782m->mute_gpio);
		ret = -EINVAL;
	}

	ret = devm_gpio_request_one(dev, tas5782m->mute_gpio, GPIOF_OUT_INIT_LOW, "tas5782m mute");
	if (ret < 0)
		return ret;

	if (gpio_is_valid(tas5782m->mute_gpio))
			gpio_set_value(tas5782m->mute_gpio, 1);

#if 0
	ret = regmap_register_patch(regmap, tas5782m_init_sequence, ARRAY_SIZE(tas5782m_init_sequence));
	if (ret != 0)
	{
		dev_err(dev, "Failed to initialize TAS5782M: %d\n",ret);
		goto err;

	}
#endif
	regmap_update_bits(tas5782m->regmap,0x0,0xFF,0x0); // page 0
	regmap_update_bits(tas5782m->regmap,0x7f,0xFF,0x0); //book0

	regmap_update_bits(tas5782m->regmap,0x2,0xFF,0x11);
	regmap_update_bits(tas5782m->regmap,0x1,0xFF,0x11); // go standby mode

	regmap_update_bits(tas5782m->regmap,0x0,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x3,0xFF,0x11); //L/R mute

	regmap_update_bits(tas5782m->regmap,0x2a,0xFF,0x00); //dac path mute output zero data
	regmap_update_bits(tas5782m->regmap,0x25,0xFF,0x18); //ignore mclk detect
	regmap_update_bits(tas5782m->regmap,0xd,0xFF,0x10);   //reference mclk

	regmap_update_bits(tas5782m->regmap,0x0,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x14,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x15,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x16,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x17,0xFF,0x1); //div value

	regmap_update_bits(tas5782m->regmap,0x0,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x7f,0xFF,0x00);
	regmap_update_bits(tas5782m->regmap,0x7,0xFF,0x0);
	regmap_update_bits(tas5782m->regmap,0x8,0xFF,0x20);
	regmap_update_bits(tas5782m->regmap,0x55,0xFF,0x7);

	ret = snd_soc_register_codec(dev, &soc_codec_tas5782m,&tas5782m_dai,1);
	if (ret != 0)
	{
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;

}

static int tas5782m_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = tas5782m_regmap;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return tas5782m_probe(&i2c->dev, regmap);
}


static int tas5782m_remove(struct device *dev)
{
	snd_soc_unregister_codec(dev);

	return 0;
}

static int tas5782m_i2c_remove(struct i2c_client *i2c)
{
	tas5782m_remove(&i2c->dev);

	return 0;
}


static const struct i2c_device_id tas5782m_i2c_id[] = {
	{ "tas5782m", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5782m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tas5782m_of_match[] = {
	{ .compatible = "ti,tas5782m", },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5782m_of_match);
#endif


static struct i2c_driver tas5782m_i2c_driver = {
	.probe 		= tas5782m_i2c_probe,
	.remove 	= tas5782m_i2c_remove,
	.id_table	= tas5782m_i2c_id,
	.driver		= {
		.name	= TAS5872M_DRV_NAME,
		.of_match_table = tas5782m_of_match,
	},
};

module_i2c_driver(tas5782m_i2c_driver);

MODULE_DESCRIPTION("TAS5782M Audio Amplifier Driver");
MODULE_LICENSE("GPL");
