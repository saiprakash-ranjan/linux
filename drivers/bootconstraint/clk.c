// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "core.h"

struct constraint_clk {
	struct dev_boot_constraint_clk_info clk_info;
	struct clk *clk;
};

int constraint_clk_add(struct constraint *constraint, void *data)
{
	struct dev_boot_constraint_clk_info *clk_info = data;
	struct constraint_clk *cclk;
	struct device *dev = constraint->cdev->dev;
	int ret;

	cclk = kzalloc(sizeof(*cclk), GFP_KERNEL);
	if (!cclk)
		return -ENOMEM;

	cclk->clk = clk_get(dev, clk_info->name);
	if (IS_ERR(cclk->clk)) {
		ret = PTR_ERR(cclk->clk);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "clk_get() failed for %s (%d)\n",
				clk_info->name, ret);
		}
		goto free;
	}

	ret = clk_prepare_enable(cclk->clk);
	if (ret) {
		dev_err(dev, "clk_prepare_enable() %s failed (%d)\n",
			clk_info->name, ret);
		goto put_clk;
	}

	cclk->clk_info.name = kstrdup_const(clk_info->name, GFP_KERNEL);
	constraint->private = cclk;

	return 0;

put_clk:
	clk_put(cclk->clk);
free:
	kfree(cclk);

	return ret;
}

void constraint_clk_remove(struct constraint *constraint)
{
	struct constraint_clk *cclk = constraint->private;

	kfree_const(cclk->clk_info.name);
	clk_disable_unprepare(cclk->clk);
	clk_put(cclk->clk);
	kfree(cclk);
}
