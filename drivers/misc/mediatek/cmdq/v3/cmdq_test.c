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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/atomic.h>

#include "cmdq_record_private.h"
#include "cmdq_reg.h"
#include "cmdq_virtual.h"
#include "cmdq_mdp_common.h"
#include "cmdq_device.h"

#define CMDQ_TEST

#ifdef CMDQ_TEST

#define CMDQ_TESTCASE_PARAMETER_MAX		4
#define CMDQ_MONITOR_EVENT_MAX			10

#ifdef CMDQ_USE_LEGACY
#define CMDQ_TEST_MMSYS_DUMMY_OFFSET (0x890)
#else
/* use DUMMY_3(0x89C) because DUMMY_0/1 is CLKMGR SW */
#define CMDQ_TEST_MMSYS_DUMMY_OFFSET (0x8B0)
#endif

#define CMDQ_TEST_MMSYS_DUMMY_PA     (0x14000000 + CMDQ_TEST_MMSYS_DUMMY_OFFSET)
#define CMDQ_TEST_MMSYS_DUMMY_VA     (cmdq_dev_get_module_base_VA_MMSYS_CONFIG() + CMDQ_TEST_MMSYS_DUMMY_OFFSET)

#define CMDQ_TEST_GCE_DUMMY_PA       CMDQ_GPR_R32_PA(CMDQ_DATA_REG_2D_SHARPNESS_1)
#define CMDQ_TEST_GCE_DUMMY_VA       CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_1)

/* test configuration */
static DEFINE_MUTEX(gCmdqTestProcLock);

enum CMDQ_TEST_TYPE_ENUM {
	CMDQ_TEST_TYPE_NORMAL = 0,
	CMDQ_TEST_TYPE_SECURE = 1,
	CMDQ_TEST_TYPE_MONITOR_EVENT = 2,
	CMDQ_TEST_TYPE_MONITOR_POLL = 3,
	CMDQ_TEST_TYPE_OPEN_COMMAND_DUMP = 4,
	CMDQ_TEST_TYPE_DUMP_DTS = 5,
	CMDQ_TEST_TYPE_FEATURE_CONFIG = 6,
	CMDQ_TEST_TYPE_MMSYS_PERFORMANCE = 7,

	CMDQ_TEST_TYPE_MAX	/* ALWAYS keep at the end */
};

enum CMDQ_MOITOR_TYPE_ENUM {
	CMDQ_MOITOR_TYPE_FLUSH = 0,
	CMDQ_MOITOR_TYPE_WFE = 1,	/* wait for event and clear */
	CMDQ_MOITOR_TYPE_WAIT_NO_CLEAR = 2,
	CMDQ_MOITOR_TYPE_QUERYREGISTER = 3,

	CMDQ_MOITOR_TYPE_MAX	/* ALWAYS keep at the end */
};

struct cmdqMonitorEventStruct {
	bool status;

	struct cmdqRecStruct *cmdqHandle;
	cmdqBackupSlotHandle slotHandle;
	uint32_t monitorNUM;
	uint32_t waitType[CMDQ_MONITOR_EVENT_MAX];
	uint64_t monitorEvent[CMDQ_MONITOR_EVENT_MAX];
	uint32_t previousValue[CMDQ_MONITOR_EVENT_MAX];
};

struct cmdqMonitorPollStruct {
	bool status;

	struct cmdqRecStruct *cmdqHandle;
	cmdqBackupSlotHandle slotHandle;
	uint64_t pollReg;
	uint64_t pollValue;
	uint64_t pollMask;
	uint32_t delayTime;
	struct delayed_work delayContinueWork;
};

static int64_t gCmdqTestConfig[CMDQ_MONITOR_EVENT_MAX];
static bool gCmdqTestSecure;
static struct cmdqMonitorEventStruct gEventMonitor;
static struct cmdqMonitorPollStruct gPollMonitor;
#ifdef _CMDQ_TEST_PROC_
static struct proc_dir_entry *gCmdqTestProcEntry;
#endif

#define CMDQ_TEST_FAIL(string, args...) \
{			\
if (1) {	\
	pr_err("[CMDQ][ERR]TEST FAIL: "string, ##args); \
}			\
}

static int32_t _test_submit_async_internal(struct cmdqRecStruct *handle, struct TaskStruct **ppTask,
	bool internal)

{
	struct TaskPrivateStruct private = {
		.node_private_data = NULL,
		.internal = internal,
		.ignore_timeout = false,
	};
	struct cmdqCommandStruct desc = {
		.scenario = handle->scenario,
		.priority = handle->priority,
		.engineFlag = handle->engineFlag,
		.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer,
		.blockSize = handle->blockSize,
		.privateData = (cmdqU32Ptr_t)&private,
	};

	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* replace instuction position */
	cmdq_setup_replace_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTaskAsync(&desc, NULL, 0, ppTask);
}

static int32_t _test_submit_async(struct cmdqRecStruct *handle, struct TaskStruct **ppTask)
{
	return _test_submit_async_internal(handle, ppTask, false);
}

static s32 _test_submit_sync(struct cmdqRecStruct *handle)
{
	struct cmdqCommandStruct desc = { 0 };
	struct TaskPrivateStruct private = {
		.node_private_data = NULL,
		.internal = true,
		.ignore_timeout = false,
	};
	s32 status;

	status = cmdq_op_finalize_command(handle, false);
	if (status < 0)
		return status;

	CMDQ_MSG("Submit task scenario: %d, priority: %d, engine: 0x%llx, buffer: 0x%p, size: %d\n",
		 handle->scenario, handle->priority, handle->engineFlag, handle->pBuffer,
		 handle->blockSize);

	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	desc.privateData = (cmdqU32Ptr_t)&private;
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* replace instuction position */
	cmdq_setup_replace_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTask(&desc);
}

s32 _test_backup_instructions(struct TaskStruct *task, s32 **instructions_out)
{
	s32 *insts_buffer = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 buffer_count = 0;

	insts_buffer = vzalloc(task->bufferSize);
	if (!insts_buffer)
		return -ENOMEM;

	list_for_each_entry(cmd_buffer, &task->cmd_buffer_list, listEntry) {
		u32 buf_size = list_is_last(&cmd_buffer->listEntry, &task->cmd_buffer_list) ?
			CMDQ_CMD_BUFFER_SIZE - task->buf_available_size : CMDQ_CMD_BUFFER_SIZE;

		memcpy(insts_buffer + CMDQ_CMD_BUFFER_SIZE / sizeof(s32) * buffer_count,
			cmd_buffer->pVABase, buf_size);
		buffer_count++;
	}

	*instructions_out = insts_buffer;

	return 0;
}

void _test_free_backup_instructions(s32 **instructions_out)
{
	if (*instructions_out)
		vfree(*instructions_out);
	*instructions_out = NULL;
}


static void testcase_scenario(void)
{
	struct cmdqRecStruct *hRec;
	int32_t ret;
	int i = 0;

	CMDQ_MSG("%s\n", __func__);

	/* make sure each scenario runs properly with empty commands */
	for (i = 0; i < CMDQ_MAX_SCENARIO_COUNT; ++i) {
		if (cmdq_core_is_request_from_user_space(i) || i == CMDQ_SCENARIO_TIMER_LOOP)
			continue;

		CMDQ_MSG("testcase_scenario id:%d\n", i);
		cmdq_task_create((enum CMDQ_SCENARIO_ENUM) i, &hRec);
		cmdq_task_reset(hRec);
		cmdq_task_set_secure(hRec, false);
		ret = cmdq_task_flush(hRec);
	}

	cmdq_task_destroy(hRec);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer;

static void _testcase_sync_token_timer_func(unsigned long data)
{
	CMDQ_MSG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
}

static void _testcase_sync_token_timer_loop_func(unsigned long data)
{
	CMDQ_MSG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);

	/* repeate timeout until user delete it */
	mod_timer(&timer, jiffies + msecs_to_jiffies(10));
}

static void testcase_sync_token(void)
{
	struct cmdqRecStruct *hRec;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hRec);

	do {
		cmdq_task_reset(hRec);
		cmdq_task_set_secure(hRec, gCmdqTestSecure);

		/* setup timer to trigger sync token */
		setup_timer(&timer, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
		mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

		/* wait for sync token */
		cmdq_op_wait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdq_task_flush(hRec);
		CMDQ_MSG("waiting done\n");

		/* clear token */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
		del_timer(&timer);
	} while (0);

	CMDQ_MSG("%s, timeout case\n", __func__);
	/*  */
	/* test for timeout */
	/*  */
	do {
		cmdq_task_reset(hRec);
		cmdq_task_set_secure(hRec, gCmdqTestSecure);

		/* wait for sync token */
		cmdq_op_wait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdq_task_flush(hRec);
		CMDQ_MSG("waiting done\n");

		/* clear token */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	} while (0);

	cmdq_task_destroy(hRec);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_suspend_resume(void)
{
	struct cmdqRecStruct *hReqA;
	struct TaskStruct *pTaskA;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	/* setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0); */
	/* mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(300)); */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	do {
		/* let this thread wait for user token, then finish */
		cmdq_task_create(CMDQ_SCENARIO_PRIMARY_ALL, &hReqA);
		cmdq_task_reset(hReqA);
		cmdq_task_set_secure(hReqA, gCmdqTestSecure);
		cmdq_op_wait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1, 0, 0);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8, 0, 0);

		ret = _test_submit_async(hReqA, &pTaskA);

		CMDQ_MSG("%s pTask %p, engine:0x%llx, scenario:%d\n",
			 __func__, pTaskA, pTaskA->engineFlag, pTaskA->scenario);
		CMDQ_MSG("%s start suspend+resume thread 0========\n", __func__);
		cmdq_core_suspend_HW_thread(0, __LINE__);
		CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(0), 0x00);	/* resume */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

		msleep_interruptible(500);
		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);
	} while (0);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_destroy(hReqA);
	/* del_timer(&timer_reqA); */

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_errors(void)
{
	struct cmdqRecStruct *hReq;
	struct TaskStruct *pTask;
	int32_t ret;
	const uint32_t UNKNOWN_OP = 0x50;
	uint32_t *pCommand;

	ret = 0;
	do {
		/* SW timeout */
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);

		CMDQ_MSG("=============== INIFINITE Wait ===================\n");

		cmdqCoreClearEvent(CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &hReq);

		/* turn on ALL engine flag to test dump */
		for (ret = 0; ret < CMDQ_MAX_ENGINE_COUNT; ++ret)
			hReq->engineFlag |= 1LL << ret;

		cmdq_task_reset(hReq);
		cmdq_task_set_secure(hReq, gCmdqTestSecure);
		cmdq_op_wait(hReq, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_task_flush(hReq);

		CMDQ_MSG("=============== INIFINITE JUMP ===================\n");

		/* HW timeout */
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);
		cmdqCoreClearEvent(CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_task_reset(hReq);
		cmdq_task_set_secure(hReq, gCmdqTestSecure);
		cmdq_op_wait(hReq, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_append_command(hReq, CMDQ_CODE_JUMP, 0, 8, 0, 0);	/* JUMP to connect tasks */
		ret = _test_submit_async(hReq, &pTask);
		msleep_interruptible(500);
		ret = cmdqCoreWaitAndReleaseTask(pTask, 8000);

		CMDQ_MSG("================ POLL INIFINITE ====================\n");

		CMDQ_MSG("testReg: %lx\n", CMDQ_TEST_GCE_DUMMY_VA);

		CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0x0);
		cmdq_task_reset(hReq);
		cmdq_task_set_secure(hReq, gCmdqTestSecure);
		cmdq_op_poll(hReq, CMDQ_TEST_GCE_DUMMY_PA, 1, 0xFFFFFFFF);
		cmdq_task_flush(hReq);

		CMDQ_MSG("================= INVALID INSTR =================\n");

		/* invalid instruction */
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);
		cmdq_task_reset(hReq);
		cmdq_task_set_secure(hReq, gCmdqTestSecure);
		cmdq_append_command(hReq, CMDQ_CODE_JUMP, -1, 0, 0, 0);
		cmdq_task_flush(hReq);

		CMDQ_MSG("================= INVALID INSTR: UNKNOWN OP(0x%x) =================\n",
			 UNKNOWN_OP);
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);

		/* invalid instruction is asserted when unknown OP */
		cmdq_task_reset(hReq);
		cmdq_task_set_secure(hReq, gCmdqTestSecure);
		{
			pCommand = (uint32_t *) ((uint8_t *) hReq->pBuffer + hReq->blockSize);
			*pCommand++ = 0x0;
			*pCommand++ = (UNKNOWN_OP << 24);
			hReq->blockSize += 8;
		}
		cmdq_task_flush(hReq);

	} while (0);

	cmdq_task_destroy(hReq);

	CMDQ_MSG("%s END\n", __func__);
}

static int32_t finishCallback(unsigned long data)
{
	CMDQ_LOG("callback() with data=0x%08lx\n", data);
	return 0;
}

static void testcase_fire_and_forget(void)
{
	struct cmdqRecStruct *hReqA, *hReqB;

	CMDQ_MSG("%s\n", __func__);
	do {
		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReqA);
		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReqB);
		cmdq_task_reset(hReqA);
		cmdq_task_reset(hReqB);
		cmdq_task_set_secure(hReqA, gCmdqTestSecure);
		cmdq_task_set_secure(hReqB, gCmdqTestSecure);

		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdq_task_flush_async(hReqA);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdq_task_flush_async_callback(hReqB, finishCallback, 443);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
	} while (0);

	cmdq_task_destroy(hReqA);
	cmdq_task_destroy(hReqB);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_request(void)
{
	struct cmdqRecStruct *hReqA, *hReqB;
	struct TaskStruct *pTaskA, *pTaskB;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(1000));

	setup_timer(&timer_reqB, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_1);
	/* mod_timer(&timer_reqB, jiffies + msecs_to_jiffies(1300)); */

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	do {
		cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hReqA);
		cmdq_task_reset(hReqA);
		cmdq_task_set_secure(hReqA, gCmdqTestSecure);
		cmdq_op_wait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1, 0, 0);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8, 0, 0);

		cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hReqB);
		cmdq_task_reset(hReqB);
		cmdq_task_set_secure(hReqB, gCmdqTestSecure);
		cmdq_op_wait(hReqB, CMDQ_SYNC_TOKEN_USER_1);
		cmdq_append_command(hReqB, CMDQ_CODE_EOC, 0, 1, 0, 0);
		cmdq_append_command(hReqB, CMDQ_CODE_JUMP, 0, 8, 0, 0);

		ret = _test_submit_async(hReqA, &pTaskA);
		ret = _test_submit_async(hReqB, &pTaskB);

		CMDQ_MSG("%s start wait sleep========\n", __func__);
		msleep_interruptible(500);

		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);

		CMDQ_MSG("%s start wait B, this should timeout========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskB, 600);
		CMDQ_MSG("%s wait B get %d ========\n", __func__, ret);

	} while (0);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	cmdq_task_destroy(hReqA);
	cmdq_task_destroy(hReqB);

	del_timer(&timer_reqA);
	del_timer(&timer_reqB);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_multiple_async_request(void)
{
#define TEST_REQ_COUNT 24
	struct cmdqRecStruct *hReq[TEST_REQ_COUNT] = { 0 };
	struct TaskStruct *pTask[TEST_REQ_COUNT] = { 0 };
	int32_t ret = 0;
	int i;

	CMDQ_MSG("%s\n", __func__);

	setup_timer(&timer, &_testcase_sync_token_timer_loop_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(10));

	/* Queue multiple async request */
	/* to test dynamic task allocation */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < TEST_REQ_COUNT; ++i) {
		ret = cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReq[i]);
		if (ret < 0) {
			CMDQ_ERR("%s cmdq_task_create failed:%d, i:%d\n ", __func__, ret, i);
			continue;
		}

		cmdq_task_reset(hReq[i]);

		/* specify engine flag in order to dispatch all tasks to the same HW thread */
		hReq[i]->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

		cmdq_task_set_secure(hReq[i], gCmdqTestSecure);
		cmdq_op_wait(hReq[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_finalize_command(hReq[i], false);

		/* higher priority for later tasks */
		hReq[i]->priority = i;

		_test_submit_async(hReq[i], &pTask[i]);

		CMDQ_MSG("======== create task[%2d]=0x%p done ========\n", i, pTask[i]);
	}

	/* release token and wait them */
	for (i = 0; i < TEST_REQ_COUNT; ++i) {

		if (pTask[i] == NULL) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		msleep_interruptible(100);

		CMDQ_LOG("======== wait task[%2d]=0x%p ========\n", i, pTask[i]);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 1000);
		cmdq_task_destroy(hReq[i]);
	}

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	del_timer(&timer);

	CMDQ_MSG("%s END\n", __func__);
}


static void testcase_async_request_partial_engine(void)
{
	int32_t ret = 0;
	int i;
	enum CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,
		CMDQ_SCENARIO_JPEG_DEC,
		CMDQ_SCENARIO_PRIMARY_MEMOUT,
		CMDQ_SCENARIO_SUB_DISP,
		CMDQ_SCENARIO_DEBUG,
	};

	struct cmdqRecStruct *hReq;
	struct TaskStruct *pTasks[ARRAY_SIZE(scn)] = { 0 };
	struct timer_list *timers;

	timers = kmalloc_array(ARRAY_SIZE(scn), sizeof(struct timer_list), GFP_ATOMIC);
	if (timers == NULL)
		return;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	for (i = 0; i < ARRAY_SIZE(scn); ++i) {
		setup_timer(&timers[i], &_testcase_sync_token_timer_func,
			    CMDQ_SYNC_TOKEN_USER_0 + i);
		mod_timer(&timers[i], jiffies + msecs_to_jiffies(400 * (1 + i)));
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);

		cmdq_task_create(scn[i], &hReq);
		cmdq_task_reset(hReq);
		cmdq_task_set_secure(hReq, false);
		cmdq_op_wait(hReq, CMDQ_SYNC_TOKEN_USER_0 + i);
		cmdq_op_finalize_command(hReq, false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_submit_async(hReq, &pTasks[i]);

		cmdq_task_destroy(hReq);
	}

	/* wait for task completion */
	for (i = 0; i < ARRAY_SIZE(scn); ++i)
		ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(3000));

	/* clear token */
	for (i = 0; i < ARRAY_SIZE(scn); ++i) {
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);
		del_timer(&timers[i]);
	}

	if (timers != NULL) {
		kfree(timers);
		timers = NULL;
	}

	CMDQ_MSG("%s END\n", __func__);

}

static void _testcase_unlock_all_event_timer_func(unsigned long data)
{
	uint32_t token = 0;

	CMDQ_MSG("%s\n", __func__);

	/* trigger sync event */
	CMDQ_MSG("trigger events\n");
	for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token) {
		/* 3 threads waiting, so update 3 times */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
	}
}

static void testcase_sync_token_threaded(void)
{
	enum CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,	/* high prio */
		CMDQ_SCENARIO_JPEG_DEC,	/* normal prio */
		CMDQ_SCENARIO_TRIGGER_LOOP	/* normal prio */
	};
	int32_t ret = 0;
	int i = 0;
	uint32_t token = 0;
	struct timer_list eventTimer;
	struct cmdqRecStruct *hReq[ARRAY_SIZE(scn)] = { 0 };
	struct TaskStruct *pTasks[ARRAY_SIZE(scn)] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token */
	for (i = 0; i < ARRAY_SIZE(scn); ++i) {
		setup_timer(&eventTimer, &_testcase_unlock_all_event_timer_func, 0);
		mod_timer(&eventTimer, jiffies + msecs_to_jiffies(500));

		/*  */
		/* 3 threads, all wait & clear 511 events */
		/*  */
		cmdq_task_create(scn[i], &hReq[i]);
		cmdq_task_reset(hReq[i]);
		cmdq_task_set_secure(hReq[i], false);
		for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token)
			cmdq_op_wait(hReq[i], (enum CMDQ_EVENT_ENUM) token);

		cmdq_op_finalize_command(hReq[i], false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_submit_async(hReq[i], &pTasks[i]);
	}


	/* wait for task completion */
	msleep_interruptible(1000);
	for (i = 0; i < ARRAY_SIZE(scn); ++i)
		ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(5000));

	/* clear token */
	for (i = 0; i < ARRAY_SIZE(scn); ++i)
		cmdq_task_destroy(hReq[i]);

	del_timer(&eventTimer);
	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list g_loopTimer;
static int g_loopIter;
static struct cmdqRecStruct *hLoopReq;

static void _testcase_loop_timer_func(unsigned long data)
{
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	g_loopIter++;
}

static void testcase_loop(void)
{
	int status = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoopReq);
	cmdq_task_reset(hLoopReq);
	cmdq_task_set_secure(hLoopReq, false);
	cmdq_op_wait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);

	setup_timer(&g_loopTimer, &_testcase_loop_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	g_loopIter = 0;

	/* should success */
	status = cmdq_task_start_loop(hLoopReq);

	/* should fail because already started */
	CMDQ_MSG("============testcase_loop start loop\n");
	status = cmdq_task_start_loop(hLoopReq);

	cmdq_task_dump_command(hLoopReq);

	/* WAIT */
	while (g_loopIter < 20)
		msleep_interruptible(2000);

	msleep_interruptible(2000);

	CMDQ_MSG("============testcase_loop stop timer\n");
	cmdq_task_destroy(hLoopReq);
	del_timer(&g_loopTimer);

	CMDQ_MSG("%s\n", __func__);
}

static unsigned long gLoopCount;
static void _testcase_trigger_func(unsigned long data)
{
	/* trigger sync event */
	CMDQ_MSG("_testcase_trigger_func");
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);

	/* start again */
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));
	gLoopCount++;
}

#if 0
static void leave_loop_func(struct work_struct *w)
{
	CMDQ_MSG("leave_loop_func: cancel loop");
	cmdq_task_stop_loop(hLoopConfig);
	hLoopConfig = NULL;
}

DECLARE_WORK(leave_loop, leave_loop_func);

int32_t my_irq_callback(unsigned long data)
{
	CMDQ_MSG("%s data=%d\n", __func__, data);

	++gLoopCount;

	switch (data) {
	case 1:
		if (gLoopCount < 20)
			return 0;
		else
			return -1;
		break;
	case 2:
		if (gLoopCount > 40) {
			/* insert stopping cal */
			schedule_work(&leave_loop);
		}
		break;
	}
	return 0;
}
#endif

static void testcase_trigger_thread(void)
{
	struct cmdqRecStruct *hTrigger, *hConfig;
	int32_t ret = 0;
	int index = 0;

	CMDQ_MSG("%s\n", __func__);

	/* setup timer to trigger sync token for every 1 sec */
	setup_timer(&timer, &_testcase_trigger_func, 0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

	do {
		/* THREAD 1, trigger loop */
		cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
		cmdq_task_reset(hTrigger);
		/* * WAIT and CLEAR config dirty */
		/* cmdq_op_wait(hTrigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY); */

		/* * WAIT and CLEAR TE */
		/* cmdq_op_wait(hTrigger, CMDQ_EVENT_MDP_DSI0_TE_SOF); */

		/* * WAIT and CLEAR stream done */
		/* cmdq_op_wait(hTrigger, CMDQ_EVENT_MUTEX0_STREAM_EOF); */

		/* * WRITE mutex enable */
		/* cmdq_op_wait(hTrigger, MM_MUTEX_BASE + 0x20); */

		cmdq_op_wait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);

		/* * RUN forever but each IRQ trigger is bypass to my_irq_callback */
		ret = cmdq_task_start_loop(hTrigger);

		/* THREAD 2, config thread */
		cmdq_task_create(CMDQ_SCENARIO_JPEG_DEC, &hConfig);


		hConfig->priority = CMDQ_THR_PRIO_NORMAL;
		cmdq_task_reset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < 10; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1, 0, 0);

		ret = cmdq_task_flush(hConfig);
		CMDQ_MSG("flush 0\n");

		hConfig->priority = CMDQ_THR_PRIO_DISPLAY_CONFIG;
		cmdq_task_reset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < 10; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1, 0, 0);

		ret = cmdq_task_flush(hConfig);
		CMDQ_MSG("flush 1\n");

		cmdq_task_reset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < 500; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1, 0, 0);

		ret = cmdq_task_flush(hConfig);
		CMDQ_MSG("flush 2\n");

		/* WAIT */
		while (gLoopCount < 20)
			msleep_interruptible(2000);
	} while (0);

	del_timer(&timer);
	cmdq_task_destroy(hTrigger);
	cmdq_task_destroy(hConfig);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_prefetch_scenarios(void)
{
	/* make sure both prefetch and non-prefetch cases */
	/* handle 248+ instructions properly */
	struct cmdqRecStruct *hConfig;
	int32_t ret = 0;
	int index = 0, scn = 0;
	const int INSTRUCTION_COUNT = 500;

	CMDQ_MSG("%s\n", __func__);

	/* make sure each scenario runs properly with 248+ commands */
	for (scn = 0; scn < CMDQ_MAX_SCENARIO_COUNT; ++scn) {
		if (cmdq_core_is_request_from_user_space(scn) ||
			scn == CMDQ_SCENARIO_TIMER_LOOP)
			continue;

		CMDQ_MSG("testcase_prefetch_scenarios scenario:%d\n", scn);
		cmdq_task_create((enum CMDQ_SCENARIO_ENUM) scn, &hConfig);
		cmdq_task_reset(hConfig);
		/* insert tons of instructions */
		for (index = 0; index < INSTRUCTION_COUNT; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1, 0, 0);

		ret = cmdq_task_flush(hConfig);
	}

	cmdq_task_destroy(hConfig);
	CMDQ_MSG("%s END\n", __func__);
}

