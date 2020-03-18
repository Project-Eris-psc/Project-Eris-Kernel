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

/* control APM is enabled or not  */
#define MTK_PM_SUPPORT 1

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



/* extern to be used by PVRCore_Init in RGX DDK module.c */
PVRSRV_ERROR MTKMFGSystemInit(void);

void MTKMFGSystemDeInit(void);
void MTKDisablePowerDomain(void);


void MTKMFGEnableDVFSTimer(bool bEnable);

/* below register interface in RGX sysconfig.c */
PVRSRV_ERROR MTKDevPrePowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				 IMG_BOOL bForced);

PVRSRV_ERROR MTKDevPostPowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				  PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				  IMG_BOOL bForced);

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

int MTKRGXDeviceInit(void *pvOSDevice);
void MTKSetICVerion(void);

#ifdef CONFIG_MTK_HIBERNATION
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
#endif

/* from gpu/hal/mtk_gpu_utility.c */
extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern unsigned int (*mtk_get_gpu_block_fp)(void);
extern unsigned int (*mtk_get_gpu_idle_fp)(void);
extern unsigned int (*mtk_get_gpu_power_loading_fp)(void);
extern void (*mtk_enable_gpu_dvfs_timer_fp)(bool bEnable);
extern void (*mtk_boost_gpu_freq_fp)(void);
extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int);

extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);
extern void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void);
/*****/

/* from gpu/ged/src/ged_dvfs.c */
extern void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int *pui32Loading,
											   unsigned int *pui32Block,
											   unsigned int *pui32Idle);
extern void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID,
										   GED_DVFS_COMMIT_TYPE eCommitType,
										   int *pbCommited);
/*****/

#ifdef SUPPORT_PDVFS
extern int (*ged_dvfs_vsync_trigger_fp)(int idx);
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
#endif
extern char rgx_fw_name[20];

#endif

