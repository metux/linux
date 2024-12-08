# MTK include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/base

# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

obj-$(CONFIG_SND_SOC_AW882XX) += aw882xx_sound.o 
aw882xx_sound-objs := aw882xx.o aw882xx_monitor.o aw882xx_init.o aw882xx_dsp.o aw882xx_device.o aw882xx_calib.o aw882xx_bin_parse.o aw882xx_spin.o