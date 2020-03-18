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
/*! \file   gl_mesh_mac80211.c
* Main routines of Linux driver interface for Wi-Fi Mesh using mac80211 interface
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

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <net/mac80211.h>

#include "gl_os.h"
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

/* Rate same as in mtk_rates[] */
int mesh_mtk_rate_set[] = {
	RATE_SET_BIT_1M,
	RATE_SET_BIT_2M,
	RATE_SET_BIT_5_5M,
	RATE_SET_BIT_11M,
	RATE_SET_BIT_6M,
	RATE_SET_BIT_9M,
	RATE_SET_BIT_12M,
	RATE_SET_BIT_18M,
	RATE_SET_BIT_24M,
	RATE_SET_BIT_36M,
	RATE_SET_BIT_48M,
	RATE_SET_BIT_54M,
};
#define mesh_rate_set_size (ARRAY_SIZE(mesh_mtk_rate_set))

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static void mtk_mesh_mac80211_hw_scan_work(struct work_struct *work);
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
void mtk_mesh_mac80211_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *control, struct sk_buff *skb)
{
	P_ADAPTER_T prAdapter = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prGlMeshInfo = NULL;
	P_STA_RECORD_T prStaRec = NULL;
	//struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	//struct ieee80211_sta *sta = NULL;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	UINT_8 ucStaRecIndex = STA_REC_INDEX_NOT_FOUND;
	int result = WLAN_STATUS_SUCCESS;
#if 1/* CFG_ENABLE_MESH_TX_RX_DEBUG */
	struct sk_buff *prSkb = skb;
	int i;
#endif

	prGlueInfo = hw->priv;
	prAdapter = prGlueInfo->prAdapter;
	prGlMeshInfo = prGlueInfo->prMeshInfo;

#if 1/* CFG_ENABLE_MESH_TX_RX_DEBUG */
	DBGLOG(MESH, INFO, "sk_buff->len: %d\n", skb->len);
	if (ieee80211_is_data(hdr->frame_control))
		DBGLOG(MESH, INFO, "    Data Frame...\n");
	else if (ieee80211_is_mgmt(hdr->frame_control))
		DBGLOG(MESH, INFO, "    Mgmt Frame...\n");
	if (ieee80211_is_action(hdr->frame_control))
		DBGLOG(MESH, INFO, "          Action Frame...\n");
	if (ieee80211_is_auth(hdr->frame_control))\
		DBGLOG(MESH, INFO, "          Auth Frame...\n");
	if (ieee80211_is_probe_req(hdr->frame_control))
		DBGLOG(MESH, INFO, "          Probe Req Frame...\n");

	DBGLOG(MESH, INFO, "Tx Dest:"MACSTR"\n", MAC2STR(hdr->addr1));

	i = 0;
	while (i < prSkb->len) {
		DBGLOG(MESH, INFO, "%4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x \n",
			prSkb->data[i], prSkb->data[i + 1], prSkb->data[i + 2], prSkb->data[i + 3],
			prSkb->data[i + 4], prSkb->data[i + 5], prSkb->data[i + 6], prSkb->data[i + 7],
			prSkb->data[i + 8], prSkb->data[i + 9], prSkb->data[i + 10], prSkb->data[i + 11]);
		i += 12;
	}
#endif
	if (!is_multicast_ether_addr(hdr->addr1)) {
		prStaRec = cnmGetStaRecByAddress(prAdapter, prGlMeshInfo->ucBssIndex, hdr->addr1);
		/* Drop Unicast Data Frames if StaRec doesnt't exist */
		if (!prStaRec && ieee80211_is_data(hdr->frame_control)) {
			DBGLOG(MESH, INFO, "Mesh: Sta Rec doesn't exist for this Unicast Data Packet from Mac(Removed?) \n");
			goto tx_fail;
		}

		if(prStaRec)
			ucStaRecIndex = prStaRec->ucIndex;
	} else
		ucStaRecIndex = STA_REC_INDEX_BMCAST;
	/* MESH TX Stats */
	prGlueInfo->prAdapter->rTxCtrl.incomingMac80211Num++;
	/* for TXD */
        if (ieee80211_is_data(hdr->frame_control)) {
        	if (skb_headroom(prSkb) < NIC_TX_HEAD_ROOM) {
			DBGLOG(MESH, ERROR, "ERR: headroom is not enough for TXD in DATA skb\n");
			goto tx_fail;
		}
		result = wlanEnqueueMac80211TxPacket(prGlueInfo->prAdapter, skb, ucStaRecIndex);
	} else if (ieee80211_is_mgmt(hdr->frame_control))
		result = wlanEnqueueMac80211TxMgmt(prGlueInfo->prAdapter, skb, ucStaRecIndex);

	if (result != WLAN_STATUS_SUCCESS)
		goto tx_fail;

	/* XXX:TODO incref count total packets --considering AP-Mesh concurrency.--Add flow control? */
#if 0
	if (ieee80211_is_data(hdr->frame_control)) {
		GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingMac80211FrameNum);
		kalSetMac80211Event(prGlueInfo);
	} else {
		GLUE_INC_REF_CNT(prGlueInfo->prAdapter->rTxCtrl.i4TxMgmtPendingNum);
		kalSetEvent(prGlueInfo);
	}
