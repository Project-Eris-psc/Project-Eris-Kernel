/*****************************************************************************
 *
 * Filename:
 * ---------
 *   gc0312yuv_Sensor.c
 *
 * Project:
 * --------
 *   MAUI
 *
 * Description:
 * ------------
 *   Image sensor driver function
 *   V1.1.0
 *
 * Author:
 * -------
 *   travis
 *
 *=============================================================
 *             HISTORY
 * Below this line, this part is controlled by GCoreinc. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * DATE				 NAME			DESCRIPTION
 * 20140801			 Travis			V1.1.0
 * 20140929			 Travis			V1.2.0
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by GCoreinc. DO NOT MODIFY!!
 *=============================================================
 ******************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "gc0312yuv_Sensor.h"
#include "gc0312yuv_Camera_Sensor_para.h"
#include "gc0312yuv_CameraCustomized.h"

/* #define GC0312YUV_DEBUG */
#ifdef GC0312YUV_DEBUG
#define SENSORDB printk
#else
#define SENSORDB(x, ...)
#endif

#define GC0312_TEST_PATTERN_CHECKSUM (0xA8CCFA09)

//#define DEBUG_SENSOR_GC0312	/* T_flash Tuning */

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData,
		       u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

//extern unsigned long TFlashSwitchValue;

kal_uint16 GC0312_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	char puSendCmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	return iWriteRegI2C(puSendCmd, 2, GC0312_WRITE_ID);

}

kal_uint16 GC0312_read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte = 0;
	char puSendCmd = { (char)(addr & 0xFF) };

	iReadRegI2C(&puSendCmd, 1, (u8 *) &get_byte, 1, GC0312_WRITE_ID);

	return get_byte;
}


#ifdef DEBUG_SENSOR_GC0312
#define gc0312_OP_CODE_INI		0x00	/* Initial value. */
#define gc0312_OP_CODE_REG		0x01	/* Register */
#define gc0312_OP_CODE_DLY		0x02	/* Delay */
#define gc0312_OP_CODE_END	0x03	/* End of initial setting. */
static kal_uint16 fromsd;

typedef struct {
	u16 init_reg;
	u16 init_val;		/* Save the register value and delay tick */
	u8 op_code;		/* 0 - Initial value, 1 - Register, 2 - Delay, 3 - End of setting. */
} gc0312_initial_set_struct;

gc0312_initial_set_struct gc0312_Init_Reg[5000];

static u32 strtol(const char *nptr, u8 base)
{
	u8 ret;

	printk("gc0312___%s____\n", __func__);
	if (!nptr || (base != 16 && base != 10 && base != 8)) {
		printk("gc0312 %s(): NULL pointer input\n", __func__);
		return -1;
	}
	for (ret = 0; *nptr; nptr++) {
		if ((base == 16 && *nptr >= 'A' && *nptr <= 'F') ||
		    (base == 16 && *nptr >= 'a' && *nptr <= 'f') ||
		    (base >= 10 && *nptr >= '0' && *nptr <= '9') ||
		    (base >= 8 && *nptr >= '0' && *nptr <= '7')) {
			ret *= base;
			if (base == 16 && *nptr >= 'A' && *nptr <= 'F')
				ret += *nptr - 'A' + 10;
			else if (base == 16 && *nptr >= 'a' && *nptr <= 'f')
				ret += *nptr - 'a' + 10;
			else if (base >= 10 && *nptr >= '0' && *nptr <= '9')
				ret += *nptr - '0';
			else if (base >= 8 && *nptr >= '0' && *nptr <= '7')
				ret += *nptr - '0';
		} else
			return ret;
	}
	return ret;
}

static u8 GC0312_Initialize_from_T_Flash(void)
{
	/* FS_HANDLE fp = -1;    Default, no file opened. */
	/* u8 *data_buff = NULL; */
	u8 *curr_ptr = NULL;
	u32 file_size = 0;
	/* u32 bytes_read = 0; */
	u32 i = 0, j = 0;
	u8 func_ind[4] = { 0 };	/* REG or DLY */

	struct file *fp;
	mm_segment_t fs;
	loff_t pos = 0;
	static u8 data_buff[10 * 1024];

	printk("gc0312 ___%s____ start\n", __func__);

	fp = filp_open("/data/misc/tuning/gc0312_sd.txt", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		/* printk("0312 create file error,IS_ERR(fp)=%s\n",IS_ERR(fp)); */
		return -1;
	} else {
		/* printk("0312 create file error,IS_ERR(fp)=%s\n",IS_ERR(fp)); */
	}
	fs = get_fs();
	set_fs(KERNEL_DS);

	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos);
	/* printk("%s %d %d\n", buf,iFileLen,pos); */
	filp_close(fp, NULL);
	set_fs(fs);

	/* Start parse the setting witch read from t-flash. */
	curr_ptr = data_buff;
	while (curr_ptr < (data_buff + file_size)) {
		while ((*curr_ptr == ' ') || (*curr_ptr == '\t'))	/* Skip the Space & TAB */
			curr_ptr++;

		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '*')) {
			while (!(((*curr_ptr) == '*') && ((*(curr_ptr + 1)) == '/'))) {
				curr_ptr++;	/* Skip block comment code. */
			}

			while (!((*curr_ptr == 0x0D) && (*(curr_ptr + 1) == 0x0A))) {
				curr_ptr++;
			}

			curr_ptr += 2;	/* Skip the enter line */

			continue;
		}

		if (((*curr_ptr) == '/') || ((*curr_ptr) == '{') || ((*curr_ptr) == '}')) {	/* Comment line, skip it. */
			while (!((*curr_ptr == 0x0D) && (*(curr_ptr + 1) == 0x0A))) {
				curr_ptr++;
			}

			curr_ptr += 2;	/* Skip the enter line */

			continue;
		}
		/* This just content one enter line. */
		if (((*curr_ptr) == 0x0D) && ((*(curr_ptr + 1)) == 0x0A)) {
			curr_ptr += 2;
			continue;
		}
		/* printk(" curr_ptr1 = %s\n",curr_ptr); */
		memcpy(func_ind, curr_ptr, 3);


		if (strcmp((const char *)func_ind, "REG") == 0) {	/* REG */
			curr_ptr += 6;	/* Skip "REG(0x" or "DLY(" */
			gc0312_Init_Reg[i].op_code = gc0312_OP_CODE_REG;

			gc0312_Init_Reg[i].init_reg = strtol((const char *)curr_ptr, 16);
			curr_ptr += 5;	/* Skip "00, 0x" */

			gc0312_Init_Reg[i].init_val = strtol((const char *)curr_ptr, 16);
			curr_ptr += 4;	/* Skip "00);" */

		} else {	/* DLY */

			/* Need add delay for this setting. */
			curr_ptr += 4;
			gc0312_Init_Reg[i].op_code = gc0312_OP_CODE_DLY;

			gc0312_Init_Reg[i].init_reg = 0xFF;
			gc0312_Init_Reg[i].init_val = strtol((const char *)curr_ptr, 10);	/* Get the delay ticks, the delay should less then 50 */
		}
		i++;


		/* Skip to next line directly. */
		while (!((*curr_ptr == 0x0D) && (*(curr_ptr + 1) == 0x0A))) {
			curr_ptr++;
		}
		curr_ptr += 2;
	}

	/* (0xFFFF, 0xFFFF) means the end of initial setting. */
	gc0312_Init_Reg[i].op_code = gc0312_OP_CODE_END;
	gc0312_Init_Reg[i].init_reg = 0xFF;
	gc0312_Init_Reg[i].init_val = 0xFF;
	i++;
	/* for (j=0; j<i; j++) */
	printk("gc0312 %x  ==  %x\n", gc0312_Init_Reg[j].init_reg, gc0312_Init_Reg[j].init_val);

	/* Start apply the initial setting to sensor. */
#if 1
	for (j = 0; j < i; j++) {
		if (gc0312_Init_Reg[j].op_code == gc0312_OP_CODE_END) {	/* End of the setting. */
			printk("gc0312 REG OK -----------------END!\n");

			break;
		} else if (gc0312_Init_Reg[j].op_code == gc0312_OP_CODE_DLY) {
			msleep(gc0312_Init_Reg[j].init_val);	/* Delay */
			printk("gc0312 REG OK -----------------DLY!\n");
		} else if (gc0312_Init_Reg[j].op_code == gc0312_OP_CODE_REG) {
			GC0312_write_cmos_sensor(gc0312_Init_Reg[j].init_reg,
						 gc0312_Init_Reg[j].init_val);
			printk("gc0312 REG OK!--------1--------REG(0x%x,0x%x)\n",
			       gc0312_Init_Reg[j].init_reg, gc0312_Init_Reg[j].init_val);
			printk("gc0312 REG OK!--------2--------REG(0x%x,0x%x)\n",
			       gc0312_Init_Reg[j].init_reg, gc0312_Init_Reg[j].init_val);
			printk("gc0312 REG OK!--------3--------REG(0x%x,0x%x)\n",
			       gc0312_Init_Reg[j].init_reg, gc0312_Init_Reg[j].init_val);
		} else {
			printk("gc0312 REG ERROR!\n");
		}
	}
#endif
	printk("gc0312 ___%s____ end\n", __func__);
	return 1;
}

#endif

/*******************************************************************************
 * // Adapter for Winmo typedef
 ********************************************************************************/
#define WINMO_USE 0

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x, ...)
#define TEXT

kal_bool GC0312_MPEG4_encode_mode = KAL_FALSE;
kal_uint16 GC0312_dummy_pixels = 0, GC0312_dummy_lines = 0;
kal_bool GC0312_MODE_CAPTURE = KAL_FALSE;
kal_bool GC0312_NIGHT_MODE = KAL_FALSE;

kal_uint32 GC0312_isp_master_clock;
static kal_uint32 GC0312_g_fPV_PCLK = 26;

kal_uint8 GC0312_sensor_write_I2C_address = GC0312_WRITE_ID;
kal_uint8 GC0312_sensor_read_I2C_address = GC0312_READ_ID;

UINT8 GC0312PixelClockDivider = 0;

MSDK_SENSOR_CONFIG_STRUCT GC0312SensorConfigData;
SENSOR_EXIF_INFO_STRUCT GC0312ExifInfo;

#define GC0312_SET_PAGE0	GC0312_write_cmos_sensor(0xfe, 0x00)
#define GC0312_SET_PAGE1	GC0312_write_cmos_sensor(0xfe, 0x01)


/*************************************************************************
 * FUNCTION
 *	GC0312_SetShutter
 *
 * DESCRIPTION
 *	This function set e-shutter of GC0312 to change exposure time.
 *
 * PARAMETERS
 *   iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0312_Set_Shutter(kal_uint16 iShutter)
{
}				/* Set_GC0312_Shutter */


/*************************************************************************
 * FUNCTION
 *	GC0312_read_Shutter
 *
 * DESCRIPTION
 *	This function read e-shutter of GC0312 .
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 GC0312_Read_Shutter(void)
{
	kal_uint8 temp_reg1, temp_reg2, t_gain;
	/* kal_uint8 temp_reg1, temp_reg2,temp_reg3, temp_reg4,t_gain; */
	kal_uint32 shutter;
	/* kal_uint32 step, shutter_iso; */

	GC0312_write_cmos_sensor(0xfe, 0x00);
	temp_reg1 = GC0312_read_cmos_sensor(0x04);
	temp_reg2 = GC0312_read_cmos_sensor(0x03);

	shutter = (temp_reg1 & 0xFF) | (temp_reg2 << 8);
	GC0312ExifInfo.CapExposureTime = (kal_uint16) (65 * shutter / 1000);
	if (GC0312ExifInfo.CapExposureTime < 1)
		GC0312ExifInfo.CapExposureTime = 1;

	GC0312_write_cmos_sensor(0xfe, 0x00);
	t_gain = GC0312_read_cmos_sensor(0xef);

	printk("GC0312ExifInfo t_gain=%d\n", t_gain);

	if (t_gain > 0xd0)
		GC0312ExifInfo.RealISOValue = AE_ISO_100;
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	else if (t_gain > 0x60)
#else
	else if (t_gain > 0x70)
