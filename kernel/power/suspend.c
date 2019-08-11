/*
 * kernel/power/suspend.c - Suspend to RAM and standby functionality.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 * Copyright (c) 2009 Rafael J. Wysocki <rjw@sisk.pl>, Novell Inc.
 *
 * This file is released under the GPLv2.
 */

#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <linux/gfp.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/snsc_boot_time.h>
#include <linux/ftrace.h>
#include <trace/events/power.h>
#include <linux/interrupt.h>
#ifdef CONFIG_SNSC_SSBOOT
#include <linux/ssboot.h>
#endif
#ifdef CONFIG_SNSC_HSS
#include <linux/snsc_hss.h>
#endif

#ifdef	CONFIG_SNSC_SAFE_SUSPEND
extern int suspend_remount(void);
extern int resume_remount(void);
#endif

#if defined(CONFIG_SNSC_SSBOOT) && defined(CONFIG_X86)
void ssboot_arch_post_nonboot_cpu_wake(void);
void ssboot_arch_pre_nonboot_cpu_susp(void);
#endif

#include "power.h"

struct pm_sleep_state pm_states[PM_SUSPEND_MAX] = {
	[PM_SUSPEND_FREEZE] = { .label = "freeze", .state = PM_SUSPEND_FREEZE },
	[PM_SUSPEND_STANDBY] = { .label = "standby", },
	[PM_SUSPEND_MEM] = { .label = "mem", },
#ifdef CONFIG_SNSC_SSBOOT
        [PM_SUSPEND_SNAPSHOT]   = { .label = "snapshot", .state = PM_SUSPEND_SNAPSHOT },
#endif

};

static const struct platform_suspend_ops *suspend_ops;

static bool need_suspend_ops(suspend_state_t state)
{
	return !!(state > PM_SUSPEND_FREEZE);
}

static DECLARE_WAIT_QUEUE_HEAD(suspend_freeze_wait_head);
static bool suspend_freeze_wake;

static void freeze_begin(void)
{
	suspend_freeze_wake = false;
}

static void freeze_enter(void)
{
	wait_event(suspend_freeze_wait_head, suspend_freeze_wake);
}

void freeze_wake(void)
{
	suspend_freeze_wake = true;
	wake_up(&suspend_freeze_wait_head);
}
EXPORT_SYMBOL_GPL(freeze_wake);

static bool valid_state(suspend_state_t state)
{
	/*
	 * PM_SUSPEND_STANDBY and PM_SUSPEND_MEM states need low level
	 * support and need to be valid to the low level
	 * implementation, no valid callback implies that none are valid.
	 */
	return suspend_ops && suspend_ops->valid && suspend_ops->valid(state);
}

/**
 * suspend_set_ops - Set the global suspend method table.
 * @ops: Suspend operations to use.
 */
void suspend_set_ops(const struct platform_suspend_ops *ops)
{
	suspend_state_t i;

	lock_system_sleep();

	suspend_ops = ops;
	for (i = PM_SUSPEND_STANDBY; i <= PM_SUSPEND_MEM; i++)
		pm_states[i].state = valid_state(i) ? i : 0;

	unlock_system_sleep();
}
EXPORT_SYMBOL_GPL(suspend_set_ops);

/**
 * suspend_valid_only_mem - Generic memory-only valid callback.
 *
 * Platform drivers that implement mem suspend only and only need to check for
 * that in their .valid() callback can use this instead of rolling their own
 * .valid() callback.
 */
int suspend_valid_only_mem(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM;
}
EXPORT_SYMBOL_GPL(suspend_valid_only_mem);

static int suspend_test(int level)
{
#ifdef CONFIG_PM_DEBUG
	if (pm_test_level == level) {
		pr_info("suspend debug: Waiting for %d milliseconds.\n",
			pm_test_delay);
		mdelay(pm_test_delay);
		return 1;
	}
#endif /* !CONFIG_PM_DEBUG */
	return 0;
}

