/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uidgid.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "mt-plat/mtk_thermal_monitor.h"
#include "mtk_thermal_typedefs.h"
#include "mach/mtk_thermal.h"
#include <linux/regmap.h>

#include <linux/mfd/mt6397/core.h>
#include  <mt-plat/upmu_common.h>

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static unsigned int interval;	/* seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = {
	120000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000
};

static unsigned int cl_dev_sysrst_state;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
static int mtktspmic_debug_log;
static int kernelmode;

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = { 0 };
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

#define mtktspmic_TEMP_CRIT 150000	/* 150.000 degree Celsius */

#define mtktspmic_dprintk(fmt, args...)   \
do {									\
	if (mtktspmic_debug_log) {				\
		pr_debug("Power/PMIC_Thermal" fmt, ##args); \
	}								   \
} while (0)

/* Cali */
static kal_int32 g_o_vts;
static kal_int32 g_degc_cali;
static kal_int32 g_adc_cali_en;
static kal_int32 g_o_slope;
static kal_int32 g_o_slope_sign;
static kal_int32 g_id;

static struct regmap *regmap_pmic_thermal;

#define y_pmic_repeat_times	1
#define THERMAL_NAME	"mt6392-thermal"

static u16 pmic_read(u16 addr)
{
	u32 rdata = 0;

	regmap_read(regmap_pmic_thermal, (u32) addr, &rdata);
	return (u16) rdata;
}

static void pmic_cali_prepare(void)
{
	kal_uint32 temp0, temp1;

	temp0 = pmic_read(0x666); /* efuse val between 160 to 175. */
	temp1 = pmic_read(0x668); /* efuse val between 176 to 191. */
	pr_info("Power/PMIC_Thermal: Reg(0x666)=0x%x, Reg(0x668)=0x%x\n", temp0, temp1);

	g_o_vts = ((temp1 & 0x001f) << 8) + ((temp0 >> 8) & 0x00ff);
	g_degc_cali = (temp0 >> 2) & 0x003f;
	g_adc_cali_en = (temp0 >> 1) & 0x0001;
	g_o_slope_sign = (temp1 >> 5) & 0x0001;
	g_o_slope = ((temp1 >> 6) & 0x0007) + ((temp1 >> 11) & 0x0007);
	g_id = (temp1 >> 14) & 0x0001;

	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en != 1) {
		/* no cali, use default value */
		g_o_vts = 1600;
		g_degc_cali = 50;
		g_o_slope = 0;
		g_o_slope_sign = 0;
		pr_err("[pmic_cali_prepare] No thermal calibration data !!!");
	}
}

static kal_int32 thermal_cal_exec(kal_uint32 ret)
{
	kal_int32 t_current = 0;
	kal_int32 y_curr = ret;
	kal_int32 slope_data = 0;
	kal_int32 VBE_T = 0;

	if (ret == 0)
		return 0;

	VBE_T = ((g_o_vts * 1800) / 4096);

	if (g_o_slope_sign == 0) {
		slope_data = (y_curr - VBE_T) * 100 / (170 + g_o_slope);
	} else {
		slope_data = (y_curr - VBE_T) * 100 / (170 - g_o_slope);
	}

	slope_data = slope_data - 2 * slope_data;

	t_current = ((g_degc_cali / 2) + slope_data) * 1000;

	mtktspmic_dprintk("[thermal_cal_exec] ret=%d, y_curr=%d\n", ret, y_curr);
	mtktspmic_dprintk("[thermal_cal_exec] data =%d, slope_data=%d, VBE_T=%d\n",
				(g_degc_cali / 2), slope_data, VBE_T);
	mtktspmic_dprintk("[thermal_cal_exec] T_PMIC=%d\n", t_current);

	return t_current;
}

/* Jerry 2013.3.24extern void pmic_thermal_dump_reg(void); */

