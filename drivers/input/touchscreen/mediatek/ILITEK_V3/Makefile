BUILD_INFAE := spi
BUILD_PLATFORM := mtk

platform=ilitek_v3_mtk
#ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6789/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ILITEK_V3/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ILITEK_V3/firmware/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

ifeq ($(BUILD_INFAE),i2c)
interface=ilitek_v3_i2c
fwupdate=ilitek_v3_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ilitek_v3_spi
fwupdate=ilitek_v3_hostdl
endif

obj-$(CONFIG_TOUCHSCREEN_ILITEK_V3) += ilitek.o
#obj-y += ilitek.o
ilitek-y += ilitek_v3.o \
		$(interface).o \
		$(platform).o \
		ilitek_v3_ic.o \
		ilitek_v3_touch.o \
		ilitek_v3_mp.o \
		$(fwupdate).o \
		ilitek_v3_node.o

ilitek-y += tpd_control.o
ilitek-y += tpd_button.o
ilitek-y += tpd_debug.o
ilitek-y += tpd_default.o
ilitek-y += tpd_misc.o
ilitek-y += tpd_setting.o
