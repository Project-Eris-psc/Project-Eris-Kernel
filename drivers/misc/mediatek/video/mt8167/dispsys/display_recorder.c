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

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <stdarg.h>
#include <mt-plat/met_drv.h>

#include "ddp_mmp.h"
#include "debug.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb_info.h"
#include "mtkfb.h"
#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "primary_display.h"

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "ddp_manager.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "disp_session.h"
#include "ddp_mmp.h"
#include <linux/trace_events.h>


#if defined(CONFIG_MTK_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)

unsigned int gCapturePriLayerEnable;
unsigned int gCaptureWdmaLayerEnable;
unsigned int gCapturePriLayerDownX = 20;
unsigned int gCapturePriLayerDownY = 20;
unsigned int gCapturePriLayerNum = 8;

static DEFINE_SPINLOCK(gdprec_logger_spinlock);

static struct dprec_debug_control _control = { 0 };

/*
*
static struct reg_base_map reg_map[] = {
	{"MMSYS", (0xf4000000)},
	{"OVL0", (0xF400c000)},
	{"OVL1", (0xF400d000)},
	{"RDMA0", (0xF400e000)},
	{"RDMA1", (0xF400f000)},
	{"RDMA2", (0xF4010000)},
	{"WDMA0", (0xF4011000)},
	{"WDMA1", (0xF4012000)},
	{"COLOR0", (0xF4013000)},
	{"COLOR1", (0xF4014000)},
	{"AAL", (0xF4015000)},
	{"GAMMA", (0xF4016000)},
	{"MERGE", (0xF4017000)},
	{"SPLIT0", (0xF4018000)},
	{"SPLIT1", (0xF4019000)},
	{"UFOE", (0xF401a000)},
	{"DSI0", (0xF401b000)},
	{"DSI1", (0xF401c000)},
	{"DPI", (0xF401d000)},
	{"PWM0", (0xF401e000)},
	{"PWM1", (0xF401f000)},
	{"MUTEX", (0xF4020000)},
	{"SMI_LARB0", (0xF4021000)},
	{"SMI_COMMON", (0xF4022000)},
	{"OD", (0xF4023000)},
	{"MIPITX0", (0xF0215000)},
	{"MIPITX1", (0xF0216000)},
};
*/

static struct event_string_map event_map[] = {
	{"Set Config Dirty", DPREC_EVENT_CMDQ_SET_DIRTY},
	{"Wait Stream EOF", DPREC_EVENT_CMDQ_WAIT_STREAM_EOF},
	{"Signal CMDQ Event-Stream EOF", DPREC_EVENT_CMDQ_SET_EVENT_ALLOW},
	{"Flush CMDQ", DPREC_EVENT_CMDQ_FLUSH},
	{"Reset CMDQ", DPREC_EVENT_CMDQ_RESET},
	{"Frame Done", DPREC_EVENT_FRAME_DONE},
	{"Frame Start", DPREC_EVENT_FRAME_START},
};



static mmp_event dprec_mmp_event_spy(enum DPREC_LOGGER_ENUM l)
{
	switch (l & 0xffffff) {
	case DPREC_LOGGER_PRIMARY_MUTEX:
		return ddp_mmp_get_events()->primary_sw_mutex;
	case DPREC_LOGGER_PRIMARY_TRIGGER:
		return ddp_mmp_get_events()->primary_trigger;
	case DPREC_LOGGER_PRIMARY_CONFIG:
		return ddp_mmp_get_events()->primary_config;
	case DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY:
		return ddp_mmp_get_events()->primary_set_dirty;
	case DPREC_LOGGER_PRIMARY_CMDQ_FLUSH:
		return ddp_mmp_get_events()->primary_cmdq_flush;
	case DPREC_LOGGER_DISPMGR_PREPARE:
		return ddp_mmp_get_events()->session_prepare;
	case DPREC_LOGGER_DISPMGR_SET_INPUT:
		return ddp_mmp_get_events()->session_set_input;
	case DPREC_LOGGER_DISPMGR_TRIGGER:
		return ddp_mmp_get_events()->session_trigger;
	case DPREC_LOGGER_DISPMGR_RELEASE:
		return ddp_mmp_get_events()->session_release;
	case DPREC_LOGGER_DISPMGR_WAIT_VSYNC:
		return ddp_mmp_get_events()->session_wait_vsync;
	case DPREC_LOGGER_DISPMGR_CACHE_SYNC:
		return ddp_mmp_get_events()->primary_cache_sync;
	case DPREC_LOGGER_DSI_EXT_TE:
		return ddp_mmp_get_events()->dsi_te;
	}
	return 0xffff;
}

static void dprec_to_mmp(unsigned int type_logsrc, mmp_log_type mmp_log, unsigned int data1,
			 unsigned data2)
{
	int mmp_event = dprec_mmp_event_spy(type_logsrc);

	if (mmp_event < MMProfileMaxEventCount)  /* if (mmp_event < 0xffff) */
		mmprofile_log_ex(mmp_event, mmp_log, data1, data2);

}

