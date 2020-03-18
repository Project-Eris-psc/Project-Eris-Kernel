#ifndef __MTK_NAND_OPS_H__
#define __MTK_NAND_OPS_H__

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/miscdevice.h>
#include <mt-plat/dma.h>
#include <linux/rtc.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
/* #include <linux/xlog.h> */

#include "mtk_nand.h"
#include "mtk_nand_util.h"
#include "partition_define.h"
#include "nand_device_define.h"
#include "mtk_nand_chip.h"

/* #define MTK_FORCE_SYNC_OPS */
/* #define MTK_FORCE_READ_FULL_PAGE */

#define MTK_NAND_OPS_BG_CTRL

/* UNIT TEST RELATED */
/* #define MTK_NAND_CHIP_TEST */
/* #define MTK_NAND_CHIP_DUMP_DATA_TEST */
#define MTK_NAND_CHIP_MULTI_PLANE_TEST
/* #define MTK_NAND_READ_COMPARE */
#define MTK_NAND_PERFORMANCE_TEST

extern flashdev_info_t devinfo;
extern bool tlc_snd_phyplane;
extern enum NFI_TLC_PG_CYCLE tlc_program_cycle;
extern bool tlc_lg_left_plane;
extern struct mtk_nand_host *host;
#if 0
extern u32 Nand_ErrBitLoc[][96];
extern u32 Nand_ErrNUM[];
#endif
extern void dump_nfi(void);

#define NAND_DEBUG_DISABLE	1
/* nand debug messages */
#if NAND_DEBUG_DISABLE
#define nand_debug(fmt, ...) do {} while (0)
#else
#define nand_debug(fmt, ...) pr_err("NAND: " fmt "\n", ##__VA_ARGS__)
#endif

/* nand error messages */
#define nand_info(fmt, ...) pr_err("NAND:%s %d info: " fmt "\n",      \
	__func__, __LINE__,  ##__VA_ARGS__)

/* nand error messages */
#define nand_err(fmt, ...) pr_err("NAND:%s %d failed: " fmt "\n",      \
	__func__, __LINE__,  ##__VA_ARGS__)

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef NULL
#define NULL  (0)
#endif


#define MTK_TLC_DIV	3
#define MTK_MLC_DIV	2

/**********  BMT Related ***********/

#define DATA_BAD_BLK		0xffff

#define DATA_MAX_BMT_COUNT		0x400
#define DATA_BMT_VERSION		(1)

#define BAD_BLOCK_SHIPPED		(1)
#define BAD_BLOCK_ERASE_FAILED	(2)
#define BAD_BLOCK_WRITE_FAILED	(3)

struct data_bmt_entry {
	u16 bad_index;	/* bad block index */
	u8 flag;		/* mapping block index in the replace pool */
};

struct data_bmt_struct {
	struct data_bmt_entry entry[DATA_MAX_BMT_COUNT];
	unsigned int  version;
	unsigned int bad_count;		/* bad block count */
	unsigned int start_block;	/* data partition start block addr */
	unsigned int end_block;		/* data partition start block addr */
	unsigned int checksum;
};

/**********  PMT Related ***********/

#define FTL_PARTITION_NAME	"userdata"

struct nand_ftl_partition_info {
	unsigned int start_block;		/* Number of data blocks */
	unsigned int total_block;		/* Number of block */
	unsigned int slc_ratio;		/* FTL SLC ratio here */
	unsigned int slc_block;		/* FTL SLC ratio here */
};

enum operation_types {
	MTK_NAND_OP_READ = 0,
	MTK_NAND_OP_WRITE,
	MTK_NAND_OP_ERASE,
};

struct list_node {
	struct list_node *next;
};

#define containerof(ptr, type, member) \
	((type *)((unsigned long)(ptr) - __builtin_offsetof(type, member)))

struct mtk_nand_chip_operation {
	struct mtk_nand_chip_info *info; /* Data info */
	enum operation_types types;
	/* Operation type, 0: Read, 1: write, 2:Erase*/
	int block;
	int page;
	int offset;
	int size;
	bool more;
	unsigned char *data_buffer;
	unsigned char *oob_buffer;
	mtk_nand_callback_func callback;
	void *userdata;
};

struct nand_work {
	struct list_node list;
	struct mtk_nand_chip_operation ops;
};

typedef struct list_node *(*is_data_ready)(
		struct mtk_nand_chip_info *info,
		struct list_node *head,
		struct list_node *cur, unsigned int *count);

struct worklist_ctrl;

typedef struct list_node *(*process_list_data)(
			struct mtk_nand_chip_info *info,
			struct worklist_ctrl *list_ctrl,
			struct list_node *start_node,
			int count);

struct worklist_ctrl {
	struct mutex sync_lock;
	spinlock_t list_lock;
	struct list_node head;
	int total_num;
	is_data_ready is_ready_func;
	process_list_data process_data_func;
};

struct mtk_nand_data_info {
	struct data_bmt_struct bmt;
	struct mtk_nand_chip_bbt_info chip_bbt;
	struct mtk_nand_chip_info chip_info;
	struct nand_ftl_partition_info partition_info;

	struct worklist_ctrl elist_ctrl;
	struct worklist_ctrl wlist_ctrl;
#ifdef MTK_NAND_OPS_BG_CTRL
	struct completion ops_ctrl;
#endif
	struct task_struct *nand_bgt;

	struct mtd_info *mtd;
};

extern int mtk_nand_ops_init(struct mtd_info *mtd, struct nand_chip *chip);
extern int mtk_nand_data_info_init(void);
extern int get_data_partition_info(struct nand_ftl_partition_info *info);

extern bool mtk_nand_exec_read_sector(struct mtd_info *mtd,
			u32 u4RowAddr, u32 u4ColAddr, u32 u4PageSize,
			u8 *pPageBuf, u8 *pFDMBuf, int subpageno);
extern int mtk_nand_exec_write_page_hw(struct mtd_info *mtd,
			u32 u4RowAddr, u32 u4PageSize,
			u8 *pPageBuf, u8 *pFDMBuf);
extern int mtk_chip_erase_blocks(struct mtd_info *mtd, int page, int page1);

#endif
