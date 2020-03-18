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

#ifndef MTK_DRM_CRTC_H
#define MTK_DRM_CRTC_H

#include <drm/drm_crtc.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include <drm/drm_writeback.h>

#ifdef CONFIG_MTK_DRM_DISABLE_HW_OVL
#define OVL_LAYER_NR	1
#else
#define OVL_LAYER_NR	4
#endif

#define RDMA_LAYER_NR	1

#define MTK_DRM_PLANE_SCALE_SUPPORT 0

/**
 * struct mtk_drm_crtc - MediaTek specific crtc structure.
 * @base: crtc object.
 * @enabled: records whether crtc_enable succeeded
 * @planes: array of 4 mtk_drm_plane structures, one for each overlay plane
 * @pending_planes: whether any plane has pending changes to be applied
 * @config_regs: memory mapped mmsys configuration register space
 * @mutex: handle to one of the ten disp_mutex streams
 * @ddp_comp_nr: number of components in ddp_comp
 * @ddp_comp: array of pointers the mtk_ddp_comp structures used by this crtc
 */
struct mtk_drm_crtc {
	struct drm_crtc			base;
	bool				enabled;

	bool				pending_needs_vblank;
	struct drm_pending_vblank_event	*event;

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct cmdq_client		*cmdq_client;
	int				cmdq_event;
#endif

	struct drm_plane		planes[OVL_LAYER_NR];
	bool				pending_planes;

	void __iomem			*config_regs;
	struct mtk_disp_mutex		*mutex;
	unsigned int			ddp_comp_nr;
	struct mtk_ddp_comp		**ddp_comp;
	unsigned int			max_plane_num;

	struct drm_writeback_connector	wb_connector;
	bool				wb_enable;
	bool				wb_hw_enable;
};

int mtk_drm_crtc_enable_vblank(struct drm_device *drm, unsigned int pipe);
void mtk_drm_crtc_disable_vblank(struct drm_device *drm, unsigned int pipe);
void mtk_drm_crtc_check_flush(struct drm_crtc *crtc);
void mtk_drm_crtc_commit(struct drm_crtc *crtc);
void mtk_crtc_ddp_irq(struct drm_crtc *crtc, struct mtk_ddp_comp *ovl);
int mtk_drm_crtc_create(struct drm_device *drm_dev,
			const enum mtk_ddp_comp_id *path,
			unsigned int path_len);

void mtk_drm_crtc_plane_update(struct drm_crtc *crtc, struct drm_plane *plane,
			       struct mtk_plane_pending_state *pending);
#endif /* MTK_DRM_CRTC_H */
