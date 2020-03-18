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

#include <kpd.h>
#include <mt-plat/aee.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6323/registers.h>
#include <linux/soc/mediatek/pmic_wrap.h>

static int kpd_show_hw_keycode = 1;
static int kpd_enable_lprst = 1;
static u16 kpd_keymap_state[KPD_NUM_MEMS] = {
	0xffff, 0xffff, 0xffff, 0xffff, 0x00ff
};

static bool kpd_sb_enable;

static struct regmap *pmic_regmap;

int kpd_init_pmic_regmap(struct platform_device *pdev)
{
	struct device_node *pnode;

	pnode = of_parse_phandle(pdev->dev.of_node, "mediatek,pwrap-regmap", 0);
	if (!pnode) {
		kpd_print("cannot get pnode...\n");
		return -EINVAL;
	}

#ifdef CONFIG_MTK_PMIC_WRAP
	pmic_regmap = pwrap_node_to_regmap(pnode);
	if (IS_ERR(pmic_regmap)) {
		kpd_print("cannot get pmic_regmap\n");
		return PTR_ERR(pmic_regmap);
	}
#endif

	return 0;
}

static void enable_kpd(int enable)
{
	if (enable == 1)
		mt_reg_sync_writew((u16) (enable), KP_EN);
	else if (enable == 0)
		mt_reg_sync_writew((u16) (enable), KP_EN);
}

void kpd_slide_qwerty_init(void)
{
/*for building pass, should remove in future.*/
}

void kpd_get_keymap_state(u16 state[])
{
	state[0] = *(volatile u16 *)KP_MEM1;
	state[1] = *(volatile u16 *)KP_MEM2;
	state[2] = *(volatile u16 *)KP_MEM3;
	state[3] = *(volatile u16 *)KP_MEM4;
	state[4] = *(volatile u16 *)KP_MEM5;
}

static void kpd_factory_mode_handler(void)
{
	int i, j;
	bool pressed;
	u16 new_state[KPD_NUM_MEMS], change, mask;
	u16 hw_keycode, linux_keycode;

	for (i = 0; i < KPD_NUM_MEMS - 1; i++)
		kpd_keymap_state[i] = 0xffff;
	if (!kpd_dts_data.kpd_use_extend_type)
		kpd_keymap_state[KPD_NUM_MEMS - 1] = 0x00ff;
	else
		kpd_keymap_state[KPD_NUM_MEMS - 1] = 0xffff;

	kpd_get_keymap_state(new_state);

	for (i = 0; i < KPD_NUM_MEMS; i++) {
		change = new_state[i] ^ kpd_keymap_state[i];
		if (!change)
			continue;

		for (j = 0; j < 16; j++) {
			mask = 1U << j;
			if (!(change & mask))
				continue;

			hw_keycode = (i << 4) + j;

			if (hw_keycode >= KPD_NUM_KEYS)
				continue;

			/* bit is 1: not pressed, 0: pressed */
			pressed = !(new_state[i] & mask);
			if (kpd_show_hw_keycode) {
				kpd_print(KPD_SAY "(%s) factory_mode HW keycode = %u\n",
				       pressed ? "pressed" : "released", hw_keycode);
			}
			linux_keycode = kpd_dts_data.kpd_hw_init_map[hw_keycode];
			if (unlikely(linux_keycode == 0)) {
				kpd_print("Linux keycode = 0\n");
				continue;
			}
			input_report_key(kpd_input_dev, linux_keycode, pressed);
			input_sync(kpd_input_dev);
			kpd_print("factory_mode report Linux keycode = %u\n", linux_keycode);
		}
	}

	memcpy(kpd_keymap_state, new_state, sizeof(new_state));
	kpd_print("save new keymap state\n");
}

/********************************************************************/
void kpd_auto_test_for_factorymode(void)
{
	kpd_info("Enter kpd_auto_test_for_factorymode!\n");

	mdelay(1000);

	kpd_factory_mode_handler();
	kpd_info("begin kpd_auto_test_for_factorymode!\n");
}

static inline void upmu_set_rg_pwrkey_rst_en(unsigned int val)
{
	#define PMIC_RG_PWRKEY_RST_EN_MASK 0x1
	#define PMIC_RG_PWRKEY_RST_EN_SHIFT 6

	if (!pmic_regmap)
		return;

	regmap_update_bits(pmic_regmap, MT6323_TOP_RST_MISC,
			PMIC_RG_PWRKEY_RST_EN_MASK << PMIC_RG_PWRKEY_RST_EN_SHIFT,
			val << PMIC_RG_PWRKEY_RST_EN_SHIFT);
}

static inline void upmu_set_rg_homekey_rst_en(unsigned int val)
{
	#define PMIC_RG_HOMEKEY_RST_EN_MASK	0x1
	#define PMIC_RG_HOMEKEY_RST_EN_SHIFT 5

	if (!pmic_regmap)
		return;

	regmap_update_bits(pmic_regmap, MT6323_TOP_RST_MISC,
			PMIC_RG_HOMEKEY_RST_EN_MASK << PMIC_RG_HOMEKEY_RST_EN_SHIFT,
			val << PMIC_RG_HOMEKEY_RST_EN_SHIFT);
}

