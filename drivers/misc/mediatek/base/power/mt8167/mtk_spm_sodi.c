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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mt-plat/mtk_cirq.h>
#include "mtk_cpuidle.h"
#include "mtk_spm_idle.h"
#include "mtk_spm_internal.h"
#include "mtk_spm_pcm.h"

/**************************************
 * only for internal debug
 **************************************/
#define SODI_TAG     "[SODI] "
#define sodi_warn(fmt, args...)		pr_warn(SODI_TAG fmt, ##args)
#define sodi_debug(fmt, args...)	pr_debug(SODI_TAG fmt, ##args)

#define SPM_BYPASS_SYSPWREQ         0	/* JTAG is used */

#define REDUCE_SODI_LOG             1
#if REDUCE_SODI_LOG
#define LOG_BUF_SIZE					256
#define SODI_LOGOUT_INTERVAL_CRITERIA	(5000U)	/* unit:ms */
#define SODI_LOGOUT_TIMEOUT_CRITERA		20
#define SODI_CONN2AP_CNT_CRITERA		20
#endif

#define SPM_USE_TWAM_DEBUG	        0

#define WAKE_SRC_FOR_SODI \
	(WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_EINT | WAKE_SRC_WDT | \
	WAKE_SRC_CONN2AP | WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_AFE | \
	WAKE_SRC_CIRQ | WAKE_SRC_SYSPWREQ | WAKE_SRC_CONN_WDT)

#ifdef CONFIG_MTK_RAM_CONSOLE
#define SPM_AEE_RR_REC 1
#else
#define SPM_AEE_RR_REC 0
#endif

#if SPM_AEE_RR_REC
enum spm_sodi_step {
	SPM_SODI_ENTER = 0,
	SPM_SODI_ENTER_SPM_FLOW,
	SPM_SODI_ENTER_WFI,
	SPM_SODI_LEAVE_WFI,
	SPM_SODI_LEAVE_SPM_FLOW,
	SPM_SODI_LEAVE,
	SPM_SODI_VCORE_HPM,
	SPM_SODI_VCORE_LPM
};
#endif

static struct pwr_ctrl sodi_ctrl = {
	.wake_src		= WAKE_SRC_FOR_SODI,
	.r0_ctrl_en		= 1,
	.r7_ctrl_en		= 1,
	.infra_dcm_lock = 1,
	.wfi_op			= WFI_OP_AND,
	.ca7_wfi0_en	= 1,
	.ca7_wfi1_en	= 1,
	.ca7_wfi2_en	= 1,
	.ca7_wfi3_en	= 1,
	.mfg_req_mask	= 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask	= 1,
#endif
};

struct spm_lp_scen __spm_sodi = {
	.pcmdesc = &suspend_pcm,
	.pwrctrl = &sodi_ctrl,
};

/* 0:power-down mode, 1:CG mode */
static bool gSpm_SODI_mempll_pwr_mode = 1;
static bool gSpm_sodi_en = 1;

#if REDUCE_SODI_LOG
static unsigned long int sodi_logout_prev_time;
static unsigned int sodi_cnt, timeout_cnt;
static unsigned int refresh_cnt, not_refresh_cnt;
static unsigned int by_conn2ap_count;

static const char *sodi_wakesrc_str[32] = {
	[0] = "SPM_MERGE",
	[1] = "AUDIO_REQ",
	[2] = "KP",
	[3] = "WDT",
	[4] = "GPT",
	[5] = "EINT",
	[6] = "CONN_WDT",
	[7] = "GCE",
	[9] = "LOW_BAT",
	[10] = "CONN2AP",
	[11] = "F26M_WAKE",
	[12] = "F26M_SLEE",
	[13] = "PCM_WDT",
	[14] = "USB_CD ",
	[15] = "USB_PDN",
	[18] = "DBGSYS",
	[19] = "UART0",
	[20] = "AFE",
	[21] = "THERM",
	[22] = "CIRQ",
	[23] = "SEJ",
	[24] = "SYSPWREQ",
	[26] = "CPU0_IRQ",
	[27] = "CPU1_IRQ",
	[28] = "CPU2_IRQ",
	[29] = "CPU3_IRQ",
	[30] = "APSRC_WAKE",
	[31] = "APSRC_SLEEP",
};
#endif

