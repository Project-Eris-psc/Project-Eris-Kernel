/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef _MT_PMIC_UPMU_SW_H_
#define _MT_PMIC_UPMU_SW_H_

#include <mach/upmu_hw.h>

#define BATTERY_DTS_SUPPORT
/*
 * Low battery level define
 */
enum LOW_BATTERY_LEVEL {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2
};

enum LOW_BATTERY_PRIO {
	LOW_BATTERY_PRIO_CPU_B      = 0,
	LOW_BATTERY_PRIO_CPU_L      = 1,
	LOW_BATTERY_PRIO_GPU        = 2,
	LOW_BATTERY_PRIO_MD         = 3,
	LOW_BATTERY_PRIO_MD5        = 4,
	LOW_BATTERY_PRIO_FLASHLIGHT = 5,
	LOW_BATTERY_PRIO_VIDEO      = 6,
	LOW_BATTERY_PRIO_WIFI       = 7,
	LOW_BATTERY_PRIO_BACKLIGHT  = 8
};

extern void (*low_battery_callback)(enum LOW_BATTERY_LEVEL);
extern void register_low_battery_notify(void (*low_battery_callback)(enum LOW_BATTERY_LEVEL),
		enum LOW_BATTERY_PRIO prio_val);


/*
 * Battery OC level define
 */
enum BATTERY_OC_LEVEL {
	BATTERY_OC_LEVEL_0 = 0,
	BATTERY_OC_LEVEL_1 = 1
};

enum BATTERY_OC_PRIO {
	BATTERY_OC_PRIO_CPU_B      = 0,
	BATTERY_OC_PRIO_CPU_L      = 1,
	BATTERY_OC_PRIO_GPU        = 2,
	BATTERY_OC_PRIO_FLASHLIGHT = 3
};

extern void (*battery_oc_callback)(enum BATTERY_OC_LEVEL);
extern void register_battery_oc_notify(void (*battery_oc_callback)(enum BATTERY_OC_LEVEL),
	enum BATTERY_OC_PRIO prio_val);

/*
 * Battery percent define
 */
enum BATTERY_PERCENT_LEVEL {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1
};

enum BATTERY_PERCENT_PRIO {
	BATTERY_PERCENT_PRIO_CPU_B      = 0,
	BATTERY_PERCENT_PRIO_CPU_L      = 1,
	BATTERY_PERCENT_PRIO_GPU        = 2,
	BATTERY_PERCENT_PRIO_MD         = 3,
	BATTERY_PERCENT_PRIO_MD5        = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO      = 6,
	BATTERY_PERCENT_PRIO_WIFI       = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT  = 8
};

extern void (*battery_percent_callback)(enum BATTERY_PERCENT_LEVEL);
#ifdef CONFIG_MTK_PMIC
extern void register_battery_percent_notify(void (*battery_percent_callback)(enum BATTERY_PERCENT_LEVEL),
	enum BATTERY_PERCENT_PRIO prio_val);

extern unsigned int pmic_config_interface(unsigned int regnum, unsigned int val, unsigned int mask,
	unsigned int shift);
extern unsigned int pmic_read_interface(unsigned int regnum, unsigned int *val, unsigned int mask, unsigned int shift);

extern void upmu_set_vcn35_on_ctrl_bt(unsigned int val);
extern void upmu_set_vcn35_on_ctrl_wifi(unsigned int val);
#else
void __attribute__((weak)) register_battery_percent_notify(void (*battery_percent_callback)(enum BATTERY_PERCENT_LEVEL),
	enum BATTERY_PERCENT_PRIO prio_val) {}

unsigned int __attribute__((weak)) pmic_config_interface(unsigned int regnum, unsigned int val, unsigned int mask,
	unsigned int shift) { return 0; }
unsigned int __attribute__((weak)) pmic_read_interface(unsigned int regnum, unsigned int *val, unsigned int mask,
			unsigned int shift) { return 0; }

void __attribute__((weak)) upmu_set_vcn35_on_ctrl_bt(unsigned int val) {}
void __attribute__((weak)) upmu_set_vcn35_on_ctrl_wifi(unsigned int val) {}
#endif

