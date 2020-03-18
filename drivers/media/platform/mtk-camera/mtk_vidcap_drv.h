#ifndef _MTK_VIDCAP_DRV_H_
#define _MTK_VIDCAP_DRV_H_

#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <linux/delay.h>

#include "mtk_vidcap_util.h"
#include "mtk_vidcap_drv_base.h"

#define MTK_VIDCAP_NAME		"mtk-video-capture"
#define MTK_VIDCAP_DEVICE	"mtk-camera"
#define MTK_PLATFORM_STR	"platform:mt8167"

#define MTK_VIDCAP_MAX_PLANES   3
#define MTK_VIDCAP_MIN_BUFFERS  3

#define MTK_MAX_CTRLS_HINT	20

/**
 * struct mtk_vidcap_fmt - Structure used to store information about pixelformats
 */
struct mtk_vidcap_fmt {
	char *name;
	u32	fourcc;
	u32 bpp[3];
	u32	num_planes;
};

/**
 * struct mtk_q_data - Structure used to store information about queue
 */
struct mtk_q_data {
	unsigned int	width;
	unsigned int	height;
	enum v4l2_field	field;
	unsigned long	bytesperline[MTK_VIDCAP_MAX_PLANES];
	unsigned long	sizeimage[MTK_VIDCAP_MAX_PLANES];
	struct mtk_vidcap_fmt	*fmt;
};

enum mtk_instance_state {
	MTK_STATE_FREE = 0,
	MTK_STATE_INIT = 1,
	MTK_STATE_START = 2,
	MTK_STATE_FLUSH = 3,
	MTK_STATE_ABORT = 4,
};

struct mtk_vidcap_ctx {
	struct mtk_vidcap_dev *dev;
	struct list_head list;

	struct v4l2_fh fh;
	struct vb2_queue queue;
	enum mtk_instance_state state;
	struct mtk_q_data q_data;
	int id;

	const struct mtk_vidcap_if *cap_if;
	unsigned long drv_handle;

	void (*callback)(struct mtk_vidcap_mem *fb);

	struct v4l2_ctrl_handler ctrl_hdl;

	enum v4l2_colorspace colorspace;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
	enum v4l2_xfer_func xfer_func;
};

struct vidcap_buffer {
	struct vb2_v4l2_buffer	vb;
	struct mtk_vidcap_mem	framebuffer;
};

struct mtk_vidcap_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *video;

	struct platform_device *plat_dev;
	struct platform_device *vcu_plat_dev;
	struct vb2_alloc_ctx *alloc_ctx;
	struct list_head ctx_list;
	struct mtk_vidcap_ctx *curr_ctx;

	unsigned long id_counter;

	struct vb2_queue *queue;

	struct mutex dev_mutex;
	struct mutex capture_mutex;
};

static inline struct mtk_vidcap_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_vidcap_ctx, fh);
}

static inline struct mtk_vidcap_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_vidcap_ctx, ctrl_hdl);
}

void mtk_vidcap_unlock(struct mtk_vidcap_ctx *ctx);
void mtk_vidcap_lock(struct mtk_vidcap_ctx *ctx);

#endif /* _MTK_VIDCAP_DRV_H_ */
