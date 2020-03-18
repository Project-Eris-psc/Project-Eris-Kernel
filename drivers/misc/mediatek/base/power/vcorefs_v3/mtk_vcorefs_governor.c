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
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include <mt-plat/mtk_pmic_wrap.h>
#include <mtk_vcorefs_manager.h>

#include <mtk_spm_vcore_dvfs.h>
#include <mtk_dramc.h>
#include <mtk_eem.h>
#include "mmdvfs_mgr.h"

__weak int emmc_autok(void)
{
	vcorefs_crit("NOT SUPPORT EMMC AUTOK\n");
	return 0;
}

__weak int sd_autok(void)
{
	vcorefs_crit("NOT SUPPORT SD AUTOK\n");
	return 0;
}

__weak int sdio_autok(void)
{
	vcorefs_crit("NOT SUPPORT SDIO AUTOK\n");
	return 0;
}

/*
 * __nosavedata will not be restored after IPO-H boot
 */
static int vcorefs_sw_opp __nosavedata;

static DEFINE_MUTEX(governor_mutex);

struct governor_profile {
	bool vcore_dvs;
	bool ddr_dfs;
	bool isr_debug;

	int curr_vcore_uv;
	int curr_ddr_khz;

	u32 autok_kir_group;
	u32 active_autok_kir;

	int late_init_opp;
};

static struct governor_profile governor_ctrl = {
	.vcore_dvs = SPM_VCORE_DVS_EN,
	.ddr_dfs = SPM_DDR_DFS_EN,
	.isr_debug = 0,

	.late_init_opp = LATE_INIT_OPP,

	.autok_kir_group = AUTOK_KIR_GROUP,
	.active_autok_kir = 0,

	.curr_vcore_uv = 0,
	.curr_ddr_khz = 0,
};

int kicker_table[LAST_KICKER] __nosavedata;

static struct opp_profile opp_table[NUM_OPP] __nosavedata;

static char *kicker_name[] = {
	"KIR_MM",
	"KIR_DCS",
	"KIR_UFO",
	"KIR_UFS",
	"KIR_PERF",
	"KIR_ANC_MD32",
	"KIR_EFUSE",
	"KIR_PASR",
	"KIR_SDIO",
	"KIR_USB",
	"KIR_SYSFS",
	"KIR_SYSFSX",
	"NUM_KICKER",

	"KIR_LATE_INIT",
	"KIR_AUTOK_EMMC",
	"KIR_AUTOK_SDIO",
	"KIR_AUTOK_SD",
	"LAST_KICKER",
};

void vcorefs_update_opp_table(void)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int opp;

	mutex_lock(&governor_mutex);
	for (opp = 0; opp < NUM_OPP; opp++)
		opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);

	mutex_unlock(&governor_mutex);
}

/*
 * Governor extern API
 */
bool is_vcorefs_feature_enable(void)
{
#if 1
	if (!dram_can_support_fh()) {
		vcorefs_err("DISABLE DVFS DUE TO NOT SUPPORT DRAM FH\n");
		return false;
	}
#endif
	if (!vcorefs_vcore_dvs_en() && !vcorefs_dram_dfs_en()) {
		vcorefs_err("DISABLE DVFS DUE TO BOTH DVS & DFS DISABLE\n");
		return false;
	}
	if (0) {
		vcorefs_err("DISABLE DVFS DUE TO NOT SPM MODE(PHYPLL)\n");
		return false;
	}

	return true;
}

bool vcorefs_vcore_dvs_en(void)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	return gvrctrl->vcore_dvs;
}

bool vcorefs_dram_dfs_en(void)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	return gvrctrl->ddr_dfs;
}

int vcorefs_enable_debug_isr(bool enable)
{
	int flag;
	struct governor_profile *gvrctrl = &governor_ctrl;

	vcorefs_crit("enable_debug_isr: %d\n", enable);

	mutex_lock(&governor_mutex);

	gvrctrl->isr_debug = enable;

	flag = spm_dvfs_flag_init();

	if (enable)
		flag |= SPM_FLAG_EN_MET_DBG_FOR_VCORE_DVFS;

	spm_go_to_vcorefs(flag);

	mutex_unlock(&governor_mutex);

	return 0;
}

int vcorefs_get_num_opp(void)
{
	return NUM_OPP;
}

int vcorefs_get_hw_opp(void)
{
	return spm_vcorefs_get_opp();
}

int vcorefs_get_sw_opp(void)
{
	return vcorefs_sw_opp;
}

int vcorefs_get_curr_vcore(void)
{
#if 1
	int vcore = VCORE_INVALID;

	pwrap_read(PMIC_VCORE_ADDR, &vcore);
	if (vcore >= VCORE_INVALID)
		pwrap_read(PMIC_VCORE_ADDR, &vcore);

	return vcore < VCORE_INVALID ? vcore_pmic_to_uv(vcore) : 0;
#else
	return 0;
#endif
}

