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

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <mt-plat/aee.h>
#include <linux/interrupt.h>
#include <mt-plat/sync_write.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"

/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(void)
{
	pr_debug("[SCP] CM4 A WDT exception\n");
	scp_A_dump_regs();
}

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	unsigned int reg = SCP_A_TO_HOST_REG;
	scp_excep_id reset_type;
	int reboot = 0;

	if (reg & SCP_IRQ_WDT) {
		scp_A_wdt_handler();
		reboot = 1;
		reset_type = EXCEP_RUNTIME;
		reg &= SCP_IRQ_WDT;
	}

	if (reg & SCP_IRQ_SCP2HOST) {
		/* if WDT and IPI triggered on the same time, ignore the IPI */
		if (!reboot)
			scp_A_ipi_handler();
		reg &= SCP_IRQ_SCP2HOST;
	}

	SCP_A_TO_HOST_REG = reg;

	if (reboot)
		scp_aed_reset(reset_type, SCP_A_ID);

	return IRQ_HANDLED;
}

/*
 * scp irq initialize
 */
void scp_A_irq_init(void)
{
	SCP_A_TO_HOST_REG = 1;  /* clear scp irq */
}


/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_B_wdt_handler(void)
{
	pr_debug("[SCP] CM4 B WDT exception\n");
	scp_B_dump_regs();
}

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_B_irq_handler(int irq, void *dev_id)
{
	unsigned int reg = SCP_B_TO_HOST_REG;
	scp_excep_id reset_type;
	int reboot = 0;

	if (reg & SCP_IRQ_WDT) {
		scp_B_wdt_handler();
		reboot = 1;
		reset_type = EXCEP_RUNTIME;
		reg &= SCP_IRQ_WDT;
	}

	if (reg & SCP_IRQ_SCP2HOST) {
		/* if WDT and IPI triggered on the same time, ignore the IPI */
		if (!reboot)
			scp_B_ipi_handler();
		reg &= SCP_IRQ_SCP2HOST;
	}

	SCP_B_TO_HOST_REG = reg;

	if (reboot)
		scp_aed_reset(reset_type, SCP_B_ID);

	return IRQ_HANDLED;
}

/*
 * scp irq initialize
 */
void scp_B_irq_init(void)
{
	SCP_B_TO_HOST_REG = 1;  /* clear scp irq */
}
