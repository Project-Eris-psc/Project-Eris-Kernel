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
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <mt-plat/mtk_boot_common.h>
#include "ccci_config.h"
#include "ccci_modem.h"
#include "ccci_swtp.h"

#ifdef FEATURE_MTK_SWITCH_TX_POWER
const struct of_device_id swtp_of_match[] = {
	{ .compatible = SWTP_COMPATIBLE_DEVICE_ID, },
	{},
};
#define SWTP_MAX_SUPPORT_MD 1
struct swtp_t swtp_data[SWTP_MAX_SUPPORT_MD];


static int swtp_switch_mode(struct swtp_t *swtp)
{
	unsigned long flags;
	int ret = 0;

	if (swtp == NULL) {
		CCCI_LEGACY_ERR_LOG(-1, KERN, "%s data is null\n", __func__);
		return -1;
	}

	spin_lock_irqsave(&swtp->spinlock, flags);
	if (swtp->curr_mode == SWTP_EINT_PIN_PLUG_IN) {
		if (swtp->eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(swtp->irq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(swtp->irq, IRQ_TYPE_LEVEL_LOW);

		swtp->curr_mode = SWTP_EINT_PIN_PLUG_OUT;
	} else {
		if (swtp->eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(swtp->irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(swtp->irq, IRQ_TYPE_LEVEL_HIGH);
		swtp->curr_mode = SWTP_EINT_PIN_PLUG_IN;
	}
	CCCI_LEGACY_ALWAYS_LOG(swtp->md_id, KERN, "%s mode %d\n", __func__, swtp->curr_mode);
	spin_unlock_irqrestore(&swtp->spinlock, flags);

	return ret;
}

static int swtp_send_tx_power_mode(struct swtp_t *swtp)
{
	unsigned long flags;
	unsigned int md_state;
	int ret = 0;

	md_state = ccci_md_get_state_by_id(swtp->md_id);
	if (md_state != BOOT_WAITING_FOR_HS1 && md_state != BOOT_WAITING_FOR_HS2 && md_state != READY) {
		CCCI_LEGACY_ERR_LOG(swtp->md_id, KERN, "%s md_state=%d no ready\n", __func__, md_state);
		ret = 1;
		goto __ERR_HANDLE__;
	}
	if (swtp->md_id == 0)
		ret = switch_MD1_Tx_Power(swtp->curr_mode);
	else {
		CCCI_LEGACY_ERR_LOG(swtp->md_id, KERN, "%s md is no support\n", __func__);
		ret = 2;
		goto __ERR_HANDLE__;
	}

	if (ret >= 0)
		CCCI_LEGACY_ALWAYS_LOG(swtp->md_id, KERN, "%s send swtp to md ret=%d, mode=%d, rety_cnt=%d\n",
			__func__, ret, swtp->curr_mode, swtp->retry_cnt);
	spin_lock_irqsave(&swtp->spinlock, flags);
	if (ret >= 0)
		swtp->retry_cnt = 0;
	else
		swtp->retry_cnt++;
	spin_unlock_irqrestore(&swtp->spinlock, flags);

__ERR_HANDLE__:

	if (ret < 0) {
		CCCI_LEGACY_ERR_LOG(swtp->md_id, KERN,
			"%s send tx power failed, ret=%d,rety_cnt=%d schedule delayed work\n",
			__func__, ret, swtp->retry_cnt);
		schedule_delayed_work(&swtp->delayed_work, 5 * HZ);
	}

	return ret;
}


static irqreturn_t swtp_irq_func(int irq, void *data)
{
	struct swtp_t *swtp = (struct swtp_t *)data;
	int ret = 0;

	ret = swtp_switch_mode(swtp);
	if (ret < 0) {
		CCCI_LEGACY_ERR_LOG(swtp->md_id, KERN,
			"%s swtp_switch_mode failed in irq, ret=%d\n", __func__, ret);
	} else {
		ret = swtp_send_tx_power_mode(swtp);
		if (ret < 0)
			CCCI_LEGACY_ERR_LOG(swtp->md_id, KERN,
				"%s send tx power failed in irq, ret=%d,and retry late\n", __func__, ret);
	}

	return IRQ_HANDLED;
}

static void swtp_tx_work(struct work_struct *work)
{
	struct swtp_t *swtp = container_of(to_delayed_work(work), struct swtp_t, delayed_work);
	int ret = 0;

	ret = swtp_send_tx_power_mode(swtp);
}
void ccci_swtp_test(int irq)
{
	swtp_irq_func(irq, &swtp_data[0]);
}

int swtp_md_tx_power_req_hdlr(int md_id, int data)
{
	int ret = 0;
	struct swtp_t *swtp = NULL;

	if (md_id < 0 || md_id >= SWTP_MAX_SUPPORT_MD)
		return -1;
	swtp = &swtp_data[md_id];
	ret = swtp_send_tx_power_mode(swtp);
	return 0;
}

int swtp_init(int md_id)
{
	int ret = 0;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };
	struct device_node *node = NULL;

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if ((get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) || (get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT))
		return ret;
#endif

	swtp_data[md_id].md_id = md_id;
	swtp_data[md_id].curr_mode = SWTP_EINT_PIN_PLUG_OUT;
	spin_lock_init(&swtp_data[md_id].spinlock);
	INIT_DELAYED_WORK(&swtp_data[md_id].delayed_work, swtp_tx_work);

	node = of_find_matching_node(NULL, swtp_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		of_property_read_u32_array(node, "interrupts", ints1, ARRAY_SIZE(ints1));
		swtp_data[md_id].gpiopin = ints[0];
		swtp_data[md_id].setdebounce = ints[1];
		swtp_data[md_id].eint_type = ints1[1];
		gpio_set_debounce(swtp_data[md_id].gpiopin, swtp_data[md_id].setdebounce);
		swtp_data[md_id].irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(swtp_data[md_id].irq, swtp_irq_func,
			IRQF_TRIGGER_NONE, "swtp-eint", &swtp_data[md_id]);
		if (ret != 0) {
			CCCI_LEGACY_ERR_LOG(md_id, KERN, "swtp-eint IRQ LINE NOT AVAILABLE\n");
		} else {
			CCCI_LEGACY_ALWAYS_LOG(md_id, KERN,
				"swtp-eint set EINT finished, irq=%d, setdebounce=%d, eint_type=%d\n",
				swtp_data[md_id].irq, swtp_data[md_id].setdebounce, swtp_data[md_id].eint_type);
		}
	} else {
		CCCI_LEGACY_ERR_LOG(md_id, KERN, "%s can't find compatible node\n", __func__);
		ret = -1;
	}
	register_ccci_sys_call_back(md_id, MD_SW_MD1_TX_POWER_REQ, swtp_md_tx_power_req_hdlr);
	return ret;
}
#endif

