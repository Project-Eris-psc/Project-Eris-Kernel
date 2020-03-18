/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "audio_messenger_ipi.h"

#include <linux/spinlock.h>
#include <scp_ipi.h>

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_task.h"
#include "audio_ipi_queue.h"



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

recv_message_t recv_message_array[TASK_SCENE_SIZE];


/*
 * =============================================================================
 *                     private functions - declaration
 * =============================================================================
 */

/* queue related */
static void audio_ipi_msg_dispatcher(int id, void *data, unsigned int len);
static uint16_t current_idx;


/*
 * =============================================================================
 *                     private functions - implementation
 * =============================================================================
 */

static void audio_ipi_msg_dispatcher(int id, void *data, unsigned int len)
{
	ipi_msg_t *p_ipi_msg = NULL;
	ipi_queue_handler_t *handler = NULL;

	AUD_LOG_V("%s(), data = %p, len = %u\n", __func__, data, len);

	if (data == NULL) {
		AUD_LOG_W("%s(), drop msg due to data = NULL\n", __func__);
		return;
	}
	if (len < IPI_MSG_HEADER_SIZE || len > MAX_IPI_MSG_BUF_SIZE) {
		AUD_LOG_W("%s(), drop msg due to len(%u) error!!\n", __func__, len);
		return;
	}

	p_ipi_msg = (ipi_msg_t *)data;
	check_msg_format(p_ipi_msg, len);

	if (p_ipi_msg->ack_type == AUDIO_IPI_MSG_ACK_BACK) {
		handler = get_ipi_queue_handler(p_ipi_msg->task_scene);
		if (handler != NULL)
			send_message_ack(handler, p_ipi_msg);
	} else {
		if (recv_message_array[p_ipi_msg->task_scene] == NULL) {
			AUD_LOG_W("%s(), recv_message_array[%d] = NULL, drop msg. msg_id = 0x%x\n",
				  __func__, p_ipi_msg->task_scene, p_ipi_msg->msg_id);
		} else
			recv_message_array[p_ipi_msg->task_scene](p_ipi_msg);
	}
}


/*
 * =============================================================================
 *                     public functions - implementation
 * =============================================================================
 */

void audio_messenger_ipi_init(void)
{
	int i = 0;
	ipi_status retval = ERROR;

	current_idx = 0;

	retval = scp_ipi_registration(IPI_AUDIO, audio_ipi_msg_dispatcher, "audio");
	if (retval != DONE)
		AUD_LOG_E("%s(), scp_ipi_registration fail!!\n", __func__);

	for (i = 0; i < TASK_SCENE_SIZE; i++)
		recv_message_array[i] = NULL;
}


void audio_reg_recv_message(uint8_t task_scene, recv_message_t recv_message)
{
	if (task_scene >= TASK_SCENE_SIZE) {
		AUD_LOG_W("%s(), not support task_scene %d!!\n", __func__, task_scene);
		return;
	}

	recv_message_array[task_scene] = recv_message;
}


int send_message_to_scp(const ipi_msg_t *p_ipi_msg)
{
	ipi_status send_status = ERROR;

	const int k_max_try_count = 10000;
	int try_count = 0;

	AUD_LOG_D("%s(+)\n", __func__);

	/* error handling */
	if (p_ipi_msg == NULL) {
		AUD_LOG_E("%s(), p_ipi_msg = NULL, return\n", __func__);
		return -1;
	}

	for (try_count = 0; try_count < k_max_try_count; try_count++) {
		send_status = scp_ipi_send(
				      IPI_AUDIO,
				      (void *)p_ipi_msg,
				      get_message_buf_size(p_ipi_msg),
				      0, /* default don't wait */
				      SCP_B_ID);

		if (send_status == DONE)
			break;

		AUD_LOG_V("%s(), #%d scp_ipi_send error %d\n",
			  __func__, try_count, send_status);
	}

	if (send_status != DONE) {
		AUD_LOG_E("%s(), scp_ipi_send error %d\n", __func__, send_status);
		print_msg_info(__func__, "fail", p_ipi_msg);
	} else
		print_msg_info(__func__, "pass", p_ipi_msg);


	AUD_LOG_D("%s(-)\n", __func__);
	return (send_status == DONE) ? 0 : -1;
}






