// SPDX-License-Identifier: GPL-2.0
/*
 * This sets up Dragonboard 410c constraints on behalf of the bootloader, which
 * uses display controller to display a flash screen during system boot.
 *
 * Copyright (c) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 * Rajendra Nayak <rnayak@codeaurora.org>
 */

#include <linux/boot_constraint.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>

static struct dev_boot_constraint_clk_info iface_clk_info = {
	.name = "iface_clk",
};

static struct dev_boot_constraint_clk_info bus_clk_info = {
	.name = "bus_clk",
};

static struct dev_boot_constraint_clk_info core_clk_info = {
	.name = "core_clk",
};

static struct dev_boot_constraint_clk_info vsync_clk_info = {
	.name = "vsync_clk",
};

static struct dev_boot_constraint_clk_info esc0_clk_info = {
	.name = "core_clk",
};

static struct dev_boot_constraint_clk_info byte_clk_info = {
	.name = "byte_clk",
};

static struct dev_boot_constraint_clk_info pixel_clk_info = {
	.name = "pixel_clk",
};

static struct dev_boot_constraint_supply_info vdda_info = {
	.name = "vdda"
};

static struct dev_boot_constraint_supply_info vddio_info = {
	.name = "vddio"
};

static struct dev_boot_constraint_clk_info uart_iface_clk_info = {
	.name = "iface",
};

static struct dev_boot_constraint constraints_mdss[] = {
	{
		.type = DEV_BOOT_CONSTRAINT_PM,
		.data = NULL,
	},
};

static struct dev_boot_constraint constraints_mdp[] = {
	{
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &iface_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &bus_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &core_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &vsync_clk_info,
	},
};

static struct dev_boot_constraint constraints_dsi[] = {
	{
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &esc0_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &byte_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &pixel_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_SUPPLY,
		.data = &vdda_info,

	}, {
		.type = DEV_BOOT_CONSTRAINT_SUPPLY,
		.data = &vddio_info,
	},
};

static struct dev_boot_constraint constraints_uart[] = {
	{
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &uart_iface_clk_info,
	},
};

static struct dev_boot_constraint_of constraints[] = {
	{
		.compat = "qcom,mdss",
		.constraints = constraints_mdss,
		.count = ARRAY_SIZE(constraints_mdss),
	}, {
		.compat = "qcom,mdp5",
		.constraints = constraints_mdp,
		.count = ARRAY_SIZE(constraints_mdp),
	}, {
		.compat = "qcom,mdss-dsi-ctrl",
		.constraints = constraints_dsi,
		.count = ARRAY_SIZE(constraints_dsi),
	}, {
		.compat = "qcom,msm-uartdm-v1.4",
		.constraints = constraints_uart,
		.count = ARRAY_SIZE(constraints_uart),
	},
};

static int __init qcom_constraints_init(void)
{
	/* Only Dragonboard 410c is supported for now */
	if (!of_machine_is_compatible("qcom,apq8016-sbc"))
		return 0;

	dev_boot_constraint_add_deferrable_of(constraints,
					      ARRAY_SIZE(constraints));

	return 0;
}
subsys_initcall(qcom_constraints_init);
