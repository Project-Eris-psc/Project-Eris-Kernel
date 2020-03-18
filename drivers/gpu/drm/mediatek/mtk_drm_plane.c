/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: CK Hu <ck.hu@mediatek.com>
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
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_debugfs.h"

static const u32 formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_RGB565,
};

static void mtk_plane_enable(struct mtk_drm_plane *mtk_plane, bool enable,
			     dma_addr_t addr, struct drm_rect *dest)
{
	struct drm_plane *plane = &mtk_plane->base;
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);
	unsigned int pitch, format;
	int x, y;

	if (WARN_ON(!plane->state || (enable && !plane->state->fb))) {
		MTK_DRM_ERROR(
		plane->dev->dev,
		"plane state 0x%p enable %d fb 0x%p\n",
		plane->state, enable, plane->state->fb);
		return;
	}

	MTK_DRM_DEBUG(
		plane->dev->dev,
		"plane enable %d fb 0x%p pixe_format 0x%x\n",
		enable, plane->state->fb,
		plane->state->fb?(plane->state->fb->pixel_format):0);

	if (plane->state->fb) {
		pitch = plane->state->fb->pitches[0];
		format = plane->state->fb->pixel_format;
	} else {
		pitch = 0;
		format = DRM_FORMAT_RGBA8888;
	}

	x = plane->state->crtc_x;
	y = plane->state->crtc_y;

	if (x < 0) {
		addr -= x * 4;
		x = 0;
	}

	if (y < 0) {
		addr -= y * pitch;
		y = 0;
	}

	state->pending.enable = enable;
	state->pending.pitch = pitch;
	state->pending.format = format;
	state->pending.addr = addr;
	state->pending.x = x;
	state->pending.y = y;
	state->pending.width = dest->x2 - dest->x1;
	state->pending.height = dest->y2 - dest->y1;
	wmb(); /* Make sure the above parameters are set before update */
	state->pending.dirty = true;
}

static void mtk_plane_reset(struct drm_plane *plane)
{
	struct mtk_plane_state *state;

	MTK_DRM_DEBUG(plane->dev->dev, "state %p fb %p\n",
	plane->state,
	plane->state?(plane->state->fb):0);

	if (plane->state) {
		if (plane->state->fb)
			drm_framebuffer_unreference(plane->state->fb);

		state = to_mtk_plane_state(plane->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		plane->state = &state->base;
	}

	state->base.plane = plane;
	state->pending.format = DRM_FORMAT_RGB565;
}

static struct drm_plane_state *mtk_plane_duplicate_state(struct drm_plane *plane)
{
	struct mtk_plane_state *old_state = to_mtk_plane_state(plane->state);
	struct mtk_plane_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	MTK_DRM_DEBUG_DRIVER("\n");
	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	WARN_ON(state->base.plane != plane);

	state->pending = old_state->pending;

	return &state->base;
}

static void mtk_drm_plane_destroy_state(struct drm_plane *plane,
					struct drm_plane_state *state)
{
	MTK_DRM_DEBUG_DRIVER("\n");
	__drm_atomic_helper_plane_destroy_state(plane, state);
	kfree(to_mtk_plane_state(state));
}

static const struct drm_plane_funcs mtk_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = mtk_plane_reset,
	.atomic_duplicate_state = mtk_plane_duplicate_state,
	.atomic_destroy_state = mtk_drm_plane_destroy_state,
};

static int mtk_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	struct drm_framebuffer *fb = state->fb;
	struct drm_crtc_state *crtc_state;
	bool visible;
	int ret;
	struct drm_rect dest = {
		.x1 = state->crtc_x,
		.y1 = state->crtc_y,
		.x2 = state->crtc_x + state->crtc_w,
		.y2 = state->crtc_y + state->crtc_h,
	};
	struct drm_rect src = {
		/* 16.16 fixed point */
		.x1 = state->src_x,
		.y1 = state->src_y,
		.x2 = state->src_x + state->src_w,
		.y2 = state->src_y + state->src_h,
	};
	struct drm_rect clip = { 0, };

	if (!fb) {
		MTK_DRM_ERROR(plane->dev->dev, "fb is null\n");
		return 0;
	}

	if (!mtk_fb_get_gem_obj(fb)) {
		DRM_DEBUG_KMS("buffer is null\n");
		MTK_DRM_ERROR(plane->dev->dev, "buffer is null\n");
		return -EFAULT;
	}

	if (!state->crtc) {
		MTK_DRM_ERROR(plane->dev->dev, "crtc is null\n");
		return 0;
	}

	crtc_state = drm_atomic_get_crtc_state(state->state, state->crtc);
	if (IS_ERR(crtc_state)) {
		MTK_DRM_ERROR(plane->dev->dev, "crtc_state is null\n");
		return PTR_ERR(crtc_state);
	}

	clip.x2 = crtc_state->mode.hdisplay;
	clip.y2 = crtc_state->mode.vdisplay;

	ret = drm_plane_helper_check_update(plane, state->crtc, fb,
					     &src, &dest, &clip,
#if !MTK_DRM_PLANE_SCALE_SUPPORT
						 DRM_PLANE_HELPER_NO_SCALING,
					     DRM_PLANE_HELPER_NO_SCALING,
#else
						 DRM_PLANE_HELPER_MIN_SCALING,
					     DRM_PLANE_HELPER_MAX_SCALING,
#endif
					     true, true, &visible);

	MTK_DRM_DEBUG_DRIVER("ret %d\n", ret);
	return ret;
}

