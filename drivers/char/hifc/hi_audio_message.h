/*
 * hi_audio_message.h
 *
 * This program is subject to copyright protection in accordance with the
 * applicable law. It must not, except where allowed by law, by any meansor
 * in any form be reproduced, distributed or lent. Moreover, no part of the
 * program may be used, viewed, printed, disassembled or otherwise interfered
 * with in any form, except where allowed by law, without the express written
 * consent of the copyright holder.
 *
 * Copyright 2014 Sony Corporation
 *
 */

#ifndef __HI_AUDIO_MESSAGE_H__
#define __HI_AUDIO_MESSAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hi_payload_header.h"
#include "audio/high_level_api/as_high_level_api.h"

#define HI_HANDLER_ID_AUDIO 0x0E

typedef enum {
    HI_AUDIO_MSG_FUNC_READY = 0x00,
    HI_AUDIO_MSG_FUNC_SEND = 0x01,
    HI_AUDIO_MSG_FUNC_RECV = 0x02,
    HI_AUDIO_MSG_FUNC_RESULT = 0x03,
    HI_AUDIO_MSG_FUNC_TRIGGER = 0x04,
    HI_AUDIO_MSG_FUNC_EXT  = 0x05,
    HI_AUDIO_MSG_FUNC_TEST = 0x7F,
    HI_AUDIO_MSG_FLAG_CONT = 0x80,
    HI_AUDIO_MSG_FLAG_MASK = 0x80,
} HI_AudioMsgFunc;

typedef struct {
    HI_PayloadHeader head;
    uint8_t buf[];
} HI_AudioMsgBuf;

typedef struct {
    HI_PayloadHeader head;
    uint8_t ready;
} HI_AudioMsgReady;

typedef struct {
    HI_PayloadHeader head;
    AudioCommand command;
} HI_AudioMsgSend;

typedef struct {
    HI_PayloadHeader head;
    AudioResult result;
} HI_AudioMsgResult;

typedef struct {
    HI_PayloadHeader head;
    uint8_t CommandCode;
    uint8_t SubCode;
    uint16_t reserved;
} HI_AudioMsgTrigger;

#ifdef __cplusplus
}
#endif

#endif //__HI_SENSOR_MESSAGE_H__
