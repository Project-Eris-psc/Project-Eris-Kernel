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

#ifndef _GL_MESH_MAC80211_H
#define _GL_MESH_MAC80211_H

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

/*******************************************************************************
*                    E X T E R N A L   V A R I A B L E
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/*Remove FIF_PROMISC_IN_BSS beacuse 4.4 does not support */
#define MESH_SUPPORTED_FILTERS	\
	(FIF_ALLMULTI |				\
	FIF_CONTROL |				\
	FIF_PSPOLL |				\
	FIF_OTHER_BSS |				\
	FIF_BCN_PRBRESP_PROMISC |	\
	FIF_PROBE_REQ |				\
	FIF_FCSFAIL)
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _MESH_SCAN_IES {
	UINT_16 u2CommonIeLen;
	UINT_16 u2Band2G4IeLen;
	UINT_16 u2Band5GIeLen;
	UINT_8 ie[1];
} MESH_SCAN_IES, *P_MESH_SCAN_IES;
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
/* mac80211 hooks */
void mtk_mesh_mac80211_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *control, struct sk_buff *skb);

int mtk_mesh_mac80211_start(struct ieee80211_hw *hw);

void mtk_mesh_mac80211_stop(struct ieee80211_hw *hw);

int mtk_mesh_mac80211_add_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif);

int mtk_mesh_mac80211_change_interface(struct ieee80211_hw *hw,
						       struct ieee80211_vif *vif,
						       enum nl80211_iftype newtype,
						       bool p2p);

void mtk_mesh_mac80211_remove_interface(struct ieee80211_hw *hw, struct ieee80211_vif *vif);

int mtk_mesh_mac80211_config(struct ieee80211_hw *hw, u32 changed);

void mtk_mesh_mac80211_configure_filter(struct ieee80211_hw *hw,
						      unsigned int changed_flags,
						      unsigned int *total_flags,
						      u64 multicast);

int mtk_mesh_mac80211_sta_add(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif,
					   struct ieee80211_sta *sta);

int mtk_mesh_mac80211_sta_remove(struct ieee80211_hw *hw,
						struct ieee80211_vif *vif,
						struct ieee80211_sta *sta);

void mtk_mesh_mac80211_sta_notify(struct ieee80211_hw *hw,
					       struct ieee80211_vif *vif,
					       enum sta_notify_cmd cmd,
					       struct ieee80211_sta *sta);

int mtk_mesh_mac80211_hw_scan(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_scan_request *req);

void  mtk_mesh_mac80211_cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif);

int mtk_mesh_mac80211_set_rts_threshold(struct ieee80211_hw *hw, u32 value);

void mtk_mesh_mac80211_bss_info_changed(struct ieee80211_hw *hw,
							  struct ieee80211_vif *vif,
							  struct ieee80211_bss_conf *bss_conf,
							  u32 changed);

int mtk_mesh_mac80211_set_key(struct ieee80211_hw *hw,
					  enum set_key_cmd cmd,
					  struct ieee80211_vif *vif,
					  struct ieee80211_sta *sta,
					  struct ieee80211_key_conf *key);

int mtk_mesh_mac80211_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan);

UINT_64 mtk_mesh_mac80211_get_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif);

int mtk_mesh_add_chanctx(struct ieee80211_hw *hw, struct ieee80211_chanctx_conf *ctx);

void mtk_mesh_remove_chanctx(struct ieee80211_hw *hw, struct ieee80211_chanctx_conf *ctx);

void mtk_mesh_change_chanctx(struct ieee80211_hw *hw, struct ieee80211_chanctx_conf *ctx, u32 changed);

int mtk_mesh_assign_vif_chanctx(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_chanctx_conf *ctx);

void mtk_mesh_unassign_vif_chanctx(struct ieee80211_hw *hw,
						struct ieee80211_vif *vif,
						struct ieee80211_chanctx_conf *ctx);

int mtk_mesh_mac80211_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set);


/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

#endif
