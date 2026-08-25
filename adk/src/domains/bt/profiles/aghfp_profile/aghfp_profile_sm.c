/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AGHFP state machine component.
*/

#include "aghfp_profile_sm.h"
#include "aghfp_profile_instance.h"
#include "aghfp.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile.h"
#include "aghfp_profile_abstraction.h"
#include "aghfp_profile_config.h"
#include "aghfp_profile_audio.h"

#include <task_list.h>
#include <panic.h>
#include <logging.h>
#include <voice_sources_list.h>
#include <bt_device.h>
#include <profile_manager.h>
#include <telephony_messages.h>
#include <ui.h>

#include <connection_manager.h>

static profile_manager_disconnected_ind_reason_t aghfpProfileSm_ConvertReason(appAgHfpDisconnectReason aghfp_reason)
{
    profile_manager_disconnected_ind_reason_t reason;

    switch (aghfp_reason)
    {
        case APP_AGHFP_DISCONNECT_NORMAL:
            reason = profile_manager_disconnected_normal;
            break;

        case APP_AGHFP_DISCONNECT_LINKLOSS:
            reason = profile_manager_disconnected_link_loss;
            break;

        case APP_AGHFP_DISCONNECT_ERROR:
        default:
            reason = profile_manager_disconnected_error;
            break;
    }
    return reason;
}

/*! \brief Enter 'connected' state

    The HFP state machine has entered 'connected' state, this means that
    there is a SLC active.  At this point we need to retreive the remote device's
    support features to determine which (e)SCO packets it supports.  Also if there's an
    incoming or active call then answer/transfer the call to HF.
*/
static void aghfpProfileSm_EnterConnected(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterConnected(%p) enum:voice_source_t:%d", instance, source);

    /* Update most recent connected device */
    appDeviceUpdateMruDevice(&instance->hf_bd_addr);

    /* Read the remote supported features of the AG */
    ConnectionReadRemoteSuppFeatures(AghfpProfile_GetInstanceTask(instance), instance->slc_sink);
    /* If this is completing a connect request, send confirmation for this device */
    if (!ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&aghfp_profile_task_data.connect_request_clients),
                                           &instance->hf_bd_addr, profile_manager_success,
                                           profile_manager_hfp_profile, profile_manager_connect))
    {
        /* Otherwise provide indication to Profile Manager */
        ProfileManager_GenericConnectedInd(profile_manager_hfp_profile, &instance->hf_bd_addr);
    }

    Telephony_NotifyConnected(source);

    AghfpProfileAbstract_EnableInbandRingTone(instance);

    /* Tell clients we have connected */
    MAKE_AGHFP_MESSAGE(APP_AGHFP_CONNECTED_IND);
    message->instance = instance;
    message->bd_addr = instance->hf_bd_addr;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetStatusNotifyList()), APP_AGHFP_CONNECTED_IND, message);
}

/*! \brief Exit 'connected' state

    The HFP state machine has exited 'connected' state, this means that
    the SLC has closed.  Make sure any SCO link is disconnected.
*/
static void aghfpProfileSm_ExitConnected(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitConnected(%p) enum:voice_source_t:%d", instance, source);

    if (instance->slc_sink)
    {
        AghfpProfileAudio_DisconnectSco(instance);
    }
}

/*! \brief Enter 'connecting local' state

    The HFP state machine has entered 'connecting local' state.  Set the operation lock to
    serialise connect attempts, reset the page timeout to the default and attempt to connect SLC.
*/
static void aghfpProfileSm_EnterConnectingLocal(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterConnectingLocal(%p) enum:voice_source_t:%d", instance, source);

    AghfpProfileInstance_SetLock(instance, TRUE);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), PAGING_START);

    DEBUG_LOG("Connecting AGHFP to HF (%x,%x,%lx)", instance->hf_bd_addr.nap, instance->hf_bd_addr.uap, instance->hf_bd_addr.lap);

    AghfpProfileAbstract_EstablishSlcConnect(instance);
}

