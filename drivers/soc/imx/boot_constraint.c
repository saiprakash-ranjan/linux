// SPDX-License-Identifier: GPL-2.0
/*
 * This takes care of IMX boot time device constraints, normally set by the
 * Bootloader.
 *
 * Copyright (c) 2018 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <linux/boot_constraint.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>

static bool earlycon_boot_constraints_enabled __initdata;

static int __init enable_earlycon_boot_constraints(char *str)
{
	earlycon_boot_constraints_enabled = true;

	return 0;
}

__setup_param("earlycon", boot_constraint_earlycon,
	      enable_earlycon_boot_constraints, 0);
__setup_param("earlyprintk", boot_constraint_earlyprintk,
	      enable_earlycon_boot_constraints, 0);


struct imx_machine_constraints {
	struct dev_boot_constraint_of *dev_constraints;
	unsigned int count;
};

static struct dev_boot_constraint_clk_info uart_ipg_clk_info = {
	.name = "ipg",
};

static struct dev_boot_constraint_clk_info uart_per_clk_info = {
	.name = "per",
};

static struct dev_boot_constraint imx_uart_constraints[] = {
	{
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &uart_ipg_clk_info,
	}, {
		.type = DEV_BOOT_CONSTRAINT_CLK,
		.data = &uart_per_clk_info,
	},
};

static struct dev_boot_constraint_of imx_dev_constraints[] = {
	{
		.compat = "fsl,imx21-uart",
		.constraints = imx_uart_constraints,
		.count = ARRAY_SIZE(imx_uart_constraints),
	},
};

static struct imx_machine_constraints imx_constraints = {
	.dev_constraints = imx_dev_constraints,
	.count = ARRAY_SIZE(imx_dev_constraints),
};

/* imx7 */
static struct dev_boot_constraint_of imx7_dev_constraints[] = {
	{
		.compat = "fsl,imx6q-uart",
		.constraints = imx_uart_constraints,
		.count = ARRAY_SIZE(imx_uart_constraints),
	},
};

static struct imx_machine_constraints imx7_constraints = {
	.dev_constraints = imx7_dev_constraints,
	.count = ARRAY_SIZE(imx7_dev_constraints),
};

static const struct of_device_id machines[] __initconst = {
	{ .compatible = "fsl,imx25", .data = &imx_constraints },
	{ .compatible = "fsl,imx27", .data = &imx_constraints },
	{ .compatible = "fsl,imx31", .data = &imx_constraints },
	{ .compatible = "fsl,imx35", .data = &imx_constraints },
	{ .compatible = "fsl,imx50", .data = &imx_constraints },
	{ .compatible = "fsl,imx51", .data = &imx_constraints },
	{ .compatible = "fsl,imx53", .data = &imx_constraints },
	{ .compatible = "fsl,imx6dl", .data = &imx_constraints },
	{ .compatible = "fsl,imx6q", .data = &imx_constraints },
	{ .compatible = "fsl,imx6qp", .data = &imx_constraints },
	{ .compatible = "fsl,imx6sl", .data = &imx_constraints },
	{ .compatible = "fsl,imx6sx", .data = &imx_constraints },
	{ .compatible = "fsl,imx6ul", .data = &imx_constraints },
	{ .compatible = "fsl,imx6ull", .data = &imx_constraints },
	{ .compatible = "fsl,imx7d", .data = &imx7_constraints },
	{ .compatible = "fsl,imx7s", .data = &imx7_constraints },
	{ }
};

static int __init imx_constraints_init(void)
{
	const struct imx_machine_constraints *constraints;
	const struct of_device_id *match;
	struct device_node *np;

	if (!earlycon_boot_constraints_enabled)
		return 0;

	np = of_find_node_by_path("/");
	if (!np)
		return -ENODEV;

	match = of_match_node(machines, np);
	of_node_put(np);

	if (!match)
		return 0;

	constraints = match->data;

	dev_boot_constraint_add_deferrable_of(constraints->dev_constraints,
					      constraints->count);

	return 0;
}
subsys_initcall(imx_constraints_init);