/* int ts_pmic_at_boot_time=0; */
static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1 = 0, PMIC_counter;
static int mtktspmic_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;

	mutex_lock(&TSPMIC_lock);

	temp = PMIC_IMM_GetOneChannelValue(4, y_pmic_repeat_times, 2);
	temp1 = thermal_cal_exec(temp);

	mtktspmic_dprintk("[mtktspmic_get_hw_temp] PMIC_IMM_GetOneChannel 4=%d, T=%d\n", temp, temp1);

	if ((temp1 > 90000) || (temp1 < -30000)) {
		pr_err("[Power/PMIC_Thermal] raw=%d, PMIC T=%d", temp, temp1);
		/* pmic_thermal_dump_reg(); */
	}

	if ((temp1 > 150000) || (temp1 < -50000)) {
		pr_info("[Power/PMIC_Thermal] drop this data\n");
		temp1 = pre_temp1;
	} else if ((PMIC_counter != 0)
		   && (((pre_temp1 - temp1) > 30000) || ((temp1 - pre_temp1) > 30000))) {
		pr_info("[Power/PMIC_Thermal] drop this data 2\n");
		temp1 = pre_temp1;
	} else {
		/* update previous temp */
		pre_temp1 = temp1;
		mtktspmic_dprintk("[Power/PMIC_Thermal] pre_temp1=%d\n", pre_temp1);

		if (PMIC_counter == 0)
			PMIC_counter++;
	}

	mutex_unlock(&TSPMIC_lock);
	return temp1;
}

static int mtktspmic_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtktspmic_get_hw_temp();
	return 0;
}

static int mtktspmic_bind(struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspmic_dprintk("[mtktspmic_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_dprintk("[mtktspmic_bind] error binding cooling dev\n");
		return -EINVAL;
	}
	mtktspmic_dprintk("[mtktspmic_bind] binding OK, %d\n", table_val);

	return 0;
}

static int mtktspmic_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtktspmic_dprintk("[mtktspmic_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktspmic_dprintk("[mtktspmic_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	}
	mtktspmic_dprintk("[mtktspmic_unbind] unbinding OK\n");

	return 0;
}

