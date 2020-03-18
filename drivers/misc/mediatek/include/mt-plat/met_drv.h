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

#ifndef MET_DRV
#define MET_DRV

#include <linux/version.h>
#include <linux/device.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/clk.h>

extern int met_mode;

#define MET_MODE_TRACE_CMD_OFFSET	(1)
#define MET_MODE_TRACE_CMD			(1<<MET_MODE_TRACE_CMD_OFFSET)

#define MET_STRBUF_SIZE		1024
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_nmi);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_irq);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf_sirq);
DECLARE_PER_CPU(char[MET_STRBUF_SIZE], met_strbuf);

#ifdef CONFIG_TRACING
#define TRACE_PUTS(p) \
	do { \
		trace_puts(p);; \
	} while (0)
#else
#define TRACE_PUTS(p) do {} while (0)
#endif

#define GET_MET_PRINTK_BUFFER_ENTER_CRITICAL() \
	({ \
		char *pmet_strbuf; \
		preempt_disable(); \
		if (in_nmi()) \
			pmet_strbuf = per_cpu(met_strbuf_nmi, smp_processor_id()); \
		else if (in_irq()) \
			pmet_strbuf = per_cpu(met_strbuf_irq, smp_processor_id()); \
		else if (in_softirq()) \
			pmet_strbuf = per_cpu(met_strbuf_sirq, smp_processor_id()); \
		else \
			pmet_strbuf = per_cpu(met_strbuf, smp_processor_id()); \
		pmet_strbuf;\
	})

#define PUT_MET_PRINTK_BUFFER_EXIT_CRITICAL(pmet_strbuf) \
	do {\
		if (pmet_strbuf)\
			TRACE_PUTS(pmet_strbuf); \
		preempt_enable_no_resched(); \
	} while (0)