/* upmu interface */
static inline void upmu_set_rg_vcdt_hv_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON0),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_HV_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_HV_EN_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_chr_ldo_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON0),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_CHR_LDO_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_CHR_LDO_DET_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_pchr_automode(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON0),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_AUTOMODE_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_AUTOMODE_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON0),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_chr_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON0),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHR_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHR_EN_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON0),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_CHRDET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_CHRDET_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_rgs_vcdt_lv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON0),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_VCDT_LV_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_VCDT_LV_DET_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_rgs_vcdt_hv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON0),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_VCDT_HV_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_VCDT_HV_DET_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_vcdt_lv_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON1),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_LV_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_LV_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_vcdt_hv_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON1),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_HV_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_HV_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_cv_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON2),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CV_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CV_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_cc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON2),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CC_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CC_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_cs_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON2),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CS_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CS_EN_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_cs_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON2),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_CS_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_CS_DET_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_rgs_vbat_cv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON2),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_VBAT_CV_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_VBAT_CV_DET_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_rgs_vbat_cc_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON2),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_VBAT_CC_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_VBAT_CC_DET_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_vbat_cv_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON3),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CV_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CV_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_cc_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON3),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CC_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CC_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_cs_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON4),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CS_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CS_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_tohtc(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON5),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_TOHTC_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_TOHTC_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_toltc(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON5),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_TOLTC_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_TOLTC_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_ov_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON6),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_OV_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_OV_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_ov_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON6),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_OV_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_OV_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_ov_deg(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON6),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_OV_DEG_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_OV_DEG_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_vbat_ov_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON6),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_VBAT_OV_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_VBAT_OV_DET_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON7),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BATON_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BATON_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_baton_ht_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON7),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BATON_HT_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BATON_HT_EN_SHIFT)
	    );
}

static inline void upmu_set_baton_tdet_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON7),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_BATON_TDET_EN_MASK),
				    (unsigned int)(MT6392_PMIC_BATON_TDET_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_baton_ht_trim(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON7),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BATON_HT_TRIM_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BATON_HT_TRIM_SHIFT)
	    );
}

static inline void upmu_set_rg_baton_ht_trim_set(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON7),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BATON_HT_TRIM_SET_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BATON_HT_TRIM_SET_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON7),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_BATON_UNDET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_BATON_UNDET_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_csdac_data(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON8),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_DATA_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_DATA_SHIFT)
	    );
}

static inline void upmu_set_rg_frc_csvth_usbdl(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON9),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_FRC_CSVTH_USBDL_MASK),
				    (unsigned int)(MT6392_PMIC_RG_FRC_CSVTH_USBDL_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_pchr_flag_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON10),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_PCHR_FLAG_OUT_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_PCHR_FLAG_OUT_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_pchr_flag_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON10),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_FLAG_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_FLAG_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_otg_bvalid_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON10),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_OTG_BVALID_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_OTG_BVALID_EN_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_otg_bvalid_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON10),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_OTG_BVALID_DET_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_OTG_BVALID_DET_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_pchr_flag_sel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON11),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_FLAG_SEL_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_FLAG_SEL_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_testmode(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON12),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_TESTMODE_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_TESTMODE_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_testmode(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON12),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_TESTMODE_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_TESTMODE_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON12),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_RST_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_RST_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_ft_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON12),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_FT_CTRL_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_FT_CTRL_SHIFT)
	    );
}

static inline void upmu_set_rg_chrwdt_td(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON13),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_TD_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_TD_SHIFT)
	    );
}

static inline void upmu_set_rg_chrwdt_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON13),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_chrwdt_wr(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON13),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_WR_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_WR_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_rv(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON14),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_RV_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_RV_SHIFT)
	    );
}

