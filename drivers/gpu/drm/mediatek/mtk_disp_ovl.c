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

#define DISP_REG_OVL_INTEN			0x0004
#define OVL_FME_CPL_INT					BIT(1)
#define DISP_REG_OVL_INTSTA			0x0008
#define DISP_REG_OVL_EN				0x000c
#define DISP_REG_OVL_RST			0x0014
#define DISP_REG_OVL_ROI_SIZE			0x0020
#define DISP_REG_OVL_ROI_BGCLR			0x0028
#define DISP_REG_OVL_SRC_CON			0x002c
#define DISP_REG_OVL_CON(n)			(0x0030 + 0x20 * (n))
#define DISP_REG_OVL_SRC_SIZE(n)		(0x0038 + 0x20 * (n))
#define DISP_REG_OVL_OFFSET(n)			(0x003c + 0x20 * (n))
#define DISP_REG_OVL_PITCH(n)			(0x0044 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_CTRL(n)		(0x00c0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_GMC(n)		(0x00c8 + 0x20 * (n))
#define DISP_REG_OVL_ADDR_MT2701		0x0040
#define DISP_REG_OVL_ADDR_MT8167		0x0f40
#define DISP_REG_OVL_ADDR_MT8173		0x0f40
#define DISP_REG_OVL_ADDR(ovl, n)		((ovl)->data->addr + 0x20 * (n))


#define	OVL_RDMA_MEM_GMC	0x40402020

#define OVL_CON_BYTE_SWAP	BIT(24)
#define OVL_CON_CLRFMT_RGB	(1 << 12)
#define OVL_CON_CLRFMT_RGBA8888	(2 << 12)
#define OVL_CON_CLRFMT_ARGB8888	(3 << 12)
#define OVL_CON_CLRFMT_RGB565(ovl)	((ovl)->data->fmt_rgb565_is_0 ? \
					0 : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(ovl)	((ovl)->data->fmt_rgb565_is_0 ? \
					OVL_CON_CLRFMT_RGB : 0)
#define	OVL_CON_AEN		BIT(8)
#define	OVL_CON_ALPHA		0xff

struct mtk_disp_ovl_data {
	unsigned int addr;
	bool fmt_rgb565_is_0;
	unsigned int fmt_uyvy;
	unsigned int fmt_yuyv;
};

/**
 * struct mtk_disp_ovl - DISP_OVL driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report vblank events to
 */
struct mtk_disp_ovl {
	struct mtk_ddp_comp		ddp_comp;
	struct drm_crtc			*crtc;
	const struct mtk_disp_ovl_data	*data;
};

void mtk_ovl_reset(struct mtk_ddp_comp *comp,void *handle)
{
	mtk_ddp_write(comp, 0x1, DISP_REG_OVL_RST, handle);
	mtk_ddp_write(comp, 0x0, DISP_REG_OVL_RST, handle);

	MTK_DRM_DEBUG_DRIVER(
	"ovl irq status 0x%X\n",
	readl(comp->regs + DISP_REG_OVL_INTSTA));
}

static inline struct mtk_disp_ovl *comp_to_ovl(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ovl, ddp_comp);
}

static irqreturn_t mtk_disp_ovl_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ovl *priv = dev_id;
	struct mtk_ddp_comp *ovl = &priv->ddp_comp;

	MTK_DRM_DEBUG_VBL("\n");

	/* Clear frame completion interrupt */
	writel(0x0, ovl->regs + DISP_REG_OVL_INTSTA);

	if (!priv->crtc) {
		MTK_DRM_DEBUG_DRIVER("crtc is invalid!\n");
		return IRQ_NONE;
	}

	mtk_crtc_ddp_irq(priv->crtc, ovl);

	return IRQ_HANDLED;
}

static void mtk_ovl_enable_vblank(struct mtk_ddp_comp *comp,
				  struct drm_crtc *crtc, void *handle)
{
	struct mtk_disp_ovl *priv = container_of(comp, struct mtk_disp_ovl,
						 ddp_comp);

	MTK_DRM_DEBUG_DRIVER("crtc 0x%p\n", crtc);

