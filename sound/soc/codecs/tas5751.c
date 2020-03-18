/*
 * TAS571x amplifier audio driver
 *
 * Copyright (C) 2015 Google, Inc.
 * Copyright (c) 2013 Daniel Mack <zonque@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/stddef.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>



#include "tas5751.h"

#define TAS5751_MAX_SUPPLIES		6

//static int tas5751_reg_read(void *context, unsigned int reg,unsigned int *value);
static bool tas5751_reg_writeable(struct device *dev, unsigned int reg);
static bool tas5751_reg_readable(struct device *dev, unsigned int reg);
static bool tas5751_reg_volatile(struct device *dev, unsigned int reg);

/*static int tas5751_reg_write(void *context, unsigned int reg,
			     unsigned int value);*/


static const char *const tas5751_supply_names[] = {
	"AVDD",
	"DVDD",
	"PVDD_A",
	"PVDD_B",
	"PVDD_C",
	"PVDD_D",
};



static const struct regmap_config tas5751_regmap_config = {
	.reg_bits			= 8,
	.val_bits			= 8,
	.max_register			= 0xff,
	.reg_read			= NULL,
	.reg_write			= NULL,
	.writeable_reg      = tas5751_reg_writeable,
	.readable_reg       = tas5751_reg_readable,
	.volatile_reg       = tas5751_reg_volatile,
	//.reg_defaults			= tas5751_reg_defaults,
	//.num_reg_defaults		= ARRAY_SIZE(tas5751_reg_defaults),
	.cache_type			= REGCACHE_RBTREE,
};

struct tas5751_chip_e {
	const char			*const *supply_names;
	int				num_supply_names;
	const struct snd_kcontrol_new	*controls;
	int				num_controls;
	const struct regmap_config	*regmap_config;
	int				vol_reg_size;
};

static const struct tas5751_chip_e tas5751_chip = {
	.supply_names			= tas5751_supply_names,
	.num_supply_names		= ARRAY_SIZE(tas5751_supply_names),
	//.controls			= tas5711_controls,
	//.num_controls			= ARRAY_SIZE(tas5711_controls),
	.regmap_config			= &tas5751_regmap_config,
	.vol_reg_size			= 1,
};

static const char *const tas5717_supply_names[] = {
	"AVDD",
	"DVDD",
	"HPVDD",
	"PVDD_AB",
	"PVDD_CD",
};


struct tas5751_private {
	const struct tas5751_chip_e	*chip;
	struct regmap			*regmap;
	struct regulator_bulk_data	supplies[TAS5751_MAX_SUPPLIES];
	struct clk			*mclk;
	int master_volume;
	unsigned int			format;
	//struct gpio_desc		*reset_gpio;
	//struct gpio_desc		*pdn_gpio;
	int reset_gpio;
	int pdn_gpio;

	int i2c_pull_gpio;
	int led_en_gpio;
	int wifi_pull_gpio;
	int wifi_en_gpio;

	struct snd_soc_codec_driver	codec_driver;
	/* power domain regulators */
	struct regulator *supply; /* power for va vd vlc vls */
};

static int tas5751_set_outPut(struct tas5751_private *priv);
//static int tas5751_drcCtrl(struct tas5751_private *priv);

static const struct of_device_id tas5751_of_match[] = {
	{ .compatible = "mediatek,tas5751", .data = &tas5751_chip, },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5751_of_match);


static bool tas5751_reg_readable(struct device *dev, unsigned int reg)
{
	return 1;
}

static bool tas5751_reg_writeable(struct device *dev, unsigned int reg)
{
	return 1;
}

static bool tas5751_reg_volatile(struct device *dev, unsigned int reg)
{
	return 1;
}

static int tas5751_set_dai_fmt(struct snd_soc_dai *dai, unsigned int format)
{
	struct tas5751_private *priv = snd_soc_codec_get_drvdata(dai->codec);

	priv->format = format;

	return 0;
}

static int tas5751_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
#if 0
	struct tas5751_private *priv = snd_soc_codec_get_drvdata(dai->codec);
	u32 val;

	switch (priv->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		val = 0x00;
		break;
	case SND_SOC_DAIFMT_I2S:
		val = 0x03;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val = 0x06;
		break;
	default:
		return -EINVAL;
	}

	if (params_width(params) >= 24)
		val += 2;
	else if (params_width(params) >= 20)
		val += 1;

	return regmap_update_bits(priv->regmap, TAS571X_SDI_REG,
				  TAS571X_SDI_FMT_MASK, val);
