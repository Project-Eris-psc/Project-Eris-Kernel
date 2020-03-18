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

#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include "../lastpc.h"
static unsigned int lastpc[4][8];

struct lastpc_imp {
	struct lastpc_plt plt;
	void __iomem *toprgu_reg;
};

#define to_lastpc_imp(p)	container_of((p), struct lastpc_imp, plt)

static int lastpc_plt_start(struct lastpc_plt *plt)
{
	return 0;
}

static int lastpc_plt_dump(struct lastpc_plt *plt, char *ptr, int len)
{
	unsigned int fp_value_32;
	unsigned int sp_value_32;
	unsigned long pc_value;
	unsigned long fp_value;
	unsigned long sp_value;
	unsigned long size = 0;
	unsigned long offset = 0;
	char str[KSYM_SYMBOL_LEN];
	int i;

	/* Get PC, FP, SP and save to buf */
	for (i = 0; i < 4; i++) {
		pc_value = (unsigned long)lastpc[i][1] << 32 | lastpc[i][0];
		fp_value_32 = lastpc[i][2];
		sp_value_32 = lastpc[i][3];
		fp_value = (unsigned long)lastpc[i][5] << 32 | lastpc[i][4];
		sp_value = (unsigned long)lastpc[i][7] << 32 | lastpc[i][6];

		kallsyms_lookup(pc_value, &size, &offset, NULL, str);
		ptr +=
		    sprintf(ptr,
			    "[LAST PC] CORE_%d PC = 0x%lx(%s + 0x%lx), FP = 0x%lx, SP = 0x%lx", i,
			    pc_value, str, offset, fp_value, sp_value);
		ptr += sprintf(ptr,
			       " FP_32 = 0x%x SP_32 = 0x%x\n", fp_value_32, sp_value_32);
		pr_err("[LAST PC] CORE_%d PC = 0x%lx(%s), FP = 0x%lx, SP = 0x%lx", i, pc_value,
			  str, fp_value, sp_value);
		pr_err("  FP_32 = 0x%x SP_32 = 0x%x\n", fp_value_32, sp_value_32);
	}

	return 0;
}

static int reboot_test(struct lastpc_plt *plt)
{
	return 0;
}

static struct lastpc_plt_operations lastpc_ops = {
	.start = lastpc_plt_start,
	.dump = lastpc_plt_dump,
	.reboot_test = reboot_test,
};

struct boot_tag_lastpc {
	unsigned int tag_size;
	unsigned int tag_magic;
	unsigned int lastpc[4][8];
};

static int __init lastpc_init(void)
{
	struct lastpc_imp *drv = NULL;
	int ret = 0;

#ifdef CONFIG_OF
	if (of_chosen) {
		struct boot_tag_lastpc *tags;

		tags = (struct boot_tag_lastpc *)of_get_property(of_chosen, "atag,lastpc", NULL);
		if (tags)
			memcpy((void *)lastpc, (void *)tags->lastpc, sizeof(lastpc));
		else
			pr_warn("[%s] No atag,lastpc found !\n", __func__);
	} else
		pr_warn("[%s] of_chosen is NULL !\n", __func__);
#endif

	drv = kzalloc(sizeof(struct lastpc_imp), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->plt.ops = &lastpc_ops;
	drv->plt.chip_code = 0x8167;
	drv->plt.min_buf_len = 2048;	/* TODO: can calculate the len by how many levels of bt we want */

	ret = lastpc_register(&drv->plt);
	if (ret) {
		pr_err("%s:%d: lastpc_register failed\n", __func__, __LINE__);
		goto register_lastpc_err;
	}

	return 0;

register_lastpc_err:
	kfree(drv);
	return ret;
}

arch_initcall(lastpc_init);
