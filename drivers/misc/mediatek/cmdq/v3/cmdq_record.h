/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CMDQ_RECORD_H__
#define __CMDQ_RECORD_H__

#include <linux/types.h>
#include "cmdq_def.h"
#include "cmdq_core.h"

struct TaskStruct;

enum CMDQ_STACK_TYPE_ENUM {
	CMDQ_STACK_NULL = -1,
	CMDQ_STACK_TYPE_IF = 0,
	CMDQ_STACK_TYPE_ELSE = 1,
	CMDQ_STACK_TYPE_WHILE = 2,
	CMDQ_STACK_TYPE_BREAK = 3,
	CMDQ_STACK_TYPE_CONTINUE = 4,
	CMDQ_STACK_TYPE_DO_WHILE = 5,
};

#define CMDQ_DATA_BIT				(62)
#define CMDQ_BIT_VALUE				(0LL)
#define CMDQ_BIT_VAR				(1LL)
#define CMDQ_TASK_CPR_INITIAL_VALUE	(0)

struct cmdq_stack_node {
	uint32_t position;
	enum CMDQ_STACK_TYPE_ENUM stack_type;
	struct cmdq_stack_node *next;
};

struct cmdq_sub_function {
	bool is_subfunction;		/* [IN]true for subfunction */
	int32_t reference_cnt;
	uint32_t in_num;
	uint32_t out_num;
	CMDQ_VARIABLE *in_arg;
	CMDQ_VARIABLE *out_arg;
};

struct cmdqRecStruct {
	uint64_t engineFlag;
	int32_t scenario;
	uint32_t blockSize;	/* command size */
	void *pBuffer;
	uint32_t bufferSize;	/* allocated buffer size */
	struct TaskStruct *pRunningTask;	/* running task after flush() or startLoop() */
	enum CMDQ_HW_THREAD_PRIORITY_ENUM priority;	/* setting high priority. This implies Prefetch ENABLE. */
	bool finalized;		/* set to true after flush() or startLoop() */
	uint32_t prefetchCount;	/* maintenance prefetch instruction */

	struct cmdqSecDataStruct secData;	/* secure execution data */

	/* For v3 CPR use */
	struct cmdq_v3_replace_struct replace_instr;
	uint8_t local_var_num;
	struct cmdq_stack_node *if_stack_node;
	struct cmdq_stack_node *while_stack_node;
	CMDQ_VARIABLE arg_source;	/* poll source, wait_timeout event */
	CMDQ_VARIABLE arg_value;	/* poll value, wait_timeout start */
	CMDQ_VARIABLE arg_timeout;	/* wait_timeout timeout */

	/* profile marker */
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	struct cmdqProfileMarkerStruct profileMarker;
#endif
};

/* typedef dma_addr_t cmdqBackupSlotHandle; */
#define cmdqBackupSlotHandle dma_addr_t

/* typedef void *CmdqRecLoopHandle; */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create command queue recorder handle
 * Parameter:
 *     pHandle: pointer to retrieve the handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_task_create(enum CMDQ_SCENARIO_ENUM scenario, struct cmdqRecStruct **pHandle);
	int32_t cmdqRecCreate(enum CMDQ_SCENARIO_ENUM scenario, struct cmdqRecStruct **pHandle);

/**
 * Set engine flag for command queue picking HW thread
 * Parameter:
 *     pHandle: pointer to retrieve the handle
 *     engineFlag: Flag use to identify which HW module can be accessed
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_task_set_engine(struct cmdqRecStruct *handle, uint64_t engineFlag);
	int32_t cmdqRecSetEngine(struct cmdqRecStruct *handle, uint64_t engineFlag);

/**
 * Reset command queue recorder commands
 * Parameter:
 *    handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_task_reset(struct cmdqRecStruct *handle);
	int32_t cmdqRecReset(struct cmdqRecStruct *handle);

/**
 * Configure as secure task
 * Parameter:
 *     handle: the command queue recorder handle
 *     is_secure: true, execute the command in secure world
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. By default struct cmdqRecStruct records a normal command,
 *		  please call cmdq_task_set_secure to set command as SECURE after cmdq_task_reset
 */
	int32_t cmdq_task_set_secure(struct cmdqRecStruct *handle, const bool is_secure);
	int32_t cmdqRecSetSecure(struct cmdqRecStruct *handle, const bool is_secure);

