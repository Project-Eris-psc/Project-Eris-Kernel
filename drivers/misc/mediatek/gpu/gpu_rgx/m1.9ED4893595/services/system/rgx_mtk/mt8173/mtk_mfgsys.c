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
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/version.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/pm_wakeup.h>

#include <trace/events/mtk_events.h>
#include <linux/mtk_gpu_utility.h>
#include <dt-bindings/clock/mt8173-clk.h>

#include <mt_gpufreq.h>
#include "mtk_mfgsys.h"
#include "pvr_gputrace.h"
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"
#include "device.h"
#include "rgxinit.h"

#ifdef CONFIG_MTK_HIBERNATION
#include "sysconfig.h"
#include <mach/mtk_hibernate_dpm.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#endif



#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50
#define MTK_SYS_BOOST_DURATION_MS       50
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 5000
#define MTK_GPIO_REG_OFFSET             0x30
#define MTK_GPU_FREQ_ID_INVALID         0xFFFFFFFF
#define MTK_RGX_DEVICE_INDEX_INVALID    -1

static IMG_HANDLE g_RGXutilUser;

#if defined(MTK_USE_HW_APM)
static IMG_PVOID g_pvRegsKM;
#endif

#ifdef MTK_CAL_POWER_INDEX
static IMG_PVOID g_pvRegsBaseKM;
#endif

static IMG_BOOL g_bDeviceInit = IMG_FALSE;
static IMG_BOOL g_bUnsync = IMG_FALSE;
static IMG_UINT32 g_ui32_unsync_freq_id;


static void mtk_mfg_enable_hw_apm(void);
static void mtk_mfg_disable_hw_apm(void);

static struct platform_device *sPVRLDMDev;
static struct platform_device *sMFGASYNCDev;
static struct platform_device *sMFG2DDev;
#define GET_MTK_MFG_BASE(x) (struct mtk_mfg_base *)(x->dev.platform_data)

static char *top_mfg_clk_name[] = {
	"mfg_mem_in_sel",
	"mfg_axi_in_sel",
	"top_axi",
	"top_mem",
	"top_mfg",
};
#define MAX_TOP_MFG_CLK ARRAY_SIZE(top_mfg_clk_name)

#define REG_MFG_AXI BIT(0)
#define REG_MFG_MEM BIT(1)
#define REG_MFG_G3D BIT(2)
#define REG_MFG_26M BIT(3)
#define REG_MFG_ALL (REG_MFG_AXI | REG_MFG_MEM | REG_MFG_G3D | REG_MFG_26M)

#define REG_MFG_CG_STA 0x00
#define REG_MFG_CG_SET 0x04
#define REG_MFG_CG_CLR 0x08

static void mtk_mfg_set_clock_gating(void __iomem *reg)
{
	writel(REG_MFG_ALL, reg + REG_MFG_CG_SET);
}

static void mtk_mfg_clr_clock_gating(void __iomem *reg)
{
	writel(REG_MFG_ALL, reg + REG_MFG_CG_CLR);
}

#ifdef CONFIG_MTK_HIBERNATION
static int gpu_pm_restore_noirq(struct device *device)
{
#if defined(MTK_CONFIG_OF) && defined(CONFIG_OF)
	int irq = MTKSysGetIRQ();
#else
	int irq = SYS_MTK_RGX_IRQ;
#endif

	mt_irq_set_sens(irq, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(irq, MT_POLARITY_LOW);

	return 0;
}
#endif /* CONFIG_MTK_HIBERNATION */

static PVRSRV_DEVICE_NODE *MTKGetRGXDevNode(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_UINT32 i;

	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
		PVRSRV_DEVICE_NODE *psDeviceNode = &psPVRSRVData->psDeviceNodeList[i];

		if (psDeviceNode
			&& psDeviceNode->psDevConfig
			/* && psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX */
		)
			return psDeviceNode;
	}

	return NULL;
}

static void MTKWriteBackFreqToRGX(PVRSRV_DEVICE_NODE *psDevNode, IMG_UINT32 ui32NewFreq)
{
	RGX_DATA *psRGXData = (RGX_DATA *)psDevNode->psDevConfig->hDevData;

	/* kHz to Hz write to RGX as the same unit */
	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000;
}

