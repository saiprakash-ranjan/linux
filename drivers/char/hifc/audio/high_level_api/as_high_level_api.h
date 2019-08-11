/*
* Copyright 2015 Sony Corporation
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions, and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/
/* This File is tentative.  */

#ifndef	AS_HIGH_LEVEL_API_H
#define AS_HIGH_LEVEL_API_H

/* API Documents creater with Doxgen */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* command code */
#define	AUDCMD_INITBBMODE		((uint8_t)0x01)
#define	AUDCMD_GETSTATUS		((uint8_t)0x02)

#define	AUDCMD_SETBBPARAM		((uint8_t)0x10)
#define	AUDCMD_STARTVOICECOMMAND	((uint8_t)0x11)
#define	AUDCMD_STOPVOICECOMMAND		((uint8_t)0x12)
#define	AUDCMD_INITMFE			((uint8_t)0x13)
#define	AUDCMD_STARTBB			((uint8_t)0x14)
#define	AUDCMD_STOPBB			((uint8_t)0x15)
#define	AUDCMD_DEBUGMFEPARAM		((uint8_t)0x1F)

#define	AUDCMD_PLAYPLAYER		((uint8_t)0x21)
#define	AUDCMD_STOPPLAYER		((uint8_t)0x22)
#define	AUDCMD_PAUSEPLAYER		((uint8_t)0x23)
#define	AUDCMD_NEXTPLAY			((uint8_t)0x24)
#define	AUDCMD_PREVPLAY			((uint8_t)0x25)
#define	AUDCMD_SETPLAYERPARAM		((uint8_t)0x2A)
#define	AUDCMD_INITMPP			((uint8_t)0x2B)
#define	AUDCMD_SETMPPPARAM		((uint8_t)0x2C)
#define	AUDCMD_DEBUGMPPPARAM		((uint8_t)0x2F)

#define AUDCMD_STARTREC			((uint8_t)0x31)
#define AUDCMD_STOPREC			((uint8_t)0x32)
#define AUDCMD_PAUSEREC			((uint8_t)0x33)

#define	AUDCMD_SETWAITKEYSTATUS		((uint8_t)0x40)
#define	AUDCMD_SETREADYSTATUS		((uint8_t)0x41)
#define	AUDCMD_SETBASEBANDSTATUS	((uint8_t)0x42)
#define	AUDCMD_SETBBACTIVESTATUS	(AUDCMD_SETBASEBANDSTATUS)	/* Be removed in future */
#define	AUDCMD_SETPLAYERSTATUS		((uint8_t)0x43)
#define	AUDCMD_SETRECORDERSTATUS	((uint8_t)0x44)

#define	AUDCMD_SETFINDTRIGGERCALLBACK	((uint8_t)0x50)

/* const for baseband */
#define AS_ADN_MIC_GAIN_HOLD            (255)
#define AS_ADN_MIC_GAIN_MAX             (150)
#define AS_ADN_MIC_GAIN_MIN             (0)

#define AS_ADN_PGA_GAIN_HOLD            (255)
#define AS_ADN_PGA_GAIN_MAX             (60)
#define AS_ADN_PGA_GAIN_MIN             (0)

#define AS_ADN_VGAIN_HOLD               (127)
#define AS_ADN_VGAIN_MAX                (60)
#define AS_ADN_VGAIN_MIN                (-95)

#define AS_AC_MTBR_GAIN_HOLD            (255)
#define AS_AC_MTBR_GAIN_MAX             (0)
#define AS_AC_MTBR_GAIN_MIN             (-630)
#define AS_AC_MTBR_GAIN_MUTE            (-635)

#define AS_AC_CIC_GAIN_HOLD             (255)
#define AS_AC_CIC_GAIN_MAX              (0)
#define AS_AC_CIC_GAIN_MIN              (-7850)
#define AS_AC_CIC_GAIN_MUTE             (-7855)
#define AS_AC_CIC_NUM                   (4)

#define AS_AC_CS_VOL_HOLD               (255)
#define AS_AC_CS_VOL_MAX                (-195)
#define AS_AC_CS_VOL_MIN                (-825)
#define AS_AC_CS_VOL_INF_ZERO           (-830)

#define AS_AC_CODEC_VOL_HOLD            (255)
#define AS_AC_CODEC_VOL_MAX             (120)
#define AS_AC_CODEC_VOL_MIN             (-1020)
#define AS_AC_CODEC_VOL_MUTE            (-1025)
#define AS_AC_CODEC_VOL_DAC             (-20)

#define AS_ADN_HP_VOL_HOLD              (255)
#define AS_ADN_HP_VOL_MAX               (0)
#define AS_ADN_HP_VOL_MIN               (-280)
#define AS_ADN_HP_VOL_MUTE              (-285)

#define AS_AC_BEEP_VOL_HOLD             (255)
#define AS_AC_BEEP_VOL_MAX              (0)
#define AS_AC_BEEP_VOL_MIN              (-90)
#define AS_AC_BEEP_VOL_MUTE             (-93)

#define AS_AC_MIC_CHANNEL_HOLD          (255)
#define AS_AC_MIC_CHANNEL_MAX           (8)
#define AS_AC_MIC_CHANNEL_MIN           (0)

/* enum for baseband */
typedef enum {
	AS_ADN_XTAL_HOLD = 0,
	AS_ADN_XTAL_8_192MHZ,
	AS_ADN_XTAL_24_576MHZ,
	AS_ADN_XTAL_49_152MHZ,
	AS_ADN_XTAL_NUM
} AsAdnXtalSelId;

typedef enum {
	AS_CLK_MODE_HOLD = 0,
	AS_CLK_MODE_LOWPOWER,
	AS_CLK_MODE_NORMAL,
	AS_CLK_MODE_HIRES,
	AS_CLK_MODE_NUM
} AsClkModeId;

typedef enum {
	AS_SER_MODE_HOLD = 0,
	AS_SER_MODE_128FS,
	AS_SER_MODE_64FS,
	AS_SER_MODE_NUM
} AsSerModeId;

typedef enum {
	AS_ADN_IN_HOLD = 0,
	AS_ADN_IN_OFF,
	AS_ADN_IN_AMIC,
	AS_ADN_IN_DMIC,
	AS_ADN_IN_BOTH,
	AS_ADN_IN_NUM
} AsAdnInputDeviceSelId;

