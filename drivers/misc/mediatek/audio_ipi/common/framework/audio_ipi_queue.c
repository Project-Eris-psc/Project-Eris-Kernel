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

#include "audio_ipi_queue.h"

#include <linux/slab.h>         /* needed by kmalloc */

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <linux/delay.h>

#include <scp_ipi.h>

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_messenger_ipi.h"
#include "audio_task.h"


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_IPI_MSG_QUEUE_SIZE (16)


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef struct {
	ipi_msg_t *msg;
	wait_queue_head_t wq;
} queue_element_t;


typedef struct {
	uint8_t k_element_size;
	queue_element_t element[MAX_IPI_MSG_QUEUE_SIZE];

	uint8_t task_scene; /* task_scene_t */

	uint8_t idx_r;
	uint8_t idx_w;

	spinlock_t rw_lock;

	ipi_msg_t ipi_msg_ack;

	bool enable;
} msg_queue_t;


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static ipi_queue_handler_t g_ipi_queue_handler[TASK_SCENE_SIZE];


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static msg_queue_t *create_msg_queue(const uint8_t task_scene);
static void destroy_msg_queue(msg_queue_t *msg_queue);

static int process_message_in_queue(msg_queue_t *msg_queue, ipi_msg_t *p_ipi_msg, int idx_msg);


/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline bool check_queue_empty(const msg_queue_t *msg_queue);
inline bool check_queue_to_be_full(const msg_queue_t *msg_queue);

inline uint8_t get_num_messages_in_queue(const msg_queue_t *msg_queue);

inline int push_msg(msg_queue_t *msg_queue, ipi_msg_t *p_ipi_msg);
inline int pop_msg(msg_queue_t *msg_queue, ipi_msg_t **pp_ipi_msg);

inline bool check_idx_msg_valid(msg_queue_t *msg_queue, int idx_msg);
inline bool check_ack_msg_valid(const ipi_msg_t *p_ipi_msg, const ipi_msg_t *p_ipi_msg_ack);


/*
 * =============================================================================
 *                     create/destroy/init/deinit functions
 * =============================================================================
 */

ipi_queue_handler_t *create_ipi_queue_handler(const uint8_t task_scene)
{
	ipi_queue_handler_t *handler = NULL;

	/* error handling */
	if (task_scene >= TASK_SCENE_SIZE) {
		AUD_LOG_W("%s(), task_scene %d invalid!! return NULL\n",
			  __func__, task_scene);
		return NULL;
	}

	/* create handler */
	handler = &g_ipi_queue_handler[task_scene];
	AUD_ASSERT(handler != NULL);

	if (handler->msg_queue == NULL) {
		handler->msg_queue = (void *)create_msg_queue(task_scene);
		if (handler->msg_queue == NULL) {
			AUD_LOG_W("%s(), task_scene %d msg_queue create fail return NULL\n",
				  __func__, task_scene);
			return NULL;
		}
	}

	return handler;
}


static msg_queue_t *create_msg_queue(const uint8_t task_scene)
{
	msg_queue_t *msg_queue = NULL;
	int i = 0;

	AUD_LOG_D("%s(+)\n", __func__);

	/* malloc */
	msg_queue = kmalloc(sizeof(msg_queue_t), GFP_KERNEL);
	if (msg_queue == NULL) {
		AUD_LOG_W("msg_queue kmalloc fail\n");
		return NULL;
	}

	/* init var */
	msg_queue->k_element_size = MAX_IPI_MSG_QUEUE_SIZE;
	for (i = 0; i < msg_queue->k_element_size; i++) {
		msg_queue->element[i].msg = NULL;
		init_waitqueue_head(&msg_queue->element[i].wq);
	}

	msg_queue->task_scene = task_scene;

	msg_queue->idx_r = 0;
	msg_queue->idx_w = 0;

	spin_lock_init(&msg_queue->rw_lock);

	memset(&msg_queue->ipi_msg_ack, 0, sizeof(ipi_msg_t));

	msg_queue->enable = true;

	AUD_LOG_D("%s(-)\n", __func__);
	return msg_queue;
}


void destroy_msg_queue(msg_queue_t *msg_queue)
{
	AUD_LOG_D("%s(+)\n", __func__);

	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return;
	}

	AUD_ASSERT(check_queue_empty(msg_queue));

	/* free */
	kfree(msg_queue);
	msg_queue = NULL;

	AUD_LOG_D("%s(-)\n", __func__);
}


