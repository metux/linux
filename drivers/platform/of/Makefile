# SPDX-License-Identifier: GPL-2.0

ofboard-y := init.o drv.o root.o

ofboard-$(CONFIG_PLATFORM_OF_DRV_PCENGINES_APU2) += apu2x.dtb.o
ofboard-y += testing.dtb.o

obj-$(CONFIG_PLATFORM_OF_DRV) += ofboard.o

# FIXME: try to find a more generic way
# DTC_FLAGS_testing += -@

# DTC_FLAGS_testing += -A
