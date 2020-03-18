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
#include <linux/rtc.h>
#include <linux/timer.h>
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "mdee_ctl.h"
#include "mdee_dumper_v3.h"
#include "ccci_platform.h"


#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#ifndef DB_OPT_DEFAULT
#define DB_OPT_DEFAULT    (0)	/* Dummy macro define to avoid build error */
#endif

#ifndef DB_OPT_FTRACE
#define DB_OPT_FTRACE   (0)	/* Dummy macro define to avoid build error */
#endif

static void ccci_aed_v3(struct md_ee *mdee, unsigned int dump_flag, char *aed_str, int db_opt)
{
	void *ex_log_addr = NULL;
	int ex_log_len = 0;
	void *md_img_addr = NULL;
	int md_img_len = 0;
	int info_str_len = 0;
	char *buff;		/*[AED_STR_LEN]; */
	char *img_inf;
	struct mdee_dumper_v3 *dumper = mdee->dumper_obj;
	int md_id = mdee->md_id;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	struct ccci_mem_layout *mem_layout = ccci_md_get_mem(mdee->md_obj);
#if defined(CONFIG_MTK_AEE_FEATURE)
	int md_dbg_dump_flag = ccci_md_get_dbg_dump_flag(mdee->md_obj);
#endif

	buff = kmalloc(AED_STR_LEN, GFP_ATOMIC);
	if (buff == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "Fail alloc Mem for buff!\n");
		goto err_exit1;
	}
	img_inf = ccci_get_md_info_str(md_id);
	if (img_inf == NULL)
		img_inf = "";
	info_str_len = strlen(aed_str);
	info_str_len += strlen(img_inf);

	if (info_str_len > AED_STR_LEN)
		buff[AED_STR_LEN - 1] = '\0';	/* Cut string length to AED_STR_LEN */
	snprintf(buff, AED_STR_LEN, "md%d:%s%s", md_id + 1, aed_str, img_inf);
	/* MD ID must sync with aee_dump_ccci_debug_info() */
 err_exit1:
	if (dump_flag & CCCI_AED_DUMP_CCIF_REG) {
		ex_log_addr = smem_layout->ccci_exp_smem_mdss_debug_vir;
		ex_log_len = smem_layout->ccci_exp_smem_mdss_debug_size;
		ccci_md_dump_info(mdee->md_obj, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF,
			smem_layout->ccci_exp_smem_base_vir + CCCI_SMEM_OFFSET_CCIF_SRAM,
			CCCC_SMEM_CCIF_SRAM_SIZE);
	}
	if (dump_flag & CCCI_AED_DUMP_EX_MEM) {
		ex_log_addr = smem_layout->ccci_exp_smem_mdss_debug_vir;
		ex_log_len = smem_layout->ccci_exp_smem_mdss_debug_size;
	}
	if (dump_flag & CCCI_AED_DUMP_EX_PKT) {
		ex_log_addr = (void *)dumper->ex_pl_info;
		ex_log_len = MD_HS1_FAIL_DUMP_SIZE;
	}
	if (dump_flag & CCCI_AED_DUMP_MD_IMG_MEM) {
		md_img_addr = (void *)mem_layout->md_region_vir;
		md_img_len = MD_IMG_DUMP_SIZE;
	}
#if defined(CONFIG_MTK_AEE_FEATURE)
	if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM))
		aed_md_exception_api(ex_log_addr, ex_log_len, md_img_addr, md_img_len, buff, db_opt);
	else
		aed_md_exception_api(NULL, 0, md_img_addr, md_img_len, buff, db_opt);
#endif
	kfree(buff);
	CCCI_ERROR_LOG(md_id, KERN, "ccci_aed_v3 end!\n");
}

static char mdee_more_inf_str[MD_EE_CASE_WDT + 1][64] = {
	"",
	"\nOnly EX case\n",
	"\nOnly EX_OK case\n",
	"\n[Others] May I-Bit dis too long\n",
	"\n[Others] May I-Bit dis too long\n",
	"\n[Others] MD long time no response\n",
	"\n[Others] May I-Bit dis too long\n",
	"\nOnly SWINT case\n",
	"\nSWINT missing case\n",
	"\n[Others] MD watchdog timeout interrupt\n"
};