#else
	if (ieee80211_is_data(hdr->frame_control))
		GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingMac80211FrameNum);
	else
		GLUE_INC_REF_CNT(prGlueInfo->prAdapter->rTxCtrl.i4TxMgmtPendingNum);
	kalSetEvent(prGlueInfo);
#endif
	return;
tx_fail:
	DBGLOG(MESH, INFO, "TX failed--indicating to mac\n");
	/* MESH TX Stats */
	prGlueInfo->prAdapter->rTxCtrl.Mac80211FreedNum++;
	ieee80211_free_txskb(hw, skb);
}

int mtk_mesh_mac80211_start(struct ieee80211_hw *hw)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;

	prGlueInfo = hw->priv;
	prAdapter = prGlueInfo->prAdapter;
	prMeshInfo = prGlueInfo->prMeshInfo;

	meshFsmInit(prAdapter);
	INIT_DELAYED_WORK(&prMeshInfo->hw_scan, mtk_mesh_mac80211_hw_scan_work);
	prGlueInfo->meshStarted = TRUE;
	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_start...\n");
	return 0;
}

void mtk_mesh_mac80211_stop(struct ieee80211_hw *hw)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;

	prGlueInfo = hw->priv;
	prGlueInfo->meshStarted = FALSE;
	prAdapter = prGlueInfo->prAdapter;
	prMeshInfo = prGlueInfo->prMeshInfo;

	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_stop...\n");
	meshFsmUninit(prAdapter);
}

int mtk_mesh_mac80211_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo = NULL;
	int i = 0;

	prGlueInfo = hw->priv;
	prMeshInfo = prGlueInfo->prMeshInfo;
	DBGLOG(MESH, INFO, "Enter mtk_mesh_mac80211_add_interface\n");

	if (prGlueInfo && prGlueInfo->prAdapter)
		prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prMeshInfo->ucBssIndex);

	/* Check if the AP interface has the same address */
	for (i = 0; i < KAL_P2P_NUM; i++) {
		if (prGlueInfo->prP2PInfo[i] && prGlueInfo->prP2PInfo[i]->prDevHandler &&
				prGlueInfo->prP2PInfo[i]->prDevHandler->dev_addr &&
				prGlueInfo->prP2PInfo[i]->prDevHandler->flags & IFF_UP) {
			if (ether_addr_equal(vif->addr, prGlueInfo->prP2PInfo[i]->prDevHandler->dev_addr))
				return -ENOTUNIQ;
		}
	}

	vif->cab_queue = 0;
	vif->hw_queue[IEEE80211_AC_VO] = 0;
	vif->hw_queue[IEEE80211_AC_VI] = 1;
	vif->hw_queue[IEEE80211_AC_BE] = 2;
	vif->hw_queue[IEEE80211_AC_BK] = 3;

	prGlueInfo->prMeshInfo->vif = vif;
	DBGLOG(MESH, INFO, "%s %pM\n", __func__, vif->addr);
	COPY_MAC_ADDR(prMeshBssInfo->aucOwnMacAddr, vif->addr);

	return 0;
}

int mtk_mesh_mac80211_change_interface(struct ieee80211_hw *hw,
						       struct ieee80211_vif *vif,
						       enum nl80211_iftype newtype,
						       bool p2p)
{
	return 0;
}

void mtk_mesh_mac80211_remove_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	prGlueInfo = hw->priv;
	prMeshInfo = prGlueInfo->prMeshInfo;
	prGlueInfo->prMeshInfo->vif = NULL;

	DBGLOG(MESH, INFO, "MESH: mtk_mac80211_remove_interface\n");
#if 0
	wiphy_debug(hw->wiphy, "%s (type=%d mac_addr=%pM)\n",
		    __func__, ieee80211_vif_type_p2p(vif),
		    vif->addr);
#endif
}

