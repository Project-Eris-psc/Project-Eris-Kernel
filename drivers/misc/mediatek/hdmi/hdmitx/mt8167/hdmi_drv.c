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

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
/* #include <asm/uaccess.h> */
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
/* #include <linux/earlysuspend.h> */
#include <linux/platform_device.h>
/* #include <asm/atomic.h> */
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/fb.h>
#include <linux/notifier.h>

#include "hdmi_ctrl.h"
#include "hdmictrl.h"
#include "hdmiddc.h"
#include "hdmihdcp.h"
#include "hdmicec.h"
#include "hdmiedid.h"

#include "hdmi_drv.h"
#include "asm-generic/irq.h"
#include <mt-plat/mtk_boot_common.h>
#include "hdmiavd.h"
#include "hdmicmd.h"
#include "mtk_smi.h"

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT))
#include "hdmi_ca.h"
#endif

/*----------------------------------------------------------------------------*/
/* Debug message defination */
/*----------------------------------------------------------------------------*/

static struct timer_list r_hdmi_timer;
static struct timer_list r_cec_timer;

static uint32_t gHDMI_CHK_INTERVAL = 10;
static uint32_t gCEC_CHK_INTERVAL = 20;

size_t hdmidrv_log_on = hdmialllog;
size_t hdmi_cec_on;
size_t hdmi_cecinit;
size_t hdmi_hdmiinit;
size_t hdmi_powerenable;
size_t hdmi_clockenable;

size_t hdmi_TmrValue[MAX_HDMI_TMR_NUMBER] = { 0 };

size_t hdmi_hdmiCmd = 0xff;
size_t hdmi_rxcecmode = CEC_NORMAL_MODE;
enum HDMI_CTRL_STATE_T e_hdmi_ctrl_state = HDMI_STATE_IDLE;
enum HDCP_CTRL_STATE_T e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;
size_t hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;


/* from DTS, for debug */
unsigned int hdmi_reg_pa_base[HDMI_REG_NUM] = {
	0x1401D000, 0x11000F00, 0x10013000
};

/* Reigster of HDMI , Include HDMI SHELL,DDC,CEC*/
unsigned long hdmi_reg[HDMI_REG_NUM] = { 0 };

/*Registers which will be used by HDMI   */
unsigned long hdmi_ref_reg[HDMI_REF_REG_NUM] = { 0 };

/* clocks that will be used by hdmi module*/
struct clk *hdmi_ref_clock[HDMI_SEL_CLOCK_NUM] = { 0 };

/*Irq of HDMI */
unsigned int hdmi_irq;

/* 5v ddc power control pin*/
int hdmi_power_control_pin;

struct platform_device *hdmi_pdev;

size_t hdmi_hdmiearlysuspend = 1;

static unsigned char port_hpd_value_bak = 0xff;

static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

static struct task_struct *cec_timer_task;
wait_queue_head_t cec_timer_wq;
atomic_t cec_timer_event = ATOMIC_INIT(0);

static struct task_struct *hdmi_irq_task;
wait_queue_head_t hdmi_irq_wq;
atomic_t hdmi_irq_event = ATOMIC_INIT(0);

static struct HDMI_UTIL_FUNCS hdmi_util = { 0 };

static int hdmi_timer_kthread(void *data);
static int cec_timer_kthread(void *data);
static int hdmi_irq_kthread(void *data);
static void hdmi_invoke_cable_callbacks(enum HDMI_STATE state);

const char *szHdmiPordStatusStr[] = {
	"HDMI_PLUG_OUT=0",
	"HDMI_PLUG_IN_AND_SINK_POWER_ON",
	"HDMI_PLUG_IN_ONLY",
	"HDMI_PLUG_IN_EDID",
	"HDMI_PLUG_IN_CEC",
	"HDMI_PLUG_IN_POWER_EDID",
};

const char *szHdmiCecPordStatusStr[] = {
	"HDMI_CEC_STATE_PLUG_OUT=0",
	"HDMI_CEC_STATE_TX_STS",
	"HDMI_CEC_STATE_GET_CMD",
};

const char *hdmi_use_module_name_spy(enum HDMI_REF_MODULE_ENUM module)
{
	switch (module) {
	case AP_CCIF0:
		return "mediatek,mt8167-apmixedsys";	/* TVD//PLL */
	case TOPCK_GEN:
		return "mediatek,mt8167-topckgen";
	case INFRA_SYS:
		return "mediatek,mt8167-infracfg";
	case MMSYS_CONFIG:
		return "mediatek,mt8167-mmsys";
	case GPIO_REG:
		return "mediatek,mt8167-pinctrl";
	case DPI1_REG:
		return "mediatek,mt8167-disp_dpi1";
	case GIC_REG:
		return "mediatek,mt8167-mcucfg";

	case HDMI_REF_REG_NUM:
		return "mediatek,HDMI_UNKNOWN";
	default:
		return "mediatek,HDMI_UNKNOWN";
	}
}


const char *hdmi_use_clock_name_spy(enum HDMI_REF_CLOCK_ENUM module)
{
	switch (module) {
/*	case MMSYS_POWER: */
/*		return "mmsys_power";	 Must be power on first, power off last */
	case INFRA_SYS_CLK_DDC:
		return "clk_ddc";	/* ddc module clock */
	case INFRA_SYS_CEC_26M:
		return "clk_cec_26m";
	case INFRA_SYS_CEC_32K:
		return "clk_cec_32k";
	case MMSYS_HDMI_PLL:
		return "mmsys_hdmi_pll";
	case MMSYS_HDMI_PIXEL:
		return "mmsys_hdmi_pixel";
	case MMSYS_HDMI_AUDIO:
		return "mmsys_hdmi_audio";
	case MMSYS_HDMI_SPIDIF:
		return "mmsys_hdmi_spidif";

	default:
		return "mediatek,HDMI_UNKNOWN_CLOCK";
	}
}


static void vInitAvInfoVar(void)
{
	_stAvdAVInfo.e_resolution = HDMI_VIDEO_1280x720p_50Hz;
	_stAvdAVInfo.fgHdmiOutEnable = TRUE;
	_stAvdAVInfo.fgHdmiTmdsEnable = TRUE;

	_stAvdAVInfo.bMuteHdmiAudio = FALSE;
	_stAvdAVInfo.e_video_color_space = HDMI_RGB;
	_stAvdAVInfo.e_deep_color_bit = HDMI_NO_DEEP_COLOR;
	_stAvdAVInfo.ui1_aud_out_ch_number = 2;
	_stAvdAVInfo.e_hdmi_fs = HDMI_FS_44K;

	_stAvdAVInfo.bhdmiRChstatus[0] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[1] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[2] = 0x02;
	_stAvdAVInfo.bhdmiRChstatus[3] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[4] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[5] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[0] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[1] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[2] = 0x02;
	_stAvdAVInfo.bhdmiLChstatus[3] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[4] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[5] = 0x00;

	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
}

