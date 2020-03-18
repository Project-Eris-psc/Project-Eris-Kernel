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

#ifndef _REG_ACCDEH_H_
#define _REG_ACCDEH_H_

/* apmixedsys */
#define AUDIO_CODEC_CON00        0x0700
#define AUDIO_CODEC_CON01        0x0704
#define AUDIO_CODEC_CON02        0x0708
#define AUDIO_CODEC_CON03        0x070C
#define AUDIO_CODEC_CON04        0x0710

/* Register address define */
#define ACCDET_RSV               0x0

#define ACCDET_CTRL              0x4
#define ACCDET_STATE_SWCTRL      0x8
#define ACCDET_PWM_WIDTH		 0xc
#define ACCDET_PWM_THRESH		 0x10
#define ACCDET_EN_DELAY_NUM      0x14
#define ACCDET_DEBOUNCE0         0x18
#define ACCDET_DEBOUNCE1         0x1c
#define ACCDET_DEBOUNCE2         0x20
#define ACCDET_DEBOUNCE3         0x24

#define ACCDET_DEFAULT_STATE_RG  0x28


#define ACCDET_IRQ_STS           0x2c

#define ACCDET_CONTROL_RG        0x30
#define ACCDET_STATE_RG          0x34

#define ACCDET_CUR_DEB			 0x38
#define ACCDET_RSV_CON0			 0x3c
#define ACCDET_RSV_CON1			 0x40

/* Register value define */
#define ACCDET_CTRL_EN           (1<<0)
#define ACCDET_MIC_PWM_IDLE      (1<<6)
#define ACCDET_VTH_PWM_IDLE      (1<<5)
#define ACCDET_CMP_PWM_IDLE      (1<<4)
#define ACCDET_CMP_EN            (1<<0)
#define ACCDET_VTH_EN            (1<<1)
#define ACCDET_MICBIA_EN         (1<<2)

#define ACCDET_ENABLE			 (1<<0)
#define ACCDET_DISABLE			 (0<<0)

#define ACCDET_RESET_SET         (1<<4)
#define ACCDET_RESET_CLR         (1<<4)

#define IRQ_CLR_BIT              0x100
#define IRQ_STATUS_BIT           (1<<0)

#define RG_ACCDET_IRQ_SET        (1<<2)
#define RG_ACCDET_IRQ_CLR        (1<<2)
#define RG_ACCDET_IRQ_STATUS_CLR (1<<2)

/* CLOCK */
#define RG_ACCDET_CLK_SET        (1<<14)
#define RG_ACCDET_CLK_CLR        (1<<14)


#define ACCDET_PWM_EN_SW         (1<<15)
#define ACCDET_MIC_EN_SW         (1<<15)
#define ACCDET_VTH_EN_SW         (1<<15)
#define ACCDET_CMP_EN_SW         (1<<15)

#define ACCDET_SWCTRL_EN         0x07

#define ACCDET_IN_SW             0x10

#define ACCDET_PWM_SEL_CMP       0x00
#define ACCDET_PWM_SEL_VTH       0x01
#define ACCDET_PWM_SEL_MIC       0x10
#define ACCDET_PWM_SEL_SW        0x11
#define ACCDET_SWCTRL_IDLE_EN    (0x07<<4)

#define ACCDET_TEST_MODE5_ACCDET_IN_GPI       (1<<5)
#define ACCDET_TEST_MODE4_ACCDET_IN_SW        (1<<4)
#define ACCDET_TEST_MODE3_MIC_SW              (1<<3)
#define ACCDET_TEST_MODE2_VTH_SW              (1<<2)
#define ACCDET_TEST_MODE1_CMP_SW              (1<<1)
#define ACCDET_TEST_MODE0_GPI                 (1<<0)

#define ACCDET_1V9_MODE_ON        0x10B0
#define ACCDET_1V9_MODE_OFF       0x1090
#define ACCDET_2V8_MODE_ON        0x01
#define ACCDET_2V8_MODE_OFF       0x5090

/* #define ACCDET_DEFVAL_SEL        (1<<15) */
#endif
