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
*
* You should have received a copy of the GNU General Public License
* along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_codec_63xx.h
 *
 * Project:
 * --------
 *    Audio codec header file
 *
 * Description:
 * ------------

 *   Audio codec function
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *   Chien-Wei Hsu (mtk10177)
 *
 *------------------------------------------------------------------------------
 *
 *******************************************************************************/

#ifndef _AUDIO_CODEC_63xx_H
#define _AUDIO_CODEC_63xx_H

/* Headphone Impedance Detection */
struct mtk_hpdet_param {
	int auxadc_upper_bound;
	int dc_Step;
	int dc_Phase0;
	int dc_Phase1;
	int dc_Phase2;
	int resistance_first_threshold;
	int resistance_second_threshold;
};

/* DPD parameter */
struct mtk_dpd_param {
	int efuse_on;
	int version;
	int a2_lch;
	int a3_lch;
	int a4_lch;
	int a5_lch;
	int a2_rch;
	int a3_rch;
	int a4_rch;
	int a5_rch;
};

void audckbufEnable(bool enable);
void OpenClassAB(void);
void OpenAnalogHeadphone(bool bEnable);
void OpenAnalogTrimHardware(bool bEnable);
void SetSdmLevel(unsigned int level);
void setOffsetTrimMux(unsigned int Mux);
void setOffsetTrimBufferGain(unsigned int gain);
void EnableTrimbuffer(bool benable);
void SetHplTrimOffset(int Offset);
void SetHprTrimOffset(int Offset);
void setHpGainZero(void);

/* headphone impedance detection function*/
bool OpenHeadPhoneImpedanceSetting(bool bEnable);
void mtk_read_hp_detection_parameter(struct mtk_hpdet_param *hpdet_param);
int mtk_calculate_impedance_formula(int pcm_offset, int aux_diff);

void SetAnalogSuspend(bool bEnable);
void OpenTrimBufferHardware(bool bEnable);

/* mtk audio interface calibration function */
void mtkaif_calibration_set_loopback(bool enable);
void mtkaif_calibration_set_phase(int mode1, int mode2);

/*mtk dpd function*/
void mtk_read_dpd_parameter(int impedance, struct mtk_dpd_param *dpd_param);
#endif

