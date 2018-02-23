// SPDX-License-Identifier: GPL-2.0
/*
 * This takes care of boot time device constraints, normally set by the
 * Bootloader.
 *
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <linux/err.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "core.h"

#define for_each_constraint(_constraint, _temp, _cdev)		\
	list_for_each_entry_safe(_constraint, _temp, &_cdev->constraints, node)

/* Global list of all constraint devices currently registered */
static LIST_HEAD(constraint_devices);
static DEFINE_MUTEX(constraint_devices_mutex);

/* Debugfs */

static struct dentry *rootdir;

static void constraint_device_add_debugfs(struct constraint_dev *cdev)
{
	struct device *dev = cdev->dev;

	cdev->dentry = debugfs_create_dir(dev_name(dev), rootdir);
}

static void constraint_device_remove_debugfs(struct constraint_dev *cdev)
{
	debugfs_remove_recursive(cdev->dentry);
}

void constraint_add_debugfs(struct constraint *constraint, const char *suffix)
{
	struct device *dev = constraint->cdev->dev;
	const char *prefix;
	char name[NAME_MAX];

	switch (constraint->type) {
	case DEV_BOOT_CONSTRAINT_CLK:
		prefix = "clk";
		break;
	case DEV_BOOT_CONSTRAINT_PM:
		prefix = "pm";
		break;
	case DEV_BOOT_CONSTRAINT_SUPPLY:
		prefix = "supply";
		break;
	default:
		dev_err(dev, "%s: Constraint type (%d) not supported\n",
			__func__, constraint->type);
		return;
	}

	snprintf(name, NAME_MAX, "%s-%s", prefix, suffix);

	constraint->dentry = debugfs_create_dir(name, constraint->cdev->dentry);
}

void constraint_remove_debugfs(struct constraint *constraint)
{
	debugfs_remove_recursive(constraint->dentry);
}

static int __init constraint_debugfs_init(void)
{
	rootdir = debugfs_create_dir("boot_constraints", NULL);

	return 0;
}
core_initcall(constraint_debugfs_init);


/* Boot constraints core */

static struct constraint_dev *constraint_device_find(struct device *dev)
{
	struct constraint_dev *cdev;

	list_for_each_entry(cdev, &constraint_devices, node) {
		if (cdev->dev == dev)
			return cdev;
	}

	return NULL;
}

static struct constraint_dev *constraint_device_allocate(struct device *dev)
{
	struct constraint_dev *cdev;

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return ERR_PTR(-ENOMEM);

	cdev->dev = dev;
	INIT_LIST_HEAD(&cdev->node);
	INIT_LIST_HEAD(&cdev->constraints);

	list_add(&cdev->node, &constraint_devices);
	constraint_device_add_debugfs(cdev);

	return cdev;
}

static void constraint_device_free(struct constraint_dev *cdev)
{
	constraint_device_remove_debugfs(cdev);
	list_del(&cdev->node);
	kfree(cdev);
}

static struct constraint_dev *constraint_device_get(struct device *dev)
{
	struct constraint_dev *cdev;

	cdev = constraint_device_find(dev);
	if (cdev)
		return cdev;

	cdev = constraint_device_allocate(dev);
	if (IS_ERR(cdev)) {
		dev_err(dev, "Failed to add constraint dev (%ld)\n",
			PTR_ERR(cdev));
	}

	return cdev;
}

static void constraint_device_put(struct constraint_dev *cdev)
{
	if (!list_empty(&cdev->constraints))
		return;

	constraint_device_free(cdev);
}

static struct constraint *constraint_allocate(struct constraint_dev *cdev,
					enum dev_boot_constraint_type type)
{
	struct constraint *constraint;
	int (*add)(struct constraint *constraint, void *data);
	void (*remove)(struct constraint *constraint);

	switch (type) {
	case DEV_BOOT_CONSTRAINT_CLK:
		add = constraint_clk_add;
		remove = constraint_clk_remove;
		break;
	case DEV_BOOT_CONSTRAINT_PM:
		add = constraint_pm_add;
		remove = constraint_pm_remove;
		break;
	case DEV_BOOT_CONSTRAINT_SUPPLY:
		add = constraint_supply_add;
		remove = constraint_supply_remove;
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	constraint = kzalloc(sizeof(*constraint), GFP_KERNEL);
	if (!constraint)
		return ERR_PTR(-ENOMEM);

	constraint->cdev = cdev;
	constraint->type = type;
	constraint->add = add;
	constraint->remove = remove;
	INIT_LIST_HEAD(&constraint->node);

	list_add(&constraint->node, &cdev->constraints);

	return constraint;
}

static void constraint_free(struct constraint *constraint)
{
	list_del(&constraint->node);
	kfree(constraint);
}

/**
 * dev_boot_constraint_add: Adds a boot constraint.
 *
 * @dev: Device for which the boot constraint is getting added.
 * @info: Structure representing the boot constraint.
 *
 * This routine adds a single boot constraint for the device. This must be
 * called before the device is probed by its driver, otherwise the boot
 * constraint will never get removed and may result in unwanted behavior of the
 * hardware. The boot constraint is removed by the driver core automatically
 * after the device is probed (successfully or unsuccessfully).
 *
 * Return: 0 on success, and a negative error otherwise.
 */
int dev_boot_constraint_add(struct device *dev,
			    struct dev_boot_constraint_info *info)
{
	struct constraint_dev *cdev;
	struct constraint *constraint;
	int ret;

	mutex_lock(&constraint_devices_mutex);

	/* Find or add the cdev type first */
	cdev = constraint_device_get(dev);
	if (IS_ERR(cdev)) {
		ret = PTR_ERR(cdev);
		goto unlock;
	}

	constraint = constraint_allocate(cdev, info->constraint.type);
	if (IS_ERR(constraint)) {
		dev_err(dev, "Failed to add constraint type: %d (%ld)\n",
			info->constraint.type, PTR_ERR(constraint));
		ret = PTR_ERR(constraint);
		goto put_cdev;
	}

	constraint->free_resources = info->free_resources;
	constraint->free_resources_data = info->free_resources_data;

	/* Set constraint */
	ret = constraint->add(constraint, info->constraint.data);
	if (ret)
		goto free_constraint;

	dev_dbg(dev, "Added boot constraint-type (%d)\n",
		info->constraint.type);

	mutex_unlock(&constraint_devices_mutex);

	return 0;

free_constraint:
	constraint_free(constraint);
put_cdev:
	constraint_device_put(cdev);
unlock:
	mutex_unlock(&constraint_devices_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_boot_constraint_add);

static void constraint_remove(struct constraint *constraint)
{
	constraint->remove(constraint);

	if (constraint->free_resources)
		constraint->free_resources(constraint->free_resources_data);

	constraint_free(constraint);
}

/**
 * dev_boot_constraints_remove: Removes all boot constraints of a device.
 *
 * @dev: Device for which the boot constraints are getting removed.
 *
 * This routine removes all the boot constraints that were previously added for
 * the device. This is called directly by the driver core and should not be
 * called by platform specific code.
 */
void dev_boot_constraints_remove(struct device *dev)
{
	struct constraint_dev *cdev;
	struct constraint *constraint, *temp;

	mutex_lock(&constraint_devices_mutex);

	cdev = constraint_device_find(dev);
	if (!cdev)
		goto unlock;

	for_each_constraint(constraint, temp, cdev)
		constraint_remove(constraint);

	constraint_device_put(cdev);
unlock:
	mutex_unlock(&constraint_devices_mutex);
}
