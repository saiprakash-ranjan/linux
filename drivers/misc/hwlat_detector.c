/*
 * hwlat_detector.c - A simple Hardware Latency detector.
 *
 * Use this module to detect large system latencies induced by the
 * behavior of certain underlying system hardware or firmware,
 * independent of Linux itself.  The code was developed originally to
 * detect the presence of SMIs on Intel and AMD systems, although
 * there is no dependency upon x86 herein.
 *
 * The classical example usage of this module is in detecting the
 * presence of SMIs or System Management Interrupts on Intel and AMD
 * systems. An SMI is a somewhat special form of hardware interrupt
 * spawned from earlier CPU debug modes in which the (BIOS/EFI/etc.)
 * firmware arranges for the South Bridge LPC (or other device) to
 * generate a special interrupt under certain circumstances, for
 * example, upon expiration of a special SMI timer device, due to
 * certain external thermal readings, on certain I/O address accesses,
 * and other situations. An SMI hits a special CPU pin, triggers a
 * special SMI mode (complete with special memory map), and the OS is
 * unaware.
 *
 * Although certain hardware-inducing latencies are necessary (for
 * example, a modern system often requires an SMI handler for correct
 * thermal control and remote management) they can wreak havoc upon
 * any OS-level performance guarantees toward low-latency, especially
 * when the OS is not even made aware of the presence of these
 * interrupts. For this reason, we need a somewhat brute force
 * mechanism to detect these interrupts. In this case, we do it by
 * hogging all of the CPU(s) for configurable timer intervals,
 * sampling the built-in CPU timer, looking for discontiguous
 * readings.
 *
 * WARNING: This implementation necessarily introduces latencies.
 *          Therefore, you should NEVER use this module in a
 *          production environment requiring any kind of low-latency
 *          performance guarantee(s).
 *
 * Copyright (C) 2015, BMW Car IT GbmH, Daniel Wagner <daniel.wagner@bmw-carit.de>
 * Copyright (C) 2008-2009 Jon Masters, Red Hat, Inc. <jcm@redhat.com>
 *
 * Includes useful feedback from Clark Williams <clark@redhat.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/trace_clock.h>

#define BANNER			"hwlat_detector: "
#define DRVNAME			"hwlat_detector"

#define DEFAULT_SAMPLE_WINDOW	1000000			/* 1s */
#define DEFAULT_SAMPLE_WIDTH	500000			/* 0.5s */
#define DEFAULT_LAT_THRESHOLD	10			/* 10us */

#define FIFO_ENTRIES		512UL			/* max entries */

struct sample {
	u64		duration;	/* ktime delta */
	u64		outer_duration;	/* ktime delta (outer loop) */
	struct timespec	timestamp;	/* wall time */
};

/* Sampling configuration parameters */
static struct {
	u64 threshold;			/* sample threshold level */
	u64 window;			/* total sampling window (on+off) */
	u64 width;			/* active sampling portion of window */

	/*
	 * Serialize updates of parameters with sampling, that is no
	 * changes while sampling.
	 */
	struct mutex lock;
} param;

/* Statistic values gathered while sampling */
static struct {
	u64 count;			/* total since reset */
	u64 max;			/* max hardware latency */
} stats;

/*
 * fifo is used as ring buffer, sampling function sample_get()
 * procudes the samples adds it to the fifo and file_sample_read()
 * consumes it.  On overflow the oldest entry will be overwritten
 */
static DEFINE_KFIFO(fifo, struct sample, FIFO_ENTRIES);
static DEFINE_SPINLOCK(fifo_lock);

static struct task_struct *kthread;	/* thread for sampling function */
static wait_queue_head_t s_wq;		/* waitqeue for new sample values */

/* module parameters */
static int enable;
static int param_threshold;
static int param_window;
static int param_width;

module_param_named(enable, enable, int, 0);
module_param_named(threshold, param_threshold, int, 0);
module_param_named(window, param_window, int, 0);
module_param_named(width, param_width, int, 0);