void testcase_clkmgr_impl(enum CMDQ_ENG_ENUM engine,
			  char *name,
			  const unsigned long testWriteReg,
			  const uint32_t testWriteValue,
			  const unsigned long testReadReg, const bool verifyWriteResult)
{
/* clkmgr is not available on FPGA */
#ifndef CONFIG_MTK_FPGA
	uint32_t value = 0;

	CMDQ_MSG("====== %s:%s ======\n", __func__, name);
	CMDQ_VERBOSE("clk engine:%d, name:%s\n", engine, name);
	CMDQ_VERBOSE("write reg(0x%lx) to 0x%08x, read reg(0x%lx), verify write result:%d\n",
		     testWriteReg, testWriteValue, testReadReg, verifyWriteResult);

	/* turn on CLK, function should work */
	CMDQ_MSG("enable_clock\n");
	if (engine == CMDQ_ENG_CMDQ) {
		/* Turn on CMDQ engine */
		cmdq_dev_enable_gce_clock(true);
	} else {
		/* Turn on MDP engines */
		cmdq_mdp_get_func()->enableMdpClock(true, engine);
	}

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if ((true == verifyWriteResult) && (testWriteValue != value)) {
		CMDQ_ERR("when enable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		/* BUG(); */
	}

	/* turn off CLK, function should not work and access register should not cause hang */
	CMDQ_MSG("disable_clock\n");
	if (engine == CMDQ_ENG_CMDQ) {
		/* Turn on CMDQ engine */
		cmdq_dev_enable_gce_clock(false);
	} else {
		/* Turn on MDP engines */
		cmdq_mdp_get_func()->enableMdpClock(false, engine);
	}


	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if (value != 0) {
		CMDQ_ERR("when disable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		/* BUG(); */
	}
#endif
}

static void testcase_clkmgr(void)
{
	CMDQ_MSG("%s\n", __func__);
#ifdef CMDQ_PWR_AWARE
	testcase_clkmgr_impl(CMDQ_ENG_CMDQ,
				 "CMDQ_TEST",
				 CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG),
				 0xFFFFDEAD, CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG), true);
	cmdq_mdp_get_func()->testcaseClkmgrMdp();
#endif				/* defined(CMDQ_PWR_AWARE) */

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_dram_access(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	uint32_t *regResults;
	dma_addr_t regResultsMVA;
	dma_addr_t dstMVA;
	uint32_t arg_a;
	uint32_t subsysCode;
	uint32_t *pCmdEnd = NULL;
	unsigned long long data64;

	CMDQ_MSG("%s\n", __func__);

	regResults = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
					       sizeof(uint32_t) * 2, &regResultsMVA, GFP_KERNEL);

	/* set up intput */
	regResults[0] = 0xdeaddead;	/* this is read-from */
	regResults[1] = 0xffffffff;	/* this is write-to */

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/*  */
	/* READ from DRAME: register to read from */
	/*  */
	/* note that we force convert to physical reg address. */
	/* if it is already physical address, it won't be affected (at least on this platform) */
	arg_a = CMDQ_TEST_GCE_DUMMY_PA;
	subsysCode = cmdq_core_subsys_from_phys_addr(arg_a);

	pCmdEnd = (uint32_t *) (((char *)handle->pBuffer) + handle->blockSize);

	CMDQ_MSG("pCmdEnd initial=0x%p, reg MVA=%pa, size=%d\n",
		 pCmdEnd, &regResultsMVA, handle->blockSize);

	/* Move &(regResults[0]) to CMDQ_DATA_REG_DEBUG_DST */
	*pCmdEnd = (uint32_t) CMDQ_PHYS_TO_AREG(regResultsMVA);
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	    ((regResultsMVA >> 32) & 0xffff) |
#endif
	    ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	pCmdEnd += 1;

	/*  */
	/* WRITE to DRAME: */
	/* from src_addr(CMDQ_DATA_REG_DEBUG_DST) to external RAM (regResults[1]) */
	/*  */

	/* Read data from *CMDQ_DATA_REG_DEBUG_DST to CMDQ_DATA_REG_DEBUG */
	*pCmdEnd = CMDQ_DATA_REG_DEBUG;
	pCmdEnd += 1;
	*pCmdEnd =
	    (CMDQ_CODE_READ << 24) | (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (6 <<
												21);
	pCmdEnd += 1;

	/* Load dst_addr to GPR: Move &(regResults[1]) to CMDQ_DATA_REG_DEBUG_DST */
	dstMVA = regResultsMVA + 4;	/* note regResults is a uint32_t array */
	*pCmdEnd = ((uint32_t) dstMVA);
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	    ((dstMVA >> 32) & 0xffff) |
#endif
	    ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	pCmdEnd += 1;

	/* Write from CMDQ_DATA_REG_DEBUG to *CMDQ_DATA_REG_DEBUG_DST */
	*pCmdEnd = CMDQ_DATA_REG_DEBUG;
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_WRITE << 24) |
	    (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (6 << 21);

	pCmdEnd += 1;

	handle->blockSize += 4 * 8;	/* 4 * 64-bit instructions */

	cmdq_task_dump_command(handle);

	cmdq_task_flush(handle);

	cmdq_task_dump_command(handle);

	cmdq_task_destroy(handle);

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_DEBUG_DST);

	CMDQ_MSG("regResults=[0x%08x, 0x%08x]\n", regResults[0], regResults[1]);
	CMDQ_MSG("CMDQ_DATA_REG_DEBUG=0x%08x, CMDQ_DATA_REG_DEBUG_DST=0x%llx\n",
		 CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG)), data64);

	if (regResults[1] != regResults[0]) {
		/* Test DRAM access fail */
		CMDQ_ERR("ERROR!!!!!!\n");
	} else {
		/* Test DRAM access success */
		CMDQ_MSG("OK!!!!!!\n");
	}

	cmdq_core_free_hw_buffer(cmdq_dev_get(), 2 * sizeof(uint32_t), regResults,
				 regResultsMVA);

	CMDQ_MSG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_long_command(void)
{
	int i;
	struct cmdqRecStruct *handle = NULL;
	uint32_t data;
	uint32_t pattern = 0x0;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	/* build a 64KB instruction buffer */
	for (i = 0; i < 64 * 1024 / 8; ++i) {
		pattern = i;
		cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, pattern, ~0);
	}
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* verify data */
	do {
		if (true == gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n", __func__);
			break;
		}

		data = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
		if (pattern != data) {
			CMDQ_TEST_FAIL("reg value is 0x%08x, not pattern 0x%08x\n", data,
				 pattern);
		}
	} while (0);
	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_perisys_apb(void)
{
#ifdef CMDQ_GPR_SUPPORT
	/* write value to PERISYS register */
	/* we use MSDC debug to test: */
	/* write SEL, read OUT. */

	const uint32_t MSDC_SW_DBG_SEL_PA = 0x11230000 + 0xA0;
	const uint32_t MSDC_SW_DBG_OUT_PA = 0x11230000 + 0xA4;
	const uint32_t AUDIO_TOP_CONF0_PA = 0x11220000;

#ifdef CMDQ_OF_SUPPORT
	const unsigned long MSDC_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MSDC0");
	const unsigned long AUDIO_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,AUDIO");
	const unsigned long MSDC_SW_DBG_OUT = MSDC_VA_BASE + 0xA4;
	const unsigned long AUDIO_TOP_CONF0 = AUDIO_VA_BASE;

	/* CMDQ_LOG("MSDC_VA_BASE:  VA:%lx, PA: 0x%08x\n", MSDC_VA_BASE, 0x11230000); */
	/* CMDQ_LOG("AUDIO_VA_BASE: VA:%lx, PA: 0x%08x\n", AUDIO_TOP_CONF0_PA, 0x11220000); */
#else
	const uint32_t MSDC_SW_DBG_OUT = 0xF1230000 + 0xA4;
	const uint32_t AUDIO_TOP_CONF0 = 0xF1220000;
#endif

	const uint32_t AUDIO_TOP_MASK = ~0 & ~(1 << 28 |
					       1 << 21 |
					       1 << 17 |
					       1 << 16 |
					       1 << 15 |
					       1 << 11 |
					       1 << 10 |
					       1 << 7 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 1 | 1 << 0);
	struct cmdqRecStruct *handle = NULL;
	uint32_t data = 0;
	uint32_t dataRead = 0;

	CMDQ_MSG("%s\n", __func__);
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_write_reg(handle, MSDC_SW_DBG_SEL_PA, 1, ~0);
	cmdq_task_flush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(MSDC_SW_DBG_OUT);
	if (data != ~0) {
		/* MSDC_SW_DBG_OUT would not same as sel setting */
		CMDQ_MSG("write 0xFFFFFFFF to MSDC_SW_DBG_OUT = 0x%08x=====\n", data);
		CMDQ_MSG("MSDC_SW_DBG_OUT: PA(0x%x) VA(0x%lx) =====\n", MSDC_SW_DBG_OUT_PA, MSDC_SW_DBG_OUT);
	}

	/* test read from AP_DMA_GLOBAL_SLOW_DOWN to CMDQ GPR */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_read_to_data_register(handle, MSDC_SW_DBG_OUT_PA, CMDQ_DATA_REG_PQ_COLOR);
	cmdq_task_flush(handle);

	/* verify data */
	dataRead = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != dataRead || data == 0) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: CMDQ_DATA_REG_PQ_COLOR is 0x%08x, different=====\n", dataRead);
		CMDQ_ERR("MSDC_SW_DBG_OUT: PA(0x%x) VA(0x%lx) =====\n", MSDC_SW_DBG_OUT_PA, MSDC_SW_DBG_OUT);
	}

	CMDQ_REG_SET32(AUDIO_TOP_CONF0, ~0);
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	if (data != ~0) {
		CMDQ_ERR("write 0xFFFFFFFF to AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
		CMDQ_ERR("AUDIO_TOP_CONF0: PA(0x%x) VA(0x%lx) =====\n", AUDIO_TOP_CONF0_PA, AUDIO_TOP_CONF0);
	}
	CMDQ_REG_SET32(AUDIO_TOP_CONF0, 0);
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("Before AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	cmdq_task_reset(handle);
	cmdq_op_write_reg(handle, AUDIO_TOP_CONF0_PA, ~0, AUDIO_TOP_MASK);
	cmdq_task_flush(handle);
	/* verify data */
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("after AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	if (data != AUDIO_TOP_MASK) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: AUDIO_TOP_CONF0 is 0x%08x=====\n", data);
		CMDQ_ERR("AUDIO_TOP_CONF0: PA(0x%x) VA(0x%lx) =====\n", AUDIO_TOP_CONF0_PA, AUDIO_TOP_CONF0);
	}

	cmdq_task_destroy(handle);

#ifdef CMDQ_OF_SUPPORT
	/* release registers map */
	cmdq_dev_free_module_base_VA(MSDC_VA_BASE);
	cmdq_dev_free_module_base_VA(AUDIO_VA_BASE);
#endif

	CMDQ_MSG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif				/* CMDQ_GPR_SUPPORT */
}

static void testcase_write_address(void)
{
	dma_addr_t pa = 0;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqCoreAllocWriteAddress(3, &pa);
	CMDQ_LOG("ALLOC: 0x%pa\n", &pa);
	value = cmdqCoreReadWriteAddress(pa);
	CMDQ_LOG("value 0: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 1);
	CMDQ_LOG("value 1: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 2);
	CMDQ_LOG("value 2: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 3);
	CMDQ_LOG("value 3: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 4);
	CMDQ_LOG("value 4: 0x%08x\n", value);

	value = cmdqCoreReadWriteAddress(pa + (4 * 20));
	CMDQ_LOG("value 80: 0x%08x\n", value);

	/* free invalid start address fist to verify error handle */
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:0, it's a error case\n");
	cmdqCoreFreeWriteAddress(0);

	/* ok case */
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:%pa, it's a ok case\n", &pa);
	cmdqCoreFreeWriteAddress(pa);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write_from_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	uint32_t value;
	const uint32_t PATTERN = 0xFFFFDEAD;
	const uint32_t srcGprId = CMDQ_DATA_REG_DEBUG;
	uint32_t dstRegPA;
	unsigned long dummy_va;

	CMDQ_MSG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dstRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dstRegPA = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* clean dst register value */
	CMDQ_REG_SET32((void *)dummy_va, 0x0);

	/* init GPR as value 0xFFFFDEAD */
	CMDQ_REG_SET32(CMDQ_GPR_R32(srcGprId), PATTERN);
	value = CMDQ_REG_GET32(CMDQ_GPR_R32(srcGprId));
	if (value != PATTERN) {
		CMDQ_ERR("init CMDQ_DATA_REG_DEBUG to 0x%08x failed, value: 0x%08x\n", PATTERN,
			 value);
	}

	/* write GPR data reg to hw register */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_from_data_register(handle, srcGprId, dstRegPA);
	cmdq_task_flush(handle);

	cmdq_task_dump_command(handle);

	cmdq_task_destroy(handle);

	/* verify */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != PATTERN) {
		CMDQ_ERR("%s failed, dstReg value is not 0x%08x, value: 0x%08x\n", __func__,
			 PATTERN, value);
	}

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_read_to_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	uint32_t data;
	unsigned long long data64;

	CMDQ_MSG("%s\n", __func__);

	/* init GPR 64 */
	CMDQ_REG_SET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST, 0x1234567890ABCDEFULL);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xbeefbeef);	/* R4 */
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0x0);	/* R5 */

	cmdq_get_func()->dumpGPR();

	/* [read 64 bit test] move data from GPR to GPR_Px: COLOR to COLOR_DST (64 bit) */
#if 1
	cmdq_op_read_to_data_register(handle, CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
				  CMDQ_DATA_REG_PQ_COLOR_DST);
#else
	/* 64 bit behavior of Read OP depends APB bus implementation */
	/* (CMDQ uses APB to access HW register, use AXI to access DRAM) */
	/* from DE's suggestion, */
	/* 1. for read HW register case, it's better to separate 1 x 64 bit length read to 2 x 32 bit length read */
	/* 2. for GPRx each assignment case, it's better performance to use MOVE op to read GPR_x1 to GPR_x2 */

	/* when Read 64 length failed, try to use move to clear up if APB issue */
	const uint32_t srcDataReg = CMDQ_DATA_REG_PQ_COLOR;
	const uint32_t dstDataReg = CMDQ_DATA_REG_PQ_COLOR_DST;
	/* arg_a, 22 bit 1: arg_b is GPR */
	/* arg_a, 23 bit 1: arg_a is GPR */
	cmdq_append_command(handle,
			    CMDQ_CODE_RAW,
			    (CMDQ_CODE_MOVE << 24) | (dstDataReg << 16) | (4 << 21) | (2 << 21),
			    srcDataReg);
#endif

	/* [read 32 bit test] move data from register value to GPR_Rx: MM_DUMMY_REG to COLOR(32 bit) */
	cmdq_op_read_to_data_register(handle, CMDQ_TEST_GCE_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);

	cmdq_task_flush(handle);
	cmdq_task_dump_command(handle);
	cmdq_task_destroy(handle);

	cmdq_get_func()->dumpGPR();

	/* verify data */
	data = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != 0xdeaddead) {
		/* Print error status */
		CMDQ_ERR("[Read 32 bit from GPR_Rx]TEST FAIL: PQ reg value is 0x%08x\n", data);
	}

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST);
	if (data64 != 0xbeefbeef) {
		CMDQ_ERR("[Read 64 bit from GPR_Px]TEST FAIL: PQ_DST reg value is 0x%llx\n",
			 data64);
	}

	CMDQ_MSG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_write_reg_from_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const uint32_t PATTEN = 0xBCBCBCBC;
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;
	uint32_t value = 0;
	long long value64 = 0LL;

	const enum CMDQ_DATA_REGISTER_ENUM dstRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM srcRegId = CMDQ_DATA_REG_DEBUG_DST;

	CMDQ_MSG("%s\n", __func__);

	/* init */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(dstRegId), 0xdeaddead);
	CMDQ_REG_SET64_GPR_PX(srcRegId, 0xdeaddeaddeaddead);

	cmdq_alloc_mem(&hSlot, 1);
	cmdq_cpu_write_mem(hSlot, 0, PATTEN);
	cmdq_cpu_read_mem(hSlot, 0, &value);
	if (value != PATTEN) {
		/* Print error status */
		CMDQ_ERR("%s, slot init failed\n", __func__);
	}

	/* Create cmdqRec */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* Reset command buffer */
	cmdq_task_reset(handle);

	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* Insert commands to write register with slot's value */
	cmdq_op_read_mem_to_reg(handle, hSlot, 0, CMDQ_TEST_GCE_DUMMY_PA);

	/* Execute commands */
	cmdq_task_flush(handle);

	/* debug dump command instructions */
	cmdq_task_dump_command(handle);

	/* we can destroy cmdqRec handle after flush. */
	cmdq_task_destroy(handle);

	/* verify */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTEN) {
		/* Print error status */
		CMDQ_ERR("%s failed, value:0x%x\n", __func__, value);
	}

	value = CMDQ_REG_GET32(CMDQ_GPR_R32(dstRegId));
	value64 = CMDQ_REG_GET64_GPR_PX(srcRegId);
	CMDQ_LOG("srcGPR(%x):0x%llx\n", srcRegId, value64);
	CMDQ_LOG("dstGPR(%x):0x%08x\n", dstRegId, value);

	/* release result free slot */
	cmdq_free_mem(hSlot);

	CMDQ_MSG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_backup_reg_to_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;
	int i;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	/* Create cmdqRec */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	/* Create Slot */
	cmdq_alloc_mem(&hSlot, 5);

	for (i = 0; i < 5; ++i)
		cmdq_cpu_write_mem(hSlot, i, i);

	for (i = 0; i < 5; ++i) {
		cmdq_cpu_read_mem(hSlot, i, &value);
		if (value != i) {
			/* Print error status */
			CMDQ_ERR("testcase_cmdqBackupWriteSlot FAILED!!!!!\n");
		}
		CMDQ_LOG("testcase_cmdqBackupWriteSlot OK!!!!!\n");
	}

	/* Reset command buffer */
	cmdq_task_reset(handle);

	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* Insert commands to backup registers */
	for (i = 0; i < 5; ++i)
		cmdq_op_read_reg_to_mem(handle, hSlot, i, CMDQ_TEST_GCE_DUMMY_PA);

	/* Execute commands */
	cmdq_task_flush(handle);

	/* debug dump command instructions */
	cmdq_task_dump_command(handle);

	/* we can destroy cmdqRec handle after flush. */
	cmdq_task_destroy(handle);

	/* verify data by reading it back from slot */
	for (i = 0; i < 5; ++i) {
		cmdq_cpu_read_mem(hSlot, i, &value);
		CMDQ_LOG("backup slot %d = 0x%08x\n", i, value);

		if (value != 0xdeaddead) {
			/* content error */
			CMDQ_ERR("content error!!!!!!!!!!!!!!!!!!!!\n");
		}
	}

	/* release result free slot */
	cmdq_free_mem(hSlot);

	CMDQ_MSG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_update_value_to_slot(void)
{
	int32_t i;
	uint32_t value;
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;
	const uint32_t PATTERNS[] = {
		0xDEAD0000, 0xDEAD0001, 0xDEAD0002, 0xDEAD0003, 0xDEAD0004
	};

	CMDQ_MSG("%s\n", __func__);

	/* Create Slot */
	cmdq_alloc_mem(&hSlot, 5);

	/*use CMDQ to update slot value */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	for (i = 0; i < 5; ++i)
		cmdq_op_write_mem(handle, hSlot, i, PATTERNS[i]);

	cmdq_task_flush(handle);
	cmdq_task_dump_command(handle);
	cmdq_task_destroy(handle);

	/* CPU verify value by reading it back from slot  */
	for (i = 0; i < 5; ++i) {
		cmdq_cpu_read_mem(hSlot, i, &value);

		if (PATTERNS[i] != value) {
			CMDQ_ERR("slot[%d] = 0x%08x...content error! It should be 0x%08x\n",
				 i, value, PATTERNS[i]);
		} else {
			CMDQ_LOG("slot[%d] = 0x%08x\n", i, value);
		}
	}

	/* release result free slot */
	cmdq_free_mem(hSlot);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_poll_run(u32 poll_value, u32 poll_mask, bool use_mmsys_dummy)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *p_task;
	uint32_t value = 0;
	uint32_t dstRegPA;
	unsigned long dummy_va;

	if (gCmdqTestSecure || use_mmsys_dummy) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dstRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dstRegPA = CMDQ_TEST_GCE_DUMMY_PA;
	}

	CMDQ_LOG("%s\n", __func__);
	CMDQ_LOG("poll value is 0x%08x\n", poll_value);
	CMDQ_LOG("poll mask is 0x%08x\n", poll_mask);
	CMDQ_LOG("use_mmsys_dummy is %u\n", use_mmsys_dummy);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	cmdq_op_poll(handle, dstRegPA, poll_value, poll_mask);

	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &p_task);
	cmdqCoreDebugDumpCommand(p_task);

	/* Set MMSYS dummy register value after clock is on */
	CMDQ_REG_SET32(dummy_va, poll_value);
	value = CMDQ_REG_GET32(dummy_va);
	CMDQ_LOG("target value is 0x%08x\n", value);

	cmdqCoreWaitAndReleaseTask(p_task, 500);
	cmdq_task_destroy(handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_poll(void)
{
	CMDQ_LOG("%s\n", __func__);

	testcase_poll_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, false);
	testcase_poll_run(0xdada1818, 0xFFFFFFFF, false);
	testcase_poll_run(0xdada1818 & 0x0000FF00, 0x0000FF00, false);
	testcase_poll_run(0x00001818, 0xFFFFFFFF, false);
	testcase_poll_run(0xdada1818 & 0xFF00FF00, 0xFF00FF00, true);
	testcase_poll_run(0xdada1818, 0xFFFFFFFF, true);
	testcase_poll_run(0xdada1818 & 0x0000FF00, 0x0000FF00, true);
	testcase_poll_run(0x00001818, 0xFFFFFFFF, true);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_write_with_mask(void)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	const uint32_t MASK = (1 << 16);
	const uint32_t EXPECT_RESULT = PATTERN & MASK;
	uint32_t value = 0;
	unsigned long dummy_va, dummy_pa;

	CMDQ_MSG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* set to 0x0 */
	CMDQ_REG_SET32((void *)dummy_va, 0x0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_reg(handle, dummy_pa, PATTERN, MASK);
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != EXPECT_RESULT) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, EXPECT_RESULT);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write(void)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	unsigned long dummy_va, dummy_pa;

	CMDQ_MSG("%s\n", __func__);

	if (gCmdqTestSecure) {
		dummy_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		dummy_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
	} else {
		dummy_va = CMDQ_TEST_GCE_DUMMY_VA;
		dummy_pa = CMDQ_TEST_GCE_DUMMY_PA;
	}

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32((void *)dummy_va, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_write_reg(handle, dummy_pa, PATTERN, ~0);
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32((void *)dummy_va);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_prefetch(void)
{
	struct cmdqRecStruct *handle = NULL;
	int i;
	uint32_t value = 0;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);	/* 0xDEADDEAD; */
	const uint32_t testRegPA = CMDQ_TEST_GCE_DUMMY_PA;
	const uint32_t REP_COUNT = 500;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* No prefetch. */
	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	for (i = 0; i < REP_COUNT; ++i)
		cmdq_op_write_reg(handle, testRegPA, PATTERN, ~0);

	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	msleep_interruptible(1000);

	/* use prefetch */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	for (i = 0; i < REP_COUNT; ++i)
		cmdq_op_write_reg(handle, testRegPA, PATTERN, ~0);

	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	cmdq_task_flush_async(handle);
	msleep_interruptible(1000);

	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_backup_register(void)
{
#ifdef CMDQ_GPR_SUPPORT
	struct cmdqRecStruct *handle = NULL;
	int ret = 0;
	uint32_t regAddr[3] = { CMDQ_TEST_GCE_DUMMY_PA,
		CMDQ_THR_CURR_ADDR_PA(5),
		CMDQ_THR_END_ADDR_PA(5)
	};
	uint32_t regValue[3] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xAAAAAAAA);
	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(5), 0xBBBBBBBB);
	CMDQ_REG_SET32(CMDQ_THR_END_ADDR(5), 0xCCCCCCCC);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	ret = cmdq_task_flush_and_read_register(handle, 3, regAddr, regValue);
	cmdq_task_destroy(handle);

	if (regValue[0] != 0xAAAAAAAA) {
		/* Print error status */
		CMDQ_ERR("regValue[0] is 0x%08x, wrong!\n", regValue[0]);
	}
	if (regValue[1] != 0xBBBBBBBB) {
		/* Print error status */
		CMDQ_ERR("regValue[1] is 0x%08x, wrong!\n", regValue[1]);
	}
	if (regValue[2] != 0xCCCCCCCC) {
		/* Print error status */
		CMDQ_ERR("regValue[2] is 0x%08x, wrong!\n", regValue[2]);
	}

	CMDQ_MSG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_get_result(void)
{
#ifdef CMDQ_GPR_SUPPORT
	int i;
	struct cmdqRecStruct *handle = NULL;
	int ret = 0;
	struct cmdqCommandStruct desc = { 0 };

	int registers[1] = { CMDQ_TEST_GCE_DUMMY_PA };
	int result[1] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	/* make sure each scenario runs properly with empty commands */
	/* use CMDQ_SCENARIO_PRIMARY_ALL to test */
	/* because it has COLOR0 HW flag */
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_ALL, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	/* insert dummy commands */
	cmdq_op_finalize_command(handle, false);

	/* init desc attributes after finalize command to ensure correct size and buffer addr */
	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;

	desc.regRequest.count = 1;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)registers;
	desc.regValue.count = 1;
	desc.regValue.regValues = (cmdqU32Ptr_t) (unsigned long)result;

	desc.secData.is_secure = handle->secData.is_secure;
	desc.secData.addrMetadataCount = 0;
	desc.secData.addrMetadataMaxCount = 0;
	desc.secData.waitCookie = 0;
	desc.secData.resetExecCnt = false;

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, 0xdeaddead);

	/* manually raise the dirty flag */
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX1_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX2_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX3_STREAM_EOF);

	for (i = 0; i < 1; ++i) {
		ret = cmdqCoreSubmitTask(&desc);
		if (CMDQ_U32_PTR(desc.regValue.regValues)[0] != 0xdeaddead) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x\n",
				 CMDQ_U32_PTR(desc.regValue.regValues)[0]);
		}
	}

	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
	return;
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static int _testcase_simplest_command_loop_submit(const uint32_t loop, enum CMDQ_SCENARIO_ENUM scenario,
						  const long long engineFlag,
						  const bool isSecureTask)
{
	struct cmdqRecStruct *handle = NULL;
	int32_t i;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(scenario, &handle);
	for (i = 0; i < loop; i++) {
		CMDQ_MSG("pid: %d, flush:%4d, engineFlag:0x%llx, isSecureTask:%d\n",
			 current->pid, i, engineFlag, isSecureTask);
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, isSecureTask);
		handle->engineFlag = engineFlag;
		cmdq_task_flush(handle);
	}
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);

	return 0;
}