/**
 * query handle is secure task or not
 * Parameter:
 *	  handle: the command queue recorder handle
 * Return:
 *	   0 for false (not secure) and 1 for true (is secure)
 */
	int32_t cmdq_task_is_secure(struct cmdqRecStruct *handle);
	int32_t cmdqRecIsSecure(struct cmdqRecStruct *handle);

/**
 * Add DPAC protection flag
 * Parameter:
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. after reset handle, user have to specify protection flag again
 */
	int32_t cmdq_task_secure_enable_dapc(struct cmdqRecStruct *handle, const uint64_t engineFlag);
	int32_t cmdqRecSecureEnableDAPC(struct cmdqRecStruct *handle, const uint64_t engineFlag);

/**
 * Add flag for M4U security ports
 * Parameter:
 * Note:
 *	   a. Secure CMDQ support when t-base enabled only
 *	   b. after reset handle, user have to specify protection flag again
 */
	int32_t cmdq_task_secure_enable_port_security(struct cmdqRecStruct *handle, const uint64_t engineFlag);
	int32_t cmdqRecSecureEnablePortSecurity(struct cmdqRecStruct *handle, const uint64_t engineFlag);

/**
 * Append mark command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecMark(struct cmdqRecStruct *handle);

/**
 * Append mark command to enable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecEnablePrefetch(struct cmdqRecStruct *handle);

/**
 * Append mark command to disable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecDisablePrefetch(struct cmdqRecStruct *handle);

/**
 * Append write command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified target register physical address
 *     argument / value: the specified target register value
 *     mask: the specified target register mask
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_write_reg(struct cmdqRecStruct *handle, uint32_t addr,
				   CMDQ_VARIABLE argument, uint32_t mask);
	int32_t cmdqRecWrite(struct cmdqRecStruct *handle, uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Append write command to the update secure buffer address in secure path
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   addr: the specified register physical address about module src/dst buffer address
 *	   type: base handle type
 *     base handle: secure handle of a secure mememory
 *     offset: offset related to base handle (secure buffer = addr(base_handle) + offset)
 *     size: secure buffer size
 *	   mask: 0xFFFF_FFFF
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *     support only when secure OS enabled
 */
	int32_t cmdq_op_write_reg_secure(struct cmdqRecStruct *handle, uint32_t addr,
				   enum CMDQ_SEC_ADDR_METADATA_TYPE type, uint32_t baseHandle,
				   uint32_t offset, uint32_t size, uint32_t port);
	int32_t cmdqRecWriteSecure(struct cmdqRecStruct *handle,
				   uint32_t addr,
				   enum CMDQ_SEC_ADDR_METADATA_TYPE type,
				   uint32_t baseHandle,
				   uint32_t offset, uint32_t size, uint32_t port);

/**
 * Append poll command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified register physical address
 *     value: the required register value
 *     mask: the required register mask
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_poll(struct cmdqRecStruct *handle, uint32_t addr, uint32_t value, uint32_t mask);
	int32_t cmdqRecPoll(struct cmdqRecStruct *handle, uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Append wait command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to "wait and CLEAR"
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_wait(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecWait(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);

/**
 * like cmdq_op_wait, but won't clear the event after
 * leaving wait state.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type wait for
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_wait_no_clear(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecWaitNoClear(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);

/**
 * Unconditionally set to given event to 0.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_clear_event(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecClearEventToken(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);

/**
 * Unconditionally set to given event to 1.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_set_event(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecSetEventToken(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM event);

/**
 * Replace overwite CPR parameters of arg_a.
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   index: the index of instruction to replace
 *	   new_arg_a: the desired cpr value to overwrite arg_a
 *	   new_arg_b: the desired cpr value to overwrite arg_b
 *	   new_arg_c: the desired cpr value to overwrite arg_c
 * Return:
 *	   0 for success; else the error code is returned
 */
	s32 cmdq_op_replace_overwrite_cpr(struct cmdqRecStruct *handle, u32 index,
		s32 new_arg_a, s32 new_arg_b, s32 new_arg_c);

