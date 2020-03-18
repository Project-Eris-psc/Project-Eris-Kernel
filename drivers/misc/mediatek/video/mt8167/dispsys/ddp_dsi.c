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

#define LOG_TAG "DSI"

#include <linux/delay.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <debug.h>
#include "disp_drv_log.h"
#include "disp_drv_platform.h"

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
/* #include <mach/irqs.h> */
#include "mtkfb.h"
#include "ddp_drv.h"
#include "ddp_manager.h"
#include "ddp_dump.h"
#include "ddp_irq.h"
#include "ddp_dsi.h"
#include "ddp_log.h"
#if defined(CONFIG_MTK_LEGACY)
#include <mt-plat/mt_gpio.h>
/* #include <cust_gpio_usage.h> */
#else
#include "disp_dts_gpio.h"
#endif
#include "ddp_mmp.h"
#include "primary_display.h"
#include "debug.h"
#include "ddp_reg.h"
#include "ddp_path.h"

#ifdef BUILD_UBOOT
#define ENABLE_DSI_INTERRUPT 0
#else
#define ENABLE_DSI_INTERRUPT 1
#endif


/* static unsigned int _dsi_reg_update_wq_flag = 0; */
static DECLARE_WAIT_QUEUE_HEAD(_dsi_reg_update_wq);
atomic_t PMaster_enable = ATOMIC_INIT(0);
#include "debug.h"
#include <mt-plat/sync_write.h>
#include "ddp_reg.h"
#include "ddp_dsi.h"

/*...below is new dsi driver...*/
#define IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII 0
static int dsi_reg_op_debug;
#include <mt-plat/sync_write.h>

#define DSI_OUTREG32(cmdq, addr, val) DISP_REG_SET(cmdq, addr, val)
#define DSI_BACKUPREG32(cmdq, hSlot, idx, addr) DISP_REG_BACKUP(cmdq, hSlot, idx, addr)
#define DSI_POLLREG32(cmdq, addr, mask, value) DISP_REG_CMDQ_POLLING(cmdq, addr, value, mask)

#define DSI_MASKREG32(cmdq, REG, MASK, VALUE)	DISP_REG_MASK((cmdq), (REG), (VALUE), (MASK))

#define DSI_OUTREGBIT(cmdq, TYPE, REG, bit, value)  \
	do {\
		TYPE r;\
		TYPE v;\
		if (cmdq) {\
			*(unsigned int *)(&r) = ((unsigned int)0x00000000);\
			r.bit = ~(r.bit);\
			*(unsigned int *)(&v) = ((unsigned int)0x00000000);\
			v.bit = value; DISP_REG_MASK(cmdq, &REG, AS_UINT32(&v), AS_UINT32(&r)); \
		} else {	\
			mt_reg_sync_writel(INREG32(&REG), &r); \
			r.bit = (value);\
			DISP_REG_SET(cmdq, &REG, INREG32(&r)); }				\
	} while (0)


#ifdef CONFIG_FPGA_EARLY_PORTING
#define MIPITX_Write60384(slave_addr, write_addr, write_data)			\
{	\
	DDPMSG("MIPITX_Write60384:0x%x,0x%x,0x%x\n", slave_addr, write_addr, write_data);		\
	mt_reg_sync_writel(0x2, MIPITX_BASE+0x14);		\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x18);		\
	mt_reg_sync_writel(((unsigned int)slave_addr << 0x1), MIPITX_BASE+0x04);		\
	mt_reg_sync_writel(write_addr, MIPITX_BASE+0x0);		\
	mt_reg_sync_writel(write_data, MIPITX_BASE+0x0);		\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x24);		\
	while (INREG32(MIPITX_BASE+0xC)&0x1 != 0x1)\
		;		\
	mt_reg_sync_writel(0xFF, MIPITX_BASE+0xC);		\
	\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x14);		\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x18);		\
	mt_reg_sync_writel(((unsigned int)slave_addr << 0x1), MIPITX_BASE+0x04);		\
	mt_reg_sync_writel(write_addr, MIPITX_BASE+0x0);		\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x24);		\
	while (INREG32(MIPITX_BASE+0xC)&0x1 != 0x1)\
		;		\
	mt_reg_sync_writel(0xFF, MIPITX_BASE+0xC);		\
	\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x14);		\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x18);		\
	mt_reg_sync_writel(((unsigned int)slave_addr << 0x1)+1, MIPITX_BASE+0x04);		\
	mt_reg_sync_writel(0x1, MIPITX_BASE+0x24);		\
	while (INREG32(MIPITX_BASE+0xC)&0x1 != 0x1)\
		;		\
	mt_reg_sync_writel(0xFF, MIPITX_BASE+0xC);		\
	DDPMSG("MIPI write data = 0x%x, read data = 0x%x\n", write_data, INREG32(MIPITX_BASE));		\
	if (INREG32(MIPITX_BASE) == write_data)\
		DDPMSG("MIPI write success\n");		\
	else\
		DDPMSG("MIPI write fail\n");		\
}

#define MIPITX_INREG32(addr)								\
	({										\
		unsigned int val = 0;							\
		if (0)\
			val = INREG32(addr);						\
		if (dsi_reg_op_debug) {							\
			DDPMSG("[mipitx/inreg]%p=0x%08x\n", (void *)addr, val);	\
		}									\
		val;									\
	})

#define MIPITX_OUTREG32(addr, val) \
	{\
		if (dsi_reg_op_debug) \
			DDPMSG("[mipitx/reg]%p=0x%08x\n", (void *)addr, val); \
		if (0)\
			mt_reg_sync_writel(val, addr); }

#define MIPITX_OUTREGBIT(TYPE, REG, bit, value)  \
	{\
		do {	\
			TYPE r;\
			if (0)\
				mt_reg_sync_writel(INREG32(&REG), &r);	  \
			*(unsigned long *)(&r) = ((unsigned long)0x00000000);	  \
			r.bit = value;	  \
			MIPITX_OUTREG32(&REG, AS_UINT32(&r));	  \
			} while (0);\
	}

#define MIPITX_MASKREG32(x, y, z)  MIPITX_OUTREG32(x, (MIPITX_INREG32(x)&~(y))|(z))
#else
#define MIPITX_INREG32(addr)								\
	({										\
		unsigned int val = 0;							\
		val = INREG32(addr);							\
		if (dsi_reg_op_debug)\
			DDPMSG("[mipitx/inreg]%p=0x%08x\n", (void *)addr, val);	\
		val;									\
	})

#define MIPITX_OUTREG32(addr, val) \
	{\
		if (dsi_reg_op_debug)\
			DDPMSG("[mipitx/reg]%p=0x%08x\n", (void *)addr, val);\
		mt_reg_sync_writel(val, addr);\
	}

#define MIPITX_OUTREGBIT(TYPE, REG, bit, value)  \
	{\
		do {	\
			TYPE r;\
			mt_reg_sync_writel(INREG32(&REG), &r);	  \
			r.bit = value;	  \
			MIPITX_OUTREG32(&REG, AS_UINT32(&r));	  \
			} while (0);\
	}

#define MIPITX_MASKREG32(x, y, z)  MIPITX_OUTREG32(x, (MIPITX_INREG32(x)&~(y))|(z))
#endif

#define DSI_INREG32(type, addr) INREG32(addr)

#define DSI_READREG32(type, dst, src) mt_reg_sync_writel(INREG32(src), dst)


struct t_dsi_context {
	unsigned int lcm_width;
	unsigned int lcm_height;
	struct cmdqRecStruct **handle;
	bool enable;

	volatile struct DSI_REGS regBackup;
	unsigned int cmdq_size;
	LCM_DSI_PARAMS dsi_params;
};

struct t_dsi_context _dsi_context[DSI_INTERFACE_NUM];
#define DSI_MODULE_BEGIN(x)		0	/* (x == DISP_MODULE_DSIDUAL?0:DSI_MODULE_to_ID(x)) */
#define DSI_MODULE_END(x)		0	/* (x == DISP_MODULE_DSIDUAL?1:DSI_MODULE_to_ID(x)) */
#define DSI_MODULE_to_ID(x)		0	/* (x == DISP_MODULE_DSI0?0:1) */
#define DIFF_CLK_LANE_LP (0x10)

struct DSI_REGS *DSI_REG[2];
struct DSI_PHY_REGS *DSI_PHY_REG[2];
struct DSI_CMDQ_REGS *DSI_CMDQ_REG[2];
struct DSI_VM_CMDQ_REGS *DSI_VM_CMD_REG[2];

static wait_queue_head_t _dsi_cmd_done_wait_queue[2];
static wait_queue_head_t _dsi_dcs_read_wait_queue[2];
static wait_queue_head_t _dsi_wait_bta_te[2];
static wait_queue_head_t _dsi_wait_ext_te[2];
static wait_queue_head_t _dsi_wait_vm_done_queue[2];
static wait_queue_head_t _dsi_wait_vm_cmd_done_queue[2];
static wait_queue_head_t _dsi_wait_sleep_out_done_queue[2];
static bool waitRDDone;
static bool wait_vm_done_irq;
static bool wait_vm_cmd_done;
static bool wait_sleep_out_done;
static int s_isDsiPowerOn;
static int dsi_currect_mode;
static int dsi_force_config;
static bool dsi_glitch_enable;
LCM_DSI_PARAMS dsi_lcm_params;
struct cmdqRecStruct *cmdq_forcb;

static void _DSI_INTERNAL_IRQ_Handler(enum DISP_MODULE_ENUM module, unsigned int param)
{
	int i = 0;
	struct DSI_INT_STATUS_REG status;
	struct DSI_TXRX_CTRL_REG txrx_ctrl;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		status = *(struct DSI_INT_STATUS_REG *) &param;

		DDPMSG("_DSI_INTERNAL_IRQ_Handler 0x%x\n", *((unsigned int *)(&status)));

		if (status.RD_RDY) {
			waitRDDone = true;
			wake_up_interruptible(&_dsi_dcs_read_wait_queue[i]);
		}

		if (status.CMD_DONE) {
			wake_up_interruptible(&_dsi_cmd_done_wait_queue[i]);
		}

		if (status.TE_RDY) {
			DSI_OUTREG32(NULL, &txrx_ctrl, INREG32(&DSI_REG[0]->DSI_TXRX_CTRL));
			if (txrx_ctrl.EXT_TE_EN == 1) {
				wake_up_interruptible(&_dsi_wait_ext_te[i]);
			} else {
				wake_up_interruptible(&_dsi_wait_bta_te[i]);
			}
		}

		if (status.VM_DONE) {
			wake_up_interruptible(&_dsi_wait_vm_done_queue[i]);
		}

		if (status.VM_CMD_DONE) {
			wait_vm_cmd_done = true;
			wake_up_interruptible(&_dsi_wait_vm_cmd_done_queue[i]);
		}

		if (status.SLEEPOUT_DONE) {
			wait_sleep_out_done = true;
			wake_up_interruptible(&_dsi_wait_sleep_out_done_queue[i]);
		}
	}
}


static enum DSI_STATUS DSI_Reset(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	int i = 0;
	unsigned int irq_en[2];
	/* DSI_RESET Protect: backup & disable dsi interrupt */
	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		irq_en[i] = AS_UINT32(&DSI_REG[i]->DSI_INTEN);
		DSI_OUTREG32(NULL, &DSI_REG[i]->DSI_INTEN, 0);
		DISPMSG("\nDSI_RESET backup dsi%d irq:0x%08x ", i, irq_en[i]);
	}

	/* do reset */
	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		DSI_OUTREGBIT(cmdq, struct DSI_COM_CTRL_REG, DSI_REG[i]->DSI_COM_CTRL, DSI_RESET, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_COM_CTRL_REG, DSI_REG[i]->DSI_COM_CTRL, DSI_RESET, 0);
	}

	/* DSI_RESET Protect: restore dsi interrupt */
	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		DSI_OUTREG32(NULL, &DSI_REG[i]->DSI_INTEN, irq_en[i]);
		DISPMSG("\nDSI_RESET restore dsi%d irq:0x%08x ", i,
			AS_UINT32(&DSI_REG[i]->DSI_INTEN));
	}
	return DSI_STATUS_OK;
}

static int _dsi_is_video_mode(enum DISP_MODULE_ENUM module)
{
	int i = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (DSI_REG[i]->DSI_MODE_CTRL.MODE == CMD_MODE)
			return 0;
		else
			return 1;
	}
	return 0;
}

static enum DSI_STATUS DSI_SetMode(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, unsigned int mode)
{
	int i = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++)
		DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG, DSI_REG[i]->DSI_MODE_CTRL, MODE, mode);

	return DSI_STATUS_OK;
}

static enum DSI_STATUS DSI_SetSwitchMode(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, unsigned int mode)
{
	int i = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (mode == 0) {
			/* DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG,DSI_REG[i]->DSI_MODE_CTRL,C2V_SWITCH_ON,0); */
			DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG, DSI_REG[i]->DSI_MODE_CTRL,
				      V2C_SWITCH_ON, 1);
		} else {
			/* DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG,DSI_REG[i]->DSI_MODE_CTRL,V2C_SWITCH_ON,0); */
			DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG, DSI_REG[i]->DSI_MODE_CTRL,
				      C2V_SWITCH_ON, 1);
		}
	}

	return DSI_STATUS_OK;
}

enum DSI_STATUS DSI_DisableClk(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	return DSI_STATUS_OK;
}

void DSI_sw_clk_trail(int module_idx)
{
	DEFINE_SPINLOCK(s_lock);
	unsigned long flags;

	register unsigned long *SW_CTRL_CON0_addr;

	DDPMSG("DSI_sw_clk_trail starts\n");

	/* init DSI clk lane software control */
	DISP_REG_SET(NULL, &DSI_REG[module_idx]->DSI_PHY_LCCON, 0x00000001);
	DISP_REG_SET(NULL, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON0, 0x00000031);
	DISP_REG_SET(NULL, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON1, 0x0F0F0F0F);
	DISP_REG_SET(NULL, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL, 0x00000001);
	/* control DSI clk trail duration */
	SW_CTRL_CON0_addr = (unsigned long *)(&DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON0);
	spin_lock_irqsave(&s_lock, flags);
	/* force clk lane keep low */
	mt_reg_sync_writel(0x00000071, (volatile unsigned long *)SW_CTRL_CON0_addr);
	/* pull clk lane to LP state */
	mt_reg_sync_writel(0x0000000F, (volatile unsigned long *)SW_CTRL_CON0_addr);
	spin_unlock_irqrestore(&s_lock, flags);
	/* give back DSI clk lane control */
	DISP_REG_SET(NULL, &DSI_REG[module_idx]->DSI_PHY_LCCON, 0x00000000);
	DDP_REG_POLLING(&DSI_REG[module_idx]->DSI_STATE_DBG0, 0x00010000);
	DISP_REG_SET(NULL, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL, 0x00000000);
}

void DSI_sw_clk_trail_cmdq(int module_idx, struct cmdqRecStruct *cmdq)
{
	/* init DSI clk lane software control */
	DISP_REG_SET(cmdq, &DSI_REG[module_idx]->DSI_PHY_LCCON, 0x00000001);
	DISP_REG_SET(cmdq, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON0, 0x31);
	DISP_REG_SET(cmdq, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON1, 0x0F0F0F0F);
	DISP_REG_SET(cmdq, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL, 0x01);

	/* control DSI clk trail duration */
	/* force clk lane keep low */
	DISP_REG_SET(cmdq, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON0, 0x71);
	/* pull clk lane to LP state */
	DISP_REG_SET(cmdq, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL_CON0, 0xF);

	/* /give back DSI clk lane control */
	DISP_REG_SET(cmdq, &DSI_REG[module_idx]->DSI_PHY_LCCON, 0x00000000);
	DISP_REG_CMDQ_POLLING(cmdq, &DSI_REG[module_idx]->DSI_STATE_DBG0, 0x10000, 0x10000);
	DISP_REG_SET(cmdq, &DSI_PHY_REG[module_idx]->MIPITX_DSI_SW_CTRL, 0x00000000);
}

void DSI_lane0_ULP_mode(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, bool enter)
{
	int i = 0;

	ASSERT(cmdq == NULL);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enter) {
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LD0CON_REG, DSI_REG[i]->DSI_PHY_LD0CON,
				      L0_HS_TX_EN, 0);
			mdelay(1);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LD0CON_REG, DSI_REG[i]->DSI_PHY_LD0CON,
				      L0_ULPM_EN, 0);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LD0CON_REG, DSI_REG[i]->DSI_PHY_LD0CON,
				      L0_ULPM_EN, 1);
			mdelay(1);
		} else {
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LD0CON_REG, DSI_REG[i]->DSI_PHY_LD0CON,
				      L0_ULPM_EN, 0);
			mdelay(1);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LD0CON_REG, DSI_REG[i]->DSI_PHY_LD0CON,
				      L0_WAKEUP_EN, 1);
			mdelay(1);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LD0CON_REG, DSI_REG[i]->DSI_PHY_LD0CON,
				      L0_WAKEUP_EN, 0);
			mdelay(1);
		}
	}
}


void DSI_clk_ULP_mode(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, bool enter)
{
	int i = 0;

	ASSERT(cmdq == NULL);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enter) {
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_ULPM_EN, 0);
			mdelay(1);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_ULPM_EN, 1);
			mdelay(1);
		} else {
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_ULPM_EN, 0);
			mdelay(1);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_WAKEUP_EN, 1);
			mdelay(1);
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_WAKEUP_EN, 0);
			mdelay(1);
		}
	}
}

bool DSI_clk_HS_state(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	int i = 0;
	struct DSI_PHY_LCCON_REG tmpreg;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		DSI_READREG32((struct DSI_PHY_LCCON_REG *), &tmpreg, &DSI_REG[i]->DSI_PHY_LCCON);
		return tmpreg.LC_HS_TX_EN ? true : false;
	}
	return false;
}

void DSI_clk_HSLP_mode(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	int i = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (cmdq)
			DSI_sw_clk_trail_cmdq(i, cmdq);
		else
			DSI_sw_clk_trail(i);
	}
}

void DSI_manual_enter_HS(struct cmdqRecStruct *cmdq)
{
	DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[0]->DSI_PHY_LCCON, LC_HS_TX_EN, 1);
}

void DSI_clk_HS_mode(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, bool enter)
{
	int i = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enter) {
			/* && !DSI_clk_HS_state(i, cmdq)) */
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_HS_TX_EN, 1);
		} else if (!enter) {
			DSI_OUTREGBIT(cmdq, struct DSI_PHY_LCCON_REG, DSI_REG[i]->DSI_PHY_LCCON,
				      LC_HS_TX_EN, 0);
		}
	}
}

const char *_dsi_cmd_mode_parse_state(unsigned int state)
{
	switch (state) {
	case 0x0001:
		return "idle";
	case 0x0002:
		return "Reading command queue for header";
	case 0x0004:
		return "Sending type-0 command";
	case 0x0008:
		return "Waiting frame data from RDMA for type-1 command";
	case 0x0010:
		return "Sending type-1 command";
	case 0x0020:
		return "Sending type-2 command";
	case 0x0040:
		return "Reading command queue for data";
	case 0x0080:
		return "Sending type-3 command";
	case 0x0100:
		return "Sending BTA";
	case 0x0200:
		return "Waiting RX-read data ";
	case 0x0400:
		return "Waiting SW RACK for RX-read data";
	case 0x0800:
		return "Waiting TE";
	case 0x1000:
		return "Get TE ";
	case 0x2000:
		return "Waiting SW RACK for TE";
	case 0x4000:
		return "Waiting external TE";
	default:
		return "unknown";
	}
	return "unknown";
}

