# Makefile for the focaltech touchscreen drivers.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_TOUCHSCREEN_FTS_SPI_V42_8208) += focaltech_touch_spi_v42_8208.o
focaltech_touch_spi_v42_8208-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_proximity.o


focaltech_touch_spi_v42_8208-y += focaltech_flash_spi.o


focaltech_touch_spi_v42_8208-y += focaltech_spi_v2.o


focaltech_touch_spi_v42_8208-y += focaltech_debug.o

focaltech_touch_spi_v42_8208-y += focaltech_hp.o


focaltech_touch_spi_v42_8208-y += focaltech_test/focaltech_test.o
focaltech_touch_spi_v42_8208-y += focaltech_test/focaltech_test_ini.o
focaltech_touch_spi_v42_8208-y += focaltech_test/supported_ic/focaltech_test_ft8208.o
