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
/*! gl_mesh.c
* Main routines of Linux driver interface for Wi-Fi MESH
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

#include "precomp.h"
#include "gl_mesh_mac80211.h"
#include "gl_mesh_os.h"

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
const static struct ieee80211_ops ieee80211_mtk_mesh_ops = {
	.tx			       = mtk_mesh_mac80211_tx,
	.start			       = mtk_mesh_mac80211_start,
	.stop			       = mtk_mesh_mac80211_stop,
	.add_interface		       = mtk_mesh_mac80211_add_interface,
	.change_interface	       = mtk_mesh_mac80211_change_interface,
	.remove_interface	       = mtk_mesh_mac80211_remove_interface,
	.config			       = mtk_mesh_mac80211_config,
	.configure_filter	       = mtk_mesh_mac80211_configure_filter,
	.sta_add		       = mtk_mesh_mac80211_sta_add,
	.sta_remove		       = mtk_mesh_mac80211_sta_remove,
	.sta_notify		       = mtk_mesh_mac80211_sta_notify,
	.hw_scan		       = mtk_mesh_mac80211_hw_scan,
	.cancel_hw_scan		       = mtk_mesh_mac80211_cancel_hw_scan,
	.set_rts_threshold	       = mtk_mesh_mac80211_set_rts_threshold,
	.bss_info_changed	       = mtk_mesh_mac80211_bss_info_changed,
	.set_key		       = mtk_mesh_mac80211_set_key,
	.suspend		       = mtk_mesh_mac80211_suspend,
	.get_tsf		       = mtk_mesh_mac80211_get_tsf,
	.add_chanctx		       = mtk_mesh_add_chanctx,
	.remove_chanctx		       = mtk_mesh_remove_chanctx,
	.change_chanctx		       = mtk_mesh_change_chanctx,
	.assign_vif_chanctx	       = mtk_mesh_assign_vif_chanctx,
	.unassign_vif_chanctx	       = mtk_mesh_unassign_vif_chanctx,
	.set_tim		       = mtk_mesh_mac80211_set_tim,
	};

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

/*----------------------------------------------------------------------------*/
/*!
* \brief Allocate memory for MESHINFO, GL_MESH_INFO_T, MESH_CONNECTION_SETTINGS
*                                          MESH_SPECIFIC_BSS_INFO, MESH_FSM_INFO
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
#if CFG_ENABLE_WIFI_MESH
static BOOLEAN meshAllocInfo(IN P_GLUE_INFO_T prGlueInfo)
{
	P_ADAPTER_T prAdapter = NULL;
	P_WIFI_VAR_T prWifiVar = NULL;

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	prWifiVar = &(prAdapter->rWifiVar);

	ASSERT(prAdapter);
	ASSERT(prWifiVar);

	do {
		if (prGlueInfo == NULL)
			break;

		if (prGlueInfo->prMeshInfo == NULL) {
			/* alloc mem for mesh info */
			prGlueInfo->prMeshInfo = kalMemAlloc(sizeof(GL_MESH_INFO_T), VIR_MEM_TYPE);
			prAdapter->prMeshInfo = kalMemAlloc(sizeof(MESH_INFO_T), VIR_MEM_TYPE);
			prWifiVar->prMeshConnSettings = kalMemAlloc(sizeof(MESH_CONNECTION_SETTINGS_T), VIR_MEM_TYPE);
			prWifiVar->prMeshSpecificBssInfo = kalMemAlloc(sizeof(MESH_SPECIFIC_BSS_INFO_T), VIR_MEM_TYPE);
		} else {
			ASSERT(prAdapter->prMeshInfo != NULL);
			ASSERT(prWifiVar->prMeshConnSettings != NULL);
			ASSERT(prWifiVar->prMeshSpecificBssInfo != NULL);
		}
		/*MUST set memory to 0 */
		kalMemZero(prGlueInfo->prMeshInfo, sizeof(GL_MESH_INFO_T));
		kalMemZero(prAdapter->prMeshInfo, sizeof(MESH_INFO_T));
		kalMemZero(prWifiVar->prMeshConnSettings, sizeof(MESH_CONNECTION_SETTINGS_T));
		kalMemZero(prWifiVar->prMeshSpecificBssInfo, sizeof(MESH_SPECIFIC_BSS_INFO_T));
	} while (FALSE);

	/* chk if alloc successful or not*/
	if (prGlueInfo->prMeshInfo && prAdapter->prMeshInfo && prWifiVar->prMeshConnSettings &&
	    prWifiVar->prMeshSpecificBssInfo)
		return TRUE;

	/* free mem because alloc fail */
	if (prWifiVar->prMeshSpecificBssInfo) {
		kalMemFree(prWifiVar->prMeshSpecificBssInfo, VIR_MEM_TYPE, sizeof(MESH_SPECIFIC_BSS_INFO_T));
		prWifiVar->prMeshSpecificBssInfo = NULL;
	}

	if (prWifiVar->prMeshConnSettings) {
		kalMemFree(prWifiVar->prMeshConnSettings, VIR_MEM_TYPE, sizeof(MESH_CONNECTION_SETTINGS_T));
		prWifiVar->prMeshConnSettings = NULL;
	}

	if (prAdapter->prMeshInfo) {
		kalMemFree(prGlueInfo->prMeshInfo, VIR_MEM_TYPE, sizeof(GL_MESH_INFO_T));
		prGlueInfo->prMeshInfo = NULL;
	}

	if (prGlueInfo->prMeshInfo) {
		kalMemFree(prAdapter->prMeshInfo, VIR_MEM_TYPE, sizeof(MESH_INFO_T));
		prAdapter->prMeshInfo = NULL;
	}
	return FALSE;
}
/*----------------------------------------------------------------------------*/
/*!
* \brief Free memory for prMESHInfo
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
static BOOLEAN meshFreeInfo(P_GLUE_INFO_T prGlueInfo)
{
	BOOLEAN ret = FALSE;

	ASSERT(prGlueInfo);
	ASSERT(prGlueInfo->prAdapter);

	/* free memory after mesh module is ALREADY unregistered */
	if (prGlueInfo->prAdapter->fgIsMESHRegistered == FALSE) {
		kalMemFree(prGlueInfo->prAdapter->prMeshInfo, VIR_MEM_TYPE, sizeof(MESH_INFO_T));
		kalMemFree(prGlueInfo->prMeshInfo, VIR_MEM_TYPE, sizeof(GL_MESH_INFO_T));
		kalMemFree(prGlueInfo->prAdapter->rWifiVar.prMeshConnSettings, VIR_MEM_TYPE,
			sizeof(MESH_CONNECTION_SETTINGS_T));
		kalMemFree(prGlueInfo->prAdapter->rWifiVar.prMeshSpecificBssInfo, VIR_MEM_TYPE,
			sizeof(MESH_SPECIFIC_BSS_INFO_T));

		/*reset all pointer to NULL */
		prGlueInfo->prMeshInfo = NULL;
		prGlueInfo->prAdapter->prMeshInfo = NULL;
		prGlueInfo->prAdapter->rWifiVar.prMeshConnSettings = NULL;
		prGlueInfo->prAdapter->rWifiVar.prMeshSpecificBssInfo = NULL;

		ret = TRUE;
	} else {
		DBGLOG(MESH, ERROR, "meshFreeInfo fail due to mesh still registered\n");
		ret = FALSE;
	}

	return ret;

}

