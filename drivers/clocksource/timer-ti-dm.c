/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * OMAP dual-mode timer driver
 *
 * Copyright (C) 2005-2006 Nokia Corporation
 * Author: Tuukka Tikkanen <tuukka.tikkanen@nokia.com>
 *
 * 2008.06.24 - Add support for clock event framework
 *
 * 2009.12.02 - Add support for 32k sync timer
 *
 * 2010.02.26 - Add support for clocksource
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>

#include "timer-ti.h"
#include "timer-of.h"

#define OMAP_DM_TIMER_REVISION		0x00
#define OMAP_DM_TIMER_ID			0x04
#define OMAP_DM_TIMER_TIOCP_CFG		0x0C
#define OMAP_TM_TIOCP_CFG_SOFTRESET	(1 << 1)
#define OMAP_DM_TIMER_TISTAT		0x14
#define OMAP_DM_TIMER_TISTAT_MAT_IT_FLAG	BIT(0)
#define OMAP_DM_TIMER_TISTAT_OVF_IT_FLAG	BIT(1)
#define OMAP_DM_TIMER_TIER		0x18
#define OMAP_DM_TIMER_TIER_MAT_IE_FLAG	BIT(0)
#define OMAP_DM_TIMER_TIER_OVF_IE_FLAG	BIT(1)
#define OMAP_DM_TIMER_TWER		0x1C
#define OMAP_DM_TIMER_TWER_MAT_WUP_ENA	BIT(0)
#define OMAP_DM_TIMER_TWER_OVF_WUP_ENA	BIT(1)
#define OMAP_DM_TIMER_TCLR		0x24
#define OMAP_DM_TIMER_TCLR_ST		BIT(0)
#define OMAP_DM_TIMER_TCLR_AR		BIT(1)
#define OMAP_DM_TIMER_TCLR_PRE		BIT(5)
#define OMAP_DM_TIMER_TCRR		0x28
#define OMAP_DM_TIMER_TLDR		0x2C
#define OMAP_DM_TIMER_TTGR		0x30
#define OMAP_DM_TIMER_TWPS		0x34
#define OMAP_DM_TIMER_TWPS_W_PEND	BIT(0)
#define OMAP_DM_TIMER_TMAR		0x38
#define OMAP_DM_TIMER_TCAR1		0x3C
#define OMAP_DM_TIMER_TCAR2		0x40
#define OMAP_DM_TIMER_TPIR		0x44
#define OMAP_DM_TIMER_TNIR		0x48
#define OMAP_DM_TIMER_TCVR		0x4C
#define OMAP_DM_TIMER_TOCR		0x50
#define OMAP_DM_TIMER_TOWR		0x54

#define OMAP_DM_TIMER_TRIGGER_REG	0x58

#define OMAP_TIMER_BASE		0x0000
#define OMAP_32K_TIMER_BASE		0x2000
#define OMAP_TIMER_SIZE		0x1000
#define OMAP_TIMER_IRQ		36

struct omap_dm_timer {
	void __iomem *base;
	struct clk *fclk;
	struct clk *iclk;
	struct clocksource clocksource;
	struct clock_event_device clkevt;
	void __iomem *base_32k;
	struct clocksource clocksource_32k;
	int irq;
	struct omap_dm_timer *next;
	unsigned long rate;
	int posted;
	struct {
		u32 tclr;
		u32 tldr;
	} context;
};

static struct omap_dm_timer *omap_dm_timers;
static int omap_dm_timer_count;

static int omap_dm_timer_count_get(void)
{
	return omap_dm_timer_count;
}

static void omap_dm_timer_set_load(struct omap_dm_timer *timer,
				   unsigned long load)
{
	unsigned long l = omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TCLR);

	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_LOAD_REG, load);
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TCLR, l);

	/* Save the context */
	timer->context.tclr = l;
	timer->context.tldr = load;

	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TRIGGER_REG, 0);

	omap_dm_timer_disable(timer);
}

static void omap_dm_timer_set_match(struct omap_dm_timer *timer,
				    unsigned long match)
{
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_MATCH_REG, match);
}

static void omap_dm_timer_write_reg(struct omap_dm_timer *timer, int reg,
				    u32 value)
{
	__raw_writel(value, timer->base + reg);
}