/* threadfn: int (*threadfn)(void *data) */
static int _testcase_thread_dispatch(void *data)
{
	long long engineFlag;

	engineFlag = *((long long *)data);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG, engineFlag, false);

	return 0;
}

static void testcase_thread_dispatch(void)
{
	char threadName[20];
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const long long engineFlag1 = (0x1 << CMDQ_ENG_ISP_IMGI) | (0x1 << CMDQ_ENG_ISP_IMGO);
	const long long engineFlag2 = (0x1 << CMDQ_ENG_MDP_RDMA0) | (0x1 << CMDQ_ENG_MDP_WROT0);

	CMDQ_MSG("%s\n", __func__);
	CMDQ_MSG("=============== 2 THREAD with different engines ===============\n");

	sprintf(threadName, "cmdqKTHR_%llx", engineFlag1);
	pKThread1 = kthread_run(_testcase_thread_dispatch, (void *)(&engineFlag1), threadName);
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	sprintf(threadName, "cmdqKTHR_%llx", engineFlag2);
	pKThread2 = kthread_run(_testcase_thread_dispatch, (void *)(&engineFlag2), threadName);
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	msleep_interruptible(5 * 1000);

	/* ensure both thread execute all command */
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, engineFlag1, false);
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, engineFlag2, false);

	CMDQ_MSG("%s END\n", __func__);
}

static int _testcase_full_thread_array(void *data)
{
	/* this testcase will be passed only when cmdqSecDr support async config mode because */
	/* never execute event setting till IWC back to NWd */
#define MAX_FULL_THREAD_TASK_COUNT 50

	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *tasks[MAX_FULL_THREAD_TASK_COUNT] = {0};
	s32 i;

	/* clearn event first */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* specify engine flag in order to dispatch all tasks to the same HW thread */
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	for (i = 0; i < MAX_FULL_THREAD_TASK_COUNT; i++) {
		CMDQ_LOG("pid: %d, flush:%6d\n", current->pid, i);

		if (i == 40) {
			CMDQ_LOG("set token: %d to 1\n", CMDQ_SYNC_TOKEN_USER_0);
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		}

		_test_submit_async(handle, &tasks[i]);
	}

	for (i = 0; i < MAX_FULL_THREAD_TASK_COUNT; i++)
		cmdqCoreWaitAndReleaseTask(tasks[i], 500);
	cmdq_task_destroy(handle);

	return 0;
}

static void testcase_full_thread_array(void)
{
	char threadName[20];
	struct task_struct *pKThread;

	CMDQ_MSG("%s\n", __func__);

	sprintf(threadName, "cmdqKTHR");
	pKThread = kthread_run(_testcase_full_thread_array, NULL, threadName);
	if (IS_ERR(pKThread)) {
		/* create thread failed */
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
	}

	msleep_interruptible(5 * 1000);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_module_full_dump(void)
{
	struct cmdqRecStruct *handle = NULL;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();

	CMDQ_MSG("%s\n", __func__);

	/* enable full dump */
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* clean SW token to invoke SW timeout latter */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	/* turn on ALL except DISP engine flag to test dump */
	handle->engineFlag = ~(CMDQ_ENG_DISP_GROUP_BITS);

	CMDQ_LOG("%s, engine: 0x%llx, it's a timeout case\n", __func__, handle->engineFlag);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_flush(handle);

	/* disable full dump */
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(0);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_profile_marker(void)
{
	struct cmdqRecStruct *handle = NULL;
	/* const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16); */
	/* uint32_t value = 0; */

	CMDQ_MSG("%s\n", __func__);

	CMDQ_MSG("%s: write op without profile marker\n", __func__);
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, 0xBCBCBCBC, ~0);
	cmdq_task_flush(handle);

	CMDQ_MSG("%s: write op with profile marker\n", __func__);
	cmdq_task_reset(handle);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, 0x11111111, ~0);
	cmdq_op_profile_marker(handle, "WRI_BEGIN");
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, 0x22222222, ~0);
	cmdq_op_profile_marker(handle, "WRI_END");

	cmdq_task_dump_command(handle);
	cmdq_task_flush(handle);

	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_estimate_command_exec_time(void)
{
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle hSlot = 0;

	cmdq_alloc_mem(&hSlot, 1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	CMDQ_MSG("%s\n", __func__);

	CMDQ_LOG("=====write(1), write_w_mask(2), poll(2), wait(2), sync(1), eof(1), jump(1)\n");
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, 0xBBBBBBBA, ~0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, 0xBBBBBBBB, 0x1);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, 0xBBBBBBBC, 0x3);

	cmdq_op_poll(handle, CMDQ_TEST_GCE_DUMMY_PA, 0xCCCCCCCA, ~0);
	cmdq_op_poll(handle, CMDQ_TEST_GCE_DUMMY_PA, 0xCCCCCCCB, 0x1);

	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_clear_event(handle, CMDQ_SYNC_TOKEN_USER_1);

	cmdq_task_dump_command(handle);
	cmdq_task_estimate_command_exec_time(handle);

	CMDQ_LOG("=====slots...\n");

	cmdq_task_reset(handle);
	cmdq_op_read_reg_to_mem(handle, hSlot, 0, CMDQ_TEST_GCE_DUMMY_PA);
	cmdq_op_read_mem_to_reg(handle, hSlot, 0, CMDQ_TEST_GCE_DUMMY_PA);
	cmdq_op_write_mem(handle, hSlot, 0, 0xDEADDEAD);

	cmdq_task_dump_command(handle);
	cmdq_task_estimate_command_exec_time(handle);

	CMDQ_MSG("%s END\n", __func__);

	cmdq_free_mem(hSlot);
	cmdq_task_destroy(handle);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec.h"
#include "cmdq_sec_iwc_common.h"
#include "cmdqsectl_api.h"
int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
						       struct TaskStruct *pTask, int32_t thread,
						       CmdqSecFillIwcCB iwcFillCB, void *data);
#endif

void testcase_secure_basic(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s\n", __func__);

	do {

		CMDQ_MSG("=========== Hello cmdqSecTl ===========\n ");
		status =
		    cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_TEST_HELLO_TL, NULL,
								   CMDQ_INVALID_THREAD, NULL, NULL);
		if (status < 0) {
			/* entry cmdqSecTL failed */
			CMDQ_ERR("entry cmdqSecTL failed, status:%d\n", status);
		}

		CMDQ_MSG("=========== Hello cmdqSecDr ===========\n ");
		status =
		    cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_TEST_DUMMY, NULL,
								   CMDQ_INVALID_THREAD, NULL, NULL);
		if (status < 0) {
			/* entry cmdqSecDr failed */
			CMDQ_ERR("entry cmdqSecDr failed, status:%d\n", status);
		}
	} while (0);

	CMDQ_MSG("%s END\n", __func__);
#endif
}

void testcase_secure_disp_scenario(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	/* note: this case used to verify command compose in secure world. */
	/* It must test when DISP driver has switched primary DISP to secure path, */
	/* otherwise we should disable "enable GCE" in SWd in order to prevent phone hang */
	struct cmdqRecStruct *hDISP;
	struct cmdqRecStruct *hSubDisp;
	struct cmdqRecStruct *hDisableDISP;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);

	CMDQ_MSG("%s\n", __func__);
	CMDQ_LOG("=========== secure primary path ===========\n");
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &hDISP);
	cmdq_task_reset(hDISP);
	cmdq_task_set_secure(hDISP, true);
	cmdq_op_write_reg(hDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush(hDISP);
	cmdq_task_destroy(hDISP);

	CMDQ_LOG("=========== secure sub path ===========\n");
	cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hSubDisp);
	cmdq_task_reset(hSubDisp);
	cmdq_task_set_secure(hSubDisp, true);
	cmdq_op_write_reg(hSubDisp, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush(hSubDisp);
	cmdq_task_destroy(hSubDisp);

	CMDQ_LOG("=========== disp secure primary path ===========\n");
	cmdq_task_create(CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH, &hDisableDISP);
	cmdq_task_reset(hDisableDISP);
	cmdq_task_set_secure(hDisableDISP, true);
	cmdq_op_write_reg(hDisableDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush(hDisableDISP);
	cmdq_task_destroy(hDisableDISP);

	CMDQ_MSG("%s END\n", __func__);
#endif
}

void testcase_secure_meta_data(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct cmdqRecStruct *hReqMDP;
	struct cmdqRecStruct *hReqDISP;
	const uint32_t PATTERN_MDP = (1 << 0) | (1 << 2) | (1 << 16);
	const uint32_t PATTERN_DISP = 0xBCBCBCBC;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	CMDQ_MSG("=========== MDP case ===========\n");
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &hReqMDP);
	cmdq_task_reset(hReqMDP);
	cmdq_task_set_secure(hReqMDP, true);

	/* specify use MDP engine */
	hReqMDP->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0);

	/* enable secure test */
	cmdq_task_secure_enable_dapc(hReqMDP,
				(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0));
	cmdq_task_secure_enable_port_security(hReqMDP,
					(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0));

	/* record command */
	cmdq_op_write_reg(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_MDP, ~0);

	cmdq_task_flush(hReqMDP);
	cmdq_task_destroy(hReqMDP);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN_MDP) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN_MDP);
	}

	CMDQ_MSG("=========== DISP case ===========\n");
	cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &hReqDISP);
	cmdq_task_reset(hReqDISP);
	cmdq_task_set_secure(hReqDISP, true);

	/* enable secure test */
	cmdq_task_secure_enable_dapc(hReqDISP, (1LL << CMDQ_ENG_DISP_WDMA1));
	cmdq_task_secure_enable_port_security(hReqDISP, (1LL << CMDQ_ENG_DISP_WDMA1));

	/* record command */
	cmdq_op_write_reg(hReqDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_DISP, ~0);

	cmdq_task_flush(hReqDISP);
	cmdq_task_destroy(hReqDISP);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN_DISP) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN_DISP);
	}

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
#endif

}

void testcase_submit_after_error_happened(void)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t pollingVal = 0x00003001;

	CMDQ_MSG("%s\n", __func__);
	CMDQ_MSG("=========== timeout case ===========\n");

	/* let poll INIFINITE */
	/* CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, pollingVal); */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);

	cmdq_op_poll(handle, CMDQ_TEST_GCE_DUMMY_PA, pollingVal, ~0);
	cmdq_task_flush(handle);

	CMDQ_MSG("=========== okay case ===========\n");
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	/* clear up */
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_write_stress_test(void)
{
	int32_t loop;

	CMDQ_MSG("%s\n", __func__);

	loop = 1;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	loop = 100;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_multiple_command(void)
{
#define TEST_PREFETCH_MARKER_LOOP 2

	int32_t i;
	int32_t ret;
	struct cmdqRecStruct *handle[TEST_PREFETCH_MARKER_LOOP] = { 0 };
	struct TaskStruct *pTask[TEST_PREFETCH_MARKER_LOOP] = { 0 };

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s\n", __func__);
	for (i = 0; i < TEST_PREFETCH_MARKER_LOOP; i++) {
		CMDQ_MSG("=============== flush:%d/%d ===============\n",
			i, TEST_PREFETCH_MARKER_LOOP);

		cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &(handle[i]));
		cmdq_task_reset(handle[i]);
		cmdq_task_set_secure(handle[i], false);

		/* record instructions which needs prefetch */
		cmdqRecEnablePrefetch(handle[i]);
		cmdq_op_wait(handle[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdqRecDisablePrefetch(handle[i]);

		/* record instructions which does not need prefetch */
		cmdq_op_write_reg(handle[i], CMDQ_TEST_GCE_DUMMY_PA, 0x3000, ~0);

		cmdq_op_finalize_command(handle[i], false);
		cmdq_task_dump_command(handle[i]);

		ret = _test_submit_async(handle[i], &pTask[i]);
	}

	for (i = 0; i < TEST_PREFETCH_MARKER_LOOP; ++i) {
		if (pTask[i] == NULL) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		msleep_interruptible(100);

		CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
		cmdq_task_destroy(handle[i]);
	}

	CMDQ_MSG("%s END\n", __func__);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static int _testcase_concurrency(void *data)
{
	uint32_t securePath;

	securePath = *((uint32_t *) data);

	CMDQ_MSG("start secure(%d) path\n", securePath);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG,
					       (0x1 << CMDQ_ENG_MDP_RSZ0), securePath);

	return 0;
}
#endif

static void testcase_concurrency_for_normal_path_and_secure_path(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const uint32_t securePath[2] = { 0, 1 };

	CMDQ_MSG("%s\n", __func__);

	pKThread1 = kthread_run(_testcase_concurrency, (void *)(&securePath[0]), "cmdqNormal");
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create cmdqNormal failed\n");
		return;
	}

	pKThread2 = kthread_run(_testcase_concurrency, (void *)(&securePath[1]), "cmdqSecure");
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create cmdqSecure failed\n");
		return;
	}

	msleep_interruptible(5 * 1000);

	/* ensure both thread execute all command */
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0x0, false);

	CMDQ_MSG("%s END\n", __func__);

	return;
#endif
}

void testcase_async_write_stress_test(void)
{
#if 0
#define LOOP 100

	int32_t i;
	int32_t ret;
	struct cmdqRecStruct *handle[LOOP] = { 0 };
	struct TaskStruct *pTask[LOOP] = { 0 };

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s\n", __func__);
	for (i = 0; i < LOOP; i++) {
		CMDQ_MSG("=============== flush:%d/%d ===============\n", i, LOOP);

		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &(handle[i]));
		cmdq_task_reset(handle[i]);
		cmdq_task_set_secure(handle[i], gCmdqTestSecure);

		cmdq_op_wait(handle[i], CMDQ_SYNC_TOKEN_USER_0);

		cmdq_op_finalize_command(handle[i], false);

		ret = _test_submit_async(handle[i], &pTask[i]);
	}

	/* release token and wait them */
	for (i = 0; i < LOOP; ++i) {

		if (pTask[i] == NULL) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		msleep_interruptible(100);

		CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
		cmdq_task_destroy(handle[i]);
	}

	CMDQ_MSG("%s END\n", __func__);
#endif
}

