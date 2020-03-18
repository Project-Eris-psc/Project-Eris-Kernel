/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "dfrc.h"

#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

#include <ged_frr.h>

#include "dfrc_drv.h"

extern void dfrc_fps_limit_cb(int fps_limit) __attribute__((weak));

#define DFRC_DEVNAME "mtk_dfrc"

#define DFRC_LOG_LEVEL 3

#if (DFRC_LOG_LEVEL > 0)
#define DFRC_ERR(x, ...) pr_err("[DFRC] " x, ##__VA_ARGS__)
#else
#define DFRC_ERR(...)
#endif

#if (DFRC_LOG_LEVEL > 1)
#define DFRC_WRN(x, ...) pr_warn("[DFRC] " x, ##__VA_ARGS__)
#else
#define DFRC_WRN(...)
#endif

#if (DFRC_LOG_LEVEL > 2)
#define DFRC_INFO(x, ...) pr_info("[DFRC] " x, ##__VA_ARGS__)
#else
#define DFRC_INFO(...)
#endif

#if (DFRC_LOG_LEVEL > 3)
#define DFRC_DBG(x, ...) pr_info("[DFRC] " x, ##__VA_ARGS__)
#else
#define DFRC_DBG(...)
#endif

#if (DFRC_LOG_LEVEL > 4)
#define DFRC_VBS(x, ...) pr_info("[DFRC] " x, ##__VA_ARGS__)
#else
#define DFRC_VBS(...)
#endif

#define NUM_UPPER_BOUND 3

static char const *dfrc_api_string[DFRC_DRV_API_MAXIMUM] = {
	"GIFT",
	"VIDEO",
	"RRC_TOUCH",
	"RRC_VIDEO",
	"THERMAL",
	"LOADING",
	"WHITELIST"
};

static char const *dfrc_mode_string[DFRC_DRV_MODE_MAXIMUM] = {
	"DEFAULT",
	"FRR",
	"ARR",
};

struct DFRC_DRV_EXPECTED_POLICY {
	int mode;
	struct DFRC_DRV_POLICY *arr_policy;
	struct DFRC_DRV_POLICY_STATISTICS_SET *frr_statistics;
};

struct DFRC_DRV_POLICY_NODE {
	struct DFRC_DRV_POLICY policy;
	struct list_head list;
	struct list_head list_statistics;
};

struct DFRC_DRV_POLICY_STATISTICS {
	int num_policy;
	int num_valid_policy;
	struct list_head list;
};

struct DFRC_DRV_POLICY_STATISTICS_SET {
	int num_policy;
	struct DFRC_DRV_POLICY_STATISTICS statistics[DFRC_DRV_API_MAXIMUM];
};

struct DFRC_DRV_PANEL_INFO_LIST {
	int support_120;
	int support_90;
	int num;
	struct DFRC_DRV_REFRESH_RANGE *range;
};

/* these parameters are for device node and operation */
static dev_t dfrc_devno;
static struct cdev *dfrc_cdev;
static struct class *dfrc_class;
static struct mutex g_mutex_fop;
static int g_has_opened;

/* these parameters use to store policy list, focus pid, fg pid, and so on */
static struct mutex g_mutex_data;
static LIST_HEAD(g_fps_policy_list);
static struct DFRC_DRV_POLICY_STATISTICS_SET g_pss[DFRC_DRV_MODE_MAXIMUM];
static int g_num_fps_policy;
static struct DFRC_DRV_HWC_INFO g_hwc_info;
static struct DFRC_DRV_INPUT_WINDOW_INFO g_input_window_info;
static int g_event_count;
static int g_processed_count;
static int g_cond_remake;
static int g_run_rrc_fps;
static int g_allow_rrc_policy = DFRC_ALLOW_VIDEO | DFRC_ALLOW_TOUCH;
static int g_init_done;
static struct DFRC_DRV_PANEL_INFO_LIST g_fps_info;
static struct DFRC_DRV_WINDOW_STATE g_window_state;
static struct DFRC_DRV_FOREGROUND_WINDOW_INFO g_fg_window_info;

/* these parameters use to record the request of rrc */
static struct mutex g_mutex_rrc;
static struct DFRC_DRV_POLICY g_policy_rrc_input;
static struct DFRC_DRV_POLICY g_policy_rrc_video;
static struct DFRC_DRV_POLICY g_policy_thermal;
static struct DFRC_DRV_POLICY g_policy_loading;

/* these parametersuse to notify sw vsync */
static struct mutex g_mutex_request;
static struct DFRC_DRV_VSYNC_REQUEST g_request_notified;
static struct DFRC_DRV_POLICY *g_request_policy;
static int g_cond_request;

/* use to store the current vsync mode. only use them in make_policy_kthread */
/* therefore they do not need to protect by mutex */
static int g_current_fps;
static int g_current_sw_mode;
static int g_current_hw_mode;

static wait_queue_head_t g_wq_vsync_request;
static wait_queue_head_t g_wq_make_policy;
static wait_queue_head_t g_wq_fps_change;
static struct task_struct *g_task_make_policy;

static bool g_stop_thread;

/* use to create debug node */
static int debug_init;
static struct dentry *debug_dir;
static struct dentry *debugfs_dump_info;
static struct dentry *debugfs_dump_reason;

/* use to store debug info */
#define DUMP_MAX_LENGTH (4096)
static char g_string_info[DUMP_MAX_LENGTH];
static ssize_t g_string_info_len;
static char g_string_reason[DUMP_MAX_LENGTH];
static ssize_t g_string_reason_len;

void dfrc_remake_policy_locked(void)
{
	g_event_count++;
	g_cond_remake = 1;
	wake_up_interruptible(&g_wq_make_policy);
}

long dfrc_reg_policy_locked(const struct DFRC_DRV_POLICY *policy)
{
	long res = 0L;
	struct list_head *iter;
	bool is_new = true;
	struct DFRC_DRV_POLICY_NODE *node;
	struct DFRC_DRV_POLICY_STATISTICS_SET *pss;
	struct DFRC_DRV_POLICY_STATISTICS *ps;

	list_for_each(iter, &g_fps_policy_list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list);
		if (node->policy.sequence == policy->sequence) {
			is_new = false;
			break;
		}
	}

	if (is_new) {
		node = vmalloc(sizeof(struct DFRC_DRV_POLICY_NODE));
		if (node == NULL) {
			DFRC_WRN("reg_fps_policy: failed to allocate memory\n");
			res = -ENOMEM;
		} else {
			DFRC_INFO("reg_fps_policy: reg policy[%llu]\n", policy->sequence);
			INIT_LIST_HEAD(&node->list);
			INIT_LIST_HEAD(&node->list_statistics);
			memcpy(&node->policy, policy, sizeof(struct DFRC_DRV_POLICY));
			g_num_fps_policy++;
			list_add(&node->list, &g_fps_policy_list);

			pss = &g_pss[policy->mode];
			ps = &pss->statistics[policy->api];
			pss->num_policy++;
			ps->num_policy++;
			list_add(&node->list_statistics, &ps->list);
			DFRC_DBG("num_fps_policy:%d  %s_set:%d  %s_statistics:%d\n",
					g_num_fps_policy, dfrc_mode_string[policy->mode],
					pss->num_policy, dfrc_api_string[policy->api],
					ps->num_policy);
		}
	} else {
		DFRC_INFO("reg_fps_policy: the policy[%llu] is existed\n", policy->sequence);
	}

	return res;
}