static const struct ieee80211_iface_limit mtk_if_limits[] = {
	{ .max = 1,
	  .types =   BIT(NL80211_IFTYPE_MESH_POINT)
	},
};

/* check in static int wiphy_verify_combinations
 * @limits: limits for the given interface types
 * @n_limits: number of limitations
 * @num_different_channels: can use up to this many different channels
 * @max_interfaces: maximum number of interfaces in total allowed in this group
 * @beacon_int_infra_match: In this combination, the beacon intervals between
 * infrastructure and AP types must match. This is required only in special cases.
 * @radar_detect_widths: bitmap of channel widths supported for radar detection
 */
static const struct ieee80211_iface_combination mtk_if_comb[] = {
	{
		.limits = mtk_if_limits,
		.n_limits = ARRAY_SIZE(mtk_if_limits),
		 /* Combinations with just one interface aren't real */
		.max_interfaces = 1,
		/* Need at least one channel */
		.num_different_channels = 1,
		.beacon_int_infra_match = true,
#if 0
		.radar_detect_widths =  BIT(NL80211_CHAN_WIDTH_20_NOHT) |
								BIT(NL80211_CHAN_WIDTH_20) |
								BIT(NL80211_CHAN_WIDTH_40) |
								BIT(NL80211_CHAN_WIDTH_80),
#endif
	},
};

