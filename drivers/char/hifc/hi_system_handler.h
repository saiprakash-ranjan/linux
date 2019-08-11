/*
 * hi_system_handler.h
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

#ifndef __HI_SYSTEM_HANDLER_H__
#define __HI_SYSTEM_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

int spz_is_handler_ready(uint8_t handlerId);
int spz_wait_for_handler_ready(uint8_t handlerId, unsigned long timeout);

#ifdef __cplusplus
}
#endif

#endif //__HI_SYSTEM_HANDLER_H__
