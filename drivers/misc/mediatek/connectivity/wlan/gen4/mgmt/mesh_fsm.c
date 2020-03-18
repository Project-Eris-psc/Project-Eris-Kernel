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

/*! \file   "mesh_fsm.c"
 *  \brief  This file defines the FSM for Mesh Module.
 *
 *  This file defines the FSM for Mesh Module.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ********************************************************************************
 */
#include "precomp.h"

#if CFG_ENABLE_WIFI_MESH

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
VOID meshFsmStateAbort_SCAN(P_ADAPTER_T prAdapter)
{
	P_MSG_SCN_SCAN_CANCEL prScanCancelMsg;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prGlMeshInfo = NULL;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prGlMeshInfo = prGlueInfo->prMeshInfo;
	/* Abort JOIN process. */
	prScanCancelMsg = (P_MSG_SCN_SCAN_CANCEL)cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_CANCEL));
	if (!prScanCancelMsg) {
		/* Can't abort SCN FSM */
		ASSERT(0);
		return;
	}

	prScanCancelMsg->rMsgHdr.eMsgId = MID_MESH_SCN_SCAN_CANCEL;
	prScanCancelMsg->ucSeqNum = prAdapter->prMeshInfo->ucSeqNumOfScanReq;
	prScanCancelMsg->ucBssIndex = prGlMeshInfo->ucBssIndex;
#if CFG_ENABLE_WIFI_DIRECT
	if (prAdapter->fgIsP2PRegistered)
		prScanCancelMsg->fgIsChannelExt = FALSE;
#endif
	/* unbuffered message to guarantee scan is cancelled in sequence */
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanCancelMsg, MSG_SEND_METHOD_UNBUF);
}

VOID meshFsmRunEventScanDone(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;
	UINT_8 ucSeqNumOfCompMsg;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prGlueInfo = prAdapter->prGlueInfo;
	prMeshInfo = prGlueInfo->prMeshInfo;
	DEBUGFUNC("meshFsmRunEventScanDone()");

	if (!prAdapter->fgIsMESHRegistered) {
		DBGLOG(MESH, ERROR, "Mesh is de-registered : Igonore Scan Done\n");
		return;
	}

	/* Stop the Scan Done Timeout Timer */
	cnmTimerStopTimer(prAdapter, &prAdapter->prMeshInfo->rScanDoneTimer);
	DBGLOG(MESH, LOUD, "EVENT-SCAN DONE: Current Time = %lu\n", (unsigned long)kalGetTimeTick());
	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE)prMsgHdr;
	ASSERT(prScanDoneMsg->ucBssIndex == prMeshInfo->ucBssIndex);
	ucSeqNumOfCompMsg = prScanDoneMsg->ucSeqNum;
	cnmMemFree(prAdapter, prMsgHdr);

	if (ucSeqNumOfCompMsg != prAdapter->prMeshInfo->ucSeqNumOfScanReq) {
		DBGLOG(MESH, ERROR, "SEQ NO of MESH SCN DONE MSG is not matched %d %d.\n"
			, ucSeqNumOfCompMsg, prAdapter->prMeshInfo->ucSeqNumOfScanReq);
		ASSERT(0);
	}

	if (prAdapter->rWifiVar.prMeshConnSettings->fgIsScanReqIssued == TRUE) {
		prAdapter->rWifiVar.prMeshConnSettings->fgIsScanReqIssued = FALSE;
		kalMeshScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_MESH_INDEX, WLAN_STATUS_SUCCESS);
	}

}

VOID meshFsmRunEventScanDoneTimeOut(P_ADAPTER_T prAdapter, UINT_32 u4Param)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	ASSERT(prAdapter);
	if (!prAdapter->fgIsMESHRegistered) {
		DBGLOG(MESH, ERROR, "Mesh is de-registered : Igonore Scan Done Timeout\n");
		return;
	}

	prGlueInfo = prAdapter->prGlueInfo;
	prMeshInfo = prGlueInfo->prMeshInfo;
	prAdapter->rWifiVar.prMeshConnSettings->fgIsScanReqIssued = FALSE;
	kalMeshScanDone(prGlueInfo, prMeshInfo->ucBssIndex, WLAN_STATUS_SUCCESS);
	/* try to stop scan in Firmware  */
	meshFsmStateAbort_SCAN(prAdapter);
}