/*
*
static const char *_find_module_by_reg_addr(unsigned int reg)
{
	int i = 0;
	unsigned int module_offset = 0x1000;
	unsigned int base = (reg & (~(module_offset - 1)));

	for (i = 0; i < sizeof(reg_map) / sizeof(struct reg_base_map); i++) {
		if (base == reg_map[i].module_reg_base)
			return reg_map[i].module_name;
	}

	return "unknown";
}
*/

static const char *_get_event_string(enum DPREC_EVENT event)
{
	int i = 0;

	for (i = 0; i < sizeof(event_map) / sizeof(struct event_string_map); i++) {
		if (event == event_map[i].event)
			return event_map[i].event_string;
	}

	return "unknown";
}

static unsigned long long get_current_time_us(void)
{
	unsigned long long time = sched_clock();
	/* return do_div(time,1000); */
	return time;
/*
*
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
*/
}


#define dprec_string_max_length 512
static unsigned char dprec_string_buffer[dprec_string_max_length] = { 0 };
static struct dprec_logger logger[DPREC_LOGGER_NUM];

#define dprec_dump_max_length (1024 * 16 * 4)
static unsigned char dprec_string_buffer_analysize[dprec_dump_max_length] = { 0 };

static unsigned int analysize_length;


unsigned int dprec_error_log_len;
unsigned int dprec_error_log_buflen = DPREC_ERROR_LOG_BUFFER_LENGTH;
unsigned int dprec_error_log_id;

char dprec_error_log_buffer[DPREC_ERROR_LOG_BUFFER_LENGTH];
static struct dprec_logger_event dprec_vsync_irq_event;
static struct met_log_map dprec_met_info[DISP_SESSION_MEMORY + 2] = {
	{"UNKWON", 0, 0},
	{"OVL0-DSI", 0, 0},
	{"OVL1-MHL", 0, 0},
	{"OVL1-SMS", 0, 0},
};

int dprec_init(void)
{
	memset((void *)&_control, 0, sizeof(_control));
	memset((void *)&logger, 0, sizeof(logger));
	memset((void *)dprec_error_log_buffer, 0, DPREC_ERROR_LOG_BUFFER_LENGTH);
	ddp_mmp_init();
	dprec_logger_event_init(&dprec_vsync_irq_event, "VSYNC_IRQ", DPREC_LOGGER_LEVEL_SYSTRACE, NULL);

	return 0;
}

void dprec_event_op(enum DPREC_EVENT event)
{
	int len = 0;

	if (_control.overall_switch == 0)
		return;


	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[DPREC]");
	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[EVENT]");
	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "[%s]", _get_event_string(event));

	len += scnprintf(dprec_string_buffer + len, dprec_string_max_length - len, "\n");
}


static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	unsigned long ret = 0;

	if ((long long)nsec < 0)
		nsec = -nsec;

	ret = do_div(nsec, 1000000);
	return ret / 10000;
}

static long long msec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000000);
		return -nsec;
	}
	do_div(nsec, 1000000000);

	return nsec;
}

static unsigned long msec_low(unsigned long long nsec)
{
	unsigned long ret = 0;

	if ((long long)nsec < 0)
		nsec = -nsec;

	ret = do_div(nsec, 1000000000);
	return ret / 10000000;
}


/* #define SPLIT_NS(x) (unsigned long long)nsec_high(x), (unsigned long)nsec_low(x) */
/* #define SPLIT_MS(x) (unsigned long long)msec_high(x), (unsigned long)msec_low(x) */


static const char *dprec_logger_spy(enum DPREC_LOGGER_ENUM l)
{
	switch (l) {
	case DPREC_LOGGER_PRIMARY_TRIGGER:
		return "Primary Path Trigger";
	case DPREC_LOGGER_PRIMARY_MUTEX:
		return "Primary Path Mutex";
	case DPREC_LOGGER_DISPMGR_WAIT_VSYNC:
		return "Wait VYSNC Signal";
	case DPREC_LOGGER_RDMA0_TRANSFER:
		return "RDMA0 Transfer";
	case DPREC_LOGGER_DSI_EXT_TE:
		return "DSI_EXT_TE";
	case DPREC_LOGGER_ESD_RECOVERY:
		return "ESD Recovery";
	case DPREC_LOGGER_ESD_CHECK:
		return "ESD Check";
	case DPREC_LOGGER_ESD_CMDQ:
		return "ESD CMDQ Keep";
	case DPREC_LOGGER_PRIMARY_CONFIG:
		return "Primary Path Config";
	case DPREC_LOGGER_DISPMGR_PREPARE:
		return "DISPMGR Prepare";
	case DPREC_LOGGER_DISPMGR_CACHE_SYNC:
		return "DISPMGR Cache Sync";
	case DPREC_LOGGER_DISPMGR_SET_INPUT:
		return "DISPMGR Set Input";
	case DPREC_LOGGER_DISPMGR_TRIGGER:
		return "DISPMGR Trigger";
	case DPREC_LOGGER_DISPMGR_RELEASE:
		return "DISPMGR Release";
	case DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY:
		return "Primary CMDQ Dirty";
	case DPREC_LOGGER_PRIMARY_CMDQ_FLUSH:
		return "Primary CMDQ Flush";
	case DPREC_LOGGER_PRIMARY_BUFFER_KEEP:
		return "Fence Buffer Keep";
	case DPREC_LOGGER_WDMA_DUMP:
		return "Screen Capture(wdma)";

	default:
		return "unknown";
	}
}

