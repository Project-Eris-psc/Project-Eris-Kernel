/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6392/registers.h>

#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_chip.h>

#define PMIC6392_E1_CID_CODE 0x1092

#define MT6392_PMIC_RG_VAUD28_CAL_MASK		0xF
#define MT6392_PMIC_RG_VAUD28_CAL_SHIFT		8
#define MT6392_PMIC_RG_VAUD22_CAL_MASK		0xF
#define MT6392_PMIC_RG_VAUD22_CAL_SHIFT		8
#define MT6392_PMIC_RG_VCAMA_CAL_MASK		0xF
#define MT6392_PMIC_RG_VCAMA_CAL_SHIFT		8
#define MT6392_PMIC_RG_VCAMD_CAL_MASK		0xF
#define MT6392_PMIC_RG_VCAMD_CAL_SHIFT		8
#define MT6392_PMIC_RG_VMC_CAL_MASK		0xF
#define MT6392_PMIC_RG_VMC_CAL_SHIFT		9

struct regmap *pwrap_regmap;

static DEFINE_SPINLOCK(vcn35_on_ctrl_spinlock);
static int vcn35_on_ctrl_bt;
static int vcn35_on_ctrl_wifi;

unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_err("[Power/PMIC][pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);

	return return_value;
}

unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;
	unsigned int pmic_reg = 0;

	return_value = regmap_read(pwrap_regmap, RegNum, &pmic_reg);
	if (return_value != 0) {
		pr_err("[Power/PMIC][pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	return_value = regmap_write(pwrap_regmap, RegNum, pmic_reg);
	if (return_value != 0) {
		pr_err("[Power/PMIC][pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	return return_value;
}

u32 upmu_get_reg_value(u32 reg)
{
	u32 reg_val = 0;

	pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(u32 reg, u32 reg_val)
{
	pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

u32 g_reg_value;
static ssize_t show_pmic_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%04X\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char temp_buf[32];
	char *pvalue;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 5) {
			ret = kstrtouint(strsep(&pvalue, " "), 16, &reg_address);
			if (ret)
				return ret;
			ret = kstrtouint(pvalue, 16, &reg_value);
			if (ret)
				return ret;
			pr_notice("[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",
				  reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = kstrtouint(pvalue, 16, &reg_address);
			if (ret)
				return ret;
			ret = pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_notice("[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",
				  reg_address, g_reg_value);
			pr_notice
			    ("[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);	/* 664 */

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int ret = 0;

	/* put init setting from DE/SA */
	ret = pmic_config_interface(0xE, 0x1, 0x1, 2); /* [2:2]: BATON_TDET_EN; */
	ret = pmic_config_interface(0x3C, 0x1, 0x1, 5); /* [5:5]: THR_HWPDN_EN; */
	ret = pmic_config_interface(0x40, 0x1, 0x1, 2); /* [2:2]: RG_FCHR_PU_EN; */
	ret = pmic_config_interface(0x40, 0x1, 0x1, 4); /* [4:4]: RG_EN_DRVSEL; */
	ret = pmic_config_interface(0x40, 0x1, 0x1, 5); /* [5:5]: RG_RST_DRVSEL; */
	ret = pmic_config_interface(0x50, 0x1, 0x1, 1); /* [1:1]: STRUP_PWROFF_PREOFF_EN; */
	ret = pmic_config_interface(0x50, 0x1, 0x1, 0); /* [0:0]: STRUP_PWROFF_SEQ_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 0); /* [0:0]: VADC18_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 1); /* [1:1]: VCORE_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 2); /* [2:2]: VPROC_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 3); /* [3:3]: VSYS_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 4); /* [4:4]: VIO18_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 5); /* [5:5]: VIO28_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 6); /* [6:6]: VGP2_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 7); /* [7:7]: VEMC33_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 8); /* [8:8]: VM25_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 9); /* [9:9]: VM_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 10); /* [10:10]: VUSB_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 11); /* [11:11]: VMC_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 12); /* [12:12]: VMCH_PG_H2L_EN; */
	ret = pmic_config_interface(0x6E, 0x1, 0x1, 13); /* [13:13]: VXO22_PG_H2L_EN; */
	/* [1:1]: RG_CLKSQ_EN; disable clk26m in low power scenario */
	ret = pmic_config_interface(0x102, 0x1, 0x1, 1);
	ret = pmic_config_interface(0x102, 0x1, 0x1, 6); /* [6:6]: RG_RTC_75K_CK_PDN; */
	ret = pmic_config_interface(0x102, 0x1, 0x1, 11); /* [11:11]: RG_TRIM_75K_CK_PDN; */
	ret = pmic_config_interface(0x102, 0x1, 0x1, 15); /* [15:15]: RG_BUCK32K_PDN; */
	ret = pmic_config_interface(0x108, 0x1, 0x1, 9); /* [9:9]: RG_RTCDET_CK_PDN; */
	ret = pmic_config_interface(0x108, 0x1, 0x1, 12); /* [12:12]: RG_EFUSE_CK_PDN; */
	ret = pmic_config_interface(0x11A, 0x1, 0x1, 1); /* [1:1]: RG_SYSRSTB_EN; */
	ret = pmic_config_interface(0x11A, 0x1, 0x1, 5); /* [5:5]: RG_HOMEKEY_RST_EN; */
	ret = pmic_config_interface(0x120, 0x1, 0x1, 4); /* [4:4]: RG_SRCLKEN_HW_MODE; */
	ret = pmic_config_interface(0x120, 0x1, 0x1, 5); /* [5:5]: RG_OSC_HW_MODE; */
	ret = pmic_config_interface(0x136, 0x1, 0x1, 0); /* [0:0]: RG_WDTRSTB_DEB; */
	ret = pmic_config_interface(0x148, 0x1, 0x1, 0); /* [0:0]: RG_SMT_SYSRSTB; */
	ret = pmic_config_interface(0x148, 0x1, 0x1, 1); /* [1:1]: RG_SMT_INT; */
	ret = pmic_config_interface(0x148, 0x1, 0x1, 2); /* [2:2]: RG_SMT_SRCLKEN; */
	ret = pmic_config_interface(0x148, 0x1, 0x1, 3); /* [3:3]: RG_SMT_RTC_32K1V8; */
	ret = pmic_config_interface(0x14A, 0x1, 0x1, 0); /* [0:0]: RG_SMT_SPI_CLK; SPI SMT to anti noise */
	ret = pmic_config_interface(0x14A, 0x1, 0x1, 1); /* [1:1]: RG_SMT_SPI_CSN; SPI SMT to anti noise */
	ret = pmic_config_interface(0x14A, 0x1, 0x1, 2); /* [2:2]: RG_SMT_SPI_MOSI; SPI SMT to anti noise */
	ret = pmic_config_interface(0x14A, 0x1, 0x1, 3); /* [3:3]: RG_SMT_SPI_MISO; SPI SMT to anti noise */
	ret = pmic_config_interface(0x154, 0xE, 0xF, 0); /* [3:0]: RG_OCTL_SPI_CLK; SPI test */
	ret = pmic_config_interface(0x154, 0xE, 0xF, 4); /* [7:4]: RG_OCTL_SPI_CSN; SPI test */
	ret = pmic_config_interface(0x154, 0xE, 0xF, 8); /* [11:8]: RG_OCTL_SPI_MOSI; SPI test */
	ret = pmic_config_interface(0x154, 0xE, 0xF, 12); /* [15:12]: RG_OCTL_SPI_MISO; SPI test */
	/* [1:0]: RG_VPROC_RZSEL; compensation change for load transient improvement */
	ret = pmic_config_interface(0x20E, 0x0, 0x3, 0);
	/* [7:6]: RG_VPROC_CSR; compensation change for load transient improvement */
	ret = pmic_config_interface(0x20E, 0x3, 0x3, 6);
	/* [1:0]: RG_VPROC_SLP; compensation change for load transient improvement */
	ret = pmic_config_interface(0x212, 0x2, 0x3, 0);
	ret = pmic_config_interface(0x212, 0x2, 0x3, 4); /* [5:4]: QI_VPROC_VSLEEP; Sleep */
	ret = pmic_config_interface(0x216, 0x1, 0x1, 1); /* [1:1]: VPROC_VOSEL_CTRL; */
	ret = pmic_config_interface(0x21C, 0x1, 0x1, 7); /* [7:7]: VPROC_SFCHG_FEN; DVFS slew rate control */
	ret = pmic_config_interface(0x21C, 0x1, 0x1, 15); /* [15:15]: VPROC_SFCHG_REN; DVFS slew rate control */
	ret = pmic_config_interface(0x222, 0x18, 0x7F, 0); /* [6:0]: VPROC_VOSEL_SLEEP; */
	/* [1:0]: VPROC_BURST; compensation change for load transient improvement */
	ret = pmic_config_interface(0x226, 0x3, 0x3, 0);
	ret = pmic_config_interface(0x230, 0x3, 0x3, 0); /* [1:0]: VPROC_TRANSTD; DVFS FPWM period */
	ret = pmic_config_interface(0x230, 0x3, 0x3, 4); /* [5:4]: VPROC_VOSEL_TRANS_EN; DVFS FPWM edge */
	ret = pmic_config_interface(0x230, 0x1, 0x1, 8); /* [8:8]: VPROC_VSLEEP_EN; */
	ret = pmic_config_interface(0x23C, 0x1, 0x1, 1); /* [1:1]: VSYS_VOSEL_CTRL; */
	ret = pmic_config_interface(0x256, 0x1, 0x1, 8); /* [8:8]: VSYS_VSLEEP_EN; */
	/* [1:0]: RG_VCORE_RZSEL; compensation change for load transient improvement */

	/*BUCK OC debounce time to 32us*/
	ret = pmic_config_interface(0x262, 0x3, 0x3, 2); /* [3:2]: BUCK_VPROC_OC_WND; */
	ret = pmic_config_interface(0x264, 0x3, 0x3, 2); /* [3:2]: BUCK_VCORE_OC_WND; */
	ret = pmic_config_interface(0x266, 0x3, 0x3, 2); /* [3:2]: BUCK_VSYS_OC_WND; */

	ret = pmic_config_interface(0x302, 0x0, 0x3, 0);
	/* [7:6]: RG_VCORE_CSR; compensation change for load transient improvement */
	ret = pmic_config_interface(0x302, 0x3, 0x3, 6);
	/* [1:0]: RG_VCORE_SLP; compensation change for load transient improvement */
	ret = pmic_config_interface(0x306, 0x2, 0x3, 0);
	ret = pmic_config_interface(0x306, 0x2, 0x3, 4); /* [5:4]: QI_VCORE_VSLEEP; Sleep */
	ret = pmic_config_interface(0x30A, 0x1, 0x1, 1); /* [1:1]: VCORE_VOSEL_CTRL; */
	ret = pmic_config_interface(0x310, 0x1, 0x1, 7); /* [7:7]: VCORE_SFCHG_FEN; DVFS slew rate control */
	ret = pmic_config_interface(0x310, 0x1, 0x1, 15); /* [15:15]: VCORE_SFCHG_REN; DVFS slew rate control */
	ret = pmic_config_interface(0x316, 0x18, 0x7F, 0); /* [6:0]: VCORE_VOSEL_SLEEP; */
	/* [1:0]: VCORE_BURST; compensation change for load transient improvement */
	ret = pmic_config_interface(0x31A, 0x3, 0x3, 0);
	ret = pmic_config_interface(0x324, 0x3, 0x3, 0); /* [1:0]: VCORE_TRANSTD; DVFS FPWM period */
	ret = pmic_config_interface(0x324, 0x3, 0x3, 4); /* [5:4]: VCORE_VOSEL_TRANS_EN; DVFS FPWM edge */
	ret = pmic_config_interface(0x324, 0x1, 0x1, 8); /* [8:8]: VCORE_VSLEEP_EN; */
	ret = pmic_config_interface(0x738, 0x0, 0x1, 15); /* [15:15]: AUXADC_CK_AON; */
	ret = pmic_config_interface(0x75C, 0x0, 0x1, 14); /* [14:14]: AUXADC_START_SHADE_EN; */
	if (mt_get_chip_sw_ver())
		ret = pmic_config_interface(0x216, 0x1, 0x1, 0); /* [1:1]: VPROC_EN_CTRL; */

	pmic_config_interface(0x21E, 0x50, 0x7F, 0); /* set Vproc to 1.2 for cpu freq 1.0GHz */
	pmic_config_interface(0x220, 0x50, 0x7F, 0); /* set Vproc to 1.2 for cpu freq 1.0GHz */
#ifdef CONFIG_PCDDR3
	pmic_config_interface(0x0554, 0x01, 0x3, 4); /* set vmem to 1.35v */
#endif

	ret = pmic_config_interface(0x052C, 0x1, 0x1, 1); /* [1:1]: VMCH_ON_CTRL; */
}

static void pmic_low_power_setting(void)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(0x4E, 0x1, 0x1, 5); /* [5:5]: STRUP_AUXADC_RSTB_SW; */
	ret = pmic_config_interface(0x4E, 0x1, 0x1, 7); /* [7:7]: STRUP_AUXADC_RSTB_SEL; */
	ret = pmic_config_interface(0x404, 0x1, 0x1, 0); /* [0:0]: VAUD22_LP_SEL; */
	ret = pmic_config_interface(0x424, 0x1, 0x1, 0); /* [0:0]: VAUD28_LP_SEL; */
	ret = pmic_config_interface(0x428, 0x1, 0x1, 0); /* [0:0]: VADC18_LP_SEL; */
	ret = pmic_config_interface(0x500, 0x1, 0x1, 0); /* [0:0]: VIO28_LP_SEL; */
	ret = pmic_config_interface(0x502, 0x1, 0x1, 0); /* [0:0]: VUSB_LP_SEL; */
	ret = pmic_config_interface(0x552, 0x1, 0x1, 0); /* [0:0]: VM_LP_SEL; */
	ret = pmic_config_interface(0x556, 0x1, 0x1, 0); /* [0:0]: VIO18_LP_SEL; */
	ret = pmic_config_interface(0x562, 0x1, 0x1, 0); /* [0:0]: VM25_LP_SEL; */
	ret = pmic_config_interface(0x738, 0x0, 0x1, 15); /* [15:15]: AUXADC_CK_AON; */
	ret = pmic_config_interface(0x75C, 0x0, 0x1, 14); /* [14:14]: AUXADC_START_SHADE_EN */
	ret = pmic_config_interface(0x102, 0x0, 0x1, 1); /* [1:1]: RG_CLKSQ_EN; */

#ifndef CONFIG_USB_C_SWITCH_MT6392
	ret = pmic_config_interface(0x10E, 0x1, 0x1, 8); /* [8:8]: RG_TYPE_C_CSR_CK_PDN; */
	ret = pmic_config_interface(0x10E, 0x1, 0x1, 9); /* [9:9]: RG_TYPE_C_CC_CK_PDN; */
#endif

	pr_notice("[Power/PMIC] low power setting done...\n");
}

static void pmic_buck_oc_enable(void)
{
	int reg_val = 0;

	pmic_read_interface(MT6392_BUCK_OC_CON0, &reg_val, 0xFFFF, 0);
	pr_notice("[pmic_buck_oc_enable] MT6392_BUCK_OC_CON0 0x%x = 0x%x\n", MT6392_BUCK_OC_CON0, reg_val);

	pmic_config_interface(MT6392_BUCK_OC_CON3, 0x7, 0x7, 0);
	pmic_config_interface(MT6392_BUCK_OC_CON4, 0x7, 0x7, 0);
	pmic_config_interface(MT6392_BUCK_OC_CON0, 0x7, 0x7, 0);

	pmic_read_interface(MT6392_BUCK_OC_CON0, &reg_val, 0xFFFF, 0);
	pr_notice("[pmic_buck_oc_enable] Clear Buck OC Flag, MT6392_BUCK_OC_CON0 0x%x = 0x%x\n",
		MT6392_BUCK_OC_CON0, reg_val);

	pmic_config_interface(MT6392_BUCK_OC_CON3, 0x0, 0x7, 0);
	pmic_config_interface(MT6392_BUCK_EFUSE_OC_CON0, 0x1, 0x1, 1);
	pmic_config_interface(MT6392_STRUP_CON16, 0x0, 0x7, 0);
	pr_notice("[pmic_buck_oc_enable] Enable Buck OC shutdown function...\n");
}

static void pmic_ldo_cali_sw_bonding(void)
{
	int reg_val = 0;
	u32 cid = upmu_get_cid();

	/* RG_VAUD28_CAL: 244~247 */
	pmic_read_interface(MT6392_EFUSE_DOUT_240_255, &reg_val, 0xF, 4);
	pmic_config_interface(MT6392_ANALDO_CON22, reg_val,
		MT6392_PMIC_RG_VAUD28_CAL_MASK, MT6392_PMIC_RG_VAUD28_CAL_SHIFT);

	/* RG_VAUD22_CAL: 248~251 */
	pmic_read_interface(MT6392_EFUSE_DOUT_240_255, &reg_val, 0xF, 8);
	pmic_config_interface(MT6392_ANALDO_CON8, reg_val,
		MT6392_PMIC_RG_VAUD22_CAL_MASK, MT6392_PMIC_RG_VAUD22_CAL_SHIFT);

	/* RG_VCAMA_CAL: 252~255 */
	pmic_read_interface(MT6392_EFUSE_DOUT_240_255, &reg_val, 0xF, 12);
	pmic_config_interface(MT6392_ANALDO_CON10, reg_val,
		MT6392_PMIC_RG_VCAMA_CAL_MASK, MT6392_PMIC_RG_VCAMA_CAL_SHIFT);

	/* RG_VCAMD_CAL: 153~156 */
	pmic_read_interface(MT6392_EFUSE_DOUT_144_159, &reg_val, 0xF, 12);
	pmic_config_interface(MT6392_DIGLDO_CON52, reg_val,
		MT6392_PMIC_RG_VCAMD_CAL_MASK, MT6392_PMIC_RG_VCAMD_CAL_SHIFT);

	if (cid == 0x1092) {
		/* RG_VMC_CAL: 256~259 */
		pmic_read_interface(MT6392_EFUSE_DOUT_256_271, &reg_val, 0xF, 0);
		pmic_config_interface(MT6392_DIGLDO_CON24, reg_val,
			MT6392_PMIC_RG_VMC_CAL_MASK, MT6392_PMIC_RG_VMC_CAL_SHIFT);
	}

	pr_notice("[pmic_ldo_cali_sw_bonding] Done...................\n");
}

void upmu_set_vcn35_on_ctrl_bt(unsigned int val)
{
	unsigned int ret, cid;
	u32 vcn35_on_ctrl;

	cid = upmu_get_cid();
	if (cid == PMIC6392_E1_CID_CODE) {
		pr_debug("%s: MT6392 PMIC CID=0x%x\n", __func__, cid);

		spin_lock(&vcn35_on_ctrl_spinlock);

		if (val)
			vcn35_on_ctrl_bt = 1;
		else
			vcn35_on_ctrl_bt = 0;

		if (vcn35_on_ctrl_bt || vcn35_on_ctrl_wifi)
			vcn35_on_ctrl = 0x1;
		else
			vcn35_on_ctrl = 0x0;

		ret = pmic_config_interface((unsigned int)(MT6392_DIGLDO_CON61),
				    (unsigned int)(vcn35_on_ctrl),
				    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_MASK),
				    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_SHIFT)
			);

		spin_unlock(&vcn35_on_ctrl_spinlock);
	} else {
		pr_debug("%s: MT6392 PMIC CID=0x%x\n",  __func__, cid);
		ret = pmic_config_interface((unsigned int)(MT6392_ANALDO_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_BT_MASK),
				    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_BT_SHIFT)
			);
	}
}

void upmu_set_vcn35_on_ctrl_wifi(unsigned int val)
{
	unsigned int ret, cid;
	u32 vcn35_on_ctrl;

	cid = upmu_get_cid();
	if (cid == PMIC6392_E1_CID_CODE) {
		pr_debug("%s: MT6392 PMIC CID=0x%x\n", __func__, cid);

		spin_lock(&vcn35_on_ctrl_spinlock);

		if (val)
			vcn35_on_ctrl_wifi = 1;
		else
			vcn35_on_ctrl_wifi = 0;

		if (vcn35_on_ctrl_bt || vcn35_on_ctrl_wifi)
			vcn35_on_ctrl = 0x1;
		else
			vcn35_on_ctrl = 0x0;

		ret = pmic_config_interface((unsigned int)(MT6392_DIGLDO_CON61),
				    (unsigned int)(vcn35_on_ctrl),
				    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_MASK),
				    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_SHIFT)
			);

		spin_unlock(&vcn35_on_ctrl_spinlock);
	} else {
		pr_debug("%s: MT6392 PMIC CID=0x%x\n", __func__, cid);
		ret = pmic_config_interface((unsigned int)(MT6392_ANALDO_CON17),
					    (unsigned int)(val),
					    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_WIFI_MASK),
					    (unsigned int)(MT6392_PMIC_VCN35_ON_CTRL_WIFI_SHIFT)
		    );
	}
}

