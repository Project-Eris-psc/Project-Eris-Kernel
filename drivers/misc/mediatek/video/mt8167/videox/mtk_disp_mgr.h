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

#ifndef __H_MTK_DISP_MGR__
#define __H_MTK_DISP_MGR__

#include "disp_session.h"
#include "primary_display.h"

long mtk_disp_mgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
extern struct mutex disp_trigger_lock;

int disp_create_session(struct disp_session_config *config);
int disp_destroy_session(struct disp_session_config *config);
int disp_get_session_number(void);
char *disp_session_mode_spy(unsigned int session_id);

extern struct mutex session_config_mutex;
extern struct disp_session_input_config *captured_session_input;
extern struct disp_session_input_config *cached_session_input;
extern struct disp_mem_output_config *captured_session_output;
extern struct disp_mem_output_config *cached_session_output;
extern unsigned int ext_session_id;
#ifdef CONFIG_SINGLE_PANEL_OUTPUT
extern struct SWITCH_MODE_INFO_STRUCT path_info;
#endif

#endif