void dprec_logger_trigger(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	enum DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;
	struct dprec_logger *l;
	unsigned long long time;

	dprec_to_mmp(type_logsrc, MMPROFILE_FLAG_PULSE, val1, val2);

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	time = get_current_time_us();
	l = &logger[source];

	if (l->count == 0) {
		l->ts_start = time;
		l->ts_trigger = time;
		l->period_min_frame = 0xffffffffffffffff;
	} else {
		l->period_frame = time - l->ts_trigger;

		if (l->period_frame > l->period_max_frame)
			l->period_max_frame = l->period_frame;

		if (l->period_frame < l->period_min_frame)
			l->period_min_frame = l->period_frame;

		l->ts_trigger = time;

		if (l->count == 0)
			l->ts_start = l->ts_trigger;
	}
	l->count++;

	if (source == DPREC_LOGGER_DSI_EXT_TE)
		;/*
		  * DISPMSG("count=%d, time=%llu, period=%llu, max=%llu,min=%llu,start=%llu,trigger=%llu\n",
			    l->count, time, l->period_frame, l->period_max_frame, l->period_min_frame,
			    l->ts_start, l->ts_trigger);
		  */
		/*
		 * DISPMSG("count=%d,period=%lld.%02lums,max=%lld.%02lums\n",
				l->count, SPLIT_NS(l->period_frame), SPLIT_NS(l->period_max_frame));
		 */


	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
}

unsigned long long dprec_logger_get_current_hold_period(unsigned int type_logsrc)
{
	unsigned long long period = 0;
	unsigned long flags = 0;
	unsigned long long time = get_current_time_us();
	enum DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;
	struct dprec_logger *l;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];

	if (l->ts_trigger)
		period = (time - l->ts_trigger);
	else
		period = 0;

	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);

	return period;
}

void dprec_logger_start(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	enum DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;
	struct dprec_logger *l;
	unsigned long long time;

	dprec_to_mmp(type_logsrc, MMPROFILE_FLAG_START, val1, val2);
	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];
	time = get_current_time_us();

	if (l->count == 0) {
		l->ts_start = time;
		l->period_min_frame = 0xffffffffffffffff;
	}

	l->ts_trigger = time;
	if (source == DPREC_LOGGER_RDMA0_TRANSFER)
		;/* DISPMSG("start, count=%llu,trigger=%llu.%02lums\n",*/
		 /*   l->count, SPLIT_NS(l->ts_trigger));*/

	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
}

void dprec_logger_done(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
	unsigned long flags = 0;
	enum DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;
	struct dprec_logger *l;
	unsigned long long time;

	dprec_to_mmp(type_logsrc, MMPROFILE_FLAG_END, val1, val2);

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];
	time = get_current_time_us();

	if (l->ts_start == 0)
		goto done;


	l->period_frame = time - l->ts_trigger;

	if (l->period_frame > l->period_max_frame)
		l->period_max_frame = l->period_frame;

	if (l->period_frame < l->period_min_frame)
		l->period_min_frame = l->period_frame;

	l->period_total += l->period_frame;
	l->count++;

	if (source == DPREC_LOGGER_RDMA0_TRANSFER)
		;/* DISPMSG("done, count=%llu,period=%llu.%02lums,max=%llu.%02lums\n",*/
		 /*   l->count, SPLIT_NS(l->period_frame), SPLIT_NS(l->period_max_frame));*/


done:
	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
}

void dprec_logger_event_init(struct dprec_logger_event *p, char *name, uint32_t level,
			     mmp_event *mmp_root)
{
	if (p) {
		scnprintf(p->name, ARRAY_SIZE(p->name), name);
		if (mmp_root)
			p->mmp = mmprofile_register_event(*mmp_root, name);
		else
			p->mmp = mmprofile_register_event(ddp_mmp_get_events()->DDP, name);

		mmprofile_enable_event_recursive(p->mmp, 1);

		p->level = level;

		memset((void *)&p->logger, 0, sizeof(p->logger));
	}
}

#ifdef CONFIG_TRACING

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");
}

static inline void mmp_kernel_trace_begin(char *name)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "B|%d|%s\n", current->tgid, name);
	preempt_enable();
}

static inline void mmp_kernel_trace_counter(char *name, int count)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d\n",
			   in_interrupt() ? -1 : current->tgid, name, count);
	preempt_enable();
}

static inline void mmp_kernel_trace_end(void)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "E\n");
	preempt_enable();
}


void dprec_logger_frame_seq_begin(unsigned int session_id, unsigned frm_sequence)
{
	unsigned device_type = DISP_SESSION_TYPE(session_id);

	if (frm_sequence <= 0 || session_id <= 0)
		return;
	if (device_type > DISP_SESSION_MEMORY) {
		pr_debug("seq_begin session_id(0x%x) error, seq(%d)\n", session_id, frm_sequence);
		return;
	}

	if (dprec_met_info[device_type].begin_frm_seq != frm_sequence) {
		__mt_update_tracing_mark_write_addr();
		preempt_disable();
		event_trace_printk(tracing_mark_write_addr, "S|%d|%s|%d\n", current->tgid,
				   dprec_met_info[device_type].log_name, frm_sequence);
		preempt_enable();
		dprec_met_info[device_type].begin_frm_seq = frm_sequence;
	}
}

