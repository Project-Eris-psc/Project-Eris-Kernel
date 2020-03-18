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


#ifndef __MMDVFS_CONFIG_UTIL_H__
#define __MMDVFS_CONFIG_UTIL_H__

#include "mtk_smi.h"
#include "mmdvfs_mgr.h"


#define MMDVFS_CLK_CONFIG_BY_MUX    (0)
#define MMDVFS_CLK_CONFIG_HOPPING   (1)
#define MMDVFS_CLK_CONFIG_NONE      (2)

/* System OPP number limitation (Max opp support)*/
#define MMDVFS_OPP_NUM_LIMITATION		(6)    /* Max total MMDVFS opps of the profile support */
#define MMDVFS_CLK_OPP_NUM_LIMITATION		(6) /* Max total CLK opps of the profile support */
#define MMDVFS_VCORE_OPP_NUM_LIMITATION		(6) /* Max total CLK opps of the profile support */

/* MMDVFS OPPs (External interface, Vcore and MMCLK combined)*/
#define MMDVFS_FINE_STEP_UNREQUEST (-1)
#define MMDVFS_FINE_STEP_OPP0 (0)
#define MMDVFS_FINE_STEP_OPP1 (1)
#define MMDVFS_FINE_STEP_OPP2 (2)
#define MMDVFS_FINE_STEP_OPP3 (3)
#define MMDVFS_FINE_STEP_OPP4 (4)
#define MMDVFS_FINE_STEP_OPP5 (5)



/* MMCLK opps (internal used in MMDVFS) */
#define MMDVFS_MMCLK_OPP0 (0)
#define MMDVFS_MMCLK_OPP1 (1)
#define MMDVFS_MMCLK_OPP2 (2)
#define MMDVFS_MMCLK_OPP3 (3)
#define MMDVFS_MMCLK_OPP4 (4)
#define MMDVFS_MMCLK_OPP5 (5)


/* Defined the max size of MMDVFS CLK MUX table */
#define MMDVFS_CLK_MUX_NUM_LIMITATION (15)

/* CLK MUX ID */
/* Define the internal index of each MM CLK */
#define MMDVFS_CLK_MUX_TOP_SMI0_2X_SEL  (0)
#define MMDVFS_CLK_MUX_TOP_MM_SEL   (1)
#define MMDVFS_CLK_MUX_TOP_CAM_SEL  (2)
#define MMDVFS_CLK_MUX_TOP_IMG_SEL  (3)
#define MMDVFS_CLK_MUX_TOP_VENC_SEL (4)
#define MMDVFS_CLK_MUX_TOP_VDEC_SEL (5)
#define MMDVFS_CLK_MUX_TOP_MJC_SEL  (6)
#define MMDVFS_CLK_MUX_TOP_DSP_SEL  (7)
#define MMDVFS_CLK_MUX_TOP_VPU_IF_SEL  (8)
#define MMDVFS_CLK_MUX_TOP_VPU_SEL  (9)
#define MMDVFS_CLK_MUX_NUM (10)


/* Configuration Header */
struct mmdvfs_cam_property {
	int sensor_size;
	int feature_flag;
	int fps;
};

struct mmdvfs_video_property {
	int width;
	int height;
	int codec;
};

struct mmdvfs_profile {
	const char *profile_name;
	int smi_scenario_id;
	struct mmdvfs_cam_property cam_limit;
	struct mmdvfs_video_property video_limit;
};

/* HW Configuration Management */

struct mmdvfs_clk_mux_desc {
	void *ccf_handle;
	char *ccf_name; /* For debugging*/
};

struct mmdvfs_clk_source_desc {
	void *ccf_handle;
	char *ccf_name; /* For debugging*/
	int requested_clk; /* For debugging*/
};

struct mmdvfs_clk_hw_map {
	int config_method; /* 0: don't set, 1: clk_mux, 2: pll hopping */
	struct mmdvfs_clk_mux_desc clk_mux;
	int pll_id;
	int total_step;
	/* DSS value of each step for this clk */
	int step_hopping_dds_map[MMDVFS_CLK_OPP_NUM_LIMITATION];
	/* CLK MUX of each step for this clk */
	int step_clk_source_id_map[MMDVFS_CLK_OPP_NUM_LIMITATION];
};

