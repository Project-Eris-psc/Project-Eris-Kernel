/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <mt-plat/mtk_chip.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8167-clk.h>

static DEFINE_SPINLOCK(mt8167_clk_lock);

static const struct mtk_fixed_clk fixed_clks[] __initconst = {
	FIXED_CLK(CLK_TOP_CLK_NULL, "clk_null", NULL, 0),
	FIXED_CLK(CLK_TOP_I2S_INFRA_BCK, "i2s_infra_bck", "clk_null", 26000000),
	FIXED_CLK(CLK_TOP_MEMPLL, "mempll", "clk26m", 800000000),
	FIXED_CLK(CLK_TOP_DSI0_LNTC_DSICK, "dsi0_lntc_dsick", "clk26m", 75000000),
	FIXED_CLK(CLK_TOP_VPLL_DPIX, "vpll_dpix", "clk26m", 75000000),
	FIXED_CLK(CLK_TOP_LVDSTX_CLKDIG_CTS, "lvdstx_dig_cts", "clk26m", 52500000),
};

static const struct mtk_fixed_factor top_divs[] __initconst = {
	FACTOR(CLK_TOP_DMPLL, "dmpll_ck", "mempll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D2, "mainpll_d2", "mainpll", 1, 2),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4", "mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D8, "mainpll_d8", "mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D16, "mainpll_d16", "mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D11, "mainpll_d11", "mainpll", 1, 11),
	FACTOR(CLK_TOP_MAINPLL_D22, "mainpll_d22", "mainpll", 1, 22),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6", "mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D12, "mainpll_d12", "mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D10, "mainpll_d10", "mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D20, "mainpll_d20", "mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D40, "mainpll_d40", "mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D14, "mainpll_d14", "mainpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4", "univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D8, "univpll_d8", "univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D16, "univpll_d16", "univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6", "univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D12, "univpll_d12", "univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D24, "univpll_d24", "univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D20, "univpll_d20", "univpll", 1, 20),
	FACTOR(CLK_TOP_MMPLL380M, "mmpll380m", "mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),
	FACTOR(CLK_TOP_MMPLL_200M, "mmpll_200m", "mmpll", 1, 3),
	FACTOR(CLK_TOP_LVDSPLL, "lvdspll_ck", "lvdspll", 1, 1),
	FACTOR(CLK_TOP_LVDSPLL_D2, "lvdspll_d2", "lvdspll", 1, 2),
	FACTOR(CLK_TOP_LVDSPLL_D4, "lvdspll_d4", "lvdspll", 1, 4),
	FACTOR(CLK_TOP_LVDSPLL_D8, "lvdspll_d8", "lvdspll", 1, 8),
	FACTOR(CLK_TOP_USB_PHY48M, "usb_phy48m_ck", "univpll", 1, 26),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "rg_apll1_d2_en", 1, 2),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "rg_apll1_d4_en", 1, 2),
	FACTOR(CLK_TOP_APLL2, "apll2_ck", "apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2", "apll2_ck", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4", "rg_apll2_d2_en", 1, 2),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8", "rg_apll2_d4_en", 1, 2),
	FACTOR(CLK_TOP_CLK26M, "clk26m_ck", "clk26m", 1, 1),
	FACTOR(CLK_TOP_CLK26M_D2, "clk26m_d2", "clk26m", 1, 2),
	FACTOR(CLK_TOP_MIPI_26M, "mipi_26m", "clk26m", 1, 1),
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck", "tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll_ck", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll_ck", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll_ck", 1, 8),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll_ck", 1, 16),
	FACTOR(CLK_TOP_AHB_INFRA_D2, "ahb_infra_d2", "ahb_infra_sel", 1, 2),
	FACTOR(CLK_TOP_NFI1X, "nfi1x_ck", "nfi2x_pad_sel", 1, 2),
	FACTOR(CLK_TOP_ETH_D2, "eth_d2_ck", "eth_sel", 1, 2),
};

static const char * const uart0_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d24"
};

static const char * const gfmux_emi1x_parents[] __initconst = {
	"clk26m_ck",
	"dmpll_ck"
};

static const char * const emi_ddrphy_parents[] __initconst = {
	"gfmux_emi1x_sel",
	"gfmux_emi1x_sel"
};

static const char * const ahb_infra_parents[] __initconst = {
	"clk_null",
	"clk26m_ck",
	"mainpll_d11",
	"clk_null",
	"mainpll_d12",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d10"
};

static const char * const csw_mux_mfg_parents[] __initconst = {
	"clk_null",
	"clk_null",
	"univpll_d3",
	"univpll_d2",
	"clk26m_ck",
	"mainpll_d4",
	"univpll_d24",
	"mmpll380m"
};

static const char * const msdc0_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d6",
	"mainpll_d8",
	"univpll_d8",
	"mainpll_d16",
	"mmpll_200m",
	"mainpll_d12",
	"mmpll_d2"
};

static const char * const camtg_mm_parents[] __initconst = {
	"clk_null",
	"clk26m_ck",
	"usb_phy48m_ck",
	"clk_null",
	"univpll_d6"
};

static const char * const pwm_mm_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d12"
};

static const char * const uart1_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d24"
};

static const char * const msdc1_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d6",
	"mainpll_d8",
	"univpll_d8",
	"mainpll_d16",
	"mmpll_200m",
	"mainpll_d12",
	"mmpll_d2"
};

static const char * const spm_52m_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d24"
};

static const char * const pmicspi_parents[] __initconst = {
	"univpll_d20",
	"usb_phy48m_ck",
	"univpll_d16",
	"clk26m_ck"
};

static const char * const qaxi_aud26m_parents[] __initconst = {
	"clk26m_ck",
	"ahb_infra_sel"
};

static const char * const aud_intbus_parents[] __initconst = {
	"clk_null",
	"clk26m_ck",
	"mainpll_d22",
	"clk_null",
	"mainpll_d11"
};

static const char * const nfi2x_pad_parents[] __initconst = {
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk26m_ck",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d12",
	"mainpll_d8",
	"clk_null",
	"mainpll_d6",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d4",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d10",
	"mainpll_d7",
	"clk_null",
	"mainpll_d5"
};