#define MET_PRINTK(FORMAT, args...) \
	do { \
		char *pmet_strbuf; \
		preempt_disable(); \
		if (in_nmi()) \
			pmet_strbuf = per_cpu(met_strbuf_nmi, smp_processor_id()); \
		else if (in_irq()) \
			pmet_strbuf = per_cpu(met_strbuf_irq, smp_processor_id()); \
		else if (in_softirq()) \
			pmet_strbuf = per_cpu(met_strbuf_sirq, smp_processor_id()); \
		else \
			pmet_strbuf = per_cpu(met_strbuf, smp_processor_id()); \
		if (met_mode & MET_MODE_TRACE_CMD) \
			snprintf(pmet_strbuf, MET_STRBUF_SIZE, "%s: " FORMAT, __func__, ##args); \
		else \
			snprintf(pmet_strbuf, MET_STRBUF_SIZE, FORMAT, ##args); \
		TRACE_PUTS(pmet_strbuf); \
		preempt_enable_no_resched(); \
	} while (0)

/*
 * SOB: start of buf
 * EOB: end of buf
 */
#define MET_PRINTK_GETBUF(pSOB, pEOB) \
	({ \
		preempt_disable(); \
		if (in_nmi()) \
			*pSOB = per_cpu(met_strbuf_nmi, smp_processor_id()); \
		else if (in_irq()) \
			*pSOB = per_cpu(met_strbuf_irq, smp_processor_id()); \
		else if (in_softirq()) \
			*pSOB = per_cpu(met_strbuf_sirq, smp_processor_id()); \
		else \
			*pSOB = per_cpu(met_strbuf, smp_processor_id()); \
		*pEOB = *pSOB; \
		if (met_mode & MET_MODE_TRACE_CMD) \
			*pEOB += snprintf(*pEOB, MET_STRBUF_SIZE, "%s: ", __func__); \
	})

#define MET_PRINTK_PUTBUF(SOB, EOB) \
	({ \
		__trace_puts(_THIS_IP_, (SOB), (uintptr_t)((EOB)-(SOB))); \
		preempt_enable_no_resched(); \
	})

#define MET_FTRACE_PRINTK(TRACE_NAME, args...)			\
	do {							\
		trace_##TRACE_NAME(args);;			\
	} while (0)


#define MET_TYPE_PMU	1
#define MET_TYPE_BUS	2
#define MET_TYPE_MISC	3

struct metdevice {
	struct list_head list;
	int type;
	const char *name;
	struct module *owner;
	struct kobject *kobj;

	int (*create_subfs)(struct kobject *parent);
	void (*delete_subfs)(void);
	int mode;
	int ondiemet_mode; /* new for ondiemet; 1: call ondiemet functions */
	int cpu_related;
	int polling_interval;
	int polling_count_reload;
	int __percpu *polling_count;
	int header_read_again; /*for header size > 1 page*/
	void (*start)(void);
	void (*stop)(void);
	int (*reset)(void);
	void (*timed_polling)(unsigned long long stamp, int cpu);
	void (*tagged_polling)(unsigned long long stamp, int cpu);
	int (*print_help)(char *buf, int len);
	int (*print_header)(char *buf, int len);
	int (*process_argument)(const char *arg, int len);

	void (*ondiemet_start)(void);
	void (*ondiemet_stop)(void);
	int (*ondiemet_reset)(void);
	int (*ondiemet_print_help)(char *buf, int len);
	int (*ondiemet_print_header)(char *buf, int len);
	int (*ondiemet_process_argument)(const char *arg, int len);
	void (*ondiemet_timed_polling)(unsigned long long stamp, int cpu);
	void (*ondiemet_tagged_polling)(unsigned long long stamp, int cpu);

	struct list_head exlist;	/* for linked list before register */
	void (*suspend)(void);
	void (*resume)(void);

	volatile unsigned long long prev_stamp;
	spinlock_t my_lock;
	void *reversed1;
};

int met_register(struct metdevice *met);
int met_deregister(struct metdevice *met);
int met_set_platform(const char *plf_name, int flag);
int met_set_topology(const char *topology_name, int flag);
int met_devlink_add(struct metdevice *met);
int met_devlink_del(struct metdevice *met);
int met_devlink_register_all(void);
int met_devlink_deregister_all(void);

int fs_reg(void);
void fs_unreg(void);

/******************************************************************************
 * Tracepoints
 ******************************************************************************/
#define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(void *data, PARAMS(proto))
#define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name, NULL)
#define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name, NULL)


/* ====================== Tagging API ================================ */

#define MAX_EVENT_CLASS	31
#define MAX_TAGNAME_LEN	128
#define MET_CLASS_ALL	0x80000000

/* IOCTL commands of MET tagging */
typedef struct _mtag_cmd_t {
	unsigned int class_id;
	unsigned int value;
	unsigned int slen;
	char tname[MAX_TAGNAME_LEN];
	void *data;
	unsigned int size;
} mtag_cmd_t;

#define TYPE_START		1
#define TYPE_END		2
#define TYPE_ONESHOT	3
#define TYPE_ENABLE		4
#define TYPE_DISABLE	5
#define TYPE_REC_SET	6
#define TYPE_DUMP		7
#define TYPE_DUMP_SIZE	8
#define TYPE_DUMP_SAVE	9
#define TYPE_USRDATA	10
#define TYPE_DUMP_AGAIN		11
#define TYPE_ASYNC_START	12
#define TYPE_ASYNC_END	13

/* Use 'm' as magic number */
#define MTAG_IOC_MAGIC  'm'
/* Please use a different 8-bit number in your code */
#define MTAG_CMD_START		_IOW(MTAG_IOC_MAGIC, TYPE_START, mtag_cmd_t)
#define MTAG_CMD_END		_IOW(MTAG_IOC_MAGIC, TYPE_END, mtag_cmd_t)
#define MTAG_CMD_ONESHOT	_IOW(MTAG_IOC_MAGIC, TYPE_ONESHOT, mtag_cmd_t)
#define MTAG_CMD_ENABLE		_IOW(MTAG_IOC_MAGIC, TYPE_ENABLE, int)
#define MTAG_CMD_DISABLE	_IOW(MTAG_IOC_MAGIC, TYPE_DISABLE, int)
#define MTAG_CMD_REC_SET	_IOW(MTAG_IOC_MAGIC, TYPE_REC_SET, int)
#define MTAG_CMD_DUMP		_IOW(MTAG_IOC_MAGIC, TYPE_DUMP, mtag_cmd_t)
#define MTAG_CMD_DUMP_SIZE	_IOWR(MTAG_IOC_MAGIC, TYPE_DUMP_SIZE, int)
#define MTAG_CMD_DUMP_SAVE	_IOW(MTAG_IOC_MAGIC, TYPE_DUMP_SAVE, mtag_cmd_t)
#define MTAG_CMD_USRDATA	_IOW(MTAG_IOC_MAGIC, TYPE_USRDATA, mtag_cmd_t)
#define MTAG_CMD_DUMP_AGAIN	_IOW(MTAG_IOC_MAGIC, TYPE_DUMP_AGAIN, void *)
#define MTAG_CMD_ASYNC_START		_IOW(MTAG_IOC_MAGIC, TYPE_ASYNC_START, mtag_cmd_t)
#define MTAG_CMD_ASYNC_END		_IOW(MTAG_IOC_MAGIC, TYPE_ASYNC_END, mtag_cmd_t)

/* include file */
#ifndef MET_USER_EVENT_SUPPORT
#define met_tag_init() ({ 0; })

#define met_tag_uninit() ({ 0; })

#define met_tag_start(id, name) ({ 0; })

#define met_tag_end(id, name) ({ 0; })

#define met_tag_async_start(id, name, cookie) ({0; })

#define met_tag_async_end(id, name, cookie) ({0; })

#define met_tag_oneshot(id, name, value) ({ 0; })

#define met_tag_userdata(pData) ({ 0; })

#define met_tag_dump(id, name, data, length) ({ 0; })

#define met_tag_disable(id) ({ 0; })

#define met_tag_enable(id) ({ 0; })

#define met_set_dump_buffer(size) ({ 0; })

#define met_save_dump_buffer(pathname) ({ 0; })

#define met_save_log(pathname) ({ 0; })

#define met_record_on() ({ 0; })

#define met_record_off() ({ 0; })

#define met_show_bw_limiter() ({ 0; })
#define met_reg_bw_limiter() ({ 0; })
#define met_show_clk_tree() ({ 0; })
#define met_reg_clk_tree() ({ 0; })
#define met_ccf_clk_enable(clk) ({ 0; })
#define met_ccf_clk_disable(clk) ({ 0; })
#define met_ccf_clk_set_rate(clk, top) ({ 0; })
#define met_ccf_clk_set_parent(clk, parent) ({ 0; })
#define met_fh_print_dds(pll_id, dds_value) ({ 0; })

#define enable_met_backlight_tag() ({ 0; })
#define output_met_backlight_tag(level) ({ 0; })
#define met_show_pmic_info(RegNum, pmic_reg) ({ 0; })

#else
#include <linux/kernel.h>
int __attribute__((weak)) met_tag_init(void);

int __attribute__((weak)) met_tag_uninit(void);

int __attribute__((weak)) met_tag_start(unsigned int class_id,
					const char *name);

int __attribute__((weak)) met_tag_end(unsigned int class_id,
					const char *name);

int __attribute__((weak)) met_tag_async_start(unsigned int class_id,
					const char *name,
					unsigned int cookie);

int __attribute__((weak)) met_tag_async_end(unsigned int class_id,
					const char *name,
					unsigned int cookie);

int __attribute__((weak)) met_tag_oneshot(unsigned int class_id,
					const char *name,
					unsigned int value);

int met_tag_userdata(char *pData);

int __attribute__((weak)) met_tag_dump(unsigned int class_id,
					const char *name,
					void *data,
					unsigned int length);

int __attribute__((weak)) met_tag_disable(unsigned int class_id);

int __attribute__((weak)) met_tag_enable(unsigned int class_id);

int __attribute__((weak)) met_set_dump_buffer(int size);

int __attribute__((weak)) met_save_dump_buffer(const char *pathname);

int __attribute__((weak)) met_save_log(const char *pathname);

int __attribute__((weak)) met_show_bw_limiter(void);
int __attribute__((weak)) met_reg_bw_limiter(void *fp);
int __attribute__((weak)) met_show_clk_tree(const char *name,
			unsigned int addr,
			unsigned int status);
int __attribute__((weak)) met_reg_clk_tree(void *fp);

int __attribute__((weak)) met_ccf_clk_enable(struct clk *clk);
int __attribute__((weak)) met_ccf_clk_disable(struct clk *clk);
int __attribute__((weak)) met_ccf_clk_set_rate(struct clk *clk, struct clk *top);
int __attribute__((weak)) met_ccf_clk_set_parent(struct clk *clk, struct clk *parent);

extern unsigned int __attribute__((weak)) met_fh_dds[];
int __attribute__((weak)) met_fh_print_dds(int pll_id, unsigned int dds_value);

int __attribute__((weak)) enable_met_backlight_tag(void);
int __attribute__((weak)) output_met_backlight_tag(int level);

void __attribute__((weak)) met_show_pmic_info(unsigned int RegNum, unsigned int pmic_reg);

#define met_record_on()		tracing_on()

#define met_record_off()	tracing_off()

#endif				/* MET_USER_EVENT_SUPPORT */



/*
 * Wrapper for DISP/MDP/GCE mmsys profiling
 */

void __attribute__((weak)) met_mmsys_event_gce_thread_begin(ulong thread_no, ulong task_handle, ulong engineFlag,
								void *pCmd, ulong size);
void __attribute__((weak)) met_mmsys_event_gce_thread_end(ulong thread_no, ulong task_handle, ulong engineFlag);

void __attribute__((weak)) met_mmsys_event_disp_sof(int mutex_id);
void __attribute__((weak)) met_mmsys_event_disp_mutex_eof(int mutex_id);
void __attribute__((weak)) met_mmsys_event_disp_ovl_eof(int ovl_id);

void __attribute__((weak)) met_mmsys_config_isp_base_addr(unsigned long *isp_reg_list);
void __attribute__((weak)) met_mmsys_event_isp_pass1_begin(int sensor_id);
void __attribute__((weak)) met_mmsys_event_isp_pass1_end(int sensor_id);

/* ====================== SPO API ================================ */
enum MET_SPO_MEM_MODULE {
	MSMM_ISP_P1_IMGO,
	MSMM_ISP_P1_RRZO,
	MSMM_ISP_P2_IMGI,

	MSMM_MDP_RDMA0,
	MSMM_MDP_RDMA1,
	MSMM_MDP_WDMA0,
	MSMM_MDP_WDMA1,

	MSMM_CODEC_READ,
	MSMM_CODE_WRITE,

	MSMM_DISP_RDMA0,
	MSMM_DISP_RDMA1,
	MSMM_DISP_WDMA0,
	MSMM_DISP_WDMA1,


	/*Base index of OVL layers, use param1 to assign layer offset*/
	MSMM_DISP_OVL_L_0,
	MSMM_DISP_OVL_L_1,
	MSMM_DISP_OVL_L_2,
	MSMM_DISP_OVL_L_3,

	MSMM_DISP_OVL_L_4,
	MSMM_DISP_OVL_L_5,
	MSMM_DISP_OVL_L_6,
	MSMM_DISP_OVL_L_7,

	MSMM_DISP_OVL_L_8,
	MSMM_DISP_OVL_L_9,
	MSMM_DISP_OVL_L_10,
	MSMM_DISP_OVL_L_11,

	MSMM_DISP_OVL_L_12,
	MSMM_DISP_OVL_L_13,
	MSMM_DISP_OVL_L_14,
	MSMM_DISP_OVL_L_15,


};

enum MET_SPO_MEM_ACCESS_TYPE {
	MSMA_READ = 0,
	MSMA_WRITE
};



void __attribute__((weak)) _met_spo_mem_decl(enum MET_SPO_MEM_MODULE mod,
		enum MET_SPO_MEM_ACCESS_TYPE type,
		unsigned long memory_bw_in_bit, unsigned long estimated_exe_time_in_us,
		unsigned long param1, unsigned long param2);

void __attribute__((weak)) _met_spo_mem_alloc(enum MET_SPO_MEM_MODULE mod,
		unsigned long param1, unsigned long param2);
void __attribute__((weak)) _met_spo_mem_free(enum MET_SPO_MEM_MODULE mod,
		unsigned long param1, unsigned long param2);

/*invoke before SOF*/
#define met_spo_mem_decl(...)			\
do {						\
	if (_met_spo_mem_decl)			\
		_met_spo_mem_decl(__VA_ARGS__);	\
} while (0)

/*invoke when SOF*/
#define met_spo_mem_alloc(...)			\
do {						\
	if (_met_spo_mem_alloc)			\
		_met_spo_mem_alloc(__VA_ARGS__);\
} while (0)

/*invoke when EOF*/
#define met_spo_mem_free(...)			\
do {						\
	if (_met_spo_mem_free)			\
		_met_spo_mem_free(__VA_ARGS__);	\
} while (0)



/* =============== MMSYS API  Wrapper for DISP/MDP/GCE mmsys profiling ========= */
void __attribute__((weak)) met_mmsys_event_gce_thread_begin(ulong thread_no, ulong task_handle, ulong engineFlag,
								void *pCmd, ulong size);
void __attribute__((weak)) met_mmsys_event_gce_thread_end(ulong thread_no, ulong task_handle, ulong engineFlag);

void __attribute__((weak)) met_mmsys_event_disp_sof(int mutex_id);
void __attribute__((weak)) met_mmsys_event_disp_mutex_eof(int mutex_id);
void __attribute__((weak)) met_mmsys_event_disp_ovl_eof(int ovl_id);

void __attribute__((weak)) met_mmsys_config_isp_base_addr(unsigned long *isp_reg_list);
void __attribute__((weak)) met_mmsys_event_isp_pass1_begin(int sensor_id);
void __attribute__((weak)) met_mmsys_event_isp_pass1_end(int sensor_id);


/* ====================== MMSYS Platform Depend ================================ */
#if defined(CONFIG_MTK_MET)
	#if defined(CONFIG_MACH_MT6757)
		#pragma message("MET MMSYS 6757 include")
		#include "../met/mt6757/platform/mt6757/met_drv_udtl.h"
	#elif defined(CONFIG_MACH_MT6799)
		#pragma message("MET MMSYS 6799 include")
		#include "../met/mt6799/platform/mt6799/met_drv_udtl.h"
	#else
		#pragma message("MET MMSYS include not found!")
		#include "met_drv_udtl_null.h"
	#endif
#else
	#pragma message("MET MMSYS Null include")
	#include "met_drv_udtl_null.h"
#endif






#endif				/* MET_DRV */
