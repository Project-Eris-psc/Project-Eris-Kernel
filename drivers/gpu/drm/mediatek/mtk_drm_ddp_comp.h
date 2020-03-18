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

#ifndef MTK_DRM_DDP_COMP_H
#define MTK_DRM_DDP_COMP_H

#include <linux/io.h>
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#include <linux/soc/mediatek/mtk-cmdq.h>
#endif
#include "mtk_drm_plane.h"

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct mtk_plane_state;

enum mtk_ddp_comp_type {
	MTK_DISP_OVL,
	MTK_DISP_RDMA,
	MTK_DISP_WDMA,
	MTK_DISP_COLOR,
	MTK_DISP_AAL,
	MTK_DISP_GAMMA,
	MTK_DISP_UFOE,
	MTK_DSI,
	MTK_DPI,
	MTK_DISP_PWM,
	MTK_DISP_MUTEX,
	MTK_DISP_OD,
	MTK_DISP_BLS,	
	MTK_DISP_CCORR,
	MTK_DISP_DITHER,
	MTK_DDP_COMP_TYPE_MAX,
};

enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL,
	DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_DITHER,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_ID_MAX,
};

struct mtk_ddp_comp;

struct mtk_ddp_comp_funcs {
	void (*config)(struct mtk_ddp_comp *comp, unsigned int w,
		       unsigned int h, unsigned int vrefresh, void *handle);
	void (*prepare)(struct mtk_ddp_comp *comp);
	void (*unprepare)(struct mtk_ddp_comp *comp);
	void (*start)(struct mtk_ddp_comp *comp, void *handle);
	void (*stop)(struct mtk_ddp_comp *comp, void *handle);
	void (*enable_vblank)(struct mtk_ddp_comp *comp, struct drm_crtc *crtc,
						void *handle);
	void (*disable_vblank)(struct mtk_ddp_comp *comp, void *handle);
	void (*layer_on)(struct mtk_ddp_comp *comp, unsigned int idx,
					void *handle);
	void (*layer_off)(struct mtk_ddp_comp *comp, unsigned int idx,
					void *handle);
	void (*layer_config)(struct mtk_ddp_comp *comp, unsigned int idx,
			     struct mtk_plane_pending_state *pending, void *handle);
};

struct mtk_ddp_comp_driver_data {
	unsigned int color_offset;
};

struct mtk_ddp_comp {
	struct clk *clk;
	void __iomem *regs;
	int irq;
	struct device *larb_dev;
	struct device *dev;
	enum mtk_ddp_comp_id id;
	struct mtk_drm_gem_obj *mtk_gem;
	const struct mtk_ddp_comp_funcs *funcs;
	const struct mtk_ddp_comp_driver_data *data;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct cmdq_base *cmdq_base;
#endif
};

static inline void mtk_ddp_comp_config(struct mtk_ddp_comp *comp,
				       unsigned int w, unsigned int h,
				       unsigned int vrefresh, void *handle)
{
	if (comp->funcs && comp->funcs->config)
		comp->funcs->config(comp, w, h, vrefresh, handle);
}

static inline void mtk_ddp_comp_prepare(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->prepare)
		comp->funcs->prepare(comp);
}

static inline void mtk_ddp_comp_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->unprepare)
		comp->funcs->unprepare(comp);
}

static inline void mtk_ddp_comp_start(struct mtk_ddp_comp *comp, void *handle)
{
	if (comp->funcs && comp->funcs->start)
		comp->funcs->start(comp, handle);
}

static inline void mtk_ddp_comp_stop(struct mtk_ddp_comp *comp, void *handle)
{
	if (comp->funcs && comp->funcs->stop)
		comp->funcs->stop(comp, handle);
}

static inline void mtk_ddp_comp_enable_vblank(struct mtk_ddp_comp *comp,
					      struct drm_crtc *crtc, void *handle)
{
	if (comp->funcs && comp->funcs->enable_vblank)
		comp->funcs->enable_vblank(comp, crtc, handle);
}

static inline void mtk_ddp_comp_disable_vblank(struct mtk_ddp_comp *comp, void *handle)
{
	if (comp->funcs && comp->funcs->disable_vblank)
		comp->funcs->disable_vblank(comp, handle);
}

static inline void mtk_ddp_comp_layer_on(struct mtk_ddp_comp *comp,
					 unsigned int idx, void *handle)
{
	if (comp->funcs && comp->funcs->layer_on)
		comp->funcs->layer_on(comp, idx, handle);
}

static inline void mtk_ddp_comp_layer_off(struct mtk_ddp_comp *comp,
					  unsigned int idx, void *handle)
{
	if (comp->funcs && comp->funcs->layer_off)
		comp->funcs->layer_off(comp, idx, handle);
}

static inline void mtk_ddp_comp_layer_config(struct mtk_ddp_comp *comp,
					     unsigned int idx,
					     struct mtk_plane_pending_state *pending, void *handle)
{
	if (comp->funcs && comp->funcs->layer_config)
		comp->funcs->layer_config(comp, idx, pending, handle);
}

int mtk_ddp_comp_get_id(struct device_node *node,
			enum mtk_ddp_comp_type comp_type);
struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id);
unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp);
int mtk_ddp_comp_init(struct device *dev, struct device_node *comp_node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs);
int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);

void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
		   unsigned int offset, void *handle);
void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			   unsigned int offset, void *handle);
void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle);

#endif /* MTK_DRM_DDP_COMP_H */