void vSetHDMIMdiTimeOut(unsigned int i4_count)
{
	HDMI_DRV_FUNC();
	hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] = i4_count;

}

/*----------------------------------------------------------------------------*/

static void hdmi_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(struct HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/

static void hdmi_get_params(struct HDMI_PARAMS *params)
{
	enum HDMI_VIDEO_RESOLUTION input_resolution = params->init_config.vformat;

	HDMI_DRV_LOG("hdmi_get_params resolution(%d)\n", input_resolution);
	memset(params, 0, sizeof(struct HDMI_PARAMS));
	params->input_clock = input_resolution;
	params->init_config.vformat = input_resolution;
	params->is_force_awake = 1;

	switch (input_resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
#endif
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_RISING;
			params->vsync_pol = HDMI_POLARITY_RISING;

			params->hsync_pulse_width = 62;
			params->hsync_back_porch = 60;
			params->hsync_front_porch = 16;

			params->vsync_pulse_width = 6;
			params->vsync_back_porch = 30;
			params->vsync_front_porch = 9;

			/* params->dpi_clock = 27027; */
			params->width = 720;
			params->height = 480;
			break;
		}

	case HDMI_VIDEO_720x576p_50Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
#endif
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_RISING;
			params->vsync_pol = HDMI_POLARITY_RISING;

			params->hsync_pulse_width = 64;
			params->hsync_back_porch = 68;
			params->hsync_front_porch = 12;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 39;
			params->vsync_front_porch = 5;

			/* params->dpi_clock = 27027; */
			params->width = 720;
			params->height = 576;

			break;
		}

	case HDMI_VIDEO_1280x720p_60Hz:
		{
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
#if defined(HDMI_TDA19989)
			params->hsync_pol = HDMI_POLARITY_FALLING;
#else
			params->hsync_pol = HDMI_POLARITY_FALLING;
#endif
			params->vsync_pol = HDMI_POLARITY_FALLING;

#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 40;
			params->hsync_back_porch = 220;
			params->hsync_front_porch = 110;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 20;
			params->vsync_front_porch = 5;

			/* params->dpi_clock = 74250; */
			params->width = 1280;
			params->height = 720;

			break;
		}

	case HDMI_VIDEO_1280x720p_50Hz:
		{
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
#if defined(HDMI_TDA19989)
			params->hsync_pol = HDMI_POLARITY_FALLING;
#else
			params->hsync_pol = HDMI_POLARITY_FALLING;
#endif
			params->vsync_pol = HDMI_POLARITY_FALLING;

#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 40;
			params->hsync_back_porch = 220;
			params->hsync_front_porch = 440;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 20;
			params->vsync_front_porch = 5;

			/* params->dpi_clock = 74250; */
			params->width = 1280;
			params->height = 720;
			break;
		}

	case HDMI_VIDEO_1920x1080p_30Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 88;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 74250; */
			params->width = 1920;
			params->height = 1080;

			break;
		}

	case HDMI_VIDEO_1920x1080p_24Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 638;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 74250; */
			params->width = 1920;
			params->height = 1080;

			break;
		}


	case HDMI_VIDEO_1920x1080p_25Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 528;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 74250; */
			params->width = 1920;
			params->height = 1080;
			break;
		}


	case HDMI_VIDEO_1920x1080p_29Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 88;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 74250; */
			params->width = 1920;
			params->height = 1080;
			break;
		}


	case HDMI_VIDEO_1920x1080p_23Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif

			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 638;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 74250; */
			params->width = 1920;
			params->height = 1080;
			break;
		}

	case HDMI_VIDEO_1920x1080p_60Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif
			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 88;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 148500; */
			params->width = 1920;
			params->height = 1080;
			break;
		}

	case HDMI_VIDEO_1920x1080p_50Hz:
		{
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif
			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 528;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 36;
			params->vsync_front_porch = 4;

			/* params->dpi_clock = 148500; */
			params->width = 1920;
			params->height = 1080;
			break;
		}

	case HDMI_VIDEO_1920x1080i_50Hz:
		{
			/* fgInterlace = TRUE; */
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif
			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 528;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 15;
			params->vsync_front_porch = 2;

			/* params->dpi_clock = 148500; */
			params->width = 1920;
			params->height = 1080;
			break;
		}

	case HDMI_VIDEO_1920x1080i_60Hz:
		{
			/* fgInterlace = TRUE; */
#if defined(MHL_SII8338) || defined(MHL_SII8348)
			params->clk_pol = HDMI_POLARITY_FALLING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#else
			params->clk_pol = HDMI_POLARITY_RISING;
			params->de_pol = HDMI_POLARITY_RISING;
			params->hsync_pol = HDMI_POLARITY_FALLING;
			params->vsync_pol = HDMI_POLARITY_FALLING;
#endif
			params->hsync_pulse_width = 44;
			params->hsync_back_porch = 148;
			params->hsync_front_porch = 88;

			params->vsync_pulse_width = 5;
			params->vsync_back_porch = 15;
			params->vsync_front_porch = 2;

			/* params->dpi_clock = 148500; */
			params->width = 1920;
			params->height = 1080;
			break;
		}

	default:
		break;
	}

#ifndef CONFIG_MTK_HDMI_HDCP_SUPPORT
	params->NeedSwHDCP = 1;
#endif

}

static int hdmi_internal_enter(void)
{
	HDMI_DRV_FUNC();
	return 0;
}

static int hdmi_internal_exit(void)
{
	HDMI_DRV_FUNC();
	return 0;
}

/*----------------------------------------------------------------------------*/

static void hdmi_internal_suspend(void)
{
	HDMI_DRV_FUNC();

	/* _stAvdAVInfo.fgHdmiTmdsEnable = 0; */
	/* av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1); */
}

/*----------------------------------------------------------------------------*/

static void hdmi_internal_resume(void)
{
	HDMI_DRV_FUNC();
}

/*----------------------------------------------------------------------------*/

void HDMI_DisableIrq(void)
{
	vWriteHdmiIntMask(0xFF);
}

void HDMI_EnableIrq(void)
{
	vWriteHdmiIntMask(0xfe);
}

int hdmi_internal_video_config(enum HDMI_VIDEO_RESOLUTION vformat,
			       enum HDMI_VIDEO_INPUT_FORMAT vin, int vout)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.e_resolution = vformat;

#if defined(MHL_BRIDGE_SUPPORT)
	mhl_bridge_drv->mutehdmi(1);
#endif

	av_hdmiset(HDMI_SET_VPLL, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_SOFT_NCTS, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_VIDEO_RES_CHG, &_stAvdAVInfo, 1);
	if (get_boot_mode() != FACTORY_BOOT)
		av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);

#if defined(MHL_BRIDGE_SUPPORT)
	mhl_bridge_drv->resolution((unsigned char)_stAvdAVInfo.e_resolution,
				   (unsigned char)_stAvdAVInfo.e_video_color_space);
#endif

	return 0;
}

