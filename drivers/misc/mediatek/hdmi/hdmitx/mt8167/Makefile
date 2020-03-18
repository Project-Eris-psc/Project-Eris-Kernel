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

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -DMTK_INTERNAL_HDMI_SUPPORT
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/ \
             -I$(srctree)/drivers/misc/mediatek/video/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/staging/android \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(CONFIG_MTK_PLATFORM) \
             -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/misc/mediatek/hdmi/inc/ \
             -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(CONFIG_MTK_PLATFORM)/inc/ \
             -I$(srctree)/drivers/misc/mediatek//ext_disp/ \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/v1/ \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone/ \


ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/mhltx/$(CONFIG_MTK_PLATFORM)
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),"nxp_tda19989")
	ccflags-y +=	-DHDMI_TDA19989
endif

ifeq ($(CONFIG_MTK_OVERLAY_ENGINE_SUPPORT), y)
ccflags-y += -DMTK_OVERLAY_ENGINE_SUPPORT
endif

ifneq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),)
ccflags-y += -I$(srctree)/include/trustzone/
endif

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),"it668x")
  ccflags-y +=	-DMHL_BRIDGE_SUPPORT/
endif

obj-y := hdmi_drv.o hdmi_ctrl.o hdmiavd.o hdmicec.o hdmiddc.o hdmiedid.o hdmihdcp.o hdmicmd.o hdmictrl.o hdmi_debug.o

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/include/trustzone/
obj-y += hdmi_ca.o
endif

endif
