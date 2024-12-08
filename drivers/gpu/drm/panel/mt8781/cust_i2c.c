#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>
#include <linux/syscore_ops.h>

#include "cust_i2c.h"

struct cust_i2c_data {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pull_down;
	struct pinctrl_state *pull_up;
	bool is_probe_done;
};

struct cust_i2c_data *g_cust_i2c = NULL;

static int cust_gpio_init(struct cust_i2c_data *cust_i2c)
{
	int ret = 0;
	struct device *dev = cust_i2c->dev;

	cust_i2c->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(cust_i2c->pinctrl)) {
		ret = PTR_ERR(cust_i2c->pinctrl);
		dev_info(dev, "failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	cust_i2c->pull_up =
			pinctrl_lookup_state(cust_i2c->pinctrl, "pull_up");
	if (IS_ERR(cust_i2c->pull_up)) {
			dev_info(dev, "Can *NOT* find pull_up\n");
			cust_i2c->pull_up = NULL;
	} else
			dev_info(dev, "Find pull_up\n");

	cust_i2c->pull_down =
			pinctrl_lookup_state(cust_i2c->pinctrl, "pull_down");

	if (IS_ERR(cust_i2c->pull_down)) {
			dev_info(dev, "Can *NOT* find pull_down\n");
			cust_i2c->pull_down = NULL;
	} else
			dev_info(dev, "Find pull_down\n");

	return ret;
}

static int cust_i2c_probe(struct platform_device *pdev)
{
	struct cust_i2c_data *cust_i2c;
	int ret = 0;

	cust_i2c = devm_kzalloc(&pdev->dev, sizeof(*cust_i2c), GFP_KERNEL);
	if (!cust_i2c) {
		dev_err(&pdev->dev, "alloc cust_i2c failed! \n");
		return -ENOMEM;
	}
	cust_i2c->dev = &pdev->dev;


	ret = cust_gpio_init(cust_i2c);
	if (ret) {
		dev_err(&pdev->dev, "cust_gpio_init failed! \n");
		return -ENODEV;
	}

	g_cust_i2c = cust_i2c;
	cust_i2c->is_probe_done = true;

	return 0;
}


static const struct of_device_id mt_ofid_table[] = {
	{ .compatible = "cust,cust_i2c", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt_ofid_table);

static const struct platform_device_id mt_id_table[] = {
	{ "cust_i2", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt_id_table);

static struct platform_driver cust_i2c_driver = {
	.driver = {
		.name = "cust_i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt_ofid_table),
	},
	.probe = cust_i2c_probe,
	.id_table = mt_id_table,
};


void cust_i2c_pull_up(void)
{
	dev_err(g_cust_i2c->dev, "enter\n");

	if (g_cust_i2c->is_probe_done == false) {
		dev_err(g_cust_i2c->dev, "is_probe_done == false! \n");
		return;
	}

	if (g_cust_i2c->pull_up)
		pinctrl_select_state(g_cust_i2c->pinctrl, g_cust_i2c->pull_up);

}
EXPORT_SYMBOL(cust_i2c_pull_up);

void cust_i2c_pull_down(void)
{
	dev_err(g_cust_i2c->dev, "enter\n");

	if (g_cust_i2c->is_probe_done == false) {
		dev_err(g_cust_i2c->dev, "is_probe_done == false! \n");
		return;
	}
	
	if (g_cust_i2c->pull_down) 
		pinctrl_select_state(g_cust_i2c->pinctrl, g_cust_i2c->pull_down);
}
EXPORT_SYMBOL(cust_i2c_pull_down);

void cust_i2c_init(void)
{
	platform_driver_register(&cust_i2c_driver);
}

void  cust_i2c_exit(void)
{
	platform_driver_unregister(&cust_i2c_driver);
}