int hdmi_audiosetting(struct HDMITX_AUDIO_PARA *audio_para)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.e_hdmi_aud_in = audio_para->e_hdmi_aud_in;
	_stAvdAVInfo.e_iec_frame = audio_para->e_iec_frame;
	_stAvdAVInfo.e_hdmi_fs = audio_para->e_hdmi_fs;
	_stAvdAVInfo.e_aud_code = audio_para->e_aud_code;
	_stAvdAVInfo.u1Aud_Input_Chan_Cnt = audio_para->u1Aud_Input_Chan_Cnt;
	_stAvdAVInfo.e_I2sFmt = audio_para->e_I2sFmt;
	_stAvdAVInfo.u1HdmiI2sMclk = audio_para->u1HdmiI2sMclk;
	_stAvdAVInfo.bhdmiLChstatus[0] = audio_para->bhdmi_LCh_status[0];
	_stAvdAVInfo.bhdmiLChstatus[1] = audio_para->bhdmi_LCh_status[1];
	_stAvdAVInfo.bhdmiLChstatus[2] = audio_para->bhdmi_LCh_status[2];
	_stAvdAVInfo.bhdmiLChstatus[3] = audio_para->bhdmi_LCh_status[3];
	_stAvdAVInfo.bhdmiLChstatus[4] = audio_para->bhdmi_LCh_status[4];
	_stAvdAVInfo.bhdmiRChstatus[0] = audio_para->bhdmi_RCh_status[0];
	_stAvdAVInfo.bhdmiRChstatus[1] = audio_para->bhdmi_RCh_status[1];
	_stAvdAVInfo.bhdmiRChstatus[2] = audio_para->bhdmi_RCh_status[2];
	_stAvdAVInfo.bhdmiRChstatus[3] = audio_para->bhdmi_RCh_status[3];
	_stAvdAVInfo.bhdmiRChstatus[4] = audio_para->bhdmi_RCh_status[4];

	av_hdmiset(HDMI_SET_AUDIO_CHG_SETTING, &_stAvdAVInfo, 1);
	HDMI_DRV_LOG("e_hdmi_aud_in=%d,e_iec_frame=%d,e_hdmi_fs=%d\n", _stAvdAVInfo.e_hdmi_aud_in,
		     _stAvdAVInfo.e_iec_frame, _stAvdAVInfo.e_hdmi_fs);
	HDMI_DRV_LOG("e_aud_code=%d,u1Aud_Input_Chan_Cnt=%d,e_I2sFmt=%d\n", _stAvdAVInfo.e_aud_code,
		     _stAvdAVInfo.u1Aud_Input_Chan_Cnt, _stAvdAVInfo.e_I2sFmt);
	HDMI_DRV_LOG("u1HdmiI2sMclk=%d\n", _stAvdAVInfo.u1HdmiI2sMclk);

	HDMI_DRV_LOG("bhdmiLChstatus0=%d\n", _stAvdAVInfo.bhdmiLChstatus[0]);
	HDMI_DRV_LOG("bhdmiLChstatus1=%d\n", _stAvdAVInfo.bhdmiLChstatus[1]);
	HDMI_DRV_LOG("bhdmiLChstatus2=%d\n", _stAvdAVInfo.bhdmiLChstatus[2]);
	HDMI_DRV_LOG("bhdmiLChstatus3=%d\n", _stAvdAVInfo.bhdmiLChstatus[3]);
	HDMI_DRV_LOG("bhdmiLChstatus4=%d\n", _stAvdAVInfo.bhdmiLChstatus[4]);
	HDMI_DRV_LOG("bhdmiRChstatus0=%d\n", _stAvdAVInfo.bhdmiRChstatus[0]);
	HDMI_DRV_LOG("bhdmiRChstatus1=%d\n", _stAvdAVInfo.bhdmiRChstatus[1]);
	HDMI_DRV_LOG("bhdmiRChstatus2=%d\n", _stAvdAVInfo.bhdmiRChstatus[2]);
	HDMI_DRV_LOG("bhdmiRChstatus3=%d\n", _stAvdAVInfo.bhdmiRChstatus[3]);
	HDMI_DRV_LOG("bhdmiRChstatus4=%d\n", _stAvdAVInfo.bhdmiRChstatus[4]);


	return 0;
}

int hdmi_tmdsonoff(unsigned char u1ionoff)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = u1ionoff;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	return 0;
}

static void config_tvdpll(unsigned int resolutionmode)
{
	int pcw_value;

	switch (resolutionmode) {
		/*TVD PLL 432MHz */
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		pcw_value = 1088906;
		break;

		/*TVD PLL 594MHz */
	case HDMI_VIDEO_1920x1080p_30Hz:
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
		pcw_value = 1497246;
		break;

		/* TVD PLL 593.4MHz */
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
		pcw_value = 1495733;
		break;

	default:
		pcw_value = 1495733;
		break;
	}

	pr_err("pcw_value = %d\n", pcw_value);
	udelay(10);
	vWriteIoPllMsk(TVDPLL_PWR_CON0, 0x3, TVDPLL_PWR_ON | TVDPLL_SDM_ISO_EN);
	udelay(10);
	vWriteIoPllMsk(TVDPLL_PWR_CON0, 0x0, TVDPLL_SDM_ISO_EN);
	udelay(10);
	vWriteIoPllMsk(TVDPLL_CON1, (pcw_value << RG_TVDPLL_SDM_PCW), RG_TVDPLL_SDM_PCW_MASK);
	udelay(10);
	vWriteIoPllMsk(TVDPLL_CON1, TVDPLL_SDM_PCW_CHG, TVDPLL_SDM_PCW_CHG);
	udelay(10);
	vWriteIoPllMsk(TVDPLL_CON0, 0x1, TVDPLL_EN);

}

