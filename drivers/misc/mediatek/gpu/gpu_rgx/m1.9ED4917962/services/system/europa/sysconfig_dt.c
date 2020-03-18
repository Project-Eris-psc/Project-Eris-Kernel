/*************************************************************************/ /*!
@File
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#include <asm/io.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>

#include "interrupt_support.h"
#include "pvrsrv_device.h"
#include "syscommon.h"
#include "sysconfig.h"
#include "physheap.h"


/*
	CPU to Device physical address translation
*/
static
void UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
									   IMG_UINT32 ui32NumOfAddr,
									   IMG_DEV_PHYADDR *psDevPAddr,
									   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);
	
	/* Optimise common case */
	psDevPAddr[0].uiAddr = psCpuPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
		}
	}
}

/*
	Device to CPU physical address translation
*/
static
void UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
									   IMG_UINT32 ui32NumOfAddr,
									   IMG_CPU_PHYADDR *psCpuPAddr,
									   IMG_DEV_PHYADDR *psDevPAddr)				  
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);
	
	/* Optimise common case */
	psCpuPAddr[0].uiAddr = psDevPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
		}
	}
}

static PHYS_HEAP_FUNCTIONS gsPhysHeapFuncs =
{
	UMAPhysHeapCpuPAddrToDevPAddr,
	UMAPhysHeapDevPAddrToCpuPAddr,
	NULL,
};

static PVRSRV_ERROR PhysHeapsCreate(PHYS_HEAP_CONFIG **ppasPhysHeapsOut,
									IMG_UINT32 *puiPhysHeapCountOut)
{
	PHYS_HEAP_CONFIG *pasPhysHeaps;

	pasPhysHeaps = OSAllocZMem(sizeof(*pasPhysHeaps));
	if (!pasPhysHeaps)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	pasPhysHeaps[0].ui32PhysHeapID = 0;
	pasPhysHeaps[0].pszPDumpMemspaceName = "SYSMEM";
	pasPhysHeaps[0].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[0].psMemFuncs = &gsPhysHeapFuncs;

	*ppasPhysHeapsOut = pasPhysHeaps;
	*puiPhysHeapCountOut = 1;

	return PVRSRV_OK;
}

static void PhysHeapsDestroy(PHYS_HEAP_CONFIG *pasPhysHeaps)
{
	OSFreeMem(pasPhysHeaps);
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	struct device *psDev = pvOSDevice;
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	RGX_DATA *psRGXData;
	RGX_TIMING_INFORMATION *psRGXTimingInfo;
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	IMG_UINT32 uiPhysHeapCount;
	struct resource sResource;
	PVRSRV_ERROR eError;
	int iErr;

	PVR_ASSERT(psDev);

	psDevConfig = OSAllocZMem(sizeof(*psDevConfig) +
							  sizeof(*psRGXData) +
							  sizeof(*psRGXTimingInfo));
	if (!psDevConfig)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	dma_set_mask(pvOSDevice, DMA_BIT_MASK(40));

	psRGXData = (RGX_DATA *)((IMG_CHAR *)psDevConfig + sizeof(*psDevConfig));
	psRGXTimingInfo = (RGX_TIMING_INFORMATION *)((IMG_CHAR *)psRGXData + sizeof(*psRGXData));

	eError = PhysHeapsCreate(&pasPhysHeaps, &uiPhysHeapCount);
	if (eError)
	{
		goto ErrorFreeDevConfig;
	}

	psDevConfig->pasPhysHeaps      = pasPhysHeaps;
	psDevConfig->ui32PhysHeapCount = uiPhysHeapCount;

	/*
	 * Setup RGX specific timing data
	 */
	psRGXTimingInfo->ui32CoreClockSpeed        = RGX_NOHW_CORE_CLOCK_SPEED;
	psRGXTimingInfo->bEnableActivePM           = IMG_FALSE;
	psRGXTimingInfo->bEnableRDPowIsland        = IMG_FALSE;
	psRGXTimingInfo->ui32ActivePMLatencyms     = SYS_RGX_ACTIVE_POWER_LATENCY_MS;

	/*
	 *Setup RGX specific data
	 */
	psRGXData->psRGXTimingInfo = psRGXTimingInfo;

	/* Setup the device config */
	psDevConfig->pvOSDevice				= pvOSDevice;
	psDevConfig->pszName                = "RGX:europa";
	psDevConfig->pszVersion             = NULL;

	psDevConfig->eBIFTilingMode = geBIFTilingMode;;
	psDevConfig->pui32BIFTilingHeapConfigs = gauiBIFTilingHeapXStrides;
	psDevConfig->ui32BIFTilingHeapCount = IMG_ARR_NUM_ELEMS(gauiBIFTilingHeapXStrides);

	iErr = of_address_to_resource(psDev->of_node, 0, &sResource);
	if (iErr)
	{
		eError = PVRSRV_ERROR_UNABLE_TO_FIND_RESOURCE;
		goto ErrorFreePhysHeaps;
	}

	psDevConfig->sRegsCpuPBase.uiAddr   = sResource.start;
	psDevConfig->ui32RegsSize           = sResource.end - sResource.start + 1;

	psDevConfig->ui32IRQ                = irq_of_parse_and_map(psDev->of_node, 0);
	if (psDevConfig->ui32IRQ == 0)
	{
		eError = PVRSRV_ERROR_UNABLE_TO_FIND_RESOURCE;
		goto ErrorFreePhysHeaps;
	}

	psDevConfig->eCacheSnoopingMode     = PVRSRV_DEVICE_SNOOP_NONE;

	psDevConfig->pfnPrePowerState       = NULL;
	psDevConfig->pfnPostPowerState      = NULL;
	psDevConfig->pfnClockFreqGet        = NULL;
	psDevConfig->pfnSysDriverMode		= NULL;

	psDevConfig->hDevData               = psRGXData;

	*ppsDevConfig = psDevConfig;

	return PVRSRV_OK;

ErrorFreePhysHeaps:
	PhysHeapsDestroy(pasPhysHeaps);
ErrorFreeDevConfig:
	OSFreeMem(psDevConfig);
	return eError;
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PhysHeapsDestroy(psDevConfig->pasPhysHeaps);
	OSFreeMem(psDevConfig);
}

PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
								  IMG_UINT32 ui32IRQ,
								  const IMG_CHAR *pszName,
								  PFN_LISR pfnLISR,
								  void *pvData,
								  IMG_HANDLE *phLISRData)
{
	PVR_UNREFERENCED_PARAMETER(hSysData);

	return OSInstallSystemLISR(phLISRData, ui32IRQ, pszName, pfnLISR, pvData,
							   SYS_IRQ_FLAG_TRIGGER_DEFAULT);
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	return OSUninstallSystemLISR(hLISRData);
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
	return PVRSRV_OK;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
