/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
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

#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_drv_if.h"
#include "mtk_vcodec_dec_pm.h"

#define OUT_FMT_IDX	4
#define CAP_FMT_IDX	0

#define MTK_VDEC_MIN_W	64U
#define MTK_VDEC_MIN_H	64U
#define DFT_CFG_WIDTH	MTK_VDEC_MIN_W
#define DFT_CFG_HEIGHT	MTK_VDEC_MIN_H

static struct mtk_video_fmt mtk_video_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_MT21C,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT2110T,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT2110R,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21C10T,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
		{
		.fourcc = V4L2_PIX_FMT_MT21C10R,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21CS,
		.type = MTK_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21S,
		.type = MTK_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21S10T,
		.type = MTK_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21S10R,
		.type = MTK_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21CS10T,
		.type = MTK_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21CS10R,
		.type = MTK_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_H264,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_H265,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_VP8,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG1,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_H263,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_S263,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_XVID,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_DIVX3,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_DIVX4,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_DIVX5,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_DIVX6,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_WMV1,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_WMV2,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_WMV3,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_WVC1,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_WMVA,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_RV30,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_RV40,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
};

static const struct mtk_codec_framesizes mtk_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_H265,
		.stepwise = {  MTK_VDEC_MIN_W, VCODEC_DEC_4K_CODED_WIDTH, 16,
				MTK_VDEC_MIN_H, VCODEC_DEC_4K_CODED_HEIGHT, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_VP8,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_VP9,
		.stepwise = {  1, MTK_VDEC_MAX_W, 16, 1, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_MPEG1,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_MPEG2,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_MPEG4,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_H263,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_S263,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_XVID,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_DIVX3,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_DIVX4,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_DIVX5,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_DIVX6,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_WMV1,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_WMV2,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_WMV3,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_WVC1,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_WMVA,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_RV30,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_RV40,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(mtk_vdec_framesizes)
#define NUM_FORMATS ARRAY_SIZE(mtk_video_formats)

static struct mtk_video_fmt *mtk_vdec_find_format(struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == f->fmt.pix_mp.pixelformat)
			return fmt;
	}

	return NULL;
}

static struct mtk_video_fmt *mtk_find_fmt_by_pixel(unsigned int pixelformat)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == pixelformat)
			return fmt;
	}

	return NULL;
}

static struct mtk_q_data *mtk_vdec_get_q_data(struct mtk_vcodec_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[MTK_Q_DATA_SRC];

	return &ctx->q_data[MTK_Q_DATA_DST];
}

/*
 * This function tries to clean all display buffers, the buffers will return
 * in display order.
 * Note the buffers returned from codec driver may still be in driver's
 * reference list.
 */
static struct vb2_buffer *get_display_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vdec_fb *disp_frame_buffer = NULL;
	struct mtk_video_dec_buf *dstbuf;
	unsigned int i = 0;
	unsigned int num_planes = 0;

	mtk_v4l2_debug(4, "[%d]", ctx->id);
	if (vdec_if_get_param(ctx,
			GET_PARAM_DISP_FRAME_BUFFER,
			&disp_frame_buffer)) {
		mtk_v4l2_err("[%d]Cannot get param : GET_PARAM_DISP_FRAME_BUFFER",
			ctx->id);
		return NULL;
	}

	if (disp_frame_buffer == NULL) {
		mtk_v4l2_debug(4, "No display frame buffer");
		return NULL;
	}

	dstbuf = container_of(disp_frame_buffer, struct mtk_video_dec_buf,
				frame_buffer);
	num_planes = dstbuf->vb.vb2_buf.num_planes;
	mutex_lock(&ctx->lock);
	if (dstbuf->used) {
		for (i = 0; i < num_planes; i++) {
			vb2_set_plane_payload(&dstbuf->vb.vb2_buf, i,
						ctx->picinfo.fb_sz[i]);
		}

		dstbuf->ready_to_display = true;

		mtk_v4l2_debug(2,
				"[%d]status=%x queue id=%d to done_list %d",
				ctx->id, disp_frame_buffer->status,
				dstbuf->vb.vb2_buf.index,
				dstbuf->queued_in_vb2);

		v4l2_m2m_buf_done(&dstbuf->vb, VB2_BUF_STATE_DONE);
		ctx->decoded_frame_cnt++;
	}
	mutex_unlock(&ctx->lock);
	return &dstbuf->vb.vb2_buf;
}

/*
 * This function tries to clean all capture buffers that are not used as
 * reference buffers by codec driver any more
 * In this case, we need re-queue buffer to vb2 buffer if user space
 * already returns this buffer to v4l2 or this buffer is just the output of
 * previous sps/pps/resolution change decode, or do nothing if user
 * space still owns this buffer
 */
static struct vb2_buffer *get_free_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_video_dec_buf *dstbuf;
	struct vdec_fb *free_frame_buffer = NULL;

	if (vdec_if_get_param(ctx,
				GET_PARAM_FREE_FRAME_BUFFER,
				&free_frame_buffer)) {
		mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
		return NULL;
	}
	if (free_frame_buffer == NULL) {
		mtk_v4l2_debug(4, " No free frame buffer");
		return NULL;
	}

	mtk_v4l2_debug(4, "[%d] tmp_frame_addr = 0x%p",
			ctx->id, free_frame_buffer);

	dstbuf = container_of(free_frame_buffer, struct mtk_video_dec_buf,
				frame_buffer);

	mutex_lock(&ctx->lock);
	if (dstbuf->used) {
		if ((dstbuf->queued_in_vb2) &&
		    (dstbuf->queued_in_v4l2) &&
		    (free_frame_buffer->status == FB_ST_FREE)) {
			/*
			 * After decode sps/pps or non-display buffer, we don't
			 * need to return capture buffer to user space, but
			 * just re-queue this capture buffer to vb2 queue.
			 * This reduce overheads that dq/q unused capture
			 * buffer. In this case, queued_in_vb2 = true.
			 */
			mtk_v4l2_debug(2,
				"[%d]status=%x queue id=%d to rdy_queue %d",
				ctx->id, free_frame_buffer->status,
				dstbuf->vb.vb2_buf.index,
				dstbuf->queued_in_vb2);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->vb);
		} else if ((dstbuf->queued_in_vb2 == false) &&
			   (dstbuf->queued_in_v4l2 == true)) {
			/*
			 * If buffer in v4l2 driver but not in vb2 queue yet,
			 * and we get this buffer from free_list, it means
			 * that codec driver do not use this buffer as
			 * reference buffer anymore. We should q buffer to vb2
			 * queue, so later work thread could get this buffer
			 * for decode. In this case, queued_in_vb2 = false
			 * means this buffer is not from previous decode
			 * output.
			 */
			mtk_v4l2_debug(2,
					"[%d]status=%x queue id=%d to rdy_queue",
					ctx->id, free_frame_buffer->status,
					dstbuf->vb.vb2_buf.index);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->vb);
			dstbuf->queued_in_vb2 = true;
		} else {
			/*
			 * Codec driver do not need to reference this capture
			 * buffer and this buffer is not in v4l2 driver.
			 * Then we don't need to do any thing, just add log when
			 * we need to debug buffer flow.
			 * When this buffer q from user space, it could
			 * directly q to vb2 buffer
			 */
			mtk_v4l2_debug(4, "[%d]status=%x err queue id=%d %d %d",
					ctx->id, free_frame_buffer->status,
					dstbuf->vb.vb2_buf.index,
					dstbuf->queued_in_vb2,
					dstbuf->queued_in_v4l2);
		}
		dstbuf->used = false;
	}
	mutex_unlock(&ctx->lock);
	return &dstbuf->vb.vb2_buf;
}