int mtk_mesh_mac80211_config(struct ieee80211_hw *hw, u32 changed)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct ieee80211_conf *conf = &hw->conf;
	WLAN_STATUS rStatus = 0;

	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_config...\n");

	prGlueInfo = hw->priv;

	if (changed & IEEE80211_CONF_CHANGE_IDLE) {
		if (conf->flags & IEEE80211_CONF_IDLE)
			DBGLOG(MESH, INFO, "    SET CONF_IDLE ...\n");
		else
			DBGLOG(MESH, INFO, "    CLR CONF_IDLE ...\n");
	}

	if (changed & IEEE80211_CONF_CHANGE_MONITOR) {
		if (conf->flags & IEEE80211_CONF_MONITOR)
			DBGLOG(MESH, INFO, "    SET CONF_MONITOR ...\n");
		else
			DBGLOG(MESH, INFO, "    CLR CONF_MONITOR ...\n");
	}

	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		DBGLOG(MESH, ERROR, "CHANCTX: Something is wrong with, Why IEEE80211_CONF_CHANGE_CHANNEL\n");
		WARN_ON(changed & IEEE80211_CONF_CHANGE_CHANNEL);
	}

	if (changed & IEEE80211_CONF_CHANGE_PS) {
		if (conf->flags & IEEE80211_CONF_PS)
			DBGLOG(MESH, INFO, "    SET PS...\n");
		else
			DBGLOG(MESH, INFO, "    SET CAM ...\n");
	}

	if (changed & IEEE80211_CONF_CHANGE_POWER)
		DBGLOG(MESH, INFO, "    SET TX_POWER = %d dBm\n", conf->power_level);

	return rStatus;
}

void mtk_mesh_mac80211_configure_filter(struct ieee80211_hw *hw,
						      unsigned int changed_flags,
						      unsigned int *total_flags,
						      u64 multicast)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	UINT_32 u4PacketFilter = 0;
	UINT_32 u4SetInfoLen;

	prGlueInfo = hw->priv;
	prMeshInfo = prGlueInfo->prMeshInfo;

	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_configure_filter...\n");
	/* CMD_ID_SET_RX_FILTER */
	changed_flags &= MESH_SUPPORTED_FILTERS;
	*total_flags &= MESH_SUPPORTED_FILTERS;
    /* No matter what , allways accepts Bcast frames */
	u4PacketFilter |= PARAM_PACKET_FILTER_BROADCAST;
	if (*total_flags & FIF_BCN_PRBRESP_PROMISC) {
		u4PacketFilter |= PARAM_PACKET_FILTER_BROADCAST;
		DBGLOG(MESH, INFO, "    SET FIF_BCN_PRBRESP_PROMISC\n");
	}
	if (*total_flags & FIF_ALLMULTI) {
		u4PacketFilter |= PARAM_PACKET_FILTER_ALL_MULTICAST;
		DBGLOG(MESH, INFO, "    SET FIF_ALLMULTI\n");
	}
	if (*total_flags & FIF_PROBE_REQ) {
		u4PacketFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
		DBGLOG(MESH, INFO, "    SET FIF_PROBE_REQ\n");
	}
	/* TODO: check if FILTER_ACTION_FRAME is needed for mesh connection setup */
	/* u4PacketFilter |= PARAM_PACKET_FILTER_ACTION_FRAME; */
	prMeshInfo->rx_filter = *total_flags;
	if (kalIoctl(prGlueInfo,
			wlanoidSetCurrentPacketFilter,
			&u4PacketFilter,
			sizeof(u4PacketFilter),
			FALSE,
			FALSE,
			TRUE,
			&u4SetInfoLen) != WLAN_STATUS_SUCCESS) {
		DBGLOG(MESH, ERROR, "Return mtk_mesh_mac80211_configure_filter !!!FAIL %x\n");
		return;
	}
}

