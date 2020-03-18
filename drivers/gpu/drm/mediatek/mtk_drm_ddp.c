/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"

#if 1 /* 8167 */
#define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN	0x030
#define DISP_REG_CONFIG_DISP_OVL1_MOUT_EN	0x034
#define DISP_REG_CONFIG_DISP_DITHER_MOUT_EN	0x038
#define DISP_REG_CONFIG_DISP_UFOE_MOUT_EN	0x03c

#define DISP_REG_CONFIG_DISP_COLOR0_SEL_IN	0x058
#define DISP_REG_CONFIG_DISP_WDMA_SEL_IN	0x05c
#define DISP_REG_CONFIG_DISP_UFOE_SEL_IN	0x060
#define DISP_REG_CONFIG_DSI0_SEL_IN			0x064
#define DISP_REG_CONFIG_DPI0_SEL_IN			0x068

#define DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN	0x06c
#define DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN	0x070
#define DISP_REG_CONFIG_DPI1_SEL_IN				0x074

#else
#define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN	0x040
#define DISP_REG_CONFIG_DISP_OVL1_MOUT_EN	0x044
#define DISP_REG_CONFIG_DISP_UFOE_MOUT_EN	0x050

#define DISP_REG_CONFIG_DISP_COLOR0_SEL_IN	0x084
#define DISP_REG_CONFIG_DPI0_SEL_IN			0x0ac
#define DISP_REG_CONFIG_MMSYS_CG_CON0		0x100

#define DISP_REG_CONFIG_DISP_OVL_MOUT_EN	0x030
#define DISP_REG_CONFIG_DSI_SEL			0x050
#define DISP_REG_CONFIG_DPI_SEL			0x064
#endif

#define DISP_REG_CONFIG_DISP_OD_MOUT_EN		0x048
#define DISP_REG_CONFIG_OUT_SEL				0x04c
#define DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN	0x04c
#define DISP_REG_CONFIG_DISP_RDMA1_MOUT_EN	0x0c8
#define DISP_REG_CONFIG_DISP_COLOR1_SEL_IN	0x088


#define DISP_REG_MUTEX_EN(n)	(0x20 + 0x20 * (n))
#define DISP_REG_MUTEX(n)		(0x24 + 0x20 * (n))
#define DISP_REG_MUTEX_RST(n)	(0x28 + 0x20 * (n))
#define DISP_REG_MUTEX_MOD(n)	(0x2c + 0x20 * (n))
#define DISP_REG_MUTEX_SOF(n)	(0x30 + 0x20 * (n))

#define MT8167_MUTEX_MOD_DISP_OVL0		BIT(6)
#define MT8167_MUTEX_MOD_DISP_OVL1		BIT(7)
#define MT8167_MUTEX_MOD_DISP_RDMA0		BIT(8)
#define MT8167_MUTEX_MOD_DISP_RDMA1		BIT(9)
#define MT8167_MUTEX_MOD_DISP_WDMA0		BIT(10)
#define MT8167_MUTEX_MOD_DISP_CCORR		BIT(11)
#define MT8167_MUTEX_MOD_DISP_COLOR		BIT(12)
#define MT8167_MUTEX_MOD_DISP_AAL		BIT(13)
#define MT8167_MUTEX_MOD_DISP_GAMMA		BIT(14)
#define MT8167_MUTEX_MOD_DISP_DITHER	BIT(15)
#define MT8167_MUTEX_MOD_DISP_UFOE		BIT(16)
#define MT8167_MUTEX_MOD_DISP_PWM		BIT(17)

#define MT8173_MUTEX_MOD_DISP_OVL0		BIT(11)
#define MT8173_MUTEX_MOD_DISP_OVL1		BIT(12)
#define MT8173_MUTEX_MOD_DISP_RDMA0		BIT(13)
#define MT8173_MUTEX_MOD_DISP_RDMA1		BIT(14)
#define MT8173_MUTEX_MOD_DISP_RDMA2		BIT(15)
#define MT8173_MUTEX_MOD_DISP_WDMA0		BIT(16)
#define MT8173_MUTEX_MOD_DISP_WDMA1		BIT(17)
#define MT8173_MUTEX_MOD_DISP_COLOR0		BIT(18)
#define MT8173_MUTEX_MOD_DISP_COLOR1		BIT(19)
#define MT8173_MUTEX_MOD_DISP_AAL		BIT(20)
#define MT8173_MUTEX_MOD_DISP_GAMMA		BIT(21)
#define MT8173_MUTEX_MOD_DISP_UFOE		BIT(22)
#define MT8173_MUTEX_MOD_DISP_PWM0		BIT(23)
#define MT8173_MUTEX_MOD_DISP_PWM1		BIT(24)
#define MT8173_MUTEX_MOD_DISP_OD		BIT(25)

