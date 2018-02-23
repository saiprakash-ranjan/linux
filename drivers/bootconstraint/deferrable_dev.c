// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <linux/err.h>
#include <linux/idr.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "core.h"

static DEFINE_IDA(pdev_index);

void driver_enable_deferred_probe(void);

struct boot_constraint_pdata {
	struct device *dev;
	struct dev_boot_constraint constraint;
	int probe_failed;
	int index;
};

static void boot_constraint_remove(void *data)
{
	struct platform_device *pdev = data;
	struct boot_constraint_pdata *pdata = dev_get_platdata(&pdev->dev);

	ida_simple_remove(&pdev_index, pdata->index);
	kfree(pdata->constraint.data);
	platform_device_unregister(pdev);
}

/*
 * A platform device is added for each and every constraint, to handle
 * -EPROBE_DEFER properly.
 */
static int boot_constraint_probe(struct platform_device *pdev)
{
	struct boot_constraint_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct dev_boot_constraint_info info;
	int ret;

	if (WARN_ON(!pdata))
		return -EINVAL;

	info.constraint = pdata->constraint;
	info.free_resources = boot_constraint_remove;
	info.free_resources_data = pdev;

	ret = dev_boot_constraint_add(pdata->dev, &info);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			driver_enable_deferred_probe();
		else
			pdata->probe_failed = ret;
	}

	return ret;
}

static struct platform_driver boot_constraint_driver = {
	.driver = {
		.name = "boot-constraints-dev",
	},
	.probe = boot_constraint_probe,
};

static int __init boot_constraint_init(void)
{
	return platform_driver_register(&boot_constraint_driver);
}
core_initcall(boot_constraint_init);

static int boot_constraint_add_dev(struct device *dev,
				   struct dev_boot_constraint *constraint)
{
	struct boot_constraint_pdata pdata = {
		.dev = dev,
		.constraint.type = constraint->type,
	};
	struct platform_device *pdev;
	struct boot_constraint_pdata *pdev_pdata;
	int size, ret;

	switch (constraint->type) {
	case DEV_BOOT_CONSTRAINT_CLK:
		size = sizeof(struct dev_boot_constraint_clk_info);
		break;
	case DEV_BOOT_CONSTRAINT_PM:
		size = 0;
		break;
	case DEV_BOOT_CONSTRAINT_SUPPLY:
		size = sizeof(struct dev_boot_constraint_supply_info);
		break;
	default:
		dev_err(dev, "%s: Constraint type (%d) not supported\n",
			__func__, constraint->type);
		return -EINVAL;
	}

	/* Will be freed from boot_constraint_remove() */
	pdata.constraint.data = kmemdup(constraint->data, size, GFP_KERNEL);
	if (!pdata.constraint.data)
		return -ENOMEM;

	ret = ida_simple_get(&pdev_index, 0, 256, GFP_KERNEL);
	if (ret < 0) {
		dev_err(dev, "failed to allocate index (%d)\n", ret);
		goto free;
	}

	pdata.index = ret;

	pdev = platform_device_register_data(NULL, "boot-constraints-dev", ret,
					     &pdata, sizeof(pdata));
	if (IS_ERR(pdev)) {
		dev_err(dev, "%s: Failed to create pdev (%ld)\n", __func__,
			PTR_ERR(pdev));
		ret = PTR_ERR(pdev);
		goto ida_remove;
	}

	/* Release resources if probe has failed */
	pdev_pdata = dev_get_platdata(&pdev->dev);
	if (pdev_pdata->probe_failed) {
		ret = pdev_pdata->probe_failed;
		goto remove_pdev;
	}

	return 0;

remove_pdev:
	platform_device_unregister(pdev);
ida_remove:
	ida_simple_remove(&pdev_index, pdata.index);
free:
	kfree(pdata.constraint.data);

	return ret;
}

static int dev_boot_constraint_add_deferrable(struct device *dev,
			struct dev_boot_constraint *constraints, int count)
{
	int ret, i;

	for (i = 0; i < count; i++) {
		ret = boot_constraint_add_dev(dev, &constraints[i]);
		if (ret)
			return ret;
	}

	return 0;
}

/* This only creates platform devices for now */
static void add_deferrable_of_single(struct device_node *np,
				     struct dev_boot_constraint *constraints,
				     int count)
{
	struct device *dev;
	int ret;

	if (!of_device_is_available(np))
		return;

	ret = of_platform_bus_create(np, NULL, NULL, NULL, false);
	if (ret)
		return;

	dev = of_find_any_device_by_node(np);
	if (!dev) {
		pr_err("Boot Constraints: Failed to find dev: %pOF\n", np);
		return;
	}

	ret = dev_boot_constraint_add_deferrable(dev, constraints, count);
	if (ret)
		dev_err(dev, "Failed to add boot constraint (%d)\n", ret);
}

/* Not all compatible device nodes may have boot constraints */
static bool node_has_boot_constraints(struct device_node *np,
				      struct dev_boot_constraint_of *oconst)
{
	int i;

	if (!oconst->dev_names)
		return true;

	for (i = 0; i < oconst->dev_names_count; i++) {
		if (!strcmp(oconst->dev_names[i], kbasename(np->full_name)))
			return true;
	}

	return false;
}

/**
 * dev_boot_constraint_add_deferrable_of: Adds all constraints for a platform.
 *
 * @oconst: This is an array of 'struct dev_boot_constraint_of', where each
 * entry of the array is used to add one or more boot constraints across one or
 * more devices having the same compatibility in the device tree.
 * @count: Size of the 'oconst' array.
 *
 * This helper routine provides an easy way to add all boot constraints for a
 * machine or platform. Just like dev_boot_constraint_add(), this must be called
 * before the devices (to which we want to add constraints) are probed by their
 * drivers, otherwise the boot constraint will never get removed for those
 * devices and may result in unwanted behavior of the hardware. The boot
 * constraints are removed by the driver core automatically after the devices
 * are probed (successfully or unsuccessfully).
 *
 * This adds the boot constraints in a deferrable way and the caller need not
 * worry about the availability of the resources required by the constraint.
 * This routine will return successfully and the constraint will be added by the
 * boot constraint core as soon as the resource is available at a later point in
 * time.
 */
void dev_boot_constraint_add_deferrable_of(struct dev_boot_constraint_of *oconst,
					   int count)
{
	struct device_node *np;
	int i;

	for (i = 0; i < count; i++) {
		for_each_compatible_node(np, NULL, oconst[i].compat) {
			if (!node_has_boot_constraints(np, &oconst[i]))
				continue;

			add_deferrable_of_single(np, oconst[i].constraints,
						 oconst[i].count);
		}
	}
}
EXPORT_SYMBOL_GPL(dev_boot_constraint_add_deferrable_of);
