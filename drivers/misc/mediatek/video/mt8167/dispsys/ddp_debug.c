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

#define LOG_TAG "DEBUG"

#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mt-plat/aee.h>
#include "disp_assert_layer.h"
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "disp_drv_ddp.h"

#include "ddp_debug.h"
#include "ddp_reg.h"
#include "ddp_drv.h"
#include "ddp_wdma.h"
#include "ddp_wdma_ex.h"
#include "ddp_hal.h"
#include "ddp_path.h"
#include "ddp_aal.h"
#include "ddp_pwm.h"
#include "ddp_info.h"
#include "ddp_dsi.h"
#include "ddp_ovl.h"

#include "ddp_manager.h"
#include "ddp_log.h"
#include "ddp_met.h"
#include "display_recorder.h"
#include "disp_session.h"
#include "primary_display.h"
#include "ddp_irq.h"
#include "mtk_disp_mgr.h"
#include "disp_drv_platform.h"

#include "DpDataType.h"

#pragma GCC optimize("O0")

/* --------------------------------------------------------------------------- */
/* External variable declarations */
/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
/* Debug Options */
/* --------------------------------------------------------------------------- */

static struct dentry *debugfs;
static struct dentry *debugDir;


static struct dentry *debugfs_dump;

static const long int DEFAULT_LOG_FPS_WND_SIZE = 30;
static int debug_init;


unsigned char pq_debug_flag;
unsigned char aal_debug_flag;

static unsigned int dbg_log_level;
static unsigned int irq_log_level;
static unsigned int dump_to_buffer;

unsigned int gOVLBackground;
unsigned int gUltraEnable = 1;
unsigned int gDumpMemoutCmdq;
unsigned int gEnableUnderflowAEE;

unsigned int disp_low_power_enlarge_blanking;
unsigned int disp_low_power_disable_ddp_clock;
unsigned int disp_low_power_disable_fence_thread;
unsigned int disp_low_power_remove_ovl = 1;
unsigned int gSkipIdleDetect;
unsigned int gDumpClockStatus = 1;
#ifdef DISP_ENABLE_SODI_FOR_VIDEO_MODE
unsigned int gEnableSODIControl = 1;
  /* workaround for SVP IT, todo: please K fix it */
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
unsigned int gPrefetchControl;
#else
unsigned int gPrefetchControl = 1;
#endif
#else
unsigned int gEnableSODIControl;
unsigned int gPrefetchControl;
#endif

/* enable it when use UART to grab log */
unsigned int gEnableUartLog;
/* mutex SOF at raing edge of vsync, can save more time for cmdq config */
unsigned int gEnableMutexRisingEdge;
/* only write dirty register, reduce register number write by cmdq */
unsigned int gEnableReduceRegWrite;

unsigned int gDumpConfigCMD;
unsigned int gDumpESDCMD;

unsigned int gESDEnableSODI = 1;
unsigned int gEnableOVLStatusCheck;
unsigned int gEnableDSIStateCheck;

unsigned int gResetRDMAEnable = 1;
unsigned int gEnableSWTrigger;
unsigned int gMutexFreeRun = 1;

unsigned int gResetOVLInAALTrigger;
unsigned int gDisableOVLTF;

unsigned long int gRDMAUltraSetting;	/* so we can modify RDMA ultra at run-time */
unsigned long int gRDMAFIFOLen = 32;

#ifdef _MTK_USER_
unsigned int gEnableIRQ;
/* #error eng_error */
#else
unsigned int gEnableIRQ = 1;
/* #error user_error */
#endif
unsigned int gDisableSODIForTriggerLoop = 1;
unsigned int force_sec;

static char STR_HELP[] =
	"USAGE:\n"
	"       echo [ACTION]>/d/dispsys\n"
	"ACTION:\n"
	"       regr:addr\n              :regr:0xf400c000\n"
	"       regw:addr,value          :regw:0xf400c000,0x1\n"
	"       dbg_log:0|1|2            :0 off, 1 dbg, 2 all\n"
	"       irq_log:0|1              :0 off, !0 on\n"
	"       met_on:[0|1],[0|1],[0|1] :fist[0|1]on|off,other [0|1]direct|decouple\n"
	"       backlight:level\n"
	"       dump_aal:arg\n"
	"       mmp\n"
	"       dump_reg:moduleID\n dump_path:mutexID\n dpfd_ut1:channel\n";
