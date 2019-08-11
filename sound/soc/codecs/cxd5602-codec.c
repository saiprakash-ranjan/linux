#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <asm/div64.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "../../../drivers/char/hifc/audio/high_level_api/as_high_level_api.h"
#pragma GCC diagnostic pop
#include "cxd5602_codec.h"

static struct input_dev *trigger_dev = NULL;
#define KEY_VOICE_TRIGGER 506
#define KEY_VOICE_COMMAND 507

#define CXD5602_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_176400)

#define CXD5602_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

#define DUMMY_REG   0
#define DUMMY_SFT   0
#define DUMMY_SFT_L 0
#define DUMMY_SFT_R 4
#define DUMMY_INV   0

static const char *mode_chg[] = {"VoiceTrigger", "VoiceCommand", "VoiceRecognition", "PhoneCall", "Recording", "ShutterSound"};
static const char *output_dev[] = {"SP", "HP"};
static const char *mic_sel[] = {"Mic1-Mic2", "Mic0-Mic3"};
static SOC_ENUM_SINGLE_EXT_DECL(soc_mode, mode_chg);
static SOC_ENUM_SINGLE_EXT_DECL(soc_output, output_dev);
static SOC_ENUM_SINGLE_EXT_DECL(soc_mic, mic_sel);

extern int saved_mute;
extern int saved_dai_mute;

static const struct snd_kcontrol_new cxd5602_snd_controls[] = {
	SOC_SINGLE_EXT(
		"Volume",
		DUMMY_REG,
		DUMMY_SFT,
		59,
		DUMMY_INV,
		cxd5602_get_volume_control,
		cxd5602_put_volume_control
	),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Raw Command",
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
			   SNDRV_CTL_ELEM_ACCESS_TLV_READWRITE |
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK),
		.info = cxd5602_raw_data_info,
		.get = cxd5602_get_raw_data,
		.put = cxd5602_put_raw_data,
		.tlv = { .c = cxd5602_get_raw_data_tlv },
	},
	SOC_SINGLE_BOOL_EXT(
		"Mute",
		0,
		cxd5602_get_mute,
		cxd5602_put_mute
	),
	SOC_ENUM_EXT(
		"Mode",
		soc_mode,
		cxd5602_get_mode,
		cxd5602_put_mode
	),
	SOC_SINGLE_BOOL_EXT(
		"xLoud",
		0,
		cxd5602_get_xLoud,
		cxd5602_put_xLoud
	),
	SOC_SINGLE_BOOL_EXT(
		"EnvAdapt",
		0,
		cxd5602_get_env_adapt,
		cxd5602_put_env_adapt
	),
	SOC_ENUM_EXT(
		"OutputDevice",
		soc_output,
		cxd5602_get_output,
		cxd5602_put_output
	),
	SOC_SINGLE_BOOL_EXT(
		"VAD",
		0,
		cxd5602_get_vad,
		cxd5602_put_vad
	),
	SOC_ENUM_EXT(
		"MicSelect",
		soc_mic,
		cxd5602_get_micsel,
		cxd5602_put_micsel
	),
	SOC_SINGLE_EXT(
		"MicGain0",
		DUMMY_REG,
		DUMMY_SFT,
		210,
		DUMMY_INV,
		cxd5602_get_micgain0_control,
		cxd5602_put_micgain0_control
	),
	SOC_SINGLE_EXT(
		"MicGain1",
		DUMMY_REG,
		DUMMY_SFT,
		210,
		DUMMY_INV,
		cxd5602_get_micgain1_control,
		cxd5602_put_micgain1_control
	),
	SOC_SINGLE_EXT(
		"MicGain2",
		DUMMY_REG,
		DUMMY_SFT,
		210,
		DUMMY_INV,
		cxd5602_get_micgain2_control,
		cxd5602_put_micgain2_control
	),
	SOC_SINGLE_EXT(
		"MicGain3",
		DUMMY_REG,
		DUMMY_SFT,
		210,
		DUMMY_INV,
		cxd5602_get_micgain3_control,
		cxd5602_put_micgain3_control
	),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "ScuMicGain",
		.info = cxd5602_info_scumicgain_control,
		.get = cxd5602_get_scumicgain_control,
		.put = cxd5602_put_scumicgain_control,
		.private_value = (unsigned long)&(struct soc_mixer_control)
			{.reg = DUMMY_REG,
			 .shift = DUMMY_SFT,
			 .min = -6,
			 .max = 24,
			 .invert = DUMMY_INV},
	},
};

static int cxd5602_dai_set_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	unsigned int ret = 0;

	printk(KERN_INFO "%s fmt=0x%X\n", __FUNCTION__, fmt);

	return ret;
}

static int cxd5602_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static int cxd5602_dai_mute(struct snd_soc_dai *dai, int mute)
{
	HI_AudioExtCmdSetMute Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s mute=%d\n", __FUNCTION__, mute);

	if (mute == saved_dai_mute) {
		return 0;
	}

	if (saved_mute == 1) {
		saved_dai_mute = mute;
		printk(KERN_INFO "muted by ctrl\n");
		return 0;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MUTE;
	Command.Mute = mute;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResult), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResult)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResult), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	saved_dai_mute = mute;

	return 0;
}

static int cxd5602_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static void cxd5602_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
}

static struct snd_soc_dai_ops cxd5602_dummy_dai_ops = {
	.startup	= cxd5602_dai_startup,
	.shutdown	= cxd5602_dai_shutdown,
	.set_fmt	= cxd5602_dai_set_fmt,
	.hw_params	= cxd5602_dai_hw_params,
	.digital_mute	= cxd5602_dai_mute,
};

