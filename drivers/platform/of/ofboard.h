#ifndef __DRIVERS_PLATFORM_OFBOARD_H
#define __DRIVERS_PLATFORM_OFBOARD_H

#include <linux/platform_device.h>

#define DRIVER_NAME	"ofboard"

extern struct platform_driver ofboard_driver;
extern void init_oftree(void);

#endif
