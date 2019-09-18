/* 2013-03-18: File added and changed by Sony Corporation */
/*
 *  lib/snsc_lctracer.c
 *
 *  Copyright 2012,2013 Sony Corporation
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/snsc_lctracer.h>
#ifdef CONFIG_SMP
#include <linux/smp.h>
#endif
#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif

#if defined(__LITTLE_ENDIAN)
#define STR_ENDIAN "little endian"
#else /* BIG_ENDIAN */
#define STR_ENDIAN "big endian"
#endif

#define STOP_TRACE		0
#define START_TRACE		1

/* path and file name */
#define DEFAULT_PATH		"/var/log/lctracer"
#define TRACE_FILE_NAME		"lct_lctracer.dat"
#define IOWAIT_FILE_NAME	"lct_iowait.log"
#define HEADER_FILE_NAME	"lct_measurement_env.log"
#define IRQ_DESC_FILE_NAME	"lct_irq_desc.log"
#define LCTRACER_PARAM_PATH	"/sys/module/snsc_lctracer/parameters"

/* path and file size */
#define FILEPATH_SIZE		48
/* file name max size is 32 */
#define FILENAME_SIZE		(FILEPATH_SIZE + 32)

#define IRQ_DESC_BUF_SIZE	256

#define IOWAIT_INFO_SIZE	64
#define IOWAIT_TIME_SIZE	17

#ifdef CONFIG_SMP
#define HEADER_INFO_SIZE	((CONFIG_NR_CPUS * 300) + FILEPATH_SIZE)
#else
#define HEADER_INFO_SIZE	(256 + FILEPATH_SIZE)
#endif

#define STATE_INFO_SIZE		(256 + FILEPATH_SIZE)

#define KB			1024
#define MAX_HASH		256
#define WCHAN_SIZE		26
#define MESSAGE_SIZE		WCHAN_SIZE
#define THREAD_NAME_SIZE	16
#define PROC_USER_SIZE		WCHAN_SIZE
#define PROC_CMD_SIZE		16
#define STATE_STR_SIZE		32
#define POLICY_STR_SIZE		16
#define INTERVAL_DEFAULT	100 /* unit is ms */
#define GET_TIME()		sched_clock()
#define LCTRACER_VERSION_MAJOR	1
#define LCTRACER_VERSION_MINOR	2
#define LCTRACER_VERSION_MINOR	3

/* if buffer is overflow and file system is not ready
 * how many data should be saved */
#define OV_THRESHOLD_NUM_SAVE	1

#define STATE_FORMAT							\
	"LCTracer state: %s\n"						\
	"buffer size : %d bytes\n"					\
	"buffer threshold : %d bytes\n"					\
	"iowait sample interval : %dms\n"				\
	"saving path : %s\n"						\
	"autostart : %s\n"						\
	"storing to filesystem : %s\n"					\
	"buffer address : 0x%lX %s\n",					\
	(lctracer_is_running() ? "running" : "stopped"),		\
	buf_size, buf_threshold,					\
	iowait_interval,						\
	saving_path,							\
	(autostart ? "enabled" : "disabled"),				\
	(filedump ? "enabled" : "disabled"),				\
	buf_addr,							\
	(buf_addr ? "" : "(Not specify buffer address)")

#define LCTR_PR_ERR(fmt, arg...)	pr_err("LCTR: "fmt, ##arg)
#define LCTR_PR_INFO(fmt, arg...)	pr_info("LCTR: "fmt, ##arg)
#define LCTR_PR_WARNING(fmt, arg...)	pr_warn("LCTR: "fmt, ##arg)

enum entry_mode {
	SNSC_LCTRACER_THREAD_MODE	= 0,
	SNSC_LCTRACER_IRQ_MODE		= 1,
	SNSC_LCTRACER_USER_MODE		= 2
};

/* entry data IRQ time(high 16-bit) + IRQ number(low 16-bit) */
#define ENTRY_DATA_IRQ_TIME_OFFSET	16
#define ENTRY_DATA_IRQ_NUM_MASK		0xffff
#define ENTRY_DATA_IRQ_NUM_SIZE		16

#define ENTRY_INFO_MODE_MASK		0xc0000000 /* bit width 2 */
#define ENTRY_INFO_POLICY_MASK		0x38000000 /* bit width 3 */
#define ENTRY_INFO_WCHAN_OST_MASK	0x07ff0000 /* bit width 11 */
#define ENTRY_INFO_PRIORITY_MASK	0x0000ff00 /* bit width 8 */
/* task->state=0/1/2/4/8/64 is possible, so set state bit width 5 */
#define ENTRY_INFO_STATE_MASK		0x000000f8 /* bit width 5 */
#define ENTRY_INFO_CPUID_MASK		0x00000007 /* bit width 3 */

#define ENTRY_INFO_MODE_OFFSET		30
#define ENTRY_INFO_POLICY_OFFSET	27
#define ENTRY_INFO_WCHAN_OST_OFFSET	16
#define ENTRY_INFO_PRIORITY_OFFSET	8
#define ENTRY_INFO_STATE_OFFSET		3
#define ENTRY_INFO_CPUID_OFFSET		0

