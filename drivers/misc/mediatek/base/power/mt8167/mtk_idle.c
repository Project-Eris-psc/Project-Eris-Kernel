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

#define _MT_IDLE_C

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/tick.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>

#define IDLE_HAVE_DPIDLE	1
#define IDLE_HAVE_SLIDLE	1
#define IDLE_HAVE_SODI		1
#define IDLE_HAVE_MCDI		0
#define IDLE_HAVE_CPUFREQ	0
#define IDLE_HAVE_THERMAL	1
#define IDLE_HAVE_GPUDVFS	0
#define IDLE_HAVE_CPUXGPT	0
#define IDLE_HAVE_DCM		0
#define IDLE_HAVE_PTP		0
#define IDLE_HAVE_STD_TIMER	1
#define IDLE_EN_LOG_DBG		0
#define IDLE_CND_MTCMOS		1

#include "mtk_spm.h"
#include "mtk_spm_idle.h"
#include "mtk_spm_sleep.h"

#if !IDLE_HAVE_STD_TIMER
#include <mt-plat/mt_gpt.h>
#endif

#if IDLE_HAVE_CPUXGPT
#include <mach/mt_cpuxgpt.h>
#endif

#if IDLE_HAVE_GPUDVFS
#include <linux/mtk_gpu_utility.h>
#endif

#if IDLE_HAVE_CPUFREQ
#include "mt_cpufreq.h"
#endif

#if IDLE_HAVE_THERMAL
#include <mach/mtk_thermal.h>
#endif

#if IDLE_HAVE_PTP
#include <mach/mt_ptp.h>
#endif

#include "mtk_idle.h"

#if IDLE_HAVE_DCM
#include "mt_dcm.h"
#endif

#define AUDINTBUS_CLOCK_SWITCH	1

/* MCDI/SODI DVT Test */
#define MCDI_DVT_IPI 0 /* 0:disable, 1:enable : mt_idle.c, mt_spm_mcdi.c */
#define MCDI_DVT_CPUxGPT 0 /* 0:disable, 1:enable : mt_idle.c, mt_spm_mcdi.c */

#define SODI_DVT_APxGPT 0 /* 0:disable, 1:enable : mt_idle.c, mt_spm_sodi.c */
#define SODI_DVT_APxGPT_TimerCount_5S 0

#if MCDI_DVT_IPI || MCDI_DVT_CPUxGPT
#include <linux/delay.h>
#endif

#define IDLE_TAG	"[Power/idle] "

#define idle_err(fmt, args...)	pr_err(IDLE_TAG fmt, ##args)
#define idle_warn(fmt, args...)	pr_warn(IDLE_TAG fmt, ##args)

#if IDLE_EN_LOG_DBG
#define idle_info(fmt, args...)	pr_debug(IDLE_TAG fmt, ##args)
#define idle_dbg(fmt, args...)	pr_debug(IDLE_TAG fmt, ##args)
#define idle_ver(fmt, args...)	pr_debug(IDLE_TAG fmt, ##args)
#else /* !IDLE_EN_LOG_DBG */
#define idle_info(fmt, args...)	do {} while (0)
#define idle_dbg(fmt, args...)	do {} while (0)
#define idle_ver(fmt, args...)	do {} while (0)
#endif /* IDLE_EN_LOG_DBG */

#define idle_gpt GPT4

#define idle_readl(addr)	readl(addr)
#define idle_writel(addr, val)	writel(val, addr)
#define idle_setl(addr, val)	idle_writel(addr, idle_readl(addr) | (val))
#define idle_clrl(addr, val)	idle_writel(addr, idle_readl(addr) & ~(val))

static unsigned long rgidle_cnt[NR_CPUS] = {0};

enum {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO = 1,
	IDLE_TYPE_MC = 2,
	IDLE_TYPE_SL = 3,
	IDLE_TYPE_RG = 4,
	NR_TYPES = 5,
};

enum {
	BY_CPU = 0,
	BY_CLK = 1,
	BY_TMR = 2,
	BY_OTH = 3,
	BY_VTG = 4,
	NR_REASONS = 5
};

/* Idle handler on/off */
static int idle_switch[NR_TYPES] = {
	1,			/* dpidle switch */
	1,			/* soidle switch */
	0,			/* mcidle switch */
	0,			/* slidle switch */
	1,			/* rgidle switch */
};

static u32 dpidle_condition_mask[NR_GRPS] = {
	0x00000037, /* CG_CTRL0: */
	0x808FB2FC, /* CG_CTRL1: */
	0x017F7F17, /* CG_CTRL2: */
	0x000430FD, /* CG_CTRL8: */
	0x000F0203, /* CG_MMSYS0: */
	0x003FC03C, /* CG_MMSYS1: */
	0x000003E1, /* CG_IMGSYS: */
	0x00000004, /* CG_MFGSYS: */
	0x00000000, /* CG_AUDIO: */
	0x00000001, /* CG_VDEC0: */
	0x00000001, /* CG_VDEC1: */
};

#if IDLE_HAVE_SODI
static u32 soidle_condition_mask[NR_GRPS] = {
	0x00000026, /* CG_CTRL0: */
	0x808FB2F8, /* CG_CTRL1: */
	0x017F7F07, /* CG_CTRL2: */
	0x000430ED, /* CG_CTRL8: */
	0x00000200, /* CG_MMSYS0: */
	0x003F0000, /* CG_MMSYS1: */
	0x000003E1, /* CG_IMGSYS: */
	0x00000004, /* CG_MFGSYS: */
	0x00000000, /* CG_AUDIO: */
	0x00000001, /* CG_VDEC0: */
	0x00000001, /* CG_VDEC1: */
};
#endif /* IDLE_HAVE_SODI */

static u32 slidle_condition_mask[NR_GRPS] = {
	0xFFFFFFFF, /* CG_CTRL0: */
	0xFFFFFFFF, /* CG_CTRL1: */
	0xFFFFFFFF, /* CG_CTRL2: */
	0xFFFFFFFF, /* CG_CTRL8: */
	0xFFFFFFFF, /* CG_MMSYS0: */
	0xFFFFFFFF, /* CG_MMSYS1: */
	0xFFFFFFFF, /* CG_IMGSYS: */
	0xFFFFFFFF, /* CG_MFGSYS: */
	0xFFFFFFFF, /* CG_AUDIO: */
	0xFFFFFFFF, /* CG_VDEC0: */
	0xFFFFFFFF, /* CG_VDEC1: */
};

static const char *idle_name[NR_TYPES] = {
	"dpidle",
	"soidle",
	"mcidle",
	"slidle",
	"rgidle",
};

static const char *reason_name[NR_REASONS] = {
	"by_cpu",
	"by_clk",
	"by_tmr",
	"by_oth",
	"by_vtg",
};

/* Slow Idle */
static u32		slidle_block_mask[NR_GRPS];
static unsigned long	slidle_cnt[NR_CPUS];
static unsigned long	slidle_block_cnt[NR_REASONS];
static u32		slidle_bypass_cnd;

/* SODI */
#if IDLE_HAVE_SODI
static u32		soidle_block_mask[NR_GRPS];
#endif
#if IDLE_HAVE_STD_TIMER
static u32		soidle_time_critera = 2000; /* 2ms */
#else /* !IDLE_HAVE_STD_TIMER */
static u32		soidle_timer_left;
static u32		soidle_timer_left2;
#ifndef CONFIG_SMP
static u32		soidle_timer_cmp;
#endif
static u32		soidle_time_critera = 26000; /* FIXME: need fine tune */
#endif /* IDLE_HAVE_STD_TIMER */
static unsigned long	soidle_cnt[NR_CPUS];
static unsigned long	soidle_block_cnt[NR_CPUS][NR_REASONS];

