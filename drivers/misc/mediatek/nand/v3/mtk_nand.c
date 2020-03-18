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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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
/* #include <linux/xlog.h> */
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/miscdevice.h>
/* #include <mach/dma.h> */
#include <mt-plat/dma.h>
/* #include <mach/devs.h> */
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#else
#include <mach/mt_reg_base.h>
#endif
#include <mtk_nand.h>
#include <bmt.h>
#include "mtk_nand_util.h"
#include <partition_define.h>
#include <linux/rtc.h>
#include <nand_device_define.h>

#ifndef CONFIG_MTK_LEGACY
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#endif

#define READ_REGISTER_UINT8(reg) \
	(*(volatile unsigned char * const)(reg))

#define READ_REGISTER_UINT16(reg) \
	(*(volatile unsigned short * const)(reg))

#define READ_REGISTER_UINT32(reg) \
	(*(volatile unsigned int * const)(reg))


#define INREG8(x)			READ_REGISTER_UINT8((unsigned char *)((void *)(x)))
#define INREG16(x)			READ_REGISTER_UINT16((unsigned short *)((void *)(x)))
#define INREG32(x)			READ_REGISTER_UINT32((unsigned int *)((void *)(x)))
#define DRV_Reg8(addr)				INREG8(addr)
#define DRV_Reg16(addr)				INREG16(addr)
#define DRV_Reg32(addr)				INREG32(addr)
#define DRV_Reg(addr)				DRV_Reg16(addr)

#define WRITE_REGISTER_UINT8(reg, val) \
	((*(volatile unsigned char * const)(reg)) = (val))
#define WRITE_REGISTER_UINT16(reg, val) \
	((*(volatile unsigned short * const)(reg)) = (val))
#define WRITE_REGISTER_UINT32(reg, val) \
	((*(volatile unsigned int * const)(reg)) = (val))

#define OUTREG8(x, y)		WRITE_REGISTER_UINT8((unsigned char *)((void *)(x)), (unsigned char)(y))
#define OUTREG16(x, y)		WRITE_REGISTER_UINT16((unsigned short *)((void *)(x)), (unsigned short)(y))
#define OUTREG32(x, y)		WRITE_REGISTER_UINT32((unsigned int *)((void *)(x)), (unsigned int)(y))
#define DRV_WriteReg8(addr, data)	OUTREG8(addr, data)
#define DRV_WriteReg16(addr, data)	OUTREG16(addr, data)
#define DRV_WriteReg32(addr, data)	OUTREG32(addr, data)
#define DRV_WriteReg(addr, data)	DRV_WriteReg16(addr, data)

static const flashdev_info_t gen_FlashTable_p[] = {
	{{0xC2, 0xDC, 0x90, 0x95, 0x56, 0x00}, 6, 5, IO_8BIT, 0x80000, 128, 2048, 64, 0x10401011,
		0x10818011, 0x101, 80, VEND_MXIC, 1024, "MX30LF4G18AC", 0},
	{{0x98, 0xDC, 0x90, 0x26, 0x76, 0x16}, 6, 5, IO_8BIT, 0x80000, 256, 4096, 256, 0x10401011,
		0x10818011, 0x101, 80, VEND_TOSHIBA, 1024, "TC58NVG2S0HTA00", 0},
	{{0x2C, 0x38, 0x00, 0x26, 0x85, 0x00}, 5, 5, IO_8BIT, 0x100000, 512, 4096, 224, 0x10401011,
		0x10818011, 0x101, 80, VEND_MICRON, 1024, "MT29F8G08ABABA", 0},
	{{0x2C, 0xDA, 0x90, 0x95, 0x06, 0x00}, 5, 5, IO_8BIT, 0x40000, 128, 2048, 64, 0x10401011,
		0x10818011, 0x101, 80, VEND_MICRON, 1024, "MT29F2G08ABAEA", 0},
};

static unsigned int flash_number = sizeof(gen_FlashTable_p) / sizeof(flashdev_info_t);
#define NFI_DEFAULT_CS				(0)

#define mtk_nand_assert(expr)  do { \
	if (unlikely(!(expr))) { \
		pr_crit("MTK nand assert failed in %s at %u (pid %d)\n", \
				__func__, __LINE__, current->pid); \
		dump_stack();	\
	}	\
} while (0)

#ifndef CONFIG_MTK_LEGACY
static struct clk *nfi_hclk;
static struct clk *nfiecc_bclk;
static struct clk *nfi_bclk;
static struct clk *onfi_sel_clk;
static struct clk *onfi_26m_clk;
static struct clk *onfi_mode5;
static struct clk *onfi_mode4;
static struct clk *nfi_bclk_sel;
static struct clk *nfi_ahb_clk;
static struct clk *nfi_1xpad_clk;
static struct clk *nfi_ecc_pclk;
static struct clk *nfi_pclk;
static struct clk *onfi_pad_clk;

static struct regulator *mtk_nand_regulator;
#endif

#define VERSION	"v2.1 Fix AHB virt2phys error"
#define MODULE_NAME	"# MTK NAND #"
#define PROCNAME	"driver/nand"
#define _MTK_NAND_DUMMY_DRIVER_
#define __INTERNAL_USE_AHB_MODE__	(1)
#define CFG_FPGA_PLATFORM (0)	/* for fpga by bean */
#define CFG_RANDOMIZER	  (1)	/* for randomizer code */

#define NFI_TIMEOUT_MS (1000)

/* #define MANUAL_CORRECT */

#if defined(CONFIG_MTK_SLC_NAND_SUPPORT)
bool MLC_DEVICE = true;		/* to build pass xiaolei */
#endif

#ifdef CONFIG_OF
void __iomem *mtk_nfi_base;
void __iomem *mtk_nfiecc_base;
static struct device_node *mtk_nfiecc_node;
unsigned int nfi_irq;
#define MT_NFI_IRQ_ID nfi_irq

void __iomem *mtk_gpio_base;
static struct device_node *mtk_gpio_node;
#define GPIO_BASE	mtk_gpio_base


#ifdef CONFIG_MTK_LEGACY
void __iomem *mtk_efuse_base;
static struct device_node *mtk_efuse_node;
#define EFUSE_BASE	mtk_efuse_base

void __iomem *mtk_infra_base;
static struct device_node *mtk_infra_node;
#endif

/*
 * NFI controller version define
 *
 * 1: MT8127
 * 2: MT8163
 * Reserved.
 */
struct mtk_nfi_compatible {
	unsigned char chip_ver;
};

static const struct mtk_nfi_compatible mt8127_compat = {
	.chip_ver = 1,
};

static const struct mtk_nfi_compatible mt8163_compat = {
	.chip_ver = 2,
};

static const struct mtk_nfi_compatible mt8167_compat = {
	.chip_ver = 3,
};

static const struct of_device_id mtk_nfi_of_match[] = {
	{ .compatible = "mediatek,mt8127-nfi", .data = &mt8127_compat },
	{ .compatible = "mediatek,mt8163-nfi", .data = &mt8163_compat },
	{ .compatible = "mediatek,mt8167-nfi", .data = &mt8167_compat },
	{}
};

const struct mtk_nfi_compatible *mtk_nfi_dev_comp;
#endif

struct device *mtk_dev;
struct scatterlist mtk_sg;
enum dma_data_direction mtk_dir;

#ifndef CONFIG_MTK_FPGA
#if defined(CONFIG_MTK_LEGACY)
#define PERI_NFI_CLK_SOURCE_SEL ((volatile unsigned int *)(mtk_infra_base + 0x098))
/* #define PERI_NFI_MAC_CTRL ((volatile unsigned int *)(PERICFG_BASE + 0x428)) */
#define NFI_PAD_1X_CLOCK (0x1 << 10)	/* nfi1X */
#endif
#endif

#define NFI_SET_REG32(reg, value) \
do {	\
	g_value = (DRV_Reg32(reg) | (value)); \
	DRV_WriteReg32(reg, g_value); \
} while (0)

#define NFI_SET_REG16(reg, value) \
do {	\
	g_value = (DRV_Reg16(reg) | (value)); \
	DRV_WriteReg16(reg, g_value); \
} while (0)

#define NFI_CLN_REG32(reg, value) \
do {	\
	g_value = (DRV_Reg32(reg) & (~(value))); \
	DRV_WriteReg32(reg, g_value); \
} while (0)

#define NFI_CLN_REG16(reg, value) \
do {	\
	g_value = (DRV_Reg16(reg) & (~(value))); \
	DRV_WriteReg16(reg, g_value); \
} while (0)

#define NFI_WAIT_STATE_DONE(state) do {; } while (__raw_readl(NFI_STA_REG32) & state)
#define NFI_WAIT_TO_READY()  do {; } while (!(__raw_readl(NFI_STA_REG32) & STA_BUSY2READY))
#define FIFO_PIO_READY(x)  (0x1 & x)
#define WAIT_NFI_PIO_READY(timeout) \
do {\
	while ((!FIFO_PIO_READY(DRV_Reg(NFI_PIO_DIRDY_REG16))) && (--timeout)) \
		; \
} while (0)


#define NAND_SECTOR_SIZE (512)
#define OOB_PER_SECTOR		(16)
#define OOB_AVAI_PER_SECTOR (8)

#if defined(MTK_COMBO_NAND_SUPPORT)
	/* BMT_POOL_SIZE is not used anymore */
#else
#ifndef PART_SIZE_BMTPOOL
#define BMT_POOL_SIZE (80)
#else
#define BMT_POOL_SIZE (PART_SIZE_BMTPOOL)
#endif
#endif
u8 ecc_threshold;
#define PMT_POOL_SIZE	(2)
/*******************************************************************************
 * Gloable Varible Definition
 *******************************************************************************/
#define TIMEOUT_1	0x1fff
#define TIMEOUT_2	0x8ff
#define TIMEOUT_3	0xffff
#define TIMEOUT_4	0xffff	/* 5000   //PIO */

#define NFI_ISSUE_COMMAND(cmd, col_addr, row_addr, col_num, row_num) \
	do { \
		DRV_WriteReg(NFI_CMD_REG16, cmd); \
		while (DRV_Reg32(NFI_STA_REG32) & STA_CMD_STATE)\
			; \
		DRV_WriteReg32(NFI_COLADDR_REG32, col_addr); \
		DRV_WriteReg32(NFI_ROWADDR_REG32, row_addr); \
		DRV_WriteReg(NFI_ADDRNOB_REG16, col_num | (row_num << ADDR_ROW_NOB_SHIFT))\
			; \
		while (DRV_Reg32(NFI_STA_REG32) & STA_ADDR_STATE)\
			; \
	} while (0)

/* ------------------------------------------------------------------------------- */
static struct completion g_comp_AHB_Done;
static struct completion g_comp_WR_Done;
static struct completion g_comp_ER_Done;
static struct NAND_CMD g_kCMD;
bool g_bInitDone;
static int g_i4Interrupt;
static bool g_bcmdstatus;
/* static bool g_brandstatus; */
static u32 g_value;
static int g_page_size;
static int g_block_size;
static u32 PAGES_PER_BLOCK = 255;
static bool g_bSyncOrToggle;
#ifndef CONFIG_MTK_FPGA
#ifdef CONFIG_MTK_LEGACY
static int g_iNFI2X_CLKSRC = ARMPLL;
#else
static int g_iNFI2X_CLKSRC;
#endif
#endif

static unsigned char g_bHwEcc;
#define LPAGE 32768
#define LSPARE 4096

static u8 *local_buffer_16_align;	/* 16 byte aligned buffer, for HW issue */
__aligned(64)
static u8 local_buffer[LPAGE + LSPARE];
static u8 *temp_buffer_16_align;	/* 16 byte aligned buffer, for HW issue */
__aligned(64)
static u8 temp_buffer[LPAGE + LSPARE];

struct mtk_nand_host *host;

int manu_id;
int dev_id;

static u8 local_oob_buf[LSPARE];

#ifdef _MTK_NAND_DUMMY_DRIVER_
int dummy_driver_debug;
#endif

flashdev_info_t devinfo;

enum NAND_TYPE_MASK {
	TYPE_ASYNC = 0x0,
	TYPE_TOGGLE = 0x1,
	TYPE_SYNC = 0x2,
	TYPE_RESERVED = 0x3,
	TYPE_MLC = 0x4,		/* 1b0 */
	TYPE_SLC = 0x4,		/* 1b1 */
};

typedef u32(*GetLowPageNumber) (u32 pageNo);
typedef u32(*TransferPageNumber) (u32 pageNo, bool high_to_low);

GetLowPageNumber functArray[] = {
	MICRON_TRANSFER,
	HYNIX_TRANSFER,
	SANDISK_TRANSFER,
};

TransferPageNumber fsFuncArray[] = {
	micron_pairpage_mapping,
	hynix_pairpage_mapping,
	sandisk_pairpage_mapping,
};

u32 SANDISK_TRANSFER(u32 pageNo)
{
	if (pageNo == 0)
		return pageNo;
	else
		return pageNo + pageNo - 1;
}

u32 HYNIX_TRANSFER(u32 pageNo)
{
	u32 temp;

	if (pageNo < 4)
		return pageNo;
	temp = pageNo + (pageNo & 0xFFFFFFFE) - 2;
	return temp;
}


u32 MICRON_TRANSFER(u32 pageNo)
{
	u32 temp;

	if (pageNo < 4)
		return pageNo;
	temp = (pageNo - 4) & 0xFFFFFFFE;
	if (pageNo <= 130)
		return (pageNo + temp);
	else
		return (pageNo + temp - 2);
}

u32 sandisk_pairpage_mapping(u32 page, bool high_to_low)
{
	if (high_to_low == true) {
		if (page == 255)
			return page - 2;
		if ((page == 0) || (1 == (page % 2)))
			return page;
		if (page == 2)
			return 0;
		else
			return (page - 3);
	} else {
		if ((page != 0) && (0 == (page % 2)))
			return page;
		if (page == 255)
			return page;
		if (page == 0 || page == 253)
			return page + 2;
		else
			return page + 3;
	}
}

u32 hynix_pairpage_mapping(u32 page, bool high_to_low)
{
	u32 offset;

	if (high_to_low == true) {
		/* Micron 256pages */
		if (page < 4)
			return page;

		offset = page % 4;
		if (offset == 2 || offset == 3)
			return page;

		if (page == 4 || page == 5 || page == 254 || page == 255)
			return page - 4;
		else
			return page - 6;
	} else {
		if (page > 251)
			return page;
		if (page == 0 || page == 1)
			return page + 4;
		offset = page % 4;
		if (offset == 0 || offset == 1)
			return page;
		else
			return page + 6;
	}
}

u32 micron_pairpage_mapping(u32 page, bool high_to_low)
{
	u32 offset;

	if (high_to_low == true) {
		/* Micron 256pages */
		if ((page < 4) || (page > 251))
			return page;

		offset = page % 4;
		if (offset == 0 || offset == 1)
			return page;
		else
			return page - 6;
	} else {
		if ((page == 2) || (page == 3) || (page > 247))
			return page;
		offset = page % 4;
		if (offset == 0 || offset == 1)
			return page + 6;
		else
			return page;
	}
}

#ifdef CONFIG_MTK_FPGA
void nand_enable_clock(void)
{

}

void nand_disable_clock(void)
{

}

void nand_prepare_clock(void)
{

}

void nand_unprepare_clock(void)
{

}
#else
void nand_prepare_clock(void)
{
	#if !defined(CONFIG_MTK_LEGACY)
	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	clk_prepare(nfi_hclk);
	clk_prepare(nfiecc_bclk);
	clk_prepare(nfi_bclk);
	if (mtk_nfi_dev_comp->chip_ver == 2) {
		clk_prepare(nfi_pclk);
		clk_prepare(nfi_ecc_pclk);
	}
	#endif
	#endif
}

void nand_unprepare_clock(void)
{
	#if !defined(CONFIG_MTK_LEGACY)
	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	clk_unprepare(nfi_hclk);
	clk_unprepare(nfiecc_bclk);
	clk_unprepare(nfi_bclk);
	if (mtk_nfi_dev_comp->chip_ver == 2) {
		clk_unprepare(nfi_pclk);
		clk_unprepare(nfi_ecc_pclk);
	}
	#endif
	#endif
}

void nand_enable_clock(void)
{
#if defined(CONFIG_MTK_LEGACY)
	if (mtk_nfi_dev_comp->chip_ver == 1) {
		enable_clock(MT_CG_PERI_NFI, "NFI");
		enable_clock(MT_CG_PERI_NFI_ECC, "NFI");
		enable_clock(MT_CG_PERI_NFIPAD, "NFI");
	} else if (mtk_nfi_dev_comp->chip_ver == 2) {
		enable_clock(MT_CG_INFRA_NFI, "NFI");
		enable_clock(MT_CG_INFRA_NFI_ECC, "NFI");
		enable_clock(MT_CG_INFRA_NFI_BCLK, "NFI");
	} else {
		pr_err("[nand_enable_clock] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
			mtk_nfi_dev_comp->chip_ver);
	}
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	clk_enable(nfi_hclk);
	clk_enable(nfiecc_bclk);
	clk_enable(nfi_bclk);
	if (mtk_nfi_dev_comp->chip_ver == 2) {
		clk_enable(nfi_pclk);
		clk_enable(nfi_ecc_pclk);
	}
#endif
#endif
}

void nand_disable_clock(void)
{
#if defined(CONFIG_MTK_LEGACY)
	if (mtk_nfi_dev_comp->chip_ver == 1) {
		disable_clock(MT_CG_PERI_NFIPAD, "NFI");
		disable_clock(MT_CG_PERI_NFI_ECC, "NFI");
		disable_clock(MT_CG_PERI_NFI, "NFI");
	} else if (mtk_nfi_dev_comp->chip_ver == 2) {
		disable_clock(MT_CG_INFRA_NFI_BCLK, "NFI");
		disable_clock(MT_CG_INFRA_NFI_ECC, "NFI");
		disable_clock(MT_CG_INFRA_NFI, "NFI");
	} else {
		pr_err("[nand_disable_clock] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
			mtk_nfi_dev_comp->chip_ver);
	}
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	clk_disable(nfi_hclk);
	clk_disable(nfiecc_bclk);
	clk_disable(nfi_bclk);
	if (mtk_nfi_dev_comp->chip_ver == 2) {
		clk_disable(nfi_pclk);
		clk_disable(nfi_ecc_pclk);
	}
#endif
#endif
}
#endif

static struct nand_ecclayout nand_oob_16 = {
	.eccbytes = 8,
	.eccpos = {8, 9, 10, 11, 12, 13, 14, 15},
	.oobfree = {{1, 6}, {0, 0}}
};

