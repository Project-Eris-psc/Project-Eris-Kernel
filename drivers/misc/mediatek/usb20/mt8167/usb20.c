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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
/*#include <linux/usb/nop-usb-xceiv.h>*/
#include <linux/switch.h>
#include <linux/i2c.h>
#ifndef CONFIG_OF
#include <mach/irqs.h>
#endif
/*#include <mach/eint.h>*/
#include "musb_core.h"
#include "mtk_musb.h"
#include <linux/usb/usb_phy_generic.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include "musbhsdma.h"
#include <mt-plat/upmu_common.h>
/*#include <mach/mt_pm_ldo.h>*/
/*#include <mach/mt_clkmgr.h>*/
/*#include <mach/emi_mpu.h>*/
#ifndef FPGA_PLATFORM
#include <linux/clk.h>
struct clk *usbpll_clk;
struct clk *usbmcu_clk;
struct clk *usb_clk;
struct clk *icusb_clk;
#endif
#include "usb20.h"
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#ifndef CONFIG_MTK_LEGACY
#include <linux/regulator/consumer.h>
#endif
#endif
/*#include <mach/mt_boot_common.h>*/
#include <mt-plat/charging.h>
#include <linux/mfd/mt6397/rtc_misc.h>

/* default value 0 */
static int usb_rdy;
#ifdef CONFIG_MTK_UART_USB_SWITCH
struct regmap *mt_regmap;
#endif

#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
static bool usb_shutdown;
#endif

void set_usb_rdy(void)
{
	DBG(0, "set usb_rdy, wake up bat\n");
	usb_rdy = 1;
#ifdef CONFIG_MTK_SMART_BATTERY
	wake_up_bat();
#endif
}
kal_bool is_usb_rdy(void)
{
	if (usb_rdy)
		return KAL_TRUE;
	else
		return KAL_FALSE;
}

/*static bool platform_init_first = true;*/
static u32 cable_mode = CABLE_MODE_NORMAL;
#if !defined(CONFIG_MTK_LEGACY) && !defined(FPGA_PLATFORM)
static struct regulator *reg;
#define	VUSB_VOL_MIN 3300000
#define	VUSB_VOL_MAX 3300000
#endif
/*extern CHARGER_TYPE mt_get_charger_type(void);*/
/*add for linux kernel 3.10*/
#ifdef CONFIG_OF
u32 usb_irq_number1;
unsigned long usb_phy_base;
#endif

#ifdef CONFIG_MTK_UART_USB_SWITCH
u32 port_mode = PORT_MODE_USB;
u32 sw_tx;
u32 sw_rx;
u32 sw_uart_path;
#endif

/*EP Fifo Config*/
static struct musb_fifo_cfg fifo_cfg[] __initdata = {
#ifdef CONFIG_MTK_TC1_FEATURE
{ .hw_ep_num =  1, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  1, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  2, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  2, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  3, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  3, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  4, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  4, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  5, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	5, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  6, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	6, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	7, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	7, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	8, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_ISO, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	8, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_ISO, .mode = MUSB_BUF_SINGLE},
#else
{ .hw_ep_num =  1, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  1, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  2, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  2, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  3, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  3, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  4, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  4, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =  5, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	5, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =  6, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	6, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_INT, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	7, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	7, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_BULK, .mode = MUSB_BUF_SINGLE},
{ .hw_ep_num =	8, .style = MUSB_FIFO_TX,   .maxpacket = 512, .ep_mode = EP_ISO, .mode = MUSB_BUF_DOUBLE},
{ .hw_ep_num =	8, .style = MUSB_FIFO_RX,   .maxpacket = 512, .ep_mode = EP_ISO, .mode = MUSB_BUF_DOUBLE},
#endif
};


/*=======================================================================*/
/* MT6589 USB GADGET                                                     */
/*=======================================================================*/
#ifdef CONFIG_OF
/*static u64 usb_dmamask = DMA_BIT_MASK(32);*/

static const struct of_device_id apusb_of_ids[] = {
	{ .compatible = "mediatek,mt8167-usb20", },
	{},
};

static struct platform_device mt_usb_device = {
	.name	  = "mt_usb",
	.id		  = -1,
};

#if defined(CONFIG_USB_MTK_ACM_TEMP)
static struct platform_device usbacm_temp_device = {
	.name	  = "USB_ACM_Temp_Driver",
	.id		  = -1,
};
#endif

#endif
MODULE_DEVICE_TABLE(of, apusb_of_ids);


static struct timer_list musb_idle_timer;

static void musb_do_idle(unsigned long _musb)
{
	struct musb	*musb = (void *)_musb;
	unsigned long	flags;
	u8	devctl;

#if !defined(CONFIG_POWER_EXT)
	if (musb->is_active) {
		DBG(0, "%s active, igonre do_idle\n",
			otg_state_string(musb->xceiv->otg->state));
		return;
	}
#endif

	spin_lock_irqsave(&musb->lock, flags);

	switch (musb->xceiv->otg->state) {
	case OTG_STATE_B_PERIPHERAL:
	case OTG_STATE_A_WAIT_BCON:
		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl & MUSB_DEVCTL_BDEVICE) {
			musb->xceiv->otg->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(musb);
		} else {
			musb->xceiv->otg->state = OTG_STATE_A_IDLE;
			MUSB_HST_MODE(musb);
		}
		break;
	case OTG_STATE_A_HOST:
		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		if (devctl &  MUSB_DEVCTL_BDEVICE)
			musb->xceiv->otg->state = OTG_STATE_B_IDLE;
		else
			musb->xceiv->otg->state = OTG_STATE_A_WAIT_BCON;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&musb->lock, flags);

	DBG(0, "otg_state %s\n", otg_state_string(musb->xceiv->otg->state));
}

static void mt_usb_try_idle(struct musb *musb, unsigned long timeout)
{
	unsigned long		default_timeout = jiffies + msecs_to_jiffies(3);
	static unsigned long	last_timer;

	if (timeout == 0)
		timeout = default_timeout;

	/* Never idle if active, or when VBUS timeout is not set as host */
	if (musb->is_active || ((musb->a_wait_bcon == 0)
			&& (musb->state == OTG_STATE_A_WAIT_BCON))) {
		DBG(2, "%s active, deleting timer\n", otg_state_string(musb->state));
		del_timer(&musb_idle_timer);
		last_timer = jiffies;
		return;
	}

	if (time_after(last_timer, timeout)) {
		if (!timer_pending(&musb_idle_timer))
			last_timer = timeout;
		else {
			DBG(2, "Longer idle timer already pending, ignoring\n");
			return;
		}
	}
	last_timer = timeout;

	DBG(2, "%s inactive, for idle timer for %lu ms\n",
		otg_state_string(musb->state),
		(unsigned long)jiffies_to_msecs(timeout - jiffies));

	mod_timer(&musb_idle_timer, timeout);
}

static void mt_usb_enable(struct musb *musb)
{
	unsigned long   flags;

	DBG(0, "%d, %d\n", mtk_usb_power, musb->power);

	if (musb->power == true)
		return;

#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	mt_usb_clock_prepare();
#endif

	flags = musb_readl(musb->mregs, USB_L1INTM);

	/* mask ID pin, so "open clock" and "set flag" won't be interrupted. ISR may call clock_disable.*/
	musb_writel(mtk_musb->mregs, USB_L1INTM, (~IDDIG_INT_STATUS) & flags);

	/* Mark by ALPS01262215
		*	if (platform_init_first) {
		*	DBG(0,"usb init first\n\r");
		*	musb->is_host = true;
		*	}
		*/

	mdelay(10);

	usb_phy_recover();

	/* update musb->power & mtk_usb_power in the same time */
	musb->power = true;
	mtk_usb_power = true;

	musb_writel(mtk_musb->mregs, USB_L1INTM, flags);
}