/* DeepIdle */
static u32		dpidle_block_mask[NR_GRPS];
#if IDLE_HAVE_STD_TIMER
static u32		dpidle_time_critera = 2000; /* 2ms */
#else /* !IDLE_HAVE_STD_TIMER */
static u32		dpidle_timer_left;
static u32		dpidle_timer_left2;
#ifndef CONFIG_SMP
static u32		dpidle_timer_cmp;
#endif
static u32		dpidle_time_critera = 26000;
#endif /* IDLE_HAVE_STD_TIMER */
static u64		dpidle_block_time_critera = 30000; /* default 30sec */
static unsigned long	dpidle_cnt[NR_CPUS];
static unsigned long	dpidle_block_cnt[NR_REASONS];
static u64		dpidle_block_prev_time;
static u32		dpidle_bypass_cnd;

/* MCDI */
#if IDLE_HAVE_STD_TIMER
static u32		mcidle_time_critera = 3000; /* 3ms */
#else /* !IDLE_HAVE_STD_TIMER */
static u32		mcidle_timer_left[NR_CPUS];
static u32		mcidle_timer_left2[NR_CPUS];
static u32		mcidle_time_critera = 39000; /* 3ms */
#endif /* IDLE_HAVE_STD_TIMER */
static unsigned long	mcidle_cnt[NR_CPUS];
static unsigned long	mcidle_block_cnt[NR_CPUS][NR_REASONS];
u64			mcidle_timer_before_wfi[NR_CPUS];
static unsigned int	idle_spm_lock;

static void __iomem *topckgen_base;	/* 0x10000000 */
static void __iomem *scpsys_base;	/* 0x10006000 */
static void __iomem *audiosys_base;	/* 0x11220000 */
static void __iomem *mfgsys_base;	/* 0x13000000 */
static void __iomem *mmsys_base;	/* 0x14000000 */
static void __iomem *imgsys_base;	/* 0x15000000 */
static void __iomem *vdecsys_base;	/* 0x16000000 */

#define TOPCKGEN_REG(ofs)	(topckgen_base + ofs)
#define SCP_REG(ofs)		(scpsys_base + ofs)
#define AUDIO_REG(ofs)		(audiosys_base + ofs)
#define MFG_REG(ofs)		(mfgsys_base + ofs)
#define MM_REG(ofs)		(mmsys_base + ofs)
#define IMG_REG(ofs)		(imgsys_base + ofs)
#define VDEC_REG(ofs)		(vdecsys_base + ofs)

#ifdef SPM_PWR_STATUS
#undef SPM_PWR_STATUS
#endif

#ifdef SPM_PWR_STATUS_2ND
#undef SPM_PWR_STATUS_2ND
#endif

#define CLK_MUX_SEL0			TOPCKGEN_REG(0x00)
#define CLK_GATING_CTRL0		TOPCKGEN_REG(0x20)
#define CLK_GATING_CTRL1		TOPCKGEN_REG(0x24)
#define CLK_GATING_CTRL2		TOPCKGEN_REG(0x3c)
#define CLK_GATING_CTRL8		TOPCKGEN_REG(0x70)
#define SPM_PWR_STATUS			SCP_REG(0x60c)
#define SPM_PWR_STATUS_2ND		SCP_REG(0x610)
#define AUDIO_TOP_CON0			AUDIO_REG(0x00)
#define MFG_CG_CON			MFG_REG(0x0)
#define MMSYS_CG_CON0			MM_REG(0x100)
#define MMSYS_CG_CON1			MM_REG(0x110)
#define IMG_CG_CON			IMG_REG(0x0)
#define VDEC_CKEN_SET			VDEC_REG(0x0)
#define LARB_CKEN_SET			VDEC_REG(0x8)

#define PWR_STA_CONN		BIT(1)
#define PWR_STA_DIS		BIT(3)
#define PWR_STA_MFG_3D		BIT(4)
#define PWR_STA_ISP		BIT(5)
#define PWR_STA_VDE		BIT(7)
#define PWR_STA_CPU		BIT(9)
#define PWR_STA_FC3		BIT(10)
#define PWR_STA_FC2		BIT(11)
#define PWR_STA_FC1		BIT(12)
#define PWR_STA_FC0		BIT(13)
#define PWR_STA_MCU		BIT(15)
#define PWR_STA_MFG_2D		BIT(24)
#define PWR_STA_MFG_ASYNC	BIT(25)

int __attribute__((weak)) gpt_check_and_ack_irq(unsigned int id)
{
	return 0;
}

int __attribute__((weak)) gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	return 0;
}

int __attribute__((weak)) gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	return 0;
}

int __attribute__((weak)) gpt_set_cmp(unsigned int id, unsigned int val)
{
	return 0;
}

int __attribute__((weak)) request_gpt(unsigned int id, unsigned int mode,
		unsigned int clksrc, unsigned int clkdiv, unsigned int cmp,
		void (*func)(unsigned long), unsigned int flags)
{
	return 0;
}

int __attribute__((weak)) start_gpt(unsigned int id)
{
	return 0;
}

int __attribute__((weak)) stop_gpt(unsigned int id)
{
	return 0;
}

#if !IDLE_HAVE_STD_TIMER

unsigned int __attribute__((weak)) localtimer_get_counter(void)
{
	return 0;
}

int __attribute__((weak)) localtimer_set_next_event(unsigned int evt)
{
	return 0;
}

#endif /* !IDLE_HAVE_STD_TIMER */

u64 __attribute__((weak)) localtimer_get_phy_count(void)
{
	return 0;
}

int __attribute__((weak)) cpu_xgpt_set_cmp(int cpuxgpt_num, u64 count)
{
	return 0;
}

unsigned int __attribute__((weak)) cpu_xgpt_irq_dis(int cpuxgpt_num)
{
	return 0;
}

int __attribute__((weak)) cpu_xgpt_register_timer(unsigned int id,
				irqreturn_t (*func)(int irq, void *dev_id))
{
	return 0;
}

#if IDLE_HAVE_CPUFREQ

bool __attribute__((weak)) mt_cpufreq_earlysuspend_status_get(void)
{
	return false;
}

#endif

#if !IDLE_HAVE_DCM

void __attribute__((weak)) bus_dcm_enable(void)
{
}

void __attribute__((weak)) bus_dcm_disable(void)
{
}

#endif /* !IDLE_HAVE_DCM */

static bool is_valid_reg(void __iomem *addr)
{
#ifdef CONFIG_ARM64
	return ((u64)addr & 0xf0000000) || (((u64)addr >> 32) & 0xf0000000);
#else
	return (u32)addr & 0xf0000000;
#endif
}

unsigned int __attribute__((weak)) spm_get_cpu_pwr_status(void)
{
	u32 val[2] = {0};
	u32 stat = 0;
	u32 cpu_mask = (PWR_STA_FC0 | PWR_STA_FC1 |
			PWR_STA_FC2 | PWR_STA_FC3);

	if (!is_valid_reg(scpsys_base))
		return 0;

	val[0] = idle_readl(SPM_PWR_STATUS);
	val[1] = idle_readl(SPM_PWR_STATUS_2ND);

	stat = val[0] & cpu_mask;
	stat &= val[1] & cpu_mask;

	return stat;
}

enum subsys_id {
	SYS_CONN,
	SYS_DIS,
	SYS_MFG_3D,
	SYS_ISP,
	SYS_VDE,
	SYS_MFG_2D,
	SYS_MFG_ASYNC,
	NR_SYSS,
};

