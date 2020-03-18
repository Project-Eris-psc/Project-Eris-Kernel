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

#ifndef __ICCS_H__
#define __ICCS_H__

#define ICCS_GOV_NAME_LEN        16
#define ICCS_MIN_SAMPLING_RATE   40ULL
#define MS_TO_NS(msec)           ((msec) * 1000 * 1000ULL)

enum transition {
	ONDEMAND = 0,
	PERFORMANCE,
	POWERSAVE,
	BENCHMARK,
	LAST_TRANSITION
};

typedef enum {
	UNINITIALIZED = 0,
	POWER_OFF_CACHE_SHARED_DISABLED,
	POWER_ON_CACHE_SHARED_DISABLED,
	POWER_ON_CACHE_SHARED_ENABLED,
} iccs_state;

struct iccs_governor {
	unsigned int enabled;
	unsigned int nr_cluster;
	unsigned int shared_cluster_freq;
	unsigned long dram_bw_threshold;
	unsigned long l2c_hitrate_threshold;
	struct task_struct *task;
	struct hrtimer hr_timer;
	ktime_t sampling;
	spinlock_t spinlock;
	enum transition policy;
	unsigned char iccs_cache_shared_useful_bitmask;
};

struct iccs_cluster_info {
	unsigned int cache_shared_supported;
	iccs_state state;
};

/* Governor API*/
int governor_activation(struct iccs_governor *gov);
void policy_update(struct iccs_governor *gov, enum transition new_policy);
void sampling_update(struct iccs_governor *gov, unsigned long long rate);

#endif

