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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>

/* local include */
#include "mtk_unified_power_internal.h"
#include "mtk_unified_power.h"
#include "mtk_unified_power_data.h"
#include "mtk_devinfo.h"

#ifndef EARLY_PORTING_SPOWER
	#include "mtk_static_power.h"
#endif

#if UPOWER_ENABLE
unsigned char upower_enable = 1;
#else
unsigned char upower_enable;
#endif

/* upower table reference */
struct upower_tbl *upower_tbl_ref;
int degree_set[NR_UPOWER_DEGREE] = {
		UPOWER_DEGREE_0,
		UPOWER_DEGREE_1,
		UPOWER_DEGREE_2,
		UPOWER_DEGREE_3,
		UPOWER_DEGREE_4,
		UPOWER_DEGREE_5,
};

/* collect all the raw tables */
#define INIT_UPOWER_TBL_INFOS(name, tbl) {__stringify(name), &tbl}
struct upower_tbl_info upower_tbl_infos_FY[NR_UPOWER_BANK] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_B, upower_tbl_b_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_B, upower_tbl_cluster_b_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_FY),
};

struct upower_tbl_info upower_tbl_infos_SB[NR_UPOWER_BANK] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_B, upower_tbl_b_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_B, upower_tbl_cluster_b_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_SB),
};

/* points to all the raw tables */
struct upower_tbl_info *p_upower_tbl_infos = &upower_tbl_infos_FY[0];
struct upower_tbl_info *new_p_tbl_infos;
struct upower_tbl_info *upower_tbl_infos;
static unsigned int binLevel;

#if 0
static void print_tbl(void)
{
	int i, j;
/* --------------------print static orig table -------------------------*/
	#if 0
	struct upower_tbl *tbl;

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		tbl = upower_tbl_infos[i].p_upower_tbl;
		/* table size must be 512 bytes */
		upower_debug("Bank %d , tbl size %ld\n", i, sizeof(struct upower_tbl));
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			upower_debug(" cap, volt, dyn, lkg: %llu, %u, %u, {%u, %u, %u, %u, %u}\n",
					tbl->row[j].cap, tbl->row[j].volt,
					tbl->row[j].dyn_pwr, tbl->row[j].lkg_pwr[0],
					tbl->row[j].lkg_pwr[1], tbl->row[j].lkg_pwr[2],
					tbl->row[j].lkg_pwr[3], tbl->row[j].lkg_pwr[4]);
		}

		upower_debug(" lkg_idx, num_row: %d, %d\n", tbl->lkg_idx, tbl->row_num);
		upower_debug("-----------------------------------------------------------------\n");
	}
	#else
/* --------------------print sram table -------------------------*/
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		/* table size must be 512 bytes */
		upower_debug("---Bank %d , tbl size %ld---\n", i, sizeof(struct upower_tbl));
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			upower_debug(" cap = %llu, volt = %u, dyn = %u, lkg = {%u, %u, %u, %u, %u}\n",
					upower_tbl_ref[i].row[j].cap, upower_tbl_ref[i].row[j].volt,
					upower_tbl_ref[i].row[j].dyn_pwr, upower_tbl_ref[i].row[j].lkg_pwr[0],
					upower_tbl_ref[i].row[j].lkg_pwr[1], upower_tbl_ref[i].row[j].lkg_pwr[2],
					upower_tbl_ref[i].row[j].lkg_pwr[3], upower_tbl_ref[i].row[j].lkg_pwr[4]);
		}
		upower_debug(" lkg_idx, num_row: %d, %d\n",
					upower_tbl_ref[i].lkg_idx, upower_tbl_ref[i].row_num);
		upower_debug("-------------------------------------------------\n");
	}
	#endif
}
#endif

