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

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>

#include <scp_helper.h>
#include <scp_ipi.h>

#include "audio_log.h"
#include "audio_assert.h"

#include "audio_task_manager.h"

#include "audio_dma_buf_control.h"

#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
#include <mtk_spm_sleep.h>       /* for spm_ap_mdsrc_req */
#include "audio_ipi_client_phone_call.h"
#endif

#ifdef CONFIG_MTK_AUDIO_TUNNELING_SUPPORT
#include "audio_ipi_client_playback.h"
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define AUDIO_IPI_DEVICE_NAME "audio_ipi"
#define AUDIO_IPI_IOC_MAGIC 'i'

#define AUDIO_IPI_IOCTL_SEND_MSG_ONLY _IOW(AUDIO_IPI_IOC_MAGIC, 0, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_PAYLOAD  _IOW(AUDIO_IPI_IOC_MAGIC, 1, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_DRAM     _IOW(AUDIO_IPI_IOC_MAGIC, 2, unsigned int)

#define AUDIO_IPI_IOCTL_LOAD_SCENE    _IOW(AUDIO_IPI_IOC_MAGIC, 10, unsigned int)

#define AUDIO_IPI_IOCTL_DUMP_PCM      _IOW(AUDIO_IPI_IOC_MAGIC, 97, unsigned int)
#define AUDIO_IPI_IOCTL_REG_FEATURE   _IOW(AUDIO_IPI_IOC_MAGIC, 98, unsigned int)
#define AUDIO_IPI_IOCTL_SPM_MDSRC_ON  _IOW(AUDIO_IPI_IOC_MAGIC, 99, unsigned int)

/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT /* TOOD: move to call */
static bool b_speech_on;
static bool b_spm_ap_mdsrc_req_on;
static bool b_dump_pcm_enable;
#endif

static audio_resv_dram_t *p_resv_dram;
static uint32_t resv_dram_offset_cur;


/*
 * =============================================================================
 *                     functions declaration
 * =============================================================================
 */



/*
 * =============================================================================
 *                     implementation
 * =============================================================================
 */


static uint32_t get_resv_dram_buf_offset(uint32_t len)
{
	const uint32_t max_resv_dram_a2d_size = 0x2000;
	uint32_t retval = 0xFFFFFFFF;

	if ((len & 0xF) != 0) /* need 16 bytes align */
		len = (len & 0xFFFFFFF0) + 0x10;

	if (len >= max_resv_dram_a2d_size)
		retval = 0xFFFFFFFF; /* invalid offset */
	else if (resv_dram_offset_cur + len < max_resv_dram_a2d_size) {
		retval = resv_dram_offset_cur;
		resv_dram_offset_cur += len;
	} else {
		retval = 0;
		resv_dram_offset_cur = len;
	}

	return retval;
}


static int parsing_ipi_msg_from_user_space(
	void __user *user_data_ptr,
	audio_ipi_msg_data_t data_type)
{
	uint32_t resv_dram_offset = 0xFFFFFFFF;

	int retval = 0;

	ipi_msg_t ipi_msg;
	uint32_t msg_len = 0;


	/* get message size to read */
	msg_len = (data_type == AUDIO_IPI_MSG_ONLY)
		  ? IPI_MSG_HEADER_SIZE
		  : MAX_IPI_MSG_BUF_SIZE;

	retval = copy_from_user(&ipi_msg, user_data_ptr, msg_len);
	if (retval != 0) {
		AUD_LOG_E("msg copy_from_user retval %d\n", retval);
		goto parsing_exit;
	}

	/* double check */
	AUD_ASSERT(ipi_msg.data_type == data_type);

	/* get dram buf if need */
	if (ipi_msg.data_type == AUDIO_IPI_DMA) {
		resv_dram_offset = get_resv_dram_buf_offset(ipi_msg.param1);
		if (resv_dram_offset == 0xFFFFFFFF || ipi_msg.param1 > p_resv_dram->size) {
			AUD_LOG_E("dma_data_len %u, no enough memory!!\n", ipi_msg.param1);
			ipi_msg.param1 = 0;
			retval = -1;
			goto parsing_exit;
		}
		retval = copy_from_user(
				 p_resv_dram->vir_addr + resv_dram_offset,
				 (void __user *)ipi_msg.dma_addr,
				 ipi_msg.param1);
		if (retval != 0) {
			AUD_LOG_E("dram copy_from_user retval %d\n", retval);
			goto parsing_exit;
		}
		ipi_msg.dma_addr = p_resv_dram->phy_addr + resv_dram_offset;
	}

	/* get message size to write */
	msg_len = get_message_buf_size(&ipi_msg);
	check_msg_format(&ipi_msg, msg_len);

	retval = audio_send_ipi_filled_msg(&ipi_msg);
	if (retval == 0) {
		retval = copy_to_user(user_data_ptr, &ipi_msg, sizeof(ipi_msg_t));
		if (retval) {
			AUD_LOG_W("%s(), copy_to_user err, id = 0x%x\n", __func__, ipi_msg.msg_id);
			retval = -EFAULT;
		}
	}

parsing_exit:
	return retval;
}


