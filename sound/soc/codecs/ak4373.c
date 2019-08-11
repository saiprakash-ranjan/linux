/*
 * ak4373.c  --  AK4373 ALSA Soc Audio driver
 *
 * Copyright (C) 2016 Sony Corporation
 *
 * Based on ak4642.c by Kuninori Morimoto
 *
 * Copyright (C) 2009 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * Based on wm8731.c by Richard Purdie
 * Based on ak4535.c by Richard Purdie
 * Based on wm8753.c by Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* ** CAUTION **
 *
 * This is very simple driver.
 * It can use speaker output only
 *
 * AK4373 is tested.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define PW_MGMT1	0x00
#define PW_MGMT2	0x01
#define SG_SL1		0x02
#define SG_SL2		0x03
#define MD_CTL1		0x04
#define MD_CTL2		0x05
#define TIMER		0x06
#define ALC_CTL1	0x07
#define ALC_CTL2	0x08
#define L_IVC		0x09
#define L_DVC		0x0a
#define ALC_CTL3	0x0b
#define R_IVC		0x0c
#define R_DVC		0x0d
#define MD_CTL3		0x0e
#define MD_CTL4		0x0f
#define PW_MGMT3	0x10
#define DF_S		0x11
#define FIL3_0		0x12
#define FIL3_1		0x13
#define FIL3_2		0x14
#define FIL3_3		0x15
#define EQ_0		0x16
#define EQ_1		0x17
#define EQ_2		0x18
#define EQ_3		0x19
#define EQ_4		0x1a
#define EQ_5		0x1b
#define FIL1_0		0x1c
#define FIL1_1		0x1d
#define FIL1_2		0x1e
#define FIL1_3		0x1f

/* PW_MGMT1*/
#define PMVCM		(1 << 6) /* VCOM Power Management */
#define PMMIN		(1 << 5) /* MIN Input Power Management */
#define PMDAC		(1 << 2) /* DAC Power Management */

/* PW_MGMT2 */
#define HPMTN		(1 << 6)
#define PMHPL		(1 << 5)
#define PMHPR		(1 << 4)
#define MS		(1 << 3) /* master/slave select */
#define MCKO		(1 << 1)
#define PMPLL		(1 << 0)

#define PMHP_MASK	(PMHPL | PMHPR)
#define PMHP		PMHP_MASK

/* SG_SL1 */
#define MINS		(1 << 6) /* Switch from MIN to Speaker */

/* TIMER */
#define ZTM(param)	((param & 0x3) << 4) /* ALC Zoro Crossing TimeOut */
#define WTM(param)	(((param & 0x4) << 4) | ((param & 0x3) << 2))

/* ALC_CTL1 */
#define ALC		(1 << 5) /* ALC Enable */
#define LMTH0		(1 << 0) /* ALC Limiter / Recovery Level */

/* MD_CTL1 */
#define PLL3		(1 << 7)
#define PLL2		(1 << 6)
#define PLL1		(1 << 5)
#define PLL0		(1 << 4)
#define PLL_MASK	(PLL3 | PLL2 | PLL1 | PLL0)

#define BCKO_MASK	(1 << 3)
#define BCKO_64		BCKO_MASK

#define DIF_MASK	(3 << 0)
#define DSP		(0 << 0)
#define RIGHT_J		(1 << 0)
#define LEFT_J		(2 << 0)
#define I2S		(3 << 0)

/* MD_CTL2 */
#define FS0		(1 << 0)
#define FS1		(1 << 1)
#define FS2		(1 << 2)
#define FS3		(1 << 5)
#define FS_MASK		(FS0 | FS1 | FS2 | FS3)

/* MD_CTL4 */
#define DACH		(1 << 0)

/*
 * Playback Volume (table 32)
 *
 * max     : 0x00 : +12.0 dB
 *           ( 0.5 dB step )
 *           ( 256 levels  )
 * min     : 0xFE : -115.0 dB
 * mute    : 0xFF
 * default : 0x18 : 0dB
 */