/**
 * Read a register value to a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     hw_addr: register address to read from
 *     dst_data_reg: CMDQ GPR to write to
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_read_to_data_register(struct cmdqRecStruct *handle, uint32_t hw_addr,
					  enum CMDQ_DATA_REGISTER_ENUM dst_data_reg);
	int32_t cmdqRecReadToDataRegister(struct cmdqRecStruct *handle, uint32_t hw_addr,
					  enum CMDQ_DATA_REGISTER_ENUM dst_data_reg);

/**
 * Write a register value from a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     src_data_reg: CMDQ GPR to read from
 *     hw_addr: register address to write to
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_write_from_data_register(struct cmdqRecStruct *handle,
					     enum CMDQ_DATA_REGISTER_ENUM src_data_reg, uint32_t hw_addr);
	int32_t cmdqRecWriteFromDataRegister(struct cmdqRecStruct *handle,
					     enum CMDQ_DATA_REGISTER_ENUM src_data_reg,
					     uint32_t hw_addr);


/**
 *  Allocate 32-bit register backup slot
 *
 */
	int32_t cmdq_alloc_mem(cmdqBackupSlotHandle *p_h_backup_slot, uint32_t slotCount);
	int32_t cmdqBackupAllocateSlot(cmdqBackupSlotHandle *p_h_backup_slot, uint32_t slotCount);

/**
 *  Read 32-bit register backup slot by index
 *
 */
	int32_t cmdq_cpu_read_mem(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index,
				   uint32_t *value);
	int32_t cmdqBackupReadSlot(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index,
				   uint32_t *value);

/**
 *  Use CPU to write value into 32-bit register backup slot by index directly.
 *
 */
	int32_t cmdq_cpu_write_mem(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index,
					uint32_t value);
	int32_t cmdqBackupWriteSlot(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index,
				    uint32_t value);


/**
 *  Free allocated backup slot. DO NOT free them before corresponding
 *  task finishes. Becareful on AsyncFlush use cases.
 *
 */
	int32_t cmdq_free_mem(cmdqBackupSlotHandle h_backup_slot);
	int32_t cmdqBackupFreeSlot(cmdqBackupSlotHandle h_backup_slot);


/**
 *  Insert instructions to backup given 32-bit HW register
 *  to a backup slot.
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE the callback of cmdq_task_flush_async_callback().
 *
 */
	int32_t cmdq_op_read_reg_to_mem(struct cmdqRecStruct *handle,
					    cmdqBackupSlotHandle h_backup_slot,
					    uint32_t slot_index, uint32_t addr);
	int32_t cmdqRecBackupRegisterToSlot(struct cmdqRecStruct *handle,
					    cmdqBackupSlotHandle h_backup_slot,
					    uint32_t slot_index, uint32_t addr);

/**
 *  Insert instructions to write 32-bit HW register
 *  from a backup slot.
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE the callback of cmdq_task_flush_async_callback().
 *
 */
	int32_t cmdq_op_read_mem_to_reg(struct cmdqRecStruct *handle,
						   cmdqBackupSlotHandle h_backup_slot,
						   uint32_t slot_index, uint32_t addr);
	int32_t cmdqRecBackupWriteRegisterFromSlot(struct cmdqRecStruct *handle,
						   cmdqBackupSlotHandle h_backup_slot,
						   uint32_t slot_index, uint32_t addr);