#ifdef UPOWER_UT
void upower_ut(void)
{
	struct upower_tbl_info **addr_ptr_tbl_info;
	struct upower_tbl_info *ptr_tbl_info;
	struct upower_tbl *ptr_tbl;
	int i, j;

	upower_debug("----upower_get_tbl()----\n");
	/* get addr of ptr which points to upower_tbl_infos[] */
	addr_ptr_tbl_info = upower_get_tbl();
	/* get ptr which points to upower_tbl_infos[] */
	ptr_tbl_info = *addr_ptr_tbl_info;
	upower_debug("get upower tbl location = %p\n", ptr_tbl_info[0].p_upower_tbl);
	#if 0
	upower_debug("ptr_tbl_info --> %p --> tbl %p (p_upower_tbl_infos --> %p)\n",
				ptr_tbl_info, ptr_tbl_info[0].p_upower_tbl, p_upower_tbl_infos);
	#endif

	/* print all the tables that record in upower_tbl_infos[]*/
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		upower_debug("bank %d\n", i);
		ptr_tbl = ptr_tbl_info[i].p_upower_tbl;
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			upower_debug(" cap = %llu, volt = %u, dyn = %u, lkg = {%u, %u, %u, %u, %u, %u}\n",
					ptr_tbl->row[j].cap, ptr_tbl->row[j].volt,
					ptr_tbl->row[j].dyn_pwr, ptr_tbl->row[j].lkg_pwr[0],
					ptr_tbl->row[j].lkg_pwr[1], ptr_tbl->row[j].lkg_pwr[2],
					ptr_tbl->row[j].lkg_pwr[3], ptr_tbl->row[j].lkg_pwr[4],
					ptr_tbl->row[j].lkg_pwr[5]);
		}
		upower_debug(" lkg_idx, num_row, nr_idle_states: %d, %d ,%d\n",
					ptr_tbl->lkg_idx, ptr_tbl->row_num, ptr_tbl->nr_idle_states);

		for (i = 0; i < NR_UPOWER_DEGREE; i++) {
			upower_debug("(%d)C c0 = %lu, c1 = %lu\n",
					degree_set[i],
					ptr_tbl->idle_states[i][0].power, ptr_tbl->idle_states[i][1].power);

		}
	}

	upower_debug("----upower_get_power()----\n");
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		upower_debug("bank %d\n", i);
		upower_debug("[dyn] %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
					upower_get_power(i, 0, UPOWER_DYN),
					upower_get_power(i, 1, UPOWER_DYN),
					upower_get_power(i, 2, UPOWER_DYN),
					upower_get_power(i, 3, UPOWER_DYN),
					upower_get_power(i, 4, UPOWER_DYN),
					upower_get_power(i, 5, UPOWER_DYN),
					upower_get_power(i, 6, UPOWER_DYN),
					upower_get_power(i, 7, UPOWER_DYN),
					upower_get_power(i, 15, UPOWER_DYN));
		upower_debug("[lkg] %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
					upower_get_power(i, 0, UPOWER_LKG),
					upower_get_power(i, 1, UPOWER_LKG),
					upower_get_power(i, 2, UPOWER_LKG),
					upower_get_power(i, 3, UPOWER_LKG),
					upower_get_power(i, 4, UPOWER_LKG),
					upower_get_power(i, 5, UPOWER_LKG),
					upower_get_power(i, 6, UPOWER_LKG),
					upower_get_power(i, 7, UPOWER_LKG),
					upower_get_power(i, 15, UPOWER_LKG));
	}
}
#endif

static void upower_update_dyn_pwr(void)
{
	unsigned long long refPower, newVolt, refVolt, newPower;
	unsigned long long temp1, temp2;
	int i, j;
	struct upower_tbl *tbl;

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		tbl = upower_tbl_infos[i].p_upower_tbl;
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			refPower = (unsigned long long)tbl->row[j].dyn_pwr;
			refVolt = (unsigned long long)tbl->row[j].volt;
			newVolt = (unsigned long long)upower_tbl_ref[i].row[j].volt;

			temp1 = (refPower * newVolt * newVolt);
			temp2 = (refVolt * refVolt);
			newPower = temp1 / temp2;
			upower_tbl_ref[i].row[j].dyn_pwr = newPower;
			/* upower_debug("dyn_pwr= %u\n", upower_tbl_ref[i].row[j].dyn_pwr);*/
		}
	}
}