/*! \brief Exit 'connecting local' state

    The HFP state machine has exited 'connecting local' state, the connection
    attempt was successful or it failed.  Clear the operation lock to allow pending
    connection attempts and any pending operations on this instance to proceed.
*/
static void aghfpProfileSm_ExitConnectingLocal(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitConnectingLocal(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    AghfpProfileInstance_SetLock(instance, FALSE);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), PAGING_STOP);

    /* We have finished (successfully or not) attempting to connect, so
     * we can relinquish our lock on the ACL.  Bluestack will then close
     * the ACL when there are no more L2CAP connections */
    ConManagerReleaseAcl(&instance->hf_bd_addr);
}

/*! \brief Enter 'connecting remote' state

    The HFP state machine has entered the 'connecting remote' state when a HF device
    has initiated a connection. Set operation lock.
*/
static void aghfpProfileSm_EnterConnectingRemote(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterConnectingRemote(%p) enum:voice_source_t:%d", instance, source);
    AghfpProfileInstance_SetLock(instance, TRUE);
}

/*! \brief Exit 'connecting remote' state

    The HFP state machine has exited the 'connection remote' state. A HF device has either connected
    or failed to connect. Clear the operation lock.
*/
static void aghfpProfileSm_ExitConnectingRemote(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitConnectingRemote(%p) enum:voice_source_t:%d", instance, source);
    AghfpProfileInstance_SetLock(instance, FALSE);
}

/*! \brief Enter 'connected idle' state
    The HFP state machine has entered 'connected idle' state, this means that
    there is a SLC active but no active call in process. If coming from an incoming call
    send the call setup indicator
*/
static void aghfpProfileSm_EnterConnectedIdle(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterConnectedIdle(%p) enum:voice_source_t:%d", instance, source);

    if (aghfp_call_setup_none != instance->bitfields.call_setup)
    {
        instance->bitfields.call_setup = aghfp_call_setup_none;
        AghfpProfileAbstract_SendCallSetupIndication(instance);
    }

    if (instance->slc_sink)
    {
        AghfpProfileAudio_DisconnectSco(instance);
    }
}

/*! \brief Exit 'connected idle' state

    The HFP state machine has exited 'connected idle' state. Either for an incoming/outgoing call
    trying to establish a SCO link or to disconnect the SLC
*/
static void aghfpProfileSm_ExitConnectedIdle(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitConnectedIdle(%p) enum:voice_source_t:%d", instance, source);
}

/*! \brief Enter 'connected active' state

    The HFP state machine has entered 'connected incoming' state, this means that
    there is a SLC active and an audio connection is being established.
*/
static void aghfpProfileSm_EnterConnectedActive(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileSm_EnterConnectedActive(%p) enum:voice_source_t:%d", instance, source);

    bool start_audio = FALSE;

    if (instance->bitfields.call_setup != aghfp_call_setup_none)
    {
        instance->bitfields.call_setup = aghfp_call_setup_none;

        start_audio = TRUE;

        AghfpProfileAbstract_SendCallStatusIndication(instance);
        AghfpProfileAbstract_SendCallSetupIndication(instance);
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_CALL_START_IND);
    }

    /* Start audio connection if out of band and no connection has been previously established
       by the outgoing state
    */
    if (!instance->bitfields.in_band_ring && start_audio)
    {
        AghfpProfileAudio_ConnectSco(instance);
    }
}

/*! \brief Exiting 'connected active' state

    The HFP state machine has exited 'connected active' state, this means that
    there is a SLC active and the audio call is being stopped.
*/
static void aghfpProfileSm_ExitConnectedActive(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileSm_ExitConnectedActive(%p) enum:voice_source_t:%d", instance, source);

    if (instance->bitfields.call_status == aghfp_call_none)
    {
        AghfpProfileAbstract_SendCallStatusIndication(instance);
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_CALL_END_IND);
    }

    if (instance->slc_sink)
    {
        AghfpProfileAudio_DisconnectSco(instance);
    }
}


/*! \brief Enter 'connected incoming' state

    The HFP state machine has entered 'connected incoming' state, this means that
    there is a SLC active and an incoming call.
*/
static void aghfpProfileSm_EnterConnectedIncoming(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfileSm_EnterConnectedIncoming(%p) enum:voice_source_t:%d", instance, source);

    if (instance->bitfields.call_setup != aghfp_call_setup_incoming)
    {
        instance->bitfields.call_setup = aghfp_call_setup_incoming;
        AghfpProfileAbstract_SendCallSetupIndication(instance);
    }

    if (instance->bitfields.in_band_ring)
    {
        AghfpProfileAudio_ConnectSco(instance);
    }
    else
    {
        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RING_REQ);
        message->addr = instance->hf_bd_addr;
        MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RING_REQ, message);
    }
}

