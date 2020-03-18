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

#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "mdee_ctl.h"
#include "mdee_dumper_v2.h"
#include "ccci_platform.h"

#ifndef DB_OPT_DEFAULT
#define DB_OPT_DEFAULT    (0)	/* Dummy macro define to avoid build error */
#endif

#ifndef DB_OPT_FTRACE
#define DB_OPT_FTRACE   (0)	/* Dummy macro define to avoid build error */
#endif
static void ccci_aed_v2(struct md_ee *mdee, unsigned int dump_flag, char *aed_str, int db_opt)
{
	void *ex_log_addr = NULL;
	int ex_log_len = 0;
	void *md_img_addr = NULL;
	int md_img_len = 0;
	int info_str_len = 0;
	char *buff;		/*[AED_STR_LEN]; */
	char *img_inf;
	struct mdee_dumper_v2 *dumper = mdee->dumper_obj;
	int md_id = mdee->md_id;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	struct ccci_mem_layout *mem_layout = ccci_md_get_mem(mdee->md_obj);
	int md_dbg_dump_flag = ccci_md_get_dbg_dump_flag(mdee->md_obj);

	buff = kmalloc(AED_STR_LEN, GFP_ATOMIC);
	if (buff == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "Fail alloc Mem for buff, %d!\n", md_dbg_dump_flag);
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
		ccci_md_dump_info(mdee->md_obj, DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG,
				   smem_layout->ccci_exp_smem_base_vir + CCCI_SMEM_OFFSET_CCIF_SRAM,
				   CCCC_SMEM_CCIF_SRAM_SIZE);
	}
	if (dump_flag & CCCI_AED_DUMP_EX_MEM) {
		ex_log_addr = smem_layout->ccci_exp_smem_mdss_debug_vir;
		ex_log_len = smem_layout->ccci_exp_smem_mdss_debug_size;
	}
	if (dump_flag & CCCI_AED_DUMP_EX_PKT) {
		ex_log_addr = (void *)dumper->ex_pl_info;
		ex_log_len = MD_HS1_FAIL_DUMP_SIZE/*sizeof(EX_PL_LOG_T)*/;
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
}