#define MT2701_MUTEX_MOD_DISP_OVL		BIT(3)
#define MT2701_MUTEX_MOD_DISP_WDMA		BIT(6)
#define MT2701_MUTEX_MOD_DISP_COLOR		BIT(7)
#define MT2701_MUTEX_MOD_DISP_BLS		BIT(9)
#define MT2701_MUTEX_MOD_DISP_RDMA0		BIT(10)
#define MT2701_MUTEX_MOD_DISP_RDMA1		BIT(12)

#define MUTEX_SOF_SINGLE_MODE		0
#define MUTEX_SOF_DSI0			1
#define MUTEX_SOF_DSI1			2
#define MUTEX_SOF_DPI0			3

#define MT8167_MUTEX_SOF_SINGLE_MODE	0
#define MT8167_MUTEX_SOF_DSI0			1
#define MT8167_MUTEX_SOF_DPI0			2
#define MT8167_MUTEX_SOF_DPI1			3

#define OVL0_MOUT_EN_COLOR0		0x1
#define OVL0_MOUT_EN_WDMA0		0x2
#define OD_MOUT_EN_RDMA0		0x1
#define UFOE_MOUT_EN_DSI0		0x1
#define COLOR0_SEL_IN_OVL0		0x1
#define WDMA0_SEL_IN_OVL0		0x0
#define WDMA0_SEL_IN_DITHER		0x1
#define WDMA0_SEL_IN_UFOE		0x2
#define OVL1_MOUT_EN_COLOR1		0x1
#define GAMMA_MOUT_EN_RDMA1		0x1
#define RDMA1_MOUT_DPI0			0x2
#if 1 /* 8167 */
#define DPI0_SEL_IN_UFOE		0x0
#define DPI0_SEL_IN_RDMA0		0x1
#define DPI0_SEL_IN_RDMA1		0x2
#else
#define DPI0_SEL_IN_RDMA1		0x1
#endif
#define COLOR1_SEL_IN_OVL1		0x1

#define OVL_MOUT_EN_RDMA		0x1
#define BLS_TO_DSI_RDMA1_TO_DPI1	0x8
#define BLS_TO_DPI_RDMA1_TO_DSI		0x2
#define DSI_SEL_IN_BLS			0x0
#define DPI_SEL_IN_BLS			0x0
#define DSI_SEL_IN_RDMA			0x1

#define COLOR0_SEL_IN_RDMA0		0x0

#define DITHER_MOUT_EN_RDMA		0x1
#define DITHER_MOUT_EN_UFOE		0x2
#define DITHER_MOUT_EN_WDMA		0x4

#define UFOE_SEL_IN_RDMA0		0x0
#define UFOE_SEL_IN_DITHER		0x1
#define DSI0_SEL_IN_UFOE		0x0
#define DSI0_SEL_IN_RDMA0		0x1
#define DSI0_SEL_IN_RDMA1		0x2

#define DPI1_SEL_IN_UFOE		0x0
#define DPI1_SEL_IN_RDMA0		0x1
#define DPI1_SEL_IN_RDMA1		0x2

#define RDMA0_SOUT_SEL_IN_UFOE		0x0
#define RDMA0_SOUT_SEL_IN_COLOR0	0x1
#define RDMA0_SOUT_SEL_IN_DSI0		0x2
#define RDMA0_SOUT_SEL_IN_DPI0		0x3
#define RDMA0_SOUT_SEL_IN_DPI1		0x4

#define RDMA1_SOUT_SEL_IN_DSI0		0x0
#define RDMA1_SOUT_SEL_IN_DPI0		0x1
#define RDMA1_SOUT_SEL_IN_DPI1		0x2

struct mtk_ddp {
	struct device			*dev;
	struct clk			*clk;
	void __iomem			*regs;
	struct mtk_disp_mutex		mutex[10];
	const struct mtk_disp_ddp_data	*data;
};

enum mtk_ddp_mutex_sof_id {
	DDP_MUTEX_SOF_SINGLE_MODE,
	DDP_MUTEX_SOF_DSI0,
	DDP_MUTEX_SOF_DSI1,
	DDP_MUTEX_SOF_DPI0,
	DDP_MUTEX_SOF_DPI1,
	DDP_MUTEX_SOF_DSI2,
	DDP_MUTEX_SOF_DSI3,
	DDP_MUTEX_SOF_MAX,
};