static int sys_is_on(enum subsys_id id)
{
	u32 pwr_sta_mask[] = {
		PWR_STA_CONN,
		PWR_STA_DIS,
		PWR_STA_MFG_3D,
		PWR_STA_ISP,
		PWR_STA_VDE,
		PWR_STA_MFG_2D,
		PWR_STA_MFG_ASYNC,
	};

	u32 mask, sta, sta_s;

	if (!is_valid_reg(scpsys_base))
		return 0;

	mask = pwr_sta_mask[id];
	sta = idle_readl(SPM_PWR_STATUS);
	sta_s = idle_readl(SPM_PWR_STATUS_2ND);

	return (sta & mask) && (sta_s & mask);
}

static void get_all_clock_state(u32 clks[NR_GRPS])
{
	int i;

	for (i = 0; i < NR_GRPS; i++)
		clks[i] = 0;

	if (is_valid_reg(topckgen_base)) {
		clks[CG_CTRL0] = ~idle_readl(CLK_GATING_CTRL0) ^ BIT(4);
		clks[CG_CTRL1] = ~idle_readl(CLK_GATING_CTRL1);
		clks[CG_CTRL2] = ~idle_readl(CLK_GATING_CTRL2);
		clks[CG_CTRL8] = ~idle_readl(CLK_GATING_CTRL8);
	}

	if (sys_is_on(SYS_DIS) && is_valid_reg(mmsys_base)) {
		clks[CG_MMSYS0] = ~idle_readl(MMSYS_CG_CON0);
		clks[CG_MMSYS1] = ~idle_readl(MMSYS_CG_CON1);
	}

	if (is_valid_reg(audiosys_base))
		clks[CG_AUDIO] = ~idle_readl(AUDIO_TOP_CON0);

#if IDLE_CND_MTCMOS
	if (sys_is_on(SYS_ISP))
		clks[CG_IMGSYS] = 0xffffffff;

	if (sys_is_on(SYS_MFG_ASYNC) && sys_is_on(SYS_MFG_2D) &&
			sys_is_on(SYS_MFG_3D))
		clks[CG_MFGSYS] = 0xffffffff;

	if (sys_is_on(SYS_VDE)) {
		clks[CG_VDEC0] = 0xffffffff;
		clks[CG_VDEC1] = 0xffffffff;
	}
#else
	if (sys_is_on(SYS_ISP) && is_valid_reg(imgsys_base))
		clks[CG_IMGSYS] = ~idle_readl(IMG_CG_CON);

	if (sys_is_on(SYS_MFG_ASYNC) && sys_is_on(SYS_MFG_2D) &&
			sys_is_on(SYS_MFG_3D) && is_valid_reg(mfgsys_base))
		clks[CG_MFGSYS] = ~idle_readl(MFG_CG_CON);

	if (sys_is_on(SYS_VDE) && is_valid_reg(vdecsys_base)) {
		clks[CG_VDEC0] = idle_readl(VDEC_CKEN_SET);
		clks[CG_VDEC1] = idle_readl(LARB_CKEN_SET);
	}
#endif
}

static bool clkmgr_idle_can_enter(u32 *condition_mask, u32 *block_mask)
{
	int i;
	u32 clks[NR_GRPS];
	u32 r = 0;

	get_all_clock_state(clks);

	for (i = 0; i < NR_GRPS; i++) {
		block_mask[i] = condition_mask[i] & clks[i];
		r |= block_mask[i];
	}

	return r == 0;
}

static const char *grp_get_name(int id)
{
	static const char * const grp_name[] = {
		"CG_CTRL0",
		"CG_CTRL1",
		"CG_CTRL2",
		"CG_CTRL8",
		"CG_MMSYS0",
		"CG_MMSYS1",
		"CG_IMGSYS",
		"CG_MFGSYS",
		"CG_AUDIO",
		"CG_VDEC0",
		"CG_VDEC1",
	};

	return grp_name[id];
}

#define INVALID_GRP_ID(grp) (grp < 0 || grp >= NR_GRPS)

#if !IDLE_HAVE_STD_TIMER

static int idle_gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cnt(id, val);
	*ptr = val[0];

	return ret;
}

static int idle_gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cmp(id, val);
	*ptr = val[0];

	return ret;
}

#endif /* !IDLE_HAVE_STD_TIMER */

static u64 idle_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

static DEFINE_SPINLOCK(idle_spm_spin_lock);

void idle_lock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock |= (1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

void idle_unlock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock &= ~(1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

static int only_one_cpu_online(void)
{
#ifdef CONFIG_SMP
	unsigned int ps = spm_get_cpu_pwr_status();

	return (ps == PWR_STA_FC3) || (ps == PWR_STA_FC2) ||
		(ps == PWR_STA_FC1) || (ps == PWR_STA_FC0);
#else /* !CONFIG_SMP */
	return true;
#endif /* CONFIG_SMP */
}

/************************************************
 * SODI part
 ************************************************/
static DEFINE_MUTEX(soidle_locked);

static void enable_soidle_by_mask(int grp, unsigned int mask)
{
#if IDLE_HAVE_SODI
	mutex_lock(&soidle_locked);
	soidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&soidle_locked);
#endif /* IDLE_HAVE_SODI */
}

static void disable_soidle_by_mask(int grp, unsigned int mask)
{
#if IDLE_HAVE_SODI
	mutex_lock(&soidle_locked);
	soidle_condition_mask[grp] |= mask;
	mutex_unlock(&soidle_locked);
#endif /* IDLE_HAVE_SODI */
}

void enable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = BIT(id % 32);

	if (WARN_ON(INVALID_GRP_ID(grp)))
		return;

	enable_soidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_soidle_by_bit);

void disable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = BIT(id % 32);

	if (WARN_ON(INVALID_GRP_ID(grp)))
		return;

	disable_soidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_soidle_by_bit);

bool soidle_can_enter(int cpu)
{
	int reason = NR_REASONS;
#if IDLE_HAVE_STD_TIMER
	struct timespec t;
	u32 expected_us;
#endif

	if (!only_one_cpu_online()) {
		reason = BY_CPU;
		goto out;
	}

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto out;
	}

#if !SODI_DVT_APxGPT
#if IDLE_HAVE_SODI
	if (spm_get_sodi_en() == 0) {
		reason = BY_OTH;
		goto out;
	}

	memset(soidle_block_mask, 0, sizeof(soidle_block_mask));
	if (!clkmgr_idle_can_enter(soidle_condition_mask, soidle_block_mask)) {
		reason = BY_CLK;
		goto out;
	}
#endif /* IDLE_HAVE_SODI */
#endif

