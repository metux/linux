# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.
ifeq (y, $(filter y, $(CONFIG_OV8856F_2LAN)))
imgsensor_isp6s-objs += \
	$(subdrv-rpath)/ov8856fmipiraw_Sensor_2lan.o
else
imgsensor_isp6s-objs += \
	$(subdrv-rpath)/ov8856fmipiraw_Sensor.o
endif
