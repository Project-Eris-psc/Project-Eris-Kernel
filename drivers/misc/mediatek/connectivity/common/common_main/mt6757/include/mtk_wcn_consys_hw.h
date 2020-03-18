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

/*! \file
 *    \brief  Declaration of library functions
 *
 *   Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/

#ifndef _MTK_WCN_CONSYS_HW_H_
#define _MTK_WCN_CONSYS_HW_H_

#include <sync_write.h>
/* #include <mach/mt_reg_base.h> */
#include "wmt_plat.h"

/*device tree mode*/
#if CONFIG_OF
/* #if 1 */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>

struct CONSYS_BASE_ADDRESS {
	SIZE_T mcu_base;
	SIZE_T ap_rgu_base;
	SIZE_T topckgen_base;
	SIZE_T spm_base;
};

/*TOPCKGEN_BASE*/
#define CONSYS_TOP_CLKCG_CLR_OFFSET	0x00000084
#define CONSYS_TOP_CLKCG_SET_OFFSET	0x00000054
#define CONSYS_WD_SYS_RST_OFFSET		0x00000018
#define CONSYS_AP2CONN_OSC_EN_OFFSET	0x00001f00
#define CONSYS_EMI_MAPPING_OFFSET		0x00001380
/*AP_RGU_BASE*/
#define CONSYS_CPU_SW_RST_OFFSET		0x00000018
/*SPM_BASE*/
#define CONSYS_PWRON_CONFG_EN_OFFSET	0x00000000
#define CONSYS_TOP1_PWR_CTRL_OFFSET	0x0000032c
#define CONSYS_PWR_CONN_ACK_OFFSET		0x00000180
#define CONSYS_PWR_CONN_ACK_S_OFFSET	0x00000184
/*CONN_MCU_CONFIG_BASE*/
#define CONSYS_CHIP_ID_OFFSET			0x00000008
#define CONSYS_ROM_RAM_DELSEL_OFFSET	0x00000114
#define CONSYS_MCU_CFG_ACR_OFFSET		0x00000110
#define CONSYS_CPUPCR_OFFSET			0x00000160
#endif
/*AXI bus*/
#define CONSYS_TOPAXI_PROT_EN_OFFSET    0x1220
#define CONSYS_TOPAXI_PROT_STA1_OFFSET  0x1228
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*tag start:new platform need to make sure these define */
#define PLATFORM_SOC_CHIP 0x6757

#define CONSYS_BT_WIFI_SHARE_V33 0

#define CONSYS_PMIC_CTRL_ENABLE 1
#define CONSYS_EMI_MPU_SETTING 0
#define CONSYS_AHB_CLK_MAGEMENT 1
#define CONSYS_USE_PLATFORM_WRITE 1
#define CONSYS_PWR_ON_OFF_API_AVAILABLE 1
#define CONSYS_CLOCK_BUF_CTRL           1
#define CONSYS_AFE_REG_SETTING          1
/*tag end*/

#define CONSYS_SET_BIT(REG, BITVAL) (*((volatile UINT32 *)(REG)) |= ((UINT32)(BITVAL)))
#define CONSYS_CLR_BIT(REG, BITVAL) ((*(volatile UINT32 *)(REG)) &= ~((UINT32)(BITVAL)))
#define CONSYS_CLR_BIT_WITH_KEY(REG, BITVAL, KEY) {\
	UINT32 val = (*(volatile UINT32 *)(REG)); \
	val &= ~((UINT32)(BITVAL)); \
	val |= ((UINT32)(KEY)); \
	(*(volatile UINT32 *)(REG)) = val;\
}
#define CONSYS_REG_READ(addr) (*((volatile UINT32 *)(addr)))
#if CONSYS_USE_PLATFORM_WRITE
#define CONSYS_REG_WRITE(addr, data)  mt_reg_sync_writel(data, addr)
#else
#define CONSYS_REG_WRITE(addr, data)  (*((volatile UINT32 *)(addr)) = (UINT32)(data))
#endif