static void testcase_nonsuspend_irq(void)
{
	struct cmdqRecStruct *handle, *handle2;
	struct TaskStruct *pTask, *pTask2;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdq_task_reset(handle2);
	cmdq_task_set_secure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	/* force GCE to wait in second command before EOC */
	cmdq_op_wait(handle2, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle2, false);

	_test_submit_async(handle, &pTask);
	_test_submit_async(handle2, &pTask2);

	msleep_interruptible(500);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* test code: use to trigger GCE continue test command, put in cmdq_core::handleIRQ to test */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_MSG("IRQ: After set user sw token\n");

	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdqCoreWaitAndReleaseTask(pTask2, 500);
	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_module_full_mdp_engine(void)
{
	struct cmdqRecStruct *handle = NULL;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();

	CMDQ_MSG("%s\n", __func__);

	/* enable full dump */
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* turn on ALL except DISP engine flag to test clock operation */
	handle->engineFlag = ~(CMDQ_ENG_DISP_GROUP_BITS);

	CMDQ_LOG("%s, engine: 0x%llx, it's a engine clock test case\n",
		 __func__, handle->engineFlag);

	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_task_flush(handle);

	/* disable full dump */
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(0);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_trigger_engine_dispatch_check(void)
{
	struct cmdqRecStruct *handle, *handle2, *hTrigger;
	struct TaskStruct *pTask;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	uint32_t loopIndex = 0;

	CMDQ_MSG("%s\n", __func__);

	/* Create first task and run without wait */
	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &pTask);

	/* Create trigger loop */
	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
	cmdq_task_reset(hTrigger);
	cmdq_op_wait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_start_loop(hTrigger);
	/* Sleep to let trigger loop run fow a while */
	CMDQ_MSG("%s before start sleep and trigger token\n", __func__);
	for (loopIndex = 0; loopIndex < 10; loopIndex++) {
		msleep_interruptible(500);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		CMDQ_MSG("%s after sleep 5000 and send (%d)\n", __func__, loopIndex);
	}

	/* Create second task and should run well */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdq_task_reset(handle2);
	cmdq_task_set_secure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle2, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush(handle2);
	cmdq_task_destroy(handle2);

	/* Call wait to release first task */
	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdq_task_destroy(handle);
	cmdq_task_destroy(hTrigger);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_complicated_engine_thread(void)
{
#define TASK_COUNT 6
	struct cmdqRecStruct *handle[TASK_COUNT] = { 0 };
	struct TaskStruct *pTask[TASK_COUNT] = { 0 };
	uint64_t engineFlag[TASK_COUNT] = { 0 };
	uint32_t taskIndex = 0;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	/* config engine flag for test */
	engineFlag[0] = (1LL << CMDQ_ENG_MDP_RDMA0);
	engineFlag[1] = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_RSZ0);
	engineFlag[2] = (1LL << CMDQ_ENG_MDP_RSZ0);
	engineFlag[3] = (1LL << CMDQ_ENG_MDP_TDSHP0);
	engineFlag[4] = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_TDSHP0);
	engineFlag[5] = (1LL << CMDQ_ENG_MDP_TDSHP0) | (1LL << CMDQ_ENG_MDP_RSZ0);

	for (taskIndex = 0; taskIndex < TASK_COUNT; taskIndex++) {
		/* Create task and run with wait */
		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle[taskIndex]);
		cmdq_task_reset(handle[taskIndex]);
		cmdq_task_set_secure(handle[taskIndex], gCmdqTestSecure);
		handle[taskIndex]->engineFlag = engineFlag[taskIndex];
		cmdq_op_wait(handle[taskIndex], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_op_finalize_command(handle[taskIndex], false);
		_test_submit_async(handle[taskIndex], &pTask[taskIndex]);
	}

	for (taskIndex = 0; taskIndex < TASK_COUNT; taskIndex++) {
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		/* Call wait to release task */
		cmdqCoreWaitAndReleaseTask(pTask[taskIndex], 500);
		cmdq_task_destroy(handle[taskIndex]);
		msleep_interruptible(1000);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_append_task_verify(void)
{
	struct cmdqRecStruct *handle, *handle2;
	struct TaskStruct *pTask, *pTask2;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	uint32_t loopIndex = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle2);
	for (loopIndex = 0; loopIndex < 2; loopIndex++) {
		/* clear token */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
		/* clear dummy register */
		CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

		/* Create first task and run with wait */
		/* use CMDQ to set to PATTERN */
		cmdq_task_reset(handle);
		cmdq_task_set_secure(handle, gCmdqTestSecure);
		if (loopIndex == 1)
			cmdqRecEnablePrefetch(handle);
		cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
		if (loopIndex == 1)
			cmdqRecDisablePrefetch(handle);
		cmdq_op_finalize_command(handle, false);

		/* Create second task and should run well */
		cmdq_task_reset(handle2);
		cmdq_task_set_secure(handle2, gCmdqTestSecure);
		if (loopIndex == 1)
			cmdqRecEnablePrefetch(handle2);
		cmdq_op_write_reg(handle2, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
		if (loopIndex == 1)
			cmdqRecDisablePrefetch(handle2);
		cmdq_op_finalize_command(handle2, false);

		_test_submit_async(handle, &pTask);
		_test_submit_async(handle2, &pTask2);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		/* Call wait to release first task */
		cmdqCoreWaitAndReleaseTask(pTask, 500);
		cmdqCoreWaitAndReleaseTask(pTask2, 500);

		/* value check */
		value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
		if (value != PATTERN) {
			/* test fail */
			CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
		}
	}

	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_manual_suspend_resume_test(void)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *pTask, *pTask2;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	_test_submit_async(handle, &pTask);

	/* Manual suspend and resume */
	cmdqCoreSuspend();
	cmdqCoreResumedNotifier();

	_test_submit_async(handle, &pTask2);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	/* Call wait to release second task */
	cmdqCoreWaitAndReleaseTask(pTask2, 500);
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_timeout_wait_early_test(void)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *pTask;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	_test_submit_async(handle, &pTask);

	cmdq_task_flush(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	/* Call wait to release first task */
	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_timeout_reorder_test(void)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, false);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);
	handle->priority = 0;
	cmdq_task_flush_async(handle);
	handle->priority = 2;
	cmdq_task_flush_async(handle);
	handle->priority = 4;
	cmdq_task_flush_async(handle);
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_error_irq(void)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	struct TaskStruct *pTask;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* wait and block instruction */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_flush_async(handle);

	/* invalid instruction */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_append_command(handle, CMDQ_CODE_JUMP, -1, 0, 0, 0);
	cmdq_task_dump_command(handle);
	cmdq_task_flush_async(handle);

	/* Normal command */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush_async(handle);

	/* invalid instruction is asserted when unknown OP */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	{
		const uint32_t UNKNOWN_OP = 0x50;
		uint32_t *pCommand;

		pCommand = (uint32_t *) ((uint8_t *) handle->pBuffer + handle->blockSize);
		*pCommand++ = 0x0;
		*pCommand++ = (UNKNOWN_OP << 24);
		handle->blockSize += 8;
	}
	cmdq_task_flush_async(handle);

	/* use CMDQ to set to PATTERN */
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &pTask);

	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_open_buffer_dump(int32_t scenario, int32_t bufferSize)
{
	CMDQ_MSG("%s\n", __func__);

	CMDQ_MSG("[TESTCASE]CONFIG: bufferSize: %d, scenario: %d\n", bufferSize, scenario);
	cmdq_core_set_command_buffer_dump(scenario, bufferSize);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_check_dts_correctness(void)
{
	CMDQ_MSG("%s\n", __func__);

	cmdq_dev_test_dts_correctness();

	CMDQ_MSG("%s END\n", __func__);
}

static int32_t testcase_monitor_callback(unsigned long data)
{
	uint32_t i;
	uint32_t monitorValue[CMDQ_MONITOR_EVENT_MAX];
	uint32_t durationTime[CMDQ_MONITOR_EVENT_MAX];

	if (false == gEventMonitor.status)
		return 0;

	for (i = 0; i < gEventMonitor.monitorNUM; i++) {
		/* Read monitor time */
		cmdq_cpu_read_mem(gEventMonitor.slotHandle, i, &monitorValue[i]);

		switch (gEventMonitor.waitType[i]) {
		case CMDQ_MOITOR_TYPE_WFE:
			durationTime[i] = (monitorValue[i] - gEventMonitor.previousValue[i]) * 76;
			CMDQ_LOG("[MONITOR][WFE] event: %s, duration: (%u ns)\n",
				cmdq_core_get_event_name_ENUM(gEventMonitor.monitorEvent[i]), durationTime[i]);
			CMDQ_MSG("[MONITOR][WFE] time:(%u ns)\n", monitorValue[i]);
			break;
		case CMDQ_MOITOR_TYPE_WAIT_NO_CLEAR:
			durationTime[i] = (monitorValue[i] - gEventMonitor.previousValue[i]) * 76;
			CMDQ_LOG("[MONITOR][Wait] event: %s, duration: (%u ns)\n",
				cmdq_core_get_event_name_ENUM(gEventMonitor.monitorEvent[i]), durationTime[i]);
			CMDQ_MSG("[MONITOR] time:(%u ns)\n", monitorValue[i]);
			break;
		case CMDQ_MOITOR_TYPE_QUERYREGISTER:
			CMDQ_LOG("[MONITOR] Register:0x08%llx, value:(0x04%x)\n", gEventMonitor.monitorEvent[i],
				monitorValue[i]);
			break;
		}
		/* Update previous monitor time */
		gEventMonitor.previousValue[i] = monitorValue[i];
	}

	return 0;
}

static void testcase_monitor_trigger_initialization(void)
{
	/* Create Slot*/
	cmdq_alloc_mem(&gEventMonitor.slotHandle, CMDQ_MONITOR_EVENT_MAX);
	/* Create CMDQ handle */
	cmdq_task_create(CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP, &gEventMonitor.cmdqHandle);
	cmdq_task_reset(gEventMonitor.cmdqHandle);
	/* Insert enable pre-fetch instruction */
	cmdqRecEnablePrefetch(gEventMonitor.cmdqHandle);
}

static void testcase_monitor_trigger(uint32_t waitType, uint64_t monitorEvent)
{
	int32_t eventID;
	bool successAddInstruction = false;

	CMDQ_MSG("%s\n", __func__);

	if (true == gEventMonitor.status) {
		/* Reset monitor status */
		gEventMonitor.status = false;

		CMDQ_LOG("stop monitor thread\n");

		/* Stop trigger loop */
		cmdq_task_stop_loop(gEventMonitor.cmdqHandle);
		/* Destroy slot & CMDQ handle */
		cmdq_free_mem(gEventMonitor.slotHandle);
		/* Dump CMDQ command */
		cmdq_task_destroy(gEventMonitor.cmdqHandle);
		/* Reset global variable */
		memset(&(gEventMonitor), 0x0, sizeof(gEventMonitor));
	}

	if (gEventMonitor.monitorNUM == 0) {
		/* Monitor trigger thread initialization */
		testcase_monitor_trigger_initialization();
	} else if (gEventMonitor.monitorNUM >= CMDQ_MONITOR_EVENT_MAX) {
		waitType = CMDQ_MOITOR_TYPE_FLUSH;
		CMDQ_LOG("[MONITOR] reach MAX monitor number: %d, force flush\n", gEventMonitor.monitorNUM);
	}

	switch (waitType) {
	case CMDQ_MOITOR_TYPE_FLUSH:
		if (gEventMonitor.monitorNUM > 0) {
			CMDQ_LOG("start monitor thread\n");

			/* Insert disable pre-fetch instruction */
			cmdqRecDisablePrefetch(gEventMonitor.cmdqHandle);
			/* Set monitor status */
			gEventMonitor.status = true;
			/* Start trigger loop */
			cmdq_task_start_loop_callback(gEventMonitor.cmdqHandle, &testcase_monitor_callback, 0);
			cmdq_task_dump_command(gEventMonitor.cmdqHandle);
		}
		break;
	case CMDQ_MOITOR_TYPE_WFE:
		eventID = (int32_t)monitorEvent;
		if (eventID >= 0 && eventID < CMDQ_SYNC_TOKEN_MAX) {
			cmdq_op_wait(gEventMonitor.cmdqHandle, eventID);
			cmdq_op_read_reg_to_mem(gEventMonitor.cmdqHandle, gEventMonitor.slotHandle,
				gEventMonitor.monitorNUM, CMDQ_APXGPT2_COUNT);
			successAddInstruction = true;
		}
		break;
	case CMDQ_MOITOR_TYPE_WAIT_NO_CLEAR:
		eventID = (int32_t)monitorEvent;
		if (eventID >= 0 && eventID < CMDQ_SYNC_TOKEN_MAX) {
			cmdq_op_wait_no_clear(gEventMonitor.cmdqHandle, eventID);
			cmdq_op_read_reg_to_mem(gEventMonitor.cmdqHandle, gEventMonitor.slotHandle,
				gEventMonitor.monitorNUM, CMDQ_APXGPT2_COUNT);
			successAddInstruction = true;
		}
		break;
	case CMDQ_MOITOR_TYPE_QUERYREGISTER:
		cmdq_op_read_reg_to_mem(gEventMonitor.cmdqHandle, gEventMonitor.slotHandle,
				gEventMonitor.monitorNUM, monitorEvent);
		successAddInstruction = true;
		break;
	}

	if (true == successAddInstruction) {
		gEventMonitor.waitType[gEventMonitor.monitorNUM] = waitType;
		gEventMonitor.monitorEvent[gEventMonitor.monitorNUM] = monitorEvent;
		gEventMonitor.monitorNUM++;
	}

	CMDQ_MSG("%s\n", __func__);
}

static void testcase_poll_monitor_delay_continue(struct work_struct *workItem)
{
	/* set event to start next polling */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_POLL_MONITOR);
	CMDQ_LOG("monitor after delay: (%d)ms, start polling again\n", gPollMonitor.delayTime);
}

static int32_t testcase_poll_monitor_callback(unsigned long data)
{
	uint32_t pollTime;

	if (false == gPollMonitor.status)
		return 0;

	cmdq_cpu_read_mem(gPollMonitor.slotHandle, 0, &pollTime);
	CMDQ_LOG("monitor, time: (%u ns), regAddr: 0x%08llx, regValue: 0x%08llx, regMask=0x%08llx\n",
		pollTime, gPollMonitor.pollReg, gPollMonitor.pollValue, gPollMonitor.pollMask);
	schedule_delayed_work(&gPollMonitor.delayContinueWork, gPollMonitor.delayTime);

	return 0;
}

static void testcase_poll_monitor_trigger(uint64_t pollReg, uint64_t pollValue, uint64_t pollMask)
{
	CMDQ_MSG("%s\n", __func__);

	if (true == gPollMonitor.status) {
		/* Reset monitor status */
		gPollMonitor.status = false;

		CMDQ_LOG("stop polling monitor thread: regAddr: 0x%08llx\n", gPollMonitor.pollReg);

		/* Stop trigger loop */
		cmdq_task_stop_loop(gPollMonitor.cmdqHandle);
		/* Destroy slot & CMDQ handle */
		cmdq_free_mem(gPollMonitor.slotHandle);
		cmdq_task_destroy(gPollMonitor.cmdqHandle);
		/* Reset global variable */
		memset(&(gPollMonitor), 0x0, sizeof(gPollMonitor));
	}

	if (-1 == pollReg)
		return;

	CMDQ_LOG("start polling monitor thread, regAddr=0x%08llx, regValue=0x%08llx, regMask=0x%08llx\n",
			pollReg, pollValue, pollMask);

	/* Set event to start first polling */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_POLL_MONITOR);
	/* Create slot */
	cmdq_alloc_mem(&gPollMonitor.slotHandle, 1);
	/* Create CMDQ handle */
	cmdq_task_create(CMDQ_SCENARIO_LOWP_TRIGGER_LOOP, &gPollMonitor.cmdqHandle);
	cmdq_task_reset(gPollMonitor.cmdqHandle);
	/* Insert monitor thread command */
	cmdq_op_wait(gPollMonitor.cmdqHandle, CMDQ_SYNC_TOKEN_POLL_MONITOR);
	if (cmdq_op_poll(gPollMonitor.cmdqHandle, pollReg, pollValue, pollMask) == 0) {
		cmdq_op_read_reg_to_mem(gPollMonitor.cmdqHandle, gPollMonitor.slotHandle, 0, CMDQ_APXGPT2_COUNT);
		/* Set value to global variable */
		gPollMonitor.pollReg = pollReg;
		gPollMonitor.pollValue = pollValue;
		gPollMonitor.pollMask = pollMask;
		gPollMonitor.delayTime = 1;
		gPollMonitor.status = true;
		INIT_DELAYED_WORK(&gPollMonitor.delayContinueWork, testcase_poll_monitor_delay_continue);
		/* Start trigger loop */
		cmdq_task_start_loop_callback(gPollMonitor.cmdqHandle, &testcase_poll_monitor_callback, 0);
		/* Dump CMDQ command */
		cmdq_task_dump_command(gPollMonitor.cmdqHandle);
	} else {
		/* Destroy slot & CMDQ handle */
		cmdq_free_mem(gPollMonitor.slotHandle);
		cmdq_task_destroy(gPollMonitor.cmdqHandle);
	}

	CMDQ_MSG("%s\n", __func__);
}

static void testcase_acquire_resource(enum CMDQ_EVENT_ENUM resourceEvent, bool acquireExpected)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	int32_t acquireResult;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	acquireResult = cmdq_resource_acquire_and_write(handle, resourceEvent,
		CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	if (acquireResult < 0) {
		/* Do error handle for acquire resource fail */
		if (acquireExpected) {
			/* print error message */
			CMDQ_ERR("Acquire resource fail: it's not expected!\n");
		} else {
			/* print message */
			CMDQ_LOG("Acquire resource fail: it's expected!\n");
		}
	} else {
		if (!acquireExpected) {
			/* print error message */
			CMDQ_ERR("Acquire resource success: it's not expected!\n");
		} else {
			/* print message */
			CMDQ_LOG("Acquire resource success: it's expected!\n");
		}
	}
	cmdq_task_flush(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN && acquireExpected) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static int32_t testcase_res_release_cb(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);

	CMDQ_MSG("%s\n", __func__);
	/* Flush release command immedately with wait MUTEX event */

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	/* simulate display need to wait single */
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	/* simulate release resource via write register */
	cmdq_resource_release_and_write(handle, resourceEvent,
		CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush_async(handle);
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
	return 0;
}

static int32_t testcase_res_available_cb(enum CMDQ_EVENT_ENUM resourceEvent)
{
	CMDQ_MSG("%s\n", __func__);
	testcase_acquire_resource(resourceEvent, true);
	CMDQ_MSG("%s END\n", __func__);
	return 0;
}

static void testcase_notify_and_delay_submit(uint32_t delayTimeMS)
{
	struct cmdqRecStruct *handle = NULL;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	const uint64_t engineFlag = (1LL << CMDQ_ENG_MDP_WROT0);
	const enum CMDQ_EVENT_ENUM resourceEvent = CMDQ_SYNC_RESOURCE_WROT0;
	uint32_t contDelay;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqCoreSetResourceCallback(resourceEvent,
		testcase_res_available_cb, testcase_res_release_cb);

	testcase_acquire_resource(resourceEvent, true);

	/* notify and delay time*/
	if (delayTimeMS > 0) {
		CMDQ_MSG("Before delay for acquire\n");
		msleep_interruptible(delayTimeMS);
		CMDQ_MSG("Before lock and delay\n");
		cmdqCoreLockResource(engineFlag, true);
		msleep_interruptible(delayTimeMS);
		CMDQ_MSG("After lock and delay\n");
	}

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	handle->engineFlag = engineFlag;
	cmdq_op_wait_no_clear(handle, resourceEvent);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);
	cmdq_task_flush_async(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	msleep_interruptible(2000);

	/* Delay and continue sent */
	for (contDelay = 300; contDelay < CMDQ_DELAY_RELEASE_RESOURCE_MS*1.2; contDelay += 300) {
		CMDQ_MSG("Before delay and flush\n");
		msleep_interruptible(contDelay);
		CMDQ_MSG("After delay\n");
		cmdq_task_flush(handle);
		CMDQ_MSG("After flush\n");
	}
	/* Simulate DISP acquire fail case, acquire immediate after flush MDP */
	cmdq_task_flush_async(handle);
	testcase_acquire_resource(resourceEvent, false);

	cmdq_task_flush_async(handle);
	cmdq_task_destroy(handle);

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_round(uint32_t loopCount, uint32_t cmdCount, bool withMask, bool withWait)
{
#define TEST_PREFETCH_LOOP 3

	int32_t i, j, k;
	int32_t ret;
	struct cmdqRecStruct *handle[TEST_PREFETCH_LOOP] = {0};
	struct TaskStruct *pTask[TEST_PREFETCH_LOOP] = { 0 };

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s: count:%d, withMask:%d, withWait:%d\n", __func__, cmdCount, withMask, withWait);
	for (i = 0; i < TEST_PREFETCH_LOOP; i++) {
		CMDQ_MSG("=============== flush:%d/%d ===============\n", i, TEST_PREFETCH_LOOP);

		for (k = 0; k < loopCount; k++) {
			CMDQ_MSG("=============== loop:%d/%d ===============\n", k, loopCount);
			cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &(handle[i]));
			cmdq_task_reset(handle[i]);
			cmdq_task_set_secure(handle[i], false);

			/* record instructions which needs prefetch */
			if (i == 1)
				cmdqRecEnablePrefetch(handle[i]); /* use pre-fetch with marker */

			if (withWait)
				cmdq_op_wait(handle[i], CMDQ_SYNC_TOKEN_USER_0);

			cmdq_op_profile_marker(handle[i], "ANA_BEGIN");
			for (j = 0; j < cmdCount; j++) {
				/* record instructions which does not need prefetch */
				if (withMask)
					cmdq_op_write_reg(handle[i], CMDQ_TEST_GCE_DUMMY_PA, 0x3210, ~0xfff0);
				else
					cmdq_op_write_reg(handle[i], CMDQ_TEST_GCE_DUMMY_PA, 0x3210, ~0);
			}

			if (i == 1)
				cmdqRecDisablePrefetch(handle[i]); /* disable pre-fetch with marker */

			cmdq_op_profile_marker(handle[i], "ANA_END");
			cmdq_op_finalize_command(handle[i], false);

			ret = _test_submit_async(handle[i], &pTask[i]);

			if (withWait) {
				msleep_interruptible(500);
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
			}

			CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
			ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
			cmdq_task_destroy(handle[i]);
		}
	}

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_from_DTS(void)
{
	int32_t i, j;
	uint32_t thread_prefetch_size;

	CMDQ_MSG("%s\n", __func__);

	for (i = 0; i < CMDQ_MAX_THREAD_COUNT; i++) {
		thread_prefetch_size = cmdq_core_get_thread_prefetch_size(i);

		for (j = 100; j <= (thread_prefetch_size + 60); j += 40) {
			testcase_prefetch_round(1, j, false, true);
			testcase_prefetch_round(1, j, false, false);
		}
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_specific_bus_MMSYS(void)
{
	uint32_t i;
	const uint32_t loop = 1000;
	const uint32_t pattern = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t mmsys_register;
	struct cmdqRecStruct *handle = NULL;
	cmdqBackupSlotHandle slot_handle;
	uint32_t start_time, end_time, duration_time;

	CMDQ_MSG("%s\n", __func__);

	cmdq_alloc_mem(&slot_handle, 2);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);

	cmdq_op_read_reg_to_mem(handle, slot_handle, 0, CMDQ_APXGPT2_COUNT);
	for (i = 0; i < loop; i++) {
		mmsys_register = CMDQ_TEST_MMSYS_DUMMY_PA + (i%2)*0x4;
		if (i%11 == 10)
			cmdq_op_read_to_data_register(handle, mmsys_register, CMDQ_DATA_REG_2D_SHARPNESS_0);
		else
			cmdq_op_write_reg(handle, mmsys_register, pattern, ~0);
	}

	cmdq_op_read_reg_to_mem(handle, slot_handle, 1, CMDQ_APXGPT2_COUNT);
	cmdq_task_flush(handle);

	cmdq_cpu_read_mem(slot_handle, 0, &start_time);
	cmdq_cpu_read_mem(slot_handle, 1, &end_time);
	duration_time = (end_time - start_time) * 76;
	CMDQ_LOG("duration time, %u, ns\n", duration_time);

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_MSG("%s END\n", __func__);
}

void cmdq_track_task(const struct TaskStruct *pTask)
{
	CMDQ_LOG("track_task: engine: 0x%08llx\n", pTask->engineFlag);
}

static void testcase_track_task_cb(void)
{
	struct cmdqRecStruct *handle = NULL;

	CMDQ_MSG("%s\n", __func__);
	cmdqCoreRegisterTrackTaskCB(CMDQ_GROUP_MDP, cmdq_track_task);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);
	cmdq_task_flush(handle);

	cmdqCoreRegisterTrackTaskCB(CMDQ_GROUP_MDP, NULL);
	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_while_test_mmsys_bus(void)
{
	int32_t i;
	const uint32_t loop = 5000;

	CMDQ_MSG("%s\n", __func__);

	for (i = 0; i < loop; i++) {
		testcase_specific_bus_MMSYS();
		msleep_interruptible(100);
	}

	CMDQ_MSG("%s END\n", __func__);
}

struct thread_set_event_config {
	enum CMDQ_EVENT_ENUM event;
	u32 sleep_ms;
	bool loop;
};

static int testcase_set_gce_event(void *data)
{
	struct thread_set_event_config config;

	CMDQ_MSG("%s\n", __func__);

	config = *((struct thread_set_event_config *) data);
	do {
		if (kthread_should_stop())
			break;

		if (config.sleep_ms > 10)
			msleep_interruptible(config.sleep_ms);
		else
			mdelay(config.sleep_ms);
		cmdqCoreSetEvent(config.event);
	} while (config.loop);

	CMDQ_MSG("%s END\n", __func__);

	return 0;
}

static int testcase_cpu_config_non_mmsys(void *data)
{
	CMDQ_MSG("%s\n", __func__);

	while (1) {
		if (kthread_should_stop())
			break;

		/* set to 0xFFFFFFFF */
		CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_JPEG), ~0);
		/* udelay(1); */
	}

	CMDQ_MSG("%s END\n", __func__);

	return 0;
}

static int testcase_cpu_config_mmsys(void *data)
{
	unsigned long mmsys_register;

	CMDQ_MSG("%s\n", __func__);

	cmdq_get_func()->enableCommonClockLocked(true);

	while (1) {
		if (kthread_should_stop())
			break;

		mmsys_register = CMDQ_TEST_MMSYS_DUMMY_VA + 0x4;
		/* set to 0xFFFFFFFF */
		CMDQ_REG_SET32(mmsys_register, ~0);
		/* udelay(1); */
	}

	CMDQ_MSG("%s END\n", __func__);

	return 0;
}

#define CMDQ_TEST_MAX_THREAD	(32)
struct task_struct *set_event_config_th;
struct task_struct *busy_mmsys_config_th[CMDQ_TEST_MAX_THREAD] = {NULL};
struct task_struct *busy_non_mmsys_config_th[CMDQ_TEST_MAX_THREAD] = {NULL};

static void testcase_run_set_gce_event(void *data)
{
	set_event_config_th = kthread_run(testcase_set_gce_event, data, "set_cmdq_event_loop");
	if (IS_ERR(set_event_config_th)) {
		/* print error log */
		CMDQ_LOG("%s, init kthread_run failed!\n", __func__);
		set_event_config_th = NULL;
	}
}

static void testcase_stop_set_gce_event(void)
{
	if (set_event_config_th == NULL)
		return;

	kthread_stop(set_event_config_th);
	set_event_config_th = NULL;
}

static void testcase_run_busy_non_mmsys_config_loop(void)
{
	uint32_t i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		busy_non_mmsys_config_th[i] = kthread_run(testcase_cpu_config_non_mmsys, NULL, "busy_config_non-mm");
		if (IS_ERR(busy_non_mmsys_config_th[i])) {
			/* print error log */
			CMDQ_LOG("%s, thread id: %d, init kthread_run failed!\n", __func__, i);
			busy_non_mmsys_config_th[i] = NULL;
		}
	}
}

static void testcase_stop_busy_non_mmsys_config_loop(void)
{
	uint32_t i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		if (busy_non_mmsys_config_th[i] == NULL)
			continue;

		kthread_stop(busy_non_mmsys_config_th[i]);
		busy_non_mmsys_config_th[i] = NULL;
	}
}

static void testcase_run_busy_mmsys_config_loop(void)
{
	uint32_t i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		busy_mmsys_config_th[i] = kthread_run(testcase_cpu_config_mmsys, NULL, "busy_config_mm");
		if (IS_ERR(busy_mmsys_config_th[i])) {
			/* print error log */
			CMDQ_LOG("%s, thread id: %d, init kthread_run failed!\n", __func__, i);
			busy_mmsys_config_th[i] = NULL;
		}
	}
}

static void testcase_stop_busy_mmsys_config_loop(void)
{
	uint32_t i;

	for (i = 0; i < CMDQ_TEST_MAX_THREAD; i++) {
		if (busy_mmsys_config_th[i] == NULL)
			continue;

		kthread_stop(busy_mmsys_config_th[i]);
		busy_mmsys_config_th[i] = NULL;
	}
}

