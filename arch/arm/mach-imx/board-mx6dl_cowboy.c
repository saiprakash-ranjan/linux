/*
 * Copyright 2016 Sony Corporation
 *
 * Based on arch/arm/mach-imx/mach-imx6q.c
 *
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/can/platform/flexcan.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clocksource.h>
#include <linux/cpu.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/opp.h>
#include <linux/phy.h>
#include <linux/regmap.h>
#include <linux/micrel_phy.h>
#include <linux/marvell_phy.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/of_net.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>

#include <asm/smp_twd.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/system_misc.h>
#include <asm/pmu.h>

#include "common.h"
#include "cpuidle.h"
#include "hardware.h"
#include "pecorino-rev_0_1_iomux/iomux_config.h"
#include "devices-imx6q.h"

#define PECORINO_LED1		IMX_GPIO_NR(4, 20)
#define PECORINO_LED2		IMX_GPIO_NR(4, 21)
#define PECORINO_LED3		IMX_GPIO_NR(4, 22)
#define PECORINO_LED4		IMX_GPIO_NR(4, 23)
#define PECORINO_CL1		IMX_GPIO_NR(4, 24)
#define PECORINO_CL2		IMX_GPIO_NR(4, 25)
#define PECORINO_CL3		IMX_GPIO_NR(4, 26)
#define PECORINO_CL4		IMX_GPIO_NR(4, 27)
#define PECORINO_OUTPUT1	IMX_GPIO_NR(6, 0)
#define PECORINO_OUTPUT2	IMX_GPIO_NR(6, 1)
#define PECORINO_OUTPUT3	IMX_GPIO_NR(6, 2)
#define PECORINO_SHELL		IMX_GPIO_NR(6, 3)
#define PECORINO_RECOVER	IMX_GPIO_NR(6, 4)
#define PECORINO_INPUT3		IMX_GPIO_NR(6, 5)
#define PECORINO_PMIC_INT	IMX_GPIO_NR(7, 17)

#define TOP                     (1 << 6)
#define DP_EN                   (1 << 5)

static void cowboy_suspend_enter(void)
{
	/* suspend preparation */
}

static void cowboy_suspend_exit(void)
{
	/* resume restore */
	iomux_config();
	/*
	 * FIXME
	 * There is no definition of following 6 symbols
	 * - HW_IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT_WR
	 * - BF_IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT_DAISY_V
	 * - SD3_CLK_ALT0
	 * - HW_IOMUXC_USDHC4_CARD_CLK_IN_SELECT_INPUT_WR
	 * - BF_IOMUXC_USDHC4_CARD_CLK_IN_SELECT_INPUT_DAISY_V
	 * - SD4_CLK_ALT0
	 * So, we __raw_writel() directly as a work-around.
	 */
	writel(0x00000001, MX6Q_IO_ADDRESS(0x020E0934));
	writel(0x00000001, MX6Q_IO_ADDRESS(0x020E0938));
}

