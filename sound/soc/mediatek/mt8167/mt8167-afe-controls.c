/*
 * Mediatek Platform driver ALSA contorls
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

#include "mt8167-afe-controls.h"
#include "mt8167-afe-common.h"
#include "mt8167-afe-regs.h"
#include "mt8167-afe-util.h"
#include <sound/soc.h>


#define ENUM_TO_STR(enum) #enum


enum {
	CTRL_SGEN_EN = 0,
	CTRL_SGEN_FS,
	CTRL_AP_LOOPBACK,
};

enum {
	AFE_SGEN_OFF = 0,
	AFE_SGEN_I0I1,
	AFE_SGEN_I2,
	AFE_SGEN_I3I4,
	AFE_SGEN_I5I6,
	AFE_SGEN_I7I8,
	AFE_SGEN_I9,
	AFE_SGEN_I10I11,
	AFE_SGEN_I12I13,
	AFE_SGEN_I14,
	AFE_SGEN_I15I16,
	AFE_SGEN_I17I18,
	AFE_SGEN_I19I20,
	AFE_SGEN_I21I22,

	AFE_SGEN_O0O1,
	AFE_SGEN_O2,
	AFE_SGEN_O3,
	AFE_SGEN_O4,
	AFE_SGEN_O3O4,
	AFE_SGEN_O5O6,
	AFE_SGEN_O7O8,
	AFE_SGEN_O9O10,
	AFE_SGEN_O11,
	AFE_SGEN_O12,
	AFE_SGEN_O13O14,
	AFE_SGEN_O15O16,
	AFE_SGEN_O17O18,
	AFE_SGEN_O19O20,
	AFE_SGEN_O21O22,
	AFE_SGEN_O23O24,
};

enum {
	AFE_SGEN_8K = 0,
	AFE_SGEN_11K,
	AFE_SGEN_12K,
	AFE_SGEN_16K,
	AFE_SGEN_22K,
	AFE_SGEN_24K,
	AFE_SGEN_32K,
	AFE_SGEN_44K,
	AFE_SGEN_48K,
};

enum {
	AP_LOOPBACK_NONE = 0,
	AP_LOOPBACK_AMIC_TO_SPK,
	AP_LOOPBACK_AMIC_TO_HP,
	AP_LOOPBACK_DMIC_TO_SPK,
	AP_LOOPBACK_DMIC_TO_HP,
	AP_LOOPBACK_HEADSET_MIC_TO_SPK,
	AP_LOOPBACK_HEADSET_MIC_TO_HP,
	AP_LOOPBACK_DUAL_AMIC_TO_SPK,
	AP_LOOPBACK_DUAL_AMIC_TO_HP,
	AP_LOOPBACK_DUAL_DMIC_TO_SPK,
	AP_LOOPBACK_DUAL_DMIC_TO_HP,
};


static const char *const sgen_func[] = {
	ENUM_TO_STR(AFE_SGEN_OFF),
	ENUM_TO_STR(AFE_SGEN_I0I1),
	ENUM_TO_STR(AFE_SGEN_I2),
	ENUM_TO_STR(AFE_SGEN_I3I4),
	ENUM_TO_STR(AFE_SGEN_I5I6),
	ENUM_TO_STR(AFE_SGEN_I7I8),
	ENUM_TO_STR(AFE_SGEN_I9),
	ENUM_TO_STR(AFE_SGEN_I10I11),
	ENUM_TO_STR(AFE_SGEN_I12I13),
	ENUM_TO_STR(AFE_SGEN_I14),
	ENUM_TO_STR(AFE_SGEN_I15I16),
	ENUM_TO_STR(AFE_SGEN_I17I18),
	ENUM_TO_STR(AFE_SGEN_I19I20),
	ENUM_TO_STR(AFE_SGEN_I21I22),
	ENUM_TO_STR(AFE_SGEN_O0O1),
	ENUM_TO_STR(AFE_SGEN_O2),
	ENUM_TO_STR(AFE_SGEN_O3),
	ENUM_TO_STR(AFE_SGEN_O4),
	ENUM_TO_STR(AFE_SGEN_O3O4),
	ENUM_TO_STR(AFE_SGEN_O5O6),
	ENUM_TO_STR(AFE_SGEN_O7O8),
	ENUM_TO_STR(AFE_SGEN_O9O10),
	ENUM_TO_STR(AFE_SGEN_O11),
	ENUM_TO_STR(AFE_SGEN_O12),
	ENUM_TO_STR(AFE_SGEN_O13O14),
	ENUM_TO_STR(AFE_SGEN_O15O16),
	ENUM_TO_STR(AFE_SGEN_O17O18),
	ENUM_TO_STR(AFE_SGEN_O19O20),
	ENUM_TO_STR(AFE_SGEN_O21O22),
	ENUM_TO_STR(AFE_SGEN_O23O24),
};

static const char *const sgen_fs_func[] = {
	ENUM_TO_STR(AFE_SGEN_8K),
	ENUM_TO_STR(AFE_SGEN_11K),
	ENUM_TO_STR(AFE_SGEN_12K),
	ENUM_TO_STR(AFE_SGEN_16K),
	ENUM_TO_STR(AFE_SGEN_22K),
	ENUM_TO_STR(AFE_SGEN_24K),
	ENUM_TO_STR(AFE_SGEN_32K),
	ENUM_TO_STR(AFE_SGEN_44K),
	ENUM_TO_STR(AFE_SGEN_48K),
};

static const char *const ap_loopback_func[] = {
	ENUM_TO_STR(AP_LOOPBACK_NONE),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_DMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_DMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_DUAL_AMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_DUAL_AMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_DUAL_DMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_DUAL_DMIC_TO_HP),
};

static int mt8167_afe_sgen_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	ucontrol->value.integer.value[0] = data->sinegen_type;
	return 0;
}

static int mt8167_afe_sgen_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	if (data->sinegen_type == ucontrol->value.integer.value[0])
		return 0;

	mt8167_afe_enable_main_clk(afe);

	if (data->sinegen_type != AFE_SGEN_OFF)
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0xf0000000);

	switch (ucontrol->value.integer.value[0]) {
	case AFE_SGEN_I0I1:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x048c2762);
		break;
	case AFE_SGEN_I2:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x146c2662);
		break;
	case AFE_SGEN_I3I4:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x24862862);
		break;
	case AFE_SGEN_I5I6:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x346c2662);
		break;
	case AFE_SGEN_I7I8:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x446c2662);
		break;
	case AFE_SGEN_I10I11:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x646c2662);
		break;
	case AFE_SGEN_I12I13:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x746c2662);
		break;
	case AFE_SGEN_I15I16:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x946c2662);
		break;
	case AFE_SGEN_O0O1:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x0c7c27c2);
		break;
	case AFE_SGEN_O2:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x1c6c26c2);
		break;
	case AFE_SGEN_O3:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x2e8c28c2);
		break;
	case AFE_SGEN_O4:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x2d8c28c2);
		break;
	case AFE_SGEN_O3O4:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x2c8c28c2);
		break;
	case AFE_SGEN_O5O6:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x3c6c26c2);
		break;
	case AFE_SGEN_O7O8:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x4c6c26c2);
		break;
	case AFE_SGEN_O9O10:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x5c6c26c2);
		break;
	case AFE_SGEN_O11:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x6c6c26c2);
		break;
	case AFE_SGEN_O12:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x7c0e80e8);
		break;
	case AFE_SGEN_O13O14:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x8c6c26c2);
		break;
	case AFE_SGEN_O15O16:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0x9c6c26c2);
		break;
	case AFE_SGEN_I9:
	case AFE_SGEN_I14:
	case AFE_SGEN_I17I18:
	case AFE_SGEN_I19I20:
	case AFE_SGEN_I21I22:
	case AFE_SGEN_O17O18:
	case AFE_SGEN_O19O20:
	case AFE_SGEN_O21O22:
	case AFE_SGEN_O23O24:
		/* not supported */
		break;
	default:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xffffffff, 0xf0000000);
		break;
	}

	mt8167_afe_disable_main_clk(afe);

	data->sinegen_type = ucontrol->value.integer.value[0];

	return 0;
}