static void mtk_mfg_enable_clock(void)
{
	int i;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	/*
	** Hold wakelock when mfg power-on, prevent suspend when gpu active.
	** When enter system suspend flow, forbbiden power domain control.
	** If power domain can control, async/2d/mfg has sequence issue.
	*/
	pm_stay_awake(&mfg_base->mfg_async_pdev->dev);

	/* Resume mfg_async power domain */
	pm_runtime_get_sync(&mfg_base->mfg_async_pdev->dev);

	/* Vgpu is power on when resume mfg_async power domain in scpsys */
	mt_gpufreq_voltage_enable_set(1);

	/* Resume mfg/mfg_2d power domain */
	pm_runtime_get_sync(&mfg_base->mfg_2d_pdev->dev);
	pm_runtime_get_sync(&mfg_base->pdev->dev);

#ifdef ENABLE_COMMON_DVFS
	/* Trigger ged timer-based dvfs in sw-apm. */
	ged_dvfs_gpu_clock_switch_notify(1);
#endif

	/* Prepare and enable mfg top clock */
	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		clk_prepare(mfg_base->top_clk[i]);
		clk_enable(mfg_base->top_clk[i]);
	}

	/* Enable(un-gated) mfg clock */
	mtk_mfg_clr_clock_gating(mfg_base->reg_base);
}

static void mtk_mfg_disable_clock(void)
{
	int i;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	/* Disable(gated) mfg clock */
	mtk_mfg_set_clock_gating(mfg_base->reg_base);

	/* Disable and unprepare mfg top clock */
	for (i = MAX_TOP_MFG_CLK - 1; i >= 0; i--) {
		clk_disable(mfg_base->top_clk[i]);
		clk_unprepare(mfg_base->top_clk[i]);
	}

#ifdef ENABLE_COMMON_DVFS
	/* Disable ged timer-based dvfs. */
	ged_dvfs_gpu_clock_switch_notify(0);
#endif

	/* Suspend mfg/mfg_2d power domain */
	pm_runtime_put_sync(&mfg_base->pdev->dev);
	pm_runtime_put_sync(&mfg_base->mfg_2d_pdev->dev);

	/* Vgpu is power off when suspend mfg_async power domain in scpsys */
	mt_gpufreq_voltage_enable_set(0);

	/* Suspend mfg_async power domain */
	pm_runtime_put_sync(&mfg_base->mfg_async_pdev->dev);

	/* Release wakelock when mfg power-off */
	pm_relax(&mfg_base->mfg_async_pdev->dev);
}

static int mfg_notify_handler(struct notifier_block *this, unsigned long code,
			      void *unused)
{
	struct mtk_mfg_base *mfg_base = container_of(this,
						     typeof(*mfg_base),
						     mfg_notifier);
	if ((code != SYS_RESTART) && (code != SYS_POWER_OFF))
		return 0;

	pr_info("PVR_K: shutdown notified, code=%x\n", (unsigned int)code);

	mutex_lock(&mfg_base->set_power_state);

	mtk_mfg_enable_clock();
	mtk_mfg_enable_hw_apm();

	mfg_base->shutdown = true;

	mutex_unlock(&mfg_base->set_power_state);

	return 0;
}

static void MTKEnableClock(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);
	if (mfg_base->shutdown) {
		mutex_unlock(&mfg_base->set_power_state);
		pr_info("skip MTKEnableClock\n");
		return;
	}
	mtk_mfg_enable_clock();
	mutex_unlock(&mfg_base->set_power_state);
}

static void MTKDisableClock(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);
	if (mfg_base->shutdown) {
		mutex_unlock(&mfg_base->set_power_state);
		pr_info("skip MTKDisableClock\n");
		return;
	}
	mtk_mfg_disable_clock();
	mutex_unlock(&mfg_base->set_power_state);
}

#if defined(MTK_USE_HW_APM)
static void mtk_mfg_enable_hw_apm(void)
{
	if (!g_pvRegsKM) {
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			IMG_CPU_PHYADDR sRegsPBase;
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
			PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;

			if (psDevInfo)
				PVR_DPF((PVR_DBG_ERROR, "psDevInfo->pvRegsBaseKM: %p", psDevInfo->pvRegsBaseKM));

			if (psDevConfig) {
				sRegsPBase = psDevConfig->sRegsCpuPBase;
				sRegsPBase.uiAddr += 0xFFF000;
				g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);

				PVR_DPF((PVR_DBG_ERROR, "sRegsCpuPBase.uiAddr: 0x%08lx",
					(unsigned long)psDevConfig->sRegsCpuPBase.uiAddr));
				PVR_DPF((PVR_DBG_ERROR, "sRegsPBase.uiAddr: 0x%08lx",
					(unsigned long)sRegsPBase.uiAddr));
			}
		}
	}

	if (g_pvRegsKM) {
		writel(0x004a3d4d, g_pvRegsKM + 0x24);
		writel(0x4d45520b, g_pvRegsKM + 0x28);
		writel(0x7a710184, g_pvRegsKM + 0xe0);
		writel(0x835f6856, g_pvRegsKM + 0xe4);
		writel(0x00470248, g_pvRegsKM + 0xe8);
		writel(0x80000000, g_pvRegsKM + 0xec);
		writel(0x08000000, g_pvRegsKM + 0xa0);
	}
}