static void dpi_setting_resolution(unsigned char arg)
{
	switch (arg) {
	case HDMI_VIDEO_1280x720p_60Hz:
		{

			hdmi_write(0x14019000, 0x1);
			hdmi_write(0x14019004, 0x0);
			hdmi_write(0x14019008, 0x1);
			hdmi_write(0x1401900c, 0x0);
			hdmi_write(0x14019010, 0x00410000);
			hdmi_write(0x140190e0, 0x82000200);
			hdmi_write(0x14019014, 0x00006000);
			hdmi_write(0x14019018, 0x02d00500);
			hdmi_write(0x14019020, 0x00000028);
			hdmi_write(0x14019024, 0x006e00dc);
			hdmi_write(0x14019028, 0x00000005);
			hdmi_write(0x1401902c, 0x00050014);
			hdmi_write(0x14019068, 0x00000000);
			hdmi_write(0x1401906c, 0x00000000);
			hdmi_write(0x14019070, 0x00000000);
			hdmi_write(0x14019074, 0x00000000);
			hdmi_write(0x14019078, 0x00000000);
			hdmi_write(0x1401907c, 0x00000000);
			hdmi_write(0x140190a0, 0x00000000);
			hdmi_write(0x14019f00, 0x00000041);
			break;
		}

	case HDMI_VIDEO_1920x1080p_60Hz:
		{

			hdmi_write(0x14019000, 0x1);
			hdmi_write(0x14019004, 0x0);
			hdmi_write(0x14019008, 0x1);
			hdmi_write(0x1401900c, 0x0);
			hdmi_write(0x14019010, 0x00410000);
			hdmi_write(0x140190e0, 0x82000200);
			hdmi_write(0x14019014, 0x00006000);
			hdmi_write(0x14019018, 0x04380780);
			hdmi_write(0x14019020, 0x0000002c);
			hdmi_write(0x14019024, 0x00580094);
			hdmi_write(0x14019028, 0x00000005);
			hdmi_write(0x1401902c, 0x00040024);
			hdmi_write(0x14019068, 0x00000000);
			hdmi_write(0x1401906c, 0x00000000);
			hdmi_write(0x14019070, 0x00000000);
			hdmi_write(0x14019074, 0x00000000);
			hdmi_write(0x14019078, 0x00000000);
			hdmi_write(0x1401907c, 0x00000000);
			hdmi_write(0x140190a0, 0x00000001);
			hdmi_write(0x14019f00, 0x00000041);

			break;
		}

	default:
		break;
	}

}


void hdmi_resolution_debug_setting(int res)
{
	pr_err(">>> hdmi_resolution_debug_setting\n");

	config_tvdpll(res);
	dpi_setting_resolution(res);
	hdmi_internal_video_config(res, HDMI_VIN_FORMAT_RGB888, 0);

	pr_err("<<< hdmi_resolution_debug_setting\n");
}


/*----------------------------------------------------------------------------*/

static int hdmi_internal_audio_config(enum HDMI_AUDIO_FORMAT aformat, int sampleBit)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_video_enable(bool enable)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_audio_enable(bool enable)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void hdmi_internal_set_mode(unsigned char ucMode)
{
	HDMI_DRV_FUNC();
	vSetClk();

}

/*----------------------------------------------------------------------------*/
static void hdmi_clock_enable(bool bEnable)
{
	int i;

	if (bEnable) {
		HDMI_DRV_LOG("Enable hdmi clocks\n");
		for (i = 0; i < HDMI_SEL_CLOCK_NUM; i++) {
			if ((i != INFRA_SYS_CEC_26M) && (i != INFRA_SYS_CEC_32K)) {
				clk_prepare(hdmi_ref_clock[i]);
				clk_enable(hdmi_ref_clock[i]);
			}
		}
	} else {
		HDMI_DRV_LOG("Disable hdmi clocks\n");
		for (i = HDMI_SEL_CLOCK_NUM - 1; i >= 0; i--) {
			if ((i != INFRA_SYS_CEC_26M) && (i != INFRA_SYS_CEC_32K)) {
				clk_disable(hdmi_ref_clock[i]);
				clk_unprepare(hdmi_ref_clock[i]);
			}
		}
	}
}

static void hdmi_poll_isr(unsigned long n)
{
	unsigned int i;

	for (i = 0; i < MAX_HDMI_TMR_NUMBER; i++) {
		if (hdmi_TmrValue[i] >= AVD_TMR_ISR_TICKS) {
			hdmi_TmrValue[i] -= AVD_TMR_ISR_TICKS;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else if (hdmi_TmrValue[i] > 0) {
			hdmi_TmrValue[i] = 0;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
	}

	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
	mod_timer(&r_hdmi_timer, jiffies + gHDMI_CHK_INTERVAL / (1000 / HZ));

}


static void cec_poll_isr(unsigned long n)
{
	if (hdmi_cec_on == 1) {
		atomic_set(&cec_timer_event, 1);
		wake_up_interruptible(&cec_timer_wq);
		cec_timer_wakeup();
	}
}


int hdmi_internal_power_on(void)
{

	HDMI_DRV_FUNC();

	if (hdmi_powerenable == 1) {
		HDMI_DRV_LOG("Already Power on,Return directly\n");
		return 0;
	}
#ifndef CONFIG_PM
#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (hdmi_hdmiearlysuspend == 0) {
		HDMI_DRV_LOG("hdmi_hdmiearlysuspend ,Return directly\n");
		return 0;
	}
#endif
#endif
	if (hdmi_hdmiearlysuspend == 0) {
		HDMI_DRV_LOG("hdmi_hdmiearlysuspend ,Return directly\n");
		return 0;
	}

	hdmi_powerenable = 1;
	if (hdmi_cec_on == 0) {
		clk_prepare(hdmi_ref_clock[INFRA_SYS_CEC_26M]);
		clk_enable(hdmi_ref_clock[INFRA_SYS_CEC_26M]);
	}
#if defined(MHL_BRIDGE_SUPPORT)
	mhl_bridge_drv->power_on();
#endif
	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

	if (hdmi_power_control_pin > 0) {
		pr_err("[hdmi]hdmi control pin number is %d\n", hdmi_power_control_pin);
		gpio_direction_output(hdmi_power_control_pin, 1);
		gpio_set_value(hdmi_power_control_pin, 1);
	}
	mtk_smi_larb_clock_on(0, true);
	vWriteHdmiSYSMsk(DISP_HDMI_SYS_CFG_01, HDMI_PCLK_FREE_RUN, HDMI_PCLK_FREE_RUN);
	vWriteHdmiSYSMsk(DISP_HDMI_SYS_CFG_01, HDMI_OUT_FIFO_EN, HDMI_OUT_FIFO_EN);
	vWriteHdmiSYSMsk(DISP_HDMI_SYS_CFG_00, ANLG_ON | HDMI_SPIDIF_ON, ANLG_ON | HDMI_SPIDIF_ON);
	vWriteHdmiSYSMsk(DISP_HDMI_SYS_CFG_01, HDMI_PSECURE_EN, HDMI_PSECURE_EN_MASK);
	vInitHdcpKeyGetMethod(NON_HOST_ACCESS_FROM_EEPROM);

	/* HDMI_DisableIrq(); */
	port_hpd_value_bak = 0xff;
	HDMI_EnableIrq();

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 1000 / (1000 / HZ);
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);
	add_timer(&r_hdmi_timer);


	memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
	r_cec_timer.expires = jiffies + 1000 / (1000 / HZ);
	r_cec_timer.function = cec_poll_isr;
	r_cec_timer.data = 0;
	init_timer(&r_cec_timer);
	add_timer(&r_cec_timer);

	atomic_set(&hdmi_irq_event, 1);
	wake_up_interruptible(&hdmi_irq_wq);

	return 0;
}

/*----------------------------------------------------------------------------*/