static struct snd_soc_dai_driver cxd5602_dai[] = {
	{
		.name = "cxd5602-mic1",
		.id = 1,
		.capture = {
			.stream_name  = "Hifi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = CXD5602_RATES,
			.formats      = CXD5602_FORMATS,
		},
		.ops = &cxd5602_dummy_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cxd5602-mic2",
		.id = 2,
		.capture = {
			.stream_name  = "Voice Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = CXD5602_RATES,
			.formats      = CXD5602_FORMATS,
		},
		.ops = &cxd5602_dummy_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cxd5602-out1",
		.id = 3,
		.playback = {
			.stream_name  = "Hifi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = CXD5602_RATES,
			.formats      = CXD5602_FORMATS,
		},
		.ops = &cxd5602_dummy_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cxd5602-out2",
		.id = 4,
		.playback = {
			.stream_name  = "Voice Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = CXD5602_RATES,
			.formats      = CXD5602_FORMATS,
		},
		.ops = &cxd5602_dummy_dai_ops,
		.symmetric_rates = 1,
	}
};

static int cxd5602_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	printk(KERN_DEBUG "cxd5602_set_bias_level level=%d\n", level);

	return 0;
}

static unsigned int cxd5602_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	printk(KERN_INFO "cxd5602_read reg=0x%x\n", reg);

	return 0;
}

static int cxd5602_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret = 0;

	printk(KERN_INFO "cxd5602_write reg=0x%x value=0x%x\n", reg, value);

	return ret;

}

static int cxd5602_suspend(struct snd_soc_codec *codec)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static int cxd5602_resume(struct snd_soc_codec *codec)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static int cxd5602_probe(struct snd_soc_codec *codec)
{
	int ret = 0;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	return ret;
}

static int cxd5602_remove(struct snd_soc_codec *codec)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static struct snd_soc_codec_driver cxd5602_codec_driver = {
	.probe		= cxd5602_probe,
	.remove		= cxd5602_remove,
#if 1
	.suspend	= cxd5602_suspend,
	.resume		= cxd5602_resume,
	.read		= cxd5602_read,
	.write		= cxd5602_write,
	.set_bias_level	= cxd5602_set_bias_level,
	.reg_cache_size	= 0,
	.reg_word_size	= sizeof(u8),
	.controls	= cxd5602_snd_controls,
	.num_controls	= ARRAY_SIZE(cxd5602_snd_controls),
#endif
};

static void cxd5602_handle_trigger(int CommandCode, int SubCode)
{
	if (trigger_dev) {
		printk(KERN_INFO "%s %#x %#x\n", __FUNCTION__, CommandCode, SubCode);
		if (CommandCode == 0x50) {
			input_event(trigger_dev, EV_KEY, KEY_VOICE_TRIGGER, 1);
		} else {
			input_event(trigger_dev, EV_KEY, KEY_VOICE_COMMAND, 1);
		}
		input_sync(trigger_dev);
	}
}

static int cxd5602_codec_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	ret = snd_soc_register_codec(&pdev->dev,
		&cxd5602_codec_driver, cxd5602_dai, ARRAY_SIZE(cxd5602_dai));
	if (ret < 0) {
		printk(KERN_ERR "Failed to register codec cxd5602\n");
		goto err_out;
	}

	trigger_dev = input_allocate_device();
	if (trigger_dev) {
		trigger_dev->name = "CXD5602 Trigger";
		trigger_dev->phys = "cxd5602/trigger";
		//trigger_dev->id.bustype = BUS_HOST;
		//trigger_dev->id.vendor = 0x0001;
		//trigger_dev->id.product = 0x0001;
		//trigger_dev->id.version = 0x0100;
		trigger_dev->dev.parent = &pdev->dev;

		input_set_capability(trigger_dev, EV_KEY, KEY_VOICE_TRIGGER);
		input_set_capability(trigger_dev, EV_KEY, KEY_VOICE_COMMAND);

		if (input_register_device(trigger_dev)) {
			input_free_device(trigger_dev);
			trigger_dev = NULL;

			printk(KERN_ERR "Failed to register input device\n");
		}
	} else {
		printk(KERN_ERR "Failed to allocate memory for input device\n");
	}

	cxd5602_codec_hifc_init(pdev->dev.parent, cxd5602_handle_trigger);

	return ret;

err_out:
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int cxd5602_codec_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (trigger_dev) {
		input_unregister_device(trigger_dev);
		input_free_device(trigger_dev);
		trigger_dev = NULL;
	}

	snd_soc_unregister_codec(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void cxd5602_codec_shutdown(struct platform_device *pdev)
{
	printk(KERN_DEBUG "%s\n", __FUNCTION__);
}

static struct platform_driver cxd5602_driver = {
	.probe		= cxd5602_codec_probe,
	.remove		= cxd5602_codec_remove,
	.shutdown	= cxd5602_codec_shutdown,
	.driver		= {
		.name	= "cxd5602-codec",
		.owner	= THIS_MODULE,
	},
};

static int __init cxd5602_codec_init(void)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return platform_driver_register(&cxd5602_driver);
}

static void __exit cxd5602_codec_exit(void)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);
	platform_driver_unregister(&cxd5602_driver);
}

module_init(cxd5602_codec_init);
module_exit(cxd5602_codec_exit);

MODULE_DESCRIPTION("ASoc Sony CXD5602 CODEC driver");
MODULE_AUTHOR("MATSUZAKI Yasuhiro <Yasuhiro.Matsuzaki@jp.sony.com>");
MODULE_LICENSE("GPL");
