# Linux driver folder
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
#ccflags-y += -I$(srctree)/drivers/input/touchscreen/
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_DEBUG=y
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_SORTING=y


#obj-y	+= jadard_module.o
#obj-y	+= jadard_ic_JD9366T.o
#obj-y	+= jadard_common.o jadard_platform.o
#obj-y	+= jadard_debug.o
#obj-y	+= jadard_sorting.o
###***********************************################

##################For build .KO **** #####################
# Makefile for the Jadard SPI touchscreen drivers.
ifeq ($(CONFIG_TOUCHSCREEN_JADARD_SPI_MODULE), y)
	obj-y += jadard_module.o
	obj-y += jadard_ic_JD9366T.o
	obj-y += jadard_common.o jadard_platform.o
	obj-y += jadard_debug.o
	obj-y += jadard_sorting.o

else ifeq ($(CONFIG_TOUCHSCREEN_JADARD_SPI_MODULE), m)
	obj-$(CONFIG_TOUCHSCREEN_JADARD_SPI_MODULE)	+=  jadard_ts.o
	ccflags-y += -D__JADARD_KMODULE__

	jadard_ts-objs += jadard_module.o
	jadard_ts-objs += jadard_ic_JD9366T.o
	jadard_ts-objs += jadard_common.o jadard_platform.o
	jadard_ts-objs += jadard_debug.o
	jadard_ts-objs += jadard_sorting.o

endif
