/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#define LOG_TAG "MMP"

#include "ddp_mmp.h"
#include "ddp_reg.h"
#include "ddp_log.h"
#include "ddp_ovl.h"
#include "ddp_drv.h"

#ifdef CONFIG_MTK_IOMMU
#include <mach/pseudo_m4u.h>
#endif
#include "DpDataType.h"

#include "ddp_debug.h"

static struct DDP_MMP_Events_t DDP_MMP_Events;

void init_ddp_mmp_events(void)
{
	if (DDP_MMP_Events.DDP == 0) {
		DDP_MMP_Events.DDP = mmprofile_register_event(MMP_ROOT_EVENT, "Display");
		DDP_MMP_Events.primary_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "primary_disp");
		DDP_MMP_Events.primary_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "trigger");
		DDP_MMP_Events.primary_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "ovl_config");
		DDP_MMP_Events.primary_rdma_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "rdma_config");
		DDP_MMP_Events.primary_wdma_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "wdma_config");
		DDP_MMP_Events.primary_set_dirty =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "set_dirty");
		DDP_MMP_Events.primary_cmdq_flush =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "cmdq_flush");
		DDP_MMP_Events.primary_cmdq_done =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "cmdq_done");
		DDP_MMP_Events.primary_display_cmd =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "display_io");
		DDP_MMP_Events.primary_suspend =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "suspend");
		DDP_MMP_Events.primary_resume =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "resume");
		DDP_MMP_Events.primary_cache_sync =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "cache_sync");
		DDP_MMP_Events.primary_wakeup =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "wakeup");
		DDP_MMP_Events.interface_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "interface_trigger");
		DDP_MMP_Events.primary_switch_mode =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "switch_session_mode");

		DDP_MMP_Events.primary_seq_insert =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_insert");
		DDP_MMP_Events.primary_seq_config =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_config");
		DDP_MMP_Events.primary_seq_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_trigger");
		DDP_MMP_Events.primary_seq_rdma_irq =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_rdma_irq");
		DDP_MMP_Events.primary_seq_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "seq_release");

		DDP_MMP_Events.primary_ovl_fence_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "ovl_fence_r");
		DDP_MMP_Events.primary_wdma_fence_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "wdma_fence_r");

		DDP_MMP_Events.present_fence_release =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "preset_fence_release");
		DDP_MMP_Events.present_fence_get =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "preset_fence_get");
		DDP_MMP_Events.present_fence_set =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "preset_fence_set");

		DDP_MMP_Events.idlemgr =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "idlemgr");
		DDP_MMP_Events.primary_error =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "primary_error");

#ifdef CONFIG_MTK_HDMI_SUPPORT
		DDP_MMP_Events.Extd_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "ext_disp");
		DDP_MMP_Events.Extd_State =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_State");
		DDP_MMP_Events.Extd_DevInfo =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_DevInfo");
		DDP_MMP_Events.Extd_ErrorInfo =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_ErrorInfo");
		DDP_MMP_Events.Extd_Mutex =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_Mutex");
		DDP_MMP_Events.Extd_ImgDump =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_ImgDump");
		DDP_MMP_Events.Extd_IrqStatus =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_IrqStatus");
		DDP_MMP_Events.Extd_UsedBuff =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_UsedBuf");
		DDP_MMP_Events.Extd_trigger =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_trigger");
		DDP_MMP_Events.Extd_config =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_config");
		DDP_MMP_Events.Extd_set_dirty =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_set_dirty");
		DDP_MMP_Events.Extd_cmdq_flush =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_cmdq_flush");
		DDP_MMP_Events.Extd_cmdq_done =
		    mmprofile_register_event(DDP_MMP_Events.Extd_Parent, "ext_cmdq_done");

