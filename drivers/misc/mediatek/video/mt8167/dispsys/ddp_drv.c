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

#define LOG_TAG "ddp_drv"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <mt-plat/mtk_smi.h>
#include <linux/proc_fs.h>	/* proc file use */
#include <linux/miscdevice.h>
/* ION */
/* #include <linux/ion.h> */
/* #include <linux/ion_drv.h> */
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
/*#include <mach/irqs.h>*/
/* #include <mach/mt_reg_base.h> */
/* #include <mach/mt_irq.h> */
/* #include <mach/irqs.h> */
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
/* #include <mach/mt_irq.h> */
#include <mt-plat/sync_write.h>

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif

#include "disp_drv_platform.h"
#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_hal.h"
#include "ddp_log.h"
#include "ddp_irq.h"
#include "ddp_info.h"

#ifndef DISP_NO_DPI
#include "ddp_dpi_reg.h"
#endif

#include "ddp_debug.h"
#include "ddp_path.h"

/* for sysfs */
#include <linux/kobject.h>
#include <linux/sysfs.h>


#define DISP_DEVNAME "DISPSYS"

struct disp_node_struct {
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head testList;
	spinlock_t node_lock;
};


static struct kobject kdispobj;

/*****************************************************************************/
/* sysfs for access information */
/* --------------------------------------// */
static ssize_t disp_kobj_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	int size = 0x0;

	if (strcmp(attr->name, "dbg1") == 0)
		size = ddp_dump_reg_to_buf(2, (unsigned long *)buffer);	/* DISP_MODULE_RDMA */
	else if (strcmp(attr->name, "dbg2") == 0)
		size = ddp_dump_reg_to_buf(1, (unsigned long *)buffer);	/* DISP_MODULE_OVL */
	else if (strcmp(attr->name, "dbg3") == 0)
		size = ddp_dump_reg_to_buf(0, (unsigned long *)buffer);	/* DISP_MODULE_WDMA0 */
	else if (strcmp(attr->name, "dbg4") == 0)
		size = ddp_dump_lcm_param_to_buf(0, (unsigned long *)buffer); /* DISP_MODULE_LCM */

	return size;
}

/* --------------------------------------// */

static struct kobj_type disp_kobj_ktype = {
	.sysfs_ops = &(const struct sysfs_ops){
		.show = disp_kobj_show,
		.store = NULL
	},
	.default_attrs = (struct attribute *[]){
		&(struct attribute){
			.name = "dbg1",	/* disp, dbg1 */
			.mode = S_IRUGO
		},
		&(struct attribute){
			.name = "dbg2",	/* disp, dbg2 */
			.mode = S_IRUGO
		},
		&(struct attribute){
			.name = "dbg3",	/* disp, dbg3 */
			.mode = S_IRUGO
		},
		&(struct attribute){
			.name = "dbg4",	/* disp, dbg4 */
			.mode = S_IRUGO
		},
		NULL
	}
};

static long disp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (cmd == DISP_IOCTL_SET_TPLAY_HANDLE) {
		unsigned int value;

		if (copy_from_user(&value, (void *)arg, sizeof(unsigned int))) {
			DDPERR("DISP_IOCTL_SET_TPLAY_HANDLE, copy_from_user failed\n");
			return -EFAULT;
		}
		if (disp_path_notify_tplay_handle(value)) {
			DDPERR
			    ("DISP_IOCTL_SET_TPLAY_HANDLE, disp_path_notify_tplay_handle failed\n");
			return -EFAULT;
		}
	}
#endif

	return 0;
}

static long disp_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if (cmd == DISP_IOCTL_SET_TPLAY_HANDLE)
		return disp_unlocked_ioctl(file, cmd, arg);
#endif

	return 0;
}

static int disp_open(struct inode *inode, struct file *file)
{
	struct disp_node_struct *pNode = NULL;

	DDPDBG("enter disp_open() process:%s\n", current->comm);

	/* Allocate and initialize private data */
	file->private_data = kmalloc(sizeof(struct disp_node_struct), GFP_ATOMIC);
	if (file->private_data == NULL) {
		DDPMSG("Not enough entry for DDP open operation\n");
		return -ENOMEM;
	}

	pNode = (struct disp_node_struct *) file->private_data;
	pNode->open_pid = current->pid;
	pNode->open_tgid = current->tgid;
	INIT_LIST_HEAD(&(pNode->testList));
	spin_lock_init(&pNode->node_lock);

	return 0;

}

