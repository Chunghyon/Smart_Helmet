/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      HFP state machine component.
*/

#include "hfp_profile_sm.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "hfp_profile_config.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
#include "hfp_profile_instance.h"
#include "hfp_profile.h"
#include "hfp_profile_port.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_states.h"
#include "hfp_profile_telephony_control.h"
#include "hfp_profile_volume.h"
#include "hfp_profile_volume_observer.h"
#include "telephony_messages.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "kymera.h"

#include <profile_manager.h>
#include <av.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <focus_voice_source.h>
#include <logging.h>
#include <message.h>
#include "ui.h"
#include "sass.h"

#include <logging.h>

static void hfpProfile_SetHfpProfile(const bdaddr *bd_addr, hfp_profile profile)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        Device_SetPropertyU8(device, device_property_hfp_profile, (uint8)profile);
    }
}

/*! \brief Enter 'connected' sub-state

    The HFP state machine has entered 'connected' sub-state, this means that
    there is a SLC active.  At this point we need to retreive the remote device's
    support features to determine which (e)SCO packets it supports.  Also if there's an
    incoming or active call then answer/transfer the call to headset.
*/
static void appHfpEnterConnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnected(%p) enum:voice_source_t:%d", instance, source);

    /* Update most recent connected device */
    appDeviceUpdateMruDevice(&instance->ag_bd_addr);
    hfpProfile_SetHfpProfile(&instance->ag_bd_addr, instance->profile);
    hfpProfile_ReadRemoteSupportedFeatures(instance);

    /* Clear detach pending flag */
    instance->bitfields.detach_pending = FALSE;

    /* Check if connected as HFP 1.5+ */
    if (instance->profile == hfp_handsfree_profile)
    {
        /* Inform AG of the current gain settings */
        /* hfp_primary_link indicates the link that was connected first */
        uint8 value = appHfpGetVolume(instance);
        hfpProfile_SetSpeakerGain(instance, value);

        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        PanicNull(device);
        if (!Device_GetPropertyU8(device, device_property_hfp_mic_gain, &value))
        {
            value = HFP_MICROPHONE_GAIN;
        }
        hfpProfile_SetMicrophoneGain(instance, value);
    }

    /* If this is completing a connect request, send confirmation for this device */
    if (!ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients),
                                           &instance->ag_bd_addr, profile_manager_success,
                                           profile_manager_hfp_profile, profile_manager_connect))
    {
        /* otherwise provide indication to the Profile Manager */
        ProfileManager_GenericConnectedInd(profile_manager_hfp_profile, &instance->ag_bd_addr);
    }

    Telephony_NotifyConnected(source);
    /* Tell clients we have connected */
    MAKE_HFP_MESSAGE(APP_HFP_CONNECTED_IND);
    message->bd_addr = instance->ag_bd_addr;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_CONNECTED_IND, message);

#if defined(HFP_CONNECT_AUTO_ANSWER) || defined(HFP_CONNECT_AUTO_TRANSFER)
    if (instance->profile != hfp_headset_profile)
    {
#if defined(HFP_CONNECT_AUTO_ANSWER)
        /* Check if incoming call and latest barge-in connection was for SASS switching. Don't auto answer the call 
           in case of Smart Audio Source Switch as it's a strange user experience when SASS switch happens upon getting 
           an incoming call on the paired handset as SASS doesn't require any user action to perform switch. */
        if (appHfpGetState(instance) == HFP_STATE_CONNECTED_INCOMING && !Sass_IsBargeInForSassSwitch())
        {
            /* Accept the incoming call */
            VoiceSources_AcceptIncomingCall(source);
        }

#endif
#if defined(HFP_CONNECT_AUTO_TRANSFER)
        /* Check if incoming call */
        if (appHfpGetState(instance) == HFP_STATE_CONNECTED_ACTIVE)
        {
            /* Check SCO is not active */
            if (instance->sco_sink == 0)
            {
                /* Attempt to transfer audio */
                hfpProfile_HandleAudioTransferRequest(instance, voice_source_audio_transfer_to_hfp);
            }
        }
#endif
    }
