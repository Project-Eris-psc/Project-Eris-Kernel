#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "mtk_vidcap_if.h"
#include "mtk_vidcap_drv.h"
#include "mtk_vidcap_drv_base.h"
#include "mtk_vcu.h"

struct mtk_vidcap_if *get_capture_if(void);

int  vidcap_if_init(void *ctx)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	contex->cap_if = get_capture_if();

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->init((void *)contex, &contex->drv_handle);
	mtk_vidcap_unlock(contex);

	return ret;
}

void vidcap_if_deinit(void *ctx)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;

	if (contex->drv_handle == 0)
		return;

	mtk_vidcap_lock(contex);
	contex->cap_if->deinit(contex->drv_handle);
	mtk_vidcap_unlock(contex);

	contex->drv_handle = 0;
}

int  vidcap_if_start_stream(void *ctx)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->start_stream(contex->drv_handle);
	mtk_vidcap_unlock(contex);

	return ret;
}

int  vidcap_if_stop_stream(void *ctx)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->stop_stream(contex->drv_handle);
	mtk_vidcap_unlock(contex);

	return ret;
}

int  vidcap_if_init_buffer(void *ctx, void *fb)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->init_buffer(contex->drv_handle, fb);
	mtk_vidcap_unlock(contex);

	return ret;
}

int  vidcap_if_deinit_buffer(void *ctx, void *fb)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->deinit_buffer(contex->drv_handle, fb);
	mtk_vidcap_unlock(contex);

	return ret;
}

int  vidcap_if_capture(void *ctx, void *fb)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->capture(contex->drv_handle, fb);
	mtk_vidcap_unlock(contex);

	return ret;
}

int  vidcap_if_get_param(void *ctx,
			enum vidcap_get_param_type type,
			void *out)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->get_param(contex->drv_handle, type, out);
	mtk_vidcap_unlock(contex);

	return ret;
}

int  vidcap_if_set_param(void *ctx,
			enum vidcap_set_param_type type,
			void *in)
{
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	int ret = 0;

	mtk_vidcap_lock(contex);
	ret = contex->cap_if->set_param(contex->drv_handle, type, in);
	mtk_vidcap_unlock(contex);

	return ret;
}
