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

#ifndef __MTK_ICCS_H__
#define __MTK_ICCS_H__

#ifdef CONFIG_MTK_ICCS_SUPPORT
extern int iccs_get_next_state(const unsigned char curr_power_state_bitmask,
		unsigned char *next_power_state_bitmask,
		unsigned char *next_cache_shared_state_bitmask);

extern unsigned int iccs_is_cache_shared_enabled(unsigned int cluster_id);
#endif

#endif