#endif
}

/*! \brief Exit 'connected' sub-state

    The HFP state machine has exited 'connected' sub-state, this means that
    the SLC has closed.  Make sure any SCO link is disconnected.
*/
static void appHfpExitConnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnected(%p) enum:voice_source_t:%d", instance, source);

    /* Unregister for battery updates */
    appBatteryUnregister(HfpProfile_GetInstanceTask(instance));

    /* Reset hf_indicator_assigned_num */
    instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;

    hfpProfile_HandleAudioTransferRequest(instance, voice_source_audio_transfer_to_ag);
}

/*! \brief Enter 'outgoing call' sub-state

    The HFP state machine has entered 'outgoing call' sub-state, this means
    that we are in the process of making an outgoing call. Update UI to indicate active call.
*/
static void appHfpEnterOutgoingCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterOutgoingCall(%p) enum:voice_source_t:%d", instance, source);
    /* User action started an outgoing call, so make this device most recently used */
    appDeviceUpdateMruDevice(&instance->ag_bd_addr);
    Telephony_NotifyCallOutgoing(source);
}

/*! \brief Exit 'outgoing call' sub-state

    The HFP state machine has exited 'outgoing call' sub-state.
*/
static void appHfpExitOutgoingCall(hfpInstanceTaskData* instance, voice_source_t source)
{    
    DEBUG_LOG_INFO("appHfpExitOutgoingCall(%p) enum:voice_source_t:%d", instance, source);
    Telephony_NotifyCallOutgoingEnded(source);
}

/*! \brief Enter 'incoming call' sub-state

    The HFP state machine has entered 'incoming call' sub-state, this means
    that there's an incoming call in progress.  Update UI to indicate incoming
    call.
*/
static void appHfpEnterIncomingCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterIncomingCall(%p) enum:voice_source_t:%d", instance, source);
    
    appDeviceUpdateMruDevice(&instance->ag_bd_addr);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_INCOMING_RING_IND);

    Telephony_NotifyCallIncoming(source);
}

/*! \brief Exit 'incoming call' sub-state

    The HFP state machine has exited 'incoming call' sub-state, this means
    that the incoming call has either been accepted or rejected.  Make sure
    any ring tone is cancelled.
*/
static void appHfpExitIncomingCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitIncomingCall(%p) enum:voice_source_t:%d", instance, source);

    /* Clear call accepted flag */
    instance->bitfields.call_accepted = FALSE;

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_INCOMING_ENDED_IND);

    Telephony_NotifyCallIncomingEnded(source);

    /* Cancel HSP incoming call timeout */
    MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT);
}

/*! \brief Enter 'established call' sub-state

    The HFP state machine has entered 'established call' sub-state, this means
    that either a call is in progress or a call is on hold
*/
static void appHfpEnterEstablishedCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterEstablishedCall(%p) enum:voice_source_t:%d", instance, source);

    BandwidthManager_FeatureStart(BANDWIDTH_MGR_FEATURE_ESCO);
    
    Telephony_NotifyCallActive(source);
}

/*! \brief Exit 'established call' sub-state

    The HFP state machine has exited 'established call' sub-state, this means
    that there are no calls established or on hold.  Make sure mute is cancelled.
*/
static void appHfpExitEstablishedCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitEstablishedCall(%p) enum:voice_source_t:%d", instance, source);

    Telephony_NotifyMicrophoneUnmuted(source);

    instance->bitfields.mute_active = FALSE;

    Telephony_NotifyCallEnded(source);
    BandwidthManager_FeatureStop(BANDWIDTH_MGR_FEATURE_ESCO);
}

/*! \brief Enter 'active call' sub-state

    The HFP state machine has entered 'active call' sub-state, this means
    that a call is in progress
*/
static void appHfpEnterActiveCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterActiveCall(%p) enum:voice_source_t:%d", instance, source);
    appDeviceUpdateMruDevice(&instance->ag_bd_addr);
}

