/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_UM_HARDIRQ_H
#define __ASM_UM_HARDIRQ_H

#include <linux/cache.h>
#include <linux/threads.h>

typedef struct {
	unsigned int __softirq_pending;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */
#include <linux/irq.h>

#define ack_bad_irq(irq)

#define __ARCH_IRQ_EXIT_IRQS_DISABLED 1

#endif /* __ASM_UM_HARDIRQ_H */
