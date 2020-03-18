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

#ifndef __BUS_TRACER_V1_H__
#define __BUS_TRACER_V1_H__

#define get_bit_at(reg, pos) (((reg) >> (pos)) & 1)

#define NUM_ID_FILTER		2

#define BUS_MON_EN		(1 << 0)
#define BUS_TRACE_EN		(1 << 2)
#define TRACE_IDF_EN		(1 << 4)
#define TRACE_WP_EN		(1 << 5)
#define TRACE_BYPASS_EN		(1 << 6)
#define TRACE_CYCLE_EN		(1 << 8)
#define WDT_RST_EN		(1 << 12)

#define BYPASS_FILTER_SHIFT	12

#define BUS_MON_CON		0x0
#define BUS_TRACE_WATCHPOINT	0x10
#define BUS_TRACE_WATCHPOINT_H	0x14
#define BUS_TRACE_WATCHPOINT_MASK	0x18
#define BUS_TRACE_IDF0		0x20
#define BUS_TRACE_IDF1		0x24
#define BUS_TRACE_BYPASS_ADDR	0x28
#define BUS_TRACE_BYPASS_MASK	0x2c
#define BUS_TRACE_RW_FILTER	0x34
#define BUS_TRACE_ATID		0x3c

/* ETB registers, "CoreSight Components TRM", 9.3 */
#define ETB_DEPTH		0x04
#define ETB_STATUS		0x0c
#define ETB_READMEM		0x10
#define ETB_READADDR		0x14
#define ETB_WRITEADDR		0x18
#define ETB_TRIGGERCOUNT	0x1c
#define ETB_CTRL		0x20
#define ETB_RWD			0x24
#define ETB_LAR			0xfb0

#define DEM_ATB_CLK		0x70

#define REPLICATOR1_BASE	0x7000
#define REPLICATOR_LAR		0xfb0
#define REPLICATOR_IDFILTER0	0x0
#define REPLICATOR_IDFILTER1	0x4

#define FUNNEL_CTRL_REG		0x0
#define FUNNEL_LOCKACCESS	0xfb0

#define CORESIGHT_LAR		0xfb0
#define CORESIGHT_UNLOCK        0xc5acce55

static inline void CS_LOCK(void __iomem *addr)
{
	do {
		/* Wait for things to settle */
		mb();
		writel_relaxed(0x0, addr + CORESIGHT_LAR);
	} while (0);
}

static inline void CS_UNLOCK(void __iomem *addr)
{
	do {
		writel_relaxed(CORESIGHT_UNLOCK, addr + CORESIGHT_LAR);
		/* Make sure everyone has seen this */
		mb();
	} while (0);
}

#endif /* end of __BUS_TRACER_V1_H__ */