static void testcase_basic_logic(void)
{
	struct cmdqRecStruct *handle;
	const unsigned long test_reg = CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR);
	const unsigned long result_reg = CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0);
	const uint32_t test_reg_pa = CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR);
	const uint32_t result_reg_pa = CMDQ_GPR_R32_PA(CMDQ_DATA_REG_2D_SHARPNESS_0);
	const uint32_t test_var_in_reg = 4, test_var = 2;
	uint32_t expected_result;
	uint32_t test_result;
	CMDQ_VARIABLE get_value, result;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(test_reg, test_var_in_reg);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	/* test logic assign */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_assign(handle, &result, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST assign FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic add */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_add(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg + test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST add FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic subtract */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_subtract(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg - test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST subtract FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic multiply */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_multiply(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg * test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST multiply FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic exclusive or */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_xor(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg ^ test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST xor FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic not */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_not(handle, &result, get_value);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = ~test_var_in_reg;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST not FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic or */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_or(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg | test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST or FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic and */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_and(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg & test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST and FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic left shift */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_left_shift(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg << test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST left shift FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	/* test logic right shift */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&result);

	cmdq_op_read_reg(handle, test_reg_pa, &get_value, ~0x0);
	cmdq_op_right_shift(handle, &result, get_value, test_var);
	cmdq_op_write_reg(handle, result_reg_pa, result, ~0x0);

	cmdq_task_flush_async(handle);

	/* value check */
	test_result = CMDQ_REG_GET32(result_reg);
	expected_result = test_var_in_reg >> test_var;
	if (test_result != expected_result) {
		/* test fail */
		CMDQ_ERR("TEST right shift FAIL: value is 0x%08x, not 0x%08x\n",
			test_result, expected_result);
	}

	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_do_while_continue(void)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *task = NULL;
	CMDQ_VARIABLE cmdq_result, cmdq_op_counter;
	cmdqBackupSlotHandle slot_handle;
	const u32 max_loop_count = 20;
	u32 test_result = 0, expect_result = 0, op_counter = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_alloc_mem(&slot_handle, 1);

	/* test logic assign */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&cmdq_result);
	cmdq_op_init_variable(&cmdq_op_counter);

	cmdq_op_assign(handle, &cmdq_result, 1);
	cmdq_op_assign(handle, &cmdq_op_counter, 0);
	cmdq_op_do_while(handle);
		cmdq_op_add(handle, &cmdq_op_counter, cmdq_op_counter, 1);
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
		cmdq_op_if(handle, cmdq_result, CMDQ_GREATER_THAN, 20);
			cmdq_op_continue(handle);
		cmdq_op_end_if(handle);
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 18);
	cmdq_op_end_do_while(handle, cmdq_op_counter, CMDQ_LESS_THAN, max_loop_count);
	cmdq_op_backup_CPR(handle, cmdq_result, slot_handle, 0);

	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &task);
	cmdq_core_dump_task_mem(task);
	cmdqCoreWaitAndReleaseTask(task, 500);

	cmdq_cpu_read_mem(slot_handle, 0, &test_result);

	expect_result = 1;
	op_counter = 0;
	do {
		op_counter += 1;
		expect_result += 1;
		if (expect_result > 20)
			continue;
		expect_result += 18;
	} while (op_counter < max_loop_count);

	if (test_result != expect_result) {
		CMDQ_TEST_FAIL("%s test value is 0x%08x, not expect 0x%08x\n",
			__func__, test_result, expect_result);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_basic_jump_c(void)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *task = NULL;
	const u32 max_rows = 10;
	const u32 max_cols = 12;
	u32 row_in_value = 0, col_in_value = 0;
	u32 test_result, expect_result, expect_temp_sum;
	CMDQ_VARIABLE cmdq_row, cmdq_col, cmdq_temp_sum, cmdq_result;
	cmdqBackupSlotHandle slot_handle;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_alloc_mem(&slot_handle, 1);

	/* test logic assign */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&cmdq_row);
	cmdq_op_init_variable(&cmdq_col);
	cmdq_op_init_variable(&cmdq_temp_sum);
	cmdq_op_init_variable(&cmdq_result);

	cmdq_op_assign(handle, &cmdq_row, row_in_value);
	cmdq_op_assign(handle, &cmdq_result, 1);
	cmdq_op_while(handle, cmdq_row, CMDQ_LESS_THAN, max_rows);
		cmdq_op_assign(handle, &cmdq_col, 0);
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
		cmdq_op_while(handle, cmdq_col, CMDQ_LESS_THAN, max_cols);
			cmdq_op_add(handle, &cmdq_col, cmdq_col, 1);
			cmdq_op_if(handle, cmdq_col, CMDQ_GREATER_THAN_AND_EQUAL, 10);
				cmdq_op_break(handle);
			cmdq_op_else_if(handle, 6, CMDQ_EQUAL, cmdq_col);
				cmdq_op_continue(handle);
			cmdq_op_else_if(handle, 5, CMDQ_EQUAL, cmdq_col);
				cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
			cmdq_op_else(handle);
				cmdq_op_add(handle, &cmdq_temp_sum, cmdq_row, cmdq_col);
				cmdq_op_add(handle, &cmdq_result, cmdq_result, cmdq_temp_sum);
			cmdq_op_end_if(handle);
		cmdq_op_end_while(handle);
		cmdq_op_add(handle, &cmdq_row, cmdq_row, 1);
	cmdq_op_end_while(handle);
	cmdq_op_backup_CPR(handle, cmdq_result, slot_handle, 0);

	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &task);
	cmdq_core_dump_task_mem(task);
	cmdqCoreWaitAndReleaseTask(task, 500);

	cmdq_cpu_read_mem(slot_handle, 0, &test_result);

	/* value check */
	expect_result = 1;
	while (row_in_value < max_rows) {
		col_in_value = 0;
		expect_result = expect_result + 1;

		while (col_in_value < max_cols) {
			col_in_value++;
			if (col_in_value >= 10) {
				break;
			} else if (col_in_value == 6) {
				continue;
			} else if (col_in_value == 5) {
				expect_result = expect_result + 1;
			} else {
				expect_temp_sum = row_in_value + col_in_value;
				expect_result = expect_result + expect_temp_sum;
			}
		}

		row_in_value++;
	}

	if (test_result != expect_result) {
		/* test fail */
		CMDQ_TEST_FAIL("%s value is 0x%08x, not 0x%08x\n",
			__func__, test_result, expect_result);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_basic_jump_c_do_while(void)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct *task = NULL;
	const u32 max_rows = 10;
	const u32 max_cols = 12;
	u32 row_in_value = 0, col_in_value = 0;
	u32 test_result, expect_result, expect_temp_sum;
	CMDQ_VARIABLE cmdq_row, cmdq_col, cmdq_temp_sum, cmdq_result;
	cmdqBackupSlotHandle slot_handle;

	CMDQ_MSG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_alloc_mem(&slot_handle, 1);

	/* test logic assign */
	cmdq_task_reset(handle);
	cmdq_op_init_variable(&cmdq_row);
	cmdq_op_init_variable(&cmdq_col);
	cmdq_op_init_variable(&cmdq_temp_sum);
	cmdq_op_init_variable(&cmdq_result);

	cmdq_op_assign(handle, &cmdq_row, row_in_value);
	cmdq_op_assign(handle, &cmdq_result, 1);
	cmdq_op_do_while(handle);
		cmdq_op_assign(handle, &cmdq_col, 0);
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
		cmdq_op_do_while(handle);
			cmdq_op_add(handle, &cmdq_col, cmdq_col, 1);
			cmdq_op_if(handle, cmdq_col, CMDQ_GREATER_THAN_AND_EQUAL, 10);
				cmdq_op_break(handle);
			cmdq_op_else_if(handle, 6, CMDQ_EQUAL, cmdq_col);
				cmdq_op_continue(handle);
			cmdq_op_else_if(handle, 5, CMDQ_EQUAL, cmdq_col);
				cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
			cmdq_op_else(handle);
				cmdq_op_add(handle, &cmdq_temp_sum, cmdq_row, cmdq_col);
				cmdq_op_add(handle, &cmdq_result, cmdq_result, cmdq_temp_sum);
			cmdq_op_end_if(handle);
		cmdq_op_end_do_while(handle, cmdq_col, CMDQ_LESS_THAN, max_cols);
		cmdq_op_add(handle, &cmdq_row, cmdq_row, 1);
	cmdq_op_end_do_while(handle, cmdq_row, CMDQ_LESS_THAN, max_rows);
	cmdq_op_backup_CPR(handle, cmdq_result, slot_handle, 0);

	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &task);
	cmdq_core_dump_task_mem(task);
	cmdqCoreWaitAndReleaseTask(task, 500);

	cmdq_cpu_read_mem(slot_handle, 0, &test_result);

	/* value check */
	expect_result = 1;
	do {
		col_in_value = 0;
		expect_result = expect_result + 1;

		do {
			col_in_value++;
			if (col_in_value >= 10) {
				break;
			} else if (col_in_value == 6) {
				continue;
			} else if (col_in_value == 5) {
				expect_result = expect_result + 1;
			} else {
				expect_temp_sum = row_in_value + col_in_value;
				expect_result = expect_result + expect_temp_sum;
			}
		} while (col_in_value < max_cols);

		row_in_value++;
	} while (row_in_value < max_rows);

	if (test_result != expect_result) {
		/* test fail */
		CMDQ_TEST_FAIL("%s value is 0x%08x, not 0x%08x\n",
			__func__, test_result, expect_result);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_long_jump_c(void)
{
	struct cmdqRecStruct *handle = NULL;
	const u32 init_val = 1, post_val = 6;
	u32 test_result, i;
	CMDQ_VARIABLE cmdq_result;
	cmdqBackupSlotHandle slot_handle;
	struct TaskStruct *task = NULL;
	s32 status = 0;

	CMDQ_MSG("%s\n", __func__);

	/* Test jump_c if cross page */

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_alloc_mem(&slot_handle, 1);

	cmdq_op_init_variable(&cmdq_result);
	cmdq_op_assign(handle, &cmdq_result, init_val);
	cmdq_op_if(handle, cmdq_result, CMDQ_NOT_EQUAL, init_val);
	/* this part of instruction should not execute */
	for (i = 0; i < CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE; i++)
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
	cmdq_op_end_if(handle);
	/* add more post instruction */
	for (i = 0; i < post_val; i++)
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
	cmdq_op_backup_CPR(handle, cmdq_result, slot_handle, 0);

	cmdq_op_finalize_command(handle, false);
	status = _test_submit_async(handle, &task);
	/* cmdqCoreDebugDumpCommand(task); */
	cmdqCoreWaitAndReleaseTask(task, 500);

	/* cmdq_task_dump_command(handle); */
	cmdq_cpu_read_mem(slot_handle, 0, &test_result);

	if (status < 0 || test_result != init_val + post_val) {
		/* test fail */
		CMDQ_TEST_FAIL("%s jump_c if, value is 0x%08x, not 0x%08x status: %d\n",
			__func__, test_result, init_val + 1, status);
	}

	cmdq_task_destroy(handle);

	/* Test jump_c while and continue cross page */

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	cmdq_op_init_variable(&cmdq_result);
	cmdq_op_assign(handle, &cmdq_result, init_val);
	cmdq_op_while(handle, cmdq_result, CMDQ_LESS_THAN_AND_EQUAL, init_val);
	cmdq_op_if(handle, cmdq_result, CMDQ_GREATER_THAN, init_val);
	/* this part of instruction should not execute */
	for (i = 0; i < CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE; i++)
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
	cmdq_op_end_if(handle);
	/* add more post instruction */
	for (i = 0; i < post_val; i++)
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
	cmdq_op_continue(handle);
	cmdq_op_end_while(handle);
	/* more instructions after end while */
	for (i = 0; i < post_val; i++)
		cmdq_op_add(handle, &cmdq_result, cmdq_result, 1);
	cmdq_op_backup_CPR(handle, cmdq_result, slot_handle, 0);

	cmdq_cpu_write_mem(slot_handle, 0, 0);
	cmdq_op_finalize_command(handle, false);
	status = _test_submit_async(handle, &task);
	/* cmdqCoreDebugDumpCommand(task); */
	cmdqCoreWaitAndReleaseTask(task, 500);

	cmdq_cpu_read_mem(slot_handle, 0, &test_result);

	if (status < 0 || test_result != init_val + post_val * 2) {
		/* test fail */
		CMDQ_TEST_FAIL("%s jump_c while and break, value is 0x%08x, not 0x%08x status: %d\n",
			__func__, test_result, init_val + 1, status);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_basic_delay(u32 delay_time_us)
{
#define CMDQ_DELAY_TOLERANCE_US (10)
	struct cmdqRecStruct *handle;
	struct TaskStruct *pTask;
	cmdqBackupSlotHandle slot_handle;
	const CMDQ_VARIABLE arg_tpr = (CMDQ_BIT_VAR<<CMDQ_DATA_BIT) | CMDQ_TPR_ID;
	uint32_t begin_tpr, end_tpr;
	int32_t duration_time;

	CMDQ_MSG("%s\n", __func__);

	cmdq_delay_dump_thread();

	cmdq_alloc_mem(&slot_handle, 2);
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	cmdq_task_reset(handle);

	cmdq_op_backup_CPR(handle, arg_tpr, slot_handle, 0);
	cmdq_op_delay_us(handle, delay_time_us);
	cmdq_op_backup_CPR(handle, arg_tpr, slot_handle, 1);

	cmdq_op_finalize_command(handle, false);
	_test_submit_async(handle, &pTask);
	cmdqCoreDebugDumpCommand(pTask);
	cmdqCoreWaitAndReleaseTask(pTask, 500);

	cmdq_cpu_read_mem(slot_handle, 0, &begin_tpr);
	cmdq_cpu_read_mem(slot_handle, 1, &end_tpr);
	duration_time = (end_tpr - begin_tpr) * 38;
	CMDQ_LOG("TEST TPR before: 0x%08x, after: 0x%08x\n", begin_tpr, end_tpr);
	CMDQ_LOG("TEST delay API duration time, %u, ns\n", duration_time);

	if ((duration_time/1000 > (delay_time_us + CMDQ_DELAY_TOLERANCE_US)) ||
		(duration_time/1000 < (delay_time_us - CMDQ_DELAY_TOLERANCE_US))) {
		CMDQ_TEST_FAIL("basic_delay test failed! over tolerance!\n");
		CMDQ_TEST_FAIL("expect:(%u), result:(%u), tolerance(%u)\n",
			delay_time_us, duration_time/1000, CMDQ_DELAY_TOLERANCE_US);
	} else {
		CMDQ_LOG("basic_delay test passed!\n");
		CMDQ_LOG("expect:(%u), result:(%u), tolerance(%u)\n",
			delay_time_us, duration_time/1000, CMDQ_DELAY_TOLERANCE_US);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_move_data_between_SRAM(void)
{
	void *p_va_src = NULL;
	dma_addr_t pa_src = 0;
	void *p_va_dest = NULL;
	dma_addr_t pa_dest = 0;
	u32 buffer_size = 32;
	size_t free_sram_size = 0;
	u32 cpr_offset = 0;
	s32 status = 0;

	CMDQ_MSG("%s\n", __func__);

	/* Allocate DRAM memory */
	p_va_src = cmdq_core_alloc_hw_buffer(cmdq_dev_get(), buffer_size, &pa_src, GFP_KERNEL);
	p_va_dest = cmdq_core_alloc_hw_buffer(cmdq_dev_get(), buffer_size, &pa_dest, GFP_KERNEL);
	memset(p_va_src, 0xda, buffer_size);
	memset(p_va_dest, 0xcc, buffer_size);

	CMDQ_LOG("copy data source:\n");
	print_hex_dump(KERN_ERR, "[CMDQ][LOG]", DUMP_PREFIX_ADDRESS, 16, 4,
			   p_va_src, buffer_size, true);

	/* Allocate SRAM memory */
	free_sram_size = cmdq_core_get_free_sram_size();
	status = cmdq_core_alloc_sram_buffer(buffer_size, "UT_SRAM", &cpr_offset);
	if (status < 0 || (free_sram_size - cmdq_core_get_free_sram_size()) != buffer_size) {
		CMDQ_ERR("%s allocate SRAM failed: before(%zu), after(%zu)\n",
			__func__, free_sram_size, cmdq_core_get_free_sram_size());
	}
	cmdq_core_dump_sram();

	status = cmdq_task_copy_to_sram(pa_src, cpr_offset, buffer_size);
	if (status < 0)
		CMDQ_TEST_FAIL("copy to sram API failed: %d", status);

	status = cmdq_task_copy_from_sram(pa_dest, cpr_offset, buffer_size);
	if (status < 0)
		CMDQ_TEST_FAIL("copy from sram API failed: %d", status);

	if (memcmp(p_va_src, p_va_dest, buffer_size) != 0) {
		CMDQ_ERR("move data between SRAM failed!\n");
		print_hex_dump(KERN_ERR, "[CMDQ][ERR]", DUMP_PREFIX_ADDRESS, 16, 4,
			       p_va_dest, buffer_size, true);
	}

	cmdq_core_free_hw_buffer(cmdq_dev_get(), buffer_size, p_va_src, pa_src);
	cmdq_core_free_hw_buffer(cmdq_dev_get(), buffer_size, p_va_dest, pa_dest);
	free_sram_size = cmdq_core_get_free_sram_size();
	cmdq_core_free_sram_buffer(cpr_offset, buffer_size);
	if ((cmdq_core_get_free_sram_size() - free_sram_size) != buffer_size) {
		CMDQ_ERR("%s free SRAM failed: before(%zu), after(%zu)\n",
			__func__, free_sram_size, cmdq_core_get_free_sram_size());
	}
	cmdq_core_dump_sram();

	CMDQ_MSG("%s END\n", __func__);
}

/**
 * Make sure driver can execute command on SRAM successfully
 * Coverage:
 *     Cannot start_in_sram with secure task
 *     Cannot flush twice SRAM task
 *     SRAM size should be normal after flush task and destroy task
 *     SRAM execution result should be correct
 */
static void testcase_run_command_on_SRAM(void)
{
	size_t free_sram_size = 0;
	s32 status = 0;
	u32 value = 0;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	struct cmdqRecStruct *handle;

	CMDQ_MSG("%s\n", __func__);

	/* set to 0xFFFFFFFF */
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* use CMDQ to set to PATTERN */
	cmdq_task_create(CMDQ_SCENARIO_SRAM_LOOP, &handle);
	cmdq_task_reset(handle);
	status = cmdq_task_set_secure(handle, true);
	if (status >= 0) {
		cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
		status = cmdq_task_start_loop_sram(handle, "UT_EXE_SRAM");
		if (status >= 0)
			CMDQ_TEST_FAIL("SRAM loop command cannot be secure!!!\n");
	}

	cmdq_task_reset(handle);
	cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_write_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, PATTERN, ~0);

	free_sram_size = cmdq_core_get_free_sram_size();
	cmdq_task_start_loop_sram(handle, "UT_EXE_SRAM");
	cmdq_task_dump_command(handle);
	cmdq_core_dump_sram();

	status = cmdq_task_start_loop_sram(handle, "UT_EXE_SRAM");
	if (status >= 0)
		CMDQ_TEST_FAIL("SRAM loop command cannot start twice!!!\n");

	cmdq_task_destroy(handle);
	if ((cmdq_core_get_free_sram_size() - free_sram_size) != 0) {
		CMDQ_TEST_FAIL("%s free SRAM failed: before(%zu), after(%zu)\n",
			__func__, free_sram_size, cmdq_core_get_free_sram_size());
	}

	/* value check */
	value = CMDQ_REG_GET32(CMDQ_TEST_GCE_DUMMY_VA);
	if (value != PATTERN) {
		/* test fail */
		CMDQ_TEST_FAIL("wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	} else {
		CMDQ_LOG("wrote value is 0x%08x\n", value);
	}

	CMDQ_MSG("%s END\n", __func__);
}

/**
 * Make sure driver support linux wait_event_timeout via GCE command
 * Coverage:
 *     Can execute before timeout (return remaining time)
 *     Can execute after timeout (return 0)
 *     Make sure multiple wait_event_timeout can also work
 */
static void testcase_wait_event_timeout(u32 max_round)
{
	struct cmdqRecStruct *handle;
	struct TaskStruct *pTask;
	cmdqBackupSlotHandle slot_handle;
	const CMDQ_VARIABLE arg_tpr = (CMDQ_BIT_VAR<<CMDQ_DATA_BIT) | CMDQ_TPR_ID;
	const CMDQ_VARIABLE loop_debug_cpr = (CMDQ_BIT_VAR<<CMDQ_DATA_BIT) | CMDQ_SPR_FOR_LOOP_DEBUG;
	CMDQ_VARIABLE wait_result_var = CMDQ_TASK_CPR_INITIAL_VALUE;
	u32 wait_result, begin_tpr, end_tpr, loop_count;
	u32 round;

	CMDQ_MSG("%s\n", __func__);

	cmdq_alloc_mem(&slot_handle, 4);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	/* round wait: 3ms, 5ms, 7ms, 9ms, 11ms*/
	for (round = 0; round < max_round; round++) {
		wait_result_var = CMDQ_TASK_CPR_INITIAL_VALUE;
		cmdq_task_reset(handle);
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

		cmdq_op_backup_CPR(handle, arg_tpr, slot_handle, 0);
		cmdq_op_wait_event_timeout(handle, &wait_result_var, CMDQ_SYNC_TOKEN_USER_0, 10000);
		cmdq_op_backup_CPR(handle, arg_tpr, slot_handle, 1);
		cmdq_op_backup_CPR(handle, wait_result_var, slot_handle, 2);
		cmdq_op_backup_CPR(handle, loop_debug_cpr, slot_handle, 3);
		cmdq_op_wait_event_timeout(handle, &wait_result_var, CMDQ_SYNC_TOKEN_USER_0, 3000);
		cmdq_op_wait_event_timeout(handle, &wait_result_var, CMDQ_SYNC_TOKEN_USER_0, 3000);

		cmdq_op_finalize_command(handle, false);
		_test_submit_async(handle, &pTask);

		/* sleep 5ms and trigger event */
		mdelay(3 + 2 * round);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		cmdqCoreDebugDumpCommand(pTask);

		cmdqCoreWaitAndReleaseTask(pTask, 500);

		cmdq_cpu_read_mem(slot_handle, 0, &begin_tpr);
		cmdq_cpu_read_mem(slot_handle, 1, &end_tpr);
		cmdq_cpu_read_mem(slot_handle, 2, &wait_result);
		cmdq_cpu_read_mem(slot_handle, 3, &loop_count);
		CMDQ_LOG("Round#%d wait result: 0x%08x = %u ns\n", round, wait_result, wait_result * 38);
		CMDQ_LOG("Round#%d wait loop count: %u\n", round, loop_count);
		CMDQ_LOG("Round#%d TPR before: 0x%08x, after: 0x%08x, %u ns\n",
			round, begin_tpr, end_tpr, (end_tpr - begin_tpr)*38);
	}

	cmdq_task_destroy(handle);
	cmdq_free_mem(slot_handle);

	CMDQ_MSG("%s END\n", __func__);
}

/* Simulate DISP use case */
static void testcase_disp_simulate(void)
{
	/*
	* int a = 0;
	* int b = 5;
	* int c;
	* while (a++ <= b) {
	*   c = wait_event_timeout(event, 10ms);
	*   if (c > 0)
	*     break;
	*   sleep(30ms);
	* }
	*/
	CMDQ_VARIABLE a_var = CMDQ_TASK_CPR_INITIAL_VALUE,
				  b_var = CMDQ_TASK_CPR_INITIAL_VALUE,
				  c_var = CMDQ_TASK_CPR_INITIAL_VALUE;
	struct cmdqRecStruct *handle;
	cmdqBackupSlotHandle slot_handle;
	u32 begin_time, end_time, duration_time_ms;
	struct thread_set_event_config config;

	config = (struct thread_set_event_config){.event = CMDQ_SYNC_TOKEN_USER_0,
			  .loop = false, .sleep_ms = 100};

	CMDQ_MSG("%s\n", __func__);

	cmdq_alloc_mem(&slot_handle, 2);
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	cmdq_op_read_reg_to_mem(handle, slot_handle, 0, CMDQ_APXGPT2_COUNT);
	cmdq_op_assign(handle, &a_var, 1);
	cmdq_op_assign(handle, &b_var, 5);
	cmdq_op_while(handle, a_var, CMDQ_LESS_THAN_AND_EQUAL, b_var);
		cmdq_op_add(handle, &a_var, a_var, 1);
		cmdq_op_wait_event_timeout(handle, &c_var, CMDQ_SYNC_TOKEN_USER_0, 10000);
		cmdq_op_if(handle, c_var, CMDQ_GREATER_THAN, 0);
			cmdq_op_break(handle);
		cmdq_op_end_if(handle);
		cmdq_op_delay_us(handle, 30000);
	cmdq_op_end_while(handle);
	cmdq_op_read_reg_to_mem(handle, slot_handle, 1, CMDQ_APXGPT2_COUNT);

	/* test round 1: expect event got early*/
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);
	testcase_run_set_gce_event((void *)(&config));

	cmdq_task_flush(handle);

	cmdq_cpu_read_mem(slot_handle, 0, &begin_time);
	cmdq_cpu_read_mem(slot_handle, 1, &end_time);
	duration_time_ms = (end_time - begin_time) * 76 / 1000000;

	CMDQ_LOG("%s expect wait: 50~150ms, real wait: %dms\n", __func__, duration_time_ms);
	if (duration_time_ms > 100)
		CMDQ_TEST_FAIL("%s wait valid event failed: %dms (100)\n", __func__, duration_time_ms);

	/* test round 2: expect event not got and timeout*/
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_flush(handle);

	cmdq_cpu_read_mem(slot_handle, 0, &begin_time);
	cmdq_cpu_read_mem(slot_handle, 1, &end_time);
	duration_time_ms = (end_time - begin_time) * 76 / 1000000;

	CMDQ_LOG("%s expect timeout: 200ms, real wait: %dms\n", __func__, duration_time_ms);
	if (duration_time_ms > 202 || duration_time_ms < 198)
		CMDQ_TEST_FAIL("%s wait valid event failed: %dms (200)\n", __func__, duration_time_ms);

	cmdq_task_destroy(handle);
	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_read_with_mask(void)
{
	struct cmdqRecStruct *handle;
	cmdqBackupSlotHandle slot_handle;
	CMDQ_VARIABLE arg_read = CMDQ_TASK_CPR_INITIAL_VALUE;
	u32 read_value = 0x00FADE00;
	u32 read_mask[2] = {0x00FF0000, 0x0000FF00};
	u32 backup_read_value = 0;
	u32 loop = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdq_alloc_mem(&slot_handle, 1);
	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, read_value);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);

	for (loop = 0; loop < ARRAY_SIZE(read_mask); loop++) {
		cmdq_task_reset(handle);
		cmdq_op_read_reg(handle, CMDQ_TEST_GCE_DUMMY_PA, &arg_read, read_mask[loop]);
		cmdq_op_backup_CPR(handle, arg_read, slot_handle, 0);
		cmdq_task_flush(handle);

		cmdq_cpu_read_mem(slot_handle, 0, &backup_read_value);

		if (backup_read_value != (read_value & read_mask[loop])) {
			CMDQ_TEST_FAIL("read value with mask error: (0x%08x) expect: (0x%08x)\n",
				backup_read_value, read_value & read_mask[loop]);
		}
	}

	cmdq_free_mem(slot_handle);
	cmdq_task_destroy(handle);
	CMDQ_MSG("%s END\n", __func__);
}

/*
 * Test Global CPR
 * 1. initialize and read should be correct
 * 2. no initialize and read should be correct
 */
static void testcase_global_variable(void)
{
	s32 status = 0;
	struct cmdqRecStruct *handle;
	cmdqBackupSlotHandle slot_handle;
	u32 cpr_offset;
	u32 gpr_buffer_size = 2*sizeof(u32);
	CMDQ_VARIABLE global_x, global_y;
	u32 test_x, test_y;

	CMDQ_MSG("%s\n", __func__);

	/* Allocate SRAM memory */
	status = cmdq_core_alloc_sram_buffer(gpr_buffer_size, "UT_ASSIGN_REF", &cpr_offset);
	if (status < 0)
		CMDQ_TEST_FAIL("timer loop SRAM buffer allocated failed!!!\n");

	cmdq_op_init_global_cpr_variable(&global_x, cpr_offset);
	cmdq_op_init_global_cpr_variable(&global_y, cpr_offset + 1);

	cmdq_alloc_mem(&slot_handle, 2);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	/* assign global variable */
	cmdq_op_assign(handle, &global_x, 0xaaaabbbb);
	cmdq_op_assign(handle, &global_y, 0xccccdddd);

	cmdq_op_backup_CPR(handle, global_x, slot_handle, 0);
	cmdq_op_backup_CPR(handle, global_y, slot_handle, 1);
	cmdq_task_dump_command(handle);

	cmdq_task_flush(handle);

	cmdq_cpu_read_mem(slot_handle, 0, &test_x);
	cmdq_cpu_read_mem(slot_handle, 1, &test_y);

	/* value check */
	if (test_x != 0xaaaabbbb || test_y != 0xccccdddd) {
		/* test fail */
		CMDQ_TEST_FAIL("1. read x: 0x%08x, read y: 0x%08x\n", test_x, test_y);
	} else {
		CMDQ_LOG("1. read x: 0x%08x, read_y: 0x%08x\n", test_x, test_y);
	}

	/* read again without initialize, should keep the value */
	cmdq_task_reset(handle);

	cmdq_op_backup_CPR(handle, global_x, slot_handle, 0);
	cmdq_op_backup_CPR(handle, global_y, slot_handle, 1);
	cmdq_task_dump_command(handle);

	cmdq_task_flush(handle);

	cmdq_cpu_read_mem(slot_handle, 0, &test_x);
	cmdq_cpu_read_mem(slot_handle, 1, &test_y);

	/* value check */
	if (test_x != 0xaaaabbbb || test_y != 0xccccdddd) {
		/* test fail */
		CMDQ_TEST_FAIL("2. read x: 0x%08x, read y: 0x%08x\n", test_x, test_y);
	} else {
		CMDQ_LOG("2. read x: 0x%08x, read_y: 0x%08x\n", test_x, test_y);
	}

	cmdq_core_free_sram_buffer(cpr_offset, gpr_buffer_size);
	cmdq_free_mem(slot_handle);
	cmdq_task_destroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

/*
 * Test Efficient Polling
 * 1. Polling basic function should work
 * 2. Polling should not block low priority thread
 */
static void testcase_efficient_polling(void)
{
	struct cmdqRecStruct *h_poll, *h_low;
	struct TaskStruct *pTask_poll, *pTask_low;
	u32 poll_value = 0x00FADE00, poll_mask = 0x00FF0000;
	s32 result = 0;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, ~0);
	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	/* create low priority thread to simulate block case*/
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &h_low);
	cmdq_task_reset(h_low);
	cmdq_op_wait(h_low, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(h_low, false);

	/* create polling thread with trigger loop priority */
	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &h_poll);
	cmdq_task_reset(h_poll);
	cmdq_op_poll(h_poll, CMDQ_TEST_GCE_DUMMY_PA, poll_value, poll_mask);
	cmdq_op_finalize_command(h_poll, false);

	_test_submit_async(h_low, &pTask_low);
	msleep_interruptible(500);
	_test_submit_async(h_poll, &pTask_poll);

	cmdqCoreDebugDumpCommand(pTask_poll);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	result = cmdqCoreWaitAndReleaseTask(pTask_low, 500);
	if (result < 0)
		CMDQ_TEST_FAIL("Low priority thread is blocked by polling. (%d)\n", result);

	CMDQ_REG_SET32(CMDQ_TEST_GCE_DUMMY_VA, poll_value);
	result = cmdqCoreWaitAndReleaseTask(pTask_poll, 500);

	if (result < 0)
		CMDQ_TEST_FAIL("Poll ability does not execute successfully. (%d)\n", result);

	cmdq_task_destroy(h_low);
	cmdq_task_destroy(h_poll);
	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_mmsys_performance(int32_t test_id)
{
	struct thread_set_event_config config = {.event = CMDQ_SYNC_TOKEN_USER_0,
											.loop = true, .sleep_ms = 150};

	switch (test_id) {
	case 0:
		/* test GCE config only in bus idle situation */
		testcase_run_set_gce_event((void *)(&config));
		msleep_interruptible(500);
		testcase_while_test_mmsys_bus();
		msleep_interruptible(500);
		testcase_stop_set_gce_event();
		break;
	case 1:
		/* test GCE config only when CPU busy configure MMSYS situation */
		testcase_run_set_gce_event((void *)(&config));
		msleep_interruptible(500);
		testcase_run_busy_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_while_test_mmsys_bus();
		msleep_interruptible(500);
		testcase_stop_busy_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_stop_set_gce_event();
		break;
	case 2:
		/* test GCE config only when CPU busy configure non-MMSYS situation */
		testcase_run_set_gce_event((void *)(&config));
		msleep_interruptible(500);
		testcase_run_busy_non_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_while_test_mmsys_bus();
		msleep_interruptible(500);
		testcase_stop_busy_non_mmsys_config_loop();
		msleep_interruptible(500);
		testcase_stop_set_gce_event();
		break;
	default:
		CMDQ_LOG("[TESTCASE] mmsys performance testcase Not Found: test_id: %d\n", test_id);
		break;
	}
}

void testcase_monitor_mem_start(void)
{
	CMDQ_MSG("%s\n", __func__);
	cmdq_core_set_mem_monitor(true);
	CMDQ_MSG("%s END\n", __func__);
}

void testcase_monitor_mem_stop(void)
{
	CMDQ_MSG("%s\n", __func__);
	cmdq_core_set_mem_monitor(false);
	cmdq_core_dump_mem_monitor();
	CMDQ_MSG("%s END\n", __func__);
}

void _testcase_boundary_mem_inst(uint32_t inst_num)
{
	int i;
	struct cmdqRecStruct *handle = NULL;
	uint32_t data;
	uint32_t pattern = 0x0;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);
	if (CMDQ_REG_GET32(MMSYS_DUMMY_REG) != 0xdeaddead)
		CMDQ_ERR("%s verify pattern register fail: 0x%08x\n",
			__func__, (uint32_t)CMDQ_REG_GET32(MMSYS_DUMMY_REG));

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	/* Build a buffer with N instructions. */
	CMDQ_MSG("%s record inst count: %u size: %u\n", __func__, inst_num, (uint32_t)(inst_num * CMDQ_INST_SIZE));
	for (i = 0; i < inst_num; ++i) {
		pattern = i;
		cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pattern, ~0);
	}

	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	/* verify data */
	do {
		if (true == gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n", __func__);
			break;
		}

		data = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
		if (pattern != data) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x, not pattern 0x%08x\n", data,
				 pattern);
		}
	} while (0);
}

void testcase_boundary_mem(void)
{
	uint32_t inst_num = 0;
	uint32_t base_inst_num = 0;
	uint32_t buffer_num = 0;

	CMDQ_MSG("%s\n", __func__);

	/* test cross page from 1 to 3 cases */
	for (buffer_num = 1; buffer_num < 4; buffer_num++) {
		base_inst_num = buffer_num * CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE;

		/*
		 * We check 0~4 cases.
		 * Case 0: 3 inst (OP+EOC+JUMP) in last buffer
		 * Case 1: 2 inst (EOC+JUMP) in last buffer
		 * Case 2: last buffer empty, EOC+JUMP at end of previous buffer
		 * Case 3: EOC+JUMP+Blank at end of last buffer
		 * Case 4: EOC+JUMP+2 Blank at end of last buffer
		 */
		for (inst_num = 0; inst_num < 5; inst_num++)
			_testcase_boundary_mem_inst(base_inst_num - inst_num);
	}

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_boundary_mem_param(void)
{
	uint32_t base_inst_num = 0;
	uint32_t buffer_num = (uint32_t)gCmdqTestConfig[2];
	uint32_t inst_num = (uint32_t)gCmdqTestConfig[3];

	CMDQ_MSG("%s\n", __func__);

	base_inst_num = buffer_num * CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE;
	_testcase_boundary_mem_inst(base_inst_num - inst_num);

	CMDQ_MSG("%s END\n", __func__);
}

void _testcase_longloop_inst(uint32_t inst_num)
{
	int i = 0;
	int status = 0;
	uint32_t data;
	uint32_t pattern = 0x0;
	const unsigned long DUMMY_REG_VA = CMDQ_TEST_GCE_DUMMY_VA;
	const unsigned long DUMMY_REG_PA = CMDQ_TEST_GCE_DUMMY_PA;

	CMDQ_REG_SET32(DUMMY_REG_VA, 0xdeaddead);
	if (CMDQ_REG_GET32(DUMMY_REG_VA) != 0xdeaddead)
		CMDQ_ERR("%s verify pattern register fail: 0x%08x\n",
			__func__, (uint32_t)CMDQ_REG_GET32(DUMMY_REG_VA));

	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoopReq);
	cmdqRecReset(hLoopReq);
	cmdqRecSetSecure(hLoopReq, false);
	cmdqRecWait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecWait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);

	g_loopIter = 0;

	setup_timer(&g_loopTimer, &_testcase_loop_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	/*
	 * Build a buffer with N instructions.
	 * The -2 for wait and clear instruction.
	 */
	CMDQ_MSG("%s record inst count: %u size: %u\n", __func__, inst_num, (uint32_t)(inst_num * CMDQ_INST_SIZE));
	for (i = 0; i < inst_num - 2; ++i) {
		pattern = i + 1;
		cmdqRecWrite(hLoopReq, DUMMY_REG_PA, pattern, ~0);
	}

	/* should success */
	status = cmdqRecStartLoop(hLoopReq);
	if (status != 0)
		CMDQ_MSG("TEST FAIL: Unable to start loop\n");

	/* WAIT */
	while (g_loopIter < 5)
		msleep_interruptible(500);

	CMDQ_MSG("%s ===== stop timer\n", __func__);
	cmdqRecDestroy(hLoopReq);
	del_timer(&g_loopTimer);

	/* verify data */
	do {
		if (true == gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n", __func__);
			break;
		}

		data = CMDQ_REG_GET32(DUMMY_REG_VA);
		if ((data >= 1 && data <= inst_num) == false) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x, not pattern 1 to 0x%08x\n",
				data, pattern);
		}
	} while (0);
}