long dfrc_reg_policy(const struct DFRC_DRV_POLICY *policy)
{
	long res = 0L;

	mutex_lock(&g_mutex_data);
	res = dfrc_reg_policy_locked(policy);
	mutex_unlock(&g_mutex_data);
	return res;
}

void dfrc_init_kernel_policy(void)
{
	DFRC_INFO("dfrc_init_kernel_policy\n");
	/* init rrc video policy */
	g_policy_rrc_video.sequence = DFRC_DRV_API_RRC_VIDEO;
	g_policy_rrc_video.api = DFRC_DRV_API_RRC_VIDEO;
	g_policy_rrc_video.pid = 0;
	g_policy_rrc_video.fps = -1;
	dfrc_reg_policy_locked(&g_policy_rrc_video);

	/* init rrc input policy */
	g_policy_rrc_input.sequence = DFRC_DRV_API_RRC_TOUCH;
	g_policy_rrc_input.api = DFRC_DRV_API_RRC_TOUCH;
	g_policy_rrc_input.pid = 0;
	g_policy_rrc_input.fps = -1;
	dfrc_reg_policy_locked(&g_policy_rrc_input);

	g_policy_thermal.sequence = DFRC_DRV_API_THERMAL;
	g_policy_thermal.api = DFRC_DRV_API_THERMAL;
	g_policy_thermal.pid = 0;
	g_policy_thermal.fps = -1;
	dfrc_reg_policy_locked(&g_policy_thermal);

	g_policy_loading.sequence = DFRC_DRV_API_LOADING;
	g_policy_loading.api = DFRC_DRV_API_LOADING;
	g_policy_loading.pid = 0;
	g_policy_loading.fps = -1;
	dfrc_reg_policy_locked(&g_policy_loading);
}

long dfrc_set_policy_locked(const struct DFRC_DRV_POLICY *policy)
{
	long res = 0L;
	struct list_head *iter;
	bool is_new = true;
	int change = false;
	struct DFRC_DRV_POLICY_NODE *node = NULL;
	struct DFRC_DRV_POLICY_STATISTICS_SET *pss = NULL;
	struct DFRC_DRV_POLICY_STATISTICS *ps = NULL;

	if (policy == NULL) {
		return -EINVAL;
	}

	list_for_each(iter, &g_fps_policy_list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list);
		if (node->policy.sequence == policy->sequence) {
			is_new = false;
			if (node->policy.fps != policy->fps ||
					node->policy.mode != policy->mode ||
					node->policy.target_pid != policy->target_pid ||
					node->policy.gl_context_id != policy->gl_context_id) {
				change = true;
				DFRC_INFO("set_policy: [%llu] fps:%d mode:%d t_pid:%d gl_id:%llu\n",
						policy->sequence, policy->fps, policy->mode,
						policy->target_pid, policy->gl_context_id);
				if (node->policy.mode != policy->mode) {
					pss = &g_pss[node->policy.mode];
					ps = &pss->statistics[node->policy.api];
					pss->num_policy--;
					ps->num_policy--;
					if (node->policy.fps != -1)
						ps->num_valid_policy--;
					DFRC_DBG("%s_set:%d  %s_statistics:%d/%d\n",
							dfrc_mode_string[node->policy.mode],
							pss->num_policy,
							dfrc_api_string[node->policy.api],
							ps->num_valid_policy,
							ps->num_policy);
					list_del(&node->list_statistics);

					INIT_LIST_HEAD(&node->list_statistics);
					pss = &g_pss[policy->mode];
					ps = &pss->statistics[policy->api];
					pss->num_policy++;
					ps->num_policy++;
					if (policy->fps != -1)
						ps->num_valid_policy++;
					DFRC_DBG("%s_set:%d  %s_statistics:%d/%d\n",
							dfrc_mode_string[policy->mode],
							pss->num_policy,
							dfrc_api_string[policy->api],
							ps->num_valid_policy,
							ps->num_policy);
					list_add(&node->list_statistics, &ps->list);
				} else if (node->policy.fps != policy->fps) {
					if (node->policy.fps == -1) {
						pss = &g_pss[node->policy.mode];
						ps = &pss->statistics[node->policy.api];
						ps->num_valid_policy++;
					} else if (policy->fps == -1) {
						pss = &g_pss[node->policy.mode];
						ps = &pss->statistics[node->policy.api];
						ps->num_valid_policy--;
					}
					if (pss != NULL)
						DFRC_DBG("%s_set:%d  %s_statistics:%d/%d\n",
								dfrc_mode_string[policy->mode],
								pss->num_policy,
								dfrc_api_string[policy->api],
								ps->num_valid_policy,
								ps->num_policy);
				}
				node->policy.fps = policy->fps;
				node->policy.mode = policy->mode;
				node->policy.target_pid = policy->target_pid;
				node->policy.gl_context_id = policy->gl_context_id;
			}
		}
	}

	if (is_new) {
		DFRC_INFO("set_policy: can not find policy[%llu]\n", policy->sequence);
		res = -ENODEV;
	} else if (change) {
		dfrc_remake_policy_locked();
	}

	return res;
}

long dfrc_set_policy(const struct DFRC_DRV_POLICY *policy)
{
	long res = 0L;

	mutex_lock(&g_mutex_data);
	res = dfrc_set_policy_locked(policy);
	mutex_unlock(&g_mutex_data);
	return res;
}

long dfrc_unreg_policy(const unsigned long long sequence)
{
	long res = 0L;
	struct list_head *iter;
	bool is_new = true;
	struct DFRC_DRV_POLICY_NODE *node = NULL;
	struct DFRC_DRV_POLICY_STATISTICS_SET *pss = NULL;
	struct DFRC_DRV_POLICY_STATISTICS *ps = NULL;

	mutex_lock(&g_mutex_data);
	list_for_each(iter, &g_fps_policy_list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list);
		if (node->policy.sequence == sequence) {
			is_new = false;
			DFRC_INFO("dfrc_unreg_policy: unreg policy[%llu]\n", sequence);
			g_num_fps_policy--;
			list_del(&node->list);

			pss = &g_pss[node->policy.mode];
			ps = &pss->statistics[node->policy.api];
			if (node->policy.fps != -1)
				ps->num_valid_policy--;
			pss->num_policy--;
			ps->num_policy--;
			DFRC_DBG("%s_set:%d  %s_statistics:%d/%d\n",
					dfrc_mode_string[node->policy.mode], pss->num_policy,
					dfrc_api_string[node->policy.api], ps->num_valid_policy,
					ps->num_policy);
			list_del(&node->list_statistics);
			vfree(node);
			break;
		}
	}

	if (is_new) {
		DFRC_INFO("unreg_fps_policy: can not find policy[%llu]\n", sequence);
		res = -ENODEV;
	} else {
		dfrc_remake_policy_locked();
	}

	mutex_unlock(&g_mutex_data);
	return res;
}

void dfrc_set_hwc_info(const struct DFRC_DRV_HWC_INFO *hwc_info)
{
	mutex_lock(&g_mutex_data);
	g_hwc_info = *hwc_info;
	DFRC_DBG("dfrc_set_hwc_info: single_layer:%d  num_display:%d\n",
			g_hwc_info.single_layer, g_hwc_info.num_display);
	dfrc_remake_policy_locked();
	mutex_unlock(&g_mutex_data);
}

