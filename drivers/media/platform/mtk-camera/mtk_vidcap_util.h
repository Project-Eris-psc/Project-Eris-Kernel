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

#ifndef _MTK_VIDCAP_UTIL_H_
#define _MTK_VIDCAP_UTIL_H_

extern int mtk_vidcap_dbg_level;

enum capture_buffer_type {
	BUFFER_STEREO = 0,
	BUFFER_MAIN1  = 1,
	BUFFER_MAIN2  = 2,
};

enum capture_buffer_status {
	BUFFER_FILLED = 0,
	BUFFER_EMPTY  = 1,
	BUFFER_ERROR  = 2,
};

struct plane_buffer {
	size_t size;
	size_t payload;
	void   *vaddr;
	dma_addr_t dma_addr;
	struct dma_buf *dmabuf;
};

struct mtk_vidcap_mem {
	unsigned int status;
	unsigned int num_planes;
	struct list_head list;
	enum capture_buffer_type type;
	struct plane_buffer planes[VIDEO_MAX_PLANES];
};

#define DEBUG	1

#if defined(DEBUG)

#define mtk_vidcap_debug(level, fmt, args...)				 \
	do {								 \
		if (mtk_vidcap_dbg_level >= level)			 \
			pr_info("[MTK_VCAP] level=%d %s(),%d: " fmt "\n",\
				level, __func__, __LINE__, ##args);	 \
	} while (0)

#define mtk_vidcap_err(fmt, args...)                \
	pr_err("[MTK_VCAP][ERROR] %s:%d: " fmt "\n", __func__, __LINE__, \
	       ##args)


#define mtk_vidcap_debug_enter()  mtk_vidcap_debug(3, "+")
#define mtk_vidcap_debug_leave()  mtk_vidcap_debug(3, "-")

#else

#define mtk_vidcap_debug(level, fmt, args...)
#define mtk_vidcap_err(fmt, args...)
#define mtk_vidcap_debug_enter()
#define mtk_vidcap_debug_leave()

#endif

#endif /*_MTK_VIDCAP_UTIL_H_*/