static void mdee_output_debug_info_to_buf(struct md_ee *mdee, DEBUG_INFO_T *debug_info, char *ex_info)
{
	int md_id = mdee->md_id;
	struct ccci_mem_layout *mem_layout;

	switch (debug_info->type) {
	case MD_EX_CLASS_ASSET:
		/* assert: file name+line number+code*3 */
		snprintf(ex_info, EE_BUF_LEN_UMOLY,
			"(%s)\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n\n",
			debug_info->core_name, debug_info->name, debug_info->dump_assert.file_name,
			debug_info->dump_assert.line_num, debug_info->dump_assert.parameters[0],
			debug_info->dump_assert.parameters[1], debug_info->dump_assert.parameters[2]);
		CCCI_ERROR_LOG(md_id, KERN, "filename = %s\n", debug_info->dump_assert.file_name);
		CCCI_ERROR_LOG(md_id, KERN, "line = %d\n", debug_info->dump_assert.line_num);
		CCCI_ERROR_LOG(md_id, KERN, "para0 = 0x%08x, para1 = 0x%08x, para2 = 0x%08x\n",
			     debug_info->dump_assert.parameters[0],
			     debug_info->dump_assert.parameters[1], debug_info->dump_assert.parameters[2]);
		break;
	case MD_EX_CLASS_FATAL:
		/* fatal:  */
		snprintf(ex_info, EE_BUF_LEN_UMOLY,
			 "(%s)\n[%s] err_code1:0x%08X err_code2:0x%08X err_code3:0x%08X\n%s%s\n",
			 debug_info->core_name, debug_info->name, debug_info->dump_fatal.err_code1,
			 debug_info->dump_fatal.err_code2, debug_info->dump_fatal.err_code3,
			 debug_info->dump_fatal.offender, debug_info->dump_fatal.ExStr);
		if (debug_info->dump_fatal.err_code1 == 0x3104) {
			mem_layout = ccci_md_get_mem(mdee->md_obj);
			snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s, MD base = 0x%08X\n\n", ex_info,
				mdee->ex_mpu_string, (unsigned int)mem_layout->md_region_phy);
			memset(mdee->ex_mpu_string, 0x0, sizeof(mdee->ex_mpu_string));
		}
		CCCI_ERROR_LOG(md_id, KERN, "fatal error code 1,2,3 = [0x%08X, 0x%08X, 0x%08X]%s\n",
			debug_info->dump_fatal.err_code1, debug_info->dump_fatal.err_code2,
			debug_info->dump_fatal.err_code3, debug_info->dump_fatal.offender);
		break;
	default:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[%s]\n", debug_info->core_name, debug_info->name);
		break;
	}


}