#define GET_ENTRY_INFO(BITTYPE, x)	\
	(((x) & ENTRY_INFO_##BITTYPE##_MASK) >> ENTRY_INFO_##BITTYPE##_OFFSET)

#define SET_ENTRY_INFO(INFO, BITTYPE, x)			\
	(INFO |= (((x) << ENTRY_INFO_##BITTYPE##_OFFSET) &	\
		ENTRY_INFO_##BITTYPE##_MASK))

/* re-define TASK_DEAD according to state bit width */
#define LCTRACER_STATE_TASK_DEAD	0x10

/* trace entry is 64 bytes */
struct snsc_lctracer_trace_entry {
	u64 time;
	u32 data;
	/*
	 * u32 info structure as follow:
	 *	u32 mode	: 2;
	 *	u32 policy	: 3;
	 *	u32 offset	: 11;
	 *	u32 prio	: 8;
	 *	u32 state	: 5;
	 *	u32 cpuid	: 3;
	 */
	u32 info;
	s16 tgid;
	s16 ppid;
	s16 npid;
	char message[MESSAGE_SIZE];
	char tname[THREAD_NAME_SIZE];
} __attribute__((__packed__));

struct trace_data {
	int trace_state;
	/* flag of allocating ring buffer for tracing data */
	int need_free_buf;
	/* flag of overflow for reading buffer data */
	int overflow;
	/* flag of reading data for proc dump when overflow happens */
	int overflow_read;
	/* the start index of storing_data_daemon reading */
	int dump_start_index;
	/* the end index of storing_data_daemon reading */
	int dump_end_index;
	/* flag of reaching the buffer threshold */
	int reach_threshold;
	/* entry index in buffer */
	unsigned int index;
	/* the last index in buffer, which is used for seq */
	unsigned int last_index;
	/* the number of entry in buffer */
	unsigned int num_entry_buf;
	/* the number of entry in buffer threshold */
	unsigned int num_entry_threshold;
	/* counter of reaching the buffer threshold for writing data */
	unsigned int threshold_cnt;
	/* task struct of storing data daemon */
	struct task_struct *trace_thread;
	/* task struct of iowait daemon */
	struct task_struct *iowait_thread;
	struct snsc_lctracer_trace_entry *entries;
	struct dump_cookie *private;
#ifdef CONFIG_MAGIC_SYSRQ
	/* SysRq work */
	struct work_struct lct_sysrq_work;
#endif
};

static struct trace_data trace_datas;
static DEFINE_SPINLOCK(lock);

/* hash table of wchan info */
struct wchan_hash {
	u32 addr;
	u32 offset;
	char wchan[WCHAN_SIZE];
};

/* the buffer of wchan info */
struct dump_cookie {
	struct wchan_hash hash[MAX_HASH];
	char wchan_buf[KSYM_SYMBOL_LEN];
};

/* saving path set in Kconfig */
static char *saving_path = CONFIG_SNSC_LCTRACER_FILE_PATH;
/* flag of auto-start measurement */
static int autostart;
/* flag of storing data to file system */
static int filedump = 1;
static unsigned long buf_addr;
/* both buffer size and threshold unit are Byte */
static int buf_size = CONFIG_SNSC_LCTRACER_BUF_SIZE_KB * KB;
static int old_buf_size;
/* by default threshold is 20% of buffer size */
static int buf_threshold = CONFIG_SNSC_LCTRACER_BUF_SIZE_KB * KB / 5;
static int iowait_interval = INTERVAL_DEFAULT;

static inline int lctracer_is_running(void)
{
	struct trace_data *pdata = &trace_datas;

	return pdata->trace_state == START_TRACE;
}

int snsc_lctracer_is_running(void)
{
	return lctracer_is_running();
}

static int param_set_val(const char *val, struct kernel_param *kp)
{
	if (lctracer_is_running()) {
		LCTR_PR_WARNING("please stop measurement at first\n");
		return -EBUSY;
	}

	return param_set_int(val, kp);
}

#define module_param_set(name, value, type, perm)			\
	param_check_##type(name, &(value));				\
	module_param_call(name, param_set_val, param_get_##type,	\
			&value, perm);					\
	__MODULE_PARM_TYPE(name, #type)

module_param_set(size, buf_size, int, S_IRUSR|S_IWUSR);
module_param_set(threshold, buf_threshold, int, S_IRUSR|S_IWUSR);
module_param_set(interval, iowait_interval, int, S_IRUSR|S_IWUSR);
module_param_set(autostart, autostart, int, S_IRUSR|S_IWUSR);
module_param_set(file, filedump, int, S_IRUSR|S_IWUSR);
module_param_set(addr, buf_addr, ulong, S_IRUSR|S_IWUSR);

static int trace_state_init(void)
{
	struct trace_data *pdata = &trace_datas;

	pdata->dump_start_index = -1;
	pdata->dump_end_index = 0;
	pdata->index = 0;
	pdata->overflow = 0;
	pdata->overflow_read = 0;
	pdata->last_index = 0;

	/* for stroing data to file system */
	pdata->threshold_cnt = 0;
	pdata->reach_threshold = 0;
	pdata->num_entry_threshold = buf_threshold /
		sizeof(struct snsc_lctracer_trace_entry);
	pdata->num_entry_buf = buf_size /
		sizeof(struct snsc_lctracer_trace_entry);

	return 0;
}

static void buffer_free(void)
{
	struct trace_data *pdata = &trace_datas;

	if (pdata->need_free_buf) {
		vfree(pdata->entries);
		pdata->need_free_buf = 0;
	}
	vfree(pdata->private);
}

static int buffer_init(void)
{
	struct trace_data *pdata = &trace_datas;

	if (buf_addr)
		pdata->entries = phys_to_virt(buf_addr);
	else {
		pdata->entries = vmalloc(pdata->num_entry_buf *
				sizeof(struct snsc_lctracer_trace_entry));
		pdata->need_free_buf = 1;
	}
	pdata->private = vmalloc(sizeof(struct dump_cookie));

	if (!pdata->entries || !pdata->private) {
		LCTR_PR_ERR("fail to allocate memory\n");
		buffer_free();
		return -ENOMEM;
	}
	old_buf_size = buf_size;

	return 0;
}

static int module_param_check(void)
{
	int ret = 0;

	if (strlen(CONFIG_SNSC_LCTRACER_FILE_PATH) >= FILEPATH_SIZE) {
		LCTR_PR_WARNING("file path in Kconfig is too long, "
				"use default path %s\n", DEFAULT_PATH);
		saving_path = DEFAULT_PATH;
	}
	if (buf_size <= 0) {
		LCTR_PR_WARNING("buffer size %d is not valid\n",
				buf_size);
		ret = -EINVAL;
		goto out;
	}
	if (iowait_interval <= 0) {
		LCTR_PR_WARNING("sample interval %d is not valid\n",
				iowait_interval);
		ret = -EINVAL;
		goto out;
	}

	if (!filedump)
		goto out;
	if (buf_threshold <= 0) {
		LCTR_PR_WARNING("buffer threshold %d is not valid\n",
				buf_threshold);
		ret = -EINVAL;
		goto out;
	}
	if (buf_threshold >= buf_size) {
		LCTR_PR_WARNING("buffer threshold is not less than buffer "
				"size\n"
				"buf_size = %d, buf_threshold = %d\n",
				buf_size, buf_threshold);
		ret = -EINVAL;
	}
out:
	return ret;
}

static const char *wchan_address_to_func(unsigned long addr,
		unsigned long *poffset)
{
	char *mod_name;
	const char *fname = NULL;
	unsigned long offset, size;
	struct wchan_hash *hash = NULL;
	struct trace_data *pdata = &trace_datas;
	struct dump_cookie *cookie = (struct dump_cookie *)pdata->private;

	hash = &cookie->hash[addr % MAX_HASH];
	if (addr != hash->addr) {
		fname = kallsyms_lookup(addr, &size, &offset,
				&mod_name, cookie->wchan_buf);
		if (fname) {
			snprintf(hash->wchan, sizeof(hash->wchan), "%s", fname);
			hash->addr = addr;
			hash->offset = offset;
		}
	}
	*poffset = hash->offset;

	return hash->wchan;
}

static inline int convert_wchan_info(int start, int end)
{
	const char *fname = NULL;
	unsigned int i;
	unsigned long offset = 0;
	struct snsc_lctracer_trace_entry *entry;
	struct trace_data *pdata = &trace_datas;

	for (i = start; i < end; i++) {
		entry = pdata->entries + (i % pdata->num_entry_buf);
		if ((GET_ENTRY_INFO(MODE, entry->info)
				!= SNSC_LCTRACER_THREAD_MODE)
				|| (!entry->data))
			continue;

		fname = wchan_address_to_func(entry->data, &offset);
		SET_ENTRY_INFO(entry->info, WCHAN_OST, offset);
		snprintf(entry->message, sizeof(entry->message), "%s", fname);
	}

	return 0;
}

static inline int store_trace_data(struct file *filp, loff_t *pos,
		int *start, int *end)
{
	size_t write_size = 0;
	size_t len = sizeof(struct snsc_lctracer_trace_entry);
	struct trace_data *pdata = &trace_datas;
	struct snsc_lctracer_trace_entry *entry;
	mm_segment_t oldfs;

	/* convert wchan information */
	convert_wchan_info(*start, *end);

	/* write to file system */
	write_size = *end - *start;
	*end = *end % pdata->num_entry_buf;
	entry = pdata->entries + *start;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	if (*end < *start) {
		vfs_write(filp, (char *)entry,
				(len * (pdata->num_entry_buf - *start)), pos);
		vfs_write(filp, (char *)pdata->entries, (len * (*end)), pos);
	} else
		vfs_write(filp, (char *)entry, (len * write_size), pos);
	set_fs(oldfs);

	return 0;
}

static void wait_for_system_ready(void)
{
	while (system_state != SYSTEM_RUNNING)
		msleep(100);
}

static int daemon_trace_data(void *data)
{
	char trace_file[FILENAME_SIZE] = {0};
	int old_reach_threshold = 0;
	int open_trace_file = 0;
	unsigned long flags = 0;
	struct file *filp = NULL;
	struct trace_data *pdata = &trace_datas;
	struct snsc_lctracer_trace_entry *entry_start;
	struct snsc_lctracer_trace_entry *entry_end;
	loff_t filp_pos = 0;

	snprintf(trace_file, sizeof(trace_file), "%s/%s", saving_path,
			TRACE_FILE_NAME);
	wait_for_system_ready();

	while (!kthread_should_stop()) {
		if (!open_trace_file) {
			filp = filp_open(trace_file, O_WRONLY | O_CREAT
					| O_TRUNC, S_IRUGO | S_IWUSR);
			if (IS_ERR(filp)) {
				msleep(100);
				continue;
			}
			open_trace_file = 1;
		}
		if (pdata->reach_threshold) {
			if (!filp)
				continue;

			/* buffer is overflow or
			 * remaining size is less than one threshold */
			spin_lock_irqsave(&lock, flags);
			if ((pdata->reach_threshold *
					pdata->num_entry_threshold) >
					(pdata->num_entry_buf -
					pdata->num_entry_threshold)) {
				pdata->threshold_cnt = 0;
				pdata->reach_threshold = OV_THRESHOLD_NUM_SAVE;
				if (pdata->index < pdata->num_entry_threshold) {
					pdata->dump_start_index =
						(pdata->index -
						OV_THRESHOLD_NUM_SAVE *
						pdata->num_entry_threshold) +
						pdata->num_entry_buf;
				} else {
					pdata->dump_start_index =
						(pdata->index -
						pdata->num_entry_threshold);
				}
				LCTR_PR_WARNING("some storing data are missing"
						" as buffer has overflowed\n");
			} else {
				pdata->dump_start_index = pdata->dump_end_index;
			}
			old_reach_threshold = pdata->reach_threshold;
			spin_unlock_irqrestore(&lock, flags);
			pdata->dump_end_index = pdata->dump_start_index +
					old_reach_threshold *
					pdata->num_entry_threshold;
			store_trace_data(filp, &filp_pos,
					&pdata->dump_start_index,
					&pdata->dump_end_index);

			/* check that maybe there is missing data */
			entry_start = pdata->entries + pdata->dump_start_index;
			entry_end = pdata->dump_end_index ?
				(pdata->entries + (pdata->dump_end_index - 1)) :
				(pdata->entries + (pdata->num_entry_buf - 1));
			if (entry_start->time > entry_end->time)
				LCTR_PR_WARNING("perhaps some storing data "
						"are missing\n");

			/* reach_threshold may be modified by add_trace_entry */
			spin_lock_irqsave(&lock, flags);
			pdata->reach_threshold -= old_reach_threshold;
			spin_unlock_irqrestore(&lock, flags);
		} else {
			msleep(100);
		}
	}
	if (open_trace_file && (pdata->dump_end_index != pdata->index)) {
		pdata->dump_start_index = pdata->dump_end_index;
		pdata->dump_end_index = (pdata->index <
				pdata->dump_start_index) ?
				(pdata->index + pdata->num_entry_buf) :
				pdata->index;
		store_trace_data(filp, &filp_pos, &pdata->dump_start_index,
				&pdata->dump_end_index);
	}
	if (open_trace_file)
		filp_close(filp, NULL);
	else
		LCTR_PR_ERR("fail to open %s\n", trace_file);

	return 0;
}

static int create_daemon_trace_data(void)
{
	struct trace_data *pdata = &trace_datas;

	pdata->trace_thread = kthread_run(daemon_trace_data, NULL,
			"lct_trace_d");
	if (IS_ERR(pdata->trace_thread)) {
		LCTR_PR_ERR("fail to create lct_trace_d\n");
		return -ENOMEM;
	}

	return 0;
}

static void kill_daemon_trace_data(void)
{
	struct trace_data *pdata = &trace_datas;

	if (pdata->trace_thread) {
		kthread_stop(pdata->trace_thread);
		pdata->trace_thread = NULL;
	}
}

static int daemon_iowait(void *d)
{
	char iowait_file[FILENAME_SIZE] = {0};
	char iowait_info[IOWAIT_INFO_SIZE] = {0};
	size_t len = (IOWAIT_INFO_SIZE - IOWAIT_TIME_SIZE) * sizeof(char) - 1;
	int open_iowait_file = 0;
	unsigned int us = 0;
	unsigned long long time = 0;
	struct file *filp_stat = NULL;
	struct file *filp_iowait = NULL;
	mm_segment_t oldfs_iowait;
	loff_t pos_stat = 0;
	loff_t pos_iowait = 0;

	snprintf(iowait_file, sizeof(iowait_file), "%s/%s", saving_path,
			IOWAIT_FILE_NAME);
	wait_for_system_ready();

	while (!kthread_should_stop()) {
		if (!open_iowait_file) {
			filp_iowait = filp_open(iowait_file, O_WRONLY |
					O_CREAT | O_TRUNC, S_IRUGO | S_IWUSR);
			if (IS_ERR(filp_iowait)) {
				msleep(100);
				continue;
			}
			open_iowait_file = 1;
		}

		filp_stat = filp_open("/proc/stat", O_RDONLY, 0);
		if (!IS_ERR(filp_stat)) {
			/* get current time */
			time = GET_TIME();
			us = do_div(time, 1000000000) / 1000;
			snprintf(iowait_info, sizeof(iowait_info),
					"[ %5d.%06d ]\n", (u32)time, us);

			/* get iowait info */
			oldfs_iowait = get_fs();
			set_fs(KERNEL_DS);
			pos_stat = 0;
			vfs_read(filp_stat, &iowait_info[IOWAIT_TIME_SIZE],
					len, &pos_stat);
			filp_close(filp_stat, NULL);

			/* write time and iowait info */
			iowait_info[IOWAIT_INFO_SIZE - 1] = '\n';
			vfs_write(filp_iowait, (char *)iowait_info,
					sizeof(iowait_info), &pos_iowait);
			set_fs(oldfs_iowait);
		}

		msleep(iowait_interval);
	}

	if (open_iowait_file)
		filp_close(filp_iowait, NULL);
	else
		LCTR_PR_ERR("fail to open %s\n", iowait_file);

	return 0;
}

static int create_daemon_iowait(void)
{
	struct trace_data *pdata = &trace_datas;

	pdata->iowait_thread = kthread_run(daemon_iowait, NULL,
			"lct_iowait_d");
	if (IS_ERR(pdata->iowait_thread)) {
		LCTR_PR_ERR("fail to create lct_iowait_d\n");
		return -ENOMEM;
	}

	return 0;
}

static void kill_daemon_iowait(void)
{
	struct trace_data *pdata = &trace_datas;

	if (pdata->iowait_thread) {
		kthread_stop(pdata->iowait_thread);
		pdata->iowait_thread = NULL;
	}
}

static void __add_trace_entry(struct task_struct *prev,
		struct task_struct *next, unsigned long data,
		int mode, const char *message)
{
	int count;
	long state;
	unsigned long flags;
	struct trace_data *pdata = &trace_datas;
	struct snsc_lctracer_trace_entry *entry;

	if ((system_state != SYSTEM_RUNNING) || (!lctracer_is_running()))
		return;

	spin_lock_irqsave(&lock, flags);
	if (pdata->index == pdata->num_entry_buf) {
		pdata->overflow = 1;
		pdata->overflow_read = 1;
		pdata->index = 0;
	}
	entry = pdata->entries + pdata->index++;
	entry->time	= GET_TIME();
	entry->info	= 0;
	SET_ENTRY_INFO(entry->info, MODE, mode);
	SET_ENTRY_INFO(entry->info, POLICY, prev->policy);
	SET_ENTRY_INFO(entry->info, PRIORITY, prev->rt_priority);
	state = (prev->state & TASK_REPORT) |
		(prev->state & TASK_DEAD ? LCTRACER_STATE_TASK_DEAD : 0);
	SET_ENTRY_INFO(entry->info, STATE, state);
	SET_ENTRY_INFO(entry->info, CPUID, raw_smp_processor_id());
	entry->tgid	= prev->tgid;
	entry->ppid	= prev->pid;
	entry->npid	= next->pid;
	entry->data	= data;
	memcpy(entry->tname, prev->comm, sizeof(entry->tname));
	entry->tname[THREAD_NAME_SIZE - 1] = 0;

	if (message) {
		count = strlen(message);
		count = (sizeof(entry->message) - 1) > count ?
			count : (sizeof(entry->message) - 1);
		memcpy(entry->message, message, count);
		entry->message[count] = 0;
	}
	if (filedump) {
		if (++pdata->threshold_cnt >= pdata->num_entry_threshold) {
			pdata->threshold_cnt = 0;
			pdata->reach_threshold++;
		}
	}
	spin_unlock_irqrestore(&lock, flags);
}

void snsc_lctracer_add_trace_entry(struct task_struct *prev,
		struct task_struct *next, unsigned long data)
{
	__add_trace_entry(prev, next, data,
			((prev == next) ?
			 SNSC_LCTRACER_IRQ_MODE : SNSC_LCTRACER_THREAD_MODE),
			NULL);
}

static void cmd_option_show(void)
{
	LCTR_PR_INFO("LCTracer /proc/snsc_lctracer/cmd parameter usage:\n"
			" - start: start measurement\n"
			" - stop : stop measurement\n");
}

static void store_measurement_env_info(void)
{
	char header_info[HEADER_INFO_SIZE] = {0};
	char header_file[FILENAME_SIZE] = {0};
	struct file *filp = NULL;
	struct file *filp_header = NULL;
	mm_segment_t oldfs_header;
	loff_t pos = 0;
	loff_t pos_header = 0;
	int cpu_nr = 1;

	snprintf(header_file, sizeof(header_file), "%s/%s", saving_path,
		 HEADER_FILE_NAME);
	filp_header = filp_open(header_file, O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUGO | S_IWUSR);
	if (IS_ERR(filp_header)) {
		LCTR_PR_ERR("fail to open %s\n", header_file);
		return;
	}
	oldfs_header = get_fs();
	set_fs(KERNEL_DS);

	/* record lctracer version */
	snprintf(header_info, sizeof(header_info), "LCTracer version: %d.%d\n",
			LCTRACER_VERSION_MAJOR, LCTRACER_VERSION_MINOR);
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);

	/* record endian info */
	snprintf(header_info, sizeof(header_info), "Endian : %s\n", STR_ENDIAN);
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);

	/* record HZ info */
	snprintf(header_info, sizeof(header_info), "HZ : %d\n", HZ);
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);
	/* record IPI and LOC IRQ info */
	snprintf(header_info, sizeof(header_info),
			"SNSC_LCTRACER_IPI_IRQ : %d\n"
			"SNSC_LCTRACER_LOC_IRQ : %d\n",
			SNSC_LCTRACER_IPI_IRQ, SNSC_LCTRACER_LOC_IRQ);
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);


	/* record kernel version */
	filp = filp_open("/proc/version", O_RDONLY, 0);
	pos = 0;
	vfs_read(filp, &header_info[0], sizeof(header_info), &pos);
	filp_close(filp, NULL);
	header_info[strlen(header_info) - 1] = '\n';
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);

	/* record cpu number */
#ifdef CONFIG_SMP
	if (setup_max_cpus == 0)
		cpu_nr = 1;
	else
		cpu_nr = (setup_max_cpus > CONFIG_NR_CPUS) ? CONFIG_NR_CPUS :
			setup_max_cpus;
#else
	cpu_nr = CONFIG_NR_CPUS;
#endif
	snprintf(header_info, sizeof(header_info), "CPU number : %d\n",
			cpu_nr);
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);

	/* record cpu info */
	filp = filp_open("/proc/cpuinfo", O_RDONLY, 0);
	pos = 0;
	vfs_read(filp, &header_info[0], sizeof(header_info), &pos);
	filp_close(filp, NULL);
	header_info[strlen(header_info) - 1] = '\n';
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);

	/* record LCTracer parameteres state */
	snprintf(header_info, sizeof(header_info), STATE_FORMAT);
	vfs_write(filp_header, (char *)header_info, strlen(header_info),
			&pos_header);
	filp_close(filp_header, NULL);
	set_fs(oldfs_header);
}