void dfrc_set_input_window(const struct DFRC_DRV_INPUT_WINDOW_INFO *input_window_info)
{
	mutex_lock(&g_mutex_data);
	g_input_window_info = *input_window_info;
	DFRC_DBG("dfrc_set_input_window: pid:%d\n", g_input_window_info.pid);
	dfrc_remake_policy_locked();
	mutex_unlock(&g_mutex_data);
}

void dfrc_reset_state(void)
{
	struct list_head *iter, *next;
	struct DFRC_DRV_POLICY_NODE *node;
	struct DFRC_DRV_POLICY_STATISTICS_SET *pss = NULL;
	struct DFRC_DRV_POLICY_STATISTICS *ps = NULL;

	DFRC_INFO("dfrc_reset_state\n");
	mutex_lock(&g_mutex_data);
	g_run_rrc_fps = 1;
	g_allow_rrc_policy = DFRC_ALLOW_VIDEO;
	list_for_each_safe(iter, next, &g_fps_policy_list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list);
		if (node->policy.pid != 0) {
			DFRC_DBG("reset policy[%llu]\n", node->policy.sequence);
			list_del(&node->list);
			g_num_fps_policy--;
			pss = &g_pss[node->policy.mode];
			ps = &pss->statistics[node->policy.api];
			if (node->policy.fps != -1)
				ps->num_valid_policy--;
			pss->num_policy--;
			ps->num_policy--;
			list_del(&node->list_statistics);
			vfree(node);
		}
	}
	dfrc_remake_policy_locked();
	mutex_unlock(&g_mutex_data);
}

long dfrc_get_request_set(struct DFRC_DRC_REQUEST_SET *request_set)
{
	long res = 0L;
	int size;

	mutex_lock(&g_mutex_request);
	if (g_request_policy != NULL && request_set->policy != NULL) {
		size = request_set->num > g_request_notified.num_policy ? g_request_notified.num_policy :
				request_set->num;
		size *= sizeof(struct DFRC_DRV_POLICY);
		if (copy_to_user((void *)request_set->policy, g_request_policy, size)) {
			DFRC_WRN("get_request_set: failed to copy data to user\n");
			res = -EFAULT;
		}
	}
	mutex_unlock(&g_mutex_request);
	return res;
}

void dfrc_get_vsync_request(struct DFRC_DRV_VSYNC_REQUEST *request)
{
	mutex_lock(&g_mutex_request);
	if (!memcmp(request, &g_request_notified, sizeof(g_request_notified))) {
		mutex_unlock(&g_mutex_request);
		wait_event_interruptible(g_wq_vsync_request, g_cond_request);
		mutex_lock(&g_mutex_request);
		g_cond_request = 0;
	}
	*request = g_request_notified;
	mutex_unlock(&g_mutex_request);
}

void dfrc_get_panel_info(struct DFRC_DRV_PANEL_INFO *panel_info)
{
	mutex_lock(&g_mutex_data);
	panel_info->support_120 = g_fps_info.support_120;
	panel_info->support_90 = g_fps_info.support_90;
	panel_info->num = g_fps_info.num;
	mutex_unlock(&g_mutex_data);
}

int dfrc_allow_rrc_adjust_fps(void)
{
	int allow;

	mutex_lock(&g_mutex_data);
	allow = g_allow_rrc_policy;
	mutex_unlock(&g_mutex_data);
	return allow;
}

static int rrc_fps_is_invalid_fps_locked(int fps)
{
	int res = 1;
	int i;

	if (fps == -1) {
		return 0;
	}

	for (i = 0; i < g_fps_info.num; i++) {
		if (g_fps_info.range[i].min_fps <= fps && fps <= g_fps_info.range[i].max_fps) {
			res = 0;
			break;
		}
	}
	return res;
}

void dfrc_set_window_state(const struct DFRC_DRV_WINDOW_STATE *window_state)
{
	mutex_lock(&g_mutex_data);
	g_window_state = *window_state;
	dfrc_remake_policy_locked();
	mutex_unlock(&g_mutex_data);
}

void dfrc_set_fg_window(const struct DFRC_DRV_FOREGROUND_WINDOW_INFO *fg_window_info)
{
	mutex_lock(&g_mutex_data);
	g_fg_window_info = *fg_window_info;
	dfrc_remake_policy_locked();
	mutex_unlock(&g_mutex_data);
}

long dfrc_set_kernel_policy(int api, int fps, int mode, int target_pid, unsigned long long gl_context_id)
{
	long res = 0L;
	struct DFRC_DRV_POLICY *temp;

	mutex_lock(&g_mutex_data);
	if (!g_init_done) {
		res = -ENODEV;
		DFRC_WRN("[RRC_DRV] api_%d failed to set %d fps: not ready\n", api, fps);
		goto set_kernel_policy_exit;
	}

	if (rrc_fps_is_invalid_fps_locked(fps)) {
		res = -EINVAL;
		DFRC_WRN("[RRC_DRV] api_%d failed to set %d fps: fps is invalid\n", api, fps);
		goto set_kernel_policy_exit;
	}

	switch (api) {
	case DFRC_DRV_API_RRC_TOUCH:
		temp = &g_policy_rrc_input;
		break;
	case DFRC_DRV_API_RRC_VIDEO:
		temp = &g_policy_rrc_video;
		break;
	case DFRC_DRV_API_THERMAL:
		temp = &g_policy_thermal;
		break;
	case DFRC_DRV_API_LOADING:
		temp = &g_policy_loading;
		break;
	default:
		DFRC_INFO("[RRC_DRV] api_%d failed to set %d fps: api is invalid\n", api, fps);
		temp = NULL;
		res = -EINVAL;
		break;
	}
	if (temp != NULL) {
		temp->fps = fps;
		temp->mode = mode;
		temp->target_pid = target_pid;
		temp->gl_context_id = gl_context_id;
		dfrc_set_policy_locked(temp);
	}

set_kernel_policy_exit:
	mutex_unlock(&g_mutex_data);

	return res;
}

long dfrc_get_panel_info_number(int *num)
{
	long res = 0L;

	mutex_lock(&g_mutex_data);
	if (!g_init_done) {
		res = -ENODEV;
		DFRC_WRN("failed to get info number: does not init\n");
	}

	if (g_fps_info.num == 0) {
		DFRC_WRN("failed to get info number: size is 0\n");
		*num = 1;
	} else {
		*num = g_fps_info.num;
	}
	mutex_unlock(&g_mutex_data);

	return res;
}

long dfrc_get_panel_fps(struct DFRC_DRV_REFRESH_RANGE *range)
{
	int res = 0;

	mutex_lock(&g_mutex_data);
	if (!g_init_done) {
		res = -ENODEV;
		DFRC_INFO("failed to get panel fps: does not init\n");
	}

	if (g_fps_info.num == 0) {
		range->min_fps = 60;
		range->max_fps = 60;
	} else if (range->index >= g_fps_info.num || range->index < 0) {
		range->min_fps = 60;
		range->max_fps = 60;
		res = -EINVAL;
	} else {
		range->min_fps = g_fps_info.range[range->index].min_fps;
		range->max_fps = g_fps_info.range[range->index].max_fps;
	}

	mutex_unlock(&g_mutex_data);
	return res;
}