struct nand_ecclayout nand_oob_64 = {
	.eccbytes = 32,
	.eccpos = {32, 33, 34, 35, 36, 37, 38, 39,
			40, 41, 42, 43, 44, 45, 46, 47,
			48, 49, 50, 51, 52, 53, 54, 55,
			56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {{1, 7}, {9, 7}, {17, 7}, {25, 6}, {0, 0}}
};

struct nand_ecclayout nand_oob_128 = {
	.eccbytes = 64,
	.eccpos = {
			64, 65, 66, 67, 68, 69, 70, 71,
			72, 73, 74, 75, 76, 77, 78, 79,
			80, 81, 82, 83, 84, 85, 86, 86,
			88, 89, 90, 91, 92, 93, 94, 95,
			96, 97, 98, 99, 100, 101, 102, 103,
			104, 105, 106, 107, 108, 109, 110, 111,
			112, 113, 114, 115, 116, 117, 118, 119,
			120, 121, 122, 123, 124, 125, 126, 127},
	.oobfree = {{1, 7}, {9, 7}, {17, 7}, {25, 7}, {33, 7}, {41, 7}, {49, 7}, {57, 6} }
};

/**************************************************************************
*  Randomizer
**************************************************************************/
#define SS_SEED_NUM 128
#ifdef CONFIG_MTK_LEGACY
#define EFUSE_RANDOM_CFG	((volatile u32 *)(EFUSE_BASE + 0x01C0))
#endif

static bool use_randomizer = false;
static bool pre_randomizer = false;

static unsigned short SS_RANDOM_SEED[SS_SEED_NUM] = {
	/* for page 0~127 */
	0x576A, 0x05E8, 0x629D, 0x45A3, 0x649C, 0x4BF0, 0x2342, 0x272E,
	0x7358, 0x4FF3, 0x73EC, 0x5F70, 0x7A60, 0x1AD8, 0x3472, 0x3612,
	0x224F, 0x0454, 0x030E, 0x70A5, 0x7809, 0x2521, 0x484F, 0x5A2D,
	0x492A, 0x043D, 0x7F61, 0x3969, 0x517A, 0x3B42, 0x769D, 0x0647,
	0x7E2A, 0x1383, 0x49D9, 0x07B8, 0x2578, 0x4EEC, 0x4423, 0x352F,
	0x5B22, 0x72B9, 0x367B, 0x24B6, 0x7E8E, 0x2318, 0x6BD0, 0x5519,
	0x1783, 0x18A7, 0x7B6E, 0x7602, 0x4B7F, 0x3648, 0x2C53, 0x6B99,
	0x0C23, 0x67CF, 0x7E0E, 0x4D8C, 0x5079, 0x209D, 0x244A, 0x747B,
	0x350B, 0x0E4D, 0x7004, 0x6AC3, 0x7F3E, 0x21F5, 0x7A15, 0x2379,
	0x1517, 0x1ABA, 0x4E77, 0x15A1, 0x04FA, 0x2D61, 0x253A, 0x1302,
	0x1F63, 0x5AB3, 0x049A, 0x5AE8, 0x1CD7, 0x4A00, 0x30C8, 0x3247,
	0x729C, 0x5034, 0x2B0E, 0x57F2, 0x00E4, 0x575B, 0x6192, 0x38F8,
	0x2F6A, 0x0C14, 0x45FC, 0x41DF, 0x38DA, 0x7AE1, 0x7322, 0x62DF,
	0x5E39, 0x0E64, 0x6D85, 0x5951, 0x5937, 0x6281, 0x33A1, 0x6A32,
	0x3A5A, 0x2BAC, 0x743A, 0x5E74, 0x3B2E, 0x7EC7, 0x4FD2, 0x5D28,
	0x751F, 0x3EF8, 0x39B1, 0x4E49, 0x746B, 0x6EF6, 0x44BE, 0x6DB7
};

void dump_nfi(void)
{
#if __DEBUG_NAND
	pr_debug("~~~~Dump NFI Register in Kernel~~~~\n");
	pr_debug("NFI_CNFG_REG16: 0x%x\n", DRV_Reg16(NFI_CNFG_REG16));
	if (mtk_nfi_dev_comp->chip_ver == 1)
		pr_debug("NFI_PAGEFMT_REG16: 0x%x\n", DRV_Reg32(NFI_PAGEFMT_REG16));
	else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3))
		pr_debug("NFI_PAGEFMT_REG32: 0x%x\n", DRV_Reg32(NFI_PAGEFMT_REG32));
	pr_debug("NFI_CON_REG16: 0x%x\n", DRV_Reg16(NFI_CON_REG16));
	pr_debug("NFI_ACCCON_REG32: 0x%x\n", DRV_Reg32(NFI_ACCCON_REG32));
	pr_debug("NFI_INTR_EN_REG16: 0x%x\n", DRV_Reg16(NFI_INTR_EN_REG16));
	pr_debug("NFI_INTR_REG16: 0x%x\n", DRV_Reg16(NFI_INTR_REG16));
	pr_debug("NFI_CMD_REG16: 0x%x\n", DRV_Reg16(NFI_CMD_REG16));
	pr_debug("NFI_ADDRNOB_REG16: 0x%x\n", DRV_Reg16(NFI_ADDRNOB_REG16));
	pr_debug("NFI_COLADDR_REG32: 0x%x\n", DRV_Reg32(NFI_COLADDR_REG32));
	pr_debug("NFI_ROWADDR_REG32: 0x%x\n", DRV_Reg32(NFI_ROWADDR_REG32));
	pr_debug("NFI_STRDATA_REG16: 0x%x\n", DRV_Reg16(NFI_STRDATA_REG16));
	pr_debug("NFI_DATAW_REG32: 0x%x\n", DRV_Reg32(NFI_DATAW_REG32));
	pr_debug("NFI_DATAR_REG32: 0x%x\n", DRV_Reg32(NFI_DATAR_REG32));
	pr_debug("NFI_PIO_DIRDY_REG16: 0x%x\n", DRV_Reg16(NFI_PIO_DIRDY_REG16));
	pr_debug("NFI_STA_REG32: 0x%x\n", DRV_Reg32(NFI_STA_REG32));
	pr_debug("NFI_FIFOSTA_REG16: 0x%x\n", DRV_Reg16(NFI_FIFOSTA_REG16));
	/* pr_debug("NFI_LOCKSTA_REG16: 0x%x\n", DRV_Reg16(NFI_LOCKSTA_REG16)); */
	pr_debug("NFI_ADDRCNTR_REG16: 0x%x\n", DRV_Reg16(NFI_ADDRCNTR_REG16));
	pr_debug("NFI_STRADDR_REG32: 0x%x\n", DRV_Reg32(NFI_STRADDR_REG32));
	pr_debug("NFI_BYTELEN_REG16: 0x%x\n", DRV_Reg16(NFI_BYTELEN_REG16));
	pr_debug("NFI_CSEL_REG16: 0x%x\n", DRV_Reg16(NFI_CSEL_REG16));
	pr_debug("NFI_IOCON_REG16: 0x%x\n", DRV_Reg16(NFI_IOCON_REG16));
	pr_debug("NFI_FDM0L_REG32: 0x%x\n", DRV_Reg32(NFI_FDM0L_REG32));
	pr_debug("NFI_FDM0M_REG32: 0x%x\n", DRV_Reg32(NFI_FDM0M_REG32));
	pr_debug("NFI_LOCK_REG16: 0x%x\n", DRV_Reg16(NFI_LOCK_REG16));
	pr_debug("NFI_LOCKCON_REG32: 0x%x\n", DRV_Reg32(NFI_LOCKCON_REG32));
	pr_debug("NFI_LOCKANOB_REG16: 0x%x\n", DRV_Reg16(NFI_LOCKANOB_REG16));
	pr_debug("NFI_FIFODATA0_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA0_REG32));
	pr_debug("NFI_FIFODATA1_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA1_REG32));
	pr_debug("NFI_FIFODATA2_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA2_REG32));
	pr_debug("NFI_FIFODATA3_REG32: 0x%x\n", DRV_Reg32(NFI_FIFODATA3_REG32));
	pr_debug("NFI_MASTERSTA_REG16: 0x%x\n", DRV_Reg16(NFI_MASTERSTA_REG16));
	pr_debug("NFI_DEBUG_CON1_REG16: 0x%x\n", DRV_Reg16(NFI_DEBUG_CON1_REG16));
	pr_debug("ECC_ENCCON_REG16	  :%x\n", *ECC_ENCCON_REG16);
	pr_debug("ECC_ENCCNFG_REG32	:%x\n", *ECC_ENCCNFG_REG32);
	pr_debug("ECC_ENCDIADDR_REG32	:%x\n", *ECC_ENCDIADDR_REG32);
	pr_debug("ECC_ENCIDLE_REG32	:%x\n", *ECC_ENCIDLE_REG32);
	pr_debug("ECC_ENCPAR0_REG32	:%x\n", *ECC_ENCPAR0_REG32);
	pr_debug("ECC_ENCPAR1_REG32	:%x\n", *ECC_ENCPAR1_REG32);
	pr_debug("ECC_ENCPAR2_REG32	:%x\n", *ECC_ENCPAR2_REG32);
	pr_debug("ECC_ENCPAR3_REG32	:%x\n", *ECC_ENCPAR3_REG32);
	pr_debug("ECC_ENCPAR4_REG32	:%x\n", *ECC_ENCPAR4_REG32);
	pr_debug("ECC_ENCPAR5_REG32	:%x\n", *ECC_ENCPAR5_REG32);
	pr_debug("ECC_ENCPAR6_REG32	:%x\n", *ECC_ENCPAR6_REG32);
	pr_debug("ECC_ENCSTA_REG32	:%x\n", *ECC_ENCSTA_REG32);
	pr_debug("ECC_ENCIRQEN_REG16	:%x\n", *ECC_ENCIRQEN_REG16);
	pr_debug("ECC_ENCIRQSTA_REG16 :%x\n", *ECC_ENCIRQSTA_REG16);
	pr_debug("ECC_DECCON_REG16	:%x\n", *ECC_DECCON_REG16);
	pr_debug("ECC_DECCNFG_REG32	:%x\n", *ECC_DECCNFG_REG32);
	pr_debug("ECC_DECDIADDR_REG32 :%x\n", *ECC_DECDIADDR_REG32);
	pr_debug("ECC_DECIDLE_REG16	:%x\n", *ECC_DECIDLE_REG16);
	pr_debug("ECC_DECFER_REG16	:%x\n", *ECC_DECFER_REG16);
	pr_debug("ECC_DECENUM0_REG32	:%x\n", *ECC_DECENUM0_REG32);
	pr_debug("ECC_DECENUM1_REG32	:%x\n", *ECC_DECENUM1_REG32);
	pr_debug("ECC_DECDONE_REG16	:%x\n", *ECC_DECDONE_REG16);
	pr_debug("ECC_DECEL0_REG32	:%x\n", *ECC_DECEL0_REG32);
	pr_debug("ECC_DECEL1_REG32	:%x\n", *ECC_DECEL1_REG32);
	pr_debug("ECC_DECEL2_REG32	:%x\n", *ECC_DECEL2_REG32);
	pr_debug("ECC_DECEL3_REG32	:%x\n", *ECC_DECEL3_REG32);
	pr_debug("ECC_DECEL4_REG32	:%x\n", *ECC_DECEL4_REG32);
	pr_debug("ECC_DECEL5_REG32	:%x\n", *ECC_DECEL5_REG32);
	pr_debug("ECC_DECEL6_REG32	:%x\n", *ECC_DECEL6_REG32);
	pr_debug("ECC_DECEL7_REG32	:%x\n", *ECC_DECEL7_REG32);
	pr_debug("ECC_DECIRQEN_REG16	:%x\n", *ECC_DECIRQEN_REG16);
	pr_debug("ECC_DECIRQSTA_REG16 :%x\n", *ECC_DECIRQSTA_REG16);
	pr_debug("ECC_DECFSM_REG32	:%x\n", *ECC_DECFSM_REG32);
	pr_debug("ECC_BYPASS_REG32	:%x\n", *ECC_BYPASS_REG32);
#endif
}

bool get_device_info(u8 *id, flashdev_info_t *devinfo)
{
	u32 i, m, n, mismatch;
	int target = -1;
	u8 target_id_len = 0;

	for (i = 0; i < flash_number; i++) {
		mismatch = 0;
		for (m = 0; m < gen_FlashTable_p[i].id_length; m++) {
			if (id[m] != gen_FlashTable_p[i].id[m]) {
				mismatch = 1;
				break;
			}
		}
		if (mismatch == 0 && gen_FlashTable_p[i].id_length > target_id_len) {
			target = i;
			target_id_len = gen_FlashTable_p[i].id_length;
		}
	}

	if (target != -1) {
		pr_debug("Recognize NAND: ID [");
		for (n = 0; n < gen_FlashTable_p[target].id_length; n++) {
			devinfo->id[n] = gen_FlashTable_p[target].id[n];
			pr_debug("%x ", devinfo->id[n]);
		}
		pr_debug("], Device Name [%s], Page Size [%d]B Spare Size [%d]B Total Size [%d]MB\n",
			gen_FlashTable_p[target].devciename, gen_FlashTable_p[target].pagesize,
			gen_FlashTable_p[target].sparesize, gen_FlashTable_p[target].totalsize);
		devinfo->id_length = gen_FlashTable_p[target].id_length;
		devinfo->blocksize = gen_FlashTable_p[target].blocksize;
		devinfo->addr_cycle = gen_FlashTable_p[target].addr_cycle;
		devinfo->iowidth = gen_FlashTable_p[target].iowidth;
		devinfo->timmingsetting = gen_FlashTable_p[target].timmingsetting;
		/* Modify MT8127 timing setting */
		if (mtk_nfi_dev_comp->chip_ver == 1) {
			if (devinfo->timmingsetting == 0x10401011)
				devinfo->timmingsetting = 0x10804222;
		}
		devinfo->advancedmode = gen_FlashTable_p[target].advancedmode;
		devinfo->pagesize = gen_FlashTable_p[target].pagesize;
		devinfo->sparesize = gen_FlashTable_p[target].sparesize;
		devinfo->totalsize = gen_FlashTable_p[target].totalsize;
		devinfo->sectorsize = gen_FlashTable_p[target].sectorsize;
		devinfo->s_acccon = gen_FlashTable_p[target].s_acccon;
		devinfo->s_acccon1 = gen_FlashTable_p[target].s_acccon1;
		devinfo->freq = gen_FlashTable_p[target].freq;
		devinfo->vendor = gen_FlashTable_p[target].vendor;
		/* devinfo->ttarget = gen_FlashTable[target].ttarget; */
		memcpy(devinfo->devciename, gen_FlashTable_p[target].devciename,
				sizeof(devinfo->devciename));
		return true;
	}
	pr_err("Not Found NAND: ID [");
	for (n = 0; n < NAND_MAX_ID; n++)
		pr_err("%x ", id[n]);
	pr_err("]\n");
	return false;
}

/* extern bool MLC_DEVICE; */
static bool mtk_nand_reset(void);

u32 mtk_nand_page_transform(struct mtd_info *mtd, struct nand_chip *chip, u32 page, u32 *blk,
				u32 *map_blk)
{
	u32 block_size = (devinfo.blocksize * 1024);
	u32 page_size = (1 << chip->page_shift);
	u32 block;
	u32 page_in_block;
	u32 mapped_block;

	block = page / (block_size / page_size);
	page_in_block = page % (block_size / page_size);
	mapped_block = get_mapping_block_index(block);
	*blk = block;
	*map_blk = mapped_block;

	return page_in_block;
}

static int mtk_nand_interface_config(struct mtd_info *mtd)
{
	u32 timeout;
	u32 acccon1;
	/* int clksrc = ARMPLL; */
	if (devinfo.iowidth == IO_ONFI || devinfo.iowidth == IO_TOGGLEDDR
		|| devinfo.iowidth == IO_TOGGLESDR) {
		nand_enable_clock();
#ifndef CONFIG_MTK_FPGA
		/* 0:26M   1:182M  2:156M  3:124.8M  4:91M	5:62.4M   6:39M   7:26M */
		if (devinfo.freq == 80) {	/* mode 4 */
#ifdef CONFIG_MTK_LEGACY
			g_iNFI2X_CLKSRC = MSDCPLL; /* 156M */
#else
			g_iNFI2X_CLKSRC = 2;	/* 156M */
#endif

		} else if (devinfo.freq == 100) {	/* mode 5 */
#ifdef CONFIG_MTK_LEGACY
			g_iNFI2X_CLKSRC = MAINPLL; /* 182M */
#else
			g_iNFI2X_CLKSRC = 1;	/* 182M */
#endif
		}
#endif
		/* reset */
		/* pr_debug("[Bean]mode:%d\n", g_iNFI2X_CLKSRC); */
		NFI_ISSUE_COMMAND(NAND_CMD_RESET, 0, 0, 0, 0);
		timeout = TIMEOUT_4;
		while (timeout)
			timeout--;
		mtk_nand_reset();
		mb();
		NFI_CLN_REG32(NFI_DEBUG_CON1_REG16, HWDCM_SWCON_ON);

		/* setup register */
		mb();
		NFI_CLN_REG32(NFI_DEBUG_CON1_REG16, NFI_BYPASS);
		/* clear bypass of ecc */
		mb();
		NFI_CLN_REG32(ECC_BYPASS_REG32, ECC_BYPASS);
		mb();
#ifndef CONFIG_MTK_FPGA
		/* DRV_WriteReg32(PERICFG_BASE + 0x5C, 0x0); // setting default AHB clock */
		mb();
#if defined(CONFIG_MTK_LEGACY)
		NFI_SET_REG32(PERI_NFI_CLK_SOURCE_SEL, NFI_PAD_1X_CLOCK);
#else
		clk_set_parent(nfi_bclk_sel, nfi_1xpad_clk);
#endif
		mb();

#if defined(CONFIG_MTK_LEGACY)
		clkmux_sel(MT_MUX_ONFI, g_iNFI2X_CLKSRC, "NFI");
#else
		if (g_iNFI2X_CLKSRC == 1)
			clk_set_parent(onfi_sel_clk, onfi_mode5);
		else if (g_iNFI2X_CLKSRC == 2)
			clk_set_parent(onfi_sel_clk, onfi_mode4);
#endif
		mb();
#endif
		DRV_WriteReg32(NFI_DLYCTRL_REG32, 0x64011);
#ifndef CONFIG_MTK_FPGA
		/* DRV_WriteReg32(PERI_NFI_MAC_CTRL, 0x10006); */
#endif
		while (0 == (DRV_Reg32(NFI_STA_REG32) && STA_FLASH_MACRO_IDLE))
			;
		if (devinfo.iowidth == IO_ONFI)
			DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 2);	/* ONFI */
		else
			DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32, 1);	/* Toggle */
		/* pr_debug("[Timing]0x%x 0x%x\n", devinfo.s_acccon, devinfo.s_acccon1); */
		acccon1 = DRV_Reg32(NFI_ACCCON1_REG3);
		DRV_WriteReg32(NFI_ACCCON1_REG3, devinfo.s_acccon1);
		DRV_WriteReg32(NFI_ACCCON_REG32, devinfo.s_acccon);
		g_bSyncOrToggle = true;

		pr_notice("[%s] success 0x%X\n", __func__, devinfo.iowidth);
		/* extern void log_boot(char *str); */
		/* log_boot("[Bean]sync mode success!"); */
	} else {
		g_bSyncOrToggle = false;
		pr_notice("[%s] legacy interface\n", __func__);
		return 0;
	}

	return 1;
}

#if CFG_RANDOMIZER
static int mtk_nand_turn_on_randomizer(u32 page, int type, int fgPage)
{
	u32 u4NFI_CFG = 0;
	u32 u4NFI_RAN_CFG = 0;
	u32 u4PgNum = page % PAGES_PER_BLOCK;

	u4NFI_CFG = DRV_Reg32(NFI_CNFG_REG16);

	DRV_WriteReg32(NFI_ENMPTY_THRESH_REG32, 40);

	if (type) {
		DRV_WriteReg32(NFI_RANDOM_ENSEED01_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_ENSEED02_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_ENSEED03_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_ENSEED04_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_ENSEED05_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_ENSEED06_TS_REG32, 0);
	} else {
		DRV_WriteReg32(NFI_RANDOM_DESEED01_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_DESEED02_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_DESEED03_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_DESEED04_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_DESEED05_TS_REG32, 0);
		DRV_WriteReg32(NFI_RANDOM_DESEED06_TS_REG32, 0);
	}

	u4NFI_CFG |= CNFG_RAN_SEL;
	if (PAGES_PER_BLOCK <= SS_SEED_NUM) {
		if (type)
			u4NFI_RAN_CFG |= RAN_CNFG_ENCODE_SEED(SS_RANDOM_SEED[u4PgNum % PAGES_PER_BLOCK])
						| RAN_CNFG_ENCODE_EN;
		else
			u4NFI_RAN_CFG |= RAN_CNFG_DECODE_SEED(SS_RANDOM_SEED[u4PgNum % PAGES_PER_BLOCK])
						| RAN_CNFG_DECODE_EN;
	} else {
		if (type)
			u4NFI_RAN_CFG |= RAN_CNFG_ENCODE_SEED(SS_RANDOM_SEED[u4PgNum & (SS_SEED_NUM-1)])
						| RAN_CNFG_ENCODE_EN;
		else
			u4NFI_RAN_CFG |= RAN_CNFG_DECODE_SEED(SS_RANDOM_SEED[u4PgNum & (SS_SEED_NUM-1)])
						| RAN_CNFG_DECODE_EN;
	}

	if (fgPage)
		u4NFI_CFG &= ~CNFG_RAN_SEC;
	else
		u4NFI_CFG |= CNFG_RAN_SEC;

	DRV_WriteReg32(NFI_CNFG_REG16, u4NFI_CFG);
	DRV_WriteReg32(NFI_RANDOM_CNFG_REG32, u4NFI_RAN_CFG);
	return 0;
}

static bool mtk_nand_israndomizeron(void)
{
	u32 nfi_ran_cnfg = 0;

	nfi_ran_cnfg = DRV_Reg32(NFI_RANDOM_CNFG_REG32);
	if (nfi_ran_cnfg & (RAN_CNFG_ENCODE_EN | RAN_CNFG_DECODE_EN))
		return true;

	return false;
}

static void mtk_nand_turn_off_randomizer(void)
{
	u32 u4NFI_CFG = DRV_Reg32(NFI_CNFG_REG16);

	u4NFI_CFG &= ~CNFG_RAN_SEL;
	u4NFI_CFG &= ~CNFG_RAN_SEC;
	DRV_WriteReg32(NFI_RANDOM_CNFG_REG32, 0);
	DRV_WriteReg32(NFI_CNFG_REG16, u4NFI_CFG);
}
#else
#define mtk_nand_israndomizeron() (false)
#define mtk_nand_turn_on_randomizer(page, type, fgPage)
#define mtk_nand_turn_off_randomizer()
#endif


/******************************************************************************
 * mtk_nand_irq_handler
 *
 * DESCRIPTION:
 *	 NAND interrupt handler!
 *
 * PARAMETERS:
 *	 int irq
 *	 void *dev_id
 *
 * RETURNS:
 *	 IRQ_HANDLED : Successfully handle the IRQ
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
/* Modified for TCM used */

static irqreturn_t mtk_nand_irq_handler(int irqno, void *dev_id)
{
	u16 u16IntStatus = DRV_Reg16(NFI_INTR_REG16);

	/* pr_debug("mtk_nand_irq_handler 0x%x here\n", u16IntStatus); */
	/* (void)irqno; */
	if (u16IntStatus & (u16) INTR_WR_DONE) {
		NFI_CLN_REG16(NFI_INTR_EN_REG16, INTR_WR_DONE_EN);
		complete(&g_comp_WR_Done);
	} else if (u16IntStatus & (u16) INTR_ERASE_DONE) {
		NFI_CLN_REG16(NFI_INTR_EN_REG16, INTR_ERASE_DONE_EN);
		complete(&g_comp_ER_Done);
	}
	return IRQ_HANDLED;
}