void hdmi_internal_power_off(void)
{

	HDMI_DRV_FUNC();

	if (hdmi_powerenable == 0)
		return;

	hdmi_powerenable = 0;

#if defined(MHL_BRIDGE_SUPPORT)
	mhl_bridge_drv->power_off();
#endif

	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

	HDMI_DisableIrq();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
	vWriteHdmiSYSMsk(DISP_HDMI_SYS_CFG_00, 0, ANLG_ON | HDMI_SPIDIF_ON);
	vWriteHdmiSYSMsk(DISP_HDMI_SYS_CFG_01, 0, HDMI_PCLK_FREE_RUN);

#ifdef GPIO_HDMI_POWER_CONTROL
	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ZERO);
#endif

	if (hdmi_power_control_pin > 0) {
		pr_err("[hdmi]hdmi control pin number is %d\n", hdmi_power_control_pin);
		gpio_direction_output(hdmi_power_control_pin, 0);
		gpio_set_value(hdmi_power_control_pin, 0);
	}

	if (hdmi_clockenable == 1) {
		HDMI_PLUG_LOG("hdmi internal power off:disable clock\n");
		hdmi_clockenable = 0;
		hdmi_clock_enable(false);
	}
	mtk_smi_larb_clock_off(0, true);
	if (hdmi_cec_on == 0) {
		clk_disable(hdmi_ref_clock[INFRA_SYS_CEC_26M]);
		clk_unprepare(hdmi_ref_clock[INFRA_SYS_CEC_26M]);
	}
	if (r_hdmi_timer.function)
		del_timer_sync(&r_hdmi_timer);
	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));

	if (r_cec_timer.function)
		del_timer_sync(&r_cec_timer);
	memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
}

/*----------------------------------------------------------------------------*/

void hdmi_internal_dump(void)
{
	HDMI_DRV_FUNC();
	/* hdmi_dump_reg(); */
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

enum HDMI_STATE hdmi_get_state(void)
{
	HDMI_DRV_FUNC();

	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;
}

void hdmi_enablehdcp(unsigned char u1hdcponoff)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

void hdmi_setcecrxmode(unsigned char u1cecrxmode)
{
	HDMI_DRV_FUNC();

	hdmi_rxcecmode = u1cecrxmode;
}

void hdmi_colordeep(unsigned char u1colorspace, unsigned char u1deepcolor)
{
	HDMI_DRV_FUNC();

	if ((u1colorspace == 0xff) && (u1deepcolor == 0xff)) {
		pr_err("color_space:HDMI_YCBCR_444 = 2\n");
		pr_err("color_space:HDMI_YCBCR_422 = 3\n");
		pr_err("deep_color:HDMI_NO_DEEP_COLOR = 1\n");
		pr_err("deep_color:HDMI_DEEP_COLOR_10_BIT = 2\n");
		pr_err("deep_color:HDMI_DEEP_COLOR_12_BIT = 3\n");
		pr_err("deep_color:HDMI_DEEP_COLOR_16_BIT = 4\n");

		return;
	}

	_stAvdAVInfo.e_video_color_space = HDMI_RGB_FULL;
	_stAvdAVInfo.e_deep_color_bit = (enum HDMI_DEEP_COLOR_T)u1deepcolor;
}

void hdmi_read(unsigned long u2Reg, unsigned int *p4Data)
{
	switch (u2Reg & 0x1ffff000) {
	case 0x1401b000:
		internal_hdmi_read(hdmi_reg[HDMI_SHELL] + u2Reg - 0x1401b000, p4Data);
		break;

	case 0x11011000:
		internal_hdmi_read(hdmi_reg[HDMI_DDC] + u2Reg - 0x11011000, p4Data);
		break;

	case 0x1001a000:
		internal_hdmi_read(hdmi_reg[HDMI_CEC] + u2Reg - 0x1001a000, p4Data);
		break;

	case 0x10018000:
		internal_hdmi_read(hdmi_ref_reg[AP_CCIF0] + u2Reg - 0x10018000, p4Data);
		break;

	case 0x10000000:
		internal_hdmi_read(hdmi_ref_reg[TOPCK_GEN] + u2Reg - 0x10000000, p4Data);
		break;

	case 0x10001000:
		internal_hdmi_read(hdmi_ref_reg[INFRA_SYS] + u2Reg - 0x10001000, p4Data);
		break;

	case 0x14000000:
		internal_hdmi_read(hdmi_ref_reg[MMSYS_CONFIG] + u2Reg - 0x14000000, p4Data);
		break;

	case 0x10005000:
		internal_hdmi_read(hdmi_ref_reg[GPIO_REG] + u2Reg - 0x10005000, p4Data);
		break;

	case 0x14019000:
		internal_hdmi_read(hdmi_ref_reg[DPI1_REG] + u2Reg - 0x14019000, p4Data);
		break;

	default:
		break;
	}
}

void hdmi_write(unsigned long u2Reg, unsigned int u4Data)
{

	switch (u2Reg & 0x1ffff000) {
	case 0x1401b000:
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
		vCaHDMIWriteReg(u2Reg - 0x1401b000, u4Data);
#else
		internal_hdmi_write(hdmi_reg[HDMI_SHELL] + u2Reg - 0x1401b000, u4Data);
#endif
		break;

	case 0x11001000:
		internal_hdmi_write(hdmi_reg[HDMI_DDC] + u2Reg - 0x11001000, u4Data);
		break;

	case 0x1001a000:
		internal_hdmi_write(hdmi_reg[HDMI_CEC] + u2Reg - 0x1001a000, u4Data);
		break;

	case 0x10018000:
		internal_hdmi_write(hdmi_ref_reg[AP_CCIF0] + u2Reg - 0x10018000, u4Data);
		break;

	case 0x10000000:
		internal_hdmi_write(hdmi_ref_reg[TOPCK_GEN] + u2Reg - 0x10000000, u4Data);
		break;

	case 0x10010000:
		internal_hdmi_write(hdmi_ref_reg[INFRA_SYS] + u2Reg - 0x10010000, u4Data);
		break;

	case 0x14000000:
		internal_hdmi_write(hdmi_ref_reg[MMSYS_CONFIG] + u2Reg - 0x14000000, u4Data);
		break;

	case 0x10005000:
		internal_hdmi_write(hdmi_ref_reg[GPIO_REG] + u2Reg - 0x10005000, u4Data);
		break;

	case 0x14019000:
		internal_hdmi_write(hdmi_ref_reg[DPI1_REG] + u2Reg - 0x14019000, u4Data);
		break;

	default:
		break;
	}
	pr_err("Reg write= 0x%08lx, data = 0x%08x\n", u2Reg, u4Data);
}

#if 0
#if defined(CONFIG_HAS_EARLYSUSPEND)
static void hdmi_hdmi_early_suspend(struct early_suspend *h)
{
	HDMI_PLUG_FUNC();
	hdmi_hdmiearlysuspend = 0;
}

static void hdmi_hdmi_late_resume(struct early_suspend *h)
{
	HDMI_PLUG_FUNC();
	hdmi_hdmiearlysuspend = 1;
}

static struct early_suspend hdmi_hdmi_early_suspend_desc = {
	.level = 0xFE,
	.suspend = hdmi_hdmi_early_suspend,
	.resume = hdmi_hdmi_late_resume,
};
#endif
#endif
static int hdmi_event_notifier_callback(struct notifier_block *self,
					unsigned long action, void *data)
{
	struct fb_event *event = data;
	int blank_mode = 0;

	if (action != FB_EARLY_EVENT_BLANK)
		return 0;

	if ((event == NULL) || (event->data == NULL))
		return 0;
	blank_mode = *((int *)event->data);

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		hdmi_hdmiearlysuspend = 1;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		hdmi_hdmiearlysuspend = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct notifier_block hdmi_event_notifier = {
	.notifier_call = hdmi_event_notifier_callback,
};

static irqreturn_t hdmi_irq_handler(int irq, void *dev_id)
{
	unsigned char port_hpd_value;

	HDMI_DRV_FUNC();
	vClear_cec_irq();
	HDMI_DisableIrq();

	if (hdmi_cec_on == 1)
		hdmi_cec_isrprocess(hdmi_rxcecmode);

	port_hpd_value = hdmi_get_port_hpd_value();
	if (port_hpd_value != port_hpd_value_bak) {
		port_hpd_value_bak = port_hpd_value;
		atomic_set(&hdmi_irq_event, 1);
		wake_up_interruptible(&hdmi_irq_wq);
	}

	HDMI_EnableIrq();
	return IRQ_HANDLED;
}


void hdmi_clock_probe(struct platform_device *pdev)
{
	int i;

	HDMI_DRV_LOG("Probe clocks start\n");

	/* ASSERT(pdev != NULL); */
	if (pdev == NULL) {
		pr_err("[HDMI] pdev Error\n");
		return;
	}

	pm_runtime_enable(&pdev->dev);
	hdmi_pdev = pdev;

	for (i = 0; i < HDMI_SEL_CLOCK_NUM; i++) {
		hdmi_ref_clock[i] = devm_clk_get(&pdev->dev, hdmi_use_clock_name_spy(i));
		WARN_ON(IS_ERR(hdmi_ref_clock[i]));

		HDMI_DRV_LOG("Get Clock %s\n", hdmi_use_clock_name_spy(i));
	}
}

int hdmi_internal_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	unsigned int reg_value;
	struct device_node *np;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;

	HDMI_DRV_LOG("[hdmi_internal_probe] probe start\n");


	if (pdev->dev.of_node == NULL) {
		pr_err("[hdmi_internal_probe] Device Node Error\n");
		return -1;
	}

	for (i = 0; i < HDMI_REG_NUM; i++) {
		hdmi_reg[i] = (unsigned long)of_iomap(pdev->dev.of_node, i);
		if (!hdmi_reg[i]) {
			HDMI_DRV_LOG("Unable to ioremap registers, of_iomap fail, i=%d\n", i);
			return -ENOMEM;
		}
		HDMI_DRV_LOG("DT, i=%d, map_addr=0x%lx, reg_pa=0x%x\n",
			     i, hdmi_reg[i], hdmi_reg_pa_base[i]);
	}

	/*get IRQ ID and request IRQ */
	hdmi_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	HDMI_DRV_LOG("hdmi_irq = %d\n", hdmi_irq);
	if (hdmi_irq > 0) {
		if (request_irq(hdmi_irq, hdmi_irq_handler, IRQF_TRIGGER_LOW, "mtkhdmi", NULL) < 0)
			pr_err("request interrupt failed.\n");
	}

	/* Get 5V DDC Power Control Pin */
	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		dev_err(&pdev->dev, "hdmi power control pin, failure of setting\n");
	} else {
		pins_default = pinctrl_lookup_state(pinctrl, "default");
		if (IS_ERR(pins_default)) {
			ret = PTR_ERR(pins_default);
			dev_err(&pdev->dev, "cannot find hdmi pinctrl default");
		} else {
			pinctrl_select_state(pinctrl, pins_default);
		}
	}
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8167-hdmitx");
	/* Get 5V DDC Power Control Pin */
	hdmi_power_control_pin = of_get_named_gpio(np, "hdmi_power_control", 0);
	if (hdmi_power_control_pin > 0) {
		ret = gpio_request(hdmi_power_control_pin, "hdmi power control pin");
		if (ret)
			pr_err("hdmi power control pin, failure of setting\n");
	}
	/* Get PLL/TOPCK/INFRA_SYS/MMSSYS /GPIO/EFUSE  phy base address and iomap virtual address */
	for (i = 0; i < HDMI_REF_REG_NUM; i++) {
		np = of_find_compatible_node(NULL, NULL, hdmi_use_module_name_spy(i));
		if (np == NULL)
			continue;

		of_property_read_u32_index(np, "reg", 0, &reg_value);
		hdmi_ref_reg[i] = (unsigned long)of_iomap(np, 0);
		HDMI_DRV_LOG("DT HDMI_Ref|%s, reg base: 0x%x --> map:0x%lx\n",
			     np->name, reg_value, hdmi_ref_reg[i]);
	}
	/*Probe Clocks that will be used by hdmi module */
	hdmi_clock_probe(pdev);
	hdmi_cec_power_on(1);
	vCec_pdn_32k();
	vHotPlugPinInit(pdev);
	return 0;

}