void testcase_longloop(void)
{
	uint32_t last_inst = 0;
	uint32_t page_num = 0;

	CMDQ_MSG("%s\n", __func__);

	for (page_num = 1; page_num < 4; page_num++) {
		for (last_inst = 0; last_inst < 5; last_inst++)
			_testcase_longloop_inst(CMDQ_CMD_BUFFER_SIZE * page_num / CMDQ_INST_SIZE - last_inst);
	}

	CMDQ_MSG("%s\n", __func__);
}

int32_t _testcase_secure_handle(uint32_t secHandle, enum CMDQ_SCENARIO_ENUM scenario)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct cmdqRecStruct *hReqMDP;
	const uint32_t PATTERN_MDP = (1 << 0) | (1 << 2) | (1 << 16);
	int32_t status;

	cmdq_task_create(scenario, &hReqMDP);
	cmdq_task_reset(hReqMDP);
	cmdq_task_set_secure(hReqMDP, true);

	/* specify use MDP engine */
	hReqMDP->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0);

	/* enable secure test */
	cmdq_task_secure_enable_dapc(hReqMDP,
		(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0));
	cmdq_task_secure_enable_port_security(hReqMDP,
		(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WROT0));

	/* record command */
	cmdq_op_write_reg(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_MDP, ~0);
	cmdq_op_write_reg_secure(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_SAM_H_2_MVA,
		secHandle, 0xf000, 0x100, 0);
	cmdq_append_command(hReqMDP, CMDQ_CODE_EOC, 0, 1, 0, 0);
	cmdq_append_command(hReqMDP, CMDQ_CODE_JUMP, 0, 8, 0, 0);

	status = cmdq_task_flush(hReqMDP);
	cmdq_task_destroy(hReqMDP);

	CMDQ_MSG("%s end\n", __func__);

	return status;
#else
	return 0;
#endif
}

void testcase_invalid_handle(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status;

	CMDQ_MSG("%s\n", __func__);

	/* In this case we use an invalid secure handle to check error handling */
	status = _testcase_secure_handle(0xdeaddead, CMDQ_SCENARIO_SUB_DISP);
	if (status >= 0)
		CMDQ_ERR("TEST FAIL: should not success with invalid handle, status: %d\n", status);

	/* Handle 0 will make SW do not translate to PA. */
	status = _testcase_secure_handle(0x0, CMDQ_SCENARIO_DEBUG);
	if (status >= 0)
		CMDQ_ERR("TEST FAIL: should not success with handle 0, status: %d\n", status);

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
#endif
}

void testcase_get_task_by_engine(void)
{
	struct cmdqRecStruct *handle = NULL;
	struct TaskStruct task;
	const uint64_t engineFlag = (0x1 << CMDQ_ENG_MDP_RDMA0) | (0x1 << CMDQ_ENG_MDP_WROT0);
	int32_t status;
	const uint32_t debug_str_len = 1024;

	CMDQ_MSG("%s\n", __func__);

	memset(&task, 0, sizeof(struct TaskStruct));

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);

	/* clearn event first */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	/* assign engine flag */
	handle->engineFlag = engineFlag;
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);

	/* must fail before flush */
	status = cmdq_core_get_running_task_by_engine(engineFlag, debug_str_len, &task);
	if (status != -EFAULT) {
		CMDQ_ERR("TEST FAIL: get task by engine flag before flush: 0x%016llx, task flag: 0x%016llx\n",
			engineFlag, task.engineFlag);
	}

	task.userDebugStr = kzalloc(debug_str_len, GFP_KERNEL);

	cmdq_task_flush_async(handle);
	status = cmdq_core_get_running_task_by_engine(engineFlag, debug_str_len, &task);
	if (status != 0) {
		CMDQ_ERR("TEST FAIL: get task by engine flag: 0x%016llx, task flag: 0x%016llx\n",
			engineFlag, task.engineFlag);
	}

	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdq_task_destroy(handle);

	kfree(task.userDebugStr);
	task.userDebugStr = NULL;

	CMDQ_MSG("%s end\n", __func__);
}

void testcase_reorder(void)
{
	struct cmdqRecStruct *handleA, *handleB;
	uint32_t idx = 0;
	uint32_t data;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	struct TaskStruct *pTaskA1 = NULL, *pTaskA2 = NULL, *pTaskA3 = NULL;
	struct TaskStruct *pTaskB = NULL;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handleA);
	cmdq_task_reset(handleA);
	cmdq_task_set_secure(handleA, gCmdqTestSecure);
	cmdq_op_wait_no_clear(handleA, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handleA, false);
	handleA->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handleB);
	cmdq_task_reset(handleB);
	cmdq_task_set_secure(handleB, gCmdqTestSecure);
	cmdq_op_wait_no_clear(handleB, CMDQ_SYNC_TOKEN_USER_0);
	handleB->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	/*
	 * Make this task to boundary size.
	 * -3 because reserve for wait+eoc+jump
	 */
	for (idx = 0; idx < CMDQ_CMD_BUFFER_SIZE / sizeof(CMDQ_INST_SIZE) - 3; idx++)
		cmdqRecWrite(handleB, CMDQ_TEST_MMSYS_DUMMY_PA, idx, ~0);
	idx--;

	cmdq_op_finalize_command(handleB, false);

	/* large task insert to front case */
	handleA->priority = 0;
	_test_submit_async(handleA, &pTaskA1);
	handleA->priority = 1;
	_test_submit_async(handleA, &pTaskA2);
	handleA->priority = 4;
	_test_submit_async(handleA, &pTaskA3);
	handleB->priority = 7;
	_test_submit_async(handleB, &pTaskB);

	/* set token to run */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

	cmdqCoreWaitAndReleaseTask(pTaskA1, 500);
	cmdqCoreWaitAndReleaseTask(pTaskA2, 500);
	cmdqCoreWaitAndReleaseTask(pTaskA3, 500);
	cmdqCoreWaitAndReleaseTask(pTaskB, 500);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	data = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (idx != data)
		CMDQ_ERR(
			"TEST FAIL: reg value is 0x%08x, not pattern 0x%08x (large in front case)\n",
			data, idx);

	/* clear dummy again */
	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	/* large task switch to last case */
	handleA->priority = 0;
	_test_submit_async(handleA, &pTaskA1);
	handleB->priority = 1;
	_test_submit_async(handleB, &pTaskB);
	handleA->priority = 4;
	_test_submit_async(handleA, &pTaskA2);

	/* set token to run */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

	cmdqCoreWaitAndReleaseTask(pTaskA1, 500);
	cmdqCoreWaitAndReleaseTask(pTaskA2, 500);
	cmdqCoreWaitAndReleaseTask(pTaskB, 500);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	data = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (idx != data)
		CMDQ_ERR(
			"TEST FAIL: reg value is 0x%08x, not pattern 0x%08x (large at last case)\n",
			data, idx);

	cmdq_task_destroy(handleA);
	cmdq_task_destroy(handleB);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_reorder_last(void)
{
	struct cmdqRecStruct *handleA, *handleB, *handleC;
	uint32_t idx = 0;
	struct TaskStruct *tasks[30] = {0};
	uint32_t task_idx = 0;
	uint32_t wait_task_idx = 0;

	CMDQ_MSG("%s\n", __func__);

	/* clear token */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handleA);
	cmdq_task_reset(handleA);
	cmdq_task_set_secure(handleA, gCmdqTestSecure);
	cmdq_op_wait(handleA, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handleA, false);
	handleA->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handleB);
	cmdq_task_reset(handleB);
	cmdq_task_set_secure(handleB, gCmdqTestSecure);
	cmdq_op_wait_no_clear(handleB, CMDQ_SYNC_TOKEN_USER_1);
	cmdq_op_finalize_command(handleB, false);
	handleB->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handleC);
	cmdq_task_reset(handleC);
	cmdq_task_set_secure(handleC, gCmdqTestSecure);
	cmdq_op_wait(handleC, CMDQ_SYNC_TOKEN_USER_1);
	cmdq_op_finalize_command(handleC, false);
	handleC->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	_test_submit_async(handleA, &tasks[task_idx++]);
	_test_submit_async(handleA, &tasks[task_idx++]);

	/* full the thread except last one */
	for (idx = 2; idx < 14; idx++)
		_test_submit_async(handleB, &tasks[task_idx++]);

	/* do not let last task in thread array run */
	_test_submit_async(handleC, &tasks[task_idx++]);

	/* this is the last task in thread list, index 15 */
	_test_submit_async(handleB, &tasks[task_idx++]);

	/* let first 1 task go and insert more one */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
	msleep_interruptible(3);
	wait_task_idx = task_idx;
	_test_submit_async(handleA, &tasks[task_idx++]);
	msleep_interruptible(3);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);

	/* wait the target, this must timeout without KE */
	CMDQ_LOG("%s wait task: 0x%p, end inst: 0x%08x:%08x\n",
		__func__, tasks[wait_task_idx],
		tasks[wait_task_idx]->pCMDEnd[0], tasks[wait_task_idx]->pCMDEnd[-1]);
	if (cmdqCoreWaitAndReleaseTask(tasks[wait_task_idx], 2500) == 0)
		CMDQ_ERR("TEST FAIL: Last task should timeout but not success!\n");

	/* clear all */
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);
	msleep_interruptible(100);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);

	for (idx = 0; idx < task_idx; idx++) {
		if (idx == wait_task_idx)
			continue;
		CMDQ_LOG("%s keep wait other tasks: 0x%p, end inst: 0x%08x:%08x\n",
			__func__, tasks[idx],
			tasks[idx]->pCMDEnd[0], tasks[idx]->pCMDEnd[-1]);
		if (cmdqCoreWaitAndReleaseTask(tasks[idx], 500) < 0)
			CMDQ_ERR("TEST FAIL: Other task cannot pass, task: 0x%p\n", tasks[idx]);
	}

	cmdq_task_destroy(handleA);
	cmdq_task_destroy(handleB);
	cmdq_task_destroy(handleC);

	CMDQ_LOG("%s END\n", __func__);
}

void _testcase_set_event(struct TaskStruct *task, s32 thread)
{
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
}