	priv->crtc = crtc;
	writel_relaxed(OVL_FME_CPL_INT, comp->regs + DISP_REG_OVL_INTEN);
}

static void mtk_ovl_disable_vblank(struct mtk_ddp_comp *comp, void *handle)
{
	struct mtk_disp_ovl *priv = container_of(comp, struct mtk_disp_ovl,
						 ddp_comp);

	MTK_DRM_DEBUG_DRIVER("\n");

	priv->crtc = NULL;
	writel_relaxed(0x0, comp->regs + DISP_REG_OVL_INTEN);
}

static void mtk_ovl_start(struct mtk_ddp_comp *comp, void *handle)
{
	int ret;

	ret = pm_runtime_get_sync(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);

	MTK_DRM_DEBUG_DRIVER("\n");

	mtk_ddp_write_relaxed(comp, 0x1, DISP_REG_OVL_EN, handle);
}

static void mtk_ovl_stop(struct mtk_ddp_comp *comp, void *handle)
{
	int ret;

	ret = pm_runtime_put(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);

	MTK_DRM_DEBUG_DRIVER("\n");

	mtk_ddp_write_relaxed(comp, 0x0, DISP_REG_OVL_EN, handle);
}

static void mtk_ovl_config(struct mtk_ddp_comp *comp, unsigned int w,
			   unsigned int h, unsigned int vrefresh, void *handle)
{
	MTK_DRM_DEBUG_DRIVER(
	"comp_id %d irq %d %dx%d@%dHZ\n",
	comp->id, comp->irq, w, h, vrefresh);

	if (w != 0 && h != 0)
		mtk_ddp_write_relaxed(comp, h << 16 | w, DISP_REG_OVL_ROI_SIZE, handle);
	mtk_ddp_write_relaxed(comp, 0x0, DISP_REG_OVL_ROI_BGCLR, handle);

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	mtk_ovl_reset(comp, handle);
#endif
}

static void mtk_ovl_layer_on(struct mtk_ddp_comp *comp,
							unsigned int idx, void *handle)
{
#ifndef CONFIG_MTK_DISPLAY_CMDQ
	unsigned int ovl_src_con;
#endif

	mtk_ddp_write(comp, 0x1, DISP_REG_OVL_RDMA_CTRL(idx), handle);
	mtk_ddp_write(comp, OVL_RDMA_MEM_GMC, DISP_REG_OVL_RDMA_GMC(idx), handle);

#ifndef CONFIG_MTK_DISPLAY_CMDQ
	ovl_src_con = readl(comp->regs + DISP_REG_OVL_SRC_CON);
	MTK_DRM_DEBUG_DRIVER("layer %d\n", idx);

	if (!ovl_src_con)
		mtk_ovl_reset(comp, handle);
#endif

	mtk_ddp_write_mask(comp, BIT(idx), DISP_REG_OVL_SRC_CON, BIT(idx), handle);


}

static void mtk_ovl_layer_off(struct mtk_ddp_comp *comp, unsigned int idx,
								void *handle)
{
	MTK_DRM_DEBUG_DRIVER("\n");

	mtk_ddp_write_mask(comp, 0, DISP_REG_OVL_SRC_CON, BIT(idx), handle);

	mtk_ddp_write(comp, 0x0, DISP_REG_OVL_RDMA_CTRL(idx), handle);
}

static unsigned int ovl_fmt_convert(struct mtk_disp_ovl *ovl, unsigned int fmt)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return OVL_CON_CLRFMT_RGB565(ovl);
	case DRM_FORMAT_BGR565:
		return OVL_CON_CLRFMT_RGB565(ovl) | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGB888:
		return OVL_CON_CLRFMT_RGB888(ovl);
	case DRM_FORMAT_BGR888:
		return OVL_CON_CLRFMT_RGB888(ovl) | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		return OVL_CON_CLRFMT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		return OVL_CON_CLRFMT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP;
	}
}