int vcorefs_get_curr_ddr(void)
{
#if 1
	int ddr_khz;

	ddr_khz = get_dram_data_rate() * 1000;

	return ddr_khz;
#else
	return 0;
#endif
}

int vcorefs_get_vcore_by_steps(u32 opp)
{
#if 1
	return vcore_pmic_to_uv(get_vcore_ptp_volt(opp));
#else
	return 0;
#endif
}

int vcorefs_get_ddr_by_steps(u32 opp)
{
#if 1
	int ddr_khz;

	ddr_khz = dram_steps_freq(opp) * 1000;

	return ddr_khz;
#else
	return 0;
#endif
}

char *governor_get_kicker_name(int id)
{
	return kicker_name[id];
}

char *vcorefs_get_opp_table_info(char *p)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int i;
	char *buff_end = p + PAGE_SIZE;

	for (i = 0; i < NUM_OPP; i++) {
		p += snprintf(p, buff_end - p, "[OPP%d] vcore_uv: %d (0x%x)\n", i, opp_ctrl_table[i].vcore_uv,
			     vcore_uv_to_pmic(opp_ctrl_table[i].vcore_uv));
		p += snprintf(p, buff_end - p, "[OPP%d] ddr_khz : %d\n", i, opp_ctrl_table[i].ddr_khz);
		p += snprintf(p, buff_end - p, "\n");
	}

	for (i = 0; i < NUM_OPP; i++)
		p += snprintf(p, buff_end - p, "OPP%d  : %u\n", i, opp_ctrl_table[i].vcore_uv);

	return p;
}

int vcorefs_output_kicker_id(char *name)
{
	int i;

	for (i = 0; i < LAST_KICKER; i++) {
		if (!strcmp(kicker_name[i], name))
			return i;
	}

	return -1;
}

static int set_init_opp_index(void)
{
	/* add condition here for diff late_init_opp */
	struct governor_profile *gvrctrl = &governor_ctrl;

	return gvrctrl->late_init_opp;
}

static void set_vcorefs_en(void)
{
	int flag;

	mutex_lock(&governor_mutex);
	flag = spm_dvfs_flag_init();
	spm_go_to_vcorefs(flag);
	mutex_unlock(&governor_mutex);
}

int governor_debug_store(const char *buf)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	int val, r = 0;
	char cmd[32];

	if (sscanf(buf, "%31s %d", cmd, &val) != 2)
		return -EPERM;

	if (!strcmp(cmd, "vcore_dvs")) {
		gvrctrl->vcore_dvs = val;
		set_vcorefs_en();
	} else if (!strcmp(cmd, "ddr_dfs")) {
		gvrctrl->ddr_dfs = val;
		set_vcorefs_en();
	} else if (!strcmp(cmd, "isr_debug")) {
		vcorefs_enable_debug_isr(val);
	} else {
		r = -EPERM;
	}

	return r;
}

char *governor_get_dvfs_info(char *p)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	int uv = vcorefs_get_curr_vcore();
	char *buff_end = p + PAGE_SIZE;

	p += snprintf(p, buff_end - p, "sw_opp: %d\n", vcorefs_get_sw_opp());
	p += snprintf(p, buff_end - p, "hw_opp: %d\n", vcorefs_get_hw_opp());
	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p, "[vcore_dvs]: %d\n", gvrctrl->vcore_dvs);
	p += snprintf(p, buff_end - p, "[ddr_dfs  ]: %d\n", gvrctrl->ddr_dfs);
	p += snprintf(p, buff_end - p, "[isr_debug]: %d\n", gvrctrl->isr_debug);
	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p, "[vcore] uv : %u (0x%x)\n", uv, vcore_uv_to_pmic(uv));
	p += snprintf(p, buff_end - p, "[ddr  ] khz: %u\n", vcorefs_get_curr_ddr());

	return p;
}

static int set_dvfs_with_opp(struct kicker_config *krconf)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	struct opp_profile *opp_ctrl_table = opp_table;
	int r = 0;

	gvrctrl->curr_vcore_uv = vcorefs_get_curr_vcore();
	gvrctrl->curr_ddr_khz = vcorefs_get_curr_ddr();

	vcorefs_crit_mask(log_mask(), krconf->kicker, "opp: %d, vcore: %u <= %u, fddr: %u <= %u %s%s\n",
			krconf->dvfs_opp,
			opp_ctrl_table[krconf->dvfs_opp].vcore_uv, gvrctrl->curr_vcore_uv,
			opp_ctrl_table[krconf->dvfs_opp].ddr_khz, gvrctrl->curr_ddr_khz,
			(gvrctrl->vcore_dvs) ? "[O]" : "[X]",
			(gvrctrl->ddr_dfs) ? "[O]" : "[X]");

	if (!gvrctrl->vcore_dvs && !gvrctrl->ddr_dfs)
		return 0;

	r = spm_set_vcore_dvfs(krconf);

	gvrctrl->curr_vcore_uv = opp_ctrl_table[krconf->dvfs_opp].vcore_uv;
	gvrctrl->curr_ddr_khz = opp_ctrl_table[krconf->dvfs_opp].ddr_khz;

	return r;
}