static int hdmi_internal_remove(struct platform_device *dev)
{
	HDMI_DRV_FUNC();
	return 0;
}

static const struct of_device_id hdmi_of_ids[] = {
	{.compatible = "mediatek,mt8167-hdmitx",},
	{}
};

static struct platform_driver hdmi_of_driver = {
	.probe = hdmi_internal_probe,
	.remove = hdmi_internal_remove,
	.driver = {
		   .name = "mtkhdmi",
		   .of_match_table = hdmi_of_ids,
		   }
};

static int hdmi_internal_init(void)
{
	HDMI_DRV_FUNC();

#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&hdmi_hdmi_early_suspend_desc);
#endif
	fb_register_client(&hdmi_event_notifier);

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	pr_err("[HDMI]fgCaHDMICreate\n");
	fgCaHDMICreate();
#endif

#if defined(MHL_BRIDGE_SUPPORT)
	mhl_bridge_drv = NULL;
	mhl_bridge_drv = MHL_Bridge_GetDriver();
#endif

	return 0;
}


static void vNotifyAppHdmiState(unsigned char u1hdmistate)
{
	HDMI_EDID_T get_info;

	HDMI_PLUG_LOG("u1hdmistate = %d,%s\n", u1hdmistate, szHdmiPordStatusStr[u1hdmistate]);

	hdmi_AppGetEdidInfo(&get_info);

	switch (u1hdmistate) {
	case HDMI_PLUG_OUT:
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		hdmi_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
		hdmi_SetPhysicCECAddress(0xffff, 0x0);
		/* hdmi_util.cec_state_callback(HDMI_CEC_STATE_PLUG_OUT); */
		break;

	case HDMI_PLUG_IN_AND_SINK_POWER_ON:
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		hdmi_invoke_cable_callbacks(HDMI_STATE_ACTIVE);
		hdmi_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0x4);
		/* hdmi_util.cec_state_callback(HDMI_CEC_STATE_GET_PA); */
		break;

	case HDMI_PLUG_IN_ONLY:
		hdmi_util.state_callback(HDMI_STATE_PLUGIN_ONLY);
		hdmi_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
		hdmi_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0xf);
		/*  hdmi_util.cec_state_callback(HDMI_CEC_STATE_GET_PA); */
		break;

	case HDMI_PLUG_IN_CEC:
		hdmi_util.state_callback(HDMI_STATE_CEC_UPDATE);
		hdmi_invoke_cable_callbacks(HDMI_STATE_NO_DEVICE);
		break;

	default:
		break;

	}
}