#endif

		DDP_MMP_Events.primary_display_aalod_trigger =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "primary_aal_trigger");
		DDP_MMP_Events.ESD_Parent = mmprofile_register_event(DDP_MMP_Events.DDP, "ESD");
		DDP_MMP_Events.esd_check_t =
		    mmprofile_register_event(DDP_MMP_Events.ESD_Parent, "ESD_Check");
		DDP_MMP_Events.esd_recovery_t =
		    mmprofile_register_event(DDP_MMP_Events.ESD_Parent, "ESD_Recovery");
		DDP_MMP_Events.esd_extte =
		    mmprofile_register_event(DDP_MMP_Events.esd_check_t, "ESD_Check_EXT_TE");
		DDP_MMP_Events.esd_rdlcm =
		    mmprofile_register_event(DDP_MMP_Events.esd_check_t, "ESD_Check_RD_LCM");
		DDP_MMP_Events.esd_vdo_eint =
		    mmprofile_register_event(DDP_MMP_Events.esd_extte, "ESD_Vdo_EINT");
		DDP_MMP_Events.primary_set_bl =
		    mmprofile_register_event(DDP_MMP_Events.primary_Parent, "set_BL_LCM");
		DDP_MMP_Events.dprec_cpu_write_reg = DDP_MMP_Events.MutexParent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "dprec_cpu_write_reg");
		DDP_MMP_Events.session_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "session");

		DDP_MMP_Events.ovl_trigger =
		    mmprofile_register_event(DDP_MMP_Events.session_Parent, "ovl2mem");
		DDP_MMP_Events.layerParent = mmprofile_register_event(DDP_MMP_Events.DDP, "Layer");
		DDP_MMP_Events.layer[0] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer0");
		DDP_MMP_Events.layer[1] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer1");
		DDP_MMP_Events.layer[2] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer2");
		DDP_MMP_Events.layer[3] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Layer3");

		DDP_MMP_Events.ovl1_layer[0] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer0");
		DDP_MMP_Events.ovl1_layer[1] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer1");
		DDP_MMP_Events.ovl1_layer[2] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer2");
		DDP_MMP_Events.ovl1_layer[3] =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "Ovl1_Layer3");

		DDP_MMP_Events.layer_dump_parent =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "layerBmpDump");
		DDP_MMP_Events.layer_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "layer0_dump");
		DDP_MMP_Events.layer_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "Layer1_dump");
		DDP_MMP_Events.layer_dump[2] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "Layer2_dump");
		DDP_MMP_Events.layer_dump[3] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "Layer3_dump");

		DDP_MMP_Events.ovl1layer_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer0_dump");
		DDP_MMP_Events.ovl1layer_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer1_dump");
		DDP_MMP_Events.ovl1layer_dump[2] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer2_dump");
		DDP_MMP_Events.ovl1layer_dump[3] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "ovl1layer3_dump");

		DDP_MMP_Events.wdma_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "wdma0_dump");
		DDP_MMP_Events.wdma_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "wdma1_dump");

		DDP_MMP_Events.rdma_dump[0] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "rdma0_dump");
		DDP_MMP_Events.rdma_dump[1] =
		    mmprofile_register_event(DDP_MMP_Events.layer_dump_parent, "rdma1_dump");

		DDP_MMP_Events.ovl_enable =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "ovl_enable_config");
		DDP_MMP_Events.ovl_disable =
		    mmprofile_register_event(DDP_MMP_Events.layerParent, "ovl_disable_config");
		DDP_MMP_Events.cascade_enable =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "cascade_enable");
		DDP_MMP_Events.cascade_disable =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "cascade_disable");
		DDP_MMP_Events.ovl1_status =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "ovl1_status");
		DDP_MMP_Events.dpmgr_wait_event_timeout =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "wait_event_timeout");
		DDP_MMP_Events.cmdq_rebuild =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "cmdq_rebuild");
		DDP_MMP_Events.dsi_te = mmprofile_register_event(DDP_MMP_Events.DDP, "dsi_te");

		DDP_MMP_Events.DDP_IRQ = mmprofile_register_event(DDP_MMP_Events.DDP, "DDP_IRQ");
		DDP_MMP_Events.MutexParent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "Mutex");
		DDP_MMP_Events.MUTEX_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex0");
		DDP_MMP_Events.MUTEX_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex1");
		DDP_MMP_Events.MUTEX_IRQ[2] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex2");
		DDP_MMP_Events.MUTEX_IRQ[3] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex3");
		DDP_MMP_Events.MUTEX_IRQ[4] =
		    mmprofile_register_event(DDP_MMP_Events.MutexParent, "Mutex4");
		DDP_MMP_Events.OVL_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "OVL_IRQ");
		DDP_MMP_Events.OVL_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.OVL_IRQ_Parent, "OVL_IRQ_0");
		DDP_MMP_Events.OVL_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.OVL_IRQ_Parent, "OVL_IRQ_1");
		DDP_MMP_Events.WDMA_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "WDMA_IRQ");
		DDP_MMP_Events.WDMA_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.WDMA_IRQ_Parent, "WDMA_IRQ_0");
		DDP_MMP_Events.WDMA_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.WDMA_IRQ_Parent, "WDMA_IRQ_1");
		DDP_MMP_Events.RDMA_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "RDMA_IRQ");
		DDP_MMP_Events.RDMA_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_0");
		DDP_MMP_Events.RDMA_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_1");
		DDP_MMP_Events.RDMA_IRQ[2] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "RDMA_IRQ_2");
		DDP_MMP_Events.ddp_abnormal_irq =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "ddp_abnormal_irq");
		DDP_MMP_Events.SCREEN_UPDATE[0] =
		    mmprofile_register_event(DDP_MMP_Events.session_Parent, "SCREEN_UPDATE_0");
		DDP_MMP_Events.SCREEN_UPDATE[1] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "SCREEN_UPDATE_1");
		DDP_MMP_Events.SCREEN_UPDATE[2] =
		    mmprofile_register_event(DDP_MMP_Events.RDMA_IRQ_Parent, "SCREEN_UPDATE_2");
		DDP_MMP_Events.DSI_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "DSI_IRQ");
		DDP_MMP_Events.DSI_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_0");
		DDP_MMP_Events.DSI_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_1");
		DDP_MMP_Events.primary_sw_mutex =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "primary_sw_mutex");
		DDP_MMP_Events.session_release =
		    mmprofile_register_event(DDP_MMP_Events.session_Parent, "session_release");

		DDP_MMP_Events.MonitorParent =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "Monitor");
		DDP_MMP_Events.trigger_delay =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Trigger Delay");
		DDP_MMP_Events.release_delay =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Release Delay");
		DDP_MMP_Events.cg_mode =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "SPM CG Mode");
		DDP_MMP_Events.power_down_mode =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "SPM Power Down Mode");
		DDP_MMP_Events.sodi_disable =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Request CG");
		DDP_MMP_Events.sodi_enable =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Request Power Down");
		DDP_MMP_Events.vsync_count =
		    mmprofile_register_event(DDP_MMP_Events.MonitorParent, "Vsync Ticket");

		DDP_MMP_Events.dal_clean =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "DAL Clean");
		DDP_MMP_Events.dal_printf =
		    mmprofile_register_event(DDP_MMP_Events.DDP, "DAL Printf");


		DDP_MMP_Events.DSI_IRQ_Parent =
		    mmprofile_register_event(DDP_MMP_Events.DDP_IRQ, "DSI_IRQ");
		DDP_MMP_Events.DSI_IRQ[0] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_0");
		DDP_MMP_Events.DSI_IRQ[1] =
		    mmprofile_register_event(DDP_MMP_Events.DSI_IRQ_Parent, "DSI_IRQ_1");

		mmprofile_enable_event_recursive(DDP_MMP_Events.DDP, 1);
		mmprofile_enable_event_recursive(DDP_MMP_Events.layerParent, 1);
		mmprofile_enable_event_recursive(DDP_MMP_Events.MutexParent, 1);
		mmprofile_enable_event_recursive(DDP_MMP_Events.DDP_IRQ, 1);

		mmprofile_enable_event(DDP_MMP_Events.primary_sw_mutex, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_insert, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_config, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_trigger, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_rdma_irq, 0);
		mmprofile_enable_event(DDP_MMP_Events.primary_seq_release, 0);
	}
}

