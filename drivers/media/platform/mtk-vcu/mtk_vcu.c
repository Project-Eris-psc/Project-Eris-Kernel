/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Andrew-CT Chen <andrew-ct.chen@mediatek.com>
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

#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#ifdef CONFIG_MTK_IOMMU
#include <linux/iommu.h>
#endif
#include "mtk_vcodec_mem.h"
#include <uapi/linux/mtk_vcu_controls.h>
#include "mtk_vcu.h"

/**
 * VCU (Video Communication/Controller Unit) is a tiny processor
 * controlling video hardware related to video codec, scaling and color
 * format converting.
 * VCU interfaces with other blocks by share memory and interrupt.
 **/
#define VCU_PATH		"/dev/vpud"
#define MDP_PATH		"/dev/mdpd"
#define CAM_PATH		"/dev/camd"
#define VCU_DEVNAME		"vpu"

#define IPI_TIMEOUT_MS		4000U
#define VCU_FW_VER_LEN		16
/*mtk vcu support mpd max value*/
#define MTK_VCU_NR_MAX       3

/* vcu extended mapping length */
#define VCU_PMEM0_LEN(vcu_data)	(vcu_data->extmem.p_len)
#define VCU_DMEM0_LEN(vcu_data)	(vcu_data->extmem.d_len)
/* vcu extended user virtural address */
#define VCU_PMEM0_VMA(vcu_data)	(vcu_data->extmem.p_vma)
#define VCU_DMEM0_VMA(vcu_data)	(vcu_data->extmem.d_vma)
/* vcu extended kernel virtural address */
#define VCU_PMEM0_VIRT(vcu_data)	(vcu_data->extmem.p_va)
#define VCU_DMEM0_VIRT(vcu_data)	(vcu_data->extmem.d_va)
/* vcu extended phsyial address */
#define VCU_PMEM0_PHY(vcu_data)	(vcu_data->extmem.p_pa)
#define VCU_DMEM0_PHY(vcu_data)	(vcu_data->extmem.d_pa)
/* vcu extended iova address*/
#define VCU_PMEM0_IOVA(vcu_data)	(vcu_data->extmem.p_iova)
#define VCU_DMEM0_IOVA(vcu_data)	(vcu_data->extmem.d_iova)

#define MAP_SHMEM_ALLOC_BASE	0x80000000UL
#define MAP_SHMEM_ALLOC_RANGE	0x08000000UL
#define MAP_SHMEM_ALLOC_END	(MAP_SHMEM_ALLOC_BASE + MAP_SHMEM_ALLOC_RANGE)
#define MAP_SHMEM_COMMIT_BASE	0x88000000UL
#define MAP_SHMEM_COMMIT_RANGE	0x08000000UL
#define MAP_SHMEM_COMMIT_END	(MAP_SHMEM_COMMIT_BASE + MAP_SHMEM_COMMIT_RANGE)

#define MAP_SHMEM_MM_BASE	0x90000000UL
#define MAP_SHMEM_MM_CACHEABLE_BASE	0x190000000UL
#define MAP_SHMEM_MM_RANGE	0xFFFFFFFFUL
#define MAP_SHMEM_MM_END	(MAP_SHMEM_MM_BASE + MAP_SHMEM_MM_RANGE)
#define MAP_SHMEM_MM_CACHEABLE_END	(MAP_SHMEM_MM_CACHEABLE_BASE + MAP_SHMEM_MM_RANGE)


enum vcu_map_hw_reg_id {
	VDEC,
	VENC,
	VENC_LT,
	VCU_MAP_HW_REG_NUM
};

static const unsigned long vcu_map_hw_type[VCU_MAP_HW_REG_NUM] = {
	0x70000000,	/* VDEC */
	0x71000000,	/* VENC */
	0x72000000	/* VENC_LT */
};

/* Default vcu_mtkdev[0] handle vdec, vcu_mtkdev[1] handle mdp */
static struct mtk_vcu *vcu_mtkdev[MTK_VCU_NR_MAX];

static struct task_struct *vcud_task;
static struct files_struct *files;
extern void smp_inner_dcache_flush_all(void);

/**
 * struct vcu_mem - VCU memory information
 *
 * @p_vma:	the user virtual memory address of
 *		VCU extended program memory
 * @d_vma:	the user  virtual memory address of VCU extended data memory
 * @p_va:	the kernel virtual memory address of
 *		VCU extended program memory
 * @d_va:	the kernel virtual memory address of VCU extended data memory
 * @p_pa:	the physical memory address of VCU extended program memory
 * @d_pa:	the physical memory address of VCU extended data memory
 * @p_iova:	the iova memory address of VCU extended program memory
 * @d_iova:	the iova memory address of VCU extended data memory
 */