static const char * const nfi1x_pad_parents[] __initconst = {
	"ahb_infra_sel",
	"nfi1x_ck"
};

static const char * const mfg_mm_parents[] __initconst = {
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"csw_mux_mfg_sel",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d3",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"mainpll_d5",
	"mainpll_d7",
	"clk_null",
	"mainpll_d14"
};

static const char * const ddrphycfg_parents[] __initconst = {
	"clk26m_ck",
	"mainpll_d16"
};

static const char * const smi_mm_parents[] __initconst = {
	"clk26m_ck",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"clk_null",
	"univpll_d4",
	"mainpll_d7",
	"clk_null",
	"mainpll_d14"
};

static const char * const usb_78m_parents[] __initconst = {
	"clk_null",
	"clk26m_ck",
	"univpll_d16",
	"clk_null",
	"mainpll_d20"
};

static const char * const scam_mm_parents[] __initconst = {
	"clk_null",
	"clk26m_ck",
	"mainpll_d14",
	"clk_null",
	"mainpll_d12"
};

static const char * const spinor_parents[] __initconst = {
	"clk26m_d2",
	"clk26m_ck",
	"mainpll_d40",
	"univpll_d24",
	"univpll_d20",
	"mainpll_d20",
	"mainpll_d16",
	"univpll_d12"
};

static const char * const msdc2_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d6",
	"mainpll_d8",
	"univpll_d8",
	"mainpll_d16",
	"mmpll_200m",
	"mainpll_d12",
	"mmpll_d2"
};

static const char * const eth_parents[] __initconst = {
	"clk26m_ck",
	"mainpll_d40",
	"univpll_d24",
	"univpll_d20",
	"mainpll_d20"
};

static const char * const vdec_mm_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d4",
	"mainpll_d4",
	"univpll_d5",
	"univpll_d6",
	"mainpll_d6"
};

static const char * const dpi0_mm_parents[] __initconst = {
	"clk26m_ck",
	"lvdspll_ck",
	"lvdspll_d2",
	"lvdspll_d4",
	"lvdspll_d8"
};

static const char * const dpi1_mm_parents[] __initconst = {
	"clk26m_ck",
	"tvdpll_d2",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16"
};

static const char * const axi_mfg_in_parents_e1[] __initconst = {
	"clk26m_ck",
	"gfmux_emi1x_sel",
	"univpll_d24",
	"mmpll380m"
};

static const char * const axi_mfg_in_parents[] __initconst = {
	"clk26m_ck",
	"mainpll_d11",
	"univpll_d24",
	"mmpll380m"
};

static const char * const slow_mfg_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d12",
	"univpll_d24"
};

static const char * const aud1_parents[] __initconst = {
	"clk26m_ck",
	"apll1_ck"
};

static const char * const aud2_parents[] __initconst = {
	"clk26m_ck",
	"apll2_ck"
};

static const char * const aud_engen1_parents[] __initconst = {
	"clk26m_ck",
	"rg_apll1_d2_en",
	"rg_apll1_d4_en",
	"rg_apll1_d8_en"
};

static const char * const aud_engen2_parents[] __initconst = {
	"clk26m_ck",
	"rg_apll2_d2_en",
	"rg_apll2_d4_en",
	"rg_apll2_d8_en"
};

static const char * const i2c_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d20",
	"univpll_d16",
	"univpll_d12"
};

static const char * const aud_i2s0_m_parents[] __initconst = {
	"rg_aud1",
	"rg_aud2"
};

static const char * const pwm_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d12"
};

static const char * const spi_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d12",
	"univpll_d8",
	"univpll_d6"
};

static const char * const aud_spdifin_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d2"
};

static const char * const uart2_parents[] __initconst = {
	"clk26m_ck",
	"univpll_d24"
};

static const char * const bsi_parents[] __initconst = {
	"clk26m_ck",
	"mainpll_d10",
	"mainpll_d12",
	"mainpll_d20"
};

static const char * const dbg_atclk_parents[] __initconst = {
	"clk_null",
	"clk26m_ck",
	"mainpll_d5",
	"clk_null",
	"univpll_d5"
};

static const char * const csw_nfiecc_parents[] __initconst = {
	"clk_null",
	"mainpll_d7",
	"mainpll_d6",
	"clk_null",
	"mainpll_d5"
};

static const char * const nfiecc_parents[] __initconst = {
	"clk_null",
	"nfi2x_pad_sel",
	"mainpll_d4",
	"clk_null",
	"csw_nfiecc_sel"
};