/* Fix me. Need to change legacy code to DT for external fpga */
static struct resource fpga_resources[] = {
	[0] = {
		.start	= EIM1_APB_BASE_ADDR,
		.end	= EIM1_APB_END_ADDR,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= EIM2_APB_BASE_ADDR,
		.end	= EIM2_APB_END_ADDR,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device fpga_data = {
	.name		= "external-fpga",
	.id		= -1,
	.resource	= fpga_resources,
	.num_resources	= ARRAY_SIZE(fpga_resources),
};

static const struct pm_platform_data mx6dl_cowboy_pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = cowboy_suspend_enter,
	.suspend_exit = cowboy_suspend_exit,
};

static void mmd_write_reg(struct phy_device *dev, int device, int reg, int val)
{
	phy_write(dev, 0x0d, device);
	phy_write(dev, 0x0e, reg);
	phy_write(dev, 0x0d, (1 << 14) | device);
	phy_write(dev, 0x0e, val);
}

static int ksz9031rn_phy_fixup(struct phy_device *dev)
{
	/*
	 * min rx data delay, max rx/tx clock delay,
	 * min rx/tx control delay
	 */
	mmd_write_reg(dev, 2, 4, 0);
	mmd_write_reg(dev, 2, 5, 0);
	mmd_write_reg(dev, 2, 8, 0x003ff);

	return 0;
}

static int marvell_88e1512_phy_fixup(struct phy_device *dev)
{
	u16 val;

	/*  step1. Patch for Hardware Errata */
	phy_write(dev, 0x16, 0x00ff);
	phy_write(dev, 0x11, 0x214b);
	phy_write(dev, 0x10, 0x2144);
	phy_write(dev, 0x11, 0x0c28);
	phy_write(dev, 0x10, 0x2146);
	phy_write(dev, 0x11, 0xb233);
	phy_write(dev, 0x10, 0x214d);
	phy_write(dev, 0x11, 0xcc0c);
	phy_write(dev, 0x10, 0x2159);
	phy_write(dev, 0x16, 0x00fb);
	phy_write(dev, 0x07, 0xc00d);
	phy_write(dev, 0x16, 0x0000);

	/*  step2. 88E1512 RGMII initialization */
	phy_write(dev, 0x16, 0x0002);
	val = phy_read(dev, 0x15);
	val |= ((0x1 << 4) | (0x1 << 5));
	phy_write(dev, 0x15, val);
	phy_write(dev, 0x16, 0x0000);

	val = phy_read(dev, 0x0);
	val |= (0x1 << 15);
	phy_write(dev, 0x0, val);

	/* step3. 88E1512 Copper initialization */
	phy_write(dev, 0x16, 0x0012);

	val = phy_read(dev, 0x0);
	val |= (0x1 << 0);
	phy_write(dev, 0x0, val);

	phy_write(dev, 0x16, 0x0002);

	val = phy_read(dev, 0x14);
	val |= (0x1 << 7);
	phy_write(dev, 0x0, val);

	phy_write(dev, 0x16, 0x0000);

	val = phy_read(dev, 0x0);
	val |= (0x1 << 15);
	phy_write(dev, 0x0, val);

	return 0;
}

/* Fix me. Use PHY ID of 88E1512 if available */
#define MARVELL_PHY_ID_88E1510 0x01410dd0
static void __init cowboy_enet_phy_init(void)
{
	if (IS_BUILTIN(CONFIG_PHYLIB)) {
		phy_register_fixup_for_uid(PHY_ID_KSZ9031, MICREL_PHY_ID_MASK,
				ksz9031rn_phy_fixup);
		/* Use PHY_ID of 88E1510 for 88E1512 as well */
		phy_register_fixup_for_uid(MARVELL_PHY_ID_88E1510,
			MARVELL_PHY_ID_MASK, marvell_88e1512_phy_fixup);
	}
}

#define OCOTP_MACn(n)	(0x00000620 + (n) * 0x10)
void __init cowboy_enet_mac_init(const char *compatible)
{
	struct device_node *ocotp_np, *enet_np, *from = NULL;
	void __iomem *base;
	struct property *newmac;
	u32 macaddr_low;
	u32 macaddr_high = 0;
	u32 macaddr1_high = 0;
	u8 *macaddr;
	int i;

	for (i = 0; i < 2; i++) {
		enet_np = of_find_compatible_node(from, NULL, compatible);
		if (!enet_np)
			return;

		from = enet_np;

		if (of_get_mac_address(enet_np))
			goto put_enet_node;

		ocotp_np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-ocotp");
		if (!ocotp_np) {
			pr_warn("failed to find ocotp node\n");
			goto put_enet_node;
		}

		base = of_iomap(ocotp_np, 0);
		if (!base) {
			pr_warn("failed to map ocotp\n");
			goto put_ocotp_node;
		}

		macaddr_low = readl_relaxed(base + OCOTP_MACn(1));
		if (i)
			macaddr1_high = readl_relaxed(base + OCOTP_MACn(2));
		else
			macaddr_high = readl_relaxed(base + OCOTP_MACn(0));

		newmac = kzalloc(sizeof(*newmac) + 6, GFP_KERNEL);
		if (!newmac)
			goto put_ocotp_node;

		newmac->value = newmac + 1;
		newmac->length = 6;
		newmac->name = kstrdup("local-mac-address", GFP_KERNEL);
		if (!newmac->name) {
			kfree(newmac);
			goto put_ocotp_node;
		}

		macaddr = newmac->value;
		if (i) {
			macaddr[5] = (macaddr_low >> 16) & 0xff;
			macaddr[4] = (macaddr_low >> 24) & 0xff;
			macaddr[3] = macaddr1_high & 0xff;
			macaddr[2] = (macaddr1_high >> 8) & 0xff;
			macaddr[1] = (macaddr1_high >> 16) & 0xff;
			macaddr[0] = (macaddr1_high >> 24) & 0xff;
		} else {
			macaddr[5] = macaddr_high & 0xff;
			macaddr[4] = (macaddr_high >> 8) & 0xff;
			macaddr[3] = (macaddr_high >> 16) & 0xff;
			macaddr[2] = (macaddr_high >> 24) & 0xff;
			macaddr[1] = macaddr_low & 0xff;
			macaddr[0] = (macaddr_low >> 8) & 0xff;
		}

		of_update_property(enet_np, newmac);

put_ocotp_node:
	of_node_put(ocotp_np);
put_enet_node:
	of_node_put(enet_np);
	}
}

static inline void cowboy_enet_init(void)
{
	cowboy_enet_mac_init("fsl,imx6q-fec");
	cowboy_enet_phy_init();
}


static struct resource cowboy_pmu_resources[] = {
	[0] = {
		.start		= 126,
		.end		= 126,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device cowboy_pmu_device = {
	.name			= "arm-pmu",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(cowboy_pmu_resources),
	.resource		= cowboy_pmu_resources,
};

static inline void  cowboy_add_armpmu(void)
{
	u32 val = 2;
	asm volatile("mcr p15, 0, %0, c1, c1, 1" : : "r" (val));

	platform_device_register(&cowboy_pmu_device);
}

#define SNVS_LPCR 0x38
static void mx6_snvs_poweroff(void)
{
	void __iomem *mx6_snvs_base = MX6Q_IO_ADDRESS(MX6Q_SNVS_BASE_ADDR);
	u32 value;
	value = readl(mx6_snvs_base + SNVS_LPCR);
	value |= TOP | DP_EN;
	writel(value, mx6_snvs_base + SNVS_LPCR);
}

static void __init cowboy_init_machine(void)
{
	struct device *parent;

	/* Set IOMUX */
	iomux_config();
	/*
	 * FIXME
	 * There is no definition of following 6 symbols
	 * - HW_IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT_WR
	 * - BF_IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT_DAISY_V
	 * - SD3_CLK_ALT0
	 * - HW_IOMUXC_USDHC4_CARD_CLK_IN_SELECT_INPUT_WR
	 * - BF_IOMUXC_USDHC4_CARD_CLK_IN_SELECT_INPUT_DAISY_V
	 * - SD4_CLK_ALT0
	 * So, we __raw_writel() directly as a work-around.
	 */
	writel(0x00000001, MX6Q_IO_ADDRESS(0x020E0934));
	writel(0x00000001, MX6Q_IO_ADDRESS(0x020E0938));

	mxc_arch_reset_init_dt();
	parent = imx_soc_device_init();
	if (parent == NULL)
		pr_warn("failed to initialize soc device\n");

	of_platform_populate(NULL, of_default_bus_match_table,
					NULL, parent);
	cowboy_enet_init();
	imx_anatop_init();
	cowboy_add_armpmu();
	imx6_pm_init();

	/* Fix me */
	imx6q_add_pm_imx(0, &mx6dl_cowboy_pm_data);
	/* External FPGA */
	platform_device_register(&fpga_data);
	/* snvs poweroff */
	pm_power_off   = mx6_snvs_poweroff;
}

static void __init cowboy_map_io(void)
{
	debug_ll_io_init();
	imx_scu_map_io();
	imx6_pm_map_io();
}

static void __init cowboy_init_irq(void)
{
	imx_init_revision_from_anatop();
	imx_init_l2cache();
	imx_src_init();
	imx_gpc_init();
	irqchip_init();
}

static void __init cowboy_timer_init(void)
{
	of_clk_init(NULL);
	clocksource_of_init();
	imx_print_silicon_rev(cpu_is_imx6dl() ? "i.MX6DL" : "i.MX6Q",
				imx_get_soc_revision());
}

static const char *cowboy_dt_compat[] __initdata = {
	"fsl,imx6dl-cowboy",
	NULL,
};

DT_MACHINE_START(COWBOY, "Cowboy Board (Device Tree)")
	/*
	 * i.MX6Q/DL/Cowboy maps system memory at 0x10000000 (offset 256MiB),
	 * and GPU has a limit on physical address that it accesses, which
	 * must be below 2GiB.
	 */
	.dma_zone_size	= (SZ_2G - SZ_256M),
	.smp		= smp_ops(imx_smp_ops),
	.map_io		= cowboy_map_io,
	.init_irq	= cowboy_init_irq,
	.init_time	= cowboy_timer_init,
	.init_machine	= cowboy_init_machine,
	.dt_compat	= cowboy_dt_compat,
	.restart	= mxc_restart,
MACHINE_END