/******************************************************************************
 * ECC_Config
 *
 * DESCRIPTION:
 *	 Configure HW ECC!
 *
 * PARAMETERS:
 *	 struct mtk_nand_host_hw *hw
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void ECC_Config(struct mtk_nand_host_hw *hw, u32 ecc_bit)
{
	u32 u4ENCODESize;
	u32 u4DECODESize;
	u32 ecc_bit_cfg = ECC_CNFG_ECC4;

	/* Sector + FDM + YAFFS2 meta data bits */
	u4DECODESize = ((hw->nand_sec_size + hw->nand_fdm_size) << 3) + ecc_bit * ECC_PARITY_BIT;

	switch (ecc_bit) {
#if !defined(MTK_COMBO_NAND_SUPPORT) || defined(CONFIG_MTK_SLC_NAND_SUPPORT)
	case 4:
		ecc_bit_cfg = ECC_CNFG_ECC4;
		break;
	case 8:
		ecc_bit_cfg = ECC_CNFG_ECC8;
		break;
	case 10:
		ecc_bit_cfg = ECC_CNFG_ECC10;
		break;
	case 12:
		ecc_bit_cfg = ECC_CNFG_ECC12;
		break;
	case 14:
		ecc_bit_cfg = ECC_CNFG_ECC14;
		break;
	case 16:
		ecc_bit_cfg = ECC_CNFG_ECC16;
		break;
	case 18:
		ecc_bit_cfg = ECC_CNFG_ECC18;
		break;
	case 20:
		ecc_bit_cfg = ECC_CNFG_ECC20;
		break;
	case 22:
		ecc_bit_cfg = ECC_CNFG_ECC22;
		break;
	case 24:
		ecc_bit_cfg = ECC_CNFG_ECC24;
		break;
#endif
	case 28:
		ecc_bit_cfg = ECC_CNFG_ECC28;
		break;
	case 32:
		ecc_bit_cfg = ECC_CNFG_ECC32;
		break;
	case 36:
		ecc_bit_cfg = ECC_CNFG_ECC36;
		break;
	case 40:
		ecc_bit_cfg = ECC_CNFG_ECC40;
		break;
	case 44:
		ecc_bit_cfg = ECC_CNFG_ECC44;
		break;
	case 48:
		ecc_bit_cfg = ECC_CNFG_ECC48;
		break;
	case 52:
		ecc_bit_cfg = ECC_CNFG_ECC52;
		break;
	case 56:
		ecc_bit_cfg = ECC_CNFG_ECC56;
		break;
	case 60:
		ecc_bit_cfg = ECC_CNFG_ECC60;
		break;
	default:
		break;
	}
	DRV_WriteReg16(ECC_DECCON_REG16, DEC_DE);
	do {
		;
	} while (!DRV_Reg16(ECC_DECIDLE_REG16));

	DRV_WriteReg16(ECC_ENCCON_REG16, ENC_DE);
	do {
		;
	} while (!DRV_Reg32(ECC_ENCIDLE_REG32));

	/* setup FDM register base */
	/* DRV_WriteReg32(ECC_FDMADDR_REG32, NFI_FDM0L_REG32); */

	/* Sector + FDM */
	u4ENCODESize = (hw->nand_sec_size + hw->nand_fdm_size) << 3;
	/* Sector + FDM + YAFFS2 meta data bits */

	/* configure ECC decoder && encoder */
	DRV_WriteReg32(ECC_DECCNFG_REG32,
				ecc_bit_cfg | DEC_CNFG_NFI | DEC_CNFG_EMPTY_EN | (u4DECODESize <<
										DEC_CNFG_CODE_SHIFT));

	DRV_WriteReg32(ECC_ENCCNFG_REG32,
				ecc_bit_cfg | ENC_CNFG_NFI | (u4ENCODESize << ENC_CNFG_MSG_SHIFT));
#ifndef MANUAL_CORRECT
	NFI_SET_REG32(ECC_DECCNFG_REG32, DEC_CNFG_CORRECT);
#else
	NFI_SET_REG32(ECC_DECCNFG_REG32, DEC_CNFG_EL);
#endif
}

/******************************************************************************
 * ECC_Decode_Start
 *
 * DESCRIPTION:
 *	 HW ECC Decode Start !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void ECC_Decode_Start(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg16(ECC_DECIDLE_REG16) & DEC_IDLE))
		;
	DRV_WriteReg16(ECC_DECCON_REG16, DEC_EN);
}

/******************************************************************************
 * ECC_Decode_End
 *
 * DESCRIPTION:
 *	 HW ECC Decode End !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void ECC_Decode_End(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg16(ECC_DECIDLE_REG16) & DEC_IDLE))
		;
	DRV_WriteReg16(ECC_DECCON_REG16, DEC_DE);
}

/******************************************************************************
 * ECC_Encode_Start
 *
 * DESCRIPTION:
 *	 HW ECC Encode Start !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void ECC_Encode_Start(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg32(ECC_ENCIDLE_REG32) & ENC_IDLE))
		;
	mb();
	DRV_WriteReg16(ECC_ENCCON_REG16, ENC_EN);
}

/******************************************************************************
 * ECC_Encode_End
 *
 * DESCRIPTION:
 *	 HW ECC Encode End !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void ECC_Encode_End(void)
{
	/* wait for device returning idle */
	while (!(DRV_Reg32(ECC_ENCIDLE_REG32) & ENC_IDLE))
		;
	mb();
	DRV_WriteReg16(ECC_ENCCON_REG16, ENC_DE);
}

/******************************************************************************
 * mtk_nand_check_bch_error
 *
 * DESCRIPTION:
 *	 Check BCH error or not !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd
 *	 u8* pDataBuf
 *	 u32 u4SecIndex
 *	 u32 u4PageAddr
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_check_bch_error(struct mtd_info *mtd, u8 *pDataBuf, u8 *spareBuf,
						u32 u4SecIndex, u32 u4PageAddr, u32 *bitmap)
{
	bool ret = true;
	u16 u2SectorDoneMask = 1 << u4SecIndex;
	u32 u4ErrorNumDebug0, u4ErrorNumDebug1, i, u4ErrNum;
#ifdef MANUAL_CORRECT
	u32 j;
#endif
	u32 timeout = 0xFFFF;
	u32 correct_count = 0;
	u32 page_size = (u4SecIndex + 1) * host->hw->nand_sec_size;
	u32 sec_num = u4SecIndex + 1;
	/* u32 bitflips = sec_num * 39; */
	u16 failed_sec = 0;
	u32 maxSectorBitErr = 0;

#ifdef MANUAL_CORRECT
	u32 err_pos, temp;
	u32 u4ErrByteLoc, u4BitOffset;
#endif
	/* u32 index1; */
	/* u32 u4ErrBitLoc1th, u4ErrBitLoc2nd; */
	/* u32 au4ErrBitLoc[20]; */
	u32 ERR_NUM0 = 0;

	if (mtk_nfi_dev_comp->chip_ver == 1)
		ERR_NUM0 = ERR_NUM0_V1;
	else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3))
		ERR_NUM0 = ERR_NUM0_V2;

	while (0 == (u2SectorDoneMask & DRV_Reg16(ECC_DECDONE_REG16))) {
		timeout--;
		if (timeout == 0)
			return false;
	}
#ifndef MANUAL_CORRECT
	if (0 == (DRV_Reg32(NFI_STA_REG32) & STA_READ_EMPTY)) {
		u4ErrorNumDebug0 = DRV_Reg32(ECC_DECENUM0_REG32);
		u4ErrorNumDebug1 = DRV_Reg32(ECC_DECENUM1_REG32);
		if (0 != (u4ErrorNumDebug0 & 0xFFFFFFFF) || 0 != (u4ErrorNumDebug1 & 0xFFFFFFFF)) {
			for (i = 0; i <= u4SecIndex; ++i) {
				u4ErrNum = (DRV_Reg32((ECC_DECENUM0_REG32 + (i / 4))) >> ((i % 4) * 8)) & ERR_NUM0;

				if (u4ErrNum == ERR_NUM0) {
					failed_sec++;
					ret = false;
					continue;
				}
				if (bitmap)
					*bitmap |= 1 << i;
				if (u4ErrNum) {
					if (maxSectorBitErr < u4ErrNum)
						maxSectorBitErr = u4ErrNum;
					correct_count += u4ErrNum;
				}
			}
			mtd->ecc_stats.failed += failed_sec;
			if ((maxSectorBitErr > ecc_threshold) && (ret != false)) {
				pr_debug("ECC bit flips (0x%x) exceed eccthreshold (0x%x), u4PageAddr 0x%x\n",
					maxSectorBitErr, ecc_threshold, u4PageAddr);
				mtd->ecc_stats.corrected++;
			}
		}
	}

	if ((DRV_Reg32(NFI_STA_REG32) & STA_READ_EMPTY) != 0) {
		ret = true;
		memset(pDataBuf, 0xff, page_size);
		memset(spareBuf, 0xff, sec_num * host->hw->nand_fdm_size);
		maxSectorBitErr = 0;
		failed_sec = 0;
	}
#else
	for (j = 0; j <= u4SecIndex; ++j) {
		u4ErrNum = (DRV_Reg32((ECC_DECENUM0_REG32 + (j / 4))) >> ((j % 4) * 8)) & ERR_NUM0;
		/* We will manually correct the error bits in the last sector, not all the sectors of the page! */
		memset(au4ErrBitLoc, 0x0, sizeof(au4ErrBitLoc));

		if (u4ErrNum) {
			if (u4ErrNum == ERR_NUM0) {
				mtd->ecc_stats.failed++;
				ret = false;
				/*pr_info("UnCorrectable at PageAddr=%d\n", u4PageAddr); */
				continue;
			}
			for (i = 0; i < ((u4ErrNum + 1) >> 1); i++) {
				/* get error location */
				au4ErrBitLoc[i] = DRV_Reg32(ECC_DECEL0_REG32 + i);
				/* pr_debug("[XL1] errloc[%d] 0x%x\n", i, au4ErrBitLoc[i]); */
			}
			for (i = 0; i < u4ErrNum; i++) {
				/* MCU error correction */
				err_pos = ((au4ErrBitLoc[i >> 1] >> ((i & 0x01) << 4)) & 0x3FFF);
				/* *(data_buff + (err_pos >> 3)) ^= (1 << (err_pos & 0x7)); */
				u4ErrByteLoc = err_pos >> 3;
				if (u4ErrByteLoc < host->hw->nand_sec_size) {
					pDataBuf[host->hw->nand_sec_size * j + u4ErrByteLoc] ^=
						(1 << (err_pos & 0x7));
					continue;
				}
				/* BytePos is in FDM data and auto-format. */
				u4ErrByteLoc -= host->hw->nand_sec_size;
				if (u4ErrByteLoc < 8) {	/* fdm size */
					if (u4ErrByteLoc >= 4) {
						temp = DRV_Reg32(NFI_FDM0M_REG32 + (j << 1));
						u4ErrByteLoc -= 4;
						temp ^= (1 << ((err_pos & 0x7) + (u4ErrByteLoc << 3)));
						DRV_WriteReg32(NFI_FDM0M_REG32 + (j << 1), temp);
					} else {
						temp = DRV_Reg32(NFI_FDM0L_REG32 + (j << 1));
						temp ^= (1 << ((err_pos & 0x7) + (u4ErrByteLoc << 3)));
						DRV_WriteReg32(NFI_FDM0L_REG32 + (j << 1), temp);
					}
				}
			}
			mtd->ecc_stats.corrected++;
		}
	}
#endif
	return ret;
}

/******************************************************************************
 * mtk_nand_RFIFOValidSize
 *
 * DESCRIPTION:
 *	 Check the Read FIFO data bytes !
 *
 * PARAMETERS:
 *	 u16 u2Size
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_RFIFOValidSize(u16 u2Size)
{
	u32 timeout = 0xFFFF;

	while (FIFO_RD_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) < u2Size) {
		timeout--;
		if (timeout == 0)
			return false;
	}
	return true;
}

/******************************************************************************
 * mtk_nand_WFIFOValidSize
 *
 * DESCRIPTION:
 *	 Check the Write FIFO data bytes !
 *
 * PARAMETERS:
 *	 u16 u2Size
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_WFIFOValidSize(u16 u2Size)
{
	u32 timeout = 0xFFFF;

	while (FIFO_WR_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) > u2Size) {
		timeout--;
		if (timeout == 0)
			return false;
	}
	return true;
}

/******************************************************************************
 * mtk_nand_status_ready
 *
 * DESCRIPTION:
 *	 Indicate the NAND device is ready or not !
 *
 * PARAMETERS:
 *	 u32 u4Status
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_status_ready(u32 u4Status)
{
	u32 timeout = 0xFFFF;

	while ((DRV_Reg32(NFI_STA_REG32) & u4Status) != 0) {
		timeout--;
		if (timeout == 0)
			return false;
	}
	return true;
}

/******************************************************************************
 * mtk_nand_reset
 *
 * DESCRIPTION:
 *	 Reset the NAND device hardware component !
 *
 * PARAMETERS:
 *	 struct mtk_nand_host *host (Initial setting data)
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_reset(void)
{
	/* HW recommended reset flow */
	int timeout = 0xFFFF;

	if (DRV_Reg16(NFI_MASTERSTA_REG16) & 0xFFF) {	/* master is busy */
		mb();
		DRV_WriteReg32(NFI_CON_REG16, CON_FIFO_FLUSH | CON_NFI_RST);
		while (DRV_Reg16(NFI_MASTERSTA_REG16) & 0xFFF) {
			timeout--;
			if (!timeout)
				pr_notice("Wait for NFI_MASTERSTA timeout\n");
		}
	}
	/* issue reset operation */
	mb();
	DRV_WriteReg32(NFI_CON_REG16, CON_FIFO_FLUSH | CON_NFI_RST);

	return mtk_nand_status_ready(STA_NFI_FSM_MASK | STA_NAND_BUSY) && mtk_nand_RFIFOValidSize(0)
		&& mtk_nand_WFIFOValidSize(0);
}

/******************************************************************************
 * mtk_nand_set_mode
 *
 * DESCRIPTION:
 *	  Set the oepration mode !
 *
 * PARAMETERS:
 *	 u16 u2OpMode (read/write)
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_set_mode(u16 u2OpMode)
{
	u16 u2Mode = DRV_Reg16(NFI_CNFG_REG16);

	u2Mode &= ~CNFG_OP_MODE_MASK;
	u2Mode |= u2OpMode;
	DRV_WriteReg16(NFI_CNFG_REG16, u2Mode);
}

/******************************************************************************
 * mtk_nand_set_autoformat
 *
 * DESCRIPTION:
 *	  Enable/Disable hardware autoformat !
 *
 * PARAMETERS:
 *	 bool bEnable (Enable/Disable)
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_set_autoformat(bool bEnable)
{
	if (bEnable)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AUTO_FMT_EN);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AUTO_FMT_EN);
}

/******************************************************************************
 * mtk_nand_configure_fdm
 *
 * DESCRIPTION:
 *	 Configure the FDM data size !
 *
 * PARAMETERS:
 *	 u16 u2FDMSize
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_configure_fdm(u16 u2FDMSize)
{
	if (mtk_nfi_dev_comp->chip_ver == 1) {
		NFI_CLN_REG16(NFI_PAGEFMT_REG16, PAGEFMT_FDM_MASK | PAGEFMT_FDM_ECC_MASK);
		NFI_SET_REG16(NFI_PAGEFMT_REG16, u2FDMSize << PAGEFMT_FDM_SHIFT);
		NFI_SET_REG16(NFI_PAGEFMT_REG16, u2FDMSize << PAGEFMT_FDM_ECC_SHIFT);
	} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
		NFI_CLN_REG32(NFI_PAGEFMT_REG32, PAGEFMT_FDM_MASK | PAGEFMT_FDM_ECC_MASK);
		NFI_SET_REG32(NFI_PAGEFMT_REG32, u2FDMSize << PAGEFMT_FDM_SHIFT);
		NFI_SET_REG32(NFI_PAGEFMT_REG32, u2FDMSize << PAGEFMT_FDM_ECC_SHIFT);
	} else {
		pr_err("[mtk_nand_configure_fdm] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
			mtk_nfi_dev_comp->chip_ver);
	}
}

static bool mtk_nand_pio_ready(void)
{
	int count = 0;

	while (!(DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1)) {
		count++;
		if (count > 0xffff) {
			pr_info("PIO_DIRDY timeout\n");
			return false;
		}
	}

	return true;
}

/******************************************************************************
 * mtk_nand_set_command
 *
 * DESCRIPTION:
 *	  Send hardware commands to NAND devices !
 *
 * PARAMETERS:
 *	 u16 command
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_set_command(u16 command)
{
	/* Write command to device */
	mb();
	DRV_WriteReg16(NFI_CMD_REG16, command);
	return mtk_nand_status_ready(STA_CMD_STATE);
}

/******************************************************************************
 * mtk_nand_set_address
 *
 * DESCRIPTION:
 *	  Set the hardware address register !
 *
 * PARAMETERS:
 *	 struct nand_chip *nand, u32 u4RowAddr
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_set_address(u32 u4ColAddr, u32 u4RowAddr, u16 u2ColNOB, u16 u2RowNOB)
{
	/* fill cycle addr */
	mb();
	DRV_WriteReg32(NFI_COLADDR_REG32, u4ColAddr);
	DRV_WriteReg32(NFI_ROWADDR_REG32, u4RowAddr);
	DRV_WriteReg16(NFI_ADDRNOB_REG16, u2ColNOB | (u2RowNOB << ADDR_ROW_NOB_SHIFT));
	return mtk_nand_status_ready(STA_ADDR_STATE);
}

/* ------------------------------------------------------------------------------- */
static bool mtk_nand_device_reset(void)
{
	u32 timeout = 0xFFFF;

	mtk_nand_reset();

	DRV_WriteReg(NFI_CNFG_REG16, CNFG_OP_RESET);

	mtk_nand_set_command(NAND_CMD_RESET);

	while (!(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY_RETURN) && (timeout--))
		;

	if (!timeout)
		return false;
	else
		return true;
}

/* ------------------------------------------------------------------------------- */

/******************************************************************************
 * mtk_nand_check_RW_count
 *
 * DESCRIPTION:
 *	  Check the RW how many sectors !
 *
 * PARAMETERS:
 *	 u16 u2WriteSize
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_check_RW_count(u16 u2WriteSize)
{
	u32 timeout = 0xFFFF;
	u16 u2SecNum = u2WriteSize >> host->hw->nand_sec_shift;

	while (ADDRCNTR_CNTR(DRV_Reg32(NFI_ADDRCNTR_REG16)) < u2SecNum) {
		timeout--;
		if (timeout == 0) {
			pr_info("[%s] timeout\n", __func__);
			return false;
		}
	}
	return true;
}

/******************************************************************************
 * mtk_nand_ready_for_read
 *
 * DESCRIPTION:
 *	  Prepare hardware environment for read !
 *
 * PARAMETERS:
 *	 struct nand_chip *nand, u32 u4RowAddr
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_ready_for_read(struct nand_chip *nand, u32 u4RowAddr, u32 u4ColAddr,
					u16 sec_num, bool full, u8 *buf, enum readCommand cmd)
{
	/* Reset NFI HW internal state machine and flush NFI in/out FIFO */
	bool bRet = false;
	/* u16 sec_num = 1 << (nand->page_shift - host->hw->nand_sec_shift); */
	u32 col_addr = u4ColAddr;
	u32 colnob = 2, rownob = devinfo.addr_cycle - 2;

	/* u32 reg_val = DRV_Reg32(NFI_MASTERRST_REG32); */
#if __INTERNAL_USE_AHB_MODE__
	unsigned int phys = 0;
#endif

	if (full) {
		mtk_dir = DMA_FROM_DEVICE;
		sg_init_one(&mtk_sg, buf, (sec_num * (1 << host->hw->nand_sec_shift)));
		dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		phys = mtk_sg.dma_address;
		/* pr_debug("[xl] phys va 0x%x\n", phys); */
	}

	if (DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32) & 0x3) {
		NFI_SET_REG16(NFI_MASTERRST_REG32, PAD_MACRO_RST);	/* reset */
		NFI_CLN_REG16(NFI_MASTERRST_REG32, PAD_MACRO_RST);	/* dereset */
	}

	if (nand->options & NAND_BUSWIDTH_16)
		col_addr /= 2;

	if (!mtk_nand_reset())
		goto cleanup;
	if (g_bHwEcc) {
		/* Enable HW ECC */
		NFI_SET_REG32(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	} else {
		NFI_CLN_REG32(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	}

	mtk_nand_set_mode(CNFG_OP_READ);
	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN);
	DRV_WriteReg32(NFI_CON_REG16, sec_num << CON_NFI_SEC_SHIFT);
	if (full) {
#if __INTERNAL_USE_AHB_MODE__
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
		if (!phys) {
			pr_err("[mtk_nand_ready_for_read]convert virt addr (%lx) to phys add (%x)fail!!!",
					(unsigned long)buf, phys);
			return false;
		}
		DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
#endif

		if (g_bHwEcc)
			NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		else
			NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	} else {
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
	}

	mtk_nand_set_autoformat(full);
	if (full) {
		if (g_bHwEcc)
			ECC_Decode_Start();
	}
	if (cmd == AD_CACHE_FINAL) {
		if (!mtk_nand_set_command(0x3F))
			goto cleanup;
		if (!mtk_nand_status_ready(STA_NAND_BUSY))
			goto cleanup;
		return true;
	}
	if (!mtk_nand_set_command(NAND_CMD_READ0))
		goto cleanup;
	if (!mtk_nand_set_address(col_addr, u4RowAddr, colnob, rownob))
		goto cleanup;
	if (cmd == NORMAL_READ) {
		if (!mtk_nand_set_command(NAND_CMD_READSTART))
			goto cleanup;
	} else {
		if (!mtk_nand_set_command(0x31))
			goto cleanup;
	}

	if (!mtk_nand_status_ready(STA_NAND_BUSY))
		goto cleanup;

	bRet = true;

cleanup:
	return bRet;
}