struct vcu_mem {
	unsigned long p_vma;
	unsigned long d_vma;
	void *p_va;
	void *d_va;
	dma_addr_t p_pa;
	dma_addr_t d_pa;
	dma_addr_t p_iova;
	dma_addr_t d_iova;
	unsigned long p_len;
	unsigned long d_len;
};

/**
 * struct vcu_run - VCU initialization status
 *
 * @signaled:		the signal of vcu initialization completed
 * @fw_ver:		VCU firmware version
 * @dec_capability:	decoder capability which is not used for now and
 *			the value is reserved for future use
 * @enc_capability:	encoder capability which is not used for now and
 *			the value is reserved for future use
 * @wq:			wait queue for VCU initialization status
 */
struct vcu_run {
	u32 signaled;
	char fw_ver[VCU_FW_VER_LEN];
	unsigned int	dec_capability;
	unsigned int	enc_capability;
	wait_queue_head_t wq;
};

/**
 * struct vcu_ipi_desc - VCU IPI descriptor
 *
 * @handler:	IPI handler
 * @name:	the name of IPI handler
 * @priv:	the private data of IPI handler
 */
struct vcu_ipi_desc {
	ipi_handler_t handler;
	const char *name;
	void *priv;
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
};

/**
 * struct mtk_vcu - vcu driver data
 * @extmem:		VCU extended memory information
 * @run:		VCU initialization status
 * @ipi_desc:		VCU IPI descriptor
 * @dev:		VCU struct device
 * @vcu_mutex:		protect mtk_vcu (except recv_buf) and ensure only
 *			one client to use VCU service at a time. For example,
 *			suppose a client is using VCU to decode VP8.
 *			If the other client wants to encode VP8,
 *			it has to wait until VP8 decode completes.
 * @file:		VCU daemon file pointer
 * @is_open:		The flag to indicate if VCUD device is open.
 * @is_alloc:		The flag to indicate if VCU extended memory is allocated.
 * @ack_wq:		The wait queue for each codec and mdp. When sleeping
 *			processes wake up, they will check the condition
 *			"ipi_id_ack" to run the corresponding action or
 *			go back to sleep.
 * @ipi_id_ack:		The ACKs for registered IPI function sending
 *			interrupt to VCU
 * @vcu_devno:		The vcu_devno for vcu init vcu character device
 * @vcu_cdev:		The point of vcu character device.
 * @vcu_class:		The class_create for create vcu device
 * @vcu_device:		VCU struct device
 * @vcuname:		VCU struct device name in dtsi
 * @path:		The path to keep mdpd path or vcud path.
 * @vpuid:		VCU device id
 *
 */
struct mtk_vcu {
	struct vcu_mem extmem;
	struct vcu_run run;
	struct vcu_ipi_desc ipi_desc[IPI_MAX];
	struct device *dev;
	struct mutex vcu_mutex; /* for protecting vcu data structure */
	struct file *file;
	struct iommu_domain *io_domain;
	struct map_hw_reg map_base[VCU_MAP_HW_REG_NUM];
	bool   is_open;
	bool   is_alloc;
	wait_queue_head_t ack_wq;
	bool ipi_id_ack[IPI_MAX];
	dev_t vcu_devno;
	struct cdev *vcu_cdev;
	struct class *vcu_class;
	struct device *vcu_device;
	const char *vcuname;
	const char *path;
	int vcuid;
};

static inline bool vcu_running(struct mtk_vcu *vcu)
{
	return (bool)vcu->run.signaled;
}

int vcu_ipi_register(struct platform_device *pdev,
		     enum ipi_id id, ipi_handler_t handler,
		     const char *name, void *priv)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	struct vcu_ipi_desc *ipi_desc;

	if (vcu == NULL) {
		dev_err(&pdev->dev, "vcu device in not ready\n");
		return -EPROBE_DEFER;
	}

	if (id >= 0 && id < IPI_MAX && handler != NULL) {
		ipi_desc = vcu->ipi_desc;
		ipi_desc[id].name = name;
		ipi_desc[id].handler = handler;
		ipi_desc[id].priv = priv;
		return 0;
	}

	dev_err(&pdev->dev, "register vcu ipi id %d with invalid arguments\n",
	       id);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(vcu_ipi_register);