/*tag start: connsys register base address (hard code, no use) */
#define AP_RGU_BASE			0xF0007000
#define TOPCKGEN_BASE			0xF0000000
#define SPM_BASE				0xF0006000
#define CONN_MCU_CONFIG_BASE	0xF8070000
/*GIC Interrupt ID*/
#define MT_CONN2AP_BTIF_WAKEUP_IRQ_ID 270
/*tag end*/

/*connsys register offset define(hard code mode)*/
#if 1
	/*top clock gating control register */
#define CONSYS_TOP_CLKCG_CLR_REG		(TOPCKGEN_BASE + 0x00000084)
#define CONSYS_TOP_CLKCG_SET_REG		(TOPCKGEN_BASE + 0x00000054)
#define CONSYS_TOP_CLKCG_BIT			(0x1 << 26)

	/*SPM clock gating control register */
#define CONSYS_PWRON_CONFG_EN_REG		(SPM_BASE + 0x00000000)
#define CONSYS_PWRON_CONFG_EN_VALUE	(0x0b160001)
#define CONSYS_PWRON_CONFG_DIS_VALUE	(0x0b160000)
#endif

#define CONSYS_CPU_SW_RST_REG				(AP_RGU_BASE + 0x00000018)
#define CONSYS_TOP1_PWR_CTRL_REG			(SPM_BASE + 0x0000032c)
#define CONSYS_PWR_CONN_ACK_REG			(SPM_BASE + 0x00000180)
#define CONSYS_PWR_CONN_ACK_S_REG			(SPM_BASE + 0x00000184)

#define CONSYS_WD_SYS_RST_REG				(TOPCKGEN_BASE + 0x00000018)
#define CONSYS_CHIP_ID_REG					(CONN_MCU_CONFIG_BASE + 0x00000008)
#define CONSYS_ROM_RAM_DELSEL_REG			(CONN_MCU_CONFIG_BASE + 0x00000114)
#define CONSYS_MCU_CFG_ACR_REG				(CONN_MCU_CONFIG_BASE + 0x00000110)

#if CONSYS_AFE_REG_SETTING
#define CONSYS_AFE_REG_BASE					(0x180B2000)
#define CONSYS_AFE_REG_WBG_AFE_01_OFFSET    (0x00000010)
#define CONSYS_AFE_REG_WBG_AFE_01_VALUE	    (0x00000001)
#define CONSYS_AFE_REG_WBG_PLL_03_OFFSET    (0x00000038)
#define CONSYS_AFE_REG_WBG_PLL_03_VALUE	    (0x000C15F0)
#define CONSYS_AFE_REG_WBG_PLL_05_OFFSET    (0x00000040)
#define CONSYS_AFE_REG_WBG_PLL_05_VALUE	    (0x07900020)
#define CONSYS_AFE_REG_WBG_WB_TX_01_OFFSET	(0x00000088)
#define CONSYS_AFE_REG_WBG_WB_TX_01_VALUE	(0x08440000)
#endif

#define CONSYS_TOPAXI_PROT_EN				(TOPCKGEN_BASE + 0x1220)
#define CONSYS_TOPAXI_PROT_STA1				(TOPCKGEN_BASE + 0x1228)
#define CONSYS_PROT_MASK				((0x1<<13) | (0x1<<14))	/* bit 13, 14 */
/*CONSYS_CPU_SW_RST_REG*/
#define CONSYS_CPU_SW_RST_BIT				(0x1 << 12)
#define CONSYS_CPU_SW_RST_CTRL_KEY			(0x88 << 24)

/*CONSYS_TOP1_PWR_CTRL_REG*/
#define CONSYS_SPM_PWR_RST_BIT				(0x1 << 0)
#define CONSYS_SPM_PWR_ISO_S_BIT			(0x1 << 1)
#define CONSYS_SPM_PWR_ON_BIT				(0x1 << 2)
#define CONSYS_SPM_PWR_ON_S_BIT			(0x1 << 3)
#define CONSYS_CLK_CTRL_BIT				(0x1 << 4)
#define CONSYS_SRAM_CONN_PD_BIT			(0x1 << 8)

