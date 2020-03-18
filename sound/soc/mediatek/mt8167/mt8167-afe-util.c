/*
 * Mediatek audio utility
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

#include "mt8167-afe-util.h"
#include "mt8167-afe-regs.h"
#include "mt8167-afe-common.h"
#ifdef IDLE_TASK_DRIVER_API
#include "mtk_idle.h"
#define MT_CG_AUDIO_AFE (CGID(CG_AUDIO, 2))
#endif
#include <linux/device.h>

static unsigned int get_top_cg_mask(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8167_AFE_CG_AFE:
		return AUD_TCON0_PDN_AFE;
	case MT8167_AFE_CG_I2S:
		return AUD_TCON0_PDN_I2S;
	case MT8167_AFE_CG_22M:
		return AUD_TCON0_PDN_22M;
	case MT8167_AFE_CG_24M:
		return AUD_TCON0_PDN_24M;
	case MT8167_AFE_CG_INTDIR_CK:
		return AUD_TCON0_PDN_INTDIR_CK;
	case MT8167_AFE_CG_APLL_TUNER:
		return AUD_TCON0_PDN_APLL_TUNER;
	case MT8167_AFE_CG_APLL2_TUNER:
		return AUD_TCON0_PDN_APLL2_TUNER;
	case MT8167_AFE_CG_HDMI:
		return AUD_TCON0_PDN_HDMI;
	case MT8167_AFE_CG_SPDIF:
		return AUD_TCON0_PDN_SPDF;
	case MT8167_AFE_CG_ADC:
		return AUD_TCON0_PDN_ADC;
	case MT8167_AFE_CG_DAC:
		return AUD_TCON0_PDN_DAC;
	case MT8167_AFE_CG_DAC_PREDIS:
		return AUD_TCON0_PDN_DAC_PREDIS;
	default:
		return 0;
	}
}


int mt8167_afe_enable_top_cg(struct mtk_afe *afe, unsigned int cg_type)
{
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->top_cg_ref_cnt[cg_type]++;
	if (afe->top_cg_ref_cnt[cg_type] == 1)
		regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, mask, 0x0);

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

int mt8167_afe_disable_top_cg(struct mtk_afe *afe, unsigned cg_type)
{
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->top_cg_ref_cnt[cg_type]--;
	if (afe->top_cg_ref_cnt[cg_type] == 0)
		regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, mask, mask);
	else if (afe->top_cg_ref_cnt[cg_type] < 0)
		afe->top_cg_ref_cnt[cg_type] = 0;

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

int mt8167_afe_enable_main_clk(struct mtk_afe *afe)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;

	ret = clk_prepare_enable(afe->clocks[MT8167_CLK_TOP_PDN_AUD]);
	if (ret)
		goto err;
#endif

	mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_AFE);
	return 0;

#if defined(COMMON_CLOCK_FRAMEWORK_API)
err:
	dev_err(afe->dev, "%s failed %d\n", __func__, ret);
	return ret;
#endif
}

int mt8167_afe_disable_main_clk(struct mtk_afe *afe)
{
	mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_AFE);
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	clk_disable_unprepare(afe->clocks[MT8167_CLK_TOP_PDN_AUD]);
#endif
	return 0;
}

int mt8167_afe_emi_clk_on(struct mtk_afe *afe)
{
#ifdef IDLE_TASK_DRIVER_API
	mutex_lock(&afe->emi_clk_mutex);
	if (afe->emi_clk_ref_cnt == 0) {
		disable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		disable_soidle_by_bit(MT_CG_AUDIO_AFE);
	}
	afe->emi_clk_ref_cnt++;
	mutex_unlock(&afe->emi_clk_mutex);
#endif
	return 0;
}

int mt8167_afe_emi_clk_off(struct mtk_afe *afe)
{
#ifdef IDLE_TASK_DRIVER_API
	mutex_lock(&afe->emi_clk_mutex);
	afe->emi_clk_ref_cnt--;
	if (afe->emi_clk_ref_cnt == 0) {
		enable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		enable_soidle_by_bit(MT_CG_AUDIO_AFE);
	} else if (afe->emi_clk_ref_cnt < 0) {
		afe->emi_clk_ref_cnt = 0;
	}
	mutex_unlock(&afe->emi_clk_mutex);
#endif
	return 0;
}

int mt8167_afe_enable_afe_on(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->afe_on_ref_cnt++;
	if (afe->afe_on_ref_cnt == 1)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

int mt8167_afe_disable_afe_on(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->afe_on_ref_cnt--;
	if (afe->afe_on_ref_cnt == 0)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);
	else if (afe->afe_on_ref_cnt < 0)
		afe->afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

int mt8167_afe_enable_apll_tuner_cfg(struct mtk_afe *afe, unsigned int apll)
{
	mutex_lock(&afe->afe_clk_mutex);

	afe->apll_tuner_ref_cnt[apll]++;
	if (afe->apll_tuner_ref_cnt[apll] != 1) {
		mutex_unlock(&afe->afe_clk_mutex);
		return 0;
	}

	if (apll == MT8167_AFE_APLL1) {
		regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
				   AFE_APLL1_TUNER_CFG_MASK, 0x832);
		regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
				   AFE_APLL1_TUNER_CFG_EN_MASK, 0x1);
	} else {
		regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
				   AFE_APLL2_TUNER_CFG_MASK, 0x634);
		regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
				   AFE_APLL2_TUNER_CFG_EN_MASK, 0x1);
	}

	mutex_unlock(&afe->afe_clk_mutex);
	return 0;
}

int mt8167_afe_disable_apll_tuner_cfg(struct mtk_afe *afe, unsigned int apll)
{
	mutex_lock(&afe->afe_clk_mutex);

	afe->apll_tuner_ref_cnt[apll]--;
	if (afe->apll_tuner_ref_cnt[apll] == 0) {
		if (apll == MT8167_AFE_APLL1)
			regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
					   AFE_APLL1_TUNER_CFG_EN_MASK, 0x0);
		else
			regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
					   AFE_APLL2_TUNER_CFG_EN_MASK, 0x0);
	} else if (afe->apll_tuner_ref_cnt[apll] < 0) {
		afe->apll_tuner_ref_cnt[apll] = 0;
	}

	mutex_unlock(&afe->afe_clk_mutex);
	return 0;
}

int mt8167_afe_enable_apll_associated_cfg(struct mtk_afe *afe, unsigned int apll)
{
	if (apll == MT8167_AFE_APLL1) {
		clk_prepare_enable(afe->clocks[MT8167_CLK_ENGEN1]);
		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_22M);
#ifdef ENABLE_AFE_APLL_TUNER
		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_APLL_TUNER);
		mt8167_afe_enable_apll_tuner_cfg(afe, MT8167_AFE_APLL1);
#endif
	} else {
		clk_prepare_enable(afe->clocks[MT8167_CLK_ENGEN2]);
		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_24M);
#ifdef ENABLE_AFE_APLL_TUNER
		mt8167_afe_enable_top_cg(afe, MT8167_AFE_CG_APLL2_TUNER);
		mt8167_afe_enable_apll_tuner_cfg(afe, MT8167_AFE_APLL2);
#endif
	}

	return 0;
}

int mt8167_afe_disable_apll_associated_cfg(struct mtk_afe *afe, unsigned int apll)
{
	if (apll == MT8167_AFE_APLL1) {
#ifdef ENABLE_AFE_APLL_TUNER
		mt8167_afe_disable_apll_tuner_cfg(afe, MT8167_AFE_APLL1);
		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_APLL_TUNER);
#endif
		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_22M);
		clk_disable_unprepare(afe->clocks[MT8167_CLK_ENGEN1]);
	} else {
#ifdef ENABLE_AFE_APLL_TUNER
		mt8167_afe_disable_apll_tuner_cfg(afe, MT8167_AFE_APLL2);
		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_APLL2_TUNER);
#endif
		mt8167_afe_disable_top_cg(afe, MT8167_AFE_CG_24M);
		clk_disable_unprepare(afe->clocks[MT8167_CLK_ENGEN2]);
	}

	return 0;
}

