/* 2016-11-28: File changed by Sony Corporation */
/*
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
#ifndef __ASM_STACKTRACE_H
#define __ASM_STACKTRACE_H

#include <asm/ptrace.h>

struct stackframe {
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
	unsigned long lr;
};

static __always_inline
void arm64_get_current_stackframe(struct pt_regs *regs,
				struct stackframe *frame)
{
		frame->fp = frame_pointer(regs);
		frame->sp = regs->sp;
		frame->lr = regs->regs[30];
		frame->pc = regs->pc;
}

extern int unwind_frame(struct stackframe *frame);
extern void walk_stackframe(struct stackframe *frame,
			    int (*fn)(struct stackframe *, void *), void *data);
extern void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk);

#endif	/* __ASM_STACKTRACE_H */