enum DSI_STATUS DSI_DumpRegisters(enum DISP_MODULE_ENUM module, int level)
{
	uint32_t i;
	/*
	*
	   description of dsi status
	   Bit Value   Description
	   [0] 0x0001  Idle (wait for command)
	   [1] 0x0002  Reading command queue for header
	   [2] 0x0004  Sending type-0 command
	   [3] 0x0008  Waiting frame data from RDMA for type-1 command
	   [4] 0x0010  Sending type-1 command
	   [5] 0x0020  Sending type-2 command
	   [6] 0x0040  Reading command queue for data
	   [7] 0x0080  Sending type-3 command
	   [8] 0x0100  Sending BTA
	   [9] 0x0200  Waiting RX-read data
	   [10]    0x0400  Waiting SW RACK for RX-read data
	   [11]    0x0800  Waiting TE
	   [12]    0x1000  Get TE
	   [13]    0x2000  Waiting external TE
	   [14]    0x4000  Waiting SW RACK for TE

	 */
	static const char *const DSI_DBG_STATUS_DESCRIPTION[] = {
		"null",
		"Idle (wait for command)",
		"Reading command queue for header",
		"Sending type-0 command",
		"Waiting frame data from RDMA for type-1 command",
		"Sending type-1 command",
		"Sending type-2 command",
		"Reading command queue for data",
		"Sending type-3 command",
		"Sending BTA",
		"Waiting RX-read data ",
		"Waiting SW RACK for RX-read data",
		"Waiting TE",
		"Get TE ",
		"Waiting external TE",
		"Waiting SW RACK for TE",
	};
	unsigned int DSI_DBG6_Status = (DISP_REG_GET(DSI_REG[0] + 0x160)) & 0xffff;
	/* unsigned int DSI_DBG6_Status_bak = DSI_DBG6_Status; */
	int count = 0;

	while (DSI_DBG6_Status) {
		DSI_DBG6_Status >>= 1;
		count++;
	}
	/* while((1<<count) != DSI_DBG6_Status) count++; */
	/* count++; */
	DDPMSG("---------- Start dump DSI registers ----------\n");
	DDPMSG("DSI_STATE_DBG6=0x%08x, count=%d, means: [%s]\n",
		       DSI_DBG6_Status, count, DSI_DBG_STATUS_DESCRIPTION[count]);

	DDPMSG("---------- Start dump DSI registers ----------\n");

	for (i = 0; i < sizeof(struct DSI_REGS); i += 16) {
		DDPMSG("DSI+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n", i,
			       DISP_REG_GET(DSI_REG[0] + i), DISP_REG_GET(DSI_REG[0] + i + 0x4),
			       DISP_REG_GET(DSI_REG[0] + i + 0x8),
			       DISP_REG_GET(DSI_REG[0] + i + 0xc));
	}

	for (i = 0; i < sizeof(struct DSI_CMDQ_REGS); i += 16) {
		DDPMSG("DSI_CMD+%04x : 0x%08x  0x%08x  0x%08x  0x%08x\n", i,
			       DISP_REG_GET((DSI_REG[0] + 0x180 + i)),
			       DISP_REG_GET((DSI_REG[0] + 0x180 + i + 0x4)),
			       DISP_REG_GET((DSI_REG[0] + 0x180 + i + 0x8)),
			       DISP_REG_GET((DSI_REG[0] + 0x180 + i + 0xc)));
	}

	for (i = 0; i < sizeof(struct DSI_PHY_REGS); i += 16) {
		DDPMSG("DSI_PHY+%04x : 0x%08x    0x%08x  0x%08x  0x%08x\n", i,
			       DISP_REG_GET((DSI_PHY_REG[0] + i)),
			       DISP_REG_GET((DSI_PHY_REG[0] + i + 0x4)),
			       DISP_REG_GET((DSI_PHY_REG[0] + i + 0x8)),
			       DISP_REG_GET((DSI_PHY_REG[0] + i + 0xc)));
	}

	return DSI_STATUS_OK;
}

int DSI_WaitVMDone(enum DISP_MODULE_ENUM module)
{
	int i = 0;
	static const long WAIT_TIMEOUT = 2 * HZ;	/* 2 sec */
	int ret = 0;

	/*...dsi video is always in busy state... */
	if (_dsi_is_video_mode(module)) {
		DDPERR("DSI_WaitVMDone error: should set DSI to CMD mode firstly\n");
		return -1;
	}

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		ret =
		    wait_event_interruptible_timeout(_dsi_wait_vm_done_queue[i],
						     !(DSI_REG[i]->DSI_INTSTA.BUSY), WAIT_TIMEOUT);
		if (ret == 0) {
			DISPERR("dsi wait VM done  timeout\n");
			DSI_DumpRegisters(module, 1);
			DSI_Reset(module, NULL);
			return -1;
		}
	}
	return 0;
}

enum DSI_STATUS DSI_SleepOut(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG, DSI_REG[0]->DSI_MODE_CTRL, SLEEP_MODE, 1);
	DSI_OUTREGBIT(cmdq, struct DSI_PHY_TIMCON4_REG,
		DSI_REG[0]->DSI_PHY_TIMECON4, ULPS_WAKEUP, 0x22E09);  /* cycle to 1ms for 520MHz */

	return DSI_STATUS_OK;
}


enum DSI_STATUS DSI_Wakeup(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, SLEEPOUT_START, 0);
	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, SLEEPOUT_START, 1);
	mdelay(1);

	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, SLEEPOUT_START, 0);
	DSI_OUTREGBIT(cmdq, struct DSI_MODE_CTRL_REG, DSI_REG[0]->DSI_MODE_CTRL, SLEEP_MODE, 0);

	return DSI_STATUS_OK;
}

enum DSI_STATUS DSI_BackupRegisters(enum DISP_MODULE_ENUM module, void *cmdq)
{
	volatile struct DSI_REGS *regs = NULL;

	regs = &(_dsi_context[0].regBackup);

	/* memcpy((void*)&(_dsiContext.regBackup),*/
	/*(void*)DSI_REG[0], sizeof(struct DSI_REGS)); */

	DSI_OUTREG32(cmdq, &regs->DSI_INTEN, AS_UINT32(&DSI_REG[0]->DSI_INTEN));
	DSI_OUTREG32(cmdq, &regs->DSI_MODE_CTRL, AS_UINT32(&DSI_REG[0]->DSI_MODE_CTRL));
	DSI_OUTREG32(cmdq, &regs->DSI_TXRX_CTRL, AS_UINT32(&DSI_REG[0]->DSI_TXRX_CTRL));
	DSI_OUTREG32(cmdq, &regs->DSI_PSCTRL, AS_UINT32(&DSI_REG[0]->DSI_PSCTRL));

	DSI_OUTREG32(cmdq, &regs->DSI_VSA_NL, AS_UINT32(&DSI_REG[0]->DSI_VSA_NL));
	DSI_OUTREG32(cmdq, &regs->DSI_VBP_NL, AS_UINT32(&DSI_REG[0]->DSI_VBP_NL));
	DSI_OUTREG32(cmdq, &regs->DSI_VFP_NL, AS_UINT32(&DSI_REG[0]->DSI_VFP_NL));
	DSI_OUTREG32(cmdq, &regs->DSI_VACT_NL, AS_UINT32(&DSI_REG[0]->DSI_VACT_NL));

	DSI_OUTREG32(cmdq, &regs->DSI_HSA_WC, AS_UINT32(&DSI_REG[0]->DSI_HSA_WC));
	DSI_OUTREG32(cmdq, &regs->DSI_HBP_WC, AS_UINT32(&DSI_REG[0]->DSI_HBP_WC));
	DSI_OUTREG32(cmdq, &regs->DSI_HFP_WC, AS_UINT32(&DSI_REG[0]->DSI_HFP_WC));
	DSI_OUTREG32(cmdq, &regs->DSI_BLLP_WC, AS_UINT32(&DSI_REG[0]->DSI_BLLP_WC));

	DSI_OUTREG32(cmdq, &regs->DSI_HSTX_CKL_WC, AS_UINT32(&DSI_REG[0]->DSI_HSTX_CKL_WC));
	DSI_OUTREG32(cmdq, &regs->DSI_MEM_CONTI, AS_UINT32(&DSI_REG[0]->DSI_MEM_CONTI));

	DSI_OUTREG32(cmdq, &regs->DSI_PHY_TIMECON0, AS_UINT32(&DSI_REG[0]->DSI_PHY_TIMECON0));
	DSI_OUTREG32(cmdq, &regs->DSI_PHY_TIMECON1, AS_UINT32(&DSI_REG[0]->DSI_PHY_TIMECON1));
	DSI_OUTREG32(cmdq, &regs->DSI_PHY_TIMECON2, AS_UINT32(&DSI_REG[0]->DSI_PHY_TIMECON2));
	DSI_OUTREG32(cmdq, &regs->DSI_PHY_TIMECON3, AS_UINT32(&DSI_REG[0]->DSI_PHY_TIMECON3));
	DSI_OUTREG32(cmdq, &regs->DSI_VM_CMD_CON, AS_UINT32(&DSI_REG[0]->DSI_VM_CMD_CON));
	return DSI_STATUS_OK;
}


enum DSI_STATUS DSI_RestoreRegisters(enum DISP_MODULE_ENUM module, void *cmdq)
{
	volatile struct DSI_REGS *regs = NULL;

	regs = &(_dsi_context[0].regBackup);

	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_INTEN, AS_UINT32(&regs->DSI_INTEN));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_MODE_CTRL, AS_UINT32(&regs->DSI_MODE_CTRL));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_TXRX_CTRL, AS_UINT32(&regs->DSI_TXRX_CTRL));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_PSCTRL, AS_UINT32(&regs->DSI_PSCTRL));

	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_VSA_NL, AS_UINT32(&regs->DSI_VSA_NL));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_VBP_NL, AS_UINT32(&regs->DSI_VBP_NL));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_VFP_NL, AS_UINT32(&regs->DSI_VFP_NL));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_VACT_NL, AS_UINT32(&regs->DSI_VACT_NL));

	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_HSA_WC, AS_UINT32(&regs->DSI_HSA_WC));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_HBP_WC, AS_UINT32(&regs->DSI_HBP_WC));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_HFP_WC, AS_UINT32(&regs->DSI_HFP_WC));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_BLLP_WC, AS_UINT32(&regs->DSI_BLLP_WC));

	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_HSTX_CKL_WC, AS_UINT32(&regs->DSI_HSTX_CKL_WC));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_MEM_CONTI, AS_UINT32(&regs->DSI_MEM_CONTI));

	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_PHY_TIMECON0, AS_UINT32(&regs->DSI_PHY_TIMECON0));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_PHY_TIMECON1, AS_UINT32(&regs->DSI_PHY_TIMECON1));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_PHY_TIMECON2, AS_UINT32(&regs->DSI_PHY_TIMECON2));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_PHY_TIMECON3, AS_UINT32(&regs->DSI_PHY_TIMECON3));
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_VM_CMD_CON, AS_UINT32(&regs->DSI_VM_CMD_CON));
	return DSI_STATUS_OK;
}

enum DSI_STATUS DSI_BIST_Pattern_Test(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, bool enable,
				 unsigned int color)
{
	int i = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enable) {
			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_BIST_PATTERN, color);
			/* DSI_OUTREG32(&DSI_REG->DSI_BIST_CON, AS_UINT32(&temp_reg)); */
			/* DSI_OUTREGBIT(DSI_BIST_CON_REG, DSI_REG->DSI_BIST_CON, SELF_PAT_MODE, 1); */
			DSI_OUTREGBIT(cmdq, struct DSI_BIST_CON_REG, DSI_REG[i]->DSI_BIST_CON,
				      SELF_PAT_MODE, 1);

			if (!_dsi_is_video_mode(module)) {
				struct DSI_T0_INS t0;

				t0.CONFG = 0x09;
				t0.Data_ID = 0x39;
				t0.Data0 = 0x2c;
				t0.Data1 = 0;

				DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[i]->data[0], AS_UINT32(&t0));
				DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_CMDQ_SIZE, 1);

				/* DSI_OUTREGBIT(cmdq, struct DSI_START_REG,DSI_REG->DSI_START,DSI_START,0); */
				DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_START, 0);
				DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_START, 1);
				/* DSI_OUTREGBIT(cmdq, struct DSI_START_REG,DSI_REG->DSI_START,DSI_START,1); */
			}
		} else {
			/* if disable dsi pattern, need enable mutex, can't just start dsi */
			/* so we just disable pattern bit, do not start dsi here */
			/* DSI_WaitForNotBusy(module,cmdq); */
			/* DSI_OUTREGBIT(cmdq, DSI_BIST_CON_REG, DSI_REG[i]->DSI_BIST_CON, SELF_PAT_MODE, 0); */
			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_BIST_CON, 0x00);
		}

	}
	return DSI_STATUS_OK;
}

void DSI_Config_VDO_Timing(enum DISP_MODULE_ENUM module, void *cmdq, LCM_DSI_PARAMS *dsi_params)
{
	int i = 0;
	unsigned int line_byte;
	unsigned int horizontal_sync_active_byte;
	unsigned int horizontal_backporch_byte;
	unsigned int horizontal_frontporch_byte;
	unsigned int horizontal_bllp_byte;
	unsigned int dsiTmpBufBpp;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (dsi_params->data_format.format == LCM_DSI_FORMAT_RGB565)
			dsiTmpBufBpp = 2;
		else
			dsiTmpBufBpp = 3;

		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_VSA_NL, dsi_params->vertical_sync_active);
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_VBP_NL, dsi_params->vertical_backporch);
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_VFP_NL, dsi_params->vertical_frontporch);
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_VACT_NL, dsi_params->vertical_active_line);

		line_byte =
		    (dsi_params->horizontal_sync_active + dsi_params->horizontal_backporch +
		     dsi_params->horizontal_frontporch +
		     dsi_params->horizontal_active_pixel) * dsiTmpBufBpp;
		horizontal_sync_active_byte =
		    (dsi_params->horizontal_sync_active * dsiTmpBufBpp - 4);

		if (dsi_params->mode == SYNC_EVENT_VDO_MODE || dsi_params->mode == BURST_VDO_MODE
		    || dsi_params->switch_mode == SYNC_EVENT_VDO_MODE
		    || dsi_params->switch_mode == BURST_VDO_MODE) {
			ASSERT((dsi_params->horizontal_backporch +
				dsi_params->horizontal_sync_active) * dsiTmpBufBpp > 9);
			horizontal_backporch_byte =
			    ((dsi_params->horizontal_backporch +
			      dsi_params->horizontal_sync_active) * dsiTmpBufBpp - 10);
		} else {
			ASSERT(dsi_params->horizontal_sync_active * dsiTmpBufBpp > 9);
			horizontal_sync_active_byte =
			    (dsi_params->horizontal_sync_active * dsiTmpBufBpp - 10);

			ASSERT(dsi_params->horizontal_backporch * dsiTmpBufBpp > 9);
			horizontal_backporch_byte =
			    (dsi_params->horizontal_backporch * dsiTmpBufBpp - 10);
		}

		ASSERT(dsi_params->horizontal_frontporch * dsiTmpBufBpp > 11);
		horizontal_frontporch_byte =
		    (dsi_params->horizontal_frontporch * dsiTmpBufBpp - 12);
		horizontal_bllp_byte = (dsi_params->horizontal_bllp * dsiTmpBufBpp);

		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HSA_WC,
			     ALIGN_TO((horizontal_sync_active_byte), 4));
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HBP_WC,
			     ALIGN_TO((horizontal_backporch_byte), 4));
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HFP_WC,
			     ALIGN_TO((horizontal_frontporch_byte), 4));
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_BLLP_WC, ALIGN_TO((horizontal_bllp_byte), 4));
	}
}

int _dsi_ps_type_to_bpp(LCM_PS_TYPE ps)
{
	switch (ps) {
	case LCM_PACKED_PS_16BIT_RGB565:
		return 2;
	case LCM_LOOSELY_PS_18BIT_RGB666:
		return 3;
	case LCM_PACKED_PS_24BIT_RGB888:
		return 3;
	case LCM_PACKED_PS_18BIT_RGB666:
		return 3;
	}
	return 0;
}

enum DSI_STATUS DSI_PS_Control(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, LCM_DSI_PARAMS *dsi_params,
			  int w, int h)
{
	int i = 0;
	unsigned int ps_sel_bitvalue = 0;
	/* /TODO: parameter checking */
	ASSERT(_dsi_ps_type_to_bpp(dsi_params->PS) <= PACKED_PS_18BIT_RGB666);

	if (_dsi_ps_type_to_bpp(dsi_params->PS) > LOOSELY_PS_18BIT_RGB666)
		ps_sel_bitvalue = (5 - dsi_params->PS);
	else
		ps_sel_bitvalue = dsi_params->PS;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		DSI_OUTREGBIT(cmdq, struct DSI_VACT_NL_REG, DSI_REG[i]->DSI_VACT_NL, VACT_NL, h);
		if (dsi_params->ufoe_enable && dsi_params->ufoe_params.lr_mode_en != 1) {
			if (dsi_params->ufoe_params.compress_ratio == 3) {	/* 1/3 */
				unsigned int ufoe_internal_width = w + w % 4;

				if (ufoe_internal_width % 3 == 0) {
					DSI_OUTREGBIT(cmdq, struct DSI_PSCTRL_REG, DSI_REG[i]->DSI_PSCTRL,
						      DSI_PS_WC,
						      (ufoe_internal_width / 3) *
						      _dsi_ps_type_to_bpp(dsi_params->PS));
				} else {
					unsigned int temp_w = ufoe_internal_width / 3 + 1;

					temp_w = ((temp_w % 2) == 1) ? (temp_w + 1) : temp_w;
					DSI_OUTREGBIT(cmdq, struct DSI_PSCTRL_REG, DSI_REG[i]->DSI_PSCTRL,
						      DSI_PS_WC,
						      temp_w * _dsi_ps_type_to_bpp(dsi_params->PS));
				}
			} else
				DSI_OUTREGBIT(cmdq, struct DSI_PSCTRL_REG, DSI_REG[i]->DSI_PSCTRL,
					      DSI_PS_WC,
					      (w +
					       w % 4) / 2 * _dsi_ps_type_to_bpp(dsi_params->PS));
		} else {
			DSI_OUTREGBIT(cmdq, struct DSI_PSCTRL_REG, DSI_REG[i]->DSI_PSCTRL, DSI_PS_WC,
				      w * _dsi_ps_type_to_bpp(dsi_params->PS));
		}

		DSI_OUTREGBIT(cmdq, struct DSI_PSCTRL_REG, DSI_REG[i]->DSI_PSCTRL, DSI_PS_SEL,
			      ps_sel_bitvalue);
	}

	return DSI_STATUS_OK;
}

enum DSI_STATUS DSI_TXRX_Control(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
			    LCM_DSI_PARAMS *dsi_params)
{
	int i = 0;
	unsigned int lane_num_bitvalue = 0;
	/*bool cksm_en = true; */
	/*bool ecc_en = true; */
	int lane_num = dsi_params->LANE_NUM;
	int vc_num = 0;
	bool null_packet_en = dsi_params->null_packet_en;
	/*bool err_correction_en = false; */
	bool dis_eotp_en = false;
	bool hstx_cklp_en = dsi_params->noncont_clock;
	int max_return_size = 0;

	switch (lane_num) {
	case LCM_ONE_LANE:
		lane_num_bitvalue = 0x1;
		break;
	case LCM_TWO_LANE:
		lane_num_bitvalue = 0x3;
		break;
	case LCM_THREE_LANE:
		lane_num_bitvalue = 0x7;
		break;
	case LCM_FOUR_LANE:
		lane_num_bitvalue = 0xF;
		break;
	}

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, VC_NUM, vc_num);
		DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, DIS_EOT,
			      dis_eotp_en);
		DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, NULL_EN,
			      null_packet_en);
		DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, MAX_RTN_SIZE,
			      max_return_size);
		DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, HSTX_CKLP_EN,
			      hstx_cklp_en);

		DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, LANE_NUM,
						  lane_num_bitvalue);

		if (dsi_params->mode == CMD_MODE) {
			if (dsi_params->ext_te_edge == LCM_POLARITY_FALLING) {
				/*use ext te falling edge */
				DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL,
					      EXT_TE_EDGE, 1);
			}
			DSI_OUTREGBIT(cmdq, struct DSI_TXRX_CTRL_REG, DSI_REG[i]->DSI_TXRX_CTRL, EXT_TE_EN,
				      1);
		}
	}
	return DSI_STATUS_OK;
}

int MIPITX_IsEnabled(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	int i = 0;
	int ret = 0;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PLL_EN)
			ret++;
	}

	DISPMSG("MIPITX for %s is %s\n", ddp_get_module_name(module), ret ? "on" : "off");
	return ret;
}

unsigned int dsi_phy_get_clk(enum DISP_MODULE_ENUM module)
{
	int i = 0;
	int j = 0;
	unsigned int pcw = DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2.RG_DSI0_MPPLL_SDM_PCW_H;
	unsigned int prediv = (1 << (DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_PREDIV));
	unsigned int posdiv = (1 << (DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_POSDIV));
	unsigned int txdiv0 = (1 << (DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_TXDIV0));
	unsigned int txdiv1 = (1 << (DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0.RG_DSI0_MPPLL_TXDIV1));

	DISPMSG("%s, pcw: %d, prediv: %d, posdiv: %d, txdiv0: %d, txdiv1: %d\n", __func__, pcw,
		prediv, posdiv, txdiv0, txdiv1);
	j = prediv * 4 * txdiv0 * txdiv1;
	if (j > 0)
		return 26 * pcw / j;

	return 0;
}

