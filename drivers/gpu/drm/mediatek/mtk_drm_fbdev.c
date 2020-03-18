/*
 * Copyright (c) 2016 MediaTek Inc.
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

#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_debugfs.h"

#define to_drm_private(x) \
		container_of(x, struct mtk_drm_private, fb_helper)

static void mtk_drm_fb_helper_cfb_fillrect(struct fb_info *info,
				const struct fb_fillrect *rect)
{
	MTK_DRM_DEBUG_DRIVER("\n");
	drm_fb_helper_cfb_fillrect(info, rect);
}
static void mtk_drm_fb_helper_cfb_copyarea(struct fb_info *info,
				const struct fb_copyarea *area)
{
	MTK_DRM_DEBUG_DRIVER("\n");
	drm_fb_helper_cfb_copyarea(info, area);
}

static void mtk_drm_fb_helper_cfb_imageblit(struct fb_info *info,
				 const struct fb_image *image)
{
	MTK_DRM_DEBUG_DRIVER("\n");
	drm_fb_helper_cfb_imageblit(info, image);
}

static int mtk_drm_fb_helper_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = drm_fb_helper_check_var(var, info);

	return ret;
}

static int mtk_drm_fb_helper_set_par(struct fb_info *info)
{
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = drm_fb_helper_set_par(info);

	return ret;
}

static int mtk_drm_fb_helper_blank(int blank, struct fb_info *info)
{
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = drm_fb_helper_blank(blank, info);

	return ret;
}

static int mtk_drm_fb_helper_pan_display(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = drm_fb_helper_pan_display(var, info);

	MTK_DRM_DEBUG_DRIVER("ret %d\n", ret);

	return ret;

}

static int mtk_drm_fb_helper_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = drm_fb_helper_setcmap(cmap, info);

	return ret;
}

static int mtk_drm_fbdev_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct mtk_drm_private *private = to_drm_private(helper);
	int ret;

	MTK_DRM_DEBUG_DRIVER("\n");

	ret = mtk_drm_gem_mmap_buf(private->fbdev_bo, vma);

	return ret;
}

static struct fb_ops mtk_fbdev_ops = {
	.owner = THIS_MODULE,
	.fb_fillrect = mtk_drm_fb_helper_cfb_fillrect,
	.fb_copyarea = mtk_drm_fb_helper_cfb_copyarea,
	.fb_imageblit = mtk_drm_fb_helper_cfb_imageblit,
	.fb_check_var = mtk_drm_fb_helper_check_var,
	.fb_set_par = mtk_drm_fb_helper_set_par,
	.fb_blank = mtk_drm_fb_helper_blank,
	.fb_pan_display = mtk_drm_fb_helper_pan_display,
	.fb_setcmap = mtk_drm_fb_helper_setcmap,
	.fb_mmap = mtk_drm_fbdev_mmap,
};

static int mtk_fbdev_probe(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct mtk_drm_private *private = to_drm_private(helper);
	struct drm_mode_fb_cmd2 mode = { 0 };
	struct mtk_drm_gem_obj *mtk_gem;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel;
	unsigned long offset;
	size_t size;
	int err;

	MTK_DRM_DEBUG_DRIVER("surface %d %d bpp %d depth %d fb_width %d fb_height %d\n",
	sizes->surface_width, sizes->surface_height,
	sizes->surface_bpp, sizes->surface_depth,
	sizes->fb_width, sizes->fb_height);

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);

	mode.width = sizes->surface_width;
	mode.height = sizes->surface_height;
	mode.pitches[0] = sizes->surface_width * bytes_per_pixel;
	mode.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
						      sizes->surface_depth);

	size = mode.pitches[0] * mode.height;

	mtk_gem = mtk_drm_gem_create(dev, size, true);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	private->fbdev_bo = &mtk_gem->base;

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		err = PTR_ERR(info);
		dev_err(dev->dev, "failed to allocate framebuffer info, %d\n",
			err);
		goto err_gem_free_object;
	}

	fb = mtk_drm_framebuffer_create(dev, &mode, private->fbdev_bo);
	if (IS_ERR(fb)) {
		err = PTR_ERR(fb);
		dev_err(dev->dev, "failed to allocate DRM framebuffer, %d\n",
			err);
		goto err_release_fbi;
	}
	helper->fb = fb;

	info->par = helper;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &mtk_fbdev_ops;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, helper, sizes->fb_width, sizes->fb_height);

	offset = info->var.xoffset * bytes_per_pixel;
	offset += info->var.yoffset * fb->pitches[0];

	dev->mode_config.fb_base = 0;
	info->screen_base = mtk_gem->kvaddr + offset;
	info->screen_size = size;
	info->fix.smem_len = size;

	MTK_DRM_DEBUG_KMS("FB base %p [%ux%u]-%u offset=%lu size=%zd\n",
	info->screen_base,
	fb->width, fb->height, fb->depth, offset, size);

	info->skip_vt_switch = true;

	return 0;

err_release_fbi:
	drm_fb_helper_release_fbi(helper);
err_gem_free_object:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return err;
}

static const struct drm_fb_helper_funcs mtk_drm_fb_helper_funcs = {
	.fb_probe = mtk_fbdev_probe,
};

int mtk_fbdev_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;
	int ret;

	if (!dev->mode_config.num_crtc || !dev->mode_config.num_connector) {
		MTK_DRM_ERROR(dev->dev,
		"failed to initialize fbdev, num_crtc %d num_connector %d\n",
		dev->mode_config.num_crtc,
		dev->mode_config.num_connector);
		return -EINVAL;
	}

	MTK_DRM_DEBUG_DRIVER("num_crtc %d num_connector %d\n",
	dev->mode_config.num_crtc,
	dev->mode_config.num_connector);

	drm_fb_helper_prepare(dev, helper, &mtk_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, dev->mode_config.num_crtc,
				 dev->mode_config.num_connector);
	if (ret) {
		dev_err(dev->dev, "failed to initialize DRM FB helper, %d\n",
			ret);
		goto fini;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret) {
		dev_err(dev->dev, "failed to add connectors, %d\n", ret);
		goto fini;
	}

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	ret = drm_fb_helper_initial_config(helper, 32);
	if (ret) {
		dev_err(dev->dev, "failed to set initial configuration, %d\n",
			ret);
		goto fini;
	}

	MTK_DRM_DEBUG_DRIVER("done\n");

	return 0;

fini:
	drm_fb_helper_fini(helper);

	return ret;
}

void mtk_fbdev_fini(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;

	drm_fb_helper_unregister_fbi(helper);
	drm_fb_helper_release_fbi(helper);

	if (helper->fb) {
		drm_framebuffer_unregister_private(helper->fb);
		drm_framebuffer_remove(helper->fb);
	}

	drm_fb_helper_fini(helper);
}

