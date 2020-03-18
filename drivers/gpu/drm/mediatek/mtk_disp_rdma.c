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
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"

#define REG_FLD(width, shift) \
	((unsigned int)((((width) & 0xFF) << 16) | ((shift) & 0xFF)))

#define REG_FLD_WIDTH(field) \
	((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) \
	((unsigned int)((field) & 0xFF))

#define REG_FLD_MASK(field) \
	(((unsigned int)(1 << REG_FLD_WIDTH(field)) - 1) << REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val) \
	(((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define DISP_REG_RDMA_INT_ENABLE		0x0000
#define DISP_REG_RDMA_INT_STATUS		0x0004
#define RDMA_TARGET_LINE_INT				BIT(5)
#define RDMA_FIFO_UNDERFLOW_INT				BIT(4)
#define RDMA_EOF_ABNORMAL_INT				BIT(3)
#define RDMA_FRAME_END_INT				BIT(2)
#define RDMA_FRAME_START_INT				BIT(1)
#define RDMA_REG_UPDATE_INT				BIT(0)
#define DISP_REG_RDMA_GLOBAL_CON		0x0010
#define RDMA_ENGINE_EN					BIT(0)
#define RDMA_MODE_SEL					REG_FLD(1, 1)
#define DISP_REG_RDMA_SIZE_CON_0		0x0014
#define DISP_REG_RDMA_SIZE_CON_1		0x0018
#define DISP_REG_RDMA_TARGET_LINE		0x001c
#define DISP_REG_RDMA_MEM_CON			0x0024
#define MEM_MODE_INPUT_FORMAT			REG_FLD(4, 4)
#define MEM_MODE_INPUT_SWAP 			REG_FLD(1, 8)
#define DISP_REG_RDMA_MEM_SRC_PITCH     0x002C
#define DISP_REG_RDMA_FIFO_CON			0x0040
#define RDMA_FIFO_UNDERFLOW_EN				BIT(31)
#define RDMA_FIFO_PSEUDO_SIZE(bytes)			(((bytes) / 16) << 16)
#define RDMA_OUTPUT_VALID_FIFO_THRESHOLD(bytes)		((bytes) / 16)
#define RDMA_FIFO_SIZE(rdma)			((rdma)->data->fifo_size)

#define DISP_REG_RDMA_MEM_START_ADDR 0xf00

#define RDMA_INSTANCES  2
#define RDMA_MAX_WIDTH  4095
#define RDMA_MAX_HEIGHT 4095

enum RDMA_INPUT_FORMAT {
	RDMA_INPUT_FORMAT_BGR565 = 0,
	RDMA_INPUT_FORMAT_RGB888 = 1,
	RDMA_INPUT_FORMAT_RGBA8888 = 2,
	RDMA_INPUT_FORMAT_ARGB8888 = 3,
	RDMA_INPUT_FORMAT_VYUY = 4,
	RDMA_INPUT_FORMAT_YVYU = 5,

	RDMA_INPUT_FORMAT_RGB565 = 6,
	RDMA_INPUT_FORMAT_BGR888 = 7,
	RDMA_INPUT_FORMAT_BGRA8888 = 8,
	RDMA_INPUT_FORMAT_ABGR8888 = 9,
	RDMA_INPUT_FORMAT_UYVY = 10,
	RDMA_INPUT_FORMAT_YUYV = 11,

	RDMA_INPUT_FORMAT_UNKNOWN = 32,
};

enum RDMA_MODE {
	RDMA_MODE_DIRECT_LINK = 0,
	RDMA_MODE_MEMORY = 1,
};

struct mtk_disp_rdma_data {
	unsigned int fifo_size;
};

/**
 * struct mtk_disp_rdma - DISP_RDMA driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_rdma {
	struct mtk_ddp_comp		ddp_comp;
	struct drm_crtc			*crtc;
	const struct mtk_disp_rdma_data	*data;
};

static inline struct mtk_disp_rdma *comp_to_rdma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_rdma, ddp_comp);
}

static enum RDMA_INPUT_FORMAT rdma_input_format_convert(unsigned int fmt)
{
	enum RDMA_INPUT_FORMAT rdma_fmt = RDMA_INPUT_FORMAT_RGB565;

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		rdma_fmt = RDMA_INPUT_FORMAT_BGR565;
		break;
	case DRM_FORMAT_BGR565:
		rdma_fmt = RDMA_INPUT_FORMAT_RGB565;
		break;
	case DRM_FORMAT_VYUY:
		rdma_fmt = RDMA_INPUT_FORMAT_VYUY;
		break;
	case DRM_FORMAT_YVYU:
		rdma_fmt = RDMA_INPUT_FORMAT_YVYU;
		break;
	case DRM_FORMAT_UYVY:
		rdma_fmt = RDMA_INPUT_FORMAT_UYVY;
		break;
	case DRM_FORMAT_YUYV:
		rdma_fmt = RDMA_INPUT_FORMAT_YUYV;
		break;

	case DRM_FORMAT_RGB888:
		rdma_fmt = RDMA_INPUT_FORMAT_RGB888;
		break;
	case DRM_FORMAT_BGR888:
		rdma_fmt = RDMA_INPUT_FORMAT_BGR888;
		break;

	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
		rdma_fmt = RDMA_INPUT_FORMAT_ABGR8888;
		break;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		rdma_fmt = RDMA_INPUT_FORMAT_ARGB8888;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		rdma_fmt = RDMA_INPUT_FORMAT_BGRA8888;
		break;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		rdma_fmt = RDMA_INPUT_FORMAT_RGBA8888;
		break;

	default:
		DRM_ERROR("rdma_input_format_convert fmt=%d, rdma_fmt=%d\n",
		fmt, rdma_fmt);
	}
	return rdma_fmt;
}

static unsigned int rdma_input_format_bpp(enum RDMA_INPUT_FORMAT inputFormat)
{
	int bpp = 0;

	switch (inputFormat) {
	case RDMA_INPUT_FORMAT_BGR565:
	case RDMA_INPUT_FORMAT_RGB565:
	case RDMA_INPUT_FORMAT_VYUY:
	case RDMA_INPUT_FORMAT_UYVY:
	case RDMA_INPUT_FORMAT_YVYU:
	case RDMA_INPUT_FORMAT_YUYV:
		bpp = 2;
		break;
	case RDMA_INPUT_FORMAT_RGB888:
	case RDMA_INPUT_FORMAT_BGR888:
		bpp = 3;
		break;
	case RDMA_INPUT_FORMAT_ARGB8888:
	case RDMA_INPUT_FORMAT_ABGR8888:
	case RDMA_INPUT_FORMAT_RGBA8888:
	case RDMA_INPUT_FORMAT_BGRA8888:
		bpp = 4;
		break;
	default:
		DRM_ERROR("unknown RDMA input format = %d\n", inputFormat);
	}
	return bpp;
}

static unsigned int rdma_input_format_color_space(enum RDMA_INPUT_FORMAT inputFormat)
{
	int space = 0;

	switch (inputFormat) {
	case RDMA_INPUT_FORMAT_BGR565:
	case RDMA_INPUT_FORMAT_RGB565:
	case RDMA_INPUT_FORMAT_RGB888:
	case RDMA_INPUT_FORMAT_BGR888:
	case RDMA_INPUT_FORMAT_RGBA8888:
	case RDMA_INPUT_FORMAT_BGRA8888:
	case RDMA_INPUT_FORMAT_ARGB8888:
	case RDMA_INPUT_FORMAT_ABGR8888:
		space = 0;
		break;
	case RDMA_INPUT_FORMAT_VYUY:
	case RDMA_INPUT_FORMAT_UYVY:
	case RDMA_INPUT_FORMAT_YVYU:
	case RDMA_INPUT_FORMAT_YUYV:
		space = 1;
		break;
	default:
		DRM_ERROR("unknown RDMA input format = %d\n", inputFormat);
	}
	return space;
}

static unsigned int rdma_input_format_byte_swap(enum RDMA_INPUT_FORMAT inputFormat)
{
	int input_swap = 0;

	switch (inputFormat) {
	case RDMA_INPUT_FORMAT_BGR565:
	case RDMA_INPUT_FORMAT_RGB888:
	case RDMA_INPUT_FORMAT_RGBA8888:
	case RDMA_INPUT_FORMAT_ARGB8888:
	case RDMA_INPUT_FORMAT_VYUY:
	case RDMA_INPUT_FORMAT_YVYU:
		input_swap = 1;
		break;
	case RDMA_INPUT_FORMAT_RGB565:
	case RDMA_INPUT_FORMAT_BGR888:
	case RDMA_INPUT_FORMAT_BGRA8888:
	case RDMA_INPUT_FORMAT_ABGR8888:
	case RDMA_INPUT_FORMAT_UYVY:
	case RDMA_INPUT_FORMAT_YUYV:
		input_swap = 0;
		break;
	default:
		DRM_ERROR("unknown RDMA input format is %d\n", inputFormat);
	}
	return input_swap;
}

static unsigned int rdma_input_format_reg_value(enum RDMA_INPUT_FORMAT inputFormat)
{
	int reg_value = 0;

	switch (inputFormat) {
	case RDMA_INPUT_FORMAT_BGR565:
	case RDMA_INPUT_FORMAT_RGB565:
		reg_value = 0x0;
		break;
	case RDMA_INPUT_FORMAT_RGB888:
	case RDMA_INPUT_FORMAT_BGR888:
		reg_value = 0x1;
		break;
	case RDMA_INPUT_FORMAT_RGBA8888:
	case RDMA_INPUT_FORMAT_BGRA8888:
		reg_value = 0x2;
		break;
	case RDMA_INPUT_FORMAT_ARGB8888:
	case RDMA_INPUT_FORMAT_ABGR8888:
		reg_value = 0x3;
		break;
	case RDMA_INPUT_FORMAT_VYUY:
	case RDMA_INPUT_FORMAT_UYVY:
		reg_value = 0x4;
		break;
	case RDMA_INPUT_FORMAT_YVYU:
	case RDMA_INPUT_FORMAT_YUYV:
		reg_value = 0x5;
		break;
	default:
		DRM_ERROR("unknown RDMA input format is %d\n", inputFormat);
	}
	return reg_value;
}

static char *rdma_intput_format_name(enum RDMA_INPUT_FORMAT fmt, int swap)
{
	switch (fmt) {
	case RDMA_INPUT_FORMAT_BGR565:
		return swap ? "eBGR565" : "eRGB565";
	case RDMA_INPUT_FORMAT_RGB565:
		return "eRGB565";
	case RDMA_INPUT_FORMAT_RGB888:
		return swap ? "eRGB888" : "eBGR888";
	case RDMA_INPUT_FORMAT_BGR888:
		return "eBGR888";
	case RDMA_INPUT_FORMAT_RGBA8888:
		return swap ? "eRGBA888" : "eBGRA888";
	case RDMA_INPUT_FORMAT_BGRA8888:
		return "eBGRA888";
	case RDMA_INPUT_FORMAT_ARGB8888:
		return swap ? "eARGB8888" : "eABGR8888";
	case RDMA_INPUT_FORMAT_ABGR8888:
		return "eABGR8888";
	case RDMA_INPUT_FORMAT_VYUY:
		return swap ? "eVYUY" : "eUYVY";
	case RDMA_INPUT_FORMAT_UYVY:
		return "eUYVY";
	case RDMA_INPUT_FORMAT_YVYU:
		return swap ? "eYVYU" : "eYUY2";
	case RDMA_INPUT_FORMAT_YUYV:
		return "eYUY2";
	default:
		DRM_ERROR("rdma_intput_format_name unknown fmt=%d, swap=%d\n", fmt, swap);
		break;
	}
	return "unknown";
}

unsigned int rdma_index(enum mtk_ddp_comp_id module)
{
	int idx = 0;

	switch (module) {
	case DDP_COMPONENT_RDMA0:
		idx = 0;
		break;
	case DDP_COMPONENT_RDMA1:
		idx = 1;
		break;
	case DDP_COMPONENT_RDMA2:
		idx = 2;
		break;
	default:
		DRM_ERROR("invalid rdma module=%d\n", module);	/* invalid module */
	}
	return idx;
}

static irqreturn_t mtk_disp_rdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_rdma *priv = dev_id;
	struct mtk_ddp_comp *rdma = &priv->ddp_comp;

	MTK_DRM_DEBUG_VBL("rdma %d irq %d status 0x%X\n",
	rdma_index(rdma->id),
	irq,
	readl(rdma->regs + DISP_REG_RDMA_INT_STATUS));

	/* Clear frame completion interrupt */
	writel(0x0, rdma->regs + DISP_REG_RDMA_INT_STATUS);

	if (!priv->crtc) {
		MTK_DRM_DEBUG_VBL("crtc is invalid!\n");
		return IRQ_NONE;
	}

	mtk_crtc_ddp_irq(priv->crtc, rdma);

	return IRQ_HANDLED;
}

static void rdma_update_bits(struct mtk_ddp_comp *comp, unsigned int reg,
			     unsigned int mask, unsigned int val)
{
	unsigned int tmp = readl(comp->regs + reg);

	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, comp->regs + reg);
}

