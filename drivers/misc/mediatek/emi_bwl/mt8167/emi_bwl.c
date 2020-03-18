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
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "mach/emi_bwl.h"
#include "mt-plat/sync_write.h"


#define MET_USER_EVENT_SUPPORT

void __iomem *EMI_BASE_ADDR;

DEFINE_SEMAPHORE(emi_bwl_sem);

static struct platform_driver mem_bw_ctrl = {
	.driver = {
		.name = "mem_bw_ctrl",
		.owner = THIS_MODULE,
	},
};

/* define EMI bandwiwth limiter control table */
static struct emi_bwl_ctrl ctrl_tbl[NR_CON_SCE];

/* current concurrency scenario */
static int cur_con_sce = 0x0FFFFFFF;

/* define concurrency scenario strings */
static const char * const con_sce_str[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) (#con_sce),
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

/****************** For LPDDR3-1600******************/

static const unsigned int emi_arba_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arba,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbb_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbb,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbc_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbc,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbd_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbd,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbe_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbe,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbf_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbf,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbg_lpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbg,
#include "mach/con_sce_lpddr3_1600.h"
#undef X_CON_SCE
};


/*
 * mtk_mem_bw_ctrl: set EMI bandwidth limiter for memory bandwidth control
 * @sce: concurrency scenario ID
 * @op: either ENABLE_CON_SCE or DISABLE_CON_SCE
 * Return 0 for success; return negative values for failure.
 */
int mtk_mem_bw_ctrl(int sce, int op)
{
	int i, highest;

	if (sce >= NR_CON_SCE)
		return -1;

	if (op != ENABLE_CON_SCE && op != DISABLE_CON_SCE)
		return -1;

	if (in_interrupt())
		return -1;

	down(&emi_bwl_sem);

	if (op == ENABLE_CON_SCE) {
		ctrl_tbl[sce].ref_cnt++;
	} else if (op == DISABLE_CON_SCE) {
		if (ctrl_tbl[sce].ref_cnt != 0)
			ctrl_tbl[sce].ref_cnt--;
	}

	/* find the scenario with the highest priority */
	highest = -1;
	for (i = 0; i < NR_CON_SCE; i++) {
		if (ctrl_tbl[i].ref_cnt != 0) {
			highest = i;
			break;
		}
	}

	if (highest == -1)
		highest = CON_SCE_NORMAL;

    /* set new EMI bandwidth limiter value */
	if (highest != cur_con_sce) {
		mt_reg_sync_writel(emi_arba_lpddr3_1600_val[highest], EMI_ARBA);
		mt_reg_sync_writel(emi_arbb_lpddr3_1600_val[highest], EMI_ARBB);
		mt_reg_sync_writel(emi_arbc_lpddr3_1600_val[highest], EMI_ARBC);
		mt_reg_sync_writel(emi_arbd_lpddr3_1600_val[highest], EMI_ARBD);
		mt_reg_sync_writel(emi_arbe_lpddr3_1600_val[highest], EMI_ARBE);
		mt_reg_sync_writel(emi_arbf_lpddr3_1600_val[highest], EMI_ARBF);
		mt_reg_sync_writel(emi_arbg_lpddr3_1600_val[highest], EMI_ARBG);
		cur_con_sce = highest;
	}

	up(&emi_bwl_sem);

	return 0;
}

/*
 * con_sce_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t con_sce_show(struct device_driver *driver, char *buf)
{
	if (cur_con_sce >= NR_CON_SCE)
		sprintf(buf, "none\n");
	else
		sprintf(buf, "%s\n", con_sce_str[cur_con_sce]);

	return strlen(buf);
}

/*
 * con_sce_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t con_sce_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;

	for (i = 0; i < NR_CON_SCE; i++) {
		if (!strncmp(buf, con_sce_str[i], strlen(con_sce_str[i]))) {
			if (!strncmp(buf + strlen(con_sce_str[i]) + 1, EN_CON_SCE_STR, strlen(EN_CON_SCE_STR))) {
				mtk_mem_bw_ctrl(i, ENABLE_CON_SCE);
				/*printk("concurrency scenario %s ON\n", con_sce_str[i]);*/
				break;
			} else if (!strncmp(buf + strlen(con_sce_str[i]) + 1, DIS_CON_SCE_STR,
						strlen(DIS_CON_SCE_STR))) {
				mtk_mem_bw_ctrl(i, DISABLE_CON_SCE);
				/*printk("concurrency scenario %s OFF\n", con_sce_str[i]);*/
				break;
			}
		}
	}

	return count;
}

DRIVER_ATTR(concurrency_scenario, 0644, con_sce_show, con_sce_store);

/*
 * emi_bwl_mod_init: module init function.
 */
static int __init emi_bwl_mod_init(void)
{
	int ret;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8167-emi");
	if (node)
		EMI_BASE_ADDR = of_iomap(node, 0);
	else
		return -1;

	ret = mtk_mem_bw_ctrl(CON_SCE_NORMAL, ENABLE_CON_SCE);
	if (ret)
		pr_err("fail to set EMI bandwidth limiter\n");

    /* Register BW ctrl interface */
	ret = platform_driver_register(&mem_bw_ctrl);
	if (ret)
		pr_err("fail to register EMI_BW_LIMITER driver\n");

	ret = driver_create_file(&mem_bw_ctrl.driver, &driver_attr_concurrency_scenario);
	if (ret)
		pr_err("fail to create EMI_BW_LIMITER sysfs file\n");


	return 0;
}

/*
 * emi_bwl_mod_exit: module exit function.
 */
static void __exit emi_bwl_mod_exit(void)
{
}

late_initcall(emi_bwl_mod_init);
module_exit(emi_bwl_mod_exit);