int mtk_mesh_mac80211_sta_add(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif,
					   struct ieee80211_sta *sta)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	MESH_CMD_PEER_ADD_T rMeshCmdPeerAdd;
	UINT_32 u4StaLegacyRateSet = 0, i, u4_5GOffset = 0, band = 0;

	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_sta_add MAC Address = %02x:%02x:%02x:%02x:%02x:%02x\n"
		, sta->addr[5], sta->addr[4], sta->addr[3], sta->addr[2], sta->addr[1], sta->addr[0]);

	prGlueInfo = hw->priv;
	prAdapter = prGlueInfo->prAdapter;

	ASSERT(prGlueInfo->prMeshInfo);
	DBGLOG(MESH, INFO, "2.4GHtMcsMask=%x 5GHtMcsMask=%x 2.4GLegacyRateSet=%x, 5GLegacyRateSet=%x\n"
		, prGlueInfo->prMeshInfo->u4MeshHtMcsMask[0], prGlueInfo->prMeshInfo->u4MeshHtMcsMask[1]
		, prGlueInfo->prMeshInfo->u4MeshLegacyRateSet[0], prGlueInfo->prMeshInfo->u4MeshLegacyRateSet[1]);

	kalMemZero(&rMeshCmdPeerAdd, sizeof(rMeshCmdPeerAdd));
	/* Copy MAC address */
	kalMemCopy(rMeshCmdPeerAdd.aucPeerMac, sta->addr, 6);
	band = vif->bss_conf.chandef.chan->band;

	/* KOKO FIXME: Find a way to align with current rate table in code */
	if (band == IEEE80211_BAND_5GHZ)
		u4_5GOffset = 4;

	for (i = 0; (i + u4_5GOffset) < mesh_rate_set_size; i++) {
		if(sta->supp_rates[band] & (1 << i))
			u4StaLegacyRateSet |= mesh_mtk_rate_set[i + u4_5GOffset];
	}

	DBGLOG(MESH, INFO, "STA Rate Set =%x MCS mask = %x\n", u4StaLegacyRateSet, sta->ht_cap.mcs.rx_mask[0]);
	/* Copy Rate Params(TODO: copy from mac80211 sta) */
	rMeshCmdPeerAdd.u2BSSBasicRateSet = BASIC_RATE_SET_ERP & prGlueInfo->prMeshInfo->u4MeshLegacyRateSet[band] & u4StaLegacyRateSet;
	rMeshCmdPeerAdd.u2DesiredNonHTRateSet = prGlueInfo->prMeshInfo->u4MeshLegacyRateSet[band] & u4StaLegacyRateSet;
	rMeshCmdPeerAdd.u2OperationalRateSet = prGlueInfo->prMeshInfo->u4MeshLegacyRateSet[band] & u4StaLegacyRateSet;
	rMeshCmdPeerAdd.ucPhyTypeSet = PHY_TYPE_SET_802_11GN;
	/* Copy HT Params */
	rMeshCmdPeerAdd.u2HtCapInfo =  sta->ht_cap.cap;
	rMeshCmdPeerAdd.ucAmpduParam = (sta->ht_cap.ampdu_density << 2) | sta->ht_cap.ampdu_factor;
	rMeshCmdPeerAdd.ucMcsSet = sta->ht_cap.mcs.rx_mask[0] & prGlueInfo->prMeshInfo->u4MeshHtMcsMask[band];
	rMeshCmdPeerAdd.ucDesiredPhyTypeSet= PHY_TYPE_BIT_HT;
	/* Copy Assoc Id*/
	rMeshCmdPeerAdd.u2AssocId = sta->aid;
	/* create a Mesh peer record */
	rStatus = kalIoctl(prGlueInfo, MeshPeerAdd, &rMeshCmdPeerAdd
			, sizeof(MESH_CMD_PEER_ADD_T), FALSE, FALSE, FALSE, &u4BufLen);
	/* Fail due to insufficent Resources */
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(MESH, INFO, "Return mtk_mesh_mac80211_sta_add:FAIL MAC Address = %pM\n", sta->addr);
		return -ENOBUFS;
	}
	DBGLOG(MESH, INFO, "Return mtk_mesh_mac80211_sta_add Success:Add Peer:"MACSTR", AID=%d\n", MAC2STR(sta->addr), sta->aid);

	return 0;
}

int mtk_mesh_mac80211_sta_remove(struct ieee80211_hw *hw,
						struct ieee80211_vif *vif,
						struct ieee80211_sta *sta)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	MESH_CMD_PEER_REMOVE_T rMeshCmdPeerRemove;

	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_sta_remove MAC Address = %02x:%02x:%02x:%02x:%02x:%02x\n"
		, sta->addr[5], sta->addr[4], sta->addr[3], sta->addr[2], sta->addr[1], sta->addr[0]);
	prGlueInfo = hw->priv;
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rMeshCmdPeerRemove, sizeof(rMeshCmdPeerRemove));
	/* Copy MAC address */
	kalMemCopy(rMeshCmdPeerRemove.aucPeerMac, sta->addr, 6);
	rStatus = kalIoctl(prGlueInfo, MeshPeerRemove, &rMeshCmdPeerRemove, sizeof(MESH_CMD_PEER_REMOVE_T), FALSE, FALSE, FALSE, &u4BufLen);
	/* Mesh Peer Remove should never fail */
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(MESH, INFO, "OOPS mtk_mesh_mac80211_sta_remove: FAILED\n");
		return -1;
	}

	DBGLOG(MESH, INFO, "Return mtk_mesh_mac80211_sta_remove: Success\n");
	return 0;
}