static struct mtk_composite top_muxes[] __initdata = {
	/* CLK_MUX_SEL0 */
	MUX(CLK_TOP_UART0_SEL, "uart0_sel", uart0_parents,
		0x000, 0, 1),
	MUX(CLK_TOP_GFMUX_EMI1X_SEL, "gfmux_emi1x_sel", gfmux_emi1x_parents,
		0x000, 1, 1),
	MUX(CLK_TOP_EMI_DDRPHY_SEL, "emi_ddrphy_sel", emi_ddrphy_parents,
		0x000, 2, 1),
	MUX(CLK_TOP_AHB_INFRA_SEL, "ahb_infra_sel", ahb_infra_parents,
		0x000, 4, 4),
	MUX(CLK_TOP_CSW_MUX_MFG_SEL, "csw_mux_mfg_sel", csw_mux_mfg_parents,
		0x000, 8, 3),
	MUX(CLK_TOP_MSDC0_SEL, "msdc0_sel", msdc0_parents,
		0x000, 11, 3),
	MUX(CLK_TOP_CAMTG_MM_SEL, "camtg_mm_sel", camtg_mm_parents,
		0x000, 15, 3),
	MUX(CLK_TOP_PWM_MM_SEL, "pwm_mm_sel", pwm_mm_parents,
		0x000, 18, 1),
	MUX(CLK_TOP_UART1_SEL, "uart1_sel", uart1_parents,
		0x000, 19, 1),
	MUX(CLK_TOP_MSDC1_SEL, "msdc1_sel", msdc1_parents,
		0x000, 20, 3),
	MUX(CLK_TOP_SPM_52M_SEL, "spm_52m_sel", spm_52m_parents,
		0x000, 23, 1),
	MUX(CLK_TOP_PMICSPI_SEL, "pmicspi_sel", pmicspi_parents,
		0x000, 24, 2),
	MUX(CLK_TOP_QAXI_AUD26M_SEL, "qaxi_aud26m_sel", qaxi_aud26m_parents,
		0x000, 26, 1),
	MUX(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel", aud_intbus_parents,
		0x000, 27, 3),
	/* CLK_MUX_SEL1 */
	MUX(CLK_TOP_NFI2X_PAD_SEL, "nfi2x_pad_sel", nfi2x_pad_parents,
		0x004, 0, 7),
	MUX(CLK_TOP_NFI1X_PAD_SEL, "nfi1x_pad_sel", nfi1x_pad_parents,
		0x004, 7, 1),
	MUX(CLK_TOP_MFG_MM_SEL, "mfg_mm_sel", mfg_mm_parents,
		0x004, 8, 6),
	MUX(CLK_TOP_DDRPHYCFG_SEL, "ddrphycfg_sel", ddrphycfg_parents,
		0x004, 15, 1),
	MUX(CLK_TOP_SMI_MM_SEL, "smi_mm_sel", smi_mm_parents,
		0x004, 16, 4),
	MUX(CLK_TOP_USB_78M_SEL, "usb_78m_sel", usb_78m_parents,
		0x004, 20, 3),
	MUX(CLK_TOP_SCAM_MM_SEL, "scam_mm_sel", scam_mm_parents,
		0x004, 23, 3),
	/* CLK_MUX_SEL8 */
	MUX(CLK_TOP_SPINOR_SEL, "spinor_sel", spinor_parents,
		0x040, 0, 3),
	MUX(CLK_TOP_MSDC2_SEL, "msdc2_sel", msdc2_parents,
		0x040, 3, 3),
	MUX(CLK_TOP_ETH_SEL, "eth_sel", eth_parents,
		0x040, 6, 3),
	MUX(CLK_TOP_VDEC_MM_SEL, "vdec_mm_sel", vdec_mm_parents,
		0x040, 9, 3),
	MUX(CLK_TOP_DPI0_MM_SEL, "dpi0_mm_sel", dpi0_mm_parents,
		0x040, 12, 3),
	MUX(CLK_TOP_DPI1_MM_SEL, "dpi1_mm_sel", dpi1_mm_parents,
		0x040, 15, 3),
	MUX(CLK_TOP_AXI_MFG_IN_SEL, "axi_mfg_in_sel", axi_mfg_in_parents,
		0x040, 18, 2),
	MUX(CLK_TOP_SLOW_MFG_SEL, "slow_mfg_sel", slow_mfg_parents,
		0x040, 20, 2),
	MUX(CLK_TOP_AUD1_SEL, "aud1_sel", aud1_parents,
		0x040, 22, 1),
	MUX(CLK_TOP_AUD2_SEL, "aud2_sel", aud2_parents,
		0x040, 23, 1),
	MUX(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel", aud_engen1_parents,
		0x040, 24, 2),
	MUX(CLK_TOP_AUD_ENGEN2_SEL, "aud_engen2_sel", aud_engen2_parents,
		0x040, 26, 2),
	MUX(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents,
		0x040, 28, 2),
	/* CLK_SEL_9 */
	MUX(CLK_TOP_AUD_I2S0_M_SEL, "aud_i2s0_m_sel", aud_i2s0_m_parents,
		0x044, 12, 1),
	MUX(CLK_TOP_AUD_I2S1_M_SEL, "aud_i2s1_m_sel", aud_i2s0_m_parents,
		0x044, 13, 1),
	MUX(CLK_TOP_AUD_I2S2_M_SEL, "aud_i2s2_m_sel", aud_i2s0_m_parents,
		0x044, 14, 1),
	MUX(CLK_TOP_AUD_I2S3_M_SEL, "aud_i2s3_m_sel", aud_i2s0_m_parents,
		0x044, 15, 1),
	MUX(CLK_TOP_AUD_I2S4_M_SEL, "aud_i2s4_m_sel", aud_i2s0_m_parents,
		0x044, 16, 1),
	MUX(CLK_TOP_AUD_I2S5_M_SEL, "aud_i2s5_m_sel", aud_i2s0_m_parents,
		0x044, 17, 1),
	MUX(CLK_TOP_AUD_SPDIF_B_SEL, "aud_spdif_b_sel", aud_i2s0_m_parents,
		0x044, 18, 1),
	/* CLK_MUX_SEL13 */
	MUX(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents,
		0x07c, 0, 1),
	MUX(CLK_TOP_SPI_SEL, "spi_sel", spi_parents,
		0x07c, 1, 2),
	MUX(CLK_TOP_AUD_SPDIFIN_SEL, "aud_spdifin_sel", aud_spdifin_parents,
		0x07c, 3, 1),
	MUX(CLK_TOP_UART2_SEL, "uart2_sel", uart2_parents,
		0x07c, 4, 1),
	MUX(CLK_TOP_BSI_SEL, "bsi_sel", bsi_parents,
		0x07c, 5, 2),
	MUX(CLK_TOP_DBG_ATCLK_SEL, "dbg_atclk_sel", dbg_atclk_parents,
		0x07c, 7, 3),
	MUX(CLK_TOP_CSW_NFIECC_SEL, "csw_nfiecc_sel", csw_nfiecc_parents,
		0x07c, 10, 3),
	MUX(CLK_TOP_NFIECC_SEL, "nfiecc_sel", nfiecc_parents,
		0x07c, 13, 3),
};

static const char * const ifr_mux1_parents[] __initconst = {
	"clk26m_ck",
	"armpll",
	"univpll",
	"mainpll_d2"
};

static const char * const ifr_eth_25m_parents[] __initconst = {
	"eth_d2_ck",
	"rg_eth"
};

static const char * const ifr_i2c0_parents[] __initconst = {
	"ahb_infra_d2",
	"rg_i2c"
};

