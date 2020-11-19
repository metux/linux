// SPDX-License-Identifier: GPL-2.0+

/*
 * GPIO driver for virtio-based virtual GPIOs
 *
 * Copyright (C) 2018 metux IT consult
 * Author: Enrico Weigelt, metux IT consult <info@metux.net>
 *
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/virtio_config.h>
#include <uapi/linux/virtio_ids.h>
#include <uapi/linux/virtio_gpio.h>

struct virtio_gpio_priv {
	struct gpio_chip		gc;
	spinlock_t			vq_lock;
	spinlock_t			op_lock;
	struct virtio_device		*vdev;
	int				num_gpios;
	char				*name;
	struct virtqueue		*vq_rx;
	struct virtqueue		*vq_tx;
	struct virtio_gpio_event	rcv_buf;
	struct virtio_gpio_event	last;
	int				irq_base;
	wait_queue_head_t		waitq;

	unsigned long reply_wait;
};

static void virtio_gpio_prepare_inbuf(struct virtio_gpio_priv *priv)
{
	struct scatterlist rcv_sg;

	sg_init_one(&rcv_sg, &priv->rcv_buf, sizeof(priv->rcv_buf));
	virtqueue_add_inbuf(priv->vq_rx, &rcv_sg, 1, &priv->rcv_buf,
			    GFP_KERNEL);
	virtqueue_kick(priv->vq_rx);
}

static int virtio_gpio_xmit(struct virtio_gpio_priv *priv, int type,
			    int pin, int value, struct virtio_gpio_event *ev)
{
	struct scatterlist sg[1];
	int ret;
	unsigned long flags;

	WARN_ON(!ev);

	ev->type = type;
	ev->pin = pin;
	ev->value = value;

	sg_init_table(sg, 1);
	sg_set_buf(&sg[0], ev, sizeof(struct virtio_gpio_event));

	spin_lock_irqsave(&priv->vq_lock, flags);
	ret = virtqueue_add_outbuf(priv->vq_tx, sg, ARRAY_SIZE(sg),
				   priv, GFP_KERNEL);
	if (ret < 0) {
		dev_err(&priv->vdev->dev,
			"virtqueue_add_outbuf() failed: %d\n", ret);
		goto out;
	}
	virtqueue_kick(priv->vq_tx);

out:
	spin_unlock_irqrestore(&priv->vq_lock, flags);
	return 0;
}

static inline void wakeup_event(struct virtio_gpio_priv *priv, int id)
{
	set_bit(id, &priv->reply_wait);
}

static inline int check_event(struct virtio_gpio_priv *priv, int id)
{
	return test_bit(id, &priv->reply_wait);
}

static inline void clear_event(struct virtio_gpio_priv *priv, int id)
{
	clear_bit(id, &priv->reply_wait);
}

static int virtio_gpio_req(struct virtio_gpio_priv *priv, int type,
			   int pin, int value)
{
	struct virtio_gpio_event *ev
		= devm_kzalloc(&priv->vdev->dev,
			       sizeof(struct virtio_gpio_event), GFP_KERNEL);

	if (!ev)
		return -ENOMEM;

	clear_event(priv, type);
	virtio_gpio_xmit(priv, type, pin, value, ev);
	wait_event_interruptible(priv->waitq, check_event(priv, type));

	devm_kfree(&priv->vdev->dev, ev);

	return priv->last.value;
}

static int virtio_gpio_direction_input(struct gpio_chip *gc,
				       unsigned int pin)
{
	return virtio_gpio_req(gpiochip_get_data(gc),
			       VIRTIO_GPIO_EV_GUEST_DIRECTION_INPUT,
			       pin, 0);
}

static int virtio_gpio_direction_output(struct gpio_chip *gc,
					unsigned int pin, int value)
{
	return virtio_gpio_req(gpiochip_get_data(gc),
			       VIRTIO_GPIO_EV_GUEST_DIRECTION_OUTPUT,
			       pin, value);
}

static int virtio_gpio_get_direction(struct gpio_chip *gc, unsigned int pin)
{
	return virtio_gpio_req(gpiochip_get_data(gc),
			       VIRTIO_GPIO_EV_GUEST_GET_DIRECTION,
			       pin, 0);
}

static void virtio_gpio_set(struct gpio_chip *gc,
			    unsigned int pin, int value)
{
	virtio_gpio_req(gpiochip_get_data(gc),
			VIRTIO_GPIO_EV_GUEST_SET_VALUE, pin, value);
}

static int virtio_gpio_get(struct gpio_chip *gc,
			   unsigned int pin)
{
	return virtio_gpio_req(gpiochip_get_data(gc),
			       VIRTIO_GPIO_EV_GUEST_GET_VALUE, pin, 0);
}

static int virtio_gpio_request(struct gpio_chip *gc,
			       unsigned int pin)
{
	return virtio_gpio_req(gpiochip_get_data(gc),
			       VIRTIO_GPIO_EV_GUEST_REQUEST, pin, 0);
}

static void virtio_gpio_signal(struct virtio_gpio_priv *priv, int event,
			      int pin, int value)
{
	if (pin < priv->num_gpios)
		generic_handle_irq(priv->irq_base + pin);
}

static void virtio_gpio_data_rx(struct virtqueue *vq)
{
	struct virtio_gpio_priv *priv = vq->vdev->priv;
	void *data;
	unsigned int len;
	struct virtio_gpio_event *ev;

	data = virtqueue_get_buf(priv->vq_rx, &len);
	if (!data || !len) {
		dev_warn(&vq->vdev->dev, "RX received no data ! %d\n", len);
		return;
	}

	ev = data;
	WARN_ON(data != &priv->rcv_buf);

	memcpy(&priv->last, &priv->rcv_buf, sizeof(struct virtio_gpio_event));

	switch (ev->type) {
	case VIRTIO_GPIO_EV_HOST_LEVEL:
		virtio_gpio_signal(priv, ev->type, ev->pin, ev->value);
	break;
	default:
		wakeup_event(priv, ev->type & ~VIRTIO_GPIO_EV_REPLY);
	break;
	}
	virtio_gpio_prepare_inbuf(priv);
	wake_up_all(&priv->waitq);
}

static int virtio_gpio_alloc_vq(struct virtio_gpio_priv *priv)
{
	struct virtqueue *vqs[2];
	vq_callback_t *cbs[] = {
		NULL,
		virtio_gpio_data_rx,
	};
	static const char * const names[] = { "in", "out", };
	int ret;

	ret = virtio_find_vqs(priv->vdev, 2, vqs, cbs, names, NULL);
	if (ret) {
		dev_err(&priv->vdev->dev, "failed to alloc vqs: %d\n", ret);
		return ret;
	}

	priv->vq_rx = vqs[0];
	priv->vq_tx = vqs[1];

	virtio_gpio_prepare_inbuf(priv);

	virtio_config_enable(priv->vdev);
	virtqueue_enable_cb(priv->vq_rx);
	virtio_device_ready(priv->vdev);

	return 0;
}

static int virtio_gpio_probe(struct virtio_device *vdev)
{
	struct virtio_gpio_priv *priv;
	struct virtio_gpio_config cf = {};
	char *name_buffer;
	const char **gpio_names = NULL;
	struct device *dev = &vdev->dev;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->name = devm_kzalloc(dev, sizeof(cf.name)+1, GFP_KERNEL);

	spin_lock_init(&priv->vq_lock);
	spin_lock_init(&priv->op_lock);

	virtio_cread(vdev, struct virtio_gpio_config, version, &cf.version);
	virtio_cread(vdev, struct virtio_gpio_config, num_gpios,
		     &cf.num_gpios);
	virtio_cread(vdev, struct virtio_gpio_config, names_size,
		     &cf.names_size);
	virtio_cread_bytes(vdev, offsetof(struct virtio_gpio_config, name),
			   priv->name, sizeof(cf.name));

	if (cf.version != 1) {
		dev_err(dev, "unsupported interface version %d\n", cf.version);
		return -EINVAL;
	}

	priv->num_gpios = cf.num_gpios;

	if (cf.names_size) {
		char *bufwalk;
		int idx = 0;

		name_buffer = devm_kzalloc(&vdev->dev, cf.names_size,
					   GFP_KERNEL)+1;
		virtio_cread_bytes(vdev, sizeof(struct virtio_gpio_config),
				   name_buffer, cf.names_size);
		name_buffer[cf.names_size] = 0;

		gpio_names = devm_kzalloc(dev,
					  sizeof(char *) * priv->num_gpios,
					  GFP_KERNEL);
		bufwalk = name_buffer;

		while (idx < priv->num_gpios &&
		       bufwalk < (name_buffer+cf.names_size)) {
			gpio_names[idx] = (strlen(bufwalk) ? bufwalk : NULL);
			bufwalk += strlen(bufwalk)+1;
			idx++;
		}
	}

	priv->gc.owner			= THIS_MODULE;
	priv->gc.parent			= dev;
	priv->gc.label			= (priv->name[0] ? priv->name
							 : dev_name(dev));
	priv->gc.ngpio			= priv->num_gpios;
	priv->gc.names			= gpio_names;
	priv->gc.base			= -1;
	priv->gc.request		= virtio_gpio_request;
	priv->gc.direction_input	= virtio_gpio_direction_input;
	priv->gc.direction_output	= virtio_gpio_direction_output;
	priv->gc.get_direction		= virtio_gpio_get_direction;
	priv->gc.get			= virtio_gpio_get;
	priv->gc.set			= virtio_gpio_set;

	priv->vdev			= vdev;
	vdev->priv = priv;

	priv->irq_base = devm_irq_alloc_descs(dev, -1, 0, priv->num_gpios,
					      NUMA_NO_NODE);
	if (priv->irq_base < 0) {
		dev_err(&vdev->dev, "failed to alloc irqs\n");
		priv->irq_base = -1;
		return -ENOMEM;
	}

	init_waitqueue_head(&priv->waitq);

	priv->reply_wait = 0;

	virtio_gpio_alloc_vq(priv);

	return devm_gpiochip_add_data(dev, &priv->gc, priv);
}

static void virtio_gpio_remove(struct virtio_device *vdev)
{
}

static const struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_GPIO, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_gpio_driver = {
	.driver.name	= KBUILD_MODNAME,
	.driver.owner	= THIS_MODULE,
	.id_table	= id_table,
	.probe		= virtio_gpio_probe,
	.remove		= virtio_gpio_remove,
};

module_virtio_driver(virtio_gpio_driver);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("VirtIO GPIO driver");
MODULE_LICENSE("GPL");