static void clean_display_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_display_buffer(ctx);
	} while (framptr);
}

static void clean_free_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_free_buffer(ctx);
	} while (framptr);
}

static void mtk_vdec_queue_res_chg_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);
}

static void mtk_vdec_queue_stop_play_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_eos = {
		.type = V4L2_EVENT_EOS,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_eos);
}

static void mtk_vdec_flush_decoder(struct mtk_vcodec_ctx *ctx)
{
	unsigned int src_chg = 0;
	int ret = 0;

	ret = vdec_if_decode(ctx, NULL, NULL, &src_chg);
	if (ret)
		mtk_v4l2_err("DecodeFinal failed, ret=%d", ret);

	clean_display_buffer(ctx);
	clean_free_buffer(ctx);
}

static void mtk_vdec_pic_info_update(struct mtk_vcodec_ctx *ctx)
{
	unsigned int dpbsize = 0;
	int ret;

	if (vdec_if_get_param(ctx,
				GET_PARAM_PIC_INFO,
				&ctx->last_decoded_picinfo)) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
				ctx->id);
		return;
	}

	if (ctx->last_decoded_picinfo.pic_w == 0 ||
		ctx->last_decoded_picinfo.pic_h == 0 ||
		ctx->last_decoded_picinfo.buf_w == 0 ||
		ctx->last_decoded_picinfo.buf_h == 0) {
		mtk_v4l2_err("Cannot get correct pic info");
		return;
	}

	if ((ctx->last_decoded_picinfo.pic_w == ctx->picinfo.pic_w) ||
	    (ctx->last_decoded_picinfo.pic_h == ctx->picinfo.pic_h))
		return;

	mtk_v4l2_debug(1,
			"[%d]-> new(%d,%d), old(%d,%d), bit(%d) real(%d,%d)",
			ctx->id, ctx->last_decoded_picinfo.pic_w,
			ctx->last_decoded_picinfo.pic_h,
			ctx->picinfo.pic_w, ctx->picinfo.pic_h,
			ctx->picinfo.bitdepth,
			ctx->last_decoded_picinfo.buf_w,
			ctx->last_decoded_picinfo.buf_h);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		mtk_v4l2_err("Incorrect dpb size, ret=%d", ret);

	ctx->dpb_size = dpbsize;
}

static void mtk_vdec_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx = container_of(work, struct mtk_vcodec_ctx,
				decode_work);
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vb2_buffer *src_buf, *dst_buf;
	struct mtk_vcodec_mem buf;
	struct vdec_fb *pfb;
	unsigned int src_chg = 0;
	bool res_chg = false;
	bool mtk_vcodec_unsupport = false;
	int ret;
	unsigned int i = 0;
	unsigned int num_planes;
	struct timeval worktvstart;
	struct timeval worktvstart1;
	struct timeval vputvend;
	struct mtk_video_dec_buf *dst_buf_info, *src_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2;
	unsigned int fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;

	if (ctx->state != MTK_STATE_HEADER) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, " %d", ctx->state);
		return;
	}

	do_gettimeofday(&worktvstart);
	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] src_buf empty!!", ctx->id);
		return;
	}

	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_buf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] dst_buf empty!!", ctx->id);
		return;
	}

	src_vb2_v4l2 = container_of(src_buf, struct vb2_v4l2_buffer, vb2_buf);
	src_buf_info = container_of(src_vb2_v4l2, struct mtk_video_dec_buf, vb);

	dst_vb2_v4l2 = container_of(dst_buf, struct vb2_v4l2_buffer, vb2_buf);
	dst_buf_info = container_of(dst_vb2_v4l2, struct mtk_video_dec_buf, vb);

	pfb = &dst_buf_info->frame_buffer;
	num_planes = dst_vb2_v4l2->vb2_buf.num_planes;
	pfb->num_planes = num_planes;
	pfb->index = dst_buf->index;

	for (i = 0; i < num_planes; i++) {
		pfb->fb_base[i].va = vb2_plane_vaddr(dst_buf, i);
#ifdef CONFIG_VB2_MEDIATEK_DMA_SG
		pfb->fb_base[i].dma_addr =
			mtk_dma_sg_plane_dma_addr(dst_buf, i);
#else
		pfb->fb_base[i].dma_addr =
			vb2_dma_contig_plane_dma_addr(dst_buf, i);
#endif
		pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
		pfb->fb_base[i].length = dst_buf->planes[i].length;
		pfb->fb_base[i].dmabuf = dst_buf->planes[i].dbuf;
	}

	pfb->status = 0;
	mtk_v4l2_debug(4, "===>[%d] vdec_if_decode() ===>", ctx->id);

	mtk_v4l2_debug(4,
			"id=%d Framebuf  pfb=%p VA=%p Y_DMA=%pad C_DMA=%pad Size=%zx",
			dst_buf->index, pfb,
			pfb->fb_base[0].va, &pfb->fb_base[0].dma_addr,
			&pfb->fb_base[1].dma_addr, pfb->fb_base[0].size);

	if (src_buf_info->lastframe) {
		/* update src buf status */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		src_buf_info->lastframe = false;
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_DONE);

		/* update dst buf status */
		dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		dst_buf_info->used = false;

		vdec_if_decode(ctx, NULL, NULL, &src_chg);
		clean_display_buffer(ctx);
		for (i = 0; i < pfb->num_planes; i++)
			vb2_set_plane_payload(&dst_buf_info->vb.vb2_buf, i, 0);
		v4l2_m2m_buf_done(&dst_buf_info->vb, VB2_BUF_STATE_DONE);
		clean_free_buffer(ctx);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return;
	}
	buf.va = vb2_plane_vaddr(src_buf, 0);
	buf.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	buf.size = (size_t)src_buf->planes[0].bytesused;
	buf.length = (size_t)src_buf->planes[0].length;
	buf.dmabuf = src_buf->planes[0].dbuf;

	if (buf.va == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_err("[%d] id=%d src_addr is NULL!!",
				ctx->id, src_buf->index);
		return;
	}
	mtk_v4l2_debug(4, "[%d] Bitstream VA=%p DMA=%pad Size=%zx vb=%p",
			ctx->id, buf.va, &buf.dma_addr, buf.size, src_buf);
	dst_buf_info->vb.timestamp
			= src_buf_info->vb.timestamp;
	dst_buf_info->vb.timecode
			= src_buf_info->vb.timecode;
	mutex_lock(&ctx->lock);
	dst_buf_info->used = true;
	mutex_unlock(&ctx->lock);
	src_buf_info->used = true;
	do_gettimeofday(&worktvstart1);
	ret = vdec_if_decode(ctx, &buf, pfb, &src_chg);
	do_gettimeofday(&vputvend);
	mtk_vcodec_perf_log("vpud:%ld",
			(vputvend.tv_sec - worktvstart1.tv_sec) * 1000000 +
			(vputvend.tv_usec - worktvstart1.tv_usec));

	res_chg = ((src_chg & VDEC_RES_CHANGE) != 0U) ? true : false;
	mtk_vcodec_unsupport = ((src_chg & VDEC_HW_NOT_SUPPORT) != 0) ?
							true : false;

	if (ret < 0 || mtk_vcodec_unsupport) {
		mtk_v4l2_err(
			" <===[%d], src_buf[%d] last_frame = %d sz=0x%zx pts=%lu dst_buf[%d] vdec_if_decode() ret=%d src_chg=%d===>",
			ctx->id,
			src_buf->index,
			src_buf_info->lastframe,
			buf.size,
			src_buf_info->vb.timestamp.tv_sec * 1000000 + src_buf_info->vb.timestamp.tv_usec,
			dst_buf->index,
			ret, src_chg);
		if (mtk_vcodec_unsupport) {
			src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
			v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_DONE);
			/*
			 * If cncounter the src unsupport during play, egs:
			 * width/height, bitdepth, level, then teturn EOS event
			 * to  user to stop play it
			 */
			mtk_v4l2_err(" <=== [%d] vcodec not support the source!===>", ctx->id);
			ctx->state = MTK_STATE_FLUSH;
			mtk_vdec_queue_stop_play_event(ctx);
		} else {
			src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
			v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_ERROR);
		}
	} else if ((ret == 0) && ((fourcc == V4L2_PIX_FMT_RV40) ||
			(fourcc == V4L2_PIX_FMT_RV30) || (res_chg == false))) {
		/*
		 * we only return src buffer with VB2_BUF_STATE_DONE
		 * when decode success without resolution change except rv30/rv40.
		 */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_DONE);
	} else {
		mtk_v4l2_debug(1, "Need more capture buffer\n");
	}

	dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	clean_display_buffer(ctx);
	clean_free_buffer(ctx);

	if (ret == 0 && res_chg) {
		if ((fourcc == V4L2_PIX_FMT_RV40) || (fourcc == V4L2_PIX_FMT_RV30)) {
			/*
			 * For rv30/rv40 stream, encountering a resolution change the current frame
			 * needs to refer to the previous frame,so driver should not flush decode,
			 * but the driver should sends a V4L2_EVENT_SOURCE_CHANGE
			 * event for source change to app.
			 * app should set new crop to mdp directly.
			 */
			mtk_v4l2_debug(0, "RV30/RV40 RPR res_chg:%d\n", res_chg);
			mtk_vdec_queue_res_chg_event(ctx);
		} else {
			mtk_vdec_pic_info_update(ctx);
			/*
			 * On encountering a resolution change in the stream.
			 * The driver must first process and decode all
			 * remaining buffers from before the resolution change
			 * point, so call flush decode here
			 */
			mtk_vdec_flush_decoder(ctx);
			/*
			 * After all buffers containing decoded frames from
			 * before the resolution change point ready to be
			 * dequeued on the CAPTURE queue, the driver sends a
			 * V4L2_EVENT_SOURCE_CHANGE event for source change
			 * type V4L2_EVENT_SRC_CH_RESOLUTION
			 */
			mtk_vdec_queue_res_chg_event(ctx);
		}
	}

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
	do_gettimeofday(&vputvend);
	mtk_vcodec_perf_log("worker:%ld",
			(vputvend.tv_sec - worktvstart.tv_sec) * 1000000 +
			(vputvend.tv_usec - worktvstart.tv_usec));
}

