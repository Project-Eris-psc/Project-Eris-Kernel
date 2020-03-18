/*
 * Copyright (c) 2014 MediaTek Inc.
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

#ifndef MTK_DRM_DEBUGFS_H
#define MTK_DRM_DEBUGFS_H

#define MTK_DRM_UT_CORE 		0x01
#define MTK_DRM_UT_DRIVER		0x02
#define MTK_DRM_UT_KMS			0x04
#define MTK_DRM_UT_PRIME		0x08
#define MTK_DRM_UT_ATOMIC		0x10
#define MTK_DRM_UT_VBL			0x20
extern unsigned int mtk_drm_debug;

struct drm_device;
struct mtk_drm_private;

#ifdef CONFIG_DEBUG_FS
void mtk_drm_debugfs_init(struct drm_device *dev,
			  struct mtk_drm_private *priv);
void mtk_drm_debugfs_deinit(void);
void mtk_drm_ut_debug_printk(const char *function_name, const char *format, ...);

#else
static inline void mtk_drm_debugfs_init(struct drm_device *dev,
					struct mtk_drm_private *priv) {}
static inline void mtk_drm_debugfs_deinit(void) {}
static inline void mtk_drm_ut_debug_printk(const char *function_name,
const char *format, ...) {}
#endif
bool force_alpha(void);

#define MTK_DRM_DEBUG_DEFAULT(fmt, args...)					\
	do {								\
		mtk_drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
	
#define MTK_DRM_ERROR(dev, fmt, args...)					\
	do {								\
		dev_err(dev, "[%s] "fmt, __func__, ##args);	\
	} while (0)
#define MTK_DRM_DEBUG(dev, fmt, args...)						\
	do {								\
		if (unlikely(mtk_drm_debug & MTK_DRM_UT_CORE))			\
			dev_err(dev, "[%s] "fmt, __func__, ##args);	\
	} while (0)

#define MTK_DRM_DEBUG_DRIVER(fmt, args...)					\
	do {								\
		if (unlikely(mtk_drm_debug & MTK_DRM_UT_DRIVER))		\
			mtk_drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define MTK_DRM_DEBUG_KMS(fmt, args...)					\
	do {								\
		if (unlikely(mtk_drm_debug & MTK_DRM_UT_KMS))			\
			mtk_drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define MTK_DRM_DEBUG_PRIME(fmt, args...)					\
	do {								\
		if (unlikely(mtk_drm_debug & MTK_DRM_UT_PRIME))			\
			mtk_drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define MTK_DRM_DEBUG_ATOMIC(fmt, args...)					\
	do {								\
		if (unlikely(mtk_drm_debug & MTK_DRM_UT_ATOMIC))		\
			mtk_drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)
#define MTK_DRM_DEBUG_VBL(fmt, args...)					\
	do {								\
		if (unlikely(mtk_drm_debug & MTK_DRM_UT_VBL))			\
			mtk_drm_ut_debug_printk(__func__, fmt, ##args);	\
	} while (0)


#endif /* MTK_DRM_DEBUGFS_H */