int vcu_ipi_send(struct platform_device *pdev,
		 enum ipi_id id, void *buf,
		 unsigned int len)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	struct share_obj send_obj;
	mm_segment_t old_fs = get_fs();
	unsigned long timeout;
	int ret;

	if (id <= IPI_VCU_INIT || id >= IPI_MAX ||
	    len > sizeof(send_obj.share_buf) || buf == NULL) {
		dev_err(&pdev->dev, "[VCU] failed to send ipi message (Invalid arg.)\n");
		return -EINVAL;
	}

	if (vcu_running(vcu) == false) {
		dev_err(&pdev->dev, "[VCU] vcu_ipi_send: VCU is not running\n");
		return -EPERM;
	}

	memcpy((void *)send_obj.share_buf, buf, len);
	send_obj.len = len;
	send_obj.id = (int)id;

	mutex_lock(&vcu->vcu_mutex);
	if (vcu->is_open == false) {
		vcu->file = filp_open(vcu->path, O_RDONLY, 0);
		if (IS_ERR(vcu->file) == true) {
			dev_err(&pdev->dev, "[VCU] Open vcud fail (ret=%ld)\n", PTR_ERR(vcu->file));
			mutex_unlock(&vcu->vcu_mutex);
			return -EINVAL;
		}
		vcu->is_open = true;
	}

	vcu->ipi_id_ack[id] = false;
	/* send the command to VCU */
	set_fs(KERNEL_DS);
#if IS_ENABLED(CONFIG_COMPAT)
	ret = vcu->file->f_op->compat_ioctl(vcu->file,
		(unsigned int)VCUD_SET_OBJECT, (unsigned long)&send_obj);
#else
	ret = vcu->file->f_op->unlocked_ioctl(vcu->file,
		(unsigned int)VCUD_SET_OBJECT, (unsigned long)&send_obj);
#endif
	set_fs(old_fs);
	mutex_unlock(&vcu->vcu_mutex);

	if (ret != 0) {
		dev_err(&pdev->dev, "[VCU] failed to send ipi message (ret=%d)\n", ret);
		goto end;
	}

	/* wait for VCU's ACK */
	timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
	ret = wait_event_timeout(vcu->ack_wq, vcu->ipi_id_ack[id], timeout);
	vcu->ipi_id_ack[id] = false;
	if (ret == 0) {
		dev_err(&pdev->dev, "vcu ipi %d ack time out !", id);
		ret = -EIO;
		goto end;
	} else if (-ERESTARTSYS == ret) {
		dev_err(&pdev->dev, "vcu ipi %d ack wait interrupted by a signal",
		       id);
		ret = -ERESTARTSYS;
		goto end;
	} else
		ret = 0;

end:
	return ret;
}
EXPORT_SYMBOL_GPL(vcu_ipi_send);

unsigned int vcu_get_vdec_hw_capa(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	return vcu->run.dec_capability;
}
EXPORT_SYMBOL_GPL(vcu_get_vdec_hw_capa);

unsigned int vcu_get_venc_hw_capa(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	return vcu->run.enc_capability;
}
EXPORT_SYMBOL_GPL(vcu_get_venc_hw_capa);

void *vcu_mapping_dm_addr(struct platform_device *pdev,
			  uintptr_t dtcm_dmem_addr)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	uintptr_t d_vma = (uintptr_t)(dtcm_dmem_addr);
	uintptr_t d_va_start = (uintptr_t)VCU_DMEM0_VIRT(vcu);
	uintptr_t d_off = d_vma - VCU_DMEM0_VMA(vcu);
	uintptr_t d_va;

	if (dtcm_dmem_addr == 0UL || d_off > VCU_DMEM0_LEN(vcu)) {
		dev_err(&pdev->dev, "[VCU] %s: Invalid vma 0x%lx len %lx\n",
			__func__, dtcm_dmem_addr, VCU_DMEM0_LEN(vcu));
		return NULL;
	}

	d_va = d_va_start + d_off;
	dev_dbg(&pdev->dev, "[VCU] %s: 0x%lx -> 0x%lx\n", __func__, d_vma, d_va);

	return (void *)d_va;
}
EXPORT_SYMBOL_GPL(vcu_mapping_dm_addr);

