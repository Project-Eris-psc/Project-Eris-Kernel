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
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reboot.h>
#include <linux/version.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>

#include "mtk_mfgsys.h"
#include <mtk_boot.h>
#include <mtk_gpufreq.h>
#include "pvr_gputrace.h"
#include "rgxdevice.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "rgxhwperf.h"
#include "device.h"
#include "rgxinit.h"
#include "pvr_dvfs.h"
#include "mtk_chip.h"

#ifdef CONFIG_MTK_HIBERNATION
#include "sysconfig.h"
#include <mach/mtk_hibernate_dpm.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#endif

#include <trace/events/mtk_events.h>
#include <mtk_gpu_utility.h>


typedef void IMG_VOID;

#define mt_gpufreq_get_frequency_by_level mt_gpufreq_get_freq_by_idx

#define MTK_GPU_DVFS                    1
#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50 /*16, 100*/
#define MTK_SYS_BOOST_DURATION_MS       50
#define MTK_WAIT_FW_RESPONSE_TIMEOUT_US 5000
#define MTK_GPIO_REG_OFFSET             0x30
#define MTK_GPU_FREQ_ID_INVALID         0xFFFFFFFF
#define MTK_RGX_DEVICE_INDEX_INVALID    -1

#define MFG_BUS_IDLE_BIT (1 << 16)

static IMG_HANDLE g_RGXutilUser;
static IMG_HANDLE g_hDVFSTimer;
static POS_LOCK ghDVFSTimerLock;
static POS_LOCK ghDVFSLock;

typedef void *IMG_PVOID;

#ifdef MTK_CAL_POWER_INDEX
static IMG_PVOID g_pvRegsBaseKM;
#endif

static IMG_BOOL g_bTimerEnable = IMG_FALSE;
static IMG_BOOL g_bExit = IMG_TRUE;
static IMG_INT32 g_iSkipCount;
static IMG_UINT32 g_sys_dvfs_time_ms;

static IMG_UINT32 g_bottom_freq_id;
static IMG_UINT32 gpu_bottom_freq;
static IMG_UINT32 g_cust_boost_freq_id;
static IMG_UINT32 gpu_cust_boost_freq;
static IMG_UINT32 g_cust_upbound_freq_id;
static IMG_UINT32 gpu_cust_upbound_freq;

static IMG_UINT32 gpu_power;
static IMG_UINT32 gpu_dvfs_enable;
static IMG_UINT32 boost_gpu_enable;
static IMG_UINT32 gpu_debug_enable;
static IMG_UINT32 gpu_dvfs_force_idle;
static IMG_UINT32 gpu_dvfs_cb_force_idle;

static IMG_UINT32 gpu_pre_loading;
static IMG_UINT32 gpu_loading;
static IMG_UINT32 gpu_block;
static IMG_UINT32 gpu_idle;
static IMG_UINT32 gpu_freq;

static IMG_BOOL g_bDeviceInit = IMG_FALSE;

static IMG_BOOL g_bUnsync = IMG_FALSE;
static IMG_UINT32 g_ui32_unsync_freq_id;
static IMG_BOOL bCoreinitSucceeded = IMG_FALSE;


static struct platform_device *sPVRLDMDev;
static struct platform_device *sMFGASYNCDev;
static struct platform_device *sMFG2DDev;
#define GET_MTK_MFG_BASE(x) (struct mtk_mfg_base *)(x->dev.platform_data)

static const char * const top_mfg_clk_sel_name[] = {
	"mfg_slow_in_sel",
	"mfg_axi_in_sel",
	"mfg_mm_in_sel",
};

static const char * const top_mfg_clk_sel_parent_name_e1[] = {
	"slow_clk26m",
	"bus_univpll_d24",
	"engine_csw_mux",
};

static const char * const top_mfg_clk_sel_parent_name[] = {
	"slow_clk26m",
	"bus_mainpll_d11",
	"engine_csw_mux",
};

static const char * const top_mfg_clk_name[] = {
	"top_slow",
	"top_axi",
	"top_mm",
};

#define TOP_MFG_CLK_SLOW    0
#define TOP_MFG_CLK_AXI     1
#define TOP_MFG_CLK_MM      2
#define MAX_TOP_MFG_CLK ARRAY_SIZE(top_mfg_clk_name)

#define REG_MFG_AXI BIT(0)
#define REG_MFG_MEM BIT(1)
#define REG_MFG_G3D BIT(2)
#define REG_MFG_26M BIT(3)
#define REG_MFG_ALL (REG_MFG_AXI | REG_MFG_MEM | REG_MFG_G3D | REG_MFG_26M)

#define REG_MFG_CG_STA 0x00
#define REG_MFG_CG_SET 0x04
#define REG_MFG_CG_CLR 0x08

#ifdef CONFIG_MTK_SEGMENT_TEST
static IMG_UINT32 efuse_mfg_enable;
#endif

char rgx_fw_name[20] = "rgx.fw.signed";

#ifdef CONFIG_MTK_HIBERNATION
int gpu_pm_restore_noirq(struct device *device)
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
#endif

static PVRSRV_DEVICE_NODE *MTKGetRGXDevNode(IMG_VOID)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
	IMG_UINT32 i;

	for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
		PVRSRV_DEVICE_NODE *psDeviceNode = &psPVRSRVData->psDeviceNodeList[i];

		if (psDeviceNode && psDeviceNode->psDevConfig)
		/* && psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX) */
		{
			return psDeviceNode;
		}
	}
	return NULL;
}

