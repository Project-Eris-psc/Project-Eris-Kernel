/*
 * Copyright (c) 2014 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <drm/drmP.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_debugfs.h"

struct mtk_drm_debugfs_table {
	int comp_id;
	char name[8];
	unsigned int offset[2];
	unsigned int length[2];
	unsigned int reg_base;
};


/* ------------------------------------------------------------------------- */
/* External variable declarations */
/* ------------------------------------------------------------------------- */
void __iomem *gdrm_disp_base[6];
void __iomem *gdrm_hdmi_base[3];
struct mtk_drm_debugfs_table gdrm_disp_table[6] = {
	{ DDP_COMPONENT_OVL0, "OVL0 ", {0, 0xf40}, {0x260, 0x80} },
	{ DDP_COMPONENT_RDMA0, "RDMA0 ", {0, 0}, {0x100, 0} },
	{ DDP_COMPONENT_COLOR0, "COLOR0 ", {0x400, 0xc00}, {0x100, 0x100} },
	{ DDP_COMPONENT_BLS, "BLS ", {0, 0}, {0x100, 0} },
	{ -1, "CONFIG ", {0, 0}, {0x120, 0} },
	{ -1, "MUTEX ", {0, 0}, {0x100, 0} }
};

struct mtk_drm_debugfs_table gdrm_hdmi_table[3] = {
	{ DDP_COMPONENT_OVL1, "OVL1 ", {0, 0xf40}, {0x260, 0x80} },
	{ -1, "CONFIG ", {0, 0}, {0x120, 0} },
	{ -1, "MUTEX ", {0, 0}, {0x100, 0} }
};
static bool dbgfs_alpha;

/* ------------------------------------------------------------------------- */
/* Debug Options */
/* ------------------------------------------------------------------------- */
static char STR_HELP[] =
	"\n"
	"USAGE\n"
	"        echo [ACTION]... > mtkdrm\n"
	"\n"
	"ACTION\n"
	"\n"
	"        dump:\n"
	"             dump all hw registers\n"
	"\n"
	"        regw:addr=val\n"
	"             write hw register\n"
	"\n"
	"        regr:addr\n"
	"             read hw register\n";

/* ------------------------------------------------------------------------- */
/* Command Processor */
/* ------------------------------------------------------------------------- */
static void process_dbg_opt(const char *opt)
{
	if (strncmp(opt, "regw:", 5) == 0) {
		char *p = (char *)opt + 5;
		char *np;
		unsigned long addr, val;
		int i;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr))
			goto error;

		if (p == NULL)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val))
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp_table); i++) {
			if (addr > gdrm_disp_table[i].reg_base &&
			    addr < gdrm_disp_table[i].reg_base + 0x1000) {
				writel(val, gdrm_disp_base[i] + addr -
					gdrm_disp_table[i].reg_base);
				break;
			}
		}

	} else if (strncmp(opt, "regr:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned long addr;
		int i;

		if (kstrtoul(p, 16, &addr))
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp_table); i++) {
			if (addr >= gdrm_disp_table[i].reg_base &&
			addr < gdrm_disp_table[i].reg_base + 0x1000) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp_table[i].name, addr,
					readl(gdrm_disp_base[i] + addr -
						gdrm_disp_table[i].reg_base));
				break;
			}
		}

	} else if (strncmp(opt, "dump:", 5) == 0) {
		int i, j;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp_table); i++) {
			if (gdrm_disp_base[i] == NULL)
				continue;
			for (j = gdrm_disp_table[i].offset[0];
			     j < gdrm_disp_table[i].offset[0] +
			     gdrm_disp_table[i].length[0]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_disp_table[i].name,
					gdrm_disp_table[i].reg_base + j,
					readl(gdrm_disp_base[i] + j),
					readl(gdrm_disp_base[i] + j + 0x4),
					readl(gdrm_disp_base[i] + j + 0x8),
					readl(gdrm_disp_base[i] + j + 0xc));

			for (j = gdrm_disp_table[i].offset[1];
			     j < gdrm_disp_table[i].offset[1] +
			     gdrm_disp_table[i].length[1]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_disp_table[i].name,
					gdrm_disp_table[i].reg_base + j,
					readl(gdrm_disp_base[i] + j),
					readl(gdrm_disp_base[i] + j + 0x4),
					readl(gdrm_disp_base[i] + j + 0x8),
					readl(gdrm_disp_base[i] + j + 0xc));
		}
	} else if (strncmp(opt, "hdmi:", 5) == 0) {
		int i, j;

		for (i = 0; i < ARRAY_SIZE(gdrm_hdmi_table); i++) {
			for (j = gdrm_hdmi_table[i].offset[0];
			     j < gdrm_hdmi_table[i].offset[0] +
			     gdrm_hdmi_table[i].length[0]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_hdmi_table[i].name,
					gdrm_hdmi_table[i].reg_base + j,
					readl(gdrm_hdmi_base[i] + j),
					readl(gdrm_hdmi_base[i] + j + 0x4),
					readl(gdrm_hdmi_base[i] + j + 0x8),
					readl(gdrm_hdmi_base[i] + j + 0xc));

			for (j = gdrm_hdmi_table[i].offset[1];
			     j < gdrm_hdmi_table[i].offset[1] +
			     gdrm_hdmi_table[i].length[1]; j += 16)
				DRM_INFO("%8s 0x%08X: %08X %08X %08X %08X\n",
					gdrm_hdmi_table[i].name,
					gdrm_hdmi_table[i].reg_base + j,
					readl(gdrm_hdmi_base[i] + j),
					readl(gdrm_hdmi_base[i] + j + 0x4),
					readl(gdrm_hdmi_base[i] + j + 0x8),
					readl(gdrm_hdmi_base[i] + j + 0xc));
		}
	} else if (strncmp(opt, "alpha", 5) == 0) {
		if (dbgfs_alpha) {
			DRM_INFO("set src alpha to src alpha\n");
			dbgfs_alpha = false;
		} else {
			DRM_INFO("set src alpha to ONE\n");
			dbgfs_alpha = true;
		}
	} else if (strncmp(opt, "debug:", 6) == 0) {
		char *p = (char *)opt + 6;
		unsigned long debug_level;

		if (kstrtoul(p, 16, &debug_level))
			goto error;

		mtk_drm_debug = debug_level;
		DRM_INFO("set mtk_drm_debug to 0x%X\n", mtk_drm_debug);
	} else {
	    goto error;
	}

	return;
 error:
	DRM_ERROR("Parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DRM_INFO("[mtkdrm_dbg] %s\n", cmd);
	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* ------------------------------------------------------------------------- */
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
				       strlen(STR_HELP));
}

