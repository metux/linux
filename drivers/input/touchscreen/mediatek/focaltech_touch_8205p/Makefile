# Makefile for the focaltech touchscreen drivers.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat


obj-$(CONFIG_TOUCHSCREEN_FTS_SPI_8205P) += focaltech_tp_8205p.o
focaltech_tp_8205p-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_proximity.o
focaltech_tp_8205p-y += focaltech_flash_spi.o
focaltech_tp_8205p-y += focaltech_spi_v2.o
focaltech_tp_8205p-y += focaltech_debug.o
focaltech_tp_8205p-y += focaltech_hp.o