/*----------------------------------------------------------------------------*/
/*!
* \brief Register Net Device for Wi-Fi MESH
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
INT_8 glRegisterMESH(P_GLUE_INFO_T prGlueInfo)
{
	P_ADAPTER_T prAdapter = NULL;
	P_GL_HIF_INFO_T prHif = NULL;
	P_GL_MESH_INFO_T prMeshInfo = NULL;
	P_BSS_INFO_T prMeshBssInfo = (P_BSS_INFO_T)NULL;
	struct ieee80211_hw *hw;
	int ret = 0;

	ASSERT(prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	ASSERT(prAdapter);

	prHif = &prGlueInfo->rHifInfo;
	ASSERT(prHif);

	/*1. allocate meshinfo */
	if (!meshAllocInfo(prGlueInfo)) {
		DBGLOG(MESH, ERROR, "Mesh Allocate Info Fail...\n");
		return -ENOMEM;
	}

	/* TODO: related to passive scan time tmp init, set by iwpriv in benten */
	prAdapter->prMeshInfo->u2MeshPassiveDwellTime = 100;
	prMeshInfo = prGlueInfo->prMeshInfo;

	/* 2. allocate ieee80211_hw(wiphy ->ieee80211_local->ieee80211_hw) */
	hw = ieee80211_alloc_hw(sizeof(P_GLUE_INFO_T), &ieee80211_mtk_mesh_ops);
	if (!hw) {
		DBGLOG(MESH, ERROR, "ieee80211_alloc_hw...fail\n");
		return -ENOMEM;
	}

	/* 2.1 fill hw and wiphy parameters */
	DBGLOG(MESH, INFO, "Fill ieee80211_hw and wiphy\n");
	hw->priv = prGlueInfo;
	prGlueInfo->prMeshInfo->hw = hw;

	/* TODO::may need to check MBSS NETWORK TYPE. its for multiple interface */
	/* prMeshBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_MBSS, false); */
	prMeshBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_MESH, false);
	prMeshInfo->ucBssIndex = prMeshBssInfo->ucBssIndex;
	/* set address */
	COPY_MAC_ADDR(prMeshBssInfo->aucOwnMacAddr, prAdapter->rMyMacAddr);
	/* may not need replaced by add_interface */
	if (prMeshBssInfo->aucOwnMacAddr[0] & 0x4)
		prMeshBssInfo->aucOwnMacAddr[0] &= ~0x4;
	else
		prMeshBssInfo->aucOwnMacAddr[0] |= 0x4;

	hw->wiphy->n_addresses = 1;
	SET_IEEE80211_PERM_ADDR(hw, prMeshBssInfo->aucOwnMacAddr);
	/* set band */
	hw->wiphy->bands[IEEE80211_BAND_2GHZ] = &mtk_band_2ghz;
	hw->wiphy->bands[IEEE80211_BAND_5GHZ] = &mtk_band_5ghz;
	/* set interface mode */
	hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_MESH_POINT);
	/* set hw flag */
	set_bit(IEEE80211_HW_HAS_RATE_CONTROL, hw->flags);
#if 0	/* FIXME check capability after */
	set_bit(IEEE80211_HW_QUEUE_CONTROL, hw->flags);
	set_bit(IEEE80211_HW_SUPPORTS_PER_STA_GTK, hw->flags);
	set_bit(IEEE80211_HW_SUPPORTS_HT_CCK_RATES, hw->flags);
				hw->flags =  IEEE80211_HW_HAS_RATE_CONTROL |
				/* Indicates that received frames passed to the stack include the FCS at the end. */
				IEEE80211_HW_RX_INCLUDES_FCS|
				IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING|
				IEEE80211_HW_SIGNAL_UNSPEC|
				/* This device needs to get data from beacon before association (i.e.dtim_period) */
				IEEE80211_HW_NEED_DTIM_BEFORE_ASSOC|
				/* Hardware supports spectrum management defined in 802.11h */
				IEEE80211_HW_SPECTRUM_MGMT|
				/* Hardware requires nullfunc frame handling in stack, implies
				 * stack support for dynamic PS.
				 */
				IEEE80211_HW_PS_NULLFUNC_STACK|
				IEEE80211_HW_WANT_MONITOR_VIF|
				IEEE80211_HW_NO_AUTO_VIF|
				IEEE80211_HW_SW_CRYPTO_CONTROL|
				IEEE80211_HW_SUPPORT_FAST_XMIT|
				IEEE80211_HW_AP_LINK_PS|
				IEEE80211_HW_SUPPORTS_RC_TABLE|
				IEEE80211_HW_P2P_DEV_ADDR_FOR_INTF|
				IEEE80211_HW_TIMING_BEACON_ONLY|
				IEEE80211_HW_CHANCTX_STA_CSA|
				IEEE80211_HW_SUPPORTS_CLONED_SKBS|
				IEEE80211_HW_SINGLE_SCAN_ON_ALL_BANDS|
				IEEE80211_HW_TDLS_WIDER_BW|
				IEEE80211_HW_SUPPORTS_AMSDU_IN_AMPDU|
				IEEE80211_HW_BEACON_TX_STATUS|
				IEEE80211_HW_SIGNAL_DBM|
				IEEE80211_HW_AMPDU_AGGREGATION|
				IEEE80211_HW_SUPPORTS_PS|
				IEEE80211_HW_SUPPORTS_DYNAMIC_PS|
				IEEE80211_HW_MFP_CAPABLE|
				IEEE80211_HW_REPORTS_TX_ACK_STATUS|
				IEEE80211_HW_CONNECTION_MONITOR|
				/* The driver wants to control per-interface
				 * queue mapping in order to use different queues (not just one per AC)
				 * for different virtual interfaces
				 * related to hw->offchannel_tx_hw_queue;
				 */
				IEEE80211_HW_QUEUE_CONTROL|
				IEEE80211_HW_SUPPORTS_PER_STA_GTK|
				IEEE80211_HW_TX_AMPDU_SETUP_IN_HW|
				IEEE80211_HW_SUPPORTS_HT_CCK_RATES;
