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

#ifndef _DAPC_H
#define _DAPC_H
#include <linux/types.h>
#define MOD_NO_IN_1_DEVAPC                  16
#define DEVAPC_TAG                          "DEVAPC"
/*For EMI API DEVAPC0_D0_VIO_STA_4, idx:150*/
#define ABORT_EMI                0x00400000
/*Define constants*/
#define DEVAPC_DEVICE_NUMBER    140
#define DEVAPC_DOMAIN_AP        0
#define DEVAPC_DOMAIN_MD        1
#define DEVAPC_DOMAIN_CONN      2
#define DEVAPC_DOMAIN_MM        3
#define VIO_DBG_MSTID 0x0003FFF
#define VIO_DBG_DMNID 0x000C000
#define VIO_DBG_RW    0x3000000
#define VIO_DBG_CLR   0x80000000

/******************************************************************************
*REGISTER ADDRESS DEFINATION
******************************************************************************/
#define DEVAPC0_D0_APC_0            ((unsigned int *)(devapc_ao_base+0x0000))
#define DEVAPC0_D0_APC_1            ((unsigned int *)(devapc_ao_base+0x0004))
#define DEVAPC0_D0_APC_2            ((unsigned int *)(devapc_ao_base+0x0008))
#define DEVAPC0_D0_APC_3            ((unsigned int *)(devapc_ao_base+0x000C))
#define DEVAPC0_D0_APC_4            ((unsigned int *)(devapc_ao_base+0x0010))
#define DEVAPC0_D0_APC_5            ((unsigned int *)(devapc_ao_base+0x0014))
#define DEVAPC0_D0_APC_6            ((unsigned int *)(devapc_ao_base+0x0018))
#define DEVAPC0_D0_APC_7            ((unsigned int *)(devapc_ao_base+0x001C))
#define DEVAPC0_D0_APC_8            ((unsigned int *)(devapc_ao_base+0x0020))

#define DEVAPC0_D1_APC_0            ((unsigned int *)(devapc_ao_base+0x0100))
#define DEVAPC0_D1_APC_1            ((unsigned int *)(devapc_ao_base+0x0104))
#define DEVAPC0_D1_APC_2            ((unsigned int *)(devapc_ao_base+0x0108))
#define DEVAPC0_D1_APC_3            ((unsigned int *)(devapc_ao_base+0x010C))
#define DEVAPC0_D1_APC_4            ((unsigned int *)(devapc_ao_base+0x0110))
#define DEVAPC0_D1_APC_5            ((unsigned int *)(devapc_ao_base+0x0114))
#define DEVAPC0_D1_APC_6            ((unsigned int *)(devapc_ao_base+0x0118))
#define DEVAPC0_D1_APC_7            ((unsigned int *)(devapc_ao_base+0x011C))
#define DEVAPC0_D1_APC_8            ((unsigned int *)(devapc_ao_base+0x0120))

#define DEVAPC0_D2_APC_0            ((unsigned int *)(devapc_ao_base+0x0200))
#define DEVAPC0_D2_APC_1            ((unsigned int *)(devapc_ao_base+0x0204))
#define DEVAPC0_D2_APC_2            ((unsigned int *)(devapc_ao_base+0x0208))
#define DEVAPC0_D2_APC_3            ((unsigned int *)(devapc_ao_base+0x020C))
#define DEVAPC0_D2_APC_4            ((unsigned int *)(devapc_ao_base+0x0210))
#define DEVAPC0_D2_APC_5            ((unsigned int *)(devapc_ao_base+0x0214))
#define DEVAPC0_D2_APC_6            ((unsigned int *)(devapc_ao_base+0x0218))
#define DEVAPC0_D2_APC_7            ((unsigned int *)(devapc_ao_base+0x021C))
#define DEVAPC0_D2_APC_8            ((unsigned int *)(devapc_ao_base+0x0220))