typedef enum {
	AS_ADN_OUT_HOLD = 0,
	AS_ADN_OUT_OFF,
	AS_ADN_OUT_HP,
	AS_ADN_OUT_EP,
	AS_ADN_OUT_PWM,
	AS_ADN_OUT_HP_PWM,
	AS_ADN_OUT_EP_PWM,
	AS_ADN_OUT_NUM
} AsAdnOutputDeviceSelId;

typedef enum {
	AS_ADN_PWM_HOLD = 0,
	AS_ADN_PWM_OFF,
	AS_ADN_PWM_LN,
	AS_ADN_PWM_LP,
	AS_ADN_PWM_RN,
	AS_ADN_PWM_RP,
	AS_ADN_PWM_NUM
} AsAdnPwmSelId;

typedef enum {
	AS_ADN_MICDET_BIAS_HIZ = 0,
	AS_ADN_MICDET_BIAS_EXT,
	AS_ADN_MICDET_BIAS_100K,
	AS_ADN_MICDET_BIAS_20K,
	AS_ADN_MICDET_BIAS_NUM
} AsAdnMicDetBiasId;

typedef enum {
	AS_ADN_IO_DS_HOLD = 0,
	AS_ADN_IO_DS_WEAKEST,
	AS_ADN_IO_DS_WEAKER,
	AS_ADN_IO_DS_STRONGER,
	AS_ADN_IO_DS_STRONGEST,
	AS_ADN_IO_DS_NUM
} AsAdnIoDs;

typedef enum {
	AS_AC_LOWEMI_HOLD = 0,
	AS_AC_LOWEMI_4MA,
	AS_AC_LOWEMI_2MA,
	AS_AC_LOWEMI_NUM
} AsAcLowemi;

typedef enum {
	AS_AC_I2S_DEVICE_HOLD = 0,
	AS_AC_I2S_DEVICE_I2S_MASTER,
	AS_AC_I2S_DEVICE_I2S_SLAVE,
	AS_AC_I2S_DEVICE_SLIMBUS_MASTER,
	AS_AC_I2S_DEVICE_SLIMBUS_SLAVE,
	AS_AC_I2S_DEVICE_NUM
} AsAcI2sDeviceId;

typedef enum {
	AS_AC_I2S_IN_CHANNEL_HOLD = 0,
	AS_AC_I2S_IN_CHANNEL_0CH,
	AS_AC_I2S_IN_CHANNEL_2CH,
	AS_AC_I2S_IN_CHANNEL_4CH,
	AS_AC_I2S_IN_CHANNEL_NUM
} AsAcI2sInputChannelId;

typedef enum {
	AS_AC_I2S_IN_FS_HOLD = 0,
	AS_AC_I2S_IN_FS_8000HZ,
	AS_AC_I2S_IN_FS_11025HZ,
	AS_AC_I2S_IN_FS_12000HZ,
	AS_AC_I2S_IN_FS_16000HZ,
	AS_AC_I2S_IN_FS_22050HZ,
	AS_AC_I2S_IN_FS_24000HZ,
	AS_AC_I2S_IN_FS_32000HZ,
	AS_AC_I2S_IN_FS_44100HZ,
	AS_AC_I2S_IN_FS_48000HZ,
	AS_AC_I2S_IN_FS_64000HZ,
	AS_AC_I2S_IN_FS_88200HZ,
	AS_AC_I2S_IN_FS_96000HZ,
	AS_AC_I2S_IN_FS_128000HZ,
	AS_AC_I2S_IN_FS_176400HZ,
	AS_AC_I2S_IN_FS_192000HZ,
	AS_AC_I2S_IN_FS_NUM
} AsAcI2sInputFsId;

typedef enum {
	AS_AC_I2S_IN_FORMAT_HOLD = 0,
	AS_AC_I2S_IN_FORMAT_I2S,
	AS_AC_I2S_IN_FORMAT_LEFT,
	AS_AC_I2S_IN_FORMAT_NUM
} AsAcI2sInputFormatId;

typedef enum {
	AS_AC_I2S_OUT_CHANNEL_HOLD = 0,
	AS_AC_I2S_OUT_CHANNEL_0CH,
	AS_AC_I2S_OUT_CHANNEL_2CH,
	AS_AC_I2S_OUT_CHANNEL_4CH,
	AS_AC_I2S_OUT_CHANNEL_NUM
} AsAcI2sOutputChannelId;

typedef enum {
	AS_AC_I2S_OUT_FS_HOLD = 0,
	AS_AC_I2S_OUT_FS_48000HZ,
	AS_AC_I2S_OUT_FS_96000HZ,
	AS_AC_I2S_OUT_FS_192000HZ,
	AS_AC_I2S_OUT_FS_NUM
} AsAcI2sOutputFsId;

typedef enum {
	AS_AC_I2S_OUT_FORMAT_HOLD = 0,
	AS_AC_I2S_OUT_FORMAT_I2S,
	AS_AC_I2S_OUT_FORMAT_LEFT,
	AS_AC_I2S_OUT_FORMAT_NUM
} AsAcI2sOutputFormatId;

typedef enum {
	AS_AC_CIC_IN_SEL_HOLD = 0,
	AS_AC_CIC_IN_SEL_ADONIS,
	AS_AC_CIC_IN_SEL_DMIC,
	AS_AC_CIC_IN_SEL_NUM
} AsAcCicInputSelId;

typedef enum {
	AS_AC_CIC_GAIN_MODE_MATSUBARA = 0,
	AS_AC_CIC_GAIN_MODE_CIC,
	AS_AC_CIC_GAIN_MODE_NUM
} AsAcCicGainModeId;

typedef enum {
	AS_AC_ALC_SPC_SEL_OFF = 0,
	AS_AC_ALC_SPC_SEL_ALC,
	AS_AC_ALC_SPC_SEL_SPC,
	AS_AC_ALC_SPC_SEL_NUM
} AsAcAlcSpcSelId;

typedef enum {
	AS_AC_DNC_SEL_OFF = 0,
	AS_AC_DNC_SEL_DNC1,
	AS_AC_DNC_SEL_DNC2,
	AS_AC_DNC_SEL_BOTH,
	AS_AC_DNC_SEL_NUM
} AsAcDncSelId;

