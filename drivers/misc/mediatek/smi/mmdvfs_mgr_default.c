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

/*
 * This is for bring up and test only.
 */
#include "mmdvfs_mgr.h"

#define MMDVFS_BRINGUP_MSG(func_name) MMDVFSMSG("This is %s bringup version, do nothing\n", func_name)

void mmdvfs_init(MTK_SMI_BWC_MM_INFO *info)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
void mmdvfs_handle_cmd(MTK_MMDVFS_CMD *cmd)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
void mmdvfs_notify_scenario_enter(MTK_SMI_BWC_SCEN scen)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
void mmdvfs_notify_scenario_exit(MTK_SMI_BWC_SCEN scen)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
void mmdvfs_notify_scenario_concurrency(unsigned int u4Concurrency)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
void mmdvfs_mhl_enable(int enable)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
void mmdvfs_mjc_enable(int enable)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
int mmdvfs_notify_mmclk_switch_request(int event)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int mmdvfs_raise_mmsys_by_mux(void)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int mmdvfs_lower_mmsys_by_mux(void)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int register_mmclk_switch_cb(clk_switch_cb notify_cb, clk_switch_cb notify_cb_nolock)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int mmdvfs_register_mmclk_switch_cb(clk_switch_cb notify_cb, int mmdvfs_client_id)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
void dump_mmdvfs_info(void)
{
	MMDVFS_BRINGUP_MSG(__func__);
}
int mmdvfs_set_step(MTK_SMI_BWC_SCEN scenario, mmdvfs_voltage_enum step)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int mmdvfs_get_mmdvfs_profile(void)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int is_mmdvfs_supported(void)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int mmdvfs_set_mmsys_clk(MTK_SMI_BWC_SCEN scenario, int mmsys_clk_mode)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return 0;
}
int mmdvfs_get_stable_isp_clk(void)
{
	MMDVFS_BRINGUP_MSG(__func__);
	return MMSYS_CLK_HIGH;
}
