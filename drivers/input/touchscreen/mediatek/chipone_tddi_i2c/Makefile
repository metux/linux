ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone-tddi_i2c.o

chipone-tddi_i2c-objs := cts_driver.o
chipone-tddi_i2c-objs += cts_core.o
chipone-tddi_i2c-objs += cts_sfctrlv2.o
chipone-tddi_i2c-objs += cts_spi_flash.o
chipone-tddi_i2c-objs += cts_firmware.o
chipone-tddi_i2c-objs += cts_test.o
chipone-tddi_i2c-objs += cts_charger_detect.o
chipone-tddi_i2c-objs += cts_earjack_detect.o
chipone-tddi_i2c-objs += cts_tcs.o
chipone-tddi_i2c-objs += cts_platform.o
chipone-tddi_i2c-objs += cts_tool.o
chipone-tddi_i2c-objs += cts_sysfs.o
chipone-tddi_i2c-objs += cts_strerror.o
chipone-tddi_i2c-objs += cts_oem.o