#endif
		GC0312ExifInfo.RealISOValue = AE_ISO_200;
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	else if (t_gain > 0x10)
#else
	else if (t_gain > 0x20)
#endif
		GC0312ExifInfo.RealISOValue = AE_ISO_400;
	else
		GC0312ExifInfo.RealISOValue = AE_ISO_800;

	/*
	   GC0312_write_cmos_sensor(0xfe,0x00);
	   analog_gain= GC0312_read_cmos_sensor(0x48);
	   pre_gain = GC0312_read_cmos_sensor(0x71);
	   post_gain = GC0312_read_cmos_sensor(0x72);
	   total_gain=analog_gain*pre_gain*post_gain;
	   printk("GC0312ExifInfo analog=%d,pre=%d,post=%d,total=%d",analog_gain,pre_gain,post_gain,total_gain);

	   if (total_gain <170000)
	   GC0312ExifInfo.RealISOValue = AE_ISO_100;
	   else if (total_gain <330000)
	   GC0312ExifInfo.RealISOValue = AE_ISO_200;
	   else if (total_gain <570000)
	   GC0312ExifInfo.RealISOValue = AE_ISO_400;
	   else
	   GC0312ExifInfo.RealISOValue = AE_ISO_800;
	 */

	/*
	   if (shutter_iso <40)
	   GC0312ExifInfo.RealISOValue = AE_ISO_100;
	   else if (shutter_iso<55)
	   GC0312ExifInfo.RealISOValue = AE_ISO_200;
	   else if (shutter_iso<105)
	   GC0312ExifInfo.RealISOValue = AE_ISO_400;
	   else
	   GC0312ExifInfo.RealISOValue = AE_ISO_800;
	 */

	return shutter;
}				/* GC0312_read_shutter */


/*************************************************************************
 * FUNCTION
 *	GC0312_write_reg
 *
 * DESCRIPTION
 *	This function set the register of GC0312.
 *
 * PARAMETERS
 *	addr : the register index of GC0312
 *  para : setting parameter of the specified register of GC0312
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0312_write_reg(kal_uint32 addr, kal_uint32 para)
{
	GC0312_write_cmos_sensor(addr, para);
}				/* GC0312_write_reg() */


/*************************************************************************
 * FUNCTION
 *	GC0312_read_cmos_sensor
 *
 * DESCRIPTION
 *	This function read parameter of specified register from GC0312.
 *
 * PARAMETERS
 *	addr : the register index of GC0312
 *
 * RETURNS
 *	the data that read from GC0312
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint32 GC0312_read_reg(kal_uint32 addr)
{
	return GC0312_read_cmos_sensor(addr);
}				/* OV7670_read_reg() */


/*************************************************************************
* FUNCTION
*	GC0312_awb_enable
*
* DESCRIPTION
*	This function enable or disable the awb (Auto White Balance).
*
* PARAMETERS
*	1. kal_bool : KAL_TRUE - enable awb, KAL_FALSE - disable awb.
*
* RETURNS
*	kal_bool : It means set awb right or not.
*
*************************************************************************/
static void GC0312_awb_enable(kal_bool enalbe)
{
	kal_uint16 temp_AWB_reg = 0;

	temp_AWB_reg = GC0312_read_cmos_sensor(0x42);

	if (enalbe) {
		GC0312_write_cmos_sensor(0x42, (temp_AWB_reg | 0x02));
	} else {
		GC0312_write_cmos_sensor(0x42, (temp_AWB_reg & (~0x02)));
	}

}


/*************************************************************************
* FUNCTION
*	GC0312_GAMMA_Select
*
* DESCRIPTION
*	This function is served for FAE to select the appropriate GAMMA curve.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0312GammaSelect(kal_uint32 GammaLvl)
{
	switch (GammaLvl) {
	case GC0312_RGB_Gamma_m1:	/* smallest gamma curve */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xbf, 0x06);
		GC0312_write_cmos_sensor(0xc0, 0x12);
		GC0312_write_cmos_sensor(0xc1, 0x22);
		GC0312_write_cmos_sensor(0xc2, 0x35);
		GC0312_write_cmos_sensor(0xc3, 0x4b);
		GC0312_write_cmos_sensor(0xc4, 0x5f);
		GC0312_write_cmos_sensor(0xc5, 0x72);
		GC0312_write_cmos_sensor(0xc6, 0x8d);
		GC0312_write_cmos_sensor(0xc7, 0xa4);
		GC0312_write_cmos_sensor(0xc8, 0xb8);
		GC0312_write_cmos_sensor(0xc9, 0xc8);
		GC0312_write_cmos_sensor(0xca, 0xd4);
		GC0312_write_cmos_sensor(0xcb, 0xde);
		GC0312_write_cmos_sensor(0xcc, 0xe6);
		GC0312_write_cmos_sensor(0xcd, 0xf1);
		GC0312_write_cmos_sensor(0xce, 0xf8);
		GC0312_write_cmos_sensor(0xcf, 0xfd);
		break;
	case GC0312_RGB_Gamma_m2:
		GC0312_write_cmos_sensor(0xBF, 0x08);
		GC0312_write_cmos_sensor(0xc0, 0x0F);
		GC0312_write_cmos_sensor(0xc1, 0x21);
		GC0312_write_cmos_sensor(0xc2, 0x32);
		GC0312_write_cmos_sensor(0xc3, 0x43);
		GC0312_write_cmos_sensor(0xc4, 0x50);
		GC0312_write_cmos_sensor(0xc5, 0x5E);
		GC0312_write_cmos_sensor(0xc6, 0x78);
		GC0312_write_cmos_sensor(0xc7, 0x90);
		GC0312_write_cmos_sensor(0xc8, 0xA6);
		GC0312_write_cmos_sensor(0xc9, 0xB9);
		GC0312_write_cmos_sensor(0xcA, 0xC9);
		GC0312_write_cmos_sensor(0xcB, 0xD6);
		GC0312_write_cmos_sensor(0xcC, 0xE0);
		GC0312_write_cmos_sensor(0xcD, 0xEE);
		GC0312_write_cmos_sensor(0xcE, 0xF8);
		GC0312_write_cmos_sensor(0xcF, 0xFF);
		break;

	case GC0312_RGB_Gamma_m3:
		GC0312_write_cmos_sensor(0xbf, 0x0b);
		GC0312_write_cmos_sensor(0xc0, 0x17);
		GC0312_write_cmos_sensor(0xc1, 0x2a);
		GC0312_write_cmos_sensor(0xc2, 0x41);
		GC0312_write_cmos_sensor(0xc3, 0x54);
		GC0312_write_cmos_sensor(0xc4, 0x66);
		GC0312_write_cmos_sensor(0xc5, 0x74);
		GC0312_write_cmos_sensor(0xc6, 0x8c);
		GC0312_write_cmos_sensor(0xc7, 0xa3);
		GC0312_write_cmos_sensor(0xc8, 0xb5);
		GC0312_write_cmos_sensor(0xc9, 0xc4);
		GC0312_write_cmos_sensor(0xca, 0xd0);
		GC0312_write_cmos_sensor(0xcb, 0xdb);
		GC0312_write_cmos_sensor(0xcc, 0xe5);
		GC0312_write_cmos_sensor(0xcd, 0xf0);
		GC0312_write_cmos_sensor(0xce, 0xf7);
		GC0312_write_cmos_sensor(0xcf, 0xff);
		break;

	case GC0312_RGB_Gamma_m4:
		GC0312_write_cmos_sensor(0xBF, 0x0E);
		GC0312_write_cmos_sensor(0xc0, 0x1C);
		GC0312_write_cmos_sensor(0xc1, 0x34);
		GC0312_write_cmos_sensor(0xc2, 0x48);
		GC0312_write_cmos_sensor(0xc3, 0x5A);
		GC0312_write_cmos_sensor(0xc4, 0x6B);
		GC0312_write_cmos_sensor(0xc5, 0x7B);
		GC0312_write_cmos_sensor(0xc6, 0x95);
		GC0312_write_cmos_sensor(0xc7, 0xAB);
		GC0312_write_cmos_sensor(0xc8, 0xBF);
		GC0312_write_cmos_sensor(0xc9, 0xCE);
		GC0312_write_cmos_sensor(0xcA, 0xD9);
		GC0312_write_cmos_sensor(0xcB, 0xE4);
		GC0312_write_cmos_sensor(0xcC, 0xEC);
		GC0312_write_cmos_sensor(0xcD, 0xF7);
		GC0312_write_cmos_sensor(0xcE, 0xFD);
		GC0312_write_cmos_sensor(0xcF, 0xFF);
		break;

	case GC0312_RGB_Gamma_m5:
		GC0312_write_cmos_sensor(0xBF, 0x10);
		GC0312_write_cmos_sensor(0xc0, 0x20);
		GC0312_write_cmos_sensor(0xc1, 0x38);
		GC0312_write_cmos_sensor(0xc2, 0x4E);
		GC0312_write_cmos_sensor(0xc3, 0x63);
		GC0312_write_cmos_sensor(0xc4, 0x76);
		GC0312_write_cmos_sensor(0xc5, 0x87);
		GC0312_write_cmos_sensor(0xc6, 0xA2);
		GC0312_write_cmos_sensor(0xc7, 0xB8);
		GC0312_write_cmos_sensor(0xc8, 0xCA);
		GC0312_write_cmos_sensor(0xc9, 0xD8);
		GC0312_write_cmos_sensor(0xcA, 0xE3);
		GC0312_write_cmos_sensor(0xcB, 0xEB);
		GC0312_write_cmos_sensor(0xcC, 0xF0);
		GC0312_write_cmos_sensor(0xcD, 0xF8);
		GC0312_write_cmos_sensor(0xcE, 0xFD);
		GC0312_write_cmos_sensor(0xcF, 0xFF);
		break;

	case GC0312_RGB_Gamma_m6:	/* largest gamma curve */
		GC0312_write_cmos_sensor(0xBF, 0x14);
		GC0312_write_cmos_sensor(0xc0, 0x28);
		GC0312_write_cmos_sensor(0xc1, 0x44);
		GC0312_write_cmos_sensor(0xc2, 0x5D);
		GC0312_write_cmos_sensor(0xc3, 0x72);
		GC0312_write_cmos_sensor(0xc4, 0x86);
		GC0312_write_cmos_sensor(0xc5, 0x95);
		GC0312_write_cmos_sensor(0xc6, 0xB1);
		GC0312_write_cmos_sensor(0xc7, 0xC6);
		GC0312_write_cmos_sensor(0xc8, 0xD5);
		GC0312_write_cmos_sensor(0xc9, 0xE1);
		GC0312_write_cmos_sensor(0xcA, 0xEA);
		GC0312_write_cmos_sensor(0xcB, 0xF1);
		GC0312_write_cmos_sensor(0xcC, 0xF5);
		GC0312_write_cmos_sensor(0xcD, 0xFB);
		GC0312_write_cmos_sensor(0xcE, 0xFE);
		GC0312_write_cmos_sensor(0xcF, 0xFF);
		break;
	case GC0312_RGB_Gamma_night:	/* Gamma for night mode */
		GC0312_write_cmos_sensor(0xBF, 0x0B);
		GC0312_write_cmos_sensor(0xc0, 0x16);
		GC0312_write_cmos_sensor(0xc1, 0x29);
		GC0312_write_cmos_sensor(0xc2, 0x3C);
		GC0312_write_cmos_sensor(0xc3, 0x4F);
		GC0312_write_cmos_sensor(0xc4, 0x5F);
		GC0312_write_cmos_sensor(0xc5, 0x6F);
		GC0312_write_cmos_sensor(0xc6, 0x8A);
		GC0312_write_cmos_sensor(0xc7, 0x9F);
		GC0312_write_cmos_sensor(0xc8, 0xB4);
		GC0312_write_cmos_sensor(0xc9, 0xC6);
		GC0312_write_cmos_sensor(0xcA, 0xD3);
		GC0312_write_cmos_sensor(0xcB, 0xDD);
		GC0312_write_cmos_sensor(0xcC, 0xE5);
		GC0312_write_cmos_sensor(0xcD, 0xF1);
		GC0312_write_cmos_sensor(0xcE, 0xFA);
		GC0312_write_cmos_sensor(0xcF, 0xFF);
		break;
	default:
		/* GC0312_RGB_Gamma_m3 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xbf, 0x0b);
		GC0312_write_cmos_sensor(0xc0, 0x17);
		GC0312_write_cmos_sensor(0xc1, 0x2a);
		GC0312_write_cmos_sensor(0xc2, 0x41);
		GC0312_write_cmos_sensor(0xc3, 0x54);
		GC0312_write_cmos_sensor(0xc4, 0x66);
		GC0312_write_cmos_sensor(0xc5, 0x74);
		GC0312_write_cmos_sensor(0xc6, 0x8c);
		GC0312_write_cmos_sensor(0xc7, 0xa3);
		GC0312_write_cmos_sensor(0xc8, 0xb5);
		GC0312_write_cmos_sensor(0xc9, 0xc4);
		GC0312_write_cmos_sensor(0xca, 0xd0);
		GC0312_write_cmos_sensor(0xcb, 0xdb);
		GC0312_write_cmos_sensor(0xcc, 0xe5);
		GC0312_write_cmos_sensor(0xcd, 0xf0);
		GC0312_write_cmos_sensor(0xce, 0xf7);
		GC0312_write_cmos_sensor(0xcf, 0xff);
		break;
	}
}


/*************************************************************************
 * FUNCTION
 *	GC0312_config_window
 *
 * DESCRIPTION
 *	This function config the hardware window of GC0312 for getting specified
 *  data of that window.
 *
 * PARAMETERS
 *	start_x : start column of the interested window
 *  start_y : start row of the interested window
 *  width  : column widht of the itnerested window
 *  height : row depth of the itnerested window
 *
 * RETURNS
 *	the data that read from GC0312
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0312_config_window(kal_uint16 startx, kal_uint16 starty, kal_uint16 width, kal_uint16 height)
{
}				/* GC0312_config_window */


