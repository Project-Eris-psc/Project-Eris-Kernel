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

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <mt-plat/mtk_battery.h>

struct shutdown_condition {
	bool is_overheat;
	bool is_soc_zero_percent;
	bool is_uisoc_one_percent;
	bool is_under_shutdown_voltage;
	bool is_dlpt_shutdown;
};

struct shutdown_controller {
	struct fgtimer kthread_fgtimer;
	bool timeout;
	wait_queue_head_t  wait_que;
	struct shutdown_condition shutdown_status;
	struct timespec pre_time[SHUTDOWN_FACTOR_MAX];
	int avgvbat;
	bool lowbatteryshutdown;
	int batdata[AVGVBAT_ARRAY_SIZE];
	int batidx;
	struct mutex lock;
	struct notifier_block psy_nb;
};

static struct shutdown_controller sdc;

static int g_vbat_lt;
static int g_vbat_lt_lv1;


static void wake_up_power_misc(struct shutdown_controller *sdd)
{
	sdd->timeout = true;
	wake_up(&sdd->wait_que);
}

void set_shutdown_vbat_lt(int vbat_lt, int vbat_lt_lv1)
{
	g_vbat_lt = vbat_lt;
	g_vbat_lt_lv1 = vbat_lt_lv1;
}

int get_shutdown_cond(void)
{
	int ret = 0;

	if (sdc.shutdown_status.is_soc_zero_percent)
		ret |= 1;
	if (sdc.shutdown_status.is_uisoc_one_percent)
		ret |= 1;
	if (sdc.lowbatteryshutdown)
		ret |= 1;
	return ret;
}

int set_shutdown_cond(int shutdown_cond)
{
	int now_current;
	int now_is_charging;
	int now_is_kpoc;

	now_current = battery_get_bat_current();
	now_is_charging = battery_get_bat_current_sign();
	now_is_kpoc = battery_get_is_kpoc();

	bm_err("set_shutdown_cond %d, is kpoc %d curr %d is_charging %d\n",
		shutdown_cond, now_is_kpoc, now_current, now_is_charging);

	switch (shutdown_cond) {
	case OVERHEAT:
		mutex_lock(&sdc.lock);
		sdc.shutdown_status.is_overheat = true;
		mutex_unlock(&sdc.lock);
		kernel_power_off();
		break;
	case SOC_ZERO_PERCENT:
		if (sdc.shutdown_status.is_soc_zero_percent != true) {
			mutex_lock(&sdc.lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sdc.shutdown_status.is_soc_zero_percent = true;
					get_monotonic_boottime(&sdc.pre_time[SOC_ZERO_PERCENT]);
					notify_fg_shutdown();
				}
			}
			mutex_unlock(&sdc.lock);
		}
		break;
	case UISOC_ONE_PERCENT:
		if (sdc.shutdown_status.is_uisoc_one_percent != true) {
			mutex_lock(&sdc.lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sdc.shutdown_status.is_uisoc_one_percent = true;
					get_monotonic_boottime(&sdc.pre_time[UISOC_ONE_PERCENT]);
					notify_fg_shutdown();
				}
			}
			mutex_unlock(&sdc.lock);
		}
		break;
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		int i;

		if (sdc.shutdown_status.is_under_shutdown_voltage != true) {
			mutex_lock(&sdc.lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sdc.shutdown_status.is_under_shutdown_voltage = true;
					for (i = 0; i < AVGVBAT_ARRAY_SIZE; i++)
						sdc.batdata[i] = battery_get_bat_avg_voltage();
					sdc.batidx = 0;
				}
			}
			mutex_unlock(&sdc.lock);
		}
		break;
#endif
	case DLPT_SHUTDOWN:
		if (sdc.shutdown_status.is_dlpt_shutdown != true) {
			mutex_lock(&sdc.lock);
			sdc.shutdown_status.is_dlpt_shutdown = true;
			get_monotonic_boottime(&sdc.pre_time[DLPT_SHUTDOWN]);
			notify_fg_dlpt_sd();
			mutex_unlock(&sdc.lock);
		}
		break;

	default:
		break;
	}

	wake_up_power_misc(&sdc);

	return 0;
}