long dfrc_get_frr_setting(int pid, unsigned long long gl_context_id, int *fps, int *mode)
{
	int api;

	return dfrc_get_frr_config(pid, gl_context_id, fps, mode, &api);
}

long dfrc_get_frr_config(int pid, unsigned long long gl_context_id, int *fps, int *mode, int *api)
{
	long res = 0;
	struct DFRC_DRV_POLICY *policy = NULL;
	int i, num;

	*fps = DFRC_DRV_FPS_NON_ASSIGN;
	*api = DFRC_DRV_API_NON_ASSIGN;
	mutex_lock(&g_mutex_request);
	*mode = g_request_notified.mode;
	if (*mode == DFRC_DRV_MODE_DEFAULT) {
		*fps = DFRC_DRV_FPS_NON_ASSIGN;
		*api = DFRC_DRV_API_NON_ASSIGN;
	} else if (*mode == DFRC_DRV_MODE_ARR) {
		*fps = g_request_notified.fps;
		if (g_request_policy != NULL)
			*api = g_request_policy->api;
	} else if (*mode == DFRC_DRV_MODE_FRR) {
		num = g_request_notified.num_policy;
		for (i = 0; i < num; i++) {
			if (g_request_policy[i].api != DFRC_DRV_API_GIFT) {
				if (policy == NULL)
					policy = &g_request_policy[i];
				else if (policy->fps > g_request_policy[i].fps)
					policy = &g_request_policy[i];
			} else {
				if (pid == g_request_policy[i].target_pid &&
						gl_context_id == g_request_policy[i].gl_context_id) {
					if (policy == NULL)
						policy = &g_request_policy[i];
					else if (policy->fps > g_request_policy[i].fps)
						policy = &g_request_policy[i];
				}
			}
		}
		if (policy != NULL) {
			*fps = policy->fps;
			*api = policy->api;
		}
	}
	mutex_unlock(&g_mutex_request);
	DFRC_DBG("dfrc_get_frr_setting: pid:%d  gl_context_id:%llu  fps[%d]  mode[%s]\n",
			pid, gl_context_id, *fps, dfrc_mode_string[*mode]);

	return res;
}

static long dfrc_find_pid_setting(int pid, int *fps, int *mode)
{
	long res = 0;
	struct list_head *iter;
	struct DFRC_DRV_POLICY_NODE *node;
	struct DFRC_DRV_POLICY_STATISTICS_SET *set;
	struct DFRC_DRV_POLICY_STATISTICS *statistics;
	struct DFRC_DRV_POLICY *policy;
	bool has_upper_bound = false;
	bool has_app_setting = false;
	int i;
	int bound_fps = 0;
	int app_fps = 0;
	int upper_bound_array[NUM_UPPER_BOUND] = {DFRC_DRV_API_THERMAL, DFRC_DRV_API_LOADING,
						DFRC_DRV_API_WHITELIST};

	mutex_lock(&g_mutex_request);
	*mode = g_request_notified.mode;
	if (*mode == DFRC_DRV_MODE_DEFAULT)
		*fps = DFRC_DRV_FPS_NON_ASSIGN;
	else if (*mode == DFRC_DRV_MODE_ARR)
		*fps = g_request_notified.fps;
	mutex_unlock(&g_mutex_request);

	if (*mode == DFRC_DRV_MODE_FRR) {
		mutex_lock(&g_mutex_data);
		set = &g_pss[DFRC_DRV_MODE_FRR];
		for (i = 0; i < NUM_UPPER_BOUND; i++) {
			statistics = &set->statistics[upper_bound_array[i]];
			if (statistics->num_valid_policy != 0) {
				list_for_each(iter, &statistics->list) {
					node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list_statistics);
					policy = &node->policy;
					if (policy->fps == -1)
						continue;
					if (has_upper_bound && bound_fps > policy->fps) {
						bound_fps = policy->fps;
					} else if (!has_upper_bound) {
						bound_fps = policy->fps;
						has_upper_bound = true;
					}
				}
			}
		}

		statistics = &set->statistics[DFRC_DRV_API_GIFT];
		if (statistics->num_valid_policy != 0) {
			list_for_each(iter, &statistics->list) {
				node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list_statistics);
				policy = &node->policy;
				if (policy->target_pid == pid) {
					if (has_app_setting && app_fps < policy->fps) {
						app_fps = policy->fps;
					} else if (!has_app_setting) {
						app_fps = policy->fps;
						has_app_setting = true;
					}
				}
			}
		}
		mutex_unlock(&g_mutex_data);
		if (has_upper_bound && has_app_setting) {
			if (app_fps < bound_fps)
				*fps = app_fps;
			else
				*fps = bound_fps;
		} else if (has_upper_bound) {
			*fps = bound_fps;
		} else if (has_app_setting) {
			*fps = app_fps;
		} else {
			*fps = DFRC_DRV_FPS_NON_ASSIGN;
		}
	}
	DFRC_DBG("dfrc_find_fg_setting: pid:%d  fps[%d]  mode[%s]\n",
			pid, *fps, dfrc_mode_string[*mode]);

	return res;
}