struct mtk_disp_ddp_data {
	const unsigned int *mutex_mod;
	const unsigned int *mutex_sof;
};

static const unsigned int mt2701_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_BLS] = MT2701_MUTEX_MOD_DISP_BLS,
	[DDP_COMPONENT_COLOR0] = MT2701_MUTEX_MOD_DISP_COLOR,
	[DDP_COMPONENT_OVL0] = MT2701_MUTEX_MOD_DISP_OVL,
	[DDP_COMPONENT_RDMA0] = MT2701_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_RDMA1] = MT2701_MUTEX_MOD_DISP_RDMA1,
	[DDP_COMPONENT_WDMA0] = MT2701_MUTEX_MOD_DISP_WDMA,
};

static const unsigned int mt8167_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL] = MT8167_MUTEX_MOD_DISP_AAL,
	[DDP_COMPONENT_CCORR0] = MT8167_MUTEX_MOD_DISP_CCORR,
	[DDP_COMPONENT_COLOR0] = MT8167_MUTEX_MOD_DISP_COLOR,
	[DDP_COMPONENT_DITHER] = MT8167_MUTEX_MOD_DISP_DITHER,
	[DDP_COMPONENT_GAMMA] = MT8167_MUTEX_MOD_DISP_GAMMA,
	[DDP_COMPONENT_OVL0] = MT8167_MUTEX_MOD_DISP_OVL0,
	[DDP_COMPONENT_PWM0] = MT8167_MUTEX_MOD_DISP_PWM,
	[DDP_COMPONENT_RDMA0] = MT8167_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_RDMA1] = MT8167_MUTEX_MOD_DISP_RDMA1,
	[DDP_COMPONENT_UFOE] = MT8167_MUTEX_MOD_DISP_UFOE,
	[DDP_COMPONENT_WDMA0] = MT8167_MUTEX_MOD_DISP_WDMA0,
};

static const unsigned int mt8173_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL] = MT8173_MUTEX_MOD_DISP_AAL,
	[DDP_COMPONENT_COLOR0] = MT8173_MUTEX_MOD_DISP_COLOR0,
	[DDP_COMPONENT_COLOR1] = MT8173_MUTEX_MOD_DISP_COLOR1,
	[DDP_COMPONENT_GAMMA] = MT8173_MUTEX_MOD_DISP_GAMMA,
	[DDP_COMPONENT_OD] = MT8173_MUTEX_MOD_DISP_OD,
	[DDP_COMPONENT_OVL0] = MT8173_MUTEX_MOD_DISP_OVL0,
	[DDP_COMPONENT_OVL1] = MT8173_MUTEX_MOD_DISP_OVL1,
	[DDP_COMPONENT_PWM0] = MT8173_MUTEX_MOD_DISP_PWM0,
	[DDP_COMPONENT_PWM1] = MT8173_MUTEX_MOD_DISP_PWM1,
	[DDP_COMPONENT_RDMA0] = MT8173_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_RDMA1] = MT8173_MUTEX_MOD_DISP_RDMA1,
	[DDP_COMPONENT_RDMA2] = MT8173_MUTEX_MOD_DISP_RDMA2,
	[DDP_COMPONENT_UFOE] = MT8173_MUTEX_MOD_DISP_UFOE,
	[DDP_COMPONENT_WDMA0] = MT8173_MUTEX_MOD_DISP_WDMA0,
	[DDP_COMPONENT_WDMA1] = MT8173_MUTEX_MOD_DISP_WDMA1,
};

static const unsigned int mt2701_mutex_sof[DDP_MUTEX_SOF_MAX] = {
	[DDP_MUTEX_SOF_SINGLE_MODE] = MUTEX_SOF_SINGLE_MODE,
	[DDP_MUTEX_SOF_DSI0] = MUTEX_SOF_DSI0,
	[DDP_MUTEX_SOF_DSI1] = MUTEX_SOF_DSI1,
	[DDP_MUTEX_SOF_DPI0] = MUTEX_SOF_DPI0,
};