/*************************************************************************
 * FUNCTION
 *	GC0312_SetGain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *   iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 GC0312_SetGain(kal_uint16 iGain)
{
	return iGain;
}


/*************************************************************************
 * FUNCTION
 *	GC0312_NightMode
 *
 * DESCRIPTION
 *	This function night mode of GC0312.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0312NightMode(kal_bool bEnable)
{
#if 0
	if (bEnable) {
		GC0312_write_cmos_sensor(0xfe, 0x01);
		if (GC0312_MPEG4_encode_mode == KAL_TRUE)
			GC0312_write_cmos_sensor(0x3c, 0x30);
		else
			GC0312_write_cmos_sensor(0x3c, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		/* GC0312GammaSelect(GC0312_RGB_Gamma_night); */
		GC0312_NIGHT_MODE = KAL_TRUE;
	} else {
		GC0312_write_cmos_sensor(0xfe, 0x01);
		if (GC0312_MPEG4_encode_mode == KAL_TRUE) {
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x0d, 0xf8);
			GC0312_write_cmos_sensor(0xfe, 0x00);
			GC0312_write_cmos_sensor(0x05, 0x01);
			GC0312_write_cmos_sensor(0x06, 0x18);
			GC0312_write_cmos_sensor(0x07, 0x00);
			GC0312_write_cmos_sensor(0x08, 0x10);
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x25, 0x00);
			GC0312_write_cmos_sensor(0x26, 0x9a);
			GC0312_write_cmos_sensor(0x27, 0x01);
			GC0312_write_cmos_sensor(0x28, 0x34);
			GC0312_write_cmos_sensor(0x29, 0x02);
			GC0312_write_cmos_sensor(0x2a, 0x68);
			GC0312_write_cmos_sensor(0x2b, 0x03);
			GC0312_write_cmos_sensor(0x2c, 0x02);
			GC0312_write_cmos_sensor(0x2d, 0x03);
			GC0312_write_cmos_sensor(0x2e, 0x9c);
			GC0312_write_cmos_sensor(0x2f, 0x04);
			GC0312_write_cmos_sensor(0x30, 0xd0);
			GC0312_write_cmos_sensor(0x31, 0x04);
			GC0312_write_cmos_sensor(0x32, 0xd0);
			GC0312_write_cmos_sensor(0x33, 0x06);
			GC0312_write_cmos_sensor(0x34, 0x04);
			GC0312_write_cmos_sensor(0x35, 0x0d);
			GC0312_write_cmos_sensor(0x36, 0x14);
			GC0312_write_cmos_sensor(0x37, 0x18);
			GC0312_write_cmos_sensor(0x38, 0x20);
			GC0312_write_cmos_sensor(0x39, 0x30);
			GC0312_write_cmos_sensor(0x3a, 0x30);
			GC0312_write_cmos_sensor(0x3b, 0x30);
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x13, 0x35);
			GC0312_write_cmos_sensor(0xfe, 0x00);
		} else {
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x0d, 0xf8);	/*new exp 3 */
			GC0312_write_cmos_sensor(0xfe, 0x00);
			GC0312_write_cmos_sensor(0x05, 0x01);	/*01 */
			GC0312_write_cmos_sensor(0x06, 0x18);	/*89 */
			GC0312_write_cmos_sensor(0x07, 0x00);
			GC0312_write_cmos_sensor(0x08, 0x10);	/*22 */
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x25, 0x00);
			GC0312_write_cmos_sensor(0x26, 0x9a);	/*8a   //step */
			GC0312_write_cmos_sensor(0x27, 0x01);
			GC0312_write_cmos_sensor(0x28, 0x34);	/*2step  level 1   30fps */
			GC0312_write_cmos_sensor(0x29, 0x02);
			GC0312_write_cmos_sensor(0x2a, 0x68);	/*4step level 2  25 fps */
			GC0312_write_cmos_sensor(0x2b, 0x04);
			GC0312_write_cmos_sensor(0x2c, 0xd0);	/*5step level 3  20 */
			GC0312_write_cmos_sensor(0x2d, 0x06);
			GC0312_write_cmos_sensor(0x2e, 0x04);	/*10step level 4 17 */
			GC0312_write_cmos_sensor(0x2f, 0x09);
			GC0312_write_cmos_sensor(0x30, 0x04);	/*14step  level 5 12fps */
			GC0312_write_cmos_sensor(0x31, 0x09);
			GC0312_write_cmos_sensor(0x32, 0x04);	/*14 step  level 6 10fps */
			GC0312_write_cmos_sensor(0x33, 0x09);
			GC0312_write_cmos_sensor(0x34, 0x08);	/* level 7 */
			GC0312_write_cmos_sensor(0x35, 0x10);	/*2.25x level 1 0x08 */
			GC0312_write_cmos_sensor(0x36, 0x12);	/*3.5x   level 2 0x0d */
			GC0312_write_cmos_sensor(0x37, 0x12);	/*6x   level 3 0x10 */
			GC0312_write_cmos_sensor(0x38, 0x14);	/*  level 4 */
			GC0312_write_cmos_sensor(0x39, 0x18);	/*  level 5 */
			GC0312_write_cmos_sensor(0x3a, 0x28);	/*  level 6 */
			GC0312_write_cmos_sensor(0x3b, 0x28);	/* level 7 3.5x */
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x13, 0x39);
			GC0312_write_cmos_sensor(0xfe, 0x00);
		}

		GC0312_write_cmos_sensor(0xfe, 0x00);
		/* GC0312GammaSelect(GC0312_RGB_Gamma_m3); */
		GC0312_NIGHT_MODE = KAL_FALSE;
	}
#endif
}				/* GC0312_NightMode */