static void mdee_info_dump_v3(struct md_ee *mdee)
{
	int md_id = mdee->md_id;
	char *ex_info; /* aed api par4 */
	int db_opt = (DB_OPT_DEFAULT | DB_OPT_FTRACE); /* aed api par5 */
	int dump_flag = 0;
	char *i_bit_ex_info = NULL;
	char buf_fail[] = "Fail alloc mem for exception\n";
	struct mdee_dumper_v3 *dumper = mdee->dumper_obj;
	DEBUG_INFO_T *debug_info = &dumper->debug_info;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	int md_dbg_dump_flag = ccci_md_get_dbg_dump_flag(mdee->md_obj);

	ex_info = kmalloc(AED_STR_LEN, GFP_ATOMIC);
	if (ex_info == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "Fail alloc Mem for ex_info!\n");
		goto err_exit;
	}

	switch (dumper->more_info) {
	case MD_EE_CASE_ONLY_EX:
	case MD_EE_CASE_ONLY_EX_OK:
	case MD_EE_CASE_TX_TRG:
	case MD_EE_CASE_ISR_TRG:
	case MD_EE_CASE_ONLY_SWINT:
	case MD_EE_CASE_SWINT_MISSING:
		mdee_output_debug_info_to_buf(mdee, debug_info, ex_info);
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, mdee_more_inf_str[dumper->more_info]);
		break;
	case MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG:
		mdee_output_debug_info_to_buf(mdee, debug_info, ex_info);
		i_bit_ex_info = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
		if (i_bit_ex_info == NULL) {
			CCCI_ERROR_LOG(mdee->md_id, KERN, "Fail alloc Mem for i_bit_ex_info!\n");
			break;
		}
		snprintf(i_bit_ex_info, EE_BUF_LEN_UMOLY, mdee_more_inf_str[dumper->more_info], ex_info);
		strncpy(ex_info, i_bit_ex_info, EE_BUF_LEN_UMOLY);
		break;
	case MD_EE_CASE_NO_RESPONSE:
		/* use strncpy, otherwise if this happens after a MD EE, the former EE info will be printed out */
		db_opt |= DB_OPT_FTRACE;
		/* fall through */
	case MD_EE_CASE_WDT:
		strncpy(ex_info, mdee_more_inf_str[dumper->more_info], EE_BUF_LEN_UMOLY);
		break;
	default:
		mdee_output_debug_info_to_buf(mdee, debug_info, ex_info);
		break;
	}

	if (debug_info->ELM_status != NULL)
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, debug_info->ELM_status);/* ELM status */

	CCCI_MEM_LOG_TAG(md_id, KERN, "Dump MD EX log, 0x%x, 0x%x\n", dumper->more_info, debug_info->par_data_source);
	if (debug_info->par_data_source == MD_EE_DATA_IN_GPD) {
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, dumper->ex_pl_info, MD_HS1_FAIL_DUMP_SIZE);
		/* MD will not fill in share memory before we send runtime data */
		dump_flag = CCCI_AED_DUMP_EX_PKT;
	} else if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
		dump_flag = CCCI_AED_DUMP_EX_MEM;
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					smem_layout->ccci_exp_smem_ccci_debug_vir,
					smem_layout->ccci_exp_smem_ccci_debug_size);
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					smem_layout->ccci_exp_smem_mdss_debug_vir,
					smem_layout->ccci_exp_smem_mdss_debug_size);
	}

err_exit:
	/* update here to maintain handshake stage info during exception handling */
	if (debug_info->ex_type == CC_C2K_EXCEPTION)
		CCCI_NORMAL_LOG(md_id, KERN, "C2K EE, No need trigger DB\n");
	else if (ex_info == NULL)
		ccci_aed_v3(mdee, dump_flag, buf_fail, db_opt);
	else
		ccci_aed_v3(mdee, dump_flag, ex_info, db_opt);

	kfree(ex_info);
	kfree(i_bit_ex_info);

}

static void strmncopy(char *src, char *dst, int src_len, int dst_len)
{
	int temp_m, temp_n, temp_i;

	temp_m = src_len - 1;
	temp_n = dst_len - 1;
	temp_n = (temp_m > temp_n) ? temp_n : temp_m;
	for (temp_i = 0; temp_i < temp_n; temp_i++) {
		dst[temp_i] = src[temp_i];
		if (dst[temp_i] == 0x00)
			break;
	}
}

static EX_OVERVIEW_T *md_ee_get_buf_ptr(struct md_ee *mdee, u8 *buf_type)
{
	unsigned int ccif_sram[CCCC_SMEM_CCIF_SRAM_SIZE/4] = { 0 };
	struct mdee_dumper_v3 *dumper = mdee->dumper_obj;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	EX_OVERVIEW_T *tar_ptr;

	if (mdee->md_id == MD_SYS1)
		ccci_md_dump_info(mdee->md_obj, DUMP_FLAG_CCIF, ccif_sram, 0);
	if (((dumper->more_info == MD_EE_CASE_NORMAL) && (mdee->ee_info_flag&MD_EE_DUMP_IN_GPD))
		|| ((mdee->md_id == MD_SYS1) && (ccif_sram[1] < 0x208) && (ccif_sram[1] >= 0x200))) {
		*buf_type = MD_EE_DATA_IN_GPD;
		tar_ptr = (EX_OVERVIEW_T *)dumper->ex_pl_info;
		CCCI_NORMAL_LOG(mdee->md_id, KERN, "parsing data from: GPD %p, %p\n", tar_ptr, dumper->ex_pl_info);
	} else {
		tar_ptr = (EX_OVERVIEW_T *)smem_layout->ccci_exp_smem_mdss_debug_vir;
		*buf_type = MD_EE_DATA_IN_SMEM;
	}
	return tar_ptr;

}

