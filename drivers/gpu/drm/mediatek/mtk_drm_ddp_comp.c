/*
 * Copyright (c) 2015 MediaTek Inc.
 * Authors:
 *	YT Shen <yt.shen@mediatek.com>
 *	CK Hu <ck.hu@mediatek.com>
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

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <drm/drmP.h>
#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#include <linux/soc/mediatek/mtk-cmdq.h>
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_gem.h"


#define DISP_OD_EN				0x0000
#define DISP_OD_INTEN				0x0008
#define DISP_OD_INTSTA				0x000c
#define DISP_OD_CFG				0x0020
#define DISP_OD_SIZE				0x0030

#define DISP_REG_UFO_START			0x0000

#define DISP_COLOR_CFG_MAIN			0x0400

#define DISP_REG_WDMA_INTSTA			0x0004
#define DISP_REG_WDMA_EN			0x0008
#define WDMA_EN					BIT(0)
#define DISP_REG_WDMA_RST			0x000c
#define DISP_REG_WDMA_CFG			0x0014
#define DISP_REG_WDMA_SRC_SIZE			0x0018
#define DISP_REG_WDMA_CLIP_SIZE			0x001c
#define DISP_REG_WDMA_DST_WIN_BYTE		0x0028
#define DISP_REG_WDMA_DST_ADDR			0x0f00
#define DISP_REG_WDMA_RGB888			0X10

#define	OD_RELAY_MODE		BIT(0)

#define	UFO_BYPASS		BIT(2)

#define	COLOR_BYPASS_ALL	BIT(7)
#define	COLOR_SEQ_SEL		BIT(13)

#define DISP_COLOR_START(comp)			((comp)->data->color_offset)
#define DISP_COLOR_WIDTH(comp)			(DISP_COLOR_START(comp) + 0x50)
#define DISP_COLOR_HEIGHT(comp)			(DISP_COLOR_START(comp) + 0x54)

void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
		   unsigned int offset, void *handle)
{
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cmdq_pkt_write((struct cmdq_pkt *)handle, value, comp->cmdq_base,
		       offset);
#else
	writel(value, comp->regs + offset);
#endif
}

void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			   unsigned int offset, void *handle)
{
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cmdq_pkt_write((struct cmdq_pkt *)handle, value, comp->cmdq_base,
		       offset);
#else
	writel_relaxed(value, comp->regs + offset);
#endif
}

void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle)
{
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cmdq_pkt_write_mask((struct cmdq_pkt *)handle,
			    value, comp->cmdq_base, offset, mask);
#else
	unsigned int tmp = readl(comp->regs + offset);

	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, comp->regs + offset);
#endif
}

static void mtk_color_config(struct mtk_ddp_comp *comp, unsigned int w,
			     unsigned int h, unsigned int vrefresh, void *handle)
{
	mtk_ddp_write(comp, w, DISP_COLOR_WIDTH(comp), handle);
	mtk_ddp_write(comp, h, DISP_COLOR_HEIGHT(comp), handle);
}

static void mtk_color_start(struct mtk_ddp_comp *comp, void *handle)
{
	mtk_ddp_write(comp, COLOR_BYPASS_ALL | COLOR_SEQ_SEL,
	       DISP_COLOR_CFG_MAIN, handle);
	mtk_ddp_write(comp, 0x1, DISP_COLOR_START(comp), handle);
}

static void mtk_od_config(struct mtk_ddp_comp *comp, unsigned int w,
			  unsigned int h, unsigned int vrefresh, void *handle)
{
	mtk_ddp_write(comp, w << 16 | h, DISP_OD_SIZE, handle);
}

static void mtk_od_start(struct mtk_ddp_comp *comp, void *handle)
{
	mtk_ddp_write(comp, OD_RELAY_MODE, DISP_OD_CFG, handle);
	mtk_ddp_write(comp, 1, DISP_OD_EN, handle);
}

static void mtk_ufoe_start(struct mtk_ddp_comp *comp, void *handle)
{
	mtk_ddp_write(comp, UFO_BYPASS, DISP_REG_UFO_START, handle);
}

static void mtk_ccorr_start(struct mtk_ddp_comp *comp, void *handle)
{
}

static void mtk_dither_start(struct mtk_ddp_comp *comp, void *handle)
{
}

static void mtk_wdma_config(struct mtk_ddp_comp *comp, unsigned int w,
			    unsigned int h, unsigned int vrefresh, void *handle)
{
	unsigned int size;

	if(!comp->mtk_gem)
		drm_err("wdma gem is invalid\n");

	mtk_ddp_write_mask(comp, DISP_REG_WDMA_RGB888,
			   DISP_REG_WDMA_CFG, 0xf0, handle);

	size = (w & 0x3FFFU) + ((h  << 16U) & 0x3FFF0000U);
	mtk_ddp_write(comp, size, DISP_REG_WDMA_SRC_SIZE, handle);
	mtk_ddp_write(comp, size, DISP_REG_WDMA_CLIP_SIZE, handle);
	mtk_ddp_write(comp, w * 3UL, DISP_REG_WDMA_DST_WIN_BYTE, handle);
	mtk_ddp_write(comp, (u32)(comp->mtk_gem?comp->mtk_gem->dma_addr:0) & 0xFFFFFFFFU,
		      DISP_REG_WDMA_DST_ADDR, handle);
}

static void mtk_wdma_start(struct mtk_ddp_comp *comp, void *handle)
{
	mtk_ddp_write(comp, WDMA_EN, DISP_REG_WDMA_EN, handle);
}

static void mtk_wdma_stop(struct mtk_ddp_comp *comp, void *handle)
{
	mtk_ddp_write(comp, 0x0, DISP_REG_WDMA_EN, handle);
	mtk_ddp_write(comp, 0x0, DISP_REG_WDMA_INTSTA, handle);
}

static const struct mtk_ddp_comp_funcs ddp_color = {
	.config = mtk_color_config,
	.start = mtk_color_start,
};

static const struct mtk_ddp_comp_funcs ddp_od = {
	.config = mtk_od_config,
	.start = mtk_od_start,
};

static const struct mtk_ddp_comp_funcs ddp_ufoe = {
	.start = mtk_ufoe_start,
};

static const struct mtk_ddp_comp_funcs ddp_ccorr = {
	.start = mtk_ccorr_start,
};

static const struct mtk_ddp_comp_funcs ddp_dither = {
	.start = mtk_dither_start,
};

static const struct mtk_ddp_comp_funcs ddp_wdma = {
	.config = mtk_wdma_config,
	.start = mtk_wdma_start,
	.stop = mtk_wdma_stop,
};

static const char * const mtk_ddp_comp_stem[MTK_DDP_COMP_TYPE_MAX] = {
	[MTK_DISP_OVL] = "ovl",
	[MTK_DISP_RDMA] = "rdma",
	[MTK_DISP_WDMA] = "wdma",
	[MTK_DISP_COLOR] = "color",
	[MTK_DISP_AAL] = "aal",
	[MTK_DISP_GAMMA] = "gamma",
	[MTK_DISP_UFOE] = "ufoe",
	[MTK_DSI] = "dsi",
	[MTK_DPI] = "dpi",
	[MTK_DISP_PWM] = "pwm",
	[MTK_DISP_MUTEX] = "mutex",
	[MTK_DISP_OD] = "od",
	[MTK_DISP_BLS] = "bls",
	[MTK_DISP_CCORR] = "ccorr",
	[MTK_DISP_DITHER] = "dither",
};

struct mtk_ddp_comp_match {
	enum mtk_ddp_comp_type type;
	int alias_id;
	const struct mtk_ddp_comp_funcs *funcs;
};

static const struct mtk_ddp_comp_match mtk_ddp_matches[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL]	= { MTK_DISP_AAL,	0, NULL },
	[DDP_COMPONENT_BLS]	= { MTK_DISP_BLS,	0, NULL },
	[DDP_COMPONENT_CCORR0]  = { MTK_DISP_CCORR,	0, &ddp_ccorr },
	[DDP_COMPONENT_COLOR0]	= { MTK_DISP_COLOR,	0, &ddp_color },
	[DDP_COMPONENT_COLOR1]	= { MTK_DISP_COLOR,	1, &ddp_color },
	[DDP_COMPONENT_DITHER]	= {	MTK_DISP_DITHER,0, &ddp_dither },
	[DDP_COMPONENT_DPI0]	= { MTK_DPI,		0, NULL },
	[DDP_COMPONENT_DPI1]	= { MTK_DPI,		1, NULL },
	[DDP_COMPONENT_DSI0]	= { MTK_DSI,		0, NULL },
	[DDP_COMPONENT_DSI1]	= { MTK_DSI,		1, NULL },
	[DDP_COMPONENT_GAMMA]	= { MTK_DISP_GAMMA,	0, NULL },
	[DDP_COMPONENT_OD]		= { MTK_DISP_OD,	0, &ddp_od },
	[DDP_COMPONENT_OVL0]	= { MTK_DISP_OVL,	0, NULL },
	[DDP_COMPONENT_OVL1]	= { MTK_DISP_OVL,	1, NULL },
	[DDP_COMPONENT_PWM0]	= { MTK_DISP_PWM,	0, NULL },
	[DDP_COMPONENT_RDMA0]	= { MTK_DISP_RDMA,	0, NULL },
	[DDP_COMPONENT_RDMA1]	= { MTK_DISP_RDMA,	1, NULL },
	[DDP_COMPONENT_RDMA2]	= { MTK_DISP_RDMA,	2, NULL },
	[DDP_COMPONENT_UFOE]	= { MTK_DISP_UFOE,	0, &ddp_ufoe },
	[DDP_COMPONENT_WDMA0]	= { MTK_DISP_WDMA,	0, &ddp_wdma },
	[DDP_COMPONENT_WDMA1]	= { MTK_DISP_WDMA,	1, NULL },
};

static const struct mtk_ddp_comp_driver_data mt2701_color_driver_data = {
	.color_offset = 0x0f00,
};

static const struct mtk_ddp_comp_driver_data mt8167_color_driver_data = {
	.color_offset = 0x0c00,
};

static const struct mtk_ddp_comp_driver_data mt8173_color_driver_data = {
	.color_offset = 0x0c00,
};

static const struct of_device_id mtk_disp_color_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-color",
	  .data = &mt2701_color_driver_data},
	{ .compatible = "mediatek,mt8167-disp-color",
	  .data = &mt8167_color_driver_data},
	{ .compatible = "mediatek,mt8173-disp-color",
	  .data = &mt8173_color_driver_data},
	{},
};

static bool mtk_drm_find_comp_in_ddp(struct mtk_ddp_comp ddp_comp,
				     const enum mtk_ddp_comp_id *path,
				     unsigned int path_len)
{
	unsigned int i;

	if (path == NULL)
		return false;

	for (i = 0U; i < path_len; i++)
		if (ddp_comp.id == path[i])
			return true;

	return false;
}

int mtk_ddp_comp_get_id(struct device_node *node,
			enum mtk_ddp_comp_type comp_type)
{
	int id = of_alias_get_id(node, mtk_ddp_comp_stem[comp_type]);
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_ddp_matches); i++) {
		if (comp_type == mtk_ddp_matches[i].type &&
		    (id < 0 || id == mtk_ddp_matches[i].alias_id))
			return i;
	}

	return -EINVAL;
}

struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id)
{
	unsigned int i;
	struct mtk_drm_crtc *mtk_crtc = container_of(crtc,
						     struct mtk_drm_crtc, base);

	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		if (comp_id == mtk_crtc->ddp_comp[i]->id)
			return mtk_crtc->ddp_comp[i];
	}

	return NULL;
}

unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp)
{
	struct mtk_drm_private *private = drm->dev_private;
	unsigned int ret;

	if (mtk_drm_find_comp_in_ddp(ddp_comp, private->data->main_path,
				     private->data->main_len) == true) {
		ret = BIT(0);
	} else if (mtk_drm_find_comp_in_ddp(ddp_comp,
					    private->data->ext_path,
					    private->data->ext_len) == true) {
		ret = BIT(1);
	}
	/*
	else if (mtk_drm_find_comp_in_ddp(ddp_comp,
					    private->data->third_path,
					    private->data->third_len) == true) {
		ret = BIT(2);
	}
	*/
	else {
		drm_err("Failed to find comp in ddp table\n");
		ret = 0;
	}

	return ret;
}