typedef enum {
	AS_AC_BB_MODE_HOLD = 0,
	AS_AC_BB_MODE_THR,
	AS_AC_BB_MODE_DMA_24_INTR,
	AS_AC_BB_MODE_DMA_24_POLL,
	AS_AC_BB_MODE_DMA_16_INTR,
	AS_AC_BB_MODE_DMA_16_POLL,
	AS_AC_BB_MODE_NUM
} AsAcBBModeId;

typedef enum {
	AS_OUTPUT_DEVICE_I2S2CH = 0x33,
	AS_OUTPUT_DEVICE_NUM
} AsOutputDevice;

typedef enum {
	AS_INPUT_DEVICE_AMIC4CH = 0x000F,
	AS_INPUT_DEVICE_NUM
} AsInputDevice;

typedef enum {
	AS_MPP_OUTPUT_I2SIN = 0,
	AS_SP_OUTPUT_DATA_NUM
} AsSpOutputData;

typedef enum {
	AS_MFE_OUTPUT_MICSIN = 0,
	AS_MIC_THROUGH,
	AS_I2S_OUTPUT_DATA_NUM
} AsI2sOutputData;

typedef enum {
	AS_SELECT_MIC0_OR_MIC2 = 0x05,
	AS_SELECT_MIC1_OR_MIC3 = 0x0A,
	AS_SELECT_OUTPUT_MIC_NUM
} AsSelectOutputMic;

typedef enum {
	AS_MFE_INPUT_FS_16K = 0,
	AS_MFE_INPUT_FS_NUM
} AsMfeInputFsId;

typedef enum {
	AS_MFE_MIC_CH_NUM_DEFAULT = 4,
	AS_MFE_MIC_CH_NUM
} AsMfeMicChNum;

typedef enum {
	AS_MFE_REF_CH_NUM_DEFAULT = 2,
	AS_MFE_REF_CH_NUM
} AsMfeRefChNum;

typedef enum {
	AS_MFE_MODE_RECOGNITION = 0,
	AS_MFE_MODE_SPEAKING,
	AS_MFE_MODE_NUM
} AsMfeModeId;

typedef enum {
	AS_NOINCLUDE_ECHOCANCEL = 0,
	AS_INCLUDE_ECHOCANCEL,
	AS_MFE_INC_ECHOCANCEL_NUM
} AsMfeIncludeEchoCancel;

typedef enum {
	AS_DISABLE_ECHOCANCEL_OFF = 0,
	AS_ENABLE_ECHOCANCEL,
	AS_MFE_ENBL_ECHOCANCEL_NUM
} AsMfeEnableEchoCancel;

typedef enum {
	AS_MPP_MODE_XLOUD_ONLY = 0,
	AS_MPP_MODE_NUM
} AsMppModeId;

typedef enum {
	AS_MPP_OUTPUT_FS_48K = 0,
	AS_MPP_OUTPUT_FS_NUM
} AsMppOutputFsId;

typedef enum {
	AS_MPP_OUTPUT_CH_DEFAULT = 2,
	AS_MPP_OUTPUT_CH_NUM
} AsMppOutputChNum;

typedef enum {
	AS_MPP_COEF_SPEAKER = 0,
	AS_MPP_COEF_HEADPHONE,
	AS_MPP_COEF_NUM
} AsMppCoefModeId;

typedef enum {
	AS_MPP_XLOUD_MODE_NORMAL = 0,
	AS_MPP_XLOUD_MODE_SPEAKING,
	AS_MPP_XLOUD_MODE_DISABLE,
	AS_MPP_XLOUD_MODE_NUM
} AsMppXloudModeId;

typedef enum {
	AS_MPP_EAX_DISABLE = 0,
	AS_MPP_EAX_ENABLE,
	AS_MPP_EAX_NUM
} AsMppEaxModeId;

#define CHECK_XLOUD_VOLUME_RANGE(vol)		(bool)(((0 <= (vol)) && ((vol) <= 59)) ? true : false)

typedef enum {
	AS_SET_BBSTS_WITH_MFE_NONE = 0,
	AS_SET_BBSTS_WITH_MFE_ACTIVE,
	AS_SET_BBSTS_WITH_MFE_NUM
} AsSetBBStsWithMfe;

typedef enum {
	AS_SET_BBSTS_WITH_VCMD_NONE = 0,
	AS_SET_BBSTS_WITH_VCMD_ACTIVE,
	AS_SET_BBSTS_WITH_VCMD_NUM
} AsSetBBStsWithVoiceCommand;

typedef enum {
	AS_SET_BBSTS_SELI2SOUT_MFE = 0,
	AS_SET_BBSTS_SELI2SOUT_MIC,
	AS_SET_BBSTS_SELI2SOUT_NUM
} AsSetBBStsSelectI2sOut;

typedef enum {
	AS_VOICE_COMMAND_USUAL = 0,
	AS_VOICE_COMMAND_VAD_ONLY,
	AS_VOICE_COMMAND_NUM
} AsVoiceCommandVadOnly;

typedef enum {
	AS_SETPLAYER_INPUTDEVICE_EMMC = 0,
	AS_SETPLAYER_INPUTDEVICE_A2DPFIFO,
	AS_SETPLAYER_INPUTDEVICE_I2SINPUT,
	AS_SETPLAYER_INPUTDEVICE_NUM
} AsSetPlayerInputDevice;

typedef enum {
	AS_SETPLAYER_OUTPUTDEVICE_SPHP = 0,
	AS_SETPLAYER_OUTPUTDEVICE_I2SOUTPUT,
	AS_SETPLAYER_OUTPUTDEVICE_A2DPFIFO,
	AS_SETPLAYER_OUTPUTDEVICE_NUM
} AsSetPlayerOutputDevice;

typedef enum {
	AS_SETPLAYER_PLAYFILTER_FILE = 0,
	AS_SETPLAYER_PLAYFILTER_ALBUM,
	AS_SETPLAYER_PLAYFILTER_ARTIST,
	AS_SETPLAYER_PLAYFILTER_NUM
} AsSetPlayerPlayFilter;

