/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_IRQ_ERR_H
#define __ASM_GENERIC_IRQ_ERR_H

extern atomic_t irq_err_counter;

static inline void irq_err_inc(void)
{
	atomic_inc(&irq_err_counter);
}

static inline int irq_err_get(void)
{
	return atomic_read(&irq_err_counter);
}

#endif /* __ASM_GENERIC_IRQ_ERR_H */