static const unsigned int mt8167_mutex_sof[DDP_MUTEX_SOF_MAX] = {
	[DDP_MUTEX_SOF_SINGLE_MODE] = MT8167_MUTEX_SOF_SINGLE_MODE,
	[DDP_MUTEX_SOF_DSI0] = MT8167_MUTEX_SOF_DSI0,
	[DDP_MUTEX_SOF_DSI1] = MT8167_MUTEX_SOF_DSI0,
	[DDP_MUTEX_SOF_DPI0] = MT8167_MUTEX_SOF_DPI0,
	[DDP_MUTEX_SOF_DPI1] = MT8167_MUTEX_SOF_DPI1,
	[DDP_MUTEX_SOF_DSI2] = MT8167_MUTEX_SOF_DSI0,
	[DDP_MUTEX_SOF_DSI3] = MT8167_MUTEX_SOF_DSI0,
};

static const unsigned int mt8173_mutex_sof[DDP_MUTEX_SOF_MAX] = {
	[DDP_MUTEX_SOF_SINGLE_MODE] = MUTEX_SOF_SINGLE_MODE,
	[DDP_MUTEX_SOF_DSI0] = MUTEX_SOF_DSI0,
	[DDP_MUTEX_SOF_DSI1] = MUTEX_SOF_DSI1,
	[DDP_MUTEX_SOF_DPI0] = MUTEX_SOF_DPI0,
};

static const struct mtk_disp_ddp_data mt2701_ddp_driver_data = {
	.mutex_mod = mt2701_mutex_mod,
	.mutex_sof = mt2701_mutex_sof,
};

static const struct mtk_disp_ddp_data mt8173_ddp_driver_data = {
	.mutex_mod = mt8173_mutex_mod,
	.mutex_sof = mt8173_mutex_sof,
};

static const struct mtk_disp_ddp_data mt8167_ddp_driver_data = {
	.mutex_mod = mt8167_mutex_mod,
	.mutex_sof = mt8167_mutex_sof,
};

static unsigned int mtk_ddp_mout_en(enum mtk_ddp_comp_id cur,
				    enum mtk_ddp_comp_id next,
				    unsigned int *addr)
{
	unsigned int value;

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_COLOR0) {
		*addr = DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
		value = OVL0_MOUT_EN_COLOR0;
	} else if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
		value = OVL0_MOUT_EN_WDMA0;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_RDMA0) {
		*addr = DISP_REG_CONFIG_DISP_DITHER_MOUT_EN;
		value = DITHER_MOUT_EN_RDMA;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_UFOE) {
		*addr = DISP_REG_CONFIG_DISP_DITHER_MOUT_EN;
		value = DITHER_MOUT_EN_UFOE;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_DITHER_MOUT_EN;
		value = DITHER_MOUT_EN_WDMA;
	} else if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_RDMA0) {
		*addr = DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
		value = OVL_MOUT_EN_RDMA;
	} else if (cur == DDP_COMPONENT_OD && next == DDP_COMPONENT_RDMA0) {
		*addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD_MOUT_EN_RDMA0;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DISP_UFOE_MOUT_EN;
		value = UFOE_MOUT_EN_DSI0;
	} else if (cur == DDP_COMPONENT_OVL1 && next == DDP_COMPONENT_COLOR1) {
		*addr = DISP_REG_CONFIG_DISP_OVL1_MOUT_EN;
		value = OVL1_MOUT_EN_COLOR1;
	} else if (cur == DDP_COMPONENT_GAMMA && next == DDP_COMPONENT_RDMA1) {
		*addr = DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN;
		value = GAMMA_MOUT_EN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DISP_RDMA1_MOUT_EN;
		value = RDMA1_MOUT_DPI0;
	} else {
		value = 0;
	}

	return value;
}

