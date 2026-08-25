/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Unexpected message handlers.

*/

#include "logging.h"

#include "headset_sm.h"
#include "av.h"
#include "headset_setup_unexpected_message.h"

#include "unexpected_message.h"

static void headsethandleUnexpectedMessage(MessageId id)
{
#if defined(INCLUDE_AV) && (INCLUDE_HFP)
    DEBUG_LOG_VERBOSE("headsethandleUnexpectedMessage, MESSAGE:0x%x, sm = %d, av = %d", id, SmGetTaskData()->state, AvGetTaskData()->bitfields.state);
#elif defined(INCLUDE_AV)
    DEBUG_LOG_VERBOSE("headsethandleUnexpectedMessage, MESSAGE:0x%x, sm = %d, av = %d", id,  SmGetTaskData()->state, AvGetTaskData()->bitfields.state);
#elif defined(INCLUDE_HFP)
    DEBUG_LOG_VERBOSE("headsethandleUnexpectedMessage, MESSAGE:0x%x, sm = %d", id,  SmGetTaskData()->sm.state);
#else
    UNUSED(id);
#endif
}

static void headsethandleUnexpectedSysMessage(MessageId id)
{
    DEBUG_LOG_VERBOSE("headsethandleUnexpectedSysMessage, MESSAGE:0x%x", id);
}

void Headset_SetupUnexpectedMessage(void)
{
    UnexpectedMessage_RegisterHandler(headsethandleUnexpectedMessage);
    UnexpectedMessage_RegisterSysHandler(headsethandleUnexpectedSysMessage);
}

