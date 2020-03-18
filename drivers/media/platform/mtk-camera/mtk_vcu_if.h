/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MTK_VCU_IF_H_
#define _MTK_VCU_IF_H_

#include <linux/dma-buf.h>
#include "mtk_vcu.h"

#define VIDEO_CAPTURE_MAX_FB	32
#define MTK_VIDEO_MAX_PLANES	3

enum cam_ipi_msg_status {
	CAM_IPI_MSG_STATUS_OK		= 0,
	CAM_IPI_MSG_STATUS_FAIL		= -1,
	CAM_IPI_MSG_TIMEOUT		= -2,
};

enum cam_ipi_msgid {
	AP_IPIMSG_CAM_INIT               = 0xC000,
	AP_IPIMSG_CAM_START_STREAM       = 0xC001,
	AP_IPIMSG_CAM_INIT_BUFFER        = 0xC002,
	AP_IPIMSG_CAM_DEINIT_BUFFER      = 0xC003,
	AP_IPIMSG_CAM_START              = 0xC004,
	AP_IPIMSG_CAM_END                = 0xC005,
	AP_IPIMSG_CAM_STOP_STREAM        = 0xC006,
	AP_IPIMSG_CAM_DEINIT             = 0xC007,
	AP_IPIMSG_CAM_SET_PARAM          = 0xC008,
	AP_IPIMSG_CAM_GET_PARAM          = 0xC009,

	VCU_IPIMSG_CAM_INIT_ACK          = 0xD000,
	VCU_IPIMSG_CAM_START_STREAM_ACK  = 0xD001,
	VCU_IPIMSG_CAM_INIT_BUFFER_ACK   = 0xD002,
	VCU_IPIMSG_CAM_DEINIT_BUFFER_ACK = 0xD003,
	VCU_IPIMSG_CAM_START_ACK         = 0xD004,
	VCU_IPIMSG_CAM_END_ACK           = 0xD005,
	VCU_IPIMSG_CAM_STOP_STREAM_ACK   = 0xD006,
	VCU_IPIMSG_CAM_DEINIT_ACK        = 0xD007,
	VCU_IPIMSG_CAM_SET_PARAM_ACK     = 0xD008,
	VCU_IPIMSG_CAM_GET_PARAM_ACK     = 0xD009,
};

struct video_inst {
	struct mtk_vidcap_ctx *ctx;
	struct platform_device *dev;
	struct video_vsi *vsi;
	struct list_head queue;
	enum   ipi_id id;
	uint32_t inst_addr;
	int32_t  signaled;
	int32_t  failure;
};

#pragma pack(push, 4)

struct fb_info {
	uint32_t fb_dma[MTK_VIDEO_MAX_PLANES];
	uint32_t fb_va[MTK_VIDEO_MAX_PLANES];
	uint32_t fb_fd[MTK_VIDEO_MAX_PLANES];
	uint32_t fb_size[MTK_VIDEO_MAX_PLANES];
	uint32_t fb_num_planes;
	uint32_t handle;

	uint32_t sensorId;
	uint32_t status;
};

struct fb_info_in {
	uint32_t handle;
	uint32_t dma_addr;
	uint32_t dma_size;
};

struct fb_info_out {
	uint32_t status;
	uint32_t sensorId;
	uint32_t handle;
};

struct video_vsi {
	uint64_t reserved[2048];
};

/**
 * struct cam_ap_ipi_cmd - generic AP to VCU ipi command format
 * @msg_id		: camera_ipi_msgid
 * @vcu_inst_addr	: VCU camera instance address
 */
struct cam_ap_ipi_cmd {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint64_t vcu_inst_addr;
	uint64_t ap_inst_addr;
	struct fb_info_in info;
};

/**
 * struct cam_vcu_ipi_ack - generic VPU to AP ipi command format
 * @msg_id		: camera_ipi_msgid
 * @status			: VPU exeuction result
 * @ap_inst_addr	: AP camera instance address
 */
struct cam_vcu_ipi_ack {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint64_t vcu_inst_addr;
	uint64_t ap_inst_addr;
	struct fb_info_out info;
	int32_t  status;
};

/**
 * struct cam_ap_ipi_init - for AP_IPIMSG_CAPTURE_INIT
 * @msg_id		: AP_IPIMSG_DEC_INIT
 * @reserved		: Reserved field
 * @ap_inst_addr	: AP camera instance address
 */
struct cam_ap_ipi_init {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint64_t ap_inst_addr;
};

/**
 * struct cam_ap_ipi_set_param - for AP_IPIMSG_CAPTURE_SET_PARAM
 * @msg_id		: AP_IPIMSG_DEC_SET_PARAM
 * @vcu_inst_addr	: VCU decoder instance address
 * @id			: set param  type
 * @data			: param data
 */
struct cam_ap_ipi_set_param {
	uint32_t msg_id;
	uint32_t ipi_id;
	uint32_t id;
	uint64_t vcu_inst_addr;
	uint64_t ap_inst_addr;
	uint32_t data[4];
};

#pragma pack(pop)

#endif
