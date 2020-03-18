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

#include "mtk_vidcap_drv_base.h"
#include "mtk_vidcap_drv.h"
#include "mtk_vidcap_util.h"
#include "mtk_vcu_if.h"

static void handle_init_ack_msg(struct cam_vcu_ipi_ack *msg)
{
	struct video_inst *inst = (struct video_inst *)
		(unsigned long)msg->ap_inst_addr;

	mtk_vidcap_debug(1, "+ ap_inst_addr = 0x%llx", (uint64_t)msg->ap_inst_addr);

	/* mapping VCU address to kernel virtual address */
	inst->vsi = vcu_mapping_dm_addr(inst->dev, msg->vcu_inst_addr);
	inst->inst_addr = msg->vcu_inst_addr;
	mtk_vidcap_debug(1, "- vcu_inst_addr = 0x%llx", (uint64_t)inst->inst_addr);
}

static void handle_capture_ack_msg(struct cam_vcu_ipi_ack *msg)
{
	struct video_inst *inst = (struct video_inst *)
		(unsigned long)msg->ap_inst_addr;
	struct mtk_vidcap_ctx *ctx = inst->ctx;
	struct fb_info_out *info = &msg->info;
	struct mtk_vidcap_mem *mem = NULL;

	mtk_vidcap_debug(3, "+ ap_inst_addr = 0x%llx, handle 0x%x",
		(uint64_t)msg->ap_inst_addr, info->handle);

	list_for_each_entry(mem, &inst->queue, list) {
		if (mem->planes[0].dma_addr == info->handle) {
			if (info->status == 0)
				mem->status = BUFFER_FILLED;
			else
				mem->status = BUFFER_ERROR;
			mem->type = info->sensorId;
			ctx->callback(mem);

			mtk_vidcap_debug(3, "- vcu_inst_addr: 0x%x, buffer handle 0x%x",
				inst->inst_addr, info->handle);
			return;
		}
	}
	mtk_vidcap_err("invalid buffer handle 0x%x\n", info->handle);
}
/*
 * This function runs in interrupt context and it means there's a IPI MSG
 * from VCU.
 */
int vcu_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct cam_vcu_ipi_ack *msg = data;
	struct video_inst *inst = (struct video_inst *)((unsigned long)msg->ap_inst_addr);
	int ret = 0;

	mtk_vidcap_debug(3, "+ id=%X status = %d\n", msg->msg_id, msg->status);

	inst->failure = msg->status;

	if (msg->status == 0) {
		switch (msg->msg_id) {
		case VCU_IPIMSG_CAM_INIT_ACK:
			handle_init_ack_msg(data);
			break;
		case VCU_IPIMSG_CAM_START_STREAM_ACK:
		case VCU_IPIMSG_CAM_STOP_STREAM_ACK:
		case VCU_IPIMSG_CAM_INIT_BUFFER_ACK:
		case VCU_IPIMSG_CAM_DEINIT_BUFFER_ACK:
		case VCU_IPIMSG_CAM_START_ACK:
		case VCU_IPIMSG_CAM_DEINIT_ACK:
		case VCU_IPIMSG_CAM_SET_PARAM_ACK:
		case AP_IPIMSG_CAM_GET_PARAM:
			break;
		case VCU_IPIMSG_CAM_END_ACK:
			handle_capture_ack_msg(data);
			ret = 1;
			break;
		default:
			mtk_vidcap_err("invalid msg=%X", msg->msg_id);
			ret = 1;
			break;
		}
	}

	mtk_vidcap_debug(3, "- id=%X", msg->msg_id);
	inst->signaled = 1;

	return ret;

}
static int vidcap_vcu_send_msg(struct video_inst *inst, void *msg, int len)
{
	uint32_t msg_id = *(uint32_t *)msg;
	int err = 0;

	mtk_vidcap_debug(3, "id=%X", msg_id);

	inst->failure  = 0;
	inst->signaled = 0;

	err = vcu_ipi_send(inst->dev, inst->id, msg, len);
	if (err) {
		mtk_vidcap_err("send fail vcu_id=%d msg_id=%X status=%d",
			       inst->id, msg_id, err);
		return err;
	}

	return inst->failure;
}

static int vidcap_send_ap_ipi(struct video_inst *inst,
		unsigned int msg_id, struct fb_info_in *info)
{
	struct cam_ap_ipi_cmd msg;
	int err = 0;

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = msg_id;
	msg.ipi_id = inst->id;
	msg.vcu_inst_addr = inst->inst_addr;
	msg.ap_inst_addr = (uint64_t)(unsigned long)inst;
	if (info != NULL)
		msg.info = *info;

	err = vidcap_vcu_send_msg(inst, &msg, sizeof(msg));
	return err;
}

static int vidcap_vcu_set_param(struct video_inst *inst,
		unsigned int id, void *param, unsigned int size)
{
	struct cam_ap_ipi_set_param msg;
	uint32_t *param_ptr = (uint32_t *)param;
	int err = 0;
	int i = 0;

	mtk_vidcap_debug(3, "+ id=%X", AP_IPIMSG_CAM_SET_PARAM);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_CAM_SET_PARAM;
	msg.ipi_id = inst->id;
	msg.id = id;
	msg.vcu_inst_addr = inst->inst_addr;
	msg.ap_inst_addr = (uint64_t)(unsigned long)inst;

	for (i = 0; i < size; ++i)
		msg.data[i] = *(param_ptr + i);

	err = vidcap_vcu_send_msg(inst, &msg, sizeof(msg));
	mtk_vidcap_debug(3, "- id=%X ret=%d", AP_IPIMSG_CAM_SET_PARAM, err);

	return err;
}


