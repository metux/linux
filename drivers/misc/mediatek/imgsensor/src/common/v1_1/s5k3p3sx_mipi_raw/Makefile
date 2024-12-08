#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ifeq ($(CONFIG_S5K3P3SM_MIPI_RAW_FULL_SIZE),y)
imgsensor_isp6s-objs += $(subdrv-rpath)/s5k3p3smmipiraw_Sensor_full_size.o
else
imgsensor_isp6s-objs += $(subdrv-rpath)/s5k3p3sxmipiraw_Sensor.o
endif
imgsensor_isp6s-objs += $(subdrv-rpath)/s5k3P3_pdafotp.o

