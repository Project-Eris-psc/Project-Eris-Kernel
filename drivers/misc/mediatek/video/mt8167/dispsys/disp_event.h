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

#ifndef __DISP_EVENT_H__
#define __DISP_EVENT_H__

enum DISP_PATH_EVENT {
	DISP_PATH_EVENT_FRAME_DONE = 0,
	DISP_PATH_EVENT_FRAME_START,
	DISP_PATH_EVENT_FRAME_REG_UPDATE,
	DISP_PATH_EVENT_FRAME_TARGET_LINE,
	DISP_PATH_EVENT_FRAME_COMPLETE,
	DISP_PATH_EVENT_FRAME_STOP,
	DISP_PATH_EVENT_IF_CMD_DONE,
	DISP_PATH_EVENT_IF_VSYNC,
	DISP_PATH_EVENT_TRIGGER,
	DISP_PATH_EVENT_AAL_OUT_END_FRAME,
	DISP_PATH_EVENT_NUM,
	DISP_PATH_EVENT_NONE = 0xff,
};

/*
*
typedef enum {
    DISP_LCD_INTERRUPT_EVENTS_START     = 0x01,
    DISP_LCD_TRANSFER_COMPLETE_INT      = 0x01,
    DISP_LCD_REG_COMPLETE_INT           = 0x02,
    DISP_LCD_CDMQ_COMPLETE_INT          = 0x03,
    DISP_LCD_HTT_INT                    = 0x04,
    DISP_LCD_SYNC_INT                   = 0x05,
    DISP_LCD_INTERRUPT_EVENTS_END       = 0x05,

    DISP_DSI_INTERRUPT_EVENTS_START     = 0x11,
    DISP_DSI_READ_RDY_INT               = 0x11,
    DISP_DSI_CMD_DONE_INT               = 0x12,
    DISP_DSI_INTERRUPT_EVENTS_END       = 0x12,

    DISP_DPI_INTERRUPT_EVENTS_START     = 0x21,
    DISP_DPI_FIFO_EMPTY_INT             = 0x21,
    DISP_DPI_FIFO_FULL_INT              = 0x22,
    DISP_DPI_OUT_EMPTY_INT              = 0x23,
    DISP_DPI_CNT_OVERFLOW_INT           = 0x24,
    DISP_DPI_LINE_ERR_INT               = 0x25,
    DISP_DPI_VSYNC_INT                  = 0x26,
    DISP_DPI_INTERRUPT_EVENTS_END       = 0x26,
} DISP_INTERRUPT_EVENTS;
*/

#endif
