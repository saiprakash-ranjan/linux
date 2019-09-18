/* 2016-11-10: File added by Sony Corporation */
/*
 * arch/arm64/include/asm/unwind.h
 *
 * Copyright (C) 2016 Sony Corporation
 *
 * Based on arch/arm64/include/asm/unwind.h
 *
 * Copyright (C) 2008 ARM Limited
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __ASM_UNWIND_H
#define __ASM_UNWIND_H

#ifndef __ASSEMBLY__

/* Unwind reason code according the the ARM EABI documents */
enum unwind_reason_code {
	URC_OK = 0,			/* operation completed successfully */
	URC_CONTINUE_UNWIND = 8,
	URC_FAILURE = 9			/* unspecified failure of some kind */
};

struct unwind_idx {
	unsigned long addr_offset;
	unsigned long insn;
};

struct unwind_table {
	struct list_head list;
	const struct unwind_idx *start;
	const struct unwind_idx *origin;
	const struct unwind_idx *stop;
	unsigned long begin_addr;
	unsigned long end_addr;
};

struct unwind_ctrl_block {
	u64 vrs[31];		/* virtual register set */
	u64 sp;
	u64 pc;
	u64 pstate;

	const unsigned long *insn;	/* pointer to the current instructions word */
	unsigned long sp_high;          /* highest value of sp allowed */
	/*
	 * 1 : check for stack overflow for each register pop.
	 * 0 : save overhead if there is plenty of stack remaining.
	 */
	int check_each_pop;
	int entries;			/* number of entries left to interpret */
	int byte;			/* current byte number in the instructions word */
};

extern struct unwind_table *unwind_table_add(unsigned long start,
					     unsigned long size,
					     unsigned long text_addr,
					     unsigned long text_size);

extern void unwind_table_del(struct unwind_table *tab);
extern void unwind_backtrace(struct pt_regs *regs, struct task_struct *tsk);

#endif	/* !__ASSEMBLY__ */

#ifdef CONFIG_ARM_UNWIND
#define UNWIND(code...)		code
#else
#define UNWIND(code...)
#endif

#endif	/* __ASM_UNWIND_H */