#ifdef CONFIG_SNSC_SSBOOT
static int suspend_prepare_ssboot(int kernel_task_only)
{
	int error;

	if (!kernel_task_only && freeze_processes()) {
		error = -EAGAIN;
		return error;
	}

	error = ssboot_prepare();
	if (error) {
		thaw_processes();
		return error;
	}

#ifdef CONFIG_SNSC_SAFE_SUSPEND
	/*
	 * Because sync_filesystem() called from
	 * suspend_remount() wakes up bdi threads, and wait
	 * for completion of it. Because bdi threads are
	 * freezable, we have to remount before freezing that
	 * tasks.
	 */
	if (suspend_remount()) {
		error = -EIO;
		thaw_processes();
		return error;
	}
#endif

	if (freeze_kernel_threads()) {
		error = -EAGAIN;
		thaw_processes();
		return error;
	}

	return 0;
}
#endif /* CONFIG_SNSC_SSBOOT */

/**
 * suspend_prepare - Prepare for entering system sleep state.
 *
 * Common code run for every system sleep state that can be entered (except for
 * hibernation).  Run suspend notifiers, allocate the "suspend" console and
 * freeze processes.
 */
static int suspend_prepare(suspend_state_t state)
{
	int error;

	if (need_suspend_ops(state) && (!suspend_ops || !suspend_ops->enter))
		return -EPERM;

	pm_prepare_console();

	error = pm_notifier_call_chain(PM_SUSPEND_PREPARE);
	if (error)
		goto Finish;

#ifdef CONFIG_SNSC_SSBOOT
	if (state == PM_SUSPEND_SNAPSHOT) {
		error = suspend_prepare_ssboot(0);
		if (error)
			goto Failed;
	} else {
#endif
#ifdef CONFIG_SUSPEND_FREEZER
	error = freeze_processes();
#endif
	if (!error) {
#ifdef	CONFIG_SNSC_SAFE_SUSPEND
	(void) suspend_remount();
#endif
#ifdef CONFIG_SUSPEND_FREEZER
	error = freeze_kernel_threads();
	if (error)
		thaw_processes();
	else
		return 0;
#else
	return 0;
#endif
	}
#ifdef CONFIG_SNSC_SSBOOT
	}
 Failed:
#endif
	suspend_stats.failed_freeze++;
	dpm_save_failed_step(SUSPEND_FREEZE);
 Finish:
	pm_notifier_call_chain(PM_POST_SUSPEND);
	pm_restore_console();
	return error;
}

/* default implementation */
void __attribute__ ((weak)) arch_suspend_disable_irqs(void)
{
	local_irq_disable();
}

/* default implementation */
void __attribute__ ((weak)) arch_suspend_enable_irqs(void)
{
	local_irq_enable();
}

/**
 * suspend_enter - Make the system enter the given sleep state.
 * @state: System sleep state to enter.
 * @wakeup: Returns information that the sleep state should not be re-entered.
 *
 * This function should be called after devices have been suspended.
 */
static int suspend_enter(suspend_state_t state, bool *wakeup)
{
	int error;

	if (need_suspend_ops(state) && suspend_ops->prepare) {
		error = suspend_ops->prepare();
		if (error)
			goto Platform_finish;
	}

	error = dpm_suspend_end(PMSG_SUSPEND);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to power down\n");
		goto Platform_finish;
	}

	if (need_suspend_ops(state) && suspend_ops->prepare_late) {
		error = suspend_ops->prepare_late();
		if (error)
			goto Platform_wake;
	}

	if (suspend_test(TEST_PLATFORM))
		goto Platform_wake;

	/*
	 * PM_SUSPEND_FREEZE equals
	 * frozen processes + suspended devices + idle processors.
	 * Thus we should invoke freeze_enter() soon after
	 * all the devices are suspended.
	 */
	if (state == PM_SUSPEND_FREEZE) {
		freeze_enter();
		goto Platform_wake;
	}

#ifdef CONFIG_SNSC_SSBOOT
	if (state == PM_SUSPEND_SNAPSHOT) {
		/* Make sure all free pages linked to free list. */
		drain_all_pages();

		/* Make sure all page caches linked to LRU list. */
		error = lru_add_drain_all();
		if (error)
			goto Platform_wake;
#ifdef CONFIG_X86
		ssboot_arch_pre_nonboot_cpu_susp();
#endif
	}
#endif
#ifdef CONFIG_SNSC_SAVE_PROCESS_AFFINITY_ON_SUSPEND
	save_process_affinity();
