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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *    pmic_ipi.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines PMIC functions
 *
 * Author:
 * -------
 * Wilma Wu
 *
 ****************************************************************************/
#include <mt-plat/upmu_common.h>
#include <sspm_ipi_pin.h>
#include <sspm_ipi.h>
#include <include/pmic_ipi.h>
#include <include/pmic_ipi_service_id.h>

#ifdef SSPM_STF
#include <linux/init.h>
#include "sspm_stf.h"
#endif /*--SSPM_STF--*/
#include <linux/mutex.h>
#include <linux/spinlock.h>

static DEFINE_SPINLOCK(pmic_ipi_spinlock);

static DEFINE_SPINLOCK(extbuck_ipi_spinlock);

int extbuck_ipi_to_sspm(void *buffer, void *retbuf, unsigned char lock)
{
	int ret_val = 0;
	int ipi_ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&extbuck_ipi_spinlock, flags);
	ret_val =
		sspm_ipi_send_sync_ex(IPI_ID_EXT_BUCK, IPI_OPT_LOCK_POLLING,
				buffer, 1, retbuf, 1);
	spin_unlock_irqrestore(&extbuck_ipi_spinlock, flags);

	ipi_ret = ((struct pmic_ipi_ret_datas *)retbuf)->data[0];

	if (ret_val)
		pr_err("%s ap_ret = %d ipi_ret =%d\n", __func__, ret_val, ipi_ret);
	else {
		if (ipi_ret != 0 && ipi_ret != 1)
			pr_err("%s ap_ret = %d ipi_ret =%d\n", __func__,
					ret_val, ipi_ret);
	}
	ret_val = ipi_ret;

	return ret_val;
}

unsigned int pmic_ipi_to_sspm(void *buffer, void *retbuf, unsigned char lock)
{
	int ret_val = 0;
	int ipi_ret = 0;
	unsigned int cmd = ((struct pmic_ipi_cmds *)buffer)->cmd[0];
	unsigned long flags;

	spin_lock_irqsave(&pmic_ipi_spinlock, flags);
	ret_val =
		sspm_ipi_send_sync_ex(IPI_ID_PMIC, IPI_OPT_LOCK_POLLING, buffer,
				PMIC_IPI_SEND_SLOT_SIZE, retbuf, PMIC_IPI_ACK_SLOT_SIZE);
	spin_unlock_irqrestore(&pmic_ipi_spinlock, flags);
	ipi_ret = ((struct pmic_ipi_ret_datas *)retbuf)->data[0];

	switch (cmd) {

	case MAIN_PMIC_WRITE_REGISTER:
		if (ret_val) {
			if (ret_val == IPI_BUSY || ret_val == IPI_TIMEOUT_ACK) {
				if (ipi_ret != 0)
					pr_err("%s ap_ret_w = %d ipi_ret_w =%d\n", __func__,
							ret_val, ipi_ret);
			} else
				/* Real PMIC service execution result, by each PMIC service */
				pr_err("%s ap_ret_w = %d ipi_ret_w =%d\n", __func__,
						ret_val, ipi_ret);
		} else {
			if (ipi_ret != 0)
				pr_err("%s ap_ret_w = %d ipi_ret_w =%d\n", __func__,
					ret_val, ipi_ret);
		}
		ret_val = ipi_ret;

		break;

	case MAIN_PMIC_READ_REGISTER:
		if (ret_val) {
			if (ret_val == IPI_BUSY || ret_val == IPI_TIMEOUT_ACK) {
				if (ipi_ret != 0)
					pr_err("%s ap_ret_r = %d ipi_ret_r =%d\n", __func__,
							ret_val, ipi_ret);
			} else
				/* Real PMIC service execution result, by each PMIC service */
				pr_err("%s ap_ret_r = %d ipi_ret_r =%d\n", __func__,
						ret_val, ipi_ret);
		} else {
			if (ipi_ret != 0)
				pr_err("%s ap_ret_r = %d ipi_ret_r =%d\n", __func__,
					ret_val, ipi_ret);
		}
		ret_val = ipi_ret;
		break;

	case MAIN_PMIC_REGULATOR:
		if (ret_val) {
			if (ret_val == IPI_BUSY || ret_val == IPI_TIMEOUT_ACK) {
				if (ipi_ret != 0)
					pr_err("%s ap_ret_mreg = %d ipi_ret_mreg =%d\n", __func__,
						ret_val, ipi_ret);
			} else
				/* Real PMIC service execution result, by each PMIC service */
				pr_err("%s ap_ret_mreg = %d ipi_ret_mreg =%d\n", __func__,
						ret_val, ipi_ret);
		} else {
			if (ipi_ret != 0)
				pr_err("%s ap_ret_mreg = %d ipi_ret_mreg =%d\n", __func__,
					ret_val, ipi_ret);
		}
		ret_val = ipi_ret;
		break;

	case SUB_PMIC_CTRL:
		break;

	default:
		pr_err("%s(%d) cmd(%d) wrong!!!\n", __func__, __LINE__, cmd);

		break;
	}
	return ret_val;
}