/******************************************************************************
 * mtk_nand_ready_for_write
 *
 * DESCRIPTION:
 *	  Prepare hardware environment for write !
 *
 * PARAMETERS:
 *	 struct nand_chip *nand, u32 u4RowAddr
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_ready_for_write(struct nand_chip *nand, u32 u4RowAddr, u32 col_addr, bool full,
						u8 *buf)
{
	bool bRet = false;
	u32 sec_num = 1 << (nand->page_shift - host->hw->nand_sec_shift);
	u32 colnob = 2, rownob = devinfo.addr_cycle - 2;
	u16 prg_cmd;
#if __INTERNAL_USE_AHB_MODE__
	unsigned int phys = 0;
	/* u32 T_phys = 0; */
#endif
	u32 temp_sec_num;

	temp_sec_num = sec_num;

	if (full) {
		mtk_dir = DMA_TO_DEVICE;
		sg_init_one(&mtk_sg, buf, (1 << nand->page_shift));
		dma_map_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		phys = mtk_sg.dma_address;
		/* pr_debug("[xl] phys va 0x%x\n", phys); */
	}

	if (nand->options & NAND_BUSWIDTH_16)
		col_addr /= 2;

	/* Reset NFI HW internal state machine and flush NFI in/out FIFO */
	if (!mtk_nand_reset()) {
		pr_err("[Bean]mtk_nand_ready_for_write (mtk_nand_reset) fail!\n");
		return false;
	}

	mtk_nand_set_mode(CNFG_OP_PRGM);

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_READ_EN);

	DRV_WriteReg32(NFI_CON_REG16, temp_sec_num << CON_NFI_SEC_SHIFT);

	if (full) {
#if __INTERNAL_USE_AHB_MODE__
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
		/* T_phys = __virt_to_phys(buf); */
		if (!phys) {
			pr_err("[mt65xx_nand_ready_for_write]convert virt addr (%lx) to phys add fail!!!",
					(unsigned long)buf);
			return false;
		}
		DRV_WriteReg32(NFI_STRADDR_REG32, phys);
#else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
#endif
		if (g_bHwEcc)
			NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		else
			NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
	} else {
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
	}

	mtk_nand_set_autoformat(full);

	if (full) {
		if (g_bHwEcc)
			ECC_Encode_Start();
	}

	prg_cmd = NAND_CMD_SEQIN;
	if (!mtk_nand_set_command(prg_cmd)) {
		pr_err("[Bean]mtk_nand_ready_for_write (mtk_nand_set_command) fail!\n");
		goto cleanup;
	}
	/* 1 FIXED ME: For Any Kind of AddrCycle */
	if (!mtk_nand_set_address(col_addr, u4RowAddr, colnob, rownob)) {
		pr_err("[Bean]mtk_nand_ready_for_write (mtk_nand_set_address) fail!\n");
		goto cleanup;
	}

	if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
		pr_err("[Bean]mtk_nand_ready_for_write (mtk_nand_status_ready) fail!\n");
		goto cleanup;
	}

	bRet = true;
cleanup:

	return bRet;
}

static bool mtk_nand_check_dececc_done(u32 u4SecNum)
{
	u32 dec_mask;
	u32 fsm_mask;
	u32 ECC_DECFSM_IDLE;
	u32 timeout_us = 1000000;

	dec_mask = (1 << (u4SecNum - 1));
	while (dec_mask != (DRV_Reg(ECC_DECDONE_REG16) & dec_mask)) {
		if (!timeout_us) {
			pr_notice("ECC_DECDONE: timeout1 0x%x %d\n", DRV_Reg(ECC_DECDONE_REG16),
				u4SecNum);
			dump_nfi();
			return false;
		}

		timeout_us--;
		udelay(1);
	}

	if (mtk_nfi_dev_comp->chip_ver == 1) {
		fsm_mask = 0x7F0F0F0F;
		ECC_DECFSM_IDLE = ECC_DECFSM_IDLE_V1;
	} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
		fsm_mask = 0x3F3FFF0F;
		ECC_DECFSM_IDLE = ECC_DECFSM_IDLE_V2;
	} else {
		fsm_mask = 0xFFFFFFFF;
		ECC_DECFSM_IDLE = 0xFFFFFFFF;
		pr_err("[mtk_nand_check_dececc_done] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
			mtk_nfi_dev_comp->chip_ver);
	}

	while ((DRV_Reg32(ECC_DECFSM_REG32) & fsm_mask) != ECC_DECFSM_IDLE) {
		if (!timeout_us) {
			pr_notice("ECC_DECDONE: timeout2 0x%x 0x%x %d\n",
				DRV_Reg32(ECC_DECFSM_REG32), DRV_Reg(ECC_DECDONE_REG16), u4SecNum);
			dump_nfi();
			return false;
		}

		timeout_us--;
		udelay(1);
	}
	return true;
}

/******************************************************************************
 * mtk_nand_read_page_data
 *
 * DESCRIPTION:
 *	 Fill the page data into buffer !
 *
 * PARAMETERS:
 *	 u8* pDataBuf, u32 u4Size
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_dma_read_data(struct mtd_info *mtd, u8 *buf, u32 length)
{
	/* Disable IRQ for DMA read data */
	int interrupt_en = 0; /*g_i4Interrupt; */
	int timeout = 0xfffff;
	/* struct scatterlist sg; */
	/* enum dma_data_direction dir = DMA_FROM_DEVICE; */
	/* pr_debug("[xl] dma read buf in 0x%lx\n", (unsigned long)buf); */
	/* sg_init_one(&sg, buf, length); */
	/* pr_debug("[xl] dma read buf out 0x%lx\n", (unsigned long)buf); */
	/* dma_map_sg(&(mtd->dev), &sg, 1, dir); */

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	/* DRV_WriteReg32(NFI_STRADDR_REG32, __virt_to_phys(pDataBuf)); */

	if ((unsigned long)buf % 16) {	/* TODO: can not use AHB mode here */
		pr_debug("Un-16-aligned address\n");
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	} else {
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	}

	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);

	DRV_Reg16(NFI_INTR_REG16);
	if (interrupt_en)
		DRV_WriteReg16(NFI_INTR_EN_REG16, INTR_AHB_DONE_EN);

	if (interrupt_en)
		init_completion(&g_comp_AHB_Done);
	/* dmac_inv_range(pDataBuf, pDataBuf + u4Size); */
	mb();
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BRD);

	if (interrupt_en) {
		/* Wait 10ms for AHB done */
		if (!wait_for_completion_timeout(&g_comp_AHB_Done, msecs_to_jiffies(NFI_TIMEOUT_MS))) {
			pr_notice("wait for completion timeout happened @ [%s]: %d\n", __func__,
				__LINE__);
			dump_nfi();
			return false;
		}
		while ((length >> host->hw->nand_sec_shift) >
				((DRV_Reg32(NFI_BYTELEN_REG16) & 0x1f000) >> 12)) {
			timeout--;
			if (timeout == 0) {
				pr_err("[%s] poll BYTELEN error\n", __func__);
				return false;	/* 4  // AHB Mode Time Out! */
			}
		}
	} else {
		/* Remove AHB Done check for IRQ driver */
		while ((length >> host->hw->nand_sec_shift) >
				((DRV_Reg32(NFI_BYTELEN_REG16) & 0x1f000) >> 12)) {
			timeout--;
			if (timeout == 0) {
				pr_err("[%s] poll BYTELEN error\n", __func__);
				dump_nfi();
				return false;	/* 4  // AHB Mode Time Out! */
			}
		}
	}

	/* dma_unmap_sg(&(mtd->dev), &sg, 1, dir); */
	return true;
}

static bool mtk_nand_mcu_read_data(struct mtd_info *mtd, u8 *buf, u32 length)
{
	int timeout = 0xffff;
	u32 i;
	u32 *buf32 = (u32 *) buf;
#ifdef TESTTIME
	unsigned long long time1, time2;

	time1 = sched_clock();
#endif
	if ((unsigned long)buf % 4 || length % 4)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);

	/* DRV_WriteReg32(NFI_STRADDR_REG32, 0); */
	mb();
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BRD);

	if ((unsigned long)buf % 4 || length % 4) {
		for (i = 0; (i < (length)) && (timeout > 0);) {
			/* if (FIFO_RD_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) >= 4) */
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				*buf++ = (u8) DRV_Reg32(NFI_DATAR_REG32);
				i++;
			} else {
				timeout--;
			}
			if (timeout == 0) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	} else {
		for (i = 0; (i < (length >> 2)) && (timeout > 0);) {
			/* if (FIFO_RD_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) >= 4) */
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				*buf32++ = DRV_Reg32(NFI_DATAR_REG32);
				i++;
			} else {
				timeout--;
			}
			if (timeout == 0) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	}
#ifdef TESTTIME
	time2 = sched_clock() - time1;
	if (!readdatatime)
		readdatatime = (time2);
#endif
	return true;
}

static bool mtk_nand_read_page_data(struct mtd_info *mtd, u8 *pDataBuf, u32 u4Size)
{
#if (__INTERNAL_USE_AHB_MODE__)
	return mtk_nand_dma_read_data(mtd, pDataBuf, u4Size);
#else
	return mtk_nand_mcu_read_data(mtd, pDataBuf, u4Size);
#endif
}

/******************************************************************************
 * mtk_nand_write_page_data
 *
 * DESCRIPTION:
 *	 Fill the page data into buffer !
 *
 * PARAMETERS:
 *	 u8* pDataBuf, u32 u4Size
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static bool mtk_nand_dma_write_data(struct mtd_info *mtd, u8 *pDataBuf, u32 u4Size)
{
	/* Disable IRQ for DMA write data */
	int i4Interrupt = 0;	/* g_i4Interrupt; */
	u32 timeout = 0xFFFF;
	/* struct scatterlist sg; */
	/* enum dma_data_direction dir = DMA_TO_DEVICE; */
	/* pr_debug("[xl] dma write buf in 0x%lx\n", (unsigned long)pDataBuf); */
	/* sg_init_one(&sg, pDataBuf, u4Size); */
	/* pr_debug("[xl] dma write buf out 0x%lx\n", (unsigned long)pDataBuf); */
	/* dma_map_sg(&(mtd->dev), &sg, 1, dir); */
	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	DRV_Reg16(NFI_INTR_REG16);
	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);
	/* DRV_WriteReg32(NFI_STRADDR_REG32, (u32*)virt_to_phys(pDataBuf)); */

	if ((unsigned long)pDataBuf % 16) {	/* TODO: can not use AHB mode here */
		pr_debug("Un-16-aligned address\n");
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	} else {
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
	}

	NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);

	if (i4Interrupt) {
		init_completion(&g_comp_AHB_Done);
		DRV_Reg16(NFI_INTR_REG16);
		DRV_WriteReg16(NFI_INTR_EN_REG16, INTR_AHB_DONE_EN);
	}

	/* dmac_clean_range(pDataBuf, pDataBuf + u4Size); */
	mb();
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	if (i4Interrupt) {
		/* Wait 10ms for AHB done */
		if (!wait_for_completion_timeout(&g_comp_AHB_Done, msecs_to_jiffies(NFI_TIMEOUT_MS))) {
			pr_notice("wait for completion timeout happened @ [%s]: %d\n", __func__,
				__LINE__);
			dump_nfi();
			return false;
		}
		/* wait_for_completion(&g_comp_AHB_Done); */
	} else {
		while ((u4Size >> host->hw->nand_sec_shift) >
				((DRV_Reg32(NFI_BYTELEN_REG16) & 0x1f000) >> 12)) {
			timeout--;
			if (timeout == 0) {
				pr_err("[%s] poll BYTELEN error\n", __func__);
				return false;	/* 4  // AHB Mode Time Out! */
			}
		}
	}

	/* dma_unmap_sg(&(mtd->dev), &sg, 1, dir); */
	return true;
}

static bool mtk_nand_mcu_write_data(struct mtd_info *mtd, const u8 *buf, u32 length)
{
	u32 timeout = 0xFFFF;
	u32 i;
	u32 *pBuf32;

	NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	mb();
	NFI_SET_REG32(NFI_CON_REG16, CON_NFI_BWR);
	pBuf32 = (u32 *) buf;

	if ((unsigned long)buf % 4 || length % 4)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
	else
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);

	if ((unsigned long)buf % 4 || length % 4) {
		for (i = 0; (i < (length)) && (timeout > 0);) {
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				DRV_WriteReg32(NFI_DATAW_REG32, *buf++);
				i++;
			} else {
				timeout--;
			}
			if (timeout == 0) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	} else {
		for (i = 0; (i < (length >> 2)) && (timeout > 0);) {
			/* if (FIFO_WR_REMAIN(DRV_Reg16(NFI_FIFOSTA_REG16)) <= 12) */
			if (DRV_Reg16(NFI_PIO_DIRDY_REG16) & 1) {
				DRV_WriteReg32(NFI_DATAW_REG32, *pBuf32++);
				i++;
			} else {
				timeout--;
			}
			if (timeout == 0) {
				pr_err("[%s] timeout\n", __func__);
				dump_nfi();
				return false;
			}
		}
	}

	return true;
}

static bool mtk_nand_write_page_data(struct mtd_info *mtd, u8 *buf, u32 size)
{
#if (__INTERNAL_USE_AHB_MODE__)
	return mtk_nand_dma_write_data(mtd, buf, size);
#else
	return mtk_nand_mcu_write_data(mtd, buf, size);
#endif
}

/******************************************************************************
 * mtk_nand_read_fdm_data
 *
 * DESCRIPTION:
 *	 Read a fdm data !
 *
 * PARAMETERS:
 *	 u8* pDataBuf, u32 u4SecNum
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_read_fdm_data(u8 *pDataBuf, u32 u4SecNum)
{
	u32 i;
	u32 *pBuf32 = (u32 *) pDataBuf;

	if (pBuf32) {
		for (i = 0; i < u4SecNum; ++i) {
			*pBuf32++ = DRV_Reg32(NFI_FDM0L_REG32 + (i << 1));
			*pBuf32++ = DRV_Reg32(NFI_FDM0M_REG32 + (i << 1));
		}
	}
}

/******************************************************************************
 * mtk_nand_write_fdm_data
 *
 * DESCRIPTION:
 *	 Write a fdm data !
 *
 * PARAMETERS:
 *	 u8* pDataBuf, u32 u4SecNum
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static u8 fdm_buf[128];
static void mtk_nand_write_fdm_data(struct nand_chip *chip, u8 *pDataBuf, u32 u4SecNum)
{
	u32 i, j;
	u8 checksum = 0;
	bool empty = true;
	struct nand_oobfree *free_entry;
	u8 *pBuf;
	u8 *byte_ptr;
	u32 fdm_data[2];

	memcpy(fdm_buf, pDataBuf, u4SecNum * host->hw->nand_fdm_size);

	free_entry = chip->ecc.layout->oobfree;
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES && free_entry[i].length; i++) {
		for (j = 0; j < free_entry[i].length; j++) {
			if (pDataBuf[free_entry[i].offset + j] != 0xFF)
				empty = false;
			checksum ^= pDataBuf[free_entry[i].offset + j];
		}
	}

	if (!empty)
		fdm_buf[free_entry[i - 1].offset + free_entry[i - 1].length] = checksum;

	pBuf = (u8 *)fdm_data;
	byte_ptr = (u8 *)fdm_buf;

	for (i = 0; i < u4SecNum; ++i) {
		fdm_data[0] = 0xFFFFFFFF;
		fdm_data[1] = 0xFFFFFFFF;

		for (j = 0; j < host->hw->nand_fdm_size; j++)
			*(pBuf + j) = *(byte_ptr + j + (i * host->hw->nand_fdm_size));

		DRV_WriteReg32(NFI_FDM0L_REG32 + (i << 1), fdm_data[0]);
		DRV_WriteReg32(NFI_FDM0M_REG32 + (i << 1), fdm_data[1]);
	}
}

/******************************************************************************
 * mtk_nand_stop_read
 *
 * DESCRIPTION:
 *	 Stop read operation !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_stop_read(void)
{
	NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BRD);
	mtk_nand_reset();
	if (g_bHwEcc)
		ECC_Decode_End();
	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);
}

/******************************************************************************
 * mtk_nand_stop_write
 *
 * DESCRIPTION:
 *	 Stop write operation !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_stop_write(void)
{
	NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BWR);
	if (g_bHwEcc)
		ECC_Encode_End();
	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);
}

/******************************************************************************
 * mtk_nand_exec_read_page
 *
 * DESCRIPTION:
 *	 Read a page data !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize,
 *	 u8* pPageBuf, u8* pFDMBuf
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
int mtk_nand_exec_read_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf,
				u8 *pFDMBuf)
{
	u8 *buf;
	int bRet = ERR_RTN_SUCCESS;
	struct nand_chip *nand = mtd->priv;
	u32 u4SecNum = u4PageSize >> host->hw->nand_sec_shift;
	u32 tempBitMap;
	u32 real_row_addr = 0;
	u32 logical_plane_num = 1;
	u32 data_sector_num = 0;
	u8 *temp_byte_ptr = NULL;
	u8 *spare_ptr = NULL;
	u32 page_per_block = 0;

	/* pr_err("mtk_nand_exec_read_page, u4RowAddr: 0x%x\n", u4RowAddr); */
	tempBitMap = 0;

	page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;

	if (((unsigned long)pPageBuf % 16) && local_buffer_16_align) {
		buf = local_buffer_16_align;
	} else {
		if (virt_addr_valid(pPageBuf) == 0) {	/* It should be allocated by vmalloc */
			buf = local_buffer_16_align;
		} else {
			buf = pPageBuf;
		}
	}

	data_sector_num = u4SecNum;
	temp_byte_ptr = buf;
	spare_ptr = pFDMBuf;
	logical_plane_num = 1;

	real_row_addr = u4RowAddr;

	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_on_randomizer(u4RowAddr, 0, 0);
	else if (pre_randomizer && u4RowAddr < RAND_START_ADDR)
		mtk_nand_turn_on_randomizer(u4RowAddr, 0, 0);

	if (mtk_nand_ready_for_read(nand, real_row_addr, 0, data_sector_num, true, buf, NORMAL_READ)) {
		while (logical_plane_num) {
			if (!mtk_nand_read_page_data(mtd, temp_byte_ptr,
				data_sector_num * (1 << host->hw->nand_sec_shift))) {
				pr_err("mtk_nand_read_page_data fail\n");
				bRet = ERR_RTN_FAIL;
			}
			dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
				pr_err("mtk_nand_status_ready fail\n");
				bRet = ERR_RTN_FAIL;
			}
			if (g_bHwEcc) {
				if (!mtk_nand_check_dececc_done(data_sector_num)) {
					pr_err("mtk_nand_check_dececc_done fail 0x%x\n", u4RowAddr);
					bRet = ERR_RTN_FAIL;
				}
			}
			mtk_nand_read_fdm_data(spare_ptr, data_sector_num);
			if (g_bHwEcc) {
				if (!mtk_nand_check_bch_error
					(mtd, temp_byte_ptr, spare_ptr, data_sector_num - 1, u4RowAddr, &tempBitMap)) {
					bRet = ERR_RTN_BCH_FAIL;
				}
			}
			mtk_nand_stop_read();

			logical_plane_num--;

			if (bRet == ERR_RTN_BCH_FAIL)
				break;
		}
	} else
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);

	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_off_randomizer();
	else if (pre_randomizer && u4RowAddr < RAND_START_ADDR)
		mtk_nand_turn_off_randomizer();

	if (buf == local_buffer_16_align)
		memcpy(pPageBuf, buf, u4PageSize);

	if (bRet != ERR_RTN_SUCCESS) {
		pr_err("[%s] page 0x%x ECC uncorrectable, fake buffer returned\n",
				__func__, real_row_addr);
		memset(pPageBuf, 0xff, u4PageSize);
		memset(pFDMBuf, 0xff, u4SecNum * host->hw->nand_fdm_size);
	}

	return bRet;
}