/*************************************************************************
* FUNCTION
*	GC0312_Sensor_Init
*
* DESCRIPTION
*	This function apply all of the initial setting to sensor.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
*************************************************************************/
void GC0312_Sensor_Init(void)
{
	GC0312_write_cmos_sensor(0xfe, 0xf0);
	GC0312_write_cmos_sensor(0xfe, 0xf0);
	GC0312_write_cmos_sensor(0xfe, 0x00);
	GC0312_write_cmos_sensor(0xfc, 0x0e);
	GC0312_write_cmos_sensor(0xfc, 0x0e);
	GC0312_write_cmos_sensor(0xf2, 0x07);
	GC0312_write_cmos_sensor(0xf3, 0x00);	/* output_disable */
	GC0312_write_cmos_sensor(0xf7, 0x1b);
	GC0312_write_cmos_sensor(0xf8, 0x04);
	GC0312_write_cmos_sensor(0xf9, 0x0e);
	GC0312_write_cmos_sensor(0xfa, 0x11);

	/* /////////////////////////////////////////////// */
	/* ///////////////  CISCTL reg   ///////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x00, 0x2f);
	GC0312_write_cmos_sensor(0x01, 0x0f);
	GC0312_write_cmos_sensor(0x02, 0x04);
	GC0312_write_cmos_sensor(0x03, 0x02);
	GC0312_write_cmos_sensor(0x04, 0x68);
	GC0312_write_cmos_sensor(0x09, 0x00);
	GC0312_write_cmos_sensor(0x0a, 0x00);
	GC0312_write_cmos_sensor(0x0b, 0x00);
	GC0312_write_cmos_sensor(0x0c, 0x04);
	GC0312_write_cmos_sensor(0x0d, 0x01);
	GC0312_write_cmos_sensor(0x0e, 0xe8);
	GC0312_write_cmos_sensor(0x0f, 0x02);
	GC0312_write_cmos_sensor(0x10, 0x88);
	GC0312_write_cmos_sensor(0x16, 0x00);
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x17, 0x17);	/* camera rotate for some device */
#else
	GC0312_write_cmos_sensor(0x17, 0x14);
#endif
	GC0312_write_cmos_sensor(0x18, 0x1a);
	GC0312_write_cmos_sensor(0x19, 0x14);
	GC0312_write_cmos_sensor(0x1b, 0x48);
	GC0312_write_cmos_sensor(0x1c, 0x6c);	/* 1c travis 20140929 */
	GC0312_write_cmos_sensor(0x1e, 0x6b);
	GC0312_write_cmos_sensor(0x1f, 0x28);
	GC0312_write_cmos_sensor(0x20, 0x8b);	/* 89 travis 20140801 */
	GC0312_write_cmos_sensor(0x21, 0x49);
	GC0312_write_cmos_sensor(0x22, 0xd0);	/* b0 travis 20140929 */
	GC0312_write_cmos_sensor(0x23, 0x04);
	GC0312_write_cmos_sensor(0x24, 0x16);
	GC0312_write_cmos_sensor(0x34, 0x20);

	/* /////////////////////////////////////////////// */
	/* //////////////////   BLK       //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x26, 0x23);
	GC0312_write_cmos_sensor(0x28, 0xff);
	GC0312_write_cmos_sensor(0x29, 0x00);
	GC0312_write_cmos_sensor(0x32, 0x04);	/* 00  travis 20140929 */
	GC0312_write_cmos_sensor(0x33, 0x10);
	GC0312_write_cmos_sensor(0x37, 0x20);
	GC0312_write_cmos_sensor(0x38, 0x10);
	GC0312_write_cmos_sensor(0x47, 0x80);
	GC0312_write_cmos_sensor(0x4e, 0x66);
	GC0312_write_cmos_sensor(0xa8, 0x02);
	GC0312_write_cmos_sensor(0xa9, 0x80);

	/* /////////////////////////////////////////////// */
	/* ////////////////      ISP reg   /////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x40, 0xff);
	GC0312_write_cmos_sensor(0x41, 0x25);	/* 21 becky */
	GC0312_write_cmos_sensor(0x42, 0xcf);
	GC0312_write_cmos_sensor(0x44, 0x02);
	GC0312_write_cmos_sensor(0x45, 0xa8);
	GC0312_write_cmos_sensor(0x46, 0x02);
	GC0312_write_cmos_sensor(0x4a, 0x11);
	GC0312_write_cmos_sensor(0x4b, 0x01);
	GC0312_write_cmos_sensor(0x4c, 0x20);
	GC0312_write_cmos_sensor(0x4d, 0x05);
	GC0312_write_cmos_sensor(0x4f, 0x01);
	GC0312_write_cmos_sensor(0x50, 0x01);
	GC0312_write_cmos_sensor(0x55, 0x01);
	GC0312_write_cmos_sensor(0x56, 0xe0);
	GC0312_write_cmos_sensor(0x57, 0x02);
	GC0312_write_cmos_sensor(0x58, 0x80);
	/*GC0312_write_cmos_sensor(0xde, 0xa8); */

	/* /////////////////////////////////////////////// */
	/* /////////////////   GAIN   //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x70, 0x70);
	GC0312_write_cmos_sensor(0x5a, 0x84);
	GC0312_write_cmos_sensor(0x5b, 0xc9);
	GC0312_write_cmos_sensor(0x5c, 0xed);
	GC0312_write_cmos_sensor(0x77, 0x54);	/*40  20160406 */
	GC0312_write_cmos_sensor(0x78, 0x40);	/*58  20160406 */
	GC0312_write_cmos_sensor(0x79, 0x62);	/*a9  20160406 */

	/* /////////////////////////////////////////////// */
	/* /////////////////   DNDD  ///////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x82, 0x01);
	GC0312_write_cmos_sensor(0x83, 0x01);
	GC0312_write_cmos_sensor(0x89, 0x0e);
	GC0312_write_cmos_sensor(0x8a, 0x0e);
	GC0312_write_cmos_sensor(0x8b, 0x0e);

	/* /////////////////////////////////////////////// */
	/* ////////////////   EEINTP  //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x8f, 0xaa);
	GC0312_write_cmos_sensor(0x90, 0x8c);
	GC0312_write_cmos_sensor(0x91, 0x90);
	GC0312_write_cmos_sensor(0x92, 0x08);
	GC0312_write_cmos_sensor(0x93, 0x08);
	GC0312_write_cmos_sensor(0x94, 0x04);	/*08  20160406 */
	GC0312_write_cmos_sensor(0x95, 0x44);
	GC0312_write_cmos_sensor(0x96, 0xf0);

	/* /////////////////////////////////////////////// */
	/* ///////////////////  ASDE  //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x00);

	GC0312_write_cmos_sensor(0x9a, 0x20);
	GC0312_write_cmos_sensor(0x9b, 0x60);
	GC0312_write_cmos_sensor(0x9c, 0x40);
	GC0312_write_cmos_sensor(0x9d, 0x80);

	GC0312_write_cmos_sensor(0xa1, 0x30);
	GC0312_write_cmos_sensor(0xa2, 0x44);
	GC0312_write_cmos_sensor(0xa4, 0x26);	/*20  20160406 */
	GC0312_write_cmos_sensor(0xa5, 0x38);	/*3c  20160406 */
	GC0312_write_cmos_sensor(0xaa, 0x2a);	/*30  20160406 */
	GC0312_write_cmos_sensor(0xac, 0x00);

	/* /////////////////////////////////////////////// */
	/* /////////////////   GAMMA   /////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x00);
	GC0312_write_cmos_sensor(0xbf, 0x00);	/*06  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0xc0, 0x0b);	/*14  20160406 */
	GC0312_write_cmos_sensor(0xc1, 0x27);	/*2b  20160406 */
	GC0312_write_cmos_sensor(0xc2, 0x45);	/*46  20160406 */
	GC0312_write_cmos_sensor(0xc3, 0x5d);
	GC0312_write_cmos_sensor(0xc4, 0x70);
	GC0312_write_cmos_sensor(0xc5, 0x80);	/*7b  20160406 */
	GC0312_write_cmos_sensor(0xc6, 0x98);	/*93  20160406 */
	GC0312_write_cmos_sensor(0xc7, 0xac);	/*a5  20160406 */
	GC0312_write_cmos_sensor(0xc8, 0xbc);	/*b7  20160406 */
	GC0312_write_cmos_sensor(0xc9, 0xca);
	GC0312_write_cmos_sensor(0xca, 0xd7);
	GC0312_write_cmos_sensor(0xcb, 0xe1);
	GC0312_write_cmos_sensor(0xcc, 0xe9);
	GC0312_write_cmos_sensor(0xcd, 0xf3);
	GC0312_write_cmos_sensor(0xce, 0xfa);
#else
	GC0312_write_cmos_sensor(0xc0, 0x0f);	/*14  20160406 */
	GC0312_write_cmos_sensor(0xc1, 0x26);	/*2b  20160406 */
	GC0312_write_cmos_sensor(0xc2, 0x43);	/*46  20160406 */
	GC0312_write_cmos_sensor(0xc3, 0x5b);
	GC0312_write_cmos_sensor(0xc4, 0x6e);
	GC0312_write_cmos_sensor(0xc5, 0x7e);	/*7b  20160406 */
	GC0312_write_cmos_sensor(0xc6, 0x97);	/*93  20160406 */
	GC0312_write_cmos_sensor(0xc7, 0xaa);	/*a5  20160406 */
	GC0312_write_cmos_sensor(0xc8, 0xb9);	/*b7  20160406 */
	GC0312_write_cmos_sensor(0xc9, 0xc6);
	GC0312_write_cmos_sensor(0xca, 0xd1);
	GC0312_write_cmos_sensor(0xcb, 0xdb);
	GC0312_write_cmos_sensor(0xcc, 0xe1);
	GC0312_write_cmos_sensor(0xcd, 0xee);
	GC0312_write_cmos_sensor(0xce, 0xf8);
#endif
	GC0312_write_cmos_sensor(0xcf, 0xfe);


	GC0312_write_cmos_sensor(0xfe, 0x00);
	GC0312_write_cmos_sensor(0x5f, 0x00);	/*12  20160406 */
	GC0312_write_cmos_sensor(0x60, 0x15);	/*22  20160406 */
	GC0312_write_cmos_sensor(0x61, 0x3d);	/*31  20160406 */
	GC0312_write_cmos_sensor(0x62, 0x58);	/*3d  20160406 */
	GC0312_write_cmos_sensor(0x63, 0x6f);	/*49  20160406 */
	GC0312_write_cmos_sensor(0x64, 0x80);	/*54  20160406 */
	GC0312_write_cmos_sensor(0x65, 0x92);	/*5e  20160406 */
	GC0312_write_cmos_sensor(0x66, 0xa8);	/*71  20160406 */
	GC0312_write_cmos_sensor(0x67, 0xb9);	/*82  20160406 */
	GC0312_write_cmos_sensor(0x68, 0xc7);	/*92  20160406 */
	GC0312_write_cmos_sensor(0x69, 0xd1);	/*a0  20160406 */
	GC0312_write_cmos_sensor(0x6a, 0xda);	/*ae  20160406 */
	GC0312_write_cmos_sensor(0x6b, 0xe3);	/*ba  20160406 */
	GC0312_write_cmos_sensor(0x6c, 0xe8);	/*c6  20160406 */
	GC0312_write_cmos_sensor(0x6d, 0xf2);	/*d8  20160406 */
	GC0312_write_cmos_sensor(0x6e, 0xfb);	/*e4  20160406 */
	GC0312_write_cmos_sensor(0x6f, 0xfd);	/*ef  20160406 */

	GC0312_write_cmos_sensor(0xb1, 0x80);	/*b0  20160406 */
	GC0312_write_cmos_sensor(0xb2, 0x90);	/*d0  20160406 */
	GC0312_write_cmos_sensor(0xb3, 0x0b);	/*80  20160406 */
	GC0312_write_cmos_sensor(0xb4, 0x10);	/*90  20160406 */

	/* /////////////////////////////////////////// */
	/* /////////////////   YCP  ////////////////// */
	/* /////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xd0, 0x40);
	GC0312_write_cmos_sensor(0xd1, 0x2c);	/*30  20160406 */
	GC0312_write_cmos_sensor(0xd2, 0x28);	/*2a  20160406 */
	GC0312_write_cmos_sensor(0xd3, 0x40);	/*42  20160406 */
	GC0312_write_cmos_sensor(0xd5, 0x00);
	GC0312_write_cmos_sensor(0xd6, 0xf2);
	GC0312_write_cmos_sensor(0xd7, 0x1b);
	GC0312_write_cmos_sensor(0xd8, 0x18);
	GC0312_write_cmos_sensor(0xde, 0xa6);	/*add  20160406 */
	GC0312_write_cmos_sensor(0xdd, 0x03);

	/* /////////////////////////////////////////// */
	/* //////////////////   AEC   //////////////// */
	/* /////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x01);
	GC0312_write_cmos_sensor(0x05, 0x30);
	GC0312_write_cmos_sensor(0x06, 0x75);
	GC0312_write_cmos_sensor(0x07, 0x40);
	GC0312_write_cmos_sensor(0x08, 0xb0);
	GC0312_write_cmos_sensor(0x0a, 0xc5);
	GC0312_write_cmos_sensor(0x0b, 0x11);
	GC0312_write_cmos_sensor(0x0c, 0x00);
	GC0312_write_cmos_sensor(0x12, 0x52);
	GC0312_write_cmos_sensor(0x13, 0x37);	/*39  20160406 */
	GC0312_write_cmos_sensor(0x18, 0x95);
	GC0312_write_cmos_sensor(0x19, 0x96);
	GC0312_write_cmos_sensor(0x1e, 0x31);	/*add by lu 0305 */
	GC0312_write_cmos_sensor(0x1f, 0x20);	/*20 */
	GC0312_write_cmos_sensor(0x20, 0xc0);
	GC0312_write_cmos_sensor(0x3e, 0x40);
	GC0312_write_cmos_sensor(0x3f, 0x57);
	GC0312_write_cmos_sensor(0x40, 0x7d);
	GC0312_write_cmos_sensor(0x03, 0x60);
	GC0312_write_cmos_sensor(0x44, 0x02);

	/* /////////////////////////////////////////////// */
	/* //////////////////   AWB   //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x1c, 0x91);
	GC0312_write_cmos_sensor(0x21, 0x15);
	GC0312_write_cmos_sensor(0x50, 0x80);
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x56, 0x0d);
	GC0312_write_cmos_sensor(0x58, 0x06);	/*add  20160406 */
#else
	GC0312_write_cmos_sensor(0x56, 0x0c);	/*04  20160406 */
	GC0312_write_cmos_sensor(0x58, 0x04);	/*add  20160406 */
#endif
	GC0312_write_cmos_sensor(0x59, 0x00);
	GC0312_write_cmos_sensor(0x5b, 0x02);
	GC0312_write_cmos_sensor(0x61, 0x96);
	GC0312_write_cmos_sensor(0x62, 0xc6);
	GC0312_write_cmos_sensor(0x63, 0x14);	/*M1 *//*d0  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x64, 0xb5);
#else
	GC0312_write_cmos_sensor(0x64, 0xa0);	/*M2 */
#endif
	GC0312_write_cmos_sensor(0x65, 0x06);
	GC0312_write_cmos_sensor(0x66, 0x06);
	GC0312_write_cmos_sensor(0x67, 0x04);
	GC0312_write_cmos_sensor(0x69, 0x08);
	GC0312_write_cmos_sensor(0x6a, 0x80);	/*03 outdoor thd 20160406 */
	GC0312_write_cmos_sensor(0x6b, 0x01);
	GC0312_write_cmos_sensor(0x6c, 0x00);
	GC0312_write_cmos_sensor(0x6d, 0x02);
	GC0312_write_cmos_sensor(0x6e, 0x00);
	GC0312_write_cmos_sensor(0x6f, 0x80);
	GC0312_write_cmos_sensor(0x76, 0x80);
	GC0312_write_cmos_sensor(0x77, 0x60);	/*45  20160406 */
	GC0312_write_cmos_sensor(0x78, 0xf0);
	GC0312_write_cmos_sensor(0x79, 0x75);
	GC0312_write_cmos_sensor(0x7a, 0x40);
	GC0312_write_cmos_sensor(0x7b, 0x5c);	/*50  20160406 */
	GC0312_write_cmos_sensor(0x7c, 0x08);	/*0c  20160406 */
	GC0312_write_cmos_sensor(0xfe, 0x01);
	GC0312_write_cmos_sensor(0x90, 0x00);	/*cb  20160406 */
	GC0312_write_cmos_sensor(0x91, 0x00);	/*be  20160406 */
	GC0312_write_cmos_sensor(0x92, 0xfa);	/*f0  20160406 */
	GC0312_write_cmos_sensor(0x93, 0xe1);	/*cc  20160406 */
	GC0312_write_cmos_sensor(0x95, 0x21);	/*1a  20160406 */
	GC0312_write_cmos_sensor(0x96, 0xfb);	/*f0  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x97, 0x47);
#else
	GC0312_write_cmos_sensor(0x97, 0x49);	/*43  20160406 */
#endif
	GC0312_write_cmos_sensor(0x98, 0x22);	/*1a  20160406 */
	GC0312_write_cmos_sensor(0x9a, 0x4a);	/*43  20160406 */
	GC0312_write_cmos_sensor(0x9b, 0x39);	/*1a  20160406 */
	GC0312_write_cmos_sensor(0x9c, 0x94);	/*c4  20160406 */
	GC0312_write_cmos_sensor(0x9d, 0x51);	/*44  20160406 */
	GC0312_write_cmos_sensor(0x9f, 0x00);	/*c7  20160406 */
	GC0312_write_cmos_sensor(0xa0, 0x00);	/*c8  20160406 */
	GC0312_write_cmos_sensor(0xa1, 0x00);
	GC0312_write_cmos_sensor(0xa2, 0x00);
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x86, 0x6a);	/*60 */
#else
	GC0312_write_cmos_sensor(0x86, 0x60);	/*00  20160406 */