static const struct mtk_composite ifr_muxes[] __initconst = {
	MUX(CLK_IFR_MUX1_SEL, "ifr_mux1_sel", ifr_mux1_parents, 0x000,
		2, 2),
	MUX(CLK_IFR_ETH_25M_SEL, "ifr_eth_25m_sel", ifr_eth_25m_parents, 0x080,
		0, 1),
	MUX(CLK_IFR_I2C0_SEL, "ifr_i2c0_sel", ifr_i2c0_parents, 0x080,
		1, 1),
	MUX(CLK_IFR_I2C1_SEL, "ifr_i2c1_sel", ifr_i2c0_parents, 0x080,
		2, 1),
	MUX(CLK_IFR_I2C2_SEL, "ifr_i2c2_sel", ifr_i2c0_parents, 0x080,
		3, 1),
};


static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_clk_gate(hw);
	u32 val;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val == 0;
}

static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_clk_gate(hw);
	u32 val;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_clk_gate(hw);

	regmap_update_bits(cg->regmap, cg->sta_ofs, BIT(cg->bit), BIT(cg->bit));
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_clk_gate(hw);

	regmap_update_bits(cg->regmap, cg->sta_ofs, BIT(cg->bit), 0);
}

static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	return 0;
}

static void mtk_cg_disable(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	return 0;
}

static void mtk_cg_disable_inv(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);
}

const struct clk_ops mtk_clk_gate_ops = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
};

const struct clk_ops mtk_clk_gate_ops_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv,
};

struct mtk_clk_divider {
	int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;

	u32 div_reg;
	unsigned char div_shift;
	unsigned char div_width;
	unsigned char clk_divider_flags;
	const struct clk_div_table *clk_div_table;
};

#define DIV_ADJ(_id, _name, _parent, _reg, _shift, _width) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.div_reg = _reg,				\
		.div_shift = _shift,				\
		.div_width = _width,				\
}

#define DIV_ADJ_FLAG(_id, _name, _parent, _reg, _shift, _width ,_flag) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.div_reg = _reg,				\
		.div_shift = _shift,				\
		.div_width = _width,				\
		.clk_divider_flags = _flag,				\
}

void mtk_clk_register_dividers(const struct mtk_clk_divider *mcds,
			int num, void __iomem *base, spinlock_t *lock,
				struct clk_onecell_data *clk_data)
{
	struct clk *clk;
	int i;

	for (i = 0; i <  num; i++) {
		const struct mtk_clk_divider *mcd = &mcds[i];

		if (clk_data && !IS_ERR_OR_NULL(clk_data->clks[mcd->id]))
			continue;

		clk = clk_register_divider(NULL, mcd->name, mcd->parent_name,
			mcd->flags, base +  mcd->div_reg, mcd->div_shift,
			mcd->div_width, mcd->clk_divider_flags, lock);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
				mcd->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[mcd->id] = clk;
	}
}

static const struct mtk_clk_divider top_adj_divs[] = {
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV0, "apll12_ck_div0", "aud_i2s0_m_sel",
		0x0048, 0, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV1, "apll12_ck_div1", "aud_i2s1_m_sel",
		0x0048, 8, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV2, "apll12_ck_div2", "aud_i2s2_m_sel",
		0x0048, 16, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV3, "apll12_ck_div3", "aud_i2s3_m_sel",
		0x0048, 24, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV4, "apll12_ck_div4", "aud_i2s4_m_sel",
		0x004c, 0, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV4B, "apll12_ck_div4b", "apll12_div4",
		0x004c, 8, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV5, "apll12_ck_div5", "aud_i2s5_m_sel",
		0x004c, 16, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV5B, "apll12_ck_div5b", "apll12_div5",
		0x004c, 24, 8),
	DIV_ADJ(CLK_TOP_APLL12_CK_DIV6, "apll12_ck_div6", "aud_spdif_b_sel",
		0x0078, 0, 8),
};

static const struct mtk_clk_divider apmixed_adj_divs[] = {
	DIV_ADJ_FLAG(CLK_APMIXED_HDMI_REF, "hdmi_ref", "tvdpll",
		0x1c4, 24, 3, CLK_DIVIDER_POWER_OF_TWO|CLK_DIVIDER_READ_ONLY),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x80,
	.sta_ofs = 0x20,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x54,
	.clr_ofs = 0x84,
	.sta_ofs = 0x24,
};

static const struct mtk_gate_regs top2_cg_regs = {
	.set_ofs = 0x6c,
	.clr_ofs = 0x9c,
	.sta_ofs = 0x3c,
};

static const struct mtk_gate_regs top3_cg_regs = {
	.set_ofs = 0xa0,
	.clr_ofs = 0xb0,
	.sta_ofs = 0x70,
};

static const struct mtk_gate_regs top4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xb4,
	.sta_ofs = 0x74,
};

static const struct mtk_gate_regs top5_cg_regs = {
	.set_ofs = 0x44,
	.clr_ofs = 0x44,
	.sta_ofs = 0x44,
};

#define GATE_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP0_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP2_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_TOP3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_TOP4_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_TOP5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops,		\
	}

