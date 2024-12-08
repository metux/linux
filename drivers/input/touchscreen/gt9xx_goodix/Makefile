#
# Makefile for the Goodix gt9xx touchscreen driver.
#
#subdir-ccflags-y += -DDEBUG
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_TOUCHSCREEN_GT9XX)		+= gt9xx_tp.o
gt9xx_tp-objs	+= gt9xx.o
gt9xx_tp-objs	+= gt9xx_update.o
gt9xx_tp-objs	+= goodix_tool.o