static int mt8167_afe_sgen_fs_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	ucontrol->value.integer.value[0] = data->sinegen_fs;
	return 0;
}

static int mt8167_afe_sgen_fs_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	mt8167_afe_enable_main_clk(afe);

	switch (ucontrol->value.integer.value[0]) {
	case AFE_SGEN_8K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x0);
		break;
	case AFE_SGEN_11K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x100100);
		break;
	case AFE_SGEN_12K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x200200);
		break;
	case AFE_SGEN_16K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x400400);
		break;
	case AFE_SGEN_22K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x500500);
		break;
	case AFE_SGEN_24K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x600600);
		break;
	case AFE_SGEN_32K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x800800);
		break;
	case AFE_SGEN_44K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0x900900);
		break;
	case AFE_SGEN_48K:
		regmap_update_bits(afe->regmap, AFE_SGEN_CON0, 0xf00f00, 0xa00a00);
		break;
	default:
		break;
	}

	mt8167_afe_disable_main_clk(afe);

	data->sinegen_fs = ucontrol->value.integer.value[0];

	return 0;
}

static int mt8167_afe_ap_loopback_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	ucontrol->value.integer.value[0] = data->loopback_type;

	return 0;
}

static int mt8167_afe_ap_loopback_put(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;
	uint32_t sample_rate = 48000;
	long val = ucontrol->value.integer.value[0];

	if (data->loopback_type == val)
		return 0;

	if (data->loopback_type != AP_LOOPBACK_NONE) {
		if (val == AP_LOOPBACK_AMIC_TO_SPK ||
		    val == AP_LOOPBACK_AMIC_TO_HP ||
		    val == AP_LOOPBACK_DMIC_TO_SPK ||
		    val == AP_LOOPBACK_DMIC_TO_HP) {
			/* disconnect I03 <-> O03, I03 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN1,
					   AFE_CONN1_I03_O03_S,
					   0);
			regmap_update_bits(afe->regmap, AFE_CONN2,
					   AFE_CONN2_I03_O04_S,
					   0);
		} else {
			/* disconnect I03 <-> O03, I04 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN1,
					   AFE_CONN1_I03_O03_S,
					   0);
			regmap_update_bits(afe->regmap, AFE_CONN2,
					   AFE_CONN2_I04_O04_S,
					   0);
		}

		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 0x1, 0x0);
		regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 0x1, 0x0);
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, 0x0);
		regmap_update_bits(afe->regmap, AFE_I2S_CON1, 0x1, 0x0);

		mt8167_afe_disable_afe_on(afe);

		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_DAC);
		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_DAC_PREDIS);
		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_ADC);
		mt8167_afe_disable_main_clk(afe);
	}

	if (val != AP_LOOPBACK_NONE) {
		if (val == AP_LOOPBACK_DMIC_TO_SPK ||
		    val == AP_LOOPBACK_DMIC_TO_HP ||
		    val == AP_LOOPBACK_DUAL_DMIC_TO_HP ||
		    val == AP_LOOPBACK_DUAL_DMIC_TO_HP) {
			sample_rate = 32000;
		}

		mt8167_afe_enable_main_clk(afe);

		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_DAC);
		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_DAC_PREDIS);
		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_ADC);

		if (val == AP_LOOPBACK_AMIC_TO_SPK ||
		    val == AP_LOOPBACK_AMIC_TO_HP ||
		    val == AP_LOOPBACK_DMIC_TO_SPK ||
		    val == AP_LOOPBACK_DMIC_TO_HP) {
			/* connect I03 <-> O03, I03 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN1,
					   AFE_CONN1_I03_O03_S,
					   AFE_CONN1_I03_O03_S);
			regmap_update_bits(afe->regmap, AFE_CONN2,
					   AFE_CONN2_I03_O04_S,
					   AFE_CONN2_I03_O04_S);
		} else {
			/* connect I03 <-> O03, I04 <-> O04 */
			regmap_update_bits(afe->regmap, AFE_CONN1,
					   AFE_CONN1_I03_O03_S,
					   AFE_CONN1_I03_O03_S);
			regmap_update_bits(afe->regmap, AFE_CONN2,
					   AFE_CONN2_I04_O04_S,
					   AFE_CONN2_I04_O04_S);
		}

		/* 16 bit by default */
		regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
				AFE_CONN_24BIT_O03 | AFE_CONN_24BIT_O04, 0);

		/* configure uplink */
		if (sample_rate == 32000) {
			regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0,
					   0x001e0000, (2 << 17) | (2 << 19));
			regmap_update_bits(afe->regmap, AFE_ADDA_NEWIF_CFG1,
					   0xc00, 1 << 10);
		} else {
			regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0,
					   0x001e0000, (3 << 17) | (3 << 19));
			regmap_update_bits(afe->regmap, AFE_ADDA_NEWIF_CFG1,
					   0xc00, 3 << 10);
		}

		regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);

		/* configure downlink */
		regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON0,
				   0xffffffff, 0);
		regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON1,
				   0xffffffff, 0);

		if (sample_rate == 32000) {
			regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0,
					   0xffffffff, 0x63001802);
			regmap_update_bits(afe->regmap, AFE_I2S_CON1,
					   0xf << 8, 0x9 << 8);
		} else {
			regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0,
					   0xffffffff, 0x83001802);
			regmap_update_bits(afe->regmap, AFE_I2S_CON1,
					   0xf << 8, 0xa << 8);
		}

		regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON1,
				   0xffffffff, 0xf74f0000);
		regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 0x1, 0x1);
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
		regmap_update_bits(afe->regmap, AFE_I2S_CON1, 0x1, 0x1);

		mt8167_afe_enable_afe_on(afe);
	}

	data->loopback_type = ucontrol->value.integer.value[0];

	return 0;
}

