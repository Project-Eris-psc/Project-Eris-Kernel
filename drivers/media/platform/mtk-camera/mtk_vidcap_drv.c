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
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_vidcap_drv.h"
#include "mtk_vidcap_util.h"
#include "mtk_vidcap_if.h"
#include "mtk_vcu.h"

module_param_named(debug, mtk_vidcap_dbg_level, int, 0644);

#define MTK_VIDEO_CAPTURE_DEF_FORMAT        V4L2_PIX_FMT_YUYV
#define MTK_VIDEO_CAPTURE_DEF_WIDTH         1920U
#define MTK_VIDEO_CAPTURE_DEF_HEIGHT        1088U

#define MTK_VIDEO_CAPTURE_MIN_WIDTH         2U
#define MTK_VIDEO_CAPTURE_MAX_WIDTH         8190U
#define MTK_VIDEO_CAPTURE_MIN_HEIGHT        2U
#define MTK_VIDEO_CAPTURE_MAX_HEIGHT        8190U

static struct mtk_vidcap_fmt mtk_vidcap_formats[] = {
	{
		.name   = "YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
		.bpp	= {16, 0, 0},
		.num_planes = 1,
	},
};

/* Sizes must be in increasing order */
static struct v4l2_frmsize_discrete mtk_vidcap_sizes[] = {
	{ 1280, 720  },
	{ 634,  474  },

};

/* -----------------------------------------------------------------------------
 * Video queue operations
 */

static int vb2ops_vidcap_queue_setup(struct vb2_queue *vq,
				const void *parg,
				unsigned int *nbuffers,
				unsigned int *nplanes,
				unsigned int sizes[], void *alloc_ctxs[])
{
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_q_data *q_data = &ctx->q_data;
	int i;

	if (q_data == NULL) {
		mtk_vidcap_err("q_data is NULL");
		return -EINVAL;
	}

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
			alloc_ctxs[i] = ctx->dev->alloc_ctx;
		}
	} else {
		*nplanes = q_data->fmt->num_planes;
		for (i = 0; i < *nplanes; i++) {
			sizes[i] = q_data->sizeimage[i];
			alloc_ctxs[i] = ctx->dev->alloc_ctx;
		}
	}

	vq->min_buffers_needed = *nbuffers;

	mtk_vidcap_debug(1,
			"[%d]\t type = %d, get %d plane(s), %d buffer(s) of size %d %d",
			ctx->id, vq->type, *nplanes, *nbuffers,
			sizes[0], sizes[1]);
	return 0;
}

static int vb2ops_vidcap_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_q_data *q_data = &ctx->q_data;
	int i;

	mtk_vidcap_debug(1, "[%d] (%d) id=%d, state=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb->state, vb);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			mtk_vidcap_err("data will not fit into plane %d (%lu < %lu)",
				i, vb2_plane_size(vb, i),
				q_data->sizeimage[i]);
		}
	}

	return 0;
}

static void vb2ops_vidcap_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
				struct vb2_v4l2_buffer, vb2_buf);
	struct vidcap_buffer *vidcapbuf = container_of(vb2_v4l2,
				struct vidcap_buffer, vb);
	struct mtk_vidcap_mem *fb = &vidcapbuf->framebuffer;
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	int i, ret = 0;

	mtk_vidcap_debug(1, "[%d] (%d) id=%d, state=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb->state, vb);

	fb->num_planes = vb->num_planes;
	for (i = 0; i < vb->num_planes; i++) {
		fb->planes[i].vaddr = vb2_plane_vaddr(vb, i);
		fb->planes[i].dma_addr = vb2_dma_contig_plane_dma_addr(vb, i);
		fb->planes[i].size = vb2_plane_size(vb, i);
		mtk_vidcap_debug(1, "plane %d, vaddr %p, dma_addr 0x%llx, size %zu",
				i, fb->planes[i].vaddr, (uint64_t)fb->planes[i].dma_addr, fb->planes[i].size);
	}

	ret = vidcap_if_capture(ctx, fb);
	if (ret) {
		mtk_vidcap_err("[%d]: vidcap_if_capture() fail ret=%d",
			ctx->id, ret);
	}
}

