/* SPDX-License-Identifier: GPL-2.0 */
/*
 * portmux/driver.h - definitions for the portmux driver interface
 *
 * Copyright (C) 2020 metux IT consult
 *
 * Author: Enrico Weigelt <info@metux.net>
 */

#ifndef _LINUX_PORTMUX_DRIVER_H
#define _LINUX_PORTMUX_DRIVER_H

#include <linux/device.h>
#include <linux/spinlock.h>

struct portmux_classdev;

/**
 * struct portmux_choice -	Choice element for a portmux chip.
 * @name:			(unique) name for the choice
 * @description:		human-readable description text
 * @data:			private data (driver specific)
 */
struct portmux_choice {
	const char *name;
	const char *description;
	void *data;
};

/**
 * struct portmux_classdev -	Represents a chip holding portmux controllers.
 * @dev:			Device structure.
 * @parent:			Parent device structure.
 * @priv:			Private (driver specific) data.
 * @spinlock:			Spin-Lock for the device.
 * @choices:			Supported choices.
 * @num_choices:		Size of the choices array.
 * @name:			Used to identify the device internally.
 * @set:			Driver callback for setting the current choice.
 * @get:			Driver callback for getting the current choice.
 * @release:			Driver callback for device release.
 */
struct portmux_classdev {
	struct device *dev;
	struct device *parent;
	void *priv;
	spinlock_t spinlock;
	const struct portmux_choice *choices;
	int num_choices;
	char *name;
	char *type;
	struct module *owner;

	int (*set)(struct portmux_classdev *portmux, int idx);
	int (*get)(struct portmux_classdev *portmux);
	int (*release)(struct portmux_classdev *portmux);
};

/*
 * Register a portmux class device.
 * @parent:			parent device
 * @portmux:			(pre-filled) portmux classdev structure
 * @return:			zero or (negative) error code
 */
int portmux_classdev_register(struct device *parent,
			      struct portmux_classdev *portmux)
	__attribute__((nonnull(1,2)));

/*
 * Unregister an already registered portmux classdev
 * @portmux:			portmux classdev structure
 */
void portmux_classdev_unregister(struct portmux_classdev *portmux)
	__attribute__((nonnull(1)));

/*
 * device-managed variant of portmux_classdev_register()
 * @parent:			parent device
 * @portmux:			(pre-filled) portmux classdev structure
 * @return:			zero or (negative) error code
 */
int devm_portmux_classdev_register(struct device *parent,
				   struct portmux_classdev *portmux)
	__attribute__((nonnull(1,2)));

/*
 * Set current choice of a portmux classdev
 * @portmux:			portmux classdev structure
 * @idx:			new choice id
 * @return:			zero or (negative) error code
 */
int portmux_classdev_set(struct portmux_classdev *portmux, int idx)
	__attribute__((nonnull(1)));

/*
 * Get current choice of a portmux classdev
 * @portmux:			portmux classdev structure
 * @return:			current choice index or (negative) error code
 */
int portmux_classdev_get(struct portmux_classdev *portmux)
	__attribute__((nonnull(1)));

#endif /* _LINUX_PORTMUX_DRIVER_H */