static int mt6392_pmic_probe(struct platform_device *dev)
{
	int ret_val = 0;
	int reg_val = 0;
	struct mt6397_chip *mt6392_chip = dev_get_drvdata(dev->dev.parent);

	pr_debug("[Power/PMIC] ******** MT6392 pmic driver probe!! ********\n");

	pwrap_regmap = mt6392_chip->regmap;

	/* get PMIC CID */
	ret_val = upmu_get_cid();
	pr_notice("[Power/PMIC] MT6392 PMIC CID=0x%x\n", ret_val);

	ret_val = pmic_read_interface(MT6392_STRUP_CON18, (&reg_val), 0xFFFF, 0);
	pr_notice("[Power/PMIC] MT6392 Debug Status: Reg[0x%x]=0x%x\n", MT6392_STRUP_CON18, reg_val);
	ret_val = pmic_read_interface(MT6392_STRUP_CON19, (&reg_val), 0xFFFF, 0);
	pr_notice("[Power/PMIC] MT6392 Debug Status: Reg[0x%x]=0x%x\n", MT6392_STRUP_CON19, reg_val);

	/* clear power off status */
	ret_val = pmic_config_interface(MT6392_STRUP_CON17, 0x1, 0x1, 0);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();

	/* pmic low power setting */
	pmic_low_power_setting();

	/* Enable PMIC RST function (depends on main chip RST function) */
	ret_val = pmic_config_interface(0x011A, 0x1, 0x1, 1);
	ret_val = pmic_config_interface(0x011A, 0x1, 0x1, 3);
	ret_val = pmic_config_interface(0x0136, 0x1, 0x1, 0);
	ret_val = pmic_read_interface(0x011A, (&reg_val), 0xFFFF, 0);
	pr_notice("[Power/PMIC] MT6392 Reg[0x%x]=0x%x\n", 0x011A, reg_val);
	ret_val = pmic_read_interface(0x0136, (&reg_val), 0xFFFF, 0);
	pr_notice("[Power/PMIC] MT6392 Reg[0x%x]=0x%x\n", 0x0136, reg_val);

	/* Clear Buck OC flag and enable function */
	pmic_buck_oc_enable();

	/* LDO efuse bits SW bonding */
	pmic_ldo_cali_sw_bonding();

	/* Configure long press pwrkey shutdown */
	ret_val = pmic_config_interface(MT6392_STRUP_CON13, 0x2, 0x3, 0);
	ret_val = pmic_config_interface(MT6392_STRUP_CON13, 0x1, 0x1, 4);
	ret_val = pmic_config_interface(MT6392_STRUP_CON13, 0x1, 0x1, 6);
	ret_val = pmic_read_interface(MT6392_STRUP_CON13, (&reg_val), 0xFFFF, 0);
	pr_notice("[Power/PMIC] MT6392 Reg[0x%x]=0x%x\n", MT6392_STRUP_CON13, reg_val);

	device_create_file(&(dev->dev), &dev_attr_pmic_access);

	pr_notice("[Power/PMIC] MT6392 Probe Done...................\n");

	return 0;
}