static inline void upmu_set_rg_chrwdt_int_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON15),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_INT_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_INT_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_chrwdt_flag_wr(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON15),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_FLAG_WR_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRWDT_FLAG_WR_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_chrwdt_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON15),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_CHRWDT_OUT_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_CHRWDT_OUT_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_uvlo_vthl(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_UVLO_VTHL_MASK),
				    (unsigned int)(MT6392_PMIC_RG_UVLO_VTHL_SHIFT)
	    );
}

static inline void upmu_set_rg_usbdl_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_USBDL_RST_MASK),
				    (unsigned int)(MT6392_PMIC_RG_USBDL_RST_SHIFT)
	    );
}

static inline void upmu_set_rg_usbdl_set(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_USBDL_SET_MASK),
				    (unsigned int)(MT6392_PMIC_RG_USBDL_SET_SHIFT)
	    );
}

static inline void upmu_set_adcin_vsen_mux_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_ADCIN_VSEN_MUX_EN_MASK),
				    (unsigned int)(MT6392_PMIC_ADCIN_VSEN_MUX_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_adcin_vsen_ext_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_ADCIN_VSEN_EXT_BATON_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_ADCIN_VSEN_EXT_BATON_EN_SHIFT)
	    );
}

static inline void upmu_set_adcin_vbat_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_ADCIN_VBAT_EN_MASK),
				    (unsigned int)(MT6392_PMIC_ADCIN_VBAT_EN_SHIFT)
	    );
}

static inline void upmu_set_adcin_vsen_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_ADCIN_VSEN_EN_MASK),
				    (unsigned int)(MT6392_PMIC_ADCIN_VSEN_EN_SHIFT)
	    );
}

static inline void upmu_set_adcin_vchr_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON16),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_ADCIN_VCHR_EN_MASK),
				    (unsigned int)(MT6392_PMIC_ADCIN_VCHR_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_rsel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON17),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_RSEL_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_RSEL_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_unchop_ph(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON17),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_UNCHOP_PH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_UNCHOP_PH_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_unchop(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON17),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_UNCHOP_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_UNCHOP_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_bb_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_BB_CTRL_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_BB_CTRL_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_RST_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_RST_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_vsrc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_VSRC_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_VSRC_EN_SHIFT)
	    );
}

static inline unsigned int upmu_get_rgs_bc11_cmp_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CHR_CON18),
				  (&val),
				  (unsigned int)(MT6392_PMIC_RGS_BC11_CMP_OUT_MASK),
				  (unsigned int)(MT6392_PMIC_RGS_BC11_CMP_OUT_SHIFT)
	    );

	return val;
}

static inline void upmu_set_rg_dpdm_adcsw_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_DPDM_ADCSW_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_DPDM_ADCSW_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_dpdm_adcbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_DPDM_ADCBUF_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_DPDM_ADCBUF_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_buf_dp_in_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BUF_DP_IN_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BUF_DP_IN_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_buf_dm_in_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BUF_DM_IN_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BUF_DM_IN_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_vbat_cv_trim(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON18),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CV_TRIM_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VBAT_CV_TRIM_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_vref_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON19),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_VREF_VTH_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_VREF_VTH_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_cmp_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON19),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_CMP_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_CMP_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_ipd_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON19),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_IPD_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_IPD_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_ipu_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON19),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_IPU_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_IPU_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bc11_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON19),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BC11_BIAS_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BC11_BIAS_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_stp_inc(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON20),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_STP_INC_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_STP_INC_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_stp_dec(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON20),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_STP_DEC_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_STP_DEC_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_dly(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON21),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_DLY_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_DLY_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_stp(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON21),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_STP_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_STP_SHIFT)
	    );
}

static inline void upmu_set_rg_low_ich_db(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON22),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_LOW_ICH_DB_MASK),
				    (unsigned int)(MT6392_PMIC_RG_LOW_ICH_DB_SHIFT)
	    );
}

static inline void upmu_set_rg_chrind_on(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON22),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRIND_ON_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRIND_ON_SHIFT)
	    );
}

static inline void upmu_set_rg_chrind_dimming(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON22),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHRIND_DIMMING_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHRIND_DIMMING_SHIFT)
	    );
}