#if SPM_AEE_RR_REC
void __attribute__((weak)) aee_rr_rec_sodi_val(u32 val)
{
}

u32 __attribute__((weak)) aee_rr_curr_sodi_val(void)
{
	return 0;
}
#endif

#if REDUCE_SODI_LOG
static long int idle_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}
#endif

static void spm_trigger_wfi_for_sodi(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags))
		mt_cpu_dormant(CPU_SODI_MODE);
	else
		wfi_with_sync();
}

static void spm_sodi_pre_process(void)
{
	/* set PMIC WRAP table for deepidle power control */
	__spm_set_pmic_phase(PMIC_WRAP_PHASE_SODI);
}

#if 0
static void spm_sodi_post_process(void)
{
	/* set PMIC WRAP table for normal power control */
	__spm_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
}
#endif

void spm_go_to_sodi(u32 spm_flags, u32 spm_data)
{
	struct wake_status wakesta;
	unsigned long flags;
	struct mtk_irq_mask mask;
	struct irq_desc *desc = irq_to_desc(spm_irq_0);
	wake_reason_t wr = WR_NONE;
	struct pcm_desc *pcmdesc = __spm_sodi.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_sodi.pwrctrl;
#if REDUCE_SODI_LOG
	unsigned long int sodi_logout_curr_time = 0;
	int need_log_out = 0;
#endif

#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(1 << SPM_SODI_ENTER);
#endif

	spm_flags |= (SPM_BUS26M_DIS | SPM_MPLLOFF_DIS | SPM_FHC_SLEEP_DIS | SPM_26M_DIS | SPM_MCU_PDN_DIS);

	if (gSpm_SODI_mempll_pwr_mode == 1)
		spm_flags |= SPM_DDRPHY_S1_DIS;	/* MEMPLL CG mode */
	else
		spm_flags &= ~SPM_DDRPHY_S1_DIS;	/* DDRPHY power down mode */

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	/* enable APxGPT timer */
	soidle_before_wfi(0);

	lockdep_off();
	spin_lock_irqsave(&__spm_lock, flags);

	mt_irq_mask_all(&mask);
	if (desc)
		unmask_irq(desc);
	mt_cirq_clone_gic();
	mt_cirq_enable();

#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(aee_rr_curr_sodi_val() | (1 << SPM_SODI_ENTER_SPM_FLOW));
#endif

#if CONFIG_SUPPORT_PCM_ALLINONE
	if (!__spm_is_pcm_loaded())
		__spm_init_pcm_AllInOne(pcmdesc);

	/* Display set SPM_PCM_SRC_REQ[0]=1'b1 to force DRAM not enter self-refresh mode */
	if ((spm_read(SPM_PCM_SRC_REQ) & 0x00000001))
		pwrctrl->pcm_apsrc_req = 1;
	else
		pwrctrl->pcm_apsrc_req = 0;

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	spm_sodi_pre_process();

	__spm_kick_pcm_to_run(pwrctrl);

	__spm_set_pcm_cmd(PCM_CMD_SUSPEND_PCM);
#else
	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	/* Display set SPM_PCM_SRC_REQ[0]=1'b1 to force DRAM not enter self-refresh mode */
	if ((spm_read(SPM_PCM_SRC_REQ) & 0x00000001))
		pwrctrl->pcm_apsrc_req = 1;
	else
		pwrctrl->pcm_apsrc_req = 0;

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	spm_sodi_pre_process();

	__spm_kick_pcm_to_run(pwrctrl);
#endif

#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(aee_rr_curr_sodi_val() | (1 << SPM_SODI_ENTER_WFI));
#endif

	spm_trigger_wfi_for_sodi(pwrctrl);

#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(aee_rr_curr_sodi_val() | (1 << SPM_SODI_LEAVE_WFI));
#endif

	/* spm_sodi_post_process(); */

	__spm_get_wakeup_status(&wakesta);

	__spm_clean_after_wakeup();

#if REDUCE_SODI_LOG == 0
	sodi_debug("pcm_flag = 0x%x, %s\n",
		   spm_read(SPM_PCM_FLAGS), pcmdesc->version);

	wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);
	if (wr == WR_PCM_ASSERT) {
		sodi_warn("PCM ASSERT AT %u (%s), r13 = 0x%x, debug_flag = 0x%x\n",
			 wakesta.assert_pc, pcmdesc->version, wakesta.r13, wakesta.debug_flag);
	} else if (wakesta.r12 == 0) {
		sodi_warn("Warning: not wakeup by SPM, r13 = 0x%x, debug_flag = 0x%x\n",
			 wakesta.r13, wakesta.debug_flag);
	}
