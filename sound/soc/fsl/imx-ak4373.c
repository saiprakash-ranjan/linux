/*
 * Machine driver for AK4373
 *
 * Copyright (C) 2016 Sony Corporation
 * Based on sound/soc/generic/simple-card.c
 *
 * ASoC simple sound card support
 *
 * Copyright (C) 2012 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <sound/simple_card.h>

#include "imx-audmux.h"

#define imx_ak4373_get_card_info(p) \
	container_of(p->dai_link, struct asoc_simple_card_info, snd_link)

static int __imx_ak4373_dai_init(struct snd_soc_dai *dai,
				       struct asoc_simple_dai *set,
				       unsigned int daifmt)
{
	int ret = 0;

	daifmt |= set->fmt;

	if (!ret && daifmt)
		ret = snd_soc_dai_set_fmt(dai, daifmt);

	if (!ret && set->sysclk)
		ret = snd_soc_dai_set_sysclk(dai, 0, set->sysclk, 0);

	return ret;
}

static int imx_ak4373_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct asoc_simple_card_info *info = imx_ak4373_get_card_info(rtd);
	struct snd_soc_dai *codec = rtd->codec_dai;
	struct snd_soc_dai *cpu = rtd->cpu_dai;
	unsigned int daifmt = info->daifmt;
	int ret;

	ret = __imx_ak4373_dai_init(codec, &info->codec_dai, daifmt);
	if (ret < 0)
		return ret;

	ret = __imx_ak4373_dai_init(cpu, &info->cpu_dai, daifmt);
	if (ret < 0)
		return ret;

	return 0;
}

static int
imx_ak4373_sub_parse_of(struct device_node *np,
			      struct asoc_simple_dai *dai,
			      struct device_node **node)
{
	struct clk *clk;
	int ret;

	/*
	 * get node via "sound-dai = <&phandle port>"
	 * it will be used as xxx_of_node on soc_bind_dai_link()
	 */
	*node = of_parse_phandle(np, "sound-dai", 0);
	if (!*node)
		return -ENODEV;

	/* get dai->name */
	ret = snd_soc_of_get_dai_name(np, &dai->name);
	if (ret < 0)
		goto parse_error;

	/*
	 * bitclock-inversion, frame-inversion
	 * bitclock-master,    frame-master
	 * and specific "format" if it has
	 */
	dai->fmt = snd_soc_of_parse_daifmt(np, NULL);

	/*
	 * dai->sysclk come from
	 *  "clocks = <&xxx>" (if system has common clock)
	 *  or "system-clock-frequency = <xxx>"
	 */
	clk = of_clk_get(np, 0);
	if (IS_ERR(clk))
		of_property_read_u32(np,
				     "system-clock-frequency",
				     &dai->sysclk);
	else
		dai->sysclk = clk_get_rate(clk);

	ret = 0;

parse_error:
	of_node_put(*node);

	return ret;
}

static int imx_ak4373_parse_of(struct device_node *node,
				     struct asoc_simple_card_info *info,
				     struct device *dev,
				     struct device_node **of_cpu,
				     struct device_node **of_codec,
				     struct device_node **of_platform)
{
	struct device_node *np;
	char *name;
	int ret = 0;
	int int_port, ext_port;

	/* parsing the card name from DT */
	snd_soc_of_parse_card_name(&info->snd_card, "model");

	/* get CPU/CODEC common format via imx-audio-ak4373,format */
	info->daifmt = snd_soc_of_parse_daifmt(node, "imx-audio-ak4373,") &
		(SND_SOC_DAIFMT_FORMAT_MASK | SND_SOC_DAIFMT_INV_MASK);

	/* CPU sub-node */
	ret = -EINVAL;
	np = of_get_child_by_name(node, "cpu");
	if (np)
		ret = imx_ak4373_sub_parse_of(np,
						  &info->cpu_dai,
						  of_cpu);
	if (ret < 0)
		return ret;

	/* AUDMUX port needs to be configured only if cpu dai is SSI */
	if (!strstr(info->cpu_dai.name, "ssi"))
		goto audmux_bypass;

	ret = of_property_read_u32(node, "mux-int-port", &int_port);
	if (ret) {
		dev_err(dev, "mux-int-port missing or invalid\n");
		return ret;
	}

	ret = of_property_read_u32(node, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;

	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));

	if (ret) {
		dev_err(dev, "audmux internal port setup failed\n");
		return ret;
	}

	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(int_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(int_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));

	if (ret) {
		dev_err(dev, "audmux external port setup failed\n");
		return ret;
	}

