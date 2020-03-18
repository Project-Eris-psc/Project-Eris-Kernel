/* stationary gesture sensor driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include "situation.h"
#include "device_orientation.h"
#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


#define PORTRAITHUB_TAG                  "[device_orientation] "
#define PORTRAITHUB_FUN(f)               pr_warn(PORTRAITHUB_TAG"%s\n", __func__)
#define PORTRAITHUB_ERR(fmt, args...)    pr_warn(PORTRAITHUB_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define PORTRAITHUB_LOG(fmt, args...)    pr_warn(PORTRAITHUB_TAG fmt, ##args)


static struct situation_init_info device_orientation_init_info;
static int device_orientation_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	uint64_t time_stamp_gpt = 0;

	err = sensor_get_data_from_hub(ID_DEVICE_ORIENTATION, &data);
	if (err < 0) {
		PORTRAITHUB_ERR("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
	time_stamp_gpt	= data.time_stamp_gpt;
	*probability	= data.gesture_data_t.probability;
	PORTRAITHUB_LOG("recv ipi: timestamp: %lld, timestamp_gpt: %lld, probability: %d!\n",
		time_stamp, time_stamp_gpt, *probability);
	return 0;
}
static int device_orientation_open_report_data(int open)
{
	int ret = 0;

#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_DEVICE_ORIENTATION, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	PORTRAITHUB_LOG("%s : type=%d, open=%d\n", __func__, ID_DEVICE_ORIENTATION, open);
	ret = sensor_enable_to_hub(ID_DEVICE_ORIENTATION, open);
	return ret;
}
static int device_orientation_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_DEVICE_ORIENTATION, flag, samplingPeriodNs, maxBatchReportLatencyNs);
}
static int device_orientation_flush(void)
{
	return sensor_flush_to_hub(ID_DEVICE_ORIENTATION);
}
static int device_orientation_recv_data(struct data_unit_t *event, void *reserved)
{
	if (event->flush_action == FLUSH_ACTION)
		situation_flush_report(ID_DEVICE_ORIENTATION);
	else if (event->flush_action == DATA_ACTION)
		situation_data_report(ID_DEVICE_ORIENTATION, event->tilt_event.state);
	return 0;
}

static int device_orientation_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = device_orientation_open_report_data;
	ctl.batch = device_orientation_batch;
	ctl.flush = device_orientation_flush,
	ctl.is_support_wake_lock = false;
	ctl.is_support_batch = false;
	err = situation_register_control_path(&ctl, ID_DEVICE_ORIENTATION);
	if (err) {
		PORTRAITHUB_ERR("register stationary control path err\n");
		goto exit;
	}

	data.get_data = device_orientation_get_data;
	err = situation_register_data_path(&data, ID_DEVICE_ORIENTATION);
	if (err) {
		PORTRAITHUB_ERR("register stationary data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_DEVICE_ORIENTATION, device_orientation_recv_data);
	if (err) {
		PORTRAITHUB_ERR("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int device_orientation_local_uninit(void)
{
	return 0;
}

static struct situation_init_info device_orientation_init_info = {
	.name = "device_orientation_hub",
	.init = device_orientation_local_init,
	.uninit = device_orientation_local_uninit,
};

static int __init device_orientation_init(void)
{
	situation_driver_add(&device_orientation_init_info, ID_DEVICE_ORIENTATION);
	return 0;
}

static void __exit device_orientation_exit(void)
{
	PORTRAITHUB_FUN();
}

module_init(device_orientation_init);
module_exit(device_orientation_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Stationary Gesture driver");
MODULE_AUTHOR("qiangming.xia@mediatek.com");