#endif
	GC0312_write_cmos_sensor(0x87, 0x58);	/*00  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x88, 0x41);	/*36 */
	GC0312_write_cmos_sensor(0x89, 0x16);	/*22 */
#else
	GC0312_write_cmos_sensor(0x88, 0x36);	/*00  20160406 */
	GC0312_write_cmos_sensor(0x89, 0x22);	/*00  20160406 */
#endif
	GC0312_write_cmos_sensor(0xa4, 0x00);	/*b4  20160406 */
	GC0312_write_cmos_sensor(0xa5, 0x00);	/*a2  20160406 */
	GC0312_write_cmos_sensor(0xa6, 0xd7);	/*b4  20160406 */
	GC0312_write_cmos_sensor(0xa7, 0xab);	/*95  20160406 */
	GC0312_write_cmos_sensor(0xa9, 0xd9);	/*bc  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0xaa, 0xa2);	/*a0 */
#else
	GC0312_write_cmos_sensor(0xaa, 0xa0);	/*95  20160406 */
#endif
	GC0312_write_cmos_sensor(0xab, 0xc1);	/*ab  20160406 */
	GC0312_write_cmos_sensor(0xac, 0xa7);	/*8d  20160406 */
	GC0312_write_cmos_sensor(0xae, 0xa2);	/*c8  20160406 */
	GC0312_write_cmos_sensor(0xaf, 0x94);	/*ab  20160406 */
	GC0312_write_cmos_sensor(0xb0, 0xe3);	/*c8  20160406 */
	GC0312_write_cmos_sensor(0xb1, 0x9e);	/*97  20160406 */
	GC0312_write_cmos_sensor(0xb3, 0x00);	/*b7  20160406 */
	GC0312_write_cmos_sensor(0xb4, 0x00);	/*7f  20160406 */
	GC0312_write_cmos_sensor(0xb5, 0x00);
	GC0312_write_cmos_sensor(0xb6, 0x00);
	GC0312_write_cmos_sensor(0x8b, 0xcb);	/*00  20160406 */
	GC0312_write_cmos_sensor(0x8c, 0xbb);	/*00  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0x8d, 0xcb);	/*ba */
	GC0312_write_cmos_sensor(0x8e, 0x94);	/*a9 */
#else
	GC0312_write_cmos_sensor(0x8d, 0xba);	/*00  20160406 */
	GC0312_write_cmos_sensor(0x8e, 0xa9);	/*00  20160406 */
#endif
	GC0312_write_cmos_sensor(0x94, 0x50);	/*55  20160406 */
	GC0312_write_cmos_sensor(0x99, 0xa6);
	GC0312_write_cmos_sensor(0x9e, 0xaa);
	GC0312_write_cmos_sensor(0xa3, 0x00);	/*0a  20160406 */
	GC0312_write_cmos_sensor(0x8a, 0xaa);	/*00  20160406 */
	GC0312_write_cmos_sensor(0xa8, 0x50);	/*55  20160406 */
	GC0312_write_cmos_sensor(0xad, 0x55);
	GC0312_write_cmos_sensor(0xb2, 0x55);
	GC0312_write_cmos_sensor(0xb7, 0x00);	/*05  20160406 */
	GC0312_write_cmos_sensor(0x8f, 0x55);	/*00  20160406 */
	GC0312_write_cmos_sensor(0xb8, 0xd6);	/*cb  20160406 */
	GC0312_write_cmos_sensor(0xb9, 0x8c);	/*9b  20160406 */
	/* /////////////////////////////////////////////// */
	/* //////////////////   CC    //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x01);
	GC0312_write_cmos_sensor(0xd0, 0x42);	/*40  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0xd1, 0xff);	/*fc  20160406 */
#else
	GC0312_write_cmos_sensor(0xd1, 0xfe);	/*fc  20160406 */
#endif
	GC0312_write_cmos_sensor(0xd2, 0x08);	/*06  20160406 */
	GC0312_write_cmos_sensor(0xd3, 0xf8);	/*e8  20160406 */
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0xd4, 0x47);	/*42  20160406 */
#else
	GC0312_write_cmos_sensor(0xd4, 0x48);	/*42  20160406 */
#endif
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
	GC0312_write_cmos_sensor(0xd5, 0xf1);	/*ea  20160406 */
#else
	GC0312_write_cmos_sensor(0xd5, 0xf0);	/*ea  20160406 */
#endif
	GC0312_write_cmos_sensor(0xd6, 0x40);	/*42  20160406 */
	GC0312_write_cmos_sensor(0xd7, 0xfe);	/*04  20160406 */
	GC0312_write_cmos_sensor(0xd8, 0x04);	/*09  20160406 */
	GC0312_write_cmos_sensor(0xd9, 0xf8);	/*00  20160406 */
	GC0312_write_cmos_sensor(0xda, 0x45);	/*40  20160406 */
	GC0312_write_cmos_sensor(0xdb, 0xe8);	/*ec  20160406 */
	/* /////////////////////////////////////////////// */
	/* //////////////////   LSC   //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x01);
	GC0312_write_cmos_sensor(0xc1, 0x3c);
	GC0312_write_cmos_sensor(0xc2, 0x50);
	GC0312_write_cmos_sensor(0xc3, 0x00);
	GC0312_write_cmos_sensor(0xc4, 0x38);	/*20  20160406 */
	GC0312_write_cmos_sensor(0xc5, 0x20);	/*1a  20160406 */
	GC0312_write_cmos_sensor(0xc6, 0x20);	/*1a  20160406 */
	GC0312_write_cmos_sensor(0xc7, 0x10);
	GC0312_write_cmos_sensor(0xc8, 0x00);
	GC0312_write_cmos_sensor(0xc9, 0x00);
	GC0312_write_cmos_sensor(0xdc, 0x20);
	GC0312_write_cmos_sensor(0xdd, 0x10);
	GC0312_write_cmos_sensor(0xdf, 0x00);
	GC0312_write_cmos_sensor(0xde, 0x00);

	/* /////////////////////////////////////////////// */
	/* /////////////////  Histogram  ///////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x01, 0x10);
	GC0312_write_cmos_sensor(0x0b, 0x31);
	GC0312_write_cmos_sensor(0x0e, 0x50);
	GC0312_write_cmos_sensor(0x0f, 0x0f);
	GC0312_write_cmos_sensor(0x10, 0x6e);
	GC0312_write_cmos_sensor(0x12, 0xa0);
	GC0312_write_cmos_sensor(0x15, 0x60);
	GC0312_write_cmos_sensor(0x16, 0x60);
	GC0312_write_cmos_sensor(0x17, 0xe0);

	/* /////////////////////////////////////////// */
	/* ////////////  Measure Windo/////////////// */
	/* /////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xcc, 0x0c);
	GC0312_write_cmos_sensor(0xcd, 0x10);
	GC0312_write_cmos_sensor(0xce, 0xa0);
	GC0312_write_cmos_sensor(0xcf, 0xe6);

	/* /////////////////////////////////////////// */
	/* ///////////////       dark sun   ////////////// */
	/* /////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0x45, 0xf7);
	GC0312_write_cmos_sensor(0x46, 0xff);
	GC0312_write_cmos_sensor(0x47, 0xff);	/*15 20160624 */
	GC0312_write_cmos_sensor(0x48, 0x03);
	GC0312_write_cmos_sensor(0x4f, 0x60);

	/* ////////////////banding////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x01);
	GC0312_write_cmos_sensor(0x0d, 0xf8);	/*new exp 3 */
	GC0312_write_cmos_sensor(0xfe, 0x00);
	GC0312_write_cmos_sensor(0x05, 0x01);	/*01 */
	GC0312_write_cmos_sensor(0x06, 0x18);	/*89 */
	GC0312_write_cmos_sensor(0x07, 0x00);
	GC0312_write_cmos_sensor(0x08, 0x10);	/*22 */
	GC0312_write_cmos_sensor(0xfe, 0x01);
	GC0312_write_cmos_sensor(0x25, 0x00);
	GC0312_write_cmos_sensor(0x26, 0x9a);	/*1step   30 */
	GC0312_write_cmos_sensor(0x27, 0x01);
	GC0312_write_cmos_sensor(0x28, 0x34);	/*2step   30 */
	GC0312_write_cmos_sensor(0x29, 0x02);
	GC0312_write_cmos_sensor(0x2a, 0x68);	/*4step   25 */
	GC0312_write_cmos_sensor(0x2b, 0x04);
	GC0312_write_cmos_sensor(0x2c, 0xd0);	/*5step   12.5 */
	GC0312_write_cmos_sensor(0x2d, 0x06);
	GC0312_write_cmos_sensor(0x2e, 0x04);	/*10step  10 */
	GC0312_write_cmos_sensor(0x2f, 0x09);
	GC0312_write_cmos_sensor(0x30, 0x06);	/*16step  6.25 *//*a0 */
	GC0312_write_cmos_sensor(0x31, 0x09);	/*0a */
	GC0312_write_cmos_sensor(0x32, 0xa0);	/*18step  5.56 *//*d4 */
	GC0312_write_cmos_sensor(0x33, 0x0a);	/*0c */
	GC0312_write_cmos_sensor(0x34, 0x3a);	/*20step  5 *//*08 */
	GC0312_write_cmos_sensor(0x35, 0x10);	/*2 *//*08 */
	GC0312_write_cmos_sensor(0x36, 0x10);	/*2 *//*12 */
	GC0312_write_cmos_sensor(0x37, 0x12);	/*2.25 */
	GC0312_write_cmos_sensor(0x38, 0x14);	/*2.5 */
	GC0312_write_cmos_sensor(0x39, 0x18);	/*3 */
	GC0312_write_cmos_sensor(0x3a, 0x28);	/*5 */
	GC0312_write_cmos_sensor(0x3b, 0x28);	/*5 */
	/* /////////////////////////////////////////////// */
	/* ///////////////////  DVP   //////////////////// */
	/* /////////////////////////////////////////////// */
	GC0312_write_cmos_sensor(0xfe, 0x03);
	GC0312_write_cmos_sensor(0x01, 0x00);
	GC0312_write_cmos_sensor(0x02, 0x00);
	GC0312_write_cmos_sensor(0x10, 0x00);
	GC0312_write_cmos_sensor(0x15, 0x00);
	GC0312_write_cmos_sensor(0xfe, 0x00);
	/* /////////////////OUTPUT////////////////////// */
	GC0312_write_cmos_sensor(0xf3, 0xff);	/* output_enable */

}