void DSI_PHY_clk_setting(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq, LCM_DSI_PARAMS *dsi_params)
{
#ifdef CONFIG_FPGA_EARLY_PORTING
#ifdef FPGA_SET_CLK
	MIPITX_Write60384(0x18, 0x00, 0x10);
	MIPITX_Write60384(0x20, 0x42, 0x01);
	MIPITX_Write60384(0x20, 0x43, 0x01);
	MIPITX_Write60384(0x20, 0x05, 0x01);
	MIPITX_Write60384(0x20, 0x22, 0x01);
	MIPITX_Write60384(0x30, 0x44, 0x83);
	MIPITX_Write60384(0x30, 0x40, 0x82);
	MIPITX_Write60384(0x30, 0x00, 0x03);
	MIPITX_Write60384(0x30, 0x68, 0x03);
	MIPITX_Write60384(0x30, 0x68, 0x01);
	MIPITX_Write60384(0x30, 0x50, 0x80);
	MIPITX_Write60384(0x30, 0x51, 0x01);
	MIPITX_Write60384(0x30, 0x54, 0x01);
	MIPITX_Write60384(0x30, 0x58, 0x00);
	MIPITX_Write60384(0x30, 0x59, 0x00);
	MIPITX_Write60384(0x30, 0x5a, 0x00);
	MIPITX_Write60384(0x30, 0x5b, (dsi_params->fbk_div) << 2);
	MIPITX_Write60384(0x30, 0x04, 0x11);
	MIPITX_Write60384(0x30, 0x08, 0x01);
	MIPITX_Write60384(0x30, 0x0C, 0x01);
	MIPITX_Write60384(0x30, 0x10, 0x01);
	MIPITX_Write60384(0x30, 0x14, 0x01);
	MIPITX_Write60384(0x30, 0x64, 0x20);
	MIPITX_Write60384(0x30, 0x50, 0x81);
	MIPITX_Write60384(0x30, 0x28, 0x00);
	mdelay(500);
	DDPMSG("PLL setting finish!!\n");

	DISP_REG_SET(NULL, DISP_REG_CONFIG_MMSYS_LCM_RST_B, 0);
	DISP_REG_SET(NULL, DISP_REG_CONFIG_MMSYS_LCM_RST_B, 1);
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_COM_CTRL, 0x5);
	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_COM_CTRL, 0x0);
#endif
#else
	int i = 0;
	unsigned int data_Rate = dsi_params->PLL_CLOCK * 2;
	unsigned int txdiv = 0;
	unsigned int txdiv0 = 0;
	unsigned int txdiv1 = 0;
	unsigned int pcw = 0;
	/* unsigned int fmod = 30; */	/*Fmod = 30KHz by default */
	unsigned int delta1 = 5;	/* Delta1 is SSC range, default is 0%~-5% */
	unsigned int pdelta1 = 0;

	/* temp1~5 is used for impedence calibration, not enable now */

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		/* step 1 */
		/* MIPITX_MASKREG32(APMIXED_BASE+0x00, (0x1<<6), 1); */

		/* step 4 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				 RG_DSI_LNT_IMP_CAL_CODE, 8);

		MIPITX_OUTREGBIT(struct MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				 RG_DSI_LNT_HS_BIAS_EN, 1);

		/* step 2 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_V032_SEL, 4);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_V04_SEL, 4);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_V072_SEL, 4);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_V10_SEL, 4);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_V12_SEL, 4);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_BG_CORE_EN, 1);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_BG_CON,
				 RG_DSI_BG_CKEN, 1);
		/* step 3 */
		mdelay(10);

		/* step 5 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI0_CON,
				 RG_DSI0_CKG_LDOOUT_EN, 1);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI0_CON,
				 RG_DSI0_LDOCORE_EN, 1);
		/* step 6 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_PWR_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_PWR,
				 DA_DSI0_MPPLL_SDM_PWR_ON, 1);

		/* step 7 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_PWR_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_PWR,
				 DA_DSI0_MPPLL_SDM_ISO_EN, 1);
		mdelay(10);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_PWR_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_PWR,
				 DA_DSI0_MPPLL_SDM_ISO_EN, 0);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG,
			DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0, RG_DSI0_MPPLL_PREDIV, 0);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG,
			DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0, RG_DSI0_MPPLL_POSDIV, 0);

		if (data_Rate != 0) {
			if (data_Rate > 1250) {
				DISPCHECK("mipitx Data Rate exceed limitation(%d)\n", data_Rate);
				ASSERT(0);
			} else if (data_Rate >= 500) {
				txdiv = 1;
				txdiv0 = 0;
				txdiv1 = 0;
			} else if (data_Rate >= 250) {
				txdiv = 2;
				txdiv0 = 1;
				txdiv1 = 0;
			} else if (data_Rate >= 125) {
				txdiv = 4;
				txdiv0 = 2;
				txdiv1 = 0;
			} else if (data_Rate > 62) {
				txdiv = 8;
				txdiv0 = 2;
				txdiv1 = 1;
			} else if (data_Rate >= 50) {
				txdiv = 16;
				txdiv0 = 2;
				txdiv1 = 2;
			}  else {
				DISPCHECK("dataRate is too low(%d)\n", data_Rate);
				ASSERT(0);
			}

			/* step 8 */
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0, RG_DSI0_MPPLL_TXDIV0,
					 txdiv0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0, RG_DSI0_MPPLL_TXDIV1,
					 txdiv1);

			/* step 10 */
			/* PLL PCW config */
			/*
			*
			   PCW bit 24~30 = floor(pcw)
			   PCW bit 16~23 = (pcw - floor(pcw))*256
			   PCW bit 8~15 = (pcw*256 - floor(pcw)*256)*256
			   PCW bit 8~15 = (pcw*256*256 - floor(pcw)*256*256)*256
			*/
			/* pcw = data_Rate*4*txdiv/(26*2);//Post DIV =4, so need data_Rate*4 */
			pcw = data_Rate * txdiv / 13;

			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
					 RG_DSI0_MPPLL_SDM_PCW_H, (pcw & 0x7F));
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
					 RG_DSI0_MPPLL_SDM_PCW_16_23,
					 ((256 * (data_Rate * txdiv % 13) / 13) & 0xFF));
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
					 RG_DSI0_MPPLL_SDM_PCW_8_15,
					 ((256 * (256 * (data_Rate * txdiv % 13) % 13) /
					   13) & 0xFF));
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
					 RG_DSI0_MPPLL_SDM_PCW_0_7,
					 ((256 *
					   (256 * (256 * (data_Rate * txdiv % 13) % 13) % 13) /
					   13) & 0xFF));

			if (dsi_params->ssc_disable != 1) {
				MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON1_REG,
						 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON1,
						 RG_DSI0_MPPLL_SDM_SSC_PH_INIT, 1);
				MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON1_REG,
						 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON1,
						 RG_DSI0_MPPLL_SDM_SSC_PRD, 0x1B1);
				if (dsi_params->ssc_range != 0)
					delta1 = dsi_params->ssc_range;

				ASSERT(delta1 <= 8);
				pdelta1 = (delta1 * data_Rate * txdiv * 262144 + 281664) / 563329;
				MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON3_REG,
						 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON3,
						 RG_DSI0_MPPLL_SDM_SSC_DELTA, pdelta1);
				MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON3_REG,
						 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON3,
						 RG_DSI0_MPPLL_SDM_SSC_DELTA1, pdelta1);
				/* DSI_OUTREGBIT(struct MIPITX_DSI_PLL_CON1_REG,DSI_PHY_REG->MIPITX_DSI_PLL_CON1,*/
				/*	RG_DSI0_MPPLL_SDM_FRA_EN,1); */
				DISPMSG
				    ("[dsi_drv.c] PLL config:data_rate=%d,txdiv=%d,pcw=%d,delta1=%d,pdelta1=0x%x\n",
				     data_Rate, txdiv, DSI_INREG32((struct MIPITX_DSI_PLL_CON2_REG *),
								   &DSI_PHY_REG[i]->
								   MIPITX_DSI_PLL_CON2), delta1,
				     pdelta1);
			}
		} else {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_TXDIV0, dsi_params->pll_div1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_TXDIV1, dsi_params->pll_div2);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
				  RG_DSI0_MPPLL_SDM_PCW_H, ((dsi_params->fbk_div) << 2));
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
				  RG_DSI0_MPPLL_SDM_PCW_16_23, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
				  RG_DSI0_MPPLL_SDM_PCW_8_15, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON2_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2,
				  RG_DSI0_MPPLL_SDM_PCW_0_7, 0);
			DISPERR("[dsi_dsi.c] PLL clock should not be 0!!!\n");
			ASSERT(0);
		}

		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON1,
				 RG_DSI0_MPPLL_SDM_FRA_EN, 1);

		/* step 11 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_CLOCK_LANE_REG, DSI_PHY_REG[i]->MIPITX_DSI0_CLOCK_LANE,
			RG_DSI0_LNTC_RT_CODE, 0x8);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_CLOCK_LANE_REG, DSI_PHY_REG[i]->MIPITX_DSI0_CLOCK_LANE,
			RG_DSI0_LNTC_PHI_SEL, 0x1);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_CLOCK_LANE_REG, DSI_PHY_REG[i]->MIPITX_DSI0_CLOCK_LANE,
			RG_DSI0_LNTC_LDOOUT_EN, 1);

		/* step 12 */
		if (dsi_params->LANE_NUM > 0) {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE0_REG,
				DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE0,
				RG_DSI0_LNT0_RT_CODE, 0x8);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE0_REG,
				DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE0,
				RG_DSI0_LNT0_LDOOUT_EN, 1);
		}

		/* step 13 */
		if (dsi_params->LANE_NUM > 1) {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE1_REG,
				DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE1,
				RG_DSI0_LNT1_RT_CODE, 0x8);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE1_REG,
				DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE1,
				RG_DSI0_LNT1_LDOOUT_EN, 1);

		}

		/* step 14 */
		if (dsi_params->LANE_NUM > 2) {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE2_REG,
				DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE2,
				RG_DSI0_LNT2_RT_CODE, 0x8);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE2_REG,
				DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE2,
				RG_DSI0_LNT2_LDOOUT_EN, 1);
		}

		/* step 15 */
		if (dsi_params->LANE_NUM > 3) {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE3_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE3,
					 RG_DSI0_LNT3_RT_CODE, 0x8);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE3_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE3,
					 RG_DSI0_LNT3_LDOOUT_EN, 1);
		}

		/* step 16 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				 RG_DSI0_MPPLL_PLL_EN, 1);

		/* step 17 */
		mdelay(1);

		if ((data_Rate != 0) && (dsi_params->ssc_disable != 1)) {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON1_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON1,
					 RG_DSI0_MPPLL_SDM_SSC_EN, 1);
		} else {
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON1_REG,
					 DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON1,
					 RG_DSI0_MPPLL_SDM_SSC_EN, 0);
		}

		/* step 18 */
		MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_TOP_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_TOP,
				RG_MPPLL_PRESERVE_L, 3);
		MIPITX_OUTREGBIT(struct MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				RG_DSI_PAD_TIE_LOW_EN, 0);
		mdelay(1);
	}
#endif
}

void DSI_PHY_TIMCONFIG(LCM_DSI_PARAMS *lcm_params)
{
	struct DSI_PHY_TIMCON0_REG timcon0;
	struct DSI_PHY_TIMCON1_REG timcon1;
	struct DSI_PHY_TIMCON2_REG timcon2;
	struct DSI_PHY_TIMCON3_REG timcon3;
	unsigned int div1 = 0;
	unsigned int div2 = 0;
	unsigned int pre_div = 0;
	unsigned int post_div = 0;
	unsigned int fbk_sel = 0;
	unsigned int fbk_div = 0;
	unsigned int lane_no = lcm_params->LANE_NUM;

	/* unsigned int div2_real; */
	unsigned int cycle_time;
	unsigned int ui;
	unsigned int hs_trail_m, hs_trail_n;
	unsigned char timcon_temp;

	if (lcm_params->PLL_CLOCK != 0) {
		ui = 1000 / (lcm_params->PLL_CLOCK * 2) + 0x01;
		cycle_time = 8000 / (lcm_params->PLL_CLOCK * 2) + 0x01;
		DDPMSG("DSI_PHY, Cycle Time: %d(ns), Unit Interval: %d(ns). , lane#: %d\n",
			       cycle_time, ui, lane_no);
	} else {
		div1 = lcm_params->pll_div1;
		div2 = lcm_params->pll_div2;
		fbk_div = lcm_params->fbk_div;
		switch (div1) {
		case 0:
			div1 = 1;
			break;

		case 1:
			div1 = 2;
			break;

		case 2:
		case 3:
			div1 = 4;
			break;

		default:
			pr_info("div1 should be less than 4!!\n");
			div1 = 4;
			break;
		}

		switch (div2) {
		case 0:
			div2 = 1;
			break;
		case 1:
			div2 = 2;
			break;
		case 2:
		case 3:
			div2 = 4;
			break;
		default:
			pr_info("div2 should be less than 4!!\n");
			div2 = 4;
			break;
		}

		switch (pre_div) {
		case 0:
			pre_div = 1;
			break;

		case 1:
			pre_div = 2;
			break;

		case 2:
		case 3:
			pre_div = 4;
			break;

		default:
			pr_info("pre_div should be less than 4!!\n");
			pre_div = 4;
			break;
		}

		switch (post_div) {
		case 0:
			post_div = 1;
			break;

		case 1:
			post_div = 2;
			break;

		case 2:
		case 3:
			post_div = 4;
			break;

		default:
			pr_info("post_div should be less than 4!!\n");
			post_div = 4;
			break;
		}

		switch (fbk_sel) {
		case 0:
			fbk_sel = 1;
			break;

		case 1:
			fbk_sel = 2;
			break;

		case 2:
		case 3:
			fbk_sel = 4;
			break;

		default:
			DDPMSG("fbk_sel should be less than 4!!\n");
			fbk_sel = 4;
			break;
		}
		cycle_time = (1000 * 4 * div2 * div1) / (fbk_div * 26) + 0x01;

		ui = (1000 * div2 * div1) / (fbk_div * 26 * 0x2) + 0x01;
		DDPMSG("DSI_PHY, Cycle Time: %d(ns), Unit Interval: %d(ns)", cycle_time, ui);
		DDPMSG(". div1: %d, div2: %d, fbk_div: %d, lane#: %d\n",
				div1, div2, fbk_div, lane_no);
	}

	/* div2_real=div2 ? div2*0x02 : 0x1; */
	/* cycle_time = (1000 * div2 * div1 * pre_div * post_div)/ (fbk_sel * (fbk_div+0x01) * 26) + 1; */
	/* ui = (1000 * div2 * div1 * pre_div * post_div)/ (fbk_sel * (fbk_div+0x01) * 26 * 2) + 1; */
#define NS_TO_CYCLE(n, c)   ((n) / (c))

	hs_trail_m = 1;
	hs_trail_n =
	    (lcm_params->HS_TRAIL == 0) ? NS_TO_CYCLE(((hs_trail_m * 0x4) + 0x60),
							  cycle_time) : lcm_params->HS_TRAIL;
	/* +3 is recommended from designer becauase of HW latency */
	timcon0.HS_TRAIL = ((hs_trail_m > hs_trail_n) ? hs_trail_m : hs_trail_n) + 0x0a;

	timcon0.HS_PRPR =
	    (lcm_params->HS_PRPR == 0) ? NS_TO_CYCLE((0x40 + 0x5 * ui),
							 cycle_time) : lcm_params->HS_PRPR;
	/* HS_PRPR can't be 1. */
	if (timcon0.HS_PRPR == 0)
		timcon0.HS_PRPR = 1;

	timcon0.HS_ZERO =
	    (lcm_params->HS_ZERO == 0) ? NS_TO_CYCLE((0xC8 + 0x0a * ui),
							 cycle_time) : lcm_params->HS_ZERO;
	timcon_temp = timcon0.HS_PRPR;
	if (timcon_temp < timcon0.HS_ZERO)
		timcon0.HS_ZERO -= timcon0.HS_PRPR;

	timcon0.LPX =
	    (lcm_params->LPX == 0) ? NS_TO_CYCLE(0x50, cycle_time) : lcm_params->LPX;
	if (timcon0.LPX == 0)
		timcon0.LPX = 1;

	/* timcon1.TA_SACK     = (lcm_params->TA_SACK == 0) ? 1 : lcm_params->TA_SACK; */
	timcon1.TA_GET =
	    (lcm_params->TA_GET == 0) ? (0x5 * timcon0.LPX) : lcm_params->TA_GET;
	timcon1.TA_SURE =
	    (lcm_params->TA_SURE == 0) ? (0x3 * timcon0.LPX / 0x2) : lcm_params->TA_SURE;
	timcon1.TA_GO = (lcm_params->TA_GO == 0) ? (0x4 * timcon0.LPX) : lcm_params->TA_GO;
	/* -------------------------------------------------------------- */
	/* NT35510 need fine tune timing */
	/* Data_hs_exit = 60 ns + 128UI */
	/* Clk_post = 60 ns + 128 UI. */
	/* -------------------------------------------------------------- */
	timcon1.DA_HS_EXIT =
	    (lcm_params->DA_HS_EXIT == 0) ? NS_TO_CYCLE((0x3c + 0x80 * ui),
							    cycle_time) : lcm_params->DA_HS_EXIT;

	timcon2.CLK_TRAIL =
	    ((lcm_params->CLK_TRAIL == 0) ? NS_TO_CYCLE(0x64,
				cycle_time) : lcm_params->CLK_TRAIL) + 0x0a;
	/* CLK_TRAIL can't be 1. */
	if (timcon2.CLK_TRAIL < 2)
		timcon2.CLK_TRAIL = 2;

	/* timcon2.LPX_WAIT    = (lcm_params->LPX_WAIT == 0) ? 1 : lcm_params->LPX_WAIT; */
	timcon2.CONT_DET = lcm_params->CONT_DET;
	timcon2.CLK_ZERO =
	    (lcm_params->CLK_ZERO == 0) ? NS_TO_CYCLE(0x190,
							  cycle_time) : lcm_params->CLK_ZERO;

	timcon3.CLK_HS_PRPR =
	    (lcm_params->CLK_HS_PRPR == 0) ? NS_TO_CYCLE(0x40,
							     cycle_time) : lcm_params->CLK_HS_PRPR;
	if (timcon3.CLK_HS_PRPR == 0)
		timcon3.CLK_HS_PRPR = 1;
	timcon3.CLK_HS_EXIT =
	    (lcm_params->CLK_HS_EXIT == 0) ? (2 * timcon0.LPX) : lcm_params->CLK_HS_EXIT;
	timcon3.CLK_HS_POST =
	    (lcm_params->CLK_HS_POST == 0) ? NS_TO_CYCLE((0x3c + 0x80 * ui),
							     cycle_time) : lcm_params->CLK_HS_POST;

	DDPMSG("DSI_PHY, HS_TRAIL:%d, HS_ZERO:%d, HS_PRPR:%d, LPX:%d, TA_GET:%d, TA_SURE:%d\n",
		       timcon0.HS_TRAIL, timcon0.HS_ZERO, timcon0.HS_PRPR, timcon0.LPX,
		       timcon1.TA_GET, timcon1.TA_SURE);
	DDPMSG("DSI_PHY, TA_GO:%d, CLK_TRAIL:%d, CLK_ZERO:%d, CLK_HS_PRPR:%d\n",
		       timcon1.TA_GO, timcon2.CLK_TRAIL,
		       timcon2.CLK_ZERO, timcon3.CLK_HS_PRPR);

	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG, DSI_REG[0]->DSI_PHY_TIMECON0, LPX, timcon0.LPX);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG, DSI_REG[0]->DSI_PHY_TIMECON0, HS_PRPR, timcon0.HS_PRPR);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG, DSI_REG[0]->DSI_PHY_TIMECON0, HS_ZERO, timcon0.HS_ZERO);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG, DSI_REG[0]->DSI_PHY_TIMECON0, HS_TRAIL, timcon0.HS_TRAIL);

	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG, DSI_REG[0]->DSI_PHY_TIMECON1, TA_GO, timcon1.TA_GO);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG, DSI_REG[0]->DSI_PHY_TIMECON1, TA_SURE, timcon1.TA_SURE);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG, DSI_REG[0]->DSI_PHY_TIMECON1, TA_GET, timcon1.TA_GET);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG, DSI_REG[0]->DSI_PHY_TIMECON1, DA_HS_EXIT, timcon1.DA_HS_EXIT);

	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON2_REG, DSI_REG[0]->DSI_PHY_TIMECON2, CONT_DET, timcon2.CONT_DET);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON2_REG, DSI_REG[0]->DSI_PHY_TIMECON2, CLK_ZERO, timcon2.CLK_ZERO);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON2_REG, DSI_REG[0]->DSI_PHY_TIMECON2, CLK_TRAIL, timcon2.CLK_TRAIL);

	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON3_REG, DSI_REG[0]->DSI_PHY_TIMECON3, CLK_HS_PRPR, timcon3.CLK_HS_PRPR);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON3_REG, DSI_REG[0]->DSI_PHY_TIMECON3, CLK_HS_POST, timcon3.CLK_HS_POST);
	DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON3_REG, DSI_REG[0]->DSI_PHY_TIMECON3, CLK_HS_EXIT, timcon3.CLK_HS_EXIT);
	DDPMSG("%s, 0x%08x,0x%08x,0x%08x,0x%08x\n", __func__,
		DISP_REG_GET(DSI_REG[0] + 0x110), DISP_REG_GET(DSI_REG[0] + 0x114),
		DISP_REG_GET(DSI_REG[0] + 0x118), DISP_REG_GET(DSI_REG[0] + 0x11c));
}