static void mt_usb_disable(struct musb *musb)
{
	DBG(0, "%s, %d, %d\n", __func__, mtk_usb_power, musb->power);

	if (musb->power == false)
		return;

	/* Mark by ALPS01262215
		*	if (platform_init_first) {
		*	DBG(0,"usb init first\n\r");
		*	musb->is_host = false;
		*	platform_init_first = false;
		*}
		*/

	usb_phy_savecurrent();
	#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	mt_usb_clock_unprepare();
	#endif

	/* update musb->power & mtk_usb_power in the same time */
	mtk_usb_power = false;
	musb->power = false;
}

/* ================================ */
/* connect and disconnect functions */
/* ================================ */
bool mt_usb_is_device(void)
{
	DBG(4, "called\n");

	if (!mtk_musb) {
		DBG(0, "mtk_musb is NULL\n");
		return false; /* don't do charger detection when usb is not ready*/
	}
	DBG(4, "is_host=%d\n", mtk_musb->is_host);


#ifdef MTK_KERNEL_POWER_OFF_CHARGING
	if (g_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
		/* prevent MHL chip initial and cable related issue.*/
		/* power off charging mode. No need OTG function*/
		return true;
	}
#endif

	return !mtk_musb->is_host;
}

#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
void mt_usb_clock_prepare(void)
{
	int retval = 0;

	DBG(0, "usb_power = %d\n", mtk_usb_power);
	retval = clk_prepare(usbpll_clk);
	if (retval)
		DBG(0, KERN_WARNING "prepare fail\n");
	retval = clk_prepare(usb_clk);
	if (retval)
		DBG(0, KERN_WARNING "prepare fail\n");

	retval = clk_prepare(usbmcu_clk);
	if (retval)
		DBG(0, KERN_WARNING "prepare fail\n");
}

void mt_usb_clock_unprepare(void)
{
	DBG(0, "usb_power = %d\n", mtk_usb_power);

	clk_unprepare(usbmcu_clk);
	clk_unprepare(usb_clk);
	clk_unprepare(usbpll_clk);
}

#endif

#define CONN_WORK_DELAY 50
static struct delayed_work connection_work;
void do_connection_work(struct work_struct *data)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);
	unsigned long flags = 0;
	bool usb_in = false;

	if (mtk_musb) {
		if (__ratelimit(&ratelimit))
			DBG(0, "is ready %d is_host %d power %d\n", mtk_musb->is_ready, mtk_musb->is_host,
					mtk_musb->power);
	} else {
		/* re issue delay work */
		if (__ratelimit(&ratelimit))
			DBG(0, "issue work after %d ms\n", CONN_WORK_DELAY);
		queue_delayed_work(mtk_musb->st_wq, &connection_work, msecs_to_jiffies(CONN_WORK_DELAY));
		return;
	}

	if (!mtk_musb->is_ready) {
		/* re issue work */
		if (__ratelimit(&ratelimit))
			DBG(0, "issue work after %d ms\n", CONN_WORK_DELAY);
		queue_delayed_work(mtk_musb->st_wq, &connection_work, msecs_to_jiffies(CONN_WORK_DELAY));
		return;
	}

	/* be aware this could not be used in non-sleep context */
	usb_in = usb_cable_connected();

	spin_lock_irqsave(&mtk_musb->lock, flags);

	if (mtk_musb->is_host) {
		DBG(0, "is host, return\n");
		spin_unlock_irqrestore(&mtk_musb->lock, flags);
		return;
	}

#ifdef CONFIG_MTK_UART_USB_SWITCH
	mt_usb_clock_prepare();
	if (usb_phy_check_in_uart_mode()) {
		DBG(0, "in uart mode, return\n");
		spin_unlock_irqrestore(&mtk_musb->lock, flags);
		return;
	}
#endif

	if (!mtk_musb->power && (usb_in == true)) {
		/* enable usb */
		if (!wake_lock_active(&mtk_musb->usb_lock)) {
			wake_lock(&mtk_musb->usb_lock);
			DBG(0, "lock\n");
		} else {
			DBG(0, "already lock\n");
		}
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		spin_unlock_irqrestore(&mtk_musb->lock, flags);
		#endif
		/* note this already put SOFTCON */
		musb_start(mtk_musb);

	}
	else if (mtk_musb->power && (usb_in == false)) {
		/* disable usb */
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		spin_unlock_irqrestore(&mtk_musb->lock, flags);
		#endif
		musb_stop(mtk_musb);
		if (wake_lock_active(&mtk_musb->usb_lock)) {
			DBG(0, "unlock\n");
			wake_unlock(&mtk_musb->usb_lock);
		} else {
			DBG(0, "lock not active\n");
		}
	} else {
		DBG(0, "do nothing, usb_in:%d, power:%d\n",
				usb_in, mtk_musb->power);
		#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
		spin_unlock_irqrestore(&mtk_musb->lock, flags);
		#endif
	}
	#ifndef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	spin_unlock_irqrestore(&mtk_musb->lock, flags);
	#endif

}

void mt_usb_connect(void)
{
#ifndef CONFIG_MTK_MUSB_SW_WITCH_MODE
	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}
	/* issue connection work */
	DBG(0, "issue work\n");

	queue_delayed_work(mtk_musb->st_wq, &connection_work, 0);
	DBG(0, "[MUSB] USB connect\n");
#endif
}

void mt_usb_disconnect(void)
{
#ifndef CONFIG_MTK_MUSB_SW_WITCH_MODE
	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}
	/* issue connection work */
	DBG(0, "issue work\n");
	queue_delayed_work(mtk_musb->st_wq, &connection_work, 0);
	DBG(0, "[MUSB] USB disconnect\n");
#endif
}

static void do_connect_rescue_work(struct work_struct *work)
{
	DBG(0, "do_connect_rescue_work, issue connection work\n");
	queue_delayed_work(mtk_musb->st_wq, &connection_work, 0);
}

/* build time force on */
#if defined(CONFIG_FPGA_EARLY_PORTING) || defined(U3_COMPLIANCE) || defined(FOR_BRING_UP)
#define BYPASS_PMIC_LINKAGE
#endif

/* to avoid build error due to PMIC module not ready */
#if !defined(CONFIG_MFD_MT6397) && !defined(CONFIG_MTK_SMART_BATTERY) && !defined(CONFIG_USB_VBUSDET_IN_GPIO)
#define BYPASS_PMIC_LINKAGE
#endif

#ifndef CONFIG_POWER_EXT
#ifdef CONFIG_USB_PHYCHK_EXTCONN
static CHARGER_TYPE musb_hal_get_charger_type(void)
{
	int extconntype;
#ifdef BYPASS_PMIC_LINKAGE
	DBG(0, "force on");
	extconntype = MT_USB_EXTCONN_STANDARDHOST;
#else
	extconntype = usb_extconntype;
#endif

	return extconntype;
}
#else
static CHARGER_TYPE musb_hal_get_charger_type(void)
{
	CHARGER_TYPE chg_type;
#ifdef BYPASS_PMIC_LINKAGE
	DBG(0, "force on");
	chg_type = STANDARD_HOST;
#else
	chg_type = mt_get_charger_type();
#endif

	return chg_type;
}
#endif
#endif