void _ddp_mmp_ovl_not_raw_under_session(unsigned int session, struct OVL_CONFIG_STRUCT *pLayer,
					mmp_metadata_bitmap_t *Bitmap)
{

	if (session == 1) {
		if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY
			|| ovl_get_status() == DDP_OVL1_STATUS_SUB_REQUESTING) {	/* 8+0 */
			if (pLayer->layer < 4)
				mmprofile_log_meta_bitmap(DDP_MMP_Events.
							  ovl1layer_dump[pLayer->layer],
							  MMPROFILE_FLAG_PULSE, Bitmap);
			else if (pLayer->layer >= 4 && pLayer->layer < 8)
				mmprofile_log_meta_bitmap(DDP_MMP_Events.
							  layer_dump[pLayer->layer % 4],
							  MMPROFILE_FLAG_PULSE, Bitmap);
		} else {
			mmprofile_log_meta_bitmap(DDP_MMP_Events.layer_dump[pLayer->layer % 4],
						  MMPROFILE_FLAG_PULSE, Bitmap);
		}
	} else if (session == 2) {
		mmprofile_log_meta_bitmap(DDP_MMP_Events.ovl1layer_dump[pLayer->layer],
					  MMPROFILE_FLAG_PULSE, Bitmap);
	}
}

