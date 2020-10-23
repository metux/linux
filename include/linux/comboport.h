#ifndef __LINUX_COMBOPORT_H
#define __LINUX_COMPOPORT_H

struct comboport_chandev {
	struct list_head node;
	struct device *dev;
	const char *name;
	struct bus_type *bus_type;
	const char* bus_type_name;
	const char *bus_name;
};

struct comboport_classdev {
	struct list_head node;
	struct mutex lock;
	struct device *dev;
	const char *name;

	struct list_head chans;

	struct kobject *devlist_kobj;

	/* issue reset on the whole comboport */
	int (*reset)(struct comboport_classdev *comboport_classdev);

	/* free all custom data structures */
	int (*free)(struct comboport_classdev *comboport_classdev);
};

int comboport_classdev_register(struct device *parent,
				struct comboport_classdev *cp_cdev);

void comboport_classdev_unregister(struct comboport_classdev *cp_cdev);

int comboport_classdev_addchan(struct comboport_classdev *cp_cdev,
			       struct comboport_chandev *channel);

int comboport_classdev_rmchan(struct comboport_classdev *cp_cdev,
			      struct comboport_chandev *channel);

int comboport_classdev_addchan_dev(struct comboport_classdev *cp_cdev,
				   const char* chan_name,
				   struct device *chan_dev);

int comboport_classdev_probe_channel(struct comboport_classdev *cdev,
				     const char *name);

/* generic driver */

#define COMBOPORT_GENERIC_DRIVER	"comboport-generic"

struct comboport_generic_initdata {
	struct comboport_chandev *chan;
	int num_chan;
};

struct comboport_generic {
	struct comboport_classdev comboport;
	struct comboport_generic_initdata *initdata;
};

#endif /* __LINUX_COMBOPORT_H */