/* --------------------------------------------------------------------------- */
/* Command Processor */
/* --------------------------------------------------------------------------- */
static char dbg_buf[2048];
static unsigned int is_reg_addr_valid(unsigned int isVa, unsigned long addr)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_REG_NUM; i++) {
		if ((isVa == 1) && (addr >= dispsys_reg[i]) && (addr <= dispsys_reg[i] + 0x1000))
			break;
		if ((isVa == 0) && (addr >= ddp_reg_pa_base[i])
		    && (addr <= ddp_reg_pa_base[i] + 0x1000))
			break;
	}

	if (i < DISP_REG_NUM) {
		DDPMSG("addr valid, isVa=0x%x, addr=0x%lx, module=%s!\n", isVa, addr,
		       ddp_get_reg_module_name(i));
		return 1;
	}

	DDPERR("is_reg_addr_valid return fail, isVa=0x%x, addr=0x%lx!\n", isVa, addr);
	return 0;
}

static void process_dbg_opt(const char *opt)
{
	char *buf = dbg_buf + strlen(dbg_buf);
	int ret = 0;
	char *p;
	char *tmp;

	if (strncmp(opt, "regr:", 5) == 0) {
		unsigned long addr = 0;

		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &addr);
		if (is_reg_addr_valid(1, addr) == 1) {	/* (addr >= 0xf0000000U && addr <= 0xff000000U) */
			unsigned int regVal = DISP_REG_GET(addr);

			DDPMSG("regr: 0x%lx = 0x%08X\n", addr, regVal);
			sprintf(buf, "regr: 0x%lx = 0x%08X\n", addr, regVal);
		} else {
			sprintf(buf, "regr, invalid address 0x%lx\n", addr);
			goto Error;
		}
	} else if (strncmp(opt, "regw:", 5) == 0) {
		unsigned long addr = 0;
		unsigned long val = 0;

		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &addr);
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &val);
		if (is_reg_addr_valid(1, addr) == 1) {	/* (addr >= 0xf0000000U && addr <= 0xff000000U) */
			unsigned int regVal;

			DISP_CPU_REG_SET(addr, val);
			regVal = DISP_REG_GET(addr);
			DDPMSG("regw: 0x%lx, 0x%08X = 0x%08X\n", addr, (int)val, regVal);
			sprintf(buf, "regw: 0x%lx, 0x%08X = 0x%08X\n", addr, (int)val, regVal);
		} else {
			sprintf(buf, "regw, invalid address 0x%lx\n", addr);
			goto Error;
		}
	} else if (strncmp(opt, "rdma_ultra:", 11) == 0) {
		p = (char *)opt + 11;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &gRDMAUltraSetting);
		DISP_CPU_REG_SET(DISP_REG_RDMA_MEM_GMC_SETTING_0, gRDMAUltraSetting);
		sprintf(buf, "rdma_ultra, gRDMAUltraSetting=0x%x, reg=0x%x\n",
			(unsigned int)gRDMAUltraSetting,
			DISP_REG_GET(DISP_REG_RDMA_MEM_GMC_SETTING_0));
	} else if (strncmp(opt, "rdma_fifo:", 10) == 0) {
		p = (char *)opt + 10;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &gRDMAFIFOLen);
		DISP_CPU_REG_SET_FIELD(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD,
				       DISP_REG_RDMA_FIFO_CON, gRDMAFIFOLen);
		sprintf(buf, "rdma_fifo, gRDMAFIFOLen=0x%x, reg=0x%x\n",
			(unsigned int)gRDMAFIFOLen, DISP_REG_GET(DISP_REG_RDMA_FIFO_CON));
	} else if (strncmp(opt, "g_regr:", 7) == 0) {
		unsigned int reg_va_before;
		unsigned long reg_va;
		unsigned long reg_pa = 0;

		p = (char *)opt + 7;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &reg_pa);
		if (reg_pa == 0) {
			sprintf(buf, "g_regr, invalid pa=0x%lx\n", reg_pa);
		} else {
			reg_va = (unsigned long)ioremap_nocache(reg_pa, sizeof(unsigned long));
			reg_va_before = DISP_REG_GET(reg_va);
			pr_debug("g_regr, pa=%lx, va=0x%lx, reg_val=0x%x\n",
				 reg_pa, reg_va, reg_va_before);
			sprintf(buf, "g_regr, pa=%lx, va=0x%lx, reg_val=0x%x\n",
				reg_pa, reg_va, reg_va_before);

			iounmap((void *)reg_va);
		}
	} else if (strncmp(opt, "g_regw:", 7) == 0) {
		unsigned int reg_va_before;
		unsigned int reg_va_after;
		unsigned long int val;
		unsigned long reg_va;
		unsigned long reg_pa = 0;

		p = (char *)opt + 7;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &reg_pa);
		if (reg_pa == 0) {
			sprintf(buf, "g_regw, invalid pa=0x%lx\n", reg_pa);
		} else {
			tmp = strsep(&p, ",");
			ret = kstrtoul(tmp, 0, &val);
			reg_va = (unsigned long)ioremap_nocache(reg_pa, sizeof(unsigned long));
			reg_va_before = DISP_REG_GET(reg_va);
			DISP_CPU_REG_SET(reg_va, val);
			reg_va_after = DISP_REG_GET(reg_va);

			pr_debug
			    ("g_regw, pa=%lx, va=0x%lx, value=0x%x, reg_val_before=0x%x, reg_val_after=0x%x\n",
			     reg_pa, reg_va, (unsigned int)val, reg_va_before, reg_va_after);
			sprintf(buf,
				"g_regw, pa=%lx, va=0x%lx, value=0x%x, reg_val_before=0x%x, reg_val_after=0x%x\n",
				reg_pa, reg_va, (unsigned int)val, reg_va_before, reg_va_after);

			iounmap((void *)reg_va);
		}
	} else if (strncmp(opt, "dbg_log:", 8) == 0) {
		unsigned long int enable = 0;

		p = (char *)opt + 8;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &enable);
		if (enable)
			dbg_log_level = 1;
		else
			dbg_log_level = 0;

		sprintf(buf, "dbg_log: %d\n", dbg_log_level);
	} else if (strncmp(opt, "irq_log:", 8) == 0) {
		unsigned long int enable = 0;

		p = (char *)opt + 8;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &enable);

		if (enable)
			irq_log_level = 1;
		else
			irq_log_level = 0;

		sprintf(buf, "irq_log: %d\n", irq_log_level);
	} else if (strncmp(opt, "met_on:", 7) == 0) {
		unsigned long int met_on = 0;
		int rdma0_mode = 0;
		int rdma1_mode = 0;

		p = (char *)opt + 7;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &met_on);
		if (strncmp(p, "1", 1) == 0)
			met_on = 1;

		ddp_init_met_tag((unsigned int)met_on, rdma0_mode, rdma1_mode);
		DDPMSG("process_dbg_opt, met_on=%d,rdma0_mode %d, rdma1 %d\n",
		       (unsigned int)met_on, rdma0_mode, rdma1_mode);
		sprintf(buf, "met_on:%d,rdma0_mode:%d,rdma1_mode:%d\n",
			(unsigned int)met_on, rdma0_mode, rdma1_mode);
	} else if (strncmp(opt, "backlight:", 10) == 0) {
		unsigned long level = 0;

		p = (char *)opt + 10;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &level);
		if (level) {
			disp_bls_set_backlight(level);
			sprintf(buf, "backlight: %d\n", (int)level);
		} else {
			goto Error;
		}
	} else if (strncmp(opt, "pwm0:", 5) == 0 || strncmp(opt, "pwm1:", 5) == 0) {
		unsigned long int level = 0;

		p = (char *)opt + 5;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &level);
		if (level) {
			disp_pwm_id_t pwm_id = DISP_PWM0;

			if (opt[3] == '1')
				pwm_id = DISP_PWM1;

			disp_pwm_set_backlight(pwm_id, (unsigned int)level);
			sprintf(buf, "PWM 0x%x : %d\n", pwm_id, (unsigned int)level);
		} else {
			goto Error;
		}
	} else if (strncmp(opt, "aal_dbg:", 8) == 0) {
		tmp = strsep(&p, ",");
		ret = kstrtoint(tmp, 0, &aal_dbg_en);
		sprintf(buf, "aal_dbg_en = 0x%x\n", aal_dbg_en);
	} else if (strncmp(opt, "aal_test:", 9) == 0) {
		aal_test(opt + 9, buf);
	} else if (strncmp(opt, "pwm_test:", 9) == 0) {
		disp_pwm_test(opt + 9, buf);
	} else if (strncmp(opt, "dump_reg:", 9) == 0) {
		unsigned long int module = 0;

		p = (char *)opt + 9;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &module);
		DDPMSG("process_dbg_opt, module=%d\n", (unsigned int)module);
		if (module < DISP_MODULE_NUM) {
			ddp_dump_reg(module);
			sprintf(buf, "dump_reg: %d\n", (unsigned int)module);
		} else {
			DDPMSG("process_dbg_opt2, module=%d\n", (unsigned int)module);
			goto Error;
		}

	} else if (strncmp(opt, "dump_path:", 10) == 0) {
		unsigned long int mutex_idx = 0;

		p = (char *)opt + 10;
		tmp = strsep(&p, ",");
		ret = kstrtoul(tmp, 0, &mutex_idx);
		DDPMSG("process_dbg_opt, path mutex=%d\n", (unsigned int)mutex_idx);
		dpmgr_debug_path_status((unsigned int)mutex_idx);
		sprintf(buf, "dump_path: %d\n", (unsigned int)mutex_idx);

	} else if (strncmp(opt, "debug:", 6) == 0) {
		char *p = (char *)opt + 6;
		unsigned int enable;

		ret = kstrtouint(p, 0, &enable);
		if (ret) {
			snprintf(buf, 50, "error to parse cmd %s\n", opt);
			return;
		}

		if (enable == 1) {
			DDPMSG("[DDP] debug=1, trigger AEE\n");
			/* aee_kernel_exception("DDP-TEST-ASSERT", "[DDP] DDP-TEST-ASSERT"); */
		} else if (enable == 2) {
			ddp_mem_test();
		} else if (enable == 3) {
			ddp_lcd_test();
		} else if (enable == 4) {
			/* DDPAEE("test 4"); */
		} else if (enable == 12) {
			if (gUltraEnable == 0)
				gUltraEnable = 1;
			else
				gUltraEnable = 0;
			sprintf(buf, "gUltraEnable: %d\n", gUltraEnable);
		} else if (enable == 13) {
			if (force_sec == 0)
				force_sec = 1;
			else
				force_sec = 0;
			pr_err("force_sec: %d\n", force_sec);
			sprintf(buf, "force_sec: %d\n", force_sec);
		} else if (enable == 14) {
			if (gSkipIdleDetect == 0)
				gSkipIdleDetect = 1;
			else
				gSkipIdleDetect = 0;
			pr_err("gSkipIdleDetect: %d\n", gSkipIdleDetect);
			sprintf(buf, "gSkipIdleDetect: %d\n", gSkipIdleDetect);
		}
	} else if (strncmp(opt, "mmp", 3) == 0) {
		init_ddp_mmp_events();
	} else {
		dbg_buf[0] = '\0';
		goto Error;
	}

	return;