static void stats_reset(void)
{
	stats.count = 0;
	stats.max = 0;
	kfifo_reset(&fifo);
}

static void fifo_add_sample(struct sample *sample)
{
	/*
	 * Mimic a ring buffer by overwriting oldest entry when the
	 * fifo is full.
	 *
	 * We need to make sure in case the fifo is full and we remove
	 * the oldest entry before adding a new entry that the reader
	 * side always sees one element in the fifo. That means we
	 * need make sure we do not race with kfifo_out_locked().
	 */
	spin_lock(&fifo_lock);
	if (kfifo_is_full(&fifo))
		kfifo_skip(&fifo);
	kfifo_in(&fifo, sample, 1);
	spin_unlock(&fifo_lock);
}

#ifndef CONFIG_TRACING
#define time_type	ktime_t
#define time_get()	ktime_get()
#define time_to_us(x)	ktime_to_us(x)
#define time_sub(a, b)	ktime_sub(a, b)
#define init_time(a, b)	(a).tv64 = b
#define time_u64(a)	((a).tv64)
#else
#define time_type	u64
#define time_get()	trace_clock_local()
#define time_to_us(x)	div_u64(x, 1000)
#define time_sub(a, b)	((a) - (b))
#define init_time(a, b)	(a = b)
#define time_u64(a)	a
#endif

/**
 * sample_get - sample the CPU TSC and look for likely hardware latencies
 *
 * Used to repeatedly capture the CPU TSC (or similar), looking for
 * potential hardware-induced latency. Called with interrupts disabled
 * and with param.lock held.
 */
static int sample_get(void)
{
	time_type start, t1, t2, last_t2;
	s64 diff, total = 0;
	u64 sample = 0;
	u64 outer_sample = 0;
	int ret = -1;

	init_time(last_t2, 0);
	start = time_get(); /* start timestamp */

	do {
		t1 = time_get();	/* we'll look for a discontinuity */
		t2 = time_get();

		if (time_u64(last_t2)) {
			/* Check the delta from outer loop (t2 to next t1) */
			diff = time_to_us(time_sub(t1, last_t2));
			/* This shouldn't happen */
			if (diff < 0) {
				pr_err(BANNER "time running backwards\n");
				goto out;
			}
			if (diff > outer_sample)
				outer_sample = diff;
		}
		last_t2 = t2;

		total = time_to_us(time_sub(t2, start)); /* sample width */

		/* This checks the inner loop (t1 to t2) */
		diff = time_to_us(time_sub(t2, t1));     /* current diff */

		/* This shouldn't happen */
		if (diff < 0) {
			pr_err(BANNER "time running backwards\n");
			goto out;
		}

		if (diff > sample)
			sample = diff; /* only want highest value */

	} while (total <= param.width);

	ret = 0;

	/* If we exceed the threshold value, we have found a hardware latency */
	if (sample > param.threshold || outer_sample > param.threshold) {
		struct sample s;

		ret = 1;

		stats.count++;
		s.duration = sample;
		s.outer_duration = outer_sample;
		s.timestamp = CURRENT_TIME;
		fifo_add_sample(&s);

		/* Keep a running maximum ever recorded hardware latency */
		if (sample > stats.max)
			stats.max = sample;
	}

out:
	return ret;
}

static int sample_fn(void *arg)
{
	u64 interval;
	int err;

	while (!kthread_should_stop()) {
		mutex_lock(&param.lock);
		local_irq_disable();

		err = sample_get();

		local_irq_enable();
		mutex_unlock(&param.lock);

		if (err > 0)
			wake_up_interruptible(&s_wq);

		interval = param.window - param.width;
		do_div(interval, USEC_PER_MSEC);

		if (msleep_interruptible(interval))
			break;
	}

	return 0;
}

