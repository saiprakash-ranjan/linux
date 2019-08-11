#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
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

static	AudioResult		rawResult;

#define	VOLUME_MIN		0
#define	VOLUME_MAX		59
#define	GAIN_MIN		0
#define	GAIN_MAX		210
#define	SCU_GAIN_MIN	-6
#define	SCU_GAIN_MAX	24

int						saved_dai_mute = 0;
int						saved_mute = 0;

int cxd5602_get_volume_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetVolume Command;
	HI_AudioExtResVolume Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_VOLUME;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResVolume), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResVolume)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResVolume), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s vol=%d\n", __FUNCTION__, Result.Volume);

	ucontrol->value.integer.value[0] = Result.Volume;

	return 0;
}

int cxd5602_put_volume_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetVolume Command;
	HI_AudioExtResult Result;
	int ret;
	int vol;
	int set_cnt;

	vol = ucontrol->value.integer.value[0];

	if (vol < VOLUME_MIN || vol > VOLUME_MAX) {
		printk(KERN_ERR "volume Param ERROR(%d)\n", vol);
		return -EINVAL;
	}

	printk(KERN_INFO "cxd5602_put_volume_control %d\n", vol);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_VOLUME;
	Command.Volume = vol;
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

	return 0;
}

int cxd5602_get_raw_data(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int cxd5602_put_raw_data(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

int cxd5602_raw_data_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	if (sizeof(AudioCommand) > sizeof(AudioResult))
		uinfo->count = sizeof(AudioCommand);
	else
		uinfo->count = sizeof(AudioResult);

	printk(KERN_INFO "%s\n", __FUNCTION__);

	return 0;
}

int cxd5602_get_raw_data_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			  unsigned int size, unsigned int __user *_tlv)
{

	AudioCommand	sendData;
	unsigned int	copySize;

	/* 0=read,1=write,-1=command */
	if (op_flag == 0) {
		if (size > sizeof(AudioResult))
			copySize = sizeof(AudioResult);
		else
			copySize = size;

		if (copy_to_user(_tlv, &rawResult, copySize)) {
			printk(KERN_ERR "cxd5602_get_raw_data_tlv: failed to copy_to_user\n");
		}
		return 0;
	} else if (op_flag == -1) {
		return 0;
	}

	memset(&sendData, 0x00, sizeof(AudioCommand));

	if (size > sizeof(AudioCommand)) {
		printk(KERN_ERR "cxd5602_get_raw_data_tlv size Error (%d)\n", size);
		return 0;
	}

	if (copy_from_user(&sendData, _tlv, size)) {
		printk(KERN_ERR "cxd5602_get_raw_data_tlv: failed to copy_from_user\n");
		return 0;
	}

	if (sendData.header.packet_length * 4 > size) {
		printk(KERN_ERR "cxd5602_get_raw_data_tlv PacketLength Error(%d)\n", sendData.header.packet_length);
		return 0;
	}

	AS_SendAudioCommand(&sendData);

	AS_ReceiveAudioResult(&rawResult);

	return 0;
}

int cxd5602_get_mute(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	printk(KERN_INFO "%s val=%d\n", __FUNCTION__, saved_mute);

	ucontrol->value.integer.value[0] = saved_mute;

	return 0;
}

int cxd5602_put_mute(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMute Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "cxd5602_put_mute Value=%ld\n", ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > 1) {
		printk(KERN_ERR "mute Param ERROR(%ld)\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == saved_mute) {
		printk(KERN_INFO "cxd5602_put_mute same state\n");
		return 0;
	}

	if (saved_dai_mute == 1) {
		saved_mute = ucontrol->value.integer.value[0];

		printk(KERN_INFO "muted by dai\n");
		return 0;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MUTE;
	Command.Mute = ucontrol->value.integer.value[0];
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

	saved_mute = ucontrol->value.integer.value[0];

	return 0;
}

int cxd5602_get_xLoud(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetXLoud Command;
	HI_AudioExtResXLoud Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_XLOUD;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResXLoud), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResXLoud)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResXLoud), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s xLoud=%d\n", __FUNCTION__, Result.XLoud);

	ucontrol->value.integer.value[0] = Result.XLoud;

	return 0;
}

int cxd5602_put_xLoud(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetXLoud Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s value=%ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > 1) {
		printk(KERN_ERR "Audio xLoud param ERROR(%ld)\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_XLOUD;
	Command.XLoud = ucontrol->value.integer.value[0];
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

	return 0;
}

int cxd5602_get_env_adapt(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetEnvAdapt Command;
	HI_AudioExtResEnvAdapt Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_ENV_ADAPT;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResEnvAdapt), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResEnvAdapt)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResEnvAdapt), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s val=%d\n", __FUNCTION__, Result.EnvAdapt);

	ucontrol->value.integer.value[0] = Result.EnvAdapt;

	return 0;
}