void DSI_PHY_clk_switch(enum DISP_MODULE_ENUM module, void *cmdq, int on)
{
	int i = 0;

	if (on) {
		DSI_PHY_clk_setting(module, cmdq, &(_dsi_context[i].dsi_params));
	} else {
		for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
			/* pre_oe/oe = 1 */
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON0,
				  SW_LNTC_LPTX_PRE_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON0,
				  SW_LNTC_LPTX_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT0_LPTX_PRE_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT0_LPTX_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT1_LPTX_PRE_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT1_LPTX_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT2_LPTX_PRE_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT2_LPTX_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT3_LPTX_PRE_OE, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_CON1_REG, DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL_CON1,
				  SW_LNT3_LPTX_OE, 1);

			/* switch to mipi tx sw mode */
			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_REG,
				DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL, SW_CTRL_EN, 1);

			/* disable mipi clock */
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_PLL_EN, 0);
			mdelay(1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_TOP_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_TOP,
				  RG_MPPLL_PRESERVE_L, 0);

			MIPITX_OUTREGBIT(struct MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				  RG_DSI_PAD_TIE_LOW_EN, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_CLOCK_LANE_REG, DSI_PHY_REG[i]->MIPITX_DSI0_CLOCK_LANE,
				  RG_DSI0_LNTC_LDOOUT_EN, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE0_REG, DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE0,
				  RG_DSI0_LNT0_LDOOUT_EN, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE1_REG, DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE1,
				  RG_DSI0_LNT1_LDOOUT_EN, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE2_REG, DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE2,
				  RG_DSI0_LNT2_LDOOUT_EN, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_DATA_LANE3_REG, DSI_PHY_REG[i]->MIPITX_DSI0_DATA_LANE3,
				  RG_DSI0_LNT3_LDOOUT_EN, 0);
			mdelay(1);

			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_PWR_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_PWR,
				  DA_DSI0_MPPLL_SDM_ISO_EN, 1);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_PWR_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_PWR,
				  DA_DSI0_MPPLL_SDM_PWR_ON, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_TOP_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				  RG_DSI_LNT_HS_BIAS_EN, 0);

			MIPITX_OUTREGBIT(struct MIPITX_DSI_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				RG_DSI0_CKG_LDOOUT_EN, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_CON_REG, DSI_PHY_REG[i]->MIPITX_DSI_TOP_CON,
				RG_DSI0_LDOCORE_EN, 0);

			MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG,
				DSI_PHY_REG[i]->MIPITX_DSI_BG_CON, RG_DSI_BG_CKEN, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_BG_CON_REG,
				DSI_PHY_REG[i]->MIPITX_DSI_BG_CON, RG_DSI_BG_CORE_EN, 0);

			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_PREDIV, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_TXDIV0, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_TXDIV1, 0);
			MIPITX_OUTREGBIT(struct MIPITX_DSI_PLL_CON0_REG, DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON0,
				  RG_DSI0_MPPLL_POSDIV, 0);


			MIPITX_OUTREG32(&DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON1, 0x00000000);
			MIPITX_OUTREG32(&DSI_PHY_REG[i]->MIPITX_DSI_PLL_CON2, 0x50000000);

			MIPITX_OUTREGBIT(struct MIPITX_DSI_SW_CTRL_REG,
				DSI_PHY_REG[i]->MIPITX_DSI_SW_CTRL, SW_CTRL_EN, 0);
			mdelay(1);
		}
	}
}

enum DSI_STATUS DSI_EnableClk(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	DSI_OUTREGBIT(cmdq, struct DSI_COM_CTRL_REG, DSI_REG[0]->DSI_COM_CTRL, DSI_EN, 1);

	return DSI_STATUS_OK;
}

enum DSI_STATUS DSI_Start(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	/* TODO: do we need this? */
	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, DSI_START, 0);
	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, DSI_START, 1);
	return DSI_STATUS_OK;
}

void DSI_Set_VM_CMD(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	DSI_OUTREGBIT(cmdq, struct DSI_VM_CMD_CON_REG, DSI_REG[0]->DSI_VM_CMD_CON,
		      TS_VFP_EN, 1);
	DSI_OUTREGBIT(cmdq, struct DSI_VM_CMD_CON_REG, DSI_REG[0]->DSI_VM_CMD_CON,
		      VM_CMD_EN, 1);
	DDPMSG("DSI_Set_VM_CMD");
}

enum DSI_STATUS DSI_EnableVM_CMD(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq)
{
	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, VM_CMD_START, 0);
	DSI_OUTREGBIT(cmdq, struct DSI_START_REG, DSI_REG[0]->DSI_START, VM_CMD_START, 1);
	return DSI_STATUS_OK;
}

/* return value: the data length we got */
uint32_t DSI_dcs_read_lcm_reg_v2(enum DISP_MODULE_ENUM module, void *cmdq, uint8_t cmd,
	uint8_t *buffer, uint8_t buffer_size)
{
	uint32_t max_try_count = 5;
	uint32_t recv_data_cnt;
	unsigned int read_timeout_ms;
	unsigned char packet_type;
	struct DSI_RX_DATA_REG read_data0;
	struct DSI_RX_DATA_REG read_data1;
	struct DSI_RX_DATA_REG read_data2;
	struct DSI_RX_DATA_REG read_data3;
	int i = 0;
	struct DSI_T0_INS t0;

#if ENABLE_DSI_INTERRUPT
	static const long WAIT_TIMEOUT = HZ / 2;
	long ret;
#endif
	if (DSI_REG[i]->DSI_MODE_CTRL.MODE)
		return 0;

	if (buffer == NULL || buffer_size == 0)
		return 0;

	do {
		if (max_try_count == 0)
			return 0;
		max_try_count--;
		recv_data_cnt = 0;
		read_timeout_ms = 20;

		DSI_WaitForNotBusy(module, cmdq);

		t0.CONFG = 0x04;	/* BTA */
		t0.Data0 = cmd;
		if (buffer_size < 0x3)
			t0.Data_ID = DSI_DCS_READ_PACKET_ID;
		else
			t0.Data_ID = DSI_GERNERIC_READ_LONG_PACKET_ID;
		t0.Data1 = 0;

		DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[i]->data[0], AS_UINT32(&t0));
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_CMDQ_SIZE, 1);

		DSI_OUTREGBIT(cmdq, struct DSI_RACK_REG, DSI_REG[i]->DSI_RACK, DSI_RACK, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_STATUS_REG, DSI_REG[i]->DSI_INTSTA, RD_RDY, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_STATUS_REG, DSI_REG[i]->DSI_INTSTA, CMD_DONE, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[i]->DSI_INTEN, RD_RDY, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[i]->DSI_INTEN, CMD_DONE, 1);

		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_START, 0);
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_START, 1);
#if ENABLE_DSI_INTERRUPT
		ret = wait_event_interruptible_timeout(_dsi_dcs_read_wait_queue[i],
						       waitRDDone, WAIT_TIMEOUT);
		waitRDDone = false;
		if (ret == 0) {
			DDPMSG(" Wait for DSI engine read ready timeout!!!\n");

			DSI_DumpRegisters(module, 1);

			DSI_OUTREGBIT(cmdq, struct DSI_RACK_REG, DSI_REG[i]->DSI_RACK, DSI_RACK, 1);
			DSI_Reset(module, NULL);

			return 0;
		}
#else
		DDPMSG(" Start polling DSI read ready!!!\n");
		while (DSI_REG[i]->DSI_INTSTA.RD_RDY == 0) {
			msleep(20);
			read_timeout_ms--;

			if (read_timeout_ms == 0) {
				DDPMSG(" Polling DSI read ready timeout!!!\n");
				DSI_DumpRegisters(module, 1);
				DSI_OUTREGBIT(cmdq, struct DSI_RACK_REG, DSI_REG[i]->DSI_RACK, DSI_RACK, 1);
				DSI_Reset(module, NULL);
				return 0;
			}
		}
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		if (dsi_log_on)
			DDPMSG(" End polling DSI read ready!!!\n");
#endif

		DSI_OUTREGBIT(cmdq, struct DSI_RACK_REG, DSI_REG[i]->DSI_RACK, DSI_RACK, 1);

		DSI_OUTREGBIT(cmdq, struct DSI_INT_STATUS_REG, DSI_REG[i]->DSI_INTSTA, RD_RDY, 1);
		DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_START, 0);

#endif

		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[i]->DSI_INTEN, RD_RDY, 1);

		DSI_OUTREG32(cmdq, &read_data0, AS_UINT32(&DSI_REG[i]->DSI_RX_DATA0));
		DSI_OUTREG32(cmdq, &read_data1, AS_UINT32(&DSI_REG[i]->DSI_RX_DATA1));
		DSI_OUTREG32(cmdq, &read_data2, AS_UINT32(&DSI_REG[i]->DSI_RX_DATA2));
		DSI_OUTREG32(cmdq, &read_data3, AS_UINT32(&DSI_REG[i]->DSI_RX_DATA3));
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		DDPMSG(" DSI_CMDQ_SIZE : 0x%x\n",
			       DSI_REG[i]->DSI_CMDQ_SIZE.CMDQ_SIZE);
		DDPMSG(" DSI_CMDQ_DATA0 : 0x%x\n",
			       DSI_CMDQ_REG[i]->data[0].byte0);
		DDPMSG(" DSI_CMDQ_DATA1 : 0x%x\n",
			       DSI_CMDQ_REG[i]->data[0].byte1);
		DDPMSG(" DSI_CMDQ_DATA2 : 0x%x\n",
			       DSI_CMDQ_REG[i]->data[0].byte2);
		DDPMSG(" DSI_CMDQ_DATA3 : 0x%x\n",
			       DSI_CMDQ_REG[i]->data[0].byte3);
		DDPMSG(" DSI_RX_DATA0 : 0x%x\n",
			       *((uint32_t *)(&DSI_REG[i]->DSI_RX_DATA0)));
		DDPMSG(" DSI_RX_DATA1 : 0x%x\n",
			       *((uint32_t *)(&DSI_REG[i]->DSI_RX_DATA1)));
		DDPMSG(" DSI_RX_DATA2 : 0x%x\n",
			       *((uint32_t *)(&DSI_REG[i]->DSI_RX_DATA2)));
		DDPMSG(" DSI_RX_DATA3 : 0x%x\n",
			       *((uint32_t *)(&DSI_REG[i]->DSI_RX_DATA3)));

		DDPMSG("read_data0, %x,%x,%x,%x\n", read_data0.byte0, read_data0.byte1,
			read_data0.byte2, read_data0.byte3);
		DDPMSG("read_data1, %x,%x,%x,%x\n", read_data1.byte0, read_data1.byte1,
			read_data1.byte2, read_data1.byte3);
		DDPMSG("read_data2, %x,%x,%x,%x\n", read_data2.byte0, read_data2.byte1,
			read_data2.byte2, read_data2.byte3);
		DDPMSG("read_data3, %x,%x,%x,%x\n", read_data3.byte0, read_data3.byte1,
			read_data3.byte2, read_data3.byte3);
#endif

		packet_type = read_data0.byte0;

#ifdef DDI_DRV_DEBUG_LOG_ENABLE
		if (dsi_log_on)
			DDPMSG(" DSI read packet_type is 0x%x\n", packet_type);
#endif

		if (packet_type == 0x1A || packet_type == 0x1C) {
			recv_data_cnt = read_data0.byte1 + read_data0.byte2 * 16;
			if (recv_data_cnt > 10) {
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
				if (dsi_log_on)
					DDPMSG(" DSI read long packet data  exceeds 4 bytes\n");
#endif
				recv_data_cnt = 10;
			}

			if (recv_data_cnt > buffer_size) {
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
				if (dsi_log_on)
					DDPMSG(" DSI read long packet data  exceeds buffer size: %d\n",
						       buffer_size);
#endif
				recv_data_cnt = buffer_size;
			}
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
			if (dsi_log_on)
				DDPMSG(" DSI read long packet size: %d\n", recv_data_cnt);
#endif
			memcpy((void *)buffer, (void *)&read_data1, recv_data_cnt);
		} else {
#ifdef DDI_DRV_DEBUG_LOG_ENABLE
			if (dsi_log_on)
				DDPMSG(" DSI read short packet data  exceeds buffer size: %d\n",
					       buffer_size);
#endif
			recv_data_cnt = buffer_size;
			memcpy((void *)buffer, (void *)&read_data0.byte1, 2);
		}
	} while (packet_type != 0x1C && packet_type != 0x21 && packet_type != 0x22
		 && packet_type != 0x1A);

	return recv_data_cnt;
}

void DSI_set_cmdq_V2(enum DISP_MODULE_ENUM module, void *cmdq, unsigned cmd,
	unsigned char count, unsigned char *para_list, unsigned char force_update)
{
	uint32_t i = 0;
	int d = 0;
	unsigned long goto_addr, mask_para, set_para;
	struct DSI_T0_INS t0;
	struct DSI_T2_INS t2;
	struct DSI_VM_CMD_CON_REG vm_cmdq;

	for (d = DSI_MODULE_BEGIN(module); d <= DSI_MODULE_END(module); d++) {
		if (DSI_REG[d]->DSI_MODE_CTRL.MODE != 0) {	/* not in cmd mode */
			DISPCHECK("set cmdq in VDO mode in set_cmdq_V2\n");
			memset(&vm_cmdq, 0, sizeof(struct DSI_VM_CMD_CON_REG));
			DSI_READREG32((struct DSI_VM_CMD_CON_REG *), &vm_cmdq, &DSI_REG[d]->DSI_VM_CMD_CON);
			vm_cmdq.VM_CMD_EN = 1;
			if (cmd < 0xB0) {
				if (count > 1) {
					vm_cmdq.LONG_PKT = 1;
					vm_cmdq.CM_DATA_ID = DSI_DCS_LONG_PACKET_ID;
					vm_cmdq.CM_DATA_0 = count + 1;
					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_VM_CMD_CON,
							 AS_UINT32(&vm_cmdq));

					goto_addr =
						(unsigned long)(&DSI_VM_CMD_REG[d]->data[0].byte0);
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (cmd << ((goto_addr & 0x3) * 8));
					DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
							  set_para);

					for (i = 0; i < count; i++) {
						goto_addr =
							(unsigned long)(&DSI_VM_CMD_REG[d]->data[0].
									byte1) + i;
						mask_para = (0xFF << ((goto_addr & 0x3) * 8));
						set_para =
							(para_list[i] << ((goto_addr & 0x3) * 8));
						DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
								  set_para);
					}
				} else {
					vm_cmdq.LONG_PKT = 0;
					vm_cmdq.CM_DATA_0 = cmd;
					if (count) {
						vm_cmdq.CM_DATA_ID = DSI_DCS_SHORT_PACKET_ID_1;
						vm_cmdq.CM_DATA_1 = para_list[0];
					} else {
						vm_cmdq.CM_DATA_ID = DSI_DCS_SHORT_PACKET_ID_0;
						vm_cmdq.CM_DATA_1 = 0;
					}
					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_VM_CMD_CON,
							 AS_UINT32(&vm_cmdq));
				}
			} else {
				if (count > 1) {
					vm_cmdq.LONG_PKT = 1;
					vm_cmdq.CM_DATA_ID = DSI_GERNERIC_LONG_PACKET_ID;
					vm_cmdq.CM_DATA_0 = count + 1;
					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_VM_CMD_CON,
							 AS_UINT32(&vm_cmdq));

					goto_addr =
						(unsigned long)(&DSI_VM_CMD_REG[d]->data[0].byte0);
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (cmd << ((goto_addr & 0x3) * 8));
					DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
							  set_para);

					for (i = 0; i < count; i++) {
						goto_addr =
							(unsigned long)(&DSI_VM_CMD_REG[d]->data[0].
									byte1) + i;
						mask_para = (0xFF << ((goto_addr & 0x3) * 8));
						set_para =
							(para_list[i] << ((goto_addr & 0x3) * 8));
						DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
								  set_para);
					}
				} else {
					vm_cmdq.LONG_PKT = 0;
					vm_cmdq.CM_DATA_0 = cmd;
					if (count) {
						vm_cmdq.CM_DATA_ID = DSI_GERNERIC_SHORT_PACKET_ID_2;
						vm_cmdq.CM_DATA_1 = para_list[0];
					} else {
						vm_cmdq.CM_DATA_ID = DSI_GERNERIC_SHORT_PACKET_ID_1;
						vm_cmdq.CM_DATA_1 = 0;
					}
					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_VM_CMD_CON,
							 AS_UINT32(&vm_cmdq));
				}
			}
		} else {
			DSI_WaitForNotBusy(module, cmdq);
			if (cmd < 0xB0) {
				if (count > 1) {
					t2.CONFG = 2;
					t2.Data_ID = DSI_DCS_LONG_PACKET_ID;
					t2.WC16 = count + 1;

					DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[d]->data[0],
							 AS_UINT32(&t2));

					goto_addr =
						(unsigned long)(&DSI_CMDQ_REG[d]->data[1].byte0);
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (cmd << ((goto_addr & 0x3) * 8));
					DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
							  set_para);

					for (i = 0; i < count; i++) {
						goto_addr =
							(unsigned long)(&DSI_CMDQ_REG[d]->data[1].
									byte1) + i;
						mask_para = (0xFF << ((goto_addr & 0x3) * 8));
						set_para =
							(para_list[i] << ((goto_addr & 0x3) * 8));
						DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
								  set_para);
					}

					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_CMDQ_SIZE,
							 2 + (count) / 4);
				} else {
					t0.CONFG = 0;
					t0.Data0 = cmd;
					if (count) {
						t0.Data_ID = DSI_DCS_SHORT_PACKET_ID_1;
						t0.Data1 = para_list[0];
					} else {
						t0.Data_ID = DSI_DCS_SHORT_PACKET_ID_0;
						t0.Data1 = 0;
					}

					DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[d]->data[0],
							 AS_UINT32(&t0));
					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_CMDQ_SIZE, 1);
				}
			} else {
				if (count > 1) {
					t2.CONFG = 2;
					t2.Data_ID = DSI_GERNERIC_LONG_PACKET_ID;
					t2.WC16 = count + 1;

					DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[d]->data[0],
							 AS_UINT32(&t2));

					goto_addr =
						(unsigned long)(&DSI_CMDQ_REG[d]->data[1].byte0);
					mask_para = (0xFF << ((goto_addr & 0x3) * 8));
					set_para = (cmd << ((goto_addr & 0x3) * 8));
					DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
							  set_para);

					for (i = 0; i < count; i++) {
						goto_addr =
							(unsigned long)(&DSI_CMDQ_REG[d]->data[1].
									byte1) + i;
						mask_para = (0xFF << ((goto_addr & 0x3) * 8));
						set_para =
							(para_list[i] << ((goto_addr & 0x3) * 8));
						DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para,
								  set_para);
					}

					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_CMDQ_SIZE,
							 2 + (count) / 4);

				} else {
					t0.CONFG = 0;
					t0.Data0 = cmd;
					if (count) {
						t0.Data_ID = DSI_GERNERIC_SHORT_PACKET_ID_2;
						t0.Data1 = para_list[0];
					} else {
						t0.Data_ID = DSI_GERNERIC_SHORT_PACKET_ID_1;
						t0.Data1 = 0;
					}
					DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[d]->data[0],
							 AS_UINT32(&t0));
					DSI_OUTREG32(cmdq, &DSI_REG[d]->DSI_CMDQ_SIZE, 1);
				}
			}
		}
	}

	if (DSI_REG[0]->DSI_MODE_CTRL.MODE != 0) { /* not in cmd mode */
		/* start DSI VM CMDQ */
		if (force_update)
			DSI_EnableVM_CMD(module, cmdq);
	} else {
		if (force_update) {
			DSI_Start(module, cmdq);
			DSI_WaitForNotBusy(module, cmdq);
		}
	}
}