typedef enum {
	AS_SETPLAYER_PLAYMODE_NORMAL = 0,
	AS_SETPLAYER_PLAYMODE_SHUFFLE,
	AS_SETPLAYER_PLAYMODE_PLAY_ONE,
	AS_SETPLAYER_PLAYMODE_NUM
} AsSetPlayerPlayModeId;

typedef enum {
	AS_SETPLAYER_REPEAT_DISABLE = 0,
	AS_SETPLAYER_REPEAT_ENABLE,
	AS_SETPLAYER_REPEATMODE_NUM
} AsSetPlayerRepeatModeId;

typedef enum {
	AS_SETRECDR_STS_INPUTDEVICE_MIC_A = 0,
	AS_SETRECDR_STS_INPUTDEVICE_MIC_D,
	AS_SETRECDR_STS_INPUTDEVICE_I2S_IN,
	AS_SETRECDR_STS_INPUTDEVICE_NUM
} AsSetRecorderStsInputDevice;

typedef enum {
	AS_SETRECDR_STS_BITLENGTH_16 = 0,
	AS_SETRECDR_STS_BITLENGTH_24,
	AS_SETRECDR_STS_BITLENGTH_NUM
} AsSetRecorderStsBitLength;

typedef enum {
	AS_SETRECDR_STS_CHNL_MONO = 0,
	AS_SETRECDR_STS_CHNL_STEREO,
	AS_SETRECDR_STS_CHNL_4CH,
	AS_SETRECDR_STS_CHNL_6CH,
	AS_SETRECDR_STS_CHNL_8CH,
	AS_SETRECDR_STS_CHNL_NUM
} AsSetRecorderStsChannelNumberIndex;

typedef enum {
	AS_SETRECDR_STS_SAMPLING_16K = 0,
	AS_SETRECDR_STS_SAMPLING_48K,
	AS_SETRECDR_STS_SAMPLING_96K,
	AS_SETRECDR_STS_SAMPLING_192K,
	AS_SETRECDR_STS_SAMPLING_NUM
} AsSetRecorderStsSamplingRateIndex;

typedef enum {
	AS_SETRECDR_STS_OUTPUTDEVICE_EMMC = 0,
	AS_SETRECDR_STS_OUTPUTDEVICE_NUM
} AsSetRecorderStsOutputDevice;

/* result code */
#define	AUDRLT_INITBBCMPLT		(uint8_t)0x01
#define	AUDRLT_NOTIFYSTATUS		(uint8_t)0x02
#define	AUDRLT_SETBBPARAMCMPLT		(uint8_t)0x10
#define	AUDRLT_STARTVOICECOMMANDCMPLT	(uint8_t)0x11
#define	AUDRLT_STOPVOICECOMMANDCMPLT	(uint8_t)0x12
#define	AUDRLT_INITMFECMPLT		(uint8_t)0x13
#define	AUDRLT_STARTBBCMPLT		(uint8_t)0x14
#define	AUDRLT_STOPBBCMPLT		(uint8_t)0x15
#define	AUDRLT_DEBUGMFECMPLT		(uint8_t)0x1F
#define	AUDRLT_PLAYCMPLT		(uint8_t)0x21
#define	AUDRLT_STOPCMPLT		(uint8_t)0x22
#define	AUDRLT_PAUSECMPLT		(uint8_t)0x23
#define	AUDRLT_NEXTCMPLTE		(uint8_t)0x24
#define	AUDRLT_PREVCMPLTE		(uint8_t)0x25
#define	AUDRLT_SETPLAYERCMPLT		(uint8_t)0x2A
#define	AUDRLT_INITMPPCMPLT		(uint8_t)0x2B
#define	AUDRLT_SETMPPCMPLT		(uint8_t)0x2C
#define	AUDRLT_DEBUGMPPCMPLT		(uint8_t)0x2F
#define	AUDRLT_RECCMPLT			(uint8_t)0x31
#define	AUDRLT_STOPRECCMPLT		(uint8_t)0x32
#define	AUDRLT_PAUSERECCMPLT		(uint8_t)0x33
#define	AUDRLT_STATUSCHANGED		(uint8_t)0x40
#define	AUDRLT_SETCALLBACKCMPLT		(uint8_t)0x50
#define	AUDRLT_ERRORRESPONSE		(uint8_t)0x80
#define	AUDRLT_ERRORATTENTION		(uint8_t)0x81

#define	LENGTH_AUDRLT			((uint8_t)2)

/* sub code for INITBBMODE command(0x01) */
#define	SUB_INITBB_ADNIOSET		(uint8_t)0x10
#define	SUB_INITBB_ADNPLGDETSET		(uint8_t)0x11
#define	SUB_INITBB_ACIOSET		(uint8_t)0x20
#define	SUB_INITBB_ACDSPSET		(uint8_t)0x21
#define	SUB_INITBB_ACDEQSET		(uint8_t)0x22
#define	SUB_INITBB_ACDNCPRMSET		(uint8_t)0x30
#define	SUB_INITBB_ACDNCRAMSET		(uint8_t)0x31

/* sub code for SETBBPARAM command(0x10) */
#define	SUB_SETBB_ADNIOSET		(uint8_t)0x10
#define	SUB_SETBB_ACIOSET		(uint8_t)0x20
#define	SUB_SETBB_ACDSPSET		(uint8_t)0x21
#define	SUB_SETBB_ACDEQSET		(uint8_t)0x22
#define	SUB_SETBB_ACDNCPRMSET		(uint8_t)0x30
#define	SUB_SETBB_ACDNCRAMSET		(uint8_t)0x31
#define	SUB_SETBB_AUDVOLSET		(uint8_t)0x40

/* Packet length of sub code for INITBBMODE command (0x01) */
#define	LENGTH_SUB_INITBB_ADNIOSET	((uint8_t)8)
#define	LENGTH_SUB_INITBB_ADNPLGDETSET	((uint8_t)7)
#define	LENGTH_SUB_INITBB_ACIOSET	((uint8_t)10)
#define	LENGTH_SUB_INITBB_ACDSPSET	((uint8_t)8)
#define	LENGTH_SUB_INITBB_ACDEQSET	((uint8_t)8)
#define	LENGTH_SUB_INITBB_ACDNCPRMSET	((uint8_t)8)
#define	LENGTH_SUB_INITBB_ACDNCRAMSET	((uint8_t)6)