/*! \brief Exit 'connected incoming' state

    The HFP state machine has exited the 'connected incoming' state, this means that
    there is a SLC active and the call has either been accepted or rejected.
    Cancel any ring messages.
*/
static void aghfpProfileSm_ExitConnectedIncoming(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitConnectedIncoming(%p) enum:voice_source_t:%d", instance, source);

    MessageCancelAll(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RING_REQ);
}

/*! \brief Enter 'connected outgoing' state

*/
static void aghfpProfileSm_EnterConnectedOutgoing(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterConnectedOutgoing(%p) enum:voice_source_t:%d", instance, source);

    if (instance->bitfields.call_setup != aghfp_call_setup_outgoing)
    {
        instance->bitfields.call_setup = aghfp_call_setup_outgoing;
        AghfpProfileAbstract_SendCallSetupIndication(instance);
    }

    AghfpProfileAudio_ConnectSco(instance);
}

/*! \brief Exit 'connected outgoing' state

*/
static void aghfpProfileSm_ExitConnectedOutgoing(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitConnectedOutgoing(%p) enum:voice_source_t:%d", instance, source);

}


/*! \brief Enter 'disconnecting' state

    The HFP state machine is entering the disconnecting state which means the SLC is being
    disconnected.
*/
static void aghfpProfileSm_EnterDisconnecting(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterDisconnecting(%p) enum:voice_source_t:%d", instance, source);

    AghfpProfileAbstract_SlcDisconnect(instance);
}

/*! \brief Exit 'disconnecting' state

    The HFP state machine is either entering the 'disconnected' state or 'connected' state
*/
static void aghfpProfileSm_ExitDisconnecting(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitDisconnecting(%p) enum:voice_source_t:%d", instance, source);
}

/*! \brief Enter 'disconnected' state

    The HFP state machine has entered 'disconnected' state, this means that
    there is now no SLC active.
*/
static void aghfpProfileSm_EnterDisconnected(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_EnterDisconnected(%p) enum:voice_source_t:%d", instance, source);

    /* Check with profile manager if this is completing a disconnect request. */
    task_list_t *list = TaskList_GetBaseTaskList(&aghfp_profile_task_data.disconnect_request_clients);;
    profile_manager_request_type_t type = profile_manager_disconnect;
    profile_manager_request_cfm_result_t result = profile_manager_success;

    if (instance->disconnect_reason == APP_AGHFP_CONNECT_FAILED)
    {
        /* Check for a connect request instead. */
        list = TaskList_GetBaseTaskList(&aghfp_profile_task_data.connect_request_clients);
        type = profile_manager_connect;
        result = profile_manager_failed;
    }

    /* Notify profile manager of request completion. */
    if (!ProfileManager_NotifyConfirmation(list, &instance->hf_bd_addr, result,
                                           profile_manager_hfp_profile, type))
    {
        /* Otherwise, indicate unsolicited disconnection to profile manager, and the reason for it. */
        profile_manager_disconnected_ind_reason_t reason = aghfpProfileSm_ConvertReason(instance->disconnect_reason);
        ProfileManager_GenericDisconnectedInd(profile_manager_hfp_profile, &instance->hf_bd_addr, reason);
    }

    /* Tell clients we have disconnected */
    MAKE_AGHFP_MESSAGE(APP_AGHFP_DISCONNECTED_IND);
    message->instance = instance;
    message->bd_addr = instance->hf_bd_addr;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetStatusNotifyList()), APP_AGHFP_DISCONNECTED_IND, message);
}

/*! \brief Exit 'disconnected' state

    The HFP state machine has entered 'connected' state, this means that
    there is now an SLC connection in progress.
*/
static void aghfpProfileSm_ExitDisconnected(aghfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG("aghfpProfileSm_ExitDisconnected(%p) enum:voice_source_t:%d", instance, source);

    /* Reset disconnect reason */
    instance->disconnect_reason = APP_AGHFP_CONNECT_FAILED;
}

aghfpState AghfpProfile_GetState(aghfpInstanceTaskData* instance)
{
    PanicNull(instance);
    return instance->state;
}