void dprec_logger_frame_seq_end(unsigned int session_id, unsigned frm_sequence)
{
	unsigned device_type = DISP_SESSION_TYPE(session_id);

	if (frm_sequence <= 0 || session_id <= 0)
		return;
	if (device_type > DISP_SESSION_MEMORY) {
		pr_debug("seq_end session_id(0x%x) , seq(%d)\n", session_id, frm_sequence);
		return;
	}

	if (dprec_met_info[device_type].end_frm_seq != frm_sequence) {
		__mt_update_tracing_mark_write_addr();
		preempt_disable();
		event_trace_printk(tracing_mark_write_addr, "F|%d|%s|%d\n", current->tgid,
				   dprec_met_info[device_type].log_name, frm_sequence);
		preempt_enable();
		dprec_met_info[device_type].end_frm_seq = frm_sequence;
	}
}

#else
void dprec_logger_frame_seq_begin(unsigned int session_id, unsigned frm_sequence)
{

}

void dprec_logger_frame_seq_end(unsigned int session_id, unsigned frm_sequence)
{

}
#endif

void dprec_start(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			mmprofile_log_ex(event->mmp, MMPROFILE_FLAG_START, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER) {
			unsigned long flags = 0;
			struct dprec_logger *l;
			unsigned long long time;

			spin_lock_irqsave(&gdprec_logger_spinlock, flags);
			l = &(event->logger);
			time = get_current_time_us();
			if (l->count == 0) {
				l->ts_start = time;
				l->period_min_frame = 0xffffffffffffffff;
			}

			l->ts_trigger = time;

			spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
		}
		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			;/* pr_debug("DPREC/ "LOG_TAG "DISP/%s start,0x%08x,0x%08x\n", event->name, val1, val2); */

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			pr_debug("DISP/%s start,0x%08x,0x%08x\n", event->name, val1, val2);

#ifdef CONFIG_TRACING
		if (event->level & DPREC_LOGGER_LEVEL_SYSTRACE && _control.systrace) {
			char name[256];

			scnprintf(name, ARRAY_SIZE(name), "K_%s_0x%x_0x%x",
				  event->name, val1, val2);

			mmp_kernel_trace_begin(name);
			/* trace_printk("B|%d|%s\n", current->pid, event->name); */
		}
#endif
	}
}

void dprec_done(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			mmprofile_log_ex(event->mmp, MMPROFILE_FLAG_END, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER) {
			unsigned long flags = 0;
			struct dprec_logger *l;
			unsigned long long time;

			spin_lock_irqsave(&gdprec_logger_spinlock, flags);
			l = &(event->logger);
			time = get_current_time_us();
			if (l->ts_start != 0) {
				l->period_frame = time - l->ts_trigger;

				if (l->period_frame > l->period_max_frame)
					l->period_max_frame = l->period_frame;

				if (l->period_frame < l->period_min_frame)
					l->period_min_frame = l->period_frame;

				l->ts_trigger = 0;
				l->period_total += l->period_frame;
				l->count++;
			}

			spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
		}
		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			;/* pr_debug("DPREC"LOG_TAG "DISP/"LOG_TAG,"done,0x%08x,0x%08x\n", event->name, val1, val2); */

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			pr_debug("DISP/%s done,0x%08x,0x%08x\n", event->name, val1, val2);

#ifdef CONFIG_TRACING
		if (event->level & DPREC_LOGGER_LEVEL_SYSTRACE && _control.systrace)
			mmp_kernel_trace_end();
			/* trace_printk("E|%s\n", event->name); */

#endif
	}
}

void dprec_trigger(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			mmprofile_log_ex(event->mmp, MMPROFILE_FLAG_PULSE, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER) {
			unsigned long flags = 0;
			struct dprec_logger *l;
			unsigned long long time;

			spin_lock_irqsave(&gdprec_logger_spinlock, flags);
			l = &(event->logger);
			time = get_current_time_us();

			if (l->count == 0) {
				l->ts_start = time;
				l->ts_trigger = time;
				l->period_min_frame = 0xffffffffffffffff;
			} else {
				l->period_frame = time - l->ts_trigger;

				if (l->period_frame > l->period_max_frame)
					l->period_max_frame = l->period_frame;

				if (l->period_frame < l->period_min_frame)
					l->period_min_frame = l->period_frame;

				l->ts_trigger = time;

				if (l->count == 0)
					l->ts_start = l->ts_trigger;
			}

			l->count++;
			spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
		}
		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			;/* pr_debug("DPREC/"LOG_TAG "%s trigger,0x%08x,0x%08x\n", event->name, val1, val2); */

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			pr_debug("DISP/%s trigger,0x%08x,0x%08x\n", event->name, val1, val2);

#ifdef CONFIG_TRACING
		if (event->level & DPREC_LOGGER_LEVEL_SYSTRACE && _control.systrace) {
			char name[256];

			scnprintf(name, ARRAY_SIZE(name), "K_%s_0x%x_0x%x",
				  event->name, val1, val2);
			mmp_kernel_trace_begin(name);
			mmp_kernel_trace_end();
			/* trace_printk("B|%d|%s\n", current->pid, event->name); */
		}
#endif
	}
}