void _ddp_mmp_ovl_raw_under_session(unsigned int session, struct OVL_CONFIG_STRUCT *pLayer,
				    mmp_metadata_t *meta)
{
	if (session == 1) {
		if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY
			|| ovl_get_status() == DDP_OVL1_STATUS_SUB_REQUESTING) {	/* 8+0 */
			if (pLayer->layer < 4)
				mmprofile_log_meta(DDP_MMP_Events.ovl1layer_dump[pLayer->layer],
						   MMPROFILE_FLAG_PULSE, meta);
			else if (pLayer->layer >= 4 && pLayer->layer < 8)
				mmprofile_log_meta(DDP_MMP_Events.layer_dump[pLayer->layer % 4],
						   MMPROFILE_FLAG_PULSE, meta);

		} else {
			mmprofile_log_meta(DDP_MMP_Events.layer_dump[pLayer->layer % 4],
					   MMPROFILE_FLAG_PULSE, meta);
		}
	} else if (session == 2) {
		mmprofile_log_meta(DDP_MMP_Events.ovl1layer_dump[pLayer->layer],
				   MMPROFILE_FLAG_PULSE, meta);
	}
}

#ifdef M4U_SUPPORT_MAP
void *ddp_map_mva_to_va(unsigned long mva, unsigned int size)
{
	void *va = NULL;
#ifdef CONFIG_MTK_IOMMU
	m4u_buf_info_t *pMvaInfo;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j, k, ret = 0;
	struct page **pages;
	unsigned int page_num;
	void *kernel_va;
	unsigned int kernel_size;

	pMvaInfo = m4u_find_buf(mva);
	if (!pMvaInfo || pMvaInfo->size < size) {
		DDPERR("%s cannot find mva: mva=0x%lx, size=0x%x\n", __func__, mva, size);
		if (pMvaInfo)
			DDPERR("pMvaInfo: mva=0x%x, size=0x%x\n", pMvaInfo->mva, pMvaInfo->size);
		return NULL;
	}

	table = pMvaInfo->sg_table;

	page_num = ((((mva) & (PAGE_SIZE - 1)) + (size) + (PAGE_SIZE - 1)) >> 12);
	pages = vmalloc(sizeof(struct page *) * page_num);
	if (pages == NULL) {
		DDPERR("mva_map_kernel:error to vmalloc for %d\n",
		       (unsigned int)sizeof(struct page *) * page_num);
		return NULL;
	}

	k = 0;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page_start;
		int pages_in_this_sg = PAGE_ALIGN(sg_dma_len(sg)) / PAGE_SIZE;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		if (sg_dma_address(sg) == 0)
			pages_in_this_sg = PAGE_ALIGN(sg->length) / PAGE_SIZE;
#endif
		page_start = sg_page(sg);
		for (j = 0; j < pages_in_this_sg; j++) {
			pages[k++] = page_start++;
			if (k >= page_num)
				goto get_pages_done;
		}
	}