/**
 *  Insert instructions to update slot with given 32-bit value
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE the callback of cmdq_task_flush_async_callback().
 *
 */
	int32_t cmdq_op_write_mem(struct cmdqRecStruct *handle, cmdqBackupSlotHandle h_backup_slot,
						uint32_t slot_index, uint32_t value);
	int32_t cmdqRecBackupUpdateSlot(struct cmdqRecStruct *handle,
					cmdqBackupSlotHandle h_backup_slot,
					uint32_t slot_index, uint32_t value);

/**
 * Trigger CMDQ to execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 * Note:
 *     This is a synchronous function. When the function
 *     returned, the recorded commands have been done.
 */
	int32_t cmdq_task_flush(struct cmdqRecStruct *handle);
	int32_t cmdqRecFlush(struct cmdqRecStruct *handle);

/**
 *  Flush the command; Also at the end of the command, backup registers
 *  appointed by addrArray.
 *
 */
	int32_t cmdq_task_flush_and_read_register(struct cmdqRecStruct *handle, uint32_t regCount,
					    uint32_t *addrArray, uint32_t *valueArray);
	int32_t cmdqRecFlushAndReadRegister(struct cmdqRecStruct *handle, uint32_t regCount,
					    uint32_t *addrArray, uint32_t *valueArray);

/**
 * Trigger CMDQ to asynchronously execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for successfully start execution; else the error code is returned
 * Note:
 *     This is an ASYNC function. When the function
 *     returned, it may or may not be finished. There is no way to retrieve the result.
 */
	int32_t cmdq_task_flush_async(struct cmdqRecStruct *handle);
	int32_t cmdqRecFlushAsync(struct cmdqRecStruct *handle);

	int32_t cmdq_task_flush_async_callback(struct cmdqRecStruct *handle, CmdqAsyncFlushCB callback,
					  uint32_t userData);
	int32_t cmdqRecFlushAsyncCallback(struct cmdqRecStruct *handle, CmdqAsyncFlushCB callback,
					  uint32_t userData);

/**
 * Trigger CMDQ to execute the recorded commands in loop.
 * each loop completion generates callback in interrupt context.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     irqCallback: this CmdqInterruptCB callback is called after each loop completion.
 *     data:   user data, this will pass back to irqCallback
 *     hLoop:  output, a handle used to stop this loop.
 *
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     This is an asynchronous function. When the function
 *     returned, the thread has started. Return -1 in irqCallback to stop it.
 */
	int32_t cmdq_task_start_loop(struct cmdqRecStruct *handle);
	int32_t cmdqRecStartLoop(struct cmdqRecStruct *handle);

	int32_t cmdq_task_start_loop_callback(struct cmdqRecStruct *handle, CmdqInterruptCB loopCB,
		unsigned long loopData);
	int32_t cmdqRecStartLoopWithCallback(struct cmdqRecStruct *handle, CmdqInterruptCB loopCB,
		unsigned long loopData);

	s32 cmdq_task_start_loop_sram(struct cmdqRecStruct *handle, const char *SRAM_owner_name);

/**
 * Unconditionally stops the loop thread.
 * Must call after cmdq_task_start_loop().
 */
	int32_t cmdq_task_stop_loop(struct cmdqRecStruct *handle);
	int32_t cmdqRecStopLoop(struct cmdqRecStruct *handle);

/**
 * Trigger CMDQ to copy data between DRAM and SRAM.
 *
 * Parameter:
 *     pa_src: the copy to source of DRAM PA address
 *     pa_dest: the copy from destination of DRAM PA address
 *     sram_src: the copy to destination of SRAM address
 *     sram_dest: the copy from source of SRAM address
 *     size: the copy size
 *
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     This is an BLOCKING function. When the function is returned,
 *     the SRAM move is done.
 */
	s32 cmdq_task_copy_to_sram(dma_addr_t pa_src, u32 sram_dest, size_t size);
	s32 cmdq_task_copy_from_sram(dma_addr_t pa_dest, u32 sram_src, size_t size);

/**
 * returns current count of instructions in given handle
 */
	int32_t cmdq_task_get_instruction_count(struct cmdqRecStruct *handle);
	int32_t cmdqRecGetInstructionCount(struct cmdqRecStruct *handle);

