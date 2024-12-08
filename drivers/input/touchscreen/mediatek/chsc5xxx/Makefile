# Makefile for the focaltech touchscreen drivers.

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_CHSC5XXX_TOUCHPANEL_DRIVER) += chsc5xxx.o
chsc5xxx-objs += basic_util.o i2c_communication.o mtk_core.o semi_touch_function.o semi_touch_upgrade.o semi_touch_apk.o semi_touch_device.o semi_touch_custom.o
#obj-y += basic_util.o
#obj-y += i2c_communication.o
#obj-y += mtk_core.o
#obj-y += semi_touch_function.o
#obj-y += semi_touch_upgrade.o
#obj-y += semi_touch_apk.o
#obj-y += semi_touch_device.o
#obj-y += semi_touch_custom.o