VOID meshRunEventFlushMsduInfoTimeOut(P_ADAPTER_T prAdapter, UINT_32 u4Param)
{
	/* TODO-mesh: Tx related */
}

WLAN_STATUS meshFsmRunEventFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	UINT_8 ucIndTxDoneOk = 1;
	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));
		if (rTxDoneStatus != TX_RESULT_SUCCESS) {
			DBGLOG(MESH, ERROR, "Frame TX Fail, Status:%d.\n", rTxDoneStatus);
			ucIndTxDoneOk = 0;
		}
		kalIndicateMac80211TxStatus(prAdapter->prGlueInfo,
			ucIndTxDoneOk,
			prMsduInfo,
			(UINT_32)prMsduInfo->u2FrameLength);
	} while (FALSE);

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS meshFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMgmtTxMsdu)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	UINT_8 ucRetryLimit = 3;	/* TX_DESC_TX_COUNT_NO_LIMIT; */

	do {
		ASSERT(prAdapter != NULL);
#if 0	/* the previous settings will be replaced, which may lead the flow become chaos */
		TX_SET_MMPDU(prAdapter,
			     prMgmtTxMsdu,
			     prMgmtTxMsdu->ucBssIndex,
			     prMgmtTxMsdu->ucStaRecIndex, /* (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),*/
			     WLAN_MAC_MGMT_HEADER_LEN,
			     prMgmtTxMsdu->u2FrameLength,
			     prMgmtTxMsdu->pfTxDoneHandler,
			     MSDU_RATE_MODE_AUTO);
#endif
		DBGLOG(MESH, INFO, "%s: bss_idx: %x StaRecIndex %x>\n",__func__, prMgmtTxMsdu->ucBssIndex, prMgmtTxMsdu->ucStaRecIndex);
		nicTxConfigPktControlFlag(prMgmtTxMsdu, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);
		nicTxSetPktRetryLimit(prMgmtTxMsdu, ucRetryLimit);
		/* send to TX queue */
		nicTxEnqueueMsdu(prAdapter, prMgmtTxMsdu);
	} while (FALSE);
	return rWlanStatus;
}

VOID meshFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_MGMT_TX_REQUEST_T prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMsgHdr != NULL));
		prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) prMsgHdr;
		meshFuncTxMgmtFrame(prAdapter, prMgmtTxMsg->prMgmtMsduInfo);
	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

