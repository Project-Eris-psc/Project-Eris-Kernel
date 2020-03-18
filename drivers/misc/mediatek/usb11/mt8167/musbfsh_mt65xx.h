/*
 * Driver for Special USB-PHY of MUSB HOST peripheral
 *
 * The power sequence and programming bits were different from SoC.
 * Please use the coorsponding USB-PHY API for your SoC.
 * This driver includes Mediatek MUSB DT/ICUSB support.
 *
 * Copyright 2015 Mediatek Inc.
 *      Marvin Lin <marvin.lin@mediatek.com>
 *      Arvin Wang <arvin.wang@mediatek.com>
 *      Vincent Fan <vincent.fan@mediatek.com>
 *      Bryant Lu <bryant.lu@mediatek.com>
 *      Yu-Chang Wang <yu-chang.wang@mediatek.com>
 *      Macpaul Lin <macpaul.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MUSBFSH_MT65XX_H__
#define __MUSBFSH_MT65XX_H__
#ifndef CONFIG_OF
#include <mach/mt_reg_base.h>
#endif

extern int usb11_init(void);
extern void usb11_exit(void);
extern void enable_mcu_clock(bool enable);
extern void enable_phy_clock(bool enable);
extern struct clk *icusb_clk;
/* extern struct clk *usb_clk; */
#ifdef CONFIG_OF
extern struct musbfsh *musbfsh_Device;
#endif
/*#define IC_USB*/

/*Level 1 interrupts:*/
#define USB11_L1INTS 0xA0
#define USB11_L1INTM 0xA4
#define USB11_L1INTP 0xA8
#define MUSBFSH_DMA_INTR_UNMASK_CLR_OFFSET (16)
#define MUSBFSH_DMA_INTR_UNMASK_SET_OFFSET (24)
#define USB11_BASE USB1_BASE
#define USB_BASE USB11_BASE
/*USB11 PHY registers:*/
#define USB11_PHY_ADDR (USB_SIF_BASE + 0x900)

#define U1PHYCR0 0xC0
#define RG_USB11_FSLS_ENBGRI 0x08	/* @U1PHYCR0+1, 1:power on or recovery; 0:save current*/

#define U1PHYCR1 0xC4
#define force_usb11_en_fs_ls_rcv 0x04	/* @U1PHYCR1+2*/
#define force_usb11_en_fs_ls_tx 0x02	/* @U1PHYCR1+2*/
#define RG_USB11_EN_FS_LS_RCV 0x04	/* @U1PHYCR1+3*/
#define RG_USB11_EN_FS_LS_TX 0x02	/* @U1PHYCR1+3*/

#define U1PHTCR2 0xC8
#define force_usb11_dm_rpu 0x01
#define force_usb11_dp_rpu 0x02
#define force_usb11_dm_rpd 0x04
#define force_usb11_dp_rpd 0x08
#define RG_USB11_DM_RPU 0x10
#define RG_USB11_DP_RPU 0x20
#define RG_USB11_DM_RPD 0x40
#define RG_USB11_DP_RPD 0x80
#define RG_USB11_AVALID 0x04	/* @U1PHYCR2+2*/
#define RG_USB11_BVALID 0x08	/* @U1PHYCR2+2*/
#define RG_USB11_SESSEND 0x10	/* @U1PHYCR2+2*/
#define RG_USB11_VBUSVALID 0x20	/* @U1PHYCR2+2*/
#define force_usb11_avalid 0x04	/*@U1PHYCR2+3*/
#define force_usb11_bvalid 0x08	/* @U1PHYCR2+3*/
#define force_usb11_sessend 0x10	/* @U1PHYCR2+3*/
#define force_usb11_vbusvalid 0x20	/* @U1PHYCR2+3*/

#ifdef CONFIG_OF

/*USB11 PHY access macro: Need Modify Later*/
#define USB11PHY_READ32(offset)         __raw_readl((void __iomem *)(musbfsh_Device->phy_reg_base + 0x900 + (offset)))
#define USB11PHY_READ8(offset)          __raw_readb((void __iomem *)(musbfsh_Device->phy_reg_base + 0x900 + (offset)))
#define USB11PHY_WRITE8(offset, value)  \
	__raw_writeb(value, (void __iomem *)(musbfsh_Device->phy_reg_base + 0x900 + (offset)))
#define USB11PHY_SET8(offset, mask)     USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) | (mask))
#define USB11PHY_CLR8(offset, mask)     USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) & (~(mask)))
#else

/*USB11 PHY access macro:*/
#define USB11PHY_READ32(offset)         __raw_readl((void __iomem *)(USB11_PHY_ADDR + (offset)))
#define USB11PHY_READ8(offset)          __raw_readb((void __iomem *)(USB11_PHY_ADDR + (offset)))
#define USB11PHY_WRITE8(offset, value)  __raw_writeb(value, (void __iomem *)(USB11_PHY_ADDR + (offset)))
#define USB11PHY_SET8(offset, mask)     USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) | (mask))
#define USB11PHY_CLR8(offset, mask)     USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) & (~(mask)))
#endif
#endif