static void store_irq_desc_table(void)
{
	char tmp_buf[IRQ_DESC_BUF_SIZE] = {0};
	char irq_desc_file[FILENAME_SIZE] = {0};
	struct file *filp = NULL;
	struct file *filp_irq_desc = NULL;
	mm_segment_t oldfs_irq_desc;
	loff_t pos = 0;
	loff_t pos_irq_desc = 0;
	ssize_t len;

	oldfs_irq_desc = get_fs();
	set_fs(KERNEL_DS);

	snprintf(irq_desc_file, sizeof(irq_desc_file), "%s/%s", saving_path,
		IRQ_DESC_FILE_NAME);
	filp_irq_desc = filp_open(irq_desc_file, O_WRONLY | O_CREAT | O_TRUNC,
		S_IRUGO | S_IWUSR);
	if (IS_ERR(filp_irq_desc)) {
		LCTR_PR_ERR("fail to open %s\n", irq_desc_file);
		return;
	}
	filp = filp_open("/proc/interrupts", O_RDONLY, 0);
	if (IS_ERR(filp)) {
		LCTR_PR_ERR("fail to open /proc/interrupts\n");
		return;
	}

	while ((len = vfs_read(filp, tmp_buf, sizeof(tmp_buf), &pos)) > 0)
		vfs_write(filp_irq_desc, tmp_buf, len, &pos_irq_desc);

	filp_close(filp, NULL);
	filp_close(filp_irq_desc, NULL);

	set_fs(oldfs_irq_desc);
}

