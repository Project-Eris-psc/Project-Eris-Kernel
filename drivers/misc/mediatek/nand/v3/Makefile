ifeq ($(CONFIG_MTK_COMBO_NAND_SUPPORT), y)
ccflags-y += -DMTK_COMBO_NAND_SUPPORT
endif

obj-$(CONFIG_MTK_MTD_NAND) += mtk_nand.o
ifeq ($(CONFIG_MTK_SLC_NAND_BURNER_SUPPORT), y)
obj-$(CONFIG_MTK_MTD_NAND) += bmt_burner.o
else
obj-$(CONFIG_MTK_MTD_NAND) += bmt.o
endif
obj-$(CONFIG_MTK_MTD_NAND) += partition_mt.o