Error:
	DDPERR("parse command error!\n%s\n\n%s", opt, STR_HELP);
}


static void process_dbg_cmd(char *cmd)
{
	char *tok;

	DDPDBG("cmd: %s\n", cmd);
	memset(dbg_buf, 0, sizeof(dbg_buf));
	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}


/* --------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* --------------------------------------------------------------------------- */

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static char cmd_buf[512];

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	if (strlen(dbg_buf))
		return simple_read_from_buffer(ubuf, count, ppos, dbg_buf, strlen(dbg_buf));
	else
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP, strlen(STR_HELP));

}


static ssize_t debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buf, ubuf, count))
		return -EFAULT;

	cmd_buf[count] = 0;

	process_dbg_cmd(cmd_buf);

	return ret;
}


static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

static ssize_t debug_dump_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

	dprec_logger_dump_reset();
	dump_to_buffer = 1;
	/* dump all */
	dpmgr_debug_path_status(-1);
	dump_to_buffer = 0;
	return simple_read_from_buffer(buf, size, ppos, dprec_logger_get_dump_addr(),
				       dprec_logger_get_dump_len());
}


static const struct file_operations debug_fops_dump = {
	.read = debug_dump_read,
};

void ddp_debug_init(void)
{
	if (!debug_init) {
		debug_init = 1;
		debugfs =
		    debugfs_create_file("dispsys", S_IFREG | S_IRUGO, NULL, (void *)0, &debug_fops);

		debugDir = debugfs_create_dir("disp", NULL);
		if (debugDir)
			debugfs_dump =
			    debugfs_create_file("dump", S_IFREG | S_IRUGO, debugDir, NULL,
						&debug_fops_dump);
	}
}