static bool musb_hal_is_vbus_exist(void)
{
	bool vbus_exist;

#ifdef BYPASS_PMIC_LINKAGE
	DBG(0, "force on");
	vbus_exist = true;
#else
#if 1
#ifdef CONFIG_POWER_EXT
#ifdef CONFIG_USB_PHYCHK_EXTCONN
	vbus_exist = true;
#endif
#ifdef CONFIG_USB_VBUSDET_IN_GPIO
	vbus_exist = gpio_get_vbus_status();
#else
	vbus_exist = upmu_get_rgs_chrdet();
#endif
#else
	vbus_exist = upmu_is_chr_det();
#endif
#else
	vbus_exist = true;
	DBG(0, "skip PMIC API first, force on");
#endif
#endif
	DBG(0, "vbus_exist:%d\n", vbus_exist);
	return vbus_exist;

}

#ifdef CONFIG_MTK_MUSB_SW_WITCH_MODE
static void mt_usb20_sw_connect(void)
{
	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}
	/* issue connection work */
	DBG(0, "issue work\n");
	queue_delayed_work(mtk_musb->st_wq, &connection_work, 0);
	DBG(0, "[MUSB] USB connect\n");
}

static void mt_usb20_sw_disconnect(void)
{
	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}
	/* issue connection work */
	DBG(0, "issue work\n");
	queue_delayed_work(mtk_musb->st_wq, &connection_work, 0);
	DBG(0, "[MUSB] USB disconnect\n");

}
#endif

DEFINE_MUTEX(cable_connected_lock);
/* be aware this could not be used in non-sleep context */
bool usb_cable_connected(void)
{
#ifndef CONFIG_POWER_EXT
#ifdef CONFIG_USB_PHYCHK_EXTCONN
	int extconntype = MT_USB_EXTCONN_UNKNOWN;
#else
	CHARGER_TYPE chg_type = CHARGER_UNKNOWN;
#endif
#endif
	bool connected = false, vbus_exist = false;
	int delay_time;

	mutex_lock(&cable_connected_lock);
	/* FORCE USB ON case */
	if (musb_force_on) {
		delay_time = 0;    /* directly issue connection */
#ifndef CONFIG_POWER_EXT
#ifdef CONFIG_USB_PHYCHK_EXTCONN
		extconntype = MT_USB_EXTCONN_STANDARDHOST;
#else
		chg_type = STANDARD_HOST;
#endif
#endif
		vbus_exist = true;
		connected = true;
		DBG(0, "%s type force to STANDARD_HOST\n", __func__);
	} else {
		/* TYPE CHECK*/
		delay_time = 2000; /* issue connection one time in case, BAT THREAD didn't come*/
#ifndef CONFIG_POWER_EXT
#ifdef CONFIG_USB_PHYCHK_EXTCONN
		extconntype = musb_hal_get_charger_type();
		if (extconntype == MT_USB_EXTCONN_STANDARDHOST)
			connected = true;
#else
		chg_type = musb_hal_get_charger_type();
		if (musb_fake_CDP && chg_type == STANDARD_HOST) {
			DBG(0, "%s, fake to type 2\n", __func__);
			chg_type = CHARGING_HOST;
		}

		if (chg_type == STANDARD_HOST || chg_type == CHARGING_HOST)
			connected = true;
#endif
#endif
		/* VBUS CHECK to avoid type miss-judge */
		vbus_exist = musb_hal_is_vbus_exist();

		#ifndef CONFIG_POWER_EXT
		DBG(0, "%s vbus_exist=%d type=%d\n", __func__, vbus_exist, chg_type);
		#else
		DBG(0, "%s vbus_exist=%d\n", __func__, vbus_exist);
		#endif
		if (!vbus_exist)
			connected = false;
		else
			connected = true;
	}

	/* CMODE CHECK */
	#ifndef CONFIG_POWER_EXT
	if (cable_mode == CABLE_MODE_CHRG_ONLY || (cable_mode == CABLE_MODE_HOST_ONLY && chg_type != CHARGING_HOST))
		connected = false;
	#endif
	/* one time job, set_usb_rdy, issue connect_rescue_work */
	if (is_usb_rdy() == KAL_FALSE && mtk_musb->is_ready) {
		static struct delayed_work connect_rescue_work;

		set_usb_rdy();

		INIT_DELAYED_WORK(&connect_rescue_work, do_connect_rescue_work);
		DBG(0, "issue connect_rescue_work on is_ready begin, delay_time:%d ms\n", delay_time);
		schedule_delayed_work(&connect_rescue_work, msecs_to_jiffies(delay_time));
		DBG(0, "issue connect_rescue_work on is_ready end, delay_time:%d ms\n", delay_time);
	}

	mutex_unlock(&cable_connected_lock);
	DBG(0, "%s, connected:%d, cable_mode:%d\n", __func__, connected, cable_mode);
	return connected;
}

static void usb_reconnect(void)
{
	DBG(0, "issue work\n");
	queue_delayed_work(mtk_musb->st_wq, &connection_work, 0);
	DBG(0, "%s end\n", __func__);
}

void musb_platform_reset(struct musb *musb)
{
	u16 swrst = 0;
	void __iomem *mbase = musb->mregs;

	swrst = musb_readw(mbase, MUSB_SWRST);
	swrst |= (MUSB_SWRST_DISUSBRESET | MUSB_SWRST_SWRST);
	musb_writew(mbase, MUSB_SWRST, swrst);
}

bool is_switch_charger(void)
{
#ifdef SWITCH_CHARGER
	return true;
#else
	return false;
#endif
}

void pmic_chrdet_int_en(int is_on)
{
#ifndef FPGA_PLATFORM
	/*upmu_interrupt_chrdet_int_en(is_on);*/
#endif
}

void musb_sync_with_bat(struct musb *musb, int usb_state)
{
#ifndef FPGA_PLATFORM
	DBG(0, "BATTERY_SetUSBState, state=%d\n", usb_state);
#ifdef CONFIG_MTK_SMART_BATTERY
	BATTERY_SetUSBState(usb_state);
	wake_up_bat();
#endif
#endif
}

/*-------------------------------------------------------------------------*/
static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long	flags;
	irqreturn_t	retval = IRQ_NONE;
	struct musb	*musb = __hci;

	spin_lock_irqsave(&musb->lock, flags);

	/* musb_read_clear_generic_interrupt */
	musb->int_usb =
		musb_readb(musb->mregs, MUSB_INTRUSB) & musb_readb(musb->mregs, MUSB_INTRUSBE);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX) & musb_readw(musb->mregs, MUSB_INTRTXE);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX) & musb_readw(musb->mregs, MUSB_INTRRXE);
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	musb->int_queue = musb_readl(musb->mregs, MUSB_QISAR);
#endif
	mb(); /* */
	musb_writew(musb->mregs, MUSB_INTRRX, musb->int_rx);
	musb_writew(musb->mregs, MUSB_INTRTX, musb->int_tx);
	musb_writeb(musb->mregs, MUSB_INTRUSB, musb->int_usb);
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	if (musb->int_queue) {
		musb_writel(musb->mregs, MUSB_QISAR, musb->int_queue);
		musb->int_queue &= ~(musb_readl(musb->mregs, MUSB_QIMR));
	}
#endif
	/* musb_read_clear_generic_interrupt */

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	if (musb->int_usb || musb->int_tx || musb->int_rx || musb->int_queue)
		retval = musb_interrupt(musb);
#else
	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval = musb_interrupt(musb);
#endif

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}

static irqreturn_t mt_usb_interrupt(int irq, void *dev_id)
{
	irqreturn_t tmp_status;
	irqreturn_t status = IRQ_NONE;
	struct musb	*musb = (struct musb *)dev_id;
	u32 usb_l1_ints;

#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	if (usb_shutdown) {
		pr_err("%s, already shut down\n", __func__);
		return IRQ_HANDLED;
	}