#ifndef EARLY_PORTING_SPOWER
static int upower_bank_to_spower_bank(int upower_bank)
{
	int ret;

	switch (upower_bank) {
	case UPOWER_BANK_LL:
		ret = MTK_SPOWER_CPULL;
		break;
	case UPOWER_BANK_L:
		ret = MTK_SPOWER_CPUL;
		break;
	case UPOWER_BANK_B:
		ret = MTK_SPOWER_CPUBIG;
		break;
	case UPOWER_BANK_CLS_LL:
		ret = MTK_SPOWER_CPULL_CLUSTER;
		break;
	case UPOWER_BANK_CLS_L:
		ret = MTK_SPOWER_CPUL_CLUSTER;
		break;
	case UPOWER_BANK_CLS_B:
		ret = MTK_SPOWER_CPUBIG_CLUSTER;
		break;
	case UPOWER_BANK_CCI:
		ret = MTK_SPOWER_CCI;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
#endif

static void upower_update_lkg_pwr(void)
{
	int i, j, k;
	struct upower_tbl *tbl;
	unsigned int spower_bank_id;
	unsigned int volt;
	int degree;
	unsigned int temp;

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		tbl = upower_tbl_infos[i].p_upower_tbl;

		#ifdef EARLY_PORTING_SPOWER
		/* get p-state lkg */
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			for (k = 0; k < NR_UPOWER_DEGREE; k++)
				upower_tbl_ref[i].row[j].lkg_pwr[k] = tbl->row[j].lkg_pwr[k];
		}

		/* get c-state lkg */
		for (j = 0; j < NR_UPOWER_DEGREE; j++) {
			for (k = 0; k < NR_UPOWER_CSTATES; k++)
				upower_tbl_ref[i].idle_states[j][k].power = tbl->idle_states[j][k].power;
		}
		#else
		spower_bank_id = upower_bank_to_spower_bank(i);

		#if 0
		upower_debug("upower bank, spower bank= %d, %d\n", i, spower_bank_id);
		upower_debug("deg = %d, %d, %d, %d, %d, %d\n", degree_set[0], degree_set[1],
							degree_set[2], degree_set[3], degree_set[4], degree_set[5]);
		#endif

		/* wrong bank */
		if (spower_bank_id == -1)
			continue;

		/* get p-state lkg */
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			volt = (unsigned int)upower_tbl_ref[i].row[j].volt;
			for (k = 0; k < NR_UPOWER_DEGREE; k++) {
				degree = degree_set[k];
				/* get leakage from spower driver and transfer mw to uw */
				temp = mt_spower_get_leakage(spower_bank_id, (volt/100), degree);
				upower_tbl_ref[i].row[j].lkg_pwr[k] = temp * 1000;
				#if 0
				upower_debug("deg[%d] temp[%u] lkg_pwr[%u]\n", degree, temp,
							upower_tbl_ref[i].row[j].lkg_pwr[k]);
				#endif
			}
			#if 0
			upower_debug("volt[%u] lkg_pwr[%u, %u, %u, %u, %u, %u]\n", volt,
							upower_tbl_ref[i].row[j].lkg_pwr[0],
							upower_tbl_ref[i].row[j].lkg_pwr[1],
							upower_tbl_ref[i].row[j].lkg_pwr[2],
							upower_tbl_ref[i].row[j].lkg_pwr[3],
							upower_tbl_ref[i].row[j].lkg_pwr[4],
							upower_tbl_ref[i].row[j].lkg_pwr[5]);
			#endif
		}

		/* get c-state lkg */
		upower_tbl_ref[i].nr_idle_states = NR_UPOWER_CSTATES;
		volt = UPOWER_C1_VOLT;
		for (j = 0; j < NR_UPOWER_DEGREE; j++) {
			for (k = 0; k < NR_UPOWER_CSTATES; k++) {
				/* if c1 state, query lkg from lkg driver */
				if (k == UPOWER_C1_IDX) {
					degree = degree_set[j];
					/* get leakage from spower driver and transfer mw to uw */
					temp = mt_spower_get_leakage(spower_bank_id, (volt/100), degree);
					upower_tbl_ref[i].idle_states[j][k].power = (unsigned long)(temp * 1000);
				} else {
					upower_tbl_ref[i].idle_states[j][k].power = tbl->idle_states[j][k].power;
				}
			}
		}
		#endif
	}
}

static void upower_init_cap(void)
{
	int i, j;
	struct upower_tbl *tbl;

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		tbl = upower_tbl_infos[i].p_upower_tbl;
		for (j = 0; j < UPOWER_OPP_NUM; j++)
			upower_tbl_ref[i].row[j].cap = tbl->row[j].cap;
	}
}