#if IDLE_HAVE_STD_TIMER
	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	expected_us = t.tv_sec * USEC_PER_SEC + t.tv_nsec / NSEC_PER_USEC;
	if (expected_us < soidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#else /* !IDLE_HAVE_STD_TIMER */
#ifdef CONFIG_SMP
	soidle_timer_left = localtimer_get_counter();
	if (soidle_timer_left < soidle_time_critera ||
			(s32)soidle_timer_left < 0) {
		reason = BY_TMR;
		goto out;
	}
#else
	idle_gpt_get_cnt(GPT1, &soidle_timer_left);
	idle_gpt_get_cmp(GPT1, &soidle_timer_cmp);
	if ((soidle_timer_cmp - soidle_timer_left) < soidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#endif
#endif /* IDLE_HAVE_STD_TIMER */

out:
	if (reason < NR_REASONS) {
		soidle_block_cnt[cpu][reason]++;
		return false;
	} else
		return true;
}

void soidle_before_wfi(int cpu)
{
#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	#if SODI_DVT_APxGPT && SODI_DVT_APxGPT_TimerCount_5S
	soidle_timer_left2 = 65000000;
	#else
	soidle_timer_left2 = localtimer_get_counter();
	#endif

	if ((s32)soidle_timer_left2 <= 0)
		gpt_set_cmp(idle_gpt, 1); /* Trigger idle_gpt Timerout */
	else
		gpt_set_cmp(idle_gpt, soidle_timer_left2);

	start_gpt(idle_gpt);
#else
	idle_gpt_get_cnt(GPT1, &soidle_timer_left2);
#endif
#endif /* !IDLE_HAVE_STD_TIMER */
}

void soidle_after_wfi(int cpu)
{
#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(idle_gpt))
		localtimer_set_next_event(1);
	else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;

		idle_gpt_get_cnt(idle_gpt, &cnt);
		idle_gpt_get_cmp(idle_gpt, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n",
					__func__, idle_gpt + 1, cnt, cmp);
			cmp = cnt;
			WARN_ON(1);
		}

		localtimer_set_next_event(cmp - cnt);
		stop_gpt(idle_gpt);
	}
#endif
#endif /* !IDLE_HAVE_STD_TIMER */

#if SODI_DVT_APxGPT
	idle_info("soidle_cnt:%d\n", soidle_cnt[cpu]);
#endif
	soidle_cnt[cpu]++;
}

/************************************************
 * multi-core idle part
 ************************************************/
static DEFINE_MUTEX(mcidle_locked);
bool mcidle_can_enter(int cpu)
{
	int reason = NR_REASONS;
#if IDLE_HAVE_STD_TIMER
	struct timespec t;
	u32 expected_us;
#endif

	if (cpu == 0 || topology_physical_package_id(cpu) == 1) {
		reason = BY_VTG;
		goto mcidle_out;
	}

	if (idle_spm_lock) {
		reason = BY_VTG;
		goto mcidle_out;
	}

	if (only_one_cpu_online()) {
		reason = BY_CPU;
		goto mcidle_out;
	}

#if IDLE_HAVE_MCDI
	if (spm_mcdi_can_enter() == 0) {
		reason = BY_OTH;
		goto mcidle_out;
	}
#endif /* IDLE_HAVE_MCDI */

#if IDLE_HAVE_STD_TIMER
	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	expected_us = t.tv_sec * USEC_PER_SEC + t.tv_nsec / NSEC_PER_USEC;
	if (expected_us < mcidle_time_critera) {
		reason = BY_TMR;
		goto mcidle_out;
	}
#else /* !IDLE_HAVE_STD_TIMER */
	#if !MCDI_DVT_IPI && !MCDI_DVT_CPUxGPT
	mcidle_timer_left[cpu] = localtimer_get_counter();
	if (mcidle_timer_left[cpu] < mcidle_time_critera ||
			(s32)mcidle_timer_left[cpu] < 0) {
		reason = BY_TMR;
		goto mcidle_out;
	}
	#endif
#endif /* IDLE_HAVE_STD_TIMER */

mcidle_out:
	if (reason < NR_REASONS) {
		mcidle_block_cnt[cpu][reason]++;
		return false;
	} else
		return true;
}

#if !IDLE_HAVE_STD_TIMER
bool spm_mcdi_xgpt_timeout[NR_CPUS];
#endif

void mcidle_before_wfi(int cpu)
{
#if !IDLE_HAVE_STD_TIMER
#if !MCDI_DVT_IPI
	u64 set_count = 0;

	spm_mcdi_xgpt_timeout[cpu] = 0;

	#if MCDI_DVT_CPUxGPT
	localtimer_set_next_event(130000000);
	mcidle_timer_left2[cpu] = 65000000;
	#else
	mcidle_timer_left2[cpu] = localtimer_get_counter();
	#endif
	mcidle_timer_before_wfi[cpu] = localtimer_get_phy_count();

	set_count = mcidle_timer_before_wfi[cpu] + (s32)mcidle_timer_left2[cpu];

	cpu_xgpt_set_cmp(cpu, set_count);
#endif
#endif /* !IDLE_HAVE_STD_TIMER */
}

int mcdi_xgpt_wakeup_cnt[NR_CPUS];

void mcidle_after_wfi(int cpu)
{
#if !IDLE_HAVE_STD_TIMER
#if !MCDI_DVT_IPI
	u64 cmp;

	cpu_xgpt_irq_dis(cpu); /* ack cpuxgpt, api need refine from Weiqi */
#if !MCDI_DVT_CPUxGPT
	cmp = (localtimer_get_phy_count() - mcidle_timer_before_wfi[cpu]);

	if (cmp < (s32)mcidle_timer_left2[cpu])
		localtimer_set_next_event(mcidle_timer_left2[cpu] - cmp);
	else
		localtimer_set_next_event(1);
#endif
#endif
#endif /* !IDLE_HAVE_STD_TIMER */
}

unsigned int g_pre_SPM_MCDI_Abnormal_WakeUp;

static bool go_to_mcidle(int cpu)
{
#if IDLE_HAVE_MCDI
	bool ret = 0;

	if (spm_mcdi_wfi(cpu) == 1) {
		mcidle_cnt[cpu] += 1;
		#if MCDI_DVT_CPUxGPT || MCDI_DVT_IPI
		mdelay(1);
		idle_info("CPU %d awake %d\n", cpu, mcidle_cnt[cpu]);
		#endif
		ret = 1;
	}

	if (g_SPM_MCDI_Abnormal_WakeUp != g_pre_SPM_MCDI_Abnormal_WakeUp) {
		pr_err("SPM-MCDI Abnormal %x\n", g_SPM_MCDI_Abnormal_WakeUp);
		g_pre_SPM_MCDI_Abnormal_WakeUp = g_SPM_MCDI_Abnormal_WakeUp;
	}
	return ret;
#else /* !IDLE_HAVE_MCDI */
	return false;
#endif /* IDLE_HAVE_MCDI */
}

static void mcidle_idle_pre_handler(int cpu)
{
	if (cpu == 0) {
#if IDLE_HAVE_MCDI
		if (only_one_cpu_online() ||
			(idle_switch[IDLE_TYPE_MC] == 0))
			spm_mcdi_switch_on_off(SPM_MCDI_IDLE, 0);
		else
			spm_mcdi_switch_on_off(SPM_MCDI_IDLE, 1);
#endif /* IDLE_HAVE_MCDI */
	}
}


/************************************************
 * deep idle part
 ************************************************/
static DEFINE_MUTEX(dpidle_locked);

#if AUDINTBUS_CLOCK_SWITCH

static u32 clk_mux_sel0;

static void backup_audintbus_26MHz(void)
{
	if (clk_mux_sel0 || !is_valid_reg(topckgen_base))
		return;

	clk_mux_sel0 = idle_readl(CLK_MUX_SEL0);

	/* select audiosys internal bus clock to 26MHz, CLK_MUX_SEL0[29:27] = 3'b001 */
	/* select audio hopping clock to 26MHz, CLK_MUX_SEL0[26] = 0 */
	idle_writel(CLK_MUX_SEL0, ((clk_mux_sel0 & ~GENMASK(29, 26)) | (1 << 27)));
}

static void restore_audintbus(void)
{
	if (!clk_mux_sel0 || !is_valid_reg(topckgen_base))
		return;

	idle_writel(CLK_MUX_SEL0, clk_mux_sel0);
	clk_mux_sel0 = 0;
}

#endif /* AUDINTBUS_CLOCK_SWITCH */

static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&dpidle_locked);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	dpidle_condition_mask[grp] |= mask;
	mutex_unlock(&dpidle_locked);
}