void mtk_mesh_mac80211_sta_notify(struct ieee80211_hw *hw,
					       struct ieee80211_vif *vif,
					       enum sta_notify_cmd cmd,
					       struct ieee80211_sta *sta)
{
	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_sta_notify MAC Address = %02x:%02x:%02x:%02x:%02x:%02x\n"
		, sta->addr[5], sta->addr[4], sta->addr[3], sta->addr[2], sta->addr[1], sta->addr[0]);
}

int mtk_mesh_mac80211_hw_scan(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_scan_request *req)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo = NULL;
	struct cfg80211_scan_request *request;

	prGlueInfo = hw->priv;
	ASSERT(prGlueInfo);

	prMeshInfo = prGlueInfo->prMeshInfo;
	ASSERT(prMeshInfo);

	prMeshBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, prMeshInfo->ucBssIndex);
	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_hw_scan...\n");
	request = &(req->req);

	/* check if there is any pending scan not yet finished */
	if (prMeshInfo->prScanRequest != NULL) {
		DBGLOG(MESH, ERROR, "MESH: prMeshInfo->prScanRequest != NULL\n");
		return -EBUSY;
	}

	if (prMeshBssInfo->fgIsBeaconActivated) {
		DBGLOG(MESH, ERROR, "MESH: Mesh Already Active\n");
		return -EBUSY;
	}

	kalMemZero(&prMeshInfo->rScanRequest, sizeof(PARAM_SCAN_REQUEST_EXT_T));
	if (request->n_ssids == 0)
		prMeshInfo->rScanRequest.rSsid.u4SsidLen = 0;
	else if (request->n_ssids == 1)
		COPY_SSID(prMeshInfo->rScanRequest.rSsid.aucSsid, prMeshInfo->rScanRequest.rSsid.u4SsidLen
			, request->ssids[0].ssid, request->ssids[0].ssid_len);
	else {
		DBGLOG(REQ, INFO, "request->n_ssids:%d\n", request->n_ssids);
		return -EINVAL;
	}

	prMeshInfo->rScanRequest.u4IELength = sizeof(MESH_SCAN_IES) + request->ie_len;
	/*
	 * passing ieee80211_scan_req in pucIE, will do the segregation of
	 * IEs for different bands in wlanoidSetBssidListScanExtMesh().
	 */
	prMeshInfo->rScanRequest.pucIE = (PUINT_8)(req);

	ieee80211_queue_delayed_work(hw, &prMeshInfo->hw_scan, 0);
	/* TODO:: FIXME in benten just send req to prScanRequest but type do not match */
	prMeshInfo->prScanRequest = request;

	return 0;
}

void  mtk_mesh_mac80211_cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
}

int mtk_mesh_mac80211_set_rts_threshold(struct ieee80211_hw *hw, u32 value)
{
	return 0;
}

int mtk_mesh_mac80211_mbss_beacon_update(struct ieee80211_hw *hw,
	struct ieee80211_vif *vif)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	P_MESH_CONNECTION_SETTINGS_T prConnSettings = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_MSDU_INFO_T prMsduInfo;
	P_MSG_MESH_BEACON_UPDATE_T prMeshBcnUpdateMsg = (P_MSG_MESH_BEACON_UPDATE_T)NULL;
	struct sk_buff *beacon = NULL;
	struct ieee80211_bss_conf *bss_conf = NULL;
	INT_32 i4Rslt = -EINVAL;
	struct ieee80211_mgmt *mgmt = NULL;

	prGlueInfo = hw->priv;
	ASSERT(prGlueInfo);
	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_mbss_beacon_update\n");

	prMeshInfo = prGlueInfo->prMeshInfo;
	ASSERT(prMeshInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prConnSettings = prAdapter->rWifiVar.prMeshConnSettings;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMeshInfo->ucBssIndex);

	DBGLOG(MESH, INFO, "	Set BSS Info...\n");
    /* 4 <1> Configure MBSS */
	bss_conf = &vif->bss_conf;

	prConnSettings->u2BeaconPeriod = bss_conf->beacon_int;
	prConnSettings->ucDTIMPeriod = bss_conf->dtim_period;

    /* 4 <2> Update MBSS Beacon content */
	/* Allocate a MSDU_INFO_T */
	prMsduInfo = prBssInfo->prBeacon;

	ASSERT(prMsduInfo);

	DBGLOG(MESH, INFO, "	Get Beacon Content...\n");
	beacon = ieee80211_beacon_get(hw, vif);

	mgmt = (struct ieee80211_mgmt *)beacon->data;
	if (mgmt->u.beacon.capab_info & WLAN_CAPABILITY_PRIVACY)
		prConnSettings->eAuthMode = AUTH_MODE_SAE;
	else
		prConnSettings->eAuthMode = AUTH_MODE_OPEN;
	/*
	 * Update the TSF adjust value here, the HW will add this value for every beacon
	 * beacon: [control + duration + da + sa + bssid +seq] [timestamp + beacon_int + cap][IE]
	 */
	kalMemCopy(prMsduInfo->prPacket, beacon->data, beacon->len);
	prMsduInfo->u2FrameLength = beacon->len;

	/* Free the beacon skb, obtained from mac80211 */
	dev_kfree_skb_any(beacon);
	do {
		prMeshBcnUpdateMsg = (P_MSG_MESH_BEACON_UPDATE_T)cnmMemAlloc(prGlueInfo->prAdapter
			, RAM_TYPE_MSG, sizeof(MSG_MESH_BEACON_UPDATE_T));

		if (prMeshBcnUpdateMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMeshBcnUpdateMsg->rMsgHdr.eMsgId = MID_MNY_MESH_BEACON_UPDATE;
		/* pucBuffer = prMeshBcnUpdateMsg->aucBuffer; */
		prMeshBcnUpdateMsg->u4BcnHdrLen = 0;
		prMeshBcnUpdateMsg->pucBcnHdr = NULL;
		prMeshBcnUpdateMsg->u4BcnBodyLen = 0;
		prMeshBcnUpdateMsg->pucBcnBody = NULL;
		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMeshBcnUpdateMsg, MSG_SEND_METHOD_BUF);
	} while (FALSE);

	return i4Rslt;
}