void mtk_vdec_unlock(struct mtk_vcodec_ctx *ctx)
{
	mutex_unlock(&ctx->dev->dec_mutex);
}

void mtk_vdec_lock(struct mtk_vcodec_ctx *ctx)
{
	mutex_lock(&ctx->dev->dec_mutex);
}

void mtk_vcodec_dec_release(struct mtk_vcodec_ctx *ctx)
{
	vdec_if_deinit(ctx);
	ctx->state = MTK_STATE_FREE;
}

void mtk_vcodec_dec_set_default_params(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_q_data *q_data;

	ctx->m2m_ctx->q_lock = &ctx->dev->dev_mutex;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	INIT_WORK(&ctx->decode_work, mtk_vdec_worker);
	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	q_data = &ctx->q_data[MTK_Q_DATA_SRC];
	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->fmt = &mtk_video_formats[OUT_FMT_IDX];
	q_data->field = V4L2_FIELD_NONE;

	q_data->sizeimage[0] = DFT_CFG_WIDTH * DFT_CFG_HEIGHT;
	q_data->bytesperline[0] = 0;

	q_data = &ctx->q_data[MTK_Q_DATA_DST];
	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
	q_data->fmt = &mtk_video_formats[CAP_FMT_IDX];
	q_data->field = V4L2_FIELD_NONE;

	v4l_bound_align_image(&q_data->coded_width,
				MTK_VDEC_MIN_W,
				MTK_VDEC_MAX_W, 4,
				&q_data->coded_height,
				MTK_VDEC_MIN_H,
				MTK_VDEC_MAX_H, 5, 6);

	q_data->sizeimage[0] = q_data->coded_width * q_data->coded_height;
	q_data->bytesperline[0] = q_data->coded_width;
	q_data->sizeimage[1] = q_data->sizeimage[0] / 2;
	q_data->bytesperline[1] = q_data->coded_width;
}