static void mdee_output_debug_info_to_buf(struct md_ee *mdee, DEBUG_INFO_T *debug_info, char *ex_info)
{
	int md_id = mdee->md_id;
	struct ccci_mem_layout *mem_layout;

	switch (debug_info->type) {
	case MD_EX_DUMP_ASSERT:
		CCCI_ERROR_LOG(md_id, KERN, "filename = %s\n", debug_info->assert.file_name);
		CCCI_ERROR_LOG(md_id, KERN, "line = %d\n", debug_info->assert.line_num);
		CCCI_ERROR_LOG(md_id, KERN, "para0 = %d, para1 = %d, para2 = %d\n",
			     debug_info->assert.parameters[0],
			     debug_info->assert.parameters[1], debug_info->assert.parameters[2]);
		snprintf(ex_info, EE_BUF_LEN_UMOLY,
				"%s\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n\n",
				debug_info->core_name,
				debug_info->name,
				debug_info->assert.file_name,
				debug_info->assert.line_num,
				debug_info->assert.parameters[0],
				debug_info->assert.parameters[1], debug_info->assert.parameters[2]);
		break;
	case MD_EX_DUMP_3P_EX:
	case MD_EX_CC_C2K_EXCEPTION:
		CCCI_ERROR_LOG(md_id, KERN, "fatal error code 1 = 0x%08X\n",
			     debug_info->fatal_error.err_code1);
		CCCI_ERROR_LOG(md_id, KERN, "fatal error code 2 = 0x%08X\n",
			     debug_info->fatal_error.err_code2);
		CCCI_ERROR_LOG(md_id, KERN, "fatal error code 3 = 0x%08X\n",
			     debug_info->fatal_error.err_code3);
		CCCI_ERROR_LOG(md_id, KERN, "fatal error offender %s\n", debug_info->fatal_error.offender);
		if (debug_info->fatal_error.offender[0] != '\0') {
			snprintf(ex_info, EE_BUF_LEN_UMOLY,
				 "%s\n[%s] err_code1:0x%08X err_code2:0x%08X erro_code3:0x%08X\nMD Offender:%s\n%s",
				 debug_info->core_name, debug_info->name, debug_info->fatal_error.err_code1,
				 debug_info->fatal_error.err_code2, debug_info->fatal_error.err_code3,
				 debug_info->fatal_error.offender, debug_info->fatal_error.ExStr);
		} else {
			snprintf(ex_info, EE_BUF_LEN_UMOLY,
				 "%s\n[%s] err_code1:0x%08X err_code2:0x%08X err_code3:0x%08X\n%s\n",
				 debug_info->core_name, debug_info->name, debug_info->fatal_error.err_code1,
				 debug_info->fatal_error.err_code2, debug_info->fatal_error.err_code3,
				 debug_info->fatal_error.ExStr);
		}
		if (debug_info->fatal_error.err_code1 == 0x3104) {
			mem_layout = ccci_md_get_mem(mdee->md_obj);
			snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s, MD base = 0x%08X\n\n", ex_info,
				mdee->ex_mpu_string, (unsigned int)mem_layout->md_region_phy);
			memset(mdee->ex_mpu_string, 0x0, sizeof(mdee->ex_mpu_string));
		}
		break;
	case MD_EX_DUMP_2P_EX:
		CCCI_ERROR_LOG(md_id, KERN, "fatal error code 1 = 0x%08X\n\n",
			     debug_info->fatal_error.err_code1);
		CCCI_ERROR_LOG(md_id, KERN, "fatal error code 2 = 0x%08X\n\n",
			     debug_info->fatal_error.err_code2);

		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[%s] err_code1:0x%08X err_code2:0x%08X\n\n",
			 debug_info->core_name, debug_info->name, debug_info->fatal_error.err_code1,
			 debug_info->fatal_error.err_code2);
		break;
	case MD_EX_DUMP_EMI_CHECK:
		CCCI_ERROR_LOG(md_id, KERN, "md_emi_check: 0x%08X, 0x%08X, %02d, 0x%08X\n\n",
			     debug_info->data.data0, debug_info->data.data1,
			     debug_info->data.channel, debug_info->data.reserved);
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[emi_chk] 0x%08X, 0x%08X, %02d, 0x%08X\n\n",
			 debug_info->core_name, debug_info->data.data0, debug_info->data.data1,
			 debug_info->data.channel, debug_info->data.reserved);
		break;
	case MD_EX_DUMP_UNKNOWN:
	default:	/* Only display exception name */
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[%s]\n", debug_info->core_name, debug_info->name);
		break;
	}
}
static void mdee_info_dump_v2(struct md_ee *mdee)
{
	char *ex_info;		/*[EE_BUF_LEN] = ""; */
	char *i_bit_ex_info = NULL;/*[EE_BUF_LEN] = "\n[Others] May I-Bit dis too long\n";*/
	char buf_fail[] = "Fail alloc mem for exception\n";
	int db_opt = (DB_OPT_DEFAULT | DB_OPT_FTRACE);
	int dump_flag = 0;
	int core_id;
	char *ex_info_temp = NULL;/*[EE_BUF_LEN] = "";*/
	DEBUG_INFO_T *debug_info = NULL;
	unsigned char c;
	struct mdee_dumper_v2 *dumper = mdee->dumper_obj;
	int md_id = mdee->md_id;
	EX_PL_LOG_T *ex_pl_info = (EX_PL_LOG_T *)dumper->ex_pl_info;
	int md_state = ccci_md_get_state(mdee->md_obj);
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct timeval tv_android = { 0 };
	struct rtc_time tm_android;
	int md_dbg_dump_flag = ccci_md_get_dbg_dump_flag(mdee->md_obj);

	ex_info = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
	if (ex_info == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "Fail alloc Mem for ex_info!\n");
		goto err_exit;
	}
	ex_info_temp = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
	if (ex_info_temp == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "Fail alloc Mem for ex_info_temp!\n");
		goto err_exit;
	}

	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);
	CCCI_ERROR_LOG(md_id, KERN, "Sync:%d%02d%02d %02d:%02d:%02d.%u(%02d:%02d:%02d.%03d(TZone))\n",
		     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     (unsigned int)tv.tv_usec,
		     tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec, (unsigned int)tv_android.tv_usec);
	for (core_id = 0; core_id < dumper->ex_core_num; core_id++) {
		if (core_id == 1)
			snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%s", ex_info);
		else if (core_id > 1)
			snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%smd%d:%s", ex_info_temp, md_id + 1, ex_info);
		debug_info = &dumper->debug_info[core_id];
		CCCI_ERROR_LOG(md_id, KERN, "exception type(%d):%s\n", debug_info->type,
			     debug_info->name ? : "Unknown");
		mdee_output_debug_info_to_buf(mdee, debug_info, ex_info);
		ccci_event_log("md%d %s\n", md_id+1, ex_info);
	}
	if (dumper->ex_core_num > 1) {
		CCCI_NORMAL_LOG(md_id, KERN, "%s+++++++%s", ex_info_temp, ex_info);
		snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%smd%d:%s", ex_info_temp, md_id + 1, ex_info);
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s", ex_info_temp);

		debug_info = &dumper->debug_info[0];
	} else if (dumper->ex_core_num == 0)
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "\n");
	/* Add additional info */
	switch (dumper->more_info) {
	case MD_EE_CASE_ONLY_SWINT:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nOnly SWINT case\n");
		break;
	case MD_EE_CASE_SWINT_MISSING:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nSWINT missing case\n");
		break;
	case MD_EE_CASE_ONLY_EX:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nOnly EX case\n");
		break;
	case MD_EE_CASE_ONLY_EX_OK:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nOnly EX_OK case\n");
		break;
	case MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG:
		i_bit_ex_info = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
		if (i_bit_ex_info == NULL) {
			CCCI_ERROR_LOG(md_id, KERN, "Fail alloc Mem for i_bit_ex_info!\n");
			break;
		}
		snprintf(i_bit_ex_info, EE_BUF_LEN_UMOLY, "\n[Others] May I-Bit dis too long\n%s", ex_info);
		strncpy(ex_info, i_bit_ex_info, EE_BUF_LEN_UMOLY);
		break;
	case MD_EE_CASE_TX_TRG:
	case MD_EE_CASE_ISR_TRG:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\n[Others] May I-Bit dis too long\n");
		break;
	case MD_EE_CASE_NO_RESPONSE:
		/* use strcpy, otherwise if this happens after a MD EE, the former EE info will be printed out */
		strncpy(ex_info, "\n[Others] MD long time no response\n", EE_BUF_LEN_UMOLY);
		db_opt |= DB_OPT_FTRACE;
		break;
	case MD_EE_CASE_WDT:
		strncpy(ex_info, "\n[Others] MD watchdog timeout interrupt\n", EE_BUF_LEN_UMOLY);
		break;
	default:
		break;
	}

	/* get ELM_status field from MD side */
	c = ex_pl_info->envinfo.ELM_status;
	CCCI_NORMAL_LOG(md_id, KERN, "ELM_status: %x\n", c);
	switch (c) {
	case 0xFF:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nno ELM info\n");
		break;
	case 0xAE:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nELM rlat:FAIL\n");
		break;
	case 0xBE:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nELM wlat:FAIL\n");
		break;
	case 0xDE:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nELM r/wlat:PASS\n");
		break;
	default:
		break;
	}

	/* Dump MD EE info */
	CCCI_MEM_LOG_TAG(md_id, KERN, "Dump MD EX log, 0x%x, 0x%x\n", dumper->more_info,
			(unsigned int)md_state);
	if (dumper->more_info == MD_EE_CASE_NORMAL && md_state == BOOT_WAITING_FOR_HS1) {
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, dumper->ex_pl_info, MD_HS1_FAIL_DUMP_SIZE);
		/* MD will not fill in share memory before we send runtime data */
		dump_flag = CCCI_AED_DUMP_EX_PKT;
	} else if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
		CCCI_NORMAL_LOG(md_id, KERN, "Dump MD exp smem_log\n");
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					smem_layout->ccci_exp_smem_base_vir, (2048 + 512));
		CCCI_NORMAL_LOG(md_id, KERN, "Dump MD exp smem_mdss_debug_log\n");
		ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					(smem_layout->ccci_exp_smem_mdss_debug_vir + 6 * 1024), 2048);
		/*
		* otherwise always dump whole share memory,
		* as MD will fill debug log into its 2nd 1K region after bootup
		*/
		dump_flag = CCCI_AED_DUMP_EX_MEM;
		if (dumper->more_info == MD_EE_CASE_NO_RESPONSE)
			dump_flag |= CCCI_AED_DUMP_CCIF_REG;
	}

