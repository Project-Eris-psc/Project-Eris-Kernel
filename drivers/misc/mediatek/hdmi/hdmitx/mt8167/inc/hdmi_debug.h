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

#ifndef _MT_HDMI_DEBUG_H_
#define _MT_HDMI_DEBUG_H_

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
extern unsigned char _bHdcpOff;

extern struct HDMI_AV_INFO_T _stAvdAVInfo;
extern struct HDMI_SINK_AV_CAP_T _HdmiSinkAvCap;
extern unsigned char cDstStr[50];
extern const unsigned char _cFsStr[][7];
extern unsigned char hdmi_plug_test_mode;
/* unsigned int hdmi_plug_out_delay; */
/* unsigned int hdmi_plug_in_delay; */
extern unsigned char is_user_mute_hdmi;
extern unsigned char is_user_mute_hdmi_audio;
extern void hdmi_force_plug_out(void);
extern void hdmi_force_plug_in(void);
extern unsigned char hdmi_hdcp_for_avmute;
extern void mt_hdmi_show_info(char *pbuf);
extern void mt_hdmi_debug_write(const char *pbuf);
extern void hdmi_read(unsigned long u2Reg, unsigned int *p4Data);
extern void hdmi_write(unsigned long u2Reg, unsigned int u4Data);
void hdmi_colordeep(unsigned char u1colorspace, unsigned char u1deepcolor);



#endif
#endif