void snsc_lctracer_add_user_entry(const char *buffer)
{
	__add_trace_entry(current, current, 0, SNSC_LCTRACER_USER_MODE, buffer);
}
EXPORT_SYMBOL(snsc_lctracer_add_user_entry);

static void start_measurement(void)
{
	struct trace_data *pdata = &trace_datas;

	if (lctracer_is_running()) {
		LCTR_PR_WARNING("measurement is already started\n");
		goto out;
	}

	if (module_param_check())
		goto out;
	if (trace_state_init())
		goto out;
	if (old_buf_size != buf_size) {
		buffer_free();
		if (buffer_init())
			goto out;
	}
	if (filedump) {
		if (create_daemon_trace_data())
			goto out;
	}
	if (create_daemon_iowait())
		goto out;

	LCTR_PR_INFO("start measurement\n");
	pdata->trace_state = START_TRACE;
out:
	return;
}

void snsc_lctracer_start(void)
{
	start_measurement();
}
EXPORT_SYMBOL(snsc_lctracer_start);

static void stop_measurement(void)
{
	struct trace_data *pdata = &trace_datas;

	if (!lctracer_is_running()) {
		LCTR_PR_WARNING("measurement is already stopped\n");
			goto out;
	}
	pdata->trace_state = STOP_TRACE;

	/* make sure tracer get to know trace is stopped */
	msleep(10);

	/* only saved in memory */
	if (!filedump) {
		pdata->dump_start_index = 0;
		if (pdata->overflow)
			pdata->dump_end_index = pdata->num_entry_buf;
		else
			pdata->dump_end_index = pdata->index;

		convert_wchan_info(pdata->dump_start_index,
				pdata->dump_end_index);
	}

	kill_daemon_trace_data();
	kill_daemon_iowait();
	store_measurement_env_info();
	store_irq_desc_table();
	LCTR_PR_INFO("stop measurement\n");
out:
	return;
}