err_exit:

	/* update here to maintain handshake stage info during exception handling */
	if (debug_info && debug_info->type == MD_EX_TYPE_C2K_ERROR)
		CCCI_NORMAL_LOG(md_id, KERN, "C2K EE, No need trigger DB\n");
	else if (debug_info && (debug_info->type == MD_EX_DUMP_EMI_CHECK) && (Is_MD_EMI_voilation() == 0))
		CCCI_NORMAL_LOG(md_id, KERN, "Not MD EMI violation, No need trigger DB\n");
	else if (ex_info == NULL)
		ccci_aed_v2(mdee, dump_flag, buf_fail, db_opt);
	else
		ccci_aed_v2(mdee, dump_flag, ex_info, db_opt);


	kfree(ex_info);
	kfree(ex_info_temp);
	kfree(i_bit_ex_info);
}

static char mdee_plstr[MD_EX_PL_FATALE_TOTAL + MD_EX_OTHER_CORE_EXCEPTIN - MD_EX_CC_INVALID_EXCEPTION][32] = {
	"INVALID",
	"Fatal error (undefine)",
	"Fatal error (swi)",
	"Fatal error (prefetch abort)",
	"Fatal error (data abort)",
	"Fatal error (stack)",
	"Fatal error (task)",
	"Fatal error (buff)",
	"Fatal error (CC invalid)",
	"Fatal error (CC PCore)",
	"Fatal error (CC L1Core)",
	"Fatal error (CC CS)",
	"Fatal error (CC MD32)",
	"Fatal error (CC C2K)",
	"Fatal error (CC spc)"
};

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
	CCCI_DEBUG_LOG(-1, KERN, "copy str(%d) %s\n", temp_i, dst);
}