struct platform_device *vcu_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *vcu_node;
	struct platform_device *vcu_pdev;

	dev_dbg(&pdev->dev, "[VCU] %s\n", __func__);

	vcu_node = of_parse_phandle(dev->of_node, "mediatek,vcu", 0);
	if (vcu_node == NULL) {
		dev_err(dev, "[VCU] can't get vcu node\n");
		return NULL;
	}

	vcu_pdev = of_find_device_by_node(vcu_node);
	if (WARN_ON(vcu_pdev == NULL) == true) {
		dev_err(dev, "[VCU] vcu pdev failed\n");
		of_node_put(vcu_node);
		return NULL;
	}

	return vcu_pdev;
}
EXPORT_SYMBOL_GPL(vcu_get_plat_device);

int vcu_load_firmware(struct platform_device *pdev)
{
	if (pdev == NULL) {
		dev_err(&pdev->dev, "[VCU] VCU platform device is invalid\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(vcu_load_firmware);

int vcu_compare_version(struct platform_device *pdev,
			const char *expected_version)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	int cur_major, cur_minor, cur_build, cur_rel, cur_ver_num;
	int major, minor, build, rel, ver_num;
	char *cur_version = vcu->run.fw_ver;

	cur_ver_num = sscanf(cur_version, "%d.%d.%d-rc%d",
			     &cur_major, &cur_minor, &cur_build, &cur_rel);
	if (cur_ver_num < 3)
		return -1;
	ver_num = sscanf(expected_version, "%d.%d.%d-rc%d",
			 &major, &minor, &build, &rel);
	if (ver_num < 3)
		return -1;

	if (cur_major < major)
		return -1;
	if (cur_major > major)
		return 1;

	if (cur_minor < minor)
		return -1;
	if (cur_minor > minor)
		return 1;

	if (cur_build < build)
		return -1;
	if (cur_build > build)
		return 1;

	if (cur_ver_num < ver_num)
		return -1;
	if (cur_ver_num > ver_num)
		return 1;

	if (ver_num > 3) {
		if (cur_rel < rel)
			return -1;
		if (cur_rel > rel)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vcu_compare_version);

void vcu_get_task(struct task_struct **task, struct files_struct **f)
{
	pr_debug("mtk_vcu_get_task %p\n", vcud_task);
	*task = vcud_task;
	*f = files;
}
EXPORT_SYMBOL_GPL(vcu_get_task);

static int vcu_ipi_handler(struct mtk_vcu *vcu, struct share_obj *rcv_obj)
{
	struct vcu_ipi_desc *ipi_desc = vcu->ipi_desc;
	int non_ack = 0;
	int ret = -1;

	if (rcv_obj->id < (int)IPI_MAX &&
		ipi_desc[rcv_obj->id].handler != NULL) {
		non_ack = ipi_desc[rcv_obj->id].handler(rcv_obj->share_buf,
							rcv_obj->len,
							ipi_desc[rcv_obj->id].priv);
		if (rcv_obj->id > (int)IPI_VCU_INIT && non_ack == 0) {
			vcu->ipi_id_ack[rcv_obj->id] = true;
			wake_up(&vcu->ack_wq);
		}
		ret = 0;
	} else {
		dev_err(vcu->dev, "[VCU] No such ipi id = %d\n", rcv_obj->id);
	}

	return ret;
}

static int vcu_ipi_init(struct mtk_vcu *vcu)
{
	vcu->is_open = false;
	vcu->is_alloc = false;
	mutex_init(&vcu->vcu_mutex);

	return 0;
}

static int vcu_init_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_vcu *vcu = (struct mtk_vcu *)priv;
	struct vcu_run *run = (struct vcu_run *)data;

	vcu->run.signaled = run->signaled;
	strncpy(vcu->run.fw_ver, run->fw_ver, VCU_FW_VER_LEN);
	vcu->run.dec_capability = run->dec_capability;
	vcu->run.enc_capability = run->enc_capability;

	dev_dbg(vcu->dev, "[VCU] fw ver: %s\n", vcu->run.fw_ver);
	dev_dbg(vcu->dev, "[VCU] dec cap: %x\n", vcu->run.dec_capability);
	dev_dbg(vcu->dev, "[VCU] enc cap: %x\n", vcu->run.enc_capability);
	return 0;
}

static int mtk_vcu_open(struct inode *inode, struct file *file)
{
	int vcuid;
	struct mtk_vcu_queue *vcu_queue;

	if (strcmp(current->comm, "camd") == 0)
		vcuid = 2;
	else if (strcmp(current->comm, "mdpd") == 0)
		vcuid = 1;
	else if (strcmp(current->comm, "vpud") == 0) {
		vcud_task = current;
		files = vcud_task->files;
		vcuid = 0;

	} else {
		pr_err("[VCU] thread name: %s\n", current->comm);
		return -ENODEV;
	}

	vcu_mtkdev[vcuid]->vcuid = vcuid;

	vcu_queue = mtk_vcu_dec_init(vcu_mtkdev[vcuid]->dev);
	vcu_queue->vcu = vcu_mtkdev[vcuid];
	file->private_data = vcu_queue;

	return 0;
}

static int mtk_vcu_release(struct inode *inode, struct file *file)
{
	mtk_vcu_dec_release((struct mtk_vcu_queue *)file->private_data);

	return 0;
}

static void vcu_free_d_ext_mem(struct mtk_vcu *vcu)
{
	mutex_lock(&vcu->vcu_mutex);
	if (vcu->is_open == true) {
		filp_close(vcu->file, NULL);
		vcu->is_open = false;
	}
	if (vcu->is_alloc == true) {
		kfree(VCU_DMEM0_VIRT(vcu));
		VCU_DMEM0_VIRT(vcu) = NULL;
		vcu->is_alloc = false;
	}
	mutex_unlock(&vcu->vcu_mutex);
}

static int vcu_alloc_d_ext_mem(struct mtk_vcu *vcu, unsigned long len)
{
	mutex_lock(&vcu->vcu_mutex);
	if (vcu->is_alloc == false) {
		VCU_DMEM0_VIRT(vcu) = kmalloc(len, GFP_KERNEL);
		VCU_DMEM0_PHY(vcu) = virt_to_phys(VCU_DMEM0_VIRT(vcu));
		VCU_DMEM0_LEN(vcu) = len;
		vcu->is_alloc = true;
	}
	mutex_unlock(&vcu->vcu_mutex);

	dev_dbg(vcu->dev, "[VCU] Data extend memory (len:%lu) phy=0x%llx virt=0x%p iova=0x%llx\n",
		VCU_DMEM0_LEN(vcu),
		(unsigned long long)VCU_DMEM0_PHY(vcu),
		VCU_DMEM0_VIRT(vcu),
		(unsigned long long)VCU_DMEM0_IOVA(vcu));
	return 0;
}

static int mtk_vcu_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long pa_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa_start_base = pa_start;
	unsigned long pa_end = pa_start + length;
	unsigned long start = vma->vm_start;
	unsigned long pos = 0;
	int i;
	struct mtk_vcu *vcu_dev;
	struct mtk_vcu_queue *vcu_queue = (struct mtk_vcu_queue *)file->private_data;

	vcu_dev = (struct mtk_vcu *)vcu_queue->vcu;
	pr_debug("[VCU] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);

	/*only vcud need this case*/
	if (vcu_dev->vcuid == 0) {
		for (i = 0; i < (int)VCU_MAP_HW_REG_NUM; i++) {
			if (pa_start == vcu_map_hw_type[i] &&
			    length <= vcu_dev->map_base[i].len) {
				vma->vm_pgoff =
					vcu_dev->map_base[i].base >> PAGE_SHIFT;
				goto reg_valid_map;
			}
		}
	}

	if (pa_start >= MAP_SHMEM_ALLOC_BASE && pa_end <= MAP_SHMEM_ALLOC_END) {
		vcu_free_d_ext_mem(vcu_dev);
		if (vcu_alloc_d_ext_mem(vcu_dev, length) != 0) {
			dev_err(vcu_dev->dev, "[VCU] allocate DM failed\n");
			return -ENOMEM;
		}
		vma->vm_pgoff =
			(unsigned long)(VCU_DMEM0_PHY(vcu_dev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start >= MAP_SHMEM_COMMIT_BASE && pa_end <= MAP_SHMEM_COMMIT_END) {
		VCU_DMEM0_VMA(vcu_dev) = vma->vm_start;
		vma->vm_pgoff =
			(unsigned long)(VCU_DMEM0_PHY(vcu_dev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start_base >= MAP_SHMEM_MM_BASE) {
		if (pa_start_base >= MAP_SHMEM_MM_CACHEABLE_BASE)
			pa_start -= MAP_SHMEM_MM_CACHEABLE_BASE;
		else
			pa_start -= MAP_SHMEM_MM_BASE;
#ifdef CONFIG_MTK_IOMMU
		while (length > 0) {
			vma->vm_pgoff = iommu_iova_to_phys(vcu_dev->io_domain,
						   pa_start + pos);
			vma->vm_pgoff >>= PAGE_SHIFT;
			if (pa_start_base < MAP_SHMEM_MM_CACHEABLE_BASE)
				vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
			if (remap_pfn_range(vma, start, vma->vm_pgoff,
			    PAGE_SIZE, vma->vm_page_prot) == true)
				return -EAGAIN;

			start += PAGE_SIZE;
			pos += PAGE_SIZE;
			if (length > PAGE_SIZE)
				length -= PAGE_SIZE;
			else
				length = 0;
		}
		return 0;
#endif
	}

	dev_err(vcu_dev->dev, "[VCU] Invalid argument\n");
	return -EINVAL;

reg_valid_map:
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

valid_map:
	dev_dbg(vcu_dev->dev, "[VCU] Mapping pgoff 0x%lx\n", vma->vm_pgoff);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start,
			    vma->vm_page_prot) != 0) {
		return -EAGAIN;
	}

	return 0;
}

static long mtk_vcu_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -1;
	void *mem_priv;
	unsigned char *user_data_addr = NULL;
	struct mtk_vcu *vcu_dev;
	struct device *dev;
	struct share_obj share_buff_data;
	struct mem_obj mem_buff_data;
	struct mtk_vcu_queue *vcu_queue = (struct mtk_vcu_queue *)file->private_data;

	vcu_dev = (struct mtk_vcu *)vcu_queue->vcu;
	dev = vcu_dev->dev;
	switch (cmd) {
	case VCUD_SET_OBJECT:
		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&share_buff_data, user_data_addr,
			(unsigned long)sizeof(struct share_obj));
		if (ret != 0L || share_buff_data.id > (int)IPI_MAX ||
		    share_buff_data.id < (int)IPI_VCU_INIT) {
			pr_err("[VCU] %s(%d) Copy data from user failed!\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		ret = vcu_ipi_handler(vcu_dev, &share_buff_data);
		ret = (long)copy_to_user(user_data_addr, &share_buff_data,
			(unsigned long)sizeof(struct share_obj));
		if (ret != 0L) {
			pr_err("[VCU] %s(%d) Copy data to user failed!\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		break;
	case VCUD_MVA_ALLOCATION:
		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&mem_buff_data, user_data_addr,
			(unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_err("[VCU] %s(%d) Copy data from user failed!\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		mem_priv = mtk_vcu_get_buffer(vcu_queue, &mem_buff_data);
		if (IS_ERR(mem_priv) == true) {
			pr_err("[VCU] Dma alloc buf failed!\n");
			return PTR_ERR(mem_priv);
		}

		ret = (long)copy_to_user(user_data_addr, &mem_buff_data,
			(unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_err("[VCU] %s(%d) Copy data to user failed!\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		ret = 0;
		break;
	case VCUD_MVA_FREE:
		user_data_addr = (unsigned char *)arg;
		ret = (long)copy_from_user(&mem_buff_data, user_data_addr,
			(unsigned long)sizeof(struct mem_obj));
		if ((ret != 0L) || (mem_buff_data.iova == 0UL)) {
			pr_err("[VCU] %s(%d) Free buf failed!\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		ret = mtk_vcu_free_buffer(vcu_queue, &mem_buff_data);
		if (ret != 0L) {
			pr_err("[VCU] Dma free buf failed!\n");
			return -EINVAL;
		}
		mem_buff_data.va = 0;
		mem_buff_data.iova = 0;

		ret = (long)copy_to_user(user_data_addr, &mem_buff_data,
			(unsigned long)sizeof(struct mem_obj));
		if (ret != 0L) {
			pr_err("[VCU] %s(%d) Copy data to user failed!\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		ret = 0;
		break;
	case VCUD_CACHE_FLUSH_ALL:
		dev_dbg(dev, "[VCU] Flush cache in kernel\n");
		smp_inner_dcache_flush_all();
		ret = 0;
		break;
	default:
		dev_err(dev, "[VCU] Unknown cmd\n");
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static int compat_get_vpud_allocation_data(
				struct compat_mem_obj __user *data32,
				struct mem_obj __user *data)
{
	compat_ulong_t l;
	compat_u64 u;
	unsigned int err = 0;

	err = get_user(l, &data32->iova);
	err |= put_user(l, &data->iova);
	err |= get_user(l, &data32->len);
	err |= put_user(l, &data->len);
	err |= get_user(u, &data32->va);
	err |= put_user(u, &data->va);

	return (int)err;
}

static int compat_put_vpud_allocation_data(
				struct compat_mem_obj __user *data32,
				struct mem_obj __user *data)
{
	compat_ulong_t l;
	compat_u64 u;
	unsigned int err = 0;

	err = get_user(l, &data->iova);
	err |= put_user(l, &data32->iova);
	err |= get_user(l, &data->len);
	err |= put_user(l, &data32->len);
	err |= get_user(u, &data->va);
	err |= put_user(u, &data32->va);

	return (int)err;
}

static long mtk_vcu_unlocked_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	long ret = -1;
	struct share_obj __user *share_data32;
	struct compat_mem_obj __user *data32;
	struct mem_obj __user *data;

	switch (cmd) {
	case COMPAT_VCUD_SET_OBJECT:
		share_data32 = compat_ptr((uint32_t)arg);
		ret = file->f_op->unlocked_ioctl(file,
			(uint32_t)VCUD_SET_OBJECT, (unsigned long)share_data32);
		break;
	case COMPAT_VCUD_MVA_ALLOCATION:
		data32 = compat_ptr((uint32_t)arg);
		data = compat_alloc_user_space(sizeof(struct mem_obj));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		ret = file->f_op->unlocked_ioctl(file,
			(uint32_t)VCUD_MVA_ALLOCATION, (unsigned long)data);

		err = compat_put_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		break;
	case COMPAT_VCUD_MVA_FREE:
		data32 = compat_ptr((uint32_t)arg);
		data = compat_alloc_user_space(sizeof(struct mem_obj));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		ret = file->f_op->unlocked_ioctl(file,
			(uint32_t)VCUD_MVA_FREE, (unsigned long)data);

		err = compat_put_vpud_allocation_data(data32, data);
		if (err != 0)
			return err;
		break;
	case COMPAT_VCUD_CACHE_FLUSH_ALL:
		ret = file->f_op->unlocked_ioctl(file,
			(uint32_t)VCUD_CACHE_FLUSH_ALL, 0);
		break;
	default:
		pr_err("[VCU] Invalid cmd_number 0x%x.\n", cmd);
		break;
	}
	return ret;
}
#endif

static const struct file_operations vcu_fops = {
	.owner      = THIS_MODULE,
	.unlocked_ioctl = mtk_vcu_unlocked_ioctl,
	.open       = mtk_vcu_open,
	.release    = mtk_vcu_release,
	.mmap       = mtk_vcu_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = mtk_vcu_unlocked_compat_ioctl,
#endif
};

static int mtk_vcu_probe(struct platform_device *pdev)
{
	struct mtk_vcu *vcu;
	struct device *dev;
	struct resource *res;
	int i, vcuid, ret = 0;

	dev_dbg(&pdev->dev, "[VCU] initialization\n");

	dev = &pdev->dev;
	vcu = devm_kzalloc(dev, sizeof(*vcu), GFP_KERNEL);
	if (vcu == NULL)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "mediatek,vcuid", &vcuid);
	if (ret != 0) {
		dev_err(dev, "[VCU] failed to find mediatek,vcuid\n");
		return ret;
	}
	vcu_mtkdev[vcuid] = vcu;

#ifdef CONFIG_MTK_IOMMU
	vcu_mtkdev[vcuid]->io_domain = iommu_get_domain_for_dev(dev);
	if (vcu_mtkdev[vcuid]->io_domain == NULL) {
		dev_err(dev, "[VCU] vcuid: %d get iommu domain fail !!\n", vcuid);
		return -EPROBE_DEFER;
	}
	dev_err(dev, "vcu iommudom: %p,vcuid:%d\n", vcu_mtkdev[vcuid]->io_domain, vcuid);
#endif

	if (vcuid == 2)
		vcu_mtkdev[vcuid]->path = CAM_PATH;
	else if (vcuid == 1)
		vcu_mtkdev[vcuid]->path = MDP_PATH;
	else if (vcuid == 0)
		vcu_mtkdev[vcuid]->path = VCU_PATH;
	else
		return -ENXIO;

	ret = of_property_read_string(dev->of_node, "mediatek,vcuname", &vcu_mtkdev[vcuid]->vcuname);
	if (ret != 0) {
		dev_err(dev, "[VCU] failed to find mediatek,vcuname\n");
		return ret;
	}

	vcu->dev = &pdev->dev;
	platform_set_drvdata(pdev, vcu_mtkdev[vcuid]);

	if (vcuid == 0) {
		for (i = 0; i < (int)VCU_MAP_HW_REG_NUM; i++) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, i);
			if (res == NULL) {
				dev_err(dev, "Get memory resource failed.\n");
				ret = -ENXIO;
				goto err_ipi_init;
			}
			vcu->map_base[i].base = res->start;
			vcu->map_base[i].len = resource_size(res);
			dev_dbg(dev, "[VCU] base[%d]: 0x%lx 0x%lx", i, vcu->map_base[i].base,
				vcu->map_base[i].len);
		}
	}
	dev_dbg(dev, "[VCU] vcu ipi init\n");
	ret = vcu_ipi_init(vcu);
	if (ret != 0) {
		dev_err(dev, "[VCU] Failed to init ipi\n");
		goto err_ipi_init;
	}

	/* register vcu initialization IPI */
	ret = vcu_ipi_register(pdev, IPI_VCU_INIT, vcu_init_ipi_handler,
			       "vcu_init", vcu);
	if (ret != 0) {
		dev_err(dev, "Failed to register IPI_VCU_INIT\n");
		goto vcu_mutex_destroy;
	}

	init_waitqueue_head(&vcu->ack_wq);
	/* init character device */

	ret = alloc_chrdev_region(&vcu_mtkdev[vcuid]->vcu_devno, 0, 1, vcu_mtkdev[vcuid]->vcuname);
	if (ret < 0) {
		dev_err(dev, "[VCU]  alloc_chrdev_region failed (ret=%d)\n", ret);
		goto err_alloc;
	}

	vcu_mtkdev[vcuid]->vcu_cdev = cdev_alloc();
	vcu_mtkdev[vcuid]->vcu_cdev->owner = THIS_MODULE;
	vcu_mtkdev[vcuid]->vcu_cdev->ops = &vcu_fops;

	ret = cdev_add(vcu_mtkdev[vcuid]->vcu_cdev, vcu_mtkdev[vcuid]->vcu_devno, 1);
	if (ret < 0) {
		dev_err(dev, "[VCU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vcu_mtkdev[vcuid]->vcu_class = class_create(THIS_MODULE, vcu_mtkdev[vcuid]->vcuname);
	if (IS_ERR(vcu_mtkdev[vcuid]->vcu_class) == true) {
		ret = (int)PTR_ERR(vcu_mtkdev[vcuid]->vcu_class);
		dev_err(dev, "[VCU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vcu_mtkdev[vcuid]->vcu_device = device_create(vcu_mtkdev[vcuid]->vcu_class, NULL,
				vcu_mtkdev[vcuid]->vcu_devno, NULL, vcu_mtkdev[vcuid]->vcuname);
	if (IS_ERR(vcu_mtkdev[vcuid]->vcu_device) == true) {
		ret = (int)PTR_ERR(vcu_mtkdev[vcuid]->vcu_device);
		dev_err(dev, "[VCU] device_create fail (ret=%d)", ret);
		goto err_device;
	}

	dev_dbg(dev, "[VCU] initialization completed\n");
	return 0;

err_device:
	class_destroy(vcu_mtkdev[vcuid]->vcu_class);
err_add:
	cdev_del(vcu_mtkdev[vcuid]->vcu_cdev);
err_alloc:
	unregister_chrdev_region(vcu_mtkdev[vcuid]->vcu_devno, 1);
vcu_mutex_destroy:
	mutex_destroy(&vcu->vcu_mutex);
err_ipi_init:
	devm_kfree(dev, vcu);

	return ret;
}

static const struct of_device_id mtk_vcu_match[] = {
	{.compatible = "mediatek,mt8173-vcu",},
	{.compatible = "mediatek,mt2701-vpu",},
	{.compatible = "mediatek,mt2712-vcu",},
	{.compatible = "mediatek,mt8167-vcu",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vcu_match);

static int mtk_vcu_remove(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	if (vcu->is_open == true) {
		filp_close(vcu->file, NULL);
		vcu->is_open = false;
	}
	devm_kfree(&pdev->dev, vcu);

	device_destroy(vcu->vcu_class, vcu->vcu_devno);
	class_destroy(vcu->vcu_class);
	cdev_del(vcu->vcu_cdev);
	unregister_chrdev_region(vcu->vcu_devno, 1);

	return 0;
}

static struct platform_driver mtk_vcu_driver = {
	.probe	= mtk_vcu_probe,
	.remove	= mtk_vcu_remove,
	.driver	= {
		.name	= "mtk_vcu",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_vcu_match,
	},
};

module_platform_driver(mtk_vcu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek Video Communication And Controller Unit driver");