int mtk_mesh_mac80211_mbss_beacon_enable(struct ieee80211_hw *hw,
	struct ieee80211_vif *vif)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	P_MESH_CONNECTION_SETTINGS_T prConnSettings;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_MSDU_INFO_T prMsduInfo;
	UINT_32 u4BufLen;
	unsigned int rStatus = WLAN_STATUS_SUCCESS;
	struct sk_buff *beacon;
	struct ieee80211_bss_conf *bss_conf;
	struct ieee80211_mgmt *mgmt;

	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_mbss_beacon_enable\n");

	prGlueInfo = hw->priv;
	ASSERT(prGlueInfo);

	prMeshInfo = prGlueInfo->prMeshInfo;
	ASSERT(prMeshInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prConnSettings = prAdapter->rWifiVar.prMeshConnSettings;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMeshInfo->ucBssIndex);

	DBGLOG(MESH, INFO, "    Set BSS Info...\n");
	/* 4 <1> Configure MBSS */
	bss_conf = &vif->bss_conf;

	prConnSettings->u2BeaconPeriod = bss_conf->beacon_int;
	prConnSettings->ucDTIMPeriod = bss_conf->dtim_period;

	/* 4 <2> Update MBSS Beacon content */
	/* Allocate a MSDU_INFO_T */
	prMsduInfo = prBssInfo->prBeacon;

	ASSERT(prMsduInfo);

	DBGLOG(MESH, INFO, "	 Get Beacon Content...\n");
	beacon = ieee80211_beacon_get(hw, vif);

	mgmt = (struct ieee80211_mgmt *)beacon->data;
	if (mgmt->u.beacon.capab_info & WLAN_CAPABILITY_PRIVACY)
		prConnSettings->eAuthMode = AUTH_MODE_SAE;
	else
		prConnSettings->eAuthMode = AUTH_MODE_OPEN;
	/*
	 * Update the TSF adjust value here, the HW will add this value for every beacon
	 * beacon: [control + duration + da + sa + bssid +seq] [timestamp + beacon_int + cap][IE]
	 */
	kalMemCopy(prMsduInfo->prPacket, beacon->data, beacon->len);
	prMsduInfo->u2FrameLength = beacon->len;

	/* Free the beacon skb, obtained from mac80211 */
	dev_kfree_skb_any(beacon);
	DBGLOG(MESH, INFO, "	 IOCTL to Set MBSS...\n");
	rStatus = kalIoctl(prGlueInfo,
		wlanoidSetMbss,
		NULL,
		0,
		FALSE,
		FALSE,
		FALSE,
		&u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, INFO, "scan error:%x\n", rStatus);
		return -EINVAL;
	}
	return 0;
}

int mtk_mesh_mac80211_mbss_beacon_disable(struct ieee80211_hw *hw,
	struct ieee80211_vif *vif)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen;

	prGlueInfo = hw->priv;
	ASSERT(prGlueInfo);
	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_mbss_beacon_disable\n");
	rStatus = kalIoctl(prGlueInfo,
		wlanoidSetMbssLeave,
		NULL,
		0,
		FALSE,
		FALSE,
		TRUE,
		&u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "Leave MBSS:%lx\n", rStatus);
		return -EFAULT;
	}
	return 0;
}