#endif

	usb_l1_ints = musb_readl(musb->mregs, USB_L1INTS) & musb_readl(mtk_musb->mregs, USB_L1INTM); /*gang  REVISIT*/
	DBG(1, "usb interrupt assert %x %x  %x %x %x\n", usb_l1_ints,
	musb_readl(mtk_musb->mregs, USB_L1INTM), musb_readb(musb->mregs, MUSB_INTRUSBE),
	musb_readw(musb->mregs, MUSB_INTRTX), musb_readw(musb->mregs, MUSB_INTRTXE));

	if ((usb_l1_ints & TX_INT_STATUS) || (usb_l1_ints & RX_INT_STATUS)
			|| (usb_l1_ints & USBCOM_INT_STATUS)
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
			|| (usb_l1_ints & QINT_STATUS)
#endif
	) {
		tmp_status = generic_interrupt(irq, musb);
		if (tmp_status != IRQ_NONE)
			status = tmp_status;
	}

	if (usb_l1_ints & DMA_INT_STATUS) {
		tmp_status = dma_controller_irq(irq, musb->dma_controller);
		if (tmp_status != IRQ_NONE)
			status = tmp_status;
	}

#ifdef CONFIG_USB_MTK_OTG
	if (usb_l1_ints & IDDIG_INT_STATUS) {
		mt_usb_iddig_int(musb);
		status = IRQ_HANDLED;
	}
#endif

	return status;
}

/*--FOR INSTANT POWER ON USAGE--------------------------------------------------*/
static ssize_t mt_usb_show_cmode(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		DBG(0, "dev is null!!\n");
		return 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", cable_mode);
}

static ssize_t mt_usb_store_cmode(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int cmode;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return count;
	/* } else if (1 == sscanf(buf, "%d", &cmode)) { */
	} else if (kstrtol(buf, 10, (long *)&cmode) == 0) {
		DBG(0, "cmode=%d, cable_mode=%d\n", cmode, cable_mode);
		if (cmode >= CABLE_MODE_MAX)
			cmode = CABLE_MODE_NORMAL;

		if (cable_mode != cmode) {
			if (mtk_musb) {
				if (down_interruptible(&mtk_musb->musb_lock))
					DBG(0, "USB20: %s: busy, Couldn't get power_clock_lock\n",
					    __func__);
			}
			if (cmode == CABLE_MODE_CHRG_ONLY) { /* IPO shutdown, disable USB*/
				if (mtk_musb)
					mtk_musb->in_ipo_off = true;

			} else {	/* IPO bootup, enable USB */
				if (mtk_musb)
					mtk_musb->in_ipo_off = false;
			}

			cable_mode = cmode;
			usb_reconnect();
			mdelay(10);

#ifdef CONFIG_USB_MTK_OTG
			if (cmode == CABLE_MODE_CHRG_ONLY) {
				if (mtk_musb && mtk_musb->is_host) { /* shut down USB host for IPO*/
					if (wake_lock_active(&mtk_musb->usb_lock))
						wake_unlock(&mtk_musb->usb_lock);
					musb_platform_set_vbus(mtk_musb, 0);
					msleep(50); /*add sleep time to ensure vbus off and disconnect irq processed.*/
					musb_stop(mtk_musb);
					MUSB_DEV_MODE(mtk_musb);
					/* Think about IPO shutdown with A-cable, then switch to B-cable and IPO bootup.
					*	We need a point to clear session bit
					*/
					musb_writeb(mtk_musb->mregs, MUSB_DEVCTL,
											(~MUSB_DEVCTL_SESSION) &
					musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
				}
				/* mask ID pin interrupt even if A-cable is not plugged in*/
				switch_int_to_host_and_mask(mtk_musb);
			} else {
				switch_int_to_host(mtk_musb); /* resotre ID pin interrupt*/
			}
#endif
			if (mtk_musb)
				up(&mtk_musb->musb_lock);
		}
	}
	return count;
}

DEVICE_ATTR(cmode,  0664, mt_usb_show_cmode, mt_usb_store_cmode);

static bool saving_mode;

static ssize_t mt_usb_show_saving_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		DBG(0, "dev is null!!\n");
		return 0;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", saving_mode);
}

static ssize_t mt_usb_store_saving_mode(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int saving;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return count;
	/* } else if (1 == sscanf(buf, "%d", &saving)) { */
	} else if (kstrtol(buf, 10, (long *)&saving) == 0) {
		DBG(0, "old=%d new=%d\n", saving, saving_mode);
		if (saving_mode == (!saving))
			saving_mode = !saving_mode;
	}
	return count;
}

bool is_saving_mode(void)
{
	DBG(0, "%d\n", saving_mode);
	return saving_mode;
}

DEVICE_ATTR(saving, 0664, mt_usb_show_saving_mode, mt_usb_store_saving_mode);

#ifdef CONFIG_MTK_UART_USB_SWITCH
static void uart_usb_switch_dump_register(void)
{
	usb_enable_clock(true);
#ifdef FPGA_PLATFORM
	DBG(0, "[MUSB]addr: 0x6B, value: %x\n", USB_PHY_Read_Register8(0x6B));
	DBG(0, "[MUSB]addr: 0x6E, value: %x\n", USB_PHY_Read_Register8(0x6E));
	DBG(0, "[MUSB]addr: 0x22, value: %x\n", USB_PHY_Read_Register8(0x22));
	DBG(0, "[MUSB]addr: 0x68, value: %x\n", USB_PHY_Read_Register8(0x68));
	DBG(0, "[MUSB]addr: 0x6A, value: %x\n", USB_PHY_Read_Register8(0x6A));
	DBG(0, "[MUSB]addr: 0x1A, value: %x\n", USB_PHY_Read_Register8(0x1A));
#else
	DBG(0, "[MUSB]addr: 0x6B, value: %x\n", USBPHY_READ8(0x6B));
	DBG(0, "[MUSB]addr: 0x6E, value: %x\n", USBPHY_READ8(0x6E));
	DBG(0, "[MUSB]addr: 0x22, value: %x\n", USBPHY_READ8(0x22));
	DBG(0, "[MUSB]addr: 0x68, value: %x\n", USBPHY_READ8(0x68));
	DBG(0, "[MUSB]addr: 0x6A, value: %x\n", USBPHY_READ8(0x6A));
	DBG(0, "[MUSB]addr: 0x1A, value: %x\n", USBPHY_READ8(0x1A));
#endif
	usb_enable_clock(false);
}

static ssize_t mt_usb_show_portmode(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		DBG(0, "dev is null!!\n");
		return 0;
	}

	if (usb_phy_check_in_uart_mode())
		port_mode = PORT_MODE_UART;
	else
		port_mode = PORT_MODE_USB;

	if (port_mode == PORT_MODE_USB)
		DBG(0, "\nUSB Port mode -> USB\n");
	else if (port_mode == PORT_MODE_UART)
		DBG(0, "\nUSB Port mode -> UART\n");
	uart_usb_switch_dump_register();

	return scnprintf(buf, PAGE_SIZE, "%d\n", port_mode);
}

static ssize_t mt_usb_store_portmode(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	long int portmode;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return count;
	/* } else if (1 == sscanf(buf, "%d", &portmode)) { */
	} else if (kstrtol(buf, 10, (long int *)&portmode) == 0) {
		DBG(0, "\nUSB Port mode: current => %d (port_mode), change to => %ld (portmode)\n",
				port_mode, portmode);
		if (portmode >= PORT_MODE_MAX)
			portmode = PORT_MODE_USB;

		if (port_mode != portmode) {
			if (portmode == PORT_MODE_USB) { /* Changing to USB Mode*/
				DBG(0, "USB Port mode -> USB\n");
				usb_phy_switch_to_usb();
			} else if (portmode == PORT_MODE_UART) { /* Changing to UART Mode*/
				DBG(0, "USB Port mode -> UART\n");
				usb_phy_switch_to_uart();
			}
			uart_usb_switch_dump_register();
			port_mode = portmode;
		}
	}
	return count;
}