/* Packet length of sub code for SETBBPARAM command (0x10) */
#define	LENGTH_SUB_SETBB_ADNIOSET	((uint8_t)8)
#define	LENGTH_SUB_SETBB_ACIOSET	((uint8_t)10)
#define	LENGTH_SUB_SETBB_ACDSPSET	((uint8_t)8)
#define	LENGTH_SUB_SETBB_ACDEQSET	((uint8_t)8)
#define	LENGTH_SUB_SETBB_ACDNCPRMSET	((uint8_t)8)
#define	LENGTH_SUB_SETBB_ACDNCRAMSET	((uint8_t)6)
#define	LENGTH_SUB_SETBB_AUDVOLSET	((uint8_t)6)

/* sub code for SETMPPPARAM command(0x2C) */
#define	SUB_SETMPP_COMMON		((uint8_t)0x00)
#define	SUB_SETMPP_XLOUD		((uint8_t)0x01)

#define	LENGTH_INITMFE			(4)
#define	LENGTH_STARTBB			(3)
#define	LENGTH_STOPBB			(3)
#define	LENGTH_DEBUGMFEPARAM		(4)
#define	LENGTH_INITMPP			(4)
#define	LENGTH_SUB_SETMPP_COMMON	(4)
#define	LENGTH_SUB_SETMPP_XLOUD		(4)
#define	LENGTH_DEBUGMPPPARAM		(4)

/* Packet length of player command */
#define LENGTH_SET_PLAYER_STATUS	(5)
#define LENGTH_PLAY_PLAYER		(2)
#define LENGTH_STOP_PLAYER		(2)
#define LENGTH_PAUSE_PLAYER		(2)
#define LENGTH_NEXT_PLAY		(2)
#define LENGTH_PREVIOUS_PLAY		(2)
#define LENGTH_SET_READY_STATUS		(2)

#define LENGTH_SET_PLAYER_PARAM		(4)
#define LENGTH_START_VOICE_COMMAND	(4)
#define LENGTH_STOP_VOICE_COMMAND	(2)
#define LENGTH_SET_RECORDER_STATUS	(5)
#define LENGTH_START_RECORDER		(2)
#define LENGTH_STOP_RECORDER		(2)
#define LENGTH_SET_WAITKEY_STATUS	(4)
#define LENGTH_SET_BASEBAND_STATUS	(4)
#define LENGTH_SET_BBACTIVE_STATUS	(LENGTH_SET_BASEBAND_STATUS)	/* Be removed in future */
#define LENGTH_SET_FIND_TRIGGER_CALLBACK	(2)

/* Audio command header */
typedef struct {
	uint8_t		reserved;
	uint8_t		sub_code;
	uint8_t		command_code;
	uint8_t		packet_length;
} AudioCommandHeader;

/* Audio result header */
typedef struct {
	uint8_t		reserved;
	uint8_t		sub_code;
	uint8_t		result_code;
	uint8_t		packet_length;
} AudioResultHeader;

/* INITBBMODE */
typedef struct {
	uint8_t		mic_bias_sel;
	uint8_t		ser_mode;
	uint8_t		clk_mode;
	uint8_t		xtal_sel;

	uint8_t		mic_gain_a;
	uint8_t		mic_gain_b;
	uint8_t		mic_gain_c;
	uint8_t		mic_gain_d;

	uint8_t		pga_gain_a;
	uint8_t		pga_gain_b;
	uint8_t		pga_gain_c;
	uint8_t		pga_gain_d;

	int8_t		vgain_a;
	int8_t		vgain_b;
	int8_t		vgain_c;
	int8_t		vgain_d;

	uint32_t	mic_channel_sel;

	uint8_t		pwm_sel_a;
	uint8_t		pwm_sel_b;
	uint8_t		output_device_sel;
	uint8_t		input_device_sel;

	uint8_t		gpo_ds;
	uint8_t		ad_data_ds;
	uint8_t		dmic_clk_ds;
	uint8_t		mclk_ds;
} AdnIOSet;

typedef struct {
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		pdet_time;
	uint8_t		pdet_num;

	uint8_t		en_mic_det_a;
	uint8_t		en_mic_det_b;
	uint8_t		en_but_det;
	uint8_t		en_plug_det;

	uint8_t		mic_det_bias_a;
	uint8_t		mic_det_bias_b;
	uint8_t		mic_bias_sel;
	uint8_t		en_ilgl_det;

	uint8_t		th_vs_4;
	uint8_t		reserved3;
	uint8_t		th_vpl;
	uint8_t		th_vph;

	uint8_t		th_vs_0;
	uint8_t		th_vs_1;
	uint8_t		th_vs_2;
	uint8_t		th_vs_3;

	uint8_t		reserved4;
	uint8_t		reserved5;
	uint8_t		reserved6;
	uint8_t		output_device_sel;
} AdnPlgDetSet;

typedef struct {
	uint8_t		bb_mode;
	uint8_t		ser_mode;
	uint8_t		clk_mode;
	uint8_t		reserved1;

	uint8_t		i2s_input_format;
	uint8_t		i2s_input_fs;
	uint8_t		i2s_input_channel;
	uint8_t		i2s_device;

	uint8_t		i2s_output_format;
	uint8_t		i2s_output_fs;
	uint8_t		i2s_output_channel;
	uint8_t		reserved2;

	uint8_t		cic_gain_mode;
	uint8_t		reserved3;
	uint8_t		reserved4;
	uint8_t		cic_input_sel;

	int16_t		cic_gain1_r;
	int16_t		cic_gain1_l;

	int16_t		cic_gain2_r;
	int16_t		cic_gain2_l;

	int16_t		cic_gain3_r;
	int16_t		cic_gain3_l;

	int16_t		cic_gain4_r;
	int16_t		cic_gain4_l;

	uint8_t		pdm_lowemi;
	uint8_t		i2s_lowemi;
	uint8_t		reserved6;
	uint8_t		mic_dma_channel;
} AcIOSet;

