/*
 * Driver for i.MX5/6 IPUv3 CSI's internal Test Image Generator
 *
 * Copyright (C) 2012 Philipp Zabel <p.zabel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/soc_camera.h>
#include <uapi/linux/v4l2-mediabus.h>

struct ipu_testgen_priv {
	struct v4l2_subdev subdev;
};

static struct ipu_testgen_priv *get_priv(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	return container_of(subdev, struct ipu_testgen_priv, subdev);
}

static int ipu_testgen_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int ipu_testgen_fill_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *mf)
{
	/* width and height are limited only by interface capabilities */
	mf->code	= MEDIA_BUS_FMT_FIXED;
	mf->colorspace	= V4L2_COLORSPACE_SRGB;
	mf->field	= 0;

	return 0;
}

static struct v4l2_subdev_core_ops ipu_testgen_subdev_core_ops;

static int ipu_testgen_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
				u32 *code)
{
	if (index)
		return -EINVAL;

	/* MEDIA_BUS_FMT_FIXED is code for enabling the test image generator */
	*code = MEDIA_BUS_FMT_FIXED;
	return 0;
}

static int ipu_testgen_g_crop(struct v4l2_subdev *sd,
			      struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= 512;
	a->c.height	= 768;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ipu_testgen_cropcap(struct v4l2_subdev *sd,
			       struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= 512;
	a->bounds.height		= 768;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

/*
 * Return the mediabus configuration as described in chapter 39.4.3.3
 * Test Mode of the i.MX6 reference manual (Rev. C)
 */
static int ipu_testgen_g_mbus_config(struct v4l2_subdev *sd,
				     struct v4l2_mbus_config *cfg)
{
	cfg->flags = V4L2_MBUS_SLAVE |
		     V4L2_MBUS_PCLK_SAMPLE_FALLING |
		     V4L2_MBUS_VSYNC_ACTIVE_HIGH |
		     V4L2_MBUS_HSYNC_ACTIVE_HIGH |
		     V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;

	return 0;
}

static struct v4l2_subdev_video_ops ipu_testgen_subdev_video_ops = {
	.s_stream	= ipu_testgen_s_stream,
	.enum_mbus_fmt	= ipu_testgen_enum_fmt,
	.cropcap	= ipu_testgen_cropcap,
	.g_crop		= ipu_testgen_g_crop,
	.try_mbus_fmt	= ipu_testgen_fill_fmt,
	.g_mbus_fmt	= ipu_testgen_fill_fmt,
	.s_mbus_fmt	= ipu_testgen_fill_fmt,
	.g_mbus_config	= ipu_testgen_g_mbus_config,
};

static struct v4l2_subdev_ops ipu_testgen_subdev_ops = {
	.core	= &ipu_testgen_subdev_core_ops,
	.video	= &ipu_testgen_subdev_video_ops,
};

static int ipu_testgen_probe(struct platform_device *pdev)
{
	struct soc_camera_host *ici;
	struct ipu_testgen_priv *priv;
	struct soc_camera_device *icd;
	int ret;

dev_info(&pdev->dev, "%s\n", __func__);

	/*
	 * Try retrieving the icd from the parent soc_camera.
	 * This only works as long as soc_camera creates us.
	 */
	icd = dev_get_drvdata(pdev->dev.parent);

	if (!icd) {
		dev_warn(&pdev->dev, "No soc_camera_device pointer yet!\n");
		return -EBUSY;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* soc-camera convention: control's drvdata points to the subdev */
	platform_set_drvdata(pdev, &priv->subdev);
	/* Set the control device reference */
	icd->control = &pdev->dev;

	ici = to_soc_camera_host(icd->parent);

	v4l2_subdev_init(&priv->subdev, &ipu_testgen_subdev_ops);
	v4l2_set_subdevdata(&priv->subdev, icd);
	strncpy(priv->subdev.name, dev_name(&pdev->dev), V4L2_SUBDEV_NAME_SIZE);

	ret = v4l2_device_register_subdev(&ici->v4l2_dev, &priv->subdev);
	if (ret)
		platform_set_drvdata(pdev, NULL);

	return ret;
}

static int ipu_testgen_remove(struct platform_device *pdev)
{
	struct ipu_testgen_priv *priv = get_priv(pdev);
	struct soc_camera_device *icd = v4l2_get_subdevdata(&priv->subdev);

	icd->control = NULL;
	v4l2_device_unregister_subdev(&priv->subdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver ipu_testgen_driver = {
	.driver 	= {
		.name	= "imx-ipuv3-testgen",
		.owner	= THIS_MODULE,
	},
	.probe		= ipu_testgen_probe,
	.remove		= ipu_testgen_remove,
};

module_platform_driver(ipu_testgen_driver);

MODULE_DESCRIPTION("i.MX5/6 IPUv3 Test Image Generator driver");
MODULE_AUTHOR("Philipp Zabel");
MODULE_LICENSE("GPL v2");