DEVICE_ATTR(portmode,  0664, mt_usb_show_portmode, mt_usb_store_portmode);


static ssize_t mt_usb_show_tx(struct device *dev, struct device_attribute *attr, char *buf)
{
	UINT8 var;
	UINT8 var2;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return 0;
	}

#ifdef FPGA_PLATFORM
	var = USB_PHY_Read_Register8(0x6E);
#else
	var = USBPHY_READ8(0x6E);
#endif
	var2 = (var >> 3) & ~0xFE;
	DBG(0, "[MUSB]addr: 0x6E (TX), value: %x - %x\n", var, var2);

	sw_tx = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var2);
}

static ssize_t mt_usb_store_tx(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	long int val;
	UINT8 var;
	UINT8 var2;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return count;
	/* } else if (1 == sscanf(buf, "%d", &val)) { */
	} else if (kstrtol(buf, 10, (long int *)&val) == 0) {
		DBG(0, "\n Write TX : %ld\n", val);

#ifdef FPGA_PLATFORM
		var = USB_PHY_Read_Register8(0x6E);
#else
		var = USBPHY_READ8(0x6E);
#endif

		if (val == 0)
			var2 = var & ~(1 << 3);
		else
			var2 = var | (1 << 3);

#ifdef FPGA_PLATFORM
		USB_PHY_Write_Register8(var2, 0x6E);
		var = USB_PHY_Read_Register8(0x6E);
#else
		USBPHY_WRITE8(0x6E, var2);
		var = USBPHY_READ8(0x6E);
#endif

		var2 = (var >> 3) & ~0xFE;

		DBG(0, "[MUSB]addr: 0x6E TX [AFTER WRITE], value after: %x - %x\n", var, var2);
		sw_tx = var;
	}
	return count;
}
DEVICE_ATTR(tx,  0664, mt_usb_show_tx, mt_usb_store_tx);

static ssize_t mt_usb_show_rx(struct device *dev, struct device_attribute *attr, char *buf)
{
	UINT8 var;
	UINT8 var2;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return 0;
	}

#ifdef FPGA_PLATFORM
	var = USB_PHY_Read_Register8(0x77);
#else
	var = USBPHY_READ8(0x77);
#endif
	var2 = (var >> 7) & ~0xFE;
	DBG(0, "[MUSB]addr: 0x77 (RX), value: %x - %x\n", var, var2);
	sw_rx = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var2);
}

DEVICE_ATTR(rx,  0444, mt_usb_show_rx, NULL);

static ssize_t mt_usb_show_uart_path(struct device *dev, struct device_attribute *attr, char *buf)
{
	UINT8 var;

	if (!dev) {
		DBG(0, "dev is null!!\n");
		return 0;
	}

	/*var = DRV_Reg8(UART2_BASE + 0xB0);*/
	var = 1;
	sw_uart_path = var;

	return scnprintf(buf, PAGE_SIZE, "%x\n", var);
}

DEVICE_ATTR(uartpath,  0444, mt_usb_show_uart_path, NULL);
#endif

#ifdef CONFIG_MTK_MUSB_SW_WITCH_MODE
static bool device_mode;
static bool host_mode;

static ssize_t mt_usb_store_swmode(struct device *dev, struct device_attribute *attr,
						   const char *buf, size_t count)
{
	if (count >= 6 && !strncmp(buf, "device", 6)) {
		if (!device_mode && !host_mode) {
			DBG(0, "switch to device mode\n");
			mt_usb20_sw_connect();
			device_mode = true;
		}
	}

	if (count >= 4 && !strncmp(buf, "host", 4)) {
		if (!device_mode && !host_mode) {
			DBG(0, "switch to host mode\n");
			musb_id_pin_sw_work(true);
			host_mode = true;
		}
	}

	if (count >= 4 && !strncmp(buf, "idle", 4)) {
		if (device_mode) {
			DBG(0, "switch to dev->idle mode\n");
			mt_usb20_sw_disconnect();
			device_mode = false;
		} else if (host_mode) {
			DBG(0, "switch to host->idle mode\n");
			musb_id_pin_sw_work(false);
			host_mode = false;
		} else
			DBG(0, "do nothing\n");
	}

	return count;
}

DEVICE_ATTR(swmode, 0220, NULL, mt_usb_store_swmode);
#endif

#ifdef FPGA_PLATFORM
static struct i2c_client *usb_i2c_client;
static const struct i2c_device_id usb_i2c_id[] = { {"mtk-usb", 0}, {} };

void USB_PHY_Write_Register8(UINT8 var,  UINT8 addr)
{
	char buffer[2];

	buffer[0] = addr;
	buffer[1] = var;
	i2c_master_send(usb_i2c_client, buffer, 2);
}

UINT8 USB_PHY_Read_Register8(UINT8 addr)
{
	UINT8 var;

	i2c_master_send(usb_i2c_client, &addr, 1);
	i2c_master_recv(usb_i2c_client, &var, 1);
	return var;
}

static int usb_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#ifdef CONFIG_OF
	unsigned long base;
	/* if i2c probe before musb prob, this would cause KE */
	/* base = (unsigned long)((unsigned long)mtk_musb->xceiv->io_priv); */
	base = usb_phy_base;
	DBG(0, "[MUSB]usb_i2c_probe, start, base:%lx\n", base);
#endif

	usb_i2c_client = client;

#ifdef CONFIG_OF
	/*disable usb mac suspend*/
	DRV_WriteReg8(base + 0x86a, 0x00);
#else
	DRV_WriteReg8(USB_SIF_BASE + 0x86a, 0x00);