static long dfrc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct DFRC_DRV_POLICY policy;
	unsigned long long sequence;
	struct DFRC_DRV_HWC_INFO hwc_info;
	struct DFRC_DRV_INPUT_WINDOW_INFO input_window_info;
	struct DFRC_DRV_VSYNC_REQUEST request;
	struct DFRC_DRV_PANEL_INFO panel_info;
	struct DFRC_DRV_REFRESH_RANGE range;
	struct DFRC_DRV_WINDOW_STATE window_state;
	struct DFRC_DRV_FOREGROUND_WINDOW_INFO fg_window_info;
	struct DFRC_DRC_REQUEST_SET request_set;
	long res = 0L;

	switch (cmd) {
	case DFRC_IOCTL_CMD_REG_POLICY:
		if (copy_from_user(&policy, (void *)arg, sizeof(policy))) {
			DFRC_WRN("reg_fps_policy : failed to copy data from user\n");
			return -EFAULT;
		}
		res = dfrc_reg_policy(&policy);
		if (res)
			DFRC_WRN("reg_fps_policy : failed to register fps policy\n");
		break;

	case DFRC_IOCTL_CMD_SET_POLICY:
		if (copy_from_user(&policy, (void *)arg, sizeof(policy))) {
			DFRC_WRN("set_fps_policy : failed to copy data from user\n");
			return -EFAULT;
		}
		res = dfrc_set_policy(&policy);
		if (res)
			DFRC_WRN("set_fps_policy : failed to set fps policy with %dfps\n",
					policy.fps);
		break;

	case DFRC_IOCTL_CMD_UNREG_POLICY:
		if (copy_from_user(&sequence, (void *)arg, sizeof(sequence))) {
			DFRC_WRN("set_unreg_policy : failed to copy data from user\n");
			return -EFAULT;
		}
		res = dfrc_unreg_policy(sequence);
		if (res)
			DFRC_WRN("set_unreg_policy : failed to unreg fps policy\n");
		break;

	case DFRC_IOCTL_CMD_SET_HWC_INFO:
		if (copy_from_user(&hwc_info, (void *)arg, sizeof(hwc_info))) {
			DFRC_WRN("set_hwc_info : failed to copy data from user\n");
			return -EFAULT;
		}
		dfrc_set_hwc_info(&hwc_info);
		break;

	case DFRC_IOCTL_CMD_SET_INPUT_WINDOW:
		if (copy_from_user(&input_window_info, (void *)arg, sizeof(input_window_info))) {
			DFRC_WRN("set_input_window_info : failed to copy data from user\n");
			return -EFAULT;
		}
		dfrc_set_input_window(&input_window_info);
		break;

	case DFRC_IOCTL_CMD_RESET_STATE:
		dfrc_reset_state();
		break;

	case DFRC_IOCTL_CMD_GET_REQUEST_SET:
		if (copy_from_user(&request_set, (void *)arg, sizeof(request_set))) {
			DFRC_WRN("get_request_set: failed to copy data from user\n");
			return -EFAULT;
		}
		res = dfrc_get_request_set(&request_set);
		break;

	case DFRC_IOCTL_CMD_GET_VSYNC_REQUEST:
		if (copy_from_user(&request, (void *)arg, sizeof(request))) {
			DFRC_WRN("get_vsync_request: failed to copy data from user\n");
			return -EFAULT;
		}
		dfrc_get_vsync_request(&request);
		if (copy_to_user((void *)arg, &request, sizeof(request))) {
			DFRC_WRN("get_vsync_request: failed to copy data to user\n");
			return -EFAULT;
		}
		break;

	case DFRC_IOCTL_CMD_GET_PANEL_INFO:
		dfrc_get_panel_info(&panel_info);
		if (copy_to_user((void *)arg, &panel_info, sizeof(panel_info))) {
			DFRC_WRN("get_panel_info: failed to copy data to user\n");
			return -EFAULT;
		}
		break;

	case DFRC_IOCTL_CMD_GET_REFRESH_RANGE:
		if (copy_from_user(&range, (void *)arg, sizeof(range))) {
			DFRC_WRN("get_refresh_range: failed to copy data from user\n");
			return -EFAULT;
		}
		dfrc_get_panel_fps(&range);
		if (copy_to_user((void *)arg, &range, sizeof(range))) {
			DFRC_WRN("get_refresh_range: failed to copy data to user\n");
			return -EFAULT;
		}
		break;

	case DFRC_IOCTL_CMD_SET_WINDOW_STATE:
		if (copy_from_user(&window_state, (void *)arg, sizeof(window_state))) {
			DFRC_WRN("set_window_state : failed to copy data from user\n");
			return -EFAULT;
		}
		dfrc_set_window_state(&window_state);
		break;

	case DFRC_IOCTL_CMD_SET_FOREGROUND_WINDOW:
		if (copy_from_user(&fg_window_info, (void *)arg, sizeof(fg_window_info))) {
			DFRC_WRN("set_fg_window : failed to copy data from user\n");
			return -EFAULT;
		}
		dfrc_set_fg_window(&fg_window_info);
		break;

	default:
		return -ENODATA;
	}

	return res;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_dfrc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case DFRC_IOCTL_CMD_REG_POLICY:
	case DFRC_IOCTL_CMD_SET_POLICY:
	case DFRC_IOCTL_CMD_UNREG_POLICY:
	case DFRC_IOCTL_CMD_SET_HWC_INFO:
	case DFRC_IOCTL_CMD_SET_INPUT_WINDOW:
	case DFRC_IOCTL_CMD_RESET_STATE:
	case DFRC_IOCTL_CMD_GET_REQUEST_SET:
	case DFRC_IOCTL_CMD_GET_VSYNC_REQUEST:
	case DFRC_IOCTL_CMD_GET_PANEL_INFO:
		break;

	default:
		return -ENODATA;
	}

	return 0L;
}
#endif

static void dfrc_idump(const char *fmt, ...)
{
	va_list vargs;
	size_t tmp;

	va_start(vargs, fmt);

	if (g_string_info_len >= DUMP_MAX_LENGTH - 32) {
		va_end(vargs);
		return;
	}

	tmp = vscnprintf(g_string_info + g_string_info_len,
			DUMP_MAX_LENGTH - g_string_info_len, fmt, vargs);
	g_string_info_len += tmp;
	if (g_string_info_len > DUMP_MAX_LENGTH)
		g_string_info_len = DUMP_MAX_LENGTH;

	va_end(vargs);
}

static void dfrc_rdump(const char *fmt, ...)
{
	va_list vargs;
	size_t tmp;

	va_start(vargs, fmt);

	if (g_string_reason_len >= DUMP_MAX_LENGTH - 32) {
		va_end(vargs);
		return;
	}

	tmp = vscnprintf(g_string_reason + g_string_reason_len,
			DUMP_MAX_LENGTH - g_string_reason_len, fmt, vargs);
	g_string_reason_len += tmp;
	if (g_string_reason_len > DUMP_MAX_LENGTH)
		g_string_reason_len = DUMP_MAX_LENGTH;

	va_end(vargs);
}

static void dfrc_dump_info(void)
{
	int i;

	dfrc_idump("Number of display[%d]  Single layer[%d]\n",
			g_hwc_info.num_display, g_hwc_info.single_layer);
	dfrc_idump("Force window pid[%d]\n", g_input_window_info.pid);
	dfrc_idump("Foreground window pid[%d]\n", g_fg_window_info.pid);
	dfrc_idump("Window state[%08x]\n", g_window_state);
	dfrc_idump("Allow RRC policy[0x%x]\n", g_allow_rrc_policy);
	dfrc_idump("Support panel refresh rate: %d\n", g_fps_info.num);
	for (i = 0; i < g_fps_info.num; i++) {
		dfrc_idump("    [%d] %d~%d\n",
				i, g_fps_info.range[i].min_fps, g_fps_info.range[i].max_fps);
	}
}

static void dfrc_dump_policy_list(void)
{
	struct list_head *iter;
	int i = 0;
	struct DFRC_DRV_POLICY_NODE *node;

	dfrc_idump("All Fps Policy\n");
	list_for_each(iter, &g_fps_policy_list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list);
		dfrc_idump("    [%d]  sequence[%llu]  api[%d]  pid[%d]  fps[%d]  mode[%d]  ",
				i, node->policy.sequence, node->policy.api,
				node->policy.pid, node->policy.fps, node->policy.mode);
		dfrc_idump("target_pid[%d]  context_id[%p]\n",
				node->policy.target_pid, node->policy.gl_context_id);
		i++;
	}
}

static void dfrc_dump_statistics_set(void)
{
	struct list_head *iter;
	struct DFRC_DRV_POLICY_STATISTICS_SET *pss;
	struct DFRC_DRV_POLICY_STATISTICS *ps;
	struct DFRC_DRV_POLICY_NODE *node;
	struct DFRC_DRV_POLICY *policy;
	int i, j;

	for (i = 0; i < DFRC_DRV_MODE_MAXIMUM; i++) {
		pss = &g_pss[i];
		dfrc_idump("%s Statistics (%d)\n", dfrc_mode_string[i], pss->num_policy);
		for (j = 0; j < DFRC_DRV_API_MAXIMUM; j++) {
			ps = &pss->statistics[j];
			dfrc_idump("    %s Statistics (%d/%d)\n", dfrc_api_string[j],
					ps->num_valid_policy, ps->num_policy);
			list_for_each(iter, &ps->list) {
				node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list_statistics);
				policy = &node->policy;
				dfrc_idump("        seq[%llu]  api[%d]  pid[%d]  fps[%d]  mode[%d]",
						policy->sequence, policy->api, policy->pid,
						policy->fps, policy->mode);
				dfrc_idump("  t_pid[%d]  context_id[%d]\n",
						policy->target_pid, policy->gl_context_id);
			}
		}
	}
}