void destroy_ipi_queue_handler(ipi_queue_handler_t *handler)
{
	/* error handling */
	if (handler == NULL) {
		AUD_LOG_W("%s(), handler == NULL!! return\n", __func__);
		return;
	}

	/* destroy handler */
	destroy_msg_queue((msg_queue_t *)handler->msg_queue);
	handler->msg_queue = NULL;
}


ipi_queue_handler_t *get_ipi_queue_handler(const uint8_t task_scene)
{
	/* error handling */
	if (task_scene >= TASK_SCENE_SIZE) {
		AUD_LOG_W("%s(), task_scene %d invalid!! return NULL\n", __func__, task_scene);
		return NULL;
	}

	return create_ipi_queue_handler(task_scene); /* TODO: get/create refine */
}


void disable_ipi_queue_handler(ipi_queue_handler_t *handler)
{
	msg_queue_t *msg_queue = NULL;

	/* error handling */
	if (handler == NULL) {
		AUD_LOG_W("%s(), handler == NULL!! return\n", __func__);
		return;
	}

	msg_queue = (msg_queue_t *)handler->msg_queue;
	msg_queue->enable = false;
}


int flush_ipi_queue_handler(ipi_queue_handler_t *handler)
{
	msg_queue_t *msg_queue = NULL;
	ipi_msg_t *p_ipi_msg = NULL;

	const uint16_t k_max_wait_times = 100;
	uint16_t i = 0;

	unsigned long flags = 0;

	/* error handling */
	if (handler == NULL) {
		AUD_LOG_W("%s(), handler == NULL!! return\n", __func__);
		return -1;
	}

	msg_queue = (msg_queue_t *)handler->msg_queue;
	AUD_ASSERT(msg_queue->enable == false);

	spin_lock_irqsave(&msg_queue->rw_lock, flags);
	if (check_queue_empty(msg_queue) == false) {
		p_ipi_msg = msg_queue->element[msg_queue->idx_r].msg;

		if (p_ipi_msg->ack_type == AUDIO_IPI_MSG_NEED_ACK) {
			print_msg_info(__func__, "fake ack return", p_ipi_msg);
			wake_up_interruptible(&msg_queue->element[msg_queue->idx_r].wq);
		}
	}
	spin_unlock_irqrestore(&msg_queue->rw_lock, flags); /* TODO: check */

	for (i = 0; i < k_max_wait_times; i++) {
		if (check_queue_empty(msg_queue))
			break;
		mdelay(10);
	}

	return 0;
}


/*
 * =============================================================================
 *                     main functions
 * =============================================================================
 */

int send_message(ipi_queue_handler_t *handler, ipi_msg_t *p_ipi_msg)
{
	msg_queue_t *msg_queue = NULL;
	bool is_queue_empty = false;

	unsigned long flags = 0;

	int idx_msg = -1;
	int retval = 0;

	AUD_LOG_V("%s(+)\n", __func__);

	/* error handling */
	if (handler == NULL) {
		AUD_LOG_W("%s(), handler == NULL!! return\n", __func__);
		return -1;
	}

	if (p_ipi_msg == NULL) {
		AUD_LOG_W("%s(), p_ipi_msg == NULL!! return\n", __func__);
		return -1;
	}


	/* send message in queue */
	msg_queue = (msg_queue_t *)handler->msg_queue;

	if (msg_queue->enable == false) {
		AUD_LOG_W("%s(), queue disabled!! return\n", __func__);
		return -1;
	}


	spin_lock_irqsave(&msg_queue->rw_lock, flags);
	is_queue_empty = check_queue_empty(msg_queue);
	idx_msg = push_msg(msg_queue, p_ipi_msg);
	spin_unlock_irqrestore(&msg_queue->rw_lock, flags);

	if (check_idx_msg_valid(msg_queue, idx_msg) == false) {
		AUD_LOG_W("%s(), idx_msg %d is invalid!! return\n", __func__, idx_msg);
		return -1;
	}

	/* process queue */
	if (is_queue_empty == true) { /* just send message to scp */
		AUD_ASSERT(msg_queue->ipi_msg_ack.magic == 0); /* no other  working msg ack */
		retval = process_message_in_queue(msg_queue, p_ipi_msg, idx_msg);
	} else { /* wait until processed, and then send message to scp */
		retval = wait_event_interruptible(
				 msg_queue->element[idx_msg].wq,
				 msg_queue->idx_r == idx_msg);

		if (retval == -ERESTARTSYS)
			retval = -EINTR;
		else if (msg_queue->enable == false)
			retval = -1;
		else
			retval = process_message_in_queue(msg_queue, p_ipi_msg, idx_msg);
	}

	AUD_LOG_V("%s(-)\n", __func__);
	return retval;
}