static int vb2ops_vidcap_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
				struct vb2_v4l2_buffer, vb2_buf);
	struct vidcap_buffer *vidcapbuf = container_of(vb2_v4l2,
				struct vidcap_buffer, vb);
	struct mtk_vidcap_mem *fb = &vidcapbuf->framebuffer;
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	int i, ret = 0;

	mtk_vidcap_debug(1, "[%d] (%d) id=%d, state=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb->state, vb);

	fb->num_planes = vb->num_planes;
	for (i = 0; i < vb->num_planes; i++) {
		fb->planes[i].vaddr = vb2_plane_vaddr(vb, i);
		fb->planes[i].dma_addr = vb2_dma_contig_plane_dma_addr(vb, i);
		fb->planes[i].size = vb2_plane_size(vb, i);
		mtk_vidcap_debug(1, "plane %d, vaddr %p, dma_addr %llx, size %zu",
				i, fb->planes[i].vaddr, (uint64_t)fb->planes[i].dma_addr, fb->planes[i].size);
	}

	if (ctx->state == MTK_STATE_INIT) {
		ret = vidcap_if_init_buffer(ctx, fb);
		if (ret) {
			mtk_vidcap_err("[%d]: vidcap_if_use_buffer() fail ret=%d",
				ctx->id, ret);
			return -EINVAL;
		}
	} else
		mtk_vidcap_err("[%d] state=(%x) invalid call",
			ctx->id, ctx->state);

	return 0;
}
static void vb2ops_vidcap_buf_deinit(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
				struct vb2_v4l2_buffer, vb2_buf);
	struct vidcap_buffer *vidcapbuf = container_of(vb2_v4l2,
				struct vidcap_buffer, vb);
	struct mtk_vidcap_mem *fb = &vidcapbuf->framebuffer;
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	int ret = 0;

	mtk_vidcap_debug(1, "[%d] (%d) id=%d, state=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb->state, vb);

	if (ctx->state == MTK_STATE_INIT) {
		ret = vidcap_if_deinit_buffer(ctx, fb);
		if (ret) {
			mtk_vidcap_err("[%d]: vidcap_if_use_buffer() fail ret=%d",
				ctx->id, ret);
			return;
		}
	} else
		mtk_vidcap_err("[%d] state=(%x) invalid call",
			ctx->id, ctx->state);
}

static int vb2ops_vidcap_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(q);
	int ret = 0;

	mtk_vidcap_debug(1, "[%d] (%d) state=(%x)",
			ctx->id, q->type, ctx->state);

	if (ctx->state == MTK_STATE_INIT) {

		ret = vidcap_if_start_stream(ctx);
		if (ret) {
			mtk_vidcap_err("[%d]: vidcap_if_start_stream() fail ret=%d",
				ctx->id, ret);
			return -EINVAL;
		}
		ctx->state = MTK_STATE_START;
	} else
		mtk_vidcap_err("[%d] state=(%x) invalid call",
			ctx->id, ctx->state);

	return 0;
}

static void vb2ops_vidcap_stop_streaming(struct vb2_queue *q)
{
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(q);
	int ret = 0;

	mtk_vidcap_debug(1, "[%d] (%d) state=(%x)",
			ctx->id, q->type, ctx->state);

	if (ctx->state == MTK_STATE_START) {

		ret = vidcap_if_stop_stream(ctx);
		if (ret) {
			mtk_vidcap_err("[%d]: vidcap_if_start_stream() fail ret=%d",
				ctx->id, ret);
			return;
		}
		ctx->state = MTK_STATE_FLUSH;

		vb2_wait_for_all_buffers(q);
		ctx->state = MTK_STATE_INIT;

		mtk_vidcap_debug(1, "[%d] wait buffer done", ctx->id);
	} else
		mtk_vidcap_err("[%d] state=(%x) invalid call",
			ctx->id, ctx->state);

}