static ssize_t disp_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static int disp_release(struct inode *inode, struct file *file)
{
	struct disp_node_struct *pNode = NULL;

	DDPDBG("enter disp_release() process:%s\n", current->comm);

	pNode = (struct disp_node_struct *) file->private_data;

	spin_lock(&pNode->node_lock);

	spin_unlock(&pNode->node_lock);

	if (file->private_data != NULL) {
		kfree(file->private_data);
		file->private_data = NULL;
	}

	return 0;
}

static int disp_flush(struct file *file, fl_owner_t a_id)
{
	return 0;
}

static int disp_mmap(struct file *file, struct vm_area_struct *a_pstVMArea)
{
	return 0;
}

static int nr_dispsys_dev;
static struct dispsys_device *dispsys_dev;
unsigned int dispsys_irq[DISP_REG_NUM] = { 0 };
unsigned long dispsys_reg[DISP_REG_NUM] = { 0 };

unsigned long mipi_tx_reg;
unsigned long dsi_reg_va;

#ifdef CONFIG_MTK_M4U
m4u_callback_ret_t disp_m4u_callback(int port, unsigned int mva, void *data)
{
	enum DISP_MODULE_ENUM module = DISP_MODULE_OVL0;

	DDPERR("fault call port=%d, mva=0x%x, data=0x%p\n", port, mva, data);
	switch (port) {
	case M4U_PORT_DISP_OVL0:
		module = DISP_MODULE_OVL0;
		break;
	case M4U_PORT_DISP_RDMA0:
		module = DISP_MODULE_RDMA0;
		break;
	case M4U_PORT_DISP_WDMA0:
		module = DISP_MODULE_WDMA0;
		break;
#if defined(MTK_FB_OVL1_SUPPORT)
	case M4U_PORT_DISP_OVL1:
		module = DISP_MODULE_OVL1;
		break;
#endif
#if defined(MTK_FB_RDMA1_SUPPORT)
	case M4U_PORT_DISP_RDMA1:
		module = DISP_MODULE_RDMA1;
		break;
#endif
	default:
		DDPERR("unknown port=%d\n", port);
	}
	ddp_dump_analysis(module);
	ddp_dump_reg(module);

#if defined(OVL_CASCADE_SUPPORT)
	/* always dump 2 OVL */
	if (module == DISP_MODULE_OVL0)
		ddp_dump_analysis(DISP_MODULE_OVL1);
	else if (module == DISP_MODULE_OVL1)
		ddp_dump_analysis(DISP_MODULE_OVL0);
#endif
	/* disable translation fault after it happens to avoid prinkt too much issues(log is override) */
	m4u_enable_tf(port, 0);

	return 0;
}
#endif

#ifdef CONFIG_MTK_M4U
void disp_m4u_tf_disable(void)
{
	/* m4u_enable_tf(M4U_PORT_DISP_OVL0, 0); */
#if defined(OVL_CASCADE_SUPPORT)
	m4u_enable_tf(M4U_PORT_DISP_OVL1, 0);
#endif
}
#endif


struct device *disp_get_device(void)
{
	return dispsys_dev->dev;
}

struct device *disp_get_iommu_device(void)
{
	if (!dispsys_dev->iommu_pdev)
		return NULL;

	return &(dispsys_dev->iommu_pdev->dev);
}


#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
static struct miscdevice disp_misc_dev;
#endif
/* Kernel interface */
static const struct file_operations disp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = disp_unlocked_ioctl,
	.compat_ioctl = disp_compat_ioctl,
	.open = disp_open,
	.release = disp_release,
	.flush = disp_flush,
	.read = disp_read,
#if 1
	.mmap = disp_mmap
#endif
};

#ifndef CONFIG_MTK_CLKMGR
/*
 * Note: The name order of the disp_clk_name[] must be synced with
 * the enum disp_clk_id in case get the wrong clock.
 */