/**
 * Record timestamp while CMDQ HW executes here
 * This is for prfiling  purpose.
 *
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     Please define CMDQ_PROFILE_MARKER_SUPPORT in cmdq_def.h
 *     to enable profile marker.
 */
	int32_t cmdq_op_profile_marker(struct cmdqRecStruct *handle, const char *tag);
	int32_t cmdqRecProfileMarker(struct cmdqRecStruct *handle, const char *tag);

/**
 * Dump command buffer to kernel log
 * This is for debugging purpose.
 */
	int32_t cmdq_task_dump_command(struct cmdqRecStruct *handle);
	int32_t cmdqRecDumpCommand(struct cmdqRecStruct *handle);

/**
 * Estimate command execu time.
 * This is for debugging purpose.
 *
 * Note this estimation supposes all POLL/WAIT condition pass immediately
 */
	int32_t cmdq_task_estimate_command_exec_time(const struct cmdqRecStruct *handle);
	int32_t cmdqRecEstimateCommandExecTime(const struct cmdqRecStruct *handle);

/**
 * Destroy command queue recorder handle
 * Parameter:
 *     handle: the command queue recorder handle
 */
	int32_t cmdq_task_destroy(struct cmdqRecStruct *handle);
	void cmdqRecDestroy(struct cmdqRecStruct *handle);

/**
 * Change instruction of index to NOP instruction
 * Current NOP is [JUMP + 8]
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     index: the index of replaced instruction (start from 0)
 * Return:
 *     > 0 (index) for success; else the error code is returned
 */
	int32_t cmdq_op_set_nop(struct cmdqRecStruct *handle, uint32_t index);
	int32_t cmdqRecSetNOP(struct cmdqRecStruct *handle, uint32_t index);