VOID meshFsmRunEventBeaconUpdate(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	ASSERT_BREAK((prAdapter != NULL) && (prMsgHdr != NULL));
	prGlueInfo = prAdapter->prGlueInfo;
	prMeshInfo = prGlueInfo->prMeshInfo;

	DBGLOG(BSS, WARN, "meshFsmRunEventBeaconUpdate\n");
	/* 4 <3.3> update beacon content */
	mbssUpdateBeaconContent(prAdapter, prMeshInfo->ucBssIndex);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

VOID meshUpdateBssInfoForCreateMBSS(P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo;
	P_GL_MESH_INFO_T prGlMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo;
	P_MESH_CONNECTION_SETTINGS_T prConnSettings;

	prGlueInfo = prAdapter->prGlueInfo;
	prGlMeshInfo = prGlueInfo->prMeshInfo;
	prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prGlMeshInfo->ucBssIndex);
	prConnSettings = prAdapter->rWifiVar.prMeshConnSettings;

	if (prMeshBssInfo->fgIsBeaconActivated)
		return;
	/* 3 <1> Update BSS_INFO_T per Network Basis */
	/* 4 <1.1> Setup Operation Mode */
	prMeshBssInfo->eCurrentOPMode = OP_MODE_MESH;
	/* 4 <1.2> Setup SSID */
	COPY_SSID(prMeshBssInfo->aucSSID,
		prMeshBssInfo->ucSSIDLen,
		prConnSettings->aucSSID,
		prConnSettings->ucSSIDLen);
	/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
	prMeshBssInfo->prStaRecOfAP = (P_STA_RECORD_T)NULL;
	prMeshBssInfo->u2AssocId = 0;
	/* 4 <1.4> Setup Channel, Band and Phy Attributes */
	prMeshBssInfo->ucPrimaryChannel = prConnSettings->ucAdHocChannelNum;
	prMeshBssInfo->eBand = prConnSettings->eAdHocBand;
	prMeshBssInfo->eBssSCO = prConnSettings->eChnlSco;
	prMeshBssInfo->ucHtOpInfo1 = (UINT_8) (((UINT_32) prMeshBssInfo->eBssSCO) | HT_OP_INFO1_STA_CHNL_WIDTH);
	DBGLOG(MESH, INFO, "MBSS Channel:%d\n", prMeshBssInfo->ucPrimaryChannel);
	if (prMeshBssInfo->eBand == BAND_2G4) {
		/* Depend on eBand */
		prMeshBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prMeshBssInfo->ucConfigAdHocAPMode = MESH_MODE_MIXED_11BG;
	} else {
		/* Depend on eBand */
		prMeshBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet
			& PHY_TYPE_SET_802_11AN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prMeshBssInfo->ucConfigAdHocAPMode = MESH_MODE_11A;
	}
	/* 4 <1.5> Setup MIB for current BSS */
	prMeshBssInfo->u2BeaconInterval = prConnSettings->u2BeaconPeriod;
	prMeshBssInfo->ucDTIMPeriod = 0;
	prMeshBssInfo->u2ATIMWindow = 0;
	prMeshBssInfo->ucBeaconTimeoutCount = 0;
	/* 4 <1.6> Setup BSSID */
	COPY_MAC_ADDR(prMeshBssInfo->aucBSSID, prMeshBssInfo->aucOwnMacAddr);
	prMeshBssInfo->ucDTIMPeriod = prConnSettings->ucDTIMPeriod;
	prMeshBssInfo->u2BeaconInterval = prConnSettings->u2BeaconPeriod;
	/* FIXME: seem set by iwpriv command */
	prMeshBssInfo->u2ATIMWindow = prConnSettings->u2AtimWindow;
	prMeshBssInfo->fgIsProtection = FALSE;
	prConnSettings->eEncStatus = ENUM_ENCRYPTION_DISABLED;
	/* 3 <2> Update BSS_INFO_T common part */
	bssInitForMesh(prAdapter, prMeshBssInfo);
	/* 3 <3> Set MAC HW */
	/* 4 <3.1> Setup channel and bandwidth */
	/* FIXME: no MESH logic in rlmBssInitForAPandIbss in BENTEN. check */
	rlmBssInitForAPandIbss(prAdapter, prMeshBssInfo);
	/* Mark connected XXX */
	meshChangeMediaState(prMeshBssInfo, PARAM_MEDIA_STATE_CONNECTED);
	/* 4 <3.2> use command packets to inform firmware */
	nicUpdateBss(prAdapter, prGlMeshInfo->ucBssIndex);
	/* 4 <3.3> enable beaconing */
	mbssUpdateBeaconContent(prAdapter, prGlMeshInfo->ucBssIndex);
	/* 4 <3.4> Update AdHoc PM parameter */
	nicPmIndicateBssCreated(prAdapter, prGlMeshInfo->ucBssIndex);
	/* 3 <4> Set ACTIVE flag */
	prMeshBssInfo->fgIsBeaconActivated = TRUE;
	prMeshBssInfo->fgHoldSameBssidForIBSS = TRUE;
}