bool mtk_nand_exec_read_sector(struct mtd_info *mtd, u32 u4RowAddr, u32 u4ColAddr, u32 u4PageSize,
					u8 *pPageBuf, u8 *pFDMBuf, int subpageno)
{
	u8 *buf;
	int bRet = ERR_RTN_SUCCESS;
	struct nand_chip *nand = mtd->priv;
	u32 u4SecNum = subpageno;
	u32 real_row_addr = 0;
	u32 logical_plane_num = 1;
	u32 temp_col_addr[2] = {0, 0};
	u32 data_sector_num[2] = {0, 0};
	u8 *temp_byte_ptr = NULL;
	u8 *spare_ptr = NULL;
	u32 page_per_block = 0;

	page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;

	if (((unsigned long)pPageBuf % 16) && local_buffer_16_align) {
		buf = local_buffer_16_align;
	} else {
		if (virt_addr_valid(pPageBuf) == 0) {	/* It should be allocated by vmalloc */
			buf = local_buffer_16_align;
		} else {
			buf = pPageBuf;
		}
	}
	temp_byte_ptr = buf;
	spare_ptr = pFDMBuf;
	temp_col_addr[0] = u4ColAddr;
	temp_col_addr[1] = 0;
	data_sector_num[0] = u4SecNum;
	data_sector_num[1] = 0;
	logical_plane_num = 1;

	real_row_addr = u4RowAddr;

	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_on_randomizer(u4RowAddr, 0, 0);
	else if (pre_randomizer && u4RowAddr < RAND_START_ADDR)
		mtk_nand_turn_on_randomizer(u4RowAddr, 0, 0);

	if (mtk_nand_ready_for_read
		(nand, real_row_addr, temp_col_addr[logical_plane_num-1],
		data_sector_num[logical_plane_num - 1], true, buf, NORMAL_READ)) {
		while (logical_plane_num) {

			if (!mtk_nand_read_page_data
				(mtd, temp_byte_ptr, data_sector_num[logical_plane_num - 1]
					* (1 << host->hw->nand_sec_shift))) {
				pr_err("mtk_nand_read_page_data fail\n");
				bRet = ERR_RTN_FAIL;
			}
			dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
			if (!mtk_nand_status_ready(STA_NAND_BUSY)) {
				pr_err("mtk_nand_status_ready fail\n");
				bRet = ERR_RTN_FAIL;
			}

			if (g_bHwEcc) {
				if (!mtk_nand_check_dececc_done(data_sector_num[logical_plane_num - 1])) {
					pr_err("mtk_nand_check_dececc_done fail 0x%x\n", u4RowAddr);
					bRet = ERR_RTN_FAIL;
				}
			}
			mtk_nand_read_fdm_data(spare_ptr, data_sector_num[logical_plane_num - 1]);
			if (g_bHwEcc) {
				if (!mtk_nand_check_bch_error
					(mtd, temp_byte_ptr, spare_ptr, data_sector_num[logical_plane_num - 1] - 1,
					u4RowAddr, NULL)) {
					bRet = ERR_RTN_BCH_FAIL;
				}
			}
			mtk_nand_stop_read();

			logical_plane_num--;

			if (bRet == ERR_RTN_BCH_FAIL)
				break;
		}
	} else
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);

	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_off_randomizer();
	else if (pre_randomizer && u4RowAddr < RAND_START_ADDR)
		mtk_nand_turn_off_randomizer();

	if (buf == local_buffer_16_align)
		memcpy(pPageBuf, buf, u4PageSize);

	if (bRet != ERR_RTN_SUCCESS) {
		pr_err("[%s] page 0x%x ECC uncorrectable, fake buffer returned\n",
				__func__, real_row_addr);
		memset(pPageBuf, 0xff, u4PageSize);
		memset(pFDMBuf, 0xff, u4SecNum * host->hw->nand_fdm_size);
	}

	return bRet;
}

/******************************************************************************
 * mtk_nand_exec_write_page
 *
 * DESCRIPTION:
 *	 Write a page data !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize,
 *	 u8* pPageBuf, u8* pFDMBuf
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
int mtk_nand_exec_write_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize, u8 *pPageBuf,
					u8 *pFDMBuf)
{
	struct nand_chip *chip = mtd->priv;
	u32 u4SecNum = u4PageSize >> host->hw->nand_sec_shift;
	u8 *buf;
	u8 status;
	u32 real_row_addr = 0;
	u32 page_per_block = 0;

	page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;

	if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
		mtk_nand_turn_on_randomizer(u4RowAddr, 1, 0);
	else if (pre_randomizer && u4RowAddr < RAND_START_ADDR)
		mtk_nand_turn_on_randomizer(u4RowAddr, 1, 0);
#ifdef _MTK_NAND_DUMMY_DRIVER_
	if (dummy_driver_debug) {
		unsigned long long time = sched_clock();

		if (!((time * 123 + 59) % 32768)) {
			pr_err("[NAND_DUMMY_DRIVER] Simulate write error at page: 0x%x\n",
					u4RowAddr);
			return -EIO;
		}
	}
#endif

	if (((unsigned long)pPageBuf % 16) && local_buffer_16_align) {
		pr_debug("Data buffer not 16 bytes aligned: %p\n", pPageBuf);
		memcpy(local_buffer_16_align, pPageBuf, u4PageSize);
		buf = local_buffer_16_align;
	} else {
		if (virt_addr_valid(pPageBuf) == 0) {	/* It should be allocated by vmalloc */
			memcpy(local_buffer_16_align, pPageBuf, u4PageSize);
			buf = local_buffer_16_align;
		} else {
			buf = pPageBuf;
		}
	}

	real_row_addr = u4RowAddr;

	if (mtk_nand_ready_for_write(chip, real_row_addr, 0, true, buf)) {
		mtk_nand_write_fdm_data(chip, pFDMBuf, u4SecNum);
		(void)mtk_nand_write_page_data(mtd, buf, u4PageSize);
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		(void)mtk_nand_check_RW_count(u4PageSize);
		/* mtk_nand_stop_write(); */
		if (g_i4Interrupt) {
			DRV_Reg16(NFI_INTR_REG16);
			NFI_SET_REG16(NFI_INTR_EN_REG16, INTR_WR_DONE_EN);
		}
		mtk_nand_set_command(NAND_CMD_PAGEPROG);
		if (!g_i4Interrupt) {
			/*
			 * if this is the first plane page program, then busy is tDCBSYW1
			 * about 0.5us. will not receive WR_DONE IRQ.
			 */
			while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY);
		} else {
			/* Wait for WR done */
			if (!wait_for_completion_timeout(&g_comp_WR_Done, msecs_to_jiffies(NFI_TIMEOUT_MS))) {
				pr_err("wait for completion timeout happened @ [%s]: %d\n", __func__,
					__LINE__);
				dump_nfi();
				mtk_nand_reset();
				return -EIO;
			}
		}
		mtk_nand_stop_write();

		if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
			mtk_nand_turn_off_randomizer();
		else if (pre_randomizer && u4RowAddr < RAND_START_ADDR)
			mtk_nand_turn_off_randomizer();

		status = chip->waitfunc(mtd, chip);

		if (status & NAND_STATUS_FAIL) {
			pr_err("write fail!! status 0x%x\n", status);
			return -EIO;
		} else
			return 0;
	} else {
		dma_unmap_sg(mtk_dev, &mtk_sg, 1, mtk_dir);
		pr_warn("[Bean]mtk_nand_ready_for_write fail!\n");
		if (use_randomizer && u4RowAddr >= RAND_START_ADDR)
			mtk_nand_turn_off_randomizer();
		return -EIO;
	}
}

/******************************************************************************
 *
 * Write a page to a logical address
 *
 *****************************************************************************/
static int mtk_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
					uint32_t offset, int data_len, const uint8_t *buf,
					int oob_required, int page, int cached, int raw)
{
	/* int block_size = 1 << (chip->phys_erase_shift); */
	int page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	u32 row_addr;
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	/* write bad index into oob */
	if (mapped_block != block)
		set_bad_index_to_oob(chip->oob_poi, block);
	else
		set_bad_index_to_oob(chip->oob_poi, FAKE_INDEX);
	/* pr_debug("[xiaolei] mtk_nand_write_page 0x%x\n", (u32)buf); */
	row_addr = page_in_block + mapped_block * page_per_block;
	if (mtk_nand_exec_write_page(mtd, row_addr, mtd->writesize, (u8 *) buf, chip->oob_poi)) {
		pr_err("write fail at block: 0x%x, page: 0x%x\n", mapped_block, page_in_block);
		if (update_bmt((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
				chip->page_shift, UPDATE_WRITE_FAIL, (u8 *) buf, chip->oob_poi)) {
			pr_debug("Update BMT success\n");
			return 0;
		}
		pr_err("Update BMT fail\n");
		return -EIO;
	}
	return 0;
}

/******************************************************************************
 * mtk_nand_command_bp
 *
 * DESCRIPTION:
 *	 Handle the commands from MTD !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, unsigned int command, int column, int page_addr
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_command_bp(struct mtd_info *mtd, unsigned int command, int column,
				int page_addr)
{
	struct nand_chip *nand = mtd->priv;
	switch (command) {
	case NAND_CMD_SEQIN:
		memset(g_kCMD.au1OOB, 0xFF, sizeof(g_kCMD.au1OOB));
		g_kCMD.pDataBuf = NULL;
		/* } */
		g_kCMD.u4RowAddr = page_addr;
		g_kCMD.u4ColAddr = column;
		break;

	case NAND_CMD_PAGEPROG:
		if (g_kCMD.pDataBuf || (g_kCMD.au1OOB[0] != 0xFF)) {
			u8 *pDataBuf = g_kCMD.pDataBuf ? g_kCMD.pDataBuf : nand->buffers->databuf;
			u32 row_addr = g_kCMD.u4RowAddr;

			mtk_nand_exec_write_page(mtd, row_addr, mtd->writesize, pDataBuf,
							g_kCMD.au1OOB);
			g_kCMD.u4RowAddr = (u32) -1;
			g_kCMD.u4OOBRowAddr = (u32) -1;
		}
		break;

	case NAND_CMD_READOOB:
		g_kCMD.u4RowAddr = page_addr;
		g_kCMD.u4ColAddr = column + mtd->writesize;
		break;

	case NAND_CMD_READ0:
		g_kCMD.u4RowAddr = page_addr;
		g_kCMD.u4ColAddr = column;
		break;

	case NAND_CMD_ERASE1:
		(void)mtk_nand_reset();
		mtk_nand_set_mode(CNFG_OP_ERASE);
		if (g_i4Interrupt) {
			DRV_Reg16(NFI_INTR_REG16);
			NFI_SET_REG16(NFI_INTR_EN_REG16, INTR_ERASE_DONE_EN);
		}
		(void)mtk_nand_set_command(NAND_CMD_ERASE1);
		(void)mtk_nand_set_address(0, page_addr, 0, devinfo.addr_cycle - 2);
		break;

	case NAND_CMD_ERASE2:
		(void)mtk_nand_set_command(NAND_CMD_ERASE2);
		if (g_i4Interrupt) {
			if (!wait_for_completion_timeout(&g_comp_ER_Done, msecs_to_jiffies(NFI_TIMEOUT_MS))) {
				pr_notice("wait for completion timeout happened @ [%s]: %d\n", __func__,
					__LINE__);
				dump_nfi();
			}
		} else {
			while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY)
				;
		}
		break;

	case NAND_CMD_STATUS:
		(void)mtk_nand_reset();
		if (mtk_nand_israndomizeron()) {
			/* g_brandstatus = true; */
			mtk_nand_turn_off_randomizer();
		}
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_BYTE_RW);
		mtk_nand_set_mode(CNFG_OP_SRD);
		mtk_nand_set_mode(CNFG_READ_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		(void)mtk_nand_set_command(NAND_CMD_STATUS);
		NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_NOB_MASK);
		mb();
		DRV_WriteReg32(NFI_CON_REG16, CON_NFI_SRD | (1 << CON_NFI_NOB_SHIFT));
		g_bcmdstatus = true;
		break;

	case NAND_CMD_RESET:
		mtk_nand_device_reset();
		break;

	case NAND_CMD_READID:
		/* Issue NAND chip reset command */
		/* NFI_ISSUE_COMMAND (NAND_CMD_RESET, 0, 0, 0, 0); */

		/* timeout = TIMEOUT_4; */

		/* while (timeout) */
		/* timeout--; */

		mtk_nand_reset();
		/* Disable HW ECC */
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);

		/* Disable 16-bit I/O */
		/* NFI_CLN_REG16(NFI_PAGEFMT_REG16, PAGEFMT_DBYTE_EN); */

		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN | CNFG_BYTE_RW);
		(void)mtk_nand_reset();
		mb();
		mtk_nand_set_mode(CNFG_OP_SRD);
		(void)mtk_nand_set_command(NAND_CMD_READID);
		(void)mtk_nand_set_address(column, 0, 1, 0);
		DRV_WriteReg32(NFI_CON_REG16, CON_NFI_SRD);
		while (DRV_Reg32(NFI_STA_REG32) & STA_DATAR_STATE)
			;
		break;

	default:
		break;
	}
}

/******************************************************************************
 * mtk_nand_select_chip
 *
 * DESCRIPTION:
 *	 Select a chip !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, int chip
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_select_chip(struct mtd_info *mtd, int chip)
{
	if (chip == -1 && false == g_bInitDone) {
		struct nand_chip *nand = mtd->priv;

		struct mtk_nand_host *host = nand->priv;
		struct mtk_nand_host_hw *hw = host->hw;
		u32 spare_per_sector = mtd->oobsize / (mtd->writesize / hw->nand_sec_size);
		u32 ecc_bit = 4;
		u32 spare_bit = PAGEFMT_SPARE_16;
		u32 pagesize = mtd->writesize;

		hw->nand_fdm_size = 8;

		switch (spare_per_sector) {
#if !defined(MTK_COMBO_NAND_SUPPORT) || defined(CONFIG_MTK_SLC_NAND_SUPPORT)
		case 16:
			spare_bit = PAGEFMT_SPARE_16;
			ecc_bit = 4;
			spare_per_sector = 16;
			break;
		case 26:
		case 27:
		case 28:
			spare_bit = PAGEFMT_SPARE_26;
			ecc_bit = 10;
			spare_per_sector = 26;
			break;
		case 32:
			ecc_bit = 12;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_32_1KS;
			else
				spare_bit = PAGEFMT_SPARE_32;
			spare_per_sector = 32;
			break;
		case 40:
			ecc_bit = 18;
			spare_bit = PAGEFMT_SPARE_40;
			spare_per_sector = 40;
			break;
		case 44:
			ecc_bit = 20;
			spare_bit = PAGEFMT_SPARE_44;
			spare_per_sector = 44;
			break;
		case 48:
		case 49:
			ecc_bit = 22;
			spare_bit = PAGEFMT_SPARE_48;
			spare_per_sector = 48;
			break;
		case 50:
		case 51:
			ecc_bit = 24;
			spare_bit = PAGEFMT_SPARE_50;
			spare_per_sector = 50;
			break;
		case 52:
		case 54:
		case 56:
			ecc_bit = 24;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_52_1KS;
			else
				spare_bit = PAGEFMT_SPARE_52;
			/* spare_per_sector = 32; */
			break;
