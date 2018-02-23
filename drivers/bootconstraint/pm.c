// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <linux/pm_domain.h>

#include "core.h"

int constraint_pm_add(struct constraint *constraint, void *data)
{
	struct device *dev = constraint->cdev->dev;

	return dev_pm_domain_attach(dev, true);
}

void constraint_pm_remove(struct constraint *constraint)
{
	/* Nothing to do for now */
}
