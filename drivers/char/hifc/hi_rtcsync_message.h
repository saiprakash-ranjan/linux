/*
* hi_rtcsync_message.h
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

#ifndef __HI_RTCSYNC_MESSAGE_H__
#define __HI_RTCSYNC_MESSAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "drivers/peripheral/pd_rtc.h"

#define HI_HANDLER_ID_RTCSYNC   0x0a
#define HI_RTCSYNC_MSG_RTCGET   1
#define HI_RTCSYNC_MSG_RTCSET   2

typedef enum {
    HI_RTCSYNC_MSG_FUNC_NONE = 0x00,
    HI_RTCSYNC_MSG_FUNC_SEND = 0x01,
    HI_RTCSYNC_MSG_FUNC_RECV = 0x02,
} HI_RtcsyncMsgFunc;

typedef struct {
    uint8_t mesgType;
    uint8_t reserved;
    uint16_t paramLength;
} RtcHeader;

typedef struct {
    RtcHeader header;
    PD_RtcCh_t ch;
} RtcGetCommand;

typedef struct {
    RtcHeader header;
    PD_RtcCh_t ch;
    PD_RtcCounter_t cnt;
} RtcSetCommand;

typedef struct {
    RtcHeader header;
    PD_RtcCounter_t cnt;
    int retcode;
} RtcGetResult;

typedef struct {
    RtcHeader header;
    int retcode;
} RtcSetResult;

int PD_RtcInit(PD_RtcCh_t ch);

#ifdef __cplusplus
}
#endif

#endif //__HI_RTCSYNC_MESSAGE_H__