void dprec_submit(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
	if (event) {
		if (event->level & DPREC_LOGGER_LEVEL_MMP)
			mmprofile_log_ex(event->mmp, MMPROFILE_FLAG_PULSE, val1, val2);

		if (event->level & DPREC_LOGGER_LEVEL_LOGGER)
			;

		if (event->level & DPREC_LOGGER_LEVEL_MOBILE_LOG)
			;/* pr_debug("DPREC/"LOG_TAG"%s trigger,0x%08x,0x%08x\n",*/
			 /*   event->name, val1, val2);*/

		if (event->level & DPREC_LOGGER_LEVEL_UART_LOG)
			pr_debug("DISP/%s trigger,0x%08x,0x%08x\n", event->name, val1, val2);

	}
}

void dprec_logger_submit(unsigned int type_logsrc, unsigned long long period,
			 unsigned int fence_idx)
{
	unsigned long flags = 0;
	enum DPREC_LOGGER_ENUM source = type_logsrc & 0xffffff;
	struct dprec_logger *l;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];

	l->period_frame = period;

	if (l->period_frame > l->period_max_frame)
		l->period_max_frame = l->period_frame;

	if (l->period_frame < l->period_min_frame)
		l->period_min_frame = l->period_frame;

	l->period_total += l->period_frame;
	l->count++;

	if (source == DPREC_LOGGER_RDMA0_TRANSFER)
		;/* DISPMSG("done, count=%llu,period=%llu.%02lums,max=%llu.%02lums\n",*/
		 /*   l->count, SPLIT_NS(l->period_frame), SPLIT_NS(l->period_max_frame));*/

	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);

	dprec_to_mmp(type_logsrc, MMPROFILE_FLAG_PULSE, (unsigned int)l->period_max_frame, fence_idx);
}

static unsigned long long ts_dprec_reset;
void dprec_logger_reset_all(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(logger); i++)
		dprec_logger_reset(i);

	ts_dprec_reset = get_current_time_us();
}

void dprec_logger_reset(enum DPREC_LOGGER_ENUM source)
{
	struct dprec_logger *l = &logger[source];

	memset((void *)l, 0, sizeof(struct dprec_logger));
	l->period_min_frame = 10000000;
	/* DISPMSG("count=%d, period=%llu, max=%llu,min=%llu,start=%llu,trigger=%llu\n",*/
	/*		l->count, l->period_frame, l->period_max_frame,*/
	/*		l->period_min_frame, l->ts_start, l->ts_trigger);*/
}

int dprec_logger_get_result_string(enum DPREC_LOGGER_ENUM source, char *stringbuf, int strlen)
{
	unsigned long flags = 0;
	int len = 0;
	struct dprec_logger *l;
	unsigned long long total = 0;
	unsigned long long avg;
	unsigned long long count;
	unsigned long long fps_high = 0;
	unsigned long fps_low = 0;

	spin_lock_irqsave(&gdprec_logger_spinlock, flags);
	l = &logger[source];

	/* calculate average period need use available total period */
	if (l->period_total)
		total = l->period_total;
	else
		total = l->ts_trigger - l->ts_start;

	avg = total ? total : 1;
	count = l->count ? l->count : 1;

	do_div(avg, count);

	/* calculate fps need use whole time period */
	/* total = l->ts_trigger - l->ts_start; */

	fps_high = l->count;
	do_div(total, 1000 * 1000 * 1000);
	if (total == 0)
		total = 1;
	fps_high *= 100;
	do_div(fps_high, total);
	fps_low = do_div(fps_high, 100);
#if 0
	DISPMSG("total = %lld, count = %lld, fps=%lld.%2ld, avg=%lld, trigger=%lld, start=%lld\n",
		total, l->count, fps_high, fps_low, avg, l->ts_trigger, l->ts_start);
	len += scnprintf(stringbuf+len, strlen - len,
			 "[DPREC][LOGGER]%s, count=%d, avg count=%d.%02dfps, avg period=%lld.%ludms, max=%d.%02dms, min=%lld.%lums\n",
			 dprec_logger_spy(source), l->count, fps / 100, fps % 100,
			 SPLIT_NS(l->period_max_frame), max / 100, max % 100, SPLIT_NS(l->period_min_frame));
#endif
	len += scnprintf(stringbuf + len, strlen - len,
			"|%-24s|%8llu |%8lld.%02ld |%8llu.%02ld |%8llu.%02ld |%8llu.%02ld |\n",
			dprec_logger_spy(source), l->count, fps_high, fps_low,
			nsec_high(avg), nsec_low(avg),
			nsec_high(l->period_max_frame), nsec_low(l->period_max_frame),
			nsec_high(l->period_min_frame), nsec_low(l->period_min_frame));




	spin_unlock_irqrestore(&gdprec_logger_spinlock, flags);
	return len;
}