static void upower_init_rownum(void)
{
	int i;

	for (i = 0; i < NR_UPOWER_BANK; i++)
		upower_tbl_ref[i].row_num = UPOWER_OPP_NUM;
}

#ifdef EARLY_PORTING_EEM
static void upower_init_lkgidx(void)
{
	int i;

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		upower_tbl_ref[i].lkg_idx = DEFAULT_LKG_IDX;
		/*
		*upower_error("[bank %d]lkg_idx=%d, row num = %d\n", i, upower_tbl_ref[i].lkg_idx,
		*								upower_tbl_ref[i].row_num);
		*/
	}
}
static void upower_init_volt(void)
{
	int i, j;
	struct upower_tbl *tbl;

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		tbl = upower_tbl_infos[i].p_upower_tbl;
		for (j = 0; j < UPOWER_OPP_NUM; j++)
			upower_tbl_ref[i].row[j].volt = tbl->row[j].volt;
	}
}
#endif

static int upower_update_tbl_ref(void)
{
	int i;
	int ret = 0;

	#ifdef UPOWER_PROFILE_API_TIME
	upower_get_start_time_us(UPDATE_TBL_PTR);
	#endif

	new_p_tbl_infos = kzalloc(sizeof(*new_p_tbl_infos) * NR_UPOWER_BANK, GFP_KERNEL);
	if (!new_p_tbl_infos) {
		upower_error("Out of mem to create new_p_tbl_infos\n");
		return -ENOMEM;
	}

	/* upower_tbl_ref is the ptr points to table in sram */
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		new_p_tbl_infos[i].p_upower_tbl = &upower_tbl_ref[i];
		new_p_tbl_infos[i].name = upower_tbl_infos[i].name;
		/* upower_debug("new_p_tbl_infos[%d].name = %s\n", i, new_p_tbl_infos[i].name);*/
	}

	#ifdef UPOWER_RCU_LOCK
	rcu_assign_pointer(p_upower_tbl_infos, new_p_tbl_infos);
	/* synchronize_rcu();*/
	#else
	p_upower_tbl_infos = new_p_tbl_infos;
	#endif

	#ifdef UPOWER_PROFILE_API_TIME
	upower_get_diff_time_us(UPDATE_TBL_PTR);
	print_diff_results(UPDATE_TBL_PTR);
	#endif

	return ret;
}

static int __init upower_get_tbl_ref(void)
{
	/* get table size */
	unsigned long long size = sizeof(struct upower_tbl) * NR_UPOWER_BANK;
	/* UPOWER_TBL_LIMIT is the bottom address of unified power table */
	unsigned long long upower_tbl_base = UPOWER_TBL_LIMIT - size;

	upower_debug("upower table size=%llu\n", size);
	upower_debug("upower table start=0x%llx\n", upower_tbl_base);

	/* get table address on sram */
	upower_tbl_ref = ioremap_nocache(upower_tbl_base, size);
	if (upower_tbl_ref == NULL)
		return -ENOMEM;

	upower_error("upower tbl location = %p, size = %llu\n", upower_tbl_ref, size);
	memset_io((u8 *)upower_tbl_ref, 0x00, size);

	return 0;
}

#ifdef UPOWER_PROFILE_API_TIME
static void profile_api(void)
{
	int i, j;

	upower_debug("----profile upower_get_power()----\n");
	/* do 56*2 times */
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			upower_get_power(i, j, UPOWER_DYN);
			upower_get_power(i, j, UPOWER_LKG);
		}
	}
	upower_debug("----profile upower_update_tbl_ref()----\n");
	for (i = 0; i < 10; i++)
		upower_update_tbl_ref();
}
#endif