UINT32 GC0312GetSensorID(UINT32 *sensorID)
{
	int retry = 3;
	/* check if sensor ID correct */
	do {
		*sensorID = ((GC0312_read_cmos_sensor(0xf0) << 8) | GC0312_read_cmos_sensor(0xf1));
		if (*sensorID == GC0312_SENSOR_ID)
			break;
		SENSORDB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
		retry--;
	} while (retry > 0);

	if (*sensorID != GC0312_SENSOR_ID) {
		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}




/*************************************************************************
* FUNCTION
*	GC0312_Write_More_Registers
*
* DESCRIPTION
*	This function is served for FAE to modify the necessary Init Regs. Do not modify the regs
*     in init_GC0312() directly.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GC0312_Write_More_Registers(void)
{
	/* //////////////////for FAE to modify the necessary Init Regs.//////////////// */

}


/*************************************************************************
 * FUNCTION
 *	GC0312Open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 GC0312Open(void)
{
	volatile signed char i;
	kal_uint16 sensor_id = 0;

//	struct file *fp;

	printk("<Jet> Entry GC0312Open!!!\r\n");

	Sleep(10);


	/* Read sensor ID to adjust I2C is OK? */
	for (i = 0; i < 3; i++) {
		sensor_id = ((GC0312_read_cmos_sensor(0xf0) << 8) | GC0312_read_cmos_sensor(0xf1));
		if (sensor_id != GC0312_SENSOR_ID) {
			SENSORDB("GC0312 Read Sensor ID Fail[open] = 0x%x\n", sensor_id);
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}

	SENSORDB("GC0312_ Sensor Read ID OK \r\n");
	GC0312_Sensor_Init();
#ifdef DEBUG_SENSOR_GC0312
	if (TFlashSwitchValue > 0) {
		/* struct file *fp; */
		/* mm_segment_t fs; */
		/* loff_t pos = 0; */
		/* static char buf[60 * 1024]; */

		printk("gc0312 open debug\n");

		fp = filp_open("/data/misc/tuning/gc0312_sd.txt", O_RDONLY, 0);

		if (IS_ERR(fp)) {
			fromsd = 0;
			printk("gc0312 open file error\n");
		} else {
			fromsd = 1;
			printk("gc0312 open file ok\n");

			filp_close(fp, NULL);
			/* set_fs(fs); */
		}

		if (fromsd == 1) {
			printk("gc0312 open from t!\n");
			GC0312_Initialize_from_T_Flash();
		} else {
			printk("gc0312 open not from t!\n");
		}

	}
#endif
	GC0312_Write_More_Registers();

	return ERROR_NONE;
}				/* GC0312Open */


/*************************************************************************
 * FUNCTION
 *	GC0312Close
 *
 * DESCRIPTION
 *	This function is to turn off sensor module power.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 GC0312Close(void)
{
	return ERROR_NONE;
}				/* GC0312Close */


/*************************************************************************
 * FUNCTION
 * GC0312Preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 GC0312Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
/* kal_uint32 iTemp; */
/* kal_uint16 iStartX = 0, iStartY = 1; */

	if (sensor_config_data->SensorOperationMode == MSDK_SENSOR_OPERATION_MODE_VIDEO) {	/* MPEG4 Encode Mode */
		RETAILMSG(1, (TEXT("Camera Video preview\r\n")));
		GC0312_MPEG4_encode_mode = KAL_TRUE;

	} else {
		RETAILMSG(1, (TEXT("Camera preview\r\n")));
		GC0312_MPEG4_encode_mode = KAL_FALSE;
	}

	image_window->GrabStartX = IMAGE_SENSOR_VGA_GRAB_PIXELS;
	image_window->GrabStartY = IMAGE_SENSOR_VGA_GRAB_LINES;
	image_window->ExposureWindowWidth = IMAGE_SENSOR_PV_WIDTH;
	image_window->ExposureWindowHeight = IMAGE_SENSOR_PV_HEIGHT;

	/* copy sensor_config_data */
	memcpy(&GC0312SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	return ERROR_NONE;
}				/* GC0312Preview */


/*************************************************************************
 * FUNCTION
 *	GC0312Capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 GC0312Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	GC0312_MODE_CAPTURE = KAL_TRUE;

	image_window->GrabStartX = IMAGE_SENSOR_VGA_GRAB_PIXELS;
	image_window->GrabStartY = IMAGE_SENSOR_VGA_GRAB_LINES;
	image_window->ExposureWindowWidth = IMAGE_SENSOR_FULL_WIDTH;
	image_window->ExposureWindowHeight = IMAGE_SENSOR_FULL_HEIGHT;

	/* copy sensor_config_data */
	memcpy(&GC0312SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	return ERROR_NONE;
}				/* GC0312_Capture() */



UINT32 GC0312GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	pSensorResolution->SensorFullWidth = IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight = IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorPreviewWidth = IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight = IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorVideoWidth = IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorVideoHeight = IMAGE_SENSOR_PV_HEIGHT;
	return ERROR_NONE;
}				/* GC0312GetResolution() */


UINT32 GC0312GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		     MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		     MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	pSensorInfo->SensorPreviewResolutionX = IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY = IMAGE_SENSOR_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX = IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY = IMAGE_SENSOR_FULL_HEIGHT;

	pSensorInfo->SensorCameraPreviewFrameRate = 30;
	pSensorInfo->SensorVideoFrameRate = 30;
	pSensorInfo->SensorStillCaptureFrameRate = 10;
	pSensorInfo->SensorWebCamCaptureFrameRate = 15;
	pSensorInfo->SensorResetActiveHigh = FALSE;
	pSensorInfo->SensorResetDelayCount = 1;
	pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_YUYV;
	pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorInterruptDelayLines = 1;
	pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_PARALLEL;
	pSensorInfo->CaptureDelayFrame = 2;
	pSensorInfo->PreviewDelayFrame = 2;
	pSensorInfo->VideoDelayFrame = 4;
	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_4MA;

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	default:
		pSensorInfo->SensorClockFreq = 24;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = IMAGE_SENSOR_VGA_GRAB_PIXELS;
		pSensorInfo->SensorGrabStartY = IMAGE_SENSOR_VGA_GRAB_LINES;
		break;
	}
	GC0312PixelClockDivider = pSensorInfo->SensorPixelClockCount;
	memcpy(pSensorConfigData, &GC0312SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	return ERROR_NONE;
}				/* GC0312GetInfo() */


UINT32 GC0312Control(MSDK_SCENARIO_ID_ENUM ScenarioId,
		     MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		     MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	default:
		GC0312Preview(pImageWindow, pSensorConfigData);
		break;
	}


	return TRUE;
}				/* GC0312Control() */

BOOL GC0312_set_Edge(UINT16 para)
{

	switch (para) {
	case ISP_EDGE_MIDDLE:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x95, 0x44);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_EDGE_HIGH:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x95, 0x99);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_EDGE_LOW:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x95, 0x00);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

BOOL GC0312_set_HUE(UINT16 para)
{

	switch (para) {
	case ISP_HUE_MIDDLE:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x7a, 0x80);
		GC0312_write_cmos_sensor(0x7b, 0x80);
#ifdef CONFIG_MTK_FRONT_CAMERA_ROT
		GC0312_write_cmos_sensor(0x7c, 0x83);
#else
		GC0312_write_cmos_sensor(0x7c, 0x80);
#endif
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_HUE_HIGH:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x7a, 0x88);
		GC0312_write_cmos_sensor(0x7b, 0x88);
		GC0312_write_cmos_sensor(0x7c, 0x80);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_HUE_LOW:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x7a, 0x80);
		GC0312_write_cmos_sensor(0x7b, 0x80);
		GC0312_write_cmos_sensor(0x7c, 0x88);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;


	default:
		return FALSE;
	}

	return TRUE;
}

BOOL GC0312_set_Brightness(UINT16 para)
{

	switch (para) {
	case ISP_BRIGHT_MIDDLE:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd5, 0x00);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x37);	/*39  20160406 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_BRIGHT_HIGH:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd5, 0x10);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x60);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_BRIGHT_LOW:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd5, 0xf8);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

BOOL GC0312_set_Contrast(UINT16 para)
{

	switch (para) {
	case ISP_CONTRAST_MIDDLE:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd3, 0x40);	/*42  20160406 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_CONTRAST_HIGH:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd3, 0x50);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_CONTRAST_LOW:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd3, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

BOOL GC0312_set_Saturation(UINT16 para)
{

	switch (para) {
	case ISP_SAT_MIDDLE:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd1, 0x2c);	/*30  20160406 */
		GC0312_write_cmos_sensor(0xd2, 0x28);	/*2a  20160406 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_SAT_HIGH:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd1, 0x40);
		GC0312_write_cmos_sensor(0xd2, 0x40);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case ISP_SAT_LOW:
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xd1, 0x20);
		GC0312_write_cmos_sensor(0xd2, 0x20);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}




BOOL GC0312_set_param_wb(UINT16 para)
{

	switch (para) {
	case AWB_MODE_OFF:

		break;

	case AWB_MODE_AUTO:
		GC0312_awb_enable(KAL_TRUE);
		break;

	case AWB_MODE_CLOUDY_DAYLIGHT:	/* cloudy */
		GC0312_awb_enable(KAL_FALSE);
		GC0312_write_cmos_sensor(0x77, 0x8c);	/* WB_manual_gain */
		GC0312_write_cmos_sensor(0x78, 0x50);
		GC0312_write_cmos_sensor(0x79, 0x40);
		break;

	case AWB_MODE_DAYLIGHT:	/* sunny */
		GC0312_awb_enable(KAL_FALSE);
		GC0312_write_cmos_sensor(0x77, 0x74);
		GC0312_write_cmos_sensor(0x78, 0x52);
		GC0312_write_cmos_sensor(0x79, 0x40);
		break;

	case AWB_MODE_INCANDESCENT:	/* office */
		GC0312_awb_enable(KAL_FALSE);
		GC0312_write_cmos_sensor(0x77, 0x48);
		GC0312_write_cmos_sensor(0x78, 0x40);
		GC0312_write_cmos_sensor(0x79, 0x5c);
		break;

	case AWB_MODE_TUNGSTEN:	/* home */
		GC0312_awb_enable(KAL_FALSE);
		GC0312_write_cmos_sensor(0x77, 0x40);
		GC0312_write_cmos_sensor(0x78, 0x54);
		GC0312_write_cmos_sensor(0x79, 0x70);
		break;

	case AWB_MODE_FLUORESCENT:
		GC0312_awb_enable(KAL_FALSE);
		GC0312_write_cmos_sensor(0x77, 0x40);
		GC0312_write_cmos_sensor(0x78, 0x42);
		GC0312_write_cmos_sensor(0x79, 0x50);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}				/* GC0312_set_param_wb */