/*! \brief Enter 'held call' sub-state

    The HFP state machine has entered 'held call' sub-state, this means
    that a call is on hold
*/
static void appHfpEnterHeldCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterHeldCall(%p) enum:voice_source_t:%d", instance, source);

    Telephony_NotifyHoldActive(source);
}

/*! \brief Exit 'held call' sub-state

    The HFP state machine has exited 'held call' sub-state, this means
    that there are no calls on hold.
*/
static void appHfpExitHeldCall(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitHeldCall(%p) enum:voice_source_t:%d", instance, source);
    Telephony_NotifyHoldInactive(source);
}

/*! \brief Enter 'disconnecting' state

    The HFP state machine has entered 'disconnecting' state, this means
    that the SLC should be disconnected.  Set the operation lock to block
    any pending operations.
*/
static void appHfpEnterDisconnecting(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterDisconnecting(%p) enum:voice_source_t:%d", instance, source);

    /* Set operation lock */
    HfpProfileInstance_SetLock(instance, TRUE);
    hfpProfile_DisconnectSlc(instance);
}

/*! \brief Exit 'disconnecting' state

    The HFP state machine has exited 'disconnecting' state, this means
    that the SLC is now disconnected.  Clear the operation lock to allow
    any pending operations.
*/
static void appHfpExitDisconnecting(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitDisconnecting(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    HfpProfileInstance_SetLock(instance, FALSE);
}

static profile_manager_disconnected_ind_reason_t hfpProfileSm_ConvertReason(appHfpDisconnectReason hfp_reason)
{
    profile_manager_disconnected_ind_reason_t reason;
    switch (hfp_reason)
    {
    case APP_HFP_DISCONNECT_NORMAL:
        reason = profile_manager_disconnected_normal;
        break;
    case APP_HFP_DISCONNECT_LINKLOSS:
        reason = profile_manager_disconnected_link_loss;
        break;
    case APP_HFP_DISCONNECT_ERROR:
    default:
        reason = profile_manager_disconnected_error;
        break;
    }
    return reason;
}

/*! \brief Enter 'disconnected' state

    The HFP state machine has entered 'disconnected' state, this means
    that there is no active SLC.  Reset all flags, clear the ACL lock to
    allow pending connections to proceed.  Also make sure AV streaming is
    resumed if previously suspended.
*/
static void appHfpEnterDisconnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    bool wasNotified = FALSE;
    device_t handset_device = HfpProfileInstance_FindDeviceFromInstance(instance);

    DEBUG_LOG_INFO("appHfpEnterDisconnected(%p) enum:voice_source_t:%d", instance, source);

    if (ProfileManager_DeviceIsOnList(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients), handset_device))
    {
        DEBUG_LOG_INFO("appHfpEnterDisconnected during connect request due to enum:appHfpDisconnectReason:%d",
                       instance->bitfields.disconnect_reason);

        /* If this is due to an unsuccessful connect request, send confirmation for this device */
        wasNotified = ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients),
                                                        &instance->ag_bd_addr, profile_manager_failed,
                                                        profile_manager_hfp_profile, profile_manager_connect);
    }
    if (ProfileManager_DeviceIsOnList(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients), handset_device))
    {
        if (instance->bitfields.disconnect_reason == APP_HFP_DISCONNECT_NORMAL)
        {
            wasNotified = ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients),
                                                            &instance->ag_bd_addr,
                                                            profile_manager_success,
                                                            profile_manager_hfp_profile, profile_manager_disconnect);
        }
    }
    if (!wasNotified)
    {
        DEBUG_LOG_INFO("appHfpEnterDisconnected when no pending requests, raising indication with enum:appHfpDisconnectReason:%d",
                       instance->bitfields.disconnect_reason);

        profile_manager_disconnected_ind_reason_t reason = hfpProfileSm_ConvertReason(instance->bitfields.disconnect_reason);
        ProfileManager_GenericDisconnectedInd(profile_manager_hfp_profile, &instance->ag_bd_addr, reason);
    }

    /* Tell clients we have disconnected */
    MAKE_HFP_MESSAGE(APP_HFP_DISCONNECTED_IND);
    message->bd_addr = instance->ag_bd_addr;
    message->reason =  instance->bitfields.disconnect_reason;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_DISCONNECTED_IND, message);

    /* Clear status flags */
    instance->bitfields.caller_id_active = FALSE;
    instance->bitfields.voice_recognition_active = FALSE;
    instance->bitfields.voice_recognition_request = FALSE;
    instance->bitfields.mute_active = FALSE;
    instance->bitfields.in_band_ring = FALSE;
    instance->bitfields.call_accepted = FALSE;

    /* Clear call state indication */
    instance->bitfields.call_state = 0;
}

