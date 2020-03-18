/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Title          PowerVR Linux buffer sync interface
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__PVR_BUFFER_SYNC_H__)
#define __PVR_BUFFER_SYNC_H__

struct _RGXFWIF_DEV_VIRTADDR_;
struct _PMR_;
struct pvr_buffer_sync_context;
struct pvr_buffer_sync_append_data;

struct pvr_buffer_sync_context *
pvr_buffer_sync_context_create(void *dev_cookie);
void pvr_buffer_sync_context_destroy(struct pvr_buffer_sync_context *ctx);

int pvr_buffer_sync_append_start(struct pvr_buffer_sync_context *ctx,
				 u32 nr_pmrs,
				 struct _PMR_ **pmrs,
				 u32 *pmr_flags,
				 u32 nr_checks,
				 struct _RGXFWIF_DEV_VIRTADDR_ *check_ufo_addrs,
				 u32 *check_values,
				 u32 nr_updates,
				 struct _RGXFWIF_DEV_VIRTADDR_ *update_ufo_addrs,
				 u32 *update_values,
				 struct pvr_buffer_sync_append_data **data_out);
void pvr_buffer_sync_append_finish(struct pvr_buffer_sync_append_data *data);
void pvr_buffer_sync_append_abort(struct pvr_buffer_sync_append_data *data);
void pvr_buffer_sync_append_checks_get(struct pvr_buffer_sync_append_data *data,
				       u32 *nr_checks_out,
				       struct _RGXFWIF_DEV_VIRTADDR_ **check_ufo_addrs_out,
				       u32 **check_values_out);
void pvr_buffer_sync_append_updates_get(struct pvr_buffer_sync_append_data *data,
					u32 *nr_updates_out,
					struct _RGXFWIF_DEV_VIRTADDR_ **update_ufo_addrs_out,
					u32 **update_values_out);

void pvr_buffer_sync_wait_handle_get(struct pvr_buffer_sync_context *ctx,
				     struct _PMR_ *pmr,
				     void **wait_handle);
void pvr_buffer_sync_wait_handle_put(void *wait_handle);
int pvr_buffer_sync_wait(void *wait_handle, bool intr, unsigned long timeout);

#if defined(PVRSRV_USE_SYNC_CHECKPOINTS)
int pvr_buffer_sync_resolve_and_create_fences(struct pvr_buffer_sync_context *ctx,
										  u32 nr_pmrs,
										  struct _PMR_ **pmrs,
										  u32 *pmr_flags,
										  u32 *nr_checks,
										  PSYNC_CHECKPOINT **fence_checkpoint_handles,
										  PSYNC_CHECKPOINT *update_checkpoint_handle,
										  struct pvr_buffer_sync_append_data **data_out);

void pvr_buffer_sync_kick_succeeded(struct pvr_buffer_sync_append_data *data_in);
void pvr_buffer_sync_kick_failed(struct pvr_buffer_sync_append_data *data_in);
#endif /* defined(PVRSRV_USE_SYNC_CHECKPOINTS) */
#endif /* !defined(__PVR_BUFFER_SYNC_H__) */