#else
	sodi_logout_curr_time = idle_get_current_time_ms();

	if (wakesta.assert_pc != 0) {
		need_log_out = 1;
	} else if (wakesta.r12 == 0 && wakesta.debug_flag != 0x4000) {
		need_log_out = 2;
	} else if ((wakesta.r12 & (WAKE_SRC_GPT | WAKE_SRC_CONN2AP)) == 0) {
		/* not wakeup by GPT, CONN2AP and CLDMA_WDT */
		need_log_out = 3;
	} else if (wakesta.r12 & WAKE_SRC_CONN2AP) {
		/* wake up by WAKE_SRC_CONN2AP */
		if (by_conn2ap_count == 0)
			need_log_out = 4;

		if (by_conn2ap_count >= SODI_CONN2AP_CNT_CRITERA)
			by_conn2ap_count = 0;
		else
			by_conn2ap_count++;
	} else if ((sodi_logout_curr_time - sodi_logout_prev_time) > SODI_LOGOUT_INTERVAL_CRITERIA) {
		need_log_out = 5;
	}

	if (need_log_out) {
		sodi_logout_prev_time = sodi_logout_curr_time;

		if (need_log_out == 1 || need_log_out == 2) {
			sodi_warn("Warning: %s, self-refrsh = %d, pcm_flag = 0x%x\n",
			     (wakesta.assert_pc != 0) ? "wakeup by SPM assert" : "not wakeup by SPM",
			     wakesta.apsrc_cnt, spm_read(SPM_PCM_FLAGS));
			sodi_warn("sodi_cnt =%d, self-refresh_cnt = %d, spm_pc = 0x%0x, r13 = 0x%x, debug_flag = 0x%x\n",
			     sodi_cnt, refresh_cnt, wakesta.assert_pc,
			     wakesta.r13, wakesta.debug_flag);
			sodi_warn("r12 = 0x%x, raw_sta = 0x%x, idle_sta = 0x%x, event_reg = 0x%x, isr = 0x%x, %s\n",
				 wakesta.r12, wakesta.raw_sta, wakesta.idle_sta,
				 wakesta.event_reg, wakesta.isr, pcmdesc->version);
		} else {
			char buf[LOG_BUF_SIZE] = { 0 };
			int i, left_size;

			if (wakesta.r12 & WAKE_SRC_SPM_MERGE) {
				if (wakesta.wake_misc & WAKE_MISC_PCM_TIMER)
					strcat(buf, " PCM_TIMER");
				if (wakesta.wake_misc & WAKE_MISC_TWAM)
					strcat(buf, " TWAM");
				if (wakesta.wake_misc & WAKE_MISC_CPU_WAKE)
					strcat(buf, " CPU");
			}
			for (i = 1; i < 32; i++) {
				if (wakesta.r12 & (1U << i)) {
					left_size = sizeof(buf) - strlen(buf) - 1;
					if (left_size >= strlen(sodi_wakesrc_str[i]))
						strcat(buf, sodi_wakesrc_str[i]);
					else
						strncat(buf, sodi_wakesrc_str[i], left_size);

					wr = WR_WAKE_SRC;
				}
			}
			WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

			sodi_debug("%d, wakeup by %s, self-refresh=%d, pcm_flag=0x%x, debug_flag = 0x%x, timer_out=%u\n",
			     need_log_out, buf,
			     wakesta.apsrc_cnt,
			     spm_read(SPM_PCM_FLAGS), wakesta.debug_flag, wakesta.timer_out);

			sodi_debug("sodi_cnt=%u, not_refresh_cnt=%u, refresh_cnt=%u, avg_refresh_cnt=%u, timeout_cnt=%u, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			     sodi_cnt, not_refresh_cnt, refresh_cnt,
			     (refresh_cnt / ((sodi_cnt - not_refresh_cnt) ? (sodi_cnt - not_refresh_cnt) : 1)),
			     timeout_cnt,
			     wakesta.r12, wakesta.r13,
			     wakesta.raw_sta, wakesta.idle_sta, wakesta.event_reg, wakesta.isr);
		}

		sodi_cnt = 0;
		refresh_cnt = 0;
		not_refresh_cnt = 0;
		timeout_cnt = 0;
	} else {
		sodi_cnt++;
		refresh_cnt += wakesta.apsrc_cnt;

		if (wakesta.apsrc_cnt == 0)
			not_refresh_cnt++;

		if (wakesta.timer_out <= SODI_LOGOUT_TIMEOUT_CRITERA)
			timeout_cnt++;
	}

	spm_write(SPM_PCM_RESERVE7, 0);