static void dfrc_reset_info_buffer(void)
{
	g_string_info_len = 0;
	memset(g_string_info, 0, sizeof(g_string_info));
}

static void dfrc_reset_reason_buffer(void)
{
	g_string_reason_len = 0;
	memset(g_string_reason, 0, sizeof(g_string_reason));
}

static ssize_t dfrc_debug_dump_info_read(struct file *file, char __user *buf, size_t size,
		loff_t *ppos)
{
	mutex_lock(&g_mutex_data);
	dfrc_reset_info_buffer();
	dfrc_dump_info();
	dfrc_dump_policy_list();
	dfrc_dump_statistics_set();
	mutex_unlock(&g_mutex_data);
	return simple_read_from_buffer(buf, size, ppos, g_string_info, g_string_info_len);
}

static ssize_t dfrc_debug_dump_reason_read(struct file *file, char __user *buf, size_t size,
		loff_t *ppos)
{
	ssize_t res;

	mutex_lock(&g_mutex_data);
	res = simple_read_from_buffer(buf, size, ppos, g_string_reason, g_string_reason_len);
	mutex_unlock(&g_mutex_data);
	return res;
}

static struct DFRC_DRV_POLICY_NODE *dfrc_find_min_fps(struct DFRC_DRV_POLICY_STATISTICS *statistics)
{
	struct list_head *iter;
	struct DFRC_DRV_POLICY_NODE *node;
	struct DFRC_DRV_POLICY_NODE *min = NULL;

	list_for_each(iter, &statistics->list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list_statistics);
		if (min == NULL)
			min = node;
		else if (min->policy.fps > node->policy.fps)
			min = node;
	}
	return min;
}

bool dfrc_have_appointed_mode(struct DFRC_DRV_POLICY_STATISTICS *statistics, int pid, int mode)
{
	bool res = false;
	struct list_head *iter;
	struct DFRC_DRV_POLICY_NODE *node;

	list_for_each(iter, &statistics->list) {
		node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list_statistics);
		if (node->policy.mode == mode && node->policy.target_pid == pid) {
			res = true;
			break;
		}
	}

	return res;
}

static void dfrc_select_policy_locked(struct DFRC_DRV_EXPECTED_POLICY *expected_policy)
{
	struct DFRC_DRV_POLICY_STATISTICS_SET *arr_statistics;
	struct DFRC_DRV_POLICY_STATISTICS_SET *frr_statistics;
	struct DFRC_DRV_POLICY_NODE *node;

	expected_policy->mode = DFRC_DRV_MODE_DEFAULT;
	expected_policy->arr_policy = NULL;

	arr_statistics = &g_pss[DFRC_DRV_MODE_ARR];
	frr_statistics = &g_pss[DFRC_DRV_MODE_FRR];
	if (frr_statistics->statistics[DFRC_DRV_API_THERMAL].num_valid_policy) {
		expected_policy->mode = DFRC_DRV_MODE_FRR;
		expected_policy->frr_statistics = frr_statistics;
		dfrc_rdump("choose thermal config with frr\n");
		return;
	} else if (arr_statistics->statistics[DFRC_DRV_API_THERMAL].num_valid_policy) {
		expected_policy->mode = DFRC_DRV_MODE_ARR;
		node = dfrc_find_min_fps(&arr_statistics->statistics[DFRC_DRV_API_THERMAL]);
		expected_policy->arr_policy = &node->policy;
		dfrc_rdump("choose thermal config with arr\n");
	}

	if (frr_statistics->statistics[DFRC_DRV_API_LOADING].num_valid_policy &&
			expected_policy->mode == DFRC_DRV_MODE_DEFAULT) {
		expected_policy->mode = DFRC_DRV_MODE_FRR;
		expected_policy->frr_statistics = frr_statistics;
		dfrc_rdump("choose loading config with frr\n");
		return;
	} else if (arr_statistics->statistics[DFRC_DRV_API_LOADING].num_valid_policy &&
			(expected_policy->mode == DFRC_DRV_MODE_DEFAULT ||
			expected_policy->mode == DFRC_DRV_MODE_ARR)) {
		expected_policy->mode = DFRC_DRV_MODE_ARR;
		node = dfrc_find_min_fps(&arr_statistics->statistics[DFRC_DRV_API_LOADING]);
		if (expected_policy->arr_policy == NULL)
			expected_policy->arr_policy = &node->policy;
		else if (expected_policy->arr_policy->fps > node->policy.fps)
			expected_policy->arr_policy = &node->policy;
		dfrc_rdump("choose loading config with arr\n");
	}

	if (frr_statistics->statistics[DFRC_DRV_API_WHITELIST].num_valid_policy &&
			expected_policy->mode == DFRC_DRV_MODE_DEFAULT) {
		expected_policy->mode = DFRC_DRV_MODE_FRR;
		expected_policy->frr_statistics = frr_statistics;
		dfrc_rdump("choose whitelist config with frr\n");
		return;
	} else if (arr_statistics->statistics[DFRC_DRV_API_WHITELIST].num_valid_policy &&
			(expected_policy->mode == DFRC_DRV_MODE_DEFAULT ||
			expected_policy->mode == DFRC_DRV_MODE_ARR)) {
		expected_policy->mode = DFRC_DRV_MODE_ARR;
		node = dfrc_find_min_fps(&arr_statistics->statistics[DFRC_DRV_API_WHITELIST]);
		if (expected_policy->arr_policy == NULL)
			expected_policy->arr_policy = &node->policy;
		else if (expected_policy->arr_policy->fps > node->policy.fps)
			expected_policy->arr_policy = &node->policy;
		dfrc_rdump("choose whitelist config with arr\n");
	}

	if (frr_statistics->statistics[DFRC_DRV_API_GIFT].num_valid_policy &&
			expected_policy->mode == DFRC_DRV_MODE_DEFAULT) {
		if (dfrc_have_appointed_mode(&frr_statistics->statistics[DFRC_DRV_API_GIFT],
				g_fg_window_info.pid, DFRC_DRV_MODE_FRR)) {
			expected_policy->mode = DFRC_DRV_MODE_FRR;
			expected_policy->frr_statistics = frr_statistics;
			dfrc_rdump("choose gift config with frr\n");
			return;
		}
	}
	if (arr_statistics->statistics[DFRC_DRV_MODE_ARR].num_valid_policy &&
			(expected_policy->mode == DFRC_DRV_MODE_DEFAULT ||
			expected_policy->mode == DFRC_DRV_MODE_ARR)) {
		if (dfrc_have_appointed_mode(&arr_statistics->statistics[DFRC_DRV_API_GIFT],
				g_fg_window_info.pid, DFRC_DRV_MODE_ARR)) {
			expected_policy->mode = DFRC_DRV_MODE_ARR;
			node = dfrc_find_min_fps(&arr_statistics->statistics[DFRC_DRV_API_GIFT]);
			if (expected_policy->arr_policy == NULL)
				expected_policy->arr_policy = &node->policy;
			else if (expected_policy->arr_policy->fps > node->policy.fps)
				expected_policy->arr_policy = &node->policy;
			dfrc_rdump("choose gift config with arr\n");
		}
	}
}