static const struct mtk_gate top_clks[] __initconst = {
	/* TOP0 */
	GATE_TOP0(CLK_TOP_PWM_MM, "pwm_mm", "pwm_mm_sel",
		0),
	GATE_TOP0(CLK_TOP_CAM_MM, "cam_mm", "camtg_mm_sel",
		1),
	GATE_TOP0(CLK_TOP_MFG_MM, "mfg_mm", "mfg_mm_sel",
		2),
	GATE_TOP0(CLK_TOP_SPM_52M, "spm_52m", "spm_52m_sel",
		3),
	GATE_TOP0_I(CLK_TOP_MIPI_26M_DBG, "mipi_26m_dbg", "mipi_26m",
		4),
	GATE_TOP0(CLK_TOP_SCAM_MM, "scam_mm", "scam_mm_sel",
		5),
	GATE_TOP0(CLK_TOP_SMI_MM, "smi_mm", "smi_mm_sel",
		9),
	/* TOP1 */
	GATE_TOP1(CLK_TOP_THEM, "them", "ahb_infra_sel",
		1),
	GATE_TOP1(CLK_TOP_APDMA, "apdma", "ahb_infra_sel",
		2),
	GATE_TOP1(CLK_TOP_I2C0, "i2c0", "ifr_i2c0_sel",
		3),
	GATE_TOP1(CLK_TOP_I2C1, "i2c1", "ifr_i2c1_sel",
		4),
	GATE_TOP1(CLK_TOP_AUXADC1, "auxadc1", "ahb_infra_sel",
		5),
	GATE_TOP1(CLK_TOP_NFI, "nfi", "nfi1x_pad_sel",
		6),
	GATE_TOP1(CLK_TOP_NFIECC, "nfiecc", "rg_nfiecc",
		7),
	GATE_TOP1(CLK_TOP_DEBUGSYS, "debugsys", "rg_dbg_atclk",
		8),
	GATE_TOP1(CLK_TOP_PWM, "pwm", "ahb_infra_sel",
		9),
	GATE_TOP1(CLK_TOP_UART0, "uart0", "uart0_sel",
		10),
	GATE_TOP1(CLK_TOP_UART1, "uart1", "uart1_sel",
		11),
	GATE_TOP1(CLK_TOP_BTIF, "btif", "ahb_infra_sel",
		12),
	GATE_TOP1(CLK_TOP_USB, "usb", "usb_78m",
		13),
	GATE_TOP1(CLK_TOP_FLASHIF_26M, "flashif_26m", "clk26m_ck",
		14),
	GATE_TOP1(CLK_TOP_AUXADC2, "auxadc2", "ahb_infra_sel",
		15),
	GATE_TOP1(CLK_TOP_I2C2, "i2c2", "ifr_i2c2_sel",
		16),
	GATE_TOP1(CLK_TOP_MSDC0, "msdc0", "msdc0_sel",
		17),
	GATE_TOP1(CLK_TOP_MSDC1, "msdc1", "msdc1_sel",
		18),
	GATE_TOP1(CLK_TOP_NFI2X, "nfi2x", "nfi2x_pad_sel",
		19),
	GATE_TOP1(CLK_TOP_PMICWRAP_AP, "pwrap_ap", "clk26m_ck",
		20),
	GATE_TOP1(CLK_TOP_SEJ, "sej", "ahb_infra_sel",
		21),
	GATE_TOP1(CLK_TOP_MEMSLP_DLYER, "memslp_dlyer", "clk26m_ck",
		22),
	GATE_TOP1(CLK_TOP_SPI, "spi", "spi_sel",
		23),
	GATE_TOP1(CLK_TOP_APXGPT, "apxgpt", "clk26m_ck",
		24),
	GATE_TOP1(CLK_TOP_AUDIO, "audio", "clk26m_ck",
		25),
	GATE_TOP1(CLK_TOP_PMICWRAP_MD, "pwrap_md", "clk26m_ck",
		27),
	GATE_TOP1(CLK_TOP_PMICWRAP_CONN, "pwrap_conn", "clk26m_ck",
		28),
	GATE_TOP1(CLK_TOP_PMICWRAP_26M, "pwrap_26m", "clk26m_ck",
		29),
	GATE_TOP1(CLK_TOP_AUX_ADC, "aux_adc", "clk26m_ck",
		30),
	GATE_TOP1(CLK_TOP_AUX_TP, "aux_tp", "clk26m_ck",
		31),
	/* TOP2 */
	GATE_TOP2(CLK_TOP_MSDC2, "msdc2", "ahb_infra_sel",
		0),
	GATE_TOP2(CLK_TOP_RBIST, "rbist", "univpll_d12",
		1),
	GATE_TOP2(CLK_TOP_NFI_BUS, "nfi_bus", "ahb_infra_sel",
		2),
	GATE_TOP2(CLK_TOP_GCE, "gce", "ahb_infra_sel",
		4),
	GATE_TOP2(CLK_TOP_TRNG, "trng", "ahb_infra_sel",
		5),
	GATE_TOP2(CLK_TOP_SEJ_13M, "sej_13m", "clk26m_ck",
		6),
	GATE_TOP2(CLK_TOP_AES, "aes", "ahb_infra_sel",
		7),
	GATE_TOP2(CLK_TOP_PWM_B, "pwm_b", "rg_pwm_infra",
		8),
	GATE_TOP2(CLK_TOP_PWM1_FB, "pwm1_fb", "rg_pwm_infra",
		9),
	GATE_TOP2(CLK_TOP_PWM2_FB, "pwm2_fb", "rg_pwm_infra",
		10),
	GATE_TOP2(CLK_TOP_PWM3_FB, "pwm3_fb", "rg_pwm_infra",
		11),
	GATE_TOP2(CLK_TOP_PWM4_FB, "pwm4_fb", "rg_pwm_infra",
		12),
	GATE_TOP2(CLK_TOP_PWM5_FB, "pwm5_fb", "rg_pwm_infra",
		13),
	GATE_TOP2(CLK_TOP_USB_1P, "usb_1p", "usb_78m",
		14),
	GATE_TOP2(CLK_TOP_FLASHIF_FREERUN, "flashif_freerun", "ahb_infra_sel",
		15),
	GATE_TOP2(CLK_TOP_26M_HDMI_SIFM, "hdmi_sifm_26m", "clk26m_ck",
		16),
	GATE_TOP2(CLK_TOP_26M_CEC, "cec_26m", "clk26m_ck",
		17),
	GATE_TOP2(CLK_TOP_32K_CEC, "cec_32k", "clk32k",
		18),
	GATE_TOP2(CLK_TOP_66M_ETH, "eth_66m", "ahb_infra_d2",
		19),
	GATE_TOP2(CLK_TOP_133M_ETH, "eth_133m", "ahb_infra_sel",
		20),
	GATE_TOP2(CLK_TOP_FETH_25M, "feth_25m", "ifr_eth_25m_sel",
		21),
	GATE_TOP2(CLK_TOP_FETH_50M, "feth_50m", "rg_eth",
		22),
	GATE_TOP2(CLK_TOP_FLASHIF_AXI, "flashif_axi", "ahb_infra_sel",
		23),
	GATE_TOP2(CLK_TOP_USBIF, "usbif", "ahb_infra_sel",
		24),
	GATE_TOP2(CLK_TOP_UART2, "uart2", "rg_uart2",
		25),
	GATE_TOP2(CLK_TOP_BSI, "bsi", "ahb_infra_sel",
		26),
	GATE_TOP2(CLK_TOP_GCPU_B, "gcpu_b", "ahb_infra_sel",
		27),
	GATE_TOP2_I(CLK_TOP_MSDC0_INFRA, "msdc0_infra", "msdc0",
		28),
	GATE_TOP2_I(CLK_TOP_MSDC1_INFRA, "msdc1_infra", "msdc1",
		29),
	GATE_TOP2_I(CLK_TOP_MSDC2_INFRA, "msdc2_infra", "rg_msdc2",
		30),
	GATE_TOP2(CLK_TOP_USB_78M, "usb_78m", "usb_78m_sel",
		31),
	/* TOP3 */
	GATE_TOP3(CLK_TOP_RG_SPINOR, "rg_spinor", "spinor_sel",
		0),
	GATE_TOP3(CLK_TOP_RG_MSDC2, "rg_msdc2", "msdc2_sel",
		1),
	GATE_TOP3(CLK_TOP_RG_ETH, "rg_eth", "eth_sel",
		2),
	GATE_TOP3(CLK_TOP_RG_VDEC, "rg_vdec", "vdec_mm_sel",
		3),
	GATE_TOP3(CLK_TOP_RG_FDPI0, "rg_fdpi0", "dpi0_mm_sel",
		4),
	GATE_TOP3(CLK_TOP_RG_FDPI1, "rg_fdpi1", "dpi1_mm_sel",
		5),
	GATE_TOP3(CLK_TOP_RG_AXI_MFG, "rg_axi_mfg", "axi_mfg_in_sel",
		6),
	GATE_TOP3(CLK_TOP_RG_SLOW_MFG, "rg_slow_mfg", "slow_mfg_sel",
		7),
	GATE_TOP3(CLK_TOP_RG_AUD1, "rg_aud1", "aud1_sel",
		8),
	GATE_TOP3(CLK_TOP_RG_AUD2, "rg_aud2", "aud2_sel",
		9),
	GATE_TOP3(CLK_TOP_RG_AUD_ENGEN1, "rg_aud_engen1", "aud_engen1_sel",
		10),
	GATE_TOP3(CLK_TOP_RG_AUD_ENGEN2, "rg_aud_engen2", "aud_engen2_sel",
		11),
	GATE_TOP3(CLK_TOP_RG_I2C, "rg_i2c", "i2c_sel",
		12),
	GATE_TOP3(CLK_TOP_RG_PWM_INFRA, "rg_pwm_infra", "pwm_sel",
		13),
	GATE_TOP3(CLK_TOP_RG_AUD_SPDIF_IN, "rg_aud_spdif_in", "aud_spdifin_sel",
		14),
	GATE_TOP3(CLK_TOP_RG_UART2, "rg_uart2", "uart2_sel",
		15),
	GATE_TOP3(CLK_TOP_RG_BSI, "rg_bsi", "bsi_sel",
		16),
	GATE_TOP3(CLK_TOP_RG_DBG_ATCLK, "rg_dbg_atclk", "dbg_atclk_sel",
		17),
	GATE_TOP3(CLK_TOP_RG_NFIECC, "rg_nfiecc", "nfiecc_sel",
		18),
	/* TOP4 */
	GATE_TOP4_I(CLK_TOP_RG_APLL1_D2_EN, "rg_apll1_d2_en", "apll1_d2",
		8),
	GATE_TOP4_I(CLK_TOP_RG_APLL1_D4_EN, "rg_apll1_d4_en", "apll1_d4",
		9),
	GATE_TOP4_I(CLK_TOP_RG_APLL1_D8_EN, "rg_apll1_d8_en", "apll1_d8",
		10),
	GATE_TOP4_I(CLK_TOP_RG_APLL2_D2_EN, "rg_apll2_d2_en", "apll2_d2",
		11),
	GATE_TOP4_I(CLK_TOP_RG_APLL2_D4_EN, "rg_apll2_d4_en", "apll2_d4",
		12),
	GATE_TOP4_I(CLK_TOP_RG_APLL2_D8_EN, "rg_apll2_d8_en", "apll2_d8",
		13),
	/* TOP5 */
	GATE_TOP5(CLK_TOP_APLL12_DIV0, "apll12_div0", "apll12_ck_div0",
		0),
	GATE_TOP5(CLK_TOP_APLL12_DIV1, "apll12_div1", "apll12_ck_div1",
		1),
	GATE_TOP5(CLK_TOP_APLL12_DIV2, "apll12_div2", "apll12_ck_div2",
		2),
	GATE_TOP5(CLK_TOP_APLL12_DIV3, "apll12_div3", "apll12_ck_div3",
		3),
	GATE_TOP5(CLK_TOP_APLL12_DIV4, "apll12_div4", "apll12_ck_div4",
		4),
	GATE_TOP5(CLK_TOP_APLL12_DIV4B, "apll12_div4b", "apll12_ck_div4b",
		5),
	GATE_TOP5(CLK_TOP_APLL12_DIV5, "apll12_div5", "apll12_ck_div5",
		6),
	GATE_TOP5(CLK_TOP_APLL12_DIV5B, "apll12_div5b", "apll12_ck_div5b",
		7),
	GATE_TOP5(CLK_TOP_APLL12_DIV6, "apll12_div6", "apll12_ck_div6",
		8),
};
static const struct mtk_gate_regs aud_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_AUD(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &aud_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops,	\
	}