/*CONSYS_PWR_CONN_ACK_REG*/
#define CONSYS_PWR_ON_ACK_BIT				(0x1 << 1)

/*CONSYS_PWR_CONN_ACK_S_REG*/
#define CONSYS_PWR_CONN_ACK_S_BIT			(0x1 << 1)

/*CONSYS_WD_SYS_RST_REG*/
#define CONSYS_WD_SYS_RST_CTRL_KEY			(0x88 << 24)
#define CONSYS_WD_SYS_RST_BIT				(0x1 << 9)

/*CONSYS_MCU_CFG_ACR_REG*/
#define CONSYS_MCU_CFG_ACR_MBIST_BIT		(0x1 << 18)

/* EMI part mapping & ctrl*/
#define KBYTE								(1024*sizeof(char))
#define CONSYS_EMI_AP_PHY_OFFSET			(0x80000)
#define CONSYS_EMI_AP_PHY_BASE				(0x80080000)
#define CONSYS_EMI_FW_PHY_BASE				(0xf0080000)
#define CONSYS_EMI_MEM_SIZE					(343*KBYTE)	/*coredump space , 343K is enough */
#define CONSYS_EMI_PAGED_TRACE_OFFSET		(0x400)
#define CONSYS_EMI_PAGED_DUMP_OFFSET		(0x8400)
#define CONSYS_EMI_FULL_DUMP_OFFSET			(0x10400)

/*cpupcr*/
#define CONSYS_CPUPCR_REG					(CONN_MCU_CONFIG_BASE + 0x00000160)
/*emi mapping*/
#define CONSYS_EMI_MAPPING					(TOPCKGEN_BASE + CONSYS_EMI_MAPPING_OFFSET)

/*control app2cnn_osc_en*/
#define CONSYS_AP2CONN_OSC_EN_REG			(TOPCKGEN_BASE + 0x00001800)
#define CONSYS_AP2CONN_OSC_EN_BIT			(0x1 << 10)
#define CONSYS_AP2CONN_WAKEUP_BIT			(0x1 << 9)

/*paged dump address start*/
#define CONSYS_PAGED_DUMP_START_ADDR		(0xf0088400)
#define CONSYS_PAGED_DUMP_SIZE				(32*KBYTE)

/*full dump address start*/
#define CONSYS_FULL_DUMP_START_ADDR			(0xf0090400)
#define CONSYS_FULL_DUMP_DLM_LEN			(0x1f000)
#define CONSYS_FULL_DUMP_SYSB2_START		(CONSYS_FULL_DUMP_START_ADDR + CONSYS_FULL_DUMP_DLM_LEN)
#define CONSYS_FULL_DUMP_SYSB2_LEN			(0x6800)
#define CONSYS_FULL_DUMP_SYSB3_START		(CONSYS_FULL_DUMP_SYSB2_START + CONSYS_FULL_DUMP_SYSB2_LEN)
#define CONSYS_FULL_DUMP_SYSB3_LEN			(0x16800)

/*force fw assert pattern*/
#define EXP_APMEM_HOST_OUTBAND_ASSERT_MAGIC_W1   (0x19b30bb1)

