// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */
#ifndef _CORE_H
#define _CORE_H

#include <linux/boot_constraint.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/list.h>

struct constraint_dev {
	struct device *dev;
	struct list_head node;
	struct list_head constraints;
	struct dentry *dentry;
};

struct constraint {
	struct constraint_dev *cdev;
	struct list_head node;
	enum dev_boot_constraint_type type;
	void (*free_resources)(void *data);
	void *free_resources_data;
	struct dentry *dentry;

	int (*add)(struct constraint *constraint, void *data);
	void (*remove)(struct constraint *constraint);
	void *private;
};

void constraint_add_debugfs(struct constraint *constraint, const char *suffix);
void constraint_remove_debugfs(struct constraint *constraint);

/* Forward declarations of constraint specific callbacks */
int constraint_clk_add(struct constraint *constraint, void *data);
void constraint_clk_remove(struct constraint *constraint);

int constraint_pm_add(struct constraint *constraint, void *data);
void constraint_pm_remove(struct constraint *constraint);

int constraint_supply_add(struct constraint *constraint, void *data);
void constraint_supply_remove(struct constraint *constraint);

#endif /* _CORE_H */
