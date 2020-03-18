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

#
# Makefile for misc devices that really don't fit anywhere else.

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

define FILTER_PROJECT_DRV
ifeq ($(wildcard $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/imgsensor/$(1)),)
platform_drv += $(1)
endif
endef

# custom setting: if demo board, set i2c bus number: 2
ifneq (,$(findstring tb8167p,$(ARCH_MTK_PROJECT)))
ccflags-y += -DSUPPORT_I2C_BUS_NUM1=2
ccflags-y += -DSUPPORT_I2C_BUS_NUM2=2
ccflags-y += -DDEMO_BOARD_SUPPORT=1
#$(info "ARCH_MTK_PROJECT:$(ARCH_MTK_PROJECT), set I2C Bus num:2")
else
	# 8167 evb
	ifneq (,$(findstring tb8167m,$(ARCH_MTK_PROJECT)))
		ccflags-y += -DDEMO_BOARD_SUPPORT=0
	# customer projects... use DEMO_BOARD_SUPPORT for using GPIO instead of pinctrl
	else
		ccflags-y += -DDEMO_BOARD_SUPPORT=1
	endif
#$(info "ARCH_MTK_PROJECT:$(ARCH_MTK_PROJECT)")
endif

obj-y				+= kd_sensorlist.o
obj-y				+= camera_hw/

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_PROJECT_DRV,$(drv))))
custom_kernel_imgsensor := $(addsuffix /, $(shell echo $(platform_drv)))
obj-y += $(custom_kernel_imgsensor)
#$(info imgsensor drv by platform $(platform_drv))