int send_message_ack(ipi_queue_handler_t *handler, ipi_msg_t *p_ipi_msg_ack)
{
	msg_queue_t *msg_queue = NULL;
	uint8_t task_scene = 0xFF;

	AUD_LOG_V("%s(+)\n", __func__);

	/* error handling */
	if (handler == NULL) {
		AUD_LOG_W("%s(), handler == NULL!! return\n", __func__);
		return -1;
	}

	if (p_ipi_msg_ack == NULL) {
		AUD_LOG_E("%s(), p_ipi_msg_ack = NULL, return\n", __func__);
		return -1;
	}

	if (p_ipi_msg_ack->ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		AUD_LOG_E("%s(), ack_type %d invalid, return\n",
			  __func__, p_ipi_msg_ack->ack_type);
		return -1;
	}


	/* get info */
	msg_queue = (msg_queue_t *)handler->msg_queue;
	task_scene = msg_queue->task_scene;

	if (msg_queue->enable == false) {
		AUD_LOG_W("%s(), queue disabled!! return\n", __func__);
		return -1;
	}


	/* get msg ack & wake up queue */
	AUD_ASSERT(msg_queue->ipi_msg_ack.magic == 0); /* no other  working msg ack */
	memcpy(&msg_queue->ipi_msg_ack, p_ipi_msg_ack, sizeof(ipi_msg_t));
	wake_up_interruptible(&msg_queue->element[msg_queue->idx_r].wq);

	AUD_LOG_V("%s(-)\n", __func__);
	return 0;
}


static int process_message_in_queue(msg_queue_t *msg_queue, ipi_msg_t *p_ipi_msg, int idx_msg)
{
	ipi_msg_t *p_ipi_msg_pop = NULL;
	bool is_queue_empty = false;

	unsigned long flags = 0;
	int retval = 0;

	AUD_LOG_D("%s(+)\n", __func__);

	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return -1;
	}

	if (p_ipi_msg == NULL) {
		AUD_LOG_W("%s(), p_ipi_msg == NULL!! return\n", __func__);
		return -1;
	}

	if (check_idx_msg_valid(msg_queue, idx_msg) == false) {
		AUD_LOG_W("%s(), idx_msg %d is invalid!! return\n", __func__, idx_msg);
		return -1;
	}


	/* process message */
	AUD_ASSERT(idx_msg == msg_queue->idx_r);

	switch (p_ipi_msg->ack_type) {
	case AUDIO_IPI_MSG_BYPASS_ACK: {
		/* no need ack, send directly and then just return */
		retval = (msg_queue->enable) ? send_message_to_scp(p_ipi_msg) : -1;
		break;
	}
	case AUDIO_IPI_MSG_NEED_ACK: {
		/* need ack, send and then wait until ack back */
		retval = (msg_queue->enable) ? send_message_to_scp(p_ipi_msg) : -1;

		if (retval == 0) { /* send to scp succeed, wait ack */
			retval = wait_event_interruptible(
					 msg_queue->element[msg_queue->idx_r].wq,
					 msg_queue->ipi_msg_ack.magic == IPI_MSG_MAGIC_NUMBER);
			if (retval == -ERESTARTSYS)
				retval = -EINTR;
			else if (msg_queue->enable == false)
				retval = -1;
			else {
				/* should be in pair */
				AUD_ASSERT(check_ack_msg_valid(p_ipi_msg, &msg_queue->ipi_msg_ack) == true);
				memcpy(p_ipi_msg, &msg_queue->ipi_msg_ack, sizeof(ipi_msg_t));
				memset(&msg_queue->ipi_msg_ack, 0, sizeof(ipi_msg_t));
			}
		}

		if (retval != 0)  /* message ack fail */
			p_ipi_msg->ack_type = AUDIO_IPI_MSG_CANCELED;

		break;
	}
	default: {
		print_msg_info(__func__, "invalid ack_type", p_ipi_msg);
		retval = -1;
		break;
	}
	}


	spin_lock_irqsave(&msg_queue->rw_lock, flags);

	/* pop message from queue */
	pop_msg(msg_queue, &p_ipi_msg_pop);
	AUD_ASSERT(p_ipi_msg_pop == p_ipi_msg);

	/* wake up next message */
	is_queue_empty = check_queue_empty(msg_queue);

	spin_unlock_irqrestore(&msg_queue->rw_lock, flags);


	if (is_queue_empty == false)
		wake_up_interruptible(&msg_queue->element[msg_queue->idx_r].wq);


	AUD_LOG_D("%s(-)\n", __func__);
	return retval;
}