static void mtk_mfg_disable_hw_apm(void)
{
}
#else
static void mtk_mfg_enable_hw_apm(void) {};
static void mtk_mfg_disable_hw_apm(void) {};
#endif /* MTK_USE_HW_APM */

static int mtk_mfg_bind_device_resource(struct platform_device *pdev,
				 struct mtk_mfg_base *mfg_base)
{
	int i, err;
	int len = sizeof(struct clk *) * MAX_TOP_MFG_CLK;

	mfg_base->top_clk = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!mfg_base->top_clk)
		return -ENOMEM;

	mfg_base->reg_base = of_iomap(pdev->dev.of_node, 1);
	if (!mfg_base->reg_base) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Unable to ioremap registers pdev %p", __func__, pdev));
		return -ENOMEM;
	}

	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		mfg_base->top_clk[i] = devm_clk_get(&pdev->dev,
						    top_mfg_clk_name[i]);
		if (IS_ERR(mfg_base->top_clk[i])) {
			err = PTR_ERR(mfg_base->top_clk[i]);
			PVR_DPF((PVR_DBG_ERROR, "%s: devm_clk_get %s failed", __func__, top_mfg_clk_name[i]));
			goto err_iounmap_reg_base;
		}
	}

	if (!sMFGASYNCDev || !sMFG2DDev) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get pm_domain", __func__));
		err = -EPROBE_DEFER;
		goto err_iounmap_reg_base;
	}

	mfg_base->mfg_2d_pdev = sMFG2DDev;
	mfg_base->mfg_async_pdev = sMFGASYNCDev;

	mfg_base->mfg_notifier.notifier_call = mfg_notify_handler;
	register_reboot_notifier(&mfg_base->mfg_notifier);

	pm_runtime_enable(&pdev->dev);

	mfg_base->pdev = pdev;
	return 0;

err_iounmap_reg_base:
	iounmap(mfg_base->reg_base);
	return err;
}

static int mtk_mfg_unbind_device_resource(struct platform_device *pdev,
				   struct mtk_mfg_base *mfg_base)
{
	mfg_base->pdev = NULL;

	if (mfg_base->mfg_notifier.notifier_call) {
		unregister_reboot_notifier(&mfg_base->mfg_notifier);
		mfg_base->mfg_notifier.notifier_call = NULL;
	}

	iounmap(mfg_base->reg_base);
	pm_runtime_disable(&pdev->dev);

	return 0;
}


static IMG_BOOL bCoreinitSucceeded = IMG_FALSE;
bool mt_gpucore_ready(void)
{
	return (bCoreinitSucceeded == IMG_TRUE);
}
EXPORT_SYMBOL(mt_gpucore_ready);

void MTKFWDump(void)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	MTK_PVRSRVDebugRequestSetSilence(IMG_TRUE);
	PVRSRVDebugRequest(psDevNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
	MTK_PVRSRVDebugRequestSetSilence(IMG_FALSE);
}
EXPORT_SYMBOL(MTKFWDump);

int MTKRGXDeviceInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	struct platform_device *pdev;
	struct mtk_mfg_base *mfg_base;
	int err;

	pdev = to_platform_device((struct device *)psDevConfig->pvOSDevice);

	mfg_base = devm_kzalloc(&pdev->dev, sizeof(*mfg_base), GFP_KERNEL);
	if (!mfg_base)
		return -ENOMEM;

	err = mtk_mfg_bind_device_resource(pdev, mfg_base);
	if (err != 0)
		return err;

	mutex_init(&mfg_base->set_power_state);

	/* attach mfg_base to pdev->dev.platform_data */
	pdev->dev.platform_data = mfg_base;
	sPVRLDMDev = pdev;

	bCoreinitSucceeded = IMG_TRUE;
	return 0;
}

int MTKRGXDeviceDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	struct platform_device *pdev;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	pdev = to_platform_device((struct device *)psDevConfig->pvOSDevice);
	if (pdev != sPVRLDMDev) {
		PVR_DPF((PVR_DBG_ERROR, "%s: release %p != %p", __func__, pdev, sPVRLDMDev));
		return 0;
	}

	mtk_mfg_unbind_device_resource(pdev, mfg_base);

	return 0;
}

#ifdef MTK_CAL_POWER_INDEX
static IMG_UINT32 MTKGetRGXDevIdx(void)
{
	static IMG_UINT32 ms_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;

	if (ms_ui32RGXDevIdx == MTK_RGX_DEVICE_INDEX_INVALID) {
		PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
		IMG_UINT32 i;

		for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
			PVRSRV_DEVICE_NODE *psDeviceNode = &psPVRSRVData->psDeviceNodeList[i];

			if (psDeviceNode
				&& psDeviceNode->psDevConfig
				/* && psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX */
			) {
				ms_ui32RGXDevIdx = i;
				break;
			}
		}
	}

	return ms_ui32RGXDevIdx;
}

static void MTKStartPowerIndex(void)
{
	if (!g_pvRegsBaseKM) {
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;

			if (psDevInfo)
				g_pvRegsBaseKM = psDevInfo->pvRegsBaseKM;
		}
	}

	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static void MTKReStartPowerIndex(void)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static void MTKStopPowerIndex(void)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
}

static IMG_UINT32 MTKCalPowerIndex(void)
{
	IMG_UINT32 ui32State, ui32Result;
	PVRSRV_DEV_POWER_STATE  ePowerState;
	IMG_BOOL bTimeout;
	IMG_UINT32 u32Deadline;
	IMG_PVOID pvGPIO_REG = g_pvRegsKM + (uintptr_t)MTK_GPIO_REG_OFFSET;
	IMG_PVOID pvPOWER_ESTIMATE_RESULT = g_pvRegsBaseKM + (uintptr_t)6328;

	if ((!g_pvRegsKM) || (!g_pvRegsBaseKM))
		return 0;

	if (PVRSRVPowerLock() != PVRSRV_OK)
		return 0;

	PVRSRVGetDevicePowerState(MTKGetRGXDevIdx(), &ePowerState);
	if (ePowerState != PVRSRV_DEV_POWER_STATE_ON) {
		PVRSRVPowerUnlock();
		return 0;
	}

	/* writes 1 to GPIO_INPUT_REQ, bit[0] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) | 0x1);

	/* wait for 1 in GPIO_OUTPUT_REQ, bit[16] */
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
	while (OSClockus() < u32Deadline) {
		if (0x10000 & DRV_Reg32(pvGPIO_REG)) {
			bTimeout = IMG_FALSE;
			break;
		}
	}

	/* writes 0 to GPIO_INPUT_REQ, bit[0] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x1));
	if (bTimeout) {
		PVRSRVPowerUnlock();
		return 0;
	}

	/* read GPIO_OUTPUT_DATA, bit[24] */
	ui32State = DRV_Reg32(pvGPIO_REG) >> 24;

	/* read POWER_ESTIMATE_RESULT */
	ui32Result = DRV_Reg32(pvPOWER_ESTIMATE_RESULT);

	/* writes 1 to GPIO_OUTPUT_ACK, bit[17] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG)|0x20000);

	/* wait for 0 in GPIO_OUTPUT_REG, bit[16] */
	bTimeout = IMG_TRUE;
	u32Deadline = OSClockus() + MTK_WAIT_FW_RESPONSE_TIMEOUT_US;
	while (OSClockus() < u32Deadline) {
		if (!(0x10000 & DRV_Reg32(pvGPIO_REG))) {
			bTimeout = IMG_FALSE;
			break;
		}
	}

	/* writes 0 to GPIO_OUTPUT_ACK, bit[17] */
	DRV_WriteReg32(pvGPIO_REG, DRV_Reg32(pvGPIO_REG) & (~0x20000));
	if (bTimeout) {
		PVRSRVPowerUnlock();
		return 0;
	}

	MTKReStartPowerIndex();
	PVRSRVPowerUnlock();

	return (ui32State == 1) ? ui32Result : 0;
}
#endif

