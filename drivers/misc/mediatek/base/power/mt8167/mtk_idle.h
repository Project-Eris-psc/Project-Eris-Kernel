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

#ifndef _MT_IDLE_H
#define _MT_IDLE_H

#define MT_IDLE_EN		1

enum idle_lock_spm_id {
	IDLE_SPM_LOCK_VCORE_DVFS = 0,
};

#if MT_IDLE_EN

extern void idle_lock_spm(enum idle_lock_spm_id id);
extern void idle_unlock_spm(enum idle_lock_spm_id id);

extern void enable_dpidle_by_bit(int id);
extern void disable_dpidle_by_bit(int id);
extern void enable_soidle_by_bit(int id);
extern void disable_soidle_by_bit(int id);

extern void set_slp_spm_deepidle_flags(bool en);

extern void mt_idle_init(void);

#else /* !MT_IDLE_EN */

static inline void idle_lock_spm(enum idle_lock_spm_id id) {}
static inline void idle_unlock_spm(enum idle_lock_spm_id id) {}

static inline void enable_dpidle_by_bit(int id) {}
static inline void disable_dpidle_by_bit(int id) {}
static inline void enable_soidle_by_bit(int id) {}
static inline void disable_soidle_by_bit(int id) {}

static inline void set_slp_spm_deepidle_flags(bool en) {}

static inline void mt_idle_init(void) {}

#endif /* MT_IDLE_EN */

enum {
	CG_CTRL0,
	CG_CTRL1,
	CG_CTRL2,
	CG_CTRL8,
	CG_MMSYS0,
	CG_MMSYS1,
	CG_IMGSYS,
	CG_MFGSYS,
	CG_AUDIO,
	CG_VDEC0,
	CG_VDEC1,

	NR_GRPS,
};

#define CGID(grp, bit)	((grp) * 32 + bit)

enum cg_clk_id {
	NR_CLKS,
};

#ifdef _MT_IDLE_C

/* TODO: remove it */

extern unsigned int g_SPM_MCDI_Abnormal_WakeUp;

#if defined(EN_PTP_OD) && EN_PTP_OD
extern u32 ptp_data[3];
#endif

extern struct kobject *power_kobj;

#endif /* _MT_IDLE_C */

#endif