static void mtk_ovl_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_pending_state *pending,
				 void *handle)
{
	struct mtk_disp_ovl *ovl = comp_to_ovl(comp);

	unsigned int addr = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int fmt = pending->format;
	unsigned int offset = (pending->y << 16) | pending->x;
	unsigned int src_size = (pending->height << 16) | pending->width;
	unsigned int con;

	MTK_DRM_DEBUG_DRIVER(
	"layer %d enable %d fmt 0x%x %s (%X %X %X %X) pitch %X addr 0x%X\n",
	idx, pending->enable, fmt, drm_get_format_name(fmt),
	pending->x, pending->y, pending->width, pending->height, pitch,
	pending->addr);

	if (!pending->enable)
		mtk_ovl_layer_off(comp, idx, handle);

	con = ovl_fmt_convert(ovl, fmt);
	if (idx != 0)
		con |= OVL_CON_AEN | OVL_CON_ALPHA;

	mtk_ddp_write_relaxed(comp, con, DISP_REG_OVL_CON(idx), handle);
	mtk_ddp_write_relaxed(comp, pitch, DISP_REG_OVL_PITCH(idx), handle);
	mtk_ddp_write_relaxed(comp, src_size, DISP_REG_OVL_SRC_SIZE(idx), handle);
	mtk_ddp_write_relaxed(comp, offset, DISP_REG_OVL_OFFSET(idx), handle);
	mtk_ddp_write_relaxed(comp, addr, DISP_REG_OVL_ADDR(ovl, idx), handle);

	if (pending->enable)
		mtk_ovl_layer_on(comp, idx, handle);
}

static const struct mtk_ddp_comp_funcs mtk_disp_ovl_funcs = {
	.config = mtk_ovl_config,
	.start = mtk_ovl_start,
	.stop = mtk_ovl_stop,
	.enable_vblank = mtk_ovl_enable_vblank,
	.disable_vblank = mtk_ovl_disable_vblank,
	.layer_on = mtk_ovl_layer_on,
	.layer_off = mtk_ovl_layer_off,
	.layer_config = mtk_ovl_layer_config,
};

static int mtk_disp_ovl_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_ovl_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	MTK_DRM_DEBUG_DRIVER("\n");

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_component_ops = {
	.bind	= mtk_disp_ovl_bind,
	.unbind = mtk_disp_ovl_unbind,
};

static int mtk_disp_ovl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl *priv;
	int comp_id;
	int irq;
	int ret;

	MTK_DRM_DEBUG_DRIVER("dev_name %s of_node %s\n",
	dev_name(dev), dev->of_node->name);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(dev, irq, mtk_disp_ovl_irq_handler,
			       IRQF_TRIGGER_NONE, dev_name(dev), priv);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq %d: %d\n", irq, ret);
		return ret;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_OVL);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = component_add(dev, &mtk_disp_ovl_component_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	pm_runtime_enable(dev);

	MTK_DRM_DEBUG(dev,
	"comp_id %d irq %d\n", comp_id, irq);

	return ret;
}

static int mtk_disp_ovl_remove(struct platform_device *pdev)
{
	MTK_DRM_DEBUG_DRIVER("\n");

	component_del(&pdev->dev, &mtk_disp_ovl_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct mtk_disp_ovl_data mt2701_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT2701,
	.fmt_rgb565_is_0 = false,
	.fmt_uyvy = 9U << 12,
	.fmt_yuyv = 8U << 12,
};

static const struct mtk_disp_ovl_data mt8167_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT8167,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
};

static const struct mtk_disp_ovl_data mt8173_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT8173,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
};

static const struct of_device_id mtk_disp_ovl_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-ovl",
	  .data = &mt2701_ovl_driver_data},
	{ .compatible = "mediatek,mt8167-disp-ovl",
	  .data = &mt8167_ovl_driver_data},
	{ .compatible = "mediatek,mt8173-disp-ovl",
	  .data = &mt8173_ovl_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_driver_dt_match);

struct platform_driver mtk_disp_ovl_driver = {
	.probe		= mtk_disp_ovl_probe,
	.remove		= mtk_disp_ovl_remove,
	.driver		= {
		.name	= "mediatek-disp-ovl",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_ovl_driver_dt_match,
	},
};