static IMG_UINT32 MTKGetRGXDevIdx(IMG_VOID)
{
	static IMG_UINT32 ms_ui32RGXDevIdx = MTK_RGX_DEVICE_INDEX_INVALID;

	if (ms_ui32RGXDevIdx == MTK_RGX_DEVICE_INDEX_INVALID) {
		PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
		IMG_UINT32 i;

		for (i = 0; i < psPVRSRVData->ui32RegisteredDevices; i++) {
			PVRSRV_DEVICE_NODE *psDeviceNode = &psPVRSRVData->psDeviceNodeList[i];

			if (psDeviceNode && psDeviceNode->psDevConfig)
			/* && psDeviceNode->psDevConfig->eDeviceType == PVRSRV_DEVICE_TYPE_RGX) */
			{
				ms_ui32RGXDevIdx = i;
				break;
			}
		}
	}
	return ms_ui32RGXDevIdx;
}

static IMG_VOID MTKWriteBackFreqToRGX(PVRSRV_DEVICE_NODE *psDevNode, IMG_UINT32 ui32NewFreq)
{
	/* PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData(); */
	/* PVRSRV_DEVICE_NODE *psDeviceNode = &psPVRSRVData->psDeviceNodeList[ui32DeviceIndex];*/
	RGX_DATA *psRGXData = (RGX_DATA *)psDevNode->psDevConfig->hDevData;

	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32NewFreq * 1000;
	/* kHz to Hz write to RGX as the same unit */
}


#define MTKCLK_prepare_enable(clk)								\
	do {											\
		if (clk) {									\
			if (clk_prepare_enable(clk))						\
				pr_alert("MALI: clk_prepare_enable failed when enabling " #clk);\
		}										\
	} while (0)

#define MTKCLK_disable_unprepare(clk)			\
	do {						\
		if (clk)				\
			clk_disable_unprepare(clk);	\
	} while (0)



static void mtk_mfg_set_clock_gating(void __iomem *reg)
{
	writel(REG_MFG_ALL, reg + REG_MFG_CG_SET);
}

static void mtk_mfg_clr_clock_gating(void __iomem *reg)
{
	writel(REG_MFG_ALL, reg + REG_MFG_CG_CLR);
}

#if defined(MTK_USE_HW_APM)
static void mtk_mfg_enable_hw_apm(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	writel(0x01a80000, mfg_base->reg_base + 0x504);
	writel(0x00080010, mfg_base->reg_base + 0x508);
	writel(0x00080010, mfg_base->reg_base + 0x50c);
	writel(0x00b800b8, mfg_base->reg_base + 0x510);
	writel(0x00b000b0, mfg_base->reg_base + 0x514);
	writel(0x00c000c8, mfg_base->reg_base + 0x518);
	writel(0x00c000c8, mfg_base->reg_base + 0x51c);
	writel(0x00d000d8, mfg_base->reg_base + 0x520);
	writel(0x00d800d8, mfg_base->reg_base + 0x524);
	writel(0x00d800d8, mfg_base->reg_base + 0x528);
	writel(0x9000001b, mfg_base->reg_base + 0x24);
	writel(0x8000001b, mfg_base->reg_base + 0x24);
}
static void mtk_mfg_disable_hw_apm(void) {};
#else
static void mtk_mfg_enable_hw_apm(void) {};
static void mtk_mfg_disable_hw_apm(void) {};
#endif /* MTK_USE_HW_APM */


static IMG_VOID mtk_mfg_enable_clock(void)
{
	int i;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	/*
	** Hold wakelock when mfg power-on, prevent suspend when gpu active.
	** When enter system suspend flow, forbbiden power domain control.
	** If power domain can control, async/2d/mfg has sequence issue.
	*/
	pm_stay_awake(&mfg_base->mfg_async_pdev->dev);

	ged_dvfs_gpu_clock_switch_notify(1);

	/* Resume mfg power domain */
	pm_runtime_get_sync(&mfg_base->mfg_async_pdev->dev);
	pm_runtime_get_sync(&mfg_base->mfg_2d_pdev->dev);
	pm_runtime_get_sync(&mfg_base->pdev->dev);

	/* Prepare and enable mfg top clock */
	for (i = 0; i < MAX_TOP_MFG_CLK; i++)
		MTKCLK_prepare_enable(mfg_base->top_clk[i]);

	/* Enable(un-gated) mfg clock */
	mtk_mfg_clr_clock_gating(mfg_base->reg_base);

	/* E1: enlarge the sync fifo, to avoid that CPU accesses GPU register fail problem */
	if (mt_get_chip_sw_ver() == 0)
		writel((readl(mfg_base->reg_base + 0x1c) | 0xf0), mfg_base->reg_base + 0x1c);
}


static IMG_VOID mtk_mfg_disable_clock(IMG_BOOL bForce)
{
	int i;
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	/* Disable(gated) mfg clock */
	mtk_mfg_set_clock_gating(mfg_base->reg_base);

	/* Disable and unprepare mfg top clock */
	for (i = MAX_TOP_MFG_CLK - 1; i >= 0; i--)
		MTKCLK_disable_unprepare(mfg_base->top_clk[i]);

	/* Suspend mfg power domain */
	pm_runtime_put_sync(&mfg_base->pdev->dev);
	pm_runtime_put_sync(&mfg_base->mfg_2d_pdev->dev);
	pm_runtime_put_sync(&mfg_base->mfg_async_pdev->dev);

	ged_dvfs_gpu_clock_switch_notify(0);

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

	dev_err(&mfg_base->pdev->dev, "shutdown notified, code=%x\n", (unsigned int)code);

	mutex_lock(&mfg_base->set_power_state);

	/* the workaround code, because it seems that GPU have un-finished commands*/
	mtk_mfg_enable_clock();
	mtk_mfg_enable_hw_apm();

	mfg_base->shutdown = true;

	mutex_unlock(&mfg_base->set_power_state);

	return 0;
}


static void MTKEnableMfgClock(void)
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

static void MTKDisableMfgClock(void)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);
	if (mfg_base->shutdown) {
		mutex_unlock(&mfg_base->set_power_state);
		pr_info("skip MTKDisableClock\n");
		return;
	}
	mtk_mfg_disable_clock(IMG_TRUE);
	mutex_unlock(&mfg_base->set_power_state);
}