static const struct vb2_ops mtk_vidcap_vb2_ops = {
	.queue_setup		= vb2ops_vidcap_queue_setup,
	.buf_prepare		= vb2ops_vidcap_buf_prepare,
	.buf_queue			= vb2ops_vidcap_buf_queue,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
	.buf_init			= vb2ops_vidcap_buf_init,
	.buf_cleanup		= vb2ops_vidcap_buf_deinit,
	.start_streaming	= vb2ops_vidcap_start_streaming,
	.stop_streaming		= vb2ops_vidcap_stop_streaming,
};

static struct mtk_vidcap_fmt *mtk_vidcap_find_format(struct v4l2_format *f)
{
	struct mtk_vidcap_fmt *fmt;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mtk_vidcap_formats); i++) {
		fmt = &mtk_vidcap_formats[i];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			return fmt;
	}

	return NULL;
}

static int mtk_vidcap_try_fmt(struct v4l2_format *f, struct mtk_vidcap_fmt *fmt)
{
	struct v4l2_pix_format *pix_fmt = &f->fmt.pix;
	int i;

	pix_fmt->field = V4L2_FIELD_NONE;

	/* Guaranteed to be a match */
	for (i = 0; i < ARRAY_SIZE(mtk_vidcap_sizes); i++)
		if ((mtk_vidcap_sizes[i].width == pix_fmt->width)
			&& (mtk_vidcap_sizes[i].height == pix_fmt->height)) {
			break;
		}

	/* Clamp the width and height. */
	pix_fmt->width = clamp(pix_fmt->width,
				MTK_VIDEO_CAPTURE_MIN_WIDTH,
				MTK_VIDEO_CAPTURE_MAX_WIDTH);
	pix_fmt->height = clamp(pix_fmt->height,
				MTK_VIDEO_CAPTURE_MIN_HEIGHT,
				MTK_VIDEO_CAPTURE_MAX_HEIGHT);

	pix_fmt->bytesperline = pix_fmt->width * fmt->bpp[0] / 8;

	pix_fmt->sizeimage = pix_fmt->height * pix_fmt->bytesperline;

	mtk_vidcap_debug(1, "mtk_vidcap_try_fmt: bytesperline: %u, sizeimage: %u",
		pix_fmt->bytesperline, pix_fmt->sizeimage);

	mtk_vidcap_debug(1, "mtk_vidcap_try_fmt: format %s width:%u, height:%u",
		fmt->name, pix_fmt->width, pix_fmt->height);

	pix_fmt->flags = 0;

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

static int
vicap_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{

	strlcpy(cap->driver, MTK_VIDCAP_DEVICE, sizeof(cap->driver));
	strlcpy(cap->bus_info, MTK_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, MTK_PLATFORM_STR, sizeof(cap->card));

	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_STREAMING | V4L2_CAP_DEVICE_CAPS;

	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static int
vidcap_enum_format(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	const struct mtk_vidcap_fmt *fmt;

	if (f->index >= ARRAY_SIZE(mtk_vidcap_formats))
		return -EINVAL;

	fmt = &mtk_vidcap_formats[f->index];

	f->pixelformat = fmt->fourcc;
	memset(f->reserved, 0, sizeof(f->reserved));

	return 0;
}

static int
vidcap_get_format(struct file *file, void *fh,
					struct v4l2_format *format)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	struct v4l2_pix_format *pix = &format->fmt.pix;
	struct mtk_q_data *q_data = &ctx->q_data;

	pix->field = V4L2_FIELD_NONE;
	pix->colorspace = ctx->colorspace;
	pix->ycbcr_enc = ctx->ycbcr_enc;
	pix->quantization = ctx->quantization;
	pix->xfer_func = ctx->xfer_func;

	/*
	 * Width and height are set to the dimensions
	 * of the movie, the buffer is bigger and
	 * further processing stages should crop to this
	 * rectangle.
	 */
	pix->width  = q_data->width;
	pix->height = q_data->height;

	/*
	 * Set pixelformat to the format in which mt vcodec
	 * outputs the decoded frame
	 */
	pix->pixelformat = q_data->fmt->fourcc;

	pix->bytesperline = q_data->bytesperline[0];
	pix->sizeimage = q_data->sizeimage[0];

	return 0;
}

static int
vidcap_set_format(struct file *file, void *fh,
					struct v4l2_format *format) {

	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	struct v4l2_pix_format *pix;
	struct mtk_q_data *q_data = &ctx->q_data;
	struct mtk_vidcap_fmt *fmt;
	uint32_t size[2];
	int ret = 0;

	mtk_vidcap_debug(1, "vidcap_set_format [%d]", ctx->id);

	if (vb2_is_busy(&ctx->queue)) {
		mtk_vidcap_err("capture buffers already requested");
		ret = -EBUSY;
	}

	if (!q_data) {
		mtk_vidcap_err("[%d]: q_data is NULL", ctx->id);
		return -EINVAL;
	}

	pix = &format->fmt.pix;

	fmt = mtk_vidcap_find_format(format);
	if (fmt == NULL) {
		format->fmt.pix.pixelformat = MTK_VIDEO_CAPTURE_DEF_FORMAT;
		fmt = mtk_vidcap_find_format(format);
	}
	q_data->fmt = fmt;

	mtk_vidcap_try_fmt(format, q_data->fmt);

	q_data->sizeimage[0] = pix->sizeimage;

	q_data->width = pix->width;
	q_data->height = pix->height;
	size[0] = pix->width;
	size[1] = pix->height;

	ctx->colorspace = format->fmt.pix.colorspace;
	ctx->ycbcr_enc = format->fmt.pix.ycbcr_enc;
	ctx->quantization = format->fmt.pix.quantization;
	ctx->xfer_func = format->fmt.pix.xfer_func;

	if (ctx->state == MTK_STATE_FREE) {
		ret = vidcap_if_init(ctx);
		if (ret) {
			mtk_vidcap_err("[%d]: vidcap_if_init() fail ret=%d",
				ctx->id, ret);
			return -EINVAL;
		}
		vidcap_if_set_param(ctx, SET_PARAM_FRAME_SIZE, (void *)size);
		ctx->state = MTK_STATE_INIT;
	} else
		mtk_vidcap_err("[%d] state=(%x) invalid call",
			ctx->id, ctx->state);

	mtk_vidcap_debug(1, "ret %d", ret);

	return 0;
}

static int
vidcap_try_format(struct file *file, void *fh,
					struct v4l2_format *format)
{
	struct mtk_vidcap_fmt *fmt;

	fmt = mtk_vidcap_find_format(format);
	if (fmt == NULL) {
		format->fmt.pix.pixelformat =
			MTK_VIDEO_CAPTURE_DEF_FORMAT;
		fmt = mtk_vidcap_find_format(format);
	}

	return mtk_vidcap_try_fmt(format, fmt);
}

static int
vidcap_get_param(struct file *file, void *fh,
					struct v4l2_streamparm *parm)
{

	return 0;
}

static int
vidcap_set_param(struct file *file, void *fh,
					struct v4l2_streamparm *parm)
{
	return 0;
}

static int
vidcap_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	return 0;
}

