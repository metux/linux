# Makefile for the focaltech touchscreen drivers.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_TOUCHSCREEN_FTS_I2C_V42) += focaltech_touch_i2c_v42.o
focaltech_touch_i2c_v42-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_proximity.o

focaltech_touch_i2c_v42-y += focaltech_i2c.o


focaltech_touch_i2c_v42-y += focaltech_flash.o
focaltech_touch_i2c_v42-y += focaltech_flash/focaltech_upgrade_ft8205.o
focaltech_touch_i2c_v42-y += focaltech_flash/focaltech_upgrade_ft8201.o
focaltech_touch_i2c_v42-y += focaltech_flash/focaltech_upgrade_ft8201ab.o
focaltech_touch_i2c_v42-y += focaltech_flash/focaltech_upgrade_ft8206.o
focaltech_touch_i2c_v42-y += focaltech_flash/focaltech_upgrade_ft8201p.o