static const DECLARE_TLV_DB_SCALE(out_tlv, -11550, 50, 1);

static const struct snd_kcontrol_new ak4373_snd_controls[] = {

	SOC_DOUBLE_R_TLV("Digital Playback Volume", L_DVC, R_DVC,
			 0, 0xFF, 1, out_tlv),
};

/*
 * ak4373 register cache
 */
static const u8 ak4373_reg[] = {
	0x00, 0x00, 0x00, 0x00, /* 0x00 */
	0x02, 0x00, 0x00, 0x00,
	0xe1, 0xe1, 0x18, 0x00,
	0xe1, 0x18, 0x11, 0x08,
	0x00, 0x00, 0x00, 0x00, /* 0x10 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, /* 0x20 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, /* 0x30 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, /* 0x40 */
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static int ak4373_dai_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	/*
	 * start speaker output
	 *
	 * PLL, Slave Mode (BICK pin)
	 * Audio I/F Format : I2S
	 */

	/* PLL reference clock input is BICK pin */
	snd_soc_write(codec, MD_CTL1, 0x33);

	/* Set PMPLL */
	snd_soc_write(codec, PW_MGMT1, 0x40);
	snd_soc_write(codec, PW_MGMT2, 0x01);
	/* Wait 4ms */
	usleep_range(4000, 4000);

	/* Setup path of DAC to SPK-Amp */
	snd_soc_write(codec, SG_SL1, 0x20);
	snd_soc_write(codec, SG_SL2, 0x00);

	/* Enable Automatic level control (ALC) */
	snd_soc_write(codec, ALC_CTL2, 0xC1);
	snd_soc_write(codec, ALC_CTL3, 0x00);
	snd_soc_write(codec, ALC_CTL1, 0x20);

	/* Set left and right channel input volume control */
	snd_soc_write(codec, L_IVC, 0x91);
	snd_soc_write(codec, R_IVC, 0x91);

	/* Set PMVCM, PMMIN, PMSPK, PMDAC */
	snd_soc_write(codec, PW_MGMT1, 0x74);

	/* Set Speaker-Amp to normal operation */
	snd_soc_write(codec, SG_SL1, 0xA0);

	return 0;
}

static void ak4373_dai_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	int is_play = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_soc_codec *codec = dai->codec;

	if (is_play) {
		/* Power Down DAC, SPK-AMP and stop external clock */
		snd_soc_write(codec, SG_SL1, 0x20);
		/* Wait 1ms for noise */
		usleep_range(1000, 1000);
		snd_soc_write(codec, SG_SL1, 0x00);
		snd_soc_write(codec, PW_MGMT1, 0x40);
		snd_soc_write(codec, PW_MGMT2, 0x00);
	}
}

static int ak4373_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	/* AK4373 DAC is tested only as slave currently. format variable
	 * is unused as of now but can be used to check dai format type
	 * and master/slave audio interface.
	 */

	/* Set PLL Power management mode and power-up .
	 * By default PLL is set as slave mode (M/S bit)
	 */
	snd_soc_update_bits(codec, PW_MGMT2, PMPLL, PMPLL);

	/* Set format type as I2S*/
	snd_soc_update_bits(codec, MD_CTL1, DIF_MASK, I2S);

	return 0;
}