int cxd5602_put_env_adapt(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetEnvAdapt Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s value=%ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > 1) {
		printk(KERN_ERR "%s: param out of range %ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_ENV_ADAPT;
	Command.EnvAdapt = ucontrol->value.integer.value[0];
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

	return 0;
}

int cxd5602_get_mode(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetMode Command;
	HI_AudioExtResMode Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_MODE;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResMode), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResMode)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResMode), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s mode=%d\n", __FUNCTION__, Result.Mode);

	ucontrol->value.integer.value[0] = Result.Mode;

	return 0;
}

int cxd5602_put_mode(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMode Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s value=%ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > 5) {
		printk(KERN_ERR "mode param ERROR(%ld)\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MODE;
	Command.Mode = ucontrol->value.integer.value[0];
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

	return 0;
}

int cxd5602_get_output(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetOutDev Command;
	HI_AudioExtResOutDev Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_OUTDEV;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResOutDev), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResOutDev)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResOutDev), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s val=%d\n", __FUNCTION__, Result.OutDev);

	ucontrol->value.integer.value[0] = Result.OutDev;

	return 0;
}

int cxd5602_put_output(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetOutDev Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s value=%ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > 1) {
		printk(KERN_ERR "output device param ERROR(%ld)\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_OUTDEV;
	Command.OutDev = ucontrol->value.integer.value[0];
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

	return 0;
}

int cxd5602_get_vad(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{

	AudioCommand cmd;
	AudioResult result;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		ucontrol->value.integer.value[0] = 0;
		return 0;
	}

	cmd.header.packet_length = 2;
	cmd.header.command_code  = AUDCMD_GETSTATUS;
	cmd.header.sub_code      = 0;

	cmd.get_status.reserved = 0;

	AS_SendAudioCommand(&cmd);
	AS_ReceiveAudioResult(&result);

	if (result.header.result_code != AUDRLT_NOTIFYSTATUS) {
		printk(KERN_ERR "Get Status ERROR(%d)\n", result.header.result_code);
		return 1;
	}

	ucontrol->value.integer.value[0] = result.notify_status.vad_status;

	return 0;
}

int cxd5602_put_vad(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	printk(KERN_INFO "%s\n", __FUNCTION__);

	return -1;
}

int cxd5602_get_micsel(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetMicSel Command;
	HI_AudioExtResMicSel Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_MIC_SELECT;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResMicSel), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResMicSel)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResMicSel), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s val=%d\n", __FUNCTION__, Result.MicSel);

	ucontrol->value.integer.value[0] = Result.MicSel;

	return 0;
}

int cxd5602_put_micsel(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMicSel Command;
	HI_AudioExtResult Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s value=%ld\n", __FUNCTION__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] < 0 || ucontrol->value.integer.value[0] > 1) {
		printk(KERN_ERR "MIC Select param ERROR(%ld)\n", ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MIC_SELECT;
	Command.MicSel = ucontrol->value.integer.value[0];
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

	return 0;
}

int cxd5602_get_micgain0_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetMicGain Command;
	HI_AudioExtResMicGain Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_MIC_GAIN0;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResMicGain), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResMicGain)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResMicGain), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s gain=%d\n", __FUNCTION__, Result.MicGain);

	ucontrol->value.integer.value[0] = Result.MicGain;

	return 0;
}

int cxd5602_put_micgain0_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMicGain Command;
	HI_AudioExtResult Result;
	int ret;
	int gain;
	int set_cnt;

	gain = ucontrol->value.integer.value[0];

	if (gain < GAIN_MIN || gain > GAIN_MAX) {
		printk(KERN_ERR "gain Param ERROR(%d)\n", gain);
		return -EINVAL;
	}

	printk(KERN_INFO "cxd5602_put_micgain0_control %d\n", gain);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MIC_GAIN0;
	Command.MicGain = gain;
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

	return 0;
}

int cxd5602_get_micgain1_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetMicGain Command;
	HI_AudioExtResMicGain Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_MIC_GAIN1;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResMicGain), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResMicGain)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResMicGain), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s gain=%d\n", __FUNCTION__, Result.MicGain);

	ucontrol->value.integer.value[0] = Result.MicGain;

	return 0;
}

