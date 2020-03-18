/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*! \file   gl_mesh_init.c
* brief  init and exit routines of Linux driver interface for Wi-Fi Mesh
*
* This file contains the main routines of Linux driver for MediaTek Inc. 802.11
* Wireless LAN Adapters.
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "gl_os.h"
#include "precomp.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#if CFG_ENABLE_WIFI_MESH
static MESH_WORK_DATA_STRUCT rMeshWorkData;
static struct work_struct rMeshWork;

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
BOOLEAN meshLaunch(P_GLUE_INFO_T prGlueInfo)
{
	BOOLEAN ret = FALSE;

	DBGLOG(MESH, INFO, "Mesh Launch...\n");
	if (prGlueInfo->prAdapter->fgIsMESHRegistered == TRUE) {
		DBGLOG(MESH, ERROR, "Mesh Already Register...\n");
		ret = FALSE;
	} else if (!glRegisterMESH(prGlueInfo)) {
		rtnl_lock();
		prGlueInfo->prAdapter->fgIsMESHRegistered = TRUE;
		rtnl_unlock();

		DBGLOG(MESH, INFO, "Mesh Launch Success...\n");
		ret = TRUE;
	} else {
		DBGLOG(MESH, ERROR, "Mesh Launch Fail...\n");
		ret = FALSE;
	}

	return ret;
}

BOOLEAN meshRemove(P_GLUE_INFO_T prGlueInfo)
{
	glUnregisterMESH(prGlueInfo);
	DBGLOG(MESH, INFO, "Mesh Removed...\n");
	return TRUE;
}

static int meshModeHandler(struct net_device *netdev, PARAM_CUSTOM_MESH_SET_STRUCT rSetMESH)
{
	P_GLUE_INFO_T prGlueInfo  = *((P_GLUE_INFO_T *)netdev_priv(netdev));

	if (prGlueInfo == NULL) {
		DBGLOG(MESH, ERROR, "Glueinfo is NULL, return\n");
		return 0;
	}

	DBGLOG(MESH, INFO, "PRIV_CMD_MESH_MODE=%u\n", (UINT32)rSetMESH.u4Enable);

	if (!rSetMESH.u4Enable) {
		DBGLOG(MESH, ERROR, "meshRemove from wq\n");
		meshRemove(prGlueInfo);
	} else {
		if (prGlueInfo->prDevHandler && (prGlueInfo->prDevHandler->flags & IFF_UP)) {
			DBGLOG(MESH, ERROR, "Error:Station interace is UP , Can't Launch Mesh\n");
			return 0;
		}
		meshLaunch(prGlueInfo);
	}

	return 0;
}

static void meshHandleWq(struct work_struct *work)
{
#if CFG_ENABLE_WIFI_MESH
	P_MESH_WORK_DATA_STRUCT prMeshWorkData = &rMeshWorkData;

	meshModeHandler(prMeshWorkData->netdev, prMeshWorkData->rSetMESH);
#endif
}

int meshWorkSchedule(struct net_device *netdev, PARAM_CUSTOM_MESH_SET_STRUCT rSetMESH)
{
	P_GLUE_INFO_T prGlueInfo  = *((P_GLUE_INFO_T *)netdev_priv(netdev));

	if (prGlueInfo == NULL) {
		DBGLOG(MESH, ERROR, "Mesh No GlueInfo\n");
		return 0;
	}

	rMeshWorkData.netdev = netdev;
	rMeshWorkData.rSetMESH = rSetMESH;
	INIT_WORK(&rMeshWork, meshHandleWq);

	if (!rSetMESH.u4Enable && prGlueInfo->prAdapter->fgIsMESHRegistered) {
		prGlueInfo->prAdapter->fgIsMESHRegistered = FALSE;
		schedule_work(&rMeshWork);
	} else if (rSetMESH.u4Enable && !prGlueInfo->prAdapter->fgIsMESHRegistered)
		schedule_work(&rMeshWork);

	return 0;
}
#endif