static int vidcap_init(void *ctx, unsigned long *handle)
{
	struct video_inst *inst = NULL;
	struct mtk_vidcap_ctx *contex = (struct mtk_vidcap_ctx *)ctx;
	struct cam_ap_ipi_init msg;
	int err = 0;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst || !ctx)
		return -ENOMEM;

	inst->ctx = contex;
	inst->id = IPI_CAMERA;
	inst->failure  = 0;
	inst->signaled = 0;
	INIT_LIST_HEAD(&inst->queue);
	inst->dev = vcu_get_plat_device(contex->dev->plat_dev);

	err = vcu_ipi_register(inst->dev, inst->id, vcu_ipi_handler, NULL, NULL);
	if (err != 0) {
		mtk_vidcap_err("vcu_ipi_register fail status=%d", err);
		return err;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_CAM_INIT;
	msg.ipi_id = inst->id;
	msg.ap_inst_addr = (uint64_t)(unsigned long)inst;

	err = vidcap_vcu_send_msg(inst, (void *)&msg, sizeof(msg));
	mtk_vidcap_debug(3, "video_inst=%p, ret=%d", inst, err);

	*handle = (unsigned long)inst;

	return err;
}

int vidcap_init_buffer(unsigned long handle, void *fb)
{
	struct video_inst *inst = (struct video_inst *)handle;
	struct mtk_vidcap_mem *mem = (struct mtk_vidcap_mem *)fb;
	struct fb_info_in info;

	mtk_vidcap_debug(3, "inst %p, vsi %p, fb %p", inst, inst->vsi, fb);

	/*now we just support one plane*/
	info.handle = mem->planes[0].dma_addr;
	info.dma_addr = (uint32_t)(mem->planes[0].dma_addr);
	info.dma_size = (uint32_t)(mem->planes[0].size);

	list_add_tail(&mem->list, &inst->queue);

	return vidcap_send_ap_ipi(inst, AP_IPIMSG_CAM_INIT_BUFFER, &info);
}
int vidcap_deinit_buffer(unsigned long handle, void *fb)
{
	struct video_inst *inst = (struct video_inst *)handle;
	struct fb_info_in info;

	struct mtk_vidcap_mem *mem = (struct mtk_vidcap_mem *)fb;
	struct mtk_vidcap_mem *child, *tmp;

	mtk_vidcap_debug(3, "inst %p, vsi %p, fb %p", inst, inst->vsi, fb);

	/*now we just support one plane*/
	info.handle = mem->planes[0].dma_addr;
	info.dma_addr = (uint32_t)(mem->planes[0].dma_addr);
	info.dma_size = (uint32_t)(mem->planes[0].size);

	list_for_each_entry_safe(child, tmp,
							&inst->queue, list) {
		if (child == mem) {
			list_del(&child->list);
			mtk_vidcap_debug(3, "remove buffer %p", child);
		}
	}

	return vidcap_send_ap_ipi(inst, AP_IPIMSG_CAM_DEINIT_BUFFER, &info);
}

int vidcap_start_stream(unsigned long handle)
{
	struct video_inst *inst = (struct video_inst *)handle;

	return vidcap_send_ap_ipi(inst, AP_IPIMSG_CAM_START_STREAM, NULL);
}

int vidcap_capture(unsigned long handle, void *fb)
{
	struct video_inst *inst = (struct video_inst *)handle;
	struct mtk_vidcap_mem *mem = (struct mtk_vidcap_mem *)fb;
	struct fb_info_in info;

	/*now we just support one plane*/
	info.handle = mem->planes[0].dma_addr;
	info.dma_addr = (uint32_t)(mem->planes[0].dma_addr);
	info.dma_size = (uint32_t)(mem->planes[0].size);

	mtk_vidcap_debug(1, "inst %p, vsi %p, handle 0x%x, num_planes %d",
		inst, inst->vsi, info.handle, mem->num_planes);

	return vidcap_send_ap_ipi(inst, AP_IPIMSG_CAM_START, &info);
}

int vidcap_get_param(unsigned long handle,
		 enum vidcap_get_param_type type, void *out)
{
	return 0;
}

int vidcap_set_param(unsigned long handle,
		 enum vidcap_set_param_type type, void *in)
{
	struct video_inst *inst = (struct video_inst *)handle;
	int ret = 0;

	switch (type) {
	case SET_PARAM_FRAME_SIZE:
		vidcap_vcu_set_param(inst, (unsigned int)type, in, 2U);
		break;
	default:
		mtk_vidcap_err("invalid set parameter type=%d\n", (int)type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int vidcap_stop_stream(unsigned long handle)
{
	struct video_inst *inst = (struct video_inst *)handle;

	return vidcap_send_ap_ipi(inst, AP_IPIMSG_CAM_STOP_STREAM, NULL);
}

void vidcap_deinit(unsigned long handle)
{
	struct video_inst *inst = (struct video_inst *)handle;

	if (inst != NULL)
	{
		vidcap_send_ap_ipi(inst, AP_IPIMSG_CAM_DEINIT, NULL);
		kfree(inst);
	}
}

static struct mtk_vidcap_if vcap_if = {
	vidcap_init,
	vidcap_capture,
	vidcap_start_stream,
	vidcap_init_buffer,
	vidcap_deinit_buffer,
	vidcap_get_param,
	vidcap_set_param,
	vidcap_stop_stream,
	vidcap_deinit,
};

struct mtk_vidcap_if *get_capture_if(void)
{
	return &vcap_if;
}