#endif
	return 0;
}

static int tas5751_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
#if 0
	struct tas5751_private *priv = snd_soc_codec_get_drvdata(codec);
	int ret;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_codec_get_bias_level(codec) == SND_SOC_BIAS_OFF) {
			if (!IS_ERR(priv->mclk)) {
				ret = clk_prepare_enable(priv->mclk);
				if (ret) {
					dev_err(codec->dev,
						"Failed to enable master clock: %d\n",
						ret);
					return ret;
				}
			}

			gpiod_set_value(priv->pdn_gpio, 0);
			usleep_range(5000, 6000);

			regcache_cache_only(priv->regmap, false);
			ret = regcache_sync(priv->regmap);
			if (ret)
				return ret;
		}
		break;
	case SND_SOC_BIAS_OFF:
		regcache_cache_only(priv->regmap, true);
		gpiod_set_value(priv->pdn_gpio, 1);

		if (!IS_ERR(priv->mclk))
			clk_disable_unprepare(priv->mclk);
		break;
	}
#endif
	return 0;
}

static const struct snd_soc_dai_ops tas5751_dai_ops = {
	.set_fmt	= tas5751_set_dai_fmt,
	.hw_params	= tas5751_hw_params,
};

#if 0
static const DECLARE_TLV_DB_SCALE(tas5711_volume_tlv, -10350, 50, 1);

static const struct snd_kcontrol_new tas5711_controls[] = {
	SOC_SINGLE_TLV("Master Volume",
	TAS571X_MVOL_REG,
	0, 0xff, 1, tas5711_volume_tlv),
	SOC_DOUBLE_R_TLV("Speaker Volume",
	TAS571X_CH1_VOL_REG,
	TAS571X_CH2_VOL_REG,
	0, 0xff, 1, tas5711_volume_tlv),
	SOC_DOUBLE("Speaker Switch",
	TAS571X_SOFT_MUTE_REG,
	TAS571X_SOFT_MUTE_CH1_SHIFT, TAS571X_SOFT_MUTE_CH2_SHIFT,
	1, 1),
};

static const struct reg_default tas5751_reg_defaults[] = {
	{ 0x04, 0x05 },
	//{ 0x05, 0x40 },
	{ 0x06, 0x00 },
	//{ 0x07, 0x3ff },
	{ 0x08, 0x30 },
	{ 0x09, 0x30 },
	//{ 0x1b, 0x82 },
	{0x46, 0x00020000},
	{0x40, 0x08000000},
	{0x3c, 0x00000100},
	{}
};
#endif

static int tas5751_vol_get(struct snd_kcontrol *kctl,
			   struct snd_ctl_elem_value *ucontrol)
{
	#if 1
	struct snd_soc_component *component = snd_kcontrol_chip(kctl);
	struct tas5751_private *tas5751 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = tas5751->master_volume;
	printk("read volume is :%d\n", tas5751->master_volume);
#endif
	return 0;
}

/*0xc0 --- 0db -49.5 ~0db two bytes */
static int tas5751_vol_put(struct snd_kcontrol *kctl,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kctl);
	struct tas5751_private *tas5751 =
		snd_soc_component_get_drvdata(component);
	int value, ret;
	unsigned short db;

	value = ucontrol->value.integer.value[0];
	printk("volume setting is :%d\n", value);
	if (value == 0)
		db = 0x3FF;
	else
		db = 0xC0 + (100 - value) * 4;

	db = (db&0xFF00)>>8 | (db&0xFF)<<8;
	//db = bMainVolTable[value] - MAINVOL_OFFSET;
	printk("volume setting is :%x db\n", db);

#if 0
	/* if set volume to -100db, do mute enable */
	if (value == 0)
		db |= 0x80;
	else
		db &= 0x7F;
#endif
	ret = regmap_raw_write(tas5751->regmap, eMasterVol, (void *)&db, sizeof(db));
	if (ret < 0) {
		printk("%s failed(%d) to set volume control\n",
		       __func__, ret);
	}
	db = 0x0;
	ret = regmap_raw_read(tas5751->regmap, eMasterVol, (void *)&db, sizeof(db));
	if (ret < 0) {
		printk("%s failed(%d) to set volume control\n",
		       __func__, ret);
	}
	printk("read volume is :%xdb\n", db);

	tas5751->master_volume = value;

	return ret;
}