get_pages_done:
	if (k < page_num) {
		/* this should not happen, because we have checked the size before. */
		DDPERR("mva_map_kernel:only get %d pages: mva=0x%lx, size=0x%x, pg_num=%d\n", k,
		       mva, size, page_num);
		ret = -1;
		goto error_out;
	}

	kernel_va = 0;
	kernel_size = 0;
	kernel_va = vmap(pages, page_num, VM_MAP, PAGE_KERNEL);
	if (kernel_va == 0 || (unsigned long)kernel_va & 0xfffL) {
		DDPERR("mva_map_kernel:vmap fail: page_num=%d, kernel_va=0x%p\n", page_num,
		       kernel_va);
		ret = -2;
		goto error_out;
	}

	kernel_va += ((unsigned long)mva & (0xfffL));

	va = kernel_va;

error_out:
	vfree(pages);
	DDPERR("mva_map_kernel:mva=0x%lx,size=0x%x,va=0x%p\n", mva, size, va);
#endif
	return va;
}

void ddp_umap_va(void *va)
{
	if (va)
		vunmap((void *)((unsigned long)va & (~0xfff)));
}
#endif

#define MVA_LINE_MAP
#ifdef MVA_LINE_MAP
void *ddp_map_mva_to_va(unsigned long mva, unsigned int size)
{
	void *va = NULL;
	phys_addr_t pa;
#ifdef CONFIG_MTK_IOMMU
	struct device *dev = disp_get_iommu_device();
	struct iommu_domain *domain;
	unsigned int addr;
	unsigned int page_count;
	unsigned int i = 0;
	struct page **pages;

	if (dev) {
		domain = iommu_get_domain_for_dev(dev);
		pa = iommu_iova_to_phys(domain, (dma_addr_t) mva);

		/* line map */
		page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		pages = kmalloc((sizeof(struct page *) * page_count), GFP_KERNEL);
		DDPMSG("page count: %d\n", page_count);
		for (i = 0; i < page_count; i++) {
			addr = pa + i * PAGE_SIZE;
			pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
		}

		va = vmap(pages, page_count, VM_MAP, PAGE_KERNEL);
		DDPMSG("ddp_map_mva_to_va: mva=0x%lx, pa=0x%lx, va=0x%p, size=%d\n",
		       mva, (unsigned long)pa, va, size);
	}
#endif
	return va;
}

void ddp_umap_va(void *va)
{
	if (va)
		vunmap(va);
}
#endif

void ddp_mmp_ovl_layer(struct OVL_CONFIG_STRUCT *pLayer, unsigned int down_sample_x,
		       unsigned int down_sample_y,
		       unsigned int session /*1:primary, 2:external, 3:memory */)
{
	mmp_metadata_bitmap_t Bitmap;
	mmp_metadata_t meta;
	int raw = 0;

	if (pLayer->security == DISP_SECURE_BUFFER)
		return;

	if (session == 1)
		mmprofile_log_ex(DDP_MMP_Events.layer_dump_parent, MMPROFILE_FLAG_START,
				 pLayer->layer, pLayer->layer_en);
	else if (session == 2)
		mmprofile_log_ex(DDP_MMP_Events.Extd_layer_dump_parent, MMPROFILE_FLAG_START,
				 pLayer->layer, pLayer->layer_en);

	if (pLayer->layer_en) {
		Bitmap.data1 = pLayer->vaddr;
		Bitmap.width = pLayer->dst_w;
		Bitmap.height = pLayer->dst_h;
		switch (pLayer->fmt) {
		case eRGB565:
		case eBGR565:
			Bitmap.format = MMPROFILE_BITMAP_RGB565;
			Bitmap.bpp = 16;
			break;
		case eRGB888:
			Bitmap.format = MMPROFILE_BITMAP_RGB888;
			Bitmap.bpp = 24;
			break;
		case eBGRA8888:
			Bitmap.format = MMPROFILE_BITMAP_BGRA8888;
			Bitmap.bpp = 32;
			break;
		case eBGR888:
			Bitmap.format = MMPROFILE_BITMAP_BGR888;
			Bitmap.bpp = 24;
			break;
		case eRGBA8888:
			Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
			Bitmap.bpp = 32;
			break;
		case eABGR8888:
			Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
			Bitmap.bpp = 32;
			break;
		default:
			DDPERR("ddp_mmp_ovl_layer(), unknown fmt=0x%x, %s, dump raw\n", pLayer->fmt,
			       disp_get_fmt_name(pLayer->fmt));
			raw = 1;
		}

		/* added for ARGB but alpha_enable=0 display */
		Bitmap.data2 = pLayer->aen;

		if (!raw) {
			Bitmap.start_pos = 0;
			Bitmap.pitch = pLayer->src_pitch;
			Bitmap.data_size = Bitmap.pitch * Bitmap.height;
			Bitmap.down_sample_x = down_sample_x;
			Bitmap.down_sample_y = down_sample_y;
			Bitmap.p_data = ddp_map_mva_to_va(pLayer->addr, Bitmap.data_size);
			if (Bitmap.p_data) {
				_ddp_mmp_ovl_not_raw_under_session(session, pLayer, &Bitmap);
				ddp_umap_va(Bitmap.p_data);
			} else {
				DDPERR("fail to dump ovl layer%d\n", pLayer->layer);
			}
		} else {
			meta.data_type = MMPROFILE_META_RAW;
			meta.size = pLayer->src_pitch * pLayer->src_h;
			meta.p_data = ddp_map_mva_to_va(pLayer->addr, meta.size);
			if (meta.p_data) {
				_ddp_mmp_ovl_raw_under_session(session, pLayer, &meta);
				ddp_umap_va(meta.p_data);
			} else {
				DDPERR("fail to dump ovl layer%d raw\n", pLayer->layer);
			}
		}
	}

	if (session == 1)
		mmprofile_log_ex(DDP_MMP_Events.layer_dump_parent, MMPROFILE_FLAG_END,
				 pLayer->fmt, pLayer->addr);
	else if (session == 2)
		mmprofile_log_ex(DDP_MMP_Events.Extd_layer_dump_parent, MMPROFILE_FLAG_END,
				 pLayer->fmt, pLayer->addr);
}

