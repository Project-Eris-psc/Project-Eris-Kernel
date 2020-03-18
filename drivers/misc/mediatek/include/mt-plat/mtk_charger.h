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

#ifndef __MTK_CHARGER_H__
#define __MTK_CHARGER_H__

#include <linux/ktime.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <mach/mtk_charger_init.h>

#include <mt-plat/charger_type.h>
#include <mt-plat/charger_class.h>

/* charger_manager notify charger_consumer */
enum {
	CHARGER_NOTIFY_EOC,
	CHARGER_NOTIFY_START_CHARGING,
	CHARGER_NOTIFY_STOP_CHARGING,
	CHARGER_NOTIFY_ERROR,
	CHARGER_NOTIFY_NORMAL,
};

enum {
	MAIN_CHARGER = 0,
	SLAVE_CHARGER = 1,
	DIRECT_CHARGER = 10,
};

struct charger_consumer {
	struct device *dev;
	void *cm;
	struct notifier_block *pnb;
	struct list_head list;
};

/* charger consumer interface */
extern struct charger_consumer *charger_manager_get_by_name(struct device *dev,
	const char *supply_name);
extern int charger_manager_set_input_current_limit(struct charger_consumer *consumer,
	int idx, int input_current_uA);
extern int charger_manager_set_charging_current_limit(struct charger_consumer *consumer,
	int idx, int charging_current_uA);
extern int charger_manager_set_pe30_input_current_limit(struct charger_consumer *consumer,
	int idx, int input_current_uA);
extern int charger_manager_get_pe30_input_current_limit(struct charger_consumer *consumer,
	int idx, int *input_current_uA, int *min_current_uA, int *max_current_uA);
extern int charger_manager_get_current_charging_type(struct charger_consumer *consumer);
extern int register_charger_manager_notifier(struct charger_consumer *consumer,
	struct notifier_block *nb);
extern int charger_manager_get_charger_temperature(struct charger_consumer *consumer,
	int idx, int *tchg_min,	int *tchg_max);
extern int unregister_charger_manager__notifier(struct charger_consumer *consumer,
				struct notifier_block *nb);

#endif /* __MTK_CHARGER_H__ */
