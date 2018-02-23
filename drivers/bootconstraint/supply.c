// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "core.h"

struct constraint_supply {
	struct dev_boot_constraint_supply_info supply;
	struct regulator *reg;
};

int constraint_supply_add(struct constraint *constraint, void *data)
{
	struct dev_boot_constraint_supply_info *supply = data;
	struct constraint_supply *csupply;
	struct device *dev = constraint->cdev->dev;
	int ret;

	csupply = kzalloc(sizeof(*csupply), GFP_KERNEL);
	if (!csupply)
		return -ENOMEM;

	csupply->reg = regulator_get(dev, supply->name);
	if (IS_ERR(csupply->reg)) {
		ret = PTR_ERR(csupply->reg);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "regulator_get() failed for %s (%d)\n",
				supply->name, ret);
		}
		goto free;
	}

	if (supply->u_volt_min != 0 && supply->u_volt_max != 0) {
		ret = regulator_set_voltage(csupply->reg, supply->u_volt_min,
					    supply->u_volt_max);
		if (ret) {
			dev_err(dev, "regulator_set_voltage %s failed (%d)\n",
				supply->name, ret);
			goto free_regulator;
		}
	}

	ret = regulator_enable(csupply->reg);
	if (ret) {
		dev_err(dev, "regulator_enable %s failed (%d)\n",
			supply->name, ret);
		goto remove_voltage;
	}

	memcpy(&csupply->supply, supply, sizeof(*supply));
	csupply->supply.name = kstrdup_const(supply->name, GFP_KERNEL);
	constraint->private = csupply;

	constraint_add_debugfs(constraint, supply->name);

	debugfs_create_u32("u_volt_min", 0444, constraint->dentry,
			   &csupply->supply.u_volt_min);

	debugfs_create_u32("u_volt_max", 0444, constraint->dentry,
			   &csupply->supply.u_volt_max);

	return 0;

remove_voltage:
	if (supply->u_volt_min != 0 && supply->u_volt_max != 0)
		regulator_set_voltage(csupply->reg, 0, INT_MAX);
free_regulator:
	regulator_put(csupply->reg);
free:
	kfree(csupply);

	return ret;
}

void constraint_supply_remove(struct constraint *constraint)
{
	struct constraint_supply *csupply = constraint->private;
	struct dev_boot_constraint_supply_info *supply = &csupply->supply;
	struct device *dev = constraint->cdev->dev;
	int ret;

	constraint_remove_debugfs(constraint);
	kfree_const(supply->name);

	ret = regulator_disable(csupply->reg);
	if (ret)
		dev_err(dev, "regulator_disable failed (%d)\n", ret);

	if (supply->u_volt_min != 0 && supply->u_volt_max != 0) {
		ret = regulator_set_voltage(csupply->reg, 0, INT_MAX);
		if (ret)
			dev_err(dev, "regulator_set_voltage failed (%d)\n",
				ret);
	}

	regulator_put(csupply->reg);
	kfree(csupply);
}
