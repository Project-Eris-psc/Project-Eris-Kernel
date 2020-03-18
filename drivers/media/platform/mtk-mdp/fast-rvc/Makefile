ifeq ($(CONFIG_MTK_CMDQ_MT2701),y)

mtk-mdp-frvc-y += mtk_mdp_frvc.o
ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/cmdq/mt2701 \
    -I$(srctree)/drivers/misc/mediatek/include

else

mtk-mdp-frvc-y += mtk_mdp_frvc_new.o

endif

obj-y += mtk-mdp-frvc.o

ccflags-y += \
    -I$(srctree)/drivers/media/platform/mtk-mdp

ccflags-$(CONFIG_VIDEO_MEDIATEK_VCU) += \
    -I$(srctree)/drivers/media/platform/mtk-vcu
