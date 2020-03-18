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

struct work_entry {
	int call_no;
	int in_use;
	struct work_struct work;
};

struct service_handler {
	unsigned int sysno;
	void *param_buf;
	unsigned size;
	long (*init)(struct service_handler *handler);
	void (*deinit)(struct service_handler *handler);
	int (*handle)(struct service_handler *handler);
};

struct NQ_entry {
	unsigned int valid_flag;
	unsigned int length;
	unsigned int buffer_addr;
	unsigned char reserve[20];
};

struct load_soter_entry {
	unsigned long vfs_addr;
	struct work_struct work;
};


struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};

#define SCHED_ENT_CNT  10

extern irqreturn_t tlog_handler(void);
extern int vfs_thread_function(unsigned long virt_addr, unsigned long para_vaddr, unsigned long buff_vaddr);
extern int get_current_cpuid(void);
extern unsigned char *get_nq_entry(unsigned char *buffer_addr);
extern unsigned long t_nt_buffer;
extern unsigned long message_buff;
extern unsigned long boot_vfs_addr;
extern unsigned long boot_soter_flag;

static struct load_soter_entry load_ent;

extern struct service_handler reetime;
extern struct service_handler socket;
extern struct service_handler vfs_handler;
extern struct service_handler printer_driver;

extern unsigned long forward_call_flag;
extern unsigned long soter_error_flag;
extern struct semaphore smc_lock;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;

extern unsigned long bdrv_message_buff;
extern void load_func(struct work_struct *entry);
extern void work_func(struct work_struct *entry);
extern void nt_sched_t_call(void);
extern int irq_call_flag;
extern struct semaphore boot_sema;
extern struct semaphore fdrv_sema;
extern int fp_call_flag;
extern int keymaster_call_flag;
static struct work_entry work_ent;
static struct work_entry sched_work_ent[SCHED_ENT_CNT];
extern struct work_queue *secure_wq;
extern struct work_queue *bdrv_wq;