/* trigger scp driver to dump scp_log to sdcard */
/*extern void scp_get_log(int save);*/ /* TODO: ask scp driver to add in .h */


static long audio_ipi_driver_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	AUD_LOG_V("%s(), cmd = %u, arg = %lu\n", __func__, cmd, arg);

	switch (cmd) {
	case AUDIO_IPI_IOCTL_SEND_MSG_ONLY: {
		retval = parsing_ipi_msg_from_user_space(
				 (void __user *)arg, AUDIO_IPI_MSG_ONLY);
		break;
	}
	case AUDIO_IPI_IOCTL_SEND_PAYLOAD: {
		retval = parsing_ipi_msg_from_user_space(
				 (void __user *)arg, AUDIO_IPI_PAYLOAD);
		break;
	}
	case AUDIO_IPI_IOCTL_SEND_DRAM: {
		retval = parsing_ipi_msg_from_user_space(
				 (void __user *)arg, AUDIO_IPI_DMA);
		break;
	}
	case AUDIO_IPI_IOCTL_LOAD_SCENE: {
		AUD_LOG_D("%s(), AUDIO_IPI_IOCTL_LOAD_SCENE(%d)\n", __func__, (uint8_t)arg);
		audio_load_task((uint8_t)arg);
		break;
	}
#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT /* TOOD: use message */
	case AUDIO_IPI_IOCTL_DUMP_PCM: {
		AUD_LOG_D("%s(), AUDIO_IPI_IOCTL_DUMP_PCM(%lu)\n", __func__, arg);
		b_dump_pcm_enable = arg;
		break;
	}
	case AUDIO_IPI_IOCTL_REG_FEATURE: {
		AUD_LOG_V("%s(), AUDIO_IPI_IOCTL_REG_FEATURE(%lu)\n", __func__, arg);
		if (arg) { /* enable scp speech */
			if (b_speech_on == false) {
				b_speech_on = true;
				scp_register_feature(OPEN_DSP_FEATURE_ID);
				if (b_dump_pcm_enable)
					open_dump_file();
			}
		} else { /* disable scp speech */
			if (b_speech_on == true) {
				b_speech_on = false;
				close_dump_file();
				scp_deregister_feature(OPEN_DSP_FEATURE_ID);
				/*scp_get_log(1);*/ /* dump scp log */
			}
		}
		break;
	}
	case AUDIO_IPI_IOCTL_SPM_MDSRC_ON: {
		if (arg) { /* enable scp speech */
			if (b_spm_ap_mdsrc_req_on == false) {
				b_spm_ap_mdsrc_req_on = true;
				AUD_LOG_D("%s(), spm_ap_mdsrc_req(%lu)\n", __func__, arg);
				spm_ap_mdsrc_req(arg);
			}
		} else { /* disable scp speech */
			if (b_spm_ap_mdsrc_req_on == true) { /* false: error handling when reboot */
				b_spm_ap_mdsrc_req_on = false;
				AUD_LOG_D("%s(), spm_ap_mdsrc_req(%lu)\n", __func__, arg);
				spm_ap_mdsrc_req(arg);
			}
		}
		break;
	}
#endif
	default:
		break;
	}
	return retval;
}

#ifdef CONFIG_COMPAT
static long audio_ipi_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		AUD_LOG_E("op null\n");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}
#endif


/* TODO: use ioctl */
static ssize_t audio_ipi_driver_read(
	struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return 0; /*audio_ipi_client_phone_call_read(file, buf, count, ppos);*/
}



static const struct file_operations audio_ipi_driver_ops = {
	.owner          = THIS_MODULE,
	.read           = audio_ipi_driver_read,
	.unlocked_ioctl = audio_ipi_driver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = audio_ipi_driver_compat_ioctl,
#endif
};


static struct miscdevice audio_ipi_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AUDIO_IPI_DEVICE_NAME,
	.fops = &audio_ipi_driver_ops
};


static int __init audio_ipi_driver_init(void)
{
	int ret = 0;

#if 0 /* TODO: this will cause KE/HWT ...... */
	if (is_scp_ready(SCP_B_ID)) {
		AUD_LOG_E("[SCP] scp not ready\n");
		return -EACCES;
	}
#endif

	audio_task_manager_init();

	/* TODO: ring buf */
	init_reserved_dram();
	p_resv_dram = get_reserved_dram();
	AUD_ASSERT(p_resv_dram != NULL);

	resv_dram_offset_cur = 0;

#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
	audio_ipi_client_phone_call_init();

	b_speech_on = false;
	b_spm_ap_mdsrc_req_on = false;
	b_dump_pcm_enable = false;
#endif

	ret = misc_register(&audio_ipi_device);
	if (unlikely(ret != 0)) {
		AUD_LOG_E("[SCP] misc register failed\n");
		return ret;
	}

	return ret;
}


static void __exit audio_ipi_driver_exit(void)
{
#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
	audio_ipi_client_phone_call_deinit();
#endif

	audio_task_manager_deinit();

	misc_deregister(&audio_ipi_device);
}


module_init(audio_ipi_driver_init);
module_exit(audio_ipi_driver_exit);

