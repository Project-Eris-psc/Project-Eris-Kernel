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
#include <linux/syscore_ops.h>
#include <linux/version.h>

#define WARN_ON_CHECK_PLL_FAIL		0
#define AEE_EXCP_CHECK_PLL_FAIL		1
#define CLKDBG_CCF_API_4_4	1

#if AEE_EXCP_CHECK_PLL_FAIL
#include <mt-plat/aee.h>
#endif

#define TAG	"[clkchk] "

#define clk_warn(fmt, args...)	pr_warn(TAG fmt, ##args)

#if !CLKDBG_CCF_API_4_4

/* backward compatible */

static const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return __clk_get_name(hw->clk);
}

static bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return __clk_is_prepared(hw->clk);
}

static bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return __clk_is_enabled(hw->clk);
}

#endif /* !CLKDBG_CCF_API_4_4 */

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

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(void)
{
	const char * const *cn = get_all_clk_names();

	clk_warn("enabled clks:\n");

	for (; *cn; cn++) {
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || !c_hw)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (!p_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !__clk_get_enable_count(c))
			continue;

		clk_warn("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			__clk_get_enable_count(c),
			clk_hw_get_rate(c_hw),
			p_hw ? clk_hw_get_name(p_hw) : "- ");
	}
}

static void check_pll_off(void)
{
	static const char * const off_pll_names[] = {
		"univpll",
		"mmpll",
		"apll1",
		"apll2",
		"tvdpll",
		"lvdspll",
		NULL
	};

	static struct clk *off_plls[ARRAY_SIZE(off_pll_names)];

	struct clk **c;
	int invalid = 0;
	char buf[128] = {0};
	int n = 0;

	if (!off_plls[0]) {
		const char * const *pn;

		for (pn = off_pll_names, c = off_plls; *pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_plls; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		clk_warn("unexpected unclosed PLL: %s\n", buf);
		print_enabled_clks();

#if AEE_EXCP_CHECK_PLL_FAIL
		aee_kernel_exception("clkchk", "unclosed PLL: %s\n", buf);
#endif

#if WARN_ON_CHECK_PLL_FAIL
		WARN_ON(1);
#endif
	}
}

static int clkchk_syscore_suspend(void)
{
	check_pll_off();

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

static int __init clkchk_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt8167"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);