void mtk_mesh_mac80211_bss_info_changed(struct ieee80211_hw *hw,
							  struct ieee80211_vif *vif,
							  struct ieee80211_bss_conf *bss_conf,
							  u32 changed)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	prGlueInfo = hw->priv;
	ASSERT(prGlueInfo);
	prMeshInfo = prGlueInfo->prMeshInfo;
	ASSERT(prMeshInfo);

	DBGLOG(MESH, INFO, "ENTER %s Changed =%x\n", __func__, changed);
	/*
	* BSS_CHANGED_ASSOC       = 1<<0,
	* BSS_CHANGED_ERP_CTS_PROT    = 1<<1,
	* BSS_CHANGED_ERP_PREAMBLE    = 1<<2,
	* BSS_CHANGED_ERP_SLOT        = 1<<3,
	* BSS_CHANGED_HT          = 1<<4,
	* BSS_CHANGED_BASIC_RATES     = 1<<5,
	* BSS_CHANGED_BEACON_INT      = 1<<6,
	* BSS_CHANGED_BSSID       = 1<<7,
	* BSS_CHANGED_BEACON      = 1<<8,
	* BSS_CHANGED_BEACON_ENABLED  = 1<<9,
	* BSS_CHANGED_CQM         = 1<<10,
	* BSS_CHANGED_IBSS        = 1<<11,
	* BSS_CHANGED_ARP_FILTER      = 1<<12,
	* BSS_CHANGED_QOS         = 1<<13,
	* BSS_CHANGED_IDLE        = 1<<14,
	* BSS_CHANGED_SSID        = 1<<15,
	* BSS_CHANGED_AP_PROBE_RESP   = 1<<16,
	* BSS_CHANGED_PS          = 1<<17,
	* BSS_CHANGED_TXPOWER     = 1<<18,
	* BSS_CHANGED_P2P_PS      = 1<<19,
	* BSS_CHANGED_BEACON_INFO     = 1<<20,
	* BSS_CHANGED_BANDWIDTH       = 1<<21,
	* BSS_CHANGED_OCB                 = 1<<22,
	*/
	if (changed & BSS_CHANGED_ASSOC)
		DBGLOG(MESH, INFO, "BSS_CHANGED_ASSOC\n");

	if (changed & BSS_CHANGED_BEACON) {
		DBGLOG(MESH, INFO, "BSS_CHANGED_BEACON\n");
		mtk_mesh_mac80211_mbss_beacon_update(hw, vif);
	}

	if (changed & BSS_CHANGED_BEACON_ENABLED) {
		DBGLOG(MESH, INFO, "BSS_CHANGED_BEACON_ENABLED: %d\n", bss_conf->enable_beacon);
		if (vif->type == NL80211_IFTYPE_MESH_POINT) {
			if (bss_conf->enable_beacon)
				mtk_mesh_mac80211_mbss_beacon_enable(hw, vif);
			else
				mtk_mesh_mac80211_mbss_beacon_disable(hw, vif);
		}
	}

	if (changed & BSS_CHANGED_ERP_SLOT) {
		DBGLOG(MESH, INFO, "BSS_CHANGED_ERP_SLOT\n");
	}
}

int mtk_mesh_mac80211_set_key(struct ieee80211_hw *hw,
					  enum set_key_cmd cmd,
					  struct ieee80211_vif *vif,
					  struct ieee80211_sta *sta,
					  struct ieee80211_key_conf *key)
{
	DBGLOG(MESH, INFO, "mtk_mesh_mac80211_set_key MAC Address = %02x:%02x:%02x:%02x:%02x:%02x\n"
		, sta->addr[5], sta->addr[4], sta->addr[3], sta->addr[2], sta->addr[1], sta->addr[0]);
	return 0;
}

int mtk_mesh_mac80211_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan)
{
	return 0;
}

UINT_64 mtk_mesh_mac80211_get_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	return 0;
}

int mtk_mesh_add_chanctx(struct ieee80211_hw *hw, struct ieee80211_chanctx_conf *ctx)
{
	P_GLUE_INFO_T prGlueInfo = hw->priv;
	UINT_8 ret = 0;
	P_MESH_CONNECTION_SETTINGS_T prConnSettings = NULL;
	P_ADAPTER_T prAdapter = prGlueInfo->prAdapter;

	MTK_CHANCTX_PRIV_T **ptr = NULL;

	ASSERT(prGlueInfo);
	ASSERT(prAdapter);

	prConnSettings = prAdapter->rWifiVar.prMeshConnSettings;
	DBGLOG(MESH, INFO, "add channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n",
		ctx->def.chan->center_freq, ctx->def.width,
		ctx->def.center_freq1, ctx->def.center_freq2);
	/* FIXME: check current usage of channel only one operate channel */
#if 0
	ret = wlanModifyChanctx(&prGlueInfo->rmtk_chanctx_priv, ctx->def.chan->center_freq, 1, 1);
	if (ret != 0)
		return -ENOSPC;
#endif
	ptr = (void *) ctx->drv_priv;
	*ptr = &prGlueInfo->rmtk_chanctx_priv;
	/* Set the Channel From Chandef */
	getMeshChan(&ctx->def, &prConnSettings->ucAdHocChannelNum
		, &prConnSettings->eAdHocBand, &prConnSettings->eChnlSco);
	(*ptr)->magic = 0xdeadbabe;
	return ret;
}