#endif
	/*usb phy initial sequence*/

	USB_PHY_Write_Register8(0x00, 0xFF);
	DBG(0, "****************before bank 0x00*************************\n");
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x05, value: %x\n", USB_PHY_Read_Register8(0x05));
	DBG(0, "[MUSB]addr: 0x18, value: %x\n", USB_PHY_Read_Register8(0x18));
	DBG(0, "*****************after **********************************\n");
	USB_PHY_Write_Register8(0x55, 0x05);
	USB_PHY_Write_Register8(0x84, 0x18);

	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x05, value: %x\n", USB_PHY_Read_Register8(0x05));
	DBG(0, "[USB]addr: 0x18, value: %x\n", USB_PHY_Read_Register8(0x18));
	DBG(0, "****************before bank 0x10*************************\n");
	USB_PHY_Write_Register8(0x10, 0xFF);
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x0A, value: %x\n", USB_PHY_Read_Register8(0x0A));
	DBG(0, "*****************after **********************************\n");

	USB_PHY_Write_Register8(0x84, 0x0A);

	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x0A, value: %x\n", USB_PHY_Read_Register8(0x0A));
	DBG(0, "****************before bank 0x40*************************\n");
	USB_PHY_Write_Register8(0x40, 0xFF);
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x38, value: %x\n", USB_PHY_Read_Register8(0x38));
	DBG(0, "[MUSB]addr: 0x42, value: %x\n", USB_PHY_Read_Register8(0x42));
	DBG(0, "[MUSB]addr: 0x08, value: %x\n", USB_PHY_Read_Register8(0x08));
	DBG(0, "[MUSB]addr: 0x09, value: %x\n", USB_PHY_Read_Register8(0x09));
	DBG(0, "[MUSB]addr: 0x0C, value: %x\n", USB_PHY_Read_Register8(0x0C));
	DBG(0, "[MUSB]addr: 0x0E, value: %x\n", USB_PHY_Read_Register8(0x0E));
	DBG(0, "[MUSB]addr: 0x10, value: %x\n", USB_PHY_Read_Register8(0x10));
	DBG(0, "[MUSB]addr: 0x14, value: %x\n", USB_PHY_Read_Register8(0x14));
	DBG(0, "*****************after **********************************\n");

	USB_PHY_Write_Register8(0x46, 0x38);
	USB_PHY_Write_Register8(0x40, 0x42);
	USB_PHY_Write_Register8(0xAB, 0x08);
	USB_PHY_Write_Register8(0x0C, 0x09);
	USB_PHY_Write_Register8(0x71, 0x0C);
	USB_PHY_Write_Register8(0x4F, 0x0E);
	USB_PHY_Write_Register8(0xE1, 0x10);
	USB_PHY_Write_Register8(0x5F, 0x14);
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x38, value: %x\n", USB_PHY_Read_Register8(0x38));
	DBG(0, "[MUSB]addr: 0x42, value: %x\n", USB_PHY_Read_Register8(0x42));
	DBG(0, "[MUSB]addr: 0x08, value: %x\n", USB_PHY_Read_Register8(0x08));
	DBG(0, "[MUSB]addr: 0x09, value: %x\n", USB_PHY_Read_Register8(0x09));
	DBG(0, "[MUSB]addr: 0x0C, value: %x\n", USB_PHY_Read_Register8(0x0C));
	DBG(0, "[MUSB]addr: 0x0E, value: %x\n", USB_PHY_Read_Register8(0x0E));
	DBG(0, "[MUSB]addr: 0x10, value: %x\n", USB_PHY_Read_Register8(0x10));
	DBG(0, "[MUSB]addr: 0x14, value: %x\n", USB_PHY_Read_Register8(0x14));
	DBG(0, "****************before bank 0x60*************************\n");
	USB_PHY_Write_Register8(0x60, 0xFF);
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x10, value: %x\n", USB_PHY_Read_Register8(0x14));
	DBG(0, "*****************after **********************************\n");

	USB_PHY_Write_Register8(0x03, 0x14);
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x10, value: %x\n", USB_PHY_Read_Register8(0x14));
	DBG(0, "****************before bank 0x00*************************\n");
	USB_PHY_Write_Register8(0x00, 0xFF);
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x6A, value: %x\n", USB_PHY_Read_Register8(0x6A));
	DBG(0, "[MUSB]addr: 0x68, value: %x\n", USB_PHY_Read_Register8(0x68));
	DBG(0, "[MUSB]addr: 0x6C, value: %x\n", USB_PHY_Read_Register8(0x6C));
	DBG(0, "[MUSB]addr: 0x6D, value: %x\n", USB_PHY_Read_Register8(0x6D));
	USB_PHY_Write_Register8(0x04, 0x6A);
	USB_PHY_Write_Register8(0x08, 0x68);
	/*USB_PHY_Write_Register8(0x26, 0x6C);*/
	/*USB_PHY_Write_Register8(0x36, 0x6D);*/
	DBG(0, "*****************after **********************************\n");
	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	DBG(0, "[MUSB]addr: 0x6A, value: %x\n", USB_PHY_Read_Register8(0x6A));
	DBG(0, "[MUSB]addr: 0x68, value: %x\n", USB_PHY_Read_Register8(0x68));
	DBG(0, "[MUSB]addr: 0x6C, value: %x\n", USB_PHY_Read_Register8(0x6C));
	DBG(0, "[MUSB]addr: 0x6D, value: %x\n", USB_PHY_Read_Register8(0x6D));

	/*
	*	USB_PHY_Write_Register8(0x00, 0xFF);
	*	USB_PHY_Write_Register8(0x04, 0x61);
	*	USB_PHY_Write_Register8(0x00, 0x68);
	*	USB_PHY_Write_Register8(0x00, 0x6a);
	*	USB_PHY_Write_Register8(0x6e, 0x00);
	*	USB_PHY_Write_Register8(0x0c, 0x1b);
	*	USB_PHY_Write_Register8(0x44, 0x08);
	*	USB_PHY_Write_Register8(0x55, 0x11);
	*	USB_PHY_Write_Register8(0x68, 0x1a);
	*
	*
	*	DBG(0, "[MUSB]addr: 0xFF, value: %x\n", USB_PHY_Read_Register8(0xFF));
	*	DBG(0, "[MUSB]addr: 0x61, value: %x\n", USB_PHY_Read_Register8(0x61));
	*	DBG(0, "[MUSB]addr: 0x68, value: %x\n", USB_PHY_Read_Register8(0x68));
	*	DBG(0, "[MUSB]addr: 0x6a, value: %x\n", USB_PHY_Read_Register8(0x6a));
	*	DBG(0, "[MUSB]addr: 0x00, value: %x\n", USB_PHY_Read_Register8(0x00));
	*	DBG(0, "[MUSB]addr: 0x1b, value: %x\n", USB_PHY_Read_Register8(0x1b));
	*	DBG(0, "[MUSB]addr: 0x08, value: %x\n", USB_PHY_Read_Register8(0x08));
	*	DBG(0, "[MUSB]addr: 0x11, value: %x\n", USB_PHY_Read_Register8(0x11));
	*	DBG(0, "[MUSB]addr: 0x1a, value: %x\n", USB_PHY_Read_Register8(0x1a));
      */

	DBG(0, "[MUSB]usb_i2c_probe, end\n");
	return 0;

}

/*static int usb_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
	*	strcpy(info->type, "mtk-usb");
	*	return 0;
	*	}
*/

static int usb_i2c_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id usb_of_match[] = {
	{.compatible = "mediatek,mtk-usb"},
	{},
};
#endif

struct i2c_driver usb_i2c_driver = {
	.probe = usb_i2c_probe,
	.remove = usb_i2c_remove,
	/*.detect = usb_i2c_detect,*/
	.driver = {
		.name = "mtk-usb",
		#ifdef CONFIG_OF
		.of_match_table = usb_of_match,
		#endif
	},
	.id_table = usb_i2c_id,
};

static int add_usb_i2c_driver(void)
{
#if 0
	i2c_register_board_info(3, &usb_i2c_dev, 1);
#endif
	if (i2c_add_driver(&usb_i2c_driver) != 0) {
		DBG(0, "[MUSB]usb_i2c_driver initialization failed!!\n");
		return -1;
	}
	DBG(0, "[MUSB]usb_i2c_driver initialization succeed!!\n");
	return 0;
}
#endif /*End of FPGA_PLATFORM*/

static int mt_usb_init(struct musb *musb)
{
#if !defined(CONFIG_MTK_LEGACY) && !defined(FPGA_PLATFORM)
	int ret;
#endif

	DBG(0, "mt_usb_init\n");

	/*usb_nop_xceiv_register();*/
	usb_phy_generic_register();
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(musb->xceiv)) {
		DBG(0, "[MUSB] usb_get_phy error!!\n");
		return -EPROBE_DEFER;
	}


#ifdef CONFIG_OF
#ifdef CONFIG_USB_PHYCHK_EXTCONN
	musb->xceiv->io_priv = musb->ctrl_base;
#endif
	/*musb->nIrq = usb_irq_number1;*/
#else
	musb->nIrq = USB_MCU_IRQ_BIT1_ID;
#endif
	musb->dma_irq = (int)SHARE_IRQ;
	musb->fifo_cfg = fifo_cfg;
	musb->fifo_cfg_size = ARRAY_SIZE(fifo_cfg);
	musb->dyn_fifo = true;
	musb->power = false;
	musb->is_host = false;
	musb->fifo_size = 8*1024;

	wake_lock_init(&musb->usb_lock, WAKE_LOCK_SUSPEND, "USB suspend lock");