void DSI_set_cmdq_V3(enum DISP_MODULE_ENUM module, void *cmdq,
	LCM_setting_table_V3 *para_tbl, unsigned int size, unsigned char force_update)
{
	uint32_t i;
	unsigned long goto_addr, mask_para, set_para;
	/* uint32_t fbPhysAddr, fbVirAddr; */
	struct DSI_T0_INS t0;
	/* struct DSI_T1_INS t1; */
	struct DSI_T2_INS t2;

	uint32_t index = 0;

	unsigned char data_id, cmd, count;
	unsigned char *para_list;

	do {
		data_id = para_tbl[index].id;
		cmd = para_tbl[index].cmd;
		count = para_tbl[index].count;
		para_list = para_tbl[index].para_list;

		if (data_id == REGFLAG_ESCAPE_ID && cmd == REGFLAG_DELAY_MS_V3) {
			udelay(1000 * count);
			pr_debug("DSI DSI_set_cmdq_V3[%d]. Delay %d (ms)\n", index, count);

			continue;
		}

		DSI_WaitForNotBusy(module, cmdq);
		/* for(i = 0; i < sizeof(DSI_CMDQ_REG->data0) / sizeof(DSI_CMDQ); i++) */
		/* OUTREG32(&DSI_CMDQ_REG->data0[i], 0); */
		/* memset(&DSI_CMDQ_REG->data[0], 0, sizeof(DSI_CMDQ_REG->data[0])); */
		DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[0]->data[0], 0);

		if (count > 1) {
			t2.CONFG = 2;
			t2.Data_ID = data_id;
			t2.WC16 = count + 1;

			DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[0]->data[0].byte0, AS_UINT32(&t2));

			goto_addr = (unsigned long) (&DSI_CMDQ_REG[0]->data[1].byte0);
			mask_para = (0xFF << ((goto_addr & 0x3) * 8));
			set_para = (cmd << ((goto_addr & 0x3) * 8));
			DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para, set_para);

			for (i = 0; i < count; i++) {
				goto_addr =
				    (unsigned long) (&DSI_CMDQ_REG[0]->data[1].byte1) + i;
				mask_para = (0xFF << ((goto_addr & 0x3) * 8));
				set_para =
				    (para_list[i] << ((goto_addr & 0x3) * 8));
				DSI_MASKREG32(cmdq, goto_addr & (~0x3), mask_para, set_para);
			}

			DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_CMDQ_SIZE, 2 + (count) / 4);
		} else {
			t0.CONFG = 0;
			t0.Data0 = cmd;
			if (count) {
				t0.Data_ID = data_id;
				t0.Data1 = para_list[0];
			} else {
				t0.Data_ID = data_id;
				t0.Data1 = 0;
			}
			DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[0]->data[0], AS_UINT32(&t0));
			DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_CMDQ_SIZE, 1);
		}

		for (i = 0; i < AS_UINT32(&DSI_REG[0]->DSI_CMDQ_SIZE); i++)
			DDPMSG("DSI_set_cmdq_V3[%d]. DSI_CMDQ+%04x : 0x%08x\n",
				       index, i * 4,
				       DISP_REG_GET(DSI_PHY_REG[0] + 0x180 + i * 4));

		if (force_update) {
			DSI_Start(module, cmdq);
			for (i = 0; i < 10; i++)
				;
			DSI_WaitForNotBusy(module, cmdq);
		}
	} while (++index < size);
}

void DSI_set_cmdq(enum DISP_MODULE_ENUM module, void *cmdq,
	unsigned int *pdata, unsigned int queue_size, unsigned char force_update)
{
	uint32_t i;

	ASSERT(queue_size <= 16);

	DSI_WaitForNotBusy(module, cmdq);

	for (i = 0; i < queue_size; i++)
		DSI_OUTREG32(cmdq, &DSI_CMDQ_REG[0]->data[i], AS_UINT32((pdata+i)));

	DSI_OUTREG32(cmdq, &DSI_REG[0]->DSI_CMDQ_SIZE, queue_size);

	if (force_update) {
		DISPMSG("DSI_force_update\n");
		DSI_Start(module, cmdq);
		DSI_WaitForNotBusy(module, cmdq);
	}
}

void _copy_dsi_params(LCM_DSI_PARAMS *src, LCM_DSI_PARAMS *dst)
{
	memcpy((LCM_DSI_PARAMS *) dst, (LCM_DSI_PARAMS *) src, sizeof(LCM_DSI_PARAMS));
}



int DSI_Send_ROI(enum DISP_MODULE_ENUM module, void *handle, unsigned int x, unsigned int y,
		 unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	DSI_set_cmdq(module, handle, data_array, 3, 1);
	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	DSI_set_cmdq(module, handle, data_array, 3, 1);
	DDPMSG("DSI_Send_ROI Done!\n");

	/* data_array[0]= 0x002c3909; */
	/* DSI_set_cmdq(module, handle, data_array, 1, 0); */
	return 0;
}



static void lcm_set_reset_pin(uint32_t value)
{
#if 1
	DSI_OUTREG32(NULL, DISPSYS_CONFIG_BASE + 0x150, value);
#else
#if !defined(CONFIG_MTK_LEGACY)
	if (value)
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	else
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
#endif
#endif
}

static void lcm_udelay(uint32_t us)
{
	udelay(us);
}

static void lcm_mdelay(uint32_t ms)
{
	if (ms < 10) {
		udelay(ms * 1000);
	} else {
		msleep(ms);
		/* udelay(ms*1000); */
	}
}

void DSI_set_cmdq_V2_DSI0(void *cmdq, unsigned cmd, unsigned char count, unsigned char *para_list,
			  unsigned char force_update)
{
	DSI_set_cmdq_V2(DISP_MODULE_DSI0, cmdq, cmd, count, para_list, force_update);
}

void DSI_set_cmdq_V2_Wrapper_DSI0(unsigned cmd, unsigned char count, unsigned char *para_list,
				  unsigned char force_update)
{
	DSI_set_cmdq_V2(DISP_MODULE_DSI0, NULL, cmd, count, para_list, force_update);
}

void DSI_set_cmdq_V3_Wrapper_DSI0(LCM_setting_table_V3 *para_tbl, unsigned int size,
				  unsigned char force_update)
{
	DSI_set_cmdq_V3(DISP_MODULE_DSI0, NULL, para_tbl, size, force_update);
}

void DSI_set_cmdq_wrapper_DSI0(unsigned int *pdata, unsigned int queue_size,
			       unsigned char force_update)
{
	DSI_set_cmdq(DISP_MODULE_DSI0, NULL, pdata, queue_size, force_update);
}

unsigned int DSI_dcs_read_lcm_reg_v2_wrapper_DSI0(uint8_t cmd, uint8_t *buffer, uint8_t buffer_size)
{
	return DSI_dcs_read_lcm_reg_v2(DISP_MODULE_DSI0, NULL, cmd, buffer, buffer_size);
}

static LCM_UTIL_FUNCS lcm_utils_dsi0;

int ddp_dsi_set_lcm_utils(enum DISP_MODULE_ENUM module, LCM_DRIVER *lcm_drv)
{
	LCM_UTIL_FUNCS *utils = NULL;

	if (lcm_drv == NULL) {
		DISPERR("lcm_drv is null\n");
		return -1;
	}

	if (module == DISP_MODULE_DSI0)
		utils = &lcm_utils_dsi0;
	else {
		DISPERR("wrong module: %d\n", module);
		return -1;
	}

	utils->set_reset_pin = lcm_set_reset_pin;
	utils->udelay = lcm_udelay;
	utils->mdelay = lcm_mdelay;
	if (module == DISP_MODULE_DSI0) {
		utils->dsi_set_cmdq = DSI_set_cmdq_wrapper_DSI0;
		utils->dsi_set_cmdq_V2 = DSI_set_cmdq_V2_Wrapper_DSI0;
		utils->dsi_set_cmdq_V3 = DSI_set_cmdq_V3_Wrapper_DSI0;
		utils->dsi_dcs_read_lcm_reg_v2 = DSI_dcs_read_lcm_reg_v2_wrapper_DSI0;
		utils->dsi_set_cmdq_V22 = DSI_set_cmdq_V2_DSI0;
	}
#ifdef DEVICE_TREE
	utils->set_gpio_out = (int (*)(unsigned int, unsigned int))mt_set_gpio_out;
	utils->set_gpio_mode = (int (*)(unsigned int, unsigned int))mt_set_gpio_mode;
	utils->set_gpio_dir = (int (*)(unsigned int, unsigned int))mt_set_gpio_dir;
	utils->set_gpio_pull_enable = (int (*)(unsigned int, unsigned char))mt_set_gpio_pull_enable;
#else
	/* TODO: attach replacements of these functions if using kernel standardization... */
	utils->set_gpio_out = 0;
	utils->set_gpio_mode = 0;
	utils->set_gpio_dir = 0;
	utils->set_gpio_pull_enable = 0;
#endif


	lcm_drv->set_util_funcs(utils);

	return 0;
}


static int dsi0_te_enable = 1;

int DSI_ChangeClk(enum DISP_MODULE_ENUM module, void *cmdq, uint32_t clk)
{
	LCM_DSI_PARAMS lcm_params_for_clk_setting;

	if (clk > 1000)
		return DSI_STATUS_ERROR;
	memcpy((void *)&lcm_params_for_clk_setting, (void *)&dsi_lcm_params, sizeof(LCM_DSI_PARAMS));

	for (lcm_params_for_clk_setting.pll_div2 = 15;
		 lcm_params_for_clk_setting.pll_div2 > 0;
		 lcm_params_for_clk_setting.pll_div2--) {
		for (lcm_params_for_clk_setting.pll_div1 = 0;
			 lcm_params_for_clk_setting.pll_div1 < 39;
			 lcm_params_for_clk_setting.pll_div1++) {
			if ((13 * (lcm_params_for_clk_setting.pll_div1 + 1) /
				 lcm_params_for_clk_setting.pll_div2) >= clk)
				goto end;
		}
	}

	if (lcm_params_for_clk_setting.pll_div2 == 0) {
		for (lcm_params_for_clk_setting.pll_div1 = 0;
			 lcm_params_for_clk_setting.pll_div1 < 39;
			 lcm_params_for_clk_setting.pll_div1++) {
			if ((26 * (lcm_params_for_clk_setting.pll_div1 + 1)) >= clk)
				goto end;
		}
	}

end:
	DSI_WaitForNotBusy(module, cmdq);
	DSI_PHY_clk_setting(module, cmdq, &lcm_params_for_clk_setting);
	DSI_PHY_TIMCONFIG(&lcm_params_for_clk_setting);
	return DSI_STATUS_OK;

}

int ddp_dsi_init(enum DISP_MODULE_ENUM module, void *cmdq)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	enum DSI_STATUS ret = DSI_STATUS_OK;
#endif
	int i = 0;

	DISPFUNC();

	DSI_REG[0] = (struct DSI_REGS *) DISPSYS_DSI0_BASE;
	DSI_PHY_REG[0] = (struct DSI_PHY_REGS *) MIPITX_BASE;
	DSI_CMDQ_REG[0] = (struct DSI_CMDQ_REGS *) (DISPSYS_DSI0_BASE + 0x180);
	DSI_VM_CMD_REG[0] = (struct DSI_VM_CMDQ_REGS *) (DISPSYS_DSI0_BASE + 0x134);
	dsi_glitch_enable = false;
	wait_vm_done_irq = true;
	cmdq_forcb = cmdq;
	memset(&_dsi_context, 0, sizeof(_dsi_context));

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		init_waitqueue_head(&_dsi_cmd_done_wait_queue[i]);
		init_waitqueue_head(&_dsi_dcs_read_wait_queue[i]);
		init_waitqueue_head(&_dsi_wait_bta_te[i]);
		init_waitqueue_head(&_dsi_wait_ext_te[i]);
		init_waitqueue_head(&_dsi_wait_vm_done_queue[i]);
		init_waitqueue_head(&_dsi_wait_vm_cmd_done_queue[i]);
		init_waitqueue_head(&_dsi_wait_sleep_out_done_queue[i]);
		DISPCHECK("dsi%d initializing\n", i);
	}

	disp_register_module_irq_callback(DISP_MODULE_DSI0, _DSI_INTERNAL_IRQ_Handler);

#ifndef CONFIG_FPGA_EARLY_PORTING
	if (MIPITX_IsEnabled(module, cmdq)) {
		s_isDsiPowerOn = true;
#ifdef ENABLE_CLK_MGR
		if (module == DISP_MODULE_DSI0) {
#ifndef CONFIG_MTK_CLKMGR
			ret += ddp_clk_prepare_enable(TOP_RG_MIPI_26M_DBG);
			ret += ddp_clk_enable(DISP1_DSI0_ENGINE);
			ret += ddp_clk_enable(DISP1_DSI0_DIGITAL);
#endif
			if (ret > 0)
				DDPERR("DSI0 power manager API return false\n");
		}
#endif
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[0]->DSI_INTEN, CMD_DONE, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[0]->DSI_INTEN, RD_RDY, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[0]->DSI_INTEN, VM_DONE, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[0]->DSI_INTEN, VM_CMD_DONE, 1);
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[0]->DSI_INTEN, SLEEPOUT_DONE, 1);

		DSI_BackupRegisters(module, NULL);
	}
#endif

	return DSI_STATUS_OK;
}

int ddp_dsi_deinit(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	enum DSI_STATUS ret = ddp_dsi_power_off(module, cmdq_handle);

	ASSERT(ret == DSI_STATUS_OK);

	return DSI_STATUS_OK;
}

void _dump_dsi_params(LCM_DSI_PARAMS *dsi_config)
{
	if (dsi_config) {
		switch (dsi_config->mode) {
		case CMD_MODE:
			DISPCHECK("[DDPDSI] DSI Mode: CMD_MODE\n");
			break;
		case SYNC_PULSE_VDO_MODE:
			DISPCHECK("[DDPDSI] DSI Mode: SYNC_PULSE_VDO_MODE\n");
			break;
		case SYNC_EVENT_VDO_MODE:
			DISPCHECK("[DDPDSI] DSI Mode: SYNC_EVENT_VDO_MODE\n");
			break;
		case BURST_VDO_MODE:
			DISPCHECK("[DDPDSI] DSI Mode: BURST_VDO_MODE\n");
			break;
		default:
			DISPCHECK("[DDPDSI] DSI Mode: Unknown\n");
			break;
		}

		/* DISPCHECK("[DDPDSI] LANE_NUM: %d,data_format: %d,vertical_sync_active: %d\n",*/
		/* dsi_config->LANE_NUM,dsi_config->data_format); */
		DISPCHECK
			("[DDPDSI] vact: %d, vbp: %d, vfp: %d, vact_line: %d, hact: %d, hbp: %d, hfp: %d, hblank: %d\n",
		     dsi_config->vertical_sync_active, dsi_config->vertical_backporch,
		     dsi_config->vertical_frontporch, dsi_config->vertical_active_line,
		     dsi_config->horizontal_sync_active, dsi_config->horizontal_backporch,
		     dsi_config->horizontal_frontporch, dsi_config->horizontal_blanking_pixel);
		DISPCHECK
			("[DDPDSI] pll_select: %d, pll_div1: %d, pll_div2: %d, fbk_div: %d,fbk_sel: %d, rg_bir: %d\n",
		     dsi_config->pll_select, dsi_config->pll_div1, dsi_config->pll_div2,
		     dsi_config->fbk_div, dsi_config->fbk_sel, dsi_config->rg_bir);
		DISPCHECK(
			"[DDPDSI] rg_bic: %d, rg_bp: %d, PLL_CLOCK: %d, dsi_clock: %d, ssc_range: %d, ssc_disable: %d, compatibility_for_nvk: %d, cont_clock: %d\n",
		     dsi_config->rg_bic, dsi_config->rg_bp, dsi_config->PLL_CLOCK,
		     dsi_config->dsi_clock, dsi_config->ssc_range, dsi_config->ssc_disable,
		     dsi_config->compatibility_for_nvk, dsi_config->cont_clock);
		DISPCHECK
			("[DDPDSI] lcm_ext_te_enable: %d, noncont_clock: %d, noncont_clock_period: %d\n",
		     dsi_config->lcm_ext_te_enable, dsi_config->noncont_clock,
		     dsi_config->noncont_clock_period);
	}
}

