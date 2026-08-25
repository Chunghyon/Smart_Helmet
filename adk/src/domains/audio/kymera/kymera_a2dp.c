/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP
*/

#include "kymera_a2dp.h"
#include "kymera_a2dp_private.h"
#include "kymera_data.h"
#include <panic.h>

bool Kymera_A2dpIsBeingRouted(void)
{
    switch(appKymeraA2dpGetState())
    {
        case kymera_a2dp_idle:
            return FALSE;
        case kymera_a2dp_preparing:
        case kymera_a2dp_prepared:
        case kymera_a2dp_starting:
        case kymera_a2dp_streaming:
        case kymera_a2dp_forwarding:
            return TRUE;
    }
    Panic();
    return FALSE;
}

bool Kymera_A2dpIsRouted(void)
{
    switch(appKymeraA2dpGetState())
    {
        case kymera_a2dp_idle:
        case kymera_a2dp_preparing:
            return FALSE;
        case kymera_a2dp_prepared:
        case kymera_a2dp_starting:
        case kymera_a2dp_streaming:
        case kymera_a2dp_forwarding:
            return TRUE;
    }
    Panic();
    return FALSE;
}

bool Kymera_A2dpIsActive(void)
{
    switch(appKymeraA2dpGetState())
    {
        case kymera_a2dp_idle:
        case kymera_a2dp_preparing:
        case kymera_a2dp_prepared:
            return FALSE;
        case kymera_a2dp_starting:
        case kymera_a2dp_streaming:
        case kymera_a2dp_forwarding:
            return TRUE;
    }
    Panic();
    return FALSE;
}

bool Kymera_A2dpIsStreaming(void)
{
    switch(appKymeraA2dpGetState())
    {
        case kymera_a2dp_idle:
        case kymera_a2dp_preparing:
        case kymera_a2dp_prepared:
        case kymera_a2dp_starting:
            return FALSE;
        case kymera_a2dp_streaming:
        case kymera_a2dp_forwarding:
            return TRUE;
    }
    Panic();
    return FALSE;
}

bool Kymera_A2dpIsForwarding(void)
{
    switch(appKymeraA2dpGetState())
    {
        case kymera_a2dp_idle:
        case kymera_a2dp_preparing:
        case kymera_a2dp_prepared:
        case kymera_a2dp_starting:
        case kymera_a2dp_streaming:
            return FALSE;
        case kymera_a2dp_forwarding:
            return TRUE;
    }
    Panic();
    return FALSE;
}

bool Kymera_A2dpHandleInternalStart(const KYMERA_INTERNAL_A2DP_START_T *msg)
{
    Kymera_A2dpHandlePrepareReq(msg);
    Kymera_A2dpHandleStartReq(msg);
    return TRUE;
}

void Kymera_A2dpHandlePrepareReq(const audio_a2dp_start_params_t *params)
{
    Kymera_A2dpHandlePrepareStage(params);
}

void Kymera_A2dpHandleStartReq(const audio_a2dp_start_params_t *params)
{
    kymeraTaskData *theKymera;
    Kymera_A2dpHandleStartStage(params->codec_settings.seid, params->volume_in_db);

    if (params->lock)
    {
        *params->lock &= ~params->lock_mask;
    }

    theKymera = KymeraGetTaskData();
    TaskList_MessageSendId(theKymera->listeners, KYMERA_NOTIFICATION_EQ_AVAILABLE);
}