static int mtktspmic_get_mode(struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktspmic_set_mode(struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktspmic_get_trip_type(struct thermal_zone_device *thermal, int trip,
				   enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktspmic_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				   int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktspmic_get_crit_temp(struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = mtktspmic_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktspmic_dev_ops = {
	.bind = mtktspmic_bind,
	.unbind = mtktspmic_unbind,
	.get_temp = mtktspmic_get_temp,
	.get_mode = mtktspmic_get_mode,
	.set_mode = mtktspmic_set_mode,
	.get_trip_type = mtktspmic_get_trip_type,
	.get_trip_temp = mtktspmic_get_trip_temp,
	.get_crit_temp = mtktspmic_get_crit_temp,
};

static int tspmic_sysrst_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int tspmic_sysrst_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int tspmic_sysrst_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_debug("Power/PMIC_Thermal: reset, reset, reset!!!");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		pr_debug("*****************************************");
		pr_debug("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

		*(unsigned int *)0x0 = 0xdead;
		/* arch_reset(0,NULL); */
	}
	return 0;
}

static struct thermal_cooling_device_ops mtktspmic_cooling_sysrst_ops = {
	.get_max_state = tspmic_sysrst_get_max_state,
	.get_cur_state = tspmic_sysrst_get_cur_state,
	.set_cur_state = tspmic_sysrst_set_cur_state,
};

static int mtktspmic_read(struct seq_file *m, void *v)
{
	seq_printf(m, "[ mtktspmic_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);
	seq_printf(m, "trip_4_temp=%d,\ntrip_5_temp=%d,trip_6_temp=%d,",
trip_temp[4], trip_temp[5], trip_temp[6]);
	seq_printf(m, "trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
trip_temp[7], trip_temp[8], trip_temp[9]);
	seq_printf(m, "g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
	seq_printf(m, "g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\n",
g_THERMAL_TRIP[3], g_THERMAL_TRIP[4]);
	seq_printf(m, "g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
	seq_printf(m, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
g_bind0, g_bind1, g_bind2, g_bind3,	g_bind4);
	seq_printf(m, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

int mtktspmic_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("mtktspmic-sysrst", NULL,
							    &mtktspmic_cooling_sysrst_ops);
	return 0;
}

int mtktspmic_register_thermal(void)
{
	mtktspmic_dprintk("[mtktspmic_register_thermal]\n");

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktspmic", num_trip, NULL,
						   &mtktspmic_dev_ops, 0, 0, 0, interval * 1000);

	return 0;
}

void mtktspmic_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

void mtktspmic_unregister_thermal(void)
{
	mtktspmic_dprintk("[mtktspmic_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static ssize_t mtktspmic_write(struct file *file, const char __user *buffer, size_t count,
			       loff_t *data)
{
	int len = 0, time_msec = 0;
	int trip[10] = { 0 };
	int t_type[10] = { 0 };
	int i;
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
	char desc[512];


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf
	    (desc,
	     "%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d",
	     &num_trip, &trip[0], &t_type[0], bind0, &trip[1], &t_type[1], bind1, &trip[2],
	     &t_type[2], bind2, &trip[3], &t_type[3], bind3, &trip[4], &t_type[4], bind4, &trip[5],
	     &t_type[5], bind5, &trip[6], &t_type[6], bind6, &trip[7], &t_type[7], bind7, &trip[8],
	     &t_type[8], bind8, &trip[9], &t_type[9], bind9, &time_msec) == 32) {
		mtktspmic_dprintk("[mtktspmic_write] mtktspmic_unregister_thermal\n");
		mtktspmic_unregister_thermal();

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] = g_bind4[0] = g_bind5[0] =
		    g_bind6[0] = g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = bind0[i];
			g_bind1[i] = bind1[i];
			g_bind2[i] = bind2[i];
			g_bind3[i] = bind3[i];
			g_bind4[i] = bind4[i];
			g_bind5[i] = bind5[i];
			g_bind6[i] = bind6[i];
			g_bind7[i] = bind7[i];
			g_bind8[i] = bind8[i];
			g_bind9[i] = bind9[i];
		}

		mtktspmic_dprintk("[mtktspmic_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,",
g_THERMAL_TRIP[0], g_THERMAL_TRIP[1]);
		mtktspmic_dprintk("g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,",
g_THERMAL_TRIP[2], g_THERMAL_TRIP[3], g_THERMAL_TRIP[4]);
		mtktspmic_dprintk("g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
		mtktspmic_dprintk("g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
		mtktspmic_dprintk("[mtktspmic_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
		mtktspmic_dprintk("cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec / 1000;

		mtktspmic_dprintk("[mtktspmic_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,",
trip_temp[0], trip_temp[1], trip_temp[2]);
		mtktspmic_dprintk("trip_3_temp=%d,trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,",
trip_temp[3], trip_temp[4],	trip_temp[5], trip_temp[6]);
		mtktspmic_dprintk("trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
trip_temp[7], trip_temp[8], trip_temp[9],	time_msec);

		mtktspmic_dprintk("[mtktspmic_write] mtktspmic_register_thermal\n");
		mtktspmic_register_thermal();

		return count;
	}
	mtktspmic_dprintk("[mtktspmic_write] bad argument\n");

	return -EINVAL;
}

static int mtktspmic_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktspmic_read, NULL);
}

static const struct file_operations mtktspmic_fops = {
	.owner = THIS_MODULE,
	.open = mtktspmic_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktspmic_write,
	.release = single_release,
};

static int mtktspmic_probe(struct platform_device *pdev)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktspmic_dir = NULL;
	struct mt6397_chip *mt6392_chip = dev_get_drvdata(pdev->dev.parent);

	regmap_pmic_thermal = mt6392_chip->regmap;

	pmic_cali_prepare();

	err = mtktspmic_register_cooler();
	if (err)
		return err;
	err = mtktspmic_register_thermal();
	if (err)
		goto err_unreg;

	mtktspmic_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktspmic_dir)
		mtktspmic_dprintk("[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	entry = proc_create("tzpmic", S_IRUGO | S_IWUSR | S_IWGRP, mtktspmic_dir, &mtktspmic_fops);
	if (entry)
		proc_set_user(entry, uid, gid);

	return 0;

err_unreg:
	mtktspmic_unregister_cooler();
	return err;
}

static int mtktspmic_remove(struct platform_device *pdev)
{
	if (thz_dev)
		thermal_zone_device_unregister(thz_dev);
	return 0;
}

static struct platform_driver mtk_thermal_pmic_driver = {
		.probe = mtktspmic_probe,
		.remove = mtktspmic_remove,
		.driver =  {
				.name = THERMAL_NAME,
		},
};

static int __init mtktspmic_init(void)
{
	return platform_driver_register(&mtk_thermal_pmic_driver);
}

static void __exit mtktspmic_exit(void)
{
	mtktspmic_dprintk("[mtktspmic_exit]\n");
	mtktspmic_unregister_thermal();
	mtktspmic_unregister_cooler();
}
module_init(mtktspmic_init);
module_exit(mtktspmic_exit);
