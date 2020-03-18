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

/*
* @file mt_cpufreq_internal.h
* @brief CPU DVFS driver interface
*/

#ifndef __MT_CPUFREQ_STRUCT_H__
#define __MT_CPUFREQ_STRUCT_H__

struct mt_cpu_dvfs {
	const char *name;
	unsigned int cpu_id;
	unsigned int cpu_level;
	struct cpufreq_policy *mt_policy;
	enum mt_cpu_dvfs_buck_id Vproc_buck_id;
	enum mt_cpu_dvfs_buck_id Vsram_buck_id;
	enum mt_cpu_dvfs_pll_id Pll_id;
	struct mt_cpu_freq_method *freq_tbl;	/* Frequency table */
	struct mt_cpu_freq_info *opp_tbl;	/* OPP table */
	int nr_opp_tbl;		/* size for OPP table */
	int idx_opp_tbl;	/* current OPP idx */
	int idx_opp_ppm_base;	/* ppm update base */
	int idx_opp_ppm_limit;	/* ppm update limit */
	int armpll_is_available;	/* For CCI clock switch flag */
	int idx_normal_max_opp;	/* idx for normal max OPP */
	struct cpufreq_frequency_table *freq_tbl_for_cpufreq;	/* freq table for cpufreq */

	/* enable/disable DVFS function */
	bool dvfs_disable_by_suspend;
	bool dvfs_disable_by_procfs;

	/* turbo mode */
	unsigned int turbo_mode;
};

struct buck_ctrl_t {
	const char *name;
	enum mt_cpu_dvfs_buck_id buck_id;
	unsigned int cur_volt;
	unsigned int fix_volt;
	struct buck_ctrl_ops *buck_ops;
};

struct buck_ctrl_ops {
	unsigned int (*get_cur_volt)(struct buck_ctrl_t *buck_p);	/* return volt (mV * 100) */
	int (*set_cur_volt)(struct buck_ctrl_t *buck_p, unsigned int volt);	/* set volt (mv * 100) */
	unsigned int (*transfer2pmicval)(unsigned int volt);
	unsigned int (*transfer2volt)(unsigned int val);
	unsigned int (*settletime)(unsigned int ori, unsigned int target);
};

struct pll_ctrl_t {
	const char *name;
	enum mt_cpu_dvfs_pll_id pll_id;
	unsigned int *armpll_addr;
	int hopping_id;
	unsigned int *armpll_div_addr;
	int armpll_div_l;
	int armpll_div_h;
	int pll_muxsel_l;
	int pll_muxsel_h;
	struct pll_ctrl_ops *pll_ops;
};

struct pll_ctrl_ops {
	unsigned int (*get_cur_freq)(struct pll_ctrl_t *pll_p);	/* return khz */
	/* int (*set_cur_freq)(struct pll_ctrl_t *pll_p, unsigned int freq); */
	void (*set_armpll_dds)(struct pll_ctrl_t *pll_p, unsigned int vco, unsigned int pos_div);
	void (*set_armpll_posdiv)(struct pll_ctrl_t *pll_p, unsigned int pos_div);
	void (*set_armpll_clkdiv)(struct pll_ctrl_t *pll_p, unsigned int clk_div);
	void (*set_freq_hopping)(struct pll_ctrl_t *pll_p, unsigned int dds);
	void (*clksrc_switch)(struct pll_ctrl_t *pll_p, enum top_ckmuxsel sel);
	enum top_ckmuxsel (*get_clksrc)(struct pll_ctrl_t *pll_p);
	int (*set_sync_dcm)(unsigned int mhz); /* set mhz */
};
#endif