static unsigned int mtk_ddp_sel_in(enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next,
				   unsigned int *addr)
{
	unsigned int value;

	if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_COLOR0) {
		*addr = DISP_REG_CONFIG_DISP_COLOR0_SEL_IN;
		value = COLOR0_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_COLOR0) {
		*addr = DISP_REG_CONFIG_DISP_COLOR0_SEL_IN;
		value = COLOR0_SEL_IN_OVL0;
	} else if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_WDMA_SEL_IN;
		value = WDMA0_SEL_IN_OVL0;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_WDMA_SEL_IN;
		value = WDMA0_SEL_IN_DITHER;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_WDMA_SEL_IN;
		value = WDMA0_SEL_IN_UFOE;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DPI0_SEL_IN;
		value = DPI0_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_OVL1 && next == DDP_COMPONENT_COLOR1) {
		*addr = DISP_REG_CONFIG_DISP_COLOR1_SEL_IN;
		value = COLOR1_SEL_IN_OVL1;
	} else if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSI0_SEL_IN;
		value = DSI_SEL_IN_BLS;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_UFOE) {
		*addr = DISP_REG_CONFIG_DISP_UFOE_SEL_IN;
		value = UFOE_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_DITHER && next == DDP_COMPONENT_UFOE) {
		*addr = DISP_REG_CONFIG_DISP_UFOE_SEL_IN;
		value = UFOE_SEL_IN_DITHER;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSI0_SEL_IN;
		value = DSI0_SEL_IN_UFOE;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSI0_SEL_IN;
		value = DSI0_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSI0_SEL_IN;
		value = DSI0_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DPI0_SEL_IN;
		value = DPI0_SEL_IN_UFOE;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DPI0_SEL_IN;
		value = DPI0_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DPI1_SEL_IN;
		value = DPI1_SEL_IN_UFOE;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DPI1_SEL_IN;
		value = DPI1_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DPI1_SEL_IN;
		value = DPI1_SEL_IN_RDMA1;
	} else {
		value = 0;
	}

	return value;
}