static u32 omap_dm_timer_read_reg(struct omap_dm_timer *timer, int reg)
{
	return __raw_readl(timer->base + reg);
}

static inline void omap_dm_timer_enable(struct omap_dm_timer *timer)
{
	u32 l = omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TCLR);
	l |= OMAP_DM_TIMER_TCLR_ST;
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TCLR, l);
}

static inline void omap_dm_timer_disable(struct omap_dm_timer *timer)
{
	u32 l = omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TCLR);
	l &= ~OMAP_DM_TIMER_TCLR_ST;
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TCLR, l);
}

static int omap_dm_timer_set_next_event(unsigned long delta,
					struct clock_event_device *evt)
{
	struct omap_dm_timer *timer = to_omap_dm_timer(evt);
	unsigned long load = omap_dm_timer_counter(timer) + delta;

	omap_dm_timer_set_match(timer, load);
	return 0;
}

static int omap_dm_timer_shutdown(struct clock_event_device *evt)
{
	omap_dm_timer_disable(to_omap_dm_timer(evt));
	return 0;
}

static int omap_dm_timer_set_periodic(struct clock_event_device *evt)
{
	struct omap_dm_timer *timer = to_omap_dm_timer(evt);
	unsigned long load = timer->rate / HZ;

	omap_dm_timer_set_load(timer, load);
	omap_dm_timer_enable(timer);
	return 0;
}

static int omap_dm_timer_set_oneshot(struct clock_event_device *evt)
{
	omap_dm_timer_disable(to_omap_dm_timer(evt));
	return 0;
}

static u32 omap_dm_timer_counter(struct omap_dm_timer *timer)
{
	return omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TCRR);
}

static int omap_dm_timer_set_state_shutdown(struct clock_event_device *evt)
{
	omap_dm_timer_shutdown(evt);
	return 0;
}

static int omap_dm_timer_set_state_oneshot(struct clock_event_device *evt)
{
	omap_dm_timer_set_oneshot(evt);
	return 0;
}

static int omap_dm_timer_set_state_periodic(struct clock_event_device *evt)
{
	omap_dm_timer_set_periodic(evt);
	return 0;
}

static irqreturn_t omap_dm_timer_interrupt(int irq, void *dev_id)
{
	struct omap_dm_timer *timer = dev_id;

	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TISTAT,
				OMAP_DM_TIMER_TISTAT_MAT_IT_FLAG |
				OMAP_DM_TIMER_TISTAT_OVF_IT_FLAG);

	if (timer->clkevt.event_handler)
		timer->clkevt.event_handler(&timer->clkevt);

	return IRQ_HANDLED;
}

static u32 omap_dm_timer_get_pending_irq(struct omap_dm_timer *timer)
{
	u32 tistat = omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TISTAT);

	return (tistat & (OMAP_DM_TIMER_TISTAT_MAT_IT_FLAG |
			  OMAP_DM_TIMER_TISTAT_OVF_IT_FLAG));
}

static int omap_dm_timer_restart(struct omap_dm_timer *timer)
{
	unsigned long flags;
	u32 tclr = timer->context.tclr;
	u32 tldr = timer->context.tldr;

	local_irq_save(flags);
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TLDR, tldr);
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TCLR, tclr);
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TRIGGER_REG, 0);
	local_irq_restore(flags);
	return 0;
}

static int omap_dm_timer_init(struct omap_dm_timer *timer, int posted)
{
	int err;

	timer->context.tclr = 0;
	timer->context.tldr = 0;

	err = omap_dm_timer_init_reg(timer);
	if (err)
		return err;

	if (!posted)
		omap_dm_timer_disable(timer);

	return 0;
}

static int omap_dm_timer_remove(struct platform_device *pdev)
{
	struct omap_dm_timer *timer = platform_get_drvdata(pdev);

	if (!timer)
		return 0;

	if (timer->irq)
		free_irq(timer->irq, timer);

	clk_disable_unprepare(timer->fclk);
	clk_disable_unprepare(timer->iclk);

	list_del(&timer->next);
	omap_dm_timer_count--;

	iounmap(timer->base);
	kfree(timer);

	return 0;
}

