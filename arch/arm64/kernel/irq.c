/* 2017-07-12: File changed by Sony Corporation */
/*
 * Based on arch/arm/kernel/irq.c
 *
 * Copyright (C) 1992 Linus Torvalds
 * Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 * Support for Dynamic Tick Timer Copyright (C) 2004-2005 Nokia Corporation.
 * Dynamic Tick Timer written by Tony Lindgren <tony@atomide.com> and
 * Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>.
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/rt_trace_lite.h>
#include <linux/rt_trace_lite_irq.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/irqchip.h>
#include <linux/seq_file.h>

unsigned long irq_err_count;

int arch_show_interrupts(struct seq_file *p, int prec)
{
	show_ipi_list(p, prec);
	seq_printf(p, "%*s: %10lu\n", prec, "Err", irq_err_count);
	return 0;
}

void (*handle_arch_irq)(struct pt_regs *) = NULL;

void __init set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
	if (handle_arch_irq)
		return;

	handle_arch_irq = handle_irq;
}

#ifdef CONFIG_SNSC_DEBUG_IRQ_DURATION
int show_irq_stat(struct seq_file *p, void *v)
{
	int i = *(loff_t *) v, cpu;
	struct irqaction * action;
	unsigned long flags;
	struct irq_desc *desc = irq_to_desc(i);

	if (i == 0) {
		char cpuname[12];

		seq_printf(p, "  ");
		for_each_present_cpu(cpu) {
			sprintf(cpuname, "CPU%d", cpu);
			seq_printf(p, " %10s               ", cpuname);
		}

		seq_putc(p, '\n');
		seq_printf(p, "     ");
		for_each_present_cpu(cpu) {
			seq_printf(p, "      count  min  avg  max");
		}
		seq_putc(p, '\n');
	}

	if (!desc)
		return 0;

	raw_spin_lock_irqsave(&desc->lock, flags);

	seq_printf(p, "%3d: ", i);
	action = desc->action;
	if (!action)
		goto unlock;

	seq_printf(p, "%3d: ", i);

	show_rt_trace_irq_stat(p, i);

	if (desc->irq_data.chip) {
		if (desc->irq_data.chip->irq_print_chip)
			desc->irq_data.chip->irq_print_chip(&desc->irq_data, p);
		else if (desc->irq_data.chip->name)
			seq_printf(p, " %8s", desc->irq_data.chip->name);
		else
			seq_printf(p, " %8s", "-");
	} else {
		seq_printf(p, " %8s", "None");
	}

	show_rt_trace_irq_stat(p, i);
	if (desc->name)
		seq_printf(p, "-%-8s", desc->name);

	if (action) {
		seq_printf(p, "  %s", action->name);
		while ((action = action->next) != NULL)
			seq_printf(p, ", %s", action->name);
	}
	seq_putc(p, '\n');
unlock:

	raw_spin_unlock_irqrestore(&desc->lock, flags);

	if (i == NR_IRQ_IPI) {
		seq_printf(p, "IPI: ");
		show_rt_trace_irq_stat(p, i);
		seq_printf(p, "  do_IPI");
		seq_putc(p, '\n');
	} else if (i == NR_IRQ_LOC) {
		seq_printf(p, "LOC: ");
		show_rt_trace_irq_stat(p, i);
		seq_printf(p, "  do_local_timer");
		seq_putc(p, '\n');

       } else if (i == NR_IRQ_INV) {

               seq_printf(p, "INV: ");
               show_rt_trace_irq_stat(p, i);
               seq_printf(p, "  invalid irq");
               seq_putc(p, '\n');

       }
       return 0;
}
#endif

void __init init_IRQ(void)
{
	irqchip_init();
	if (!handle_arch_irq)
		panic("No interrupt controller found.");
}