static void DSI_PHY_CLK_LP_PerLine_config(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
					  LCM_DSI_PARAMS *dsi_params)
{
	int i;
	struct DSI_PHY_TIMCON0_REG timcon0;	/* LPX */
	struct DSI_PHY_TIMCON2_REG timcon2;	/* CLK_HS_TRAIL, CLK_HS_ZERO */
	struct DSI_PHY_TIMCON3_REG timcon3;	/* CLK_HS_EXIT, CLK_HS_POST, CLK_HS_PREP */
	struct DSI_HSA_WC_REG hsa;
	struct DSI_HBP_WC_REG hbp;
	struct DSI_HFP_WC_REG hfp, new_hfp;
	struct DSI_BLLP_WC_REG bllp;
	struct DSI_PSCTRL_REG ps;
	uint32_t hstx_ckl_wc, new_hstx_ckl_wc;
	uint32_t v_a, v_b, v_c, lane_num;
	LCM_DSI_MODE_CON dsi_mode;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		lane_num = dsi_params->LANE_NUM;
		dsi_mode = dsi_params->mode;

		if (dsi_mode == CMD_MODE)
			continue;

		/* vdo mode */
		DSI_OUTREG32(cmdq, &hsa, AS_UINT32(&DSI_REG[i]->DSI_HSA_WC));
		DSI_OUTREG32(cmdq, &hbp, AS_UINT32(&DSI_REG[i]->DSI_HBP_WC));
		DSI_OUTREG32(cmdq, &hfp, AS_UINT32(&DSI_REG[i]->DSI_HFP_WC));
		DSI_OUTREG32(cmdq, &bllp, AS_UINT32(&DSI_REG[i]->DSI_BLLP_WC));
		DSI_OUTREG32(cmdq, &ps, AS_UINT32(&DSI_REG[i]->DSI_PSCTRL));
		DSI_OUTREG32(cmdq, &hstx_ckl_wc, AS_UINT32(&DSI_REG[i]->DSI_HSTX_CKL_WC));
		DSI_OUTREG32(cmdq, &timcon0, AS_UINT32(&DSI_REG[i]->DSI_PHY_TIMECON0));
		DSI_OUTREG32(cmdq, &timcon2, AS_UINT32(&DSI_REG[i]->DSI_PHY_TIMECON2));
		DSI_OUTREG32(cmdq, &timcon3, AS_UINT32(&DSI_REG[i]->DSI_PHY_TIMECON3));

		/* 1. sync_pulse_mode */
		/* Total    WC(A) = HSA_WC + HBP_WC + HFP_WC + PS_WC + 32 */
		/* CLK init WC(B) = (CLK_HS_EXIT + LPX + CLK_HS_PREP + CLK_HS_ZERO)*lane_num */
		/* CLK end  WC(C) = (CLK_HS_POST + CLK_HS_TRAIL)*lane_num */
		/* HSTX_CKLP_WC = A - B */
		/* Limitation: B + C < HFP_WC */
		if (dsi_mode == SYNC_PULSE_VDO_MODE) {
			v_a = hsa.HSA_WC + hbp.HBP_WC + hfp.HFP_WC + ps.DSI_PS_WC + 32;
			v_b =
			    (timcon3.CLK_HS_EXIT + timcon0.LPX + timcon3.CLK_HS_PRPR +
			     timcon2.CLK_ZERO) * lane_num;
			v_c = (timcon3.CLK_HS_POST + timcon2.CLK_TRAIL) * lane_num;

			DISPDBG("===>v_a-v_b=0x%x,HSTX_CKLP_WC=0x%x\n", (v_a - v_b), hstx_ckl_wc);
/* DISPCHECK("===>v_b+v_c=0x%x,HFP_WC=0x%x\n",(v_b+v_c),hfp); */
			DISPDBG("===>Will Reconfig in order to fulfill LP clock lane per line\n");

			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HFP_WC, (v_b + v_c + DIFF_CLK_LANE_LP));
			DSI_OUTREG32(cmdq, &new_hfp, AS_UINT32(&DSI_REG[i]->DSI_HFP_WC));
			v_a = hsa.HSA_WC + hbp.HBP_WC + new_hfp.HFP_WC + ps.DSI_PS_WC + 32;
			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HSTX_CKL_WC, (v_a - v_b));
			DSI_OUTREG32(cmdq, &new_hstx_ckl_wc,
				     AS_UINT32(&DSI_REG[i]->DSI_HSTX_CKL_WC));
			DISPDBG("===>new HSTX_CKL_WC=0x%x, HFP_WC=0x%x\n", new_hstx_ckl_wc,
				  new_hfp.HFP_WC);
		}
		/* 2. sync_event_mode */
		/* Total    WC(A) = HBP_WC + HFP_WC + PS_WC + 26 */
		/* CLK init WC(B) = (CLK_HS_EXIT + LPX + CLK_HS_PREP + CLK_HS_ZERO)*lane_num */
		/* CLK end  WC(C) = (CLK_HS_POST + CLK_HS_TRAIL)*lane_num */
		/* HSTX_CKLP_WC = A - B */
		/* Limitation: B + C < HFP_WC */
		else if (dsi_mode == SYNC_EVENT_VDO_MODE) {
			v_a = hbp.HBP_WC + hfp.HFP_WC + ps.DSI_PS_WC + 26;
			v_b =
			    (timcon3.CLK_HS_EXIT + timcon0.LPX + timcon3.CLK_HS_PRPR +
			     timcon2.CLK_ZERO) * lane_num;
			v_c = (timcon3.CLK_HS_POST + timcon2.CLK_TRAIL) * lane_num;

			DISPDBG("===>v_a-v_b=0x%x,HSTX_CKLP_WC=0x%x\n", (v_a - v_b), hstx_ckl_wc);
/* DISPCHECK("===>v_b+v_c=0x%x,HFP_WC=0x%x\n",(v_b+v_c),hfp); */
			DISPDBG("===>Will Reconfig in order to fulfill LP clock lane per line\n");

			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HFP_WC, (v_b + v_c + DIFF_CLK_LANE_LP));
			DSI_OUTREG32(cmdq, &new_hfp, AS_UINT32(&DSI_REG[i]->DSI_HFP_WC));
			v_a = hbp.HBP_WC + new_hfp.HFP_WC + ps.DSI_PS_WC + 26;
			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HSTX_CKL_WC, (v_a - v_b));
			DSI_OUTREG32(cmdq, &new_hstx_ckl_wc,
				     AS_UINT32(&DSI_REG[i]->DSI_HSTX_CKL_WC));
			DISPDBG("===>new HSTX_CKL_WC=0x%x, HFP_WC=0x%x\n", new_hstx_ckl_wc,
				  new_hfp.HFP_WC);

		}
		/* 3. burst_mode */
		/* Total    WC(A) = HBP_WC + HFP_WC + PS_WC + BLLP_WC + 32 */
		/* CLK init WC(B) = (CLK_HS_EXIT + LPX + CLK_HS_PREP + CLK_HS_ZERO)*lane_num */
		/* CLK end  WC(C) = (CLK_HS_POST + CLK_HS_TRAIL)*lane_num */
		/* HSTX_CKLP_WC = A - B */
		/* Limitation: B + C < HFP_WC */
		else if (dsi_mode == BURST_VDO_MODE) {
			v_a = hbp.HBP_WC + hfp.HFP_WC + ps.DSI_PS_WC + bllp.BLLP_WC + 32;
			v_b =
			    (timcon3.CLK_HS_EXIT + timcon0.LPX + timcon3.CLK_HS_PRPR +
			     timcon2.CLK_ZERO) * lane_num;
			v_c = (timcon3.CLK_HS_POST + timcon2.CLK_TRAIL) * lane_num;

			DISPDBG("===>v_a-v_b=0x%x,HSTX_CKLP_WC=0x%x\n", (v_a - v_b), hstx_ckl_wc);
			/* DISPCHECK("===>v_b+v_c=0x%x,HFP_WC=0x%x\n",(v_b+v_c),hfp); */
			DISPDBG("===>Will Reconfig in order to fulfill LP clock lane per line\n");

			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HFP_WC, (v_b + v_c + DIFF_CLK_LANE_LP));
			DSI_OUTREG32(cmdq, &new_hfp, AS_UINT32(&DSI_REG[i]->DSI_HFP_WC));
			v_a = hbp.HBP_WC + new_hfp.HFP_WC + ps.DSI_PS_WC + bllp.BLLP_WC + 32;
			DSI_OUTREG32(cmdq, &DSI_REG[i]->DSI_HSTX_CKL_WC, (v_a - v_b));
			DSI_OUTREG32(cmdq, &new_hstx_ckl_wc,
				     AS_UINT32(&DSI_REG[i]->DSI_HSTX_CKL_WC));
			DISPDBG("===>new HSTX_CKL_WC=0x%x, HFP_WC=0x%x\n", new_hstx_ckl_wc,
				  new_hfp.HFP_WC);
		}
	}

}

int ddp_dsi_config(enum DISP_MODULE_ENUM module, struct disp_ddp_path_config *config, void *cmdq)
{
	int i = 0;
	LCM_DSI_PARAMS *dsi_config = &(config->dispif_config.dsi);

	if (!config->dst_dirty) {
		if (atomic_read(&PMaster_enable) == 0)
			return 0;
	}
	DISPFUNC();
	DISPDBG("===>run here 00 Pmaster: clk:%d\n", _dsi_context[0].dsi_params.PLL_CLOCK);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		_copy_dsi_params(dsi_config, &(_dsi_context[i].dsi_params));
		_copy_dsi_params(dsi_config, &(dsi_lcm_params));
		_dsi_context[i].lcm_width = config->dst_w;
		_dsi_context[i].lcm_height = config->dst_h;
		_dump_dsi_params(&(_dsi_context[i].dsi_params));
		if (dsi_config->mode == CMD_MODE) {
			/*enable TE in cmd mode */
			DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[i]->DSI_INTEN, TE_RDY, 1);
		}
	}
	DISPCHECK("===>01Pmaster: clk:%d\n", _dsi_context[0].dsi_params.PLL_CLOCK);
	if (dsi_config->mode != CMD_MODE)
		dsi_currect_mode = 1;

#ifdef CONFIG_FPGA_EARLY_PORTING
	return 0;	/* DONOT config again when LK has already done it*/
#endif

	if ((MIPITX_IsEnabled(module, cmdq)) /*&& (atomic_read(&PMaster_enable) == 0)*/) {
		DISPDBG("mipitx is already init\n");
		if (dsi_force_config)
			goto force_config;
		else
			goto done;
	} else {
		DISPDBG("MIPITX is not inited, will config mipitx clock now\n");
		DISPDBG("===>Pmaster:CLK SETTING??==> clk:%d\n",
			  _dsi_context[0].dsi_params.PLL_CLOCK);
		DSI_PHY_clk_setting(module, NULL, dsi_config);
	}

force_config:
	if (dsi_config->mode == CMD_MODE
	    || ((dsi_config->switch_mode_enable == 1)
		&& (dsi_config->switch_mode == CMD_MODE)))
		DSI_OUTREGBIT(cmdq, struct DSI_INT_ENABLE_REG, DSI_REG[0]->DSI_INTEN, TE_RDY, 1);

	/* DSI_Reset(module, cmdq_handle); */
	DSI_TXRX_Control(module, cmdq, dsi_config);
	DSI_PS_Control(module, cmdq, dsi_config, config->dst_w, config->dst_h);
	DSI_PHY_TIMCONFIG(dsi_config);
	DSI_EnableClk(module, cmdq);

	if (dsi_config->mode != CMD_MODE
	    || ((dsi_config->switch_mode_enable == 1) && (dsi_config->switch_mode != CMD_MODE))) {
		DSI_Config_VDO_Timing(module, cmdq, dsi_config);
		DSI_Set_VM_CMD(module, cmdq);
	}
	/* Enable clk low power per Line ; */
	if (dsi_config->clk_lp_per_line_enable)
		DSI_PHY_CLK_LP_PerLine_config(module, cmdq, dsi_config);
done:
	return 0;
}

int ddp_dsi_start(enum DISP_MODULE_ENUM module, void *cmdq)
{
	DISPFUNC();
	DSI_Send_ROI(module, cmdq, 0, 0, _dsi_context[0].lcm_width,
				_dsi_context[0].lcm_height);
	DSI_SetMode(module, cmdq, _dsi_context[0].dsi_params.mode);
	DSI_clk_HS_mode(module, cmdq, true);
	return DSI_STATUS_OK;
}

int ddp_dsi_stop(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int i = 0;
	unsigned int tmp = 0;

	DISPFUNC();

	if (_dsi_is_video_mode(module)) {
		DISPMSG("dsi is video mode\n");
		DSI_SetMode(module, cmdq_handle, CMD_MODE);

		i = DSI_MODULE_BEGIN(module);
		while (1) {
			tmp = INREG32(&DSI_REG[i]->DSI_INTSTA);
			if (!(tmp & 0x80000000))
				break;
		}

		i = DSI_MODULE_END(module);
		while (1) {
			DISPMSG("dsi%d is busy\n", i);
			tmp = INREG32(&DSI_REG[i]->DSI_INTSTA);
			if (!(tmp & 0x80000000))
				break;
		}
		DSI_clk_HSLP_mode(module, cmdq_handle);
	} else {
		DISPMSG("dsi is cmd mode\n");
		/* TODO: modify this with wait event */
		DSI_WaitForNotBusy(module, cmdq_handle);
		/*DSI clk lane enter LP at this time when Clktrail not deploy at trigger loop */
		/*if (gEnable_DSI_ClkTrail)*/
			DSI_clk_HS_mode(module, cmdq_handle, false);
		/*else*/
		/*	DSI_clk_HSLP_mode(module, cmdq_handle);*/
	}
	return 0;
}

int ddp_dsi_switch_lcm_mode(enum DISP_MODULE_ENUM module, void *params)
{
	int i = 0;
	LCM_DSI_MODE_SWITCH_CMD lcm_cmd = *((LCM_DSI_MODE_SWITCH_CMD *) (params));
	int mode = (int)(lcm_cmd.mode);

	if (dsi_currect_mode == mode) {
		DDPMSG
		    ("[ddp_dsi_switch_mode] not need switch mode, current mode = %d, switch to %d\n",
		     dsi_currect_mode, mode);
		return 0;
	}
	if (lcm_cmd.cmd_if == (unsigned int)LCM_INTERFACE_DSI0)
		i = 0;
	else if (lcm_cmd.cmd_if == (unsigned int)LCM_INTERFACE_DSI1)
		i = 1;
	else {
		DDPERR("dsi switch not support this cmd IF:%d\n", lcm_cmd.cmd_if);
		return -1;
	}

	if (mode == 0) {	/* V2C */
/* DSI_OUTREGBIT(NULL, DSI_INT_ENABLE_REG,DSI_REG[i]->DSI_INTEN,EXT_TE,1); */
		DSI_OUTREG32(NULL, (unsigned long)(DSI_REG[i]) + 0x130,
			     0x00001521 | (lcm_cmd.addr << 16) | (lcm_cmd.val[0] << 24));
		DSI_OUTREGBIT(NULL, struct DSI_START_REG, DSI_REG[i]->DSI_START, VM_CMD_START, 0);
		DSI_OUTREGBIT(NULL, struct DSI_START_REG, DSI_REG[i]->DSI_START, VM_CMD_START, 1);
		wait_vm_cmd_done = false;
		wait_event_interruptible(_dsi_wait_vm_cmd_done_queue[i], wait_vm_cmd_done);
	}
	return 0;
}

int ddp_dsi_switch_mode(enum DISP_MODULE_ENUM module, void *cmdq_handle, void *params)
{
	int i = 0;
	LCM_DSI_MODE_SWITCH_CMD lcm_cmd = *((LCM_DSI_MODE_SWITCH_CMD *) (params));
	int mode = (int)(lcm_cmd.mode);

	if (dsi_currect_mode == mode) {
		DDPMSG
		    ("[ddp_dsi_switch_mode] not need switch mode, current mode = %d, switch to %d\n",
		     dsi_currect_mode, mode);
		return 0;
	}
	if (lcm_cmd.cmd_if == (unsigned int)LCM_INTERFACE_DSI0)
		i = 0;
	else {
		DDPERR("dsi switch not support this cmd IF:%d\n", lcm_cmd.cmd_if);
		return -1;
	}

	if (mode == 0) {	/* V2C */
#if 1
		wait_vm_done_irq = false;
		DSI_SetSwitchMode(module, cmdq_handle, 0);
		DSI_OUTREG32(cmdq_handle, (unsigned long)(DSI_REG[i]) + 0x130,
			     0x00001539 | (lcm_cmd.addr << 16) | (lcm_cmd.val[1] << 24));
		DSI_OUTREGBIT(cmdq_handle, struct DSI_START_REG, DSI_REG[i]->DSI_START, VM_CMD_START, 0);
		DSI_OUTREGBIT(cmdq_handle, struct DSI_START_REG, DSI_REG[i]->DSI_START, VM_CMD_START, 1);
		DSI_MASKREG32(cmdq_handle, 0xF4020028, 0x1, 0x1);	/* reset mutex for V2C */
		DSI_MASKREG32(cmdq_handle, 0xF4020028, 0x1, 0x0);	/*  */
		DSI_MASKREG32(cmdq_handle, 0xF4020030, 0x1, 0x0);	/* mutext to cmd  mode */
		cmdqRecFlush(cmdq_handle);
		cmdqRecReset(cmdq_handle);
		cmdqRecWaitNoClear(cmdq_handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
		DSI_SetMode(module, NULL, 0);
		/* DSI_SetBypassRack(module, NULL, 0); */
#else
		DSI_SetSwitchMode(module, cmdq_handle, 0);
		DSI_MASKREG32(cmdq_handle, 0xF4020028, 0x1, 0x1);	/* reset mutex for V2C */
		DSI_MASKREG32(cmdq_handle, 0xF4020028, 0x1, 0x0);	/*  */
		DSI_MASKREG32(cmdq_handle, 0xF4020030, 0x1, 0x0);	/* mutext to cmd  mode */
		cmdqRecFlush(cmdq_handle);
		cmdqRecReset(cmdq_handle);
		cmdqRecWaitNoClear(cmdq_handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
		DSI_SetMode(module, NULL, 0);
		/* DSI_SetBypassRack(module, NULL, 0); */
#endif
	} else {
#if 1
		wait_vm_done_irq = true;
		DSI_SetMode(module, cmdq_handle, mode);
		DSI_SetSwitchMode(module, cmdq_handle, 1);
		DSI_MASKREG32(cmdq_handle, 0xF4020030, 0x1, 0x1);	/* mutext to video mode */
		DSI_OUTREG32(cmdq_handle, (unsigned long)(DSI_REG[i]) + 0x200 + 0,
			     0x00001500 | (lcm_cmd.addr << 16) | (lcm_cmd.val[0] << 24));
		DSI_OUTREG32(cmdq_handle, (unsigned long)(DSI_REG[i]) + 0x200 + 4, 0x00000020);
		DSI_OUTREG32(cmdq_handle, (unsigned long)(DSI_REG[i]) + 0x60, 2);
		DSI_Start(module, cmdq_handle);
		DSI_MASKREG32(NULL, 0xF4020020, 0x1, 0x1);	/* release mutex for video mode */
		cmdqRecFlush(cmdq_handle);
		cmdqRecReset(cmdq_handle);
		cmdqRecWaitNoClear(cmdq_handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
#else
/* DSI_WaitForNotBusy(module,NULL); */
		DSI_SetMode(module, NULL, mode);
/* DSI_SetBypassRack(module,NULL,1); */
		DSI_SetSwitchMode(module, NULL, 1);
		DSI_MASKREG32(NULL, 0xF4020030, 0x1, 0x1);	/* mutext to video mode */
		DSI_OUTREG32(NULL, (unsigned int)(DSI_REG[i]) + 0x200 + 0,
			     0x00001500 | (lcm_cmd.addr << 16) | (lcm_cmd.val[0] << 24));
		DSI_OUTREG32(NULL, (unsigned int)(DSI_REG[i]) + 0x204 + 0, 0x00000020);
		DSI_OUTREG32(NULL, (unsigned int)(DSI_REG[i]) + 0x60, 2);
		DSI_Start(module, NULL);
		DSI_MASKREG32(NULL, 0xF4020020, 0x1, 0x1);	/* release mutex for video mode */
#endif
	}
	dsi_currect_mode = mode;
	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++)
		_dsi_context[i].dsi_params.mode = mode;

	return 0;
}

int ddp_dsi_clk_on(enum DISP_MODULE_ENUM module, void *cmdq_handle, unsigned int level)
{
	return 0;
}

int ddp_dsi_clk_off(enum DISP_MODULE_ENUM module, void *cmdq_handle, unsigned int level)
{
	return 0;
}

int ddp_dsi_ioctl(enum DISP_MODULE_ENUM module, void *cmdq_handle, unsigned int ioctl_cmd,
		  unsigned long *params)
{
	int ret = 0;
	/* DISPFUNC(); */
	enum DDP_IOCTL_NAME ioctl = (enum DDP_IOCTL_NAME) ioctl_cmd;
	/* DISPCHECK("[ddp_dsi_ioctl] index = %d\n", ioctl); */
	switch (ioctl) {
	case DDP_STOP_VIDEO_MODE:
		{
			/* ths caller should call wait_event_or_idle for frame stop event then. */
			DSI_SetMode(module, cmdq_handle, CMD_MODE);
			/* TODO: modify this with wait event */

			if (DSI_WaitVMDone(module) != 0)
				ret = -1;
			break;
		}

	case DDP_SWITCH_DSI_MODE:
		{
			ret = ddp_dsi_switch_mode(module, cmdq_handle, params);
			break;
		}
	case DDP_SWITCH_LCM_MODE:
		{
			ret = ddp_dsi_switch_lcm_mode(module, params);
			break;
		}
	case DDP_BACK_LIGHT:
		{
			unsigned int cmd = 0x51;
			unsigned int count = 1;
			unsigned int level = params[0];

			DDPMSG("[ddp_dsi_ioctl] level = %d\n", level);
			DSI_set_cmdq_V2(module, cmdq_handle, cmd, count, ((unsigned char *)&level), 1);
			break;
		}
	case DDP_DSI_IDLE_CLK_CLOSED:
		{
			unsigned int idle_cmd = params[0];

			/* DISPCHECK("[ddp_dsi_ioctl_close] level = %d\n", idle_cmd); */
			if (idle_cmd == 0)
				ddp_dsi_clk_off(module, cmdq_handle, 0);
			else
				ddp_dsi_clk_off(module, cmdq_handle, idle_cmd);
			break;
		}
	case DDP_DSI_IDLE_CLK_OPEN:
		{
			unsigned int idle_cmd = params[0];
			/* DISPCHECK("[ddp_dsi_ioctl_open] level = %d\n", idle_cmd); */
			if (idle_cmd == 0)
				ddp_dsi_clk_on(module, cmdq_handle, 0);
			else
				ddp_dsi_clk_on(module, cmdq_handle, idle_cmd);

			break;
		}
	case DDP_DSI_VFP_LP:
		{
			unsigned int vertical_frontporch = *((unsigned int *)params);

			DDPMSG("vertical_frontporch=%d.\n", vertical_frontporch);
			if (module == DISP_MODULE_DSI0) {
				DSI_OUTREG32(cmdq_handle, &DSI_REG[0]->DSI_VFP_NL,
					     vertical_frontporch);
			}
			break;
		}
	case DDP_DPI_FACTORY_TEST:
		{
			break;
		}
	}
	return ret;
}

int ddp_dsi_trigger(enum DISP_MODULE_ENUM module, void *cmdq)
{
	int i = 0;
	unsigned int data_array[16];

	if (_dsi_context[i].dsi_params.mode == CMD_MODE) {
		data_array[0] = 0x002c3909;
		DSI_set_cmdq(module, cmdq, data_array, 1, 0);
	}
	DSI_Start(module, cmdq);

	return 0;
}

int ddp_dsi_reset(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	DSI_Reset(module, cmdq_handle);

	return 0;
}


int ddp_dsi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int ret = 0;

	DISPFUNC();

	if (!s_isDsiPowerOn) {
#ifdef ENABLE_CLK_MGR
		if (is_ipoh_bootup) {
			if (module == DISP_MODULE_DSI0 || module == DISP_MODULE_DSIDUAL) {
#ifndef CONFIG_MTK_CLKMGR
				ret += ddp_clk_prepare_enable(TOP_RG_MIPI_26M_DBG);
				ret += ddp_clk_enable(DISP1_DSI0_ENGINE);
				ret += ddp_clk_enable(DISP1_DSI0_DIGITAL);
#endif
				if (ret > 0)
					DDPERR("DISP/DSI, DSI power manager API return false\n");
			}
			s_isDsiPowerOn = true;
			DDPMSG("ipoh dsi power on return\n");
			return DSI_STATUS_OK;
		}

		DSI_PHY_clk_switch(module, NULL, true);

		if (module == DISP_MODULE_DSI0 || module == DISP_MODULE_DSIDUAL) {
#ifndef CONFIG_MTK_CLKMGR
			ret += ddp_clk_prepare_enable(TOP_RG_MIPI_26M_DBG);
			ret += ddp_clk_enable(DISP1_DSI0_ENGINE);
			ret += ddp_clk_enable(DISP1_DSI0_DIGITAL);
#endif
			if (ret > 0)
				DDPERR("DSI power manager API return false\n");
		}

		/* restore dsi register */
		DSI_RestoreRegisters(module, NULL);

		/* enable sleep-out mode */
		DSI_SleepOut(module, NULL);

		/* restore lane_num */
		{
			volatile struct DSI_REGS *regs = NULL;

			regs = &(_dsi_context[0].regBackup);
			DSI_OUTREG32(NULL, &DSI_REG[0]->DSI_TXRX_CTRL,
				     AS_UINT32(&regs->DSI_TXRX_CTRL));
		}
		/* enter wakeup */
		DSI_Wakeup(module, NULL);

		/* enable clock */
		DSI_EnableClk(module, NULL);

		DSI_Reset(module, NULL);
#endif
		s_isDsiPowerOn = true;
	}

	return DSI_STATUS_OK;
}


int ddp_dsi_power_off(enum DISP_MODULE_ENUM module, void *cmdq_handle)
{
	int i = 0;
	int ret = 0;
	unsigned int value = 0;

	DISPFUNC();

	if (s_isDsiPowerOn) {
		for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
			/*disable TE when power off */
			DSI_OUTREGBIT(NULL, struct DSI_INT_ENABLE_REG, DSI_REG[i]->DSI_INTEN, TE_RDY, 0);
		}
		DSI_BackupRegisters(module, NULL);
#ifdef ENABLE_CLK_MGR
		/* disable HS mode */
		DSI_clk_HS_mode(module, NULL, false);
		/* enter ULPS mode */
		DSI_clk_ULP_mode(module, NULL, 1);
		DSI_lane0_ULP_mode(module, NULL, 1);

		/* make sure enter ulps mode */
		while (1) {
			mdelay(1);
			value = INREG32(&DSI_REG[0]->DSI_STATE_DBG1);
			value = value >> 24;
			if (value == 0x20)
				break;
			DDPMSG("dsi not in ulps mode, try again...\n");
		}

		/* clear lane_num when enter ulps */
		DSI_OUTREGBIT(NULL, struct DSI_TXRX_CTRL_REG, DSI_REG[0]->DSI_TXRX_CTRL, LANE_NUM, 0);

		DSI_Reset(module, NULL);

		/* disable clock */
		DSI_DisableClk(module, NULL);

		if (module == DISP_MODULE_DSI0 || module == DISP_MODULE_DSIDUAL) {
#ifndef CONFIG_MTK_CLKMGR
			ddp_clk_disable(DISP1_DSI0_ENGINE);
			ddp_clk_disable(DISP1_DSI0_DIGITAL);
			ddp_clk_disable_unprepare(TOP_RG_MIPI_26M_DBG);
#endif
			if (ret > 0)
				DDPERR("DSI power manager API return false\n");
		}
		/* disable mipi pll */
		DSI_PHY_clk_switch(module, NULL, false);
#endif
		s_isDsiPowerOn = false;
	}

	return DSI_STATUS_OK;
}


