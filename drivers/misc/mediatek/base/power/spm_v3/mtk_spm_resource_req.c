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

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include "mtk_spm_resource_req.h"

#define NF_SPM_USER_USAGE_STRUCT	2

DEFINE_SPINLOCK(spm_resource_desc_update_lock);

struct spm_resource_desc {
	int id;
	unsigned int user_usage[NF_SPM_USER_USAGE_STRUCT];        /* 64 bits */
	unsigned int user_usage_mask[NF_SPM_USER_USAGE_STRUCT];   /* 64 bits */
};

static struct spm_resource_desc resc_desc[NF_SPM_RESOURCE];

static const char * const spm_resource_name[] = {
	"mainpll",
	"dram",
	"26m",
	"axi_bus"
};

static struct dentry *spm_resource_req_file;

bool spm_resource_req(unsigned int user, unsigned int req_mask)
{
	int i;
	int value = 0;
	unsigned int field = 0;
	unsigned int offset = 0;
	unsigned long flags;

	if (user >= NF_SPM_RESOURCE_USER)
		return false;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	for (i = 0; i < NF_SPM_RESOURCE; i++) {

		value = !!(req_mask & (1 << i));
		field = user / 32;
		offset = user % 32;

		if (value)
			resc_desc[i].user_usage[field] |= (1 << offset);
		else
			resc_desc[i].user_usage[field] &= ~(1 << offset);
	}

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);

	return true;
}

unsigned int spm_get_resource_usage(void)
{
	int i, k;
	unsigned int resource_usage = 0;
	int resource_in_use = 0;
	unsigned long flags;

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	for (i = 0; i < NF_SPM_RESOURCE; i++) {

		unsigned int usage[NF_SPM_USER_USAGE_STRUCT] = {0};

		for (k = 0; k < NF_SPM_USER_USAGE_STRUCT; k++)
			usage[k] = resc_desc[i].user_usage[k] & resc_desc[i].user_usage_mask[k];

		resource_in_use = !!(usage[0] | usage[1]);

		if (resource_in_use)
			resource_usage |= (1 << i);
	}

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);

	return resource_usage;
}

/*
 * debugfs
 */
#define NF_CMD_BUF		128
#define DBG_BUF_LEN		4096

static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

static int _resource_req_open(struct seq_file *s, void *data)
{
	return 0;
}

static int resource_req_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _resource_req_open, inode->i_private);
}

static ssize_t resource_req_read(struct file *filp,
			       char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	for (i = 0; i < NF_SPM_RESOURCE; i++) {
		p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "resource_req_bypass_stat[%s] = %x %x\n",
						spm_resource_name[i],
						~resc_desc[i].user_usage_mask[1],
						~resc_desc[i].user_usage_mask[0]);
	}

	p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "enable:\n");
	p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "echo enable [bit] > /d/spm/resource_req\n");
	p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "bypass:\n");
	p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "echo bypass [bit] > /d/spm/resource_req\n");
	p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "\n");
	p += snprintf(p, DBG_BUF_LEN - strlen(dbg_buf), "[1]: UFS, [2]: SSUSB\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t resource_req_write(struct file *filp,
				const char __user *userbuf, size_t count, loff_t *f_pos)
{
	char cmd[NF_CMD_BUF];
	int i;
	int param;
	unsigned int field = 0;
	unsigned int offset = 0;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "enable")) {

			field = param / 32;
			offset = param % 32;

			for (i = 0; i < NF_SPM_RESOURCE; i++)
				resc_desc[i].user_usage_mask[field] |= (1 << offset);
		} else if (!strcmp(cmd, "bypass")) {

			field = param / 32;
			offset = param % 32;

			for (i = 0; i < NF_SPM_RESOURCE; i++)
				resc_desc[i].user_usage_mask[field] &= ~(1 << offset);
		}
		return count;
	}

	return -EINVAL;
}

static const struct file_operations resource_req_fops = {
	.open = resource_req_open,
	.read = resource_req_read,
	.write = resource_req_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void spm_resource_req_debugfs_init(struct dentry *spm_dir)
{
	spm_resource_req_file =
			debugfs_create_file("resource_req",
								S_IRUGO,
								spm_dir,
								NULL,
								&resource_req_fops);
}

bool spm_resource_req_init(void)
{
	int i, k;

	for (i = 0; i < NF_SPM_RESOURCE_USER; i++) {
		resc_desc[i].id = i;

		for (k = 0; k < NF_SPM_USER_USAGE_STRUCT; k++) {
			resc_desc[i].user_usage[k] = 0;
			resc_desc[i].user_usage_mask[k] = 0xFFFFFFFF;
		}
	}

	return true;
}

/* Debug only */
void spm_resource_req_dump(void)
{
	int i;
	unsigned long flags;

	pr_err("resource_req:\n");

	spin_lock_irqsave(&spm_resource_desc_update_lock, flags);

	for (i = 0; i < NF_SPM_RESOURCE; i++)
		pr_err("[%s]: %x, %x, mask = %x, %x\n",
				spm_resource_name[i],
				resc_desc[i].user_usage[0],
				resc_desc[i].user_usage[1],
				resc_desc[i].user_usage_mask[0],
				resc_desc[i].user_usage_mask[1]);

	spin_unlock_irqrestore(&spm_resource_desc_update_lock, flags);
}