/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline bool check_queue_empty(const msg_queue_t *msg_queue)
{
	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return false;
	}

	return (msg_queue->idx_r == msg_queue->idx_w);
}


inline bool check_queue_to_be_full(const msg_queue_t *msg_queue)
{
	uint8_t idx_w_to_be = 0;

	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return false;
	}

	idx_w_to_be = msg_queue->idx_w + 1;

	if (idx_w_to_be == msg_queue->k_element_size)
		idx_w_to_be = 0;

	return (idx_w_to_be == msg_queue->idx_r) ? true : false;
}


inline uint8_t get_num_messages_in_queue(const msg_queue_t *msg_queue)
{
	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return 0;
	}

	return (msg_queue->idx_w >= msg_queue->idx_r) ?
	       (msg_queue->idx_w - msg_queue->idx_r) :
	       ((msg_queue->k_element_size - msg_queue->idx_r) + msg_queue->idx_w);
}


inline int push_msg(msg_queue_t *msg_queue, ipi_msg_t *p_ipi_msg)
{
	int idx_msg = -1;

	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return -1;
	}

	if (p_ipi_msg == NULL) {
		AUD_LOG_E("%s(), p_ipi_msg = NULL, return\n", __func__);
		return -1;
	}

	/* check queue full */
	if (check_queue_to_be_full(msg_queue) == true) {
		AUD_LOG_W("task: %d, queue overflow, idx_w = %d, idx_r = %d\n",
			  p_ipi_msg->task_scene, msg_queue->idx_w, msg_queue->idx_r);
		print_msg_info(__func__, "drop msg", p_ipi_msg);
		return -1;
	}


	/* push */
	msg_queue->element[msg_queue->idx_w].msg = p_ipi_msg;
	idx_msg = msg_queue->idx_w;
	msg_queue->idx_w++;
	if (msg_queue->idx_w == msg_queue->k_element_size)
		msg_queue->idx_w = 0;

	AUD_LOG_D("task %d, push msg: 0x%x, idx_msg = %d, idx_r = %d, idx_w = %d\n",
		  p_ipi_msg->task_scene, p_ipi_msg->msg_id,
		  idx_msg, msg_queue->idx_r, msg_queue->idx_w);
	AUD_LOG_D("=> queue status(%d/%d)\n",
		  get_num_messages_in_queue(msg_queue), msg_queue->k_element_size);

	return idx_msg;
}


inline int pop_msg(msg_queue_t *msg_queue, ipi_msg_t **pp_ipi_msg)
{
	/* error handling */
	if (msg_queue == NULL) {
		AUD_LOG_W("%s(), msg_queue == NULL!! return\n", __func__);
		return -1;
	}

	if (pp_ipi_msg == NULL) {
		AUD_LOG_W("%s(), pp_ipi_msg == NULL!! return\n", __func__);
		return -1;
	}


	/* check queue empty */
	if (check_queue_empty(msg_queue) == true) {
		AUD_LOG_W("%s(), task: %d, queue is empty, idx_r = %d\n",
			  __func__, msg_queue->task_scene, msg_queue->idx_r);
		return -1;
	}

	/* pop */
	*pp_ipi_msg = msg_queue->element[msg_queue->idx_r].msg;
	msg_queue->idx_r++;
	if (msg_queue->idx_r == msg_queue->k_element_size)
		msg_queue->idx_r = 0;


	if (*pp_ipi_msg == NULL) {
		AUD_LOG_E("%s(), p_ipi_msg = NULL, return\n", __func__);
		return -1;
	}

	AUD_LOG_D("task %d, pop msg: 0x%x, idx_r = %d, idx_w = %d\n",
		  (*pp_ipi_msg)->task_scene, (*pp_ipi_msg)->msg_id,
		  msg_queue->idx_r, msg_queue->idx_w);
	AUD_LOG_D("=> queue status(%d/%d)\n",
		  get_num_messages_in_queue(msg_queue), msg_queue->k_element_size);

	return msg_queue->idx_r;
}

inline bool check_idx_msg_valid(msg_queue_t *msg_queue, int idx_msg)
{
	return (idx_msg >= 0 && idx_msg < msg_queue->k_element_size) ? true : false;
}


inline bool check_ack_msg_valid(const ipi_msg_t *p_ipi_msg, const ipi_msg_t *p_ipi_msg_ack)
{
	return (p_ipi_msg->task_scene == p_ipi_msg_ack->task_scene &&
		p_ipi_msg->msg_id     == p_ipi_msg_ack->msg_id) ? true : false;
}