static const struct mtk_gate aud_clks[] __initconst = {
	GATE_AUD(CLK_AUD_AFE, "aud_afe", "clk26m_ck", 2),
	GATE_AUD(CLK_AUD_I2S, "aud_i2s", "i2s_infra_bck", 6),
	GATE_AUD(CLK_AUD_22M, "aud_22m", "rg_aud_engen1", 8),
	GATE_AUD(CLK_AUD_24M, "aud_24m", "rg_aud_engen2", 9),
	GATE_AUD(CLK_AUD_INTDIR, "aud_intdir", "rg_aud_spdif_in", 15),
	GATE_AUD(CLK_AUD_APLL2_TUNER, "aud_apll2_tuner", "rg_aud_engen2", 18),
	GATE_AUD(CLK_AUD_APLL_TUNER, "aud_apll_tuner", "rg_aud_engen1", 19),
	GATE_AUD(CLK_AUD_HDMI, "aud_hdmi", "apll12_div4", 20),
	GATE_AUD(CLK_AUD_SPDF, "aud_spdf", "apll12_div6", 21),
	GATE_AUD(CLK_AUD_ADC, "aud_adc", "aud_afe", 24),
	GATE_AUD(CLK_AUD_DAC, "aud_dac", "aud_afe", 25),
	GATE_AUD(CLK_AUD_DAC_PREDIS, "aud_dac_predis", "aud_afe", 26),
	GATE_AUD(CLK_AUD_TML, "aud_tml", "aud_afe", 27),
};
static const struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mfg_clks[] __initconst = {
	GATE_MFG(CLK_MFG_BAXI, "mfg_baxi", "ahb_infra_sel", 0),
	GATE_MFG(CLK_MFG_BMEM, "mfg_bmem", "gfmux_emi1x_sel", 1),
	GATE_MFG(CLK_MFG_BG3D, "mfg_bg3d", "mfg_mm", 2),
	GATE_MFG(CLK_MFG_B26M, "mfg_b26m", "clk26m_ck", 3),
};
static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MM0(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] __initconst = {
	/* MM0 */
	GATE_MM0(CLK_MM_SMI_COMMON, "mm_smi_common", "smi_mm", 0),
	GATE_MM0(CLK_MM_SMI_LARB0, "mm_smi_larb0", "smi_mm", 1),
	GATE_MM0(CLK_MM_CAM_MDP, "mm_cam_mdp", "smi_mm", 2),
	GATE_MM0(CLK_MM_MDP_RDMA, "mm_mdp_rdma", "smi_mm", 3),
	GATE_MM0(CLK_MM_MDP_RSZ0, "mm_mdp_rsz0", "smi_mm", 4),
	GATE_MM0(CLK_MM_MDP_RSZ1, "mm_mdp_rsz1", "smi_mm", 5),
	GATE_MM0(CLK_MM_MDP_TDSHP, "mm_mdp_tdshp", "smi_mm", 6),
	GATE_MM0(CLK_MM_MDP_WDMA, "mm_mdp_wdma", "smi_mm", 7),
	GATE_MM0(CLK_MM_MDP_WROT, "mm_mdp_wrot", "smi_mm", 8),
	GATE_MM0(CLK_MM_FAKE_ENG, "mm_fake_eng", "smi_mm", 9),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0", "smi_mm", 10),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0", "smi_mm", 11),
	GATE_MM0(CLK_MM_DISP_RDMA1, "mm_disp_rdma1", "smi_mm", 12),
	GATE_MM0(CLK_MM_DISP_WDMA, "mm_disp_wdma", "smi_mm", 13),
	GATE_MM0(CLK_MM_DISP_COLOR, "mm_disp_color", "smi_mm", 14),
	GATE_MM0(CLK_MM_DISP_CCORR, "mm_disp_ccorr", "smi_mm", 15),
	GATE_MM0(CLK_MM_DISP_AAL, "mm_disp_aal", "smi_mm", 16),
	GATE_MM0(CLK_MM_DISP_GAMMA, "mm_disp_gamma", "smi_mm", 17),
	GATE_MM0(CLK_MM_DISP_DITHER, "mm_disp_dither", "smi_mm", 18),
	GATE_MM0(CLK_MM_DISP_UFOE, "mm_disp_ufoe", "smi_mm", 19),
	/* MM1 */
	GATE_MM1(CLK_MM_DISP_PWM_MM, "mm_disp_pwm_mm", "smi_mm", 0),
	GATE_MM1(CLK_MM_DISP_PWM_26M, "mm_disp_pwm_26m", "smi_mm", 1),
	GATE_MM1(CLK_MM_DSI_ENGINE, "mm_dsi_engine", "smi_mm", 2),
	GATE_MM1(CLK_MM_DSI_DIGITAL, "mm_dsi_digital", "dsi0_lntc_dsick", 3),
	GATE_MM1(CLK_MM_DPI0_ENGINE, "mm_dpi0_engine", "smi_mm", 4),
	GATE_MM1(CLK_MM_DPI0_PXL, "mm_dpi0_pxl", "rg_fdpi0", 5),
	GATE_MM1(CLK_MM_LVDS_PXL, "mm_lvds_pxl", "vpll_dpix", 14),
	GATE_MM1(CLK_MM_LVDS_CTS, "mm_lvds_cts", "lvdstx_dig_cts", 15),
	GATE_MM1(CLK_MM_DPI1_ENGINE, "mm_dpi1_engine", "smi_mm", 16),
	GATE_MM1(CLK_MM_DPI1_PXL, "mm_dpi1_pxl", "rg_fdpi1", 17),
	GATE_MM1(CLK_MM_HDMI_PXL, "mm_hdmi_pxl", "rg_fdpi1", 18),
	GATE_MM1(CLK_MM_HDMI_SPDIF, "mm_hdmi_spdif", "apll12_div6", 19),
	GATE_MM1(CLK_MM_HDMI_ADSP_BCK, "mm_hdmi_adsp_b", "apll12_div4b", 20),
	GATE_MM1(CLK_MM_HDMI_PLL, "mm_hdmi_pll", "hdmtx_dig_cts", 21),
};
static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate img_clks[] __initconst = {
	GATE_IMG(CLK_IMG_LARB1_SMI, "img_larb1_smi", "smi_mm", 0),
	GATE_IMG(CLK_IMG_CAM_SMI, "img_cam_smi", "smi_mm", 5),
	GATE_IMG(CLK_IMG_CAM_CAM, "img_cam_cam", "smi_mm", 6),
	GATE_IMG(CLK_IMG_SEN_TG, "img_sen_tg", "cam_mm", 7),
	GATE_IMG(CLK_IMG_SEN_CAM, "img_sen_cam", "smi_mm", 8),
	GATE_IMG(CLK_IMG_VENC, "img_venc", "smi_mm", 9),
};
static const struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDEC0_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDEC1_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate vdec_clks[] __initconst = {
	/* VDEC0 */
	GATE_VDEC0_I(CLK_VDEC_CKEN, "vdec_cken", "rg_vdec", 0),
	/* VDEC1 */
	GATE_VDEC1_I(CLK_VDEC_LARB1_CKEN, "vdec_larb1_cken", "smi_mm", 0),
};