static inline void upmu_set_rg_cv_mode(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON23),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CV_MODE_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CV_MODE_SHIFT)
	    );
}

static inline void upmu_set_rg_vcdt_mode(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON23),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_MODE_MASK),
				    (unsigned int)(MT6392_PMIC_RG_VCDT_MODE_SHIFT)
	    );
}

static inline void upmu_set_rg_csdac_mode(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON23),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_MODE_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CSDAC_MODE_SHIFT)
	    );
}

static inline void upmu_set_rg_tracking_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON23),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_TRACKING_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_TRACKING_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_hwcv_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON23),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_HWCV_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_HWCV_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_ulc_det_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON23),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_ULC_DET_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_ULC_DET_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_trim_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON24),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TRIM_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TRIM_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_ichrg_trim(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON24),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_ICHRG_TRIM_MASK),
				    (unsigned int)(MT6392_PMIC_RG_ICHRG_TRIM_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_trim(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON25),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TRIM_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TRIM_SHIFT)
	    );
}

static inline void upmu_set_rg_ovp_trim(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON26),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_OVP_TRIM_MASK),
				    (unsigned int)(MT6392_PMIC_RG_OVP_TRIM_SHIFT)
	    );
}

static inline void upmu_set_rg_chr_osc_trim(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON27),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_CHR_OSC_TRIM_MASK),
				    (unsigned int)(MT6392_PMIC_RG_CHR_OSC_TRIM_SHIFT)
	    );
}

static inline void upmu_set_qi_bgr_ext_buf_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON27),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_QI_BGR_EXT_BUF_EN_MASK),
				    (unsigned int)(MT6392_PMIC_QI_BGR_EXT_BUF_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_test_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON27),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TEST_EN_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TEST_EN_SHIFT)
	    );
}

static inline void upmu_set_rg_bgr_test_rstb(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON27),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TEST_RSTB_MASK),
				    (unsigned int)(MT6392_PMIC_RG_BGR_TEST_RSTB_SHIFT)
	    );
}

static inline void upmu_set_rg_dac_usbdl_max(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON28),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_DAC_USBDL_MAX_MASK),
				    (unsigned int)(MT6392_PMIC_RG_DAC_USBDL_MAX_SHIFT)
	    );
}

static inline void upmu_set_rg_pchr_rsv(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(MT6392_CHR_CON29),
				    (unsigned int)(val),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_RSV_MASK),
				    (unsigned int)(MT6392_PMIC_RG_PCHR_RSV_SHIFT)
	    );
}

static inline unsigned int upmu_get_cid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_CID),
				  (&val),
				  (unsigned int)(MT6392_PMIC_CID_MASK),
				  (unsigned int)(MT6392_PMIC_CID_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_ni_vproc_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_VPROC_CON12),
				  (&val),
				  (unsigned int)(MT6392_PMIC_NI_VPROC_VOSEL_MASK),
				  (unsigned int)(MT6392_PMIC_NI_VPROC_VOSEL_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_auxadc_adc_out_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_AUXADC_ADC15),
				  (&val),
				  (unsigned int)(MT6392_PMIC_AUXADC_ADC_OUT_WAKEUP_PCHR_MASK),
				  (unsigned int)(MT6392_PMIC_AUXADC_ADC_OUT_WAKEUP_PCHR_SHIFT)
	    );

	return val;
}

static inline unsigned int upmu_get_auxadc_adc_out_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(MT6392_AUXADC_ADC16),
				  (&val),
				  (unsigned int)(MT6392_PMIC_AUXADC_ADC_OUT_WAKEUP_SWCHR_MASK),
				  (unsigned int)(MT6392_PMIC_AUXADC_ADC_OUT_WAKEUP_SWCHR_SHIFT)
	    );

	return val;
}

