# Makefile for the focaltech touchscreen drivers.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat


obj-$(CONFIG_TOUCHSCREEN_FTS_SPI)	+=  focaltechspi.o
focaltechspi-objs	+=  focaltech_core.o
focaltechspi-objs	+=  focaltech_ex_fun.o
focaltechspi-objs	+=  focaltech_ex_mode.o
focaltechspi-objs	+=  focaltech_gesture.o
focaltechspi-objs	+=  focaltech_esdcheck.o
focaltechspi-objs	+=  focaltech_point_report_check.o
focaltechspi-objs  +=  focaltech_flash.o
focaltechspi-objs  +=  focaltech_spi.o