void enable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = BIT(id % 32);

	if (WARN_ON(INVALID_GRP_ID(grp)))
		return;

	enable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = BIT(id % 32);

	if (WARN_ON(INVALID_GRP_ID(grp)))
		return;

	disable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_dpidle_by_bit);

static bool dpidle_can_enter(void)
{
	int reason = NR_REASONS;
	int i = 0;
	u64 dpidle_block_curr_time = 0;
#if IDLE_HAVE_STD_TIMER
	struct timespec t;
	u32 expected_us;
#endif

#ifdef SPM_SODI_ENABLED
	if (gSPM_SODI_EN != 0) {
		reason = BY_OTH;
		goto out;
	}
#endif

#if IDLE_HAVE_CPUFREQ
	if (!mt_cpufreq_earlysuspend_status_get()) {
		if (!(dpidle_bypass_cnd & 0x2)) {
			reason = BY_VTG;
			goto out;
		}
	}
#endif /* IDLE_HAVE_CPUFREQ */

	if (!only_one_cpu_online()) {
		reason = BY_CPU;
		goto out;
	}

	if (idle_spm_lock) {
		if (!(dpidle_bypass_cnd & 0x2)) {
			reason = BY_VTG;
			goto out;
		}
	}

	memset(dpidle_block_mask, 0, sizeof(dpidle_block_mask));
	if (!clkmgr_idle_can_enter(dpidle_condition_mask, dpidle_block_mask)) {
		if (!(dpidle_bypass_cnd & 0x1)) {
			reason = BY_CLK;
			goto out;
		}
	}

#if IDLE_HAVE_STD_TIMER
	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	expected_us = t.tv_sec * USEC_PER_SEC + t.tv_nsec / NSEC_PER_USEC;
	if (expected_us < dpidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#else /* !IDLE_HAVE_STD_TIMER */
#ifdef CONFIG_SMP
	dpidle_timer_left = localtimer_get_counter();
	if (dpidle_timer_left < dpidle_time_critera ||
			(s32)dpidle_timer_left < 0) {
		reason = BY_TMR;
		goto out;
	}
#else
	idle_gpt_get_cnt(GPT1, &dpidle_timer_left);
	idle_gpt_get_cmp(GPT1, &dpidle_timer_cmp);
	if ((dpidle_timer_cmp-dpidle_timer_left) < dpidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}
#endif
#endif /* IDLE_HAVE_STD_TIMER */

out:
	if (reason >= NR_REASONS) {
		dpidle_block_prev_time = idle_get_current_time_ms();
		return true;
	}

	dpidle_block_cnt[reason]++;

	if (dpidle_block_prev_time == 0)
		dpidle_block_prev_time = idle_get_current_time_ms();

	dpidle_block_curr_time = idle_get_current_time_ms();
	if ((dpidle_block_curr_time - dpidle_block_prev_time) <=
		dpidle_block_time_critera)
		return false;

	if ((smp_processor_id() != 0))
		return false;

	for (i = 0; i < nr_cpu_ids; i++) {
		idle_ver("dpidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
				i, dpidle_cnt[i], i, rgidle_cnt[i]);
	}

	for (i = 0; i < NR_REASONS; i++) {
		idle_ver("[%d]dpidle_block_cnt[%s]=%lu\n", i, reason_name[i],
				dpidle_block_cnt[i]);
	}

	for (i = 0; i < NR_GRPS; i++) {
		idle_ver(
			"[%02d]dpidle_condition_mask[%-9s]=0x%08x\t\tdpidle_block_mask[%-9s]=0x%08x\n",
			i,
			grp_get_name(i), dpidle_condition_mask[i],
			grp_get_name(i), dpidle_block_mask[i]);
	}

	memset(dpidle_block_cnt, 0, sizeof(dpidle_block_cnt));
	dpidle_block_prev_time = idle_get_current_time_ms();

	return false;
}

void spm_dpidle_before_wfi(void)
{
	bus_dcm_enable();

#if AUDINTBUS_CLOCK_SWITCH
	backup_audintbus_26MHz();
#endif

#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	dpidle_timer_left2 = localtimer_get_counter();

	if ((s32)dpidle_timer_left2 <= 0)
		gpt_set_cmp(idle_gpt, 1); /* Trigger GPT4 Timerout imediately */
	else
		gpt_set_cmp(idle_gpt, dpidle_timer_left2);

	start_gpt(idle_gpt);
#else
	idle_gpt_get_cnt(idle_gpt, &dpidle_timer_left2);
#endif
#endif /* !IDLE_HAVE_STD_TIMER */
}

void spm_dpidle_after_wfi(void)
{
#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(idle_gpt)) {
		/* waked up by WAKEUP_GPT */
		localtimer_set_next_event(1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;

		idle_gpt_get_cnt(idle_gpt, &cnt);
		idle_gpt_get_cmp(idle_gpt, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n",
				__func__, idle_gpt + 1, cnt, cmp);
			cmp = cnt;
			WARN_ON(1);
		}

		localtimer_set_next_event(cmp - cnt);
		stop_gpt(idle_gpt);
		/* GPT_ClearCount(WAKEUP_GPT); */
	}
#endif
#endif /* !IDLE_HAVE_STD_TIMER */

#if AUDINTBUS_CLOCK_SWITCH
	restore_audintbus();
#endif

	bus_dcm_disable();

	dpidle_cnt[0]++;
}


/************************************************
 * slow idle part
 ************************************************/
static DEFINE_MUTEX(slidle_locked);


static void enable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] &= ~mask;
	mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	slidle_condition_mask[grp] |= mask;
	mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = BIT(id % 32);

	if (WARN_ON(INVALID_GRP_ID(grp)))
		return;

	enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = BIT(id % 32);

	if (WARN_ON(INVALID_GRP_ID(grp)))
		return;

	disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);