#ifdef ENABLE_COMMON_DVFS
static void MTKCalGpuLoading(unsigned int *pui32Loading, unsigned int *pui32Block, unsigned int *pui32Idle)
{
	PVRSRV_DEVICE_NODE *psDevNode;
	PVRSRV_RGXDEV_INFO *psDevInfo;

	psDevNode = MTKGetRGXDevNode();
	if (!psDevNode)
		return;

	psDevInfo = (PVRSRV_RGXDEV_INFO *)(psDevNode->pvDevice);
	if (psDevInfo && psDevInfo->pfnGetGpuUtilStats) {
		RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};

		/* 1.5DDK support multiple user to query GPU utilization, Need Init here */
		if (g_RGXutilUser == NULL) {
			if (psDevInfo)
				RGXRegisterGpuUtilStats(&g_RGXutilUser);
		}

		if (g_RGXutilUser == NULL)
			return;

		psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode, g_RGXutilUser, &sGpuUtilStats);
		if (sGpuUtilStats.bValid) {
#if defined(__arm64__) || defined(__aarch64__)
			*pui32Loading = (100*(sGpuUtilStats.ui64GpuStatActiveHigh +
				sGpuUtilStats.ui64GpuStatActiveLow)) /
				sGpuUtilStats.ui64GpuStatCumulative;
			*pui32Block = (100*(sGpuUtilStats.ui64GpuStatBlocked)) /
				sGpuUtilStats.ui64GpuStatCumulative;
			*pui32Idle = (100*(sGpuUtilStats.ui64GpuStatIdle)) /
				sGpuUtilStats.ui64GpuStatCumulative;
#else
			*pui32Loading = (unsigned long)(100*(sGpuUtilStats.ui64GpuStatActiveHigh +
				sGpuUtilStats.ui64GpuStatActiveLow)) /
				(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
			*pui32Block = (unsigned long)(100*(sGpuUtilStats.ui64GpuStatBlocked)) /
				(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
			*pui32Idle = (unsigned long)(100*(sGpuUtilStats.ui64GpuStatIdle)) /
				(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
#endif
		}
	}
}

/* For ged_dvfs idx commit */
static void MTKCommitFreqIdx(unsigned long ui32NewFreqID, GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	PVRSRV_ERROR eResult;
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	if (psDevNode) {
		eResult = PVRSRVDevicePreClockSpeedChange(psDevNode, IMG_FALSE, (void *)NULL);
		if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
			unsigned int ui32GPUFreq;
			unsigned int ui32CurFreqID;
			PVRSRV_DEV_POWER_STATE ePowerState;

			PVRSRVGetDevicePowerState(psDevNode, &ePowerState);

			if (ePowerState == PVRSRV_DEV_POWER_STATE_ON) {
				mt_gpufreq_target(ui32NewFreqID);
				g_bUnsync = IMG_FALSE;
			} else {
				g_ui32_unsync_freq_id = ui32NewFreqID;
				g_bUnsync = IMG_TRUE;
			}

			ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
			ui32GPUFreq = mt_gpufreq_get_freq_by_idx(ui32CurFreqID);

#if defined(CONFIG_TRACING) && defined(CONFIG_MTK_SCHED_TRACERS)
			if (PVRGpuTraceEnabled())
				trace_gpu_freq(ui32GPUFreq);
#endif
			MTKWriteBackFreqToRGX(psDevNode, ui32GPUFreq);

#ifdef MTK_DEBUG
			pr_info("3DFreq=%d, Volt=%d\n", ui32GPUFreq, mt_gpufreq_get_cur_volt());
#endif

			if (eResult == PVRSRV_OK)
				PVRSRVDevicePostClockSpeedChange(psDevNode, IMG_FALSE, (void *)NULL);

			/* Always return true because the APM would almost letting GPU power down */
			/* with high possibility while DVFS committing */
			if (pbCommited)
				*pbCommited = IMG_TRUE;

			return;
		}
	}

	if (pbCommited)
		*pbCommited = IMG_FALSE;
}
#endif

static bool MTKCheckDeviceInit(void)
{
	bool ret = false;
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	if (psDevNode) {
		if (psDevNode->eDevState == PVRSRV_DEVICE_STATE_ACTIVE)
			ret = true;
	}

	return ret;
}

PVRSRV_ERROR MTKDevPrePowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				 IMG_BOOL bForced)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);

	if ((eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF) &&
	    (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON)) {
		if (g_bDeviceInit) {
#ifdef MTK_CAL_POWER_INDEX
			MTKStopPowerIndex();
#endif
		} else
			g_bDeviceInit = MTKCheckDeviceInit();

		mtk_mfg_disable_hw_apm();
		mtk_mfg_disable_clock();
	}

	mutex_unlock(&mfg_base->set_power_state);
	return PVRSRV_OK;

}