#endif
#ifdef CONFIG_SNSC_SAVE_IRQ_AFFINITY_ON_SUSPEND
	irq_save_affinity();
#endif

	error = disable_nonboot_cpus();
	if (error || suspend_test(TEST_CPUS))
		goto Enable_cpus;

	arch_suspend_disable_irqs();
	BUG_ON(!irqs_disabled());

	BOOT_TIME_ADD1("PM: suspend enter");
	error = syscore_suspend();
	if (!error) {
		*wakeup = pm_wakeup_pending();
		if (!(suspend_test(TEST_CORE) || *wakeup)) {
			error = suspend_ops->enter(state);

#ifdef CONFIG_SNSC_SSBOOT
			/*
			 * very low-level suspend code cannot
			 * return error on some architectures
			 */
	if (state == PM_SUSPEND_SNAPSHOT) {
			if (!error && ssboot_is_error())
				error = ssboot_get_error();
	}
#endif

			events_check_enabled = false;
		}
		syscore_resume();
	}
#ifdef CONFIG_SNSC_BOOT_TIME
      boot_time_resume();
#endif
	BOOT_TIME_ADD1("PM: resume start");

	arch_suspend_enable_irqs();
	BUG_ON(irqs_disabled());

 Enable_cpus:
	enable_nonboot_cpus();

#ifdef CONFIG_SNSC_SAVE_IRQ_AFFINITY_ON_SUSPEND
	irq_restore_affinity();
#endif
#ifdef CONFIG_SNSC_SAVE_PROCESS_AFFINITY_ON_SUSPEND
	restore_process_affinity();
#endif

#if defined(CONFIG_SNSC_SSBOOT) && defined(CONFIG_X86)
	if (state == PM_SUSPEND_SNAPSHOT) {
		ssboot_arch_post_nonboot_cpu_wake();
	}
#endif

 Platform_wake:
	if (need_suspend_ops(state) && suspend_ops->wake)
		suspend_ops->wake();

	dpm_resume_start(PMSG_RESUME);
	BOOT_TIME_ADD1("PM: device resumed(noirq)");

 Platform_finish:
	if (need_suspend_ops(state) && suspend_ops->finish)
		suspend_ops->finish();

	return error;
}

/**
 * suspend_devices_and_enter - Suspend devices and enter system sleep state.
 * @state: System sleep state to enter.
 */
int suspend_devices_and_enter(suspend_state_t state)
{
	int error;
	bool wakeup = false;

	if (need_suspend_ops(state) && !suspend_ops)
		return -ENOSYS;

	trace_machine_suspend(state);
	if (need_suspend_ops(state) && suspend_ops->begin) {
		error = suspend_ops->begin(state);
		if (error)
			goto Close;
	}
#ifdef CONFIG_SNSC_SSBOOT
 Rewrite:
#endif
	suspend_console();
	ftrace_stop();
	suspend_test_start();
	error = dpm_suspend_start(PMSG_SUSPEND);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to suspend\n");
		goto Recover_platform;
	}
	suspend_test_finish("suspend devices");
	if (suspend_test(TEST_DEVICES))
		goto Recover_platform;

	do {
#ifdef CONFIG_SNSC_SSBOOT
		error = suspend_enter(state, &wakeup);
#else
		suspend_enter(state, &wakeup);
#endif
	} while (!error && !wakeup && need_suspend_ops(state)
		&& suspend_ops->suspend_again && suspend_ops->suspend_again());

 Resume_devices:
	suspend_test_start();
	dpm_resume_end(PMSG_RESUME);
	BOOT_TIME_ADD1("PM: device resumed");
	suspend_test_finish("resume devices");
	ftrace_start();
	resume_console();
 Close:
	if (need_suspend_ops(state) && suspend_ops->end)
		suspend_ops->end();
	trace_machine_suspend(PWR_EVENT_EXIT);

#ifdef CONFIG_SNSC_SSBOOT
	if (state == PM_SUSPEND_SNAPSHOT) {
		int ret;
		if (!error && ssboot_is_writing()) {
#ifdef CONFIG_SNSC_SAFE_SUSPEND
			(void) resume_remount();
#endif
			/* thaw kernel threads only */
			thaw_kernel_threads();
			error = ssboot_write();
		}
		ret = ssboot_finish();
		if (!error) {
			error = ret;
		}
		if (!error && ssboot_is_rewriting()) {
#ifdef CONFIG_SNSC_SAFE_SUSPEND
			(void) resume_remount();
#endif
			/* thaw kernel threads only */
			thaw_kernel_threads();
			thaw_workqueues();

			/* prepare for suspend again */
			error = suspend_prepare_ssboot(1);
			if (!error) {
				goto Rewrite;
			}
		}
	}
