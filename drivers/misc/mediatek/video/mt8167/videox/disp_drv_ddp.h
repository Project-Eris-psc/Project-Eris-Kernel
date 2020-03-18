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

#ifndef __DISP_DRV_DDP_H__
#define __DISP_DRV_DDP_H__
#include <linux/types.h>

typedef int (*DISP_EXTRA_CHECKUPDATE_PTR) (int);
typedef int (*DISP_EXTRA_CONFIG_PTR) (int);
int DISP_RegisterExTriggerSource(DISP_EXTRA_CHECKUPDATE_PTR pCheckUpdateFunc,
				 DISP_EXTRA_CONFIG_PTR pConfFunc);
void DISP_UnRegisterExTriggerSource(int u4ID);
void GetUpdateMutex(void);
void ReleaseUpdateMutex(void);
bool DISP_IsVideoMode(void);
unsigned long DISP_GetLCMIndex(void);

#endif