static int omap_dm_timer_probe(struct platform_device *pdev)
{
	struct omap_dm_timer *timer;
	struct resource *res;
	int err;

	timer = devm_kzalloc(&pdev->dev, sizeof(*timer), GFP_KERNEL);
	if (!timer)
		return -ENOMEM;

	platform_set_drvdata(pdev, timer);

	timer->irq = platform_get_irq(pdev, 0);
	if (timer->irq < 0)
		return timer->irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	timer->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(timer->base))
		return PTR_ERR(timer->base);

	timer->fclk = devm_clk_get(&pdev->dev, "fck");
	if (IS_ERR(timer->fclk))
		return PTR_ERR(timer->fclk);

	timer->iclk = devm_clk_get(&pdev->dev, "ick");
	if (IS_ERR(timer->iclk))
		return PTR_ERR(timer->iclk);

	err = clk_prepare_enable(timer->fclk);
	if (err)
		return err;

	err = clk_prepare_enable(timer->iclk);
	if (err)
		return err;

	err = omap_dm_timer_init(timer, 0);
	if (err)
		return err;

	timer->irq = platform_get_irq(pdev, 0);
	if (timer->irq < 0)
		return timer->irq;

	err = devm_request_irq(&pdev->dev, timer->irq, omap_dm_timer_interrupt,
			       IRQF_TIMER | IRQF_IRQPOLL, dev_name(&pdev->dev), timer);
	if (err)
		return err;

	timer->next = omap_dm_timers;
	omap_dm_timers = timer;
	omap_dm_timer_count++;

	return 0;
}

static int omap_dm_timer_suspend(struct device *dev)
{
	struct omap_dm_timer *timer = dev_get_drvdata(dev);

	if (!timer)
		return 0;

	/* Save context */
	timer->context.tclr = omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TCLR);
	timer->context.tldr = omap_dm_timer_read_reg(timer, OMAP_DM_TIMER_TLDR);

	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TRIGGER_REG, 0);
	omap_dm_timer_disable(timer);

	return 0;
}

static int omap_dm_timer_resume(struct device *dev)
{
	struct omap_dm_timer *timer = dev_get_drvdata(dev);

	if (!timer)
		return 0;

	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TLDR, timer->context.tldr);
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TCLR, timer->context.tclr);
	omap_dm_timer_write_reg(timer, OMAP_DM_TIMER_TRIGGER_REG, 0);

	return 0;
}

static int omap_dm_timer_freeze(struct device *dev)
{
	return omap_dm_timer_suspend(dev);
}

static int omap_dm_timer_thaw(struct device *dev)
{
	return omap_dm_timer_resume(dev);
}

static int omap_dm_timer_poweroff(struct device *dev)
{
	return omap_dm_timer_suspend(dev);
}

static int omap_dm_timer_restore(struct device *dev)
{
	return omap_dm_timer_resume(dev);
}

static const struct dev_pm_ops omap_dm_timer_pm_ops = {
	.suspend = omap_dm_timer_suspend,
	.resume = omap_dm_timer_resume,
	.freeze = omap_dm_timer_freeze,
	.thaw = omap_dm_timer_thaw,
	.poweroff = omap_dm_timer_poweroff,
	.restore = omap_dm_timer_restore,
};

static const struct of_device_id omap_dm_timer_of_match[] = {
	{ .compatible = "ti,omap-dm-timer" },
	{ .compatible = "ti,omap-32k-timer" },
	{},
};
MODULE_DEVICE_TABLE(of, omap_dm_timer_of_match);

static struct platform_driver omap_dm_timer_driver = {
	.driver = {
		.name = "omap-dm-timer",
		.of_match_table = omap_dm_timer_of_match,
		.pm = &omap_dm_timer_pm_ops,
	},
	.probe = omap_dm_timer_probe,
	.remove = omap_dm_timer_remove,
};

module_platform_driver(omap_dm_timer_driver);

MODULE_AUTHOR("Tuukka Tikkanen <tuukka.tikkanen@nokia.com>");
MODULE_DESCRIPTION("OMAP dual mode timer driver");
MODULE_LICENSE("GPL");
