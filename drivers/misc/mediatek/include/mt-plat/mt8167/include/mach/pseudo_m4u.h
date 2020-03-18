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

#ifndef __PSEUDO_M4U_PORT_H__
#define __PSEUDO_M4U_PORT_H__

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <dt-bindings/memory/mt8167-larb-port.h>
#include <linux/list.h>
#include <linux/iova.h>
#include <linux/iommu.h>

#define M4U_PAGE_SIZE	0x1000

#define M4U_DEVNAME "m4u"
#define M4U_GET_PAGE_NUM(va, size) (((((unsigned long)va)&(M4U_PAGE_SIZE-1))+(size)+(M4U_PAGE_SIZE-1))>>12)

#define NORMAL_M4U_NUMBER	1
#define TOTAL_M4U_NUM	2
#define M4U_REG_SIZE	0x5e0
#define M4U_PORT_NR	28
#define SMI_LARB_NR	3

#define M4U_PAGE_MASK	0xfff

typedef int M4U_MODULE_ID_ENUM;
typedef int M4U_MODULE_ID_ENUM;
typedef int M4U_PORT_ID_ENUM;
typedef int M4U_RANGE_PRIORITY_ENUM;
typedef int M4U_PORT_ID;

/* public flags */
#define M4U_FLAGS_SEQ_ACCESS (1<<0) /* engine access this buffer in sequncial way. */
#define M4U_FLAGS_FIX_MVA   (1<<1)  /* fix allocation, we will use mva user specified. */
#define M4U_FLAGS_SEC_SHAREABLE   (1<<2)  /* the mva will share in SWd */

typedef enum {
	/* optimized, recommand to use */
	M4U_CACHE_FLUSH_BEFORE_HW_READ_MEM = 0,
	M4U_CACHE_FLUSH_BEFORE_HW_WRITE_MEM = 1,
	M4U_CACHE_CLEAN_BEFORE_HW_READ_MEM = 2,
	M4U_CACHE_CLEAN_ALL,
	M4U_CACHE_INVALID_ALL,
	M4U_CACHE_FLUSH_ALL,
	M4U_NONE_OP,
} M4U_CACHE_SYNC_ENUM;

/* port related: virtuality, security, distance */
typedef struct _M4U_PORT {
	/*hardware port ID, defined in M4U_PORT_ID_ENUM*/
	M4U_PORT_ID ePortID;
	unsigned int Virtuality;
	unsigned int Security;
	unsigned int domain;            /*domain : 0 1 2 3*/
	unsigned int Distance;
	unsigned int Direction;         /* 0:- 1:+*/
} M4U_PORT_STRUCT;

typedef enum {
	ROTATE_0 = 0,
	ROTATE_90,
	ROTATE_180,
	ROTATE_270,
	ROTATE_HFLIP_0,
	ROTATE_HFLIP_90,
	ROTATE_HFLIP_180,
	ROTATE_HFLIP_270
} M4U_ROTATOR_ENUM;


typedef enum {
	M4U_ID_0 = 0,
	M4U_ID_1,
	M4U_ID_ALL
} M4U_ID_ENUM;

typedef struct _M4U_RANGE_DES {	/* sequential entry range */
	unsigned int Enabled;
	M4U_MODULE_ID_ENUM eModuleID;
	unsigned int MVAStart;
	unsigned int MVAEnd;
	unsigned int entryCount;
} M4U_RANGE_DES_T;

typedef struct _M4U_WRAP_DES {
	unsigned int Enabled;
	M4U_MODULE_ID_ENUM eModuleID;
	M4U_PORT_ID_ENUM ePortID;
	unsigned int MVAStart;
	unsigned int MVAEnd;
} M4U_WRAP_DES_T;

typedef enum {
	M4U_DMA_READ_WRITE = 0,
	M4U_DMA_READ = 1,
	M4U_DMA_WRITE = 2,
	M4U_DMA_NONE_OP = 3,
} M4U_DMA_DIR_ENUM;


/* module related:  alloc/dealloc MVA buffer */
typedef struct {
	M4U_PORT_ID port;
	unsigned long BufAddr;
	unsigned int BufSize;
	unsigned int prot;
	unsigned int MVAStart;
	unsigned int MVAEnd;
	unsigned int flags;

} M4U_MOUDLE_STRUCT;


