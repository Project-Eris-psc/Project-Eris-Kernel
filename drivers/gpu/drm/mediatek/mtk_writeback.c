/*
 * Copyright (c) 2017 MediaTek Inc.
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
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_writeback.h>

#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_writeback.h"
#include "mtk_drm_debugfs.h"

#define DISP_REG_WDMA_DST_ADDR                  0x0f00

static enum mtk_ddp_comp_id mtk_wb_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static enum mtk_ddp_comp_id mtk_wb_ext[] = {
};

static const u32 wb_output_formats[] = {
	DRM_FORMAT_RGB888,
};

static enum drm_connector_status mtk_wb_connector_detect(
	struct drm_connector *connector, bool force)
{
	MTK_DRM_DEBUG_DRIVER("\n");
	return connector_status_disconnected;
}

static int mtk_wb_connector_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	MTK_DRM_DEBUG_DRIVER("\n");

	return drm_add_modes_noedid(connector, dev->mode_config.max_width,
				    dev->mode_config.max_height);
}

static enum drm_mode_status mtk_wb_connector_mode_valid(
	struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct drm_device *dev = connector->dev;
	struct drm_mode_config *mode_config = &dev->mode_config;
	int w = mode->hdisplay, h = mode->vdisplay;

	MTK_DRM_DEBUG_DRIVER("\n");

	if ((w < mode_config->min_width) || (w > mode_config->max_width)) {
		drm_err("w %d (%d %d) is invalid\n",
		w,
		mode_config->min_width,
		mode_config->max_width);
		return MODE_BAD_HVALUE;
	}

	if ((h < mode_config->min_height) || (h > mode_config->max_height)) {
		drm_err("h %d (%d %d) is invalid\n",
		w,
		mode_config->min_height,
		mode_config->max_height);
		return MODE_BAD_VVALUE;
	}

	return MODE_OK;
}

static int mtk_wb_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	MTK_DRM_DEBUG_DRIVER("\n");
	return 0;
}

static const struct drm_encoder_helper_funcs mtk_wb_encoder_helper_funcs = {
	.atomic_check = mtk_wb_atomic_check,
};

static const struct drm_connector_funcs mtk_wb_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = mtk_wb_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
	mtk_wb_connector_helper_funcs = {
	.get_modes = mtk_wb_connector_get_modes,
	.mode_valid = mtk_wb_connector_mode_valid,
};

static struct mtk_ddp_comp *mtk_wb_find_wdma(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *wdma = NULL;

	MTK_DRM_DEBUG_DRIVER("\n");

	wdma = mtk_ddp_comp_find_by_id(&mtk_crtc->base, mtk_wb_main[1]);
	if (wdma == NULL) {
		drm_err("cannot find out wdma in wb main path\n");

		return wdma;
		
		wdma = mtk_ddp_comp_find_by_id(&mtk_crtc->base, mtk_wb_ext[1]);
	}

	return wdma;
}

void mtk_wb_add_comp_to_path(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *wdma = NULL;
	struct drm_device *drm_dev = mtk_crtc->base.dev;

	wdma = mtk_wb_find_wdma(mtk_crtc);
	if (wdma == NULL) {
		MTK_DRM_ERROR(drm_dev?(drm_dev->dev):0, "cannot find out wdma\n");
		return;
	}

	wdma->mtk_gem = mtk_drm_gem_create(drm_dev, 1920 * 1080 * 3, true);

	MTK_DRM_DEBUG_DRIVER(
	"crtc display %dx%d create wdma gem cookie 0x%p kvaddr 0x%p\n",
	mtk_crtc->base.mode.hdisplay,
	mtk_crtc->base.mode.vdisplay,
	wdma->mtk_gem->cookie,
	wdma->mtk_gem->kvaddr);

	if (wdma->id == mtk_wb_main[1]) {
		mtk_ddp_add_comp_to_path(mtk_crtc->config_regs,
					 mtk_wb_main[0],
					 mtk_wb_main[1]);
	} else if (wdma->id == mtk_wb_ext[1]) {
		mtk_ddp_add_comp_to_path(mtk_crtc->config_regs,
					 mtk_wb_ext[0],
					 mtk_wb_ext[1]);
	} else {
		return;
	}
}

void mtk_wb_remove_comp_from_path(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *wdma = NULL;

	wdma = mtk_wb_find_wdma(mtk_crtc);
	if (wdma == NULL) {
		drm_err("can not find wdma\n");
		return;
	}

	MTK_DRM_DEBUG_DRIVER("\n");

	mtk_drm_gem_free_object(&wdma->mtk_gem->base);

	if (wdma->id == mtk_wb_main[1]) {
		mtk_ddp_remove_comp_from_path(mtk_crtc->config_regs,
					      mtk_wb_main[0],
					      mtk_wb_main[1]);
	} else if (wdma->id == mtk_wb_ext[1]) {
		mtk_ddp_remove_comp_from_path(mtk_crtc->config_regs,
					      mtk_wb_ext[0],
					      mtk_wb_ext[1]);
	} else {
		return;
	}
}

int mtk_wb_set_possible_crtcs(struct drm_device *drm_dev,
			      struct mtk_drm_crtc *mtk_crtc,
			      struct mtk_ddp_comp *comp)
{
	struct drm_writeback_connector *wb_connector = &mtk_crtc->wb_connector;
	unsigned int *possible_crtcs = &wb_connector->encoder.possible_crtcs;

	*possible_crtcs = mtk_drm_find_possible_crtc_by_comp(drm_dev, *comp);

	if (*possible_crtcs == 0) {
		drm_err("Failed to set wb possible_crtcs\n");
		return -1;
	}

	MTK_DRM_DEBUG_DRIVER("\n");

	return 0;
}

int mtk_wb_connector_init(struct drm_device *drm_dev,
			  struct mtk_drm_crtc *mtk_crtc)
{
	int ret;
	struct drm_writeback_connector *wb_connector = &mtk_crtc->wb_connector;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = drm_writeback_connector_init(drm_dev, wb_connector,
					   &mtk_wb_connector_funcs,
					   &mtk_wb_encoder_helper_funcs,
					   wb_output_formats,
					   (int)ARRAY_SIZE(wb_output_formats));
	if (ret != 0) {
		drm_err("writeback connector init fail\n");
		return ret;
	}

	drm_connector_helper_add(&mtk_crtc->wb_connector.base,
				 &mtk_wb_connector_helper_funcs);

	return 0;
}

#ifdef CONFIG_MTK_DISPLAY_CMDQ
void mtk_wb_atomic_commit(struct mtk_drm_crtc *mtk_crtc, unsigned int v,
			  unsigned int h, void *cmdq_handle)
#else
void mtk_wb_atomic_commit(struct mtk_drm_crtc *mtk_crtc)
#endif
{
	struct mtk_ddp_comp *wdma = NULL;
	struct drm_writeback_connector *wb_conn = &mtk_crtc->wb_connector;
	struct drm_connector_state *conn_state = wb_conn->base.state;

	if (conn_state == NULL) {
		drm_err("writeback connector state is invalid\n");
		return;
	}

	MTK_DRM_DEBUG_DRIVER("writeback_job 0x%p, fb 0x%p\n",
	conn_state->writeback_job,
	conn_state->writeback_job?conn_state->writeback_job->fb:0);

	if (conn_state->writeback_job != NULL &&
	    conn_state->writeback_job->fb != NULL) {
		struct drm_framebuffer *fb = conn_state->writeback_job->fb;
		struct drm_gem_object *gem;
		struct mtk_drm_gem_obj *mtk_wb_gem;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#else
		void *cmdq_handle = NULL;
#endif
		wdma = mtk_wb_find_wdma(mtk_crtc);

		mtk_crtc->wb_enable = true;

		gem = mtk_fb_get_gem_obj(fb);
		mtk_wb_gem = to_mtk_gem_obj(gem);

		drm_writeback_queue_job(wb_conn, conn_state->writeback_job);
		conn_state->writeback_job = NULL;

		mtk_ddp_write(wdma, (u32)mtk_wb_gem->dma_addr & 0xFFFFFFFFU,
			      DISP_REG_WDMA_DST_ADDR, cmdq_handle);
	}
}
