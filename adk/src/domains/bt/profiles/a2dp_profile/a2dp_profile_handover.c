/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       a2dp_profile_handover.c
\brief      A2DP profile handover related interfaces
*/

/* Only compile if AV defined */
#ifdef INCLUDE_AV
#ifdef INCLUDE_MIRRORING

#include "a2dp_profile.h"
#include "av.h"

#include <a2dp.h>
#include <panic.h>
#include <logging.h>

bool A2dpProfile_Veto(avInstanceTaskData *the_inst)
{
    bool veto = FALSE;

    if (appA2dpGetLock(the_inst) & APP_A2DP_TRANSITION_LOCK)
    {
        DEBUG_LOG_INFO("A2dpProfile_Veto, APP_A2DP_TRANSITION_LOCK");
        veto = TRUE;
    }

    return veto;
}

static void a2dpProfile_SetInstanceDataForPrimaryOrAcceptor(avInstanceTaskData *the_inst)
{
    /* Set the stream handle (device local identifier) to 0 as we have only one stream */
    the_inst->a2dp.stream_id = 0;

    /* Derive the device ID for active handset connection */
    A2dpDeviceFromBdaddr(&the_inst->bd_addr,&the_inst->a2dp.device_id);

    the_inst->a2dp.media_sink = A2dpMediaGetSink(the_inst->a2dp.device_id, the_inst->a2dp.stream_id);

    /* Set the Client-task in A2DP Profile library to receive further event data */
    PanicFalse(A2dpSetAppTask(the_inst->a2dp.device_id, &the_inst->av_task));
    /* TODO sync interfaces */
}

static void a2dpProfile_SetInstanceDataForSecondaryOrInitiator(avInstanceTaskData *the_inst)
{
    /* The A2DP profile state is set to disconnecting as the underlying connection 
     * library would initiate an internal disconnection and send 
     * A2DP_SIGNALLING_DISCONNECT_IND, and A2DP_MEDIA_CLOSE_IND with status 
     * a2dp_disconnect_transferred during the handover commit operation.
     *
     * We do not intend to make any UI specific changes during A2DP disconnection 
     * as the connection is handed over to peer earbud and disconnection event 
     * received on the local device is only an internal event.
     */
    if(the_inst->a2dp.state != A2DP_STATE_DISCONNECTED)
    {
        the_inst->a2dp.state = A2DP_STATE_DISCONNECTING;

        /* Clear audio lock flags to ensure AV instance on new secondary is destroyed */
        appA2dpClearAudioStartLockBit(the_inst);
        appA2dpClearAudioStopLockBit(the_inst);
    }
}

void A2dpProfile_Commit(avInstanceTaskData *the_inst,bool is_primary)
{
    if(is_primary == FALSE)
    {
        a2dpProfile_SetInstanceDataForSecondaryOrInitiator(the_inst);
    }
    else
    {
        a2dpProfile_SetInstanceDataForPrimaryOrAcceptor(the_inst);
    }
}

#endif /* INCLUDE_MIRRORING */
#endif /* INCLUDE_AV */