int dprec_logger_get_result_string_all(char *stringbuf, int strlen)
{
	int n = 0;
	int i = 0;

	n += scnprintf(stringbuf + n, strlen - n,
		       "|**** Display Driver Statistic Information Dump ****\n");
	n += scnprintf(stringbuf + n, strlen - n, "|Timestamp Begin=%llu.%03lds, End=%llu.%03lds\n",
			msec_high(ts_dprec_reset), msec_low(ts_dprec_reset),
			msec_high(get_current_time_us()), msec_low(get_current_time_us()));
	n += scnprintf(stringbuf + n, strlen - n,
		       "|------------------------+---------+------------+------------+------------+------------|\n");
	n += scnprintf(stringbuf + n, strlen - n,
		       "|Event                   | count   | fps        |average(ms) | max(ms)    | min(ms)    |\n");
	n += scnprintf(stringbuf + n, strlen - n,
		       "|------------------------+---------+------------+------------+------------+------------|\n");
	for (i = 0; i < ARRAY_SIZE(logger); i++)
		n += dprec_logger_get_result_string(i, stringbuf + n, strlen - n);

	n += scnprintf(stringbuf + n, strlen - n,
		       "|------------------------+---------+------------+------------+------------+------------|\n");
	n += scnprintf(stringbuf + n, strlen - n, "dprec error log buffer\n");

	return n;
}

enum dprec_record_type {
	DPREC_REG_OP = 1,
	DPREC_CMDQ_EVENT,
	DPREC_ERROR,
	DPREC_FENCE
};

struct dprec_reg_op_record {
	unsigned int use_cmdq;
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};

struct dprec_fence_record {
	unsigned int fence_fd;
	unsigned int buffer_idx;
	unsigned int ion_fd;
};

struct dprec_record {
	enum dprec_record_type type;
	unsigned int ts;
	union {
		struct dprec_fence_record fence;
		struct dprec_reg_op_record reg_op;
	} rec;
};

void dprec_stub_irq(unsigned int irq_bit)
{
	/* DISP_REG_SET(NULL, DISP_REG_CONFIG_MUTEX_INTEN,0xffffffff); */
	if (irq_bit == DDP_IRQ_DSI0_EXT_TE)
		dprec_logger_trigger(DPREC_LOGGER_DSI_EXT_TE, irq_bit, 0);
	else if (irq_bit == DDP_IRQ_RDMA0_START)
		dprec_logger_start(DPREC_LOGGER_RDMA0_TRANSFER, irq_bit, 0);
	else if (irq_bit == DDP_IRQ_RDMA0_DONE)
		dprec_logger_done(DPREC_LOGGER_RDMA0_TRANSFER, irq_bit, 0);

	/* DISPMSG("irq:0x%08x\n", irq_bit); */
}

static unsigned int vsync_cnt;

void dprec_stub_event(enum DISP_PATH_EVENT event)
{
	/* DISP_REG_SET(NULL,DISP_REG_CONFIG_MUTEX_INTEN,0xffffffff); */
	if (event == DISP_PATH_EVENT_IF_VSYNC) {
		vsync_cnt++;
		/* mmprofile_log_ex(ddp_mmp_get_events()->vsync_count, MMPROFILE_FLAG_PULSE, vsync_cnt, 0); */
		dprec_start(&dprec_vsync_irq_event, 0, 0);
		dprec_done(&dprec_vsync_irq_event, 0, 0);
	}
}

unsigned int dprec_get_vsync_count(void)
{
	return vsync_cnt;
}

void dprec_reg_op(void *cmdq, unsigned int reg, unsigned int val, unsigned int mask)
{
}

void dprec_logger_dump(char *string)
{
	analysize_length +=
	    scnprintf(dprec_string_buffer_analysize + analysize_length,
		      dprec_dump_max_length - analysize_length, string);
}

void dprec_logger_dump_reset(void)
{
	analysize_length = 0;
	memset(dprec_string_buffer_analysize, 0, sizeof(dprec_string_buffer_analysize));
}

char *dprec_logger_get_dump_addr()
{
	return dprec_string_buffer_analysize;
}

unsigned int dprec_logger_get_dump_len(void)
{
	pr_debug("dump_len %d\n", analysize_length);

	return analysize_length;
}

enum DPREC_DEBUG_BIT_ENUM {
	DPREC_DEBUG_BIT_OVERALL_SWITCH = 0,
	DPREC_DEBUG_BIT_CMM_DUMP_SWITCH,
	DPREC_DEBUG_BIT_CMM_DUMP_VA,
	DPREC_DEBUG_BIT_SYSTRACE,
};

int dprec_handle_option(unsigned int option)
{
	_control.overall_switch = (option & (1 << DPREC_DEBUG_BIT_OVERALL_SWITCH));
	_control.cmm_dump = (option & (1 << DPREC_DEBUG_BIT_CMM_DUMP_SWITCH));
	_control.cmm_dump_use_va = (option & (1 << DPREC_DEBUG_BIT_CMM_DUMP_VA));
	_control.systrace = (option & (1 << DPREC_DEBUG_BIT_SYSTRACE));
	DISPMSG("dprec control=%p\n", &_control);

	return 0;
}

