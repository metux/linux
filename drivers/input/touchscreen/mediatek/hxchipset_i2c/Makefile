ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
# Makefile for the Himax touchscreen drivers.

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/


ccflags-y += -DHX_MTK_K510

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

# Generally, it is useless for this platform
# ifneq ($(filter y, $(CONFIG_DRM)),)
# 	ccflags-y += -DHX_CONFIG_DRM
# endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  ccflags-y += -D__HIMAX_MOD__
endif
# obj-y += himax_modular_table.o

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	hxchipset_i2c-objs += himax_debug.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)),)
	hxchipset_i2c-objs += himax_inspection.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INCELL)),)
	hxchipset_i2c-objs += himax_ic_incell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_ONCELL)),)
	hxchipset_i2c-objs += himax_ic_oncell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xJ)),)
  ccflags-y += -D__HIMAX_HX852xJ_MOD__
	hxchipset_i2c-objs += himax_ic_HX852xJ.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)),)
  ccflags-y += -D__HIMAX_HX83102_MOD__
	hxchipset_i2c-objs += himax_ic_HX83102.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108)),)
  ccflags-y += -D__HIMAX_HX83108_MOD__
	hxchipset_i2c-objs += himax_ic_HX83108.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)),)
  ccflags-y += -D__HIMAX_HX83112_MOD__
	hxchipset_i2c-objs += himax_ic_HX83112.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)),)
  ccflags-y += -D__HIMAX_HX83121_MOD__
	hxchipset_i2c-objs += himax_ic_HX83121.o
endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  ccflags-y += -D__HIMAX_MOD__
  hxchipset_i2c-objs += himax_common.o
  hxchipset_i2c-objs += himax_platform.o
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
    ccflags-y += -DHX_FIRMWARE_HEADER
    hxchipset_i2c-objs += himax_firmware.o
  endif
  obj-m += hxchipset_i2c.o
endif
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  hxchipset_i2c-objs += himax_common.o
  hxchipset_i2c-objs += himax_platform.o
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	ccflags-y += -DHX_FIRMWARE_HEADER
	hxchipset_i2c-objs += himax_firmware.o
  endif
  obj-y += hxchipset_i2c.o
endif

