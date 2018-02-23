// SPDX-License-Identifier: GPL-2.0
/*
 * Boot constraints header.
 *
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */
#ifndef _LINUX_BOOT_CONSTRAINT_H
#define _LINUX_BOOT_CONSTRAINT_H

#include <linux/err.h>
#include <linux/types.h>

struct device;

/**
 * enum dev_boot_constraint_type - This defines different boot constraint types.
 *
 */
enum dev_boot_constraint_type {
	DEV_BOOT_CONSTRAINT_NONE,
};

/**
 * struct dev_boot_constraint - This represents a single boot constraint.
 *
 * @type: This is boot constraint type (like: clk, supply, etc.).
 * @data: This points to constraint type specific data (like:
 * dev_boot_constraint_clk_info).
 */
struct dev_boot_constraint {
	enum dev_boot_constraint_type type;
	void *data;
};

/**
 * struct dev_boot_constraint_info - This is used to add a single boot
 * constraint.
 *
 * @constraint: This represents a single boot constraint.
 * @free_resources: This callback is called by the boot constraint core after
 * the constraint is removed. This is an optional field.
 * @free_resources_data: This is data to be passed to free_resources() callback.
 * This is an optional field.
 */
struct dev_boot_constraint_info {
	struct dev_boot_constraint constraint;

	/* This will be called just before the constraint is removed */
	void (*free_resources)(void *data);
	void *free_resources_data;
};

#ifdef CONFIG_DEV_BOOT_CONSTRAINT
int dev_boot_constraint_add(struct device *dev,
			    struct dev_boot_constraint_info *info);
void dev_boot_constraints_remove(struct device *dev);
#else
static inline
int dev_boot_constraint_add(struct device *dev,
			    struct dev_boot_constraint_info *info)
{ return 0; }
static inline void dev_boot_constraints_remove(struct device *dev) {}
#endif /* CONFIG_DEV_BOOT_CONSTRAINT */

#endif /* _LINUX_BOOT_CONSTRAINT_H */
