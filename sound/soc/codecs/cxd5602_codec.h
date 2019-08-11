#ifndef __HI_AUDIO_CODEC_H__
#define __HI_AUDIO_CODEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sound/soc.h>

#include "../../../drivers/char/hifc/hi_audio_ext_ngl.h"

extern int cxd5602_codec_hifc_init(const struct device *dev, void (*trigger_handler)(int, int));
extern int cxd5602_codec_hifc_is_ready(void);
extern int HI_AudioExtCommand(HI_AudioExtCmdHeader *cmd, HI_AudioExtCmdHeader *result, int req_size, int *set_size);

int cxd5602_get_volume_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_volume_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_raw_data(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_raw_data(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_raw_data_info(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_info  *uinfo);
int cxd5602_get_raw_data_tlv(
	struct snd_kcontrol *kcontrol, int op_flag,
	unsigned int size, unsigned int __user *_tlv);

int cxd5602_get_mute(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_mute(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_xLoud(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_xLoud(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_env_adapt(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_env_adapt(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_mode(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_mode(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_output(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_output(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_vad(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_vad(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_micsel(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_micsel(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_micgain0_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_micgain0_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_micgain1_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_micgain1_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_micgain2_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_micgain2_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_get_micgain3_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_micgain3_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

int cxd5602_info_scumicgain_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_info  *uinfo);
int cxd5602_get_scumicgain_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
int cxd5602_put_scumicgain_control(
	struct snd_kcontrol       *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

#ifdef __cplusplus
}
#endif

#endif //__HI_AUDIO_CODEC_H__