void mtk_mesh_remove_chanctx(struct ieee80211_hw *hw, struct ieee80211_chanctx_conf *ctx)
{
	P_GLUE_INFO_T prGlueInfo = hw->priv;
	MTK_CHANCTX_PRIV_T **ptr = (void *) ctx->drv_priv;

	ASSERT(prGlueInfo);

	DBGLOG(MESH, INFO, "Remove channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n",
		ctx->def.chan->center_freq, ctx->def.width,
		ctx->def.center_freq1, ctx->def.center_freq2);
	/* FIXME: check current usage of channel only one operate channel */
	/* wlanModifyChanctx((MTK_CHANCTX_PRIV_T *)(*ptr), ctx->def.chan->center_freq, 1, 0); */
	(*ptr)->magic = 0;
	*ptr = NULL;
}

void mtk_mesh_change_chanctx(struct ieee80211_hw *hw, struct ieee80211_chanctx_conf *ctx, u32 changed)
{
	DBGLOG(MESH, INFO, "change channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n"
		, ctx->def.chan->center_freq, ctx->def.width
		, ctx->def.center_freq1, ctx->def.center_freq2);
}

int mtk_mesh_assign_vif_chanctx(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_chanctx_conf *ctx)
{
	DBGLOG(MESH, INFO, "assgin channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n"
		, ctx->def.chan->center_freq, ctx->def.width
		, ctx->def.center_freq1, ctx->def.center_freq2);
	return 0;
}


void mtk_mesh_unassign_vif_chanctx(struct ieee80211_hw *hw,
						struct ieee80211_vif *vif,
						struct ieee80211_chanctx_conf *ctx)
{
	DBGLOG(MESH, INFO, "unassgin channel context control: %d MHz/width: %d/cfreqs:%d/%d MHz\n"
		, ctx->def.chan->center_freq, ctx->def.width
		, ctx->def.center_freq1, ctx->def.center_freq2);
}

int mtk_mesh_mac80211_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set)
{
	return 0;
}

static void mtk_mesh_mac80211_hw_scan_work(struct work_struct *work)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	struct ieee80211_hw *hw;
	unsigned int rStatus = 0;
	UINT_32 u4BufLen;

	prMeshInfo = container_of(work, struct _GL_MESH_INFO_T, hw_scan.work);
	ASSERT(prMeshInfo);
	hw = prMeshInfo->hw;
	prGlueInfo = hw->priv;
	ASSERT(prGlueInfo);

	DBGLOG(MESH, INFO, "Scan work...\n");
	rStatus = kalIoctl(prGlueInfo,
		wlanoidSetBssidListScanExtMesh,
		&prMeshInfo->rScanRequest,
		sizeof(PARAM_SCAN_REQUEST_EXT_T),
		FALSE,
		FALSE,
		FALSE,
		&u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, INFO, "scan error:%x\n", rStatus);
}

VOID getMeshChan(struct cfg80211_chan_def *chandef, UINT_8 *channum, P_ENUM_BAND_T band, ENUM_CHNL_EXT_T *prChnlSco)
{
	struct ieee80211_channel *channel = chandef->chan;

	if (chandef == NULL)
		return;

	channel = chandef->chan;
	if (channel == NULL)
		return;

	DBGLOG(MESH, INFO, "Apply channel info from upper layer\n");
	/*SCO*/
	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		*prChnlSco = CHNL_EXT_SCN;
		break;
	case NL80211_CHAN_WIDTH_40:
		*prChnlSco = ((chandef->chan->center_freq) > (chandef->center_freq1))?(CHNL_EXT_SCB):(CHNL_EXT_SCA);
		break;
	default:
		/* TODO: handle the rest case */
		*prChnlSco = CHNL_EXT_SCN;
		break;
	}

	switch (channel->band) {
	case IEEE80211_BAND_2GHZ:
		*band = BAND_2G4;
		break;
	case IEEE80211_BAND_5GHZ:
		*band = BAND_5G;
		break;
	default:
		*band = BAND_2G4;
		break;
	}

	*channum = ieee80211_frequency_to_channel(channel->center_freq);
}
#endif