static int mtk_vdec_set_param(struct mtk_vcodec_ctx *ctx)
{
	unsigned long in[8] = {0};

	mtk_v4l2_debug(4, "[%d] param change %d decode mode %d frame width %d frame height %d max width %d max height %d",
		ctx->id, ctx->dec_param_change, ctx->dec_params.decode_mode,
		ctx->dec_params.frame_size_width, ctx->dec_params.frame_size_height,
		ctx->dec_params.fixed_max_frame_size_width, ctx->dec_params.fixed_max_frame_size_height);

	if (ctx->dec_param_change & MTK_DEC_PARAM_DECODE_MODE) {
		in[0] = ctx->dec_params.decode_mode;
		if (vdec_if_set_param(ctx, SET_PARAM_DECODE_MODE, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_DECODE_MODE);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_FRAME_SIZE) {
		in[0] = ctx->dec_params.frame_size_width;
		in[1] = ctx->dec_params.frame_size_height;
		if (in[0] != 0 && in[1] != 0) {
			if (vdec_if_set_param(ctx, SET_PARAM_FRAME_SIZE, in) != 0) {
				mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
				return -EINVAL;
			}
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_FRAME_SIZE);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE) {
		in[0] = ctx->dec_params.fixed_max_frame_size_width;
		in[1] = ctx->dec_params.fixed_max_frame_size_height;
		if (in[0] != 0 && in[1] != 0) {
			if (vdec_if_set_param(ctx, SET_PARAM_SET_FIXED_MAX_OUTPUT_BUFFER, in) != 0) {
				mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
				return -EINVAL;
			}
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_CRC_PATH) {
		in[0] = (unsigned long)ctx->dec_params.crc_path;
		if (vdec_if_set_param(ctx, SET_PARAM_CRC_PATH, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_CRC_PATH);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_GOLDEN_PATH) {
		in[0] = (unsigned long)ctx->dec_params.golden_path;
		if (vdec_if_set_param(ctx, SET_PARAM_GOLDEN_PATH, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_GOLDEN_PATH);
	}

	return 0;
}

static u32 mtk_vdec_get_capture_fourcc(struct mtk_vcodec_ctx *ctx)
{
	u32 fourcc;
	unsigned int num_planes;
	struct mtk_q_data *src_q_data, *dst_q_data;
	struct vdec_pic_info *picinfo;

	fourcc = V4L2_PIX_FMT_MT21;
	picinfo = &ctx->picinfo;
	src_q_data = &ctx->q_data[MTK_Q_DATA_SRC];
	dst_q_data = &ctx->q_data[MTK_Q_DATA_DST];
	num_planes = dst_q_data->fmt->num_planes;

	if (num_planes == 1U) {
		fourcc = picinfo->ufo_mode == 0U ?
			V4L2_PIX_FMT_MT21S : V4L2_PIX_FMT_MT21CS;
		if (picinfo->bitdepth == 10U) {
			if (src_q_data->fmt->fourcc == V4L2_PIX_FMT_H264) {
				fourcc = picinfo->ufo_mode == 0U ?
					V4L2_PIX_FMT_MT21S10R :
					V4L2_PIX_FMT_MT21CS10R;
			} else {
				fourcc = picinfo->ufo_mode == 0U ?
					V4L2_PIX_FMT_MT21S10T :
					V4L2_PIX_FMT_MT21CS10T;
			}
		}
	} else if (num_planes == 2U) {
		fourcc = picinfo->ufo_mode == 0U ?
			V4L2_PIX_FMT_MT21 : V4L2_PIX_FMT_MT21C;
		if (picinfo->bitdepth == 10U) {
			if (src_q_data->fmt->fourcc == V4L2_PIX_FMT_H264) {
				fourcc = picinfo->ufo_mode == 0U ?
					V4L2_PIX_FMT_MT2110R :
					V4L2_PIX_FMT_MT21C10R;
			} else {
				fourcc = picinfo->ufo_mode == 0U ?
					V4L2_PIX_FMT_MT2110T :
					V4L2_PIX_FMT_MT21C10T;
			}
		}
	}

	return fourcc;
}

static int vidioc_vdec_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_video_dec_buf *mtkbuf;
	struct vb2_v4l2_buffer	*vb2_v4l2;

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_err("[%d] Call on QBUF after unrecoverable error",
				ctx->id);
		return -EIO;
	}

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
	vb = vq->bufs[buf->index];
	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);

	if ((buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    (buf->m.planes[0].bytesused == 0)) {
		mtkbuf->lastframe = true;
		mtk_v4l2_debug(1, "[%d] (%d) id=%d lastframe=%d (%d,%d, %d) vb=%p",
			 ctx->id, buf->type, buf->index,
			 mtkbuf->lastframe, buf->bytesused,
			 buf->m.planes[0].bytesused, buf->length,
			 vb);
	}

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_err("[%d] Call on DQBUF after unrecoverable error",
				ctx->id);
		return -EIO;
	}

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	strlcpy(cap->driver, MTK_VCODEC_DEC_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, MTK_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, MTK_PLATFORM_STR, sizeof(cap->card));

	cap->device_caps  = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vidioc_vdec_subscribe_evt(struct v4l2_fh *fh,
				     const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int vidioc_try_fmt(struct v4l2_format *f, struct mtk_video_fmt *fmt)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	unsigned int i;

	pix_fmt_mp->field = V4L2_FIELD_NONE;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pix_fmt_mp->num_planes = 1;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		int tmp_w, tmp_h;

		pix_fmt_mp->height = clamp(pix_fmt_mp->height,
					MTK_VDEC_MIN_H,
					MTK_VDEC_MAX_H);
		pix_fmt_mp->width = clamp(pix_fmt_mp->width,
					MTK_VDEC_MIN_W,
					MTK_VDEC_MAX_W);

		/*
		 * Find next closer width align 64, heign align 64, size align
		 * 64 rectangle
		 * Note: This only get default value, the real HW needed value
		 *       only available when ctx in MTK_STATE_HEADER state
		 */
		tmp_w = pix_fmt_mp->width;
		tmp_h = pix_fmt_mp->height;
		v4l_bound_align_image(&pix_fmt_mp->width,
					MTK_VDEC_MIN_W,
					MTK_VDEC_MAX_W, 6,
					&pix_fmt_mp->height,
					MTK_VDEC_MIN_H,
					MTK_VDEC_MAX_H, 6, 9);

		if (pix_fmt_mp->width < tmp_w &&
			(pix_fmt_mp->width + 64) <= MTK_VDEC_MAX_W)
			pix_fmt_mp->width += 64;
		if (pix_fmt_mp->height < tmp_h &&
			(pix_fmt_mp->height + 64) <= MTK_VDEC_MAX_H)
			pix_fmt_mp->height += 64;

		mtk_v4l2_debug(0,
			"before resize width=%d, height=%d, after resize width=%d, height=%d, sizeimage=%d",
			tmp_w, tmp_h, pix_fmt_mp->width,
			pix_fmt_mp->height,
			pix_fmt_mp->width * pix_fmt_mp->height);

		pix_fmt_mp->num_planes = fmt->num_planes;

		for (i = 0; i < pix_fmt_mp->num_planes; i++) {
			pix_fmt_mp->plane_fmt[i].sizeimage =
					(pix_fmt_mp->width *
					pix_fmt_mp->height) >> i;
			pix_fmt_mp->plane_fmt[i].bytesperline =
					pix_fmt_mp->width;
		}
	}

	for (i = 0; i < pix_fmt_mp->num_planes; i++)
		memset(&(pix_fmt_mp->plane_fmt[i].reserved[0]), 0x0,
			   sizeof(pix_fmt_mp->plane_fmt[0].reserved));

	pix_fmt_mp->flags = 0;
	memset(&pix_fmt_mp->reserved, 0x0, sizeof(pix_fmt_mp->reserved));
	return 0;
}

static int vidioc_vdec_g_crop(struct file *file, void *priv,
			 struct v4l2_crop *cr)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (ctx->state < MTK_STATE_HEADER)
		return -EINVAL;

	if ((ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H264) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H265) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP8) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP9) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG1) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG2) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG4) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H263) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_S263) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_XVID) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_DIVX3) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_DIVX4) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_DIVX5) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_DIVX6) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMV1) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMV2) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMV3) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WVC1) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMVA) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_RV30) ||
		(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_RV40)) {
		if (vdec_if_get_param(ctx, GET_PARAM_CROP_INFO, cr) != 0) {
			mtk_v4l2_debug(2, "[%d]Error!! Cannot get param : GET_PARAM_CROP_INFO ERR",
					 ctx->id);
			cr->c.left = 0;
			cr->c.top = 0;
			cr->c.width = ctx->picinfo.pic_w;
			cr->c.height = ctx->picinfo.pic_h;
		}
		mtk_v4l2_debug(2, "Cropping info: l=%d t=%d w=%d h=%d",
				 cr->c.left, cr->c.top, cr->c.width,
				 cr->c.height);
	} else {
		cr->c.left = 0;
		cr->c.top = 0;
		cr->c.width = ctx->picinfo.pic_w;
		cr->c.height = ctx->picinfo.pic_h;
		mtk_v4l2_debug(2, "Cropping info: w=%d h=%d fw=%d fh=%d",
				 cr->c.width, cr->c.height, ctx->picinfo.buf_w,
				 ctx->picinfo.buf_h);
	}

	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;

	fmt = mtk_vdec_find_format(f);
	if (!fmt) {
		f->fmt.pix.pixelformat = mtk_video_formats[CAP_FMT_IDX].fourcc;
		fmt = mtk_vdec_find_format(f);
	}

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct mtk_video_fmt *fmt;

	fmt = mtk_vdec_find_format(f);
	if (!fmt) {
		f->fmt.pix.pixelformat = mtk_video_formats[OUT_FMT_IDX].fourcc;
		fmt = mtk_vdec_find_format(f);
	}

	if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
		mtk_v4l2_err("sizeimage of output format must be given");
		return -EINVAL;
	}

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_vdec_g_selection(struct file *file, void *priv,
			struct v4l2_selection *s)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct mtk_q_data *q_data;

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	q_data = &ctx->q_data[MTK_Q_DATA_DST];

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.pic_w;
		s->r.height = ctx->picinfo.pic_h;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.buf_w;
		s->r.height = ctx->picinfo.buf_h;
		break;
	case V4L2_SEL_TGT_COMPOSE:
		if (vdec_if_get_param(ctx, GET_PARAM_CROP_INFO, &(s->r))) {
			/* set to default value if header info not ready yet*/
			s->r.left = 0;
			s->r.top = 0;
			s->r.width = q_data->visible_width;
			s->r.height = q_data->visible_height;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ctx->state < MTK_STATE_HEADER) {
		/* set to default value if header info not ready yet*/
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = q_data->visible_width;
		s->r.height = q_data->visible_height;
		return 0;
	}

	return 0;
}