static int mdee_pl_core_parse(int md_id, DEBUG_INFO_T *debug_info, EX_PL_LOG_T *ex_PLloginfo)
{
	int ee_type = 0;
	int ee_case = 0;

	ee_type = ex_PLloginfo->header.ex_type;
	debug_info->type = ee_type;
	ee_case = ee_type;
	CCCI_ERROR_LOG(md_id, KERN, "PL ex type(0x%x)\n", ee_type);
	switch (ee_type) {
	case MD_EX_PL_INVALID:
		debug_info->name = "INVALID";
		break;
	case MD_EX_CC_INVALID_EXCEPTION:
		/* Fall through */
	case MD_EX_CC_PCORE_EXCEPTION:
		/* Fall through */
	case MD_EX_CC_L1CORE_EXCEPTION:
		/* Fall through */
	case MD_EX_CC_CS_EXCEPTION:
		/* Fall through */
	case MD_EX_CC_MD32_EXCEPTION:
		/* Fall through */
	case MD_EX_CC_C2K_EXCEPTION:
		/* Fall through */
	case MD_EX_CC_ARM7_EXCEPTION:
		/*
		* md1:(MCU_PCORE)
		* [Fatal error (CC xxx)] err_code1:0x00000xx err_code2:0x00xxx err_code3:0xxxx
		* Offender:
		*/
		ee_type = ee_type - MD_EX_CC_INVALID_EXCEPTION + MD_EX_PL_FATALE_TOTAL;
		/* Fall through */
	case MD_EX_PL_UNDEF:
		/* Fall through */
	case MD_EX_PL_SWI:
		/* Fall through */
	case MD_EX_PL_PREF_ABT:
		/* Fall through */
	case MD_EX_PL_DATA_ABT:
		/* Fall through */
	case MD_EX_PL_STACKACCESS:
		/* Fall through */
	case MD_EX_PL_FATALERR_TASK:
		/* Fall through */
	case MD_EX_PL_FATALERR_BUF:
		/* all offender is zero, goto from tail of function, reparser. */
		/* the only one case: none offender, c2k ee */
		if (ee_type ==
				(MD_EX_CC_C2K_EXCEPTION  - MD_EX_CC_INVALID_EXCEPTION +
				MD_EX_PL_FATALE_TOTAL))
			debug_info->type = MD_EX_CC_C2K_EXCEPTION;
		else
			debug_info->type = MD_EX_DUMP_3P_EX;
		debug_info->name = mdee_plstr[ee_type];
		if (ex_PLloginfo->content.fatalerr.ex_analy.owner[0] != 0xCC) {
			strmncopy(ex_PLloginfo->content.fatalerr.ex_analy.owner,
				debug_info->fatal_error.offender,
				sizeof(ex_PLloginfo->content.fatalerr.ex_analy.owner),
				sizeof(debug_info->fatal_error.offender));
			CCCI_NORMAL_LOG(md_id, KERN, "offender: %s\n",
				     debug_info->fatal_error.offender);
		}
		debug_info->fatal_error.err_code1 =
		    ex_PLloginfo->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_PLloginfo->content.fatalerr.error_code.code2;
		debug_info->fatal_error.err_code3 =
		    ex_PLloginfo->content.fatalerr.error_code.code3;
		if (ex_PLloginfo->content.fatalerr.ex_analy.is_cadefa_sup == 0x01)
			debug_info->fatal_error.ExStr = "CaDeFa Supported\n";
		else
			debug_info->fatal_error.ExStr = "";
		break;
	case MD_EX_PL_ASSERT_FAIL:
		/* Fall through */
	case MD_EX_PL_ASSERT_DUMP:
		/* Fall through */
	case MD_EX_PL_ASSERT_NATIVE:
		debug_info->type = MD_EX_DUMP_ASSERT;/* = MD_EX_TYPE_ASSERT; */
		debug_info->name = "ASSERT";
		CCCI_DEBUG_LOG(md_id, KERN, "p filename1(%s)\n",
			ex_PLloginfo->content.assert.filepath);
		strmncopy(ex_PLloginfo->content.assert.filepath,
			debug_info->assert.file_name,
			sizeof(ex_PLloginfo->content.assert.filepath),
			sizeof(debug_info->assert.file_name));
		CCCI_DEBUG_LOG(md_id, KERN,
			"p filename2:(%s)\n", debug_info->assert.file_name);
		debug_info->assert.line_num = ex_PLloginfo->content.assert.linenumber;
		debug_info->assert.parameters[0] = ex_PLloginfo->content.assert.para[0];
		debug_info->assert.parameters[1] = ex_PLloginfo->content.assert.para[1];
		debug_info->assert.parameters[2] = ex_PLloginfo->content.assert.para[2];
		break;

	case EMI_MPU_VIOLATION:
		debug_info->type = MD_EX_DUMP_EMI_CHECK;
		ee_case = MD_EX_TYPE_EMI_CHECK;
		debug_info->name = "Fatal error (rmpu violation)";
		debug_info->fatal_error.err_code1 =
		    ex_PLloginfo->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_PLloginfo->content.fatalerr.error_code.code2;
		debug_info->fatal_error.err_code3 =
		    ex_PLloginfo->content.fatalerr.error_code.code3;
		debug_info->fatal_error.ExStr = "EMI MPU VIOLATION\n";
		break;
	default:
		debug_info->name = "UNKNOWN Exception";
		break;
	}
	debug_info->ext_mem = ex_PLloginfo;
	debug_info->ext_size = sizeof(EX_PL_LOG_T);

	return ee_case;
}