/**
 * Query offset of instruction by instruction name
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     startIndex: Query offset from "startIndex" of instruction (start from 0)
 *     opCode: instruction name, you can use the following 6 instruction names:
 *		CMDQ_CODE_WFE					: create via cmdq_op_wait()
 *		CMDQ_CODE_SET_TOKEN			: create via cmdq_op_set_event()
 *		CMDQ_CODE_WAIT_NO_CLEAR		: create via cmdq_op_wait_no_clear()
 *		CMDQ_CODE_CLEAR_TOKEN			: create via cmdq_op_clear_event()
 *		CMDQ_CODE_PREFETCH_ENABLE		: create via cmdqRecEnablePrefetch()
 *		CMDQ_CODE_PREFETCH_DISABLE		: create via cmdqRecDisablePrefetch()
 *     event: the desired event type to set, clear, or wait
 * Return:
 *     > 0 (index) for offset of instruction; else the error code is returned
 */
	int32_t cmdq_task_query_offset(struct cmdqRecStruct *handle, uint32_t startIndex,
				   const enum CMDQ_CODE_ENUM opCode, enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecQueryOffset(struct cmdqRecStruct *handle, uint32_t startIndex,
				   const enum CMDQ_CODE_ENUM opCode, enum CMDQ_EVENT_ENUM event);

/**
 * acquire resource by resourceEvent
 * Parameter:
 *     handle: the command queue recorder handle
 *     resourceEvent: the event of resource to control in GCE thread
 * Return:
 *     0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 */
	int32_t cmdq_resource_acquire(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent);
	int32_t cmdqRecAcquireResource(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent);

/**
 * acquire resource by resourceEvent and ALSO ADD write instruction to use resource
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 *       addr, value, mask: same as cmdq_op_write_reg
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *	   Order: CPU clear resourceEvent at first, then add write instruction
 */
	int32_t cmdq_resource_acquire_and_write(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);
	int32_t cmdqRecWriteForResource(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Release resource by ADD INSTRUCTION to set event
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *       Remember to flush handle after this API to release resource via GCE
 */
	int32_t cmdq_resource_release(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent);
	int32_t cmdqRecReleaseResource(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent);

/**
 * Release resource by ADD INSTRUCTION to set event
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 *	   addr, value, mask: same as cmdq_op_write_reg
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *	   Order: Add add write instruction at first, then set resourceEvent instruction
 *       Remember to flush handle after this API to release resource via GCE
 */
	int32_t cmdq_resource_release_and_write(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);
	int32_t cmdqRecWriteAndReleaseResource(struct cmdqRecStruct *handle, enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Initialize the logical variable
 * Parameter:
 *	   arg: the variable you want to Initialize
 */
	void cmdq_op_init_variable(CMDQ_VARIABLE *arg);

/**
 * Initialize the logical variable
 * Parameter:
 *	   arg: the variable you want to Initialize
 *      cpr_offset: the cpr offset you want to use
 */
	void cmdq_op_init_global_cpr_variable(CMDQ_VARIABLE *arg, u32 cpr_offset);

/**
 * Append logic command for assign
 * arg_out = arg_b
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_in: the specified GCE CPR or value
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_assign(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out, CMDQ_VARIABLE arg_in);

/**
 * Append logic command for addition
 * arg_out = arg_b + arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_add(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for subtraction
 * arg_out = arg_b - arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_subtract(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for multiplication
 * arg_out = arg_b * arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_multiply(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for exclusive or operation
 * arg_out = arg_b ^ arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_xor(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for not operation
 * arg_out = ~arg_b
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_not(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out, CMDQ_VARIABLE arg_b);

/**
 * Append logic command for or operation
 * arg_out = arg_b | arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_or(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for or operation
 * arg_out = arg_b & arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_and(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for left shift operation
 * arg_out = arg_b << arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_left_shift(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append logic command for left right operation
 * arg_out = arg_b >> arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_right_shift(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
				   CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/**
 * Append commands for delay (micro second)
 * Parameter:
 *     handle: the command queue recorder handle
 *     delay_time: delay time in us
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_delay_us(struct cmdqRecStruct *handle, u32 delay_time);
	s32 cmdq_op_backup_CPR(struct cmdqRecStruct *handle, CMDQ_VARIABLE cpr,
		cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index);

/**
 * Append if statement command
 * if (arg_b condition arg_c)
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_if(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
				   enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/**
 * Append end if statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_end_if(struct cmdqRecStruct *handle);

/**
 * Append else statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_else(struct cmdqRecStruct *handle);

/**
 * Append if statement command
 * else if (arg_b condition arg_c)
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_else_if(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
				   enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/**
 * Append while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_while(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
				   enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/**
 * Append continue statement command into while loop
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_continue(struct cmdqRecStruct *handle);

/**
 * Append break statement command into while loop
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_break(struct cmdqRecStruct *handle);

/**
 * Append end while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_end_while(struct cmdqRecStruct *handle);

/*
 * Append do-while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_do_while(struct cmdqRecStruct *handle);

/*
 * Append end do while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_end_do_while(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
		enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);


/**
 * Linux like wait_event_timeout
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out, wait result (=0: timeout, >0: wait time when got event)
 *     wait_event: GCE event
 *     timeout_time: timeout time in us
 * Return:
 *     0 for success; else the error code is returned
 */
	s32 cmdq_op_wait_event_timeout(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
			enum CMDQ_EVENT_ENUM wait_event, u32 timeout_time);

/**
 * Append write command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified source register physical address
 *     arg_out: the value will be save in GCE CPR
 *     mask: the specified target register mask
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_read_reg(struct cmdqRecStruct *handle, uint32_t addr,
				   CMDQ_VARIABLE *arg_out, uint32_t mask);

/**
 *  Insert instructions to write to CMDQ variable
 *  from a backup memory.
 *
 */
	int32_t cmdq_op_read_mem(struct cmdqRecStruct *handle, cmdqBackupSlotHandle h_backup_slot,
				    uint32_t slot_index, CMDQ_VARIABLE *arg_out);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_RECORD_H__ */
