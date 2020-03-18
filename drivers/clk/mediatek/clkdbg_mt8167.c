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

#include <linux/clk-provider.h>
#include <linux/io.h>

#include "clkdbg.h"

#define ALL_CLK_ON		0
#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infracfg,
	scpsys,
	apmixed,
	audio,
	mfgsys,
	mmsys,
	imgsys,
	vdecsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen),
	[infracfg] = REGBASE_V(0x10001000, infracfg),
	[scpsys]   = REGBASE_V(0x10006000, scpsys),
	[apmixed]  = REGBASE_V(0x10018000, apmixed),
	[audio]    = REGBASE_V(0x11140000, audio),
	[mfgsys]   = REGBASE_V(0x13ffe000, mfgsys),
	[mmsys]    = REGBASE_V(0x14000000, mmsys),
	[imgsys]   = REGBASE_V(0x15000000, imgsys),
	[vdecsys]  = REGBASE_V(0x16000000, vdecsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen, 0x000, CLK_MUX_SEL0),
	REGNAME(topckgen, 0x004, CLK_MUX_SEL1),
	REGNAME(topckgen, 0x040, CLK_MUX_SEL8),
	REGNAME(topckgen, 0x044, CLK_SEL_9),
	REGNAME(topckgen, 0x07c, CLK_MUX_SEL13),
	REGNAME(topckgen, 0x020, CLK_GATING_CTRL0),
	REGNAME(topckgen, 0x050, CLK_GATING_CTRL0_SET),
	REGNAME(topckgen, 0x080, CLK_GATING_CTRL0_CLR),
	REGNAME(topckgen, 0x024, CLK_GATING_CTRL1),
	REGNAME(topckgen, 0x054, CLK_GATING_CTRL1_SET),
	REGNAME(topckgen, 0x084, CLK_GATING_CTRL1_CLR),
	REGNAME(topckgen, 0x03c, CLK_GATING_CTRL2),
	REGNAME(topckgen, 0x06c, CLK_GATING_CTRL2_SET),
	REGNAME(topckgen, 0x09c, CLK_GATING_CTRL2_CLR),
	REGNAME(topckgen, 0x070, CLK_GATING_CTRL8),
	REGNAME(topckgen, 0x0a0, CLK_GATING_CTRL8_SET),
	REGNAME(topckgen, 0x0b0, CLK_GATING_CTRL8_CLR),
	REGNAME(topckgen, 0x074, CLK_GATING_CTRL9),
	REGNAME(topckgen, 0x0a4, CLK_GATING_CTRL9_SET),
	REGNAME(topckgen, 0x0b4, CLK_GATING_CTRL9_CLR),
	REGNAME(infracfg, 0x080, INFRA_CLKSEL),
	REGNAME(scpsys, 0x210, SPM_VDE_PWR_CON),
	REGNAME(scpsys, 0x214, SPM_MFG_PWR_CON),
	REGNAME(scpsys, 0x238, SPM_ISP_PWR_CON),
	REGNAME(scpsys, 0x23c, SPM_DIS_PWR_CON),
	REGNAME(scpsys, 0x280, SPM_CONN_PWR_CON),
	REGNAME(scpsys, 0x2c0, SPM_MFG_2D_PWR_CON),
	REGNAME(scpsys, 0x2c4, SPM_MFG_ASYNC_PWR_CON),
	REGNAME(scpsys, 0x60c, SPM_PWR_STATUS),
	REGNAME(scpsys, 0x610, SPM_PWR_STATUS_2ND),
	REGNAME(apmixed, 0x004, AP_PLL_CON1),
	REGNAME(apmixed, 0x008, AP_PLL_CON2),
	REGNAME(apmixed, 0x014, PLL_HP_CON0),
	REGNAME(apmixed, 0x100, ARMPLL_CON0),
	REGNAME(apmixed, 0x104, ARMPLL_CON1),
	REGNAME(apmixed, 0x110, ARMPLL_PWR_CON0),
	REGNAME(apmixed, 0x120, MAINPLL_CON0),
	REGNAME(apmixed, 0x124, MAINPLL_CON1),
	REGNAME(apmixed, 0x130, MAINPLL_PWR_CON0),
	REGNAME(apmixed, 0x140, UNIVPLL_CON0),
	REGNAME(apmixed, 0x144, UNIVPLL_CON1),
	REGNAME(apmixed, 0x150, UNIVPLL_PWR_CON0),
	REGNAME(apmixed, 0x160, MMPLL_CON0),
	REGNAME(apmixed, 0x164, MMPLL_CON1),
	REGNAME(apmixed, 0x170, MMPLL_PWR_CON0),
	REGNAME(apmixed, 0x180, APLL1_CON0),
	REGNAME(apmixed, 0x184, APLL1_CON1),
	REGNAME(apmixed, 0x190, APLL1_PWR_CON0),
	REGNAME(apmixed, 0x1a0, APLL2_CON0),
	REGNAME(apmixed, 0x1a4, APLL2_CON1),
	REGNAME(apmixed, 0x1b0, APLL2_PWR_CON0),
	REGNAME(apmixed, 0x1c0, TVDPLL_CON0),
	REGNAME(apmixed, 0x1c4, TVDPLL_CON1),
	REGNAME(apmixed, 0x1d0, TVDPLL_PWR_CON0),
	REGNAME(apmixed, 0x1e0, LVDSPLL_CON0),
	REGNAME(apmixed, 0x1e4, LVDSPLL_CON1),
	REGNAME(apmixed, 0x1f0, LVDSPLL_PWR_CON0),
	REGNAME(audio, 0x000, AUDIO_TOP_CON0),
	REGNAME(mfgsys, 0x000, MFG_CG_CON),
	REGNAME(mfgsys, 0x004, MFG_CG_SET),
	REGNAME(mfgsys, 0x008, MFG_CG_CLR),
	REGNAME(mmsys, 0x100, MMSYS_CG_CON0),
	REGNAME(mmsys, 0x104, MMSYS_CG_SET0),
	REGNAME(mmsys, 0x108, MMSYS_CG_CLR0),
	REGNAME(mmsys, 0x110, MMSYS_CG_CON1),
	REGNAME(mmsys, 0x114, MMSYS_CG_SET1),
	REGNAME(mmsys, 0x118, MMSYS_CG_CLR1),
	REGNAME(imgsys, 0x000, IMG_CG_CON),
	REGNAME(imgsys, 0x004, IMG_CG_SET),
	REGNAME(imgsys, 0x008, IMG_CG_CLR),
	REGNAME(vdecsys, 0x000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x004, VDEC_CKEN_CLR),
	REGNAME(vdecsys, 0x008, LARB_CKEN_SET),
	REGNAME(vdecsys, 0x00c, LARB_CKEN_CLR),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_writel_mask(addr, h, l, v)	\
	clk_writel(addr, (clk_readl(addr) & ~GENMASK(h, l)) | ((v) << (l)))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,  1, "g_mainpll_d8_188m_ck"),
	FMCLK(CKGEN,  2, "g_mainpll_d11_137m_ck"),
	FMCLK(CKGEN,  3, "g_mainpll_d12_126m_ck"),
	FMCLK(CKGEN,  4, "g_mainpll_d20_76m_ck"),
	FMCLK(CKGEN,  5, "g_mainpll_d7_215m_ck"),
	FMCLK(CKGEN,  6, "g_univpll_d16_78m_ck"),
	FMCLK(CKGEN,  7, "g_univpll_d24_52m_ck"),
	FMCLK(CKGEN,  8, "csw_mux_nfi2x_ck"),
	FMCLK(ABIST, 11, "AD_SYS_26M_CK"),
	FMCLK(ABIST, 12, "AD_USB_F48M_CK"),
	FMCLK(CKGEN, 13, "csw_gfmux_emi1x_ck"),
	FMCLK(CKGEN, 14, "csw_gfmux_axibus_ck"),
	FMCLK(CKGEN, 15, "csw_mux_smi_ck"),
	FMCLK(CKGEN, 16, "csw_gfmux_uart0_ck"),
	FMCLK(CKGEN, 17, "csw_gfmux_uart1_ck"),
	FMCLK(CKGEN, 18, "csw_mux_mfg_ck"),
	FMCLK(CKGEN, 19, "csw_mux_msdc0_ck"),
	FMCLK(CKGEN, 20, "csw_mux_msdc1_ck"),
	FMCLK(ABIST, 21, "AD_DSI0_LNTC_DSICK_BUF"),
	FMCLK(ABIST, 22, "AD_MPPLL_TST_CK"),
	FMCLK(ABIST, 23, "AD_PLLGP_TST_CK"),
	FMCLK(CKGEN, 24, "csw_52m_ck"),
	FMCLK(CKGEN, 25, "mcusys_debug_mon0"),
	FMCLK(ABIST, 26, "csw_32k_ck"),
	FMCLK(ABIST, 27, "AD_MEMPLL_MONCLK"),
	FMCLK(ABIST, 28, "AD_MEMPLL2_MONCLK"),
	FMCLK(ABIST, 29, "AD_MEMPLL3_MONCLK"),
	FMCLK(ABIST, 30, "AD_MEMPLL4_MONCLK"),
	FMCLK(CKGEN, 32, "csw_mux_camtg_ck"),
	FMCLK(CKGEN, 33, "csw_mux_scam_ck"),
	FMCLK(CKGEN, 34, "csw_mux_pwm_mm_ck"),
	FMCLK(CKGEN, 35, "csw_mux_ddrphycfg_ck"),
	FMCLK(CKGEN, 36, "csw_mux_pmicspi_ck"),
	FMCLK(CKGEN, 37, "csw_mux_spi_ck"),
	FMCLK(CKGEN, 38, "csw_104m_ck"),
	FMCLK(CKGEN, 39, "csw_78m_ck"),
	FMCLK(CKGEN, 40, "csw_mux_spinor_ck"),
	FMCLK(CKGEN, 41, "csw_mux_eth_ck"),
	FMCLK(CKGEN, 42, "csw_mux_vdec_ck"),
	FMCLK(CKGEN, 43, "csw_mux_fdpi0_ck"),
	FMCLK(CKGEN, 44, "csw_mux_fdpi1_ck"),
	FMCLK(CKGEN, 45, "csw_mux_axi_mfg_ck"),
	FMCLK(CKGEN, 46, "csw_mux_slow_mfg_ck"),
	FMCLK(CKGEN, 47, "csw_mux_aud1_ck"),
	FMCLK(CKGEN, 48, "csw_mux_aud2_ck"),
	FMCLK(CKGEN, 49, "csw_mux_aud_engen1_ck"),
	FMCLK(CKGEN, 50, "csw_mux_aud_engen2_ck"),
	FMCLK(CKGEN, 51, "csw_mux_i2c_ck"),
	FMCLK(CKGEN, 52, "csw_mux_pwm_infra_ck"),
	FMCLK(CKGEN, 53, "csw_mux_aud_spdif_in_ck"),
	FMCLK(CKGEN, 54, "csw_gfmux_uart2_ck"),
	FMCLK(CKGEN, 55, "csw_mux_bsi_ck"),
	FMCLK(CKGEN, 56, "csw_mux_dbg_atclk_ck"),
	FMCLK(CKGEN, 57, "csw_mux_nfiecc_ck"),
	FMCLK(ABIST, 58, "AD_LVDSTX_MONCLK"),
	FMCLK(ABIST, 59, "AD_MONREF_CK"),
	FMCLK(ABIST, 60, "AD_MONFBK_CK"),
	FMCLK(ABIST, 61, "AD_USB20_CLK480M"),
	FMCLK(ABIST, 62, "AD_HDMITX_MONCLK"),
	FMCLK(ABIST, 63, "AD_HDMITX_CLKDIG_CTS"),
	FMCLK(ABIST, 64, "AD_ARMPLL_650M_CK"),
	FMCLK(ABIST, 65, "AD_MAINPLL_1501P5M_CK"),
	FMCLK(ABIST, 66, "AD_UNIVPLL_1248M_CK"),
	FMCLK(ABIST, 67, "AD_MMPLL_380M_CK"),
	FMCLK(ABIST, 68, "AD_TVDPLL_594M_CK"),
	FMCLK(ABIST, 69, "AD_AUD1PLL_180P6336M_CK"),
	FMCLK(ABIST, 70, "AD_AUD2PLL_196P608M_CK"),
	FMCLK(ABIST, 71, "AD_LVDSPLL_150M_CK"),
	FMCLK(ABIST, 72, "AD_USB20_48M_CK"),
	FMCLK(ABIST, 73, "AD_UNIV_48M_CK"),
	FMCLK(ABIST, 74, "AD_MMPLL_200M_CK"),
	FMCLK(ABIST, 75, "AD_MIPI_26M_CK"),
	FMCLK(ABIST, 76, "AD_SYS_26M_CK"),
	FMCLK(ABIST, 77, "AD_MEM_26M_CK"),
	FMCLK(ABIST, 78, "AD_MEMPLL5_MONCLK"),
	FMCLK(ABIST, 79, "AD_MEMPLL6_MONCLK"),
	{}
};