typedef struct _M4U_PORT_ROTATOR {
	/* hardware port ID, defined in M4U_PORT_ID_ENUM*/
	M4U_PORT_ID_ENUM ePortID;
	unsigned int Virtuality;
	unsigned int Security;
	/* unsigned int Distance; */
	/* will be caculated actomatically inside M4U driver */
	/* unsigned int Direction; */
	unsigned int MVAStart;
	unsigned int BufAddr;
	unsigned int BufSize;
	M4U_ROTATOR_ENUM angle;
} M4U_PORT_STRUCT_ROTATOR;

typedef enum {
	M4U_DMA_MAP_AREA,
	M4U_DMA_UNMAP_AREA,
} M4U_DMA_TYPE;

typedef enum {
	M4U_DMA_FROM_DEVICE,
	M4U_DMA_TO_DEVICE,
	M4U_DMA_BIDIRECTIONAL,
} M4U_DMA_DIR;

typedef struct {
	M4U_PORT_ID port;
	M4U_CACHE_SYNC_ENUM eCacheSync;
	unsigned long va;
	unsigned int size;
	unsigned int mva;
} M4U_CACHE_STRUCT;

typedef struct _M4U_DMA {
	M4U_PORT_ID port;
	M4U_DMA_TYPE eDMAType;
	M4U_DMA_DIR eDMADir;
	unsigned long va;
	unsigned int size;
	unsigned int mva;
} M4U_DMA_STRUCT;


typedef struct _M4U_PERF_COUNT {
	unsigned int transaction_cnt;
	unsigned int main_tlb_miss_cnt;
	unsigned int pfh_tlb_miss_cnt;
	unsigned int pfh_cnt;
} M4U_PERF_COUNT;

typedef struct {
	struct list_head link;
	unsigned long bufAddr;
	unsigned int mvaStart;
	unsigned int size;
	M4U_MODULE_ID_ENUM eModuleId;
	unsigned int flags;
	int security;
	int cache_coherent;
	unsigned int mapped_kernel_va_for_debug;
} mva_info_t;

typedef struct {
	struct list_head link;
	unsigned long va;
	unsigned int mva;
	unsigned int size;
	M4U_PORT_ID port;
	unsigned int prot;
	unsigned int flags;
	struct sg_table *sg_table;
	unsigned int mva_align;
	unsigned int size_align;
	int seq_id;
	unsigned long mapped_kernel_va_for_debug;
} m4u_buf_info_t;


typedef struct {
	/* mutex to protect mvaList */
	/* should get this mutex whenever add/delete/interate mvaList */
	struct mutex dataMutex;
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head mvaList;
} m4u_client_t;

typedef struct {
	int eModuleID;
	unsigned int BufSize;
	int security;
	int cache_coherent;
	unsigned int flags;
	unsigned int iova_start;
	unsigned int iova_end;
	unsigned int *pRetMVABuf;
} port_mva_info_t;

struct m4u_device {
	/*struct miscdevice dev;*/
	struct proc_dir_entry *m4u_dev_proc_entry;
};

/* we use this for trace the mva<-->sg_table relation ship */
struct mva_sglist {
	struct list_head list;
	unsigned int mva;
	struct iova *iova;
	struct sg_table *table;
};

struct pseudo_device {
	struct device *dev;		/* record the device for config mmu use. */
	int larbid;		/* the larb id of this device*/
	bool mmuen;		/* whether this larb have been configed or not. */
};

struct m4u_port_array {
#define M4U_PORT_ATTR_EN		(1<<0)
#define M4U_PORT_ATTR_VIRTUAL	(1<<1)
#define M4U_PORT_ATTR_SEC	(1<<2)
	unsigned char ports[M4U_PORT_NR];
};


extern unsigned long max_pfn;
extern unsigned char *pMlock_cnt;
extern unsigned int mlock_cnt_size;
extern unsigned long pagetable_pa;
extern unsigned long mtk_iommu_pgt_base;

#ifndef dmac_map_area
#ifdef CONFIG_ARM64
#define dmac_map_area __dma_map_area
#else
#define dmac_map_area v7_dma_map_area
extern void dmac_map_area(const void *, size_t, int);
#endif
#endif

#ifndef dmac_unmap_area
#ifdef CONFIG_ARM64
#define dmac_unmap_area __dma_unmap_area
#else
#define dmac_unmap_area v7_dma_unmap_area
extern void dmac_unmap_area(const void *, size_t, int);
#endif
#endif

#ifndef dmac_flush_range
#define dmac_flush_range __dma_flush_range
#endif