typedef struct {
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		alc_spc_sel;

	uint8_t		reserved4;
	uint8_t		reserved5;
	uint8_t		reserved6;
	uint8_t		reserved7;

	uint8_t		reserved8;
	uint8_t		reserved9;
	uint8_t		reserved10;
	uint8_t		reserved11;

	uint8_t		reserved12;
	uint8_t		reserved13;
	uint8_t		reserved14;
	uint8_t		reserved15;

	uint8_t		reserved16;
	uint8_t		reserved17;
	uint8_t		reserved18;
	uint8_t		reserved19;

	uint8_t		reserved20;
	uint8_t		reserved21;
	uint8_t		reserved22;
	uint8_t		reserved23;

	int16_t		clear_stereo_vol;
	uint8_t		reserved24;
	uint8_t		reserved25;
} AcDspSet;

typedef struct {
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		deq_en;

	uint32_t	deq_coef_band1_addr;
	uint32_t	deq_coef_band2_addr;
	uint32_t	deq_coef_band3_addr;
	uint32_t	deq_coef_band4_addr;
	uint32_t	deq_coef_band5_addr;
	uint32_t	deq_coef_band6_addr;
} AcDeqSet;

typedef struct {
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		dnc_sel;

	uint8_t		reserved4;
	uint8_t		reserved5;
	uint8_t		reserved6;
	uint8_t		reserved7;

	uint8_t		reserved8;
	uint8_t		reserved9;
	uint8_t		reserved10;
	uint8_t		reserved11;

	uint8_t		reserved12;
	uint8_t		reserved13;
	uint8_t		reserved14;
	uint8_t		reserved15;

	uint8_t		reserved16;
	uint8_t		reserved17;
	uint8_t		reserved18;
	uint8_t		reserved19;

	uint8_t		reserved20;
	uint8_t		reserved21;
	uint8_t		reserved22;
	uint8_t		reserved23;

	uint8_t		reserved24;
	uint8_t		reserved25;
	uint8_t		reserved26;
	uint8_t		reserved27;
} AcDncPrmSet;

typedef struct {
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		dnc_sel;

	uint32_t	dnc_iram1_addr;
	uint32_t	dnc_cram1_addr;
	uint32_t	dnc_iram2_addr;
	uint32_t	dnc_cram2_addr;
} AcDncRamSet;

#pragma anon_unions
typedef struct {
	union {
		/* Adonis Input / Output setting */
		AdnIOSet	adn_io_set;

		/* Adonis Plug Detection setting */
		AdnPlgDetSet	adn_plg_det_set;

		/* Audio Codec Input / Output setting */
		AcIOSet		ac_io_set;

		/* Audio Codec ALC / SPC / ClearStereo setting */
		AcDspSet	ac_dsp_set;

		/* Audio Codec DEQ setting */
		AcDeqSet	ac_deq_set;

		/* Audio Codec DNC Parameter setting */
		AcDncPrmSet	ac_dnc_prm_set;

		/* Audio Codec DNC RAM setting */
		AcDncRamSet	ac_dnc_ram_set;
	};
} InitBBParam;

/* SETBBPARAM */
typedef struct {
	int16_t		sdin1_vol;
	uint8_t		reserved1;
	uint8_t		reserved2;

	int16_t		sdin2_vol;
	uint8_t		reserved3;
	uint8_t		reserved4;

	int16_t 	dac_vol;
	uint8_t		reserved5;
	uint8_t		reserved6;

	int16_t		hp_vol;
	uint8_t		reserved7;
	uint8_t		reserved8;

	int16_t		beep_vol;
	uint8_t		reserved9;
	uint8_t		beep_en;
} AudVolSet;

#pragma anon_unions
typedef struct {
	union {
		/* Adonis Input / Output setting */
		AdnIOSet	adn_io_set;

		/* Audio Codec Input / Output setting */
		AcIOSet		ac_io_set;

		/* Audio Codec ALC / SPC / ClearStereo setting */
		AcDspSet	ac_dsp_set;

		/* Audio Codec DEQ setting */
		AcDeqSet	ac_deq_set;

		/* Audio Codec DNC Parameter setting */
		AcDncPrmSet	ac_dnc_prm_set;

		/* Audio Codec DNC RAM setting */
		AcDncRamSet	ac_dnc_ram_set;

		/* Audio Volume setting */
		AudVolSet	aud_vol_set;
	};
} SetBBParam;

/* GetStatus */
typedef struct {
	uint32_t	reserved;
} GetStatus;


/* InitMFE */
typedef struct {
	uint8_t		reserved1;
	uint8_t		input_fs;			/* AsMfeInputFsId */
	uint8_t		ref_channel_num;		/* AsMfeMicChNum */
	uint8_t		mic_channel_num;		/* AsMfeRefChNum */

	uint8_t		enable_echocancel;		/* AsMfeEnableEchoCancel */
	uint8_t		include_echocancel;		/* AsMfeIncludeEchoCancel */
	uint8_t		reserved2;
	uint8_t		mfe_mode;			/* AsMfeModeId */

	uint32_t	config_table;
} InitMFEParam;

/* StartBB */
typedef struct {
	uint8_t		output_device;			/* AsOutputDevice */
	uint8_t		reserved1;
	uint16_t	input_device;			/* AsInputDevice */

	uint8_t		select_output_mic;		/* AsSelectOutputMic */
	uint8_t		reserved2;
	uint8_t		I2S_output_data;		/* AsI2sOutputData */
	uint8_t		SP_output_data;			/* AsSpOutputData */
} StartBBParam;

/* StopBB */
typedef struct {
	uint8_t		stop_device;
	uint8_t		reserved1;
	uint16_t	keyword;

	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		reserved4;
	uint8_t		reserved5;
} StopBBParam;

/* DebugMFEParam */
typedef struct {
	uint32_t	mic_delay;
	uint32_t	ref_delay;
	uint32_t	mfe_config_table;
} DebugMFEParam;

/* InitMPP */
typedef struct {
	uint8_t		output_fs;			/* AsMppOutputFsId */
	uint8_t		output_channel_num;		/* AsMppOutputChNum */
	uint8_t		reserved1;
	uint8_t		mpp_mode;			/* AsMppModeId */

	uint8_t		reserved2;
	uint8_t		eax_mode;			/* AsMppEaxModeId */
	uint8_t		xloud_mode;			/* AsMppXloudModeId */
	uint8_t		coef_mode;			/* AsMppCoefModeId */

	uint32_t	xloud_coef_table;
	uint32_t	eax_coef_table;

} InitMPPParam;


