/*#############################################################################
 * Copyright 2014 Sony Corporation.
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Sony Corporation.
 * No part of this file may be copied, modified, sold, and distributed in any
 * form or by any means without prior explicit permission in writing from
 * Sony Corporation.
 */
/**
 * @file        pd_rtc.h
 * @note        RTC driver interface
 */
/*###########################################################################*/

#ifndef __PD_RTC_H__
#define __PD_RTC_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/**
 * @defgroup pd_rtc RTC Driver
 *
 * RTC driver.
 * Actual usage and more details are see other documents.
 *
 * <pre>\#include <drivers/peripheral/pd_rtc.h></pre>
 *
 * @{
 */

/******************************************************************************
 * Include
 *****************************************************************************/
#include <stdbool.h>

/******************************************************************************
 * Define
 *****************************************************************************/

/******************************************************************************
 * Type
 *****************************************************************************/
/**
 * RTC channel number
 */
typedef enum PD_RtcCh_t
{
	PD_RTC_CH0 = 0,
	PD_RTC_CH1,
//	PD_RTC_CH2,
	PD_RTC_CH_MAX,
} PD_RtcCh_t;

/**
 * RTC alarm number
 */
typedef enum PD_RtcAlmCh_t
{
	PD_RTC_ALM_CH0 = 0,
	PD_RTC_ALM_CH1,
	PD_RTC_ALM_CH2,
	PD_RTC_ALM_CH_MAX,
} PD_RtcAlmCh_t;

/**
 * RTC Counter structure
 */
typedef struct PD_RtcCounter_t
{
	uint32_t preCounter;
	uint32_t postCounter;
} PD_RtcCounter_t;

/**
 * RTC Sync Counter structure
 */
typedef struct PD_RtcSyncCounter_t
{
	PD_RtcCh_t srcCh;
	PD_RtcCh_t dstCh;
	PD_RtcCounter_t rtcSyncCounter;
	PD_RtcCounter_t rtcAlarmCounter;
} PD_RtcSyncCounter_t;

/**
 * RTC callback
 */
typedef void (*PD_RtcCallback_t)(void *arg);

/**
 * Alarm request structure
 */
typedef struct PD_RtcAlmRequest_t {
	PD_RtcCh_t rtcCh;
	PD_RtcCounter_t counter;
	PD_RtcCallback_t callback;
} PD_RtcAlmRequest_t;

/******************************************************************************
 * Prototype
 *****************************************************************************/
/*---------------------------------------------------------------------------*/
/**
 *  Initialize RTC
 *
 *  @param[in]  ch: RTC channel number
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcInit(PD_RtcCh_t ch);

/*---------------------------------------------------------------------------*/
/**
 *  Set RTC counter
 *
 *  @param[in]  ch: RTC channel number
 *  @param[in]  pRtcCounter: pointer to counter value that'll be set to RTC counter
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcSetCounter(PD_RtcCh_t ch, PD_RtcCounter_t* pRtcCounter);

/*---------------------------------------------------------------------------*/
/**
 *  Get RTC counter
 *
 *  @param[in]  ch: RTC channel number
 *  @param[out] pRtcCounter: pointer to counter overwrote in this function
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcGetCounter(PD_RtcCh_t ch, PD_RtcCounter_t* pRtcCounter);

/*---------------------------------------------------------------------------*/
/**
 *  Get RTC counter immediately
 *
 *  @param[in]  ch: RTC channel number
 *  @param[out] pRtcCounter: pointer to counter overwrote in this function
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task and interrupt
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcGetRtCounter(PD_RtcCh_t ch, PD_RtcCounter_t* pRtcCounter);

/*---------------------------------------------------------------------------*/
/**
 *  Request RTC alarm
 *
 *  @param[in]  pReq: pointer to PD_RtcAlmRequest_t
 *
 *  @return     0: success
 *  @return -EINVAL: Invalid argument
 *  @return -ENOMEM: Cannot allocate memory
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcRequestAlarm(PD_RtcAlmRequest_t *pReq);

/*---------------------------------------------------------------------------*/
/**
 *  Cancel RTC alarm
 *
 *  @param[in]  pReq: pointer to PD_RtcAlmRequest_t
 *
 *  @return     0: success
 *  @return -EINVAL: Invalid argument
 *  @return -EFAULT: Bad address
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcCancelAlarm(PD_RtcAlmRequest_t *pReq);

/*---------------------------------------------------------------------------*/
/**
 *  Set sync counter
 *
 *  @param[in]  pRtcSyncCounter: structure  for sync counter
 *
 *  @return     0: success
 *  @return -EPERM: Operation not permitted
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcSyncCounter(PD_RtcSyncCounter_t* pRtcSyncCounter);

/*---------------------------------------------------------------------------*/
/**
 *  Set sync counter by Pompier(PMIC)
 *
 *  @param[in]  dstCh: RTC channel number detects alarm and be synchronized counter you have set.
 *  @param[in]  pRtcCounter: pointer to counter that'll be set when alarm assert.
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcSyncExtCounter(PD_RtcCh_t dstCh, PD_RtcCounter_t* pRtcCounter);

/*---------------------------------------------------------------------------*/
/**
 *  Adjust RTC counter
 *
 *  @param[in]  ch: RTC channel number
 *  @param[in]  offsetValue: adjust value
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcAdjustCounter(PD_RtcCh_t ch, int8_t offsetValue);

/*---------------------------------------------------------------------------*/
/**
 *  Add addValue to PD_RtcCounter_t type RtcCounter
 *
 *  @param[out] pRtcCounter: pointer to counter value that'll be added
 *  @param[in]  addValue:  additional value
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcAddCounter(PD_RtcCounter_t* pRtcCounter, uint64_t addValue);

/*---------------------------------------------------------------------------*/
/**
 *  Subtract subValue from PD_RtcCounter_t type RtcCounter
 *
 *  @param[out] pRtcCounter: pointer to counter value that'll be subtracted
 *  @param[in]  subValue: subtraction value
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcSubCounter(PD_RtcCounter_t* pRtcCounter, uint64_t subValue);

/*---------------------------------------------------------------------------*/
/**
 *  Finalize RTC
 *
 *  @param[in]  ch: RTC channel number
 *
 *  @return     0: success
 *
 *  @par Blocking
 *      Yes
 *  @par Context
 *      Task
 *  @par Reentrant
 *      No
 *
 */
/*---------------------------------------------------------------------------*/
int PD_RtcUninit(PD_RtcCh_t ch);

/** @} */

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PD_RTC_H__ */
/******************************************************************************
 * End of file
 *****************************************************************************/