audmux_bypass:
	/* CODEC sub-node */
	ret = -EINVAL;
	np = of_get_child_by_name(node, "codec");
	if (np)
		ret = imx_ak4373_sub_parse_of(np,
						  &info->codec_dai,
						  of_codec);
	if (ret < 0)
		return ret;

	/*
	 * overwrite cpu_dai->fmt as its DAIFMT_MASTER bit is based on CODEC
	 * while the other bits should be identical unless buggy SW/HW design.
	 */
	info->cpu_dai.fmt = info->codec_dai.fmt;

	/* card name is created from CPU/CODEC dai name */
	name = devm_kzalloc(dev,
			    strlen(info->cpu_dai.name)   +
			    strlen(info->codec_dai.name) + 2,
			    GFP_KERNEL);
	sprintf(name, "%s-%s", info->cpu_dai.name, info->codec_dai.name);
	info->name = info->card = name;

	if (!info->snd_card.name)
		info->name = info->card = name;
	else
		info->name = info->card = info->snd_card.name;

	/* simple-card assumes platform == cpu */
	*of_platform = *of_cpu;

	dev_dbg(dev, "card-name : %s\n", info->card);
	dev_dbg(dev, "platform : %04x\n", info->daifmt);
	dev_dbg(dev, "cpu : %s / %04x / %d\n",
		info->cpu_dai.name,
		info->cpu_dai.fmt,
		info->cpu_dai.sysclk);
	dev_dbg(dev, "codec : %s / %04x / %d\n",
		info->codec_dai.name,
		info->codec_dai.fmt,
		info->codec_dai.sysclk);

	return 0;
}

static int imx_ak4373_probe(struct platform_device *pdev)
{
	struct asoc_simple_card_info *cinfo;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *of_cpu, *of_codec, *of_platform;
	struct device *dev = &pdev->dev;

	cinfo		= NULL;
	of_cpu		= NULL;
	of_codec	= NULL;
	of_platform	= NULL;
	if (np && of_device_is_available(np)) {
		cinfo = devm_kzalloc(dev, sizeof(*cinfo), GFP_KERNEL);
		if (cinfo) {
			int ret;
			cinfo->snd_card.dev = &pdev->dev;
			ret = imx_ak4373_parse_of(np, cinfo, dev,
							&of_cpu,
							&of_codec,
							&of_platform);
			if (ret < 0) {
				if (ret != -EPROBE_DEFER)
					dev_err(dev, "parse error %d\n", ret);
				return ret;
			}
		}
	} else {
		cinfo = pdev->dev.platform_data;
		cinfo->snd_card.dev = &pdev->dev;
	}

	if (!cinfo) {
		dev_err(dev, "no info for imx-ak4373\n");
		return -EINVAL;
	}

	if (!cinfo->name	||
	    !cinfo->card	||
	    !cinfo->codec_dai.name	||
	    !(cinfo->codec		|| of_codec)	||
	    !(cinfo->platform		|| of_platform)	||
	    !(cinfo->cpu_dai.name	|| of_cpu)) {
		dev_err(dev, "insufficient asoc_simple_card_info settings\n");
		return -EINVAL;
	}

	/*
	 * init snd_soc_dai_link
	 */
	cinfo->snd_link.name		= cinfo->name;
	cinfo->snd_link.stream_name	= cinfo->name;
	cinfo->snd_link.cpu_dai_name	= cinfo->cpu_dai.name;
	cinfo->snd_link.platform_name	= cinfo->platform;
	cinfo->snd_link.codec_name	= cinfo->codec;
	cinfo->snd_link.codec_dai_name	= cinfo->codec_dai.name;
	cinfo->snd_link.cpu_of_node	= of_cpu;
	cinfo->snd_link.codec_of_node	= of_codec;
	cinfo->snd_link.platform_of_node = of_platform;
	cinfo->snd_link.init		= imx_ak4373_dai_init;

	/*
	 * init snd_soc_card
	 */
	cinfo->snd_card.name		= cinfo->card;
	cinfo->snd_card.owner		= THIS_MODULE;
	cinfo->snd_card.dai_link	= &cinfo->snd_link;
	cinfo->snd_card.num_links	= 1;

	return snd_soc_register_card(&cinfo->snd_card);
}

static int imx_ak4373_remove(struct platform_device *pdev)
{
	struct asoc_simple_card_info *cinfo = pdev->dev.platform_data;

	return snd_soc_unregister_card(&cinfo->snd_card);
}

static const struct of_device_id imx_ak4373_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-ak4373", },
	{},
};
MODULE_DEVICE_TABLE(of, imx_ak4373_dt_ids);

static struct platform_driver imx_ak4373_driver = {
	.driver = {
		.name	= "imx-ak4373",
		.of_match_table = imx_ak4373_dt_ids,
	},
	.probe		= imx_ak4373_probe,
	.remove		= imx_ak4373_remove,
};

module_platform_driver(imx_ak4373_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Freescale i.MX AK4373 ASoC machine driver");
MODULE_AUTHOR("Krishanth Jagaduri <krishanth.jagaduri@ap.sony.com>");