static int mdee_set_core_name(int md_id, char *core_name, EX_OVERVIEW_T *ex_overview)
{
	int core_id;
	u8 temp_sys_inf_1, temp_sys_inf_2;
	EX_BRIEF_MAININFO_T *brief_info;

	for (core_id = 0; core_id < ex_overview->core_num; core_id++) {
		if (ex_overview->main_reson[core_id].is_offender) {
			strmncopy(ex_overview->main_reson[core_id].core_name, core_name,
				sizeof(ex_overview->main_reson[core_id].core_name),
				MD_CORE_NAME_DEBUG);
			break; /* just show one core name */
		}
	}

	if (core_id == 0) {
		brief_info =  &ex_overview->ex_info;
		temp_sys_inf_1 = brief_info->system_info1;
		temp_sys_inf_2 = brief_info->system_info2;
		snprintf(core_name, MD_CORE_NAME_DEBUG, "%s_core%d,vpe%d,tc%d(VPE%d)", core_name,
			(temp_sys_inf_1>>1), (temp_sys_inf_1&0x1), temp_sys_inf_2, temp_sys_inf_1);
	}
	CCCI_NORMAL_LOG(md_id, KERN, "brief_info: core_name = %s", core_name);
	return core_id;
}

static char *ee_type_str_v3_23h[] = {
	[INTERRUPT_EXCEPTION] = "Fatal error(INTERRUPT)",
	[TLB_MOD_EXCEPTION] = "Fatal error(TLB_MOD)",
	[TLB_MISS_LOAD_EXCEPTION] = "Fatal error(TLB_MISS_LOAD)",
	[TLB_MISS_STORE_EXCEPTION] = "Fatal error(TLB_MISS_STORE)",
	[ADDRESS_ERROR_LOAD_EXCEPTION] = "Fatal error(ADDRESS_ERROR_LOAD)",
	[ADDRESS_ERROR_STORE_EXCEPTION] = "Fatal error(ADDRESS_ERROR_STORE)",
	[INSTR_BUS_ERROR] = "Fatal error(INSTR_BUS_ERROR)",
	[DATA_BUS_ERROR] = "Fatal error(DATA_BUS_ERROR)",
	[SYSTEM_CALL_EXCEPTION] = "Fatal error(SYSTEM_CALL)",
	[BREAKPOINT_EXCEPTION] = "Fatal error(BREAKPOINT)",
	[RESERVED_INSTRUCTION_EXCEPTION] = "Fatal error(RESERVED_INSTRUCTION)",
	[COPROCESSORS_UNUSABLE_EXCEPTION] = "Fatal error(COPROCESSORS_UNUSABLE)",
	[INTEGER_OVERFLOW_EXCEPTION] = "Fatal error(INTEGER_OVERFLOW)",
	[TRAP_EXCEPTION] = "Fatal error(TRAP)",
	[MSA_FLOATING_POINT_EXCEPTION] = "Fatal error(MSA_FLOATING_POINT)",
	[FLOATING_POINT_EXCEPTION] = "Fatal error(FLOATING_POINT)",
	[COPROCESSOR_2_IS_1_EXCEPTION] = "Fatal error(COPROCESSOR_2_IS_1)",
	[COR_EXTEND_UNUSABLE_EXCEPTION] = "Fatal error(COR_EXTEND_UNUSABLE)",
	[COPROCESSOR_2_EXCEPTION] = "Fatal error(COPROCESSOR_2)",
	[TLB_READ_INHIBIT_EXCEPTION] = "Fatal error(TLB_READ_INHIBIT)",
	[TLB_EXECUTE_INHIBIT_EXCEPTION] = "Fatal error(TLB_EXECUTE_INHIBIT)",
	[MSA_UNUSABLE_EXCEPTION] = "Fatal error(MSA_UNUSABLE)",
	[MDMX_EXCEPTION] = "Fatal error(MDMX)",
	[WATCH_EXCEPTION] = "Fatal error(WATCH)",
	[MCHECK_EXCEPTION] = "Fatal error(MCHECK)",
	[THREAD_EXCEPTION] = "Fatal error(THREAD)",
	[DSP_UNUSABLE_EXCEPTION] = "Fatal error(DSP_UNUSABLE)",
	[RESERVED_27_EXCEPTION] = "Fatal error(RESERVED_27)",
	[RESERVED_28_EXCEPTION] = "Fatal error(RESERVED_28)",
	[RESERVED_29_EXCEPTION] = "Fatal error(RESERVED_29)",
	[CACHE_ERROR_EXCEPTION_DBG_MODE] = "Fatal error(CACHE_ERROR_DBG_MODE)",
	[RESERVED_31_EXCEPTION] = "Fatal error(RESERVED_31)",
	[NMI_EXCEPTION] = "Fatal error(NMI)",
	[CACHE_ERROR_EXCEPTION] = "Fatal error(CACHE_ERROR)",
	[TLB_REFILL_LOAD_EXCEPTION] = "Fatal error(TLB_REFILL_LOAD)",
	[TLB_REFILL_STORE_EXCEPTION] = "Fatal error(TLB_REFILL_STORE)"
};