int ddp_dsi_is_busy(enum DISP_MODULE_ENUM module)
{
	int i = 0;
	int busy = 0;
	struct DSI_INT_STATUS_REG status;
	/* DISPFUNC(); */

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		status = DSI_REG[i]->DSI_INTSTA;

		if (status.BUSY)
			busy++;
	}

	DISPDBG("%s is %s\n", ddp_get_module_name(module), busy ? "busy" : "idle");
	return busy;
}

int ddp_dsi_is_idle(enum DISP_MODULE_ENUM module)
{
	return !ddp_dsi_is_busy(module);
}

void DSI_WaitForNotBusy(enum DISP_MODULE_ENUM module, void *cmdq)
{
	int i = 0;
	uint32_t timeout_ms = 500000;

	if (cmdq) {
		for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++)
			DSI_POLLREG32(cmdq, &DSI_REG[i]->DSI_INTSTA, 0x80000000, 0x0);

		return;
	}

	/*...dsi video is always in busy state... */
	if (_dsi_is_video_mode(module))
		return;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		while (timeout_ms--) {
			if (!ddp_dsi_is_busy(module))
				break;

			udelay(4);
		}

		if (timeout_ms == 0) {
			DDPMSG(" Wait for DSI engine not busy timeout!!!\n");
			DSI_DumpRegisters(module, 1);
			DSI_Reset(module, cmdq);
		}
	}
}

static const char *dsi_mode_spy(LCM_DSI_MODE_CON mode)
{
	switch (mode) {
	case CMD_MODE:
		return "CMD_MODE";
	case SYNC_PULSE_VDO_MODE:
		return "SYNC_PULSE_VDO_MODE";
	case SYNC_EVENT_VDO_MODE:
		return "SYNC_EVENT_VDO_MODE";
	case BURST_VDO_MODE:
		return "BURST_VDO_MODE";
	default:
		return "unknown";
	}
}

void dsi_analysis(enum DISP_MODULE_ENUM module)
{
	int i = 0;

	DDPMSG("==DISP DSI ANALYSIS==\n");
	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		DDPMSG("MIPITX Clock: %d\n", dsi_phy_get_clk(module));
		DDPMSG
		    ("DSI%d Start:%x, Busy:%d, DSI_DUAL_EN:%d, MODE:%s, High Speed:%d, FSM State:%s\n",
		     i, DSI_REG[i]->DSI_START.DSI_START, DSI_REG[i]->DSI_INTSTA.BUSY,
		     DSI_REG[i]->DSI_COM_CTRL.DSI_EN,
		     dsi_mode_spy(DSI_REG[i]->DSI_MODE_CTRL.MODE),
		     DSI_REG[i]->DSI_PHY_LCCON.LC_HS_TX_EN,
		     _dsi_cmd_mode_parse_state(DSI_REG[i]->DSI_STATE_DBG6.CMTRL_STATE));

		DDPMSG
		    ("DSI%d IRQ,RD_RDY:%d, CMD_DONE:%d, SLEEPOUT_DONE:%d, TE_RDY:%d, VM_CMD_DONE:%d, VM_DONE:%d\n",
		     i, DSI_REG[i]->DSI_INTSTA.RD_RDY, DSI_REG[i]->DSI_INTSTA.CMD_DONE,
		     DSI_REG[i]->DSI_INTSTA.SLEEPOUT_DONE, DSI_REG[i]->DSI_INTSTA.TE_RDY,
		     DSI_REG[i]->DSI_INTSTA.VM_CMD_DONE, DSI_REG[i]->DSI_INTSTA.VM_DONE);

		DDPMSG("DSI%d Lane Num:%d, Ext_TE_EN:%d, Ext_TE_Edge:%d, HSTX_CKLP_EN:%d\n", i,
			DSI_REG[i]->DSI_TXRX_CTRL.LANE_NUM,
			DSI_REG[i]->DSI_TXRX_CTRL.EXT_TE_EN,
			DSI_REG[i]->DSI_TXRX_CTRL.EXT_TE_EDGE,
			DSI_REG[i]->DSI_TXRX_CTRL.HSTX_CKLP_EN);
	}
}

int ddp_dsi_dump(enum DISP_MODULE_ENUM module, int level)
{
	if (!s_isDsiPowerOn)
		return 0;


	if (level >= 1) {
		dsi_analysis(module);
		DSI_DumpRegisters(module, level);
	} else if (level >= 0) {
		DSI_DumpRegisters(module, level);
	}
	return 0;
}

int ddp_dsi_build_cmdq(enum DISP_MODULE_ENUM module, void *cmdq_trigger_handle, enum CMDQ_STATE state)
{
	int ret = 0;
	int i = 0;
	int dsi_i = 0;
	LCM_DSI_PARAMS *dsi_params = NULL;
	struct DSI_T0_INS t0;
	struct DSI_RX_DATA_REG read_data0, read_data1, read_data2, read_data3;
	unsigned char packet_type;
	unsigned int recv_data_cnt = 0;
	int result = 0;

	static cmdqBackupSlotHandle hSlot;

	if (module == DISP_MODULE_DSIDUAL)
		dsi_i = 0;
	else
		dsi_i = DSI_MODULE_to_ID(module);

	dsi_params = &_dsi_context[dsi_i].dsi_params;

	if (cmdq_trigger_handle == NULL) {
		DISPCHECK("cmdq_trigger_handle is NULL\n");
		return -1;
	}

	if (state == CMDQ_BEFORE_STREAM_SOF) {
		/* need waiting te */
		if (module == DISP_MODULE_DSI0) {
			if (dsi0_te_enable == 0)
				return 0;
#ifndef MTK_FB_CMDQ_DISABLE
			ret = cmdqRecClearEventToken(cmdq_trigger_handle, CMDQ_EVENT_DSI_TE);
			ret = cmdqRecWait(cmdq_trigger_handle, CMDQ_EVENT_DSI_TE);
#endif
		} else {
			DISPERR("wrong module: %s\n", ddp_get_module_name(module));
			return -1;
		}
	} else if (state == CMDQ_CHECK_IDLE_AFTER_STREAM_EOF) {
		/* need waiting te */
		if (module == DISP_MODULE_DSI0) {
			DSI_POLLREG32(cmdq_trigger_handle, &DSI_REG[dsi_i]->DSI_INTSTA, 0x80000000,
				      0);
		} else {
			DISPERR("wrong module: %s\n", ddp_get_module_name(module));
			return -1;
		}
	} else if (state == CMDQ_ESD_CHECK_READ) {
		/* enable dsi interrupt: RD_RDY/CMD_DONE (need do this here?) */
		DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_INT_ENABLE_REG, DSI_REG[dsi_i]->DSI_INTEN,
			      RD_RDY, 1);
		DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_INT_ENABLE_REG, DSI_REG[dsi_i]->DSI_INTEN,
			      CMD_DONE, 1);

		for (i = 0; i < 3; i++) {
			if (dsi_params->lcm_esd_check_table[i].cmd == 0)
				break;

			/* 0. send read lcm command(short packet) */
			t0.CONFG = 0x04;	/* /BTA */
			t0.Data0 = dsi_params->lcm_esd_check_table[i].cmd;
			/* / 0xB0 is used to distinguish DCS cmd or Gerneric cmd, is that Right??? */
			t0.Data_ID =
			    (t0.Data0 <
			     0xB0) ? DSI_DCS_READ_PACKET_ID : DSI_GERNERIC_READ_LONG_PACKET_ID;
			t0.Data1 = 0;

			/* write DSI CMDQ */
			DSI_OUTREG32(cmdq_trigger_handle, &DSI_CMDQ_REG[dsi_i]->data[0],
				     0x00013700);
			DSI_OUTREG32(cmdq_trigger_handle, &DSI_CMDQ_REG[dsi_i]->data[1],
				     AS_UINT32(&t0));
			DSI_OUTREG32(cmdq_trigger_handle, &DSI_REG[dsi_i]->DSI_CMDQ_SIZE, 2);

			/* start DSI */
			DSI_OUTREG32(cmdq_trigger_handle, &DSI_REG[dsi_i]->DSI_START, 0);
			DSI_OUTREG32(cmdq_trigger_handle, &DSI_REG[dsi_i]->DSI_START, 1);

			/* 1. wait DSI RD_RDY(must clear, in case of cpu RD_RDY interrupt handler) */
			if (dsi_i == 0) {
				DSI_POLLREG32(cmdq_trigger_handle, &DSI_REG[dsi_i]->DSI_INTSTA,
					      0x00000001, 0x1);
				DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_INT_STATUS_REG,
					      DSI_REG[dsi_i]->DSI_INTSTA, RD_RDY, 0x0);
			}
			/* 2. save RX data */
			if (hSlot) {
				DSI_BACKUPREG32(cmdq_trigger_handle,
						hSlot, i * 4 + 0, &DSI_REG[0]->DSI_RX_DATA0);
				DSI_BACKUPREG32(cmdq_trigger_handle,
						hSlot, i * 4 + 1, &DSI_REG[0]->DSI_RX_DATA1);
				DSI_BACKUPREG32(cmdq_trigger_handle,
						hSlot, i * 4 + 2, &DSI_REG[0]->DSI_RX_DATA2);
				DSI_BACKUPREG32(cmdq_trigger_handle,
						hSlot, i * 4 + 3, &DSI_REG[0]->DSI_RX_DATA3);
			}


			/* 3. write RX_RACK */
			DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_RACK_REG, DSI_REG[dsi_i]->DSI_RACK,
				      DSI_RACK, 1);

			/* 4. polling not busy(no need clear) */
			if (dsi_i == 0) {
				DSI_POLLREG32(cmdq_trigger_handle, &DSI_REG[dsi_i]->DSI_INTSTA,
					      0x80000000, 0);
			}
			/* loop: 0~4 */
		}
		/* DSI_OUTREGBIT(cmdq_trigger_handle, DSI_INT_ENABLE_REG,DSI_REG[dsi_i]->DSI_INTEN,RD_RDY,0); */
	} else if (state == CMDQ_ESD_CHECK_CMP) {

		DISPCHECK("[DSI]enter cmp\n");
		/* cmp just once and only 1 return value */
		for (i = 0; i < 3; i++) {
			if (dsi_params->lcm_esd_check_table[i].cmd == 0)
				break;

			DISPCHECK("[DSI]enter cmp i=%d\n", i);

			/* read data */
			if (hSlot) {
				/* read from slot */
				cmdqBackupReadSlot(hSlot, i * 4 + 0, ((uint32_t *)&read_data0));
				cmdqBackupReadSlot(hSlot, i * 4 + 1, ((uint32_t *)&read_data1));
				cmdqBackupReadSlot(hSlot, i * 4 + 2, ((uint32_t *)&read_data2));
				cmdqBackupReadSlot(hSlot, i * 4 + 3, ((uint32_t *)&read_data3));
			} else {
				/* read from dsi , support only one cmd read */
				if (i == 0) {
					DSI_OUTREG32(NULL, &read_data0,
							AS_UINT32(&DSI_REG[dsi_i]->DSI_RX_DATA0));
					DSI_OUTREG32(NULL, &read_data1,
							AS_UINT32(&DSI_REG[dsi_i]->DSI_RX_DATA1));
					DSI_OUTREG32(NULL, &read_data2,
							AS_UINT32(&DSI_REG[dsi_i]->DSI_RX_DATA2));
					DSI_OUTREG32(NULL, &read_data3,
							AS_UINT32(&DSI_REG[dsi_i]->DSI_RX_DATA3));
				}
			}

			mmprofile_log_ex(ddp_mmp_get_events()->esd_rdlcm, MMPROFILE_FLAG_PULSE,
				       AS_UINT32(&read_data0),
				       AS_UINT32(&(dsi_params->lcm_esd_check_table[i])));

			DISPDBG("[DSI]enter cmp read_data0 byte0=0x%x byte1=0x%x byte2=0x%x byte3=0x%x\n",
				read_data0.byte0,
				read_data0.byte1,
				read_data0.byte2,
				read_data0.byte3);
			DISPDBG
			    ("[DSI]enter cmp check_table cmd=0x%x,count=0x%x,para_list[0]=0x%x,para_list[1]=0x%x\n",
			     dsi_params->lcm_esd_check_table[i].cmd,
			     dsi_params->lcm_esd_check_table[i].count,
			     dsi_params->lcm_esd_check_table[i].para_list[0],
			     dsi_params->lcm_esd_check_table[i].para_list[1]);
			DISPDBG("[DSI]enter cmp DSI+0x200=0x%x\n",
				AS_UINT32(DDP_REG_BASE_DSI0 + 0x200));
			DISPDBG("[DSI]enter cmp DSI+0x204=0x%x\n",
				AS_UINT32(DDP_REG_BASE_DSI0 + 0x204));
			DISPDBG("[DSI]enter cmp DSI+0x60=0x%x\n",
				AS_UINT32(DDP_REG_BASE_DSI0 + 0x60));
			DISPDBG("[DSI]enter cmp DSI+0x74=0x%x\n",
				AS_UINT32(DDP_REG_BASE_DSI0 + 0x74));
			DISPDBG("[DSI]enter cmp DSI+0x88=0x%x\n",
				AS_UINT32(DDP_REG_BASE_DSI0 + 0x88));
			DISPDBG("[DSI]enter cmp DSI+0x0c=0x%x\n",
				AS_UINT32(DDP_REG_BASE_DSI0 + 0x0c));

			/* 0x02: acknowledge & error report */
			/* 0x11: generic short read response(1 byte return) */
			/* 0x12: generic short read response(2 byte return) */
			/* 0x1a: generic long read response */
			/* 0x1c: dcs long read response */
			/* 0x21: dcs short read response(1 byte return) */
			/* 0x22: dcs short read response(2 byte return) */
			packet_type = read_data0.byte0;

			if (packet_type == 0x1A || packet_type == 0x1C) {
				recv_data_cnt = read_data0.byte1 + read_data0.byte2 * 16;
				if (recv_data_cnt > 2) {
					DISPCHECK
					("Set receive data count from %d to 2 as ESD check supported max data count.\n",
						recv_data_cnt);
					recv_data_cnt = 2;
				}
				if (recv_data_cnt > dsi_params->lcm_esd_check_table[i].count) {
					DISPCHECK
					("Set receive data count from %d to %d as ESD check table specified.\n",
							recv_data_cnt, dsi_params->lcm_esd_check_table[i].count);
					recv_data_cnt = dsi_params->lcm_esd_check_table[i].count;
				}
				DISPCHECK("DSI read long packet size: %d\n", recv_data_cnt);
				result = memcmp((void *)&(dsi_params->lcm_esd_check_table[i].para_list[0]),
					(void *)&read_data1, recv_data_cnt);
			} else if (packet_type == 0x11 ||
				   packet_type == 0x12 ||
				   packet_type == 0x21 ||
				   packet_type == 0x22) {
				/* short read response */
				if (packet_type == 0x11 || packet_type == 0x21)
					recv_data_cnt = 1;
				else
					recv_data_cnt = 2;

				if (recv_data_cnt > dsi_params->lcm_esd_check_table[i].count) {
					DISPCHECK
					("Set receive data count from %d to %d as ESD check table specified.\n",
						recv_data_cnt, dsi_params->lcm_esd_check_table[i].count);
					recv_data_cnt = dsi_params->lcm_esd_check_table[i].count;
				}
				DISPCHECK("DSI read short packet size: %d\n", recv_data_cnt);
				result = memcmp((void *)&(dsi_params->lcm_esd_check_table[i].para_list[0]),
						(void *)&read_data0.byte1, recv_data_cnt);
			} else if (packet_type == 0x02) {
				DISPCHECK("read return type is 0x02\n");
				result = 1;
			} else {
				DISPCHECK("read return type is non-recognite, type = 0x%x\n", packet_type);
				result = 1;
			}

			if (result == 0) {
				/* clear rx data */
				/* DSI_OUTREG32(NULL, &DSI_REG[dsi_i]->DSI_RX_DATA0,0); */
				ret = 0; /* esd pass */
			} else {
				ret = 1; /* esd fail */
				break;
			}
		}

	} else if (state == CMDQ_ESD_ALLC_SLOT) {
		/* create 12 slot */
		/* 3 cmd * RX_DATA0~RX_DATA4 */
		cmdqBackupAllocateSlot(&hSlot, 3 * 4);
	} else if (state == CMDQ_ESD_FREE_SLOT) {
		if (hSlot) {
			cmdqBackupFreeSlot(hSlot);
			hSlot = 0;
		}
	} else if (state == CMDQ_STOP_VDO_MODE) {
		/* use cmdq to stop dsi vdo mode */
		/* 0. set dsi cmd mode */
		DSI_SetMode(module, cmdq_trigger_handle, CMD_MODE);

		/* 1. polling dsi not busy */
		i = DSI_MODULE_BEGIN(module);
		if (i == 0) {	/* polling vm done */
			/* polling dsi busy */
			DSI_POLLREG32(cmdq_trigger_handle, &DSI_REG[i]->DSI_INTSTA, 0x80000000, 0);
		}
		/* 2.dual dsi need do reset DSI_DUAL_EN/DSI_START */
		if (module == DISP_MODULE_DSIDUAL) {
			DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_COM_CTRL_REG,
				      DSI_REG[0]->DSI_COM_CTRL, DSI_EN, 0);
			DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_COM_CTRL_REG,
				      DSI_REG[1]->DSI_COM_CTRL, DSI_EN, 0);
			DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_START_REG,
				      DSI_REG[0]->DSI_START, DSI_START, 0);
			DSI_OUTREGBIT(cmdq_trigger_handle, struct DSI_START_REG,
				      DSI_REG[1]->DSI_START, DSI_START, 0);
		}

		/* 3.disable HS */
		/* DSI_clk_HS_mode(module, cmdq_trigger_handle, false); */

	} else if (state == CMDQ_START_VDO_MODE) {
		/* 1. set dsi vdo mode */
		DSI_SetMode(module, cmdq_trigger_handle, dsi_params->mode);

		/* 2. enable HS */
		/* DSI_clk_HS_mode(module, cmdq_trigger_handle, true); */

		/* 3. enable mutex */
		/* ddp_mutex_enable(mutex_id_for_latest_trigger,0,cmdq_trigger_handle); */

		/* 4. start dsi */
		/* DSI_Start(module, cmdq_trigger_handle); */

	} else if (state == CMDQ_DSI_RESET) {
		DISPCHECK("CMDQ Timeout, Reset DSI\n");
		DSI_DumpRegisters(module, 1);
		DSI_Reset(module, NULL);
	} else if (state == CMDQ_DSI_LFR_MODE)
		DISPDBG("[DSI]enter CMDQ_DSI_LFR_MODE\n");
	return ret;
}