/* SetMPPParam */
typedef struct {
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
} MppCommonSet;
typedef struct {
	uint8_t		xloud_vol;			/* CHECK_XLOUD_VOLUME_RANGE(xloud_vol) */
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;

	uint32_t	reserved4;
	uint32_t	reserved5;
} MppXloudSet;

#pragma anon_unions
typedef struct {
	union {
		MppCommonSet	mpp_common_set;		/* sub_code=0 */
		MppXloudSet	mpp_xloud_set;		/* sub_code=1 */
	};
} SetMPPParam;

/* DebugMPPParam */
typedef struct {
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
	uint32_t	reserved4;
} MppCommonDebug;

typedef struct {
	uint32_t	xloud_config_table;
	uint32_t	xloud_param_table;
	uint32_t	eax_config_table;
	uint32_t	eax_param_table;
} MppXloudDebug;

#pragma anon_unions
typedef struct {
	union {
		MppCommonDebug	mpp_common_debug;	/* sub_code=0 */
		MppXloudDebug	mpp_xloud_debug;	/* sub_code=1 */
	};
} DebugMPPParam;


typedef struct {
	uint8_t		input_device;			/* AsSetPlayerInputDevice */
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;

	uint32_t	input_device_handler;

	uint8_t		output_device;			/* AsSetPlayerOutputDevice */
	uint8_t		reserved4;
	uint8_t		reserved5;
	uint8_t		reserved6;

	uint32_t	output_device_handler;
} SetPlayerStsParam;

typedef struct {
	uint8_t		reserved1;
	uint8_t		play_filter;			/* AsSetPlayerPlayFilter */
	uint8_t		play_mode;			/* AsSetPlayerPlayModeId */
	uint8_t		repeat_mode;			/* AsSetPlayerRepeatModeId */

	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		reserved4;
	uint8_t		reserved5;

	uint8_t		reserved6;
	uint8_t		reserved7;
	uint8_t		reserved8;
	uint8_t		reserved9;
} SetPlayerParamParam;

typedef struct {
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		reserved4;

	uint32_t	mathfunc_config_table;

	uint8_t		reserved5;
	uint8_t		reserved6;
	uint8_t		reserved7;
	uint8_t		reserved8;
} SetWaitKeyStatusParam;

typedef struct {
	uint8_t		select_i2s_out;			/* AsSetBBStsSelectI2sOut */
	uint8_t		reserved1;
	uint8_t		with_voice_command;		/* AsSetBBStsWithVoiceCommand */
	uint8_t		with_mfe;			/* AsSetBBStsWithMfe */

	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		reserved4;
	uint8_t		reserved5;

	uint8_t		reserved6;
	uint8_t		reserved7;
	uint8_t		reserved8;
	uint8_t		reserved9;
} SetBaseBandStatusParam;


/* for voice trigger */
typedef void (*AudioFindTriggerCallbackFunction)(uint8_t command_code, uint8_t sub_code);
typedef struct {
	AudioFindTriggerCallbackFunction	callback_function;
} SetFindTriggerCallbackParam;

/* for voice command callback function and arguments */
typedef enum {
	AS_RECOGNITION_KEYWORD_REC_START = 0,
	AS_RECOGNITION_KEYWORD_REC_STOP,
} AsRecognitionKeyword;
typedef enum {
	AS_RECOGNITION_STATUS_VOICE_UNRECOGNIZED = 0,
	AS_RECOGNITION_STATUS_VOICE_RECOGNIZED,
	AS_RECOGNITION_STATUS_KEYWORD_RECOGNIZED,
	AS_RECOGNITION_STATUS_NUM
} AsVoiceRecognitionStatus;

typedef void (*AudioFindCommandCallbackFunction)(
	uint16_t keyword,
	uint8_t status					/* AsVoiceRecognitionStatus */
	);

typedef struct {
	uint8_t		vad_only;			/* AsVoiceCommandVadOnly */
	uint8_t		reserved1;
	uint16_t	keyword;

	AudioFindCommandCallbackFunction	callback_function;

	uint8_t		reserved2;
	uint8_t		reserved3;
	uint8_t		reserved4;
	uint8_t		reserved5;
} StartVoiceCommandParam;

typedef struct {
	uint8_t		sampling_rate;			/* AsSetRecorderStsSamplingRateIndex */
	uint8_t		channel_number;			/* AsSetRecorderStsChannelNumberIndex */
	uint8_t		bit_length;			/* AsSetRecorderStsBitLength */
	uint8_t		input_device;			/* AsSetRecorderStsInputDevice */

	uint32_t	input_device_handler;

	uint8_t		output_device;			/* AsSetRecorderStsOutputDevice */
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;

	uint32_t	output_device_handler;
} SetRecorderStatusParam;


#pragma anon_unions
typedef struct {
	AudioCommandHeader	header;
	union {
		InitBBParam 			init_bb_param;
		GetStatus 			get_status;
		SetBBParam			set_bb_param;
		InitMFEParam			init_mfe_param;
		StartBBParam			start_bb_param;
		StopBBParam			stop_bb_param;
		DebugMFEParam			debug_mfe_param;
		InitMPPParam			init_mpp_param;
		SetMPPParam			set_mpp_param;
		DebugMPPParam			debug_mpp_param;
		SetPlayerStsParam		set_player_sts_param;
		SetPlayerParamParam		set_player_param_param;
		SetRecorderStatusParam		set_recorder_status_param;
		SetWaitKeyStatusParam		set_waitkey_status_param;
		SetBaseBandStatusParam		set_baseband_status_param;
		StartVoiceCommandParam		start_voice_command_param;
		SetFindTriggerCallbackParam	set_find_trigger_callback_param;
	};
#ifdef __cplusplus
	uint8_t getCode(void){ return(header.command_code); }
	bool isStateChange(void){ return((getCode() & 0x40)!=0); }
//	int getNextState(void){ isStateChange()?return(0):return(getCode() & 0x0f);}
#endif

} AudioCommand;




/*-----------------------------------------------------------------------------
	Result Structures
  -----------------------------------------------------------------------------*/