void snsc_lctracer_stop(void)
{
	stop_measurement();
}
EXPORT_SYMBOL(snsc_lctracer_stop);

static void process_cmd(const int cmd)
{
	/* start measurement */
	if (cmd == 1)
		start_measurement();
	/* stop measurement */
	else if (cmd == 0)
		stop_measurement();
	/* invalid input */
	else {
		LCTR_PR_WARNING("input parameter is not valid\n");
		cmd_option_show();
	}
}

static int proc_write_cmd(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char ubuf[PROC_CMD_SIZE] = {0};
	size_t cmd_count = count;
	int cmd = -1;

	if (count > (PROC_CMD_SIZE - 1)) {
		cmd_count = PROC_CMD_SIZE - 1;
		ubuf[cmd_count] = 0;
	}
	if (copy_from_user(ubuf, buffer, cmd_count))
		goto out;

	if (ubuf[strlen(ubuf) - 1] == '\n')
		ubuf[strlen(ubuf) - 1] = 0;

	/* process input command */
	if (!strcmp(ubuf, "start"))
		cmd = 1;
	else if (!strcmp(ubuf, "stop"))
		cmd = 0;
	else {
		LCTR_PR_WARNING("input parameter is not valid\n");
		cmd_option_show();
		goto out;
	}
	process_cmd((const int)cmd);
out:
	return count;
}

