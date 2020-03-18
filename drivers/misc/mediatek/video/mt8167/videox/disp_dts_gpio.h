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

#ifndef __DISP_DTS_GPIO_H__
#define __DISP_DTS_GPIO_H__

/*
 * This module helps you to set GPIO pin according to linux device tree (DTS). To
 * use this module, you MUST init this module once before any operation.
 */

#include <linux/platform_device.h> /* struct platform_device */

/* DTS state */
enum DTS_GPIO_STATE {
	DTS_GPIO_STATE_TE_MODE_GPIO = 0,    /* mode_te_gpio */
	DTS_GPIO_STATE_TE_MODE_TE,          /* mode_te_te */
	DTS_GPIO_STATE_PWM_TEST_PINMUX_55,  /* pwm_test_pin_mux_gpio55 */
	DTS_GPIO_STATE_PWM_TEST_PINMUX_69,  /* pwm_test_pin_mux_gpio69 */
	DTS_GPIO_STATE_PWM_TEST_PINMUX_129, /* pwm_test_pin_mux_gpio129 */

	DTS_GPIO_STATE_MAX,                 /* for array size */
};

/* this function MUST be called in mtkfb_probe.
 *  @param *pdev    - reference of struct platform_device which contains pinctrl
 *                    state information of GPIO
 *  @return         - 0 for OK, otherwise returns PTR_ERR(pdev).
 */
long    disp_dts_gpio_init(struct platform_device *pdev);

/* set gpio according sepcified DTS state.
 *  @notice         - to call this function, you MUST init this module first.
 *                    If not, we will trigger BUG_ON(0).
 *  @param s        - state which describes GPIO statement.
 *  @return         - 0 for OK, otherwise returns PTR_ERR(pdev).
 */
long    disp_dts_gpio_select_state(enum DTS_GPIO_STATE s);

/* repo of initialization */
#ifdef CONFIG_MTK_LEGACY
#define disp_dts_gpio_init_repo(x)  (0)
#else
#define disp_dts_gpio_init_repo(x)  (disp_dts_gpio_init(x))
#endif

#endif/*__DISP_DTS_GPIO_H__ */