static int sample_start(void)
{
	kthread = kthread_run(sample_fn, NULL, DRVNAME);
	if (IS_ERR(kthread)) {
		pr_err(BANNER "could not start sampling thread\n");
		enable = 0;
		return -ENOMEM;
	}

	return 0;
}

static void sample_stop(void)
{
	int err;

	err = kthread_stop(kthread);
	if (err) {
		pr_err(BANNER "could not stop sampling thread\n");
		return;
	}

	enable = 0;
}

#define TMPBUFSIZE 128

static ssize_t sample_to_user(struct sample *sample,
				char __user *buf, size_t count)
{
	char tmpbuf[TMPBUFSIZE];
	size_t len;

	len = snprintf(tmpbuf, TMPBUFSIZE, "%010lu.%010lu\t%llu\t%llu\n",
			sample->timestamp.tv_sec,
			sample->timestamp.tv_nsec,
			sample->duration,
			sample->outer_duration);

	if (len > TMPBUFSIZE)
		len = TMPBUFSIZE;

	if (len > count)
		len = count;

	if (copy_to_user(buf, tmpbuf, len))
		return -EFAULT;

	return len;
}

static ssize_t u64_to_user(u64 val, char __user *buf,
				size_t count, loff_t *offset)
{
	char tmpbuf[TMPBUFSIZE];
	size_t len;

	len = snprintf(tmpbuf, TMPBUFSIZE, "%llu\n", val);
	if (len > TMPBUFSIZE)
		len = TMPBUFSIZE;

	return simple_read_from_buffer(buf, count, offset, tmpbuf, len);
}

static int u64_from_user(u64 *val, char const __user *buf, size_t count)
{
	char tmpbuf[TMPBUFSIZE];
	int err;

	if (!count)
		return 0;

	if (count > TMPBUFSIZE - 1)
		return -EINVAL;

	memset(tmpbuf, 0x0, TMPBUFSIZE);

	if (copy_from_user(tmpbuf, buf, count))
		return -EFAULT;

	err = kstrtoull(tmpbuf, 10, val);
	if (err)
		return err;

	return count;
}

static ssize_t file_write(u64 *val, char const __user *buf,
				size_t count, loff_t *offset)
{
	u64 tmp;
	int err;

	if (*offset)
		return -EINVAL;

	err = u64_from_user(&tmp, buf, count);
	if (err <= 0)
		return err;

	err = 0;
	mutex_lock(&param.lock);
	if (tmp < *val)
		*val = tmp;
	else
		err = -EINVAL;
	mutex_unlock(&param.lock);

	if (err)
		return err;
	return count;
}

static ssize_t file_window_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	return u64_to_user(param.window, buf, count, offset);
}

static ssize_t file_window_write(struct file *file, char const __user *buf,
					size_t count, loff_t *offset)
{
	return file_write(&param.window, buf, count, offset);
}

static ssize_t file_width_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	return u64_to_user(param.width, buf, count, offset);
}

static ssize_t file_width_write(struct file *file, char const __user *buf,
					size_t count, loff_t *offset)
{
	return file_write(&param.width, buf, count, offset);
}

static ssize_t file_threshold_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	return u64_to_user(param.threshold, buf, count, offset);
}

static ssize_t file_threshold_write(struct file *file, char const __user *buf,
					size_t count, loff_t *offset)
{
	return file_write(&param.threshold, buf, count, offset);
}

static ssize_t file_sample_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	struct sample sample;

	if (!(file->f_flags & O_NONBLOCK)) {
		/* block until we have a sample */
		wait_event_interruptible(s_wq, !kfifo_is_empty(&fifo));
		if (signal_pending(current))
			return -EINTR;
	}

	if (!kfifo_out_locked(&fifo, &sample, 1, &fifo_lock))
		return -EAGAIN;

	/* Ignore offset and just produce the next sample */
	return sample_to_user(&sample, buf, count);
}

static ssize_t file_enable_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	return u64_to_user(enable, buf, count, offset);
}