static char dis_cmd_buf[512];
static ssize_t debug_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(dis_cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&dis_cmd_buf, ubuf, count))
		return -EFAULT;

	dis_cmd_buf[count] = 0;

	process_dbg_cmd(dis_cmd_buf);

	return ret;
}

struct dentry *mtkdrm_dbgfs;
static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

bool force_alpha(void)
{
	return dbgfs_alpha;
}

void mtk_drm_debugfs_init(struct drm_device *dev,
			  struct mtk_drm_private *priv)
{
	void __iomem *mutex_regs;
	unsigned int mutex_phys;
	struct device_node *np;
	struct resource res;
	int i;

	MTK_DRM_DEBUG_DRIVER("\n");
	mtkdrm_dbgfs = debugfs_create_file("mtkdrm", S_IFREG | S_IRUGO |
			S_IWUSR | S_IWGRP, NULL, (void *)0, &debug_fops);

	for (i = 0; gdrm_disp_table[i].comp_id >= 0; i++) {
		np = priv->comp_node[gdrm_disp_table[i].comp_id];

		if (!np) {
			DRM_INFO("main path %d comp_node[%d] invalid\n", i, gdrm_disp_table[i].comp_id);
			continue;
		}
		
		gdrm_disp_base[i] = of_iomap(np, 0);
		of_address_to_resource(np, 0, &res);
		gdrm_disp_table[i].reg_base = res.start;
	}
	gdrm_disp_base[i] = priv->config_regs;
	gdrm_disp_table[i++].reg_base = 0x14000000;
	mutex_regs = of_iomap(priv->mutex_node, 0);
	of_address_to_resource(priv->mutex_node, 0, &res);
	mutex_phys = res.start;
	gdrm_disp_base[i] = mutex_regs;
	gdrm_disp_table[i++].reg_base = mutex_phys;

	for (i = 0; gdrm_hdmi_table[i].comp_id >= 0; i++) {
		np = priv->comp_node[gdrm_hdmi_table[i].comp_id];

		if (!np) {
			DRM_INFO("ext path %d comp_node[%d] invalid\n", i, gdrm_hdmi_table[i].comp_id);
			continue;
		}

		gdrm_hdmi_base[i] = of_iomap(np, 0);
		of_address_to_resource(np, 0, &res);
		gdrm_hdmi_table[i].reg_base = res.start;
	}
	gdrm_hdmi_base[i++] = priv->config_regs;
	gdrm_hdmi_base[i] = mutex_regs;
	gdrm_hdmi_table[i].reg_base = mutex_phys;

	MTK_DRM_DEBUG_DRIVER("..done\n");
}

void mtk_drm_debugfs_deinit(void)
{
	debugfs_remove(mtkdrm_dbgfs);
}

void mtk_drm_ut_debug_printk(const char *function_name, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk(KERN_ERR "[" DRM_NAME ":%s] %pV", function_name, &vaf);

	va_end(args);
}