static int vidioc_vdec_s_selection(struct file *file, void *priv,
				struct v4l2_selection *s)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.pic_w;
		s->r.height = ctx->picinfo.pic_h;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_vdec_s_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp;
	struct mtk_q_data *q_data;
	int ret = 0;
	struct mtk_video_fmt *fmt;
	uint32_t size[2];

	mtk_v4l2_debug(4, "[%d]", ctx->id);

	q_data = mtk_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	pix_mp = &f->fmt.pix_mp;
	if ((f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->out_q_ctx.q)) {
		mtk_v4l2_err("out_q_ctx buffers already requested");
		ret = -EBUSY;
	}

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->cap_q_ctx.q)) {
		mtk_v4l2_err("cap_q_ctx buffers already requested");
		ret = -EBUSY;
	}

	fmt = mtk_vdec_find_format(f);
	if (fmt == NULL) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			f->fmt.pix.pixelformat =
				mtk_video_formats[OUT_FMT_IDX].fourcc;
			fmt = mtk_vdec_find_format(f);
		} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			f->fmt.pix.pixelformat =
				mtk_video_formats[CAP_FMT_IDX].fourcc;
			fmt = mtk_vdec_find_format(f);
		}
	}

	q_data->fmt = fmt;
	vidioc_try_fmt(f, q_data->fmt);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q_data->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
		q_data->coded_width = pix_mp->width;
		q_data->coded_height = pix_mp->height;
		size[0] = pix_mp->width;
		size[1] = pix_mp->height;

		ctx->colorspace = f->fmt.pix_mp.colorspace;
		ctx->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->quantization = f->fmt.pix_mp.quantization;
		ctx->xfer_func = f->fmt.pix_mp.xfer_func;

		if (ctx->state == MTK_STATE_FREE) {
			ret = vdec_if_init(ctx, q_data->fmt->fourcc);
			if (ret) {
				mtk_v4l2_err("[%d]: vdec_if_init() fail ret=%d",
					ctx->id, ret);
				return -EINVAL;
			}
			vdec_if_set_param(ctx, SET_PARAM_FRAME_SIZE, (void *) size);
			ctx->state = MTK_STATE_INIT;
		}
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		vdec_if_set_param(ctx, SET_PARAM_FB_NUM_PLANES,
			(void *) &q_data->fmt->num_planes);

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
				struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (fsize->index != 0)
		return -EINVAL;

	for (i = 0; i < NUM_SUPPORTED_FRAMESIZE; ++i) {
		if (fsize->pixel_format != mtk_vdec_framesizes[i].fourcc)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise = mtk_vdec_framesizes[i].stepwise;
		if (!(ctx->dev->dec_capability &
				VCODEC_CAPABILITY_4K_DISABLED)) {
			mtk_v4l2_debug(4, "4K is enabled");
			fsize->stepwise.max_width =
					VCODEC_DEC_4K_CODED_WIDTH;
			fsize->stepwise.max_height =
					VCODEC_DEC_4K_CODED_HEIGHT;
		}
		mtk_v4l2_debug(1, "%x, %d %d %d %d %d %d",
				ctx->dev->dec_capability,
				fsize->stepwise.min_width,
				fsize->stepwise.max_width,
				fsize->stepwise.step_width,
				fsize->stepwise.min_height,
				fsize->stepwise.max_height,
				fsize->stepwise.step_height);
		return 0;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool output_queue)
{
	struct mtk_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (output_queue && (mtk_video_formats[i].type != MTK_FMT_DEC))
			continue;
		if (!output_queue &&
			(mtk_video_formats[i].type != MTK_FMT_FRAME))
			continue;

		if (j == f->index)
			break;
		++j;
	}

	if (i == NUM_FORMATS)
		return -EINVAL;

	fmt = &mtk_video_formats[i];
	f->pixelformat = fmt->fourcc;

	return 0;
}

static int vidioc_vdec_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false);
}

static int vidioc_vdec_enum_fmt_vid_out_mplane(struct file *file, void *priv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true);
}