static int shutdown_event_handler(struct shutdown_controller *sdd)
{
	struct timespec now, duraction;
	int polling = 0;
	static int ui_zero_time_flag;
	static int down_to_low_bat;
	int current_ui_soc = battery_get_bat_uisoc();
	int current_soc = battery_get_bat_soc();


	now.tv_sec = 0;
	now.tv_nsec = 0;
	duraction.tv_sec = 0;
	duraction.tv_nsec = 0;

	get_monotonic_boottime(&now);

	if (sdd->shutdown_status.is_soc_zero_percent) {
		if (current_ui_soc == 0) {
			duraction = timespec_sub(now, sdd->pre_time[SOC_ZERO_PERCENT]);
			polling = 10;
			if (duraction.tv_sec >= SHUTDOWN_TIME) {
				bm_err("soc zero shutdown\n");
				kernel_power_off();
			}
		} else if (current_soc > 0) {
			sdd->shutdown_status.is_soc_zero_percent = false;
			polling = 0;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling = 10;
		}
	}

	if (sdd->shutdown_status.is_uisoc_one_percent) {
		if (current_ui_soc == 0) {
			duraction = timespec_sub(now, sdd->pre_time[UISOC_ONE_PERCENT]);
			polling = 10;
			if (duraction.tv_sec >= SHUTDOWN_TIME) {
				bm_err("uisoc one shutdown\n");
				kernel_power_off();
			}
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling = 10;
		}
	}

	if (sdd->shutdown_status.is_dlpt_shutdown) {
		duraction = timespec_sub(now, sdd->pre_time[DLPT_SHUTDOWN]);
		polling = 10;
		if (duraction.tv_sec >= SHUTDOWN_TIME) {
			bm_err("dlpt shutdown\n");
			kernel_power_off();
		}
	}

	if (sdd->shutdown_status.is_under_shutdown_voltage) {
#if 1
		int vbatcnt = 0, i;

		sdd->batdata[sdd->batidx] = battery_get_bat_avg_voltage();

		for (i = 0; i < AVGVBAT_ARRAY_SIZE; i++)
			vbatcnt += sdd->batdata[i];
		sdd->avgvbat = vbatcnt / AVGVBAT_ARRAY_SIZE;
#else
		sdd->avgvbat = battery_get_bat_avg_voltage();
#endif

#if 0
		if (sdd->avgvbat < (UNIT_TRANS_10 * BAT_VOLTAGE_LOW_BOUND) && sdd->lowbatteryshutdown == false) {
			sdd->lowbatteryshutdown = true;
			/*get_monotonic_boottime(&sdc.pre_time[LOW_BAT_VOLT]);*/
			notify_fg_shutdown();
		}

		if ((ui_soc == 0) && (ui_zero_time_flag == 0)) {
			get_monotonic_boottime(&sdc.pre_time[LOW_BAT_VOLT]);
			ui_zero_time_flag = 1;
		}

		if (sdd->lowbatteryshutdown == true) {
			polling = 10;
			if (ui_soc == 0) {
				duraction = timespec_sub(now, sdd->pre_time[LOW_BAT_VOLT]);
				if (duraction.tv_sec >= SHUTDOWN_TIME) {
					bm_err("low bat shutdown\n");
					kernel_power_off();
				}
			}

			if (sdd->avgvbat > (UNIT_TRANS_10 * BAT_VOLTAGE_LOW_BOUND))
				sdd->lowbatteryshutdown == false;
		}
#else
		if (sdd->avgvbat < (UNIT_TRANS_10 * BAT_VOLTAGE_LOW_BOUND)) {
			/* less than 3.4v */

			if (down_to_low_bat == 0) {
				down_to_low_bat = 1;
				notify_fg_shutdown();
			}

			if ((current_ui_soc == 0) && (ui_zero_time_flag == 0)) {
				duraction = timespec_sub(now, sdd->pre_time[LOW_BAT_VOLT]);
				ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				if (duraction.tv_sec >= SHUTDOWN_TIME) {
					bm_err("low bat shutdown\n");
					kernel_power_off();
				}
			}

			sdd->lowbatteryshutdown = true;
			polling = 10;
		} else {
			/* greater than 3.4v, clear status */
			down_to_low_bat = 0;
			ui_zero_time_flag = 0;
			sdd->pre_time[LOW_BAT_VOLT].tv_sec = 0;
			sdd->lowbatteryshutdown = false;
			polling = 10;
		}
#endif
		if ((sdd->avgvbat >= (UNIT_TRANS_10 * BAT_VOLTAGE_HIGH_BOUND)) &&
			(g_vbat_lt == g_vbat_lt_lv1)) {
			sdd->shutdown_status.is_under_shutdown_voltage = false;
			polling = 0;
		} else
			polling = 10;
			bm_err("[shutdown_event_handler][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d\n",
				sdd->avgvbat, current_ui_soc, (int)duraction.tv_sec,
			down_to_low_bat, ui_zero_time_flag,
			(int)sdd->pre_time[LOW_BAT_VOLT].tv_sec,
			sdd->lowbatteryshutdown, sdd->batidx, sdd->batdata[sdd->batidx]);

		sdd->batidx++;
		if (sdd->batidx >= AVGVBAT_ARRAY_SIZE)
			sdd->batidx = 0;
	}

	bm_err("shutdown_event_handler %d avgvbat:%d sec:%d lowst:%d\n", polling, sdd->avgvbat,
		(int)duraction.tv_sec, sdd->lowbatteryshutdown);
	return polling;
}

static int power_misc_kthread_fgtimer_func(struct fgtimer *data)
{
	struct shutdown_controller *info = container_of(data, struct shutdown_controller, kthread_fgtimer);

	wake_up_power_misc(info);
	return 0;
}

static int power_misc_routine_thread(void *arg)
{
	struct shutdown_controller *sdd = arg;
	int ret;

	while (1) {
		bm_err("power_misc_routine_thread\n");

		wait_event(sdd->wait_que, (sdd->timeout == true));
		sdd->timeout = false;

		mutex_lock(&sdd->lock);
		ret = shutdown_event_handler(sdd);
		mutex_unlock(&sdd->lock);
		if (ret != 0)
			fgtimer_start(&sdd->kthread_fgtimer, ret);
	}

	return 0;
}

int mtk_power_misc_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret;
	int tmp = 0;

	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = psy->desc->get_property(psy, POWER_SUPPLY_PROP_batt_temp, &val);
		if (!ret) {
			tmp = val.intval / 10;
			if (tmp >= BATTERY_SHUTDOWN_TEMPERATURE) {
				bm_err("battery temperature >= %d , shutdown", tmp);
				kernel_power_off();
			}
		}
	}

	return NOTIFY_DONE;
}

void mtk_power_misc_init(struct platform_device *pdev)
{
	mutex_init(&sdc.lock);
	fgtimer_init(&sdc.kthread_fgtimer, &pdev->dev, "power_misc");
	sdc.kthread_fgtimer.callback = power_misc_kthread_fgtimer_func;
	init_waitqueue_head(&sdc.wait_que);

	sdc.psy_nb.notifier_call = mtk_power_misc_psy_event;
	power_supply_reg_notifier(&sdc.psy_nb);

	kthread_run(power_misc_routine_thread, &sdc, "power_misc_thread");
}