static IMG_BOOL MTKDoGpuDVFS(IMG_UINT32 ui32NewFreqID, IMG_BOOL bIdleDevice)
{
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32RGXDevIdx;

	/* bottom bound */
	if (ui32NewFreqID > g_bottom_freq_id)
		ui32NewFreqID = g_bottom_freq_id;
	if (ui32NewFreqID > g_cust_boost_freq_id)
		ui32NewFreqID = g_cust_boost_freq_id;

	/* up bound */
	if (ui32NewFreqID < g_cust_upbound_freq_id)
		ui32NewFreqID = g_cust_upbound_freq_id;

	/* thermal power limit */
	if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
		ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();

	/* no change */
	if (ui32NewFreqID == mt_gpufreq_get_cur_freq_index())
		return IMG_FALSE;

	ui32RGXDevIdx = MTKGetRGXDevIdx();
	if (ui32RGXDevIdx == MTK_RGX_DEVICE_INDEX_INVALID)
		return IMG_FALSE;

	eResult = PVRSRVDevicePreClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID *)NULL);
	if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
		unsigned int ui32GPUFreq;
		unsigned int ui32CurFreqID;
		PVRSRV_DEV_POWER_STATE ePowerState;

		PVRSRVGetDevicePowerState(ui32RGXDevIdx, &ePowerState);
		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
			MTKEnableMfgClock();

		mt_gpufreq_target(ui32NewFreqID);
		ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
		ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32CurFreqID);
		gpu_freq = ui32GPUFreq;
#if defined(CONFIG_TRACING) && defined(CONFIG_MTK_SCHED_TRACERS)
		if (PVRGpuTraceEnabled())
			trace_gpu_freq(ui32GPUFreq);
#endif
		MTKWriteBackFreqToRGX(ui32RGXDevIdx, ui32GPUFreq);

#ifdef MTK_DEBUG
		pr_info("PVR_K: 3DFreq=%d, Volt=%d\n", ui32GPUFreq, mt_gpufreq_get_cur_volt());
#endif

		if (ePowerState != PVRSRV_DEV_POWER_STATE_ON)
			MTKDisableMfgClock();

		if (eResult == PVRSRV_OK)
			PVRSRVDevicePostClockSpeedChange(ui32RGXDevIdx, bIdleDevice, (IMG_VOID *)NULL);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

#ifdef MTK_GPU_DVFS
/* For ged_dvfs idx commit */
static void MTKCommitFreqIdx(unsigned long ui32NewFreqID, GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited)
{
	PVRSRV_DEV_POWER_STATE ePowerState;
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	PVRSRV_ERROR eResult;

	if (psDevNode) {
		eResult = PVRSRVDevicePreClockSpeedChange(psDevNode, IMG_FALSE, (IMG_VOID *)NULL);
		if ((eResult == PVRSRV_OK) || (eResult == PVRSRV_ERROR_RETRY)) {
			unsigned int ui32GPUFreq;
			unsigned int ui32CurFreqID;
			PVRSRV_DEV_POWER_STATE ePowerState;

			/* debug patch for ALPS03164004 */
			if (eResult == PVRSRV_ERROR_RETRY)
				dump_stack();

			PVRSRVGetDevicePowerState(psDevNode, &ePowerState);

			if (ePowerState == PVRSRV_DEV_POWER_STATE_ON) {
				mt_gpufreq_target(ui32NewFreqID);
				g_bUnsync = IMG_FALSE;
			} else {
				g_ui32_unsync_freq_id = ui32NewFreqID;
				g_bUnsync = IMG_TRUE;
			}

			ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
			ui32GPUFreq = mt_gpufreq_get_frequency_by_level(ui32CurFreqID);
			gpu_freq = ui32GPUFreq;
	#if defined(CONFIG_TRACING) && defined(CONFIG_MTK_SCHED_TRACERS)
			if (PVRGpuTraceEnabled())
				trace_gpu_freq(ui32GPUFreq);
	#endif
			MTKWriteBackFreqToRGX(psDevNode, ui32GPUFreq);

	#ifdef MTK_DEBUG
			if (gpu_debug_enable)
				pr_info("PVR_K: 3DFreq=%d, Volt=%d\n", ui32GPUFreq, mt_gpufreq_get_cur_volt());
	#endif

			if (eResult == PVRSRV_OK)
				PVRSRVDevicePostClockSpeedChange(psDevNode, IMG_FALSE, (IMG_VOID *)NULL);

			/*
			 * Always return true because the APM would almost letting GPU power down
			 * with high possibility while DVFS committing
			 */
			if (pbCommited)
				*pbCommited = IMG_TRUE;
			return;
		}
	}

	if (pbCommited)
		*pbCommited = IMG_FALSE;
}

