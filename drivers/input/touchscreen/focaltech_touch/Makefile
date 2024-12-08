# Makefile for the focaltech touchscreen drivers.
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech.o
focaltech-objs	+=  focaltech_core.o
focaltech-objs	+=  focaltech_ex_fun.o
focaltech-objs	+=  focaltech_ex_mode.o
focaltech-objs	+=  focaltech_gesture.o
focaltech-objs	+=  focaltech_esdcheck.o
focaltech-objs	+=  focaltech_point_report_check.o
focaltech-objs	+=  focaltech_flash.o
focaltech-objs	+=  focaltech_i2c.o
focaltech-objs	+=  focaltech_upgrade_ft8201.o
focaltech-objs	+=  focaltech_upgrade_ft8201ab.o
focaltech-objs	+=  focaltech_upgrade_ft8205.o
#focaltech-objs	+=  focaltech_test.o
#focaltech-objs	+=  focaltech_test_ini.o
#focaltech-objs	+=  focaltech_test_ft8201.o
#focaltech-objs	+=  focaltech_test_ft8201ab.o