BOOL GC0312_set_param_effect(UINT16 para)
{
	kal_uint32 ret = KAL_TRUE;

	switch (para) {
	case MEFFECT_OFF:
		GC0312_write_cmos_sensor(0x43, 0x00);
		break;

	case MEFFECT_SEPIA:
		GC0312_write_cmos_sensor(0x43, 0x02);
		GC0312_write_cmos_sensor(0xda, 0xd0);
		GC0312_write_cmos_sensor(0xdb, 0x28);
		break;

	case MEFFECT_NEGATIVE:
		GC0312_write_cmos_sensor(0x43, 0x01);
		break;

	case MEFFECT_SEPIAGREEN:
		GC0312_write_cmos_sensor(0x43, 0x02);
		GC0312_write_cmos_sensor(0xda, 0xc0);
		GC0312_write_cmos_sensor(0xdb, 0xc0);
		break;

	case MEFFECT_SEPIABLUE:
		GC0312_write_cmos_sensor(0x43, 0x02);
		GC0312_write_cmos_sensor(0xda, 0x50);
		GC0312_write_cmos_sensor(0xdb, 0xe0);
		break;

	case MEFFECT_MONO:
		GC0312_write_cmos_sensor(0x43, 0x02);
		GC0312_write_cmos_sensor(0xda, 0x00);
		GC0312_write_cmos_sensor(0xdb, 0x00);
		break;
	default:
		ret = FALSE;
	}

	return ret;

}				/* GC0312_set_param_effect */


BOOL GC0312_set_param_banding(UINT16 para)
{
	switch (para) {
	case AE_FLICKER_MODE_50HZ:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x0d, 0xf8);	/*new exp 3 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x05, 0x01);	/*01 */
		GC0312_write_cmos_sensor(0x06, 0x18);	/*89 */
		GC0312_write_cmos_sensor(0x07, 0x00);
		GC0312_write_cmos_sensor(0x08, 0x10);	/*22 */
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x25, 0x00);
		GC0312_write_cmos_sensor(0x26, 0x9a);	/*1step   30 */
		GC0312_write_cmos_sensor(0x27, 0x01);
		GC0312_write_cmos_sensor(0x28, 0x34);	/*2step   30 */
		GC0312_write_cmos_sensor(0x29, 0x02);
		GC0312_write_cmos_sensor(0x2a, 0x68);	/*4step   25 */
		GC0312_write_cmos_sensor(0x2b, 0x04);
		GC0312_write_cmos_sensor(0x2c, 0xd0);	/*5step   12.5 */
		GC0312_write_cmos_sensor(0x2d, 0x06);
		GC0312_write_cmos_sensor(0x2e, 0x04);	/*10step  10 */
		GC0312_write_cmos_sensor(0x2f, 0x09);
		GC0312_write_cmos_sensor(0x30, 0x06);	/*16step  6.25 *//*a0 */
		GC0312_write_cmos_sensor(0x31, 0x09);	/*0a */
		GC0312_write_cmos_sensor(0x32, 0xa0);	/*18step  5.56 *//*d4 */
		GC0312_write_cmos_sensor(0x33, 0x0a);	/*0c */
		GC0312_write_cmos_sensor(0x34, 0x3a);	/*20step  5 *//*08 */
		GC0312_write_cmos_sensor(0x35, 0x10);	/*2 */
		GC0312_write_cmos_sensor(0x36, 0x10);	/*2 *//*12 */
		GC0312_write_cmos_sensor(0x37, 0x12);	/*2.25 */
		GC0312_write_cmos_sensor(0x38, 0x14);	/*2.5 */
		GC0312_write_cmos_sensor(0x39, 0x18);	/*3 */
		GC0312_write_cmos_sensor(0x3a, 0x28);	/*5 */
		GC0312_write_cmos_sensor(0x3b, 0x28);	/*5 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case AE_FLICKER_MODE_60HZ:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x0d, 0xf8);	/*new exp 3 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x05, 0x01);	/*01 */
		GC0312_write_cmos_sensor(0x06, 0x13);	/*89 */
		GC0312_write_cmos_sensor(0x07, 0x00);
		GC0312_write_cmos_sensor(0x08, 0x14);	/*22 */
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x25, 0x00);
		GC0312_write_cmos_sensor(0x26, 0x81);	/*8a   //step */
		GC0312_write_cmos_sensor(0x27, 0x01);
		GC0312_write_cmos_sensor(0x28, 0x83);	/*2step  level 1   30fps */
		GC0312_write_cmos_sensor(0x29, 0x02);
		GC0312_write_cmos_sensor(0x2a, 0x85);	/*4step level 2  25 fps */
		GC0312_write_cmos_sensor(0x2b, 0x05);
		GC0312_write_cmos_sensor(0x2c, 0x0a);
		GC0312_write_cmos_sensor(0x2d, 0x06);
		GC0312_write_cmos_sensor(0x2e, 0x0c);	/*10step level 4 17 */
		GC0312_write_cmos_sensor(0x2f, 0x09);
		GC0312_write_cmos_sensor(0x30, 0x12);	/*14step 12fps 93 */
		GC0312_write_cmos_sensor(0x31, 0x09);	/*0a */
		GC0312_write_cmos_sensor(0x32, 0x93);	/*14step 10fps 95 */
		GC0312_write_cmos_sensor(0x33, 0x0a);	/*0b */
		GC0312_write_cmos_sensor(0x34, 0x14);	/*level 7 *//*97 */
		GC0312_write_cmos_sensor(0x35, 0x10);	/*2.25x level 1 */
		GC0312_write_cmos_sensor(0x36, 0x10);	/*3.5x  level 2 12 */
		GC0312_write_cmos_sensor(0x37, 0x12);	/*6x   level 3 */
		GC0312_write_cmos_sensor(0x38, 0x14);	/*  level 4 */
		GC0312_write_cmos_sensor(0x39, 0x18);	/*  level 5 */
		GC0312_write_cmos_sensor(0x3a, 0x28);	/*  level 6 */
		GC0312_write_cmos_sensor(0x3b, 0x28);	/* level 7 3.5x */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}				/* GC0312_set_param_banding */


BOOL GC0312_set_param_exposure(UINT16 para)
{


	switch (para) {
	case AE_EV_COMP_n30:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x20);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;
	case AE_EV_COMP_n20:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x28);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case AE_EV_COMP_n10:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;



	case AE_EV_COMP_00:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x37);	/*39  20160406 */
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;



	case AE_EV_COMP_10:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x40);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;

	case AE_EV_COMP_20:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x48);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;
	case AE_EV_COMP_30:
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x50);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}				/* GC0312_set_param_exposure */


UINT32 GC0312YUVSetVideoMode(UINT16 u2FrameRate)
{				/* lanking add */

	GC0312_MPEG4_encode_mode = KAL_TRUE;
	if (u2FrameRate == 30) {

	    /*********video frame ************/
/*
	    GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x0d, 0xf8);
			GC0312_write_cmos_sensor(0xfe, 0x00);
			GC0312_write_cmos_sensor(0x05, 0x01);
			GC0312_write_cmos_sensor(0x06, 0x18);
			GC0312_write_cmos_sensor(0x07, 0x00);
			GC0312_write_cmos_sensor(0x08, 0x10);
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x25, 0x00);
			GC0312_write_cmos_sensor(0x26, 0x9a);
			GC0312_write_cmos_sensor(0x27, 0x01);
			GC0312_write_cmos_sensor(0x28, 0x34);
			GC0312_write_cmos_sensor(0x29, 0x02);
			GC0312_write_cmos_sensor(0x2a, 0x68);
			GC0312_write_cmos_sensor(0x2b, 0x03);
			GC0312_write_cmos_sensor(0x2c, 0x02);
			GC0312_write_cmos_sensor(0x2d, 0x03);
			GC0312_write_cmos_sensor(0x2e, 0x9c);
			GC0312_write_cmos_sensor(0x2f, 0x04);
			GC0312_write_cmos_sensor(0x30, 0xd0);
			GC0312_write_cmos_sensor(0x31, 0x04);
			GC0312_write_cmos_sensor(0x32, 0xd0);
			GC0312_write_cmos_sensor(0x33, 0x06);
			GC0312_write_cmos_sensor(0x34, 0x04);
			GC0312_write_cmos_sensor(0x35, 0x0d);
			GC0312_write_cmos_sensor(0x36, 0x14);
			GC0312_write_cmos_sensor(0x37, 0x18);
			GC0312_write_cmos_sensor(0x38, 0x20);
			GC0312_write_cmos_sensor(0x39, 0x30);
			GC0312_write_cmos_sensor(0x3a, 0x30);
			GC0312_write_cmos_sensor(0x3b, 0x30);
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x13, 0x35);
			GC0312_write_cmos_sensor(0xfe, 0x00);
*/
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x0d, 0xf8);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x05, 0x01);
		GC0312_write_cmos_sensor(0x06, 0x18);
		GC0312_write_cmos_sensor(0x07, 0x00);
		GC0312_write_cmos_sensor(0x08, 0x10);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x25, 0x00);
		GC0312_write_cmos_sensor(0x26, 0x9a);
		GC0312_write_cmos_sensor(0x27, 0x01);
		GC0312_write_cmos_sensor(0x28, 0x34);
		GC0312_write_cmos_sensor(0x29, 0x01);
		GC0312_write_cmos_sensor(0x2a, 0x34);
		GC0312_write_cmos_sensor(0x2b, 0x01);
		GC0312_write_cmos_sensor(0x2c, 0x34);
		GC0312_write_cmos_sensor(0x2d, 0x01);
		GC0312_write_cmos_sensor(0x2e, 0x34);
		GC0312_write_cmos_sensor(0x2f, 0x01);
		GC0312_write_cmos_sensor(0x30, 0x34);
		GC0312_write_cmos_sensor(0x31, 0x01);
		GC0312_write_cmos_sensor(0x32, 0x34);
		GC0312_write_cmos_sensor(0x33, 0x01);
		GC0312_write_cmos_sensor(0x34, 0x34);
		GC0312_write_cmos_sensor(0x35, 0x0d);
		GC0312_write_cmos_sensor(0x36, 0x14);
		GC0312_write_cmos_sensor(0x37, 0x18);
		GC0312_write_cmos_sensor(0x38, 0x20);
		GC0312_write_cmos_sensor(0x39, 0x30);
		GC0312_write_cmos_sensor(0x3a, 0x30);
		GC0312_write_cmos_sensor(0x3b, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x35);
		GC0312_write_cmos_sensor(0xfe, 0x00);
	} else if (u2FrameRate == 15) {

	    /*********video frame ************/
/*
		GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x0d, 0xf8);
			GC0312_write_cmos_sensor(0xfe, 0x00);
			GC0312_write_cmos_sensor(0x05, 0x04);
			GC0312_write_cmos_sensor(0x06, 0xe6);
			GC0312_write_cmos_sensor(0x07, 0x00);
			GC0312_write_cmos_sensor(0x08, 0x10);
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x25, 0x00);
			GC0312_write_cmos_sensor(0x26, 0x4d);
			GC0312_write_cmos_sensor(0x27, 0x01);
			GC0312_write_cmos_sensor(0x28, 0x34);
			GC0312_write_cmos_sensor(0x29, 0x02);
			GC0312_write_cmos_sensor(0x2a, 0x68);
			GC0312_write_cmos_sensor(0x2b, 0x03);
			GC0312_write_cmos_sensor(0x2c, 0x02);
			GC0312_write_cmos_sensor(0x2d, 0x03);
			GC0312_write_cmos_sensor(0x2e, 0x9c);
			GC0312_write_cmos_sensor(0x2f, 0x04);
			GC0312_write_cmos_sensor(0x30, 0xd0);
			GC0312_write_cmos_sensor(0x31, 0x04);
			GC0312_write_cmos_sensor(0x32, 0xd0);
			GC0312_write_cmos_sensor(0x33, 0x06);
			GC0312_write_cmos_sensor(0x34, 0x04);
			GC0312_write_cmos_sensor(0x35, 0x0d);
			GC0312_write_cmos_sensor(0x36, 0x14);
			GC0312_write_cmos_sensor(0x37, 0x18);
			GC0312_write_cmos_sensor(0x38, 0x20);
			GC0312_write_cmos_sensor(0x39, 0x30);
			GC0312_write_cmos_sensor(0x3a, 0x30);
			GC0312_write_cmos_sensor(0x3b, 0x30);
			GC0312_write_cmos_sensor(0xfe, 0x01);
			GC0312_write_cmos_sensor(0x13, 0x35);
			GC0312_write_cmos_sensor(0xfe, 0x00);
*/
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x0d, 0xf8);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x05, 0x04);
		GC0312_write_cmos_sensor(0x06, 0xe6);
		GC0312_write_cmos_sensor(0x07, 0x00);
		GC0312_write_cmos_sensor(0x08, 0x10);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x25, 0x00);
		GC0312_write_cmos_sensor(0x26, 0x4d);
		GC0312_write_cmos_sensor(0x27, 0x01);
		GC0312_write_cmos_sensor(0x28, 0x34);
		GC0312_write_cmos_sensor(0x29, 0x01);
		GC0312_write_cmos_sensor(0x2a, 0x34);
		GC0312_write_cmos_sensor(0x2b, 0x01);
		GC0312_write_cmos_sensor(0x2c, 0x34);
		GC0312_write_cmos_sensor(0x2d, 0x01);
		GC0312_write_cmos_sensor(0x2e, 0x34);
		GC0312_write_cmos_sensor(0x2f, 0x01);
		GC0312_write_cmos_sensor(0x30, 0x34);
		GC0312_write_cmos_sensor(0x31, 0x01);
		GC0312_write_cmos_sensor(0x32, 0x34);
		GC0312_write_cmos_sensor(0x33, 0x01);
		GC0312_write_cmos_sensor(0x34, 0x34);
		GC0312_write_cmos_sensor(0x35, 0x0d);
		GC0312_write_cmos_sensor(0x36, 0x14);
		GC0312_write_cmos_sensor(0x37, 0x18);
		GC0312_write_cmos_sensor(0x38, 0x20);
		GC0312_write_cmos_sensor(0x39, 0x30);
		GC0312_write_cmos_sensor(0x3a, 0x30);
		GC0312_write_cmos_sensor(0x3b, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x35);
		GC0312_write_cmos_sensor(0xfe, 0x00);
	} else if (u2FrameRate == 24) {
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x0d, 0xf8);
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0x05, 0x02);
		GC0312_write_cmos_sensor(0x06, 0x2c);
		GC0312_write_cmos_sensor(0x07, 0x00);
		GC0312_write_cmos_sensor(0x08, 0x10);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x25, 0x00);
		GC0312_write_cmos_sensor(0x26, 0x78);
		GC0312_write_cmos_sensor(0x27, 0x00);
		GC0312_write_cmos_sensor(0x28, 0xf0);
		GC0312_write_cmos_sensor(0x29, 0x00);
		GC0312_write_cmos_sensor(0x2a, 0xf0);
		GC0312_write_cmos_sensor(0x2b, 0x00);
		GC0312_write_cmos_sensor(0x2c, 0xf0);
		GC0312_write_cmos_sensor(0x2d, 0x00);
		GC0312_write_cmos_sensor(0x2e, 0xf0);
		GC0312_write_cmos_sensor(0x2f, 0x00);
		GC0312_write_cmos_sensor(0x30, 0xf0);
		GC0312_write_cmos_sensor(0x31, 0x00);
		GC0312_write_cmos_sensor(0x32, 0xf0);
		GC0312_write_cmos_sensor(0x33, 0x00);
		GC0312_write_cmos_sensor(0x34, 0xf0);
		GC0312_write_cmos_sensor(0x35, 0x0d);
		GC0312_write_cmos_sensor(0x36, 0x14);
		GC0312_write_cmos_sensor(0x37, 0x18);
		GC0312_write_cmos_sensor(0x38, 0x20);
		GC0312_write_cmos_sensor(0x39, 0x30);
		GC0312_write_cmos_sensor(0x3a, 0x30);
		GC0312_write_cmos_sensor(0x3b, 0x30);
		GC0312_write_cmos_sensor(0xfe, 0x01);
		GC0312_write_cmos_sensor(0x13, 0x35);
		GC0312_write_cmos_sensor(0xfe, 0x00);
	} else {

		SENSORDB("Wrong Frame Rate");

	}

	return TRUE;

}


