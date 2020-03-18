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
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>
#include <linux/dma-buf.h>
#include <linux/reservation.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_debugfs.h"

/*
 * mtk specific framebuffer structure.
 *
 * @fb: drm framebuffer object.
 * @gem_obj: array of gem objects.
 */
struct mtk_drm_fb {
	struct drm_framebuffer	base;
	/* For now we only support a single plane */
	struct drm_gem_object	*gem_obj;
};

#define to_mtk_fb(x) container_of(x, struct mtk_drm_fb, base)

struct drm_gem_object *mtk_fb_get_gem_obj(struct drm_framebuffer *fb)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	return mtk_fb->gem_obj;
}

static int mtk_drm_fb_create_handle(struct drm_framebuffer *fb,
				    struct drm_file *file_priv,
				    unsigned int *handle)
{
	int ret;
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	ret = drm_gem_handle_create(file_priv, mtk_fb->gem_obj, handle);

	MTK_DRM_DEBUG_DRIVER("handle %d\n", handle);

	return ret;
}

static void mtk_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	MTK_DRM_DEBUG_DRIVER("\n");

	drm_framebuffer_cleanup(fb);

	drm_gem_object_unreference_unlocked(mtk_fb->gem_obj);

	kfree(mtk_fb);
}

static const struct drm_framebuffer_funcs mtk_drm_fb_funcs = {
	.create_handle = mtk_drm_fb_create_handle,
	.destroy = mtk_drm_fb_destroy,
};

static struct mtk_drm_fb *mtk_drm_framebuffer_init(struct drm_device *dev,
					const struct drm_mode_fb_cmd2 *mode,
					struct drm_gem_object *obj)
{
	struct mtk_drm_fb *mtk_fb;
	int ret;

	MTK_DRM_DEBUG_DRIVER("cmd2 %dx%d pitch %d %d handle %d %d\n",
	mode->width, mode->height,
	mode->pitches[0], mode->pitches[1],
	mode->handles[0], mode->handles[1]);

	if (drm_format_num_planes(mode->pixel_format) != 1) {
		MTK_DRM_ERROR(dev->dev, "invalid format for plane\n");
		return ERR_PTR(-EINVAL);
	}

	mtk_fb = kzalloc(sizeof(*mtk_fb), GFP_KERNEL);
	if (!mtk_fb) {
		MTK_DRM_ERROR(dev->dev, "failed to alloc for mtk_fb\n");
		return ERR_PTR(-ENOMEM);
	}

	drm_helper_mode_fill_fb_struct(&mtk_fb->base, mode);

	mtk_fb->gem_obj = obj;

	ret = drm_framebuffer_init(dev, &mtk_fb->base, &mtk_drm_fb_funcs);
	if (ret) {
		DRM_ERROR("failed to initialize framebuffer\n");
		kfree(mtk_fb);
		return ERR_PTR(ret);
	}

	return mtk_fb;
}

struct drm_framebuffer *mtk_drm_framebuffer_create(struct drm_device *dev,
		const struct drm_mode_fb_cmd2 *mode,
		struct drm_gem_object *obj)
{
	struct mtk_drm_fb *mtk_fb;

	MTK_DRM_DEBUG_DRIVER("\n");

	mtk_fb = mtk_drm_framebuffer_init(dev, mode, obj);
	if (IS_ERR(mtk_fb)) {
		MTK_DRM_ERROR(dev->dev, "failed to framebuffer init\n");
		return ERR_CAST(mtk_fb);
	}

	return &mtk_fb->base;
}

/*
 * Wait for any exclusive fence in fb's gem object's reservation object.
 *
 * Returns -ERESTARTSYS if interrupted, else 0.
 */
int mtk_fb_wait(struct drm_framebuffer *fb)
{
	struct drm_gem_object *gem;
	struct reservation_object *resv;
	long ret;

	if (!fb) {
		/* MTK_DRM_ERROR(fb?(fb->dev?fb->dev->dev:0):0, "fb is invalid\n"); */
		return 0;
	}

	gem = mtk_fb_get_gem_obj(fb);
	if (!gem || !gem->dma_buf || !gem->dma_buf->resv) {
		MTK_DRM_ERROR(fb->dev->dev,
		"gem 0x%p 0x%p 0x%p is invalid\n",
		gem,
		gem?gem->dma_buf:0,
		gem?(gem->dma_buf?gem->dma_buf->resv:0):0);
		return 0;
	}

	MTK_DRM_DEBUG_DRIVER("============\n");

	resv = gem->dma_buf->resv;
	ret = reservation_object_wait_timeout_rcu(resv, false, true,
						  MAX_SCHEDULE_TIMEOUT);
	/* MAX_SCHEDULE_TIMEOUT on success, -ERESTARTSYS if interrupted */
	if (WARN_ON(ret < 0)) {
		MTK_DRM_DEBUG_DRIVER("failed to wait fence timeout\n");
		return ret;
	}

	return 0;
}

struct drm_framebuffer *mtk_drm_mode_fb_create(struct drm_device *dev,
					       struct drm_file *file,
					       const struct drm_mode_fb_cmd2 *cmd)
{
	struct mtk_drm_fb *mtk_fb;
	struct drm_gem_object *gem;
	unsigned int width = cmd->width;
	unsigned int height = cmd->height;
	unsigned int size, bpp;
	int ret;

	MTK_DRM_DEBUG_DRIVER("handle %d\n", cmd->handles[0]);

	if (drm_format_num_planes(cmd->pixel_format) != 1) {
		MTK_DRM_ERROR(dev->dev, "invalid format for plane\n");
		return ERR_PTR(-EINVAL);
	}

	gem = drm_gem_object_lookup(dev, file, cmd->handles[0]);
	if (!gem) {
		MTK_DRM_ERROR(dev->dev, "can not find gem\n");
		return ERR_PTR(-ENOENT);
	}

	bpp = drm_format_plane_cpp(cmd->pixel_format, 0);
	size = (height - 1) * cmd->pitches[0] + width * bpp;
	size += cmd->offsets[0];

	if (gem->size < size) {
		ret = -EINVAL;
		MTK_DRM_ERROR(dev->dev, "gem size %d %d is invalid\n",
		(int)gem->size, size);
		goto unreference;
	}

	mtk_fb = mtk_drm_framebuffer_init(dev, cmd, gem);
	if (IS_ERR(mtk_fb)) {
		ret = PTR_ERR(mtk_fb);
		MTK_DRM_ERROR(dev->dev, "mtk fb init fail ret %d\n", ret);
		goto unreference;
	}

	return &mtk_fb->base;

unreference:
	drm_gem_object_unreference_unlocked(gem);
	return ERR_PTR(ret);
}