static bool slidle_can_enter(void)
{
	int reason = NR_REASONS;

	if (!only_one_cpu_online()) {
		reason = BY_CPU;
		goto out;
	}

	memset(slidle_block_mask, 0, sizeof(slidle_block_mask));
	if (!clkmgr_idle_can_enter(slidle_condition_mask, slidle_block_mask)) {
		if (!(slidle_bypass_cnd & 0x1)) {
			reason = BY_CLK;
			goto out;
		}
	}

#if defined(EN_PTP_OD) && EN_PTP_OD
	if (ptp_data[0]) {
		if (!(slidle_bypass_cnd & 0x2)) {
			reason = BY_OTH;
			goto out;
		}
	}
#endif

out:
	if (reason < NR_REASONS) {
		slidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

static void slidle_before_wfi(int cpu)
{
	bus_dcm_enable();
}

static void slidle_after_wfi(int cpu)
{
	bus_dcm_disable();
	slidle_cnt[cpu]++;
}

static void go_to_slidle(int cpu)
{
	slidle_before_wfi(cpu);

	mb();	/* sync before wfi */
	__asm__ __volatile__("wfi" : : : "memory");

	slidle_after_wfi(cpu);
}


/************************************************
 * regular idle part
 ************************************************/
static void rgidle_before_wfi(int cpu)
{
}

static void rgidle_after_wfi(int cpu)
{
	rgidle_cnt[cpu]++;
}

static noinline void go_to_rgidle(int cpu)
{
	rgidle_before_wfi(cpu);

	mb();	/* sync before wfi */
	__asm__ __volatile__("wfi" : : : "memory");

	rgidle_after_wfi(cpu);
}

/************************************************
 * idle task flow part
 ************************************************/

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */

#if MCDI_DVT_IPI
DEFINE_SPINLOCK(__mcdi_lock);

unsigned int core0_IPI_issue_count;
u8 mcdi_enter;

static void empty_function(void *info)
{
	unsigned long flags;
	int cpu = smp_processor_id();

	spin_lock_irqsave(&__mcdi_lock, flags);
	mcdi_enter &= ~(1 << cpu);
	spin_unlock_irqrestore(&__mcdi_lock, flags);
	mdelay(1);
	idle_info(
		"core %x ipi received, WFI count %d, core IPI command count: %d\n",
		cpu, mcidle_cnt[cpu], core0_IPI_issue_count);
}

static inline void mcidle_DVT_IPI_handler(int cpu)
{
	int cpu0_enter_forLoop = 0;
	unsigned long flags;

	if (!idle_switch[IDLE_TYPE_MC])
		return;

	for (;;) {
		if (cpu == 0) {
			if ((spm_read(SPM_SLEEP_TIMER_STA) & 0xfe)) {
				mdelay(1);
				smp_call_function(empty_function, NULL, 0);
				idle_info("core0 IPI\n");
				core0_IPI_issue_count++;
			}
		} else {
			if (go_to_mcidle(cpu) == 0)
				return;

			mdelay(1);
		}
	}
}
#endif

#if MCDI_DVT_CPUxGPT
unsigned int test_cpu;

static inline void mcidle_DVT_CPUxGPT_handler(int cpu)
{
	if (!idle_switch[IDLE_TYPE_MC])
		return;

	for (;;) {
		if (cpu != 0) {
			if (mcidle_can_enter(cpu)) {
				if (go_to_mcidle(cpu) == 0)
					return;
			}
		}
	}
}
#endif

static inline int mcidle_handler(int cpu)
{
#if MCDI_DVT_IPI
	mcidle_DVT_IPI_handler(cpu);
#elif MCDI_DVT_CPUxGPT
	mcidle_DVT_CPUxGPT_handler(cpu);
#else
	if (idle_switch[IDLE_TYPE_MC]) {
		if (mcidle_can_enter(cpu)) {
			go_to_mcidle(cpu);
			return 1;
		}
	}
#endif
	return 0;
}

#if IDLE_HAVE_SODI

static u32 slp_spm_SODI_flags = {
	/* SPM_CPU_DVS_DIS */ /* not verfication yet */
	0
};

#endif /* IDLE_HAVE_SODI */

static inline int soidle_handler(int cpu)
{
#if IDLE_HAVE_SODI
	if (!idle_switch[IDLE_TYPE_SO])
		return 0;

	if (!soidle_can_enter(cpu))
		return 0;

	#if SODI_DVT_APxGPT
	idle_info("SPM-Enter SODI+\n");
	#endif
	spm_go_to_sodi(slp_spm_SODI_flags, 0);
	#if SODI_DVT_APxGPT
	idle_info("SPM-Enter SODI-\n");
	#endif
#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	idle_ver("SO:timer_left=%u, timer_left2=%u, delta=%u\n",
		soidle_timer_left, soidle_timer_left2,
		soidle_timer_left - soidle_timer_left2);
#else
	idle_ver("SO:timer_left=%u, timer_left2=%u, delta=%lu,timeout val=%u\n",
		soidle_timer_left, soidle_timer_left2,
		soidle_timer_left2 - soidle_timer_left,
		soidle_timer_cmp - soidle_timer_left);
#endif
#endif /* !IDLE_HAVE_STD_TIMER */

	return 1;
#else /* !IDLE_HAVE_SODI */
	return 0;
#endif /* IDLE_HAVE_SODI */
}

static u32 slp_spm_deepidle_flags = {
	SPM_INFRA_PDN_DIS | SPM_26M_OFF_DIS
};

void set_slp_spm_deepidle_flags(bool en)
{
	idle_err("unused function: %s()\n", __func__);
}

static inline void dpidle_pre_handler(void)
{
#if IDLE_HAVE_THERMAL
	/* cancel thermal hrtimer for power saving */
	tscpu_cancel_thermal_timer();
#endif /* IDLE_HAVE_THERMAL */

#if IDLE_HAVE_GPUDVFS
	/* disable gpu dvfs timer */
	mtk_enable_gpu_dvfs_timer(false);
#endif /* IDLE_HAVE_GPUDVFS */
}

static inline void dpidle_post_handler(void)
{
#if IDLE_HAVE_GPUDVFS
	/* enable gpu dvfs timer */
	mtk_enable_gpu_dvfs_timer(true);
#endif /* IDLE_HAVE_GPUDVFS */

#if IDLE_HAVE_THERMAL
	/* restart thermal hrtimer for update temp info */
	tscpu_start_thermal_timer();
#endif /* IDLE_HAVE_THERMAL */
}

static inline int dpidle_handler(int cpu)
{
	int ret = 0;

	if (!idle_switch[IDLE_TYPE_DP])
		return ret;

	if (!dpidle_can_enter())
		return ret;

#if IDLE_HAVE_DPIDLE
	dpidle_pre_handler();
	spm_go_to_dpidle(slp_spm_deepidle_flags, 0, DEEPIDLE_LOG_NONE);
	dpidle_post_handler();
	ret = 1;
#endif /* IDLE_HAVE_DPIDLE */

#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	idle_ver("DP:timer_left=%u, timer_left2=%u, delta=%u\n",
		dpidle_timer_left, dpidle_timer_left2,
		dpidle_timer_left - dpidle_timer_left2);
#else
	idle_ver("DP:timer_left=%u, timer_left2=%u, delta=%u, timeout val=%u\n",
		dpidle_timer_left, dpidle_timer_left2,
		dpidle_timer_left2 - dpidle_timer_left,
		dpidle_timer_cmp - dpidle_timer_left);
#endif
#endif /* !IDLE_HAVE_STD_TIMER */

	return ret;
}

static inline int slidle_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_SL]) {
		if (slidle_can_enter()) {
			go_to_slidle(cpu);
			ret = 1;
		}
	}

	return ret;
}

static inline int rgidle_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_RG]) {
		go_to_rgidle(cpu);
		ret = 1;
	}

	return ret;
}

static int (*idle_handlers[NR_TYPES])(int) = {
	dpidle_handler,
	soidle_handler,
	mcidle_handler,
	slidle_handler,
	rgidle_handler,
};

void arch_idle(void)
{
	int cpu = smp_processor_id();
	int i;

	/* dynamic on/offload between single/multi core deepidles */
	mcidle_idle_pre_handler(cpu);

	for (i = 0; i < NR_TYPES; i++) {
		if (idle_handlers[i](cpu))
			break;
	}
}

static inline int dpidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_DP]) {
		if (dpidle_can_enter())
			ret = 1;
	}

	return ret;
}

static inline int soidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_SO]) {
		if (soidle_can_enter(cpu))
			ret = 1;
	}

	return ret;
}

static inline int mcidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_MC]) {
		if (mcidle_can_enter(cpu))
			ret = 1;
	}

	return ret;
}

static inline int slidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_SL]) {
		if (slidle_can_enter())
			ret = 1;
	}

	return ret;
}

static inline int rgidle_select_handler(int cpu)
{
	int ret = 0;

	if (idle_switch[IDLE_TYPE_RG])
		ret = 1;

	return ret;
}

static int (*idle_select_handlers[NR_TYPES])(int) = {
	dpidle_select_handler,
	soidle_select_handler,
	mcidle_select_handler,
	slidle_select_handler,
	rgidle_select_handler,
};

