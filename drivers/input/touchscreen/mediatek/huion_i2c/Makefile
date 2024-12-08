
ccflags-y += -I$(srctree)/drivers/input/touchscreen/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -DANDROID_GKI_VFS_EXPORT_ONLY=VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver

#
# Makefile test.
#
obj-$(CONFIG_TOUCHSCREEN_HUION_PANELS) += huion_i2c.o
huion_i2c-y	+=	huiontablet.o
huion_i2c-y	+= huiontablet.o
huion_i2c-y	+= huiontablet_update.o
huion_i2c-y	+= huion_tool.o
huion_i2c-y	+= dfu.o
huion_i2c-y	+= dvb_math.o