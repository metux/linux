subdir-ccflags-y += -Werror
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# In case the platform does NOT support this type of sensors
obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI)	+=  NT36XXX_SPI/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_SPI)       +=  hxchipset_spi/
obj-$(CONFIG_TOUCHSCREEN_MTK_GSLX680)       +=  gslX680/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_83122)       +=  hxchipset_83122/
obj-$(CONFIG_TOUCHSCREEN_FTS_SPI)       +=  focaltech_touch_spi/
obj-$(CONFIG_TOUCHSCREEN_FTS_SPI_8205P)       +=  focaltech_touch_8205p/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone_tddi/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE_I2C) += chipone_tddi_i2c/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_I2C)       +=  hxchipset_i2c/
obj-$(CONFIG_TOUCHSCREEN_FTS_I2C_V42)       +=  focaltech_touch_i2c_v42/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_1186)       +=  hxchipset_83102_iic_1186/

obj-$(CONFIG_TOUCHSCREEN_ILITEK_V3) += ILITEK_V3/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX)	+= GT9XX/
obj-$(CONFIG_TOUCHSCREEN_HUION_PANELS) += huion_i2c/
obj-$(CONFIG_TOUCHSCREEN_JADARD_SPI_MODULE) += jdchipset_spi/
obj-$(CONFIG_TOUCHSCREEN_FTS_I2C_5726) += focaltech_touch_5726/
obj-$(CONFIG_CHSC5XXX_TOUCHPANEL_DRIVER)       +=  chsc5xxx/

TOUCH_LISTS := $(subst ",,$(CONFIG_TOUCHSCREEN_WB_TOUCH))
obj-y += $(foreach TOUCH,$(TOUCH_LISTS),$(TOUCH)/)