struct mmdvfs_clk_hw_configuration {
	int config_method; /* 0: don't set, 1: clk_mux, 2: pll hopping */
	struct mmdvfs_clk_mux_desc clk_mux;
	int pll_id;
	int total_steps;
	int step_hopping_dds_map[MMDVFS_CLK_MUX_NUM_LIMITATION];
	int step_clk_source_id_map[MMDVFS_CLK_MUX_NUM_LIMITATION];
};

/* Record vcore and step of clks to be requested for a single MMDVFS step */
struct mmdvfs_hw_configurtion {
	int vcore_step;
	int clk_steps[MMDVFS_CLK_MUX_NUM_LIMITATION];
	int total_clks;
};

/* For a single mmdvfs step's profiles and associated hardware configuration */
struct mmdvfs_step_to_profile_mapping {
	int mmdvfs_step;
	struct mmdvfs_profile *profiles;
	int total_profiles;
	struct mmdvfs_hw_configurtion hw_config;
};

struct mmdvfs_adaptor {
	int enable_vcore;
	int enable_clk_mux;
	int enable_pll_hopping;
	struct mmdvfs_clk_source_desc *mmdvfs_clk_sources;
	int mmdvfs_clk_sources_num;
	struct mmdvfs_clk_hw_map *mmdvfs_clk_hw_maps;
	int mmdvfs_clk_hw_maps_num;
	struct mmdvfs_step_to_profile_mapping *step_profile_mappings;
	int step_num;
	int disable_auto_control_mask;
	void (*profile_dump_func)(struct mmdvfs_adaptor *self);
	void (*single_hw_configuration_dump_func)(struct mmdvfs_adaptor *self,
	struct mmdvfs_hw_configurtion *hw_configuration);
	void (*hw_configuration_dump_func)(struct mmdvfs_adaptor *self);
	int (*determine_mmdvfs_step)(struct mmdvfs_adaptor *self,
	int smi_scenario, struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting);
	int (*apply_hw_configurtion_by_step)(struct mmdvfs_adaptor *self, int mmdvfs_step, int current_step);
	int (*apply_vcore_hw_configurtion_by_step)(struct mmdvfs_adaptor *self, int mmdvfs_step);
	int (*apply_clk_hw_configurtion_by_step)(struct mmdvfs_adaptor *self, int mmdvfs_step);
	int (*get_cam_sys_clk)(struct mmdvfs_adaptor *self, int mmdvfs_step);
	/* static members */
	void (*single_profile_dump_func)(struct mmdvfs_profile *profile);

};


struct mmdvfs_step_util {
	/* Record each scenaio's opp */
	int mmdvfs_scenario_mmdvfs_opp[MMDVFS_SCEN_COUNT];
	int total_scenario;
	/* Record the currency status of each opps*/
	int mmdvfs_concurrency_of_opps[MMDVFS_OPP_NUM_LIMITATION];
	int total_opps;
	int *mmclk_oop_to_legacy_step;
	int mmclk_oop_to_legacy_step_num;
	int *legacy_step_to_oop;
	int legacy_step_to_oop_num;
	void (*init)(struct mmdvfs_step_util *self);
	int (*get_legacy_mmclk_step_from_mmclk_opp)(
	struct mmdvfs_step_util *self, int mmclk_step);
	int (*get_opp_from_legacy_step)(struct mmdvfs_step_util *self,
	int legacy_step);
	int (*set_step)(struct mmdvfs_step_util *self, int step, int client_id);
	int (*get_clients_clk_opp)(struct mmdvfs_step_util *self, struct mmdvfs_adaptor *adaptor,
	int clients_mask, int clk_id);
};

extern struct mmdvfs_adaptor *g_mmdvfs_adaptor;
extern struct mmdvfs_step_util *g_mmdvfs_step_util;
void mmdvfs_config_util_init(void);

#endif /* __MMDVFS_CONFIG_UTIL_H__ */