void vNotifyAppHdmiCecState(enum HDMI_NFY_CEC_STATE_T u1hdmicecstate)
{
	HDMI_CEC_LOG("u1hdmicecstate = %d, %s\n", u1hdmicecstate,
		     szHdmiCecPordStatusStr[u1hdmicecstate]);

	switch (u1hdmicecstate) {
	case HDMI_CEC_TX_STATUS:
		hdmi_util.cec_state_callback(HDMI_CEC_STATE_TX_STS);
		break;

	case HDMI_CEC_GET_CMD:
		hdmi_util.cec_state_callback(HDMI_CEC_STATE_GET_CMD);
		break;

	default:
		break;
	}
}

static void vPlugDetectService(enum HDMI_CTRL_STATE_T e_state)
{
	unsigned char bData = 0xff;

	HDMI_PLUG_FUNC();
	e_hdmi_ctrl_state = HDMI_STATE_IDLE;

	switch (e_state) {
	case HDMI_STATE_HOT_PLUG_OUT:
		vClearEdidInfo();
		vHDCPReset();
		if (hdmi_clockenable == 1) {
			HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug out\n");
			hdmi_clockenable = 0;
			hdmi_clock_enable(false);
		}
		bData = HDMI_PLUG_OUT;

		break;

	case HDMI_STATE_HOT_PLUGIN_AND_POWER_ON:
		if (hdmi_clockenable == 0) {
			HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug in and power on\n");
			hdmi_clockenable = 1;
			hdmi_clock_enable(true);
		}
		hdmi_checkedid(0);
		bData = HDMI_PLUG_IN_AND_SINK_POWER_ON;

		break;

	case HDMI_STATE_HOT_PLUG_IN_ONLY:
		if (hdmi_clockenable == 0) {
			HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug in only\n");
			hdmi_clockenable = 1;
			hdmi_clock_enable(true);
		}
		vClearEdidInfo();
		vHDCPReset();
		hdmi_checkedid(0);
		bData = HDMI_PLUG_IN_ONLY;

		break;

	case HDMI_STATE_IDLE:

		break;
	default:
		break;

	}

	if (bData != 0xff)
		vNotifyAppHdmiState(bData);
}

void hdmi_drvlog_enable(unsigned short enable)
{
	HDMI_DRV_FUNC();

	if (enable == 0) {
		pr_err("hdmi_pll_log =   0x1\n");
		pr_err("hdmi_dgi_log =   0x2\n");
		pr_err("hdmi_plug_log =  0x4\n");
		pr_err("hdmi_video_log = 0x8\n");
		pr_err("hdmi_audio_log = 0x10\n");
		pr_err("hdmi_hdcp_log =  0x20\n");
		pr_err("hdmi_cec_log =   0x40\n");
		pr_err("hdmi_ddc_log =   0x80\n");
		pr_err("hdmi_edid_log =  0x100\n");
		pr_err("hdmi_drv_log =   0x200\n");

		pr_err("hdmi_all_log =   0x3ff\n");

	}

	hdmidrv_log_on = enable;

	if ((enable & 0xc000) == 0xc000) {
		;
	} else if ((enable & 0x8000) == 0x8000) {
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	} else if ((enable & 0x4000) == 0x4000) {

		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
		vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
	}

}

#ifdef CONFIG_PM
int hdmi_drv_pm_restore_noirq(struct device *device)
{
#if defined(CONFIG_HAS_EARLYSUSPEND)
	hdmi_hdmiearlysuspend = 1;
#endif
	hdmi_hdmiinit = 0;

	/* mt_irq_set_sens(hdmi_irq, MT_LEVEL_SENSITIVE); */
	/* mt_irq_set_polarity(hdmi_irq, MT_POLARITY_LOW); */
	return 0;
}



void hdmi_module_init(void)
{

	/* must set this bit, otherwise hpd/port and cec module can not read/write */
	vWriteHdmiTOPCKMsk(INFRA_PDN1, CEC_PDN1, CEC_PDN1);
	udelay(2);

	/* power cec module for cec and hpd/port detect */
	hdmi_cec_power_on(1);
	vCec_pdn_32k();
	/*vHotPlugPinInit(); */

	/* hdmi_internal_power_off(); */
	/* vMoveHDCPInternalKey(INTERNAL_ENCRYPT_KEY); */
	vInitAvInfoVar();

	if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUG_OUT)
	    && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)) {
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
		vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
	}
	return;

}

#endif

void hdmi_timer_impl(void)
{
	if (hdmi_hdmiinit == 0) {
		hdmi_hdmiinit = 1;

		/* must set this bit, otherwise hpd/port and cec module can not read/write */
		/* vWriteHdmiTOPCKMsk(INFRA_PDN1, CEC_PDN1,CEC_PDN1); */

		vWriteINFRASYS(INFRA_PDN1, CEC_PDN1);
		udelay(2);

		/* power cec module for cec and hpd/port detect */
		hdmi_cec_power_on(1);
		vCec_pdn_32k();
		/*vHotPlugPinInit(); */

		/* hdmi_internal_power_off(); */
		/* vMoveHDCPInternalKey(INTERNAL_ENCRYPT_KEY); */
		vInitAvInfoVar();


		if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUG_OUT)
		    && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)) {
			hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
			vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
			vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
		}
		return;
	}

	if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
	    && ((e_hdcp_ctrl_state == HDCP_WAIT_RI)
		|| (e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY))) {
		if (bCheckHDCPStatus(HDCP_STA_RI_RDY)) {
			vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
	}

	if (hdmi_hdmiCmd == HDMI_PLUG_DETECT_CMD) {
		vClearHdmiCmd();
		/* vcheckhdmiplugstate(); */
		/* vPlugDetectService(e_hdmi_ctrl_state); */
	} else if (hdmi_hdmiCmd == HDMI_HDCP_PROTOCAL_CMD) {
		vClearHdmiCmd();
		HdcpService(e_hdcp_ctrl_state);
	}
}


void cec_timer_impl(void)
{
	if (hdmi_cec_on == 1)
		hdmi_cec_mainloop(hdmi_rxcecmode);

}