const char *disp_clk_name[MAX_DISP_CLK_CNT] = {
	"MMSYS_CLK_FAKE_ENG",
	"MMSYS_CLK_DISP_OVL0",
	"MMSYS_CLK_DISP_RDMA0",
	"MMSYS_CLK_DISP_RDMA1",
	"MMSYS_CLK_DISP_WDMA0",
	"MMSYS_CLK_DISP_COLOR",
	"MMSYS_CLK_DISP_CCORR",
	"MMSYS_CLK_DISP_AAL",
	"MMSYS_CLK_DISP_GAMMA",
	"MMSYS_CLK_DISP_DITHER",
	"MMSYS_CLK_DISP_UFOE",
	"MMSYS_CLK_TOP_PWM_MM",
	"MMSYS_CLK_DISP_PWM",
	"MMSYS_CLK_DISP_PWM26M",
	"MMSYS_CLK_DSI_ENGINE",
	"MMSYS_CLK_DSI_DIGITAL",
	"MMSYS_CLK_DPI0_PIXEL",
	"MMSYS_CLK_DPI0_ENGINE",
	"MMSYS_CLK_LVDS_PIXEL",
	"MMSYS_CLK_LVDS_CTS",
	"MMSYS_CLK_DPI1_PIXEL",
	"MMSYS_CLK_DPI1_ENGINE",
	"MMSYS_CLK_TOP_MIPI_26M",
	"MMSYS_CLK_TOP_MIPI_26M_DBG",
	"MMSYS_CLK_TOP_RG_FDPI0",
	"MMSYS_CLK_MUX_DPI0_SEL",
	"MMSYS_APMIXED_LVDSPLL",
	"MMSYS_CLK_MUX_LVDSPLL_D2",
	"MMSYS_CLK_MUX_LVDSPLL_D4",
	"MMSYS_CLK_MUX_LVDSPLL_D8",
	"MMSYS_CLK_TOP_RG_FDPI1",
	"MMSYS_CLK_MUX_DPI1_SEL",
	"MMSYS_APMIXED_TVDPLL",
	"MMSYS_CLK_MUX_TVDPLL_D2",
	"MMSYS_CLK_MUX_TVDPLL_D4",
	"MMSYS_CLK_MUX_TVDPLL_D8",
	"MMSYS_CLK_MUX_TVDPLL_D16",
	"MMSYS_CLK_PWM_SEL",
	"TOP_UNIVPLL_D12",
};

int ddp_clk_prepare(enum eDDP_CLK_ID id)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	int ret = 0;

	ret = clk_prepare(dispsys_dev->disp_clk[id]);
	if (ret)
		pr_err("DISPSYS CLK prepare failed: errno %d\n", ret);

	return ret;
#endif
#endif
	return 0;
}

int ddp_clk_unprepare(enum eDDP_CLK_ID id)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	int ret = 0;

	clk_unprepare(dispsys_dev->disp_clk[id]);
	return ret;
#endif
#endif
	return 0;
}

int ddp_clk_enable(enum eDDP_CLK_ID id)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	int ret = 0;

	ret = clk_prepare_enable(dispsys_dev->disp_clk[id]);
	if (ret)
		pr_err("DISPSYS CLK enable failed: errno %d\n", ret);

	return ret;
#endif
#endif
	return 0;
}

int ddp_clk_disable(enum eDDP_CLK_ID id)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	int ret = 0;

	clk_disable_unprepare(dispsys_dev->disp_clk[id]);
	return ret;
#endif
#endif
	return 0;
}

int ddp_clk_prepare_enable(enum eDDP_CLK_ID id)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	int ret = 0;

	ret = clk_prepare_enable(dispsys_dev->disp_clk[id]);
	if (ret)
		pr_err("DISPSYS CLK prepare failed: errno %d\n", ret);

	return ret;
#endif
#endif
	return 0;
}

int ddp_clk_disable_unprepare(enum eDDP_CLK_ID id)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	int ret = 0;

	clk_disable_unprepare(dispsys_dev->disp_clk[id]);
	return ret;
#endif
#endif
	return 0;
}

int ddp_clk_set_parent(enum eDDP_CLK_ID id, enum eDDP_CLK_ID parent)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	return clk_set_parent(dispsys_dev->disp_clk[id], dispsys_dev->disp_clk[parent]);
#endif
#endif
	return 0;
}