static int mdee_cs_core_parse(int md_id, DEBUG_INFO_T *debug_info, EX_CS_LOG_T *ex_csLogInfo)
{
	int ee_type = 0;
	int ee_case = 0;

	ee_type = ex_csLogInfo->except_type;
	CCCI_ERROR_LOG(md_id, KERN, "cs ex type(0x%x)\n", ee_type);
	switch (ee_type) {
	case CS_EXCEPTION_ASSERTION:
		debug_info->type = MD_EX_DUMP_ASSERT;
		ee_case = MD_EX_TYPE_ASSERT;

		debug_info->name = "ASSERT";
		strmncopy(ex_csLogInfo->except_content.assert.file_name,
			debug_info->assert.file_name,
			sizeof(ex_csLogInfo->except_content.assert.file_name),
			sizeof(debug_info->assert.file_name));
		debug_info->assert.line_num = ex_csLogInfo->except_content.assert.line_num;
		debug_info->assert.parameters[0] = ex_csLogInfo->except_content.assert.para1;
		debug_info->assert.parameters[1] = ex_csLogInfo->except_content.assert.para2;
		debug_info->assert.parameters[2] = ex_csLogInfo->except_content.assert.para3;
		break;
	case CS_EXCEPTION_FATAL_ERROR:
		debug_info->type = MD_EX_DUMP_2P_EX;
		ee_case = MD_EX_TYPE_FATALERR_TASK;

		debug_info->name = "Fatal error";
		debug_info->fatal_error.err_code1 =
		    ex_csLogInfo->except_content.fatalerr.error_code1;
		debug_info->fatal_error.err_code2 =
		    ex_csLogInfo->except_content.fatalerr.error_code2;
		break;
	case CS_EXCEPTION_CTI_EVENT:
		debug_info->name = "CC CTI Exception";
		break;
	case CS_EXCEPTION_UNKNOWN:
	default:
		debug_info->name = "UNKNOWN Exception";
		break;
	}
	debug_info->ext_mem = ex_csLogInfo;
	debug_info->ext_size = sizeof(EX_CS_LOG_T);
	return ee_case;
}
static int mdee_md32_core_parse(int md_id, DEBUG_INFO_T *debug_info, EX_MD32_LOG_T *ex_md32LogInfo)
{
	int ee_type = 0;
	int ee_case = 0;

	ee_type = ex_md32LogInfo->except_type;
	CCCI_ERROR_LOG(md_id, KERN, "md32 ex type(0x%x), name: %s\n", ee_type,
		     ex_md32LogInfo->except_content.assert.file_name);
	switch (ex_md32LogInfo->md32_active_mode) {
	case 1:
		snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "%s%s",
			debug_info->core_name, MD32_FDD_ROCODE);
		break;
	case 2:
		snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "%s%s",
			debug_info->core_name, MD32_TDD_ROCODE);
		break;
	default:
		break;
	}
	switch (ee_type) {
	case CMIF_MD32_EX_ASSERT_LINE:
		/* Fall through */
	case CMIF_MD32_EX_ASSERT_EXT:
		debug_info->type = MD_EX_DUMP_ASSERT;
		ee_case = MD_EX_TYPE_ASSERT;
		debug_info->name = "ASSERT";
		strmncopy(ex_md32LogInfo->except_content.assert.file_name,
			debug_info->assert.file_name,
			sizeof(ex_md32LogInfo->except_content.assert.file_name),
			sizeof(debug_info->assert.file_name));
		debug_info->assert.line_num = ex_md32LogInfo->except_content.assert.line_num;
		debug_info->assert.parameters[0] =
		    ex_md32LogInfo->except_content.assert.ex_code[0];
		debug_info->assert.parameters[1] =
		    ex_md32LogInfo->except_content.assert.ex_code[1];
		debug_info->assert.parameters[2] =
		    ex_md32LogInfo->except_content.assert.ex_code[2];
		break;
	case CMIF_MD32_EX_FATAL_ERROR:
		/* Fall through */
	case CMIF_MD32_EX_FATAL_ERROR_EXT:
		debug_info->type = MD_EX_DUMP_2P_EX;
		ee_case = MD_EX_TYPE_FATALERR_TASK;

		debug_info->name = "Fatal error";
		debug_info->fatal_error.err_code1 =
		    ex_md32LogInfo->except_content.fatalerr.ex_code[0];
		debug_info->fatal_error.err_code2 =
		    ex_md32LogInfo->except_content.fatalerr.ex_code[1];
		break;
	case CS_EXCEPTION_CTI_EVENT:
		/* Fall through */
	case CS_EXCEPTION_UNKNOWN:
	default:
		debug_info->name = "UNKNOWN Exception";
		break;
	}
	debug_info->ext_mem = ex_md32LogInfo;
	debug_info->ext_size = sizeof(EX_MD32_LOG_T);

	return ee_case;
}
static void mdee_set_core_name(int md_id, DEBUG_INFO_T *debug_info, char *core_name)
{
	unsigned int temp_i;
	/* (core name): PCORE/L1CORE/CS_ICC/CS_IMC/CS_MPC/MD32_BRP/MD32_DFE/MD32_RAKE */
	debug_info->core_name[0] = '(';
	for (temp_i = 1; temp_i < MD_CORE_NAME_LEN; temp_i++) {
		debug_info->core_name[temp_i] = core_name[temp_i - 1];
		if (debug_info->core_name[temp_i] == '\0')
			break;
	}
	debug_info->core_name[temp_i++] = ')';
	debug_info->core_name[temp_i] = '\0';
}
/* todo: copy error code can convert a mini function */
static void mdee_info_prepare_v2(struct md_ee *mdee)
{
	EX_OVERVIEW_T *ex_overview;
	int ee_case = 0;
	struct mdee_dumper_v2 *dumper = mdee->dumper_obj;
	int md_id = mdee->md_id;
	DEBUG_INFO_T *debug_info = NULL;
	int core_id;
	unsigned char off_core_num = 0; /* number of offender core: need parse */

	EX_PL_LOG_T *ex_pl_info = (EX_PL_LOG_T *)dumper->ex_pl_info;
	EX_PL_LOG_T *ex_PLloginfo;
	EX_CS_LOG_T *ex_csLogInfo;
	EX_MD32_LOG_T *ex_md32LogInfo;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);

	CCCI_NORMAL_LOG(md_id, KERN, "ccci_md_exp_change, ee_case(0x%x)\n", dumper->more_info);

	if ((dumper->more_info == MD_EE_CASE_NORMAL) && (ccci_md_get_state(mdee->md_obj) == BOOT_WAITING_FOR_HS1)) {
		debug_info = &dumper->debug_info[0];
		ex_PLloginfo = ex_pl_info;
		off_core_num++;
		snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "(MCU_PCORE)");
		ee_case = mdee_pl_core_parse(md_id, debug_info, ex_PLloginfo);
		mdee->ex_type = ee_case;
		dumper->ex_core_num = off_core_num;
		CCCI_NORMAL_LOG(md_id, KERN, "core_ex_num(%d/%d)\n", off_core_num, dumper->ex_core_num);
		return;
	}

	ex_overview = (EX_OVERVIEW_T *) smem_layout->ccci_exp_smem_mdss_debug_vir;
	for (core_id = 0; core_id < MD_CORE_NUM; core_id++) {
		CCCI_DEBUG_LOG(md_id, KERN, "core_id(%x/%x): offset=%x, if_offender=%d, %s\n", (core_id + 1),
			     ex_overview->core_num, ex_overview->main_reson[core_id].core_offset,
			     ex_overview->main_reson[core_id].is_offender, ex_overview->main_reson[core_id].core_name);
		if (ex_overview->main_reson[core_id].is_offender == 0)
			continue;
		debug_info = &dumper->debug_info[off_core_num];
		memset(debug_info, 0, sizeof(DEBUG_INFO_T));

		off_core_num++;
		mdee_set_core_name(md_id, debug_info, ex_overview->main_reson[core_id].core_name);
		CCCI_NORMAL_LOG(md_id, KERN, "core_id(0x%x/%d), %s\n", core_id, off_core_num,
		     debug_info->core_name);
		ex_pl_info->envinfo.ELM_status = 0;
		switch (core_id) {
		case MD_PCORE:
		case MD_L1CORE:
			ex_PLloginfo =
			    (EX_PL_LOG_T *) ((char *)ex_overview +
					     ex_overview->main_reson[core_id].core_offset);
			ex_pl_info->envinfo.ELM_status = ex_PLloginfo->envinfo.ELM_status;
			ee_case = mdee_pl_core_parse(md_id, debug_info, ex_PLloginfo);
			break;
		case MD_CS_ICC:
			/* Fall through */
		case MD_CS_IMC:
			/* Fall through */
		case MD_CS_MPC:
			ex_csLogInfo =
			    (EX_CS_LOG_T *) ((char *)ex_overview +
					     ex_overview->main_reson[core_id].core_offset);
			ee_case = mdee_cs_core_parse(md_id, debug_info, ex_csLogInfo);
			break;
		case MD_MD32_DFE:
			/* Fall through */
		case MD_MD32_BRP:
			/* Fall through */
		case MD_MD32_RAKE:
			ex_md32LogInfo = (EX_MD32_LOG_T *) ((char *)ex_overview +
					       ex_overview->main_reson[core_id].core_offset);
			ee_case = mdee_md32_core_parse(md_id, debug_info, ex_md32LogInfo);
			break;
		default:
			ee_case = 0;
			break;
		}
		if (off_core_num == 1) {
			mdee->ex_type = ee_case;
			CCCI_ERROR_LOG(md_id, KERN, "set ee_type=%d\n", mdee->ex_type);
		}
	}

	if (off_core_num == 0) {
		debug_info = &dumper->debug_info[0];
		ex_PLloginfo = (EX_PL_LOG_T *) ((char *)ex_overview +
				ex_overview->main_reson[MD_PCORE].core_offset);
		ex_pl_info->envinfo.ELM_status = ex_PLloginfo->envinfo.ELM_status;
		off_core_num++;
		core_id = MD_CORE_NUM;
		snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "(MCU_PCORE)");
		ee_case = mdee_pl_core_parse(md_id, debug_info, ex_PLloginfo);
		mdee->ex_type = ee_case;
	}

	dumper->ex_core_num = off_core_num;
	CCCI_ERROR_LOG(md_id, KERN, "core_ex_num(%d/%d) ee_type=%d\n",
		off_core_num, dumper->ex_core_num, mdee->ex_type);
}