#endif
		case 62:
		case 63:
			ecc_bit = 28;
			spare_bit = PAGEFMT_SPARE_62;
			spare_per_sector = 62;
			break;
		case 64:
			ecc_bit = 32;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_64_1KS;
			else
				spare_bit = PAGEFMT_SPARE_64;
			spare_per_sector = 64;
			break;
		case 72:
			ecc_bit = 36;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_72_1KS;
			spare_per_sector = 72;
			break;
		case 80:
			ecc_bit = 40;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_80_1KS;
			spare_per_sector = 80;
			break;
		case 88:
			ecc_bit = 44;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_88_1KS;
			spare_per_sector = 88;
			break;
		case 96:
		case 98:
			ecc_bit = 48;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_96_1KS;
			spare_per_sector = 96;
			break;
		case 100:
		case 102:
		case 104:
			ecc_bit = 52;
			if (MLC_DEVICE == true)
				spare_bit = PAGEFMT_SPARE_100_1KS;
			spare_per_sector = 100;
			break;
		case 122:
		case 124:
		case 126:
				ecc_bit = 60;
			if (hw->nand_sec_size == 1024)
				spare_bit = PAGEFMT_SPARE_122_1KS;
			spare_per_sector = 122;
			break;
		case 128:
				ecc_bit = 68;
			if (hw->nand_sec_size == 1024)
				spare_bit = PAGEFMT_SPARE_128_1KS;
			spare_per_sector = 128;
			break;
		default:
			pr_notice("[NAND]: NFI not support oobsize: %x\n", spare_per_sector);
			mtk_nand_assert(0);
		}

		mtd->oobsize = spare_per_sector * (mtd->writesize / hw->nand_sec_size);
		pr_debug("[NAND]select ecc bit:%d, sparesize :%d\n", ecc_bit, mtd->oobsize);

		pagesize = mtd->writesize;
		/* Setup PageFormat */
		if (mtk_nfi_dev_comp->chip_ver == 1) {
			if (pagesize == 16384) {
				NFI_SET_REG16(NFI_PAGEFMT_REG16,
							(spare_bit << PAGEFMT_SPARE_SHIFT_V1) | PAGEFMT_16K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			} else if (pagesize == 8192) {
				NFI_SET_REG16(NFI_PAGEFMT_REG16,
							(spare_bit << PAGEFMT_SPARE_SHIFT_V1) | PAGEFMT_8K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			} else if (pagesize == 4096) {
				if (MLC_DEVICE == false)
					NFI_SET_REG16(NFI_PAGEFMT_REG16,
								(spare_bit << PAGEFMT_SPARE_SHIFT_V1) | PAGEFMT_4K);
				else
					NFI_SET_REG16(NFI_PAGEFMT_REG16,
								(spare_bit << PAGEFMT_SPARE_SHIFT_V1) | PAGEFMT_4K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			} else if (pagesize == 2048) {
				if (MLC_DEVICE == false)
					NFI_SET_REG16(NFI_PAGEFMT_REG16,
								(spare_bit << PAGEFMT_SPARE_SHIFT_V1) | PAGEFMT_2K);
				else
					NFI_SET_REG16(NFI_PAGEFMT_REG16,
								(spare_bit << PAGEFMT_SPARE_SHIFT_V1) | PAGEFMT_2K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			}
		} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
			if (pagesize == 16384) {
				NFI_SET_REG32(NFI_PAGEFMT_REG32,
							(spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_16K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			} else if (pagesize == 8192) {
				NFI_SET_REG32(NFI_PAGEFMT_REG32,
							(spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_8K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			} else if (pagesize == 4096) {
				if (MLC_DEVICE == false)
					NFI_SET_REG32(NFI_PAGEFMT_REG32,
								(spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_4K);
				else
					NFI_SET_REG32(NFI_PAGEFMT_REG32,
								(spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_4K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			} else if (pagesize == 2048) {
				if (MLC_DEVICE == false)
					NFI_SET_REG32(NFI_PAGEFMT_REG32,
								(spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_2K);
				else
					NFI_SET_REG32(NFI_PAGEFMT_REG32,
								(spare_bit << PAGEFMT_SPARE_SHIFT) | PAGEFMT_2K_1KS);
				nand->cmdfunc = mtk_nand_command_bp;
			}
		} else {
			pr_err("[mtk_nand_select_chip] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
				mtk_nfi_dev_comp->chip_ver);
		}

		mtk_nand_configure_fdm(hw->nand_fdm_size);

		ecc_threshold = ecc_bit * 4 / 5;
		ECC_Config(hw, ecc_bit);
		g_bInitDone = true;

		/* xiaolei for kernel3.10 */
		nand->ecc.strength = ecc_bit;
		mtd->bitflip_threshold = nand->ecc.strength;
	}
	switch (chip) {
	case -1:
		break;
	case 0:
#ifdef CFG_FPGA_PLATFORM	/* FPGA NAND is placed at CS1 not CS0 */
		DRV_WriteReg16(NFI_CSEL_REG16, 0);
		break;
#endif
	case 1:
		DRV_WriteReg16(NFI_CSEL_REG16, chip);
		break;
	}
}

/******************************************************************************
 * mtk_nand_read_byte
 *
 * DESCRIPTION:
 *	 Read a byte of data !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static uint8_t mtk_nand_read_byte(struct mtd_info *mtd)
{
	uint8_t retval = 0;

	if (!mtk_nand_pio_ready()) {
		pr_err("pio ready timeout\n");
		retval = false;
	}

	if (g_bcmdstatus) {
		retval = DRV_Reg8(NFI_DATAR_REG32);
		NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_NOB_MASK);
		mtk_nand_reset();
#if (__INTERNAL_USE_AHB_MODE__)
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_DMA_BURST_EN);
#endif
		if (g_bHwEcc)
			NFI_SET_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		else
			NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);
		g_bcmdstatus = false;
	} else
		retval = DRV_Reg8(NFI_DATAR_REG32);

	return retval;
}

/******************************************************************************
 * mtk_nand_read_buf
 *
 * DESCRIPTION:
 *	 Read NAND data !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, uint8_t *buf, int len
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *nand = (struct nand_chip *)mtd->priv;
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4ColAddr = pkCMD->u4ColAddr;
	u32 u4PageSize = mtd->writesize;
	u32 row_addr = pkCMD->u4RowAddr;

	if (u4ColAddr < u4PageSize) {
		if ((u4ColAddr == 0) && (len >= u4PageSize)) {
			mtk_nand_exec_read_page(mtd, row_addr, u4PageSize, buf, pkCMD->au1OOB);
			if (len > u4PageSize) {
				u32 u4Size = min(len - u4PageSize, (u32) (sizeof(pkCMD->au1OOB)));

				memcpy(buf + u4PageSize, pkCMD->au1OOB, u4Size);
			}
		} else {
			mtk_nand_exec_read_page(mtd, row_addr, u4PageSize, nand->buffers->databuf, pkCMD->au1OOB);
			memcpy(buf, nand->buffers->databuf + u4ColAddr, len);
		}
		pkCMD->u4OOBRowAddr = pkCMD->u4RowAddr;
	} else {
		u32 u4Offset = u4ColAddr - u4PageSize;
		u32 u4Size = min(len - u4Offset, (u32) (sizeof(pkCMD->au1OOB)));

		if (pkCMD->u4OOBRowAddr != pkCMD->u4RowAddr) {
			mtk_nand_exec_read_page(mtd, row_addr, u4PageSize, nand->buffers->databuf, pkCMD->au1OOB);
			pkCMD->u4OOBRowAddr = pkCMD->u4RowAddr;
		}
		memcpy(buf, pkCMD->au1OOB + u4Offset, u4Size);
	}
	pkCMD->u4ColAddr += len;
}

/******************************************************************************
 * mtk_nand_write_buf
 *
 * DESCRIPTION:
 *	 Write NAND data !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, const uint8_t *buf, int len
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4ColAddr = pkCMD->u4ColAddr;
	u32 u4PageSize = mtd->writesize;
	int i4Size, i;

	if (u4ColAddr >= u4PageSize) {
		u32 u4Offset = u4ColAddr - u4PageSize;
		u8 *pOOB = pkCMD->au1OOB + u4Offset;

		i4Size = min(len, (int)(sizeof(pkCMD->au1OOB) - u4Offset));

		for (i = 0; i < i4Size; i++)
			pOOB[i] &= buf[i];
	} else {
		pkCMD->pDataBuf = (u8 *) buf;
	}

	pkCMD->u4ColAddr += len;
}

/******************************************************************************
 * mtk_nand_write_page_hwecc
 *
 * DESCRIPTION:
 *	 Write NAND data with hardware ecc !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static int mtk_nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
						const uint8_t *buf, int oob_required, int page)
{
	mtk_nand_write_buf(mtd, buf, mtd->writesize);
	mtk_nand_write_buf(mtd, chip->oob_poi, mtd->oobsize);
	return 0;
}

/******************************************************************************
 * mtk_nand_read_page_hwecc
 *
 * DESCRIPTION:
 *	 Read NAND data with hardware ecc !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static int mtk_nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf,
					int oob_required, int page)
{
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4ColAddr = pkCMD->u4ColAddr;
	u32 u4PageSize = mtd->writesize;
	u32 row_addr = pkCMD->u4RowAddr;

	if (u4ColAddr == 0) {
		mtk_nand_exec_read_page(mtd, row_addr, u4PageSize, buf, chip->oob_poi);
		pkCMD->u4ColAddr += u4PageSize + mtd->oobsize;
	}
	return 0;
}

/******************************************************************************
 *
 * Read a page to a logical address
 *
 *****************************************************************************/
static int mtk_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip, u8 *buf, int page)
{
	int page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	u32 row_addr;
	int bRet = ERR_RTN_SUCCESS;
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	row_addr = page_in_block + mapped_block * page_per_block;

	bRet = mtk_nand_exec_read_page(mtd, row_addr, mtd->writesize, buf, chip->oob_poi);
	if (bRet == ERR_RTN_SUCCESS) {
		return 0;
	}

	return 0;
}

static int mtk_nand_read_subpage(struct mtd_info *mtd, struct nand_chip *chip, u8 *buf, int page,
					int subpage, int subpageno)
{
	int page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;
	u32 block;
	int coladdr;
	u32 page_in_block;
	u32 mapped_block;
	int bRet = ERR_RTN_SUCCESS;
	int sec_num = 1 << (chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
	u32 row_addr;
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	coladdr = subpage * (devinfo.sectorsize + spare_per_sector);

	row_addr = page_in_block + mapped_block * page_per_block;
	bRet = mtk_nand_exec_read_sector(mtd, row_addr, coladdr, devinfo.sectorsize * subpageno,
						buf, chip->oob_poi, subpageno);
	if (bRet == ERR_RTN_SUCCESS) {
		return 0;
	}
	return 0;
}

/******************************************************************************
 *
 * Erase a block at a logical address
 *
 *****************************************************************************/
int mtk_nand_erase_hw(struct mtd_info *mtd, int page)
{
	int result;
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	u32 real_row_addr = 0;
#ifdef _MTK_NAND_DUMMY_DRIVER_
	if (dummy_driver_debug) {
		unsigned long long time = sched_clock();

		if (!((time * 123 + 59) % 1024)) {
			pr_err("[NAND_DUMMY_DRIVER] Simulate erase error at page: 0x%x\n",
					page);
			return NAND_STATUS_FAIL;
		}
	}
#endif
	/* pr_err("mtk_nand_erase_hw: page 0x%x\n", page); */

	real_row_addr = page;
	result = chip->erase(mtd, real_row_addr);

	return result;
}

static int mtk_nand_erase(struct mtd_info *mtd, int page)
{
	int status;
	struct nand_chip *chip = mtd->priv;
	/* int block_size = 1 << (chip->phys_erase_shift); */
	int page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;
	u32 block;
	u32 page_in_block;
	u32 mapped_block;
	bool erase_fail = false;
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	status = mtk_nand_erase_hw(mtd, page_in_block + page_per_block * mapped_block);

	if (status & NAND_STATUS_FAIL)
		erase_fail = true;

	if (erase_fail) {
		if (update_bmt
			((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
				chip->page_shift, UPDATE_ERASE_FAIL, NULL, NULL)) {
			pr_err("Erase fail at block: 0x%x, update BMT success\n", mapped_block);
		} else {
			pr_err("Erase fail at block: 0x%x, update BMT fail\n", mapped_block);
			return NAND_STATUS_FAIL;
		}
	}

	return 0;
}

/******************************************************************************
 * mtk_nand_read_oob_raw
 *
 * DESCRIPTION:
 *	 Read oob data
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, const uint8_t *buf, int addr, int len
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 this function read raw oob data out of flash, so need to re-organise
 *	 data format before using.
 *	 len should be times of 8, call this after nand_get_device.
 *	 Should notice, this function read data without ECC protection.
 *
 *****************************************************************************/
static int mtk_nand_read_oob_raw(struct mtd_info *mtd, uint8_t *buf, int page_addr, int len)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	u32 col_addr = 0;
	u32 sector = 0;
	int res = 0;
	u32 colnob = 2, rawnob = devinfo.addr_cycle - 2;
	int randomread = 0;
	int read_len = 0;
	int sec_num = 1 << (chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
	u32 sector_size = 1024;

	if (len > NAND_MAX_OOBSIZE || len % OOB_AVAI_PER_SECTOR || !buf) {
		pr_warn("[%s] invalid parameter, len: %d, buf: %p\n", __func__, len,
				buf);
		return -EINVAL;
	}
	if (len > spare_per_sector)
		randomread = 1;

	if (!randomread || !(devinfo.advancedmode & RAMDOM_READ)) {
		while (len > 0) {
			read_len = min(len, spare_per_sector);
			col_addr = sector_size +
				sector * (sector_size + spare_per_sector);	/* TODO: Fix this hard-code 16 */
			if (!mtk_nand_ready_for_read(chip,
					page_addr, col_addr, sec_num, false, NULL, NORMAL_READ)) {
				pr_warn("mtk_nand_ready_for_read return failed\n");
				res = -EIO;
				goto error;
			}
			if (!mtk_nand_mcu_read_data(mtd,
					buf + spare_per_sector * sector, read_len)) {	/* TODO: and this 8 */
				pr_warn("mtk_nand_mcu_read_data return failed\n");
				res = -EIO;
				goto error;
			}
			mtk_nand_stop_read();
			/* dump_data(buf + 16 * sector, 16); */
			sector++;
			len -= read_len;
		}
	} else {		/* should be 64 */

		col_addr = sector_size;
		if (chip->options & NAND_BUSWIDTH_16)
			col_addr /= 2;

		if (!mtk_nand_reset())
			goto error;

		mtk_nand_set_mode(0x6000);
		NFI_SET_REG16(NFI_CNFG_REG16, CNFG_READ_EN);
		DRV_WriteReg32(NFI_CON_REG16, 4 << CON_NFI_SEC_SHIFT);

		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_AHB);
		NFI_CLN_REG16(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

		mtk_nand_set_autoformat(false);

		if (!mtk_nand_set_command(NAND_CMD_READ0))
			goto error;
		/* 1 FIXED ME: For Any Kind of AddrCycle */
		if (!mtk_nand_set_address(col_addr, page_addr, colnob, rawnob))
			goto error;

		if (!mtk_nand_set_command(NAND_CMD_READSTART))
			goto error;
		if (!mtk_nand_status_ready(STA_NAND_BUSY))
			goto error;

		read_len = min(len, spare_per_sector);
		if (!mtk_nand_mcu_read_data(mtd, buf + spare_per_sector * sector, read_len)) {	/* TODO: and this 8 */
			pr_warn("mtk_nand_mcu_read_data return failed first 16\n");
			res = -EIO;
			goto error;
		}
		sector++;
		len -= read_len;
		mtk_nand_stop_read();
		while (len > 0) {
			read_len = min(len, spare_per_sector);
			if (!mtk_nand_set_command(0x05))
				goto error;

			col_addr = sector_size + sector * (sector_size + 16);	/* :TODO_JP careful 16 */
			if (chip->options & NAND_BUSWIDTH_16)
				col_addr /= 2;
			DRV_WriteReg32(NFI_COLADDR_REG32, col_addr);
			DRV_WriteReg16(NFI_ADDRNOB_REG16, 2);
			DRV_WriteReg32(NFI_CON_REG16, 4 << CON_NFI_SEC_SHIFT);

			if (!mtk_nand_status_ready(STA_ADDR_STATE))
				goto error;

			if (!mtk_nand_set_command(0xE0))
				goto error;
			if (!mtk_nand_status_ready(STA_NAND_BUSY))
				goto error;
			if (!mtk_nand_mcu_read_data(mtd,
					buf + spare_per_sector * sector, read_len)) {	/* TODO: and this 8 */
				pr_warn("mtk_nand_mcu_read_data return failed first 16\n");
				res = -EIO;
				goto error;
			}
			mtk_nand_stop_read();
			sector++;
			len -= read_len;
		}
		/* dump_data(&testbuf[16], 16); */
		/* pr_debug(KERN_ERR "\n"); */
	}
error:
	NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BRD);
	return res;
}

static int mtk_nand_write_oob_raw(struct mtd_info *mtd, const uint8_t *buf, int page_addr, int len)
{
	struct nand_chip *chip = mtd->priv;
	/* int i; */
	u32 col_addr = 0;
	u32 sector = 0;
	/* int res = 0; */
	/* u32 colnob = 2, rawnob = devinfo.addr_cycle-2; */
	/* int randomread =0; */
	int write_len = 0;
	int status;
	int sec_num = 1 << (chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
	u32 sector_size = 1024;

	if (len > NAND_MAX_OOBSIZE || len % OOB_AVAI_PER_SECTOR || !buf) {
		pr_warn("[%s] invalid parameter, len: %d, buf: %p\n", __func__, len,
				buf);
		return -EINVAL;
	}

	while (len > 0) {
		write_len = min(len, spare_per_sector);
		col_addr = sector * (sector_size + spare_per_sector) + sector_size;
		if (!mtk_nand_ready_for_write(chip, page_addr, col_addr, false, NULL))
			return -EIO;

		if (!mtk_nand_mcu_write_data(mtd, buf + sector * spare_per_sector, write_len))
			return -EIO;

		(void)mtk_nand_check_RW_count(write_len);
		NFI_CLN_REG32(NFI_CON_REG16, CON_NFI_BWR);
		(void)mtk_nand_set_command(NAND_CMD_PAGEPROG);

		while (DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY)
			;

		status = chip->waitfunc(mtd, chip);
		if (status & NAND_STATUS_FAIL) {
			pr_debug("status: %d\n", status);
			return -EIO;
		}

		len -= write_len;
		sector++;
	}

	return 0;
}

static int mtk_nand_write_oob_hw(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	/* u8 *buf = chip->oob_poi; */
	int i, iter;

	int sec_num = 1 << (chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;

	memcpy(local_oob_buf, chip->oob_poi, mtd->oobsize);

	/* copy ecc data */
	for (i = 0; i < chip->ecc.layout->eccbytes; i++) {
		iter = (i / OOB_AVAI_PER_SECTOR) * spare_per_sector + OOB_AVAI_PER_SECTOR +
			i % OOB_AVAI_PER_SECTOR;
		local_oob_buf[iter] = chip->oob_poi[chip->ecc.layout->eccpos[i]];
		/* chip->oob_poi[chip->ecc.layout->eccpos[i]] = local_oob_buf[iter]; */
	}

	/* copy FDM data */
	for (i = 0; i < sec_num; i++) {
		memcpy(&local_oob_buf[i * spare_per_sector],
				&chip->oob_poi[i * OOB_AVAI_PER_SECTOR], OOB_AVAI_PER_SECTOR);
	}

	return mtk_nand_write_oob_raw(mtd, local_oob_buf, page, mtd->oobsize);
}

static int mtk_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int page_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);
	u32 block;
	u16 page_in_block;
	u32 mapped_block;

	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

	if (mapped_block != block)
		set_bad_index_to_oob(chip->oob_poi, block);
	else
		set_bad_index_to_oob(chip->oob_poi, FAKE_INDEX);

	if (mtk_nand_write_oob_hw(mtd, chip, page_in_block + mapped_block * page_per_block /* page */)) {
		pr_err("write oob fail at block: 0x%x, page: 0x%x\n", mapped_block,
			page_in_block);
		if (update_bmt((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
				chip->page_shift, UPDATE_WRITE_FAIL, NULL, chip->oob_poi)) {
			pr_debug("Update BMT success\n");
			return 0;
		}
		pr_err("Update BMT fail\n");
		return -EIO;
	}

	return 0;
}

int mtk_nand_block_markbad_hw(struct mtd_info *mtd, loff_t offset)
{
	int block; /* = (int)(offset / (devinfo.blocksize * 1024)); */
	int page; /* = block * (devinfo.blocksize * 1024 / devinfo.pagesize); */
	int ret;
	loff_t temp;
	u8 buf[8];

	temp = offset;
	do_div(temp, ((devinfo.blocksize * 1024) & 0xFFFFFFFF));
	block = (u32) temp;

	page = block * (devinfo.blocksize * 1024 / devinfo.pagesize);

	memset(buf, 0xFF, 8);
	buf[0] = 0;

	ret = mtk_nand_write_oob_raw(mtd, buf, page, 8);
	return ret;
}

static int mtk_nand_block_markbad(struct mtd_info *mtd, loff_t offset)
{
	struct nand_chip *chip = mtd->priv;
	u32 block; /*  = (u32)(offset  / (devinfo.blocksize * 1024)); */
	int page; /* = block * (devinfo.blocksize * 1024 / devinfo.pagesize); */
	u32 mapped_block;
	int ret;
	loff_t temp;

	temp = offset;
	do_div(temp, ((devinfo.blocksize * 1024) & 0xFFFFFFFF));
	block = (u32) temp;
	page = block * (devinfo.blocksize * 1024 / devinfo.pagesize);

	nand_get_device(mtd, FL_WRITING);

	page = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);
	ret = mtk_nand_block_markbad_hw(mtd, mapped_block * (devinfo.blocksize * 1024));

	nand_release_device(mtd);

	return ret;
}

int mtk_nand_read_oob_hw(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	int i;
	u8 iter = 0;

	int sec_num = 1 << (chip->page_shift - host->hw->nand_sec_shift);
	int spare_per_sector = mtd->oobsize / sec_num;
#ifdef TESTTIME
	unsigned long long time1, time2;

	time1 = sched_clock();
#endif

	if (mtk_nand_read_oob_raw(mtd, chip->oob_poi, page, mtd->oobsize)) {
		/* pr_debug(KERN_ERR "[%s]mtk_nand_read_oob_raw return failed\n", __FUNCTION__); */
		return -EIO;
	}
#ifdef TESTTIME
	time2 = sched_clock() - time1;
	if (!readoobflag) {
		readoobflag = 1;
		pr_err("[%s] time is %llu", __func__, time2);
	}
#endif

	/* adjust to ecc physical layout to memory layout */
	/*********************************************************/
	/* FDM0 | ECC0 | FDM1 | ECC1 | FDM2 | ECC2 | FDM3 | ECC3 */
	/*	8B	|  8B  |  8B  |  8B  |	8B	|  8B  |  8B  |  8B  */
	/*********************************************************/

	memcpy(local_oob_buf, chip->oob_poi, mtd->oobsize);

	/* copy ecc data */
	for (i = 0; i < chip->ecc.layout->eccbytes; i++) {
		iter = (i / OOB_AVAI_PER_SECTOR) * spare_per_sector + OOB_AVAI_PER_SECTOR +
			i % OOB_AVAI_PER_SECTOR;
		chip->oob_poi[chip->ecc.layout->eccpos[i]] = local_oob_buf[iter];
	}

	/* copy FDM data */
	for (i = 0; i < sec_num; i++) {
		memcpy(&chip->oob_poi[i * OOB_AVAI_PER_SECTOR],
				&local_oob_buf[i * spare_per_sector], OOB_AVAI_PER_SECTOR);
	}

	return 0;
}

static int mtk_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip, int page)
{
	mtk_nand_read_page(mtd, chip, temp_buffer_16_align, page);
	return 0;		/* the return value is sndcmd */
}

int mtk_nand_block_bad_hw(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	int page_addr = (int)(ofs >> chip->page_shift);
	u32 block, mapped_block;
	int ret;
	unsigned int page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;

	page_addr &= ~(page_per_block - 1);
	memset(temp_buffer_16_align, 0xFF, LPAGE);

	ret = mtk_nand_read_subpage(mtd, chip, temp_buffer_16_align, (ofs >> chip->page_shift), 0, 1);

	page_addr = mtk_nand_page_transform(mtd, chip, page_addr, &block, &mapped_block);
	if (ret != 0) {
		pr_warn("mtk_nand_read_oob_raw return error %d\n", ret);
		return 1;
	}

	if (chip->oob_poi[0] != 0xff) {
		pr_debug("Bad block detected at 0x%x, oob_buf[0] is 0x%x\n",
				block * page_per_block, chip->oob_poi[0]);
		return 1;
	}

	return 0;		/* everything is OK, good block */
}

static int mtk_nand_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	int chipnr = 0;

	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	int block; /* = (int)(ofs / (devinfo.blocksize * 1024)); */
	int mapped_block;
	int page = (int)(ofs >> chip->page_shift);
	int page_in_block;
	int page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;
	loff_t temp;
	int ret;

	temp = ofs;
	do_div(temp, ((devinfo.blocksize * 1024) & 0xFFFFFFFF));
	block = (int) temp;

	if (getchip) {
		chipnr = (int)(ofs >> chip->chip_shift);
		nand_get_device(mtd, FL_READING);
		/* Select the NAND device */
		chip->select_chip(mtd, chipnr);
	}

	ret = mtk_nand_block_bad_hw(mtd, ofs);
	page_in_block = mtk_nand_page_transform(mtd, chip, page, &block, &mapped_block);

	if (ret) {
		pr_debug("Unmapped bad block: 0x%x %d\n", mapped_block, ret);
		if (update_bmt((u64) ((u64) page_in_block + (u64) mapped_block * page_per_block) <<
				chip->page_shift, UPDATE_UNMAPPED_BLOCK, NULL, NULL)) {
			pr_debug("Update BMT success\n");
			ret = 0;
		} else {
			pr_err("Update BMT fail\n");
			ret = 1;
		}
	}

	if (getchip)
		nand_release_device(mtd);

	return ret;
}

/******************************************************************************
 * mtk_nand_init_size
 *
 * DESCRIPTION:
 *	 initialize the pagesize, oobsize, blocksize
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, struct nand_chip *this, u8 *id_data
 *
 * RETURNS:
 *	 Buswidth
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/

int mtk_nand_init_size(struct mtd_info *mtd, struct nand_chip *this, u8 *id_data)
{
	/* Get page size */
	mtd->writesize = devinfo.pagesize;

	/* Get oobsize */
	mtd->oobsize = devinfo.sparesize;

	/* Get blocksize. */
	mtd->erasesize = devinfo.blocksize * 1024;
	/* Get buswidth information */
	if (devinfo.iowidth == 16)
		return NAND_BUSWIDTH_16;
	else
		return 0;
}
EXPORT_SYMBOL(mtk_nand_init_size);

/******************************************************************************
 * mtk_nand_verify_buf
 *
 * DESCRIPTION:
 *	 Verify the NAND write data is correct or not !
 *
 * PARAMETERS:
 *	 struct mtd_info *mtd, const uint8_t *buf, int len
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
#ifdef CONFIG_MTD_NAND_VERIFY_WRITE

char gacBuf[LPAGE + LSPARE];

static int mtk_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
#if 1
	struct nand_chip *chip = (struct nand_chip *)mtd->priv;
	struct NAND_CMD *pkCMD = &g_kCMD;
	u32 u4PageSize = mtd->writesize;
	u32 *pSrc, *pDst;
	int i;
	u32 row_addr = pkCMD->u4RowAddr;
	u32 page_per_block = devinfo.blocksize * 1024 / devinfo.pagesize;

	mtk_nand_exec_read_page(mtd, row_addr, u4PageSize, gacBuf, gacBuf + u4PageSize);

	pSrc = (u32 *) buf;
	pDst = (u32 *) gacBuf;
	len = len / sizeof(u32);
	for (i = 0; i < len; ++i) {
		if (*pSrc != *pDst) {
			pr_err("mtk_nand_verify_buf page fail at page %d\n", pkCMD->u4RowAddr);
			return -1;
		}
		pSrc++;
		pDst++;
	}

	pSrc = (u32 *) chip->oob_poi;
	pDst = (u32 *) (gacBuf + u4PageSize);

	if ((pSrc[0] != pDst[0]) || (pSrc[1] != pDst[1]) || (pSrc[2] != pDst[2])
		|| (pSrc[3] != pDst[3]) || (pSrc[4] != pDst[4]) || (pSrc[5] != pDst[5]))
		/* TODO: Ask Designer Why? */
		/* (pSrc[6] != pDst[6]) || (pSrc[7] != pDst[7])) */
	{
		pr_err("mtk_nand_verify_buf oob fail at page %d\n", pkCMD->u4RowAddr);
		pr_err("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", pSrc[0], pSrc[1], pSrc[2],
			pSrc[3], pSrc[4], pSrc[5], pSrc[6], pSrc[7]);
		pr_err("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", pDst[0], pDst[1], pDst[2],
			pDst[3], pDst[4], pDst[5], pDst[6], pDst[7]);
		return -1;
	}
	/* pr_debug(KERN_INFO"mtk_nand_verify_buf OK at page %d\n", g_kCMD.u4RowAddr); */

	return 0;
#else
	return 0;
#endif
}
#endif

/******************************************************************************
 * mtk_nand_init_hw
 *
 * DESCRIPTION:
 *	 Initial NAND device hardware component !
 *
 * PARAMETERS:
 *	 struct mtk_nand_host *host (Initial setting data)
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void mtk_nand_init_hw(struct mtk_nand_host *host)
{
	struct mtk_nand_host_hw *hw = host->hw;


	g_bInitDone = false;
	g_kCMD.u4OOBRowAddr = (u32) -1;

	/* Set default NFI access timing control */
	DRV_WriteReg32(NFI_ACCCON_REG32, hw->nfi_access_timing);
	DRV_WriteReg16(NFI_CNFG_REG16, 0);
	if (mtk_nfi_dev_comp->chip_ver == 1) {
		DRV_WriteReg16(NFI_PAGEFMT_REG16, 4);
	} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
		DRV_WriteReg32(NFI_PAGEFMT_REG32, 4);
	} else {
		pr_err("[mtk_nand_init_hw] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
			mtk_nfi_dev_comp->chip_ver);
	}
	DRV_WriteReg32(NFI_ENMPTY_THRESH_REG32, 40);

	/* Reset the state machine and data FIFO, because flushing FIFO */
	(void)mtk_nand_reset();

	/* Set the ECC engine */
	if (hw->nand_ecc_mode == NAND_ECC_HW) {
		pr_notice("Use HW ECC\n");
		if (g_bHwEcc)
			NFI_SET_REG32(NFI_CNFG_REG16, CNFG_HW_ECC_EN);

		ECC_Config(host->hw, 4);
		mtk_nand_configure_fdm(8);
	}

	/* Initialize interrupt. Clear interrupt, read clear. */
	DRV_Reg16(NFI_INTR_REG16);

	/* Interrupt arise when read data or program data to/from AHB is done. */
	DRV_WriteReg16(NFI_INTR_EN_REG16, 0);

	/* Enable automatic disable ECC clock when NFI is busy state */
	if (mtk_nfi_dev_comp->chip_ver != 3)
		DRV_WriteReg16(NFI_DEBUG_CON1_REG16, (NFI_BYPASS | WBUF_EN | HWDCM_SWCON_ON));

#ifdef CONFIG_PM
	host->saved_para.suspend_flag = 0;
#endif
	/* Reset */
}

/* ------------------------------------------------------------------------------- */
static int mtk_nand_dev_ready(struct mtd_info *mtd)
{
	return !(DRV_Reg32(NFI_STA_REG32) & STA_NAND_BUSY);
}

/******************************************************************************
 * mtk_nand_proc_read
 *
 * DESCRIPTION:
 *	 Read the proc file to get the interrupt scheme setting !
 *
 * PARAMETERS:
 *	 char *page, char **start, off_t off, int count, int *eof, void *data
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
int mtk_nand_proc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *p = buffer;
	int len = 0;
	int i;

	p += sprintf(p, "ID:");
	for (i = 0; i < devinfo.id_length; i++)
		p += sprintf(p, " 0x%x", devinfo.id[i]);

	p += sprintf(p, "\n");
	p += sprintf(p, "total size: %dMiB; part number: %s\n", devinfo.totalsize,
				devinfo.devciename);
	p += sprintf(p, "Current working in %s mode\n", g_i4Interrupt ? "interrupt" : "polling");
	p += sprintf(p, "NFI_ACCON = 0x%x\n", DRV_Reg32(NFI_ACCCON_REG32));
	p += sprintf(p, "NFI_NAND_TYPE_CNFG_REG32= 0x%x\n", DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32));
#ifdef CONFIG_MTK_FPGA
	p += sprintf(p, "[FPGA Dummy]DRV_CFG_NFIA(0x0) = 0x0\n");
	p += sprintf(p, "[FPGA Dummy]DRV_CFG_NFIB(0x0) = 0x0\n");
#else
	p += sprintf(p, "DRV_CFG_NFIA = 0x%x\n", *((volatile u32 *)(GPIO_BASE + 0xC20)));
	p += sprintf(p, "DRV_CFG_NFIB = 0x%x\n", *((volatile u32 *)(GPIO_BASE + 0xB50)));
#endif
	len = p - buffer;

	return len < count ? len : count;
}

/******************************************************************************
 * mtk_nand_proc_write
 *
 * DESCRIPTION:
 *	 Write the proc file to set the interrupt scheme !
 *
 * PARAMETERS:
 *	 struct file* file, const char* buffer,	unsigned long count, void *data
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
ssize_t mtk_nand_proc_write(struct file *file, const char __user *buffer, size_t count,
				loff_t *data)
{
	struct mtd_info *mtd = &host->mtd;
	char buf[16];
	char cmd;
	int value;
	int len = count;	/* , n; */

	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	if (sscanf(buf, "%c%x", &cmd, &value) != 2)
		return -EINVAL;

	switch (cmd) {
	case 'A':		/* NFIA driving setting */
#ifdef CONFIG_MTK_FPGA
		pr_debug("[FPGA Dummy]NFIA driving setting\n");
#else
		if ((value >= 0x0) && (value <= 0x7)) {	/* driving step */
			pr_debug("[NAND]IO PAD driving setting value(0x%x)\n\n", value);
			*((volatile u32 *)(GPIO_BASE + 0xC20)) = value;	/* pad 7 6 4 3 0 1 5 8 2 */
		} else
			pr_err("[NAND]IO PAD driving setting value(0x%x) error\n", value);
#endif
		break;
	case 'B':		/* NFIB driving setting */
#ifdef CONFIG_MTK_FPGA
		pr_debug("[FPGA Dummy]NFIB driving setting\n");
#else
		if ((value >= 0x0) && (value <= 0x7)) {	/* driving step */
			pr_debug("[NAND]Ctrl PAD driving setting value(0x%x)\n\n", value);
			*((volatile u32 *)(GPIO_BASE + 0xB50)) = value;	/* CLE CE1 CE0 RE RB */
			*((volatile u32 *)(GPIO_BASE + 0xC10)) = value;	/* ALE */
			*((volatile u32 *)(GPIO_BASE + 0xC00)) = value;	/* WE */
		} else
			pr_err("[NAND]Ctrl PAD driving setting value(0x%x) error\n",
					value);
#endif
		break;
	case 'D':
#ifdef _MTK_NAND_DUMMY_DRIVER_
		pr_debug("Enable dummy driver\n");
		dummy_driver_debug = 1;
#endif
		break;
	case 'I':		/* Interrupt control */
		if ((value > 0 && !g_i4Interrupt) || (value == 0 && g_i4Interrupt)) {
			nand_get_device(mtd, FL_READING);

			g_i4Interrupt = value;

			if (g_i4Interrupt) {
				DRV_Reg16(NFI_INTR_REG16);
				enable_irq(MT_NFI_IRQ_ID);
			} else
				disable_irq(MT_NFI_IRQ_ID);

			nand_release_device(mtd);
		}
		break;
	case 'T':		/* ACCCON Setting */
		nand_get_device(mtd, FL_READING);
		DRV_WriteReg32(NFI_ACCCON_REG32, value);
		nand_release_device(mtd);
		break;
	default:
		break;
	}

	return len;
}

/******************************************************************************
 * mtk_nand_probe
 *
 * DESCRIPTION:
 *	 register the nand device file operations !
 *
 * PARAMETERS:
 *	 struct platform_device *pdev : device structure
 *
 * RETURNS:
 *	 0 : Success
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static int mtk_nand_probe(struct platform_device *pdev)
{

	struct mtk_nand_host_hw *hw;
	struct mtd_info *mtd;
	struct nand_chip *nand_chip;
	/*struct resource *res = pdev->resource; */
	int err = 0;
	u64 temp;
#if !defined(CONFIG_MTK_LEGACY)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		int ret = 0;
#endif
	u32 efuse_index = 0;
#endif
	u32 EFUSE_RANDOM_ENABLE = 0x00000004;

	u8 id[NAND_MAX_ID];
	int i;
	u32 sector_size = NAND_SECTOR_SIZE;
	int bmt_sz = 0;

#ifdef CONFIG_OF
	const struct of_device_id *of_id;

	of_id = of_match_node(mtk_nfi_of_match, pdev->dev.of_node);
	if (!of_id)
		return -EINVAL;

	mtk_nfi_dev_comp = of_id->data;
	/* dt modify */
	mtk_nfi_base = of_iomap(pdev->dev.of_node, 0);
	pr_debug("of_iomap for nfi base @ 0x%p\n", mtk_nfi_base);

	if (mtk_nfiecc_node == NULL) {
		if (mtk_nfi_dev_comp->chip_ver == 1)
			mtk_nfiecc_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8127-nfiecc");
		else if (mtk_nfi_dev_comp->chip_ver == 2)
			mtk_nfiecc_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-nfiecc");
		else if (mtk_nfi_dev_comp->chip_ver == 3)
			mtk_nfiecc_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8167-nfiecc");
		mtk_nfiecc_base = of_iomap(mtk_nfiecc_node, 0);
		pr_debug("of_iomap for nfiecc base @ 0x%p\n", mtk_nfiecc_base);
	}
	nfi_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	if (mtk_gpio_node == NULL) {
		/* mtk_gpio_node = of_find_compatible_node(NULL, NULL, "mediatek,GPIO"); */
		if (mtk_nfi_dev_comp->chip_ver == 1)
			mtk_gpio_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8127-pctl-a-syscfg");
		else if (mtk_nfi_dev_comp->chip_ver == 2)
			mtk_gpio_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-pctl-a-syscfg");
		else if (mtk_nfi_dev_comp->chip_ver == 3)
			mtk_gpio_node = of_find_compatible_node(NULL, NULL, "mediatek,mt8167-pctl-a-syscfg");
		mtk_gpio_base = of_iomap(mtk_gpio_node, 0);
		pr_debug("of_iomap for gpio base @ 0x%p\n", mtk_gpio_base);
	}

#ifdef CONFIG_MTK_LEGACY
	if (mtk_efuse_node == NULL) {
		mtk_efuse_node = of_find_compatible_node(NULL, NULL, "mediatek,EFUSEC");
		mtk_efuse_base = of_iomap(mtk_efuse_node, 0);
		pr_debug("of_iomap for efuse base @ 0x%p\n", mtk_efuse_base);
	}

	if (mtk_infra_node == NULL) {
		mtk_infra_node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
		mtk_infra_base = of_iomap(mtk_infra_node, 0);
		pr_debug("of_iomap for infra base @ 0x%p\n", mtk_infra_base);
	}
#endif

	/* dt modify */
#endif

#if !defined(CONFIG_MTK_LEGACY)
	if (mtk_nfi_dev_comp->chip_ver == 1) {
		nfi_hclk = devm_clk_get(&pdev->dev, "nfi_ck");
		WARN_ON(IS_ERR(nfi_hclk));
		nfiecc_bclk = devm_clk_get(&pdev->dev, "nfi_ecc_ck");
		WARN_ON(IS_ERR(nfiecc_bclk));
		nfi_bclk = devm_clk_get(&pdev->dev, "nfi_pad_ck");
		WARN_ON(IS_ERR(nfi_bclk));
		mtk_nand_regulator = devm_regulator_get(&pdev->dev, "vmch");
		WARN_ON(IS_ERR(mtk_nand_regulator));
	} else if (mtk_nfi_dev_comp->chip_ver == 2) {
		nfi_hclk = devm_clk_get(&pdev->dev, "nfi_hclk");
		WARN_ON(IS_ERR(nfi_hclk));
		nfiecc_bclk = devm_clk_get(&pdev->dev, "nfiecc_bclk");
		WARN_ON(IS_ERR(nfiecc_bclk));
		nfi_bclk = devm_clk_get(&pdev->dev, "nfi_bclk");
		WARN_ON(IS_ERR(nfi_bclk));
		onfi_sel_clk = devm_clk_get(&pdev->dev, "onfi_sel");
		WARN_ON(IS_ERR(onfi_sel_clk));
		onfi_26m_clk = devm_clk_get(&pdev->dev, "onfi_clk26m");
		WARN_ON(IS_ERR(onfi_26m_clk));
		onfi_mode5 = devm_clk_get(&pdev->dev, "onfi_mode5");
		WARN_ON(IS_ERR(onfi_mode5));
		onfi_mode4 = devm_clk_get(&pdev->dev, "onfi_mode4");
		WARN_ON(IS_ERR(onfi_mode4));
		nfi_bclk_sel = devm_clk_get(&pdev->dev, "nfi_bclk_sel");
		WARN_ON(IS_ERR(nfi_bclk_sel));
		nfi_ahb_clk = devm_clk_get(&pdev->dev, "nfi_ahb_clk");
		WARN_ON(IS_ERR(nfi_ahb_clk));
		nfi_1xpad_clk = devm_clk_get(&pdev->dev, "nfi_1xpad_clk");
		WARN_ON(IS_ERR(nfi_1xpad_clk));
		nfi_ecc_pclk = devm_clk_get(&pdev->dev, "nfiecc_pclk");
		WARN_ON(IS_ERR(nfi_ecc_pclk));
		nfi_pclk = devm_clk_get(&pdev->dev, "nfi_pclk");
		WARN_ON(IS_ERR(nfi_pclk));
		onfi_pad_clk = devm_clk_get(&pdev->dev, "onfi_pad_clk");
		WARN_ON(IS_ERR(onfi_pad_clk));
		mtk_nand_regulator = devm_regulator_get(&pdev->dev, "vmch");
		WARN_ON(IS_ERR(mtk_nand_regulator));
	} else if (mtk_nfi_dev_comp->chip_ver == 3) {
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		nfi_hclk = devm_clk_get(&pdev->dev, "nfi_hclk");
		WARN_ON(IS_ERR(nfi_hclk));
		nfiecc_bclk = devm_clk_get(&pdev->dev, "nfiecc_bclk");
		WARN_ON(IS_ERR(nfiecc_bclk));
		nfi_bclk = devm_clk_get(&pdev->dev, "nfi_bclk");
		WARN_ON(IS_ERR(nfi_bclk));
		mtk_nand_regulator = devm_regulator_get(&pdev->dev, "vmch");
		WARN_ON(IS_ERR(mtk_nand_regulator));
#endif
	}
#endif

#if defined(CONFIG_MTK_LEGACY)
#ifdef CONFIG_MTK_PMIC_MT6397
	hwPowerOn(MT65XX_POWER_LDO_VMCH, VOL_3300, "NFI");
#else
	hwPowerOn(MT6323_POWER_LDO_VMCH, VOL_3300, "NFI");
#endif
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	ret = regulator_set_voltage(mtk_nand_regulator, 3300000, 3300000);
	if (ret != 0)
		pr_err("regulator set vol failed: %d\n", ret);

	ret = regulator_enable(mtk_nand_regulator);
	if (ret != 0)
		pr_err("regulator_enable failed: %d\n", ret);
#endif
#endif

#ifdef CONFIG_OF
	hw = (struct mtk_nand_host_hw *)pdev->dev.platform_data;
	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	WARN_ON(!hw);
	hw->nfi_bus_width = 8;
	hw->nfi_access_timing = 0x10804333;
	hw->nfi_cs_num = 2;
	hw->nand_sec_size = 512;
	hw->nand_sec_shift = 9;
	hw->nand_ecc_size = 2048;
	hw->nand_ecc_bytes = 32;
	hw->nand_ecc_mode = 2;
#else
	hw = (struct mtk_nand_host_hw *)pdev->dev.platform_data;
	WARN_ON(!hw);

	if (pdev->num_resources != 4 || res[0].flags != IORESOURCE_MEM
		|| res[1].flags != IORESOURCE_MEM || res[2].flags != IORESOURCE_IRQ
		|| res[3].flags != IORESOURCE_IRQ) {
		pr_err("%s: invalid resource type\n", __func__);
		return -ENODEV;
	}

	/* Request IO memory */
	if (!request_mem_region(res[0].start, res[0].end - res[0].start + 1, pdev->name))
		return -EBUSY;

	if (!request_mem_region(res[1].start, res[1].end - res[1].start + 1, pdev->name))
		return -EBUSY;
#endif

	/* Allocate memory for the device structure (and zero it) */
	host = kzalloc(sizeof(struct mtk_nand_host), GFP_KERNEL);
	if (!host) {
		/* pr_err("failed to allocate device structure.\n"); */
		return -ENOMEM;
	}

#if __INTERNAL_USE_AHB_MODE__
	g_bHwEcc = true;
#else
	g_bHwEcc = false;
#endif

	/* Allocate memory for 16 byte aligned buffer */
	local_buffer_16_align = local_buffer;
	temp_buffer_16_align = temp_buffer;
	/* pr_debug(KERN_INFO "Allocate 16 byte aligned buffer: %p\n", local_buffer_16_align); */

	host->hw = hw;

	/* init mtd data structure */
	nand_chip = &host->nand_chip;
	nand_chip->priv = host;	/* link the private data structures */

	mtd = &host->mtd;
	mtd->priv = nand_chip;
	mtd->owner = THIS_MODULE;
	mtd->name = "MTK-Nand";
	mtd->eraseregions = host->erase_region;

	hw->nand_ecc_mode = NAND_ECC_HW;

	/* Set address of NAND IO lines */
	nand_chip->IO_ADDR_R = (void __iomem *)NFI_DATAR_REG32;
	nand_chip->IO_ADDR_W = (void __iomem *)NFI_DATAW_REG32;
	nand_chip->chip_delay = 20;	/* 20us command delay time */
	nand_chip->ecc.mode = hw->nand_ecc_mode;	/* enable ECC */

	nand_chip->read_byte = mtk_nand_read_byte;
	nand_chip->read_buf = mtk_nand_read_buf;
	nand_chip->write_buf = mtk_nand_write_buf;
#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	nand_chip->verify_buf = mtk_nand_verify_buf;
#endif
	nand_chip->select_chip = mtk_nand_select_chip;
	nand_chip->dev_ready = mtk_nand_dev_ready;
	nand_chip->cmdfunc = mtk_nand_command_bp;
	nand_chip->ecc.read_page = mtk_nand_read_page_hwecc;
	nand_chip->ecc.write_page = mtk_nand_write_page_hwecc;

	nand_chip->ecc.layout = &nand_oob_64;
	nand_chip->ecc.size = hw->nand_ecc_size;	/* 2048 */
	nand_chip->ecc.bytes = hw->nand_ecc_bytes;	/* 32 */

	nand_chip->options = NAND_SKIP_BBTSCAN;

	/* For BMT, we need to revise driver architecture */
	nand_chip->write_page = mtk_nand_write_page;
	nand_chip->read_page = mtk_nand_read_page;
	nand_chip->read_subpage = mtk_nand_read_subpage;
	nand_chip->ecc.write_oob = mtk_nand_write_oob;
	nand_chip->ecc.read_oob = mtk_nand_read_oob;
	/* need to add nand_get_device()/nand_release_device(). */
	nand_chip->block_markbad = mtk_nand_block_markbad;
	nand_chip->erase_hw = mtk_nand_erase;
	nand_chip->block_bad = mtk_nand_block_bad;
#if CFG_FPGA_PLATFORM
	pr_debug("[FPGA Dummy]Enable NFI and NFIECC Clock\n");
#else
	nand_prepare_clock();
	nand_enable_clock();

#endif
#ifndef CONFIG_MTK_FPGA
	/* mtk_nand_gpio_init(); */
#endif

	mtk_nand_init_hw(host);
	/* Select the device */
	nand_chip->select_chip(mtd, NFI_DEFAULT_CS);

	/*
	 * Reset the chip, required by some chips (e.g. Micron MT29FxGxxxxx)
	 * after power-up
	 */
	nand_chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);

	/* Send the command for reading device ID */
	nand_chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);

	for (i = 0; i < NAND_MAX_ID; i++)
		id[i] = nand_chip->read_byte(mtd);

	manu_id = id[0];
	dev_id = id[1];

	if (!get_device_info(id, &devinfo))
		pr_err("Not Support this Device! \r\n");

	if (devinfo.pagesize == 16384) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 16384;
	} else if (devinfo.pagesize == 8192) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 8192;
	} else if (devinfo.pagesize == 4096) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 4096;
	} else if (devinfo.pagesize == 2048) {
		nand_chip->ecc.layout = &nand_oob_64;
		hw->nand_ecc_size = 2048;
	} else if (devinfo.pagesize == 512) {
		nand_chip->ecc.layout = &nand_oob_16;
		hw->nand_ecc_size = 512;
	} else if (devinfo.pagesize == 32768) {
		nand_chip->ecc.layout = &nand_oob_128;
		hw->nand_ecc_size = 32768;
	}
	if (devinfo.sectorsize == 1024) {
		sector_size = 1024;
		hw->nand_sec_shift = 10;
		hw->nand_sec_size = 1024;
		if (mtk_nfi_dev_comp->chip_ver == 1) {
			NFI_CLN_REG16(NFI_PAGEFMT_REG16, PAGEFMT_SECTOR_SEL);
		} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
			NFI_CLN_REG32(NFI_PAGEFMT_REG32, PAGEFMT_SECTOR_SEL);
		} else {
			pr_err("[mtk_nand_init_hw] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
				mtk_nfi_dev_comp->chip_ver);
		}
	}
	if (devinfo.pagesize <= 4096) {
		nand_chip->ecc.layout->eccbytes =
			devinfo.sparesize - OOB_AVAI_PER_SECTOR * (devinfo.pagesize / sector_size);
		hw->nand_ecc_bytes = nand_chip->ecc.layout->eccbytes;
		/* Modify to fit device character */
		nand_chip->ecc.size = hw->nand_ecc_size;
		nand_chip->ecc.bytes = hw->nand_ecc_bytes;
	} else {
		/* devinfo.sparesize-OOB_AVAI_PER_SECTOR * (devinfo.pagesize/sector_size); */
		nand_chip->ecc.layout->eccbytes = 64;
		hw->nand_ecc_bytes = nand_chip->ecc.layout->eccbytes;
		/* Modify to fit device character */
		nand_chip->ecc.size = hw->nand_ecc_size;
		nand_chip->ecc.bytes = hw->nand_ecc_bytes;
	}
	nand_chip->subpagesize = devinfo.sectorsize;
	nand_chip->subpage_size = devinfo.sectorsize;

	for (i = 0; i < nand_chip->ecc.layout->eccbytes; i++) {
		nand_chip->ecc.layout->eccpos[i] =
			OOB_AVAI_PER_SECTOR * (devinfo.pagesize / sector_size) + i;
	}