/* timeperframe is arbitrary and continuous */
static int
vidcap_enum_frameintervals(struct file *file, void *fh,
					struct v4l2_frmivalenum *fival)
{
	return 0;
}

static int
vidcap_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *rb)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_reqbufs [%d], cnt[%d] mem[%d] type[%d, %d] point[0x%p]",
		ctx->id, rb->count, rb->memory, rb->type, ctx->queue.type, &ctx->queue);

	ret = vb2_reqbufs(&ctx->queue, rb);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

static int
vidcap_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_querybuf [%d], queue %p",
		ctx->id, &ctx->queue);

	ret = vb2_querybuf(&ctx->queue, b);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

static int
vidcap_expbuf(struct file *file, void *fh, struct v4l2_exportbuffer *p)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_expbuf [%d], fd[%d], flag[%d], idx[%d], plane[%d]",
		ctx->id, p->fd, p->flags, p->index, p->plane);

	ret = vb2_expbuf(&ctx->queue, p);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

static int
vidcap_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_qbuf [%d], length[%d], bytes[%d]",
		ctx->id, b->length, b->bytesused);

	if (ctx->state == MTK_STATE_FREE || ctx->state == MTK_STATE_FLUSH) {
		mtk_vidcap_err("[%d] state=(%x) invalid call",
			ctx->id, ctx->state);
		return -EIO;
	}

	ret = vb2_qbuf(&ctx->queue, b);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