int kick_dvfs_by_opp_index(struct kicker_config *krconf)
{
	int r = 0;

	r = set_dvfs_with_opp(krconf);

	vcorefs_sw_opp = krconf->dvfs_opp;

	return r;
}

int vcorefs_late_init_dvfs(void)
{
	struct kicker_config krconf;
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	gvrctrl->late_init_opp = set_init_opp_index();

	krconf.kicker = KIR_LATE_INIT;
	krconf.opp = gvrctrl->late_init_opp;
	krconf.dvfs_opp = gvrctrl->late_init_opp;
	kick_dvfs_by_opp_index(&krconf);

	mutex_unlock(&governor_mutex);

	vcorefs_crit("[%s] late_init_opp: %d, sw_opp: %d (%d)\n", __func__,
				gvrctrl->late_init_opp, vcorefs_sw_opp, NUM_OPP);

	vcorefs_drv_init(gvrctrl->late_init_opp);

	return 0;
}

void vcorefs_init_opp_table(void)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	struct opp_profile *opp_ctrl_table = opp_table;
	int opp;

	mutex_lock(&governor_mutex);
	gvrctrl->curr_vcore_uv = vcorefs_get_curr_vcore();
	gvrctrl->curr_ddr_khz = vcorefs_get_curr_ddr();

	vcorefs_crit("curr_vcore_uv: %u, curr_ddr_khz: %u\n",
							gvrctrl->curr_vcore_uv,
							gvrctrl->curr_ddr_khz);

	for (opp = 0; opp < NUM_OPP; opp++) {
		opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);
		opp_ctrl_table[opp].ddr_khz = vcorefs_get_ddr_by_steps(opp);

		vcorefs_crit("opp %u: vcore_uv: %u, ddr_khz: %u\n", opp,
								opp_ctrl_table[opp].vcore_uv,
								opp_ctrl_table[opp].ddr_khz);
	}

	mt_eem_vcorefs_set_volt();
	mutex_unlock(&governor_mutex);
}

int vcorefs_module_init(void)
{
	int r;

	r = init_vcorefs_sysfs();
	if (r) {
		vcorefs_err("FAILED TO CREATE /sys/power/vcorefs (%d)\n", r);
		return r;
	}

	return r;
}

/*
 * AutoK related API
 */
void governor_autok_manager(void)
{
	int r;
	struct mmdvfs_prepare_action_event evt_from_vcore = {MMDVFS_EVENT_PREPARE_CALIBRATION_START};

	/* notify MM DVFS for msdc autok start */
	mmdvfs_notify_prepare_action(&evt_from_vcore);

	r = emmc_autok();
	vcorefs_crit("EMMC autok done: %s\n", (r == 0) ? "Yes" : "No");

	r = sd_autok();
	vcorefs_crit("SD autok done: %s\n", (r == 0) ? "Yes" : "No");

	r = sdio_autok();
	vcorefs_crit("SDIO autok done: %s\n", (r == 0) ? "Yes" : "No");
}

bool governor_autok_check(int kicker)
{
	int is_autok = true;
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	if (!((1U << kicker) & gvrctrl->autok_kir_group)) {
		is_autok = false;
	} else if (gvrctrl->active_autok_kir != 0 && gvrctrl->active_autok_kir != kicker) {
		vcorefs_err("Not allow kir:%d autok (other kir: %d on-going)\n", kicker,
							gvrctrl->active_autok_kir);
		is_autok = false;
	} else {
		is_autok = true;
	}
	mutex_unlock(&governor_mutex);

	return is_autok;
}

bool governor_autok_lock_check(int kicker, int opp)
{
	bool lock_r = true;
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);

	if (gvrctrl->active_autok_kir == 0) {
		gvrctrl->active_autok_kir = kicker;
		lock_r = true;	/* start autok */
	} else if (kicker == gvrctrl->active_autok_kir) {
		lock_r = true;	/* continue autok */
	} else {
		WARN_ON(1);
	}

	if (opp == OPP_UNREQ) {
		gvrctrl->active_autok_kir = 0;
		lock_r = false;
	}
	mutex_unlock(&governor_mutex);
	return lock_r;
}