#if CFG_RANDOMIZER
	if (devinfo.vendor != VEND_NONE) {
#ifdef CONFIG_MTK_LEGACY
		if ((*EFUSE_RANDOM_CFG) & EFUSE_RANDOM_ENABLE) {
#else
		if ((mtk_nfi_dev_comp->chip_ver == 1) || (mtk_nfi_dev_comp->chip_ver == 2)) {
			efuse_index = 26;
			EFUSE_RANDOM_ENABLE = 0x00000004;
		} else if (mtk_nfi_dev_comp->chip_ver == 3) {
			efuse_index = 0;
			EFUSE_RANDOM_ENABLE = 0x00001000;
			pr_err("8167 nand randomizer efuse index %d\n", efuse_index);
		}
		/* the index of reg:0x102061C0 is 26 */
		if ((get_devinfo_with_index(efuse_index)) & EFUSE_RANDOM_ENABLE) {
#endif
			pr_notice("EFUSE RANDOM CFG is ON\n");
			use_randomizer = true;
			pre_randomizer = true;
		} else {
			pr_notice("EFUSE RANDOM CFG is OFF\n");
			use_randomizer = false;
			pre_randomizer = false;
		}
	}
#endif

	hw->nfi_bus_width = devinfo.iowidth;
#ifdef CONFIG_MTK_SLC_NAND_SUPPORT
	DRV_WriteReg32(NFI_ACCCON_REG32, devinfo.s_acccon);
#else
	DRV_WriteReg32(NFI_ACCCON_REG32, devinfo.timmingsetting);
#endif

	/* 16-bit bus width */
	if (hw->nfi_bus_width == 16) {
		pr_notice("Set the 16-bit I/O settings!\n");
		nand_chip->options |= NAND_BUSWIDTH_16;
	}

	mtk_dev = &pdev->dev;
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		dev_err(&pdev->dev, "set dma mask fail\n");
		pr_err("set dma mask fail\n");
	} else
		pr_notice("set dma mask ok\n");

	init_completion(&g_comp_WR_Done);
	init_completion(&g_comp_ER_Done);