unsigned int ddp_debug_analysis_to_buffer(void)
{
	return dump_to_buffer;
}

unsigned int ddp_debug_dbg_log_level(void)
{
	return dbg_log_level;
}

unsigned int ddp_debug_irq_log_level(void)
{
	return irq_log_level;
}


void ddp_debug_exit(void)
{
	debugfs_remove(debugfs);
	debugfs_remove(debugfs_dump);
	debug_init = 0;
}

int ddp_mem_test(void)
{
	return -1;
}

int ddp_lcd_test(void)
{
	return -1;
}

char *disp_get_fmt_name(enum DP_COLOR_ENUM color)
{
	switch (color) {
	case DP_COLOR_FULLG8:
		return "fullg8";
	case DP_COLOR_FULLG10:
		return "fullg10";
	case DP_COLOR_FULLG12:
		return "fullg12";
	case DP_COLOR_FULLG14:
		return "fullg14";
	case DP_COLOR_UFO10:
		return "ufo10";
	case DP_COLOR_BAYER8:
		return "bayer8";
	case DP_COLOR_BAYER10:
		return "bayer10";
	case DP_COLOR_BAYER12:
		return "bayer12";
	case DP_COLOR_RGB565:
		return "rgb565";
	case DP_COLOR_BGR565:
		return "bgr565";
	case DP_COLOR_RGB888:
		return "rgb888";
	case DP_COLOR_BGR888:
		return "bgr888";
	case DP_COLOR_RGBA8888:
		return "rgba";
	case DP_COLOR_BGRA8888:
		return "bgra";
	case DP_COLOR_ARGB8888:
		return "argb";
	case DP_COLOR_ABGR8888:
		return "abgr";
	case DP_COLOR_I420:
		return "i420";
	case DP_COLOR_YV12:
		return "yv12";
	case DP_COLOR_NV12:
		return "nv12";
	case DP_COLOR_NV21:
		return "nv21";
	case DP_COLOR_I422:
		return "i422";
	case DP_COLOR_YV16:
		return "yv16";
	case DP_COLOR_NV16:
		return "nv16";
	case DP_COLOR_NV61:
		return "nv61";
	case DP_COLOR_YUYV:
		return "yuyv";
	case DP_COLOR_YVYU:
		return "yvyu";
	case DP_COLOR_UYVY:
		return "uyvy";
	case DP_COLOR_VYUY:
		return "vyuy";
	case DP_COLOR_I444:
		return "i444";
	case DP_COLOR_YV24:
		return "yv24";
	case DP_COLOR_IYU2:
		return "iyu2";
	case DP_COLOR_NV24:
		return "nv24";
	case DP_COLOR_NV42:
		return "nv42";
	case DP_COLOR_GREY:
		return "grey";
	default:
		return "undefined";
	}

}