PVRSRV_ERROR MTKDevPostPowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				  PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				  IMG_BOOL bForced)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);

	if ((eNewPowerState == PVRSRV_DEV_POWER_STATE_ON) &&
	    (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)) {
		mtk_mfg_enable_clock();
		mtk_mfg_enable_hw_apm();

		if (g_bDeviceInit) {
#ifdef MTK_CAL_POWER_INDEX
			MTKStartPowerIndex();
#endif
		} else
			g_bDeviceInit = MTKCheckDeviceInit();

		if (g_bUnsync == IMG_TRUE) {
			mt_gpufreq_target(g_ui32_unsync_freq_id);
			g_bUnsync = IMG_FALSE;
		}
	}

	mutex_unlock(&mfg_base->set_power_state);
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	return PVRSRV_OK;
}


PVRSRV_ERROR MTKMFGSystemInit(void)
{
#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_GPU, gpu_pm_restore_noirq, NULL);
#endif

#if defined(MTK_USE_HW_APM)
	if (!g_pvRegsKM) {
		PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

		if (psDevNode) {
			IMG_CPU_PHYADDR sRegsPBase;
			PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;

			if (psDevConfig && (!g_pvRegsKM)) {
				sRegsPBase = psDevConfig->sRegsCpuPBase;
				sRegsPBase.uiAddr += 0xFFF000;
				g_pvRegsKM = OSMapPhysToLin(sRegsPBase, 0xFF, 0);
			}
		}
	}
#endif

#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_cal_gpu_utilization_fp = MTKCalGpuLoading;
	ged_dvfs_gpu_freq_commit_fp = MTKCommitFreqIdx;
#endif

	mt_gpufreq_mfgclock_notify_registerCB(MTKEnableClock, MTKDisableClock);

	return PVRSRV_OK;
}

void MTKMFGSystemDeInit(void)
{
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_GPU);
#endif

#ifdef MTK_CAL_POWER_INDEX
	g_pvRegsBaseKM = NULL;
#endif

#if defined(MTK_USE_HW_APM)
	if (g_pvRegsKM) {
		OSUnMapPhysToLin(g_pvRegsKM, 0xFF, 0);
		g_pvRegsKM = NULL;
	}
#endif
}


static int mtk_mfg_async_probe(struct platform_device *pdev)
{
	pr_info("mtk_mfg_async_probe\n");

	if (!pdev->dev.pm_domain) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get dev->pm_domain", __func__));
		return -EPROBE_DEFER;
	}

	sMFGASYNCDev = pdev;
	pm_runtime_enable(&pdev->dev);

	/* Use async power domain as a system suspend indicator. */
	device_init_wakeup(&pdev->dev, true);
	return 0;
}

static int mtk_mfg_async_remove(struct platform_device *pdev)
{
	device_init_wakeup(&pdev->dev, false);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_mfg_async_of_ids[] = {
	{ .compatible = "mediatek,mt8173-mfg-async",},
	{}
};

static struct platform_driver mtk_mfg_async_driver = {
	.probe  = mtk_mfg_async_probe,
	.remove = mtk_mfg_async_remove,
	.driver = {
		.name = "mfg-async",
		.of_match_table = mtk_mfg_async_of_ids,
	}
};

#if defined(MODULE)
int mtk_mfg_async_init(void)
#else
static int __init mtk_mfg_async_init(void)
#endif
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_async_driver);
	if (ret != 0) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to register mfg driver", __func__));
		return ret;
	}

	return ret;
}
subsys_initcall(mtk_mfg_async_init);

static int mtk_mfg_2d_probe(struct platform_device *pdev)
{
	pr_info("mtk_mfg_2d_probe\n");

	if (!pdev->dev.pm_domain) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get dev->pm_domain", __func__));
		return -EPROBE_DEFER;
	}

	sMFG2DDev = pdev;
	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int mtk_mfg_2d_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_mfg_2d_of_ids[] = {
	{ .compatible = "mediatek,mt8173-mfg-2d",},
	{}
};

static struct platform_driver mtk_mfg_2d_driver = {
	.probe  = mtk_mfg_2d_probe,
	.remove = mtk_mfg_2d_remove,
	.driver = {
		.name = "mfg-2d",
		.of_match_table = mtk_mfg_2d_of_ids,
	}
};

#if defined(MODULE)
int mtk_mfg_2d_init(void)
#else
static int __init mtk_mfg_2d_init(void)
#endif
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_2d_driver);
	if (ret != 0) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to register mfg driver", __func__));
		return ret;
	}

	return ret;
}
subsys_initcall(mtk_mfg_2d_init);