static int upower_debug_proc_show(struct seq_file *m, void *v)
{

	struct upower_tbl_info **addr_ptr_tbl_info;
	struct upower_tbl_info *ptr_tbl_info;
	struct upower_tbl *ptr_tbl;
	int i, j;

	/* get addr of ptr which points to upower_tbl_infos[] */
	addr_ptr_tbl_info = upower_get_tbl();
	/* get ptr which points to upower_tbl_infos[] */
	ptr_tbl_info = *addr_ptr_tbl_info;
	upower_debug("get upower tbl location = %p\n", ptr_tbl_info[0].p_upower_tbl);

	/* print all the tables that record in upower_tbl_infos[]*/
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		seq_printf(m, "%s\n", upower_tbl_infos[i].name);
		ptr_tbl = ptr_tbl_info[i].p_upower_tbl;
		for (j = 0; j < UPOWER_OPP_NUM; j++) {
			seq_printf(m, " cap = %llu, volt = %u, dyn = %u, lkg = {%u, %u, %u, %u, %u, %u}\n",
					ptr_tbl->row[j].cap, ptr_tbl->row[j].volt,
					ptr_tbl->row[j].dyn_pwr, ptr_tbl->row[j].lkg_pwr[0],
					ptr_tbl->row[j].lkg_pwr[1], ptr_tbl->row[j].lkg_pwr[2],
					ptr_tbl->row[j].lkg_pwr[3], ptr_tbl->row[j].lkg_pwr[4],
					ptr_tbl->row[j].lkg_pwr[5]);
		}
		seq_printf(m, " lkg_idx, num_row: %d, %d\n\n",
					ptr_tbl->lkg_idx, ptr_tbl->row_num);
	}

	return 0;
}

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,				\
		.open		   = name ## _proc_open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,			\
		.write		  = name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,				\
		.open		   = name ## _proc_open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
/* create fops */
PROC_FOPS_RO(upower_debug);

static int create_procfs(void)
{
	struct proc_dir_entry *upower_dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry upower_entries[] = {
		/* {__stringify(name), &name ## _proc_fops} */
		PROC_ENTRY(upower_debug),
	};

	/* create proc/upower node */
	upower_dir = proc_mkdir("upower", NULL);
	if (!upower_dir) {
		upower_error("[%s] mkdir /proc/upower failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(upower_entries); i++) {
		if (!proc_create(upower_entries[i].name,
			S_IRUGO | S_IWUSR | S_IWGRP,
			upower_dir,
			upower_entries[i].fops)) {
			upower_error("[%s]: create /proc/upower/%s failed\n", __func__,
							upower_entries[i].name);
			return -3;
			}
	}
	return 0;
}

static void get_original_table(void)
{
	binLevel = GET_BITS_VAL(7:0, get_devinfo_with_index(UPOWER_FUNC_CODE_EFUSE_INDEX));

	if (binLevel == 0) /* 1.6G */
		upower_tbl_infos = &upower_tbl_infos_FY[0];
	else if (binLevel == 1) /* 2G */
		upower_tbl_infos = &upower_tbl_infos_SB[0];
	else if (binLevel == 2) /* 2.2 G */
		upower_tbl_infos = &upower_tbl_infos_FY[0]; /* should be FYA */
	else /* 1.6G */
		upower_tbl_infos = &upower_tbl_infos_FY[0];

	upower_error("binLevel=%d\n", binLevel);
}

static int __init upower_init(void)
{
	if (upower_enable == 0) {
		upower_error("upower is disabled\n");
		return 0;
	}
	/* PTP has no efuse, so volt will be set to orig data */
	/* before upower_init_volt(), PTP has called upower_update_volt_by_eem() */
	get_original_table();
	upower_debug("upower tbl orig location([0](%p)= %p\n",
					upower_tbl_infos, upower_tbl_infos[0].p_upower_tbl);

	#ifdef UPOWER_UT
	upower_debug("--------- (UT)before tbl ready--------------\n");
	upower_ut();
	#endif

	/* init rownum to UPOWER_OPP_NUM*/
	upower_init_rownum();

	upower_init_cap();

	#ifdef EARLY_PORTING_EEM
	/* apply orig volt and lkgidx, due to ptp not ready*/
	upower_init_lkgidx();
	upower_init_volt();
	#endif

	upower_update_dyn_pwr();
	upower_update_lkg_pwr();
	upower_update_tbl_ref();

	#ifdef UPOWER_UT
	upower_debug("--------- (UT)tbl ready--------------\n");
	upower_ut();
	#endif

	#ifdef UPOWER_PROFILE_API_TIME
	profile_api();
	#endif

	create_procfs();

	return 0;
}
#ifdef __KERNEL__
arch_initcall(upower_get_tbl_ref);
late_initcall(upower_init);
#endif
MODULE_DESCRIPTION("MediaTek Unified Power Driver v0.0");
MODULE_LICENSE("GPL");