int mtk_idle_select(int cpu)
{
	int i = NR_TYPES - 1;

	for (i = 0; i < NR_TYPES; i++) {
		if (idle_select_handlers[i](cpu))
			break;
	}

	return i;
}
EXPORT_SYMBOL(mtk_idle_select);

int dpidle_enter(int cpu)
{
	int ret = 1;

	dpidle_pre_handler();
	spm_go_to_dpidle(slp_spm_deepidle_flags, 0, DEEPIDLE_LOG_NONE);
	dpidle_post_handler();

#if !IDLE_HAVE_STD_TIMER
#ifdef CONFIG_SMP
	idle_ver("DP:timer_left=%u, timer_left2=%u, delta=%u\n",
		dpidle_timer_left, dpidle_timer_left2,
		dpidle_timer_left - dpidle_timer_left2);
#else
	idle_ver("DP:timer_left=%u, timer_left2=%u, delta=%u, timeout val=%u\n",
		dpidle_timer_left, dpidle_timer_left2,
		dpidle_timer_left2 - dpidle_timer_left,
		dpidle_timer_cmp - dpidle_timer_left);
#endif
#endif /* !IDLE_HAVE_STD_TIMER */

	return ret;
}
EXPORT_SYMBOL(dpidle_enter);

int soidle_enter(int cpu)
{
	int ret = 1;

#if IDLE_HAVE_SODI
	spm_go_to_sodi(slp_spm_SODI_flags, 0);
#endif

	return ret;
}
EXPORT_SYMBOL(soidle_enter);

int mcidle_enter(int cpu)
{
	int ret = 1;

	go_to_mcidle(cpu);

	return ret;
}
EXPORT_SYMBOL(mcidle_enter);

int slidle_enter(int cpu)
{
	int ret = 1;

	go_to_slidle(cpu);

	return ret;
}
EXPORT_SYMBOL(slidle_enter);

int rgidle_enter(int cpu)
{
	int ret = 1;

	go_to_rgidle(cpu);

	return ret;
}
EXPORT_SYMBOL(rgidle_enter);

static int __init get_base_from_node(
	const char *cmp, void __iomem **pbase, int idx)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node) {
		idle_err("node '%s' not found!\n", cmp);
		return -1;
	}

	*pbase = of_iomap(node, idx);

	return 0;
}

static void __init iomap_init(void)
{
	get_base_from_node("mediatek,mt8167-topckgen", &topckgen_base, 0);
	get_base_from_node("mediatek,mt8167-scpsys", &scpsys_base, 0);
	get_base_from_node("mediatek,mt8167-audiotop", &audiosys_base, 0);
	get_base_from_node("mediatek,mt8167-mfgcfg", &mfgsys_base, 0);
	get_base_from_node("mediatek,mt8167-mmsys", &mmsys_base, 0);
	get_base_from_node("mediatek,mt8167-imgsys", &imgsys_base, 0);
	get_base_from_node("mediatek,mt8167-vdecsys", &vdecsys_base, 0);
}

/*
 * debugfs
 */
static char dbg_buf[2048] = {0};
static char cmd_buf[512] = {0};

/*
 * idle_state
 */
static int _idle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int idle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _idle_state_open, inode->i_private);
}

static ssize_t idle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	static const char *d = "/sys/kernel/debug/cpuidle";
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "********** idle state dump **********\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		p += sprintf(p,
			"soidle_cnt[%d]=%lu, dpidle_cnt[%d]=%lu, mcidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
			i, soidle_cnt[i], i, dpidle_cnt[i],
			i, mcidle_cnt[i], i, slidle_cnt[i],
			i, rgidle_cnt[i]);
	}

	p += sprintf(p, "\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		p += sprintf(p, "%s_switch=%d, ", idle_name[i], idle_switch[i]);

	p += sprintf(p, "\n");

	p += sprintf(p, "\n********** idle command help **********\n");
	p += sprintf(p, "status help:   cat %s/idle_state\n", d);
	p += sprintf(p, "switch on/off: echo switch mask > %s/idle_state\n", d);

	p += sprintf(p, "soidle help:   cat %s/soidle_state\n", d);
	p += sprintf(p, "mcidle help:   cat %s/mcidle_state\n", d);
	p += sprintf(p, "dpidle help:   cat %s/dpidle_state\n", d);
	p += sprintf(p, "slidle help:   cat %s/slidle_state\n", d);
	p += sprintf(p, "rgidle help:   cat %s/rgidle_state\n", d);

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t idle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int idx;
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "switch")) {
			for (idx = 0; idx < NR_TYPES; idx++)
				idle_switch[idx] = (param & BIT(idx)) ? 1 : 0;
		}
		return count;
	}

	return -EINVAL;
}

static const struct file_operations idle_state_fops = {
	.open = idle_state_open,
	.read = idle_state_read,
	.write = idle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};
/*
 * dpidle_state
 */
static int _dpidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int dpidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _dpidle_state_open, inode->i_private);
}

static ssize_t dpidle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	static const char *d = "/sys/kernel/debug/cpuidle/dpidle_state";
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "*********** deep idle state ************\n");
	p += sprintf(p, "dpidle_time_critera=%u\n", dpidle_time_critera);
	p += sprintf(p, "dpidle_bypass_cnd=%u\n", dpidle_bypass_cnd);

	p += sprintf(p, "\n");

	p += sprintf(p, "block_reason\tblock_cnt\n");
	p += sprintf(p, "------------\t---------\n");
	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d][%s]\t%lu\n", i, reason_name[i],
			dpidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

	p += sprintf(p, "grp\t\tcondition_mask\tblock_mask\n");
	p += sprintf(p, "---\t\t--------------\t----------\n");
	for (i = 0; i < NR_GRPS; i++) {
		if (!dpidle_condition_mask[i])
			continue;

		p += sprintf(p, "[%02d][%-9s]\t0x%08x\t0x%08x\n",
			i, grp_get_name(i), dpidle_condition_mask[i],
			dpidle_block_mask[i]);
	}

	p += sprintf(p, "\n*********** dpidle command help  ************\n");
	p += sprintf(p, "dpidle help:   cat %s\n", d);
	p += sprintf(p, "switch on/off: echo [dpidle] 1/0 > %s\n", d);
	p += sprintf(p, "cpupdn on/off: echo cpupdn 1/0 > %s\n", d);
	p += sprintf(p, "en_dp_by_bit:  echo enable id > %s\n", d);
	p += sprintf(p, "dis_dp_by_bit: echo disable id > %s\n", d);
	p += sprintf(p, "modify tm_cri: echo time value(dec) > %s\n", d);
	p += sprintf(p, "bypass clk:    echo bypass 1/0 > %s\n", d);
	p += sprintf(p, "bypass vtg:    echo bypass 2/0 > %s\n", d);

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t dpidle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			idle_switch[IDLE_TYPE_DP] = param;
		else if (!strcmp(cmd, "enable"))
			enable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			dpidle_time_critera = param;
		else if (!strcmp(cmd, "bypass"))
			dpidle_bypass_cnd = param;
		return count;
	} else if (!kstrtoint(cmd_buf, 0, &param)) {
		idle_switch[IDLE_TYPE_DP] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations dpidle_state_fops = {
	.open = dpidle_state_open,
	.read = dpidle_state_read,
	.write = dpidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * soidle_state
 */
static int _soidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle_state_open, inode->i_private);
}

static ssize_t soidle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	static const char *d = "/sys/kernel/debug/cpuidle/soidle_state";
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "*********** screen-on idle state ************\n");
	p += sprintf(p, "soidle_time_critera=%u\n", soidle_time_critera);

	p += sprintf(p, "\n");

	p += sprintf(p, "block_reason\tblock_cnt\n");
	p += sprintf(p, "------------\t---------\n");
	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d][%s]\t%lu\n", i, reason_name[i],
			soidle_block_cnt[0][i]);
	}

	p += sprintf(p, "\n");