static ssize_t proc_read_state(struct file *file, char __user *buf, size_t len,
		loff_t *offset)
{
	char state_info[STATE_INFO_SIZE] = {0};
	const void *start = &state_info[0];
	size_t size = 0;

	snprintf(state_info, sizeof(state_info), STATE_FORMAT);
	size = strlen(state_info);

	return simple_read_from_buffer(buf, len, offset, start, size);
}

static ssize_t proc_read_data(struct file *file, char __user *buf, size_t len,
		loff_t *offset)
{
	ssize_t cnt = 0;
	size_t size = 0;
	size_t entry_size = sizeof(struct snsc_lctracer_trace_entry);
	const void *start = NULL;
	struct trace_data *pdata = &trace_datas;

	if (lctracer_is_running()) {
		LCTR_PR_WARNING("please stop measurement at first\n");
		return 0;
	}

	if (pdata->overflow_read) {
		start = (const void *)(pdata->entries + pdata->index);
		size = (pdata->num_entry_buf - pdata->index) * entry_size;
	} else {
		start = (const void *)pdata->entries;
		size = pdata->index * entry_size;
	}

	cnt = simple_read_from_buffer(buf, len, offset, start, size);
	if ((*offset == size) && (pdata->overflow_read)) {
		*offset = 0;
		pdata->overflow_read = 0;
	}
	if ((!cnt) && (pdata->overflow))
		pdata->overflow_read = 1;

	return cnt;
}

static int proc_write_user(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char ubuf[PROC_USER_SIZE] = {0};
	size_t user_count = count;

	if (count > (sizeof(ubuf) - 1))
		user_count = sizeof(ubuf) - 1;

	if (copy_from_user(ubuf, buffer, user_count))
		goto out;

	if (ubuf[strlen(ubuf) - 1] == '\n')
		ubuf[strlen(ubuf) - 1] = 0;

	snsc_lctracer_add_user_entry(ubuf);
out:
	return count;
}

static void *lctracer_seq_start(struct seq_file *m, loff_t *pos)
{
	struct trace_data *pdata = &trace_datas;
	int offset = *pos;

	if (lctracer_is_running()) {
		LCTR_PR_WARNING("please stop measurement at first\n");
		return NULL;
	}

	/* set last_index at the first time */
	if (!offset) {
		if ((pdata->index == 0) && (!pdata->overflow))
			return NULL;

		pdata->last_index = pdata->index;
		if (pdata->overflow)
			pdata->last_index += pdata->num_entry_buf;
	}

	/* if overflow happens, offset is based on pdata->index */
	if (pdata->overflow)
		offset += pdata->index;

	if (offset >= pdata->last_index)
		return NULL;

	return (void *)(offset + 1);
}