int ddp_clk_set_rate(enum eDDP_CLK_ID id, unsigned long rate)
{
#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING	/* CCF not ready */
	return clk_set_rate(dispsys_dev->disp_clk[id], rate);
#endif
#endif
	return 0;
}
#endif				/* CONFIG_MTK_CLKMGR */

const char *ddp_get_reg_module_name(enum DISP_REG_ENUM reg)
{
	switch (reg) {
	case DISP_REG_CONFIG:
		return "mediatek,mt8167-mmsys";
	case DISP_REG_AAL:
		return "mediatek,mt8167-disp_aal";
	case DISP_REG_COLOR:
		return "mediatek,mt8167-disp_color";
	case DISP_REG_RDMA0:
		return "mediatek,mt8167-disp_rdma0";
	case DISP_REG_RDMA1:
		return "mediatek,mt8167-disp_rdma1";
	case DISP_REG_WDMA0:
		return "mediatek,mt8167-disp_wdma0";
	case DISP_REG_OVL0:
		return "mediatek,mt8167-disp_ovl0";
	case DISP_REG_CCORR:
		return "mediatek,mt8167-disp_ccorr";
	case DISP_REG_GAMMA:
		return "mediatek,mt8167-disp_gamma";
	case DISP_REG_DITHER:
		return "mediatek,mt8167-disp_dither";
	case DISP_REG_PWM:
		return "mediatek,mt8167-disp_pwm";
	case DISP_REG_DSI0:
		return "mediatek,mt8167-disp_dsi0";
	case DISP_REG_DPI0:
		return "mediatek,mt8167-disp_dpi0";
	case DISP_REG_DPI1:
		return "mediatek,mt8167-disp_dpi1";
	case DISP_REG_SMI_LARB0:
		return "mediatek,mt8167-smi-larb";
	case DISP_REG_SMI_COMMON:
		return "mediatek,mt8167-smi-common";
	case DISP_REG_MIPI:
		return "mediatek,mt8167-disp_mipi_tx";
	case DISP_REG_MUTEX:
		return "mediatek,mt8167-disp_mutex";
#if 0
	case DISP_REG_CONFIG2:
		return "mediatek,mt8167-disp_config2";
	case DISP_REG_CONFIG3:
		return "mediatek,mt8167-disp_config3";
#endif
	case DISP_LVDS_ANA:
		return "mediatek,mt8167-disp_lvds_ana";
	case DISP_LVDS_TX:
		return "mediatek,mt8167-disp_lvds_tx";
	default:
		return "mediatek,DISP_UNKNOWN";
	}
}

static int disp_probe(struct platform_device *pdev)
{
	struct class_device;
	int i;
	int new_count;
	static unsigned int disp_probe_cnt;
	struct device_node *np;
	unsigned int reg_value, irq_value;
#ifdef CONFIG_MTK_IOMMU
	struct device_node *larb_node[1];
	struct platform_device *larb_pdev[1];
	struct platform_device *iommu_pdev = NULL;
#endif

	DDPMSG("real disp_probe\n");

	if (disp_probe_cnt != 0)
		return 0;

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#ifdef DISP_SVP_DEBUG
	disp_misc_dev.minor = MISC_DYNAMIC_MINOR;
	disp_misc_dev.name = "mtk_disp";
	disp_misc_dev.fops = &disp_fops;
	disp_misc_dev.parent = NULL;
	ret = misc_register(&disp_misc_dev);
	if (ret) {
		pr_err("disp: fail to create mtk_disp node\n");
		return ERR_PTR(ret);
	}
#endif
#endif

#ifdef CONFIG_MTK_IOMMU
	larb_node[0] = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!larb_node[0])
		return -EINVAL;

	larb_pdev[0] = of_find_device_by_node(larb_node[0]);
	of_node_put(larb_node[0]);
	if ((!larb_pdev[0]) || (!larb_pdev[0]->dev.driver)) {
		DDPERR("disp_probe is earlier than SMI\n");
		return -EPROBE_DEFER;
	}

	/* add for mmp dump mva->pa */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8167-pseudo-port-m4u");
	if (np == NULL) {
		DDPERR("DT mediatek,mt8167-pseudo-port-m4u is not found\n");
	} else {
		iommu_pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (!iommu_pdev)
			DDPERR("get iommu device failed\n");
	}

