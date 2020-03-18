/*
 * Copyright (c) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VIDCAP_DRV_BASE_
#define _VIDCAP_DRV_BASE_

#include "mtk_vidcap_if.h"

struct mtk_vidcap_if {
	int (*init)(void *ctx, unsigned long *handle);

	int (*capture)(unsigned long handle, void *fb);

	int (*start_stream)(unsigned long handle);

	int (*init_buffer)(unsigned long handle, void *fb);

	int (*deinit_buffer)(unsigned long handle, void *fb);

	int (*get_param)(unsigned long handle, enum vidcap_get_param_type type,
			 void *out);

	int (*set_param)(unsigned long handle, enum vidcap_set_param_type type,
			 void *in);

	int (*stop_stream)(unsigned long handle);

	void (*deinit)(unsigned long handle);
};

#endif /*_VIDCAP_DRV_BASE_*/
