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

#ifndef __DDP_DPI_REG_H__
#define __DDP_DPI_REG_H__

/*#include <stddef.h>*/

#ifdef __cplusplus
extern "C" {
#endif

	struct DPI_REG_EN {
		unsigned EN:1;
		unsigned rsv_1:31;
	};

	struct DPI_REG_RST {
		unsigned RST:1;
		unsigned rsv_1:31;
	};

	struct DPI_REG_INTERRUPT {
		unsigned VSYNC:1;
		unsigned VDE:1;
		unsigned UNDERFLOW:1;
		unsigned rsv_3:29;
	};

	struct DPI_REG_CNTL {
		unsigned BG_EN:1;
		unsigned RGB_SWAP:1;
		unsigned INTL_EN:1;
		unsigned TDFP_EN:1;
		unsigned CLPF_EN:1;
		unsigned YUV422_EN:1;
		unsigned MAXTRIX_EN:1;
		unsigned rsv_7:1;
		unsigned EMBSYNC_EN:1;
		unsigned rsv_9:3;
		unsigned PIXREP:4;	/* new */
		unsigned VS_LODD_EN:1;
		unsigned VS_LEVEN_EN:1;
		unsigned VS_RODD_EN:1;
		unsigned VS_REVEN_EN:1;
		unsigned FAKE_DE_LODD:1;
		unsigned FAKE_DE_LEVEN:1;
		unsigned FAKE_DE_RODD:1;
		unsigned FAKE_DE_REVEN:1;
		unsigned rsv_24:8;
	};

	struct DPI_REG_OUTPUT_SETTING {
		unsigned CH_SWAP:3;
		unsigned BIT_SWAP:1;
		unsigned B_MASK:1;
		unsigned G_MASK:1;
		unsigned R_MASK:1;
		unsigned rsv_7:1;
		unsigned DE_MASK:1;
		unsigned HS_MASK:1;
		unsigned VS_MASK:1;
		unsigned rsv_11:1;
		unsigned DE_POL:1;
		unsigned HSYNC_POL:1;
		unsigned VSYNC_POL:1;
		unsigned CLK_POL:1;
		unsigned DPI_O_EN:1;
		unsigned DUAL_EDGE_SEL:1;
		unsigned OUT_BIT:2;
		unsigned YC_MAP:3;
		unsigned rsv_23:9;
	};

	struct DPI_REG_SIZE {
		unsigned WIDTH:13;
		unsigned rsv_13:3;
		unsigned HEIGHT:13;
		unsigned rsv_28:3;
	};

	struct DPI_REG_DDR_SETTING {
		unsigned DDR_EN:1;
		unsigned DDR_SEL:1;
		unsigned DDR_4PHASE:1;
		unsigned DATA_THROT:1;	/* new */
		unsigned DDR_WIDTH:2;
		unsigned rsv_6:2;
		unsigned DDR_PAD_MODE:1;
		unsigned rsv_9:23;
	};

	struct DPI_REG_TGEN_HPORCH {
		unsigned HBP:12;
		unsigned rsv_12:4;
		unsigned HFP:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_TGEN_VWIDTH_LODD {
		unsigned VPW_LODD:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_LODD:1;
		unsigned rsv_17:15;
	};

	struct DPI_REG_TGEN_VPORCH_LODD {
		unsigned VBP_LODD:12;
		unsigned rsv_12:4;
		unsigned VFP_LODD:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_BG_HCNTL {
		unsigned BG_RIGHT:13;
		unsigned rsv_13:3;
		unsigned BG_LEFT:13;
		unsigned rsv_29:3;
	};

	struct DPI_REG_BG_VCNTL {
		unsigned BG_BOT:13;
		unsigned rsv_13:3;
		unsigned BG_TOP:13;
		unsigned rsv_29:3;
	};


	struct DPI_REG_BG_COLOR {
		unsigned BG_B:8;
		unsigned BG_G:8;
		unsigned BG_R:8;
		unsigned rsv_24:8;
	};

	struct DPI_REG_FIFO_CTL {
		unsigned FIFO_VALID_SET:5;
		unsigned rsv_5:3;
		unsigned FIFO_RST_SEL:1;
		unsigned rsv_9:23;
	};

	struct DPI_REG_STATUS {
		unsigned V_CNT:13;
		unsigned rsv_13:3;
		unsigned DPI_BUSY:1;
		unsigned OUT_EN:1;
		unsigned rsv_18:2;
		unsigned FIELD:1;
		unsigned TDLR:1;
		unsigned rsv_22:10;
	};

	struct DPI_REG_TMODE {
		unsigned OEN_EN:1;
		unsigned rsv_1:31;
	};

	struct DPI_REG_CHKSUM {
		unsigned CHKSUM:24;
		unsigned rsv_24:6;
		unsigned CHKSUM_RDY:1;
		unsigned CHKSUM_EN:1;
	};

	struct DPI_REG_TGEN_VWIDTH_LEVEN {
		unsigned VPW_LEVEN:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_LEVEN:1;
		unsigned rsv_17:15;
	};

	struct DPI_REG_TGEN_VPORCH_LEVEN {
		unsigned VBP_LEVEN:12;
		unsigned rsv_12:4;
		unsigned VFP_LEVEN:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_TGEN_VWIDTH_RODD {
		unsigned VPW_RODD:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_RODD:1;
		unsigned rsv_17:15;
	};

	struct DPI_REG_TGEN_VPORCH_RODD {
		unsigned VBP_RODD:12;
		unsigned rsv_12:4;
		unsigned VFP_RODD:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_TGEN_VWIDTH_REVEN {
		unsigned VPW_REVEN:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_REVEN:1;
		unsigned rsv_17:15;
	};

	struct DPI_REG_TGEN_VPORCH_REVEN {
		unsigned VBP_REVEN:12;
		unsigned rsv_12:4;
		unsigned VFP_REVEN:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_ESAV_VTIM_LOAD {
		unsigned ESAV_VOFST_LODD:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_LODD:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_ESAV_VTIM_LEVEN {
		unsigned ESAV_VVOFST_LEVEN:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_LEVEN:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_ESAV_VTIM_ROAD {
		unsigned ESAV_VOFST_RODD:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_RODD:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_ESAV_VTIM_REVEN {
		unsigned ESAV_VOFST_REVEN:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_REVEN:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_ESAV_FTIM {
		unsigned ESAV_FOFST_ODD:12;
		unsigned rsv_12:4;
		unsigned ESAV_FOFST_EVEN:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_CLPF_SETTING {
		unsigned CLPF_TYPE:2;
		unsigned rsv2:2;
		unsigned ROUND_EN:1;
		unsigned rsv5:27;
	};

	struct DPI_REG_Y_LIMIT {
		unsigned Y_LIMIT_BOT:12;
		unsigned rsv12:4;
		unsigned Y_LIMIT_TOP:12;
		unsigned rsv28:4;
	};

	struct DPI_REG_C_LIMIT {
		unsigned C_LIMIT_BOT:12;
		unsigned rsv12:4;
		unsigned C_LIMIT_TOP:12;
		unsigned rsv28:4;
	};

	struct DPI_REG_YUV422_SETTING {
		unsigned UV_SWAP:1;
		unsigned rsv1:3;
		unsigned CR_DELSEL:1;
		unsigned CB_DELSEL:1;
		unsigned Y_DELSEL:1;
		unsigned DE_DELSEL:1;
		unsigned rsv8:24;
	};

	struct DPI_REG_EMBSYNC_SETTING {
		unsigned EMBVSYNC_R_CR:1;
		unsigned EMBVSYNC_G_Y:1;
		unsigned EMBVSYNC_B_CB:1;
		unsigned rsv_3:1;
		unsigned ESAV_F_INV:1;
		unsigned ESAV_V_INV:1;
		unsigned ESAV_H_INV:1;
		unsigned rsv_7:1;
		unsigned ESAV_CODE_MAN:1;
		unsigned rsv_9:3;
		unsigned VS_OUT_SEL:3;
		unsigned rsv_15:1;
		unsigned EMBSYNC_OPT:1;	/* new */
		unsigned rsv_17:15;
	};

	struct DPI_REG_ESAV_CODE_SET0 {
		unsigned ESAV_CODE0:12;
		unsigned rsv_12:4;
		unsigned ESAV_CODE1:12;
		unsigned rsv_28:4;
	};

	struct DPI_REG_ESAV_CODE_SET1 {
		unsigned ESAV_CODE2:12;
		unsigned rsv_12:4;
		unsigned ESAV_CODE3_MSB:1;
		unsigned rsv_17:15;
	};

struct DPI_REG_BLANK_CODE_SET {
	unsigned BLANK_R:8;
	unsigned BLANK_G:8;
	unsigned BLANK_B:8;
	unsigned BLANK_CODE_EN:1;
	unsigned BLANK_PAD_OPT:1;
	unsigned rsv_26:6;
};

struct DPI_REG_MATRIX_SET {
	unsigned INT_MATRIX_SEL:5;
	unsigned rsv_5:3;
	unsigned MATRIX_BIT:2;
	unsigned rsv_10:2;
	unsigned EXT_MATRIX_EN:1;
	unsigned rsv_13:19;
};

struct DPI_REG_MATRIX_COEF_00 {
	unsigned MATRIX_COEF_00:13;
	unsigned rsv_13:3;
	unsigned MATRIX_COEF_01:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEF_02 {
	unsigned MATRIX_COEF_02:13;
	unsigned rsv_13:3;
	unsigned MATRIX_COEF_10:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEF_11 {
	unsigned MATRIX_COEF_11:13;
	unsigned rsv_13:3;
	unsigned MATRIX_COEF_12:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEF_20 {
	unsigned MATRIX_COEF_20:13;
	unsigned rsv_13:3;
	unsigned MATRIX_COEF_21:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEF_22 {
	unsigned MATRIX_COEF_22:13;
	unsigned rsv_13:19;
};

struct DPI_REG_MATRIX_IN_OFFSET_0 {
	unsigned MATRIX_IN_OFFSET_0:9;
	unsigned rsv_9:7;
	unsigned MATRIX_IN_OFFSET_1:9;
	unsigned rsv_25:7;
};

struct DPI_REG_MATRIX_IN_OFFSET_2 {
	unsigned MATRIX_IN_OFFSET_2:9;
	unsigned rsv_9:23;
};

struct DPI_REG_MATRIX_OUT_OFFSET_0 {
	unsigned MATRIX_OUT_OFFSET_0:9;
	unsigned rsv_9:7;
	unsigned MATRIX_OUT_OFFSET_1:9;
	unsigned rsv_25:7;
};

struct DPI_REG_MATRIX_OUT_OFFSET_2 {
	unsigned MATRIX_IN_OFFSET_2:9;
	unsigned rsv_9:23;
};

	struct DPI_REG_PATTERN {
		unsigned PAT_EN:1;
		unsigned rsv_1:3;
		unsigned PAT_SEL:3;
		unsigned rsv_6:1;
		unsigned PAT_B_MAN:8;
		unsigned PAT_G_MAN:8;
		unsigned PAT_R_MAN:8;
	};

/* <--not be used */
	struct DPI_REG_TGEN_HCNTL {
		unsigned HPW:8;
		unsigned HBP:8;
		unsigned HFP:8;
		unsigned HSYNC_POL:1;
		unsigned DE_POL:1;
		unsigned rsv_26:6;
	};

	struct DPI_REG_TGEN_VCNTL {
		unsigned VPW:8;
		unsigned VBP:8;
		unsigned VFP:8;
		unsigned VSYNC_POL:1;
		unsigned rsv_25:7;
	};
/* --> */

	struct DPI_REG_MATRIX_COEFF_SET0 {
		unsigned MATRIX_C00:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C01:13;
		unsigned rsv_29:3;
	};

	struct DPI_REG_MATRIX_COEFF_SET1 {
		unsigned MATRIX_C02:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C10:13;
		unsigned rsv_29:3;
	};

	struct DPI_REG_MATRIX_COEFF_SET2 {
		unsigned MATRIX_C11:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C12:13;
		unsigned rsv_29:3;
	};

	struct DPI_REG_MATRIX_COEFF_SET3 {
		unsigned MATRIX_C20:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C21:13;
		unsigned rsv_29:3;
	};

	struct DPI_REG_MATRIX_COEFF_SET4 {
		unsigned MATRIX_C22:13;
		unsigned rsv_13:19;
	};

	struct DPI_REG_MATRIX_PREADD_SET0 {
		unsigned MATRIX_PRE_ADD_0:9;
		unsigned rsv_9:7;
		unsigned MATRIX_PRE_ADD_1:9;
		unsigned rsv_24:7;
	};

	struct DPI_REG_MATRIX_PREADD_SET1 {
		unsigned MATRIX_PRE_ADD_2:9;
		unsigned rsv_9:23;
	};

	struct DPI_REG_MATRIX_POSTADD_SET0 {
		unsigned MATRIX_POST_ADD_0:13;
		unsigned rsv_13:3;
		unsigned MATRIX_POST_ADD_1:13;
		unsigned rsv_24:3;
	};

	struct DPI_REG_MATRIX_POSTADD_SET1 {
		unsigned MATRIX_POST_ADD_2:13;
		unsigned rsv_13:19;
	};

	struct DPI_REG_CLKCNTL {
		unsigned DPI_CK_DIV:5;
		unsigned EDGE_SEL_EN:1;
		unsigned rsv_6:2;
		unsigned DPI_CK_DUT:5;
		unsigned rsv_13:12;
		unsigned DPI_CKOUT_DIV:1;
		unsigned rsv_26:5;
		unsigned DPI_CK_POL:1;
	};

	struct DPI_REGS {
		struct DPI_REG_EN DPI_EN;	/* 0000 */
		struct DPI_REG_RST DPI_RST;	/* 0004 */
		struct DPI_REG_INTERRUPT INT_ENABLE;	/* 0008 */
		struct DPI_REG_INTERRUPT INT_STATUS;	/* 000C */
		struct DPI_REG_CNTL CNTL;	/* 0010 */
		struct DPI_REG_OUTPUT_SETTING OUTPUT_SETTING;	/* 0014 */
		struct DPI_REG_SIZE SIZE;	/* 0018 */
		struct DPI_REG_DDR_SETTING DDR_SETTING;	/* 001c */
		uint32_t TGEN_HWIDTH;	/* 0020 */
		struct DPI_REG_TGEN_HPORCH TGEN_HPORCH;	/* 0024 */
		struct DPI_REG_TGEN_VWIDTH_LODD TGEN_VWIDTH_LODD;	/* 0028 */
		struct DPI_REG_TGEN_VPORCH_LODD TGEN_VPORCH_LODD;	/* 002C */
		struct DPI_REG_BG_HCNTL BG_HCNTL;	/* 0030 */
		struct DPI_REG_BG_VCNTL BG_VCNTL;	/* 0034 */
		struct DPI_REG_BG_COLOR BG_COLOR;	/* 0038 */
		struct DPI_REG_FIFO_CTL FIFO_CTL;	/* 003C */

		struct DPI_REG_STATUS STATUS;	/* 0040 */
		struct DPI_REG_TMODE TMODE;	/* 0044 */
		struct DPI_REG_CHKSUM CHKSUM;	/* 0048 */
		uint32_t rsv_4C;
		uint32_t DUMMY;	/* 0050 */
		uint32_t rsv_54[5];
		struct DPI_REG_TGEN_VWIDTH_LEVEN TGEN_VWIDTH_LEVEN;	/* 0068 */
		struct DPI_REG_TGEN_VPORCH_LEVEN TGEN_VPORCH_LEVEN;	/* 006C */
		struct DPI_REG_TGEN_VWIDTH_RODD TGEN_VWIDTH_RODD;	/* 0070 */
		struct DPI_REG_TGEN_VPORCH_RODD TGEN_VPORCH_RODD;	/* 0074 */
		struct DPI_REG_TGEN_VWIDTH_REVEN TGEN_VWIDTH_REVEN;	/* 0078 */
		struct DPI_REG_TGEN_VPORCH_REVEN TGEN_VPORCH_REVEN;	/* 007C */
		struct DPI_REG_ESAV_VTIM_LOAD ESAV_VTIM_LOAD;	/* 0080 */
		struct DPI_REG_ESAV_VTIM_LEVEN ESAV_VTIM_LEVEN;	/* 0084 */
		struct DPI_REG_ESAV_VTIM_ROAD ESAV_VTIM_ROAD;	/* 0088 */
		struct DPI_REG_ESAV_VTIM_REVEN ESAV_VTIM_REVEN;	/* 008C */
		struct DPI_REG_ESAV_FTIM ESAV_FTIM;	/* 0090 */
		struct DPI_REG_CLPF_SETTING CLPF_SETTING;	/* 0094 */
		struct DPI_REG_Y_LIMIT Y_LIMIT;	/* 0098 */
		struct DPI_REG_C_LIMIT C_LIMIT;	/* 009C */
		struct DPI_REG_YUV422_SETTING YUV422_SETTING;	/* 00A0 */
		struct DPI_REG_EMBSYNC_SETTING EMBSYNC_SETTING;	/* 00A4 */
		struct DPI_REG_ESAV_CODE_SET0 ESAV_CODE_SET0;	/* 00A8 */
		struct DPI_REG_ESAV_CODE_SET1 ESAV_CODE_SET1;	/* 00AC */
		struct DPI_REG_BLANK_CODE_SET BLANK_CODE_SET;	/* 00b0*/
		struct DPI_REG_MATRIX_SET MATRIX_SET;	/* 00b4*/
		struct DPI_REG_MATRIX_COEF_00 MATRIX_COEF_00;	/* 00b8*/
		struct DPI_REG_MATRIX_COEF_02 MATRIX_COEF_02;	/* 00bc*/
		struct DPI_REG_MATRIX_COEF_11 MATRIX_COEF_11;	/* 00c0*/
		struct DPI_REG_MATRIX_COEF_20 MATRIX_COEF_20;	/* 00c4*/
		struct DPI_REG_MATRIX_COEF_22 MATRIX_COEF_22;	/* 00c8*/
		struct DPI_REG_MATRIX_IN_OFFSET_0 MATRIX_IN_OFFSET_0;	/* 00cc*/
		struct DPI_REG_MATRIX_IN_OFFSET_2 MATRIX_IN_OFFSET_2;	/* 00d0*/
		struct DPI_REG_MATRIX_OUT_OFFSET_0 MATRIX_OUT_OFFSET_0;	/* 00d4*/
		struct DPI_REG_MATRIX_OUT_OFFSET_2 MATRIX_OUT_OFFSET_2;	/* 00d8*/
		uint32_t rsv_dc;
		struct DPI_REG_CLKCNTL DPI_CLKCON;	/* 00E0 */
	};

/*LVDS TX REG defines*/
	struct LVDS_REG_FMTCNTL {
		unsigned HSYNC_INV:1;
		unsigned VSYNC_INV:1;
		unsigned DE_INV:1;
		unsigned rsv_3:1;
		unsigned DATA_FMT:3;
		unsigned rsv_7:25;
	};

	struct LVDS_REG_DATA_SRC {
		unsigned R_SEL:2;
		unsigned G_SEL:2;
		unsigned B_SEL:2;
		unsigned rsv_6:2;
		unsigned R_SRC_VAL:8;
		unsigned G_SRC_VAL:8;
		unsigned B_SRC_VAL:8;
	};

	struct LVDS_REG_CNTL {
		unsigned HSYNC_SEL:2;
		unsigned VSYNC_SEL:2;
		unsigned DE_SEL:2;
		unsigned SRC_FOR_HSYNC:1;
		unsigned SRC_FOR_VSYNC:1;
		unsigned SRC_FOR_DE:1;
		unsigned rsv_9:23;
	};

	struct LVDS_REG_R_SEL {
		unsigned R0_SEL:3;
		unsigned R1_SEL:3;
		unsigned R2_SEL:3;
		unsigned R3_SEL:3;
		unsigned R4_SEL:3;
		unsigned R5_SEL:3;
		unsigned R6_SEL:3;
		unsigned R7_SEL:3;
		unsigned rsv_24:8;
	};

	struct LVDS_REG_G_SEL {
		unsigned G0_SEL:3;
		unsigned G1_SEL:3;
		unsigned G2_SEL:3;
		unsigned G3_SEL:3;
		unsigned G4_SEL:3;
		unsigned G5_SEL:3;
		unsigned G6_SEL:3;
		unsigned G7_SEL:3;
		unsigned rsv_24:8;
	};

	struct LVDS_REG_B_SEL {
		unsigned B0_SEL:3;
		unsigned B1_SEL:3;
		unsigned B2_SEL:3;
		unsigned B3_SEL:3;
		unsigned B4_SEL:3;
		unsigned B5_SEL:3;
		unsigned B6_SEL:3;
		unsigned B7_SEL:3;
		unsigned rsv_24:8;
	};

	struct LVDS_REG_OUT_CTRL {
		unsigned LVDS_EN:1;
		unsigned LVDS_OUT_FIFO_EN:1;
		unsigned DPMODE:1;
		unsigned rsv_4:28;
		unsigned LVDSRX_FIFO_EN:1;
	};

	struct LVDS_REG_CH_SWAP {
		unsigned CH0_SEL:3;
		unsigned CH1_SEL:3;
		unsigned CH2_SEL:3;
		unsigned CH3_SEL:3;
		unsigned CLK_SEL:3;
		unsigned rsv_15:1;
		unsigned ML_SWAP:5;
		unsigned rsv_21:3;
		unsigned PN_SWAP:5;
		unsigned rsv_29:2;
		unsigned BIT_SWAP:1;
	};

	struct LVDS_REG_CLK_CTRL {
		unsigned TX_CK_EN:1;
		unsigned RX_CK_EN:1;	/* used for out crc */
		unsigned TEST_CK_EN:1;
		unsigned rsv_3:5;
		unsigned TEST_CK_SEL0:1;
		unsigned TEST_CK_SEL1:1;
		unsigned rsv_10:22;
	};

	struct LVDS_REG_CRC_CTRL {
		unsigned RG_CRC_START:1;
		unsigned RG_CRC_CLR:1;
		unsigned rsv_2:6;
		unsigned RG_LVDSRX_CRC_START1:1;
		unsigned rsv_9:1;
		unsigned RG_LVDSRX_CRC_CLR1:1;
		unsigned rsv_11:21;
	};

	struct LVDS_REG_RG_CLK {
		unsigned RG_CLK_CTRL:7;
		unsigned RG_CLKEN:1;	/* used for out crc */
		unsigned RG_RES_BIT:1;
		unsigned rsv_9:23;
	};

	struct LVDS_REG_RG_MON {
		unsigned RG_MON_SEL:6;
		unsigned rsv_6:2;
		unsigned RG_MON_EN:1;	/* used for out crc */
		unsigned rsv_9:7;
		unsigned RG_TEST_EN:1;
		unsigned rsv_17:15;
	};

	struct LVDS_REG_SOFT_RESET {
		unsigned RG_PCLK_SW_RST:1;
		unsigned RG_CTSCLK_SW_RST:1;
		unsigned rsv_2:30;
	};

	struct LVDS_REG_RGCHL_SRC0 {	/*10BIT LVDS */
		unsigned RG_LVDS_CH0:10;
		unsigned RG_LVDS_CH1:10;
		unsigned RG_LVDS_CH2:10;
		unsigned rsv_30:2;
	};

	struct LVDS_REG_RGCHL_SRC1 {	/*10BIT LVDS */
		unsigned RG_LVDS_CH3:10;
		unsigned RG_LVDS_CLK:10;
		unsigned rsv_20:11;
		unsigned RG_PAT_EN:1;
	};

	struct LVDS_REG_PATN_TOTAL {
		unsigned RG_PTGEN_HTOTAL:13;
		unsigned rsv_13:3;
		unsigned RG_PTGEN_VTOTAL:12;
		unsigned rsv_28:4;
	};

	struct LVDS_REG_PATN_WIDTH {
		unsigned RG_PTGEN_HWIDTH:13;
		unsigned rsv_13:3;
		unsigned RG_PTGEN_VWIDTH:12;
		unsigned rsv_28:4;
	};

	struct LVDS_REG_PATN_START {
		unsigned RG_PTGEN_HSTART:13;
		unsigned rsv_13:3;
		unsigned RG_PTGEN_VSTART:12;
		unsigned rsv_28:4;
	};

	struct LVDS_REG_PATN_ACTIVE {
		unsigned RG_PTGEN_HACTIVE:13;
		unsigned rsv_13:3;
		unsigned RG_PTGEN_VACTIVE:12;
		unsigned rsv_28:4;
	};

	struct LVDS_REG_RGTST_PAT {
		unsigned RG_TST_PATN_EN:1;
		unsigned rsv_1:7;
		unsigned RG_TST_PATN_TYPE:8;
		unsigned RG_PTGEN_CLOR_BAR_TH:12;
		unsigned rsv_28:4;
	};

	struct LVDS_REG_RGPAT_EDGE {
		unsigned RG_BD_R:8;
		unsigned RG_BD_G:8;
		unsigned RG_BD_B:8;
		unsigned rsv_24:8;
	};

	struct LVDS_REG_RGPAT_SRC {
		unsigned RG_PTGEN_R:8;
		unsigned RG_PTGEN_G:8;
		unsigned RG_PTGEN_B:8;
		unsigned rsv_24:8;
	};

	struct LVDS_REG_RGCRC_STATU {
		unsigned RG_OUT_CRC_RDY:1;
		unsigned RG_OUT_CRC_VALUE:16;
		unsigned rsv_17:15;
	};

	struct LVDS_TX_REGS {
		struct LVDS_REG_FMTCNTL LVDS_FMTCTRL;	/* 0000 */
		struct LVDS_REG_DATA_SRC LVDS_DATA_SRC;	/* 0004 */
		struct LVDS_REG_CNTL LVDS_CTRL;	/* 0008 */
		struct LVDS_REG_R_SEL LVDS_R_SEL;	/* 000C */
		struct LVDS_REG_G_SEL LVDS_G_SEL;	/* 0010 */
		struct LVDS_REG_B_SEL LVDS_B_SEL;	/* 0014 */
		struct LVDS_REG_OUT_CTRL LVDS_OUT_CTRL;	/* 0018 */
		struct LVDS_REG_CH_SWAP LVDS_CH_SWAP;	/* 001C */
		struct LVDS_REG_CLK_CTRL LVDS_CLK_CTRL;	/* 0020 */
		struct LVDS_REG_CRC_CTRL LVDS_CRC_CTRL;
		struct LVDS_REG_RG_CLK LVDS_RG_CLK;
		struct LVDS_REG_RG_MON LVDS_RG_MON;
		struct LVDS_REG_SOFT_RESET LVDS_SOFT_RESET;
		struct LVDS_REG_RGCHL_SRC0 LVDS_RGCHL_SCR0;
		struct LVDS_REG_RGCHL_SRC1 LVDS_RGCHL_SCR1;
		struct LVDS_REG_PATN_TOTAL LVDS_PATN_TOTAL;
		struct LVDS_REG_PATN_WIDTH LVDS_PATN_WIDTH;
		struct LVDS_REG_PATN_START LVDS_PATN_START;
		struct LVDS_REG_PATN_ACTIVE LVDS_PATN_ACTIVE;
		struct LVDS_REG_RGTST_PAT LVDS_RGTST_PAT;
		struct LVDS_REG_RGPAT_EDGE LVDS_RGPAT_EDGE;
		struct LVDS_REG_RGPAT_SRC LVDS_RGPAT_SRC;
		struct LVDS_REG_RGCRC_STATU LVDS_RGCRC_STATU;
	};


/* LVDS ANA REG defines*/
	struct LVDS_ANA_REG_CTL2 {
		unsigned LVDSTX_ANA_TVO:4;
		unsigned LVDSTX_ANA_TVCM:4;
		unsigned LVDSTX_ANA_NSRC:4;
		unsigned LVDSTX_ANA_PSRC:4;
		unsigned LVDSTX_ANA_BIAS_SEL:2;
		unsigned LVDSTX_ANA_R_TERM:2;
		unsigned LVDSTX_ANA_SEL_CKTST:1;
		unsigned LVDSTX_ANA_SEL_MERGE:1;
		unsigned LVDSTX_ANA_LDO_EN:1;
		unsigned LVDSTX_ANA_BIAS_EN:1;
		unsigned LVDSTX_ANA_SER_ABIST_EN:1;
		unsigned LVDSTX_ANA_SER_ABEDG_EN:1;
		unsigned LVDSTX_ANA_SER_BIST_TOG:1;

		unsigned rsv_27:5;
	};

	struct LVDS_ANA_REG_CTL3 {
		unsigned LVDSTX_ANA_IMP_TEST_EN:5;
		unsigned LVDSTX_ANA_EXT_EN:5;
		unsigned LVDSTX_ANA_DRV_EN:5;
		unsigned rsv_15:1;
		unsigned LVDSTX_ANA_SER_DIN_SEL:1;
		unsigned LVDSTX_ANA_SER_CLKDIG_INV:1;
		unsigned rsv_18:2;
		unsigned LVDSTX_ANA_SER_DIN:10;
		unsigned rsv_30:2;
	};

	struct LVDS_VPLL_REG_CTL1 {
		unsigned LVDS_VPLL_RESERVE:2;
		unsigned LVDS_VPLL_LVROD_EN:1;
		unsigned rsv_3:1;
		unsigned LVDS_VPLL_RST_DLY:2;
		unsigned LVDS_VPLL_FBKSEL:2;
		unsigned LVDS_VPLL_DDSFBK_EN:1;
		unsigned rsv_9:3;
		unsigned LVDS_VPLL_FBKDIV:7;
		unsigned rsv_19:1;
		unsigned LVDS_VPLL_PREDIV:2;
		unsigned LVDS_VPLL_POSDIV:2;
		unsigned LVDS_VPLL_VCO_DIV_SEL:1;
		unsigned LVDS_VPLL_BLP:1;
		unsigned LVDS_VPLL_BP:1;
		unsigned LVDS_VPLL_BR:1;
		unsigned rsv_28:4;
	};

	struct LVDS_VPLL_REG_CTL2 {
		unsigned LVDS_VPLL_DIVEN:3;
		unsigned rsv_3:1;
		unsigned LVDS_VPLL_MONCK_EN:1;
		unsigned LVDS_VPLL_MONVC_EN:1;
		unsigned LVDS_VPLL_MONREF_EN:1;
		unsigned LVDS_VPLL_EN:1;
		unsigned LVDS_VPLL_TXDIV1:2;
		unsigned LVDS_VPLL_TXDIV2:2;
		unsigned LVDS_VPLL_LVDS_EN:1;
		unsigned LVDS_VPLL_LVDS_DPIX_DIV2:1;
		unsigned LVDS_VPLL_TTL_EN:1;
		unsigned rsv_15:1;
		unsigned LVDS_VPLL_TTLDIV:2;
		unsigned LVDS_VPLL_TXSEL:1;
		unsigned rsv_19:1;
		unsigned LVDS_VPLL_RESERVE1:3;
		unsigned LVDS_VPLL_CLK_SEL:1;
		unsigned rsv_24:8;
	};

struct LVDS_VPLL_REG_CTL3 {
	unsigned LVDS_TX_21EDG	: 1;
	unsigned LVDS_TX_21LEV	: 1;
	unsigned LVDS_TX_51EDG	: 1;
	unsigned LVDS_TX_51LEV	: 1;
	unsigned rsv_4: 28;
};

	struct LVDS_ANA_REGS {
		uint32_t LVDSTX_ANA_CTL1;	/* 0000 */
		struct LVDS_ANA_REG_CTL2 LVDSTX_ANA_CTL2;	/* 0004 */
		struct LVDS_ANA_REG_CTL3 LVDSTX_ANA_CTL3;	/* 0008 */
		uint32_t LVDSTX_ANA_CTL4;	/* 000C */
		uint32_t LVDSTX_ANA_CTL5;	/* 0010 */
		struct LVDS_VPLL_REG_CTL1 LVDS_VPLL_CTL1;	/* 0014 */
		struct LVDS_VPLL_REG_CTL2 LVDS_VPLL_CTL2;	/* 0018 */
		struct LVDS_VPLL_REG_CTL3 VPLL_REG_CTL3;
	};


/*
*
#ifndef BUILD_UBOOT
STATIC_ASSERT(0x0018 == offsetof(DPI_REGS, SIZE));
STATIC_ASSERT(0x0038 == offsetof(DPI_REGS, BG_COLOR));
STATIC_ASSERT(0x0070 == offsetof(DPI_REGS, TGEN_VWIDTH_RODD));
STATIC_ASSERT(0x00AC == offsetof(DPI_REGS, ESAV_CODE_SET1));
#endif
*/
#ifdef __cplusplus
}
#endif
#endif				/* __DPI_REG_H__ */