void meshFsmInit(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prMeshBssInfo = (P_BSS_INFO_T)NULL;
	P_MESH_SPECIFIC_BSS_INFO_T prMeshSpecificBssInfo = (P_MESH_SPECIFIC_BSS_INFO_T)NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	ASSERT(prAdapter);
	prGlueInfo = prAdapter->prGlueInfo;
	prMeshInfo = prGlueInfo->prMeshInfo;

	DBGLOG(MESH, INFO, "->meshFsmInit()\n");
	do {
		prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMeshInfo->ucBssIndex);
		prMeshSpecificBssInfo = prAdapter->rWifiVar.prMeshSpecificBssInfo;

		cnmTimerInitTimer(prAdapter, &prAdapter->prMeshInfo->rScanDoneTimer
						, (PFN_MGMT_TIMEOUT_FUNC)meshFsmRunEventScanDoneTimeOut,
						(ULONG) NULL);

		/*Init the Flush MsduInfo Timer*/
		cnmTimerInitTimer(prAdapter, &prAdapter->prMeshInfo->rFlushMsduInfoTimer,
						(PFN_MGMT_TIMEOUT_FUNC)meshRunEventFlushMsduInfoTimeOut,
						(ULONG) NULL);

		/*Start the Flush MsduInfo Timer*/
		cnmTimerStartTimer(prAdapter, &prAdapter->prMeshInfo->rFlushMsduInfoTimer
						, MESH_FLUSH_MSDUINFO_TIMER_MSEC);

		/* 4 <2> Initiate BSS_INFO_T - common part */
		BSS_INFO_INIT(prAdapter, prMeshBssInfo);
		/* 4 <2.1> Initiate BSS_INFO_T - Setup HW ID */
		prMeshBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
		prMeshBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		prMeshBssInfo->ucNonHTBasicPhyType = (UINT_8)
			rNonHTApModeAttributes[prMeshBssInfo->ucConfigAdHocAPMode].ePhyTypeIndex;
		prMeshBssInfo->u2BSSBasicRateSet =
			rNonHTApModeAttributes[prMeshBssInfo->ucConfigAdHocAPMode].u2BSSBasicRateSet;
		prMeshBssInfo->u2OperationalRateSet =
			rNonHTPhyAttributes[prMeshBssInfo->ucNonHTBasicPhyType].u2SupportedRateSet;
		rateGetDataRatesFromRateSet(prMeshBssInfo->u2OperationalRateSet, prMeshBssInfo->u2BSSBasicRateSet,
			prMeshBssInfo->aucAllSupportedRates, &prMeshBssInfo->ucAllSupportedRatesLen);

		prMeshBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter
				, OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem[0]) + MAX_IE_LENGTH);
		if (prMeshBssInfo->prBeacon) {
			prMeshBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
			prMeshBssInfo->prBeacon->ucStaRecIndex = 0xFF; /* NULL STA_REC */
			prMeshBssInfo->prBeacon->ucBssIndex = prMeshInfo->ucBssIndex;
		} else {
			/* Out of memory. */
			ASSERT(FALSE);
		}
		prMeshBssInfo->eCurrentOPMode = OP_MODE_NUM;

		prMeshBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
		prMeshBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
		prMeshBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
		prMeshBssInfo->ucPrimaryChannel = P2P_DEFAULT_LISTEN_CHANNEL;
		prMeshBssInfo->eBand = BAND_2G4;
		prMeshBssInfo->eBssSCO =  CHNL_EXT_SCN;

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucQoS))
			prMeshBssInfo->fgIsQBSS = TRUE;
		else
			prMeshBssInfo->fgIsQBSS = FALSE;

		SET_NET_PWR_STATE_IDLE(prAdapter, prMeshInfo->ucBssIndex);
	} while (FALSE);
}