#if IDLE_HAVE_SODI
	p += sprintf(p, "grp\t\tcondition_mask\tblock_mask\n");
	p += sprintf(p, "---\t\t--------------\t----------\n");
	for (i = 0; i < NR_GRPS; i++) {
		if (!soidle_condition_mask[i])
			continue;

		p += sprintf(p, "[%02d][%-9s]\t0x%08x\t0x%08x\n",
			i, grp_get_name(i), soidle_condition_mask[i],
			soidle_block_mask[i]);
	}
#endif /* IDLE_HAVE_SODI */

	p += sprintf(p, "\n*********** soidle command help  ************\n");
	p += sprintf(p, "soidle help:   cat %s\n", d);
	p += sprintf(p, "switch on/off: echo [soidle] 1/0 > %s\n", d);
	p += sprintf(p, "cpupdn on/off: echo cpupdn 1/0 > %s\n", d);
	p += sprintf(p, "en_dp_by_bit:  echo enable id > %s\n", d);
	p += sprintf(p, "dis_dp_by_bit: echo disable id > %s\n", d);
	p += sprintf(p, "modify tm_cri: echo time value(dec) > %s\n", d);

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t soidle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle"))
			idle_switch[IDLE_TYPE_SO] = param;
		else if (!strcmp(cmd, "enable"))
			enable_soidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_soidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			soidle_time_critera = param;
		return count;
	} else if (!kstrtoint(cmd_buf, 0, &param)) {
		idle_switch[IDLE_TYPE_SO] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle_state_fops = {
	.open = soidle_state_open,
	.read = soidle_state_read,
	.write = soidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * mcidle_state
 */
static int _mcidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int mcidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _mcidle_state_open, inode->i_private);
}

static ssize_t mcidle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	static const char *d = "/sys/kernel/debug/cpuidle/mcidle_state";
	int len = 0;
	char *p = dbg_buf;
	int cpus, reason;

	p += sprintf(p, "*********** multi-core idle state ************\n");
	p += sprintf(p, "mcidle_time_critera=%u\n", mcidle_time_critera);

	for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
		p += sprintf(p, "cpu:%d\n", cpus);
		for (reason = 0; reason < NR_REASONS; reason++) {
			p += sprintf(p, "[%d]mcidle_block_cnt[%s]=%lu\n",
				reason,
				reason_name[reason],
				mcidle_block_cnt[cpus][reason]);
		}
		p += sprintf(p, "\n");
	}

	p += sprintf(p, "\n*********** mcidle command help  ************\n");
	p += sprintf(p, "mcidle help:   cat %s\n", d);
	p += sprintf(p, "switch on/off: echo [mcidle] 1/0 > %s\n", d);
	p += sprintf(p, "modify tm_cri: echo time value(dec) > %s\n", d);

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcidle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "mcidle"))
			idle_switch[IDLE_TYPE_MC] = param;
		else if (!strcmp(cmd, "time"))
			mcidle_time_critera = param;
		return count;
	} else if (!kstrtoint(cmd_buf, 0, &param)) {
		idle_switch[IDLE_TYPE_MC] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations mcidle_state_fops = {
	.open = mcidle_state_open,
	.read = mcidle_state_read,
	.write = mcidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * slidle_state
 */
static int _slidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int slidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _slidle_state_open, inode->i_private);
}

static ssize_t slidle_state_read(struct file *filp, char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	static const char *d = "/sys/kernel/debug/cpuidle/slidle_state";
	int len = 0;
	char *p = dbg_buf;
	int i;

	p += sprintf(p, "*********** slow idle state ************\n");

	p += sprintf(p, "slidle_bypass_cnd=%u\n", slidle_bypass_cnd);

	p += sprintf(p, "\n");

	p += sprintf(p, "block_reason\tblock_cnt\n");
	p += sprintf(p, "------------\t---------\n");
	for (i = 0; i < NR_REASONS; i++) {
		p += sprintf(p, "[%d][%s]\t%lu\n", i, reason_name[i],
			slidle_block_cnt[i]);
	}

	p += sprintf(p, "\n");

	p += sprintf(p, "grp\t\tcondition_mask\tblock_mask\n");
	p += sprintf(p, "---\t\t--------------\t----------\n");
	for (i = 0; i < NR_GRPS; i++) {
		if (!slidle_condition_mask[i])
			continue;

		p += sprintf(p, "[%02d][%-9s]\t0x%08x\t0x%08x\n",
			i, grp_get_name(i), slidle_condition_mask[i],
			slidle_block_mask[i]);
	}

	p += sprintf(p, "\n********** slidle command help **********\n");
	p += sprintf(p, "slidle help:   cat %s\n", d);
	p += sprintf(p, "switch on/off: echo [slidle] 1/0 > %s\n", d);
	p += sprintf(p, "bypass clk:    echo bypass 1/0 > %s\n", d);
	p += sprintf(p, "bypass oth:    echo bypass 2/0 > %s\n", d);

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t slidle_state_write(struct file *filp, const char __user *userbuf,
				size_t count, loff_t *f_pos)
{
	char cmd[32];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(userbuf, "%31s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "slidle"))
			idle_switch[IDLE_TYPE_SL] = param;
		else if (!strcmp(cmd, "enable"))
			enable_slidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_slidle_by_bit(param);
		else if (!strcmp(cmd, "bypass"))
			slidle_bypass_cnd = param;

		return count;
	} else if (!kstrtoint(cmd_buf, 0, &param)) {
		idle_switch[IDLE_TYPE_SL] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations slidle_state_fops = {
	.open = slidle_state_open,
	.read = slidle_state_read,
	.write = slidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *root_entry;

static int mt_cpuidle_debugfs_init(void)
{
	/* Initialize debugfs */
	root_entry = debugfs_create_dir("cpuidle", NULL);
	if (!root_entry) {
		idle_err("Can not create debugfs 'cpuidle'\n");
		return 1;
	}

	debugfs_create_file("idle_state", 0644, root_entry, NULL,
				&idle_state_fops);
	debugfs_create_file("dpidle_state", 0644, root_entry, NULL,
				&dpidle_state_fops);
	debugfs_create_file("soidle_state", 0644, root_entry, NULL,
				&soidle_state_fops);
	debugfs_create_file("mcidle_state", 0644, root_entry, NULL,
				&mcidle_state_fops);
	debugfs_create_file("slidle_state", 0644, root_entry, NULL,
				&slidle_state_fops);

	return 0;
}

void mtk_cpuidle_framework_init(void)
{
#if !IDLE_HAVE_STD_TIMER
	int err = 0;
	int i = 0;
#endif
	static int init;

	if (init)
		return;

	init++;

	idle_ver("[%s]entry!!\n", __func__);

#if !IDLE_HAVE_STD_TIMER
	err = request_gpt(idle_gpt, GPT_ONE_SHOT, GPT_CLK_SRC_SYS,
		GPT_CLK_DIV_1, 0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_err("[%s]fail to request GPT%d\n", __func__, idle_gpt + 1);

	err = 0;

	for (i = 0; i < num_possible_cpus(); i++)
		err |= cpu_xgpt_register_timer(i, NULL);

	if (err)
		idle_err("[%s]fail to request cpuxgpt\n", __func__);
#endif /* !IDLE_HAVE_STD_TIMER */

	iomap_init();
	mt_cpuidle_debugfs_init();
}
EXPORT_SYMBOL(mtk_cpuidle_framework_init);