static int vidioc_vdec_g_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	u32	fourcc;
	unsigned int i = 0;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		mtk_v4l2_err("no vb2 queue for type=%d", f->type);
		return -EINVAL;
	}

	q_data = mtk_vdec_get_q_data(ctx, f->type);

	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->colorspace = ctx->colorspace;
	pix_mp->ycbcr_enc = ctx->ycbcr_enc;
	pix_mp->quantization = ctx->quantization;
	pix_mp->xfer_func = ctx->xfer_func;

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
	    (ctx->state >= MTK_STATE_HEADER)) {
		/* Until STREAMOFF is called on the CAPTURE queue
		 * (acknowledging the event), the driver operates as if
		 * the resolution hasn't changed yet.
		 * So we just return picinfo yet, and update picinfo in
		 * stop_streaming hook function
		 */
		for (i = 0; i < q_data->fmt->num_planes; i++) {
			q_data->sizeimage[i] = ctx->picinfo.fb_sz[i];
			q_data->bytesperline[i] =
				ctx->last_decoded_picinfo.buf_w;
		}
		q_data->coded_width = ctx->picinfo.buf_w;
		q_data->coded_height = ctx->picinfo.buf_h;

		/*
		 * Width and height are set to the dimensions
		 * of the movie, the buffer is bigger and
		 * further processing stages should crop to this
		 * rectangle.
		 */
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		if (fourcc == V4L2_PIX_FMT_RV30 ||
			fourcc == V4L2_PIX_FMT_RV40) {
			pix_mp->width = 1920;
			pix_mp->height = 1088;
		} else {
			pix_mp->width = q_data->coded_width;
			pix_mp->height = q_data->coded_height;
		}
		/*
		 * Set pixelformat to the format in which mt vcodec
		 * outputs the decoded frame
		 */
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;

		if (fourcc == V4L2_PIX_FMT_RV30 ||
			fourcc == V4L2_PIX_FMT_RV40) {
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->plane_fmt[i].bytesperline = 1920;
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		} else {
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->plane_fmt[i].bytesperline =
					q_data->bytesperline[i];
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		}

		mtk_v4l2_debug(1, "fourcc:(%d %d),bytesperline:%d,sizeimage:%d,%d,%d\n",
			ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc,
			q_data->fmt->fourcc,
			pix_mp->plane_fmt[0].bytesperline,
			pix_mp->plane_fmt[0].sizeimage,
			pix_mp->plane_fmt[1].bytesperline,
			pix_mp->plane_fmt[1].sizeimage);

	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * This is run on OUTPUT
		 * The buffer contains compressed image
		 * so width and height have no meaning.
		 * Assign value here to pass v4l2-compliance test
		 */
		pix_mp->width = q_data->visible_width;
		pix_mp->height = q_data->visible_height;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->num_planes = q_data->fmt->num_planes;
	} else {
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;

		if (fourcc == V4L2_PIX_FMT_RV30 ||
			fourcc == V4L2_PIX_FMT_RV40) {
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->width = 1920;
				pix_mp->height = 1088;
				pix_mp->plane_fmt[i].bytesperline = 1920;
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		} else {
			pix_mp->width = q_data->coded_width;
			pix_mp->height = q_data->coded_height;
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->plane_fmt[i].bytesperline =
					q_data->bytesperline[i];
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		}

		mtk_v4l2_debug(1, " [%d] type=%d state=%d Format information could not be read, not ready yet!",
			ctx->id, f->type, ctx->state);
	}

	return 0;
}

static int vb2ops_vdec_queue_setup(struct vb2_queue *vq,
				const void *parg,
				unsigned int *nbuffers,
				unsigned int *nplanes,
				unsigned int sizes[], void *alloc_ctxs[])
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_q_data *q_data;
	unsigned int i;

	q_data = mtk_vdec_get_q_data(ctx, vq->type);

	if (q_data == NULL) {
		mtk_v4l2_err("vq->type=%d err\n", vq->type);
		return -EINVAL;
	}

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
			alloc_ctxs[i] = ctx->dev->alloc_ctx;
		}
	} else {
		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			*nplanes = q_data->fmt->num_planes;
		else
			*nplanes = 1;

		for (i = 0; i < *nplanes; i++) {
			sizes[i] = q_data->sizeimage[i];
			alloc_ctxs[i] = ctx->dev->alloc_ctx;
		}
	}

	mtk_v4l2_debug(1,
			"[%d]\t type = %d, get %d plane(s), %d buffer(s) of size 0x%x 0x%x ",
			ctx->id, vq->type, *nplanes, *nbuffers,
			sizes[0], sizes[1]);

	return 0;
}

static int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_q_data *q_data;
	unsigned int i;

	mtk_v4l2_debug(4, "[%d] (%d) id=%d",
			ctx->id, vb->vb2_queue->type, vb->index);

	q_data = mtk_vdec_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			mtk_v4l2_err("data will not fit into plane %d (%lu < %d)",
				i, vb2_plane_size(vb, i),
				q_data->sizeimage[i]);
		}
	}

	return 0;
}

