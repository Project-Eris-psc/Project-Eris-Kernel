/*
 * mtk-afe-common.h  --  Mediatek audio driver common definitions
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

#ifndef _MT8167_AFE_COMMON_H_
#define _MT8167_AFE_COMMON_H_

#include <linux/clk.h>
#include <linux/regmap.h>
#include <sound/asound.h>

#define COMMON_CLOCK_FRAMEWORK_API
#define IDLE_TASK_DRIVER_API
#define ENABLE_AFE_APLL_TUNER
#define MT8167_AFE_E1_ONLY

enum {
	MT8167_AFE_MEMIF_DL1,
	MT8167_AFE_MEMIF_DL2,
	MT8167_AFE_MEMIF_VUL,
	MT8167_AFE_MEMIF_DAI,
	MT8167_AFE_MEMIF_AWB,
	MT8167_AFE_MEMIF_MOD_DAI,
	MT8167_AFE_MEMIF_HDMI,
	MT8167_AFE_MEMIF_TDM_IN,
	MT8167_AFE_MEMIF_NUM,
	MT8167_AFE_BACKEND_BASE = MT8167_AFE_MEMIF_NUM,
	MT8167_AFE_IO_MOD_PCM1 = MT8167_AFE_BACKEND_BASE,
	MT8167_AFE_IO_MOD_PCM2,
	MT8167_AFE_IO_INT_ADDA,
	MT8167_AFE_IO_I2S,
	MT8167_AFE_IO_2ND_I2S,
	MT8167_AFE_IO_HW_GAIN1,
	MT8167_AFE_IO_HW_GAIN2,
	MT8167_AFE_IO_MRG,
	MT8167_AFE_IO_MRG_BT,
	MT8167_AFE_IO_PCM_BT,
	MT8167_AFE_IO_HDMI,
	MT8167_AFE_IO_TDM_IN,
	MT8167_AFE_IO_DL_BE,
	MT8167_AFE_BACKEND_END,
	MT8167_AFE_BACKEND_NUM = (MT8167_AFE_BACKEND_END - MT8167_AFE_BACKEND_BASE),
};

enum {
	MT8167_CLK_TOP_PDN_AUD,
	MT8167_CLK_APLL12_DIV0,
	MT8167_CLK_APLL12_DIV1,
	MT8167_CLK_APLL12_DIV2,
	MT8167_CLK_APLL12_DIV3,
	MT8167_CLK_APLL12_DIV4,
	MT8167_CLK_APLL12_DIV4B,
	MT8167_CLK_APLL12_DIV5,
	MT8167_CLK_APLL12_DIV5B,
	MT8167_CLK_APLL12_DIV6,
	MT8167_CLK_SPDIF_IN,
	MT8167_CLK_ENGEN1,
	MT8167_CLK_ENGEN2,
	MT8167_CLK_AUD1,
	MT8167_CLK_AUD2,
	MT8167_CLK_I2S0_M_SEL,
	MT8167_CLK_I2S1_M_SEL,
	MT8167_CLK_I2S2_M_SEL,
	MT8167_CLK_I2S3_M_SEL,
	MT8167_CLK_I2S4_M_SEL,
	MT8167_CLK_I2S5_M_SEL,
	MT8167_CLK_SPDIF_B_SEL,
	MT8167_CLK_NUM
};

enum mt8167_afe_tdm_ch_start {
	AFE_TDM_CH_START_O28_O29 = 0,
	AFE_TDM_CH_START_O30_O31,
	AFE_TDM_CH_START_O32_O33,
	AFE_TDM_CH_START_O34_O35,
	AFE_TDM_CH_ZERO,
};

enum mt8167_afe_irq_mode {
	MT8167_AFE_IRQ_1 = 0,
	MT8167_AFE_IRQ_2,
	MT8167_AFE_IRQ_5, /* dedicated for HDMI */
	MT8167_AFE_IRQ_6, /* dedicated for SPDIF */
	MT8167_AFE_IRQ_7,
	MT8167_AFE_IRQ_10,/* dedicated for TDM IN */
	MT8167_AFE_IRQ_NUM
};