#endif

	new_count = nr_dispsys_dev + 1;
	/*dispsys_dev = krealloc(dispsys_dev, sizeof(struct dispsys_device) * new_count, GFP_KERNEL);*/
	dispsys_dev = kcalloc(new_count, sizeof(*dispsys_dev), GFP_KERNEL);
	if (!dispsys_dev) {
		DDPERR("Unable to allocate dispsys_dev\n");
		return -ENOMEM;
	}

	dispsys_dev = &(dispsys_dev[nr_dispsys_dev]);
	dispsys_dev->dev = &pdev->dev;
#ifdef CONFIG_MTK_IOMMU
	dispsys_dev->larb_pdev[0] = larb_pdev[0];
	dispsys_dev->iommu_pdev = iommu_pdev;
	dev_set_drvdata(&pdev->dev, dispsys_dev);
#endif

#ifndef CONFIG_MTK_CLKMGR
#ifndef CONFIG_FPGA_EARLY_PORTING
	for (i = 0; i < MAX_DISP_CLK_CNT; i++) {
		DDPMSG("DISPSYS get clock %s\n", disp_clk_name[i]);
		dispsys_dev->disp_clk[i] = devm_clk_get(&pdev->dev, disp_clk_name[i]);
		if (IS_ERR(dispsys_dev->disp_clk[i]))
			DDPERR("%s:%d, DISPSYS get %d,%s clock error!!!\n",
			       __FILE__, __LINE__, i, disp_clk_name[i]);
	}
#endif
#endif

	/* iomap registers and irq */
	for (i = 0; i < DISP_REG_NUM; i++) {

		np = of_find_compatible_node(NULL, NULL, ddp_get_reg_module_name(i));
		if (np == NULL) {
			DDPERR("DT %s is not found from DTS\n", ddp_get_reg_module_name(i));
			continue;
		}

		of_property_read_u32_index(np, "reg", 1, &reg_value);
		of_property_read_u32_index(np, "interrupts", 1, &irq_value);

		dispsys_dev->regs[i] = of_iomap(np, 0);
		if (!dispsys_dev->regs[i]) {
			DDPERR("Unable to ioremap registers, of_iomap fail, i=%d\n", i);
			return -ENOMEM;
		}
		dispsys_reg[i] = (unsigned long)dispsys_dev->regs[i];

		if (i < DISP_REG_CONFIG && (i != DISP_REG_PWM)) {
			/* get IRQ ID and request IRQ */
			dispsys_dev->irq[i] = irq_of_parse_and_map(np, 0);
			dispsys_irq[i] = dispsys_dev->irq[i];
		}
		DDPMSG("DT, i=%d, module=%s, map_addr=%p, map_irq=%d, reg_pa=0x%x, irq=%d\n",
		       i, ddp_get_reg_module_name(i), dispsys_dev->regs[i], dispsys_dev->irq[i],
		       ddp_reg_pa_base[i], ddp_irq_num[i]);
	}
	nr_dispsys_dev = new_count;
	/* mipi tx reg map here */
	dsi_reg_va = dispsys_reg[DISP_REG_DSI0];
	mipi_tx_reg = dispsys_reg[DISP_REG_MIPI];

	DPI_REG[0] = (struct DPI_REGS *)dispsys_reg[DISP_REG_DPI0];
	DPI_REG[1] = (struct DPI_REGS *) dispsys_reg[DISP_REG_DPI1];
	/*DPI_TVDPLL_CON0 = dispsys_reg[DISP_TVDPLL_CON0];*/
	/*DPI_TVDPLL_CON1 = dispsys_reg[DISP_TVDPLL_CON1];*/


	/* power on MMSYS for early porting */
#ifdef CONFIG_FPGA_EARLY_PORTING
	DDPMSG("[DISP Probe] power MMSYS:0x%x,0x%x!\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));

	DISP_REG_SET(0, DISP_REG_CONFIG_MMSYS_CG_CLR0, 0xFFFFFFFF);
	DISP_REG_SET(0, DISP_REG_CONFIG_MMSYS_CG_CLR1, 0xFFFFFFFF);
#ifdef MTKFB_NO_M4U
	DISP_REG_SET(0, DISP_REG_SMI_LARB_MMU_EN, 0x0); /* m4u disable */
#endif
	DISP_REG_SET(0, DISPSYS_CONFIG_BASE + 0xC04, 0x1C000);	/* for fpga */

	DDPMSG("[after] power MMSYS:0x%x,0x%x\n",
	DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0),
	DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));