static void vb2ops_vdec_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_buffer *src_buf;
	struct mtk_vcodec_mem src_mem;
	unsigned int src_chg = 0;
	bool res_chg = false;
	bool mtk_vcodec_unsupport = false;
	bool wait_seq_header = false;
	int ret = 0;
	unsigned int i = 0;
	unsigned int dpbsize = 1;
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct mtk_video_dec_buf *buf = NULL;
	struct mtk_q_data *dst_q_data;
	u32 fourcc;

	mtk_v4l2_debug(4, "[%d] (%d) id=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb);
	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		vb2_v4l2 = to_vb2_v4l2_buffer(vb);
		buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
		mutex_lock(&ctx->lock);
		if (buf->used == false) {
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
			buf->queued_in_vb2 = true;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;
		} else {
			buf->queued_in_vb2 = false;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;
		}
		mutex_unlock(&ctx->lock);
		return;
	}

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));

	if (ctx->state != MTK_STATE_INIT) {
		mtk_v4l2_debug(4, "[%d] already init driver %d",
				ctx->id, ctx->state);
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (!src_buf) {
		mtk_v4l2_err("No src buffer");
		return;
	}

	src_mem.va = vb2_plane_vaddr(src_buf, 0);
	src_mem.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	src_mem.size = (size_t)src_buf->planes[0].bytesused;
	src_mem.length = (size_t)src_buf->planes[0].length;
	src_mem.dmabuf = src_buf->planes[0].dbuf;
    mtk_v4l2_debug(0, "[%d] %x %x %x %x %x %x %x %x %x %x %x \n",ctx->id,
        ((char*)src_mem.va)[0],
        ((char*)src_mem.va)[1],
        ((char*)src_mem.va)[2],
        ((char*)src_mem.va)[3],
        ((char*)src_mem.va)[4],
        ((char*)src_mem.va)[5],
        ((char*)src_mem.va)[6],
        ((char*)src_mem.va)[7],
        ((char*)src_mem.va)[8],
        ((char*)src_mem.va)[9],
        ((char*)src_mem.va)[10]);
	mtk_v4l2_debug(2,
			"[%d] buf id=%d va=%p dma=%pad size=%zx length=%zu dmabuf=%p",
			ctx->id, src_buf->index,
			src_mem.va, &src_mem.dma_addr,
			src_mem.size, src_mem.length, src_mem.dmabuf);

	ret = vdec_if_decode(ctx, &src_mem, NULL, &src_chg);

	/* src_chg bit0 for res change flag, bit1 for realloc mv buf flag,
	 * bit2 for not support flag, other bits are reserved
	 */
	res_chg = ((src_chg & VDEC_RES_CHANGE) != 0U) ? true : false;
	mtk_vcodec_unsupport = ((src_chg & VDEC_HW_NOT_SUPPORT) != 0) ?
							true : false;
	wait_seq_header = ((src_chg & VDEC_NEED_SEQ_HEADER) != 0U) ?
						true : false;
	if (ret || !res_chg || mtk_vcodec_unsupport || wait_seq_header) {
		/*
		 * fb == NULL menas to parse SPS/PPS header or
		 * resolution info in src_mem. Decode can fail
		 * if there is no SPS header or picture info
		 * in bs
		 */

		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf),
					VB2_BUF_STATE_DONE);
		mtk_v4l2_debug(ret ? 0 : 1,
			       "[%d] vdec_if_decode() src_buf=%d, size=%zu, fail=%d, res_chg=%d, mtk_vcodec_unsupport=%d",
			       ctx->id, src_buf->index,
			       src_mem.size, ret, res_chg, mtk_vcodec_unsupport);

		/* If not support the source, eg: w/h, bitdepth, level, we need to stop to play it */
		if (mtk_vcodec_unsupport) {
			mtk_v4l2_err("[%d]Error!! Codec driver not support the file!",
				ctx->id);
			mtk_vdec_queue_stop_play_event(ctx);
		}
		return;
	}

	if (res_chg) {
		mtk_v4l2_debug(3, "[%d] vdec_if_decode() res_chg: %d\n",
			ctx->id, res_chg);
		mtk_vdec_queue_res_chg_event(ctx);
	}

	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo)) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
				ctx->id);
		return;
	}

	dst_q_data = &ctx->q_data[MTK_Q_DATA_DST];
	ctx->last_decoded_picinfo = ctx->picinfo;
	fourcc = mtk_vdec_get_capture_fourcc(ctx);
	dst_q_data->fmt = mtk_find_fmt_by_pixel(fourcc);

	for (i = 0; i < dst_q_data->fmt->num_planes; i++) {
		dst_q_data->sizeimage[i] = ctx->picinfo.fb_sz[i];
		dst_q_data->bytesperline[i] = ctx->picinfo.buf_w;
	}

	mtk_v4l2_debug(2, "[%d] Init Vdec OK wxh=%dx%d pic wxh=%dx%d bitdepth:%d ufo:%d sz[0]=0x%x sz[1]=0x%x",
			ctx->id,
			ctx->picinfo.buf_w, ctx->picinfo.buf_h,
			ctx->picinfo.pic_w, ctx->picinfo.pic_h,
			ctx->picinfo.bitdepth, ctx->picinfo.ufo_mode,
			dst_q_data->sizeimage[0],
			dst_q_data->sizeimage[1]);

	mtk_v4l2_debug(2, "[%d] fmt %d, num_planes %d, fb_sz[0] %d, fb_sz[1] %d",
			ctx->id,
			dst_q_data->fmt->fourcc,
			dst_q_data->fmt->num_planes,
			ctx->picinfo.fb_sz[0],
			ctx->picinfo.fb_sz[1]);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		mtk_v4l2_err("[%d] GET_PARAM_DPB_SIZE fail=%d", ctx->id, ret);

	ctx->dpb_size = dpbsize;
	ctx->state = MTK_STATE_HEADER;
	mtk_v4l2_debug(1, "[%d] dpbsize=%d", ctx->id, ctx->dpb_size);

	mtk_vdec_set_param(ctx);
}

static void vb2ops_vdec_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2;
	struct mtk_video_dec_buf *buf;

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return;

	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
	mutex_lock(&ctx->lock);
	buf->queued_in_v4l2 = false;
	buf->queued_in_vb2 = false;
	mutex_unlock(&ctx->lock);
}

static int vb2ops_vdec_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
					struct vb2_v4l2_buffer, vb2_buf);
	struct mtk_video_dec_buf *buf = container_of(vb2_v4l2,
					struct mtk_video_dec_buf, vb);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = false;
		buf->ready_to_display = false;
		buf->queued_in_v4l2 = false;
	} else {
		buf->lastframe = false;
	}

	return 0;
}

static int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);

	if (ctx->state == MTK_STATE_FLUSH)
		ctx->state = MTK_STATE_HEADER;


	return 0;
}

static void vb2ops_vdec_stop_streaming(struct vb2_queue *q)
{
	struct vb2_buffer *src_buf = NULL, *dst_buf = NULL;
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	unsigned int i = 0;

	mtk_v4l2_debug(4, "[%d] (%d) state=(%x) ctx->decoded_frame_cnt=%d",
			ctx->id, q->type, ctx->state, ctx->decoded_frame_cnt);

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		while ((src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
			v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf),
					VB2_BUF_STATE_ERROR);
		return;
	}

	if (ctx->state >= MTK_STATE_HEADER) {

		/* Until STREAMOFF is called on the CAPTURE queue
		 * (acknowledging the event), the driver operates
		 * as if the resolution hasn't changed yet, i.e.
		 * VIDIOC_G_FMT< etc. return previous resolution.
		 * So we update picinfo here
		 */
		ctx->picinfo = ctx->last_decoded_picinfo;

		mtk_v4l2_debug(2,
				"[%d]-> new(%d,%d), old(%d,%d), real(%d,%d) bit:%d\n",
				ctx->id, ctx->last_decoded_picinfo.pic_w,
				ctx->last_decoded_picinfo.pic_h,
				ctx->picinfo.pic_w, ctx->picinfo.pic_h,
				ctx->last_decoded_picinfo.buf_w,
				ctx->last_decoded_picinfo.buf_h,
				ctx->picinfo.bitdepth);

		mtk_vdec_flush_decoder(ctx);
	}
	ctx->state = MTK_STATE_FLUSH;

	while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx))) {
		for (i = 0; i < dst_buf->num_planes; i++)
			vb2_set_plane_payload(dst_buf, i, 0);
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf),
					VB2_BUF_STATE_ERROR);
	}

}

static void m2mops_vdec_device_run(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;
	struct mtk_vcodec_dev *dev = ctx->dev;

	queue_work(dev->decode_workqueue, &ctx->decode_work);
}

static int m2mops_vdec_job_ready(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(4, "[%d]", ctx->id);

	if (ctx->state == MTK_STATE_ABORT)
		return 0;

	if ((ctx->last_decoded_picinfo.pic_w != ctx->picinfo.pic_w) ||
	    (ctx->last_decoded_picinfo.pic_h != ctx->picinfo.pic_h))
		return 0;

	if (ctx->state != MTK_STATE_HEADER)
		return 0;

	return 1;
}

