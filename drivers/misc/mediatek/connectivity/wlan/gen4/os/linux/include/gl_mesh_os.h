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
/*! \file   gl_mesh_os.h
*    \brief  List the external reference to OS for mesh GLUE Layer.
*
*    In this file we define the data structure - GLUE_INFO_T to store those objects
*    we acquired from OS - e.g. TIMER, SPINLOCK, NET DEVICE ... . And all the
*    external reference (header file, extern func() ..) to OS for GLUE Layer should
*    also list down here.
*/

#ifndef _GL_MESH_OS_H
#define _GL_MESH_OS_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   V A R I A B L E
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
#if CFG_ENABLE_WIFI_MESH
struct _GL_MESH_INFO_T {
	struct ieee80211_hw *hw;
	struct cfg80211_scan_request *prScanRequest;
	PARAM_SCAN_REQUEST_EXT_T rScanRequest;
	struct delayed_work hw_scan;
	unsigned int rx_filter;
	struct ieee80211_vif *vif;
	UINT_8 ucBssIndex;
	UINT_32 u4MeshHtMcsMask[2]; /* used to add StaRec */
	UINT_32 u4MeshLegacyRateSet[2]; /* used to add StaRec */
};

struct mtk_vif_priv {
	u32 magic;
	u8 bssid[ETH_ALEN];
	bool assoc;
	u16 aid;
};

struct mtk_sta_priv {
	u32 magic;
};

typedef struct _MESH_WORK_DATA_STRUCT {
	struct net_device *netdev;
	PARAM_CUSTOM_MESH_SET_STRUCT rSetMESH;
} MESH_WORK_DATA_STRUCT, *P_MESH_WORK_DATA_STRUCT;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
BOOLEAN meshLaunch(P_GLUE_INFO_T prGlueInfo);

BOOLEAN meshRemove(P_GLUE_INFO_T prGlueInfo);

INT_8 glRegisterMESH(P_GLUE_INFO_T prGlueInfo);

BOOLEAN glUnregisterMESH(P_GLUE_INFO_T prGlueInfo);

int meshWorkSchedule(struct net_device *netdev, PARAM_CUSTOM_MESH_SET_STRUCT rSetMESH);

VOID getMeshChan(struct cfg80211_chan_def *chandef, UINT_8 *channum
	, P_ENUM_BAND_T band, ENUM_CHNL_EXT_T *prChnlSco);

BOOLEAN secEnabledInMesh(IN P_ADAPTER_T prAdapter);
#endif
#endif
