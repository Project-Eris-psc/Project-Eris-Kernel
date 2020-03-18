# WMT HAL driver for MT8167

ccflags-y += \
	-I$(src)/include \
	-I$(src)/../linux/include \
	-I$(src)/../include \
	-I$(src)/../../common_detect

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach

subdir-ccflags-y += -DMTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

obj-y += mtk_wcn_consys_hw.o
obj-y += wmt_plat_alps.o