#define PLL_HP_CON0			(rb[apmixed].virt + 0x014)
#define PLL_TEST_CON1			(rb[apmixed].virt + 0x064)
#define TEST_DBG_CTRL			(rb[topckgen].virt + 0x38)
#define FREQ_MTR_CTRL_REG		(rb[topckgen].virt + 0x10)
#define FREQ_MTR_CTRL_RDATA		(rb[topckgen].virt + 0x14)

#define RG_FQMTR_CKDIV_GET(x)		(((x) >> 28) & 0x3)
#define RG_FQMTR_CKDIV_SET(x)		(((x) & 0x3) << 28)
#define RG_FQMTR_FIXCLK_SEL_GET(x)	(((x) >> 24) & 0x3)
#define RG_FQMTR_FIXCLK_SEL_SET(x)	(((x) & 0x3) << 24)
#define RG_FQMTR_MONCLK_SEL_GET(x)	(((x) >> 16) & 0x7f)
#define RG_FQMTR_MONCLK_SEL_SET(x)	(((x) & 0x7f) << 16)
#define RG_FQMTR_MONCLK_EN_GET(x)	(((x) >> 15) & 0x1)
#define RG_FQMTR_MONCLK_EN_SET(x)	(((x) & 0x1) << 15)
#define RG_FQMTR_MONCLK_RST_GET(x)	(((x) >> 14) & 0x1)
#define RG_FQMTR_MONCLK_RST_SET(x)	(((x) & 0x1) << 14)
#define RG_FQMTR_MONCLK_WINDOW_GET(x)	(((x) >> 0) & 0xfff)
#define RG_FQMTR_MONCLK_WINDOW_SET(x)	(((x) & 0xfff) << 0)