static void dfrc_pack_choosed_frr_policy(int num, struct DFRC_DRV_POLICY *new_policy,
					struct DFRC_DRV_POLICY_STATISTICS_SET *set)
{
	int i = 0, j = 0;
	struct list_head *iter;
	struct DFRC_DRV_POLICY_NODE *node;
	struct DFRC_DRV_POLICY_STATISTICS *statistics;

	for (i = 0; i < DFRC_DRV_API_MAXIMUM; i++) {
		statistics = &set->statistics[i];
		list_for_each(iter, &statistics->list) {
			node = list_entry(iter, struct DFRC_DRV_POLICY_NODE, list_statistics);
			if (node->policy.fps != -1) {
				if (j < num)
					new_policy[j] = node->policy;
				else
					break;
			}
			j++;
		}
	}
}

static void dfrc_adjust_vsync_locked(struct DFRC_DRV_EXPECTED_POLICY *expected_policy)
{
	struct DFRC_DRV_VSYNC_REQUEST new_request;
	int sw_mode = DFRC_DRV_SW_MODE_CALIBRATED_SW;
	int hw_mode = DFRC_DRV_HW_MODE_DEFAULT;
	int fps = -1;
	struct DFRC_DRV_POLICY *new_policy = NULL;
	bool change = false;
	int size = 0;
	int i;

	memset(&new_request, 0, sizeof(new_request));
	if (expected_policy->mode == DFRC_DRV_MODE_DEFAULT) {
		dfrc_rdump("use default mode\n");
		fps = -1;
		sw_mode = DFRC_DRV_SW_MODE_CALIBRATED_SW;
		hw_mode = DFRC_DRV_HW_MODE_DEFAULT;
		new_request.fps = -1;
		new_request.mode = DFRC_DRV_MODE_DEFAULT;
		new_request.sw_mode = DFRC_DRV_SW_MODE_CALIBRATED_SW;
		new_request.valid_info = false;
		new_request.transient_state = false;
		new_request.num_policy = 0;
	} else if (expected_policy->mode == DFRC_DRV_MODE_FRR) {
		dfrc_rdump("use frr mode\n");
		fps = 60;
		sw_mode = DFRC_DRV_SW_MODE_CALIBRATED_SW;
		hw_mode = DFRC_DRV_HW_MODE_DEFAULT;
		new_request.fps = -1;
		new_request.mode = DFRC_DRV_MODE_FRR;
		new_request.sw_fps = 60;
		new_request.sw_mode = DFRC_DRV_SW_MODE_CALIBRATED_SW;
		new_request.valid_info = true;
		new_request.transient_state = false;
		for (i = 0; i < DFRC_DRV_API_MAXIMUM; i++)
			size += expected_policy->frr_statistics->statistics[i].num_valid_policy;
		new_request.num_policy = size;

		new_policy = vmalloc(sizeof(struct DFRC_DRV_POLICY) * size);
		dfrc_pack_choosed_frr_policy(size, new_policy, expected_policy->frr_statistics);
	} else if (expected_policy->mode == DFRC_DRV_MODE_ARR) {
		dfrc_rdump("use arr mode\n");
		fps = expected_policy->arr_policy->fps;
		sw_mode = DFRC_DRV_SW_MODE_PASS_HW;
		hw_mode = DFRC_DRV_HW_MODE_ARR;
		new_request.fps = expected_policy->arr_policy->fps;
		new_request.mode = DFRC_DRV_MODE_ARR;
		new_request.sw_fps = expected_policy->arr_policy->fps;
		new_request.sw_mode = DFRC_DRV_SW_MODE_PASS_HW;
		new_request.valid_info = true;
		new_request.transient_state = false;
		new_request.num_policy = 1;
		if (g_input_window_info.pid != g_fg_window_info.pid &&
				(expected_policy->arr_policy->api != DFRC_DRV_API_THERMAL &&
				expected_policy->arr_policy->api != DFRC_DRV_API_LOADING)) {
			new_request.fps = 60;
			new_request.transient_state = true;
		}

		new_policy = vmalloc(sizeof(struct DFRC_DRV_POLICY));
		*new_policy = *expected_policy->arr_policy;
	}

	if (memcmp(&new_request, &g_request_notified, sizeof(g_request_notified))) {
		change = true;
	} else {
		if ((new_policy != NULL && g_request_policy == NULL) ||
				(new_policy == NULL && g_request_policy != NULL)) {
			change = true;
		} else if (new_policy != NULL && g_request_policy != NULL) {
			size = (new_request.num_policy > g_request_notified.num_policy) ?
					g_request_notified.num_policy : new_request.num_policy;
			size *= sizeof(struct DFRC_DRV_POLICY);
			if (memcmp(new_policy, g_request_policy, size)) {
				change = true;
			}
		}
	}

	if (change) {
		mutex_lock(&g_mutex_request);
		g_request_notified = new_request;
		if (g_request_policy != NULL)
			vfree(g_request_policy);
		g_request_policy = new_policy;
		g_cond_request = 1;
		wake_up_interruptible(&g_wq_vsync_request);
		mutex_unlock(&g_mutex_request);
	} else {
		vfree(new_policy);
	}

	if (fps != g_current_fps || hw_mode != g_current_hw_mode) {
		/* adjust hw vsync */
	}

	if (change) {
		DFRC_INFO("adjust vsync: [%d|%d|%d] -> [%d|%d|%d]\n",
				g_current_fps, g_current_sw_mode, g_current_hw_mode,
				fps, sw_mode, hw_mode);
		g_current_fps = fps;
		g_current_sw_mode = sw_mode;
		g_current_hw_mode = hw_mode;
	}
}

static void dfrc_send_fps_info_to_other_module(void)
{
	int fps = DFRC_DRV_FPS_NON_ASSIGN;
	int mode = DFRC_DRV_MODE_ARR;
	int pid = DFRC_DRV_API_NON_ASSIGN;

	mutex_lock(&g_mutex_data);
	pid = g_fg_window_info.pid;
	mutex_unlock(&g_mutex_data);

	dfrc_find_pid_setting(pid, &fps, &mode);
	if (dfrc_fps_limit_cb)
		dfrc_fps_limit_cb(fps);
}

static int dfrc_make_policy_kthread_func(void *data)
{
	struct DFRC_DRV_EXPECTED_POLICY expected_policy;
	struct sched_param param = { .sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);
	while (true) {
		mutex_lock(&g_mutex_data);
		if (g_stop_thread) {
			mutex_unlock(&g_mutex_data);
			return 0;
		}

		/* if count is not equal to g_event_count, we has new request. */
		if (g_processed_count == g_event_count) {
			mutex_unlock(&g_mutex_data);
			wait_event_interruptible(g_wq_make_policy, g_cond_remake);
			mutex_lock(&g_mutex_data);
			g_cond_remake = 0;
		}

		dfrc_reset_reason_buffer();
		dfrc_select_policy_locked(&expected_policy);
		dfrc_adjust_vsync_locked(&expected_policy);
		g_processed_count = g_event_count;
		mutex_unlock(&g_mutex_data);
		dfrc_send_fps_info_to_other_module();
	}

	return 0;
}