void hdmi_irq_impl(void)
{
	msleep(50);

	if (down_interruptible(&hdmi_update_mutex)) {
		pr_err("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUG_OUT)
	    && (bCheckPordHotPlug(HOTPLUG_MODE) == FALSE)) {
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
		HDMI_PLUG_LOG("hdmi plug out return\n");
	} else if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
		   && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)) {
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
		vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
		HDMI_PLUG_LOG("hdmi plug in return\n");
	} else if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUG_IN_ONLY)
		   && (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)
		   && (bCheckPordHotPlug(PORD_MODE) == FALSE)) {
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_IN_ONLY;
		/* vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON); */
		/* vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON); */
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_STATE_HOT_PLUG_IN_ONLY);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_IN_ONLY);
		HDMI_PLUG_LOG("hdmi plug in only return\n");
	}
	up(&hdmi_update_mutex);

}

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 91 };

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_timer_wq, atomic_read(&hdmi_timer_event));
		atomic_set(&hdmi_timer_event, 0);
		hdmi_timer_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int cec_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 91 };

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(cec_timer_wq, atomic_read(&cec_timer_event));
		atomic_set(&cec_timer_event, 0);
		cec_timer_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int hdmi_irq_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_irq_wq, atomic_read(&hdmi_irq_event));
		atomic_set(&hdmi_irq_event, 0);
		hdmi_irq_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

#define HDMI_MAX_INSERT_CALLBACK   10
static CABLE_INSERT_CALLBACK hdmi_callback_table[HDMI_MAX_INSERT_CALLBACK];
void hdmi_register_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
	int i = 0;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == cb)
			break;
	}
	if (i < HDMI_MAX_INSERT_CALLBACK)
		return;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == NULL)
		break;
	}
	if (i == HDMI_MAX_INSERT_CALLBACK) {
		HDMI_DRV_LOG("not enough mhl callback entries for module\n");
		return;
	}

	hdmi_callback_table[i] = cb;
	HDMI_DRV_LOG("callback: %p,i: %d\n", hdmi_callback_table[i], i);
}

void hdmi_unregister_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
	int i;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == cb) {
			HDMI_DRV_LOG("unregister cable insert callback: %p, i: %d\n", hdmi_callback_table[i], i);
			hdmi_callback_table[i] = NULL;
			break;
		}
	}
	if (i == HDMI_MAX_INSERT_CALLBACK) {
		HDMI_DRV_LOG("Try to unregister callback function 0x%lx which was not registered\n",
				(unsigned long int)cb);
		return;
	}
}

static void hdmi_invoke_cable_callbacks(enum HDMI_STATE state)
{
	int i = 0, j = 0;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i])
			j = i;
	}

	if (hdmi_callback_table[j]) {
		HDMI_DRV_LOG("callback: %p, state: %d, j: %d\n", hdmi_callback_table[j], state, j);
		hdmi_callback_table[j](state);
	}
}

void cec_timer_wakeup(void)
{
	if (r_cec_timer.function)
		mod_timer(&r_cec_timer, jiffies + gCEC_CHK_INTERVAL / (1000 / HZ));
}

const struct HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = hdmi_set_util_funcs,
		.get_params = hdmi_get_params,
		.init = hdmi_internal_init,
		.enter = hdmi_internal_enter,
		/* .hdmidrv_probe = hdmi_internal_probe,*/
		.exit = hdmi_internal_exit,
		.suspend = hdmi_internal_suspend,
		.resume = hdmi_internal_resume,
		.video_config = hdmi_internal_video_config,
		.audio_config = hdmi_internal_audio_config,
		.video_enable = hdmi_internal_video_enable,
		.audio_enable = hdmi_internal_audio_enable,
		.power_on = hdmi_internal_power_on,
		.power_off = hdmi_internal_power_off,
		.set_mode = hdmi_internal_set_mode,
		.dump = hdmi_internal_dump,
		.read = hdmi_read,
		.write = hdmi_write,
		.get_state = hdmi_get_state,
		.log_enable = hdmi_drvlog_enable,
		.InfoframeSetting = hdmi_InfoframeSetting,
		.checkedid = hdmi_checkedid,
		.colordeep = hdmi_colordeep,
		.enablehdcp = hdmi_enablehdcp,
		.setcecrxmode = hdmi_setcecrxmode,
		.hdmistatus = hdmi_hdmistatus,
		.hdcpkey = hdmi_hdcpkey,
		.getedid = hdmi_AppGetEdidInfo,
		.setcecla = hdmi_CECMWSetLA,
		.sendsltdata = hdmi_u4CecSendSLTData,
		.getceccmd = hdmi_CECMWGet,
		.getsltdata = hdmi_GetSLTData,
		.setceccmd = hdmi_CECMWSend,
		.cecenable = hdmi_CECMWSetEnableCEC,
		.getcecaddr = hdmi_NotifyApiCECAddress,
		.getcectxstatus = hdmi_cec_api_get_txsts,
		.audiosetting = hdmi_audiosetting,
		.tmdsonoff = hdmi_tmdsonoff,
		.mutehdmi = vDrm_mutehdmi,
		.svpmutehdmi = vSvp_mutehdmi,
		.cecusrcmd = hdmi_cec_usr_cmd,
		.checkedidheader = hdmi_check_edid_header,
		.resolution_setting = hdmi_resolution_debug_setting,
		.get_external_device_capablity = hdmi_drv_get_external_device_capablity,
		.register_callback   = hdmi_register_cable_insert_callback,
		.unregister_callback = hdmi_unregister_cable_insert_callback,
	};

	return &HDMI_DRV;
}

EXPORT_SYMBOL(HDMI_GetDriver);

static int __init mtk_hdmitx_init(void)
{
	int ret = 0;

	HDMI_DRV_FUNC();

	init_waitqueue_head(&hdmi_timer_wq);
	hdmi_timer_task = kthread_create(hdmi_timer_kthread, NULL, "hdmi_timer_kthread");
	wake_up_process(hdmi_timer_task);

	init_waitqueue_head(&cec_timer_wq);
	cec_timer_task = kthread_create(cec_timer_kthread, NULL, "cec_timer_kthread");
	wake_up_process(cec_timer_task);

	init_waitqueue_head(&hdmi_irq_wq);
	hdmi_irq_task = kthread_create(hdmi_irq_kthread, NULL, "hdmi_irq_kthread");
	wake_up_process(hdmi_irq_task);

	if (platform_driver_register(&hdmi_of_driver)) {
		pr_err("failed to register disp driver\n");
		ret = -1;
	}
	return ret;

}

static void __exit mtk_hdmitx_exit(void)
{
	HDMI_DRV_FUNC();
}

late_initcall(mtk_hdmitx_init);
module_exit(mtk_hdmitx_exit);
MODULE_AUTHOR("Junzhi, Zhao <junzhi.zhao@mediatek.com>");
MODULE_DESCRIPTION("mtk hdmi Driver");
MODULE_LICENSE("GPL");

#endif
