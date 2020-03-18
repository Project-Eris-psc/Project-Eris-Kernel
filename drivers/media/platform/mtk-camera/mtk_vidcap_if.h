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

#ifndef _VID_CAP_IF_H_
#define _VID_CAP_IF_H_

enum vidcap_get_param_type {
	GET_PARAM
};

enum vidcap_set_param_type {
	SET_PARAM_FRAME_SIZE
};

int  vidcap_if_init(void *ctx);

void vidcap_if_deinit(void *ctx);

int  vidcap_if_start_stream(void *ctx);

int  vidcap_if_stop_stream(void *ctx);

int  vidcap_if_init_buffer(void *ctx, void *fb);

int  vidcap_if_deinit_buffer(void *ctx, void *fb);
int  vidcap_if_capture(void *ctx, void *fb);

int  vidcap_if_get_param(void *ctx,
			enum vidcap_get_param_type type,
			void *out);

int  vidcap_if_set_param(void *ctx,
			enum vidcap_set_param_type type,
			void *in);

#endif /*_VID_CAP_IF_H_*/
