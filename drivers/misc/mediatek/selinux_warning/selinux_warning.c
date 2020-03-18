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

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/mm.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/audit.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/freezer.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/signal.h>
#include "selinux_warning.h"

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#define PRINT_BUF_LEN   100
#define MOD		"SELINUX"
/* #define ENABLE_CURRENT_NE_CORE_DUMP */

#ifdef ENABLE_CURRENT_NE_CORE_DUMP
#include <mt-plat/mtk_sysenv.h>
static atomic_t ne_warning_count;
#define POLLING_NE_PROCESS_COUNT     5
#endif


const char *aee_filter_list[AEE_FILTER_NUM] = {
/*	"u:r:adbd:s0", */
	"u:r:bootanim:s0",
	"u:r:bluetooth:s0",
	"u:r:binderservicedomain:s0",
/*	"u:r:clatd:s0", */
	"u:r:dex2oat:s0",
/*	"u:r:debuggerd:s0", */
	"u:r:dhcp:s0",
	"u:r:dnsmasq:s0",
/*	"u:r:drmserver:s0", */
	"u:r:dumpstate:s0",
	"u:r:gpsd:s0",
	"u:r:healthd:s0",
	"u:r:hci_attach:s0",
	"u:r:hostapd:s0",
	"u:r:inputflinger:s0",
/*	"u:r:installd:s0", */
	"u:r:isolated_app:s0",
	"u:r:keystore:s0",
	"u:r:lmkd:s0",
	"u:r:mdnsd:s0",
	"u:r:logd:s0",
/*	"u:r:mediaserver:s0", */
	"u:r:mtp:s0",
	"u:r:netd:s0",
	"u:r:nfc:s0",
	"u:r:ppp:s0",
/*	"u:r:platform_app:s0", */
	"u:r:racoon:s0",
/*	"u:r:radio:s0", */
	"u:r:recovery:s0",
	"u:r:rild:s0",
	"u:r:runas:s0",
	"u:r:sdcardd:s0",
/*	"u:r:servicemanager:s0", */
	"u:r:shared_relro:s0",
/*	"u:r:shell:s0", */
/*	"u:r:system_app:s0", */
	"u:r:system_server:s0",
/*	"u:r:surfaceflinger:s0", */
	"u:r:tee:s0",
	"u:r:uncrypt:s0",
	"u:r:watchdogd:s0",
	"u:r:wpa:s0",
	"u:r:ueventd:s0",
	"u:r:vold:s0",
	"u:r:vdc:s0",
/*	"u:r:untrusted_app:s0", */
	"u:r:zygote:s0",
};

static int mtk_check_filter(char *scontext);
static int mtk_get_scontext(char *data, char *buf);
static int mtk_check_filter(char *scontext)
{
	int i = 0;

	/*check whether scontext in filter list */
	for (i = 0; i < AEE_FILTER_NUM && aee_filter_list[i] != NULL; i++) {
		if (strcmp(scontext, aee_filter_list[i]) == 0)
			return i;
	}

	return -1;
}


static int mtk_get_scontext(char *data, char *buf)
{
	char *t1;
	char *t2;
	int diff = 0;

	t1 = strstr(data, "scontext=");

	if (t1 == NULL)
		return 0;

	t1 += 9;
	t2 = strchr(t1, ' ');

	if (t2 == NULL)
		return 0;

	diff = t2 - t1;
	strncpy(buf, t1, diff);
	return 1;
}


static char *mtk_get_process(char *in)
{
	char *out = in;
	char *tmp;
	int i;

	/*Omit two ':' */
	for (i = 0; i < 2; i++) {
		out = strchr(out, ':');
		if (out == NULL)
			return 0;
		out = out + 1;
	}

	tmp = strchr(out, ':');

	if (tmp == NULL)
		return 0;

	*tmp = '\0';
	return out;
}

void mtk_audit_hook(char *data)
{
	char scontext[AEE_FILTER_LEN] = { '\0' };
	char *pname = scontext;
	int ret = 0;

	/*get scontext from avc warning */
	ret = mtk_get_scontext(data, scontext);
	if (!ret)
		return;

	/*check scontext is in warning list */
	ret = mtk_check_filter(scontext);
	if (ret >= 0) {
		pr_warn("[%s]Enforce: %d, In AEE Warning List scontext: %s\n", MOD,
			selinux_enforcing, scontext);
		pname = mtk_get_process(scontext);
#ifdef CONFIG_MTK_AEE_FEATURE
		if (pname != 0) {
			char printbuf[PRINT_BUF_LEN] = { '\0' };

			#ifdef ENABLE_CURRENT_NE_CORE_DUMP
			int count = 0;
			struct task_struct *task;
			pid_t pid = current->pid;  /* pid need dump */
			pid_t tgid = current->tgid;
			char *selinux_ne = get_env("selinux_ne");

			if (selinux_ne != NULL) {
				long ne_option;
				int err = kstrtol(selinux_ne, 10, &ne_option);

				if (err || (ne_option != 1)) {
					pr_warn("[%s] invalid ne option:%ld, err:%d\n", MOD, ne_option, err);
					return;
				}
			} else {
				pr_debug("[%s] ne option is null\n", MOD);
				return;
			}


			if (atomic_read(&ne_warning_count) >= 1)
				return;

			send_sig(SIGSEGV, current, 0);
			atomic_inc(&ne_warning_count);
			#endif

			snprintf(printbuf, PRINT_BUF_LEN-1, "[%s][WARNING]\nCR_DISPATCH_PROCESSNAME:%s\n",
				MOD, pname);

			if (selinux_enforcing) {
				aee_kernel_warning_api(__FILE__, __LINE__,
						       DB_OPT_DEFAULT | DB_OPT_NATIVE_BACKTRACE,
						       printbuf, data);
			}

			#ifdef ENABLE_CURRENT_NE_CORE_DUMP

			/* poll NE process and wait for its exitence */
			while (count < POLLING_NE_PROCESS_COUNT) {
				rcu_read_lock();
				task = find_task_by_vpid(pid);
				rcu_read_unlock();

				if (task == NULL) {
					pr_warn("[%s] pid: %d exist.\n", MOD, pid);
					break;  /* pid exit, safe to return */
				}
				/* wait two more seconds */
				pr_warn("[%s] pid: %d , tgid: %d still exist, wait for two secs. (%d)\n",
					MOD, pid, tgid, count);
				msleep(2000);
				count++;
			}
			#endif
		}

#endif
	}
}
EXPORT_SYMBOL(mtk_audit_hook);
