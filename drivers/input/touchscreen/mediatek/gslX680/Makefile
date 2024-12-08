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
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/gslX680/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/gslX680/include/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/gslX680/mid/

obj-$(CONFIG_TOUCHSCREEN_MTK_GSLX680)		+= gslx680_tp.o

ifeq ($(CONFIG_MID_CSCI_SUPPORT), y)
KBUILD_CPPFLAGS += -DTP_COMPATIBLE
else ifeq ($(CONFIG_MID_CSCI_SUPPORT), m)
KBUILD_CPPFLAGS += -DTP_COMPATIBLE
endif


##GSLX680
ifeq ($(strip $(CONFIG_TOUCHSCREEN_MTK_GSLX680_TPGESTURE)),y)
gslx680_tp-objs   +=  gsl_point_id.o
else
gslx680_tp-objs   +=  gsl_point_id_point_active_pen_no_gesture.o
endif
gslx680_tp-objs	+=  gslx680_config.o
gslx680_tp-objs  +=  mtk_gslX680.o