#else
#ifdef MTKFB_NO_M4U
	DISP_REG_SET(0, DISP_REG_SMI_LARB_MMU_EN, 0x0);	/* m4u disable */
#endif
#endif
	/* init arrays */
	ddp_path_init();

	/* init M4U callback */
	/* DDPMSG("register m4u callback\n"); */
#ifdef CONFIG_MTK_M4U
	m4u_register_fault_callback(M4U_PORT_DISP_OVL0, disp_m4u_callback, 0);
	m4u_register_fault_callback(M4U_PORT_DISP_RDMA0, disp_m4u_callback, 0);
	m4u_register_fault_callback(M4U_PORT_DISP_WDMA0, disp_m4u_callback, 0);
#endif

	/* sysfs */
	DDPMSG("sysfs disp +");
	/* add kobject */
	if (kobject_init_and_add(&kdispobj, &disp_kobj_ktype, NULL, "disp") < 0) {
		DDPERR("fail to add disp\n");
		return -ENOMEM;
	}

	DDPMSG("dispsys probe done.\n");
	/* NOT_REFERENCED(class_dev); */

	/* bus hang issue error intr enable */
	/* when MMSYS clock off but GPU/MJC/PWM clock on, avoid display hang and trigger error intr */
	{
		DISP_REG_SET_FIELD(NULL, MMSYS_TO_MFG_TX_ERROR, DISP_REG_CONFIG_MMSYS_INTEN, 1);
		DISP_REG_SET_FIELD(NULL, MMSYS_TO_MJC_TX_ERROR, DISP_REG_CONFIG_MMSYS_INTEN, 1);
		DISP_REG_SET_FIELD(NULL, PWM0_APB_TX_ERROR, DISP_REG_CONFIG_MMSYS_INTEN, 1);

		/*DISP_REG_SET_FIELD(NULL, MFG_APB_TX_CON_FLD_MFG_APB_COUNTER_EN,*/
		/*		   DISP_REG_CONFIG_MFG_APB_TX_CON, 1);*/
		/*DISP_REG_SET_FIELD(NULL, MJC_APB_TX_CON_FLD_MJC_APB_COUNTER_EN,*/
		/*		   DISP_REG_CONFIG_MJC_APB_TX_CON, 1);*/
	}

	return 0;
}

static int disp_remove(struct platform_device *pdev)
{
	/* sysfs */
	kobject_put(&kdispobj);

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	misc_deregister(&disp_misc_dev);
#endif
	return 0;
}

static void disp_shutdown(struct platform_device *pdev)
{
	/* Nothing yet */
}


/* PM suspend */
static int disp_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	pr_debug("\n\n==== DISP suspend is called ====\n");


	return 0;
}

/* PM resume */
static int disp_resume(struct platform_device *pdev)
{
	pr_debug("\n\n==== DISP resume is called ====\n");


	return 0;
}


static const struct of_device_id dispsys_of_ids[] = {
	{ .compatible = "mediatek,mt8167-dispsys", },
	{ .compatible = "mediatek,dispsys", },
	{}
};

static struct platform_driver dispsys_of_driver = {
	.driver = {
		   .name = DISP_DEVNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = dispsys_of_ids,
		   },
	.probe = disp_probe,
	.remove = disp_remove,
	.shutdown = disp_shutdown,
	.suspend = disp_suspend,
	.resume = disp_resume,
};

static int __init disp_init(void)
{
	int ret = 0;

	DDPPRINT("Register the disp driver\n");
	if (platform_driver_register(&dispsys_of_driver)) {
		DDPERR("failed to register disp driver\n");
		/* platform_device_unregister(&disp_device); */
		ret = -ENODEV;
		return ret;
	}

	return 0;
}

static void __exit disp_exit(void)
{
	platform_driver_unregister(&dispsys_of_driver);
}

/* arch_initcall(disp_init); */
/* module_init(disp_probe_1); */
module_init(disp_init);
module_exit(disp_exit);
MODULE_AUTHOR("Tzu-Meng, Chung <Tzu-Meng.Chung@mediatek.com>");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
