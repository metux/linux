# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT9XX/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT9XX/include/$(CONFIG_GTP_FIRMWARE)/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT9XX/include/$(CONFIG_GTP_CONFIG)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT9XX/include/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/GT9XX/include/mid/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -DANDROID_GKI_VFS_EXPORT_ONLY=VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver


ifeq ($(CONFIG_MID_CSCI_SUPPORT), y)
KBUILD_CPPFLAGS += -DTP_COMPATIBLE
else ifeq ($(CONFIG_MID_CSCI_SUPPORT), m)
KBUILD_CPPFLAGS += -DTP_COMPATIBLE
endif

obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+=  gt9xx.o

gt9xx-$(CONFIG_MID_CSCI_SUPPORT) += gtxx_config.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+=  gt9xx_driver.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+=  gt9xx_extents.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+=  gt9xx_update.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+=  goodix_tool.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)   +=  tpd_control.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)   +=  tpd_button.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)   +=  tpd_debug.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)   +=  tpd_default.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)   +=  tpd_misc.o
gt9xx-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)   +=  tpd_setting.o