/* function prototype */
#ifndef outer_clean_all
#define outer_clean_all(...)
#endif

#ifndef outer_flush_all
#define outer_flush_all(...)
#endif

#ifndef IOVA_PFN
#define IOVA_PFN(addr)		((addr) >> PAGE_SHIFT)
#endif

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define M4U_TEE_SERVICE_ENABLE
#endif

int m4u_config_port(M4U_PORT_STRUCT *pM4uPort);
struct device *m4u_get_larbdev(int portid);
mva_info_t *m4u_alloc_garbage_list(unsigned int mvaStart,
			   unsigned int bufSize,
			   M4U_MODULE_ID_ENUM eModuleID,
			   unsigned long va,
			   unsigned int flags, int security, int cache_coherent);

struct sg_table *m4u_find_sgtable(unsigned int mva);
struct sg_table *m4u_del_sgtable(unsigned int mva);
struct sg_table *m4u_add_sgtable(struct mva_sglist *mva_sg);
int m4u_va_align(unsigned long *addr, unsigned int *size);
int m4u_alloc_mva(M4U_MODULE_ID_ENUM eModuleID,
		  unsigned long BufAddr,
		  unsigned int BufSize,
		  int security, int cache_coherent, unsigned int *pRetMVABuf);
struct sg_table *pseudo_get_sg(int portid, unsigned long va, int size);
int pseudo_alloc_mva(m4u_client_t *client, M4U_PORT_ID port,
			  unsigned long va, struct sg_table *sg_table,
			  unsigned int size, unsigned int prot, unsigned int flags, unsigned int *pMva);

int pseudo_dealloc_mva(m4u_client_t *client, M4U_PORT_ID port, unsigned int mva);

int __m4u_dealloc_mva(M4U_MODULE_ID_ENUM eModuleID,
		      const unsigned long BufAddr,
		      const unsigned int BufSize, const unsigned int MVA, struct sg_table *sg_table);

int m4u_dealloc_mva(M4U_MODULE_ID_ENUM eModuleID,
		    const unsigned long BufAddr, const unsigned int BufSize,
		    const unsigned int MVA);

mva_info_t *m4u_delete_from_garbage_list(M4U_MOUDLE_STRUCT *p_m4u_module,
						struct file *a_pstFile);
int m4u_cache_sync_init(void);
struct page *m4u_cache_get_page(unsigned long va);
int __m4u_cache_sync_kernel(const void *start, size_t size, int direction);
void *m4u_cache_map_page_va(struct page *page);
void m4u_cache_unmap_page_va(unsigned long va);
int __m4u_cache_sync_user(unsigned long start, size_t size, int direction);
int m4u_do_dma_cache_maint(M4U_MODULE_ID_ENUM eModuleID, const void *va, size_t size, int direction);
int m4u_dma_cache_maint(M4U_MODULE_ID_ENUM eModuleID,
		const void *start, size_t size, int direction);

int m4u_dealloc_mva_sg(M4U_MODULE_ID_ENUM eModuleID,
		       struct sg_table *sg_table,
		       const unsigned int BufSize, const unsigned int MVA);

int m4u_alloc_mva_sg(port_mva_info_t *port_info,
			struct sg_table *sg_table);

int pseudo_config_port_tee(int kernelport);

extern void smp_inner_dcache_flush_all(void);
extern phys_addr_t mtkfb_get_fb_base(void);
extern size_t mtkfb_get_fb_size(void);
static inline bool m4u_enable_4G(void)
{
	return (max_pfn > (0xffffffffUL >> PAGE_SHIFT));
}
#ifdef CONFIG_ARM64
struct iova *__alloc_iova(struct iova_domain *iovad, size_t size,
		dma_addr_t dma_limit);
void __free_iova(struct iova_domain *iovad, struct iova *iova);
void __iommu_dma_unmap(struct iommu_domain *domain, dma_addr_t dma_addr);
#else
int arm_coherent_iommu_map_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs);

void arm_coherent_iommu_unmap_sg(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir, struct dma_attrs *attrs);
#endif