#define RG_FQMTR_CKDIV_DIV_2		0
#define RG_FQMTR_CKDIV_DIV_4		1
#define RG_FQMTR_CKDIV_DIV_8		2
#define RG_FQMTR_CKDIV_DIV_16		3

#define RG_FQMTR_FIXCLK_26MHZ		0
#define RG_FQMTR_FIXCLK_32KHZ		2

#define RG_FQMTR_EN     1
#define RG_FQMTR_RST    1

#define RG_FRMTR_WINDOW     519

static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 cnt = 0;

	/* reset & reset deassert */
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

	/* set window and target */
	clk_writel(FREQ_MTR_CTRL_REG,
		RG_FQMTR_MONCLK_WINDOW_SET(RG_FRMTR_WINDOW) |
		RG_FQMTR_MONCLK_SEL_SET(clk) |
		RG_FQMTR_FIXCLK_SEL_SET(RG_FQMTR_FIXCLK_26MHZ) |
		RG_FQMTR_MONCLK_EN_SET(RG_FQMTR_EN));

	udelay(30);

	cnt = clk_readl(FREQ_MTR_CTRL_RDATA);
	/* reset & reset deassert */
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

	return ((cnt * 26000) / (RG_FRMTR_WINDOW + 1));
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 last_freq = 0;
	u32 freq = fmeter_freq(type, k1, clk);
	u32 maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

