# Copyright (C) Microarray
# MicroArray Fprint Driver Code
# MAKEFILE
# Date: 2017-3-15
# Version: v4.0.06 
# Author: guq
# Contact: guq@microarray.com.cn


subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include \
					-I$(srctree)/drivers/misc/mediatek/include/mt-plat

subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/

#compatible for trustkernel
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/tkcore/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/tkcore/core
subdir-ccflags-y += -I$(srctree)/drivers/tee/tkcore/include/linux/
subdir-ccflags-y += -I$(srctree)/drivers/tee/tkcore/include/

#compatible for microtrust
ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/V1.0/tz_vfs

obj-$(CONFIG_MICROARRAY_FINGERPRINT) +=  microarray_fp.o
microarray_fp-y	+= madev.o mtk-settings.o