struct DDP_MODULE_DRIVER ddp_driver_dsi0 = {

	.module = DISP_MODULE_DSI0,
	.init = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_init,
	.deinit = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_deinit,
	.config = (int (*)(enum DISP_MODULE_ENUM, struct disp_ddp_path_config*, void*)) ddp_dsi_config,
	.build_cmdq = (int (*)(enum DISP_MODULE_ENUM, void*, enum CMDQ_STATE)) ddp_dsi_build_cmdq,
	.trigger = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_trigger,
	.start = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_start,
	.stop = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_stop,
	.reset = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_reset,
	.power_on = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_power_on,
	.power_off = (int (*)(enum DISP_MODULE_ENUM, void*)) ddp_dsi_power_off,
	.is_idle = (int (*)(enum DISP_MODULE_ENUM)) ddp_dsi_is_idle,
	.is_busy = (int (*)(enum DISP_MODULE_ENUM)) ddp_dsi_is_busy,
	.dump_info = (int (*)(enum DISP_MODULE_ENUM, int)) ddp_dsi_dump,
	.set_lcm_utils = (int (*)(enum DISP_MODULE_ENUM, LCM_DRIVER*)) ddp_dsi_set_lcm_utils,
	.ioctl = (int (*)(enum DISP_MODULE_ENUM, void*, unsigned int, unsigned long*)) ddp_dsi_ioctl
};

const LCM_UTIL_FUNCS PM_lcm_utils_dsi0 = {

	.set_reset_pin = lcm_set_reset_pin,
	.udelay = lcm_udelay,
	.mdelay = lcm_mdelay,
	.dsi_set_cmdq = DSI_set_cmdq_wrapper_DSI0,
	.dsi_set_cmdq_V2 = DSI_set_cmdq_V2_Wrapper_DSI0
};

void *get_dsi_params_handle(uint32_t dsi_idx)
{
	if (dsi_idx != PM_DSI1)
		return (void *)(&_dsi_context[0].dsi_params);
	else
		return (void *)(&_dsi_context[1].dsi_params);
}

int32_t DSI_ssc_enable(uint32_t dsi_index, uint32_t en)
{
	uint32_t disable = en ? 0 : 1;

	if (dsi_index == PM_DSI0) {
		DSI_OUTREGBIT(NULL, struct MIPITX_DSI_PLL_CON1_REG, DSI_PHY_REG[0]->MIPITX_DSI_PLL_CON1,
			      RG_DSI0_MPPLL_SDM_SSC_EN, en);
		_dsi_context[0].dsi_params.ssc_disable = disable;
	} else if (dsi_index == PM_DSI1) {
		DSI_OUTREGBIT(NULL, struct MIPITX_DSI_PLL_CON1_REG, DSI_PHY_REG[1]->MIPITX_DSI_PLL_CON1,
			      RG_DSI0_MPPLL_SDM_SSC_EN, en);
		_dsi_context[1].dsi_params.ssc_disable = disable;
	} else if (dsi_index == PM_DSI_DUAL) {
		DSI_OUTREGBIT(NULL, struct MIPITX_DSI_PLL_CON1_REG, DSI_PHY_REG[0]->MIPITX_DSI_PLL_CON1,
			      RG_DSI0_MPPLL_SDM_SSC_EN, en);
		DSI_OUTREGBIT(NULL, struct MIPITX_DSI_PLL_CON1_REG, DSI_PHY_REG[1]->MIPITX_DSI_PLL_CON1,
			      RG_DSI0_MPPLL_SDM_SSC_EN, en);
		_dsi_context[0].dsi_params.ssc_disable = _dsi_context[1].dsi_params.ssc_disable =
		    disable;
	}
	return 0;
}


uint32_t PanelMaster_get_TE_status(uint32_t dsi_idx)
{
	if (dsi_idx == 0)
		return dsi0_te_enable ? 1 : 0;
	return 0;
}

uint32_t PanelMaster_get_CC(uint32_t dsi_idx)
{
	struct DSI_TXRX_CTRL_REG tmp_reg;

	DSI_READREG32((struct DSI_TXRX_CTRL_REG *), &tmp_reg, &DSI_REG[dsi_idx]->DSI_TXRX_CTRL);
	return tmp_reg.HSTX_CKLP_EN ? 1 : 0;
}

void PanelMaster_set_CC(uint32_t dsi_index, uint32_t enable)
{
	DDPMSG("dsi_index:%d set_cc :%d\n", dsi_index, enable);
	if (dsi_index == PM_DSI0) {
		DSI_OUTREGBIT(NULL, struct DSI_TXRX_CTRL_REG, DSI_REG[0]->DSI_TXRX_CTRL,
			      HSTX_CKLP_EN, enable);
	}
}

void PanelMaster_DSI_set_timing(uint32_t dsi_index, struct MIPI_TIMING timing)
{
	uint32_t hbp_byte;
	LCM_DSI_PARAMS *dsi_params;
	int fbconfig_dsiTmpBufBpp = 0;

	if (_dsi_context[dsi_index].dsi_params.data_format.format == LCM_DSI_FORMAT_RGB565)
		fbconfig_dsiTmpBufBpp = 2;
	else
		fbconfig_dsiTmpBufBpp = 3;
	dsi_params = get_dsi_params_handle(dsi_index);
	switch (timing.type) {
	case LPX:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON0, LPX, timing.value);
		}
		break;
	case HS_PRPR:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON0, HS_PRPR, timing.value);
		}
		break;
	case HS_ZERO:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON0, HS_ZERO, timing.value);
		}
		break;
	case HS_TRAIL:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON0_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON0, HS_TRAIL, timing.value);
		}
		break;
	case TA_GO:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON1, TA_GO, timing.value);
		}
		break;
	case TA_SURE:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON1, TA_SURE, timing.value);
		}
		break;
	case TA_GET:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON1, TA_GET, timing.value);
		}
		break;
	case DA_HS_EXIT:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON1_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON1, DA_HS_EXIT,
				      timing.value);
		}
		break;
	case CONT_DET:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON2_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON2, CONT_DET, timing.value);
		}
		break;
	case CLK_ZERO:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON2_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON2, CLK_ZERO, timing.value);
		}
		break;
	case CLK_TRAIL:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON2_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON2, CLK_TRAIL,
				      timing.value);
		}
		break;
	case CLK_HS_PRPR:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON3_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON3, CLK_HS_PRPR,
				      timing.value);
		}
		break;
	case CLK_HS_POST:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON3_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON3, CLK_HS_POST,
				      timing.value);
		}
		break;
	case CLK_HS_EXIT:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_PHY_TIMCON3_REG,
				      DSI_REG[0]->DSI_PHY_TIMECON3, CLK_HS_EXIT,
				      timing.value);
		}
		break;
	case HPW:
		if (dsi_params->mode == SYNC_EVENT_VDO_MODE || dsi_params->mode == BURST_VDO_MODE)
			;
		else
			timing.value = (timing.value * fbconfig_dsiTmpBufBpp - 10);

		timing.value = ALIGN_TO((timing.value), 4);
		if (dsi_index == PM_DSI0)
			DSI_OUTREG32(NULL, &DSI_REG[0]->DSI_HSA_WC, timing.value);
		break;
	case HFP:
		timing.value = timing.value * fbconfig_dsiTmpBufBpp - 12;
		timing.value = ALIGN_TO(timing.value, 4);
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, DSI_REG[0]->DSI_HFP_WC, HFP_WC,
				      timing.value);
		} else {
			DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, DSI_REG[0]->DSI_HFP_WC, HFP_WC,
				      timing.value);
			DSI_OUTREGBIT(NULL, struct DSI_HFP_WC_REG, DSI_REG[1]->DSI_HFP_WC, HFP_WC,
				      timing.value);
		}
		break;
	case HBP:
		if (dsi_params->mode == SYNC_EVENT_VDO_MODE || dsi_params->mode == BURST_VDO_MODE) {
			hbp_byte =
			    ((timing.value +
			      dsi_params->horizontal_sync_active) * fbconfig_dsiTmpBufBpp - 10);
		} else {
			/* hsa_byte = (dsi_params->horizontal_sync_active * fbconfig_dsiTmpBufBpp - 10); */
			hbp_byte = timing.value * fbconfig_dsiTmpBufBpp - 10;
		}
		if (dsi_index == PM_DSI0) {
			DSI_OUTREG32(NULL, &DSI_REG[0]->DSI_HBP_WC, ALIGN_TO((hbp_byte), 4));
		} else {
			DSI_OUTREG32(NULL, &DSI_REG[0]->DSI_HBP_WC, ALIGN_TO((hbp_byte), 4));
			DSI_OUTREG32(NULL, &DSI_REG[1]->DSI_HBP_WC, ALIGN_TO((hbp_byte), 4));
		}

		break;
	case VPW:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_VACT_NL_REG, DSI_REG[0]->DSI_VACT_NL,
				      VACT_NL, timing.value);
		} else {
			DSI_OUTREGBIT(NULL, struct DSI_VACT_NL_REG, DSI_REG[0]->DSI_VACT_NL,
				      VACT_NL, timing.value);
			DSI_OUTREGBIT(NULL, struct DSI_VACT_NL_REG, DSI_REG[1]->DSI_VACT_NL,
				      VACT_NL, timing.value);
		}
		break;
	case VFP:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG, DSI_REG[0]->DSI_VFP_NL, VFP_NL,
				      timing.value);
		} else {
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG, DSI_REG[0]->DSI_VFP_NL, VFP_NL,
				      timing.value);
			DSI_OUTREGBIT(NULL, struct DSI_VFP_NL_REG, DSI_REG[1]->DSI_VFP_NL, VFP_NL,
				      timing.value);
		}
		break;
	case VBP:
		if (dsi_index == PM_DSI0) {
			DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG, DSI_REG[0]->DSI_VBP_NL, VBP_NL,
				      timing.value);
		} else {
			DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG, DSI_REG[0]->DSI_VBP_NL, VBP_NL,
				      timing.value);
			DSI_OUTREGBIT(NULL, struct DSI_VBP_NL_REG, DSI_REG[1]->DSI_VBP_NL, VBP_NL,
				      timing.value);
		}
		break;
	case SSC_EN:
		DSI_ssc_enable(dsi_index, timing.value);
		break;
	default:
		DDPMSG("fbconfig dsi set timing :no such type!!\n");
		break;
	}
}


uint32_t PanelMaster_get_dsi_timing(uint32_t dsi_index, enum MIPI_SETTING_TYPE type)
{
	uint32_t dsi_val;
	struct DSI_REGS *dsi_reg;
	int fbconfig_dsiTmpBufBpp = 0;

	if (_dsi_context[dsi_index].dsi_params.data_format.format == LCM_DSI_FORMAT_RGB565)
		fbconfig_dsiTmpBufBpp = 2;
	else
		fbconfig_dsiTmpBufBpp = 3;
	dsi_reg = DSI_REG[0];
	switch (type) {
	case LPX:
		dsi_val = dsi_reg->DSI_PHY_TIMECON0.LPX;
		return dsi_val;
	case HS_PRPR:
		dsi_val = dsi_reg->DSI_PHY_TIMECON0.HS_PRPR;
		return dsi_val;
	case HS_ZERO:
		dsi_val = dsi_reg->DSI_PHY_TIMECON0.HS_ZERO;
		return dsi_val;
	case HS_TRAIL:
		dsi_val = dsi_reg->DSI_PHY_TIMECON0.HS_TRAIL;
		return dsi_val;
	case TA_GO:
		dsi_val = dsi_reg->DSI_PHY_TIMECON1.TA_GO;
		return dsi_val;
	case TA_SURE:
		dsi_val = dsi_reg->DSI_PHY_TIMECON1.TA_SURE;
		return dsi_val;
	case TA_GET:
		dsi_val = dsi_reg->DSI_PHY_TIMECON1.TA_GET;
		return dsi_val;
	case DA_HS_EXIT:
		dsi_val = dsi_reg->DSI_PHY_TIMECON1.DA_HS_EXIT;
		return dsi_val;
	case CONT_DET:
		dsi_val = dsi_reg->DSI_PHY_TIMECON2.CONT_DET;
		return dsi_val;
	case CLK_ZERO:
		dsi_val = dsi_reg->DSI_PHY_TIMECON2.CLK_ZERO;
		return dsi_val;
	case CLK_TRAIL:
		dsi_val = dsi_reg->DSI_PHY_TIMECON2.CLK_TRAIL;
		return dsi_val;
	case CLK_HS_PRPR:
		dsi_val = dsi_reg->DSI_PHY_TIMECON3.CLK_HS_PRPR;
		return dsi_val;
	case CLK_HS_POST:
		dsi_val = dsi_reg->DSI_PHY_TIMECON3.CLK_HS_POST;
		return dsi_val;
	case CLK_HS_EXIT:
		dsi_val = dsi_reg->DSI_PHY_TIMECON3.CLK_HS_EXIT;
		return dsi_val;
	case HPW:
		{
			struct DSI_HSA_WC_REG tmp_reg;

			DSI_READREG32((struct DSI_HSA_WC_REG *), &tmp_reg, &dsi_reg->DSI_HSA_WC);
			dsi_val = (tmp_reg.HSA_WC + 10) / fbconfig_dsiTmpBufBpp;
			return dsi_val;
		}
	case HFP:
		{
			struct DSI_HFP_WC_REG tmp_hfp;

			DSI_READREG32((struct DSI_HFP_WC_REG *), &tmp_hfp, &dsi_reg->DSI_HFP_WC);
			dsi_val = ((tmp_hfp.HFP_WC + 12) / fbconfig_dsiTmpBufBpp);
			return dsi_val;
		}
	case HBP:
		{
			struct DSI_HBP_WC_REG tmp_hbp;
			LCM_DSI_PARAMS *dsi_params;

			dsi_params = get_dsi_params_handle(dsi_index);
			OUTREG32(&tmp_hbp, AS_UINT32(&dsi_reg->DSI_HBP_WC));
			if (dsi_params->mode == SYNC_EVENT_VDO_MODE
			    || dsi_params->mode == BURST_VDO_MODE)
				return ((tmp_hbp.HBP_WC + 10) / fbconfig_dsiTmpBufBpp -
					dsi_params->horizontal_sync_active);
			else
				return (tmp_hbp.HBP_WC + 10) / fbconfig_dsiTmpBufBpp;
		}
	case VPW:
		{
			struct DSI_VACT_NL_REG tmp_vpw;

			DSI_READREG32((struct DSI_VACT_NL_REG *), &tmp_vpw, &dsi_reg->DSI_VACT_NL);
			dsi_val = tmp_vpw.VACT_NL;
			return dsi_val;
		}
	case VFP:
		{
			struct DSI_VFP_NL_REG tmp_vfp;

			DSI_READREG32((struct DSI_VFP_NL_REG *), &tmp_vfp, &dsi_reg->DSI_VFP_NL);
			dsi_val = tmp_vfp.VFP_NL;
			return dsi_val;
		}
	case VBP:
		{
			struct DSI_VBP_NL_REG tmp_vbp;

			DSI_READREG32((struct DSI_VBP_NL_REG *), &tmp_vbp, &dsi_reg->DSI_VBP_NL);
			dsi_val = tmp_vbp.VBP_NL;
			return dsi_val;
		}
	case SSC_EN:
		{
			if (_dsi_context[dsi_index].dsi_params.ssc_disable)
				dsi_val = 0;
			else
				dsi_val = 1;
			return dsi_val;
		}
	default:
		DDPMSG("fbconfig dsi set timing :no such type!!\n");
	}
	dsi_val = 0;
	return dsi_val;
}

unsigned int PanelMaster_set_PM_enable(unsigned int value)
{
	atomic_set(&PMaster_enable, value);
	return 0;
}

/* ///////////////////////////////No DSI Driver //////////////////////////////////////////////// */
int DSI_set_roi(int x, int y)
{
	DDPMSG("[DSI](x0,y0,x1,y1)=(%d,%d,%d,%d)\n", x, y, _dsi_context[0].lcm_width,
	       _dsi_context[0].lcm_height);
	return DSI_Send_ROI(DISP_MODULE_DSI0, NULL, x, y, _dsi_context[0].lcm_width - x,
			    _dsi_context[0].lcm_height - y);
}

int DSI_check_roi(void)
{
	int ret = 0;
	unsigned char read_buf[4] = { 1, 1, 1, 1 };
	unsigned int data_array[16];
	int count, x0, y0;

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */

	DSI_set_cmdq(DISP_MODULE_DSI0, NULL, data_array, 1, 1);
	msleep(20);
	count = DSI_dcs_read_lcm_reg_v2(DISP_MODULE_DSI0, NULL, 0x2a, read_buf, 4);
	x0 = (read_buf[0] << 8) | read_buf[1];
	msleep(20);

	DDPMSG("x0=%d count=%d,read_buf[0]=%d,read_buf[1]=%d,read_buf[2]=%d,read_buf[3]=%d\n", x0,
	       count, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
	if ((count == 0) || (x0 != 0)) {
		DDPMSG
		    ("[DSI]x count %d read_buf[0]=%d,read_buf[1]=%d,read_buf[2]=%d,read_buf[3]=%d\n",
		     count, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
		return -1;
	}
	msleep(20);
	count = DSI_dcs_read_lcm_reg_v2(DISP_MODULE_DSI0, NULL, 0x2b, read_buf, 4);
	y0 = (read_buf[0] << 8) | read_buf[1];

	DDPMSG("y0=%d count %d,read_buf[0]=%d,read_buf[1]=%d,read_buf[2]=%d,read_buf[3]=%d\n", y0,
	       count, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
	if ((count == 0) || (y0 != 0)) {
		DDPMSG
		    ("[DSI]y count %d read_buf[0]=%d,read_buf[1]=%d,read_buf[2]=%d,read_buf[3]=%d\n",
		     count, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
		return -1;
	}
	return ret;
}

void DSI_ForceConfig(int forceconfig)
{
	dsi_force_config = forceconfig;
}