unsigned int MTKCommitFreqForPVR(unsigned long ui32NewFreq)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	unsigned int ui32NewFreqID = 0;
	unsigned long gpu_freq;
	int i;
	int promoted = 0;

	/* promotion bit at LSB  */
	promoted = ui32NewFreq&0x1;

	ui32NewFreq /= 1000; /* shrink to KHz*/
	ui32NewFreqID = i32MaxLevel;
	for (i = 0; i <= i32MaxLevel; i++) {
		gpu_freq = mt_gpufreq_get_freq_by_idx(i);

		if (ui32NewFreq > gpu_freq) {
			if (i == 0)
				ui32NewFreqID = 0;
			else
				ui32NewFreqID = i - 1;
			break;
		}
	}

	if (ui32NewFreqID != 0)
		ui32NewFreqID -= promoted;

	MTKCommitFreqIdx(ui32NewFreqID, 0x7788, NULL);

	gpu_freq = mt_gpufreq_get_freq_by_idx(ui32NewFreqID);
	return gpu_freq * 1000; /* scale up to Hz */
}

static void MTKFreqInputBoostCB(unsigned int ui32BoostFreqID)
{
#if 0
	if (g_iSkipCount > 0)
		return;

	if (boost_gpu_enable == 0)
		return;

	OSLockAcquire(ghDVFSLock);

	if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index()) {
		if (MTKDoGpuDVFS(ui32BoostFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
			g_sys_dvfs_time_ms = OSClockms();
	}

	OSLockRelease(ghDVFSLock);
#endif
}

static void MTKFreqPowerLimitCB(unsigned int ui32LimitFreqID)
{
	if (g_iSkipCount > 0)
		return;

	OSLockAcquire(ghDVFSLock);

	if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index()) {
		if (MTKDoGpuDVFS(ui32LimitFreqID, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE))
			g_sys_dvfs_time_ms = OSClockms();
	}

	OSLockRelease(ghDVFSLock);
}
#endif /* ifdef MTK_GPU_DVFS */

#ifdef MTK_CAL_POWER_INDEX
static IMG_VOID MTKStartPowerIndex(IMG_VOID)
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

static IMG_VOID MTKReStartPowerIndex(IMG_VOID)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x1);
}

static IMG_VOID MTKStopPowerIndex(IMG_VOID)
{
	if (g_pvRegsBaseKM)
		DRV_WriteReg32(g_pvRegsBaseKM + (uintptr_t)0x6320, 0x0);
}


static IMG_UINT32 MTKCalPowerIndex(IMG_VOID)
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

	/*writes 1 to GPIO_OUTPUT_ACK, bit[17]*/
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