void ddp_mmp_wdma_layer(struct WDMA_CONFIG_STRUCT *wdma_layer, unsigned int wdma_num,
			unsigned int down_sample_x, unsigned int down_sample_y)
{
	mmp_metadata_bitmap_t Bitmap;
	mmp_metadata_t meta;
	int raw = 0;

	if (wdma_num > 1) {
		DDPERR("dprec_mmp_dump_wdma_layer is error %d\n", wdma_num);
		return;
	}

	if (wdma_layer->security == DISP_SECURE_BUFFER)
		return;

	Bitmap.data1 = wdma_layer->dstAddress;
	Bitmap.width = wdma_layer->srcWidth;
	Bitmap.height = wdma_layer->srcHeight;
	switch (wdma_layer->outputFormat) {
	case eRGB565:
	case eBGR565:
		Bitmap.format = MMPROFILE_BITMAP_RGB565;
		Bitmap.bpp = 16;
		break;
	case eRGB888:
		Bitmap.format = MMPROFILE_BITMAP_RGB888;
		Bitmap.bpp = 24;
		break;
	case eBGRA8888:
		Bitmap.format = MMPROFILE_BITMAP_BGRA8888;
		Bitmap.bpp = 32;
		break;
	case eBGR888:
		Bitmap.format = MMPROFILE_BITMAP_BGR888;
		Bitmap.bpp = 24;
		break;
	case eRGBA8888:
		Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
		Bitmap.bpp = 32;
		break;
	default:
		DDPERR("dprec_mmp_dump_wdma_layer(), unknown fmt=%d, dump raw\n",
		       wdma_layer->outputFormat);
		raw = 1;
	}
	if (!raw) {
		Bitmap.start_pos = 0;
		Bitmap.pitch = wdma_layer->dstPitch;
		Bitmap.data_size = Bitmap.pitch * Bitmap.height;
		Bitmap.down_sample_x = down_sample_x;
		Bitmap.down_sample_y = down_sample_y;
		Bitmap.p_data = ddp_map_mva_to_va(wdma_layer->dstAddress, Bitmap.data_size);
		if (Bitmap.p_data) {
			mmprofile_log_meta_bitmap(DDP_MMP_Events.wdma_dump[wdma_num],
						  MMPROFILE_FLAG_PULSE, &Bitmap);
			ddp_umap_va(Bitmap.p_data);
		} else {
			DDPERR("dprec_mmp_dump_wdma_layer(),fail to dump rgb(0x%x)\n",
			       wdma_layer->outputFormat);
		}
	} else {
		meta.data_type = MMPROFILE_META_RAW;
		meta.size = wdma_layer->dstPitch * wdma_layer->srcHeight;
		meta.p_data = ddp_map_mva_to_va(wdma_layer->dstAddress, meta.size);
		if (meta.p_data) {
			mmprofile_log_meta(DDP_MMP_Events.wdma_dump[wdma_num], MMPROFILE_FLAG_PULSE,
					   &meta);
			ddp_umap_va(meta.p_data);
		} else
			DDPERR("dprec_mmp_dump_wdma_layer(),fail to dump raw(0x%x)\n",
			       wdma_layer->outputFormat);
	}
}