static int mt6392_suspend(struct device *dev)
{
	unsigned int ret = 0;

	/* disable vproc oc interrupt issue*/
	ret = pmic_config_interface(0x25A, 0x0, 0x1, 2); /* [2:2]: BUCK_VPROC_OC_INT_EN; */

	return 0;
}

static int mt6392_resume(struct device *dev)
{
	unsigned int ret = 0;

	/* disable vproc oc interrupt issue*/
	ret = pmic_config_interface(0x25A, 0x1, 0x1, 2); /* [2:2]: BUCK_VPROC_OC_INT_EN; */

	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6392_pm_ops, mt6392_suspend,
			mt6392_resume);


static const struct platform_device_id mt6392_pmic_ids[] = {
	{"mt6392-pmic", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6392_pmic_ids);

static const struct of_device_id mt6392_pmic_of_match[] = {
	{ .compatible = "mediatek,mt6392-pmic", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mt6392_pmic_of_match);

static struct platform_driver mt6392_pmic_driver = {
	.driver = {
		.name = "mt6392-pmic",
		.of_match_table = of_match_ptr(mt6392_pmic_of_match),
		.pm = &mt6392_pm_ops,
	},
	.probe = mt6392_pmic_probe,
	.id_table = mt6392_pmic_ids,
};

module_platform_driver(mt6392_pmic_driver);

MODULE_AUTHOR("Chen Zhong <chen.zhong@mediatek.com>");
MODULE_DESCRIPTION("PMIC Misc Setting Driver for MediaTek MT6392 PMIC");
MODULE_LICENSE("GPL v2");