static char *ee_type_str_v3_30s[] = {
	/*[STACKACCESS_EXCEPTION - 0x30] = */"Fatal error(STACKACCESS)",
	/*[SYS_FATALERR_EXT_TASK_EXCEPTION - 0x30] = */"Fatal error(task)",
	/*[SYS_FATALERR_EXT_BUF_EXCEPTION - 0x30] = */"Fatal error(buf)"
};

static char *ee_type_str_v3_50s[] = {
	/*[ASSERT_FAIL_EXCEPTION - 0x50] = */"ASSERT",
	/*[ASSERT_DUMP_EXTENDED_RECORD - 0x50] = */"ASSERT",
	/*[ASSERT_FAIL_NATIVE - 0x50] = */"ASSERT"
};

static char *ee_type_str_v3_60s[] = {
	/*[CC_INVALID_EXCEPTION - 0x60] = */"CC_INVALID",
	/*[CC_CS_EXCEPTION- 0x60] = */"Fatal error (CC_CS)",
	/*[CC_MD32_EXCEPTION- 0x60] = */"Fatal error (CC_MD32)",
	/*[CC_C2K_EXCEPTION- 0x60] = */"Fatal error (CC_C2K)",
	/*[CC_VOLTE_EXCEPTION- 0x60] = */"Fatal error (CC_VOLTE)",
};

static void md_ee_set_exp_type(EX_BRIEF_MAININFO_T *brief_info, int core_id, char **ex_str)
{
	u32 exp_type = brief_info->ex_type;

	if (core_id == 0) {
		if (exp_type < TLB_REFILL_MAX_NUM)
			*ex_str = ee_type_str_v3_23h[exp_type];
		else if ((exp_type < SYS_FATALERR_MAX_NUM) && (exp_type >= STACKACCESS_EXCEPTION))
			*ex_str = ee_type_str_v3_30s[exp_type - STACKACCESS_EXCEPTION];
		else if ((exp_type < ASSERT_FAIL_MAX_NUM) && (exp_type >= ASSERT_FAIL_EXCEPTION))
			*ex_str = ee_type_str_v3_50s[exp_type - ASSERT_FAIL_EXCEPTION];
		else if ((exp_type < CC_EXCEPTION_MAX_NUM) && (exp_type >= CC_INVALID_EXCEPTION))
			*ex_str = ee_type_str_v3_60s[exp_type - CC_INVALID_EXCEPTION];
		else if (exp_type == EMI_MPU_VIOLATION_EXCEPTION)
			*ex_str = "Fatal error (rmpu violation)";
		else
			*ex_str = "INVALID_EXCEPTION_TYPE";
	} else {
		if (brief_info->maincontent_type == MD_EX_CLASS_ASSET)
			*ex_str = "ASSERT";
		else if (brief_info->maincontent_type == MD_EX_CLASS_FATAL)
			*ex_str = "Fatal error";
		else
			*ex_str = "INVALID_EXCEPTION_TYPE";
	}

}