void testcase_insert_when_jump(void)
{
	struct cmdqRecStruct *handle, *handle2;
	struct TaskStruct *pTask;
	uint32_t i;
	struct StressContextStruct *stress_context = cmdq_core_get_stress_context();

	stress_context->exec_suspend = _testcase_set_event;

	CMDQ_LOG("%s\n", __func__);

	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
	cmdq_task_reset(handle);
	cmdq_task_set_secure(handle, gCmdqTestSecure);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	/* create command without irq enable */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdq_task_reset(handle2);
	cmdq_task_set_secure(handle2, gCmdqTestSecure);
	cmdq_append_command(handle2, CMDQ_CODE_EOC, 0, 0, 0, 0);
	cmdq_append_command(handle2, CMDQ_CODE_JUMP, 0, 8, 0, 0);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

	for (i = 0; i < 50000; i++) {
		/* clear token */
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

		_test_submit_async(handle2, &pTask);
		cmdqCoreAutoReleaseTask(pTask);

		_test_submit_async(handle, &pTask);
		cmdqCoreAutoReleaseTask(pTask);
	}

	/* set token to run for safe */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	cmdq_core_clean_stress_context();

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_engineflag_conflict_dump(void)
{
	struct cmdqRecStruct *handle, *handle2;
	struct TaskStruct *task, *task2;

	CMDQ_LOG("%s\n", __func__);

	cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_USER_0);

	cmdq_task_create(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdq_op_wait_no_clear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle, false);

	cmdq_task_create(CMDQ_SCENARIO_SUB_DISP, &handle2);
	handle2->engineFlag |= CMDQ_ENG_DISP_AAL;
	cmdq_op_wait_no_clear(handle2, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_op_finalize_command(handle2, false);

	_test_submit_async(handle, &task);
	_test_submit_async(handle2, &task2);

	/* After wait we should get conflict dump in log without crash. */
	cmdqCoreAutoReleaseTask(task);
	cmdqCoreAutoReleaseTask(task2);

	cmdq_task_destroy(handle);
	cmdq_task_destroy(handle2);

	CMDQ_LOG("%s END\n", __func__);
}

static void testcase_end_addr_conflict(void)
{
	struct cmdqRecStruct *loop_handle, *submit_handle;
	u32 index;
	u32 test_thread = cmdq_get_func()->dispThread(CMDQ_SCENARIO_DEBUG_PREFETCH);
	u32 test_thread_end = CMDQ_THR_FIX_END_ADDR(test_thread);

	/* build trigger loop to write END addr value */
	cmdq_task_create(CMDQ_SCENARIO_TRIGGER_LOOP, &loop_handle);
	cmdq_task_reset(loop_handle);
	cmdq_task_set_secure(loop_handle, gCmdqTestSecure);
	for (index = 0; index < 48; index++)
		cmdq_op_write_reg(loop_handle, CMDQ_TEST_GCE_DUMMY_PA, test_thread_end, ~0);

	cmdq_task_start_loop(loop_handle);

	/* repeatly submit task to catch error */
	cmdq_task_create(CMDQ_SCENARIO_DEBUG_PREFETCH, &submit_handle);
	for (index = 0; index < 48; index++) {
		cmdq_task_reset(submit_handle);
		cmdq_task_set_secure(submit_handle, gCmdqTestSecure);
		cmdq_op_write_reg(submit_handle, CMDQ_TEST_GCE_DUMMY_PA, test_thread_end, ~0);
		cmdq_task_flush(submit_handle);
		CMDQ_LOG("Flush test round #%u\n", index);
		msleep_interruptible(500);
	}

	cmdq_task_stop_loop(loop_handle);
	cmdq_task_destroy(loop_handle);
	cmdq_task_destroy(submit_handle);
}

enum ENGINE_POLICY_ENUM {
	CMDQ_TESTCASE_ENGINE_NOT_SET,
	CMDQ_TESTCASE_ENGINE_SAME,
	CMDQ_TESTCASE_ENGINE_RANDOM,
};

enum WAIT_POLICY_ENUM {
	CMDQ_TESTCASE_WAITOP_NOT_SET,
	CMDQ_TESTCASE_WAITOP_ALWAYS,
	CMDQ_TESTCASE_WAITOP_RANDOM,
	CMDQ_TESTCASE_WAITOP_BEFORE_END,
};

enum BRANCH_POLICY_ENUM {
	CMDQ_TESTCASE_BRANCH_NONE = 0,
	CMDQ_TESTCASE_BRANCH_CONTINUE,
	CMDQ_TESTCASE_BRANCH_BREAK,
	CMDQ_TESTCASE_BRANCH_MAX,
};

enum CONDITION_TEST_POLICY_ENUM {
	CMDQ_TESTCASE_CONDITION_NONE,
	CMDQ_TESTCASE_CONDITION_RANDOM,
	CMDQ_TESTCASE_CONDITION_RANDOM_MORE,
};

enum ROUND_LOOP_TIME_POLICY_ENUM {
	CMDQ_TESTCASE_LOOP_FAST = 4,
	CMDQ_TESTCASE_LOOP_MEDIUM = 16,
	CMDQ_TESTCASE_LOOP_SLOW = 60,
};

enum POLL_POLICY_ENUM {
	CMDQ_TESTCASE_POLL_NONE,
	CMDQ_TESTCASE_POLL_PASS,
	CMDQ_TESTCASE_POLL_ALL,
};

enum TRIGGER_THREAD_POLICY_ENUM {
	CMDQ_TESTCASE_TRIGGER_RANDOM = 0,
	CMDQ_TESTCASE_TRIGGER_FASE = 4,
	CMDQ_TESTCASE_TRIGGER_MEDIUM = 16,
	CMDQ_TESTCASE_TRIGGER_SLOW = 80,
};

enum SECURE_POLICY_ENUM {
	CMDQ_TESTCASE_NO_SECURE,
	CMDQ_TESTCASE_SECURE_RANDOM,
};

struct stress_policy {
	enum ENGINE_POLICY_ENUM engines_policy;
	enum WAIT_POLICY_ENUM wait_policy;
	enum CONDITION_TEST_POLICY_ENUM condition_policy;
	enum ROUND_LOOP_TIME_POLICY_ENUM loop_policy;
	enum POLL_POLICY_ENUM poll_policy;
	enum TRIGGER_THREAD_POLICY_ENUM trigger_policy;
	enum SECURE_POLICY_ENUM secure_policy;
};

struct thread_param {
	struct completion cmplt;
	atomic_t stop;
	u32 run_count;
	bool multi_task;
	struct stress_policy policy;
};

struct random_data {
	struct work_struct release_work;
	struct thread_param *thread;
	struct cmdqRecStruct *handle;
	struct TaskStruct *task;
	atomic_t *ref_count;
	cmdqBackupSlotHandle slot;
	u32 round;
	u32 *slot_expect_values;
	u32 slot_reserve_count;
	u32 slot_count;
	u32 last_write;
	u32 mask;
	u32 inst_count;
	u32 wait_count;
	u64 dummy_reg_pa;
	u64 dummy_reg_va;
	u32 expect_result;
	CMDQ_VARIABLE var_result;
	bool may_wait;
	u32 condition_count;
	u32 poll_count;
};

/* trigger thread only set these bits */
#define CMDQ_TEST_POLL_BIT 0xFFFFFFF

/* secure task command buffer is limited */
#define CMDQ_MAX_SECURE_INST_COUNT (0x7F00 / CMDQ_INST_SIZE)

bool _is_boundary_offset(u32 offset)
{
	u32 offset_idx = offset / CMDQ_INST_SIZE;
	u32 buffer_inst_count = CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE;
	u32 offset_idx_mod = ((offset_idx + (offset_idx / (buffer_inst_count - 1))) % buffer_inst_count);

	return (offset_idx_mod == 0 || offset_idx_mod == 1);
}

static bool _append_op_read_mem_to_reg(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	u32 slot_idx = 0;

	slot_idx = (get_random_int() % data->slot_count) + data->slot_reserve_count;
	cmdq_op_read_mem_to_reg(handle, data->slot, slot_idx, data->dummy_reg_pa);
	data->last_write = data->slot_expect_values[slot_idx];
	return true;
}

static bool _append_op_read_reg_to_mem(struct cmdqRecStruct *handle,
	struct random_data *data, u32 limit_size)
{
	u32 slot_idx = 0;

	if (data->thread->multi_task)
		return true;

	slot_idx = (get_random_int() % data->slot_count) + data->slot_reserve_count;
	cmdq_op_read_reg_to_mem(handle, data->slot, slot_idx, data->dummy_reg_pa);
	data->slot_expect_values[slot_idx] = data->last_write;
	return true;
}

static bool _append_op_write_reg(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size)
{
	u32 random_value = get_random_int();
	bool use_mask = get_random_int() % 10;

	if (use_mask) {
		data->mask = get_random_int();
		random_value = (data->last_write & ~data->mask) | (random_value & data->mask);
	} else {
		data->mask = ~0;
	}
	cmdq_op_write_reg(handle, data->dummy_reg_pa, data->last_write, data->mask);
	data->last_write = random_value;
	return true;
}

static bool _append_op_wait(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size)
{
	const u32 max_wait_count = 2;
	const u32 max_wait_bound_count = 5;
	const unsigned long tokens[] = {
		CMDQ_SYNC_TOKEN_USER_0,
		CMDQ_SYNC_TOKEN_USER_1
	};
	unsigned long token;

	if (!data->may_wait || data->wait_count > max_wait_bound_count)
		return true;

	/* we save few chance to insert wait in boundary case */
	if (data->wait_count > max_wait_count &&
		handle->blockSize >= CMDQ_CMD_BUFFER_SIZE &&
		!_is_boundary_offset(handle->blockSize))
		return true;

	token = tokens[get_random_int() % 2];
	data->wait_count++;
	if (get_random_int() % 8)
		cmdq_op_wait_no_clear(handle, token);
	else
		cmdq_op_wait(handle, token);
	return true;
}

static bool _append_op_poll(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size)
{
	const u64 dummy_poll_pa = CMDQ_GPR_R32_PA(CMDQ_GET_GPR_PX2RX_LOW(
		CMDQ_DATA_REG_2D_SHARPNESS_0_DST));
	const u32 max_poll_count = 2;
	const u32 min_poll_instruction = 18;
	u32 poll_bit = 0;
	u32 size_before = handle->blockSize;

	if (data->thread->policy.poll_policy == CMDQ_TESTCASE_POLL_NONE ||
		limit_size < CMDQ_INST_SIZE * min_poll_instruction ||
		data->poll_count >= max_poll_count)
		return true;

	CMDQ_MSG("%s limit: %u block size: %u\n", __func__, limit_size, handle->blockSize);

	data->poll_count++;
	if (data->thread->policy.poll_policy == CMDQ_TESTCASE_POLL_PASS)
		poll_bit = 1 << (get_random_int() % 28);
	else
		poll_bit = 1 << (get_random_int() % 32);
	cmdq_op_write_reg(handle, dummy_poll_pa, 0, poll_bit);
	size_before = handle->blockSize;
	cmdq_op_poll(handle, dummy_poll_pa, poll_bit, poll_bit);

	CMDQ_LOG("%s round: %u poll: 0x%08x count: %u block size: %u op size: %u\n",
		__func__, data->round, poll_bit, data->poll_count, handle->blockSize,
		handle->blockSize - size_before);

	return true;
}

static bool _append_op_add(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size)
{
	cmdq_op_add(handle, &data->var_result, data->var_result, 1);
	data->expect_result++;
	return true;
}

static bool _append_op_while(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size, u32 *op_result)
{
	/*
	 * Pattern of generate op:
	 * while (variable < condition_limit)
	 *     variable = variable + 1  (1~N times)
	 *     break/continue/empty
	 *     variable = variable + 1  (0~N times)
	 * end while
	 */

	u32 expect_result = 0;
	u32 condition_limit, pre_op_count, post_op_count, i;
	u32 remain_op_count = limit_size / CMDQ_INST_SIZE;
	enum BRANCH_POLICY_ENUM branch_op;

	if (data->expect_result >= 0xFFFF)
		return true;

	/* condition value accept max 16 bit value */
	condition_limit = get_random_int() % (0xFFFF - data->expect_result);
	pre_op_count = get_random_int() % remain_op_count + 1;
	post_op_count = remain_op_count > pre_op_count ?
		get_random_int() % (remain_op_count - pre_op_count) : 0;
	branch_op = get_random_int() % (u32)CMDQ_TESTCASE_BRANCH_MAX;

	if (limit_size > 0xFFFF || pre_op_count > 0xFFFF || post_op_count > 0xFFFF) {
		CMDQ_ERR("%s start size: %u limit: %u pre/post: %u/%u op: %u current: %u condition: %u\n",
			__func__, handle->blockSize, limit_size,
			pre_op_count, post_op_count, branch_op,
			data->expect_result, condition_limit);
		return false;
	}

	CMDQ_MSG("%s start size: %u limit: %u pre/post: %u/%u op: %u current: %u condition: %u\n",
		__func__, handle->blockSize, limit_size,
		pre_op_count, post_op_count, branch_op,
		data->expect_result, condition_limit);

	cmdq_op_while(handle, data->var_result, CMDQ_LESS_THAN,
		condition_limit + data->expect_result);
	for (i = 0; i < pre_op_count; i++)
		cmdq_op_add(handle, &data->var_result, data->var_result, 1);

	if (data->expect_result < data->expect_result + condition_limit) {
		switch (branch_op) {
		case CMDQ_TESTCASE_BRANCH_CONTINUE:
			cmdq_op_continue(handle);
			while (expect_result < condition_limit)
				expect_result += pre_op_count;
			break;
		case CMDQ_TESTCASE_BRANCH_BREAK:
			cmdq_op_break(handle);
			expect_result = pre_op_count;
			break;
		case CMDQ_TESTCASE_BRANCH_NONE:
		default:
			while (expect_result < condition_limit)
				expect_result += pre_op_count + post_op_count;
			break;
		}
	} else {
		/* content of while loop would not run */
		expect_result = 0;
	}

	for (i = 0; i < post_op_count; i++)
		cmdq_op_add(handle, &data->var_result, data->var_result, 1);
	cmdq_op_end_while(handle);

	*op_result = expect_result;
	data->condition_count += 1;

	CMDQ_MSG("%s result: %u size: %u\n", __func__, expect_result, handle->blockSize);
	return true;
}

bool _test_is_condition_match(enum CMDQ_CONDITION_ENUM condition_op, u32 arg_a, u32 arb_b)
{
	bool match;

	switch (condition_op) {
	case CMDQ_EQUAL:
		match = (arg_a == arb_b);
		break;
	case CMDQ_NOT_EQUAL:
		match = (arg_a != arb_b);
		break;
	case CMDQ_GREATER_THAN_AND_EQUAL:
		match = (arg_a >= arb_b);
		break;
	case CMDQ_LESS_THAN_AND_EQUAL:
		match = (arg_a <= arb_b);
		break;
	case CMDQ_GREATER_THAN:
		match = (arg_a > arb_b);
		break;
	case CMDQ_LESS_THAN:
		match = (arg_a < arb_b);
		break;
	default:
		CMDQ_TEST_FAIL("%s cannot recognize IF condition: %u\n", __func__, (s32)condition_op);
		match = false;
		break;
	}

	return match;
}

static bool _append_op_if(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size, u32 *op_result)
{
	/*
	 * Pattern of generate op:
	 * if (variable op condition_limit)
	 *     variable = variable + 1  (1~N times)
	 * else if (variable op condition_limit)  (0~N times)
	 *     variable = variable + 1  (1~N times)
	 * else/empty
	 *     variable = variable + 1  (1~N times)
	 * end if
	 */

	const u32 min_op_count = 2;
	const u32 reserve_op_count = 2;
	u32 current_result = 0;
	u32 ramain_op_count = limit_size / CMDQ_INST_SIZE;
	u32 op_if_counter = 0;
	bool already_matched = false;
	bool terminate = false;
	enum IF_BRANCH_OP_ENUM {
		BRANCH_NONE,
		BRANCH_ELSEIF,
		BRANCH_ELSE,
		BRANCH_MAX
	};

	if (data->expect_result >= 0xFFFF)
		return true;

	CMDQ_MSG("%s start size: %u limit: %u\n", __func__, handle->blockSize, limit_size);

	while (ramain_op_count > min_op_count + reserve_op_count && !terminate &&
		data->expect_result + current_result < 0xFFFF) {
		u32 logic_op_count = 0;
		u32 condition_limit, i;
		enum IF_BRANCH_OP_ENUM branch_op;
		enum CMDQ_CONDITION_ENUM condition_op;

		/* reserve for if/add+1 */
		ramain_op_count -= reserve_op_count;

		/* condition value accept max 16 bit value */
		condition_limit = get_random_int() % (0xFFFF - current_result - data->expect_result);
		condition_op = get_random_int() % (u32)CMDQ_CONDITION_MAX;

		if (op_if_counter == 0) {
			cmdq_op_if(handle, data->var_result, condition_op, condition_limit);
			branch_op = BRANCH_MAX;
		} else {
			branch_op = get_random_int() % (u32)BRANCH_MAX;

			switch (branch_op) {
			case BRANCH_ELSEIF:
				cmdq_op_else_if(handle, data->var_result, condition_op, condition_limit);
				break;
			case BRANCH_ELSE:
				cmdq_op_else(handle);
				break;
			case BRANCH_NONE:
			default:
				terminate = true;
				break;
			}
		}

		if (terminate)
			break;

		logic_op_count = get_random_int() % ramain_op_count + 1;
		for (i = 0; i < logic_op_count; i++)
			cmdq_op_add(handle, &data->var_result, data->var_result, 1);
		ramain_op_count -= logic_op_count;

		if (!already_matched) {
			if (branch_op != BRANCH_ELSE) {
				already_matched = _test_is_condition_match(
					condition_op, data->expect_result, condition_limit);
			} else {
				already_matched = true;
			}

			if (already_matched)
				current_result += logic_op_count;
		}

		if (branch_op == BRANCH_ELSE)
			terminate = true;

		CMDQ_MSG(
			"%s if: %u remaind: %u logic: %u condition: %u branch: %u limit: %u matched: %u\n",
			__func__, op_if_counter, ramain_op_count, logic_op_count,
			(u32)condition_op, (s32)branch_op, condition_limit, already_matched);
		op_if_counter++;
	}

	if (op_if_counter > 0) {
		cmdq_op_end_if(handle);
		data->condition_count += 1;
	}

	*op_result = current_result;

	CMDQ_MSG("%s result: %u size: %u\n", __func__, current_result, handle->blockSize);

	return true;
}

static bool _append_op_condition(struct cmdqRecStruct *handle, struct random_data *data,
	u32 limit_size)
{
	typedef bool(*append_op_func)(struct cmdqRecStruct *handle, struct random_data *data,
		u32 limit_size, u32 *result);
	const append_op_func op_funcs[] = {
		_append_op_while,
		_append_op_if,
	};
	const u32 min_op_count = 4;
	u32 op_result = 0;
	u32 limit_op_count;

	CMDQ_MSG("%s start limit: %u\n", __func__, limit_size);

	if (data->thread->policy.condition_policy == CMDQ_TESTCASE_CONDITION_NONE)
		return true;

	if (data->thread->policy.condition_policy == CMDQ_TESTCASE_CONDITION_RANDOM &&
		data->condition_count >= 1)
		return true;

	/* make op count 4~N */
	limit_op_count = get_random_int() % (limit_size / CMDQ_INST_SIZE - min_op_count);
	limit_op_count += min_op_count;

	if (!op_funcs[get_random_int() % ARRAY_SIZE(op_funcs)](handle, data,
		limit_op_count * CMDQ_INST_SIZE, &op_result))
		return false;
	data->expect_result += op_result;

	CMDQ_MSG("%s op result: %u expect: %u\n", __func__, op_result, limit_size);

	return true;
}

static bool _append_random_instructions(struct cmdqRecStruct *handle,
	struct random_data *random_context, u32 limit_size) {
	typedef bool(*append_op_func)(struct cmdqRecStruct *handle, struct random_data *data,
		u32 limit_size);
	const append_op_func op_funcs[] = {
		_append_op_read_mem_to_reg,
		_append_op_read_reg_to_mem,
		_append_op_write_reg,
		_append_op_wait,
		_append_op_poll,

		/* v3 instructions */
		_append_op_add,
		_append_op_condition,
	};
	const u32 min_condition_op_count = 4;

	while (handle->blockSize < limit_size) {
		u32 total_func = ARRAY_SIZE(op_funcs);
		s32 op_idx;

		if (limit_size - handle->blockSize <= CMDQ_INST_SIZE * min_condition_op_count)
			total_func--;
		op_idx = get_random_int() % total_func;
		if (!op_funcs[op_idx](handle, random_context, limit_size - handle->blockSize))
			return false;
	}

	return true;
}

static void _dump_stress_task_result(s32 status, struct random_data *random_context,
	s32 *insts_buffer, u32 insts_buffer_size)
{
	u32 result_val = 0, i = 0;
	bool error_happen = false;

	do {
		if (status == -CMDQ_ERROR_TIMEOUT_IGNORE) {
			error_happen = true;

			/* exclude some case that timeout is expected */
			if (random_context->thread->policy.wait_policy != CMDQ_TESTCASE_WAITOP_NOT_SET) {
				/* insert wait op into testcase will trigger timeout */
				error_happen = false;
			} else if (random_context->thread->policy.engines_policy ==
				CMDQ_TESTCASE_ENGINE_RANDOM) {
				/* random engine flag may cause task never able to run, thus timedout */
				error_happen = false;
			} else if (random_context->thread->policy.poll_policy != CMDQ_TESTCASE_POLL_NONE) {
				/* timeout due to poll fail */
				error_happen = false;
			}

			if (!error_happen) {
				CMDQ_LOG("Task timeout: %p round: %u skip compare ...\n",
					random_context->task, random_context->round);
			}

			break;
		} else if (status < 0) {
			error_happen = true;
			break;
		}

		/* register may write by other task, thus only check in multi-task case */
		if (!random_context->thread->multi_task) {
			result_val = CMDQ_REG_GET32(random_context->dummy_reg_va);
			if (result_val != random_context->last_write) {
				CMDQ_TEST_FAIL("Reg value does not match: 0x%08x to 0x%08x round: %u\n",
					result_val, random_context->last_write, random_context->round);
				error_happen = true;
			}
		}

		for (i = random_context->slot_reserve_count;
			i < random_context->slot_reserve_count + random_context->slot_count; i++) {
			cmdq_cpu_read_mem(random_context->slot, i, &result_val);
			if (result_val != random_context->slot_expect_values[i]) {
				CMDQ_TEST_FAIL("Slot %u value does not match expect: 0x%08x reg 0x%08x round: %u\n",
					i, random_context->slot_expect_values[i], result_val, random_context->round);
				error_happen = true;
			}
		}

		cmdq_cpu_read_mem(random_context->slot, 1, &result_val);
		if (result_val != random_context->expect_result) {
			CMDQ_TEST_FAIL("Counter value not match expect: 0x%08x mem: 0x%08x round: %u\n",
				random_context->expect_result, result_val, random_context->round);
			error_happen = true;
		}
	} while (0);

	if (error_happen) {
		char longMsg[CMDQ_LONGSTRING_MAX];
		u32 msgOffset;
		s32 msgMAXSize;
		s32 poll_value = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_GET_GPR_PX2RX_LOW(
			CMDQ_DATA_REG_2D_SHARPNESS_0_DST)));

		atomic_set(&random_context->thread->stop, 1);

		cmdq_core_longstring_init(longMsg, &msgOffset, &msgMAXSize);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			"task: 0x%p round: %u size: %u wait: %d:%d multi: %d engine: %d ",
			random_context->task,
			random_context->round,
			insts_buffer_size ? insts_buffer_size : random_context->handle->blockSize,
			random_context->may_wait,
			random_context->wait_count,
			random_context->thread->multi_task,
			random_context->thread->policy.engines_policy);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			"condition: %d conditions: %u status: %d poll: %d, reg: 0x%08x count: %u\n",
			random_context->thread->policy.condition_policy,
			random_context->condition_count,
			status,
			random_context->thread->policy.poll_policy,
			poll_value,
			random_context->poll_count);
		CMDQ_TEST_FAIL("%s", longMsg);

		/* wait other threads stop print messages */
		msleep_interruptible(10);
		if (insts_buffer && insts_buffer_size)
			cmdqCoreDumpCommandMem(insts_buffer, insts_buffer_size);
		else
			cmdq_task_dump_command(random_context->handle);
	}
}

static void _testcase_stress_release_work(struct work_struct *work_item)
{
	struct random_data *random_context = (struct random_data *)container_of(
		work_item, struct random_data, release_work);
	s32 *insts_buffer = NULL;
	u32 insts_buffer_size = 0;
	s32 status = 0;

	do {
		if (!random_context->task) {
			CMDQ_TEST_FAIL("Task does not submit, round: %u\n", random_context->round);
			break;
		}

		_test_backup_instructions(random_context->task, &insts_buffer);
		insts_buffer_size = random_context->task->bufferSize;

		status = cmdqCoreWaitAndReleaseTask(random_context->task, 500);
		_dump_stress_task_result(status, random_context, insts_buffer, insts_buffer_size);
	} while (0);

	_test_free_backup_instructions(&insts_buffer);
	cmdq_task_destroy(random_context->handle);
	cmdq_free_mem(random_context->slot);
	atomic_dec(random_context->ref_count);
	kfree(random_context->slot_expect_values);
	kfree(random_context);
}

void _testcase_stress_submit_release_work(struct work_struct *work_item)
{
	struct random_data *random_context = (struct random_data *)container_of(
		work_item, struct random_data, release_work);
	s32 status = 0;

	do {
		status = _test_submit_sync(random_context->handle);
		_dump_stress_task_result(status, random_context, NULL, 0);
	} while (0);

	cmdq_task_destroy(random_context->handle);
	cmdq_free_mem(random_context->slot);
	atomic_dec(random_context->ref_count);
	kfree(random_context->slot_expect_values);
	kfree(random_context);
}

void _testcase_on_exec_suspend(struct TaskStruct *task, s32 thread)
{
	const unsigned long tokens[] = {
		CMDQ_SYNC_TOKEN_USER_0,
		CMDQ_SYNC_TOKEN_USER_1
	};

	cmdqCoreSetEvent(tokens[get_random_int() % ARRAY_SIZE(tokens)]);
}

