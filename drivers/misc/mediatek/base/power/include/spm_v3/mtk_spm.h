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

#ifndef __MTK_SPM_H__
#define __MTK_SPM_H__

#include <linux/kernel.h>
#include <linux/io.h>
#include <mach/upmu_hw.h> /* for PMIC power settings */

extern void __iomem *spm_base;
extern u32 spm_irq_0;
extern int spm_for_gps_flag;

#undef SPM_BASE
#define SPM_BASE spm_base

/* #include <mach/mt_irq.h> */
#include <mt-plat/sync_write.h>

/**************************************
 * Config and Parameter
 **************************************/
#define SPM_IRQ0_ID		spm_irq_0

#include <mtk_spm_reg.h>

typedef enum {
	WR_NONE = 0,
	WR_UART_BUSY = 1,
	WR_PCM_ASSERT = 2,
	WR_PCM_TIMER = 3,
	WR_WAKE_SRC = 4,
	WR_UNKNOWN = 5,
} wake_reason_t;

enum mt_sodi_fw {
	SODI_FW_LPM	= (1 << 0),	/*  1600/1.0  : 1270/0.9 : 1066/0.9 */
	SODI_FW_HPM	= (1 << 1),	/*  1700/1.0  : 1270/0.9 : 1066/0.9 */
	SODI_FW_ULTRA	= (1 << 2),	/*  1866/1.05 : 1600/1.0 : 1270/0.9 */
};

struct twam_sig {
	u32 sig0;		/* signal 0: config or status */
	u32 sig1;		/* signal 1: config or status */
	u32 sig2;		/* signal 2: config or status */
	u32 sig3;		/* signal 3: config or status */
};

typedef void (*twam_handler_t) (struct twam_sig *twamsig);
typedef void (*vcorefs_handler_t) (int opp);
typedef void (*vcorefs_start_handler_t) (void);

/* check if spm firmware ready */
extern int spm_load_firmware_status(void);

/* for power management init */
extern int spm_module_init(void);

/* for ANC in talking */
extern void spm_mainpll_on_request(const char *drv_name);
extern void spm_mainpll_on_unrequest(const char *drv_name);

/* for TWAM in MET */
extern void spm_twam_register_handler(twam_handler_t handler);
extern void spm_twam_enable_monitor(const struct twam_sig *twamsig, bool speed_mode);
extern void spm_twam_disable_monitor(void);
extern void spm_twam_set_idle_select(unsigned int sel);
extern void spm_twam_set_window_length(unsigned int len);
extern void spm_twam_set_mon_type(struct twam_sig *mon);

/* for Vcore DVFS */
extern int spm_go_to_ddrdfs(u32 spm_flags, u32 spm_data);

/* for Vcore DVFS in MET */
extern void spm_vcorefs_register_handler(vcorefs_handler_t handler, vcorefs_start_handler_t start_handler);

#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
/* for PMIC power settings */
enum {
	PMIC_PWR_NORMAL = 0,
	PMIC_PWR_DEEPIDLE,
	PMIC_PWR_SODI3,
	PMIC_PWR_SODI,
	PMIC_PWR_SUSPEND,
	PMIC_PWR_NUM,
};
void spm_pmic_power_mode(int mode, int force, int lock);
#endif /* !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) */

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
struct spm_data;
int spm_to_sspm_command(u32 cmd, struct spm_data *spm_d);
int spm_to_sspm_command_async(u32 cmd, struct spm_data *spm_d);
int spm_to_sspm_command_async_wait(u32 cmd);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

void mt_spm_for_gps_only(int enable);
void mt_spm_dcs_s1_setting(int enable, int flags);
extern void unmask_edge_trig_irqs_for_cirq(void);

/**************************************
 * Macro and Inline
 **************************************/
#define get_high_cnt(sigsta)		((sigsta) & 0x3ff)
#define get_high_percent(sigsta)	((get_high_cnt(sigsta) * 100 + 511) / 1023)

#endif
