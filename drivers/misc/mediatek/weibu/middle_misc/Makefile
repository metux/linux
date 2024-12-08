ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_CM_MIDMISC_SUPPORT) += middle_misc.o
middle_misc-y += md_misc.o
middle_misc-$(CONFIG_CM_NOTIFY_EVENT_SUPPORT) += notify_event.o
obj-$(CONFIG_CM_MIDMISC_V_SUPPORT) += middle_misc_v.o