/* IOCTL commnad */
#define MTK_M4U_MAGICNO 'g'
#define MTK_M4U_T_POWER_ON            _IOW(MTK_M4U_MAGICNO, 0, int)
#define MTK_M4U_T_POWER_OFF           _IOW(MTK_M4U_MAGICNO, 1, int)
#define MTK_M4U_T_DUMP_REG            _IOW(MTK_M4U_MAGICNO, 2, int)
#define MTK_M4U_T_DUMP_INFO           _IOW(MTK_M4U_MAGICNO, 3, int)
#define MTK_M4U_T_ALLOC_MVA           _IOWR(MTK_M4U_MAGICNO, 4, int)
#define MTK_M4U_T_DEALLOC_MVA         _IOW(MTK_M4U_MAGICNO, 5, int)
#define MTK_M4U_T_INSERT_TLB_RANGE    _IOW(MTK_M4U_MAGICNO, 6, int)
#define MTK_M4U_T_INVALID_TLB_RANGE   _IOW(MTK_M4U_MAGICNO, 7, int)
#define MTK_M4U_T_INVALID_TLB_ALL     _IOW(MTK_M4U_MAGICNO, 8, int)
#define MTK_M4U_T_MANUAL_INSERT_ENTRY _IOW(MTK_M4U_MAGICNO, 9, int)
#define MTK_M4U_T_CACHE_SYNC          _IOW(MTK_M4U_MAGICNO, 10, int)
#define MTK_M4U_T_CONFIG_PORT         _IOW(MTK_M4U_MAGICNO, 11, int)
#define MTK_M4U_T_CONFIG_ASSERT       _IOW(MTK_M4U_MAGICNO, 12, int)
#define MTK_M4U_T_INSERT_WRAP_RANGE   _IOW(MTK_M4U_MAGICNO, 13, int)
#define MTK_M4U_T_MONITOR_START       _IOW(MTK_M4U_MAGICNO, 14, int)
#define MTK_M4U_T_MONITOR_STOP        _IOW(MTK_M4U_MAGICNO, 15, int)
#define MTK_M4U_T_RESET_MVA_RELEASE_TLB  _IOW(MTK_M4U_MAGICNO, 16, int)
#define MTK_M4U_T_CONFIG_PORT_ROTATOR _IOW(MTK_M4U_MAGICNO, 17, int)
#define MTK_M4U_T_QUERY_MVA           _IOW(MTK_M4U_MAGICNO, 18, int)
#define MTK_M4U_T_M4UDrv_CONSTRUCT    _IOW(MTK_M4U_MAGICNO, 19, int)
#define MTK_M4U_T_M4UDrv_DECONSTRUCT  _IOW(MTK_M4U_MAGICNO, 20, int)
#define MTK_M4U_T_DUMP_PAGETABLE      _IOW(MTK_M4U_MAGICNO, 21, int)
#define MTK_M4U_T_REGISTER_BUFFER     _IOW(MTK_M4U_MAGICNO, 22, int)
#define MTK_M4U_T_CACHE_FLUSH_ALL     _IOW(MTK_M4U_MAGICNO, 23, int)
#define MTK_M4U_T_CONFIG_PORT_ARRAY   _IOW(MTK_M4U_MAGICNO, 26, int)
#define MTK_M4U_T_CONFIG_MAU          _IOW(MTK_M4U_MAGICNO, 27, int)
#define MTK_M4U_T_CONFIG_TF           _IOW(MTK_M4U_MAGICNO, 28, int)

#if IS_ENABLED(CONFIG_COMPAT)
typedef struct {
	compat_uint_t port;
	compat_ulong_t BufAddr;
	compat_uint_t BufSize;
	compat_uint_t prot;
	compat_uint_t MVAStart;
	compat_uint_t MVAEnd;
	compat_uint_t flags;
} COMPAT_M4U_MOUDLE_STRUCT;

typedef struct {
	compat_uint_t port;
	compat_uint_t eCacheSync;
	compat_ulong_t va;
	compat_uint_t size;
	compat_uint_t mva;
} COMPAT_M4U_CACHE_STRUCT;


#define COMPAT_MTK_M4U_T_ALLOC_MVA	  _IOWR(MTK_M4U_MAGICNO, 4, int)
#define COMPAT_MTK_M4U_T_DEALLOC_MVA  _IOW(MTK_M4U_MAGICNO, 5, int)
#define COMPAT_MTK_M4U_T_CACHE_SYNC   _IOW(MTK_M4U_MAGICNO, 10, int)
#endif

#ifdef M4U_TEE_SERVICE_ENABLE
extern bool m4u_tee_en;
int smi_reg_restore_sec(void);
int smi_reg_backup_sec(void);
/*int m4u_config_port_array_tee(unsigned char* port_array);*/
int m4u_dump_secpgd(unsigned int port, unsigned int faultmva);
#endif

#endif
