/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Telephony service private
*/

#ifndef TELEPHONY_SERVICE_PRIVATE_H_
#define TELEPHONY_SERVICE_PRIVATE_H_

#include "telephony_service.h"

typedef struct
{
    bool caller_id_enabled;
    uint16 command_lock;
} telephony_service_data_t;

extern telephony_service_data_t telephony_service;

extern const TaskData telephony_message_handler_task;

/* command_lock is 16 bits and we use 2 bits per voice source */
#if ((max_voice_sources * 2) > 16)
    #error "Telephony service error - too many voice sources for command_lock"
#endif

#define telephonyService_SetStateChangeLock(source)     telephony_service.command_lock |= (1 << (source))
#define telephonyService_ClearStateChangeLock(source)   telephony_service.command_lock &= ~(1 << (source))

#define telephonyService_SetAudioTransferLock(source)   telephony_service.command_lock |= (1 << (max_voice_sources + (source)))
#define telephonyService_ClearAudioTransferLock(source) telephony_service.command_lock &= ~(1 << (max_voice_sources + (source)))

#define telephonyService_ClearAllLocks()                telephony_service.command_lock = 0

#endif /* TELEPHONY_SERVICE_PRIVATE_H_ */