static ssize_t file_enable_write(struct file *file, char const __user *buf,
					size_t count, loff_t *offset)
{
	u64 val;
	int err;

	if (*offset)
		return -EINVAL;

	err = u64_from_user(&val, buf, count);
	if (err <= 0)
		return err;

	if (val == enable)
		return count;

	err = 0;
	if (val) {
		enable = 1;
		stats_reset();
		err = sample_start();
	} else
		sample_stop();

	if (err)
		return err;
	return count;
}

static const struct file_operations window_fops = {
	.read		= file_window_read,
	.write		= file_window_write,
	.llseek		= no_llseek,
	.owner		= THIS_MODULE
};

static const struct file_operations width_fops = {
	.read		= file_width_read,
	.write		= file_width_write,
	.llseek		= no_llseek,
	.owner		= THIS_MODULE
};

static const struct file_operations threshold_fops = {
	.read		= file_threshold_read,
	.write		= file_threshold_write,
	.llseek		= no_llseek,
	.owner		= THIS_MODULE
};

static const struct file_operations sample_fops = {
	.read		= file_sample_read,
	.llseek		= no_llseek,
	.owner		= THIS_MODULE,
};

static const struct file_operations enable_fops = {
	.read		= file_enable_read,
	.write		= file_enable_write,
	.llseek		= no_llseek,
	.owner		= THIS_MODULE
};

static struct dentry *hwlatdir;

static int __init debugfs_init(void)
{
	struct dentry *retval;

	retval = debugfs_create_dir(DRVNAME, NULL);
	if (IS_ERR_OR_NULL(retval))
		return PTR_ERR(retval);

	hwlatdir = retval;

	retval = debugfs_create_u64("count", S_IRUGO,
				hwlatdir, &stats.count);
	if (IS_ERR(retval))
		goto err;

	retval = debugfs_create_u64("max", S_IRUGO,
				hwlatdir, &stats.max);
	if (IS_ERR(retval))
		goto err;

	retval = debugfs_create_file("window",
				S_IRUGO | S_IWUSR,
				hwlatdir, NULL, &window_fops);
	if (IS_ERR(retval))
		goto err;

	retval = debugfs_create_file("width",
				S_IRUGO | S_IWUSR,
				hwlatdir, NULL, &width_fops);
	if (IS_ERR(retval))
		goto err;

	retval = debugfs_create_file("threshold",
				S_IRUGO | S_IWUSR,
				hwlatdir, NULL, &threshold_fops);
	if (IS_ERR(retval))
		goto err;

	retval = debugfs_create_file("sample", S_IRUGO,
				hwlatdir, NULL, &sample_fops);
	if (IS_ERR(retval))
		goto err;

	retval = debugfs_create_file("enable",
				S_IRUGO | S_IWUSR,
				hwlatdir, NULL, &enable_fops);
	if (IS_ERR(retval))
		goto err;

	return 0;
err:
	debugfs_remove_recursive(hwlatdir);
	hwlatdir = NULL;
	return retval ? PTR_ERR(retval) : -ENODEV;
}

static void __exit debugfs_cleanup(void)
{
	debugfs_remove_recursive(hwlatdir);
}

static void __init __hwlat_init(void)
{
	init_waitqueue_head(&s_wq);

	mutex_init(&param.lock);
	param.threshold = param_threshold ?
		param_threshold : DEFAULT_LAT_THRESHOLD;
	param.window = param_window ? param_window : DEFAULT_SAMPLE_WINDOW;
	param.width = param_width ? param_width : DEFAULT_SAMPLE_WIDTH;
}

static int __init hwlat_init(void)
{
	int err;

	__hwlat_init();

	err = debugfs_init();
	if (err)
		return err;

	if (enable)
		err = sample_start();

	return err;
}

static void __exit hwlat_exit(void)
{
	if (enable)
		sample_stop();

	debugfs_cleanup();
}

module_init(hwlat_init);
module_exit(hwlat_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Daniel Wagner");
MODULE_DESCRIPTION("Hardware latency detector");