int mtk_ddp_comp_init(struct device *dev, struct device_node *node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs)
{
	enum mtk_ddp_comp_type type;
	struct device_node *larb_node;
	struct platform_device *larb_pdev;
	struct platform_device *comp_pdev;
	const struct of_device_id *match;

	if (comp_id < 0 || comp_id >= DDP_COMPONENT_ID_MAX) {
		dev_err(dev,
			"Failed to comp init %s node, comp_id %d error\n",
			node->full_name, comp_id);
		return -EINVAL;
	}

	comp->id = comp_id;
	comp->funcs = funcs ?: mtk_ddp_matches[comp_id].funcs;

	if (comp_id == DDP_COMPONENT_DPI0 ||
	    comp_id == DDP_COMPONENT_DSI0 ||
	    comp_id == DDP_COMPONENT_PWM0) {
		comp->regs = NULL;
		comp->clk = NULL;
		comp->irq = 0;
		return 0;
	}

	comp->regs = of_iomap(node, 0);
	comp->irq = of_irq_get(node, 0);
	comp->clk = of_clk_get(node, 0);

	comp_pdev = of_find_device_by_node(node);
	if (!comp_pdev) {
		dev_err(dev, "Waiting for device %s\n",
			 node->full_name);
		return -EPROBE_DEFER;
	}
	comp->dev = &comp_pdev->dev;

	if (IS_ERR(comp->clk)) {
		dev_err(dev,
			"Failed to comp init %s node, comp_id %d clk error\n",
			node->full_name, comp_id);
		return PTR_ERR(comp->clk);
	}

	type = mtk_ddp_matches[comp_id].type;

	if (type == MTK_DISP_COLOR) {
		match = of_match_node(mtk_disp_color_driver_dt_match, node);
		comp->data = match->data;
	}

	/* Only DMA capable components need the LARB property */
	comp->larb_dev = NULL;
	if (type != MTK_DISP_OVL &&
	    type != MTK_DISP_RDMA &&
	    type != MTK_DISP_WDMA)
		return 0;

	larb_node = of_parse_phandle(node, "mediatek,larb", 0);
	if (!larb_node) {
		dev_err(dev,
			"Missing mediadek,larb phandle in %s node\n",
			node->full_name);
		return -EINVAL;
	}

	larb_pdev = of_find_device_by_node(larb_node);
	if (!larb_pdev || !larb_pdev->dev.driver) {
		dev_warn(dev, "Waiting for larb device %s\n",
			 larb_node->full_name);
		of_node_put(larb_node);
		return -EPROBE_DEFER;
	}
	of_node_put(larb_node);

	comp->larb_dev = &larb_pdev->dev;

	MTK_DRM_DEBUG(dev,
	"comp init %s node, id %d reg 0x%p, irq %d larb_dev %s dev %s\n",
	node->full_name, comp_id, comp->regs, comp->irq,
	dev_name(comp->larb_dev),
	dev_name(comp->dev));

	return 0;
}

int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	if (private->ddp_comp[comp->id])
		return -EBUSY;

	private->ddp_comp[comp->id] = comp;
	return 0;
}

void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	private->ddp_comp[comp->id] = NULL;
}
