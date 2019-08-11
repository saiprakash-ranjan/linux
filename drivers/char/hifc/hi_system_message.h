/*
 * hi_system_message.h
 *
 * This program is subject to copyright protection in accordance with the
 * applicable law. It must not, except where allowed by law, by any meansor
 * in any form be reproduced, distributed or lent. Moreover, no part of the
 * program may be used, viewed, printed, disassembled or otherwise interfered
 * with in any form, except where allowed by law, without the express written
 * consent of the copyright holder.
 *
 * Copyright 2016 Sony Corporation
 *
 */

#ifndef __HI_SYSTEM_MESSAGE_H__
#define __HI_SYSTEM_MESSAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hi_payload_header.h"

typedef enum {
    HI_SYS_MSG_FUNC_READY = 0x00,
    HI_SYS_MSG_FUNC_VER = 0x01,
    HI_SYS_MSG_FUNC_REPORT_ERROR = 0x02,
} HI_SysMsgFunc;

typedef struct {
    HI_PayloadHeader head;
    uint8_t handlerId;
    uint8_t ready;
} HI_SysMsgReady;

typedef enum {
    HI_SYS_GET_VER_SBL,
    HI_SYS_GET_VER_FW,
    HI_SYS_GET_VER_CUID0,
    HI_SYS_GET_VER_CUID1,
    HI_SYS_GET_VER_UDID0,
    HI_SYS_GET_VER_UDID1,
} HI_SysGetVerType;

typedef struct {
    HI_PayloadHeader head;
    int type;
} HI_SysMsgVerGet;

typedef struct {
    HI_PayloadHeader head;
    char version[];
} HI_SysMsgVerRet;

typedef struct {
    HI_PayloadHeader head;
    uint32_t level;
    uint32_t module;
    uint32_t code;
} HI_SysMsgReportError;

#define HI_SYS_MAX_VER_STR_LEN 32
#define HI_SYS_MAX_MSG_SIZE (sizeof(HI_PayloadHeader) + sizeof(HI_SysMsgVerRet) + HI_SYS_MAX_VER_STR_LEN + 1)

#ifdef __cplusplus
}
#endif

#endif //__HI_SYSTEM_MESSAGE_H__
