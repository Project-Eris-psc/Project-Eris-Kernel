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

#ifndef __TEEI_GATEKEEPER_H__
#define __TEEI_GATEKEEPER_H__
#include "utdriver_macro.h"

struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};

struct fdrv_message_head {
	unsigned int driver_type;
	unsigned int fdrv_param_length;
};

struct create_fdrv_struct {
	unsigned int fdrv_type;
	unsigned int fdrv_phy_addr;
	unsigned int fdrv_size;
};

struct ack_fast_call_struct {
	int retVal;
};

struct gatekeeper_command_struct {
	unsigned long mem_size;
	int retVal;
};

struct gatekeeper_command_struct gatekeeper_command_entry;

extern unsigned long message_buff;
extern unsigned long fdrv_message_buff;
extern int gatekeeper_call_flag;
unsigned long gatekeeper_buff_addr;
/* extern struct semaphore gatekeeper_lock; */
extern struct semaphore smc_lock;
extern struct semaphore boot_sema;
extern struct semaphore fdrv_sema;
extern struct mutex pm_mutex;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;
extern int fp_call_flag;
extern struct semaphore fdrv_lock;
extern struct fdrv_call_struct {
	int fdrv_call_type;
	int fdrv_call_buff_size;
	int retVal;
};

extern int get_current_cpuid(void);
extern void invoke_fastcall(void);
extern int add_work_entry(int work_type, unsigned long buff);
extern void ut_pm_mutex_lock(struct mutex *lock);
extern void ut_pm_mutex_unlock(struct mutex *lock);

#endif /*__TEEI_GATEKEEPER_H__*/
