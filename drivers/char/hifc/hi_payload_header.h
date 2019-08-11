/*
 * hi_payload_header.h
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

#ifndef __HI_PAYLOAD_HEADER_H__
#define __HI_PAYLOAD_HEADER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HI_MESSAGE_TYPE_PLD_MASK	0x0c
#define HI_MESSAGE_TYPE_REQ_MASK	0x03
#define HI_MESSAGE_TYPE_PLD_CONT	0x08
#define HI_MESSAGE_TYPE_PLD_FINI	0x04
#define HI_MESSAGE_TYPE_REQ_CONT	0x02
#define HI_MESSAGE_TYPE_REQ_FINI	0x01
#define HI_MESSAGE_TYPE_FLG_NONE	0X00

#define HI_HANDLER_ID_SYSTEM       0x00
#define HI_HANDLER_ID_SENSOR       0x0B
#define HI_HANDLER_ID_ACTIVITY_LOG 0x0C
#define HI_HANDLER_ID_RTC          0x0D
#define HI_HANDLER_ID_AUDIO        0x0E
#define HI_HANDLER_ID_AP_STORAGE   0x0F

typedef struct HI_PayloadHeader {
	uint8_t handlerId;
	uint8_t flag;
	uint16_t apiId;
	uint16_t payloadSize;
	uint16_t reserved;
} HI_PayloadHeader;

#endif //__HI_PAYLOAD_HEADER_H__