static int mt8167_afe_hdmi_force_clk_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	ucontrol->value.integer.value[0] = data->hdmi_force_clk;

	return 0;
}

static int mt8167_afe_hdmi_force_clk_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt8167_afe_control_data *data = &afe->ctrl_data;

	data->hdmi_force_clk = ucontrol->value.integer.value[0];

	return 0;
}

static int mt8167_afe_tdm_out_sgen_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	unsigned int val = 0;

	mt8167_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON_TDM, &val);

	mt8167_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & AFE_SINEGEN_CON_TDM_OUT_EN);

	return 0;
}

static int mt8167_afe_tdm_out_sgen_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);

	mt8167_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON_TDM,
				   GENMASK(31, 0), 0x11071071);
	else
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON_TDM,
				   GENMASK(31, 0), 0x100100);

	mt8167_afe_disable_main_clk(afe);

	return 0;
}

static int mt8167_afe_tdm_in_sgen_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	unsigned int val = 0;

	mt8167_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON_TDM_IN, &val);

	mt8167_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & AFE_SINEGEN_CON_TDM_IN_EN);

	return 0;
}

static int mt8167_afe_tdm_in_sgen_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);

	mt8167_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON_TDM_IN,
				   GENMASK(31, 0), 0x11071071);
	else
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON_TDM_IN,
				   GENMASK(31, 0), 0x100100);

	mt8167_afe_disable_main_clk(afe);

	return 0;
}


