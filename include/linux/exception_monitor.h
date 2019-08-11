/*
 * Copyright 2006  Sony Corporation
 */
#ifndef EXCEPTION_MONITOR_H
#define EXCEPTION_MONITOR_H
#ifdef CONFIG_EXCEPTION_MONITOR
#include <asm/kdebug.h>
extern int exception_check_mode;
extern int em_panic_from_die;
extern void (*exception_check)(int mode, struct pt_regs *regs);
#ifdef CONFIG_MODULES
extern struct list_head *em_modules;
#endif
extern int stop_all_threads_timeout(struct mm_struct *mm, long timeout,
				    int give_up_all_sibling);
extern void start_all_threads(struct mm_struct *mm);
extern int flush_serial_tty(int line);
extern int em_show_here(void);

static inline int  em_show_coredump(struct pt_regs *regs)
{
	int give_up_all_sibling = 0;

#ifdef CONFIG_EXCEPTION_MONITOR_GIVE_UP_WAITING_FOR_THREADS_TO_STOP
	give_up_all_sibling = 1;
#endif

	if (exception_check_mode && exception_check) {
		int ret = stop_all_threads_timeout(current->mm,
			  CONFIG_EXCEPTION_MONITOR_STOP_SIBLING_TIMEOUT,
			  give_up_all_sibling);
		if (ret == -ETIMEDOUT) {
			printk(KERN_WARNING
			       "Exception Monitor: waiting for "
			       "uninterruptible threads timedout.\n");
		}
		else if (ret != 0)
			return -1;
		/* This is a WORKAROUND !!
		* Here, exception_check variable is needed for checking
		* whether em_exception_monitor() is completely executed
		* or not. Without this workaround exception_check variable
		* becomes zero when abrupt closing of gzclient happens
		* and this leads to kernel panic!! This fix avoids by
		* returning from such scenario by having NULL check for
		* exception_check variable.
		*/
		if(exception_check)
			exception_check(exception_check_mode, regs);
		else
			return 0;
	/* Restart our siblings (so they can die, or whatever). */
		start_all_threads(current->mm);
	}
	return 0;
}

static inline int  em_show(struct pt_regs *regs)
{
	if (exception_check_mode && exception_check) {
		exception_check(exception_check_mode, regs);
		return 1;
	}
	return 0;
}

static inline void em_show_stack_overflow(struct pt_regs *regs)
{
	if (exception_check_mode && exception_check) {
#ifdef CONFIG_MIPS
		die("stack overflow", regs);
#else
		die("stack overflow", regs, SIGSTKFLT);
#endif
	}
}
#else
static inline int  em_show_coredump(struct pt_regs *regs) { return 0; }
static inline int  em_show(struct pt_regs *regs) { return 0; }
static inline void em_show_stack_overflow(struct pt_regs *regs) {}
#endif /* CONFIG_EXCEPTION_MONITOR */
#endif /* EXCEPTION_MONITOR_H */