static void appHfpExitDisconnected(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitDisconnected(%p) enum:voice_source_t:%d", instance, source);

    /* Reset disconnect reason */
    instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
}

/*! \brief Enter 'connecting remote' state

    The HFP state machine has entered 'connecting remote' state, this is due
    to receiving a incoming SLC indication. Set operation lock to block any
    pending operations.
*/
static void appHfpEnterConnectingRemote(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectingRemote(%p) enum:voice_source_t:%d", instance, source);

    /* Set operation lock */
    HfpProfileInstance_SetLock(instance, TRUE);

    /* Clear detach pending flag */
    instance->bitfields.detach_pending = FALSE;
}

/*! \brief Exit 'connecting remote' state

    The HFP state machine has exited 'connecting remote' state.  Clear the
    operation lock to allow pending operations on this instance to proceed.
*/
static void appHfpExitConnectingRemote(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectingRemote(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    HfpProfileInstance_SetLock(instance, FALSE);
}

/*! \brief Enter 'connecting local' state

    The HFP state machine has entered 'connecting local' state.  Set the
    'connect busy' flag and operation lock to serialise connect attempts,
    reset the page timeout to the default and attempt to connect SLC.
    Make sure AV streaming is suspended.
*/
static void appHfpEnterConnectingLocal(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpEnterConnectingLocal(%p) enum:voice_source_t:%d", instance, source);

    /* Set operation lock */
    HfpProfileInstance_SetLock(instance, TRUE);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), PAGING_START);
    
    hfpProfile_ConnectSlc(instance);

    /* Clear detach pending flag */
    instance->bitfields.detach_pending = FALSE;
}

/*! \brief Exit 'connecting local' state

    The HFP state machine has exited 'connecting local' state, the connection
    attempt was successful or it failed.  Clear the 'connect busy' flag and
    operation lock to allow pending connection attempts and any pending
    operations on this instance to proceed.  AV streaming can resume now.
*/
static void appHfpExitConnectingLocal(hfpInstanceTaskData* instance, voice_source_t source)
{
    DEBUG_LOG_INFO("appHfpExitConnectingLocal(%p) enum:voice_source_t:%d", instance, source);

    /* Clear operation lock */
    HfpProfileInstance_SetLock(instance, FALSE);

    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), PAGING_STOP);

    /* We have finished (successfully or not) attempting to connect, so
     * we can relinquish our lock on the ACL.  Bluestack will then close
     * the ACL when there are no more L2CAP connections */
    ConManagerReleaseAcl(&instance->ag_bd_addr);
}