typedef enum {
	AS_NOTIFY_STATUS_INFO_READY = 0,
	AS_NOTIFY_STATUS_INFO_BASEBAND,
	AS_NOTIFY_STATUS_INFO_WAIT_KEYWORD,
	AS_NOTIFY_STATUS_INFO_PLAYER,
	AS_NOTIFY_STATUS_INFO_RECORDER,
	AS_NOTIFY_STATUS_INFO_NUM
} AsNotifyStatusInfo;
typedef enum {
	AS_NOTIFY_SUB_STATUS_INFO_PLAYREADY = 0,
	AS_NOTIFY_SUB_STATUS_INFO_PLAYACTIVE,
	AS_NOTIFY_SUB_STATUS_INFO_PLAYPAUSE,
	AS_NOTIFY_SUB_STATUS_INFO_RECORDERREADY,
	AS_NOTIFY_SUB_STATUS_INFO_RECORDERACTIVE,
	AS_NOTIFY_SUB_STATUS_INFO_BASEBANDREADY,
	AS_NOTIFY_SUB_STATUS_INFO_BASEBANDACTIVE,
	AS_NOTIFY_SUB_STATUS_INFO_WAITCOMMANDWORD,
	AS_NOTIFY_SUB_STATUS_INFO_NUM
} AsNotifySubStatusInfo;
typedef enum {
	AS_NOTIFY_VAD_STATUS_OUT_OF_VOICE_SECTION = 0,
	AS_NOTIFY_VAD_STATUS_INSIDE_VOICE_SECTION,
	AS_NOTIFY_VAD_STATUS_NUM
} AsNotifyVadStatus;

typedef enum {
	AS_STATUS_CHANGED_STS_READY = 0,
	AS_STATUS_CHANGED_STS_BBACTIVE,
	AS_STATUS_CHANGED_STS_WAITKEYWORD,
	AS_STATUS_CHANGED_STS_NUM
} AsStatusChangedSts;


typedef struct {
	uint32_t	reserved1;
} InitBBCmpltParam;

typedef struct {
	uint8_t		vad_status;			/* AsNotifyVadStatus */
	uint8_t		reserved;
	uint8_t		sub_status_info;
	uint8_t		status_info;			/* AsNotifyStatusInfo */
} NotifyStatus;


typedef struct {
	uint32_t	reserved1;
} SetBBCmpltParam;

typedef struct {
	uint32_t	reserved1;
} InitMFECmpltParam;

typedef struct {
	uint32_t	reserved1;
} StartBBCmpltParam;

typedef struct {
	uint32_t	reserved1;
} StopBBCmpltParam;

typedef struct {
	uint32_t	reserved1;
} DebugMFECmpltParam;

typedef struct {
	uint32_t	reserved1;
} InitMPPCmpltParam;

typedef struct {
	uint32_t	reserved1;
} SetMPPCmpltParam;

typedef struct {
	uint32_t	reserved1;
} DebugMPPCmpltParam;

typedef struct {
	uint32_t	reserved1;
} PlayCmpltParam;

typedef struct {
	uint32_t	reserved1;
} StopCmpltParam;

typedef struct {
	uint32_t	reserved1;
} PauseCmpltParam;

typedef struct {
	uint32_t	reserved1;
} NextCmpltParam;

typedef struct {
	uint32_t	reserved1;
} PrevCmpltParam;

typedef struct {
	uint32_t	reserved1;
} SetPlayerCmpltParam;

typedef struct {
	uint8_t		changed_status;			/* AsStatusChangedSts */
	uint8_t		reserved1;
	uint8_t		reserved2;
	uint8_t		reserved3;
} StatusChangedParam;

typedef struct {
	uint8_t		error_code;
	uint8_t		reserved1;
	uint8_t		sub_module_id;
	uint8_t		module_id;

	uint32_t	error_sub_code;
	uint32_t	reserved2;
	uint32_t	reserved3;
} ErrorResponseParam;

typedef struct {
	uint32_t	reserved1;

	uint8_t		error_code;
	uint8_t		cpu_id;
	uint8_t		sub_module_id;
	uint8_t		module_id;

	uint32_t	error_att_sub_code;
	uint32_t	reserved2;

	uint16_t	line_number;
	uint8_t		task_id;
	uint8_t		reserved3;

	uint32_t	error_filename_1;
	uint32_t	error_filename_2;
	uint32_t	error_filename_3;
	uint32_t	error_filename_4;
	uint32_t	error_filename_5;
	uint32_t	error_filename_6;
	uint32_t	error_filename_7;
	uint32_t	error_filename_8;
} ErrorAttentionParam;

#pragma anon_unions
typedef struct {
	AudioResultHeader	header;
	union {
		InitBBCmpltParam 	init_bb_cmplt_param;
		NotifyStatus 		notify_status;
		SetBBCmpltParam		set_bb_cmplt_param;
		InitMFECmpltParam 	init_mfe_cmplt_param;
		StartBBCmpltParam	start_bb_cmplt_param;
		StopBBCmpltParam	stop_bb_cmplt_param;
		DebugMFECmpltParam 	debug_mfe_cmplt_param;
		InitMPPCmpltParam 	init_mpp_cmplt_param;
		SetMPPCmpltParam 	set_mpp_cmplt_param;
		DebugMPPCmpltParam 	debug_mpp_cmplt_param;
		PlayCmpltParam		play_cmplt_param;
		StopCmpltParam		stop_cmplt_param;
		PauseCmpltParam		pause_cmplt_param;
		NextCmpltParam		next_cmplt_param;
		PrevCmpltParam		prev_cmplt_param;
		SetPlayerCmpltParam	set_player_cmplt_param;
		StatusChangedParam	status_changed_param;
/* とりあえず、エラーパケットはサイズもありはずす。*/
//		ErrorResponseParam	error_response_param;
//		ErrorAttentionParam	error_attention_param;
	};

} AudioResult;

/* Error Code */
/* [T.B.D]

	no error = 0
	state error
	paramater error
	timeout

*/

#ifdef __cplusplus
extern "C" {
#endif
/* API */
extern void AS_SendAudioCommand(AudioCommand*);
extern void AS_ReceiveAudioResult(AudioResult*);
#ifdef __cplusplus
}
#endif

#endif	/* AS_HIGH_LEVEL_API_H */