void ddp_mmp_rdma_layer(struct RDMA_CONFIG_STRUCT *rdma_layer, unsigned int rdma_num,
			unsigned int down_sample_x, unsigned int down_sample_y)
{
	mmp_metadata_bitmap_t Bitmap;
	mmp_metadata_t meta;
	int raw = 0;

	if (rdma_num > 1) {
		DDPERR("dump_rdma_layer is error %d\n", rdma_num);
		return;
	}

	if (rdma_layer->security == DISP_SECURE_BUFFER)
		return;

	Bitmap.data1 = rdma_layer->address;
	Bitmap.width = rdma_layer->width;
	Bitmap.height = rdma_layer->height;
	switch (rdma_layer->inputFormat) {
	case eRGB565:
	case eBGR565:
		Bitmap.format = MMPROFILE_BITMAP_RGB565;
		Bitmap.bpp = 16;
		break;
	case eRGB888:
		Bitmap.format = MMPROFILE_BITMAP_RGB888;
		Bitmap.bpp = 24;
		break;
	case eBGRA8888:
		Bitmap.format = MMPROFILE_BITMAP_BGRA8888;
		Bitmap.bpp = 32;
		break;
	case eBGR888:
		Bitmap.format = MMPROFILE_BITMAP_BGR888;
		Bitmap.bpp = 24;
		break;
	case eRGBA8888:
		Bitmap.format = MMPROFILE_BITMAP_RGBA8888;
		Bitmap.bpp = 32;
		break;
	default:
		DDPERR("dump_rdma_layer(), unknown fmt=%d, dump raw\n", rdma_layer->inputFormat);
		raw = 1;
	}
	if (!raw) {
		Bitmap.start_pos = 0;
		Bitmap.pitch = rdma_layer->pitch;
		Bitmap.data_size = Bitmap.pitch * Bitmap.height;
		Bitmap.down_sample_x = down_sample_x;
		Bitmap.down_sample_y = down_sample_y;
		Bitmap.p_data = ddp_map_mva_to_va(rdma_layer->address, Bitmap.data_size);
		if (Bitmap.p_data) {
			mmprofile_log_meta_bitmap(DDP_MMP_Events.rdma_dump[rdma_num],
						  MMPROFILE_FLAG_PULSE, &Bitmap);
			ddp_umap_va(Bitmap.p_data);
		} else {
			DDPERR("dump_rdma_layer(),fail to dump rgb(0x%x)\n",
			       rdma_layer->inputFormat);
		}
	} else {
		meta.data_type = MMPROFILE_META_RAW;
		meta.size = rdma_layer->pitch * rdma_layer->height;
		meta.p_data = ddp_map_mva_to_va(rdma_layer->address, meta.size);
		if (meta.p_data) {
			mmprofile_log_meta(DDP_MMP_Events.rdma_dump[rdma_num], MMPROFILE_FLAG_PULSE,
					   &meta);
			ddp_umap_va(meta.p_data);
		} else {
			DDPERR("dprec_mmp_dump_rdma_layer(),fail to dump raw(0x%x)\n",
			       rdma_layer->inputFormat);
		}
	}
}


struct DDP_MMP_Events_t *ddp_mmp_get_events(void)
{
	return &DDP_MMP_Events;
}

void ddp_mmp_init(void)
{
#ifdef DEFAULT_MMP_ENABLE
	DDPPRINT("ddp_mmp_init\n");
	mmprofile_enable(1);
	init_ddp_mmp_events();
	mmprofile_start(1);
#endif
}
