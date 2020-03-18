/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DDP_DPI_H__
#define __DDP_DPI_H__

#ifdef BUILD_UBOOT
#include <asm/arch/mt65xx_typedefs.h>
#else
/*#include <mach/mt_typedefs.h>*/
#endif

#include "lcm_drv.h"
#include "ddp_info.h"
#include "cmdq_record.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DPI_MASKREG32(cmdq, REG, MASK, VALUE)	DISP_REG_MASK((cmdq), (REG), (VALUE), (MASK))
#define MASKREG32(x, y, z)  OUTREG32(x, (INREG32(x)&~(y))|(z))

#define DPI_OUTREGBIT(cmdq, TYPE, REG, bit, value)  \
	do {\
		TYPE r;\
		TYPE v;\
		if (cmdq) {									\
			*(unsigned int *)(&r) = ((unsigned int)0x00000000); \
			r.bit = ~(r.bit);						\
			*(unsigned int *)(&v) = ((unsigned int)0x00000000);	\
			v.bit = value; DISP_REG_MASK(cmdq, &REG, AS_UINT32(&v), AS_UINT32(&r));	\
		} else {									\
			mt_reg_sync_writel(INREG32(&REG), &r);	\
			r.bit = (value);						\
			DISP_REG_SET(cmdq, &REG, INREG32(&r));	\
			}										\
	} while (0)

/* for legacy DPI Driver */

#define DPI_CHECK_RET(expr)             \
	do {                                \
		enum DPI_STATUS ret = (expr);        \
		ASSERT(ret == DPI_STATUS_OK);   \
	} while (0)

