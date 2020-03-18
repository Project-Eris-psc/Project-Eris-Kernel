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
#ifndef __MTK_HOOKS_H__
#define __MTK_HOOKS_H__

/* platform-dependent hook functions */
int arm_undefinstr_retry(struct pt_regs *regs, unsigned int instr);

/* common hook functoins */
int mem_fault_debug_hook(struct pt_regs *regs);

#endif /* __MTK_HOOKS_H__ */
