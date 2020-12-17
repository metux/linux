/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ALPHA_HW_IRQ_H
#define _ALPHA_HW_IRQ_H

DECLARE_PER_CPU(unsigned long, irq_pmi_count);

#ifdef CONFIG_ALPHA_GENERIC
#define ACTUAL_NR_IRQS	alpha_mv.nr_irqs
#else
#define ACTUAL_NR_IRQS	NR_IRQS
#endif

#endif
