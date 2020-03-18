/*
 * MediaTek Controls Header
 *
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
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

#ifndef __UAPI_MTK_VCU_CONTROLS_H__
#define __UAPI_MTK_VCU_CONTROLS_H__

#define SHARE_BUF_SIZE 48

#define VCUD_SET_OBJECT	_IOW('v', 0, struct share_obj)
#define VCUD_MVA_ALLOCATION	_IOWR('v', 1, struct mem_obj)
#define VCUD_MVA_FREE		_IOWR('v', 2, struct mem_obj)
#define VCUD_CACHE_FLUSH_ALL	_IOWR('v', 3, struct mem_obj)

#define COMPAT_VCUD_SET_OBJECT	_IOW('v', 0, struct share_obj)
#define COMPAT_VCUD_MVA_ALLOCATION	_IOWR('v', 1, struct compat_mem_obj)
#define COMPAT_VCUD_MVA_FREE		_IOWR('v', 2, struct compat_mem_obj)
#define COMPAT_VCUD_CACHE_FLUSH_ALL	_IOWR('v', 3, struct compat_mem_obj)

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @iova:	iova of buffer
 * @len:	buffer length
 * @va: kernel virtual address
 */
struct mem_obj {
	unsigned long iova;
	unsigned long len;
	u64 va;
};

#if IS_ENABLED(CONFIG_COMPAT)
struct compat_mem_obj {
	compat_ulong_t iova;
	compat_ulong_t len;
	compat_u64 va;
};
#endif

/**
 * struct share_obj - DTCM (Data Tightly-Coupled Memory) buffer shared with
 *		      AP and VCU
 *
 * @id:		IPI id
 * @len:	share buffer length
 * @share_buf:	share buffer data
 */
struct share_obj {
	s32 id;
	u32 len;
	unsigned char share_buf[SHARE_BUF_SIZE];
};

#endif