static void mtk_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);
	struct drm_crtc *crtc = state->base.crtc;
	struct drm_gem_object *gem;
	struct mtk_drm_gem_obj *mtk_gem;
	struct mtk_drm_plane *mtk_plane = to_mtk_plane(plane);
	struct drm_rect src = {
		.x1 = state->base.src_x/DRM_PLANE_HELPER_NO_SCALING,
		.y1 = state->base.src_y/DRM_PLANE_HELPER_NO_SCALING,
		.x2 = (state->base.src_x + state->base.src_w)/DRM_PLANE_HELPER_NO_SCALING,
		.y2 = (state->base.src_y + state->base.src_h)/DRM_PLANE_HELPER_NO_SCALING,
	};
	struct drm_rect dest = {
		.x1 = state->base.crtc_x,
		.y1 = state->base.crtc_y,
		.x2 = state->base.crtc_x + state->base.crtc_w,
		.y2 = state->base.crtc_y + state->base.crtc_h,
	};
	struct drm_rect clip = { 0, };

	if (!crtc) {
		MTK_DRM_ERROR(plane->dev->dev,
		"crtc is invalid!\n");
		return;
	}

	clip.x2 = state->base.crtc->state->mode.hdisplay;
	clip.y2 = state->base.crtc->state->mode.vdisplay;
	drm_rect_intersect(&src, &clip);
	drm_rect_intersect(&dest, &clip);

	gem = mtk_fb_get_gem_obj(state->base.fb);
	mtk_gem = to_mtk_gem_obj(gem);

	MTK_DRM_DEBUG_DRIVER(
	"cookie 0x%p kvaddr 0x%p dma 0x%p src (%d %d %d %d) crtc (%d %d %d %d)\n",
	mtk_gem->cookie, mtk_gem->kvaddr, (void *)mtk_gem->dma_addr,
	state->base.src_x, state->base.src_y,
	state->base.src_w, state->base.src_h,
	state->base.crtc_x, state->base.crtc_y,
	state->base.crtc_w, state->base.crtc_h);

#if MTK_DRM_PLANE_SCALE_SUPPORT
	mtk_plane_enable(mtk_plane, true, mtk_gem->dma_addr, &src);
#else
	mtk_plane_enable(mtk_plane, true, mtk_gem->dma_addr, &dest);
#endif

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	mtk_drm_crtc_plane_update(crtc, plane, &state->pending);
#endif
}

static void mtk_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);

	state->pending.enable = false;
	wmb(); /* Make sure the above parameter is set before update */
	state->pending.dirty = true;

	MTK_DRM_DEBUG_DRIVER("\n");

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	/* Fetch CRTC from old plane state when disabling. */
	mtk_drm_crtc_plane_update(old_state->crtc, plane, &state->pending);
#endif
}

static const struct drm_plane_helper_funcs mtk_plane_helper_funcs = {
	.atomic_check = mtk_plane_atomic_check,
	.atomic_update = mtk_plane_atomic_update,
	.atomic_disable = mtk_plane_atomic_disable,
};

int mtk_plane_init(struct drm_device *dev, struct drm_plane *plane,
		   unsigned long possible_crtcs, enum drm_plane_type type)
{
	int err;

	MTK_DRM_DEBUG_DRIVER("crtc 0x%x\n", possible_crtcs);

	err = drm_universal_plane_init(dev, plane, possible_crtcs,
				       &mtk_plane_funcs, formats,
				       ARRAY_SIZE(formats), type, NULL);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	drm_plane_helper_add(plane, &mtk_plane_helper_funcs);

	return 0;
}