VOID meshFsmUninit(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo = (P_BSS_INFO_T)NULL;

	ASSERT_BREAK(prAdapter != NULL);
	prGlueInfo = prAdapter->prGlueInfo;
	prMeshInfo = prGlueInfo->prMeshInfo;

	DBGLOG(MESH, INFO, "->meshFsmUninit()\n");
	prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMeshInfo->ucBssIndex);
	UNSET_NET_ACTIVE(prAdapter, prMeshInfo->ucBssIndex);

	/*Stop the Flush MsduInfo Timer*/
	cnmTimerStopTimer(prAdapter, &(prAdapter->prMeshInfo->rFlushMsduInfoTimer));

	SET_NET_PWR_STATE_IDLE(prAdapter, prMeshInfo->ucBssIndex);
	DBGLOG(MESH, INFO, "wlanProcessCommandQueue, num of element:%d\n",
		(UINT32)prAdapter->prGlueInfo->rCmdQueue.u4NumElem);

	/* Clear CmdQue */
	kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo, prMeshInfo->ucBssIndex);
	kalClearSecurityFramesByBssIdx(prAdapter->prGlueInfo, prMeshInfo->ucBssIndex);
	/* Clear PendingCmdQue */
	wlanReleasePendingCMDbyBssIdx(prAdapter, prMeshInfo->ucBssIndex);
	/* Clear PendingTxMsdu */
	nicFreePendingTxMsduInfoByBssIdx(prAdapter, prMeshInfo->ucBssIndex);
	/* Deactivate BSS. */
	UNSET_NET_ACTIVE(prAdapter, prMeshInfo->ucBssIndex);
	nicDeactivateNetwork(prAdapter, prMeshInfo->ucBssIndex);

	if (prMeshBssInfo->prBeacon) {
		cnmMgtPktFree(prAdapter, prMeshBssInfo->prBeacon);
		prMeshBssInfo->prBeacon = NULL;
	}

	cnmFreeBssInfo(prAdapter, prMeshBssInfo);
}

WLAN_STATUS MeshPeerAdd(P_ADAPTER_T prAdapter, VOID *pvSetBuffer, UINT_32 u4SetBufferLen, UINT_32 *pu4SetInfoLen)
{
	GLUE_INFO_T *prGlueInfo;
	P_MESH_CMD_PEER_ADD_T prMeshCmdPeerAdd;
	STA_RECORD_T *prStaRec;
	P_GL_MESH_INFO_T prGlMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo = (P_BSS_INFO_T)NULL;
#if 0
	UINT_8 ucCurrent_mp_count = 0;
	/* Current Number of AP Clients */
	UINT_8 ucCurrent_apc_count = 0;
#endif

	DBGLOG(MESH, INFO, "->%s\n", __func__);
	prGlueInfo = (GLUE_INFO_T *) prAdapter->prGlueInfo;
	prGlMeshInfo = prGlueInfo->prMeshInfo;
	prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prGlMeshInfo->ucBssIndex);

#if 0	/* KOKO FIXME: should be fixed at concurrent stage */
	ucCurrent_mp_count = prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_MESH_INDEX].rStaRecOfClientList.u4NumElem;
	ucCurrent_apc_count = prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].rStaRecOfClientList.u4NumElem;
	/* Firmware now has 8 entries (Entries No 2 to 9) shared b/w Mesh and AP.
	 * Mesh has entries 10 and 11 exclusively reserved for it(in absece of STA interface)
	 * So, Only 8 Mesh Peers can be supported and that too when ucCurrent_apc_count <=2 */
	BUG_ON((ucCurrent_mp_count + ucCurrent_apc_count >= 11) || (ucCurrent_mp_count > 8));
	if (ucCurrent_mp_count == 8) {
		DBGLOG(MESH, INFO, "MESH ADD: Failed(At Limit=8): Can't add more Mesh Peers(Already Max)\n");
		return WLAN_STATUS_FAILURE;
	} else if (ucCurrent_mp_count  == (10 - ucCurrent_apc_count)) {
		DBGLOG(MESH, INFO, "MESH ADD: Failed(At Limit=%x): SoftAP Clients(%x) have eaten up shared WTBLE resources\n",
			ucCurrent_mp_count, ucCurrent_apc_count);
			return WLAN_STATUS_FAILURE;
	} else
		DBGLOG(MESH, INFO, "MESH ADD: This would be Mesh peer number %x\n", ucCurrent_mp_count +1);