static const struct soc_enum mt8167_afe_soc_enums[] = {
	[CTRL_SGEN_EN] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sgen_func),
				sgen_func),
	[CTRL_SGEN_FS] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sgen_fs_func),
				sgen_fs_func),
	[CTRL_AP_LOOPBACK] = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ap_loopback_func),
				ap_loopback_func),
};

static int mt8167_afe_hw_gain1_vol_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	unsigned int val;

	mt8167_afe_enable_main_clk(afe);
	regmap_read(afe->regmap, AFE_GAIN1_CON1, &val);
	mt8167_afe_disable_main_clk(afe);
	ucontrol->value.integer.value[0] = val & AFE_GAIN1_CON1_MASK;

	return 0;
}

static int mt8167_afe_hw_gain1_vol_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	mt8167_afe_enable_main_clk(afe);
	regmap_update_bits(afe->regmap, AFE_GAIN1_CON1, AFE_GAIN1_CON1_MASK, val);
	mt8167_afe_disable_main_clk(afe);
	return 0;
}

static int mt8167_afe_hw_gain1_sampleperstep_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	unsigned int val;

	mt8167_afe_enable_main_clk(afe);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &val);
	mt8167_afe_disable_main_clk(afe);
	ucontrol->value.integer.value[0] = (val & AFE_GAIN1_CON0_SAMPLE_PER_STEP_MASK) >> 8;

	return 0;
}