#endif /* CONFIG_SNSC_SSBOOT */
	return error;

 Recover_platform:
	if (need_suspend_ops(state) && suspend_ops->recover)
		suspend_ops->recover();
	goto Resume_devices;
}

/**
 * suspend_finish - Clean up before finishing the suspend sequence.
 *
 * Call platform code to clean up, restart processes, and free the console that
 * we've allocated. This routine is not called for hibernation.
 */
static void suspend_finish(void)
{
#ifdef	CONFIG_SNSC_SAFE_SUSPEND
	suspend_thaw_kernel_threads();
	/*
	 * Ideally we should be able to call resume_remount()
	 * twice as suspend_rw_fs_list would be empty.
	 * Calling resume_remount() simplifies code as of now.
	 */
	(void) resume_remount();
#endif
	suspend_thaw_processes();
	pm_notifier_call_chain(PM_POST_SUSPEND);
	pm_restore_console();
}

/**
 * enter_state - Do common work needed to enter system sleep state.
 * @state: System sleep state to enter.
 *
 * Make sure that no one else is trying to put the system into a sleep state.
 * Fail if that's not the case.  Otherwise, prepare for system suspend, make the
 * system enter the given sleep state and clean up after wakeup.
 */
static int enter_state(suspend_state_t state)
{
	int error;
#ifdef CONFIG_SNSC_SSBOOT
	unsigned int nofreeze;
#endif

	if (state == PM_SUSPEND_FREEZE) {
#ifdef CONFIG_PM_DEBUG
		if (pm_test_level != TEST_NONE && pm_test_level <= TEST_CPUS) {
			pr_warning("PM: Unsupported test mode for freeze state,"
				   "please choose none/freezer/devices/platform.\n");
			return -EAGAIN;
		}
#endif
	} else if (!valid_state(state)) {
		return -EINVAL;
	}
	if (!mutex_trylock(&pm_mutex))
		return -EBUSY;

	if (state == PM_SUSPEND_FREEZE)
		freeze_begin();

#ifdef CONFIG_SNSC_HSS
	hss_suspend_prepare();
#endif

	printk(KERN_INFO "PM: Syncing filesystems ... ");
	sys_sync();
	printk("done.\n");

	pr_debug("PM: Preparing system for %s sleep\n", pm_states[state].label);
#ifdef CONFIG_SNSC_SSBOOT
       /* prevent this task from being frozen */
        nofreeze = current->flags & PF_NOFREEZE;
        current->flags |= PF_NOFREEZE;
#endif

	error = suspend_prepare(state);
	if (error)
		goto Unlock;

	if (suspend_test(TEST_FREEZER))
		goto Finish;

	pr_debug("PM: Entering %s sleep\n", pm_states[state].label);
	pm_restrict_gfp_mask();
	error = suspend_devices_and_enter(state);
	pm_restore_gfp_mask();

 Finish:
	pr_debug("PM: Finishing wakeup.\n");
	suspend_finish();
	BOOT_TIME_ADD1("PM: resume finished");
#ifdef CONFIG_SNSC_SSBOOT
	if (!nofreeze)
		set_freezable();
#endif
 Unlock:
	mutex_unlock(&pm_mutex);
	return error;
}

/**
 * pm_suspend - Externally visible function for suspending the system.
 * @state: System sleep state to enter.
 *
 * Check if the value of @state represents one of the supported states,
 * execute enter_state() and update system suspend statistics.
 */
int pm_suspend(suspend_state_t state)
{
	int error;

	if (state <= PM_SUSPEND_ON || state >= PM_SUSPEND_MAX)
		return -EINVAL;

	error = enter_state(state);
	if (error) {
		suspend_stats.fail++;
		dpm_save_failed_errno(error);
	} else {
		suspend_stats.success++;
	}
	return error;
}
EXPORT_SYMBOL(pm_suspend);
