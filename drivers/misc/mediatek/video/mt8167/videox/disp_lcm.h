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

#ifndef _DISP_LCM_H_
#define _DISP_LCM_H_
#include "lcm_drv.h"

#define MAX_LCM_NUMBER	2

/*#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)*/
extern unsigned char lcm_name_list[][128];
/*#endif*/

extern unsigned int lcm_count;


struct disp_lcm_handle {
	LCM_PARAMS *params;
	LCM_DRIVER *drv;
	LCM_INTERFACE_ID lcm_if_id;
	int module;
	int is_inited;
	unsigned int lcm_original_width;
	unsigned int lcm_original_height;
	int index;
#if !defined(CONFIG_MTK_LEGACY)
	unsigned int driver_id;
	unsigned int module_id;
#endif
};

/* these 2 variables are defined in mt65xx_lcm_list.c */
extern LCM_DRIVER *lcm_driver_list[];
extern unsigned int lcm_count;

struct disp_lcm_handle *disp_lcm_probe(char *plcm_name, LCM_INTERFACE_ID lcm_id);
int disp_lcm_init(struct disp_lcm_handle *plcm, int force);
LCM_PARAMS *disp_lcm_get_params(struct disp_lcm_handle *plcm);
LCM_INTERFACE_ID disp_lcm_get_interface_id(struct disp_lcm_handle *plcm);
int disp_lcm_update(struct disp_lcm_handle *plcm, int x, int y, int w, int h, int force);
int disp_lcm_esd_check(struct disp_lcm_handle *plcm);
int disp_lcm_esd_recover(struct disp_lcm_handle *plcm);
int disp_lcm_suspend(struct disp_lcm_handle *plcm);
int disp_lcm_resume(struct disp_lcm_handle *plcm);
int disp_lcm_set_backlight(struct disp_lcm_handle *plcm, int level);
int disp_lcm_read_fb(struct disp_lcm_handle *plcm);
int disp_lcm_ioctl(struct disp_lcm_handle *plcm, LCM_IOCTL ioctl, unsigned int arg);
int disp_lcm_is_video_mode(struct disp_lcm_handle *plcm);
int disp_lcm_is_inited(struct disp_lcm_handle *plcm);
unsigned int disp_lcm_ATA(struct disp_lcm_handle *plcm);
void *disp_lcm_switch_mode(struct disp_lcm_handle *plcm, int mode);
int disp_lcm_set_cmd(struct disp_lcm_handle *plcm, void *handle, int *lcm_cmd, unsigned int cmd_num);

#endif