static void __init mtk_topckgen_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r, i;
	void __iomem *base;
	enum chip_sw_ver ver = mt_get_chip_sw_ver();

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	if (ver == CHIP_SW_VER_01) {
		for (i = 0; i < ARRAY_SIZE(top_muxes); i++) {
			struct mtk_composite *t = &top_muxes[i];

			if (t->id == CLK_TOP_AXI_MFG_IN_SEL)
				t->parent_names = axi_mfg_in_parents_e1;
		}
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_fixed_clks(fixed_clks, ARRAY_SIZE(fixed_clks), clk_data);
	mtk_clk_register_gates(node, top_clks, ARRAY_SIZE(top_clks), clk_data);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
	mtk_clk_register_composites(top_muxes, ARRAY_SIZE(top_muxes), base,
		&mt8167_clk_lock, clk_data);
	mtk_clk_register_dividers(top_adj_divs, ARRAY_SIZE(top_adj_divs),
				base, &mt8167_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_topckgen, "mediatek,mt8167-topckgen", mtk_topckgen_init);

static void __init mtk_infracfg_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;
	void __iomem *base;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(CLK_IFR_NR_CLK);

	mtk_clk_register_composites(ifr_muxes, ARRAY_SIZE(ifr_muxes), base,
		&mt8167_clk_lock, clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE(mtk_infracfg, "mediatek,mt8167-infracfg", mtk_infracfg_init);

/* FIXME: modify FMAX */
#define MT8167_PLL_FMAX		(2500UL * MHZ)

#define CON0_MT8167_RST_BAR	BIT(27)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pd_reg, _pd_shift, _tuner_reg, _pcw_reg,	\
			_pcw_shift, _div_table) {			\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = CON0_MT8167_RST_BAR,			\
		.fmax = MT8167_PLL_FMAX,				\
		.pcwbits = _pcwbits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pd_reg, _pd_shift, _tuner_reg, _pcw_reg,	\
			_pcw_shift)					\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits, \
			_pd_reg, _pd_shift, _tuner_reg, _pcw_reg, _pcw_shift, \
			NULL)