struct bak {
	u32 pll_hp_con0;
	u32 pll_test_con1;
	u32 test_dbg_ctrl;
};

static void *prepare_fmeter(void)
{
	static struct bak regs;

	regs.pll_hp_con0 = clk_readl(PLL_HP_CON0);
	regs.pll_test_con1 = clk_readl(PLL_TEST_CON1);
	regs.test_dbg_ctrl = clk_readl(TEST_DBG_CTRL);

	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	clk_writel_mask(PLL_TEST_CON1, 9, 0, 0x287);
					/* AD_PLLGP_TST_CK = ARMPLL / 2 */
	clk_writel_mask(TEST_DBG_CTRL, 7, 0, 0);
					/* mcusys_debug_mon0 = ARMPLL */
	udelay(10);

	return &regs;
}

static void unprepare_fmeter(void *data)
{
	struct bak *regs = data;

	/* restore old setting */
	clk_writel(TEST_DBG_CTRL, regs->test_dbg_ctrl);
	clk_writel(PLL_TEST_CON1, regs->pll_test_con1);
	clk_writel(PLL_HP_CON0, regs->pll_hp_con0);
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type)
		return measure_stable_fmeter_freq(fclk->type, 0, fclk->id);

	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* plls */
		"armpll",
		"mainpll",
		"univpll",
		"mmpll",
		"apll1",
		"apll2",
		"tvdpll",
		"lvdspll",
		/* CLK_MUX_SEL0 */
		"uart0_sel",
		"gfmux_emi1x_sel",
		"emi_ddrphy_sel",
		"ahb_infra_sel",
		"csw_mux_mfg_sel",
		"msdc0_sel",
		"camtg_mm_sel",
		"pwm_mm_sel",
		"uart1_sel",
		"msdc1_sel",
		"spm_52m_sel",
		"pmicspi_sel",
		"qaxi_aud26m_sel",
		"aud_intbus_sel",
		/* CLK_MUX_SEL1 */
		"nfi2x_pad_sel",
		"nfi1x_pad_sel",
		"mfg_mm_sel",
		"ddrphycfg_sel",
		"smi_mm_sel",
		"usb_78m_sel",
		"scam_mm_sel",
		/* CLK_MUX_SEL8 */
		"spinor_sel",
		"msdc2_sel",
		"eth_sel",
		"vdec_mm_sel",
		"dpi0_mm_sel",
		"dpi1_mm_sel",
		"axi_mfg_in_sel",
		"slow_mfg_sel",
		"aud1_sel",
		"aud2_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"i2c_sel",
		/* CLK_SEL_9 */
		"aud_i2s0_m_sel",
		"aud_i2s1_m_sel",
		"aud_i2s2_m_sel",
		"aud_i2s3_m_sel",
		"aud_i2s4_m_sel",
		"aud_i2s5_m_sel",
		"aud_spdif_b_sel",
		/* CLK_MUX_SEL13 */
		"pwm_sel",
		"spi_sel",
		"aud_spdifin_sel",
		"uart2_sel",
		"bsi_sel",
		"dbg_atclk_sel",
		"csw_nfiecc_sel",
		"nfiecc_sel",
		/* ifr_muxes */
		"ifr_eth_25m_sel",
		"ifr_i2c0_sel",
		"ifr_i2c1_sel",
		"ifr_i2c2_sel",
		/* TOP0 */
		"pwm_mm",
		"cam_mm",
		"mfg_mm",
		"spm_52m",
		"mipi_26m_dbg",
		"scam_mm",
		"smi_mm",
		/* TOP1 */
		"them",
		"apdma",
		"i2c0",
		"i2c1",
		"auxadc1",
		"nfi",
		"nfiecc",
		"debugsys",
		"pwm",
		"uart0",
		"uart1",
		"btif",
		"usb",
		"flashif_26m",
		"auxadc2",
		"i2c2",
		"msdc0",
		"msdc1",
		"nfi2x",
		"pwrap_ap",
		"sej",
		"memslp_dlyer",
		"spi",
		"apxgpt",
		"audio",
		"pwrap_md",
		"pwrap_conn",
		"pwrap_26m",
		"aux_adc",
		"aux_tp",
		/* TOP2 */
		"msdc2",
		"rbist",
		"nfi_bus",
		"gce",
		"trng",
		"sej_13m",
		"aes",
		"pwm_b",
		"pwm1_fb",
		"pwm2_fb",
		"pwm3_fb",
		"pwm4_fb",
		"pwm5_fb",
		"usb_1p",
		"flashif_freerun",
		"hdmi_sifm_26m",
		"cec_26m",
		"cec_32k",
		"eth_66m",
		"eth_133m",
		"feth_25m",
		"feth_50m",
		"flashif_axi",
		"usbif",
		"uart2",
		"bsi",
		"gcpu_b",
		"msdc0_infra",
		"msdc1_infra",
		"msdc2_infra",
		"usb_78m",
		/* TOP3 */
		"rg_spinor",
		"rg_msdc2",
		"rg_eth",
		"rg_vdec",
		"rg_fdpi0",
		"rg_fdpi1",
		"rg_axi_mfg",
		"rg_slow_mfg",
		"rg_aud1",
		"rg_aud2",
		"rg_aud_engen1",
		"rg_aud_engen2",
		"rg_i2c",
		"rg_pwm_infra",
		"rg_aud_spdif_in",
		"rg_uart2",
		"rg_bsi",
		"rg_dbg_atclk",
		"rg_nfiecc",
		/* TOP4 */
		"rg_apll1_d2_en",
		"rg_apll1_d4_en",
		"rg_apll1_d8_en",
		"rg_apll2_d2_en",
		"rg_apll2_d4_en",
		"rg_apll2_d8_en",
		/* TOP5 */
		"apll12_div0",
		"apll12_div1",
		"apll12_div2",
		"apll12_div3",
		"apll12_div4",
		"apll12_div4b",
		"apll12_div5",
		"apll12_div5b",
		"apll12_div6",
		/* mtk_gate aud_clks */
		"aud_afe",
		"aud_i2s",
		"aud_22m",
		"aud_24m",
		"aud_intdir",
		"aud_apll2_tuner",
		"aud_apll_tuner",
		"aud_hdmi",
		"aud_spdf",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		/* mfgcfg_clks */
		"mfg_baxi",
		"mfg_bmem",
		"mfg_bg3d",
		"mfg_b26m",
		/* mm_clks */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_cam_mdp",
		"mm_mdp_rdma",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wdma",
		"mm_mdp_wrot",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma",
		"mm_disp_color",
		"mm_disp_ccorr",
		"mm_disp_aal",
		"mm_disp_gamma",
		"mm_disp_dither",
		"mm_disp_ufoe",
		"mm_disp_pwm_mm",
		"mm_disp_pwm_26m",
		"mm_dsi_engine",
		"mm_dsi_digital",
		"mm_dpi0_engine",
		"mm_dpi0_pxl",
		"mm_lvds_pxl",
		"mm_lvds_cts",
		"mm_dpi1_engine",
		"mm_dpi1_pxl",
		"mm_hdmi_pxl",
		"mm_hdmi_spdif",
		"mm_hdmi_adsp_b",
		"mm_hdmi_pll",
		/* img_clks */
		"img_larb1_smi",
		"img_cam_smi",
		"img_cam_cam",
		"img_sen_tg",
		"img_sen_cam",
		"img_venc",
		/* end */
		NULL
	};

	return clks;
}