#define DEVAPC0_D3_APC_0            ((unsigned int *)(devapc_ao_base+0x0300))
#define DEVAPC0_D3_APC_1            ((unsigned int *)(devapc_ao_base+0x0304))
#define DEVAPC0_D3_APC_2            ((unsigned int *)(devapc_ao_base+0x0308))
#define DEVAPC0_D3_APC_3            ((unsigned int *)(devapc_ao_base+0x030C))
#define DEVAPC0_D3_APC_4            ((unsigned int *)(devapc_ao_base+0x0310))
#define DEVAPC0_D3_APC_5            ((unsigned int *)(devapc_ao_base+0x0314))
#define DEVAPC0_D3_APC_6            ((unsigned int *)(devapc_ao_base+0x0318))
#define DEVAPC0_D3_APC_7            ((unsigned int *)(devapc_ao_base+0x031C))
#define DEVAPC0_D3_APC_8            ((unsigned int *)(devapc_ao_base+0x0320))

#define DEVAPC0_MAS_DOM_0           ((unsigned int *)(devapc_ao_base+0x0400))
#define DEVAPC0_MAS_DOM_1           ((unsigned int *)(devapc_ao_base+0x0404))
#define DEVAPC0_MAS_SEC             ((unsigned int *)(devapc_ao_base+0x0500))
#define DEVAPC0_APC_CON             ((unsigned int *)(devapc_ao_base+0x0F00))
#define DEVAPC0_APC_LOCK_0          ((unsigned int *)(devapc_ao_base+0x0F04))
#define DEVAPC0_APC_LOCK_1          ((unsigned int *)(devapc_ao_base+0x0F08))
#define DEVAPC0_APC_LOCK_2          ((unsigned int *)(devapc_ao_base+0x0F0C))
#define DEVAPC0_APC_LOCK_3          ((unsigned int *)(devapc_ao_base+0x0F10))
#define DEVAPC0_APC_LOCK_4          ((unsigned int *)(devapc_ao_base+0x0F14))

#define DEVAPC0_PD_APC_CON          ((unsigned int *)(devapc_pd_base+0x0F00))
#define DEVAPC0_D0_VIO_MASK_0       ((unsigned int *)(devapc_pd_base+0x0000))
#define DEVAPC0_D0_VIO_MASK_1       ((unsigned int *)(devapc_pd_base+0x0004))
#define DEVAPC0_D0_VIO_MASK_2       ((unsigned int *)(devapc_pd_base+0x0008))
#define DEVAPC0_D0_VIO_MASK_3       ((unsigned int *)(devapc_pd_base+0x000C))
#define DEVAPC0_D0_VIO_MASK_4       ((unsigned int *)(devapc_pd_base+0x0010))
#define DEVAPC0_D0_VIO_STA_0        ((unsigned int *)(devapc_pd_base+0x0400))
#define DEVAPC0_D0_VIO_STA_1        ((unsigned int *)(devapc_pd_base+0x0404))
#define DEVAPC0_D0_VIO_STA_2        ((unsigned int *)(devapc_pd_base+0x0408))
#define DEVAPC0_D0_VIO_STA_3        ((unsigned int *)(devapc_pd_base+0x040C))
#define DEVAPC0_D0_VIO_STA_4        ((unsigned int *)(devapc_pd_base+0x0410))
#define DEVAPC0_VIO_DBG0            ((unsigned int *)(devapc_pd_base+0x0900))
#define DEVAPC0_VIO_DBG1            ((unsigned int *)(devapc_pd_base+0x0904))
#define DEVAPC0_DEC_ERR_CON         ((unsigned int *)(devapc_pd_base+0x0F80))
#define DEVAPC0_DEC_ERR_ADDR        ((unsigned int *)(devapc_pd_base+0x0F84))
#define DEVAPC0_DEC_ERR_ID          ((unsigned int *)(devapc_pd_base+0x0F88))

struct DEVICE_INFO {
	const char *device;
	bool forbidden;
};

#endif

