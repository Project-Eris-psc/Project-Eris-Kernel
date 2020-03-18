/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _H_DDP_INFO
#define _H_DDP_INFO

#include <linux/types.h>
#include "ddp_hal.h"
#include "DpDataType.h"
#include "lcm_drv.h"
#include "disp_event.h"
#include "ddp_ovl.h"
#include <disp_session.h>

struct OVL_CONFIG_STRUCT {
	unsigned int ovl_index;
	unsigned int layer;
	unsigned int layer_en;
	enum OVL_LAYER_SOURCE source;
	unsigned int fmt;
	unsigned long addr;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;	/* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int sur_aen;
	unsigned int src_alpha;
	unsigned int dst_alpha;

	unsigned int isTdshp;
	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int identity;
	unsigned int connected_type;
	enum DISP_BUFFER_TYPE security;

	unsigned int yuv_range;
};

struct OVL_BASIC_STRUCT {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned long addr;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int bpp;
};

struct RDMA_BASIC_STRUCT {
	unsigned long addr;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int bpp;
};

struct RDMA_CONFIG_STRUCT {
	unsigned idx;		/* instance index */
	enum DP_COLOR_ENUM inputFormat;
	unsigned long address;
	unsigned pitch;
	unsigned width;
	unsigned height;
	enum DISP_BUFFER_TYPE security;
	bool is_interlace;
	bool is_top_filed;
};

struct WDMA_CONFIG_STRUCT {
	unsigned srcWidth;
	unsigned srcHeight;	/* input */
	unsigned clipX;
	unsigned clipY;
	unsigned clipWidth;
	unsigned clipHeight;	/* clip */
	enum DP_COLOR_ENUM outputFormat;
	unsigned long dstAddress;
	unsigned dstPitch;	/* output */
	unsigned int useSpecifiedAlpha;
	unsigned char alpha;
	enum DISP_BUFFER_TYPE security;
};

struct disp_ddp_path_config {
	/* for ovl */
	bool ovl_dirty;
	bool rdma_dirty;
	bool wdma_dirty;
	bool dst_dirty;
	bool roi_dirty;
	bool is_memory;
	struct OVL_CONFIG_STRUCT ovl_config[OVL_LAYER_NUM];
	struct RDMA_CONFIG_STRUCT rdma_config;
	struct WDMA_CONFIG_STRUCT wdma_config;
	LCM_PARAMS dispif_config;
	unsigned int lcm_bpp;
	unsigned int dst_w;
	unsigned int dst_h;
	unsigned int fps;
	unsigned int hw_mutex_id;
};

typedef int (*ddp_module_notify)(enum DISP_MODULE_ENUM, enum DISP_PATH_EVENT);

struct DDP_MODULE_DRIVER {
	enum DISP_MODULE_ENUM module;
	int (*init)(enum DISP_MODULE_ENUM module, void *handle);
	int (*deinit)(enum DISP_MODULE_ENUM module, void *handle);
	int (*config)(enum DISP_MODULE_ENUM module, struct disp_ddp_path_config *config, void *handle);
	int (*start)(enum DISP_MODULE_ENUM module, void *handle);
	int (*trigger)(enum DISP_MODULE_ENUM module, void *handle);
	int (*stop)(enum DISP_MODULE_ENUM module, void *handle);
	int (*reset)(enum DISP_MODULE_ENUM module, void *handle);
	int (*power_on)(enum DISP_MODULE_ENUM module, void *handle);
	int (*power_off)(enum DISP_MODULE_ENUM module, void *handle);
	int (*suspend)(enum DISP_MODULE_ENUM module, void *handle);
	int (*resume)(enum DISP_MODULE_ENUM module, void *handle);
	int (*is_idle)(enum DISP_MODULE_ENUM module);
	int (*is_busy)(enum DISP_MODULE_ENUM module);
	int (*dump_info)(enum DISP_MODULE_ENUM module, int level);
	int (*bypass)(enum DISP_MODULE_ENUM module, int bypass);
	int (*build_cmdq)(enum DISP_MODULE_ENUM module, void *cmdq_handle, enum CMDQ_STATE state);
	int (*set_lcm_utils)(enum DISP_MODULE_ENUM module, LCM_DRIVER *lcm_drv);
	int (*set_listener)(enum DISP_MODULE_ENUM module, ddp_module_notify notify);
	int (*cmd)(enum DISP_MODULE_ENUM module, int msg, unsigned long arg, void *handle);
	int (*ioctl)(enum DISP_MODULE_ENUM module, void *handle, unsigned int ioctl_cmd,
		      unsigned long *params);
	int (*enable_irq)(enum DISP_MODULE_ENUM module, void *handle, enum DDP_IRQ_LEVEL irq_level);
};

char *ddp_get_module_name(enum DISP_MODULE_ENUM module);
int ddp_get_module_max_irq_bit(enum DISP_MODULE_ENUM module);

extern struct DDP_MODULE_DRIVER *ddp_modules_driver[DISP_MODULE_NUM];

/* TODO: FIXME */
/* dsi */
extern struct DDP_MODULE_DRIVER ddp_driver_dsi0;
/* extern struct DDP_MODULE_DRIVER ddp_driver_dsi1; */
/* extern struct DDP_MODULE_DRIVER ddp_driver_dsidual; */
/* dpi */
extern struct DDP_MODULE_DRIVER ddp_driver_dpi0;
extern struct DDP_MODULE_DRIVER ddp_driver_dpi1;

/* ovl */
extern struct DDP_MODULE_DRIVER ddp_driver_ovl;
/* rdma */
extern struct DDP_MODULE_DRIVER ddp_driver_rdma;
/* wdma */
extern struct DDP_MODULE_DRIVER ddp_driver_wdma;
/* color */
extern struct DDP_MODULE_DRIVER ddp_driver_color;
/* aal */
extern struct DDP_MODULE_DRIVER ddp_driver_aal;
/* od */
#if defined(MTK_FB_OD_SUPPORT)
extern struct DDP_MODULE_DRIVER ddp_driver_od;
#endif
/* gamma */
extern struct DDP_MODULE_DRIVER ddp_driver_gamma;
/* dither */
extern struct DDP_MODULE_DRIVER ddp_driver_dither;
/* ccorr */
extern struct DDP_MODULE_DRIVER ddp_driver_ccorr;
/* split */
extern struct DDP_MODULE_DRIVER ddp_driver_split;
/* pwm */
extern struct DDP_MODULE_DRIVER ddp_driver_pwm;

#endif