static void md_ee_set_assert_para(EX_ASSERT_V3_T *assert_src, DUMP_INFO_ASSERT *assert_tar)
{
	/* 1. assert file name */
	strmncopy(assert_src->filepath, assert_tar->file_name,
		sizeof(assert_src->filepath), sizeof(assert_tar->file_name));
	/* 2. assert line number */
	assert_tar->line_num = assert_src->line_number;
	/* 3. assert parameters */
	assert_tar->parameters[0] = assert_src->para1;
	assert_tar->parameters[1] = assert_src->para2;
	assert_tar->parameters[2] = assert_src->para3;
}

static void md_ee_set_fatal_para(EX_FATAL_V3_T *fatal_src, DUMP_INFO_FATAL *fata_tar)
{
	char temp_str[32] = { 0 };
	/* 1. offender string */
	if ((fatal_src->offender[0] != 0xCC) && (fatal_src->offender[0] != 0)) {
		strmncopy(fatal_src->offender, temp_str,
			sizeof(fatal_src->offender), sizeof(temp_str));
		snprintf(fata_tar->offender, sizeof(fata_tar->offender),
			"MD Offender:%s\n", temp_str);
		CCCI_NORMAL_LOG(-1, KERN, "offender: %s\n",
			     fata_tar->offender);
	} else
		fata_tar->offender[0] = '\0';
	/* 2. error code of fatal error */
	fata_tar->err_code1 = fatal_src->code1;
	fata_tar->err_code2 = fatal_src->code2;
	fata_tar->err_code3 = fatal_src->code3;
	/* 3. cadefa support */
	if (fatal_src->is_cadefa_supported == 0x01)
		fata_tar->ExStr = "CaDeFa Supported\n";
	else
		fata_tar->ExStr = "";
}

static void mdee_info_prepare_v3(struct md_ee *mdee)
{
	EX_OVERVIEW_T *ex_overview;
	EX_BRIEF_MAININFO_T *brief_info;
	struct mdee_dumper_v3 *dumper = mdee->dumper_obj;
	DEBUG_INFO_T *debug_info = &dumper->debug_info;
	int md_id = mdee->md_id;
	int core_id = 0;

	CCCI_NORMAL_LOG(md_id, KERN, "mdee_info_prepare_v3, ee_case(0x%x)\n", dumper->more_info);

	memset(debug_info, 0, sizeof(DEBUG_INFO_T));
	if (dumper->more_info == MD_EE_CASE_NO_RESPONSE || dumper->more_info == MD_EE_CASE_WDT)
		return;
	ex_overview = md_ee_get_buf_ptr(mdee, &debug_info->par_data_source);/* mem of parsing */
	/* version: 0xABxxyyyy: xx rule version for AP parsing, yyyy for md parsing */
	if ((ex_overview->overview_verno&0xFFFF0000) != 0xAB000000 || ex_overview->core_num > MD_CORE_TOTAL_NUM) {
		debug_info->type = MD_EX_CLASS_INVALID;
		snprintf(debug_info->core_name, sizeof(debug_info->core_name), "%s", "MCU_core0,vpe0,tc0(VPE0)");
		debug_info->name = "INVALID_EXCEPTION_TYPE";
		return;
	}
	core_id = mdee_set_core_name(md_id, debug_info->core_name, ex_overview);/* core_name */
	/* ======== exception type ========= */
	brief_info = &ex_overview->ex_info;
	debug_info->type = brief_info->maincontent_type;
	debug_info->ex_type = brief_info->ex_type;
	md_ee_set_exp_type(brief_info, core_id, &debug_info->name);
	/* ======== exception parameters ========= */
	switch (debug_info->type) {
	case MD_EX_CLASS_ASSET:
		md_ee_set_assert_para(&brief_info->info.assert, &debug_info->dump_assert);
		break;
	case MD_EX_CLASS_FATAL:
		md_ee_set_fatal_para(&brief_info->info.fatalerr, &debug_info->dump_fatal);
		break;
	}
	/* ======== ELM status ========= */
	switch (brief_info->elm_status) {
	case 0xFF:
		debug_info->ELM_status = "\nno ELM info\n";
		break;
	case 0xAE:
		debug_info->ELM_status = "\nELM rlat:FAIL\n";
		break;
	case 0xBE:
		debug_info->ELM_status = "\nELM wlat:FAIL\n";
		break;
	case 0xDE:
		debug_info->ELM_status = "\nELM r/wlat:PASS\n";
		break;
	default:
		debug_info->ELM_status = "";
		break;
	}

}

