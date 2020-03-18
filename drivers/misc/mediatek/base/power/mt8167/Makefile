#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mt8167
ccflags-y += -I$(srctree)/drivers/misc/mediatek/thermal/mt8167/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/hps_v1/
obj-$(CONFIG_MACH_MT8167) += mt8167-cpufreq.o
obj-$(CONFIG_MACH_MT8167) += mtk_static_power.o
obj-$(CONFIG_MACH_MT8167) += mtk_power_throttle.o
obj-$(CONFIG_MACH_MT8167) += mtk_ptp.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm_internal.o
obj-$(CONFIG_MACH_MT8167) += mtk_sleep.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm_pcm.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm_sleep.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm_dpidle.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm_sodi.o
obj-$(CONFIG_MACH_MT8167) += mtk_spm_fs.o
obj-$(CONFIG_MACH_MT8167) += mtk_udi.o
obj-$(CONFIG_MACH_MT8167) += mtk_dcm.o
obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpufreq.o

obj-y += mtk_cpuidle.o

obj-$(CONFIG_ARM64) += cpu_dormant.o
obj-$(CONFIG_ARM) += cpu_dormant-v7.o
obj-y += mtk_idle.o