/* ADC Channel Number */
typedef enum {
	/* MT6397 */
	MT6323_AUX_BATON2 = 0x000,
	MT6397_AUX_CH6,
	MT6397_AUX_THR_SENSE2,
	MT6397_AUX_THR_SENSE1,
	MT6397_AUX_VCDT,
	MT6397_AUX_BATON1,
	MT6397_AUX_ISENSE,
	MT6397_AUX_BATSNS,
	MT6397_AUX_ACCDET,
	MT6397_AUX_AUDIO0,
	MT6397_AUX_AUDIO1,
	MT6397_AUX_AUDIO2,
	MT6397_AUX_AUDIO3,
	MT6397_AUX_AUDIO4,
	MT6397_AUX_AUDIO5,
	MT6397_AUX_AUDIO6,
	MT6397_AUX_AUDIO7,
} pmic_adc_ch_list_enum;

typedef enum MT65XX_POWER_TAG {

	/* MT6323 Digital LDO */
	MT6323_POWER_LDO_VIO28 = 0,
	MT6323_POWER_LDO_VUSB,
	MT6323_POWER_LDO_VMC,
	MT6323_POWER_LDO_VMCH,
	MT6323_POWER_LDO_VEMC_3V3,
	MT6323_POWER_LDO_VGP1,
	MT6323_POWER_LDO_VGP2,
	MT6323_POWER_LDO_VGP3,
	MT6323_POWER_LDO_VCN_1V8,
	MT6323_POWER_LDO_VSIM1,
	MT6323_POWER_LDO_VSIM2,
	MT6323_POWER_LDO_VRTC,
	MT6323_POWER_LDO_VCAM_AF,
	MT6323_POWER_LDO_VIBR,
	MT6323_POWER_LDO_VM,
	MT6323_POWER_LDO_VRF18,
	MT6323_POWER_LDO_VIO18,
	MT6323_POWER_LDO_VCAMD,
	MT6323_POWER_LDO_VCAM_IO,

	/* MT6323 Analog LDO */
	MT6323_POWER_LDO_VTCXO,
	MT6323_POWER_LDO_VA,
	MT6323_POWER_LDO_VCAMA,
	MT6323_POWER_LDO_VCN33_BT,
	MT6323_POWER_LDO_VCN33_WIFI,
	MT6323_POWER_LDO_VCN28,

	/* MT6320 Digital LDO */
	MT65XX_POWER_LDO_VIO28,
	MT65XX_POWER_LDO_VUSB,
	MT65XX_POWER_LDO_VMC,
	MT65XX_POWER_LDO_VMCH,
	MT65XX_POWER_LDO_VMC1,
	MT65XX_POWER_LDO_VMCH1,
	MT65XX_POWER_LDO_VEMC_3V3,
	MT65XX_POWER_LDO_VEMC_1V8,
	MT65XX_POWER_LDO_VCAMD,
	MT65XX_POWER_LDO_VCAMIO,
	MT65XX_POWER_LDO_VCAMAF,
	MT65XX_POWER_LDO_VGP1,
	MT65XX_POWER_LDO_VGP2,
	MT65XX_POWER_LDO_VGP3,
	MT65XX_POWER_LDO_VGP4,
	MT65XX_POWER_LDO_VGP5,
	MT65XX_POWER_LDO_VGP6,
	MT65XX_POWER_LDO_VSIM1,
	MT65XX_POWER_LDO_VSIM2,
	MT65XX_POWER_LDO_VIBR,
	MT65XX_POWER_LDO_VRTC,
	MT65XX_POWER_LDO_VAST,

	/* MT6320 Analog LDO */
	MT65XX_POWER_LDO_VRF28,
	MT65XX_POWER_LDO_VRF28_2,
	MT65XX_POWER_LDO_VTCXO,
	MT65XX_POWER_LDO_VTCXO_2,
	MT65XX_POWER_LDO_VA,
	MT65XX_POWER_LDO_VA28,
	MT65XX_POWER_LDO_VCAMA,

	MT6323_POWER_LDO_DEFAULT,
	MT65XX_POWER_LDO_DEFAULT,
	MT65XX_POWER_COUNT_END,
	MT65XX_POWER_NONE = -1
} MT65XX_POWER, MT_POWER;

#endif