unsigned int ddp_dump_reg_to_buf(unsigned int start_module, unsigned long *addr)
{
	unsigned int cnt = 0;
	unsigned long reg_addr;

	switch (start_module) {
	case 0:		/* DISP_MODULE_WDMA0: */
		reg_addr = DISP_REG_WDMA_INTEN;

		while (reg_addr <= DISP_REG_WDMA_PRE_ADD2) {
			addr[cnt++] = DISP_REG_GET(reg_addr);
			reg_addr += 4;
		}
		/* fallthrough */
	case 1:		/* DISP_MODULE_OVL: */
		reg_addr = DISP_REG_OVL_STA;

		while (reg_addr <= DISP_REG_OVL_L3_PITCH) {
			addr[cnt++] = DISP_REG_GET(reg_addr);
			reg_addr += 4;
		}
		/* fallthrough */
	case 2:		/* DISP_MODULE_RDMA: */
		reg_addr = DISP_REG_RDMA_INT_ENABLE;

		while (reg_addr <= DISP_REG_RDMA_PRE_ADD_1) {
			addr[cnt++] = DISP_REG_GET(reg_addr);
			reg_addr += 4;
		}
		break;
	}
	return cnt * sizeof(unsigned long);
}

/* ddp_dump_lcm_parameter */
unsigned int ddp_dump_lcm_param_to_buf(unsigned int start_module, unsigned long *addr)
{
	unsigned int cnt = 0;

	if (start_module == 3) {	/*3 correspond dbg4 */
		addr[cnt++] = primary_display_get_width();
		addr[cnt++] = primary_display_get_height();
	}

	return cnt * sizeof(unsigned long);
}