static IMG_VOID MTKCalGpuLoading(unsigned int *pui32Loading, unsigned int *pui32Block, unsigned int *pui32Idle)
{

	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();

	if (!psDevNode) {
		PVR_DPF((PVR_DBG_ERROR, "psDevNode not found"));
		return;
	}
	PVRSRV_RGXDEV_INFO *psDevInfo = (psDevNode->pvDevice);

	if (psDevInfo && psDevInfo->pfnGetGpuUtilStats) {
		RGXFWIF_GPU_UTIL_STATS sGpuUtilStats = {0};

		/* 1.5DDK support multiple user to query GPU utilization, Need Init here*/
		if (g_RGXutilUser == NULL) {
			if (psDevInfo)
				RGXRegisterGpuUtilStats(&g_RGXutilUser);
		}

		if (g_RGXutilUser == NULL)
			return;

		psDevInfo->pfnGetGpuUtilStats(psDevInfo->psDeviceNode, g_RGXutilUser, &sGpuUtilStats);
		if (sGpuUtilStats.bValid) {
#if 0
			PVR_DPF((PVR_DBG_ERROR, "Loading: A(%d), I(%d), B(%d)",
				sGpuUtilStats.ui64GpuStatActiveHigh, sGpuUtilStats.ui64GpuStatIdle,
				sGpuUtilStats.ui64GpuStatBlocked));
#endif
#if defined(__arm64__) || defined(__aarch64__)
		*pui32Loading = (100*(sGpuUtilStats.ui64GpuStatActiveHigh +
				sGpuUtilStats.ui64GpuStatActiveLow)) /
				sGpuUtilStats.ui64GpuStatCumulative;
		*pui32Block = (100*(sGpuUtilStats.ui64GpuStatBlocked)) / sGpuUtilStats.ui64GpuStatCumulative;
		*pui32Idle = (100*(sGpuUtilStats.ui64GpuStatIdle)) / sGpuUtilStats.ui64GpuStatCumulative;
#else
		*pui32Loading = (unsigned long)(100*(sGpuUtilStats.ui64GpuStatActiveHigh +
				sGpuUtilStats.ui64GpuStatActiveLow)) /
				(unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
		*pui32Block =  (unsigned long)(100*(sGpuUtilStats.ui64GpuStatBlocked)) /
			       (unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
		*pui32Idle = (unsigned long)(100*(sGpuUtilStats.ui64GpuStatIdle)) /
			     (unsigned long)sGpuUtilStats.ui64GpuStatCumulative;
#endif
		}
	}
}

static IMG_BOOL MTKGpuDVFSPolicy(IMG_UINT32 ui32GPULoading, unsigned int *pui32NewFreqID)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();
	int i32NewFreqID = i32CurFreqID;

	if (ui32GPULoading >= 99)
		i32NewFreqID = 0;
	else if (ui32GPULoading <= 1)
		i32NewFreqID = i32MaxLevel;
	else if (ui32GPULoading >= 85)
		i32NewFreqID -= 2;
	else if (ui32GPULoading <= 30)
		i32NewFreqID += 2;
	else if (ui32GPULoading >= 70)
		i32NewFreqID -= 1;
	else if (ui32GPULoading <= 50)
		i32NewFreqID += 1;

	if (i32NewFreqID < i32CurFreqID) {
		if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
			i32NewFreqID -= 1;
	} else if (i32NewFreqID > i32CurFreqID) {
		if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
			i32NewFreqID += 1;
	}

	if (i32NewFreqID > i32MaxLevel)
		i32NewFreqID = i32MaxLevel;
	else if (i32NewFreqID < 0)
		i32NewFreqID = 0;

	if (i32NewFreqID != i32CurFreqID) {
		*pui32NewFreqID = (unsigned int)i32NewFreqID;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static IMG_VOID MTKDVFSTimerFuncCB(IMG_PVOID pvData)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	int i32CurFreqID = (int)mt_gpufreq_get_cur_freq_index();

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKDVFSTimerFuncCB"));

	if (gpu_dvfs_enable == 0) {
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		return;
	}

	if (g_iSkipCount > 0) {
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		g_iSkipCount -= 1;
	} else if ((!g_bExit) || (i32CurFreqID < i32MaxLevel)) {
		IMG_UINT32 ui32NewFreqID;

		/* calculate power index */
#ifdef MTK_CAL_POWER_INDEX
		gpu_power = MTKCalPowerIndex();
#else
		gpu_power = 0;
#endif

		MTKCalGpuLoading(&gpu_loading, &gpu_block, &gpu_idle);
		OSLockAcquire(ghDVFSLock);

		/* check system boost duration */
		if ((g_sys_dvfs_time_ms > 0) && (OSClockms() - g_sys_dvfs_time_ms < MTK_SYS_BOOST_DURATION_MS)) {
			OSLockRelease(ghDVFSLock);
			return;
		}
		g_sys_dvfs_time_ms = 0;

		/* do gpu dvfs */
		if (MTKGpuDVFSPolicy(gpu_loading, &ui32NewFreqID))
			MTKDoGpuDVFS(ui32NewFreqID, gpu_dvfs_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

		gpu_pre_loading = gpu_loading;
		OSLockRelease(ghDVFSLock);
	}
}

void MTKMFGEnableDVFSTimer(bool bEnable)
{
	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: %s", bEnable ? "yes" : "no"));

	if (g_hDVFSTimer == NULL) {
		PVR_DPF((PVR_DBG_ERROR, "MTKMFGEnableDVFSTimer: g_hDVFSTimer is NULL"));
		return;
	}

	OSLockAcquire(ghDVFSTimerLock);

	if (bEnable) {
		if (!g_bTimerEnable) {
			if (OSEnableTimer(g_hDVFSTimer) == PVRSRV_OK)
				g_bTimerEnable = IMG_TRUE;
		}
	} else {
		if (g_bTimerEnable) {
			if (OSDisableTimer(g_hDVFSTimer) == PVRSRV_OK)
				g_bTimerEnable = IMG_FALSE;
		}
	}

	OSLockRelease(ghDVFSTimerLock);
}

static bool MTKCheckDeviceInit(void)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	bool ret = false;

	if (psDevNode) {
		if (psDevNode->eDevState == PVRSRV_DEVICE_STATE_ACTIVE)
			ret = true;
	}
	return ret;
}

void MTKSetICVerion(void)
{
	if (mt_get_chip_sw_ver() == 0) /* E1 */
		strcat(rgx_fw_name, ".e1");
}

PVRSRV_ERROR MTKDevPrePowerState(IMG_HANDLE hSysData, PVRSRV_DEV_POWER_STATE eNewPowerState,
				 PVRSRV_DEV_POWER_STATE eCurrentPowerState,
				 IMG_BOOL bForced)
{
	struct mtk_mfg_base *mfg_base = GET_MTK_MFG_BASE(sPVRLDMDev);

	mutex_lock(&mfg_base->set_power_state);
	if ((eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF) &&
	    (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON)) {
#ifndef ENABLE_COMMON_DVFS
		if (g_hDVFSTimer && g_bDeviceInit) {
#else
		if (g_bDeviceInit) {
#endif
			g_bExit = IMG_TRUE;
#ifdef MTK_CAL_POWER_INDEX
			MTKStopPowerIndex();
#endif
		} else
			g_bDeviceInit = MTKCheckDeviceInit();

		mtk_mfg_disable_hw_apm();
		mtk_mfg_disable_clock(IMG_FALSE);
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
	if ((eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF) &&
	    (eNewPowerState == PVRSRV_DEV_POWER_STATE_ON)) {
		mtk_mfg_enable_clock();
		mtk_mfg_enable_hw_apm();

#ifndef ENABLE_COMMON_DVFS
		if (g_hDVFSTimer && g_bDeviceInit) {
#else
		if (g_bDeviceInit) {
#endif
#ifdef MTK_CAL_POWER_INDEX
			MTKStartPowerIndex();
#endif
			g_bExit = IMG_FALSE;
		} else
			g_bDeviceInit = MTKCheckDeviceInit();

		if (g_bUnsync == IMG_TRUE) {
#ifdef MTK_GPU_DVFS
			mt_gpufreq_target(g_ui32_unsync_freq_id);
#endif
			g_bUnsync = IMG_FALSE;
		}
	}

	mutex_unlock(&mfg_base->set_power_state);
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	/* if (PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState) */
	return PVRSRV_OK;
}

PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
	/* if (PVRSRV_SYS_POWER_STATE_ON == eNewPowerState) */
	return PVRSRV_OK;
}

#ifdef MTK_GPU_DVFS
static void MTKBoostGpuFreq(void)
{
	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKBoostGpuFreq"));
	MTKFreqInputBoostCB(0);
}

static void MTKSetBottomGPUFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKSetBottomGPUFreq: freq = %d", ui32FreqLevel));

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	OSLockAcquire(ghDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	if (g_bottom_freq_id < mt_gpufreq_get_cur_freq_index())
		MTKDoGpuDVFS(g_bottom_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

	OSLockRelease(ghDVFSLock);
}

static unsigned int MTKCustomGetGpuFreqLevelCount(void)
{
	return mt_gpufreq_get_dvfs_table_num();
}

static void MTKCustomBoostGpuFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKCustomBoostGpuFreq: freq = %d", ui32FreqLevel));

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	OSLockAcquire(ghDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_cust_boost_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

	if (g_cust_boost_freq_id < mt_gpufreq_get_cur_freq_index())
		MTKDoGpuDVFS(g_cust_boost_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

	OSLockRelease(ghDVFSLock);
}

static void MTKCustomUpBoundGpuFreq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		PVR_DPF((PVR_DBG_ERROR, "MTKCustomUpBoundGpuFreq: freq = %d", ui32FreqLevel));

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	OSLockAcquire(ghDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_cust_upbound_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

	if (g_cust_upbound_freq_id > mt_gpufreq_get_cur_freq_index())
		MTKDoGpuDVFS(g_cust_upbound_freq_id, gpu_dvfs_cb_force_idle == 0 ? IMG_FALSE : IMG_TRUE);

	OSLockRelease(ghDVFSLock);
}

unsigned int MTKGetCustomBoostGpuFreq(void)
{
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	return ui32MaxLevel - g_cust_boost_freq_id;
}

unsigned int MTKGetCustomUpBoundGpuFreq(void)
{
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	return ui32MaxLevel - g_cust_upbound_freq_id;
}

static IMG_UINT32 MTKGetGpuLoading(IMG_VOID)
{
#ifdef ENABLE_COMMON_DVFS
	MTKCalGpuLoading(&gpu_loading, &gpu_block, &gpu_idle);
#endif

	return gpu_loading;
}

static IMG_UINT32 MTKGetGpuBlock(IMG_VOID)
{
	return gpu_block;
}

static IMG_UINT32 MTKGetGpuIdle(IMG_VOID)
{
	return gpu_idle;
}

static IMG_UINT32 MTKGetPowerIndex(IMG_VOID)
{
	return gpu_power;
}
#endif /* #ifdef MTK_GPU_DVFS */

typedef void (*gpufreq_input_boost_notify)(unsigned int);
typedef void (*gpufreq_power_limit_notify)(unsigned int);


#ifdef SUPPORT_PDVFS
#include "rgxpdvfs.h"
#define  mt_gpufreq_get_freq_by_idx mt_gpufreq_get_frequency_by_level
static IMG_OPP *gpasOPPTable;

static int MTKMFGOppUpdate(int ui32ThrottlePoint)
{
	PVRSRV_DEVICE_NODE *psDevNode = MTKGetRGXDevNode();
	int i, ui32OPPTableSize;

	static RGXFWIF_PDVFS_OPP sPDFVSOppInfo;
	static int bNotReady = 1;

	if (bNotReady) {
		ui32OPPTableSize = mt_gpufreq_get_dvfs_table_num();
		gpasOPPTable = (IMG_OPP  *)OSAllocZMem(sizeof(IMG_OPP) * ui32OPPTableSize);

		for (i = 0; i < ui32OPPTableSize; i++) {
			gpasOPPTable[i].ui32Volt = mt_gpufreq_get_volt_by_idx(i);
			gpasOPPTable[i].ui32Freq = mt_gpufreq_get_freq_by_idx(i) * 1000;
		}

		if (psDevNode) {
			PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
			PVRSRV_DEVICE_CONFIG *psDevConfig = psDevNode->psDevConfig;

			psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable = gpasOPPTable;
			psDevConfig->sDVFS.sDVFSDeviceCfg.ui32OPPTableSize = ui32OPPTableSize;

			/* sPDFVSOppInfo.ui32ThrottlePoint = ui32ThrottlePoint; */
			/* PDVFSSendOPPPoints(psDevInfo, sPDFVSOppInfo); */
			bNotReady = 0;

			PVR_DPF((PVR_DBG_ERROR, "PDVFS opptab=%p size=%d init completed",
				 psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable, ui32OPPTableSize));
		} else {
			if (gpasOPPTable)
				OSFreeMem(gpasOPPTable);
		}
	}

}

static IMG_VOID MTKFakeGpuLoading(unsigned int *pui32Loading, unsigned int *pui32Block, unsigned int *pui32Idle)
{
	*pui32Loading = 0;
	*pui32Block = 0;
	*pui32Idle = 0;
}
#endif

PVRSRV_ERROR MTKMFGSystemInit(void)
{
#ifndef MTK_GPU_DVFS
	gpu_dvfs_enable = 0;
#else
	int i;
	PVRSRV_ERROR error;

	gpu_dvfs_enable = 1;
#ifndef ENABLE_COMMON_DVFS
	error = OSLockCreate(&ghDVFSLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Lock Failed"));
		goto ERROR;
	}

	error = OSLockCreate(&ghDVFSTimerLock, LOCK_TYPE_PASSIVE);
	if (error != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Lock Failed"));
		goto ERROR;
	}

	g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

	g_hDVFSTimer = OSAddTimer(MTKDVFSTimerFuncCB, (IMG_VOID *)NULL, MTK_DVFS_SWITCH_INTERVAL_MS);
	if (!g_hDVFSTimer) {
		PVR_DPF((PVR_DBG_ERROR, "Create DVFS Timer Failed"));
		goto ERROR;
	}

	if (OSEnableTimer(g_hDVFSTimer) == PVRSRV_OK)
		g_bTimerEnable = IMG_TRUE;
	boost_gpu_enable = 1;

	g_sys_dvfs_time_ms = 0;

	g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_bottom_freq = mt_gpufreq_get_frequency_by_level(g_bottom_freq_id);

	g_cust_boost_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_cust_boost_freq = mt_gpufreq_get_frequency_by_level(g_cust_boost_freq_id);

	g_cust_upbound_freq_id = 0;
	gpu_cust_upbound_freq = mt_gpufreq_get_frequency_by_level(g_cust_upbound_freq_id);

	gpu_debug_enable = 0;

	mt_gpufreq_input_boost_notify_registerCB(MTKFreqInputBoostCB);
	mt_gpufreq_power_limit_notify_registerCB(MTKFreqPowerLimitCB);

	mtk_boost_gpu_freq_fp = MTKBoostGpuFreq;

	mtk_set_bottom_gpu_freq_fp = MTKSetBottomGPUFreq;

	mtk_custom_get_gpu_freq_level_count_fp = MTKCustomGetGpuFreqLevelCount;

	mtk_custom_boost_gpu_freq_fp = MTKCustomBoostGpuFreq;

	mtk_custom_upbound_gpu_freq_fp = MTKCustomUpBoundGpuFreq;

	mtk_get_custom_boost_gpu_freq_fp = MTKGetCustomBoostGpuFreq;

	mtk_get_custom_upbound_gpu_freq_fp = MTKGetCustomUpBoundGpuFreq;

	mtk_get_gpu_power_loading_fp = MTKGetPowerIndex;

	mtk_get_gpu_loading_fp = MTKGetGpuLoading;
	mtk_get_gpu_block_fp = MTKGetGpuBlock;
	mtk_get_gpu_idle_fp = MTKGetGpuIdle;
#else
#ifdef SUPPORT_PDVFS
	ged_dvfs_vsync_trigger_fp = MTKMFGOppUpdate;
	ged_dvfs_cal_gpu_utilization_fp = MTKFakeGpuLoading; /* turn-off GED loading based DVFS */
	/* mt_gpufreq_power_limit_notify_registerCB(); */
#else
	ged_dvfs_cal_gpu_utilization_fp = MTKCalGpuLoading;
	ged_dvfs_gpu_freq_commit_fp = MTKCommitFreqIdx;
#endif
#endif

	/* Set the CB for ptpod use */
	mt_gpufreq_mfgclock_notify_registerCB(MTKEnableMfgClock, MTKDisableMfgClock);
#endif /* ifdef MTK_GPU_DVFS */

#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_GPU, gpu_pm_restore_noirq, NULL);
#endif

	return PVRSRV_OK;

#ifndef ENABLE_COMMON_DVFS
ERROR:
	MTKMFGSystemDeInit();
	return PVRSRV_ERROR_INIT_FAILURE;
#endif
}

IMG_VOID MTKMFGSystemDeInit(void)
{

#ifdef SUPPORT_PDVFS
	if (gpasOPPTable)
		OSFreeMem(gpasOPPTable);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_GPU);
#endif

	g_bExit = IMG_TRUE;

#ifdef MTK_GPU_DVFS
#ifndef ENABLE_COMMON_DVFS
	if (g_hDVFSTimer) {
		OSDisableTimer(g_hDVFSTimer);
		OSRemoveTimer(g_hDVFSTimer);
		g_hDVFSTimer = IMG_NULL;
	}

	if (ghDVFSLock) {
		OSLockDestroy(ghDVFSLock);
		ghDVFSLock = NULL;
	}

	if (ghDVFSTimerLock) {
		OSLockDestroy(ghDVFSTimerLock);
		ghDVFSTimerLock = NULL;
	}
#endif
#endif /* MTK_GPU_DVFS */

#ifdef MTK_CAL_POWER_INDEX
	g_pvRegsBaseKM = NULL;
#endif
}

static void mfg_clk_set_parent(struct mtk_mfg_base *mfg_base)
{
	/* mfg_slow_in_sel/mfg_axi_in_sel: non-glitch-free mux, should disable mux before set parent.
	 * mfg_mm_in_sel: glitch-free mux, should enable mux before set parent.
	 */
	clk_set_parent(mfg_base->top_clk_sel[TOP_MFG_CLK_SLOW], mfg_base->top_clk_sel_parent[TOP_MFG_CLK_SLOW]);
	clk_set_parent(mfg_base->top_clk_sel[TOP_MFG_CLK_AXI], mfg_base->top_clk_sel_parent[TOP_MFG_CLK_AXI]);

	clk_prepare_enable(mfg_base->top_clk_sel[TOP_MFG_CLK_MM]);
	clk_set_parent(mfg_base->top_clk_sel[TOP_MFG_CLK_MM], mfg_base->top_clk_sel_parent[TOP_MFG_CLK_MM]);
	clk_disable_unprepare(mfg_base->top_clk_sel[TOP_MFG_CLK_MM]);
}

static int mtk_mfg_bind_device_resource(struct platform_device *pdev,
				 struct mtk_mfg_base *mfg_base)
{
	int i, err;
	int len_clk = sizeof(struct clk *) * MAX_TOP_MFG_CLK;

	if (!sMFGASYNCDev || !sMFG2DDev) {
		dev_err(&pdev->dev, "Failed to get pm_domain\n");
		return -EPROBE_DEFER;
	}

	mfg_base->top_clk_sel = devm_kzalloc(&pdev->dev, len_clk, GFP_KERNEL);
	if (!mfg_base->top_clk_sel)
		return -ENOMEM;

	mfg_base->top_clk_sel_parent = devm_kzalloc(&pdev->dev, len_clk, GFP_KERNEL);
	if (!mfg_base->top_clk_sel_parent)
		return -ENOMEM;

	mfg_base->top_clk = devm_kzalloc(&pdev->dev, len_clk, GFP_KERNEL);
	if (!mfg_base->top_clk)
		return -ENOMEM;

	mfg_base->reg_base = of_iomap(pdev->dev.of_node, 1);
	if (!mfg_base->reg_base) {
		pr_err("Unable to ioremap registers pdev %p\n", pdev);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		if (mt_get_chip_sw_ver() == 0) { /* E1 */
			mfg_base->top_clk_sel_parent[i] = devm_clk_get(&pdev->dev,
						top_mfg_clk_sel_parent_name_e1[i]);
			if (IS_ERR(mfg_base->top_clk_sel_parent[i])) {
				err = PTR_ERR(mfg_base->top_clk_sel_parent[i]);
				dev_err(&pdev->dev, "devm_clk_get %s failed !!!\n",
					top_mfg_clk_sel_parent_name_e1[i]);
				goto err_iounmap_reg_base;
			}
		} else {
			mfg_base->top_clk_sel_parent[i] = devm_clk_get(&pdev->dev,
						top_mfg_clk_sel_parent_name[i]);
			if (IS_ERR(mfg_base->top_clk_sel_parent[i])) {
				err = PTR_ERR(mfg_base->top_clk_sel_parent[i]);
				dev_err(&pdev->dev, "devm_clk_get %s failed !!!\n",
					top_mfg_clk_sel_parent_name[i]);
				goto err_iounmap_reg_base;
			}
		}
	}

	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		mfg_base->top_clk_sel[i] = devm_clk_get(&pdev->dev,
						    top_mfg_clk_sel_name[i]);
		if (IS_ERR(mfg_base->top_clk_sel[i])) {
			err = PTR_ERR(mfg_base->top_clk_sel[i]);
			dev_err(&pdev->dev, "devm_clk_get %s failed !!!\n",
				top_mfg_clk_sel_name[i]);
			goto err_iounmap_reg_base;
		}
	}

	for (i = 0; i < MAX_TOP_MFG_CLK; i++) {
		mfg_base->top_clk[i] = devm_clk_get(&pdev->dev,
						    top_mfg_clk_name[i]);
		if (IS_ERR(mfg_base->top_clk[i])) {
			err = PTR_ERR(mfg_base->top_clk[i]);
			dev_err(&pdev->dev, "devm_clk_get %s failed !!!\n",
				top_mfg_clk_name[i]);
			goto err_iounmap_reg_base;
		}
	}

	mfg_clk_set_parent(mfg_base);

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


int MTKRGXDeviceInit(void *pvOSDevice)
{
	struct platform_device *pdev;
	struct mtk_mfg_base *mfg_base;
	int err;

	pdev = to_platform_device((struct device *)pvOSDevice);

	sPVRLDMDev = pdev;
	mfg_base = devm_kzalloc(&pdev->dev, sizeof(*mfg_base), GFP_KERNEL);
	if (!mfg_base)
		return -ENOMEM;

	err = mtk_mfg_bind_device_resource(pdev, mfg_base);
	if (err != 0)
		return err;

	mutex_init(&mfg_base->set_power_state);
	pdev->dev.platform_data = mfg_base;

	bCoreinitSucceeded = IMG_TRUE;
	return 0;
}

void MTKDisablePowerDomain(void)
{
	if (sMFG2DDev)
		pm_runtime_put_sync(&sMFG2DDev->dev);

	if (sMFGASYNCDev)
		pm_runtime_put_sync(&sMFGASYNCDev->dev);
}

bool mt_gpucore_ready(void)
{
	return (bCoreinitSucceeded == IMG_TRUE);
}
EXPORT_SYMBOL(mt_gpucore_ready);


#ifndef ENABLE_COMMON_DVFS
module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(boost_gpu_enable, uint, 0644);
module_param(gpu_dvfs_force_idle, uint, 0644);
module_param(gpu_dvfs_cb_force_idle, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_cust_boost_freq, uint, 0644);
module_param(gpu_cust_upbound_freq, uint, 0644);
module_param(gpu_freq, uint, 0644);
#endif

module_param(gpu_power, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);


#ifdef CONFIG_MTK_SEGMENT_TEST
module_param(efuse_mfg_enable, uint, 0644);
#endif


static int mtk_mfg_async_probe(struct platform_device *pdev)
{
#ifdef MTK_DEBUG
	pr_info("mtk_mfg_async_probe\n");
#endif

	if (!pdev->dev.pm_domain) {
		dev_err(&pdev->dev, "Failed to get dev->pm_domain\n");
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
	{ .compatible = "mediatek,mt8167-mfg-async",},
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

static int __init mtk_mfg_async_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_async_driver);
	if (ret != 0) {
		pr_err("Failed to register mfg async driver\n");
		return ret;
	}

	if (sMFGASYNCDev)
		pm_runtime_get_sync(&sMFGASYNCDev->dev);
	else
		pr_err("mtk_mfg_async_init enable async power domain failed\n");

	return ret;
}
subsys_initcall(mtk_mfg_async_init);

static int mtk_mfg_2d_probe(struct platform_device *pdev)
{
#ifdef MTK_DEBUG
	pr_info("mtk_mfg_2d_probe\n");
#endif

	if (!pdev->dev.pm_domain) {
		dev_err(&pdev->dev, "Failed to get dev->pm_domain\n");
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
	{ .compatible = "mediatek,mt8167-mfg-2d",},
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

static int __init mtk_mfg_2d_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_2d_driver);
	if (ret != 0) {
		pr_err("Failed to register mfg async driver\n");
		return ret;
	}

	if (sMFG2DDev)
		pm_runtime_get_sync(&sMFG2DDev->dev);
	else
		pr_err("mtk_mfg_2d_init enable power domain failed\n");

	return ret;
}
subsys_initcall(mtk_mfg_2d_init);