/* for legacy DPI Driver */
	enum LCD_IF_ID {
		LCD_IF_PARALLEL_0 = 0,
		LCD_IF_PARALLEL_1 = 1,
		LCD_IF_PARALLEL_2 = 2,
		LCD_IF_SERIAL_0 = 3,
		LCD_IF_SERIAL_1 = 4,

		LCD_IF_ALL = 0xFF,
	};

	struct LCD_REG_CMD_ADDR {
		unsigned rsv_0:4;
		unsigned addr:4;
		unsigned rsv_8:24;
	};

	struct LCD_REG_DAT_ADDR {
		unsigned rsv_0:4;
		unsigned addr:4;
		unsigned rsv_8:24;
	};


	enum LCD_IF_FMT_COLOR_ORDER {
		LCD_IF_FMT_COLOR_ORDER_RGB = 0,
		LCD_IF_FMT_COLOR_ORDER_BGR = 1,
	};


	enum LCD_IF_FMT_TRANS_SEQ {
		LCD_IF_FMT_TRANS_SEQ_MSB_FIRST = 0,
		LCD_IF_FMT_TRANS_SEQ_LSB_FIRST = 1,
	};


	enum LCD_IF_FMT_PADDING {
		LCD_IF_FMT_PADDING_ON_LSB = 0,
		LCD_IF_FMT_PADDING_ON_MSB = 1,
	};


	enum LCD_IF_FORMAT {
		LCD_IF_FORMAT_RGB332 = 0,
		LCD_IF_FORMAT_RGB444 = 1,
		LCD_IF_FORMAT_RGB565 = 2,
		LCD_IF_FORMAT_RGB666 = 3,
		LCD_IF_FORMAT_RGB888 = 4,
	};

	enum LCD_IF_WIDTH {
		LCD_IF_WIDTH_8_BITS = 0,
		LCD_IF_WIDTH_9_BITS = 2,
		LCD_IF_WIDTH_16_BITS = 1,
		LCD_IF_WIDTH_18_BITS = 3,
		LCD_IF_WIDTH_24_BITS = 4,
		LCD_IF_WIDTH_32_BITS = 5,
	};


	enum DPI_STATUS {
		DPI_STATUS_OK = 0,

		DPI_STATUS_ERROR,
	};

	enum DPI_POLARITY {
		DPI_POLARITY_RISING = 0,
		DPI_POLARITY_FALLING = 1
	};

	enum DPI_RGB_ORDER {
		DPI_RGB_ORDER_RGB = 0,
		DPI_RGB_ORDER_BGR = 1,
	};

	enum DPI_CLK_FREQ {
		DPI_CLK_480p = 27027,
		DPI_CLK_720p = 74250,
		DPI_CLK_1080p = 148500
	};

	enum DPI_VIDEO_RESOLUTION {
		DPI_VIDEO_720x480p_60Hz = 0,	/* 0 */
		DPI_VIDEO_720x576p_50Hz,	/* 1 */
		DPI_VIDEO_1280x720p_60Hz,	/* 2 */
		DPI_VIDEO_1280x720p_50Hz,	/* 3 */
		DPI_VIDEO_1920x1080i_60Hz,	/* 4 */
		DPI_VIDEO_1920x1080i_50Hz,	/* 5 */
		DPI_VIDEO_1920x1080p_30Hz,	/* 6 */
		DPI_VIDEO_1920x1080p_25Hz,	/* 7 */
		DPI_VIDEO_1920x1080p_24Hz,	/* 8 */
		DPI_VIDEO_1920x1080p_23Hz,	/* 9 */
		DPI_VIDEO_1920x1080p_29Hz,	/* a */
		DPI_VIDEO_1920x1080p_60Hz,	/* b */
		DPI_VIDEO_1920x1080p_50Hz,	/* c */

		DPI_VIDEO_1280x720p3d_60Hz,	/* d */
		DPI_VIDEO_1280x720p3d_50Hz,	/* e */
		DPI_VIDEO_1920x1080i3d_60Hz,	/* f */
		DPI_VIDEO_1920x1080i3d_50Hz,	/* 10 */
		DPI_VIDEO_1920x1080p3d_24Hz,	/* 11 */
		DPI_VIDEO_1920x1080p3d_23Hz,	/* 12 */

		/*the 2160 mean 3840x2160 */
		DPI_VIDEO_2160P_23_976HZ,	/* 13 */
		DPI_VIDEO_2160P_24HZ,	/* 14 */
		DPI_VIDEO_2160P_25HZ,	/* 15 */
		DPI_VIDEO_2160P_29_97HZ,	/* 16 */
		DPI_VIDEO_2160P_30HZ,	/* 17 */
		/*the 2161 mean 4096x2160 */
		DPI_VIDEO_2161P_24HZ,	/* 18 */

		DPI_VIDEO_RESOLUTION_NUM
	};

	enum COLOR_SPACE_T {
		RGB = 0,
		RGB_FULL,
		YCBCR_444,
		YCBCR_422,
		XV_YCC,
		YCBCR_444_FULL,
		YCBCR_422_FULL,
	};

	struct LCD_REG_WROI_CON {
		unsigned RGB_ORDER:1;
		unsigned BYTE_ORDER:1;
		unsigned PADDING:1;
		unsigned DATA_FMT:3;
		unsigned IF_FMT:2;
		unsigned COMMAND:5;
		unsigned rsv_13:2;
		unsigned ENC:1;
		unsigned rsv_16:8;
		unsigned SEND_RES_MODE:1;
		unsigned IF_24:1;
		unsigned rsv_6:6;
	};

	int ddp_dpi_stop(enum DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_dump(enum DISP_MODULE_ENUM module, int level);
	int ddp_dpi_start(enum DISP_MODULE_ENUM module, void *cmdq);
	int ddp_dpi_init(enum DISP_MODULE_ENUM module, void *cmdq);
	int ddp_dpi_deinit(enum DISP_MODULE_ENUM module, void *cmdq_handle);
	int ddp_dpi_config(enum DISP_MODULE_ENUM module, struct disp_ddp_path_config *config,
			   void *cmdq_handle);
	int ddp_dpi_trigger(enum DISP_MODULE_ENUM module, void *cmdq);
	int ddp_dpi_ioctl(enum DISP_MODULE_ENUM module, void *cmdq_handle, unsigned int ioctl_cmd,
			  unsigned long *params);
	enum DPI_STATUS ddp_dpi_EnableColorBar(enum DISP_MODULE_ENUM module);
	unsigned int ddp_dpi_get_cur_addr(bool rdma_mode, int layerid);
	enum DPI_STATUS ddp_dpi_3d_ctrl(enum DISP_MODULE_ENUM module, struct cmdqRecStruct cmdq, bool fg3DFrame);

	enum DPI_STATUS ddp_dpi_config_colorspace(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					     uint8_t ColorSpace, uint8_t HDMI_Res);
	enum DPI_STATUS ddp_dpi_yuv422_setting(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					  uint32_t uvsw);
	enum DPI_STATUS ddp_dpi_clpf_setting(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					uint8_t clpfType, bool roundingEnable, uint32_t clpfen);
	enum DPI_STATUS ddp_dpi_vsync_lr_enable(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					   uint32_t vs_lo_en, uint32_t vs_le_en, uint32_t vs_ro_en,
					   uint32_t vs_re_en);
	enum DPI_STATUS ddp_dpi_ConfigVsync_LEVEN(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					     uint32_t pulseWidth, uint32_t backPorch,
					     uint32_t frontPorch, bool fgInterlace);
	enum DPI_STATUS ddp_dpi_ConfigVsync_RODD(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					    uint32_t pulseWidth, uint32_t backPorch,
					    uint32_t frontPorch);
	enum DPI_STATUS ddp_dpi_ConfigVsync_REVEN(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					     uint32_t pulseWidth, uint32_t backPorch,
					     uint32_t frontPorch, bool fgInterlace);
	void ddp_dpi_lvds_config(enum DISP_MODULE_ENUM module, LCM_DPI_FORMAT format,
				 void *cmdq_handle);
	bool ddp_dpi_is_top_filed(enum DISP_MODULE_ENUM module);

#ifdef __cplusplus
}
#endif
#endif				/* __DPI_DRV_H__ */