int dprec_mmp_dump_ovl_layer(struct OVL_CONFIG_STRUCT *ovl_layer, unsigned int l,
			     unsigned int session /*1:primary, 2:external, 3:memory */)
{
	if (gCapturePriLayerEnable) {
		if (gCapturePriLayerNum >= HW_OVERLAY_COUNT)
			ddp_mmp_ovl_layer(ovl_layer, gCapturePriLayerDownX, gCapturePriLayerDownY, session);
		else if (gCapturePriLayerNum == l)
			ddp_mmp_ovl_layer(ovl_layer, gCapturePriLayerDownX, gCapturePriLayerDownY, session);

		return 0;
	}
	return -1;
}

int dprec_mmp_dump_wdma_layer(void *wdma_layer, unsigned int wdma_num)
{
	if (gCaptureWdmaLayerEnable)
		ddp_mmp_wdma_layer((struct WDMA_CONFIG_STRUCT *)wdma_layer, wdma_num, gCapturePriLayerDownX,
				   gCapturePriLayerDownY);

	return -1;
}

int dprec_mmp_dump_rdma_layer(void *rdma_layer, unsigned int rdma_num)
{
	if (gCaptureWdmaLayerEnable)
		ddp_mmp_rdma_layer((struct RDMA_CONFIG_STRUCT *)rdma_layer, rdma_num, gCapturePriLayerDownX,
				   gCapturePriLayerDownY);

	return -1;
}


#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 2
#define FENCE_BUFFER_COUNT 22
#define DEBUG_BUFFER_COUNT 4
#define DUMP_BUFFER_COUNT 2

struct logger_buffer {
	char (*buffer_ptr)[LOGGER_BUFFER_SIZE];
	unsigned int len;
	unsigned int id;
	const unsigned int count;
	const unsigned int size;
};


static char err_buffer[ERROR_BUFFER_COUNT][LOGGER_BUFFER_SIZE];
static char fence_buffer[FENCE_BUFFER_COUNT][LOGGER_BUFFER_SIZE];
static char dbg_buffer[DEBUG_BUFFER_COUNT][LOGGER_BUFFER_SIZE];
static char dump_buffer[DUMP_BUFFER_COUNT][LOGGER_BUFFER_SIZE];
static struct logger_buffer dprec_logger_buffer[DPREC_LOGGER_PR_NUM] = {
	{err_buffer, 0, 0, ERROR_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{fence_buffer, 0, 0, FENCE_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{dbg_buffer, 0, 0, DEBUG_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
	{dump_buffer, 0, 0, DUMP_BUFFER_COUNT, LOGGER_BUFFER_SIZE},
};

void get_disp_err_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
	*addr = (unsigned long)err_buffer;
	*size = ERROR_BUFFER_COUNT * LOGGER_BUFFER_SIZE;
	*start = 0;
}

void get_disp_fence_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
	*addr = (unsigned long)fence_buffer;
	*size = FENCE_BUFFER_COUNT * LOGGER_BUFFER_SIZE;
	*start = 0;
}

void get_disp_dbg_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
	*addr = (unsigned long)dbg_buffer;
	*size = DEBUG_BUFFER_COUNT * LOGGER_BUFFER_SIZE;
	*start = 0;
}

void get_disp_dump_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
	*addr = (unsigned long)dump_buffer;
	*size = DUMP_BUFFER_COUNT * LOGGER_BUFFER_SIZE;
	*start = 0;
}

static DEFINE_SPINLOCK(dprec_logger_spinlock);

int dprec_logger_pr(unsigned int type, char *fmt, ...)
{
	int n = 0;
	unsigned long flags = 0;
	uint64_t time = get_current_time_us();
	unsigned long rem_nsec;
	char (*buf_arr)[LOGGER_BUFFER_SIZE];
	char *buf = NULL;
	int len = 0;

	if (type >= DPREC_LOGGER_PR_NUM)
		return -1;

	spin_lock_irqsave(&dprec_logger_spinlock, flags);
	if (dprec_logger_buffer[type].len < 128) {
		dprec_logger_buffer[type].id++;
		dprec_logger_buffer[type].id = dprec_logger_buffer[type].id % dprec_logger_buffer[type].count;
		dprec_logger_buffer[type].len = dprec_logger_buffer[type].size;
	}
	buf_arr = (char (*)[LOGGER_BUFFER_SIZE])dprec_logger_buffer[type].buffer_ptr;
	buf = buf_arr[dprec_logger_buffer[type].id] +
		dprec_logger_buffer[type].size - dprec_logger_buffer[type].len;
	len = dprec_logger_buffer[type].len;

	if (buf) {
		va_list args;

		rem_nsec = do_div(time, 1000000000);
		n += snprintf(buf + n, len - n, "[%5lu.%06lu]", (unsigned long)time,
			      rem_nsec / 1000);

		va_start(args, fmt);
		n += vscnprintf(buf + n, len - n, fmt, args);
		va_end(args);
	}

	dprec_logger_buffer[type].len -= n;
	spin_unlock_irqrestore(&dprec_logger_spinlock, flags);

	return n;
}

