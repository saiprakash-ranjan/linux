/*
 * arch/arm/mach-tegra/tegra_simon.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/hrtimer.h>
#include <linux/thermal.h>
#include <linux/regulator/consumer.h>

#include "tegra_simon.h"

static DEFINE_MUTEX(simon_lock);
static RAW_NOTIFIER_HEAD(simon_nh);
static void tegra_simon_grade_update(struct work_struct *work);

static u32 grading_sec = TEGRA_SIMON_GRADING_INTERVAL_SEC;

static struct tegra_simon_grader simon_graders[TEGRA_SIMON_DOMAIN_NUM] = {
	[TEGRA_SIMON_DOMAIN_CPU] = {
		.domain_name = "cpu",
		.domain = TEGRA_SIMON_DOMAIN_CPU,
	},
	[TEGRA_SIMON_DOMAIN_GPU] = {
		.domain_name = "gpu",
		.domain = TEGRA_SIMON_DOMAIN_GPU,
	},
};

/*
 * GPU grading is implemented within vdd_gpu post-change notification chain that
 * guarantees constant voltage during grading. First grading after boot can be
 * executed anytime set voltage is below specified threshold, next grading is
 * always separated by the grading interval from the last successful grading.
 */
static int tegra_simon_gpu_grading_cb(
	struct notifier_block *nb, unsigned long event, void *v)
{
	int mv = (int)((long)v);
	struct tegra_simon_grader *grader = container_of(
		nb, struct tegra_simon_grader, grading_condition_nb);
	ktime_t now = ktime_get();
	unsigned long t;
	int grade = 0;

	if (!(event & REGULATOR_EVENT_OUT_POSTCHANGE))
		return NOTIFY_DONE;

	mv = (mv > 0) ? mv / 1000 : mv;
	if ((mv <= 0) || (mv > grader->grading_mv_limit))
		return NOTIFY_OK;

	if (grader->last_grading.tv64 &&
	    (ktime_to_ms(ktime_sub(now, grader->last_grading)) <
	     (s64)grading_sec * 1000))
		return NOTIFY_OK;

	if (thermal_zone_get_temp(grader->tzd, &t)) {
		pr_err("%s: Failed to get %s temperature\n",
		       __func__, grader->domain_name);
		return NOTIFY_OK;
	}

	if (grader->grade_simon_domain) {
		grade = grader->grade_simon_domain(grader->domain, mv, t);
		if (grade < 0) {
			pr_err("%s: Failed to grade %s\n",
			       __func__, grader->domain_name);
			return NOTIFY_OK;
		}
	}

	grader->last_grading = now;
	if (grader->grade != grade) {
		set_mb(grader->grade, grade);
		schedule_work(&grader->grade_update_work);
	}
	pr_info("%s: graded %s: v = %d, t = %lu, grade = %d\n",
		__func__, grader->domain_name, mv, t, grade);
	return NOTIFY_OK;
}

static int __init tegra_simon_init_gpu(void)
{
	struct tegra_simon_grader *grader =
		&simon_graders[TEGRA_SIMON_DOMAIN_GPU];
	struct regulator *reg;

	INIT_WORK(&grader->grade_update_work, tegra_simon_grade_update);

	grader->tzd = thermal_zone_device_find_by_name("GPU-therm");
	if (!grader->tzd) {
		pr_err("%s: Failed to find %s thermal zone\n",
		       __func__, grader->domain_name);
		return -ENOENT;
	}

	reg = regulator_get(NULL, "vdd_gpu_simon");
	if (IS_ERR(reg)) {
		pr_err("%s: Failed to get vdd_%s regulator\n",
		       __func__, grader->domain_name);
		return PTR_ERR(reg);
	}

	grader->grading_condition_nb.notifier_call = tegra_simon_gpu_grading_cb;
	regulator_register_notifier(reg, &grader->grading_condition_nb);
	regulator_put(reg);

	/* FIXME: settings below are tegar12_ specific */
	grader->grading_mv_limit = 850;
	grader->grade_simon_domain = NULL;

	return 0;
}

static int __init tegra_simon_init_cpu(void)
{
	struct tegra_simon_grader *grader =
		&simon_graders[TEGRA_SIMON_DOMAIN_CPU];

	INIT_WORK(&grader->grade_update_work, tegra_simon_grade_update);

	return 0;
}

int tegra_register_simon_notifier(struct notifier_block *nb)
{
	int ret;

	mutex_lock(&simon_lock);
	ret = raw_notifier_chain_register(&simon_nh, nb);
	mutex_unlock(&simon_lock);
	return ret;
}

void tegra_unregister_simon_notifier(struct notifier_block *nb)
{
	mutex_lock(&simon_lock);
	raw_notifier_chain_unregister(&simon_nh, nb);
	mutex_unlock(&simon_lock);
}

static void tegra_simon_grade_update(struct work_struct *work)
{
	struct tegra_simon_grader *grader = container_of(
		work, struct tegra_simon_grader, grade_update_work);

	mutex_lock(&simon_lock);
	raw_notifier_call_chain(&simon_nh, grader->grade,
				(void *)((long)grader->domain));
	mutex_unlock(&simon_lock);
}

#ifdef CONFIG_DEBUG_FS

static int grade_get(void *data, u64 *val)
{
	struct tegra_simon_grader *grader = data;

	if (grader->domain >= TEGRA_SIMON_DOMAIN_NUM) {
		*val = -EINVAL;
		return -EINVAL;
	}

	*val = grader->grade;
	return 0;
}
static int grade_set(void *data, u64 val)
{
	struct tegra_simon_grader *grader = data;

	if (grader->domain >= TEGRA_SIMON_DOMAIN_NUM)
		return -EINVAL;

	if (grader->grade != (int)val) {
		grader->grade = (int)val;
		schedule_work(&grader->grade_update_work);
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(grade_fops, grade_get, grade_set, "%llu\n");

static int __init simon_debugfs_init_domain(struct dentry *dir,
					    struct tegra_simon_grader *grader)
{
	struct dentry *d;

	d = debugfs_create_dir(grader->domain_name, dir);
	if (!d)
		return -ENOMEM;

	if (!debugfs_create_file("grade", S_IWUSR | S_IRUGO, d,
				 (void *)grader, &grade_fops))
		return -ENOMEM;

	return 0;
}

static int __init simon_debugfs_init(void)
{
	int i;
	struct tegra_simon_grader *grader;
	struct dentry *dir;

	dir = debugfs_create_dir("tegra_simon", NULL);
	if (!dir)
		return -ENOMEM;

	if (!debugfs_create_u32("grading_sec", S_IWUSR | S_IRUGO, dir,
				&grading_sec))
		goto err_out;

	for (i = 0; i < TEGRA_SIMON_DOMAIN_NUM; i++) {
		grader = &simon_graders[i];

		if (!grader->domain_name)
			continue;

		if (simon_debugfs_init_domain(dir, grader))
			goto err_out;
	}

	return 0;

err_out:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}
#endif

static int __init tegra_simon_init(void)
{
	tegra_simon_init_gpu();
	tegra_simon_init_cpu();

#ifdef CONFIG_DEBUG_FS
	simon_debugfs_init();
#endif
	return 0;

}
late_initcall_sync(tegra_simon_init);