static int _testcase_gen_task_thread(void *data)
{
#define MAX_RELEASE_QUEUE 4
	const u64 dummy_reg_va = CMDQ_TEST_GCE_DUMMY_VA;
	const u64 dummy_reg_pa = CMDQ_TEST_GCE_DUMMY_PA;
	const u32 inst_count_pattern[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
		126, 127, 128, 129, 130,
		254, 255, 256, 257, 258,
		510, 511, 512, 513, 514,
		1022, 1023, 1024, 1025, 1026};
	const u32 reserve_slot_count = 2;
	const u32 max_slot_count = 4;
	const u32 total_slot = reserve_slot_count + max_slot_count;
	const u32 max_muti_task = CMDQ_MAX_TASK_IN_THREAD + 1;
	const u32 max_buffer_count = 4;
	struct thread_param *thread_data = (struct thread_param *)data;
	struct workqueue_struct *release_queues[MAX_RELEASE_QUEUE] = {0};
	u32 engines[] = {CMDQ_ENG_MDP_CAMIN, CMDQ_ENG_MDP_RDMA0, CMDQ_ENG_MDP_RDMA1,
		CMDQ_ENG_MDP_WROT0, CMDQ_ENG_MDP_WROT1};
	u32 task_count = 0;
	atomic_t task_ref_count;
	u32 wait_count = 0;
	u32 engine_sel = 0;
	s32 status = 0;
	u32 i = 0;
	struct StressContextStruct *stress_context = cmdq_core_get_stress_context();

	CMDQ_MSG("%s\n", __func__);

	for (i = 0; i < MAX_RELEASE_QUEUE; i++)
		release_queues[i] = create_singlethread_workqueue("cmdq_random_release");

	CMDQ_REG_SET32(dummy_reg_va, 0xdeaddead);
	if (CMDQ_REG_GET32(dummy_reg_va) != 0xdeaddead)
		CMDQ_ERR("%s verify pattern register fail: 0x%08x\n",
			__func__, (u32)CMDQ_REG_GET32(dummy_reg_va));

	if (thread_data->policy.engines_policy == CMDQ_TESTCASE_ENGINE_SAME)
		engine_sel = get_random_int() % ((1 << ARRAY_SIZE(engines)) - 1);
	else
		engine_sel = 0;

	if (thread_data->policy.wait_policy == CMDQ_TESTCASE_WAITOP_BEFORE_END)
		stress_context->exec_suspend = _testcase_on_exec_suspend;
	else
		cmdq_core_clean_stress_context();

	atomic_set(&task_ref_count, 0);
	for (task_count = 0; task_count < thread_data->run_count &&
		!atomic_read(&thread_data->stop); task_count++) {
		struct random_data *random_context = NULL;
		struct cmdqRecStruct *handle = NULL;
		u32 i = 0;

		if (!thread_data->multi_task) {
			if (atomic_read(&task_ref_count) > 0) {
				msleep_interruptible(8);
				task_count--;
				continue;
			}
		} else {
			if (atomic_read(&task_ref_count) >= max_muti_task) {
				msleep_interruptible(8);
				task_count--;
				continue;
			}
		}

		random_context = kzalloc(sizeof(*random_context), GFP_KERNEL);
		random_context->thread = thread_data;
		random_context->round = task_count;
		random_context->dummy_reg_pa = dummy_reg_pa;
		random_context->dummy_reg_va = dummy_reg_va;
		random_context->ref_count = &task_ref_count;
		random_context->slot_reserve_count = reserve_slot_count;
		random_context->slot_count = max_slot_count;
		random_context->slot_expect_values = kzalloc(
			total_slot * sizeof(*random_context->slot_expect_values), GFP_KERNEL);
		cmdq_alloc_mem(&random_context->slot, total_slot);
		for (i = 0; i < reserve_slot_count; i++) {
			/*
			 * slot 0: Final dummy register value read back.
			 * slot 1: Logic add counter
			 */
			cmdq_cpu_write_mem(random_context->slot, i, 0);
			random_context->slot_expect_values[i] = 0;
		}
		for (i = reserve_slot_count; i < total_slot; i++) {
			cmdq_cpu_write_mem(random_context->slot, i, i);
			random_context->slot_expect_values[i] = i;
		}

		cmdq_task_create(CMDQ_SCENARIO_DEBUG, &handle);
		cmdq_task_reset(handle);
		random_context->handle = handle;

		/* variable for final result */
		cmdq_op_init_variable(&random_context->var_result);

		switch (thread_data->policy.wait_policy) {
		case CMDQ_TESTCASE_WAITOP_NOT_SET:
		case CMDQ_TESTCASE_WAITOP_BEFORE_END:
			random_context->may_wait = false;
			break;
		case CMDQ_TESTCASE_WAITOP_ALWAYS:
			random_context->may_wait = true;
			break;
		case CMDQ_TESTCASE_WAITOP_RANDOM:
			/* give 1/10 chance the task has wait instructions */
			random_context->may_wait = get_random_int() % 10;
			break;
		default:
			random_context->may_wait = get_random_int() % 10;
			break;
		}

		for (i = 0; i < (get_random_int() % max_buffer_count) + 1; i++)
			random_context->inst_count +=
				inst_count_pattern[get_random_int() % ARRAY_SIZE(inst_count_pattern)];

#ifdef CMDQ_SECURE_PATH_SUPPORT
		/* decide if this task can be secure */
		if (thread_data->policy.secure_policy == CMDQ_TESTCASE_SECURE_RANDOM &&
			random_context->inst_count < CMDQ_MAX_SECURE_INST_COUNT) {
			cmdq_task_set_secure(handle, (get_random_int() % 20) == 0);
			random_context->dummy_reg_pa = CMDQ_TEST_MMSYS_DUMMY_PA;
			random_context->dummy_reg_va = CMDQ_TEST_MMSYS_DUMMY_VA;
		}
#endif

		/* set engine flag and priority */
		handle->priority = get_random_int() % 16;
		if (thread_data->policy.engines_policy == CMDQ_TESTCASE_ENGINE_RANDOM) {
			engine_sel = get_random_int() % ((1 << ARRAY_SIZE(engines)) - 1);
			for (i = 0; i < ARRAY_SIZE(engines); i++) {
				if (((1 << i) & engine_sel) != 0)
					handle->engineFlag = handle->engineFlag | (1 << engines[i]);
			}
		} else {
			handle->engineFlag = engine_sel;
		}

		if (!thread_data->multi_task) {
			/* clear the reg first */
			cmdq_op_write_reg(handle, dummy_reg_pa, 0, ~0);
		}

		cmdq_op_assign(handle, &random_context->var_result, 0);

		/* append random instructions */
		if (random_context->inst_count > 4) {
			if (!_append_random_instructions(handle, random_context,
				(random_context->inst_count - 4) * CMDQ_INST_SIZE)) {
				CMDQ_ERR("%s error during append random instructions\n", __func__);
				atomic_set(&thread_data->stop, 1);
				cmdq_free_mem(random_context->slot);
				kfree(random_context->slot_expect_values);
				kfree(random_context);
				break;
			}
		}

		if (!thread_data->multi_task) {
			cmdqRecBackupRegisterToSlot(handle, random_context->slot, 0, dummy_reg_pa);
			random_context->slot_expect_values[0] = random_context->last_write;
		}

		/* Note: backup to slot 1 which is reserved for register final value */
		cmdq_op_backup_CPR(handle, random_context->var_result, random_context->slot, 1);

		if (thread_data->policy.wait_policy == CMDQ_TESTCASE_WAITOP_BEFORE_END) {
			/* make sure task wait before eoc */
			cmdq_op_clear_event(handle, CMDQ_SYNC_TOKEN_USER_0);
			cmdq_op_wait(handle, CMDQ_SYNC_TOKEN_USER_0);
			/* do not queue too much tasks in thread */
			if ((u32)atomic_read(&task_ref_count) >= 2)
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		}

		if (get_random_int() % 2) {
			status = cmdq_op_finalize_command(handle, false);
			if (status < 0) {
				CMDQ_ERR("Fail to finalize round: %u status: %d\n",
					task_count, status);
				cmdq_free_mem(random_context->slot);
				kfree(random_context->slot_expect_values);
				kfree(random_context);
				break;
			}

			/* async submit case, with contains more info during release */
			status = _test_submit_async_internal(handle, &random_context->task, true);
			if (status < 0) {
				CMDQ_ERR("Fail to submit round: %u status: %d\n",
					task_count, status);
				cmdq_free_mem(random_context->slot);
				kfree(random_context->slot_expect_values);
				kfree(random_context);
				break;
			}

			INIT_WORK(&random_context->release_work, _testcase_stress_release_work);
			atomic_inc(&task_ref_count);

			CMDQ_LOG("Round: %u task: %p thread: %d size: %u ref: %u start async\n",
				task_count, random_context->task, random_context->task->thread,
				random_context->task->bufferSize,
				(u32)atomic_read(&task_ref_count));
		} else {
			/* sync flush case, will blocking worker */
			INIT_WORK(&random_context->release_work, _testcase_stress_submit_release_work);
			atomic_inc(&task_ref_count);

			CMDQ_LOG("Round: %u handle size: %u ref: %u start sync\n",
				task_count, handle->blockSize,
				(u32)atomic_read(&task_ref_count));
		}

		queue_work(release_queues[get_random_int() % MAX_RELEASE_QUEUE],
			&random_context->release_work);

		msleep_interruptible(get_random_int() % (u32)thread_data->policy.loop_policy + 1);
	}

	while (atomic_read(&task_ref_count) > 0) {
		/* set events to speed up task finish */
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_1);

		msleep_interruptible(500);
		CMDQ_ERR("%s wait for all task done: %u\n",
			__func__, wait_count++);
	}

	CMDQ_LOG("%s END\n", __func__);

	cmdq_core_clean_stress_context();
	for (i = 0; i < MAX_RELEASE_QUEUE; i++)
		destroy_workqueue(release_queues[i]);
	complete(&thread_data->cmplt);

	return 0;
}

static int _testcase_trigger_event_thread(void *data)
{
	struct thread_param *thread_data = (struct thread_param *)data;
	u8 event_idx = 0;
	const unsigned long tokens[] = {
		CMDQ_SYNC_TOKEN_USER_0,
		CMDQ_SYNC_TOKEN_USER_1
	};
	const u64 dummy_poll_va = CMDQ_GPR_R32(CMDQ_GET_GPR_PX2RX_LOW(
		CMDQ_DATA_REG_2D_SHARPNESS_0_DST));
	u32 poll_bit_counter = 0xf;
	u32 dummy_value = 0;
	u32 trigger_interval = (u32)thread_data->policy.trigger_policy;

	CMDQ_LOG("%s\n", __func__);

	if (!trigger_interval)
		trigger_interval = get_random_int() % (u32)CMDQ_TESTCASE_TRIGGER_SLOW;

	/* randomly clear/set event */
	while (!atomic_read(&thread_data->stop)) {
		event_idx = get_random_int() % ARRAY_SIZE(tokens);
		if (get_random_int() % 2)
			cmdqCoreSetEvent(tokens[event_idx]);
		else
			cmdqCoreClearEvent(tokens[event_idx]);

		dummy_value = CMDQ_REG_GET32(dummy_poll_va);
		CMDQ_REG_SET32(dummy_poll_va, dummy_value | poll_bit_counter);
		poll_bit_counter = poll_bit_counter << 4;
		if (poll_bit_counter > CMDQ_TEST_POLL_BIT)
			poll_bit_counter = 0xf;

		msleep_interruptible(get_random_int() % trigger_interval + 1);
	}

	/* clear token */
	for (event_idx = 0; event_idx < ARRAY_SIZE(tokens); event_idx++)
		cmdqCoreClearEvent(tokens[event_idx]);

	CMDQ_LOG("%s END\n", __func__);

	complete(&thread_data->cmplt);

	return 0;
}

static void testcase_gen_random_case(bool multi_task, struct stress_policy policy)
{
	struct task_struct *random_thread_handle;
	struct task_struct *trigger_thread_handle;
	struct thread_param random_thread = { {0} };
	struct thread_param trigger_thread = { {0} };

	CMDQ_LOG("%s start with multi-task: %s engine: %d wait: %d condition: %d loop: %d\n",
		__func__, multi_task ? "True" : "False", policy.engines_policy,
		policy.wait_policy, policy.condition_policy, policy.loop_policy);

	random_thread.multi_task = multi_task;
	random_thread.policy = policy;

	do {
		init_completion(&random_thread.cmplt);
		atomic_set(&random_thread.stop, 0);

		init_completion(&trigger_thread.cmplt);
		atomic_set(&trigger_thread.stop, 0);

		random_thread.run_count = 3000;

		random_thread_handle = kthread_run(_testcase_gen_task_thread,
			(void *)&random_thread, "cmdq_gen");
		if (IS_ERR(random_thread_handle)) {
			CMDQ_TEST_FAIL("Fail to start gen task thread\n");
			break;
		}

		trigger_thread_handle = kthread_run(_testcase_trigger_event_thread,
			(void *)&trigger_thread, "mdq_trigger");
		if (IS_ERR(trigger_thread_handle)) {
			CMDQ_TEST_FAIL("Fail to start trigger event thread\n");
			atomic_set(&random_thread.stop, 1);
			wait_for_completion(&random_thread.cmplt);
			kthread_stop(random_thread_handle);
			break;
		}

		wait_for_completion(&random_thread.cmplt);
		atomic_set(&trigger_thread.stop, 1);
		wait_for_completion(&trigger_thread.cmplt);
	} while (0);

	CMDQ_LOG("%s END\n", __func__);
}

void testcase_stress_basic(void)
{
	struct stress_policy policy = {0};

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (gCmdqTestSecure)
		policy.secure_policy = CMDQ_TESTCASE_SECURE_RANDOM;
#endif

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_BEFORE_END;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_NONE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_FAST;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_RANDOM;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_condition(void)
{
	struct stress_policy policy = {0};

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_RANDOM;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_RANDOM;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_FAST;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_RANDOM_MORE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_poll(void)
{
	struct stress_policy policy = {0};

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_NOT_SET;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_NOT_SET;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_RANDOM;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	policy.poll_policy = CMDQ_TESTCASE_POLL_PASS;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.poll_policy = CMDQ_TESTCASE_POLL_ALL;
	testcase_gen_random_case(true, policy);
}

void testcase_stress_timeout(void)
{
	struct stress_policy policy = {0};

	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	policy.wait_policy = CMDQ_TESTCASE_WAITOP_RANDOM;
	policy.condition_policy = CMDQ_TESTCASE_CONDITION_NONE;
	policy.loop_policy = CMDQ_TESTCASE_LOOP_MEDIUM;
	testcase_gen_random_case(true, policy);

	msleep_interruptible(10);
	policy.engines_policy = CMDQ_TESTCASE_ENGINE_SAME;
	testcase_gen_random_case(true, policy);
}

enum CMDQ_TESTCASE_ENUM {
	CMDQ_TESTCASE_DEFAULT = 0,
	CMDQ_TESTCASE_BASIC = 1,
	CMDQ_TESTCASE_ERROR = 2,
	CMDQ_TESTCASE_FPGA = 3,
	CMDQ_TESTCASE_READ_REG_REQUEST,	/* user request get some registers' value when task execution */
	CMDQ_TESTCASE_GPR,
	CMDQ_TESTCASE_SW_TIMEOUT_HANDLE,

	CMDQ_TESTCASE_END,	/* always at the end */
};

static void testcase_general_handling(int32_t testID)
{
	/* Turn on GCE clock to make sure GPR is always alive */
	cmdq_dev_enable_gce_clock(true);
	switch (testID) {
	case 303:
		testcase_stress_timeout();
		break;
	case 302:
		testcase_stress_poll();
		break;
	case 301:
		testcase_stress_condition();
		break;
	case 300:
		testcase_stress_basic();
		break;
	case 152:
		testcase_end_addr_conflict();
		break;
	case 151:
		testcase_engineflag_conflict_dump();
		break;
	case 150:
		testcase_insert_when_jump();
		break;
	case 149:
		testcase_global_variable();
		break;
	case 148:
		testcase_read_with_mask();
		break;
	case 147:
		testcase_efficient_polling();
		break;
	case 146:
		testcase_disp_simulate();
		break;
	case 145:
		testcase_wait_event_timeout(20);
		break;
	case 144:
		testcase_wait_event_timeout(1);
		break;
	case 143:
		testcase_run_command_on_SRAM();
		break;
	case 142:
		testcase_move_data_between_SRAM();
		break;
	case 141:
		testcase_track_task_cb();
		break;
	case 140:
		testcase_reorder();
		testcase_reorder_last();
		break;
	case 139:
		testcase_invalid_handle();
		break;
	case 131:
		testcase_get_task_by_engine();
		break;
	case 130:
		testcase_longloop();
		break;
	case 129:
		testcase_boundary_mem();
		break;
	case 128:
		testcase_boundary_mem_param();
		break;
	case 126:
		testcase_basic_delay(100);
		testcase_basic_delay(1000);
		testcase_basic_delay(10000);
		testcase_basic_delay(100000);
		break;
	case 125:
		testcase_do_while_continue();
		testcase_basic_jump_c();
		testcase_basic_jump_c_do_while();
		testcase_long_jump_c();
		break;
	case 124:
		testcase_basic_logic();
		break;
	case 123:
		testcase_monitor_mem_stop();
		break;
	case 122:
		testcase_monitor_mem_start();
		break;
	case 121:
		testcase_prefetch_from_DTS();
		break;
	case 120:
		testcase_notify_and_delay_submit(16);
		break;
	case 119:
		testcase_check_dts_correctness();
		break;
	case 118:
		testcase_error_irq();
		break;
	case 117:
		testcase_timeout_reorder_test();
		break;
	case 116:
		testcase_timeout_wait_early_test();
		break;
	case 115:
		testcase_manual_suspend_resume_test();
		break;
	case 114:
		testcase_append_task_verify();
		break;
	case 113:
		testcase_trigger_engine_dispatch_check();
		break;
	case 112:
		testcase_complicated_engine_thread();
		break;
	case 111:
		testcase_module_full_mdp_engine();
		break;
	case 110:
		testcase_nonsuspend_irq();
		break;
	case 109:
		testcase_estimate_command_exec_time();
		break;
	case 108:
		testcase_profile_marker();
		break;
	case 107:
		testcase_prefetch_multiple_command();
		break;
	case 106:
		testcase_concurrency_for_normal_path_and_secure_path();
		break;
	case 105:
		testcase_async_write_stress_test();
		break;
	case 104:
		testcase_submit_after_error_happened();
		break;
	case 103:
		testcase_secure_meta_data();
		break;
	case 102:
		testcase_secure_disp_scenario();
		break;
	case 101:
		testcase_write_stress_test();
		break;
	case 100:
		testcase_secure_basic();
		break;
	case 99:
		testcase_write();
		testcase_write_with_mask();
		break;
	case 98:
		testcase_errors();
		break;
	case 97:
		testcase_scenario();
		break;
	case 96:
		testcase_sync_token();
		break;
	case 95:
		testcase_write_address();
		break;
	case 94:
		testcase_async_request();
		break;
	case 93:
		testcase_async_suspend_resume();
		break;
	case 92:
		testcase_async_request_partial_engine();
		break;
	case 91:
		testcase_prefetch_scenarios();
		break;
	case 90:
		testcase_loop();
		break;
	case 89:
		testcase_trigger_thread();
		break;
	case 88:
		testcase_multiple_async_request();
		break;
	case 87:
		testcase_get_result();
		break;
	case 86:
		testcase_read_to_data_reg();
		break;
	case 85:
		testcase_dram_access();
		break;
	case 84:
		testcase_backup_register();
		break;
	case 83:
		testcase_fire_and_forget();
		break;
	case 82:
		testcase_sync_token_threaded();
		break;
	case 81:
		testcase_long_command();
		break;
	case 80:
		testcase_clkmgr();
		break;
	case 79:
		testcase_perisys_apb();
		break;
	case 78:
		testcase_backup_reg_to_slot();
		break;
	case 77:
		testcase_thread_dispatch();
		break;
	case 75:
		testcase_full_thread_array();
		break;
	case 74:
		testcase_module_full_dump();
		break;
	case 73:
		testcase_write_from_data_reg();
		break;
	case 72:
		testcase_update_value_to_slot();
		break;
	case 71:
		testcase_poll();
		break;
	case 70:
		testcase_write_reg_from_slot();
		break;
	case CMDQ_TESTCASE_FPGA:
		testcase_write();
		testcase_write_with_mask();
		testcase_poll();
		testcase_scenario();
		testcase_estimate_command_exec_time();
		testcase_prefetch_multiple_command();
		testcase_write_stress_test();
		testcase_async_suspend_resume();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_trigger_thread();
		testcase_multiple_async_request();
		testcase_get_result();
		testcase_dram_access();
		testcase_backup_register();
		testcase_fire_and_forget();
		testcase_long_command();
		testcase_backup_reg_to_slot();
		testcase_write_from_data_reg();
		testcase_update_value_to_slot();
		break;
	case CMDQ_TESTCASE_ERROR:
		testcase_errors();
		testcase_async_request();
		testcase_module_full_dump();
		break;
	case CMDQ_TESTCASE_BASIC:
		testcase_write();
		testcase_write_with_mask();
		testcase_poll();
		testcase_scenario();
		break;
	case CMDQ_TESTCASE_READ_REG_REQUEST:
		testcase_get_result();
		break;
	case CMDQ_TESTCASE_GPR:
		testcase_read_to_data_reg();	/* must verify! */
		testcase_dram_access();
		break;
	case CMDQ_TESTCASE_DEFAULT:
		testcase_multiple_async_request();
		testcase_read_to_data_reg();
		testcase_get_result();
		testcase_scenario();
		testcase_write();
		testcase_poll();
		testcase_write_address();
		testcase_async_suspend_resume();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_trigger_thread();
		testcase_prefetch();
		testcase_long_command();
		testcase_dram_access();
		testcase_backup_register();
		testcase_fire_and_forget();
		testcase_backup_reg_to_slot();
		testcase_thread_dispatch();
		testcase_full_thread_array();
		break;
	default:
		CMDQ_LOG("[TESTCASE]CONFIG Not Found: gCmdqTestSecure: %d, testType: %lld\n",
			 gCmdqTestSecure, gCmdqTestConfig[0]);
		break;
	}
	/* Turn off GCE clock */
	cmdq_dev_enable_gce_clock(false);
}

ssize_t cmdq_test_proc(struct file *fp, char __user *u, size_t s, loff_t *l)
{
	int64_t testParameter[CMDQ_TESTCASE_PARAMETER_MAX];

	mutex_lock(&gCmdqTestProcLock);
	/* make sure the following section is protected */
	smp_mb();

	CMDQ_LOG("[TESTCASE]CONFIG: gCmdqTestSecure: %d, testType: %lld\n",
		 gCmdqTestSecure, gCmdqTestConfig[0]);
	CMDQ_LOG("[TESTCASE]CONFIG PARAMETER: [1]: %lld, [2]: %lld, [3]: %lld\n",
		 gCmdqTestConfig[1], gCmdqTestConfig[2], gCmdqTestConfig[3]);
	memcpy(testParameter, gCmdqTestConfig, sizeof(testParameter));
	gCmdqTestConfig[0] = 0LL;
	gCmdqTestConfig[1] = -1LL;
	mutex_unlock(&gCmdqTestProcLock);

	/* trigger test case here */
	CMDQ_MSG("//\n//\n//\ncmdq_test_proc\n");

	cmdq_get_func()->testSetup();

	switch (testParameter[0]) {
	case CMDQ_TEST_TYPE_NORMAL:
	case CMDQ_TEST_TYPE_SECURE:
		testcase_general_handling((int32_t)testParameter[1]);
		break;
	case CMDQ_TEST_TYPE_MONITOR_EVENT:
		/* (wait type, event ID or back register) */
		testcase_monitor_trigger((uint32_t)testParameter[1], (uint64_t)testParameter[2]);
		break;
	case CMDQ_TEST_TYPE_MONITOR_POLL:
		/* (poll register, poll value, poll mask) */
		testcase_poll_monitor_trigger((uint64_t)testParameter[1], (uint64_t)testParameter[2],
			(uint64_t)testParameter[3]);
		break;
	case CMDQ_TEST_TYPE_OPEN_COMMAND_DUMP:
		/* (scenario, buffersize) */
		testcase_open_buffer_dump((int32_t)testParameter[1], (int32_t)testParameter[2]);
		break;
	case CMDQ_TEST_TYPE_DUMP_DTS:
		cmdq_core_dump_dts_setting();
		break;
	case CMDQ_TEST_TYPE_FEATURE_CONFIG:
		if ((int32_t)testParameter[1] < 0)
			cmdq_core_dump_feature();
		else
			cmdq_core_set_feature((int32_t)testParameter[1], (uint32_t)testParameter[2]);
		break;
	case CMDQ_TEST_TYPE_MMSYS_PERFORMANCE:
		testcase_mmsys_performance((int32_t)testParameter[1]);
		break;
	default:
		break;
	}

	cmdq_get_func()->testCleanup();

	CMDQ_MSG("cmdq_test_proc ended\n");
	return 0;
}

static ssize_t cmdq_write_test_proc_config(struct file *file,
					   const char __user *userBuf, size_t count, loff_t *data)
{
	bool trick_test = false;
	char desc[50];
	long long int testConfig[CMDQ_TESTCASE_PARAMETER_MAX];
	int32_t len = 0;

	do {
		/* copy user input */
		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
		if (copy_from_user(desc, userBuf, len)) {
			CMDQ_MSG("TEST_CONFIG: data fail, length:%d\n", len);
			break;
		}
		desc[len] = '\0';

		/* Set initial test config value */
		memset(testConfig, -1, sizeof(testConfig));

		/* process and update config */
		if (sscanf(desc, "%lld %lld %lld %lld", &testConfig[0], &testConfig[1],
			&testConfig[2], &testConfig[3]) <= 0) {
			/* sscanf returns the number of items in argument list successfully filled. */
			CMDQ_MSG("TEST_CONFIG: sscanf failed, len:%d\n", len);
			break;
		}
		CMDQ_MSG("TEST_CONFIG: %lld, %lld, %lld, %lld\n",
			testConfig[0], testConfig[1], testConfig[2], testConfig[3]);

		if ((testConfig[0] < 0) || (testConfig[0] >= CMDQ_TEST_TYPE_MAX)) {
			CMDQ_MSG("TEST_CONFIG: testType:%lld, newTestSuit:%lld\n", testConfig[0], testConfig[1]);
			break;
		}
		if ((testConfig[0] < 2) && (testConfig[1] < 0))
			trick_test = true;

		mutex_lock(&gCmdqTestProcLock);
		/* set memory barrier for lock */
		smp_mb();

		memcpy(&gCmdqTestConfig, &testConfig, sizeof(testConfig));
		if (testConfig[0] == CMDQ_TEST_TYPE_NORMAL)
			gCmdqTestSecure = false;
		else
			gCmdqTestSecure = true;

		mutex_unlock(&gCmdqTestProcLock);
	} while (0);

	if (trick_test) {
		char node_name[25];
		char clk_name[20];
		int clk_enable = 0;
		struct clk *clk_module;

		/* trick to control clock by test node for testing */
		if (sscanf(desc, "%d %25s %20s", &clk_enable, node_name, clk_name) <= 0) {
			/* sscanf returns the number of items in argument list successfully filled. */
			CMDQ_LOG("CLOCK_TEST_CONFIG: sscanf failed: %s\n", desc);
		} else {
			cmdq_dev_get_module_clock_by_name(node_name, clk_name, &clk_module);
			cmdq_dev_enable_device_clock(clk_enable, clk_module, clk_name);
		}
	}

	return count;
}

void cmdq_test_init_setting(void)
{
	memset(&(gEventMonitor), 0x0, sizeof(gEventMonitor));
	memset(&(gPollMonitor), 0x0, sizeof(gPollMonitor));
}

static int cmdq_test_open(struct inode *pInode, struct file *pFile)
{
	return 0;
}

static const struct file_operations cmdq_fops = {
	.owner = THIS_MODULE,
	.open = cmdq_test_open,
	.read = cmdq_test_proc,
	.write = cmdq_write_test_proc_config,
};

static int __init cmdq_test_init(void)
{
#ifdef _CMDQ_TEST_PROC_
	CMDQ_MSG("cmdq_test_init\n");
	/* Initial value */
	gCmdqTestSecure = false;
	gCmdqTestConfig[0] = 0LL;
	gCmdqTestConfig[1] = -1LL;
	/* Mout proc entry for debug */
	gCmdqTestProcEntry = proc_mkdir("cmdq_test", NULL);
	if (gCmdqTestProcEntry != NULL) {
		if (proc_create("test", 0660, gCmdqTestProcEntry, &cmdq_fops) == NULL) {
			/* cmdq_test_init failed */
			CMDQ_MSG("cmdq_test_init failed\n");
		}
	}
#endif
	return 0;
}

static void __exit cmdq_test_exit(void)
{
#ifdef _CMDQ_TEST_PROC_
	CMDQ_MSG("cmdq_test_exit\n");
	if (gCmdqTestProcEntry != NULL) {
		proc_remove(gCmdqTestProcEntry);
		gCmdqTestProcEntry = NULL;
	}
#endif
}
module_init(cmdq_test_init);
module_exit(cmdq_test_exit);

MODULE_LICENSE("GPL");
#endif				/* CMDQ_TEST */
