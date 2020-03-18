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

#include <linux/kernel.h>
#include <linux/clk.h>
#include <ddp_drv.h>
#include <ddp_pwm_mux.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <ddp_reg.h>

#define PWM_MSG(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)
#define PWM_ERR(fmt, arg...) pr_err("[PWM] " fmt "\n", ##arg)

/*****************************************************************************
 *
 * variable for get clock node fromdts
 *
*****************************************************************************/
static void __iomem *disp_pmw_mux_base;

#ifndef MUX_DISPPWM_ADDR /* disp pwm source clock select register address */
#define MUX_DISPPWM_ADDR (disp_pmw_mux_base + 0xB0)
#endif

/* clock hard code access API */
#define DRV_Reg32(addr) INREG32(addr)
#define clk_readl(addr) DRV_Reg32(addr)
#define clk_writel(addr, val) mt_reg_sync_writel(val, addr)

/*****************************************************************************
 *
 * disp pwm source clock select mux api
 *
*****************************************************************************/
#ifndef CONFIG_MTK_CLKMGR
enum eDDP_CLK_ID disp_pwm_get_clkid(unsigned int clk_req)
{
	enum eDDP_CLK_ID clkid = -1;

	switch (clk_req) {
	case 0:
		clkid = DISP_PWM_26M;
		break;
	case 1:
		clkid = UNIVPLL_D12;
		break;
	default:
		clkid = -1;
		break;
	}

	return clkid;
}
#endif
/*****************************************************************************
 *
 * get disp pwm source mux node
 *
*****************************************************************************/
#define DTSI_TOPCKGEN "mediatek,mt8167-topckgen"

int disp_pwm_get_muxbase(void)
{
	int ret = 0;
	struct device_node *node;

	if (disp_pmw_mux_base != NULL) {
		PWM_MSG("TOPCKGEN node exist");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, DTSI_TOPCKGEN);
	if (!node) {
		PWM_ERR("DISP find TOPCKGEN node failed\n");
		return -1;
	}
	disp_pmw_mux_base = of_iomap(node, 0);
	if (!disp_pmw_mux_base) {
		PWM_ERR("DISP TOPCKGEN base failed\n");
		return -1;
	}
	PWM_MSG("find TOPCKGEN node");
	return ret;
}

unsigned int disp_pwm_get_pwmmux(void)
{
	unsigned int regsrc = 0;
#ifdef CONFIG_MTK_CLKMGR /* MTK Clock Manager */
	regsrc = DISP_REG_GET(CLK_CFG_7);

#else /* Common Clock Framework */
	if (MUX_DISPPWM_ADDR != NULL)
		regsrc = clk_readl(MUX_DISPPWM_ADDR);
	else
		PWM_ERR("mux addr illegal");

#endif
	return regsrc;
}

/*****************************************************************************
 *
 * disp pwm source clock select mux api
 *
*****************************************************************************/
int disp_pwm_set_pwmmux(unsigned int clk_req)
{
	unsigned int regsrc;

#ifdef CONFIG_MTK_CLKMGR /* MTK Clock Manager */
	regsrc = disp_pwm_get_pwmmux();
	clkmux_sel(MT_MUX_DISPPWM, clk_req, "DISP_PWM");
	PWM_MSG("PWM_MUX %x->%x", regsrc, disp_pwm_get_pwmmux());

#else /* Common Clock Framework */
	int ret = 0;
	enum eDDP_CLK_ID clkid = -1;

	clkid = disp_pwm_get_clkid(clk_req);
	ret = disp_pwm_get_muxbase();
	regsrc = disp_pwm_get_pwmmux();

	PWM_MSG("clk_req=%d clkid=%d", clk_req, clkid);
	if (clkid != -1) {
		ddp_clk_enable(TOP_PWM_SEL);
		ddp_clk_set_parent(TOP_PWM_SEL, clkid);
		ddp_clk_disable(TOP_PWM_SEL);
	}

	PWM_MSG("PWM_MUX %x->%x", regsrc, disp_pwm_get_pwmmux());
#endif

	return 0;
}

int disp_pwm_clksource_enable(int clk_req)
{
	ddp_clk_prepare_enable(DISP_PWM_TOP);
	return 0;
}

int disp_pwm_clksource_disable(int clk_req)
{
	ddp_clk_disable(DISP_PWM_TOP);
	return 0;
}