static void *lctracer_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	int offset = (int)v;
	struct trace_data *pdata = &trace_datas;

	(*pos)++;
	offset++;

	if (offset <= pdata->last_index)
		return (void *)(offset);
	else
		return NULL;
}

static void lctracer_seq_stop(struct seq_file *m, void *v)
{
}

static void convert_state_to_str(int state, char *str)
{
	switch (state) {
	case TASK_RUNNING:
		snprintf(str, STATE_STR_SIZE, "%s", "TASK_RUNNING");
		break;
	case TASK_INTERRUPTIBLE:
		snprintf(str, STATE_STR_SIZE, "%s", "TASK_INTERRUPTIBLE");
		break;
	case TASK_UNINTERRUPTIBLE:
		snprintf(str, STATE_STR_SIZE, "%s", "TASK_UNINTERRUPTIBLE");
		break;
	case TASK_STOPPED:
		snprintf(str, STATE_STR_SIZE, "%s", "TASK_STOPPED");
		break;
	case TASK_TRACED:
		snprintf(str, STATE_STR_SIZE, "%s", "TASK_TRACED");
		break;
	case LCTRACER_STATE_TASK_DEAD:
		snprintf(str, STATE_STR_SIZE, "%s", "TASK_DEAD");
		break;
	default:
		snprintf(str, STATE_STR_SIZE, "UNKNOW_TYPE(0x%x)", state);
	}
}

static void convert_policy_to_str(int policy, char *str)
{
	switch (policy) {
	case SCHED_NORMAL:
		snprintf(str, POLICY_STR_SIZE, "%s", "NORMAL");
		break;
	case SCHED_FIFO:
		snprintf(str, POLICY_STR_SIZE, "%s", "FIFO");
		break;
	case SCHED_RR:
		snprintf(str, POLICY_STR_SIZE, "%s", "R.R.");
		break;
	default:
		snprintf(str, POLICY_STR_SIZE, "\"%d\"", policy);
		break;
	}
}

typedef int (*print_func)(struct seq_file *, const char *, ...);

#ifdef CONFIG_MAGIC_SYSRQ
static int sysrq_printf(struct seq_file *m, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vprintk(fmt, args);
	va_end(args);

	return ret;
}
#endif

static char *get_irq_str(char *str, size_t size, int num)
{
	switch (num) {
	case SNSC_LCTRACER_IPI_IRQ:
		return "IPI";
	case SNSC_LCTRACER_LOC_IRQ:
		return "LOC";
	default:
		snprintf(str, size, "%d", num);
		return str;
	}
}


static int dump_text_entry(struct seq_file *m, int i, print_func print)
{
	struct trace_data *pdata = &trace_datas;
	struct snsc_lctracer_trace_entry *entry;
	u64 time;
	u32 sec;
	u32 us;
	int state;
	char state_str[STATE_STR_SIZE] = {0};
	char irq_str[ENTRY_DATA_IRQ_NUM_SIZE] = {0};
	int policy;
	char policy_str[POLICY_STR_SIZE] = {0};
	entry = pdata->entries + (i % pdata->num_entry_buf);
	time = entry->time;
	us = do_div(time, NSEC_PER_SEC) / NSEC_PER_USEC;
	sec = (u32)time;

	/* user entry */
	if (GET_ENTRY_INFO(MODE, entry->info) == SNSC_LCTRACER_USER_MODE) {
		(*print)(m, "[ %5d.%06d ] -cpu%d -usr current:%d log:%s\n",
				sec, us,
				GET_ENTRY_INFO(CPUID, entry->info),
				entry->npid,
				entry->message);
	}
	/* IRQ entry */
	else if (GET_ENTRY_INFO(MODE, entry->info) == SNSC_LCTRACER_IRQ_MODE) {
		(*print)(m, "[ %5d.%06d ] -cpu%d -int irq:%d exe:%d\n",
				sec, us,
				GET_ENTRY_INFO(CPUID, entry->info),
				(entry->data & ENTRY_DATA_IRQ_NUM_MASK),

				get_irq_str(irq_str, sizeof(irq_str),
					(entry->data &
					ENTRY_DATA_IRQ_NUM_MASK)),
				(entry->data >> ENTRY_DATA_IRQ_TIME_OFFSET));
	}
	/* thread entry */
	else {
		state = GET_ENTRY_INFO(STATE, entry->info);
		convert_state_to_str(state, state_str);
		policy = GET_ENTRY_INFO(POLICY, entry->info);
		convert_policy_to_str(policy, policy_str);
		(*print)(m, "[ %5d.%06d ] -cpu%d -ctx prev:%d:%d -> next:%d "
				"pstate:%s pwchan:%s%s%x(%x) ptask:%s "
				"pprio:%d ppolicy:%s\n",
				sec, us,
				GET_ENTRY_INFO(CPUID, entry->info),
				entry->ppid,
				entry->tgid,
				entry->npid,
				state_str,
				state ? entry->message : "",
				state ? "+" : "",
				entry->data,
				GET_ENTRY_INFO(WCHAN_OST, entry->info),
				entry->tname,
				GET_ENTRY_INFO(PRIORITY, entry->info),
				policy_str);
	}

	return 0;
}

static int lctracer_seq_show(struct seq_file *m, void *v)
{
	int offset = (int)v;

	dump_text_entry(m, offset - 1, seq_printf);

	return 0;
}

static const struct seq_operations lctracer_seq_op = {
	.start = lctracer_seq_start,
	.next = lctracer_seq_next,
	.stop = lctracer_seq_stop,
	.show = lctracer_seq_show
};