#endif
#if 0 /* special for benten? */
	hw->flags2 = IEEE80211_HW_CLOCK_SYNC |
				IEEE80211_HW_MESH_BEACON_RX_FILTERING |
				IEEE80211_HW_PROBE_RESPONSE_OFFLOAD |
				IEEE80211_HW_MESH_RADIO_POWER_SAVE;
#endif
	/* set scan cap */
	hw->wiphy->max_scan_ssids = 1;    /* FIXME: for combo scan */
	hw->wiphy->max_scan_ie_len = 512;
	hw->vif_data_size = sizeof(struct mtk_vif_priv);
	hw->sta_data_size = sizeof(struct mtk_sta_priv);
	hw->chanctx_data_size = sizeof(void *);
	/* set hw queue FIXME */
	hw->queues = 4;
	/* @offchannel_tx_hw_queue: HW queue ID to use for offchannel TX
	 * (if IEEE80211_HW_QUEUE_CONTROL is set)
	 */
	hw->offchannel_tx_hw_queue = 3;
	/* set if combination */

	hw->wiphy->interface_modes = (
		BIT(NL80211_IFTYPE_STATION) |
		BIT(NL80211_IFTYPE_P2P_GO) |
		BIT(NL80211_IFTYPE_P2P_CLIENT) |
		BIT(NL80211_IFTYPE_MESH_POINT));

	hw->wiphy->iface_combinations = mtk_if_comb;
	hw->wiphy->n_iface_combinations = ARRAY_SIZE(mtk_if_comb);


	hw->wiphy->iface_combinations = NULL; // mtk_if_comb;
	hw->wiphy->n_iface_combinations = 0; //ARRAY_SIZE(mtk_if_comb);
	/* set reg */
	hw->wiphy->regulatory_flags = REGULATORY_CUSTOM_REG;
	/* set chipher suit */
	/* 3. register ieee80211 hw */
	DBGLOG(MESH, INFO, "Call ieee80211_register_hw\n");
	/* add extra tx headrrom for data */
	hw->extra_tx_headroom = NIC_TX_HEAD_ROOM;
	ret = ieee80211_register_hw(hw);
	if (ret < 0) {
		DBGLOG(MESH, ERROR, "ieee80211_register_hw fail... ret %x\n", ret);
		ieee80211_free_hw(hw);
	}
	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Unregister Net Device for Wi-Fi MESH
*
* \param[in] prGlueInfo      Pointer to glue info
*
* \return   TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN glUnregisterMESH(P_GLUE_INFO_T prGlueInfo)
{
	P_GL_MESH_INFO_T prMeshInfo = NULL;

	ASSERT(prGlueInfo);
	prMeshInfo = prGlueInfo->prMeshInfo;
	ieee80211_unregister_hw(prMeshInfo->hw);
	ieee80211_free_hw(prMeshInfo->hw);
	meshFreeInfo(prGlueInfo);
	return TRUE;
}

BOOLEAN secEnabledInMesh(IN P_ADAPTER_T prAdapter)
{
	P_MESH_CONNECTION_SETTINGS_T prConnSettings;

	prConnSettings = prAdapter->rWifiVar.prMeshConnSettings;

	DBGLOG(MESH, INFO, "#### DBG %s, EncStatus %x>\n", __func__, prConnSettings->eEncStatus);
	switch (prConnSettings->eEncStatus) {
	case ENUM_ENCRYPTION_DISABLED:
		return FALSE;
	case ENUM_ENCRYPTION1_ENABLED:
	case ENUM_ENCRYPTION2_ENABLED:
	case ENUM_ENCRYPTION3_ENABLED:
		return TRUE;
	default:
		DBGLOG(MESH, INFO, "Unknown encryption setting %d\n", prAdapter->rWifiVar.rConnSettings.eEncStatus);
		break;
	}
	return FALSE;

}		/* secEnabledInAis */
#endif