/*! \brief Set HFP state

    Called to change state.  Handles calling the state entry and exit functions.
    Note: The entry and exit functions will be called regardless of whether or not
    the state actually changes value.
*/
void appHfpSetState(hfpInstanceTaskData* instance, hfpState state)
{
    /* copy old state */
    hfpState old_state = appHfpGetState(instance);
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    voice_source_t focused_source = voice_source_none;

    DEBUG_LOG("appHfpSetState(%p, enum:hfpState:%d -> enum:hfpState:%d)",
              instance, old_state, state);

    /* Handle state exit functions */
    switch (old_state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
            appHfpExitConnectingLocal(instance, source);
            break;
        case HFP_STATE_CONNECTING_REMOTE:
            appHfpExitConnectingRemote(instance, source);
            break;
        case HFP_STATE_DISCONNECTING:
            appHfpExitDisconnecting(instance, source);
            break;
        case HFP_STATE_DISCONNECTED:
            appHfpExitDisconnected(instance, source);
            break;
        default:
            break;
    }
    
    /* Handle sub-state transitions which cover exiting several states */
    if(HfpProfile_StateHasIncomingCall(old_state) && !HfpProfile_StateHasIncomingCall(state))
    {
        appHfpExitIncomingCall(instance, source);
    }
    
    if(HfpProfile_StateHasOutgoingCall(old_state) && !HfpProfile_StateHasOutgoingCall(state))
    {
        appHfpExitOutgoingCall(instance, source);
    }
    
    if(HfpProfile_StateHasHeldCall(old_state) && !HfpProfile_StateHasHeldCall(state))
    {
        appHfpExitHeldCall(instance, source);
    }
    
    if(HfpProfile_StateHasEstablishedCall(old_state) && !HfpProfile_StateHasEstablishedCall(state))
    {
        appHfpExitEstablishedCall(instance, source);
    }
    
    if(HfpProfile_StateIsSlcConnected(old_state) && !HfpProfile_StateIsSlcConnected(state))
    {
        appHfpExitConnected(instance, source);
    }

    /* Set new state */
    instance->state = state;
    
    /* Handle sub-state transitions which cover entering several states */
    if(!HfpProfile_StateIsSlcConnected(old_state) && HfpProfile_StateIsSlcConnected(state))
    {
        appHfpEnterConnected(instance, source);
    }
    
    if(!HfpProfile_StateHasEstablishedCall(old_state) && HfpProfile_StateHasEstablishedCall(state))
    {
        appHfpEnterEstablishedCall(instance, source);
    }
    
    if(!HfpProfile_StateHasActiveCall(old_state) && HfpProfile_StateHasActiveCall(state))
    {
        appHfpEnterActiveCall(instance, source);
    }
    
    if(!HfpProfile_StateHasHeldCall(old_state) && HfpProfile_StateHasHeldCall(state))
    {
        appHfpEnterHeldCall(instance, source);
    }
    
    if(!HfpProfile_StateHasOutgoingCall(old_state) && HfpProfile_StateHasOutgoingCall(state))
    {
        appHfpEnterOutgoingCall(instance, source);
    }
    
    if(!HfpProfile_StateHasIncomingCall(old_state) && HfpProfile_StateHasIncomingCall(state))
    {
        appHfpEnterIncomingCall(instance, source);
    }

    /* Handle state entry functions */
    switch (state)
    {
        case HFP_STATE_CONNECTING_LOCAL:
            appHfpEnterConnectingLocal(instance, source);
            break;
        case HFP_STATE_CONNECTING_REMOTE:
            appHfpEnterConnectingRemote(instance, source);
            break;
        case HFP_STATE_DISCONNECTING:
            appHfpEnterDisconnecting(instance, source);
            break;
        case HFP_STATE_DISCONNECTED:
            appHfpEnterDisconnected(instance, source);
            break;
        default:
            break;
    }

    Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source);
    if (focused_source == source)
    {
        Ui_InformContextChange(ui_provider_telephony, VoiceSources_GetSourceContext(source));
    }
    else
    {
        DEBUG_LOG_VERBOSE("appHfpSetState didn't push context for unfocused enum:voice_source_t:%d", source);
    }

    /* Update link policy following change in state */
    appLinkPolicyUpdatePowerTable(HfpProfile_GetHandsetBdAddr(instance));
}

hfpState appHfpGetState(hfpInstanceTaskData* instance)
{
    if(instance)
    {
        return instance->state;
    }
    
    return HFP_STATE_NULL;
}