static int proc_text_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &lctracer_seq_op);
}

static const struct file_operations lctracer_op_text = {
	.owner = THIS_MODULE,
	.open = proc_text_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

#ifdef CONFIG_MAGIC_SYSRQ
static void sysrq_task(struct work_struct *work)
{
	struct trace_data *pdata = &trace_datas;
	int start = 0;
	int end = 0;
	int i = 0;
	/* Record previous state for autostart after text data is dump,
	 * only if filedump is disabled */
	int prev_trace_state = STOP_TRACE;

	if (pdata->overflow) {
		start = pdata->index;
		end = pdata->index + pdata->num_entry_buf;
	} else
		end = pdata->index;

	if (lctracer_is_running()) {
		stop_measurement();
		if (!filedump)
			prev_trace_state = START_TRACE;
	}

	for (i = start; i < end; i++)
		dump_text_entry(NULL, i, sysrq_printf);

	if (prev_trace_state == START_TRACE)
		start_measurement();
}

static void sysrq_handle_dump_text(int key)
{
	struct trace_data *pdata = &trace_datas;

	schedule_work(&pdata->lct_sysrq_work);
}

static struct sysrq_key_op sysrq_dump_text_op = {
	.handler	= sysrq_handle_dump_text,
	.help_msg	= "LCTracer-dump-text-data(X)",
	.action_msg	= "LCTR: dump text data",
};
#endif

static const struct file_operations lctracer_op_cmd = {
	.owner = THIS_MODULE,
	.write = proc_write_cmd,
};

static const struct file_operations lctracer_op_state = {
	.owner = THIS_MODULE,
	.read = proc_read_state,
};

static const struct file_operations lctracer_op_dump = {
	.owner = THIS_MODULE,
	.read = proc_read_data,
};

static const struct file_operations lctracer_op_user = {
	.owner = THIS_MODULE,
	.write = proc_write_user,
};

static int create_proc_interface(void)
{
	int ret = 0;
	struct proc_dir_entry *entry;

	entry = proc_mkdir("snsc_lctracer", NULL);
	if (!entry) {
		LCTR_PR_ERR("fail to create /proc/snsc_lctracer\n");
		ret = -ENOMEM;
		goto out;
	}

	entry = proc_create("snsc_lctracer/cmd", S_IRUSR | S_IWUSR,
			NULL, &lctracer_op_cmd);
	if (!entry) {
		LCTR_PR_ERR("fail to create /proc/snsc_lctracer/cmd\n");
		ret = -ENOMEM;
		goto out;
	}

	entry = proc_create("snsc_lctracer/state", S_IRUSR | S_IWUSR,
			NULL, &lctracer_op_state);
	if (!entry) {
		LCTR_PR_ERR("fail to create /proc/snsc_lctracer/state\n");
		ret = -ENOMEM;
		goto out;
	}

	entry = proc_create("snsc_lctracer/dump", S_IRUSR | S_IWUSR,
			NULL, &lctracer_op_dump);
	if (!entry) {
		LCTR_PR_ERR("fail to create /proc/snsc_lctracer/dump\n");
		ret = -ENOMEM;
		goto out;
	}

	entry = proc_symlink("parameters", entry, LCTRACER_PARAM_PATH);
	if (!entry) {
		LCTR_PR_ERR("fail to create a link to %s\n",
			LCTRACER_PARAM_PATH);
		ret = -ENOMEM;
		goto out;
	}

	entry = proc_create("snsc_lctracer/text", S_IRUSR | S_IWUSR,
			NULL, &lctracer_op_text);
	if (!entry) {
		LCTR_PR_ERR("fail to create /proc/snsc_lctracer/text\n");
		ret = -ENOMEM;
		goto out;
	}

	entry = proc_create("snsc_lctracer/user", S_IRUSR | S_IWUSR,
			NULL, &lctracer_op_user);
	if (!entry) {
		LCTR_PR_ERR("fail to create /proc/snsc_lctracer/user\n");
		ret = -ENOMEM;
		goto out;
	}
out:
	return ret;
}

static void __exit lctracer_exit(void)
{
	remove_proc_entry("snsc_lctracer/cmd", NULL);
	remove_proc_entry("snsc_lctracer/state", NULL);
	remove_proc_entry("snsc_lctracer/dump", NULL);
	remove_proc_entry("snsc_lctracer/user", NULL);
	remove_proc_entry("snsc_lctracer", NULL);
	buffer_free();
	kill_daemon_trace_data();
	kill_daemon_iowait();
}

static int __init lctracer_init(void)
{
	int ret = 0;
	struct trace_data *pdata = &trace_datas;

	ret = create_proc_interface();
	if (ret)
		goto out;

	/* Check module parameters of cmdline */
	ret = module_param_check();
	if (ret)
		goto out;

	/* Initialize counters, index and so on */
	ret = trace_state_init();
	if (ret)
		goto out;

#ifdef CONFIG_MAGIC_SYSRQ
	/* Register sysrq key to dump text data */
	ret = register_sysrq_key('x', &sysrq_dump_text_op);
	if (ret)
		goto out;

	/* Initialize SysRq work */
	INIT_WORK(&pdata->lct_sysrq_work, sysrq_task);
#endif

	/* If autostart is enabled, allocate memory and create daemons */
	if (autostart) {
		ret = buffer_init();
		if (ret)
			goto out;

		ret = create_daemon_trace_data();
		if (ret)
			goto out;

		ret = create_daemon_iowait();
		if (ret)
			goto out;

		pdata->trace_state = START_TRACE;
	}

	LCTR_PR_INFO("Lite Context Tracer initilization\n");

	if (autostart)
		LCTR_PR_INFO("Lite Context Tracer auto-starts measurment\n");
out:
	return ret;
}

module_init(lctracer_init);
module_exit(lctracer_exit);