UINT32 GC0312YUVSensorSetting(FEATURE_ID iCmd, UINT16 iPara)
{

#ifdef DEBUG_SENSOR_GC0312
	if (TFlashSwitchValue > 0) {
		printk("______%s______ Tflash debug\n", __func__);
		return TRUE;
	}
#endif

	switch (iCmd) {
	case FID_ISP_EDGE:
		GC0312_set_Edge(iPara);
		break;
	case FID_ISP_HUE:
		GC0312_set_HUE(iPara);
		break;
	case FID_ISP_BRIGHT:
		GC0312_set_Brightness(iPara);
		break;
	case FID_ISP_CONTRAST:
		GC0312_set_Contrast(iPara);
		break;
	case FID_ISP_SAT:
		GC0312_set_Saturation(iPara);
		break;
	case FID_AWB_MODE:
		GC0312_set_param_wb(iPara);
		break;
	case FID_COLOR_EFFECT:
		GC0312_set_param_effect(iPara);
		break;
	case FID_AE_EV:
		GC0312_set_param_exposure(iPara);
		break;
	case FID_AE_FLICKER:
		GC0312_set_param_banding(iPara);
		break;
	case FID_SCENE_MODE:
		GC0312NightMode(iPara);
		break;
	default:
		break;
	}
	return TRUE;
}				/* GC0312YUVSensorSetting */

UINT32 GC0312_SetTestPatternMode(kal_bool bEnable)
{
	SENSORDB("bEnable: %d", bEnable);
	if (bEnable) {
		GC0312_write_cmos_sensor(0xfe, 0x00);
		GC0312_write_cmos_sensor(0xfa, 0x32);
		GC0312_write_cmos_sensor(0x40, 0x08);
		GC0312_write_cmos_sensor(0x41, 0x00);
		GC0312_write_cmos_sensor(0x42, 0x00);
		GC0312_write_cmos_sensor(0x4f, 0x00);
		GC0312_write_cmos_sensor(0x18, 0x06);
		GC0312_write_cmos_sensor(0x77, 0x40);
		GC0312_write_cmos_sensor(0x78, 0x40);
		GC0312_write_cmos_sensor(0x79, 0x40);
		GC0312_write_cmos_sensor(0xd0, 0x40);
		GC0312_write_cmos_sensor(0xdd, 0x00);
		GC0312_write_cmos_sensor(0x71, 0x40);
		GC0312_write_cmos_sensor(0x72, 0x40);
		GC0312_write_cmos_sensor(0x03, 0x00);
		GC0312_write_cmos_sensor(0x04, 0x00);
		GC0312_write_cmos_sensor(0x4c, 0x01);
	} else {
	}

	return ERROR_NONE;
}

void GC0312GetAFMaxNumFocusAreas(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
	SENSORDB("GC0312GetAFMaxNumFocusAreas, *pFeatureReturnPara32 = %d\n",
		 *pFeatureReturnPara32);

}


void GC0312GetAFMaxNumMeteringAreas(UINT32 *pFeatureReturnPara32)
{
	*pFeatureReturnPara32 = 0;
	SENSORDB("GC0312GetAFMaxNumMeteringAreas,*pFeatureReturnPara32 = %d\n",
		 *pFeatureReturnPara32);

}

void GC0312GetExifInfo(uintptr_t exifAddr)
{

	SENSOR_EXIF_INFO_STRUCT *pExifInfo = (SENSOR_EXIF_INFO_STRUCT *) exifAddr;

	GC0312_Read_Shutter();

	/*
	   //pExifInfo->FNumber = 28;
	   //pExifInfo->AEISOSpeed = GC0312ExifInfo.AEISOSpeed;
	   //pExifInfo->FlashLightTimeus = 0;
	   //pExifInfo->RealISOValue = GC0312ExifInfo.RealISOValue;
	   //pExifInfo->AWBMode = GC0312ExifInfo.AWBMode;
	 */
#if 0  // smartdisplay
	pExifInfo->RealISOValue = GC0312ExifInfo.RealISOValue;
	pExifInfo->CapExposureTime = GC0312ExifInfo.CapExposureTime * 1000;

#else
	{
		MUINT32 value = GC0312ExifInfo.RealISOValue;
		if (copy_to_user
			((void __user *)(uintptr_t)(&pExifInfo->RealISOValue), (void *)&value,
			 sizeof(MUINT32))) {
			printk("[CAMERA_HW][GC0312GetExifInfo] ioctl copy to user failed\n");
		}
		value = GC0312ExifInfo.CapExposureTime * 1000;
		if (copy_to_user
			((void __user *)(uintptr_t)(&pExifInfo->CapExposureTime), (void *)&value,
			 sizeof(MUINT32))) {
			printk("[CAMERA_HW][GC0312GetExifInfo] ioctl copy to user failed\n");
		}
	}
#endif
	printk("GC0312ExifInfo.CapExposureTime is %d", GC0312ExifInfo.CapExposureTime);
}

UINT32 GC0312FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
			    UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	unsigned long long *pFeatureData = (unsigned long long *)pFeaturePara;
	/* UINT32 GC0312SensorRegNumber; */
	/* UINT32 i; */
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData = (MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData = (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

	RETAILMSG(1, (_T("gaiyang GC0312FeatureControl FeatureId=%d\r\n"), FeatureId));

	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ = IMAGE_SENSOR_FULL_WIDTH;
		*pFeatureReturnPara16 = IMAGE_SENSOR_FULL_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*pFeatureReturnPara16++ = (VGA_PERIOD_PIXEL_NUMS) + GC0312_dummy_pixels;
		*pFeatureReturnPara16 = (VGA_PERIOD_LINE_NUMS) + GC0312_dummy_lines;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*pFeatureReturnPara32 = GC0312_g_fPV_PCLK;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		/* GC0312NightMode((BOOL) *pFeatureData16); */
		break;
	case SENSOR_FEATURE_SET_GAIN:
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		GC0312_isp_master_clock = *pFeatureData32;
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		GC0312_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		pSensorRegData->RegData = GC0312_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		memcpy(pSensorConfigData, &GC0312SensorConfigData,
		       sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_GET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_GROUP_COUNT:
	case SENSOR_FEATURE_GET_GROUP_INFO:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_YUV_CMD:
		/* GC0312YUVSensorSetting((FEATURE_ID) *pFeatureData32, *(pFeatureData32 + 1)); */
		GC0312YUVSensorSetting((FEATURE_ID) *pFeatureData, *(pFeatureData + 1));
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:	/* lanking */
		GC0312YUVSetVideoMode(*pFeatureData16);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		GC0312GetSensorID(pFeatureData32);
		break;
	case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
		GC0312GetAFMaxNumFocusAreas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;

	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
		GC0312GetAFMaxNumMeteringAreas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		GC0312_SetTestPatternMode((BOOL) *pFeatureData16);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*pFeatureReturnPara32 = GC0312_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_EXIF_INFO:
		GC0312GetExifInfo(*pFeatureData32);
		break;
	default:
		break;
	}
	return ERROR_NONE;
}				/* GC0312FeatureControl() */


SENSOR_FUNCTION_STRUCT SensorFuncGC0312YUV = {
	GC0312Open,
	GC0312GetInfo,
	GC0312GetResolution,
	GC0312FeatureControl,
	GC0312Control,
	GC0312Close
};


UINT32 GC0312_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncGC0312YUV;
	return ERROR_NONE;
}				/* SensorInit() */