#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
#define PMIC_DCXO_CW15						0x0D46
#define PMIC_DCXO_CW16						0x0D48
#define PMIC_DCXO_CW15_VAL					0x18
#define AP_CONSYS_NOCO_CLOCK_BITA			(0x1 << 11)
#define AP_CONSYS_NOCO_CLOCK_BITB			(0x1 << 13)
#define AP_CONSYS_CO_CLOCK_BITA				(0x1 << 10)
#define AP_CONSYS_CO_CLOCK_BITB				(0x1 << 12)
#else
#define PMIC_DCXO_CW15						0x701E
#define PMIC_DCXO_CW16						0x7000
#define PMIC_DCXO_CW15_VAL					0x7
#define AP_CONSYS_NOCO_CLOCK_BITA			(0x1 << 7)
#define AP_CONSYS_NOCO_CLOCK_BITB			(0x1 << 9)
#define AP_CONSYS_CO_CLOCK_BITA				(0x1 << 6)
#define AP_CONSYS_CO_CLOCK_BITB				(0x1 << 8)
#endif

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef enum _ENUM_EMI_CTRL_STATE_OFFSET_ {
	EXP_APMEM_CTRL_STATE = 0x0,
	EXP_APMEM_CTRL_HOST_SYNC_STATE = 0x4,
	EXP_APMEM_CTRL_HOST_SYNC_NUM = 0x8,
	EXP_APMEM_CTRL_CHIP_SYNC_STATE = 0xc,
	EXP_APMEM_CTRL_CHIP_SYNC_NUM = 0x10,
	EXP_APMEM_CTRL_CHIP_SYNC_ADDR = 0x14,
	EXP_APMEM_CTRL_CHIP_SYNC_LEN = 0x18,
	EXP_APMEM_CTRL_CHIP_PRINT_BUFF_START = 0x1c,
	EXP_APMEM_CTRL_CHIP_PRINT_BUFF_LEN = 0x20,
	EXP_APMEM_CTRL_CHIP_PRINT_BUFF_IDX = 0x24,
	EXP_APMEM_CTRL_CHIP_INT_STATUS = 0x28,
	EXP_APMEM_CTRL_CHIP_PAGED_DUMP_END = 0x2c,
	EXP_APMEM_CTRL_HOST_OUTBAND_ASSERT_W1 = 0x30,
	EXP_APMEM_CTRL_CHIP_PAGE_DUMP_NUM = 0x44,
	EXP_APMEM_CTRL_CHIP_FW_DBGLOG_MODE = 0x40,
	EXP_APMEM_CTRL_CHIP_DYNAMIC_DUMP = 0x48,
	EXP_APMEM_CTRL_MAX
} ENUM_EMI_CTRL_STATE_OFFSET, *P_ENUM_EMI_CTRL_STATE_OFFSET;
extern struct CONSYS_BASE_ADDRESS conn_reg;
#ifdef CONSYS_BT_WIFI_SHARE_V33
typedef struct _BT_WIFI_V33_STATUS_ {
	UINT32 counter;
	UINT32 flags;
	spinlock_t lock;
} BT_WIFI_V33_STATUS;

#endif
#if CONSYS_BT_WIFI_SHARE_V33
extern BT_WIFI_V33_STATUS gBtWifiV33;
#endif

/*******************************************************************************
*                            P U B L I C   D A T A
*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
extern INT32 mtk_wcn_consys_hw_init(VOID);
extern INT32 mtk_wcn_consys_hw_deinit(VOID);
extern INT32 mtk_wcn_consys_hw_pwr_off(UINT32 co_clock_en);
extern INT32 mtk_wcn_consys_hw_pwr_on(UINT32 co_clock_en);
extern INT32 mtk_wcn_consys_hw_rst(UINT32 co_clock_en);
extern INT32 mtk_wcn_consys_hw_bt_paldo_ctrl(UINT32 enable);
extern INT32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT32 enable);
extern INT32 mtk_wcn_consys_hw_vcn28_ctrl(UINT32 enable);
extern INT32 mtk_wcn_consys_hw_state_show(VOID);
extern UINT8 *mtk_wcn_consys_emi_virt_addr_get(UINT32 ctrl_state_offset);
#if CONSYS_ENALBE_SET_JTAG
extern UINT32 mtk_wcn_consys_jtag_flag_ctrl(UINT32 en);
#endif
extern UINT32 mtk_wcn_consys_soc_chipid(VOID);
#if !defined(CONFIG_MTK_LEGACY)
extern struct pinctrl *mtk_wcn_consys_get_pinctrl(VOID);
#endif
extern INT32 mtk_wcn_consys_co_clock_type(VOID);
extern INT32 mtk_wcn_consys_set_dbg_mode(UINT32 flag);
extern INT32 mtk_wcn_consys_set_dynamic_dump(PUINT32 buf);
#endif /* _MTK_WCN_CMB_HW_H_ */