#ifndef FPGA_PLATFORM
#ifdef CONFIG_MTK_LEGACY
	hwPowerOn(MT6325_POWER_LDO_VUSB33, VOL_3300, "VUSB_LDO");
	DBG(0, "%s, enable VBUS_LDO\n", __func__);
#else
	reg = regulator_get(musb->controller, "vusb");
	if (!IS_ERR(reg)) {
		ret = regulator_set_voltage(reg, VUSB_VOL_MIN, VUSB_VOL_MAX);
		if (ret < 0)
			DBG(0, "regulator set vol failed: %d\n", ret);
		else
			DBG(0, "regulator set vol ok, <%d,%d>\n", VUSB_VOL_MIN, VUSB_VOL_MAX);
		ret = regulator_enable(reg);
		if (ret < 0) {
			DBG(0, "regulator_enable failed: %d\n", ret);
			regulator_put(reg);
		} else {
			DBG(0, "enable USB regulator\n");
		}
	} else {
		DBG(0, "regulator_get failed %d\n", IS_ERR(reg));
	}
#endif
#endif

	/*mt_usb_enable(musb);*/

	musb->isr = mt_usb_interrupt;
	musb_writel(musb->mregs, MUSB_HSDMA_INTR, 0xff | (0xff << DMA_INTR_UNMASK_SET_OFFSET));
	DBG(0, "musb platform init %x\n", musb_readl(musb->mregs, MUSB_HSDMA_INTR));

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	/* FIXME, workaround for device_qmu + host_dma */
	musb_writel(musb->mregs, USB_L1INTM,
							TX_INT_STATUS | RX_INT_STATUS | USBCOM_INT_STATUS |
							DMA_INT_STATUS | QINT_STATUS);
#else
	musb_writel(musb->mregs, USB_L1INTM,
							TX_INT_STATUS | RX_INT_STATUS | USBCOM_INT_STATUS |
							DMA_INT_STATUS);
#endif

	setup_timer(&musb_idle_timer, musb_do_idle, (unsigned long) musb);

	DBG(0, "%s, init_otg before\n", __func__);

#ifdef CONFIG_USB_MTK_OTG
	DBG(0, "%s, init_otg\n", __func__);
	mt_usb_otg_init(musb);
#endif

	return 0;
}

static int mt_usb_exit(struct musb *musb)
{
	del_timer_sync(&musb_idle_timer);
	return 0;
}

static const struct musb_platform_ops mt_usb_ops = {
	.init		= mt_usb_init,
	.exit		= mt_usb_exit,
	/*.set_mode	= mt_usb_set_mode,*/
	.try_idle	= mt_usb_try_idle,
	.enable		= mt_usb_enable,
	.disable	= mt_usb_disable,
	.set_vbus	= mt_usb_set_vbus,
	.vbus_status = mt_usb_get_vbus_status
};

static u64 mt_usb_dmamask = DMA_BIT_MASK(32);
static int mt_usb_probe(struct platform_device *pdev)
{

	struct musb_hdrc_platform_data	*pdata = pdev->dev.platform_data;
	struct platform_device		*musb;
	struct mt_usb_glue		*glue;
#ifdef CONFIG_OF
	struct musb_hdrc_config		*config;
	struct device_node		*np = pdev->dev.of_node;
	/*u32 temp_id = 0; */
	/*unsigned long usb_mac;*/
#endif
	int ret = -ENOMEM;
	/*int retval = 0;*/

	DBG(0, "[U2]usb20 mt_usb_probe\n");

	glue = kzalloc(sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		/*dev_err(&pdev->dev, "failed to allocate glue context\n");*/
		DBG(0, "[U2]failed to allocate glue context\n");
		goto err0;
	}

	musb = platform_device_alloc("musb-hdrc", PLATFORM_DEVID_AUTO);
	if (!musb) {
		dev_err(&pdev->dev, "failed to allocate musb device\n");
		DBG(0, "[U2]failed to allocate musb device\n");
		goto err1;
	}

#ifdef CONFIG_OF
	dts_np = pdev->dev.of_node;

	/*usb_irq_number1 = irq_of_parse_and_map(pdev->dev.of_node, 0);*/
	/*usb_mac = (unsigned long)of_iomap(pdev->dev.of_node, 0);*/
	usb_phy_base = (unsigned long)of_iomap(pdev->dev.of_node, 1);
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "failed to allocate musb platform data\n");
		DBG(0, "[U2]failed to allocate musb platform data\n");
		goto err2;
	}

	config = devm_kzalloc(&pdev->dev, sizeof(*config), GFP_KERNEL);
	if (!config) {
		/*dev_err(&pdev->dev, "failed to allocate musb hdrc config\n");*/
		DBG(0, "[U2]failed to allocate musb hdrc config\n");
		goto err2;
	}

#ifdef CONFIG_USB_MTK_OTG
	pdata->mode = MUSB_OTG;
#else
	of_property_read_u32(np, "mode", (u32 *)&pdata->mode);
#endif

	/*of_property_read_u32(np, "dma_channels",    (u32 *)&config->dma_channels);*/
	of_property_read_u32(np, "num_eps", (u32 *)&config->num_eps);
	config->multipoint = of_property_read_bool(np, "multipoint");
	/*config->dyn_fifo = of_property_read_bool(np, "dyn_fifo");*/
	/*config->soft_con = of_property_read_bool(np, "soft_con");*/
	/*config->dma = of_property_read_bool(np, "dma");*/

	pdata->config       = config;
#endif

	musb->dev.parent		= &pdev->dev;
	musb->dev.dma_mask		= &mt_usb_dmamask;
	musb->dev.coherent_dma_mask	= mt_usb_dmamask;
#ifdef CONFIG_OF
	pdev->dev.dma_mask = &mt_usb_dmamask;
	pdev->dev.coherent_dma_mask = mt_usb_dmamask;
	arch_setup_dma_ops(&musb->dev, 0, mt_usb_dmamask, NULL, 0);
#endif

	glue->dev			= &pdev->dev;
	glue->musb			= musb;

	pdata->platform_ops		= &mt_usb_ops;
	platform_set_drvdata(pdev, glue);

	ret = platform_device_add_resources(musb, pdev->resource, pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "failed to add resources\n");
		goto err2;
	}

	ret = platform_device_add_data(musb, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(&pdev->dev, "failed to add platform_data\n");
		goto err2;
	}
	DBG(0, "[U2]platform_device_add musb\n");
	ret = platform_device_add(musb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register musb device\n");
		DBG(0, "[U2]failed to register musb device\n");
		goto err2;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_cmode);
	ret = device_create_file(&pdev->dev, &dev_attr_saving);
#ifdef CONFIG_MTK_UART_USB_SWITCH
	ret = device_create_file(&pdev->dev, &dev_attr_portmode);
	ret = device_create_file(&pdev->dev, &dev_attr_tx);
	ret = device_create_file(&pdev->dev, &dev_attr_rx);
	ret = device_create_file(&pdev->dev, &dev_attr_uartpath);
#endif
#ifdef CONFIG_MTK_MUSB_SW_WITCH_MODE
	ret = device_create_file(&pdev->dev, &dev_attr_swmode);
#endif


	if (ret) {
		dev_err(&pdev->dev, "failed to create musb device\n");
		goto err2;
	}

#ifdef CONFIG_OF
	/*pr_info("usb probe irq: 0x%d usb phy address: 0x%lx usb mac : 0x%1x\n",
		*				usb_irq_number1, usb_phy_base, usb_mac);
	*/
	pr_info("USB probe done!\n");
#endif

	/* run time force on */
#if defined(FPGA_PLATFORM) || defined(FOR_BRING_UP)
	musb_force_on = 1;