static void mdee_dumper_v2_set_ee_pkg(struct md_ee *mdee, char *data, int len)
{
	struct mdee_dumper_v2 *dumper = mdee->dumper_obj;
	int cpy_len = len > MD_HS1_FAIL_DUMP_SIZE ? MD_HS1_FAIL_DUMP_SIZE : len;

	memcpy(dumper->ex_pl_info, data, cpy_len);
}
static void mdee_dumper_v2_dump_ee_info(struct md_ee *mdee, MDEE_DUMP_LEVEL level, int more_info)
{
	struct mdee_dumper_v2 *dumper = mdee->dumper_obj;
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
			ccci_aed_v2(mdee, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_MD_IMG_MEM | CCCI_AED_DUMP_EX_MEM,
					ex_info, DB_OPT_DEFAULT);
		} else if (md_state == BOOT_WAITING_FOR_HS2) {
			snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", 2);
			/* Handshake 2 fail */
			CCCI_NORMAL_LOG(md_id, KERN, "Dump MD EX log\n");
			if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM))
				ccci_mem_dump(md_id, smem_layout->ccci_exp_smem_base_vir,
							smem_layout->ccci_exp_dump_size);

			ccci_aed_v2(mdee, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_EX_MEM, ex_info, DB_OPT_FTRACE);
		}
	} else if (level == MDEE_DUMP_LEVEL_TIMER1) {
		if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
			ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					smem_layout->ccci_exp_smem_mdss_debug_vir, (2048 + 512));
			ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP,
					(smem_layout->ccci_exp_smem_mdss_debug_vir + 6 * 1024), 2048);
		}
	} else if (level == MDEE_DUMP_LEVEL_TIMER2) {
		mdee_info_prepare_v2(mdee);
		mdee_info_dump_v2(mdee);
	}
}

static struct md_ee_ops mdee_ops_v2 = {
	.dump_ee_info = &mdee_dumper_v2_dump_ee_info,
	.set_ee_pkg = &mdee_dumper_v2_set_ee_pkg,
};
int mdee_dumper_v2_alloc(struct md_ee *mdee)
{
	struct mdee_dumper_v2 *dumper;
	int md_id = mdee->md_id;

	/* Allocate port_proxy obj and set all member zero */
	dumper = kzalloc(sizeof(struct mdee_dumper_v2), GFP_KERNEL);
	if (dumper == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "%s:alloc mdee_parser_v2 fail\n", __func__);
		return -1;
	}
	mdee->dumper_obj = dumper;
	mdee->ops = &mdee_ops_v2;
	return 0;
}

