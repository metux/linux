#for AWINIC AW87XXX Smart K PA
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_SND_SOC_AW87XXX) += aw87xxx_sound.o 
aw87xxx_sound-objs := aw87xxx.o aw87xxx_device.o aw87xxx_monitor.o aw87xxx_bin_parse.o aw87xxx_dsp.o aw87xxx_acf_bin.o