/*! \brief Set AG HFP state

    Called to change state.  Handles calling the state entry and exit functions.
    Note: The entry and exit functions will be called regardless of whether or not
    the state actually changes value.
*/
void AghfpProfile_SetState(aghfpInstanceTaskData* instance, aghfpState state)
{
    aghfpState old_state = AghfpProfile_GetState(instance);
    voice_source_t source = AghfpProfileInstance_GetVoiceSourceForInstance(instance);

    DEBUG_LOG("AghfpProfile_SetState(%p, enum:aghfpState:%d -> enum:aghfpState:%d)",
              instance, old_state, state);

    switch (old_state)
    {
        case AGHFP_STATE_CONNECTING_LOCAL:
            aghfpProfileSm_ExitConnectingLocal(instance, source);
            break;

        case AGHFP_STATE_CONNECTING_REMOTE:
            aghfpProfileSm_ExitConnectingRemote(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_IDLE:
            aghfpProfileSm_ExitConnectedIdle(instance, source);
            if (state < AGHFP_STATE_CONNECTED_IDLE || state > AGHFP_STATE_CONNECTED_ACTIVE)
                aghfpProfileSm_ExitConnected(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_ACTIVE:
            aghfpProfileSm_ExitConnectedActive(instance, source);
            if (state < AGHFP_STATE_CONNECTED_IDLE || state > AGHFP_STATE_CONNECTED_ACTIVE)
                aghfpProfileSm_ExitConnected(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_INCOMING:
            aghfpProfileSm_ExitConnectedIncoming(instance, source);
            if (state < AGHFP_STATE_CONNECTED_IDLE || state > AGHFP_STATE_CONNECTED_ACTIVE)
                aghfpProfileSm_ExitConnected(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_OUTGOING:
            aghfpProfileSm_ExitConnectedOutgoing(instance, source);
            if (state < AGHFP_STATE_CONNECTED_IDLE || state > AGHFP_STATE_CONNECTED_ACTIVE)
                aghfpProfileSm_ExitConnected(instance, source);
            break;

        case AGHFP_STATE_DISCONNECTING:
            aghfpProfileSm_ExitDisconnecting(instance, source);
            break;

        case AGHFP_STATE_DISCONNECTED:
            aghfpProfileSm_ExitDisconnected(instance, source);
            break;
        default:
            break;
    }

    /* Set new state */
    instance->state = state;

    /* Handle state entry functions */
    switch (state)
    {
        case AGHFP_STATE_CONNECTING_LOCAL:
            aghfpProfileSm_EnterConnectingLocal(instance, source);
            break;

        case AGHFP_STATE_CONNECTING_REMOTE:
            aghfpProfileSm_EnterConnectingRemote(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_IDLE:
            if (old_state < AGHFP_STATE_CONNECTED_IDLE || old_state > AGHFP_STATE_CONNECTED_ACTIVE)
            {
                aghfpProfileSm_EnterConnected(instance, source);
            }
            aghfpProfileSm_EnterConnectedIdle(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_ACTIVE:
            if (old_state < AGHFP_STATE_CONNECTED_IDLE || old_state > AGHFP_STATE_CONNECTED_ACTIVE)
            {
                aghfpProfileSm_EnterConnected(instance, source);
            }
            aghfpProfileSm_EnterConnectedActive(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_INCOMING:
            if (old_state < AGHFP_STATE_CONNECTED_IDLE || old_state > AGHFP_STATE_CONNECTED_ACTIVE)
            {
                aghfpProfileSm_EnterConnected(instance, source);
            }
            aghfpProfileSm_EnterConnectedIncoming(instance, source);
            break;

        case AGHFP_STATE_CONNECTED_OUTGOING:
            if (old_state < AGHFP_STATE_CONNECTED_IDLE || old_state > AGHFP_STATE_CONNECTED_ACTIVE)
            {
                aghfpProfileSm_EnterConnected(instance, source);
            }
            aghfpProfileSm_EnterConnectedOutgoing(instance, source);
            break;

        case AGHFP_STATE_DISCONNECTING:
            aghfpProfileSm_EnterDisconnecting(instance, source);
            break;

        case AGHFP_STATE_DISCONNECTED:
            aghfpProfileSm_EnterDisconnected(instance, source);
            break;
        default:
            break;
    }
}
