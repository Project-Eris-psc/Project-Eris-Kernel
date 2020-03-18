#
# Copyright (C) 2016 MediaTek Inc.
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

obj-$(CONFIG_MTK_PMIC)          += pmic_mt6392.o pmic_chr_type_det.o pmic_auxadc.o

obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o

ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24196.o tbl_helper_func.o
else
ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
     obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24296.o tbl_helper_func.o
else
     obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_pmic.o
endif
endif