static void mdee_dumper_v3_set_ee_pkg(struct md_ee *mdee, char *data, int len)
{
	struct mdee_dumper_v3 *dumper = mdee->dumper_obj;
	int cpy_len = len > MD_HS1_FAIL_DUMP_SIZE ? MD_HS1_FAIL_DUMP_SIZE : len;

	memcpy(dumper->ex_pl_info, data, cpy_len);
}
static void mdee_dumper_v3_dump_ee_info(struct md_ee *mdee, MDEE_DUMP_LEVEL level, int more_info)
{
	struct mdee_dumper_v3 *dumper = mdee->dumper_obj;
	int md_id = mdee->md_id;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	int md_state = ccci_md_get_state(mdee->md_obj);
	char ex_info[EE_BUF_LEN] = {0};
	int md_dbg_dump_flag = ccci_md_get_dbg_dump_flag(mdee->md_obj);

	dumper->more_info = more_info;
	if (level == MDEE_DUMP_LEVEL_BOOT_FAIL) {
		if (md_state == BOOT_WAITING_FOR_HS1) {
			snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", 1);
			/* Handshake 1 fail */
			ccci_aed_v3(mdee, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_MD_IMG_MEM | CCCI_AED_DUMP_EX_MEM,
					ex_info, DB_OPT_DEFAULT);
		} else if (md_state == BOOT_WAITING_FOR_HS2) {
			snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", 2);
			/* Handshake 2 fail */
			CCCI_NORMAL_LOG(md_id, KERN, "Dump MD EX log\n");
			if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM))
				ccci_mem_dump(md_id, smem_layout->ccci_exp_smem_base_vir,
							smem_layout->ccci_exp_dump_size);

			ccci_aed_v3(mdee, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_EX_MEM, ex_info, DB_OPT_FTRACE);
		}
	} else if (level == MDEE_DUMP_LEVEL_TIMER1) {
		if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
			ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					smem_layout->ccci_exp_smem_mdss_debug_vir,
					smem_layout->ccci_exp_smem_mdss_debug_size);
		}
	} else if (level == MDEE_DUMP_LEVEL_TIMER2) {
		mdee_info_prepare_v3(mdee);
		mdee_info_dump_v3(mdee);
	}
}

static struct md_ee_ops mdee_ops_v3 = {
	.dump_ee_info = &mdee_dumper_v3_dump_ee_info,
	.set_ee_pkg = &mdee_dumper_v3_set_ee_pkg,
};
int mdee_dumper_v3_alloc(struct md_ee *mdee)
{
	struct mdee_dumper_v3 *dumper;
	int md_id = mdee->md_id;

	/* Allocate port_proxy obj and set all member zero */
	dumper = kzalloc(sizeof(struct mdee_dumper_v3), GFP_KERNEL);
	if (dumper == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "%s:alloc mdee_parser_v3 fail\n", __func__);
		return -1;
	}
	mdee->dumper_obj = dumper;
	mdee->ops = &mdee_ops_v3;
	return 0;
}

