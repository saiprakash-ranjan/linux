#ifndef __HI_AUDIO_EXT_NGL_H__
#define __HI_AUDIO_EXT_NGL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hi_payload_header.h"

// ===== Common Constants =====

typedef enum {
	HI_AUDIO_EXT_CMD_INIT = 0,
	HI_AUDIO_EXT_CMD_SET_VOLUME,
	HI_AUDIO_EXT_CMD_SET_MUTE,
	HI_AUDIO_EXT_CMD_SET_MODE,
	HI_AUDIO_EXT_CMD_SET_XLOUD,
	HI_AUDIO_EXT_CMD_SET_OUTDEV,
	HI_AUDIO_EXT_CMD_GET_VOLUME,
	HI_AUDIO_EXT_CMD_GET_MUTE,
	HI_AUDIO_EXT_CMD_GET_MODE,
	HI_AUDIO_EXT_CMD_GET_XLOUD,
	HI_AUDIO_EXT_CMD_GET_OUTDEV,
	HI_AUDIO_EXT_CMD_SET_ENV_ADAPT,
	HI_AUDIO_EXT_CMD_GET_ENV_ADAPT,
	HI_AUDIO_EXT_CMD_SET_MIC_GAIN0,
	HI_AUDIO_EXT_CMD_SET_MIC_GAIN1,
	HI_AUDIO_EXT_CMD_SET_MIC_GAIN2,
	HI_AUDIO_EXT_CMD_SET_MIC_GAIN3,
	HI_AUDIO_EXT_CMD_GET_MIC_GAIN0,
	HI_AUDIO_EXT_CMD_GET_MIC_GAIN1,
	HI_AUDIO_EXT_CMD_GET_MIC_GAIN2,
	HI_AUDIO_EXT_CMD_GET_MIC_GAIN3,
	HI_AUDIO_EXT_CMD_SET_MIC_SELECT,
	HI_AUDIO_EXT_CMD_GET_MIC_SELECT,
	HI_AUDIO_EXT_CMD_SET_SCU_MIC_GAIN,
	HI_AUDIO_EXT_CMD_GET_SCU_MIC_GAIN,
	HI_AUDIO_EXT_CMD_CODE_MAX
} HI_AudioExtCmdCode;

typedef enum {
	HI_AUDIO_EXT_OFF = 0,
	HI_AUDIO_EXT_ON = 1,
} HI_AudioExtOnOff;

typedef enum {
	HI_AUDIO_EXT_RESULT_OK = 0,
	HI_AUDIO_EXT_RESULT_ERROR,
} HI_AudioExtResultCode;

// ===== Common Structures =====

typedef struct {
	HI_PayloadHeader Header;
	uint8_t CmdCode; /* HI_AudioExtCommandCode */
} HI_AudioExtCmdHeader;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
} HI_AudioExtResult;

// ===== Commands =====

// ----- Init -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	/* TBD */
} HI_AudioExtCmdInit;

// ----- Volume -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t Volume; /* 0-59 */
} HI_AudioExtCmdSetVolume;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetVolume;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t Volume; /* 0-59 */
} HI_AudioExtResVolume;

// ----- Mute -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t Mute; /* HI_AudioExtOnOff */
} HI_AudioExtCmdSetMute;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetMute;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t Mute; /* Mute Nest Count? */
} HI_AudioExtResMute;

// ----- Mode -----

typedef enum {
	HI_AUDIO_EXT_MODE_VOICE_TRIGGER = 0,
	HI_AUDIO_EXT_MODE_VOICE_COMMAND,
	HI_AUDIO_EXT_MODE_VOICE_RECOGNITION,
	HI_AUDIO_EXT_MODE_PHONE_CALL,
	HI_AUDIO_EXT_MODE_RECORDING,
	HI_AUDIO_EXT_MODE_SHUTTER_SOUND,
} HI_AudioExtMode;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t Mode; /* HI_AudioExtMode */
} HI_AudioExtCmdSetMode;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetMode;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t Mode; /* HI_AudioExtMode */
} HI_AudioExtResMode;

// ----- xLoud -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t XLoud; /* HI_AudioExtOnOff */
} HI_AudioExtCmdSetXLoud;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetXLoud;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t XLoud; /* HI_AudioExtOnOff */
} HI_AudioExtResXLoud;

// ----- Environment Adaptation -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t EnvAdapt; /* HI_AudioExtOnOff */
} HI_AudioExtCmdSetEnvAdapt;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetEnvAdapt;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t EnvAdapt; /* HI_AudioExtOnOff */
} HI_AudioExtResEnvAdapt;

// ----- Output Device -----

typedef enum {
	HI_AUDIO_EXT_OUTDEV_SP, /* Speaker */
	HI_AUDIO_EXT_OUTDEV_HP, /* Headphone */
} HI_AudioExtOutDev;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t OutDev; /* HI_AudioExtOutDev */
} HI_AudioExtCmdSetOutDev;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetOutDev;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t OutDev; /* HI_AudioExtOutDev */
} HI_AudioExtResOutDev;

// ----- MIC Select -----

typedef enum {
	HI_AUDIO_EXT_MICSEL_M12, /* Mic1-Mic2 */
	HI_AUDIO_EXT_MICSEL_M03, /* Mic0-Mic3 */
} HI_AudioExtMicSel;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t MicSel; /* HI_AudioExtMicSel */
} HI_AudioExtCmdSetMicSel;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetMicSel;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t MicSel; /* HI_AudioExtMicSel */
} HI_AudioExtResMicSel;

// ----- MIC Gain -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t MicGain; /* 0-210 */
} HI_AudioExtCmdSetMicGain;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetMicGain;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	uint8_t MicGain; /* 0-210 */
} HI_AudioExtResMicGain;

// ----- SCU MIC Gain -----

typedef struct {
	HI_AudioExtCmdHeader Header;
	int8_t ScuMicGain;  /* -6-24 */
} HI_AudioExtCmdSetScuMicGain;

typedef struct {
	HI_AudioExtCmdHeader Header;
} HI_AudioExtCmdGetScuMicGain;

typedef struct {
	HI_AudioExtCmdHeader Header;
	uint8_t ResultCode; /* HI_AudioExtResultCode */
	int8_t ScuMicGain;  /* -6-24 */
} HI_AudioExtResScuMicGain;

/* Debug Related Cmds: TBD */


#ifdef __cplusplus
}
#endif

#endif // __HI_AUDIO_EXT_NGL_H__