struct tas5751_private *priv_test,*priv_resume;

#if 0
static int tas5751_test_get(struct snd_kcontrol *kctl,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int tas5751_test_put(struct snd_kcontrol *kctl,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct tas5751_private *priv;
	int  ret;
	int cmd = ucontrol->value.integer.value[0];
	priv = priv_test;

	printk(KERN_EMERG"tas5751_test_put cmd=%d \n", cmd);

	if (cmd == 0) {

		ret = tas5751_set_outPut(priv);
		if (ret) {
			printk(" ERR:tas5751_set_outPut %d\n", ret);
			return ret;
		}
		ret = tas5751_drcCtrl(priv);
		if (ret) {
			printk("ERR:tas5751_drcCtrl %d\n", ret);
			return ret;
		}
	}

	if (cmd == 1) {

		printk("gpio_rest\n");

		if (gpio_is_valid(priv->pdn_gpio))
			gpio_set_value(priv->pdn_gpio, 0);
		mdelay(10);


		if (gpio_is_valid(priv->reset_gpio))
			gpio_set_value(priv->reset_gpio, 0);
		mdelay(10);

		if (gpio_is_valid(priv->pdn_gpio))
			gpio_set_value(priv->pdn_gpio, 0);
		mdelay(1000);


		if (gpio_is_valid(priv->reset_gpio))
			gpio_set_value(priv->reset_gpio, 1);
		mdelay(10);

	}
	return 0;
}

static int tas5751_regw_get(struct snd_kcontrol *kctl,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int tas5751_regw_put(struct snd_kcontrol *kctl,
			    struct snd_ctl_elem_value *ucontrol)
{
	int  ret;
	int reg = ucontrol->value.integer.value[0];
	unsigned char writeregval = ucontrol->value.integer.value[1];
	unsigned char readregval;

	printk(KERN_EMERG"tas5751_regw_put reg=%d,writeregval=0x%x \n", reg, writeregval);
	ret = regmap_raw_write(priv_test->regmap, reg, (void *)&writeregval, sizeof(writeregval));
	ret = regmap_raw_read(priv_test->regmap, reg, (void *)&readregval, sizeof(readregval));
	printk(KERN_EMERG"tas5751_regw_put readregval=0x%x \n", readregval);

	return 0;
}


static int tas5751_regr_get(struct snd_kcontrol *kctl,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int tas5751_regr_put(struct snd_kcontrol *kctl,
			    struct snd_ctl_elem_value *ucontrol)
{
	int  ret;
	int reg = ucontrol->value.integer.value[0];
	unsigned char regvalue[4], d1regvalue;

	printk(KERN_EMERG"tas5751_regr_put reg = 0x%x \n", reg);
	if ((reg == 0x25) || (reg = 0x20) || (reg = 0x46) || (reg = 0x4f) || (reg = 0x50)) {
		ret = regmap_raw_read(priv_test->regmap, reg, (void *)regvalue, sizeof(regvalue));

		printk(KERN_EMERG"tas5751_regr_put regvalue0 = 0x%x \n", regvalue[0]);
		printk(KERN_EMERG"tas5751_regr_put regvalue1 = 0x%x \n", regvalue[1]);
		printk(KERN_EMERG"tas5751_regr_put regvalue2 = 0x%x \n", regvalue[2]);
		printk(KERN_EMERG"tas5751_regr_put regvalue3 = 0x%x \n", regvalue[3]);
	} else {
		ret = regmap_raw_read(priv_test->regmap, reg, (void *)&d1regvalue, sizeof(d1regvalue));
		printk(KERN_EMERG"tas5751_regr_put reg[%d] = 0x%x \n", reg, d1regvalue);
	}

	return 0;

}

static int tas5751_regr_getchar(struct snd_kcontrol *kctl,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int tas5751_regr_putchar(struct snd_kcontrol *kctl,
				struct snd_ctl_elem_value *ucontrol)
{
	int  ret;
	int reg = ucontrol->value.integer.value[0];
	unsigned char d1regvalue;

	printk(KERN_EMERG"tas5751_regr_put reg = 0x%x \n", reg);
	ret = regmap_raw_read(priv_test->regmap, reg, (void *)&d1regvalue, sizeof(d1regvalue));
	printk(KERN_EMERG"tas5751_regr_put reg[%d] = 0x%x \n", reg, d1regvalue);

	return 0;

}

static int tas5751_regr_getshort(struct snd_kcontrol *kctl,
				 struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int tas5751_regr_putshort(struct snd_kcontrol *kctl,
				 struct snd_ctl_elem_value *ucontrol)
{
	int  ret;
	int reg = ucontrol->value.integer.value[0];
	unsigned short d2regvalue;

	printk(KERN_EMERG"tas5751_regr_put reg = 0x%x \n", reg);
	ret = regmap_raw_read(priv_test->regmap, reg, (void *)&d2regvalue, sizeof(d2regvalue));
	printk(KERN_EMERG"tas5751_regr_put reg[%d] = 0x%x \n", reg, d2regvalue);

	return 0;
}
#endif

static const DECLARE_TLV_DB_SCALE(tas5717_volume_tlv, -10375, 25, 0);

static const struct snd_kcontrol_new tas5751_controls[] = {
	SOC_SINGLE_EXT("DAC Master Volume",
	0,
	0,
	100,
	0,
	tas5751_vol_get,
	tas5751_vol_put),
#if 0
	SOC_SINGLE_EXT("test probe",
	0,
	0,
	100,
	0,
	tas5751_test_get,
	tas5751_test_put),

	SOC_DOUBLE_EXT("reg regw",
	SND_SOC_NOPM, 0, 0, 100, 0,
	tas5751_regw_get,
	tas5751_regw_put),

	SOC_SINGLE_EXT("reg regr",
	0,
	0,
	100,
	0,
	tas5751_regr_get,
	tas5751_regr_put),
	SOC_SINGLE_EXT("reg regrchar",
	0,
	0,
	100,
	0,
	tas5751_regr_getchar,
	tas5751_regr_putchar),
	SOC_SINGLE_EXT("reg regrshort",
	0,
	0,
	100,
	0,
	tas5751_regr_getshort,
	tas5751_regr_putshort)
#endif
};

static const struct snd_soc_dapm_widget tas5751_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUT_A"),
	SND_SOC_DAPM_OUTPUT("OUT_B"),
	SND_SOC_DAPM_OUTPUT("OUT_C"),
	SND_SOC_DAPM_OUTPUT("OUT_D"),
};

static const struct snd_soc_dapm_route tas5751_dapm_routes[] = {
	{ "DACL",  NULL, "Playback" },
	{ "DACR",  NULL, "Playback" },

	{ "OUT_A", NULL, "DACL" },
	{ "OUT_B", NULL, "DACL" },
	{ "OUT_C", NULL, "DACR" },
	{ "OUT_D", NULL, "DACR" },
};
static int tas5751_soc_suspend(struct snd_soc_codec *codec)
{
	struct tas5751_private *priv = snd_soc_codec_get_drvdata(codec);
         int ret;

	if (!IS_ERR(priv->supply)) {
		ret = regulator_disable(priv->supply);
		if (ret != 0) {
			printk("tas5751_soc_suspend regulator_disable is ERR\r\n");
			return ret;
		}
	}
	return 0;
}

static int tas5751_soc_resume(struct snd_soc_codec *codec)
{
	struct tas5751_private *priv = snd_soc_codec_get_drvdata(codec);
	int ret;

	if (!IS_ERR(priv->supply)) {
		ret = regulator_enable(priv->supply);
		if (ret != 0) {
			printk("tas5751_soc_resume regulator_enable is ERR\r\n");
			return ret;
		}
	}

	if (gpio_is_valid(priv_resume->pdn_gpio))
		gpio_set_value(priv_resume->pdn_gpio, 0);
	mdelay(300);

	if (gpio_is_valid(priv_resume->reset_gpio))
		gpio_set_value(priv_resume->reset_gpio, 1);

    	mdelay(20);/*delay13.5ms after reset accoding to 5751spec*/

	ret = tas5751_set_outPut(priv_resume);
	if (ret) {
		printk("ERR:tas5751_set_outPut %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct snd_soc_codec_driver tas5751_codec = {
	.set_bias_level = tas5751_set_bias_level,
	.idle_bias_off = true,
	.suspend = tas5751_soc_suspend,
	.resume = tas5751_soc_resume,

	/*.dapm_widgets = tas5751_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas5751_dapm_widgets),
	.dapm_routes = tas5751_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(tas5751_dapm_routes),*/
	.controls =		tas5751_controls,
	.num_controls =		ARRAY_SIZE(tas5751_controls),
};

static struct snd_soc_dai_driver tas5751_dai = {
	.name = "tas5751-i2s",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE |
		SNDRV_PCM_FMTBIT_S24_LE |
		SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &tas5751_dai_ops,
};

static const struct of_device_id tas571x_of_match[];

static int tas5751_set_outPut(struct tas5751_private *priv)
{
	int ret;
	unsigned char trimPtr = 0x0, sysCtl2Ptr = 0x0; //0x80;
	//unsigned short mastVolPtr = 0xb0;
	//unsigned char abData,awData[2],adData[4];
	unsigned char awData[2];

	//printk("tas5751_set_outPut\n");
	regcache_cache_bypass(priv->regmap, true);
	ret = regmap_raw_write(priv->regmap, eOscillatorTrim, (void *)&trimPtr, sizeof(trimPtr));
	if (ret)
		return ret;

	mdelay(50);
#if 0
	abData = 0xb8;
	ret = regmap_raw_write(priv->regmap, eICDelayCh1, (void *)&abData, sizeof(abData));
	if (ret)
		return ret;

	abData = 0x60;
	ret = regmap_raw_write(priv->regmap, eICDelayCh2, (void *)&abData, sizeof(abData));
	if (ret)
		return ret;
	abData = 0xa0;
	ret = regmap_raw_write(priv->regmap, eICDelayCh3, (void *)&abData, sizeof(abData));
	if (ret)
		return ret;

	abData = 0x48;
	ret = regmap_raw_write(priv->regmap, eICDelayCh4, (void *)&abData, sizeof(abData));
	if (ret)
		return ret;

	adData[0] = 0x01;
	adData[1] = 0x02;
	adData[2] = 0x13;
	adData[3] = 0x45;
	ret = regmap_raw_write(priv->regmap, ePWMMux, adData, sizeof(adData));
	if (ret)
		return ret;
#endif


	ret = regmap_raw_write(priv->regmap, eSysCtl2, (void *)&sysCtl2Ptr, sizeof(sysCtl2Ptr));
	if (ret)
		return ret;

#if 0
	abData = 0x0;
	ret = regmap_raw_write(priv->regmap, ePWMSDG, (void *)&abData, sizeof(abData));
	if (ret)
		return ret;
	mdelay(50);
#endif
	awData[0] = 0x00;
	awData[1] = 0xC0;
	ret = regmap_raw_write(priv->regmap, eMasterVol, (void *)awData, sizeof(awData));
	if (ret)
		return ret;

	mdelay(50);
#if 0
	ret = regmap_raw_read(priv->regmap, eOscillatorTrim, (void *)&trimPtr, sizeof(trimPtr));
	if (ret)
		return ret;

	ret = regmap_raw_read(priv->regmap, eMasterVol, (void *)&mastVolPtr, sizeof(mastVolPtr));
	if (ret)
		return ret;

	ret = regmap_raw_read(priv->regmap, eSysCtl2, (void *)&sysCtl2Ptr, sizeof(sysCtl2Ptr));
	if (ret)
		return ret;
	printk("eOscillatorTrim is %x\r\n", trimPtr);
	printk("eMasterVol is %x\r\n", mastVolPtr);
	printk("sysCtl2Ptr is %x\r\n", sysCtl2Ptr);
#endif
	regcache_cache_bypass(priv->regmap, false);
	return 0;
}

#if 0
static int tas5751_drcCtrl(struct tas5751_private *priv)
{

	int ret, i;
	unsigned int readDate = 0;
	unsigned char AGLCtrlData4[4] = {0x0, 0x02, 0x0, 0x05};

	unsigned char AGL1AttkThdData[4] = {0x07, 0x8d, 0x6f, 0xc9}; // -0.9db

	unsigned char AGL1AttkRelseRateData[8] = {0x00, 0x00, 0x15, 0x8b, 0xff, 0xff, 0xff, 0xe5};
	unsigned char AGL3AttkRelseRateData[8] = {0x00, 0x00, 0x00, 0x6d, 0xff, 0xff, 0xff, 0xe5};

	unsigned char AGL1SoftFtData[8] = {0x00, 0x07, 0x60, 0x53, 0x00, 0x78, 0x9f, 0xac};
	unsigned char AGL3SoftFtData[8] = {0x00, 0x07, 0x60, 0x53, 0x00, 0x78, 0x9f, 0xac};

	regcache_cache_bypass(priv->regmap, true);
	ret = regmap_raw_write(priv->regmap, eAGLCtrl, AGLCtrlData4, sizeof(AGLCtrlData4));
	if (ret)
		return ret;

	ret = regmap_raw_write(priv->regmap, eAGL1AttkThd, AGL1AttkThdData, sizeof(AGL1AttkThdData));
	if (ret)
		return ret;

	ret = regmap_raw_write(priv->regmap, eAGL1AttkRelseRate, AGL1AttkRelseRateData, sizeof(AGL1AttkRelseRateData));
	if (ret)
		return ret;

	ret = regmap_raw_write(priv->regmap, eAGL1SoftFt, AGL1SoftFtData, sizeof(AGL1SoftFtData));
	if (ret)
		return ret;

	ret = regmap_raw_write(priv->regmap, eAGL3SoftFt, AGL3SoftFtData, sizeof(AGL3SoftFtData));
	if (ret)
		return ret;

	ret = regmap_raw_write(priv->regmap, eAGL3AttkRelseRate, AGL3AttkRelseRateData, sizeof(AGL3AttkRelseRateData));
	if (ret)
		return ret;
	#if 0
	///AGL1AttkThdData[0] = 0x00;
	//AGL1AttkThdData[1] = 0x40;
	//AGL1AttkThdData[2] = 0x00;
	//AGL1AttkThdData[3] = 0x00;
	//ret = regmap_raw_write(priv->regmap, 0x72, AGL1AttkThdData,sizeof(AGL1AttkThdData));
	//ret = regmap_raw_write(priv->regmap, 0x73, AGL1AttkThdData,sizeof(AGL1AttkThdData));
	//if (ret)
	//	return ret;

	AGL1AttkThdData[0] = 0x01;
	AGL1AttkThdData[1] = 0x02;
	AGL1AttkThdData[2] = 0x13;
	AGL1AttkThdData[3] = 0x45;
	ret = regmap_raw_write(priv->regmap, ePWMMux, AGL1AttkThdData, sizeof(AGL1AttkThdData));
	if (ret)
		return ret;
	#endif

	ret = regmap_read(priv->regmap, eErrStat, &readDate);
	if (ret == 0)
		printk("the eErrStat is %x\r\n", readDate);
	ret = regmap_raw_read(priv->regmap, eAGLCtrl, AGLCtrlData4, sizeof(AGLCtrlData4));
	if (ret)
		return ret;
	for (i = 0; i < sizeof(AGLCtrlData4); i++)
		printk("the AGLCtrlData4[%d] is %x\r\n", i, AGLCtrlData4[i]);

	ret = regmap_raw_read(priv->regmap, eAGL1AttkThd, AGL1AttkThdData, sizeof(AGL1AttkThdData));
	if (ret)
		return ret;
	for (i = 0; i < sizeof(AGL1AttkThdData); i++)
		printk("the AGL1AttkThdData[%d] is %x\r\n", i, AGL1AttkThdData[i]);

	ret = regmap_raw_read(priv->regmap, eAGL1AttkRelseRate, AGL1AttkRelseRateData, sizeof(AGL1AttkRelseRateData));
	if (ret)
		return ret;
	for (i = 0; i < sizeof(AGL1AttkRelseRateData); i++)
		printk("the AGL1AttkRelseRateData[%d] is %x\r\n", i, AGL1AttkRelseRateData[i]);

	ret = regmap_raw_read(priv->regmap, eAGL1SoftFt, AGL1SoftFtData, sizeof(AGL1SoftFtData));
	if (ret)
		return ret;
	for (i = 0; i < sizeof(AGL1SoftFtData); i++)
		printk("the AGL1SoftFtData[%d] is %x\r\n", i, AGL1SoftFtData[i]);

	ret = regmap_raw_read(priv->regmap, eAGL3SoftFt, AGL3SoftFtData, sizeof(AGL3SoftFtData));
	if (ret)
		return ret;
	for (i = 0; i < sizeof(AGL3SoftFtData); i++)
		printk("the AGL3SoftFtData[%d] is %x\r\n", i, AGL3SoftFtData[i]);

	ret = regmap_raw_read(priv->regmap, eAGL3AttkRelseRate, AGL3AttkRelseRateData, sizeof(AGL3AttkRelseRateData));
	if (ret)
		return ret;
	for (i = 0; i < sizeof(AGL3AttkRelseRateData); i++)
		printk("the AGL3AttkRelseRateData[%d] is %x\r\n", i, AGL3AttkRelseRateData[i]);

	regcache_cache_bypass(priv->regmap, false);
	return 0;
}
#endif
static int tas5751_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct tas5751_private *priv;
	struct device *dev = &client->dev;
	const struct of_device_id *of_id;
	struct device_node *np = dev->of_node;
	int  ret;

	printk(KERN_EMERG"tas5751_i2c_probe address is %x\n", client->addr);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	i2c_set_clientdata(client, priv);
	priv_test = priv;

	of_id = of_match_device(tas5751_of_match, dev);
	if (!of_id) {
		dev_err(dev, "Unknown device type\n");
		printk(">Unknown device type \r\n");
		return -EINVAL;
	}
	priv->chip = of_id->data;

	priv->supply =  devm_regulator_get(dev, "tas5751");
	if (IS_ERR(priv->supply)) {
		ret = PTR_ERR(priv->supply);
		printk("tas5751_i2c_probe devm_regulator_get is ERR\r\n");
		return ret;
	}

	ret = regulator_enable(priv->supply);
	if (ret != 0) {
		printk("tas5751_i2c_probe regulator_set_voltage is ERR\r\n");
		return ret;
	}

	/*tas5751 amp reset*/
	priv->reset_gpio = of_get_named_gpio(np, "rst-gpio", 0);
	if (!gpio_is_valid(priv->reset_gpio)) {
		printk("%s get invalid extamp-rst-gpio %d\n", __func__, priv->reset_gpio);
		ret = -EINVAL;
	}

	ret = devm_gpio_request_one(dev, priv->reset_gpio, GPIOF_OUT_INIT_LOW, "tas5751 reset");
	if (ret < 0)
		return ret;

	priv->pdn_gpio = of_get_named_gpio(np, "pdn-gpio", 0);
	if (!gpio_is_valid(priv->pdn_gpio)) {
		printk("%s get invalid extamp-pdn-gpio %d\n", __func__, priv->pdn_gpio);
		ret = -EINVAL;
	}

	ret = devm_gpio_request_one(dev, priv->pdn_gpio, GPIOF_OUT_INIT_HIGH, "tas5751 pdn");
	if (ret < 0)
		return ret;
	mdelay(10);

	if (gpio_is_valid(priv->pdn_gpio))
		gpio_set_value(priv->pdn_gpio, 0);
	mdelay(300);

	if (gpio_is_valid(priv->reset_gpio))
		gpio_set_value(priv->reset_gpio, 1);

	/*tas5751 amp init*/
	priv->regmap = devm_regmap_init_i2c(client, priv->chip->regmap_config);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);
    	mdelay(20);/*delay13.5ms after reset accoding to 5751spec*/
	ret = tas5751_set_outPut(priv);
	if (ret) {
		printk("ERR:tas5751_set_outPut %d\n", ret);
		return ret;
	}
	priv_resume = priv;
	#if 0
	ret = tas5751_drcCtrl(priv);
	if (ret) {
		printk(" ERR:tas5751_drcCtrl %d\n", ret);
		return ret;
	}
	#endif
	memcpy(&priv->codec_driver, &tas5751_codec, sizeof(priv->codec_driver));


	//printk("tas5751_i2c_probe end \r\n");

	return snd_soc_register_codec(&client->dev, &priv->codec_driver,
				      &tas5751_dai, 1);

}

static int tas5751_i2c_remove(struct i2c_client *client)
{
	struct tas5751_private *priv = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
	regulator_bulk_disable(priv->chip->num_supply_names, priv->supplies);

	return 0;
}

static const struct i2c_device_id tas5751_i2c_id[] = {
	{ "tas5751", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5751_i2c_id);

static struct i2c_driver tas5751_i2c_driver = {
	.driver = {
		.name = "tas5751",
		.of_match_table = tas5751_of_match,
	},
	.probe = tas5751_i2c_probe,
	.remove = tas5751_i2c_remove,
	.id_table = tas5751_i2c_id,
};
module_i2c_driver(tas5751_i2c_driver);

MODULE_DESCRIPTION("ASoC TAS5751 driver");
MODULE_LICENSE("GPL");