/*
 * clkdbg pwr_status
 */

static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0]  = "",
		[1]  = "CONN",
		[2]  = "DDRPHY",
		[3]  = "DISP",
		[4]  = "MFG_3D",
		[5]  = "ISP",
		[6]  = "INFRA",
		[7]  = "VDEC",
		[8]  = "",
		[9]  = "MP0_CPUTOP",
		[10] = "MP0_CPU3",
		[11] = "MP0_CPU2",
		[12] = "MP0_CPU1",
		[13] = "MP0_CPU0",
		[14] = "",
		[15] = "MCUSYS",
		[16] = "",
		[17] = "",
		[18] = "",
		[19] = "",
		[20] = "",
		[21] = "",
		[22] = "",
		[23] = "",
		[24] = "MFG_2D (DUMMY)",
		[25] = "MFG_ASYNC",
		[26] = "MP0_CPUTOP_SRM_SLPB",
		[27] = "MP0_CPUTOP_SRM_PDN",
		[28] = "MP0_CPU3_SRM_PDN",
		[29] = "MP0_CPU2_SRM_PDN",
		[30] = "MP0_CPU1_SRM_PDN",
		[31] = "MP0_CPU0_SRM_PDN",
	};

	return pwr_names;
}

/*
 * clkdbg dump_clks
 */

void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
		{"mfgcfg",  BIT(25)},
		{"mmsys",   BIT(3)},
		{"imgsys",  BIT(5)},
		{"vdecsys", BIT(7)},
	};

	int i;
	const char *pvdname = pvdck->provider_name;

	if (!pvdname)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * chip_ver functions
 */

#include <linux/seq_file.h>
#include <mt-plat/mtk_chip.h>

static int clkdbg_chip_ver(struct seq_file *s, void *v)
{
	static const char * const sw_ver_name[] = {
		"CHIP_SW_VER_01",
		"CHIP_SW_VER_02",
		"CHIP_SW_VER_03",
		"CHIP_SW_VER_04",
	};

	enum chip_sw_ver ver = mt_get_chip_sw_ver();

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", ver, sw_ver_name[ver]);

	return 0;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt8167_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = prepare_fmeter,
	.unprepare_fmeter = unprepare_fmeter,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		CMDFN("chip_ver", clkdbg_chip_ver),
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt8167_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt8167"))
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt8167_ops);

#if ALL_CLK_ON
	prepare_enable_provider("topckgen");
	reg_pdrv("all");
	prepare_enable_provider("all");
#endif

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
device_initcall(clkdbg_mt8167_init);