#ifdef CONFIG_OF
	err = request_irq(MT_NFI_IRQ_ID, mtk_nand_irq_handler, IRQF_TRIGGER_NONE, "mtk-nand", NULL);
#else
	err = request_irq(MT_NFI_IRQ_ID, mtk_nand_irq_handler, IRQF_DISABLED, "mtk-nand", NULL);
#endif

	if (err != 0) {
		pr_err("Request IRQ fail: err = %d\n", err);
		goto out;
	}

	if (g_i4Interrupt)
		enable_irq(MT_NFI_IRQ_ID);
	else
		disable_irq(MT_NFI_IRQ_ID);

	mtd->oobsize = devinfo.sparesize;
	/* Scan to find existence of the device */
	if (nand_scan(mtd, hw->nfi_cs_num)) {
		pr_err("nand_scan fail.\n");
		err = -ENXIO;
		goto out;
	}

	g_page_size = mtd->writesize;
	g_block_size = devinfo.blocksize << 10;
	PAGES_PER_BLOCK = (u32) (g_block_size / g_page_size);
#ifdef PART_SIZE_BMTPOOL
	if (PART_SIZE_BMTPOOL) {
		bmt_sz = (PART_SIZE_BMTPOOL) >> nand_chip->phys_erase_shift;
	} else
#endif
	{
		temp = nand_chip->chipsize;
		do_div(temp, ((devinfo.blocksize * 1024) & 0xFFFFFFFF));
		bmt_sz = (int)(((u32) temp) / 100 * BBPOOL_RATIO);
	}
	platform_set_drvdata(pdev, host);

	if (hw->nfi_bus_width == 16) {
		if (mtk_nfi_dev_comp->chip_ver == 1) {
			NFI_SET_REG16(NFI_PAGEFMT_REG16, PAGEFMT_DBYTE_EN);
		} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
			NFI_SET_REG32(NFI_PAGEFMT_REG32, PAGEFMT_DBYTE_EN);
		} else {
			pr_err("[mtk_nand_init_hw] ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
				mtk_nfi_dev_comp->chip_ver);
		}
	}

	nand_chip->select_chip(mtd, 0);
#if defined(MTK_COMBO_NAND_SUPPORT)
	nand_chip->chipsize -= (bmt_sz * g_block_size);
#else
	nand_chip->chipsize -= (BMT_POOL_SIZE) << nand_chip->phys_erase_shift;
#endif
	mtd->size = nand_chip->chipsize;

	if (devinfo.vendor != VEND_NONE) {
		err = mtk_nand_interface_config(mtd);
	}

#if defined(MTK_COMBO_NAND_SUPPORT)
#ifdef CONFIG_MTK_SLC_NAND_BURNER_SUPPORT
	if (init_bmt(host,
					1 << (nand_chip->chip_shift - nand_chip->phys_erase_shift),
					(nand_chip->chipsize >> nand_chip->phys_erase_shift) - 2) != 0)
#else
	if (init_bmt(nand_chip, bmt_sz) == NULL)
#endif
#else
	if (init_bmt(nand_chip, BMT_POOL_SIZE) == NULL)
#endif
	{
		pr_err("Error: init bmt failed\n");
		return 0;
	}

	nand_chip->chipsize -= (PMT_POOL_SIZE) * (devinfo.blocksize * 1024);
	mtd->size = nand_chip->chipsize;
#ifdef PMT
	part_init_pmt(mtd, (u8 *) &g_exist_Partition[0]);
	err = mtd_device_register(mtd, g_exist_Partition, part_num);
#else
	err = mtd_device_register(mtd, g_pasStatic_Partition, part_num);
#endif

#ifdef _MTK_NAND_DUMMY_DRIVER_
	dummy_driver_debug = 0;
#endif

	/* Successfully!! */
	if (!err) {
		nand_disable_clock();
		return err;
	}

	/* Fail!! */
out:
	pr_err("[NFI] mtk_nand_probe fail, err = %d!\n", err);
	nand_release(mtd);
	platform_set_drvdata(pdev, NULL);
	kfree(host);
	nand_disable_clock();
	nand_unprepare_clock();
	return err;
}

/******************************************************************************
 * mtk_nand_suspend
 *
 * DESCRIPTION:
 *	 Suspend the nand device!
 *
 * PARAMETERS:
 *	 struct platform_device *pdev : device structure
 *
 * RETURNS:
 *	 0 : Success
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static int mtk_nand_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mtk_nand_host *host = platform_get_drvdata(pdev);
#if !defined(CONFIG_MTK_LEGACY)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		int ret = 0;
#endif
#endif
	/* struct mtd_info *mtd = &host->mtd; */
	/* backup register */
#ifdef CONFIG_PM

	if (host->saved_para.suspend_flag == 0) {
		nand_enable_clock();
		/* Save NFI register */
		host->saved_para.sNFI_CNFG_REG16 = DRV_Reg16(NFI_CNFG_REG16);
		if (mtk_nfi_dev_comp->chip_ver == 1) {
			host->saved_para.sNFI_PAGEFMT_REG16 = DRV_Reg16(NFI_PAGEFMT_REG16);
		} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
			host->saved_para.sNFI_PAGEFMT_REG32 = DRV_Reg32(NFI_PAGEFMT_REG32);
		} else {
			pr_err("[NFI] Suspend ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
				mtk_nfi_dev_comp->chip_ver);
		}
		host->saved_para.sNFI_CON_REG16 = DRV_Reg32(NFI_CON_REG16);
		host->saved_para.sNFI_ACCCON_REG32 = DRV_Reg32(NFI_ACCCON_REG32);
		host->saved_para.sNFI_INTR_EN_REG16 = DRV_Reg16(NFI_INTR_EN_REG16);
		host->saved_para.sNFI_IOCON_REG16 = DRV_Reg16(NFI_IOCON_REG16);
		host->saved_para.sNFI_CSEL_REG16 = DRV_Reg16(NFI_CSEL_REG16);
		host->saved_para.sNFI_DEBUG_CON1_REG16 = DRV_Reg16(NFI_DEBUG_CON1_REG16);

		/* save ECC register */
		host->saved_para.sECC_ENCCNFG_REG32 = DRV_Reg32(ECC_ENCCNFG_REG32);
		/* host->saved_para.sECC_FDMADDR_REG32 = DRV_Reg32(ECC_FDMADDR_REG32); */
		host->saved_para.sECC_DECCNFG_REG32 = DRV_Reg32(ECC_DECCNFG_REG32);
		/* for sync mode */
		if (g_bSyncOrToggle) {
			host->saved_para.sNFI_DLYCTRL_REG32 = DRV_Reg32(NFI_DLYCTRL_REG32);
#ifndef CONFIG_MTK_FPGA
			/* host->saved_para.sPERI_NFI_MAC_CTRL = DRV_Reg32(PERI_NFI_MAC_CTRL); */
#endif
			host->saved_para.sNFI_NAND_TYPE_CNFG_REG32 =
				DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32);
			host->saved_para.sNFI_ACCCON1_REG32 = DRV_Reg32(NFI_ACCCON1_REG3);
		}
#ifdef CONFIG_MTK_PMIC_MT6397
		hwPowerDown(MT65XX_POWER_LDO_VMCH, "NFI");
#else
#if defined(CONFIG_MTK_LEGACY)
		hwPowerDown(MT6323_POWER_LDO_VMCH, "NFI");
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		ret = regulator_disable(mtk_nand_regulator);
		if (ret != 0)
			pr_err("[NFI] Suspend regulator disable failed: %d\n", ret);
#endif
#endif
#endif
		nand_disable_clock();
		nand_unprepare_clock();
		host->saved_para.suspend_flag = 1;
	} else {
		pr_debug("[NFI] Suspend twice !\n");
	}
#endif

	pr_debug("[NFI] Suspend !\n");
	return 0;
}

/******************************************************************************
 * mtk_nand_resume
 *
 * DESCRIPTION:
 *	 Resume the nand device!
 *
 * PARAMETERS:
 *	 struct platform_device *pdev : device structure
 *
 * RETURNS:
 *	 0 : Success
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static int mtk_nand_resume(struct platform_device *pdev)
{
	struct mtk_nand_host *host = platform_get_drvdata(pdev);
#if !defined(CONFIG_MTK_LEGACY)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		int ret = 0;
#endif
#endif

#ifdef CONFIG_PM

	if (host->saved_para.suspend_flag == 1) {
		/* restore NFI register */
#ifdef CONFIG_MTK_PMIC_MT6397
		hwPowerOn(MT65XX_POWER_LDO_VMCH, VOL_3300, "NFI");
#else
#if defined(CONFIG_MTK_LEGACY)
		hwPowerOn(MT6323_POWER_LDO_VMCH, VOL_3300, "NFI");
#else
#if !defined(CONFIG_FPGA_EARLY_PORTING)
		ret = regulator_set_voltage(mtk_nand_regulator, 3300000, 3300000);
		if (ret != 0)
			pr_err("[NFI] Resume regulator set vol failed: %d\n", ret);

		ret = regulator_enable(mtk_nand_regulator);
		if (ret != 0)
			pr_err("[NFI] Resume regulator_enable failed: %d\n", ret);
#endif
#endif
#endif
		udelay(200);
		pr_debug("[NFI] delay 200us for power on reset flow!\n");
		nand_prepare_clock();
		nand_enable_clock();
		DRV_WriteReg16(NFI_CNFG_REG16, host->saved_para.sNFI_CNFG_REG16);
		if (mtk_nfi_dev_comp->chip_ver == 1) {
			DRV_WriteReg16(NFI_PAGEFMT_REG16, host->saved_para.sNFI_PAGEFMT_REG16);
		} else if ((mtk_nfi_dev_comp->chip_ver == 2) || (mtk_nfi_dev_comp->chip_ver == 3)) {
			DRV_WriteReg32(NFI_PAGEFMT_REG32, host->saved_para.sNFI_PAGEFMT_REG32);
		} else {
			pr_err("[NFI] Resume ERROR, mtk_nfi_dev_comp->chip_ver=%d\n",
				mtk_nfi_dev_comp->chip_ver);
		}
		DRV_WriteReg32(NFI_CON_REG16, host->saved_para.sNFI_CON_REG16);
		DRV_WriteReg32(NFI_ACCCON_REG32, host->saved_para.sNFI_ACCCON_REG32);
		DRV_WriteReg16(NFI_IOCON_REG16, host->saved_para.sNFI_IOCON_REG16);
		DRV_WriteReg16(NFI_CSEL_REG16, host->saved_para.sNFI_CSEL_REG16);
		DRV_WriteReg16(NFI_DEBUG_CON1_REG16, host->saved_para.sNFI_DEBUG_CON1_REG16);

		/* restore ECC register */
		DRV_WriteReg32(ECC_ENCCNFG_REG32, host->saved_para.sECC_ENCCNFG_REG32);
		/* DRV_WriteReg32(ECC_FDMADDR_REG32, host->saved_para.sECC_FDMADDR_REG32); */
		DRV_WriteReg32(ECC_DECCNFG_REG32, host->saved_para.sECC_DECCNFG_REG32);

		/* Reset NFI and ECC state machine */
		/* Reset the state machine and data FIFO, because flushing FIFO */
		(void)mtk_nand_reset();
		/* Reset ECC */
		DRV_WriteReg16(ECC_DECCON_REG16, DEC_DE);
		while (!DRV_Reg16(ECC_DECIDLE_REG16))
			;

		DRV_WriteReg16(ECC_ENCCON_REG16, ENC_DE);
		while (!DRV_Reg32(ECC_ENCIDLE_REG32))
			;


		/* Initialize interrupt. Clear interrupt, read clear. */
		DRV_Reg16(NFI_INTR_REG16);

		DRV_WriteReg16(NFI_INTR_EN_REG16, host->saved_para.sNFI_INTR_EN_REG16);

		/* mtk_nand_interface_config(&host->mtd); */
		if (g_bSyncOrToggle) {
			NFI_CLN_REG32(NFI_DEBUG_CON1_REG16, HWDCM_SWCON_ON);
			NFI_CLN_REG32(NFI_DEBUG_CON1_REG16, NFI_BYPASS);
			NFI_CLN_REG32(ECC_BYPASS_REG32, ECC_BYPASS);
#ifndef CONFIG_MTK_FPGA
#if defined(CONFIG_MTK_LEGACY)
			/* DRV_WriteReg32(PERICFG_BASE + 0x5C, 0x0); */
			NFI_SET_REG32(PERI_NFI_CLK_SOURCE_SEL, NFI_PAD_1X_CLOCK);
#else
			clk_set_parent(nfi_bclk_sel, nfi_1xpad_clk);
#endif
#if defined(CONFIG_MTK_LEGACY)
			clkmux_sel(MT_MUX_ONFI, g_iNFI2X_CLKSRC, "NFI");
#else
			if (g_iNFI2X_CLKSRC == 0)
				clk_set_parent(onfi_sel_clk, onfi_26m_clk);
			else if (g_iNFI2X_CLKSRC == 1)
				clk_set_parent(onfi_sel_clk, onfi_mode5);
			else if (g_iNFI2X_CLKSRC == 2)
				clk_set_parent(onfi_sel_clk, onfi_mode4);
#endif
#endif
			DRV_WriteReg32(NFI_DLYCTRL_REG32, host->saved_para.sNFI_DLYCTRL_REG32);
#ifndef CONFIG_MTK_FPGA
			/* DRV_WriteReg32(PERI_NFI_MAC_CTRL, host->saved_para.sPERI_NFI_MAC_CTRL); */
#endif
			while (0 == (DRV_Reg32(NFI_STA_REG32) && STA_FLASH_MACRO_IDLE))
				;
			DRV_WriteReg16(NFI_NAND_TYPE_CNFG_REG32,
						host->saved_para.sNFI_NAND_TYPE_CNFG_REG32);
			DRV_WriteReg32(NFI_ACCCON1_REG3, host->saved_para.sNFI_ACCCON1_REG32);
		}

		mtk_nand_device_reset();

		nand_disable_clock();
		host->saved_para.suspend_flag = 0;
	} else {
		pr_debug("[NFI] Resume twice !\n");
	}
#endif
	pr_debug("[NFI] Resume !\n");
	return 0;
}

/******************************************************************************
 * mtk_nand_remove
 *
 * DESCRIPTION:
 *	 unregister the nand device file operations !
 *
 * PARAMETERS:
 *	 struct platform_device *pdev : device structure
 *
 * RETURNS:
 *	 0 : Success
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/

static int mtk_nand_remove(struct platform_device *pdev)
{
	struct mtk_nand_host *host = platform_get_drvdata(pdev);
	struct mtd_info *mtd = &host->mtd;

	nand_release(mtd);

	kfree(host);

	nand_disable_clock();
	nand_unprepare_clock();
	return 0;
}

static struct platform_driver mtk_nand_driver = {
	.probe = mtk_nand_probe,
	.remove = mtk_nand_remove,
	.suspend = mtk_nand_suspend,
	.resume = mtk_nand_resume,
	.driver = {
			.name = "mtk-nand",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mtk_nfi_of_match,
#endif
			},
};

/******************************************************************************
 * mtk_nand_init
 *
 * DESCRIPTION:
 *	 Init the device driver !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
#define SEQ_printf(m, x...)		\
do {			\
	if (m)			\
		seq_printf(m, x);	\
	else			\
		pr_debug(x);		\
} while (0)

int mtk_nand_proc_show(struct seq_file *m, void *v)
{
	int i;

	SEQ_printf(m, "ID:");
	for (i = 0; i < devinfo.id_length; i++)
		SEQ_printf(m, " 0x%x", devinfo.id[i]);

	SEQ_printf(m, "\n");
	SEQ_printf(m, "total size: %dMiB; part number: %s\n", devinfo.totalsize,
			devinfo.devciename);
	SEQ_printf(m, "Current working in %s mode\n", g_i4Interrupt ? "interrupt" : "polling");
	SEQ_printf(m, "NFI_ACCON = 0x%x\n", DRV_Reg32(NFI_ACCCON_REG32));
	SEQ_printf(m, "NFI_NAND_TYPE_CNFG_REG32= 0x%x\n", DRV_Reg32(NFI_NAND_TYPE_CNFG_REG32));
#ifdef CONFIG_MTK_FPGA
	SEQ_printf(m, "[FPGA Dummy]DRV_CFG_NFIA(0x0) = 0x0\n");
	SEQ_printf(m, "[FPGA Dummy]DRV_CFG_NFIB(0x0) = 0x0\n");
#else
	SEQ_printf(m, "DRV_CFG_NFIA = 0x%x\n", *((volatile u32 *)(GPIO_BASE + 0xC20)));
	SEQ_printf(m, "DRV_CFG_NFIB = 0x%x\n", *((volatile u32 *)(GPIO_BASE + 0xB50)));
#endif
	return 0;
}


static int mt_nand_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_nand_proc_show, inode->i_private);
}


static const struct file_operations mtk_nand_fops = {
	.open = mt_nand_proc_open,
	.write = mtk_nand_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init mtk_nand_init(void)
{
	struct proc_dir_entry *entry;

	g_i4Interrupt = 1;
	if (g_i4Interrupt)
		pr_debug("Enable IRQ for NFI module!\n");

	entry = proc_create(PROCNAME, 0664, NULL, &mtk_nand_fops);

	/* pr_debug("MediaTek Nand driver init, version %s\n", VERSION); */

	return platform_driver_register(&mtk_nand_driver);
}

/******************************************************************************
 * mtk_nand_exit
 *
 * DESCRIPTION:
 *	 Free the device driver !
 *
 * PARAMETERS:
 *	 None
 *
 * RETURNS:
 *	 None
 *
 * NOTES:
 *	 None
 *
 ******************************************************************************/
static void __exit mtk_nand_exit(void)
{
	pr_debug("MediaTek Nand driver exit, version %s\n", VERSION);

	platform_driver_unregister(&mtk_nand_driver);
	remove_proc_entry(PROCNAME, NULL);
}
late_initcall(mtk_nand_init);
module_exit(mtk_nand_exit);
/* MODULE_LICENSE("GPL"); */