static char *_logger_pr_type_spy(enum DPREC_LOGGER_PR_TYPE type)
{
	switch (type) {
	case DPREC_LOGGER_ERROR:
		return "error";
	case DPREC_LOGGER_FENCE:
		return "fence";
	case DPREC_LOGGER_DEBUG:
		return "dbg";
	case DPREC_LOGGER_DUMP:
		return "dump";
	default:
		return "unknown";
	}
}

ssize_t dprec_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	loff_t pos = *ppos;
	size_t ret;

	if (pos < 0)
		return -EINVAL;

	ret = copy_to_user(to, from, available);
	if (ret == available)
		return -EFAULT;
	available -= ret;
	*ppos = pos + available;
	return available;
}

int dprec_logger_get_buf(enum DPREC_LOGGER_PR_TYPE type, char *stringbuf, int len)
{
	int n = 0;
	int i;
	int c = dprec_logger_buffer[type].id;
	char (*buf_arr)[LOGGER_BUFFER_SIZE];

	if (type >= DPREC_LOGGER_PR_NUM || len < 0)
		return 0;
	buf_arr = (char (*)[LOGGER_BUFFER_SIZE])dprec_logger_buffer[type].buffer_ptr;

	for (i = 0; i < dprec_logger_buffer[type].count; i++) {
		c++;
		c %= dprec_logger_buffer[type].count;
		n += scnprintf(stringbuf + n, len - n, "dprec log buffer[%s][%d]\n",
					   _logger_pr_type_spy(type), c);
		n += scnprintf(stringbuf + n, len - n, "%s\n", buf_arr[c]);

	}

	return n;
}

#else

unsigned int gCapturePriLayerEnable;
unsigned int gCaptureWdmaLayerEnable;
unsigned int gCapturePriLayerDownX = 20;
unsigned int gCapturePriLayerDownY = 20;
unsigned int gCapturePriLayerNum = 8;

struct dprec_logger logger[DPREC_LOGGER_NUM] = { { 0 } };

unsigned int dprec_error_log_len;
unsigned int dprec_error_log_buflen = DPREC_ERROR_LOG_BUFFER_LENGTH;
unsigned int dprec_error_log_id;

int dprec_init(void)
{
	return 0;
}

void dprec_event_op(enum DPREC_EVENT event)
{
}

void dprec_logger_trigger(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
}

unsigned long long dprec_logger_get_current_hold_period(unsigned int type_logsrc)
{
	return 0;
}

void dprec_logger_start(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
}

void dprec_logger_done(unsigned int type_logsrc, unsigned int val1, unsigned int val2)
{
}

void dprec_logger_event_init(struct dprec_logger_event *p, char *name, uint32_t level,
			     mmp_event *mmp_root)
{
}

void dprec_logger_frame_seq_begin(unsigned int session_id, unsigned frm_sequence)
{
}

void dprec_logger_frame_seq_end(unsigned int session_id, unsigned frm_sequence)
{
}

void dprec_start(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_done(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_trigger(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_submit(struct dprec_logger_event *event, unsigned int val1, unsigned int val2)
{
}

void dprec_logger_submit(unsigned int type_logsrc, unsigned long long period,
			 unsigned int fence_idx)
{
}

void dprec_logger_reset_all(void)
{
}

void dprec_logger_reset(enum DPREC_LOGGER_ENUM source)
{
}

int dprec_logger_get_result_string(enum DPREC_LOGGER_ENUM source, char *stringbuf, int strlen)
{
	return 0;
}

int dprec_logger_get_result_string_all(char *stringbuf, int strlen)
{
	return 0;
}

void dprec_stub_irq(unsigned int irq_bit)
{
}

void dprec_stub_event(enum DISP_PATH_EVENT event)
{
}

unsigned int dprec_get_vsync_count(void)
{
	return 0;
}

void dprec_reg_op(void *cmdq, unsigned int reg, unsigned int val, unsigned int mask)
{
}

void dprec_logger_dump(char *string)
{
}

void dprec_logger_dump_reset(void)
{
}

char *dprec_logger_get_dump_addr()
{
	return NULL;
}

unsigned int dprec_logger_get_dump_len(void)
{
	return 0;
}

int dprec_handle_option(unsigned int option)
{
	return 0;
}

int dprec_mmp_dump_ovl_layer(struct OVL_CONFIG_STRUCT *ovl_layer, unsigned int l,
			     unsigned int session /*1:primary, 2:external, 3:memory */)
{
	return 0;
}

int dprec_mmp_dump_wdma_layer(void *wdma_layer, unsigned int wdma_num)
{
	return 0;
}

int dprec_mmp_dump_rdma_layer(void *rdma_layer, unsigned int rdma_num)
{
	return 0;
}

int dprec_logger_pr(unsigned int type, char *fmt, ...)
{
	return 0;
}

int dprec_logger_get_buf(enum DPREC_LOGGER_PR_TYPE type, char *stringbuf, int len)
{
	return 0;
}

ssize_t dprec_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
				const void *from, size_t available)
{
	loff_t pos = *ppos;
	size_t ret;

	if (pos < 0)
		return -EINVAL;

	ret = copy_to_user(to, from, available);
	if (ret == available)
		return -EFAULT;
	available -= ret;
	*ppos = pos + available;
	return available;
}


#endif