static void mtk_rdma_enable_vblank(struct mtk_ddp_comp *comp,
				   struct drm_crtc *crtc, void *handle)
{
	struct mtk_disp_rdma *priv = container_of(comp, struct mtk_disp_rdma,
						  ddp_comp);

	priv->crtc = crtc;

	MTK_DRM_DEBUG_DRIVER("rdma %d crtc 0x%p\n",
	rdma_index(priv->ddp_comp.id),
	crtc);

	rdma_update_bits(comp, DISP_REG_RDMA_INT_ENABLE, RDMA_FRAME_END_INT,
			 RDMA_FRAME_END_INT);
}

static void mtk_rdma_disable_vblank(struct mtk_ddp_comp *comp, void *handle)
{
	struct mtk_disp_rdma *priv = container_of(comp, struct mtk_disp_rdma,
						  ddp_comp);

	priv->crtc = NULL;
	rdma_update_bits(comp, DISP_REG_RDMA_INT_ENABLE, RDMA_FRAME_END_INT, 0);
}

static void mtk_rdma_start(struct mtk_ddp_comp *comp, void *handle)
{
	int ret;

	ret = pm_runtime_get_sync(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);

	mtk_ddp_write_mask(comp, RDMA_ENGINE_EN, DISP_REG_RDMA_GLOBAL_CON,
			 RDMA_ENGINE_EN, handle);
}