#ifdef CONFIG_MTK_EXTBUCK
int extbuck_ipi_enable(unsigned char buck_id, unsigned char en)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };

	send.cmd[0] = en;

	return extbuck_ipi_to_sspm(&send, &recv, 0);
}
#endif /* CONFIG_MTK_EXTBUCK */

unsigned int pmic_ipi_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK,
					unsigned int SHIFT, unsigned char lock)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = MAIN_PMIC_READ_REGISTER;
	send.cmd[1] = RegNum;
	send.cmd[2] = MASK;
	send.cmd[3] = SHIFT;

	ret = pmic_ipi_to_sspm(&send, &recv, lock);

	if (ret >= 0)
		*val = recv.data[1];

	return ret;
}

unsigned int pmic_ipi_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK,
					unsigned int SHIFT, unsigned char lock)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = MAIN_PMIC_WRITE_REGISTER;
	send.cmd[1] = RegNum;
	send.cmd[2] = val;
	send.cmd[3] = MASK;
	send.cmd[4] = SHIFT;

	ret = pmic_ipi_to_sspm(&send, &recv, lock);

	return ret;
}

unsigned int pmic_regulator_profiling(unsigned char type)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = MAIN_PMIC_REGULATOR;
	send.cmd[1] = type;  /*PMIC_REGULATOR PROFILLING TYPE*/

	ret = pmic_ipi_to_sspm(&send, &recv, 1);

	return ret;
}

unsigned int sub_pmic_ipi_interface(unsigned int type, unsigned int ctrl)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = SUB_PMIC_CTRL;
	send.cmd[1] = type;  /*PMIC_IPI_FUNC_SUB_PMIC_EN*/
	send.cmd[2] = ctrl;

	ret = pmic_ipi_to_sspm(&send, &recv, 1);

	return ret;
}

unsigned int pmic_ipi_test_code(void)
{
	unsigned int ret_val = 0;
	unsigned int rdata = 0;
	unsigned int error = 0;
	int i = 0;
	unsigned int test_data[30] = {
	    0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669,
		0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A,
		0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27
	};

	for (i = 0; i < 30; i++) {
		ret_val =
		    pmic_ipi_config_interface(PMIC_DEW_WRITE_TEST_ADDR, test_data[i], 0xffff, 0, 1);

		if (ret_val != 0) {
			pr_err("%s config failed: test_data[%d]=%x ret_val=%x\n", __func__, i,
			       test_data[i], ret_val);
			error++;
			break;
		}

		ret_val = pmic_ipi_read_interface(PMIC_DEW_WRITE_TEST_ADDR, &rdata, 0xffff, 0, 1);

		if (ret_val != 0 || rdata != test_data[i]) {
			pr_err("%s read failed: test_data[%d]=%x rdata =%x ret_val=%x\n", __func__,
			       i, test_data[i], rdata, ret_val);
			error++;
			break;
		}

		pr_debug("%s ok: test_data[%d]=%x rdata=%x\n", __func__, i, test_data[i], rdata);
	}

	return error;

}

#ifdef SSPM_STF

struct pmic_ipi_cmds stf_send = {
	.cmd[0] = MAIN_PMIC_WRITE_REGISTER,
	.cmd[1] = 0xC06,
	.cmd[2] = 0x5AA5,
	.cmd[3] = 0xFFFF,
	.cmd[4] = 0,
};
struct pmic_ipi_ret_datas stf_recv = { {0} };

unsigned int stf_val;
/*
 * 0: SSPM_STF_PASS
 * 1: SSPM_STF_FAIL
 */
int stf_pmic_test(void *data)
{
	pr_err("stf_pmic_test\n");
	stf_val = 0;
	stf_val = sspm_ipi_send_sync_ex(IPI_ID_PMIC, IPI_OPT_DEFAUT, &stf_send,
					PMIC_IPI_SEND_SLOT_SIZE, &stf_recv, PMIC_IPI_ACK_SLOT_SIZE);
	return stf_val;
}

int stf_pmic_chk(void *data)
{
	unsigned int ret_val = 0;
	/* Real PMIC service execution result, by each PMIC service */
	if (stf_val) {
		ret_val = ((struct pmic_ipi_ret_datas *)(&stf_recv))->data[0];
		pr_err("stf_pmic_chk = %d\n", ret_val);
	}

	return ret_val;
}

struct chk_data stf_pmic_chk_data[] = {
	{
		.ack_data = 0,
		.time_us = 100,
	},
};

struct sspm_stf_device pmic = {
	.name = "PMIC",
	.ID = IPI_ID_PMIC,
	.ipi_num = 1,
	.ipi_chk_data = stf_pmic_chk_data,
	.start_addr = NULL,
	.len_byte = 0,
	.bin_file = NULL,
	.test_func = stf_pmic_test,
	.chk_func = stf_pmic_chk,
};

static int __init sspm_stf_pmic_init(void)
{
	sspm_stf_register(&pmic);

	return 0;
}
late_initcall(sspm_stf_pmic_init);

#endif /*--SSPM_STF--*/