static int dfrc_init_param(void)
{
	mutex_init(&g_mutex_data);
	mutex_init(&g_mutex_request);
	mutex_init(&g_mutex_fop);
	mutex_init(&g_mutex_rrc);
	init_waitqueue_head(&g_wq_make_policy);
	init_waitqueue_head(&g_wq_vsync_request);
	init_waitqueue_head(&g_wq_fps_change);

	/* init thread to decide fps setting */
	g_task_make_policy = kthread_create(dfrc_make_policy_kthread_func, NULL,
			"dfrc_make_policy_kthread_func");
	if (IS_ERR(g_task_make_policy)) {
		DFRC_ERR("dfrc_init_param: failed to create dfrc_make_policy_kthread_func\n");
		return -ENODEV;
	}
	wake_up_process(g_task_make_policy);

	dfrc_rdump("Use initial setting\n");

	return 0;
}

static int dfrc_open(struct inode *inode, struct file *file)
{
	unsigned int *pStatus;
	int res = 0;

	mutex_lock(&g_mutex_fop);
	if (g_has_opened) {
		DFRC_WRN("device is busy\n");
		res = -EBUSY;
		goto open_exit;
	}

	file->private_data = vmalloc(sizeof(unsigned int));

	if (file->private_data == NULL) {
		DFRC_WRN("Not enough entry for RRC open operation\n");
		res = -ENOMEM;
		goto open_exit;
	}

	pStatus = (unsigned int *)file->private_data;
	*pStatus = 0;
	g_has_opened = 1;

open_exit:
	mutex_unlock(&g_mutex_fop);

	return res;
}

static int dfrc_release(struct inode *inode, struct file *file)
{
	int res = 0;

	mutex_lock(&g_mutex_fop);
	if (!g_has_opened) {
		res = -ENXIO;
		goto release_exit;
	}

	if (file->private_data != NULL) {
		vfree(file->private_data);
		file->private_data = NULL;
	}
	g_has_opened = 0;

release_exit:
	mutex_unlock(&g_mutex_fop);

	return res;
}

static const struct file_operations dfrc_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = dfrc_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = compat_dfrc_ioctl,
#endif
	.open           = dfrc_open,
	.release        = dfrc_release,
};

static const struct file_operations debug_fops_info = {
	.read = dfrc_debug_dump_info_read,
};

static const struct file_operations debug_fops_reason = {
	.read = dfrc_debug_dump_reason_read,
};

static void dfrc_debug_init(void)
{
	dfrc_reset_info_buffer();
	dfrc_reset_reason_buffer();

	if (!debug_init) {
		debug_init = 1;
		debug_dir = debugfs_create_dir("dfrc", NULL);
		if (debug_dir) {
			debugfs_dump_info = debugfs_create_file("info",
					S_IFREG | S_IRUGO, debug_dir, NULL,
					&debug_fops_info);

			debugfs_dump_reason = debugfs_create_file("reason",
					S_IFREG | S_IRUGO, debug_dir, NULL,
					&debug_fops_reason);
		}
	}
}

static void dfrc_init_variable(void)
{
	int i, j;
	struct DFRC_DRV_POLICY_STATISTICS *statistics;
	struct DFRC_DRV_POLICY_STATISTICS_SET *set;

	for (i = 0; i < DFRC_DRV_MODE_MAXIMUM; i++) {
		set = &g_pss[i];
		set->num_policy = 0;
		for (j = 0; j < DFRC_DRV_API_MAXIMUM; j++) {
			statistics = &set->statistics[j];
			statistics->num_policy = 0;
			statistics->num_valid_policy = 0;
			INIT_LIST_HEAD(&statistics->list);
		}
	}
}

static int dfrc_probe(struct platform_device *pdev)
{
	struct class_device *class_dev = NULL;
	int ret = 0;

	ret = alloc_chrdev_region(&dfrc_devno, 0, 1, DFRC_DEVNAME);
	if (ret)
		DFRC_ERR("Can't Get Major number for FPS policy Device\n");
	else
		DFRC_DBG("Get FPS policy Device Major number (%d)\n", dfrc_devno);
	dfrc_cdev = cdev_alloc();
	dfrc_cdev->owner = THIS_MODULE;
	dfrc_cdev->ops = &dfrc_fops;
	ret = cdev_add(dfrc_cdev, dfrc_devno, 1);
	dfrc_class = class_create(THIS_MODULE, DFRC_DEVNAME);
	class_dev = (struct class_device *)device_create(dfrc_class, NULL,
			dfrc_devno, NULL, DFRC_DEVNAME);

	dfrc_debug_init();
	dfrc_init_variable();

	g_init_done = 1;
	g_fps_info.support_120 = 0;
	g_fps_info.support_90 = 0;
	g_fps_info.num = 2;
	g_fps_info.range = vmalloc(sizeof(struct DFRC_DRV_REFRESH_RANGE) * g_fps_info.num);
	g_fps_info.range[0].min_fps = 30;
	g_fps_info.range[0].max_fps = 60;
	g_fps_info.range[1].min_fps = 15;
	g_fps_info.range[1].max_fps = 30;

	dfrc_init_kernel_policy();

	return ret;
}

static int dfrc_remove(struct platform_device *pdev)
{
	DFRC_DBG("start RRC FPS driver remove\n");
	device_destroy(dfrc_class, dfrc_devno);
	class_destroy(dfrc_class);
	cdev_del(dfrc_cdev);
	unregister_chrdev_region(dfrc_devno, 1);
	DFRC_DBG("done RRC FPS driver remove\n");

	return 0;
}

static void dfrc_shutdown(struct platform_device *pdev)
{
}

static int dfrc_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int dfrc_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver dfrc_driver = {
	.probe          = dfrc_probe,
	.remove         = dfrc_remove,
	.shutdown       = dfrc_shutdown,
	.suspend        = dfrc_suspend,
	.resume         = dfrc_resume,
	.driver = {
		.name = DFRC_DEVNAME,
	},
};

static void dfrc_device_release(struct device *dev)
{
}

static u64 dfrc_dmamask = ~(u32)0;

static struct platform_device dfrc_device = {
	.name    = DFRC_DEVNAME,
	.id     = 0,
	.dev    = {
		.release = dfrc_device_release,
		.dma_mask = &dfrc_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = 0,
};

static int __init dfrc_init(void)
{
	int res = 0;

	DFRC_INFO("start to initialize fps policy\n");

	DFRC_INFO("register fps policy device\n");
	if (platform_device_register(&dfrc_device)) {
		DFRC_ERR("failed to register fps policy device\n");
		res = -ENODEV;
		return res;
	}

	DFRC_INFO("register fps policy driver\n");
	if (platform_driver_register(&dfrc_driver)) {
		DFRC_ERR("failed to register fps policy driver\n");
		res = -ENODEV;
		return res;
	}

	res = dfrc_init_param();

	return res;
}

static void __exit dfrc_exit(void)
{
	platform_driver_unregister(&dfrc_driver);
	platform_device_unregister(&dfrc_device);
}

module_init(dfrc_init);
module_exit(dfrc_exit);
MODULE_AUTHOR("Jih-Cheng, Chiu <Jih-Cheng.Chiu@mediatek.com>");
MODULE_DESCRIPTION("FPS policy driver");
MODULE_LICENSE("GPL");