static void mtk_rdma_stop(struct mtk_ddp_comp *comp, void *handle)
{
	int ret;

	mtk_ddp_write_mask(comp, 0, DISP_REG_RDMA_GLOBAL_CON, RDMA_ENGINE_EN, handle);

	ret = pm_runtime_put(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);
}

static void mtk_rdma_config(struct mtk_ddp_comp *comp, unsigned int width,
			    unsigned int height, unsigned int vrefresh, void *handle)
{
	unsigned int threshold;
	unsigned int reg;
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);

	mtk_ddp_write_mask(comp, width, DISP_REG_RDMA_SIZE_CON_0, 0xfff, handle);
	mtk_ddp_write_mask(comp, height, DISP_REG_RDMA_SIZE_CON_1, 0xfffff, handle);

	/*
	 * Enable FIFO underflow since DSI and DPI can't be blocked.
	 * Keep the FIFO pseudo size reset default of 8 KiB. Set the
	 * output threshold to 6 microseconds with 7/6 overhead to
	 * account for blanking, and with a pixel depth of 4 bytes:
	 */
	threshold = width * height * vrefresh * 4 * 7 / 1000000;
	reg = RDMA_FIFO_UNDERFLOW_EN |
	      RDMA_FIFO_PSEUDO_SIZE(RDMA_FIFO_SIZE(rdma)) |
	      RDMA_OUTPUT_VALID_FIFO_THRESHOLD(threshold);

	MTK_DRM_DEBUG(comp->larb_dev, "comp %d rdma %d config %ux%u@%uHZ fifo 0x%X\n",
	comp->id, rdma_index(comp->id),
	width, height, vrefresh,
	reg);
	mtk_ddp_write(comp, reg, DISP_REG_RDMA_FIFO_CON, handle);
}