int cxd5602_put_micgain1_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMicGain Command;
	HI_AudioExtResult Result;
	int ret;
	int gain;
	int set_cnt;

	gain = ucontrol->value.integer.value[0];

	if (gain < GAIN_MIN || gain > GAIN_MAX) {
		printk(KERN_ERR "gain Param ERROR(%d)\n", gain);
		return -EINVAL;
	}

	printk(KERN_INFO "cxd5602_put_micgain1_control %d\n", gain);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MIC_GAIN1;
	Command.MicGain = gain;
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

	return 0;
}

int cxd5602_get_micgain2_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetMicGain Command;
	HI_AudioExtResMicGain Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_MIC_GAIN2;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResMicGain), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResMicGain)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResMicGain), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s gain=%d\n", __FUNCTION__, Result.MicGain);

	ucontrol->value.integer.value[0] = Result.MicGain;

	return 0;
}

int cxd5602_put_micgain2_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMicGain Command;
	HI_AudioExtResult Result;
	int ret;
	int gain;
	int set_cnt;

	gain = ucontrol->value.integer.value[0];

	if (gain < GAIN_MIN || gain > GAIN_MAX) {
		printk(KERN_ERR "gain Param ERROR(%d)\n", gain);
		return -EINVAL;
	}

	printk(KERN_INFO "cxd5602_put_micgain2_control %d\n", gain);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MIC_GAIN2;
	Command.MicGain = gain;
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

	return 0;
}

int cxd5602_get_micgain3_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetMicGain Command;
	HI_AudioExtResMicGain Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_MIC_GAIN3;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResMicGain), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResMicGain)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResMicGain), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s gain=%d\n", __FUNCTION__, Result.MicGain);

	ucontrol->value.integer.value[0] = Result.MicGain;

	return 0;
}

int cxd5602_put_micgain3_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetMicGain Command;
	HI_AudioExtResult Result;
	int ret;
	int gain;
	int set_cnt;

	gain = ucontrol->value.integer.value[0];

	if (gain < GAIN_MIN || gain > GAIN_MAX) {
		printk(KERN_ERR "gain Param ERROR(%d)\n", gain);
		return -EINVAL;
	}

	printk(KERN_INFO "cxd5602_put_micgain3_control %d\n", gain);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_MIC_GAIN3;
	Command.MicGain = gain;
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

	return 0;
}

int cxd5602_info_scumicgain_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_info  *uinfo)
{

	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = mc->min;
	uinfo->value.integer.max = mc->max;

	return 0;

}

int cxd5602_get_scumicgain_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdGetScuMicGain Command;
	HI_AudioExtResScuMicGain Result;
	int ret;
	int set_cnt;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_GET_SCU_MIC_GAIN;
	ret = HI_AudioExtCommand(&Command.Header, &Result.Header, sizeof(HI_AudioExtResScuMicGain), &set_cnt);
	if (ret != 0) {
		printk(KERN_ERR "HI_AudioExtCommand ERROR(%d)\n", ret);
		return ret;
	}

	if (set_cnt < sizeof(HI_AudioExtResScuMicGain)) {
		printk(KERN_ERR "HI_AudioExtCommand result size ERROR(%d:%d)\n", sizeof(HI_AudioExtResScuMicGain), set_cnt);
		return ret;
	}

	if (Result.ResultCode != HI_AUDIO_EXT_RESULT_OK) {
		printk(KERN_ERR "HI_AudioExtCommand result ERROR(%d)\n", Result.ResultCode);
		return ret;
	}

	printk(KERN_INFO "%s gain=%d\n", __FUNCTION__, Result.ScuMicGain);

	ucontrol->value.integer.value[0] = Result.ScuMicGain;

	return 0;
}

int cxd5602_put_scumicgain_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	HI_AudioExtCmdSetScuMicGain Command;
	HI_AudioExtResult Result;
	int ret;
	int gain;
	int set_cnt;

	gain = ucontrol->value.integer.value[0];

	if (gain < SCU_GAIN_MIN || gain > SCU_GAIN_MAX) {
		printk(KERN_ERR "gain Param ERROR(%d)\n", gain);
		return -EINVAL;
	}

	printk(KERN_INFO "cxd5602_put_sucmicgain_control %d\n", gain);

	if (!cxd5602_codec_hifc_is_ready()) {
		printk(KERN_INFO "%s: spz is not ready\n", __FUNCTION__);
		return -1;
	}

	Command.Header.Header.payloadSize = sizeof(Command) - sizeof(Command.Header.Header);
	Command.Header.CmdCode = HI_AUDIO_EXT_CMD_SET_SCU_MIC_GAIN;
	Command.ScuMicGain = gain;
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

	return 0;
}