static int
vidcap_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_dqbuf [%d], length[%d], bytes[%d]",
		ctx->id, b->length, b->bytesused);

	ret = vb2_dqbuf(&ctx->queue, b, file->f_flags & O_NONBLOCK);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

int vidcap_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_streamon [%d], queue %p",
		ctx->id, &ctx->queue);

	ret = vb2_streamon(&ctx->queue, i);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

int vidcap_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(fh);
	int ret;

	mtk_vidcap_debug(1, "vidcap_streamoff [%d], queue %p",
		ctx->id, &ctx->queue);

	ret = vb2_streamoff(&ctx->queue, i);

	mtk_vidcap_debug(1, "ret %d", ret);

	return ret;
}

static int
vidcap_enum_input(struct file *file, void *fh, struct v4l2_input *input)
{
	if (input->index > 0)
		return -EINVAL;

	strlcpy(input->name, "camera", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;

	return 0;
}

static int
vidcap_g_input(struct file *file, void *fh, unsigned int *input)
{
	*input = 0;

	return 0;
}

static int
vidcap_s_input(struct file *file, void *fh, unsigned int input)
{
	return input == 0 ? 0 : -EINVAL;
}

static const struct v4l2_ioctl_ops mtk_vidcap_ioctl_ops = {
	.vidioc_querycap		= vicap_querycap,

	.vidioc_enum_fmt_vid_cap= vidcap_enum_format,
	.vidioc_g_fmt_vid_cap	= vidcap_get_format,
	.vidioc_s_fmt_vid_cap	= vidcap_set_format,
	.vidioc_try_fmt_vid_cap	= vidcap_try_format,

	.vidioc_g_parm			= vidcap_get_param,
	.vidioc_s_parm			= vidcap_set_param,

	.vidioc_enum_framesizes = vidcap_enum_framesizes,
	.vidioc_enum_frameintervals = vidcap_enum_frameintervals,

	.vidioc_reqbufs			= vidcap_reqbufs,
	.vidioc_querybuf		= vidcap_querybuf,
	.vidioc_expbuf			= vidcap_expbuf,
	.vidioc_qbuf			= vidcap_qbuf,
	.vidioc_dqbuf			= vidcap_dqbuf,
	.vidioc_streamon		= vidcap_streamon,
	.vidioc_streamoff		= vidcap_streamoff,

	.vidioc_enum_input		= vidcap_enum_input,
	.vidioc_g_input			= vidcap_g_input,
	.vidioc_s_input			= vidcap_s_input,
};

static void mtk_handle_buffer(struct mtk_vidcap_mem *fb)
{
	struct vidcap_buffer *vidcapbuf = container_of(fb,
		struct vidcap_buffer, framebuffer);
	struct vb2_v4l2_buffer *vb2_v4l2 = &vidcapbuf->vb;
	struct vb2_buffer *vb = &vb2_v4l2->vb2_buf;
	struct mtk_vidcap_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	int i = 0;

	mtk_vidcap_debug(1, "[%d] (%d) id=%d, state=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb->state, vb);

	/*should be replaced by payload */
	for (i = 0; i < fb->num_planes; i++)
		vb2_set_plane_payload(vb, i, fb->planes[i].size);

	if (fb->status == BUFFER_FILLED)
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	else
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
}

void mtk_vidcap_unlock(struct mtk_vidcap_ctx *ctx)
{
	mutex_unlock(&ctx->dev->capture_mutex);
}

void mtk_vidcap_lock(struct mtk_vidcap_ctx *ctx)
{
	mutex_lock(&ctx->dev->capture_mutex);
}

static void mtk_vidcap_release(struct mtk_vidcap_ctx *ctx)
{
	vidcap_if_deinit(ctx);
	ctx->state = MTK_STATE_FREE;
}

int mtk_vidcap_ctrls_setup(struct mtk_vidcap_ctx *ctx)
{
	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, MTK_MAX_CTRLS_HINT);

	if (ctx->ctrl_hdl.error) {
		mtk_vidcap_err("adding control failed %d",
			ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

	return 0;
}

void mtk_vidcap_set_default_params(struct mtk_vidcap_ctx *ctx)
{
	struct mtk_q_data *q_data = &ctx->q_data;

	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->width  = MTK_VIDEO_CAPTURE_DEF_WIDTH;
	q_data->height = MTK_VIDEO_CAPTURE_DEF_HEIGHT;
	q_data->fmt = mtk_vidcap_formats;
	q_data->field = V4L2_FIELD_NONE;

	q_data->sizeimage[0] = q_data->width * q_data->height * 2;
	q_data->bytesperline[0] = q_data->width * 2;
}

/* -----------------------------------------------------------------------------
 * V4L2 file operations
 */

static int fops_vidcap_open(struct file *file)
{
	struct mtk_vidcap_dev *dev = video_drvdata(file);
	struct mtk_vidcap_ctx *ctx = NULL;
	struct vb2_queue *queue;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mutex_lock(&dev->dev_mutex);
	ctx->id = dev->id_counter++;
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);
	ctx->dev = dev;

	ret = mtk_vidcap_ctrls_setup(ctx);
	if (ret) {
		mtk_vidcap_err("Failed to setup video capture controls");
		goto err_ctrls_setup;
	}

	queue = &ctx->queue;
	queue->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	queue->io_modes	= VB2_DMABUF | VB2_MMAP;
	queue->drv_priv	= ctx;
	queue->buf_struct_size = sizeof(struct vidcap_buffer);
	queue->ops		= &mtk_vidcap_vb2_ops;
	queue->mem_ops	= &vb2_dma_contig_memops;
	queue->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	queue->lock		= &ctx->dev->dev_mutex;
	queue->allow_zero_bytesused = 1;

	ret = vb2_queue_init(&ctx->queue);
	if (ret < 0) {
		mtk_vidcap_err("Failed to initialize videobuf2 queue");
		goto err_vb2_init;
	}

	dev->queue = &ctx->queue;

	mtk_vidcap_set_default_params(ctx);

	ctx->callback = mtk_handle_buffer;

	if (v4l2_fh_is_singular(&ctx->fh)) {
		/*
		 * vcu_load_firmware checks if it was loaded already and
		 * does nothing in that case
		 */
		ret = vcu_load_firmware(dev->vcu_plat_dev);
		if (ret < 0) {
			/*
			 * Return 0 if downloading firmware successfully,
			 * otherwise it is failed
			 */
			mtk_vidcap_err("vcu_load_firmware failed!");
			goto err_load_fw;
		}
	}

	list_add(&ctx->list, &dev->ctx_list);

	mutex_unlock(&dev->dev_mutex);
	mtk_vidcap_debug(0, "%s capture [%d]", dev_name(&dev->plat_dev->dev),
			ctx->id);
	return ret;

	/* Deinit when failure occurred */
err_load_fw:
	vb2_queue_release(&ctx->queue);
err_vb2_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vidcap_release(struct file *file)
{
	struct mtk_vidcap_dev *dev = video_drvdata(file);
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(file->private_data);

	mtk_vidcap_debug(0, "[%d] video capture", ctx->id);
	mutex_lock(&dev->dev_mutex);

	vidcap_streamoff(file, &ctx->fh, ctx->queue.type);

	vb2_queue_release(&ctx->queue);
	mtk_vidcap_release(ctx);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	list_del_init(&ctx->list);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return 0;
}

static unsigned int fops_vidcap_poll(struct file *file, poll_table *wait)
{
	struct mtk_vidcap_dev *dev = video_drvdata(file);
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(file->private_data);
	int ret;

	mtk_vidcap_debug(0, "[%d] video capture", ctx->id);

	mutex_lock(&dev->dev_mutex);
	ret = vb2_poll(&ctx->queue, file, wait);
	mutex_unlock(&dev->dev_mutex);

	mtk_vidcap_debug(1, "ret %d", ret);
	return ret;
}

static int fops_vidcap_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct mtk_vidcap_ctx *ctx = fh_to_ctx(file->private_data);
	int ret;

	mtk_vidcap_debug(0, "[%d] video capture", ctx->id);

	ret = vb2_mmap(&ctx->queue, vma);

	mtk_vidcap_debug(1, "ret %d", ret);
	return ret;
}

static const struct v4l2_file_operations mtk_vidcap_fops = {
	.owner		= THIS_MODULE,
	.open		= fops_vidcap_open,
	.release	= fops_vidcap_release,
	.poll		= fops_vidcap_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fops_vidcap_mmap,
};

static int mtk_vidcap_probe(struct platform_device *pdev)
{
	struct mtk_vidcap_dev *dev;
	struct video_device *video;
	int ret;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&dev->ctx_list);
	dev->plat_dev = pdev;
	dev->vcu_plat_dev = vcu_get_plat_device(dev->plat_dev);
	if (dev->vcu_plat_dev == NULL) {
		mtk_vidcap_err("[VCU] vcu device in not ready");
		return -EPROBE_DEFER;
	}

	mutex_init(&dev->capture_mutex);
	mutex_init(&dev->dev_mutex);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
		"[/MTK_V4L2_VIDCAP]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		mtk_vidcap_err("v4l2_device_register err=%d", ret);
		return -ENOMEM;
	}

	video = video_device_alloc();
	if (!video) {
		mtk_vidcap_err("Failed to allocate video device");
		goto err_alloc;
	}

	video->fops = &mtk_vidcap_fops;
	video->ioctl_ops = &mtk_vidcap_ioctl_ops;
	video->release = video_device_release;
	video->lock = &dev->dev_mutex;
	video->v4l2_dev  = &dev->v4l2_dev;
	video->vfl_type = VFL_TYPE_GRABBER;

	snprintf(video->name, sizeof(video->name), "%s", MTK_VIDCAP_NAME);

	video_set_drvdata(video, dev);
	dev->video = video;
	platform_set_drvdata(pdev, dev);

	dev->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(dev->alloc_ctx)) {
		mtk_vidcap_err("Failed to alloc vb2 dma context");
		ret = PTR_ERR(dev->alloc_ctx);
		goto err_vb2_ctx_init;
	}

	ret = video_register_device(video, VFL_TYPE_GRABBER, 0);
	if (ret) {
		mtk_vidcap_err("Failed to register video device");
		goto err__mem_init;
	}

	mtk_vidcap_debug(0, "video capture registered as /dev/video%d",
		video->num);

	return 0;

