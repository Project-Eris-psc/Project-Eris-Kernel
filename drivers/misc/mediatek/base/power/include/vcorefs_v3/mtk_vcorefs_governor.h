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

#ifndef _MTK_VCOREFS_GOVERNOR_H
#define _MTK_VCOREFS_GOVERNOR_H

#define VCPREFS_TAG "[VcoreFS] "
#define vcorefs_err(fmt, args...)	pr_err(VCPREFS_TAG fmt, ##args)
#define vcorefs_crit(fmt, args...)	pr_crit(VCPREFS_TAG fmt, ##args)
#define vcorefs_warn(fmt, args...)	pr_warn(VCPREFS_TAG fmt, ##args)
#define vcorefs_debug(fmt, args...)	pr_debug(VCPREFS_TAG fmt, ##args)

/* Uses for DVFS Request */
#define vcorefs_crit_mask(log_mask, kicker, fmt, args...)	\
do {								\
	if (log_mask & (1U << kicker))				\
		;						\
	else							\
		vcorefs_crit(fmt, ##args);			\
} while (0)

struct kicker_config {
	int kicker;
	int opp;
	int dvfs_opp;
};

enum dvfs_kicker {
	KIR_MM = 0,
	KIR_DCS,
	KIR_UFO,
	KIR_UFS,
	KIR_PERF,
	KIR_ANC_MD32,
	KIR_EFUSE,
	KIR_PASR,
	KIR_SDIO,
	KIR_USB,
	KIR_SYSFS,
	KIR_SYSFSX,
	NUM_KICKER,

	/* internal kicker */
	KIR_LATE_INIT,
	KIR_AUTOK_EMMC,
	KIR_AUTOK_SDIO,
	KIR_AUTOK_SD,
	LAST_KICKER,
};

enum dvfs_opp {
	OPP_UNREQ = -1,
	OPP_0 = 0,
	OPP_1,
	OPP_2,
	OPP_3,
	NUM_OPP,
};

struct opp_profile {
	int vcore_uv;
	int ddr_khz;
};

/* boot up opp for SPM init */
#define BOOT_UP_OPP             OPP_0

/* target OPP when feature enable */
#define LATE_INIT_OPP           (NUM_OPP - 1)

/* need autok in MSDC group */
#define AUTOK_KIR_GROUP         ((1U << KIR_AUTOK_EMMC) | (1U << KIR_AUTOK_SDIO) | (1U << KIR_AUTOK_SD))

/*
 * VOUT selection in normal mode (SW mode)
 * VOUT = 0.4V + 6.25mV * VOSEL
 */
#define PMIC_VCORE_ADDR         MT6335_BUCK_VCORE_CON1
#define VCORE_BASE_UV           400000
#define VCORE_STEP_UV           6250
#define VCORE_INVALID           0x80

#define vcore_uv_to_pmic(uv)	/* pmic >= uv */	\
	((((uv) - VCORE_BASE_UV) + (VCORE_STEP_UV - 1)) / VCORE_STEP_UV)

#define vcore_pmic_to_uv(pmic)	\
	(((pmic) * VCORE_STEP_UV) + VCORE_BASE_UV)


extern int kicker_table[LAST_KICKER];

/* Governor extern API */
extern bool is_vcorefs_feature_enable(void);
bool vcorefs_vcore_dvs_en(void);
bool vcorefs_dram_dfs_en(void);
int vcorefs_module_init(void);
extern int vcorefs_get_num_opp(void);
extern int vcorefs_get_sw_opp(void);
extern int vcorefs_get_curr_vcore(void);
extern int vcorefs_get_curr_ddr(void);
extern void vcorefs_update_opp_table(void);
extern char *governor_get_kicker_name(int id);
extern char *vcorefs_get_opp_table_info(char *p);
extern int vcorefs_output_kicker_id(char *name);
extern int governor_debug_store(const char *);
extern int vcorefs_late_init_dvfs(void);
extern int kick_dvfs_by_opp_index(struct kicker_config *krconf);
extern char *governor_get_dvfs_info(char *p);
extern int vcorefs_get_vcore_by_steps(u32 opp);
extern void vcorefs_init_opp_table(void);
extern void governor_autok_manager(void);
extern bool governor_autok_check(int kicker);
extern bool governor_autok_lock_check(int kicker, int opp);
extern int vcorefs_get_hw_opp(void);
extern int vcorefs_enable_debug_isr(bool enable);

#endif	/* _MTK_VCOREFS_GOVERNOR_H */