#endif

#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(aee_rr_curr_sodi_val() | (1 << SPM_SODI_LEAVE_SPM_FLOW));
#endif

	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);

	spin_unlock_irqrestore(&__spm_lock, flags);
	lockdep_on();

	/* stop APxGPT timer and enable caore0 local timer */
	soidle_after_wfi(0);

#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(0);
#endif
}

void spm_sodi_mempll_pwr_mode(bool pwr_mode)
{
	/* printk("[SODI]set pwr_mode = %d\n",pwr_mode); */
	gSpm_SODI_mempll_pwr_mode = pwr_mode;
}

void spm_enable_sodi(bool en)
{
	gSpm_sodi_en = en;
}

bool spm_get_sodi_en(void)
{
	return gSpm_sodi_en;
}

#if SPM_AEE_RR_REC
static void spm_sodi_aee_init(void)
{
	aee_rr_rec_sodi_val(0);
}
#endif

#if SPM_USE_TWAM_DEBUG
#define SPM_TWAM_MONITOR_TICK 333333
static void twam_handler(struct twam_sig *twamsig)
{
	spm_crit("sig_high = %u%%  %u%%  %u%%  %u%%, r13 = 0x%x\n",
		 get_percent(twamsig->sig0, SPM_TWAM_MONITOR_TICK),
		 get_percent(twamsig->sig1, SPM_TWAM_MONITOR_TICK),
		 get_percent(twamsig->sig2, SPM_TWAM_MONITOR_TICK),
		 get_percent(twamsig->sig3, SPM_TWAM_MONITOR_TICK),
			spm_read(SPM_PCM_REG13_DATA));
}
#endif

void spm_sodi_init(void)
{
#if SPM_USE_TWAM_DEBUG
	unsigned long flags;
	struct twam_sig twamsig = {
		.sig0 = 10,	/* disp_req */
		.sig1 = 23,	/* self-refresh */
		.sig2 = 25,	/* md2_srcclkena */
		.sig3 = 21,	/* md2_apsrc_req_mux */
	};
#endif

#if SPM_AEE_RR_REC
	spm_sodi_aee_init();
#endif

#if SPM_USE_TWAM_DEBUG
#if 0
	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_AP_STANBY_CON, spm_read(SPM_AP_STANBY_CON) | ASC_MD_DDR_EN_SEL);
	spin_unlock_irqrestore(&__spm_lock, flags);
#endif

	spm_twam_register_handler(twam_handler);
	spm_twam_enable_monitor(&twamsig, false, SPM_TWAM_MONITOR_TICK);
#endif
}

MODULE_DESCRIPTION("SPM-SODI Driver v0.1");