#endif
	/* init */
	*pu4SetInfoLen = sizeof(MESH_CMD_PEER_ADD_T);
	prMeshCmdPeerAdd = (P_MESH_CMD_PEER_ADD_T) pvSetBuffer;

	prStaRec = cnmGetStaRecByAddress(prAdapter, prGlMeshInfo->ucBssIndex, prMeshCmdPeerAdd->aucPeerMac);
	if (!prStaRec) {
		DBGLOG(MESH, INFO, "MESH ADD: Allocating new Sta Record\n");
		/* KOKO TODO: Check sta type in firmware */
		prStaRec = cnmStaRecAlloc(prAdapter, STA_TYPE_MESH_PEER, prGlMeshInfo->ucBssIndex, prMeshCmdPeerAdd->aucPeerMac);
		prStaRec->u2BSSBasicRateSet = prMeshCmdPeerAdd->u2BSSBasicRateSet;
		prStaRec->u2DesiredNonHTRateSet = prMeshCmdPeerAdd->u2DesiredNonHTRateSet;
		prStaRec->u2OperationalRateSet = prMeshCmdPeerAdd->u2OperationalRateSet;
		prStaRec->ucPhyTypeSet = prMeshCmdPeerAdd->ucPhyTypeSet;
		/* HT PARAMS */
		prStaRec->u2HtCapInfo =  prMeshCmdPeerAdd->u2HtCapInfo;
		prStaRec->ucAmpduParam = prMeshCmdPeerAdd->ucAmpduParam;
		prStaRec->ucMcsSet = prMeshCmdPeerAdd->ucMcsSet;
		prStaRec->ucDesiredPhyTypeSet= prMeshCmdPeerAdd->ucDesiredPhyTypeSet;
		/* ASSOC ID */
		prStaRec->u2AssocId = prMeshCmdPeerAdd->u2AssocId;
		/* Update default Tx rate */
		nicTxUpdateStaRecDefaultRate(prStaRec);
		/* KOKO TODO: check which state to check when security is applied */
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
	} else
		DBGLOG(MESH, INFO, "MESH ADD: Fail to alloc new Sta Record\n");

	bssAddClient(prAdapter, prMeshBssInfo, prStaRec);
 	meshChangeMediaState(prMeshBssInfo, PARAM_MEDIA_STATE_CONNECTED);
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
	DBGLOG(MESH, INFO, "MESH ADD: %pM added @ %d\n", prStaRec->aucMacAddr, prStaRec->ucIndex);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS MeshPeerRemove(P_ADAPTER_T prAdapter, VOID *pvSetBuffer, UINT_32 u4SetBufferLen, UINT_32 *pu4SetInfoLen)
{
	GLUE_INFO_T *prGlueInfo;
	P_GL_MESH_INFO_T prGlMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo = (P_BSS_INFO_T)NULL;
	MESH_CMD_PEER_REMOVE_T *prMeshCmdPeerRemove;
	STA_RECORD_T *prStaRec = NULL;

	/* init */
	prGlueInfo = (GLUE_INFO_T *) prAdapter->prGlueInfo;
	prGlMeshInfo = prGlueInfo->prMeshInfo;
	prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prGlMeshInfo->ucBssIndex);

	*pu4SetInfoLen = sizeof(MESH_CMD_PEER_REMOVE_T);
	prMeshCmdPeerRemove = (MESH_CMD_PEER_REMOVE_T *) pvSetBuffer;
	prStaRec = cnmGetStaRecByAddress(prAdapter, prGlMeshInfo->ucBssIndex, prMeshCmdPeerRemove->aucPeerMac);

	if (prStaRec == NULL) {
		DBGLOG(MESH, INFO, "REMOVE:Station Record Null during remove_sta\n");
		return WLAN_STATUS_SUCCESS;
	}

	bssRemoveClient(prAdapter, prMeshBssInfo, prStaRec);
	/* DO this, before freeing */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);
	cnmStaRecFree(prAdapter, prStaRec);
	DBGLOG(MESH, INFO, "REMOVE: %pM is now removed\n", prMeshCmdPeerRemove->aucPeerMac);
	return WLAN_STATUS_SUCCESS;
}
#endif /* CFG_ENABLE_WIFI_MESH */