enum mt8167_afe_top_clock_gate {
	MT8167_AFE_CG_AFE,
	MT8167_AFE_CG_I2S,
	MT8167_AFE_CG_22M,
	MT8167_AFE_CG_24M,
	MT8167_AFE_CG_INTDIR_CK,
	MT8167_AFE_CG_APLL_TUNER,
	MT8167_AFE_CG_APLL2_TUNER,
	MT8167_AFE_CG_HDMI,
	MT8167_AFE_CG_SPDIF,
	MT8167_AFE_CG_ADC,
	MT8167_AFE_CG_DAC,
	MT8167_AFE_CG_DAC_PREDIS,
	MT8167_AFE_CG_NUM
};

enum {
	MT8167_AFE_DEBUGFS_AFE,
	MT8167_AFE_DEBUGFS_HDMI,
	MT8167_AFE_DEBUGFS_TDM_IN,
	MT8167_AFE_DEBUGFS_NUM,
};

enum {
	MT8167_AFE_TDM_OUT_HDMI = 0,
	MT8167_AFE_TDM_OUT_I2S,
	MT8167_AFE_TDM_OUT_TDM,
	MT8167_AFE_TDM_OUT_I2S_32BITS,
};

enum {
	MT8167_AFE_1ST_I2S = 0,
	MT8167_AFE_2ND_I2S,
	MT8167_AFE_I2S_SETS,
};

enum {
	MT8167_AFE_I2S_SEPARATE_CLOCK = 0,
	MT8167_AFE_I2S_SHARED_CLOCK,
};

enum {
	MT8167_AFE_APLL1 = 0,
	MT8167_AFE_APLL2,
	MT8167_AFE_APLL_NUM,
};

struct snd_pcm_substream;

struct mt8167_afe_memif_data {
	int id;
	const char *name;
	int reg_ofs_base;
	int reg_ofs_end;
	int reg_ofs_cur;
	int fs_shift;
	int mono_shift;
	int enable_shift;
	int irq_reg_cnt;
	int irq_cnt_shift;
	int irq_mode;
	int irq_fs_reg;
	int irq_fs_shift;
	int irq_clr_shift;
	int max_sram_size;
	int sram_offset;
	int format_reg;
	int format_shift;
	int conn_format_mask;
	int prealloc_size;
	unsigned long buffer_align_bytes;
};

struct mt8167_afe_be_dai_data {
	bool prepared[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int fmt_mode;
	unsigned int cached_rate[SNDRV_PCM_STREAM_LAST + 1];
	snd_pcm_format_t cached_format[SNDRV_PCM_STREAM_LAST + 1];
	int cached_channels[SNDRV_PCM_STREAM_LAST + 1];
};

struct mt8167_afe_memif {
	unsigned int phys_buf_addr;
	int buffer_size;
	bool use_sram;
	bool prepared;
	struct snd_pcm_substream *substream;
	const struct mt8167_afe_memif_data *data;
};

struct mt8167_afe_control_data {
	unsigned int sinegen_type;
	unsigned int sinegen_fs;
	unsigned int loopback_type;
	bool hdmi_force_clk;
};

struct mtk_afe {
	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *sram_address;
	u32 sram_phy_address;
	u32 sram_size;
	struct device *dev;
	struct regmap *regmap;
	struct mt8167_afe_memif memif[MT8167_AFE_MEMIF_NUM];
	struct mt8167_afe_be_dai_data be_data[MT8167_AFE_BACKEND_NUM];
	struct mt8167_afe_control_data ctrl_data;
	struct clk *clocks[MT8167_CLK_NUM];
	unsigned int *backup_regs;
	bool suspended;
	int afe_on_ref_cnt;
	int adda_afe_on_ref_cnt;
	int i2s_out_on_ref_cnt;
	int daibt_on_ref_cnt;
	int irq_mode_ref_cnt[MT8167_AFE_IRQ_NUM];
	int top_cg_ref_cnt[MT8167_AFE_CG_NUM];
	int apll_tuner_ref_cnt[MT8167_AFE_APLL_NUM];
	unsigned int tdm_out_mode;
	unsigned int i2s_clk_modes[MT8167_AFE_I2S_SETS];
	unsigned int awb_irq_mode;
	unsigned int tdm_in_lrck_cycle;
	/* locks */
	spinlock_t afe_ctrl_lock;
	struct mutex afe_clk_mutex;
#ifdef IDLE_TASK_DRIVER_API
	int emi_clk_ref_cnt;
	struct mutex emi_clk_mutex;
#endif
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry[MT8167_AFE_DEBUGFS_NUM];
#endif
};

#endif