err__mem_init:
	vb2_dma_contig_cleanup_ctx(dev->alloc_ctx);
err_vb2_ctx_init:
	video_unregister_device(video);
err_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
	return ret;
}

static const struct of_device_id mtk_vidcap_match[] = {
	{.compatible = "mediatek,mt8167-vidcap",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_vidcap_match);

static int mtk_vidcap_remove(struct platform_device *pdev)
{
	struct mtk_vidcap_dev *dev = platform_get_drvdata(pdev);

	mtk_vidcap_debug(0, "mtk_vidcap_remove dev %p", dev);

	if (dev->alloc_ctx)
		vb2_dma_contig_cleanup_ctx(dev->alloc_ctx);

	if (dev->video)
		video_unregister_device(dev->video);

	v4l2_device_unregister(&dev->v4l2_dev);

	mutex_destroy(&dev->capture_mutex);
	mutex_destroy(&dev->dev_mutex);

	return 0;
}

static struct platform_driver mtk_vidcap_driver = {
	.probe	= mtk_vidcap_probe,
	.remove	= mtk_vidcap_remove,
	.driver	= {
		.name	= MTK_VIDCAP_NAME,
		.of_match_table = mtk_vidcap_match,
	},
};

module_platform_driver(mtk_vidcap_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek V4L2 video capture driver");