static inline void upmu_set_rg_pwrkey_rst_td(unsigned int val)
{
	#define PMIC_RG_PWRKEY_RST_TD_MASK 0x3
	#define PMIC_RG_PWRKEY_RST_TD_SHIFT 8

	if (!pmic_regmap)
		return;

	regmap_update_bits(pmic_regmap, MT6323_TOP_RST_MISC,
			PMIC_RG_PWRKEY_RST_TD_MASK << PMIC_RG_PWRKEY_RST_TD_SHIFT,
			val << PMIC_RG_PWRKEY_RST_TD_SHIFT);
}

/********************************************************************/
void long_press_reboot_function_setting(void)
{
	if (kpd_enable_lprst && get_boot_mode() == NORMAL_BOOT) {
		kpd_info("Normal Boot long press reboot selection\n");
#ifdef CONFIG_KPD_PMIC_LPRST_TD
		kpd_info("Enable normal mode LPRST\n");
#ifdef CONFIG_ONEKEY_REBOOT_NORMAL_MODE
		upmu_set_rg_pwrkey_rst_en(0x01);
		upmu_set_rg_homekey_rst_en(0x00);
		upmu_set_rg_pwrkey_rst_td(CONFIG_KPD_PMIC_LPRST_TD);
#endif

#ifdef CONFIG_TWOKEY_REBOOT_NORMAL_MODE
		upmu_set_rg_pwrkey_rst_en(0x01);
		upmu_set_rg_homekey_rst_en(0x01);
		upmu_set_rg_pwrkey_rst_td(CONFIG_KPD_PMIC_LPRST_TD);
#endif
#else
		kpd_info("disable normal mode LPRST\n");
		upmu_set_rg_pwrkey_rst_en(0x00);
		upmu_set_rg_homekey_rst_en(0x00);

#endif
	} else {
		kpd_info("Other Boot Mode long press reboot selection\n");
#ifdef CONFIG_KPD_PMIC_LPRST_TD
		kpd_info("Enable other mode LPRST\n");
#ifdef CONFIG_ONEKEY_REBOOT_OTHER_MODE
		upmu_set_rg_pwrkey_rst_en(0x01);
		upmu_set_rg_homekey_rst_en(0x00);
		upmu_set_rg_pwrkey_rst_td(CONFIG_KPD_PMIC_LPRST_TD);
#endif

#ifdef CONFIG_TWOKEY_REBOOT_OTHER_MODE
		upmu_set_rg_pwrkey_rst_en(0x01);
		upmu_set_rg_homekey_rst_en(0x01);
		upmu_set_rg_pwrkey_rst_td(CONFIG_KPD_PMIC_LPRST_TD);
#endif
#else
		kpd_info("disable other mode LPRST\n");
		upmu_set_rg_pwrkey_rst_en(0x00);
		upmu_set_rg_homekey_rst_en(0x00);
#endif
	}
}

/********************************************************************/
void kpd_wakeup_src_setting(int enable)
{
	if (enable == 1) {
		kpd_print("enable kpd work!\n");
		enable_kpd(1);
	} else {
		kpd_print("disable kpd work!\n");
		enable_kpd(0);
	}
}

/********************************************************************/
void kpd_init_keymap(u16 keymap[])
{
	int i = 0;

	if (kpd_dts_data.kpd_use_extend_type)
		kpd_keymap_state[4] = 0xffff;
	for (i = 0; i < KPD_NUM_KEYS; i++) {
		keymap[i] = kpd_dts_data.kpd_hw_init_map[i];
		/*kpd_print(KPD_SAY "keymap[%d] = %d\n", i,keymap[i]);*/
	}
}

void kpd_init_keymap_state(u16 keymap_state[])
{
	int i = 0;

	for (i = 0; i < KPD_NUM_MEMS; i++)
		keymap_state[i] = kpd_keymap_state[i];
	kpd_info("init_keymap_state done: %x %x %x %x %x!\n", keymap_state[0], keymap_state[1], keymap_state[2],
		 keymap_state[3], keymap_state[4]);
}

/********************************************************************/

void kpd_set_debounce(u16 val)
{
	mt_reg_sync_writew((u16) (val & KPD_DEBOUNCE_MASK), KP_DEBOUNCE);
}

/********************************************************************/
void kpd_pmic_rstkey_hal(unsigned long pressed)
{
	if (kpd_dts_data.kpd_sw_rstkey != 0) {
		if (!kpd_sb_enable) {
			input_report_key(kpd_input_dev, kpd_dts_data.kpd_sw_rstkey, pressed);
			input_sync(kpd_input_dev);
			if (kpd_show_hw_keycode) {
				kpd_print(KPD_SAY "(%s) HW keycode =%d using PMIC\n",
				       pressed ? "pressed" : "released", kpd_dts_data.kpd_sw_rstkey);
			}
		}
	}
}

void kpd_pmic_pwrkey_hal(unsigned long pressed)
{
#ifdef CONFIG_KPD_PWRKEY_USE_PMIC
	if (!kpd_sb_enable) {
		input_report_key(kpd_input_dev, kpd_dts_data.kpd_sw_pwrkey, pressed);
		input_sync(kpd_input_dev);
		if (kpd_show_hw_keycode) {
			kpd_print(KPD_SAY "(%s) keycode =%d using PMIC\n",
			       pressed ? "pressed" : "released", kpd_dts_data.kpd_sw_pwrkey);
		}
	}
#endif
}
