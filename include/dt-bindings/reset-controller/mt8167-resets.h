/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_RESET_CONTROLLER_MT8167
#define _DT_BINDINGS_RESET_CONTROLLER_MT8167

/* TOPRGU resets */
#define MT8167_TOPRGU_DDRPHY_FLASH_RST        0 /* reset ddrphy and flash pad macro */
#define MT8167_TOPRGU_AUD_PAD_RST             1 /* Write 1 to reset audio_tdm_in_pad,audio_tdm_pad,audio_fifo */
#define MT8167_TOPRGU_MM_RST                  2 /* Write 1 to reset MMSYS */
#define MT8167_TOPRGU_MFG_RST                 3 /* Write 1 to reset MFG */
#define MT8167_TOPRGU_MDSYS_RST               4 /* Write 1 to reset INFRA_AO */
#define MT8167_TOPRGU_CONN_RST                5 /* Write 1 to reset CONNSYS WDT reset */
#define MT8167_TOPRGU_PAD2CAM_DIG_MIPI_RX_RST 6 /* Write 1 to reset MM and its related pad macro(DPI,MIPI_CFG,MIPI_TX) */
#define MT8167_TOPRGU_DIG_MIPI_TX_RST         7 /* Write 1 to reset digi_mipi_tx */
#define MT8167_TOPRGU_SPI_PAD_MACRO_RST       8 /* Write 1 to reset SPI_PAD_MACRO */
#define MT8167_TOPRGU_APMIXED_RST             10 /* Write 1 to reset APMIXEDSYS */
#define MT8167_TOPRGU_VDEC_RST                11 /* Write 1 to reset VDEC module */
#define MT8167_TOPRGU_CONN_MCU_RST            12 /* Write 1 to reset CONNSYS */
#define MT8167_TOPRGU_EFUSE_RST               13 /* Write 1 to reset efuse */
#define MT8167_TOPRGU_PWRAP_SPICTL_RST        14 /* Write 1 to reset pwrap_spictl module */

#endif  /* _DT_BINDINGS_RESET_CONTROLLER_MT8167 */
