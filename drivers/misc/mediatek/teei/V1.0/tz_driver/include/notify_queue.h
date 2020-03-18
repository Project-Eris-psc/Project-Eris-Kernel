/*
 * Copyright (c) 2015-2016 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "utdriver_macro.h"

struct NQ_head {
	unsigned int start_index;
	unsigned int end_index;
	unsigned int Max_count;
	unsigned char reserve[20];
};

struct NQ_entry {
	unsigned int valid_flag;
	unsigned int length;
	unsigned int buffer_addr;
	unsigned char reserve[20];
};

struct create_NQ_struct {
	unsigned int n_t_nq_phy_addr;
	unsigned int n_t_size;
	unsigned int t_n_nq_phy_addr;
	unsigned int t_n_size;
};

/******************************
 * Message header
 ******************************/

struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};

struct ack_fast_call_struct {
	int retVal;
};

static unsigned long nt_t_buffer;
unsigned long t_nt_buffer;

extern struct semaphore boot_sema;
extern struct semaphore smc_lock;
extern unsigned long message_buff;

extern void invoke_fastcall(void);
