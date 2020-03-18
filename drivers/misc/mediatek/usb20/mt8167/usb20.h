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

#ifndef __USB20_H__
#define __USB20_H__
#ifdef CONFIG_MTK_UART_USB_SWITCH
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#endif

#define ID_PIN_USE_EX_EINT 1
extern bool mtk_usb_power;
extern int ep_config_from_table_for_host(struct musb *musb);

#ifdef CONFIG_MTK_HDMI_SUPPORT
extern void mt_usb_check_reconnect(void);
#endif

#ifdef CONFIG_MTK_MUSB_SW_WITCH_MODE
extern void musb_id_pin_sw_work(bool host_mode);
#endif
#ifdef CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE
extern void mt_usb_clock_prepare(void);
extern void mt_usb_clock_unprepare(void);
#endif

extern unsigned musb_charger_info;

#ifdef CONFIG_MTK_UART_USB_SWITCH
extern struct regmap *mt_regmap;
#endif

#ifdef CONFIG_USB_MTK_OTG
#ifdef ID_PIN_USE_EX_EINT
#if defined(CONFIG_MTK_LEGACY)
#define IDDIG_EINT_PIN (GPIO_OTG_IDDIG_EINT_PIN & ~(0x80000000))
/*#define IDDIG_EINT_PIN 49 */
#endif
#else
#ifdef CONFIG_OF
extern struct musb *mtk_musb;
#define U2PHYDTM1  (((unsigned long)mtk_musb->xceiv->io_priv)+0x800 + 0x6c)
#else
#define U2PHYDTM1  (USB_SIF_BASE+0x800 + 0x6c)
#endif
#define ID_PULL_UP 0x0101
#define ID_PHY_RESET 0x3d11
#endif
#endif

#if (defined(CONFIG_MTK_FAN5405_SUPPORT) \
	|| defined(CONFIG_MTK_BQ24158_SUPPORT) \
	|| defined(CONFIG_MTK_NCP1851_SUPPORT) \
	|| defined(CONFIG_MTK_BQ24196_SUPPORT) \
	|| defined(CONFIG_MTK_NCP1854_SUPPORT) \
	|| defined(CONFIG_MTK_BQ24297_SUPPORT) \
	|| defined(CONFIG_MTK_BQ24296_SUPPORT)) \
	&& !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
#define OTG_BOOST_BY_SWITCH_CHARGER 1
#endif

#if defined(CONFIG_FPGA_EARLY_PORTING)
#define FPGA_PLATFORM 1
#endif

struct mt_usb_glue {
	struct device		*dev;
	struct platform_device	*musb;
};

extern void wake_up_bat(void);
extern enum CHARGER_TYPE mt_charger_type_detection(void);
extern bool upmu_is_chr_det(void);
extern kal_uint32 upmu_get_rgs_chrdet(void);
extern void BATTERY_SetUSBState(int usb_state);
extern void upmu_interrupt_chrdet_int_en(kal_uint32 val);

/* specific USB fuctnion */
enum CABLE_MODE {
	CABLE_MODE_CHRG_ONLY = 0,
	CABLE_MODE_NORMAL,
	CABLE_MODE_HOST_ONLY,
	CABLE_MODE_MAX
};

#ifdef CONFIG_MTK_UART_USB_SWITCH
typedef enum {
	PORT_MODE_USB = 0,
	PORT_MODE_UART,

	PORT_MODE_MAX
} USB_PORT_MODE;

extern u32 usb_port_mode_temp;
extern void mtk_uart_usb_rx_sel(unsigned int uart_port, unsigned int enable);

extern bool usb_phy_check_in_uart_mode(void);
extern void usb_phy_switch_to_usb(void);
extern void usb_phy_switch_to_uart(void);
#endif
/*add struct and enum for linux kernel 3.10 dts*/

#if 0
/*enum {
*	usb0 = 0,
*	usb_sif,
*	usb_acm_temp_device,
*	};
*/
#endif

/* switch charger API*/
#ifdef CONFIG_MTK_FAN5405_SUPPORT
extern void fan5405_set_opa_mode(unsigned int val);
extern void fan5405_set_otg_pl(unsigned int val);
extern void fan5405_set_otg_en(unsigned int val);
extern unsigned int fan5405_reg_config_interface(unsigned char RegNum, unsigned char val);
#elif defined(CONFIG_MTK_BQ24261_SUPPORT)
extern void bq24261_set_en_boost(unsigned int val);
#elif defined(CONFIG_MTK_BQ24296_SUPPORT)
extern void bq24296_set_otg_config(unsigned int val);
extern void bq24296_set_boostv(unsigned int val);
extern void bq24296_set_boost_lim(unsigned int val);
extern void bq24296_set_en_hiz(unsigned int val);
#elif defined(CONFIG_MTK_BQ24196_SUPPORT)
extern void bq24196_set_otg_config(unsigned int val);
extern void bq24196_set_boost_lim(unsigned int val);
#elif defined CONFIG_MTK_NCP1854_SUPPORT
extern void ncp1854_set_chg_en(unsigned int val);
extern void ncp1854_set_otg_en(unsigned int val);
#endif

#ifndef FPGA_PLATFORM
extern struct clk *usbpll_clk;
extern struct clk *usbmcu_clk;
extern struct clk *usb_clk;
extern struct clk *icusb_clk;
#endif

#ifdef OTG_BOOST_BY_SWITCH_CHARGER
extern void tbl_charger_otg_vbus(kal_uint32 mode);
#endif
#ifdef FPGA_PLATFORM
extern void USB_PHY_Write_Register8(UINT8 var,  UINT8 addr);
extern UINT8 USB_PHY_Read_Register8(UINT8 addr);
#endif

#ifdef CONFIG_USB_VBUSDET_IN_GPIO
int gpio_get_vbus_status(void);
#endif

#ifdef CONFIG_USB_PHYCHK_EXTCONN
enum mt65xx_usb_extconn_type {
	MT_USB_EXTCONN_UNKNOWN = 0,
	MT_USB_EXTCONN_STANDARDHOST = 1,
	MT_USB_EXTCONN_CHARGINGHOST = 2,
	MT_USB_EXTCONN_NONSTANDARDCHARGER = 3,
	MT_USB_EXTCONN_STANDARDCHARGER = 4,
	MT_USB_EXTCONN_MAXIMUM = 5,
};
extern int mt_usb_phychk_extconn(void);
#endif
#endif