#endif

	DBG(0, "[U2]success to register musb\n");
	return 0;

err2:
	platform_device_put(musb);

err1:
	kfree(glue);

err0:
	DBG(0, "[U2]failed to register musb\n");
	return ret;
}

static int mt_usb_dts_probe(struct platform_device *pdev)
{
	/*struct musb_hdrc_platform_data	*pdata = pdev->dev.platform_data;*/
	/*struct platform_device		*musb;*/
	/*struct mt_usb_glue		*glue;*/
#ifdef CONFIG_OF
	/*struct musb_hdrc_config		*config;*/
	/*struct device_node		*np = pdev->dev.of_node;*/
	/*u32 temp_id = 0;*/
	/*unsigned long usb_mac;*/
#endif
	/*int				ret = -ENOMEM;*/
	int retval = 0;
	#ifdef CONFIG_MTK_UART_USB_SWITCH
	struct device_node *np, *node_pctl;
	#endif


	DBG(0, "[U2]usb20 dts probe\n");

	DBG(0, "init connection_work\n");
	INIT_DELAYED_WORK(&connection_work, do_connection_work);
	DBG(0, "keep musb->power & mtk_usb_power in the samae value\n");
	mtk_usb_power = false;

#ifndef FPGA_PLATFORM
	usbpll_clk = devm_clk_get(&pdev->dev, "usbpll");
	if (IS_ERR(usbpll_clk)) {
		DBG(0, KERN_WARNING "cannot get usbpll clock\n");
		return PTR_ERR(usbpll_clk);
	}
	#ifndef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	DBG(0, KERN_WARNING "get usbpll clock ok, prepare it\n");
	retval = clk_prepare(usbpll_clk);
	if (retval == 0) {
		DBG(0, KERN_WARNING "prepare done\n");
	} else {
		DBG(0, KERN_WARNING "prepare fail\n");
		return retval;
	}
	#endif

	usb_clk = devm_clk_get(&pdev->dev, "usb");
	if (IS_ERR(usb_clk)) {
		DBG(0, KERN_WARNING "cannot get usb clock\n");
		return PTR_ERR(usb_clk);
	}
	#ifndef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	DBG(0, KERN_WARNING "get usb clock ok, prepare it\n");
	retval = clk_prepare(usb_clk);
	if (retval == 0) {
		DBG(0, KERN_WARNING "prepare done\n");
	} else {
		DBG(0, KERN_WARNING "prepare fail\n");
		return retval;
	}
	#endif

	usbmcu_clk = devm_clk_get(&pdev->dev, "usbmcu");
	if (IS_ERR(usbmcu_clk)) {
		DBG(0, KERN_WARNING "cannot get usbmcu clock\n");
		return PTR_ERR(usbmcu_clk);
	}
	#ifndef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	DBG(0, KERN_WARNING "get usbmcu clock ok, prepare it\n");
	retval = clk_prepare(usbmcu_clk);
	if (retval == 0) {
		DBG(0, KERN_WARNING "prepare done\n");
	} else {
		DBG(0, KERN_WARNING "prepare fail\n");
		return retval;
	}
	#endif

	icusb_clk = devm_clk_get(&pdev->dev, "icusb");
	if (IS_ERR(icusb_clk)) {
		DBG(0, KERN_WARNING "cannot get icusb clock\n");
		return PTR_ERR(icusb_clk);
	}
	#ifndef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	DBG(0, KERN_WARNING "get icusb clock ok, prepare it\n");
	retval = clk_prepare(icusb_clk);
	if (retval == 0) {
		DBG(0, KERN_WARNING "prepare done\n");
	} else {
		DBG(0, KERN_WARNING "prepare fail\n");
		return retval;
	}
	#endif
#endif
#ifdef CONFIG_MTK_UART_USB_SWITCH
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8167-pinctrl");
	node_pctl = of_parse_phandle(np, "mediatek,pctl-regmap", 0);
	if (node_pctl) {
		mt_regmap = syscon_node_to_regmap(node_pctl);
		if (IS_ERR(mt_regmap))
			return PTR_ERR(mt_regmap);
		DBG(0, KERN_WARNING "Get PINCTRL SUCCESS!!\n");
	}
#endif
	DBG(0, "[U2]usb20 dts probe\n");
	mt_usb_device.dev.of_node = pdev->dev.of_node;
	retval = platform_device_register(&mt_usb_device);
	if (retval != 0)
		DBG(0, "register musb device fail!\n");

	DBG(0, "[U2]usb20 dts probe ret:%d\n", retval);

	return retval;
}

static int mt_usb_remove(struct platform_device *pdev)
{
	struct mt_usb_glue		*glue = platform_get_drvdata(pdev);

	platform_device_unregister(glue->musb);
	kfree(glue);

	return 0;
}

#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
static void mt_usb_shutdown(struct platform_device *pdev)
{
	pr_err("%s, start to shut down\n", __func__);
	/* disable all level-1 interrupts in shutdown status */
	musb_writel(mtk_musb->mregs, USB_L1INTM, 0x0);
	musb_writeb(mtk_musb->mregs, MUSB_HSDMA_INTR, 0xff);
	pr_err("%s, 0xa4 = 0x%x, 0x200 = 0x%x\n", __func__,
		musb_readl(mtk_musb->mregs, USB_L1INTM),
		musb_readb(mtk_musb->mregs, MUSB_HSDMA_INTR));
	usb_shutdown = true;

}
#endif

static int mt_usb_dts_remove(struct platform_device *pdev)
{
	struct mt_usb_glue		*glue = platform_get_drvdata(pdev);

	platform_device_unregister(glue->musb);
	kfree(glue);

#ifndef FPGA_PLATFORM
	clk_unprepare(icusb_clk);
	clk_unprepare(usbmcu_clk);
	clk_unprepare(usb_clk);
	clk_unprepare(usbpll_clk);
#endif

	return 0;
}


static struct platform_driver mt_usb_driver = {
	.remove		= mt_usb_remove,
	.probe		= mt_usb_probe,
	.driver		= {
		.name	= "mt_usb",
	},
	#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
	.shutdown = mt_usb_shutdown,
	#endif
};

static struct platform_driver mt_usb_dts_driver = {
	.remove		= mt_usb_dts_remove,
	.probe		= mt_usb_dts_probe,
	.driver		= {
		.name	= "mt_dts_usb",
#ifdef CONFIG_OF
		.of_match_table = apusb_of_ids,
#endif
	},
};

#ifdef CONFIG_MTK_USB_DISABLE_SDP
static int usb20_process_charger_type(void)
{
	int charger_type;

	charger_type = hw_charging_get_charger_type();
	pr_info("charger type:%d\n", charger_type);
	if (charger_type == STANDARD_HOST || charger_type == CHARGER_UNKNOWN) {
		/* close pmic */
		/* mt_power_off(); */
		musb_charger_info = 1;
	} else {
		/* go on */
		musb_charger_info = charger_type;
	}

	return charger_type;
}
#endif

static int __init usb20_init(void)
{
	int ret;

	DBG(0, "usb20 init\n");

	platform_driver_register(&mt_usb_driver);
	ret = platform_driver_register(&mt_usb_dts_driver);

#ifdef FPGA_PLATFORM
	add_usb_i2c_driver();
#endif

#ifdef CONFIG_MTK_USB_DISABLE_SDP
	usb20_process_charger_type();
#endif

	DBG(0, "usb20 init ret:%d\n", ret);
	return ret;
}

late_initcall(usb20_init);

static void __exit usb20_exit(void)
{
	platform_driver_unregister(&mt_usb_driver);
	platform_driver_unregister(&mt_usb_dts_driver);
}
module_exit(usb20_exit)
