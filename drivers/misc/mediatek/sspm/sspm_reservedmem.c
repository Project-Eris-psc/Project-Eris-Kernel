/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_excep.h"
#include "sspm_reservedmem.h"
#define _SSPM_INTERNAL_
#include "sspm_reservedmem_define.h"

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#include <mt-plat/mtk_memcfg.h>

#define SSPM_MEM_RESERVED_KEY "mediatek,reserve-memory-sspm_share"
#endif

static phys_addr_t sspm_mem_base_phys;
static phys_addr_t sspm_mem_base_virt;
static phys_addr_t sspm_mem_size;

#ifdef CONFIG_OF_RESERVED_MEM
static int __init sspm_reserve_mem_of_init(struct reserved_mem *rmem)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

	sspm_mem_base_phys = (phys_addr_t) rmem->base;
	sspm_mem_size = (phys_addr_t) rmem->size;

	pr_debug("[SSPM] phys:0x%llx - 0x%llx (0x%llx)\n", (phys_addr_t)rmem->base,
			(phys_addr_t)rmem->base + (phys_addr_t)rmem->size, (phys_addr_t)rmem->size);
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		sspm_reserve_mblock[id].start_phys = sspm_mem_base_phys + accumlate_memory_size;
		accumlate_memory_size += sspm_reserve_mblock[id].size;

		if (accumlate_memory_size > sspm_mem_size) {
			sspm_reserve_mblock[id].start_phys = 0;
			break;
		}

		pr_debug("[SSPM][reserve_mem:%d]: phys:0x%llx - 0x%llx (0x%llx)\n", id,
				sspm_reserve_mblock[id].start_phys,
				sspm_reserve_mblock[id].start_phys + sspm_reserve_mblock[id].size,
				sspm_reserve_mblock[id].size);
	}
	return 0;
}

RESERVEDMEM_OF_DECLARE(sspm_reservedmem, SSPM_MEM_RESERVED_KEY, sspm_reserve_mem_of_init);
#endif

phys_addr_t sspm_reserve_mem_get_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_phys);

phys_addr_t sspm_reserve_mem_get_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_virt);

phys_addr_t sspm_reserve_mem_get_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SSPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return sspm_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(sspm_reserve_mem_get_size);


int sspm_reserve_memory_init(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;

	if (NUMS_MEM_ID == 0)
		return 0;

	if (sspm_mem_base_phys == 0)
		return -1;

	accumlate_memory_size = 0;
	sspm_mem_base_virt = (phys_addr_t)ioremap_nocache(sspm_mem_base_phys, sspm_mem_size);
	pr_debug("[SSPM]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n", (phys_addr_t)sspm_mem_base_virt,
			(phys_addr_t)sspm_mem_base_virt + (phys_addr_t)sspm_mem_size, sspm_mem_size);
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (sspm_reserve_mblock[id].start_phys == 0)
			break;

		sspm_reserve_mblock[id].start_virt = sspm_mem_base_virt + accumlate_memory_size;
		accumlate_memory_size += sspm_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or sspm_reserve_mblock does not match dts
	 */

	BUG_ON(accumlate_memory_size > sspm_mem_size);
#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_debug("[SSPM][mem_reserve-%d] phys:0x%llx,virt:0x%llx,size:0x%llx\n",
				id, sspm_reserve_mem_get_phys(id),
				sspm_reserve_mem_get_virt(id), sspm_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

void sspm_lock_emi_mpu(void)
{
#if SSPM_EMI_PROTECTION_SUPPORT
	if (sspm_mem_size > 0)
		sspm_set_emi_mpu(sspm_mem_base_phys, sspm_mem_size);
#endif
}