static void m2mops_vdec_job_abort(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	ctx->state = MTK_STATE_ABORT;
}

static int mtk_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;
	static unsigned int value;

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= MTK_STATE_HEADER) {
			ctrl->val = ctx->dpb_size;
		} else {
			mtk_v4l2_debug(0, "Seqinfo not ready");
			ctrl->val = 0;
		}
		break;
	case V4L2_CID_MPEG_MTK_FRAME_INTERVAL:
		if (vdec_if_get_param(ctx, GET_PARAM_FRAME_INTERVAL, &value) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		ctrl->p_new.p_u32 = &value;
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_MTK_FRAME_INTERVAL val = %u", *(ctrl->p_new.p_u32));
		break;
	case V4L2_CID_MPEG_MTK_ERRORMB_MAP:
		if (vdec_if_get_param(ctx, GET_PARAM_ERRORMB_MAP, &value) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}

		ctrl->val = value;
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_MTK_ERRORMB_MAP val = %d",
				 ctrl->val);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int mtk_vdec_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);

	mtk_v4l2_debug(4, "[%d] id %d val %d array[0] %d array[1] %d",
		ctx->id, ctrl->id, ctrl->val,
		ctrl->p_new.p_u32[0], ctrl->p_new.p_u32[1]);

	if (!ctx->drv_handle)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_MTK_DECODE_MODE:
		ctx->dec_params.decode_mode = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_DECODE_MODE;
		break;
	case V4L2_CID_MPEG_MTK_FRAME_SIZE:
		ctx->dec_params.frame_size_width = ctrl->p_new.p_u32[0];
		ctx->dec_params.frame_size_height = ctrl->p_new.p_u32[1];
		ctx->dec_param_change |= MTK_DEC_PARAM_FRAME_SIZE;
		break;
	case V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER:
		ctx->dec_params.fixed_max_frame_size_width =
				 ctrl->p_new.p_u32[0];
		ctx->dec_params.fixed_max_frame_size_height = ctrl->p_new.p_u32[1];
		ctx->dec_param_change |= MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE;
		break;
	case V4L2_CID_MPEG_MTK_CRC_PATH:
		ctx->dec_params.crc_path = ctrl->p_new.p_char;
		ctx->dec_param_change |= MTK_DEC_PARAM_CRC_PATH;
		break;
	case V4L2_CID_MPEG_MTK_GOLDEN_PATH:
		ctx->dec_params.golden_path = ctrl->p_new.p_char;
		ctx->dec_param_change |= MTK_DEC_PARAM_GOLDEN_PATH;
		break;
	default:
		mtk_v4l2_err("ctrl-id=%d not support!", ctrl->id);
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops mtk_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = mtk_vdec_g_v_ctrl,
	.s_ctrl = mtk_vdec_s_ctrl,
};

int mtk_vcodec_dec_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, MTK_MAX_CTRLS_HINT);

	/* g_volatile_ctrl */
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				0, 32, 1, 1);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FRAME_INTERVAL,
				16666, 41719, 1, 33333);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_ERRORMB_MAP,
				0, 65535, 1, 0);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	/* s_ctrl */
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_DECODE_MODE,
				0, 32, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FRAME_SIZE,
				0, 65535, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER,
				0, 65535, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_CRC_PATH,
				0, 255, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_GOLDEN_PATH,
				0, 255, 1, 0);

	if (ctx->ctrl_hdl.error) {
		mtk_v4l2_err("Adding control failed %d",
				ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	return 0;
}

static void m2mops_vdec_lock(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(4, "[%d]", ctx->id);
	mutex_lock(&ctx->dev->dev_mutex);
}

static void m2mops_vdec_unlock(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(4, "[%d]", ctx->id);
	mutex_unlock(&ctx->dev->dev_mutex);
}

const struct v4l2_m2m_ops mtk_vdec_m2m_ops = {
	.device_run	= m2mops_vdec_device_run,
	.job_ready	= m2mops_vdec_job_ready,
	.job_abort	= m2mops_vdec_job_abort,
	.lock		= m2mops_vdec_lock,
	.unlock		= m2mops_vdec_unlock,
};

static const struct vb2_ops mtk_vdec_vb2_ops = {
	.queue_setup	= vb2ops_vdec_queue_setup,
	.buf_prepare	= vb2ops_vdec_buf_prepare,
	.buf_queue	= vb2ops_vdec_buf_queue,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.buf_init	= vb2ops_vdec_buf_init,
	.buf_finish	= vb2ops_vdec_buf_finish,
	.start_streaming	= vb2ops_vdec_start_streaming,
	.stop_streaming	= vb2ops_vdec_stop_streaming,
};

const struct v4l2_ioctl_ops mtk_vdec_ioctl_ops = {
	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,
	.vidioc_reqbufs		= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,

	.vidioc_qbuf		= vidioc_vdec_qbuf,
	.vidioc_dqbuf		= vidioc_vdec_dqbuf,

	.vidioc_try_fmt_vid_cap_mplane	= vidioc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= vidioc_try_fmt_vid_out_mplane,

	.vidioc_s_fmt_vid_cap_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= vidioc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_vdec_g_fmt,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,

	.vidioc_enum_fmt_vid_cap_mplane	= vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane	= vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes	= vidioc_enum_framesizes,

	.vidioc_querycap		= vidioc_vdec_querycap,
	.vidioc_subscribe_event		= vidioc_vdec_subscribe_evt,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
	.vidioc_g_selection             = vidioc_vdec_g_selection,
	.vidioc_s_selection             = vidioc_vdec_s_selection,

	.vidioc_g_crop			= vidioc_vdec_g_crop,
};

int mtk_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq)
{
	struct mtk_vcodec_ctx *ctx = priv;
	int ret = 0;

	mtk_v4l2_debug(4, "[%d]", ctx->id);

	src_vq->type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes	= VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv	= ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_video_dec_buf);
	src_vq->ops		= &mtk_vdec_vb2_ops;
	src_vq->mem_ops		= &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock		= &ctx->dev->dev_mutex;
	src_vq->allow_zero_bytesused = 1;

	ret = vb2_queue_init(src_vq);
	if (ret) {
		mtk_v4l2_err("Failed to initialize videobuf2 queue(output)");
		return ret;
	}
	dst_vq->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes	= VB2_DMABUF | VB2_MMAP;
	dst_vq->drv_priv	= ctx;
	dst_vq->buf_struct_size = sizeof(struct mtk_video_dec_buf);
	dst_vq->ops		= &mtk_vdec_vb2_ops;
#ifdef CONFIG_VB2_MEDIATEK_DMA_SG
	dst_vq->mem_ops		= &mtk_dma_sg_memops;
#else
	dst_vq->mem_ops		= &vb2_dma_contig_memops;
#endif
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock		= &ctx->dev->dev_mutex;
	dst_vq->allow_zero_bytesused = 1;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		mtk_v4l2_err("Failed to initialize videobuf2 queue(capture)");
	}

	return ret;
}