static const struct mtk_pll_div_table mmpll_div_table[] = {
	{ .div = 0, .freq = MT8167_PLL_FMAX },
	{ .div = 1, .freq = 1000000000 },
	{ .div = 2, .freq = 604500000 },
	{ .div = 3, .freq = 253500000 },
	{ .div = 4, .freq = 126750000 },
	{ } /* sentinel */
};

static const struct mtk_pll_data plls[] = {
	/* FIXME: need to fix flags/div_table/tuner_reg/table */
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x0100, 0x0110, 0x00000001, 0,
		21, 0x0104, 24, 0, 0x0104, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0120, 0x0130, 0x00000001,
		HAVE_RST_BAR, 21, 0x0124, 24, 0, 0x0124, 0),
	PLL(CLK_APMIXED_UNIVPLL, "univpll", 0x0140, 0x0150, 0x30000001,
		HAVE_RST_BAR, 7, 0x0144, 24, 0, 0x0144, 0),
	PLL_B(CLK_APMIXED_MMPLL, "mmpll", 0x0160, 0x0170, 0x00000001, 0,
		21, 0x0164, 24, 0, 0x0164, 0, mmpll_div_table),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x0180, 0x0190, 0x00000001, 0,
		31, 0x0180, 1, 0x0194, 0x0184, 0),
	PLL(CLK_APMIXED_APLL2, "apll2", 0x01A0, 0x01B0, 0x00000001, 0,
		31, 0x01A0, 1, 0x01B4, 0x01A4, 0),
	PLL(CLK_APMIXED_TVDPLL, "tvdpll", 0x01C0, 0x01D0, 0x00000001, 0,
		21, 0x01C4, 24, 0, 0x01C4, 0),
	PLL(CLK_APMIXED_LVDSPLL, "lvdspll", 0x01E0, 0x01F0, 0x00000001, 0,
		21, 0x01E4, 24, 0, 0x01E4, 0),
};

static void __init mtk_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);

	/* FIXME: add code for APMIXEDSYS */
	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	mtk_clk_register_dividers(apmixed_adj_divs, ARRAY_SIZE(apmixed_adj_divs),
		base, &mt8167_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

}
CLK_OF_DECLARE(mtk_apmixedsys, "mediatek,mt8167-apmixedsys",
		mtk_apmixedsys_init);

static void __init mtk_audiotop_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_AUD_NR_CLK);

	mtk_clk_register_gates(node, aud_clks, ARRAY_SIZE(aud_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

}
CLK_OF_DECLARE(mtk_audiotop, "mediatek,mt8167-audiotop", mtk_audiotop_init);

static void __init mtk_mfgcfg_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MFG_NR_CLK);

	mtk_clk_register_gates(node, mfg_clks, ARRAY_SIZE(mfg_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

}
CLK_OF_DECLARE(mtk_mfgcfg, "mediatek,mt8167-mfgcfg", mtk_mfgcfg_init);

static void __init mtk_mmsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

}
CLK_OF_DECLARE(mtk_mmsys, "mediatek,mt8167-mmsys", mtk_mmsys_init);

static void __init mtk_imgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_IMG_NR_CLK);

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

}
CLK_OF_DECLARE(mtk_imgsys, "mediatek,mt8167-imgsys", mtk_imgsys_init);

static void __init mtk_vdecsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_VDEC_NR_CLK);

	mtk_clk_register_gates(node, vdec_clks, ARRAY_SIZE(vdec_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

}
CLK_OF_DECLARE(mtk_vdecsys, "mediatek,mt8167-vdecsys", mtk_vdecsys_init);
