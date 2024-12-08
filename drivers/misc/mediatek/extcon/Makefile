# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

ccflags-y += -I$(srctree)/drivers/power/supply/


obj-$(CONFIG_EXTCON_MTK_USB) += extcon-mtk-usb.o
ifeq (y, $(filter y, $(CONFIG_ADAPT_CHARGER_V1)))
ccflags-y += -DADAPT_CHARGER_V1
endif
