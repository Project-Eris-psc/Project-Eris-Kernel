/*
 * mt8167-codec-utils.h  --  MT8167 codec driver utility functions
 *
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT8167_CODEC_UTILS_H__
#define __MT8167_CODEC_UTILS_H__

uint32_t mt8167_codec_conv_dc_offset_to_comp_val(int32_t dc_offset);

int mt8167_codec_get_hpl_cali_val(struct snd_soc_codec *codec,
	uint32_t *dccomp_val, int32_t *dc_offset);
int mt8167_codec_get_hpr_cali_val(struct snd_soc_codec *codec,
	uint32_t *dccomp_val, int32_t *dc_offset);

void mt8167_codec_turn_off_lpbk_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type);

void mt8167_codec_turn_on_lpbk_path(struct snd_soc_codec *codec,
	uint32_t lpbk_type);

#endif