static int mt8167_afe_hw_gain1_sampleperstep_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(platform);
	unsigned int val;

	val = ucontrol->value.integer.value[0];
	mt8167_afe_enable_main_clk(afe);
	regmap_update_bits(afe->regmap, AFE_GAIN1_CON0, AFE_GAIN1_CON0_SAMPLE_PER_STEP_MASK, val << 8);
	mt8167_afe_disable_main_clk(afe);
	return 0;
}

static const struct snd_kcontrol_new mt8167_afe_controls[] = {
	SOC_ENUM_EXT("Audio_SideGen_Switch",
		     mt8167_afe_soc_enums[CTRL_SGEN_EN],
		     mt8167_afe_sgen_get,
		     mt8167_afe_sgen_put),
	SOC_ENUM_EXT("Audio_SideGen_SampleRate",
		     mt8167_afe_soc_enums[CTRL_SGEN_FS],
		     mt8167_afe_sgen_fs_get,
		     mt8167_afe_sgen_fs_put),
	SOC_ENUM_EXT("AP_Loopback_Select",
		     mt8167_afe_soc_enums[CTRL_AP_LOOPBACK],
		     mt8167_afe_ap_loopback_get,
		     mt8167_afe_ap_loopback_put),
	SOC_SINGLE_BOOL_EXT("HDMI_Force_Clk_Switch",
			    0,
			    mt8167_afe_hdmi_force_clk_get,
			    mt8167_afe_hdmi_force_clk_put),
	SOC_SINGLE_BOOL_EXT("TDM_Out_Sgen_Switch",
			    0,
			    mt8167_afe_tdm_out_sgen_get,
			    mt8167_afe_tdm_out_sgen_put),
	SOC_SINGLE_BOOL_EXT("TDM_In_Sgen_Switch",
			    0,
			    mt8167_afe_tdm_in_sgen_get,
			    mt8167_afe_tdm_in_sgen_put),
	SOC_SINGLE_EXT("HW Gain1 Volume",
			    0,
			    0,
			    0x80000,
			    0,
			    mt8167_afe_hw_gain1_vol_get,
			    mt8167_afe_hw_gain1_vol_put),
	SOC_SINGLE_EXT("HW Gain1 SamplePerStep",
			    0,
			    0,
			    255,
			    0,
			    mt8167_afe_hw_gain1_sampleperstep_get,
			    mt8167_afe_hw_gain1_sampleperstep_put),
};


int mt8167_afe_add_controls(struct snd_soc_platform *platform)
{
	return snd_soc_add_platform_controls(platform, mt8167_afe_controls,
					     ARRAY_SIZE(mt8167_afe_controls));
}