static void mtk_rdma_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_pending_state *pending, void *handle)
{
	unsigned int address = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int fmt = pending->format;

	enum RDMA_INPUT_FORMAT inputFormat = rdma_input_format_convert(fmt);
	unsigned int bpp = rdma_input_format_bpp(inputFormat);
	unsigned int input_is_yuv = rdma_input_format_color_space(inputFormat);
	unsigned int input_swap = rdma_input_format_byte_swap(inputFormat);
	unsigned int input_format_reg = rdma_input_format_reg_value(inputFormat);
	unsigned int rdma_idx = rdma_index(comp->id);
	unsigned int width = pending->width;
	unsigned int height = pending->height;
	enum RDMA_MODE mode = RDMA_MODE_MEMORY;

	MTK_DRM_DEBUG_DRIVER(
	"comp id %d rdma %d enable %d mode %d fmt 0x%X %dx%d addr 0x%x\n",
	comp->id, rdma_idx, pending->enable, mode, fmt, width, height, address);
	/*
	* if ((mode == RDMA_MODE_DIRECT_LINK) || width == 0 || height == 0)
	*	return;
	*/

	MTK_DRM_DEBUG_DRIVER("idx %d %d mode %d addr 0x%x fmt %s pitch %u width %u height %u\n",
		idx, rdma_idx, mode, address, rdma_intput_format_name(inputFormat, input_swap),
		pitch, width, height);

	MTK_DRM_DEBUG_DRIVER("bpp %d is_yuv %d fmt %d swap %d reg %d\n",
	bpp, input_is_yuv, inputFormat, input_swap, input_format_reg);

	if ((width > RDMA_MAX_WIDTH) || (height > RDMA_MAX_HEIGHT))
		DRM_ERROR("RDMA input overflow, w=%d, h=%d, max_w=%d, max_h=%d\n",
		width, height,
		RDMA_MAX_WIDTH, RDMA_MAX_HEIGHT);

	mtk_ddp_write_mask(comp,
	REG_FLD_VAL(RDMA_MODE_SEL, mode),
	DISP_REG_RDMA_GLOBAL_CON,
	REG_FLD_MASK(RDMA_MODE_SEL),
	handle);

	/* FORMAT & SWAP only works when RDMA memory mode,
	* set both to 0 when RDMA direct link mode. */
	mtk_ddp_write_mask(comp,
	REG_FLD_VAL(MEM_MODE_INPUT_FORMAT, input_format_reg),
	DISP_REG_RDMA_MEM_CON,
	REG_FLD_MASK(MEM_MODE_INPUT_FORMAT),
	handle);

	mtk_ddp_write_mask(comp,
	REG_FLD_VAL(MEM_MODE_INPUT_SWAP, input_swap),
	DISP_REG_RDMA_MEM_CON,
	REG_FLD_MASK(MEM_MODE_INPUT_SWAP),
	handle);

	mtk_ddp_write_mask(comp, address,
	DISP_REG_RDMA_MEM_START_ADDR, 0xFFFFFFFF, handle);

	mtk_ddp_write_mask(comp, pitch,
	DISP_REG_RDMA_MEM_SRC_PITCH, 0xFFFF, handle);

	return;
}