static void mtk_ddp_sout_sel(void __iomem *config_regs,
			    enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next)
{
	if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DSI0) {
		writel_relaxed(BLS_TO_DSI_RDMA1_TO_DPI1,
			       config_regs + DISP_REG_CONFIG_OUT_SEL);
	} else if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DPI0) {
		writel_relaxed(BLS_TO_DPI_RDMA1_TO_DSI,
			       config_regs + DISP_REG_CONFIG_OUT_SEL);
		writel_relaxed(DSI_SEL_IN_RDMA,
			       config_regs + DISP_REG_CONFIG_DSI0_SEL_IN);
		writel_relaxed(DPI_SEL_IN_BLS,
			       config_regs + DISP_REG_CONFIG_DPI0_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_UFOE) {
		writel_relaxed(RDMA0_SOUT_SEL_IN_UFOE,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_COLOR0) {
		writel_relaxed(RDMA0_SOUT_SEL_IN_COLOR0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI0) {
		writel_relaxed(RDMA0_SOUT_SEL_IN_DSI0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI0) {
		writel_relaxed(RDMA0_SOUT_SEL_IN_DPI0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI1) {
		writel_relaxed(RDMA0_SOUT_SEL_IN_DPI1,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI0) {
		writel_relaxed(RDMA1_SOUT_SEL_IN_DSI0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		writel_relaxed(RDMA1_SOUT_SEL_IN_DPI0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI1) {
		writel_relaxed(RDMA1_SOUT_SEL_IN_DPI1,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA1_SOUT_SEL_IN);
	}
}

void mtk_ddp_add_comp_to_path(void __iomem *config_regs,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next)
{
	unsigned int addr, value, reg;

	value = mtk_ddp_mout_en(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) | value;
		writel_relaxed(reg, config_regs + addr);
	}

	mtk_ddp_sout_sel(config_regs, cur, next);

	value = mtk_ddp_sel_in(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) | value;
		writel_relaxed(reg, config_regs + addr);
	}
}

void mtk_ddp_remove_comp_from_path(void __iomem *config_regs,
				   enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next)
{
	unsigned int addr, value, reg;

	value = mtk_ddp_mout_en(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) & ~value;
		writel_relaxed(reg, config_regs + addr);
	}

	value = mtk_ddp_sel_in(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) & ~value;
		writel_relaxed(reg, config_regs + addr);
	}
}

struct mtk_disp_mutex *mtk_disp_mutex_get(struct device *dev, unsigned int id)
{
	struct mtk_ddp *ddp = dev_get_drvdata(dev);

	if (id >= 10)
		return ERR_PTR(-EINVAL);
	if (ddp->mutex[id].claimed)
		return ERR_PTR(-EBUSY);

	ddp->mutex[id].claimed = true;

	return &ddp->mutex[id];
}

void mtk_disp_mutex_put(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	mutex->claimed = false;
}

int mtk_disp_mutex_prepare(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	int ret;

	ret = pm_runtime_get_sync(ddp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);

	if (IS_ERR(ddp->clk)) {
		dev_err(ddp->dev, "Failed to prepare mutex clock\n");
		return 0;
	}

	return clk_prepare_enable(ddp->clk);
}

void mtk_disp_mutex_unprepare(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	int ret;

	if (IS_ERR(ddp->clk)) {
		dev_err(ddp->dev, "Failed to unprepare mutex clock\n");
		return;
	}

	clk_disable_unprepare(ddp->clk);

	ret = pm_runtime_put(ddp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);
}

void mtk_disp_mutex_add_comp(struct mtk_disp_mutex *mutex,
			     enum mtk_ddp_comp_id id)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	unsigned int reg;

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	switch (id) {
	case DDP_COMPONENT_DSI0:
		reg = DDP_MUTEX_SOF_DSI0;
		break;
	case DDP_COMPONENT_DSI1:
		reg = DDP_MUTEX_SOF_DSI1;
		break;
	case DDP_COMPONENT_DPI0:
		reg = DDP_MUTEX_SOF_DPI0;
		break;
	case DDP_COMPONENT_DPI1:
		reg = DDP_MUTEX_SOF_DPI1;
		break;
	default:
		reg = readl_relaxed(ddp->regs + DISP_REG_MUTEX_MOD(mutex->id));
		reg |= ddp->data->mutex_mod[id];
		writel_relaxed(reg, ddp->regs + DISP_REG_MUTEX_MOD(mutex->id));
		return;
	}

	writel_relaxed(ddp->data->mutex_sof[reg],
	ddp->regs + DISP_REG_MUTEX_SOF(mutex->id));
}

void mtk_disp_mutex_remove_comp(struct mtk_disp_mutex *mutex,
				enum mtk_ddp_comp_id id)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	unsigned int reg;

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	switch (id) {
	case DDP_COMPONENT_DSI0:
	case DDP_COMPONENT_DSI1:
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
		writel_relaxed(MUTEX_SOF_SINGLE_MODE,
			       ddp->regs + DISP_REG_MUTEX_SOF(mutex->id));
		break;
	default:
		reg = readl_relaxed(ddp->regs + DISP_REG_MUTEX_MOD(mutex->id));
		reg &= ~(ddp->data->mutex_mod[id]);
		writel_relaxed(reg, ddp->regs + DISP_REG_MUTEX_MOD(mutex->id));
		break;
	}
}

void mtk_disp_mutex_enable(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	writel(1, ddp->regs + DISP_REG_MUTEX_EN(mutex->id));
}

void mtk_disp_mutex_disable(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	writel(0, ddp->regs + DISP_REG_MUTEX_EN(mutex->id));
}

void mtk_disp_mutex_acquire(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	u32 tmp;

	writel(1, ddp->regs + DISP_REG_MUTEX_EN(mutex->id));
	writel(1, ddp->regs + DISP_REG_MUTEX(mutex->id));
	readl_poll_timeout_atomic(ddp->regs + DISP_REG_MUTEX(mutex->id), tmp,
				  tmp & 0x2, 1, 10000);
}

void mtk_disp_mutex_release(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	writel(0, ddp->regs + DISP_REG_MUTEX(mutex->id));
}

static int mtk_ddp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ddp *ddp;
	struct resource *regs;
	int i;

	MTK_DRM_DEBUG(dev, "\n");

	ddp = devm_kzalloc(dev, sizeof(*ddp), GFP_KERNEL);
	if (!ddp)
		return -ENOMEM;

	for (i = 0; i < 10; i++)
		ddp->mutex[i].id = i;

	ddp->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ddp->clk)) {
		dev_err(dev, "Failed to get clock\n");
		/* return PTR_ERR(ddp->clk); */
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ddp->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(ddp->regs)) {
		dev_err(dev, "Failed to map mutex registers\n");
		return PTR_ERR(ddp->regs);
	}

	ddp->data = of_device_get_match_data(dev);
	ddp->dev = dev;

	platform_set_drvdata(pdev, ddp);

	pm_runtime_enable(dev);

	MTK_DRM_DEBUG(dev, "ddp mutex reg base 0x%p\n", ddp->regs);

	return 0;
}

static int mtk_ddp_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id ddp_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-mutex",
	.data = &mt2701_ddp_driver_data},
	{ .compatible = "mediatek,mt8167-disp-mutex",
	.data = &mt8167_ddp_driver_data},
	{ .compatible = "mediatek,mt8173-disp-mutex",
	.data = &mt8167_ddp_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, ddp_driver_dt_match);

struct platform_driver mtk_ddp_driver = {
	.probe		= mtk_ddp_probe,
	.remove		= mtk_ddp_remove,
	.driver		= {
		.name	= "mediatek-ddp",
		.owner	= THIS_MODULE,
		.of_match_table = ddp_driver_dt_match,
	},
};
