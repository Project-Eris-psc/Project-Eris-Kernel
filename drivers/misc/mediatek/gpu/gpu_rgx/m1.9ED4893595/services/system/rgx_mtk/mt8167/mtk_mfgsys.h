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

#ifndef MTK_MFGSYS_H
#define MTK_MFGSYS_H

#include "servicesext.h"
#include "rgxdevice.h"
#include "ged_dvfs.h"
#include <linux/regulator/consumer.h>

/* Control SW APM is enabled or not  */
#ifndef MTK_BRINGUP
#define MTK_PM_SUPPORT 1
#else
#define MTK_PM_SUPPORT 0
#endif

struct mtk_mfg_base {
	struct platform_device *pdev;
	struct platform_device *mfg_2d_pdev;
	struct platform_device *mfg_async_pdev;

	struct clk **top_clk_sel;
	struct clk **top_clk_sel_parent;
	struct clk **top_clk;
	void __iomem *reg_base;

	/* mutex protect for set power state */
	struct mutex set_power_state;
	bool shutdown;
	struct notifier_block mfg_notifier;
};

PVRSRV_ERROR MTKMFGSystemInit(void);
void MTKMFGSystemDeInit(void);
void MTKDisablePowerDomain(void);
void MTKFWDump(void);

/* below register interface in RGX sysconfig.c */
PVRSRV_ERROR MTKDevPrePowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				 IMG_BOOL bForced);

PVRSRV_ERROR MTKDevPostPowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				  PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				  IMG_BOOL bForced);

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

int MTKRGXDeviceInit(PVRSRV_DEVICE_CONFIG *psDevConfig);
int MTKRGXDeviceDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig);

/* from gpu/ged/src/ged_dvfs.c */
extern void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int *pui32Loading,
											   unsigned int *pui32Block,
											   unsigned int *pui32Idle);
extern void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID,
										   GED_DVFS_COMMIT_TYPE eCommitType,
										   int *pbCommited);

#ifdef CONFIG_MTK_HIBERNATION
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
int gpu_pm_restore_noirq(struct device *device);
#endif

#ifdef SUPPORT_PDVFS
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
#endif

#if defined(MODULE)
int mtk_mfg_async_init(void);
int mtk_mfg_2d_init(void);
#endif

#endif