static const struct mtk_ddp_comp_funcs mtk_disp_rdma_funcs = {
	.config = mtk_rdma_config,
	.start = mtk_rdma_start,
	.stop = mtk_rdma_stop,
	.enable_vblank = mtk_rdma_enable_vblank,
	.disable_vblank = mtk_rdma_disable_vblank,
	.layer_config = mtk_rdma_layer_config,
};

static int mtk_disp_rdma_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct mtk_disp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;

}

static void mtk_disp_rdma_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_rdma_component_ops = {
	.bind	= mtk_disp_rdma_bind,
	.unbind = mtk_disp_rdma_unbind,
};

static int mtk_disp_rdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_rdma *priv;
	int comp_id;
	int irq;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_RDMA);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_rdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	/* Disable and clear pending interrupts */
	writel(0x0, priv->ddp_comp.regs + DISP_REG_RDMA_INT_ENABLE);
	writel(0x0, priv->ddp_comp.regs + DISP_REG_RDMA_INT_STATUS);

	ret = devm_request_irq(dev, irq, mtk_disp_rdma_irq_handler,
			       IRQF_TRIGGER_NONE, dev_name(dev), priv);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq %d: %d\n", irq, ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = component_add(dev, &mtk_disp_rdma_component_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	pm_runtime_enable(dev);

	MTK_DRM_DEBUG(dev,
	"comp_id %d irq %d rdma fifo size 0x%X 0x%X\n",
	comp_id, irq,
	RDMA_FIFO_SIZE(priv),
	RDMA_FIFO_PSEUDO_SIZE(RDMA_FIFO_SIZE(priv)));

	return ret;
}

static int mtk_disp_rdma_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_rdma_component_ops);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct mtk_disp_rdma_data mt2701_rdma_driver_data = {
	.fifo_size = SZ_4K,
};

static const struct mtk_disp_rdma_data mt8167_rdma_driver_data = {
	.fifo_size = SZ_4K,
};

static const struct mtk_disp_rdma_data mt8173_rdma_driver_data = {
	.fifo_size = SZ_8K,
};

static const struct of_device_id mtk_disp_rdma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-rdma",
	  .data = &mt2701_rdma_driver_data},
	{ .compatible = "mediatek,mt8167-disp-rdma",
	  .data = &mt8167_rdma_driver_data},
	{ .compatible = "mediatek,mt8173-disp-rdma",
	  .data = &mt8173_rdma_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_rdma_driver_dt_match);

struct platform_driver mtk_disp_rdma_driver = {
	.probe		= mtk_disp_rdma_probe,
	.remove		= mtk_disp_rdma_remove,
	.driver		= {
		.name	= "mediatek-disp-rdma",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_rdma_driver_dt_match,
	},
};