static int ak4373_dai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 rate;

	/* Sampling frequency range selected by FS3 and FS2 bit
	 * when PLL reference clock input is LRCK or BICK  pin
	 */
	switch (params_rate(params)) {
	case 7350:
	case 8000:
		snd_soc_write(codec, TIMER, 0x00);
	case 11025:
	case 12000:
		rate = 0;
		break;
	case 14700:
	case 16000:
		snd_soc_write(codec, TIMER, 0x14);
	case 22050:
	case 24000:
		rate = FS2;
		break;
	case 29400:
	case 32000:
	case 44100:
	case 48000:
		snd_soc_write(codec, TIMER, 0x2C);
		rate = FS3;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_update_bits(codec, MD_CTL2, FS_MASK, rate);

	switch (params_channels(params)) {
	case 1:
		/* Mute right channel for mono */
		snd_soc_write(codec, MD_CTL3, 0x00);
		snd_soc_write(codec, L_DVC, 0x18);
		snd_soc_write(codec, R_DVC, 0xFF);
		break;
	case 2:
		/* stereo */
		snd_soc_write(codec, MD_CTL3, 0x10);
		snd_soc_write(codec, L_DVC, 0x18);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ak4373_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, PW_MGMT1, 0x00);
		break;
	default:
		snd_soc_update_bits(codec, PW_MGMT1, PMVCM, PMVCM);
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static const struct snd_soc_dai_ops ak4373_dai_ops = {
	.startup	= ak4373_dai_startup,
	.shutdown	= ak4373_dai_shutdown,
	.set_fmt	= ak4373_dai_set_fmt,
	.hw_params	= ak4373_dai_hw_params,
};

static struct snd_soc_dai_driver ak4373_dai = {
	.name = "ak4373-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE },
	.ops = &ak4373_dai_ops,
};

static int ak4373_resume(struct snd_soc_codec *codec)
{
	snd_soc_cache_sync(codec);
	return 0;
}

static int ak4373_probe(struct snd_soc_codec *codec)
{
	int ret;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	snd_soc_add_codec_controls(codec, ak4373_snd_controls,
			     ARRAY_SIZE(ak4373_snd_controls));

	ak4373_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int ak4373_remove(struct snd_soc_codec *codec)
{
	ak4373_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_ak4373 = {
	.probe			= ak4373_probe,
	.remove			= ak4373_remove,
	.resume			= ak4373_resume,
	.set_bias_level		= ak4373_set_bias_level,
	.reg_cache_default	= ak4373_reg,			/* ak4373 reg */
	.reg_cache_size		= ARRAY_SIZE(ak4373_reg),	/* ak4373 reg */
	.reg_word_size		= sizeof(u8),
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static struct of_device_id ak4373_of_match[];
static int ak4373_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	const struct snd_soc_codec_driver *driver;

	driver = NULL;
	if (np) {
		const struct of_device_id *of_id;

		of_id = of_match_device(ak4373_of_match, &i2c->dev);
		if (of_id)
			driver = of_id->data;
	} else {
		driver = (struct snd_soc_codec_driver *)id->driver_data;
	}

	if (!driver) {
		dev_err(&i2c->dev, "no driver\n");
		return -EINVAL;
	}

	return snd_soc_register_codec(&i2c->dev,
				      driver, &ak4373_dai, 1);
}

static int ak4373_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static struct of_device_id ak4373_of_match[] = {
	{ .compatible = "asahi-kasei,ak4373",	.data = &soc_codec_dev_ak4373},
	{},
};
MODULE_DEVICE_TABLE(of, ak4373_of_match);

static const struct i2c_device_id ak4373_i2c_id[] = {
	{ "ak4373", (kernel_ulong_t)&soc_codec_dev_ak4373 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4373_i2c_id);

static struct i2c_driver ak4373_i2c_driver = {
	.driver = {
		.name = "ak4373",
		.owner = THIS_MODULE,
		.of_match_table = ak4373_of_match,
	},
	.probe		= ak4373_i2c_probe,
	.remove		= ak4373_i2c_remove,
	.id_table	= ak4373_i2c_id,
};
#endif

static int __init ak4373_modinit(void)
{
	int ret = 0;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&ak4373_i2c_driver);
#endif
	return ret;

}
module_init(ak4373_modinit);

static void __exit ak4373_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&ak4373_i2c_driver);
#endif

}
module_exit(ak4373_exit);

MODULE_DESCRIPTION("Soc AK4373 driver");
MODULE_AUTHOR("Krishanth Jagaduri <krishanth.jagaduri@ap.sony.com>");
MODULE_LICENSE("GPL");
