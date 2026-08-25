/*!
\copyright  Copyright (c) 2008 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain HFP component.
*/

#include <panic.h>
#include <ps.h>
#include <ps_key_map.h>

#ifdef INCLUDE_HFP

#include "hfp_profile.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
#include "hfp_profile_battery_level.h"
#include "hfp_profile_config.h"
#include "hfp_profile_instance.h"
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
#include <device_db_serialiser.h>
#include <focus_voice_source.h>
#include <focus_generic_source.h>
#include <logging.h>
#include <stdio.h>
#include <stdlib.h>
#include <message.h>
#include "ui.h"
#include <timestamp_event.h>
#include <power_manager.h>
#include <string.h>

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(APP_HFP, APP_HFP_MESSAGE_END)
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(HFP_INTERNAL_MESSAGE_END)

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(hfp_profile_messages)
LOGGING_PRESERVE_MESSAGE_ENUM(hfp_profile_internal_messages)

/*! \brief Application HFP component main data structure. */
hfpTaskData hfp_profile_task_data;

/* Local Function Prototypes */
static void hfpProfile_TaskMessageHandler(Task task, MessageId id, Message message);

/*! \brief Is HFP voice recognition active for the specified instance*/
static bool hfpProfile_IsVoiceRecognitionActive(hfpInstanceTaskData * instance)
{
    return (instance != NULL) ? instance->bitfields.voice_recognition_active : FALSE;
}

/*! \brief Check SCO encryption

    This functions is called to check if SCO is encrypted or not.  If there is
    a SCO link active, a call is in progress and the link becomes unencrypted,
    send a Telephony message that could be used to provide an indication tone
    to the user, depenedent on UI configuration.
*/
void HfpProfile_CheckEncryptedSco(hfpInstanceTaskData * instance)
{
    DEBUG_LOG("HfpProfile_CheckEncryptedSco(%p) encrypted=%d sink=%x)",
              instance, instance->bitfields.encrypted, instance->sco_sink);

    /* Check SCO is active */
    if (HfpProfile_IsScoActiveForInstance(instance) && appHfpIsCallForInstance(instance))
    {
        /* Check if link is encrypted */
        if (!HfpProfileInstance_IsEncrypted(instance))
        {
            voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
            if (source != voice_source_none)
            {
                Telephony_NotifyCallBecameUnencrypted(source);
            }
            /* \todo Mute the MIC to prevent eavesdropping */
        }
    }
}

void hfpProfile_HandleInitComplete(bool compete_success)
{
    /* Check HFP initialisation was successful */
    if (compete_success)
    {
        /* Tell main application task we have initialised */
        MessageSend(SystemState_GetTransitionTask(), APP_HFP_INIT_CFM, 0);
    }
    else
    {
        Panic();
    }
}

/*! \brief Send SLC status indication to all clients on the list.
 */
static void hfpProfile_SendSlcStatus(bool connected, const bdaddr* bd_addr)
{
    Task next_client = 0;

    while (TaskList_Iterate(TaskList_GetFlexibleBaseTaskList(appHfpGetSlcStatusNotifyList()), &next_client))
    {
        MAKE_HFP_MESSAGE(APP_HFP_SLC_STATUS_IND);
        message->slc_connected = connected;
        message->bd_addr = *bd_addr;
        MessageSend(next_client, APP_HFP_SLC_STATUS_IND, message);
    }
}

static deviceLinkMode hfpProfile_GetLinkMode(device_t device)
{
    deviceLinkMode link_mode = DEVICE_LINK_MODE_UNKNOWN;
    void *value = NULL;
    size_t size = sizeof(deviceLinkMode);
    if (Device_GetProperty(device, device_property_link_mode, &value, &size))
    {
        link_mode = *(deviceType *)value;
    }
    return link_mode;
}

bool hfpProfile_IsSecureConnection(const bdaddr *bd_addr)
{
    bool is_secure_connection = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        is_secure_connection = (hfpProfile_GetLinkMode(device) == DEVICE_LINK_MODE_SECURE_CONNECTION);
    }
    return is_secure_connection;
}

static void hfpProfile_SendCallStatusNotification(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    if (VoiceSource_IsHfp(source))
    {
        /* Notify if the instance state indicates there is an active call */
        if(HfpProfile_StateHasOutgoingCall(state))
        {
            Telephony_NotifyCallOutgoing(source);
        }
        else if (HfpProfile_StateHasActiveCall(state))
        {
            Telephony_NotifyCallActive(source);
        }

        /* Also indicate if there is an incoming call (eg. active + incoming) */
        if(HfpProfile_StateHasIncomingCall(state))
        {
            Telephony_NotifyCallIncoming(source);
        }
    }
}

void hfpProfile_HandleConnectCompleteSuccess(hfpInstanceTaskData * instance)
{
    appHfpSetState(instance, hfpProfile_GetStateFromCallState(instance->bitfields.call_state));
    
    /* Notify clients of SLC connection and also if there is an active call */
    hfpProfile_SendSlcStatus(TRUE, &(instance->ag_bd_addr));
    hfpProfile_SendCallStatusNotification(instance);
}

void hfpProfile_HandleConnectCompleteFailed(hfpInstanceTaskData * instance, bool hfp_not_supported)
{
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    
    if (hfp_not_supported)
    {
        BtDevice_RemoveSupportedProfiles(&(instance->ag_bd_addr), DEVICE_PROFILE_HFP);
    }

    /* The SLC connection was not successful, notify clients */
    Telephony_NotifyCallConnectFailure(source);

    /* Tear down the HFP instance. */
    instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
    appHfpSetState(instance, HFP_STATE_DISCONNECTED);
    HfpProfileInstance_Destroy(instance);
}

void hfpProfile_HandleDisconnectComplete(hfpInstanceTaskData * instance, appHfpDisconnectReason reason)
{
    PanicNull(instance);

    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    hfpState state = appHfpGetState(instance);

    if(HfpProfile_StateIsSlcConnectedOrConnecting(state))
    {
        if (reason == APP_HFP_DISCONNECT_LINKLOSS && !instance->bitfields.detach_pending)
        {
            Telephony_NotifyDisconnectedDueToLinkloss(source);

             /* Set disconnect reason */
             instance->bitfields.disconnect_reason = APP_HFP_DISCONNECT_LINKLOSS;
        }
        else
        {
            Telephony_NotifyDisconnected(source);

            /* Set disconnect reason */
            instance->bitfields.disconnect_reason = APP_HFP_DISCONNECT_NORMAL;
        }

        /* inform clients */
        hfpProfile_SendSlcStatus(FALSE, HfpProfile_GetHandsetBdAddr(instance));

        /* Move to disconnected state */
        appHfpSetState(instance, HFP_STATE_DISCONNECTED);

        HfpProfileInstance_Destroy(instance);
    }
    else if(HfpProfile_StateIsSlcDisconnectedOrDisconnecting(state))
    {
        bool reconnect_handset = instance->bitfields.reconnect_handset;

        /* If the status is "transferred" do not notify clients and change
           state in the usual manner. Notifying clients could cause UI
           changes (e.g. playing the "disconnected" prompt) which isn't
           required during handover, as the link is "transferred", not
           disconnected. The new secondary sets its state to
           HFP_STATE_DISCONNECTED on commit, allowing the HFP instance
           to be cleanly destroyed. */
        if (reason != APP_HFP_DISCONNECT_TRANSFERRED)
        {
            Telephony_NotifyDisconnected(source);

            /* Set disconnect reason */
            instance->bitfields.disconnect_reason = APP_HFP_DISCONNECT_NORMAL;

            /* Move to disconnected state */
            appHfpSetState(instance, HFP_STATE_DISCONNECTED);
        }

        HfpProfileInstance_Destroy(instance);

        if (reconnect_handset)
        {
            DEBUG_LOG("hfpProfile_HandleDisconnectComplete: Connecting Back Handset");
            HfpProfile_ConnectHandset();
        }
    }
}

void hfpProfile_HandleHfpAudioConnectIncoming(hfpInstanceTaskData * instance, bool is_esco)
{
    voice_source_t source;
    PanicNull(instance);
    
    source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    hfpState state = appHfpGetState(instance);

    if(HfpProfile_StateIsSlcConnectedOrConnecting(state))
    {
        /* Set flag so context presented to focus module reflects that this link will have audio */
        instance->bitfields.esco_connecting = TRUE;
        instance->bitfields.esco_disconnecting = FALSE;

        if(HfpProfile_IsScoActive() && (Focus_GetFocusForVoiceSource(source) != focus_foreground))
        {
            DEBUG_LOG("hfpProfile_HandleHfpAudioConnectInd reject as not in focus");
            /* If we already have an active SCO and this link does not have priority, reject it */
            hfpProfile_SendAudioConnectResponse(instance, is_esco, FALSE);
            instance->bitfields.esco_connecting = FALSE;

            if(HfpProfile_StateHasIncomingCall(state))
            {
                /* Fake an out-of-band ring once to notify user */
                MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ);
                MessageSend(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ, NULL);
            }
        }
        else
        {
            if (hfp_profile_task_data.sco_sync_task)
            {
                Telephony_NotifyCallAudioConnecting(source);
                
                /* Notify sco_sync_task and wait for HfpProfile_ScoConnectingSyncResponse */
                MESSAGE_MAKE(sync_ind, APP_HFP_SCO_CONNECTING_SYNC_IND_T);
                sync_ind->device = BtDevice_GetDeviceForBdAddr(&instance->ag_bd_addr);
                sync_ind->link_type = is_esco ? sync_link_esco : sync_link_sco;
                MessageSend(hfp_profile_task_data.sco_sync_task, APP_HFP_SCO_CONNECTING_SYNC_IND, sync_ind);
            }
            else
            {
                hfpProfile_SendAudioConnectResponse(instance, is_esco, TRUE);
            }
        }
    }
    else
    {
        /* Reject SCO connection */
        hfpProfile_SendAudioConnectResponse(instance, is_esco, FALSE);
    }
}

bool HfpProfile_HandsetSupportsSuperWideband(uint16 qce_codec_mode_id)
{
    bool swb_supported = FALSE;

    if((qce_codec_mode_id == aptx_adaptive_64_2_EV3) ||
       (qce_codec_mode_id == aptx_adaptive_64_2_EV3_QHS3) ||
       (qce_codec_mode_id == aptx_adaptive_64_QHS3))
    {
        swb_supported = TRUE;
    }
    DEBUG_LOG("HfpProfile_HandsetSupportsSuperWideband:%d",swb_supported);
    return swb_supported;
}

bool HfpProfile_IsHandsetBlockedForSwb(const bdaddr *bd_addr)
{
    bool handset_blocked = FALSE;
    device_t device  = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        handset_blocked = ((flags & DEVICE_FLAGS_SWB_NOT_SUPPORTED) == DEVICE_FLAGS_SWB_NOT_SUPPORTED);
    }
     DEBUG_LOG("HfpProfile_IsHandsetBlockedForSwb:%d",handset_blocked);
    return handset_blocked;
}

static bool hfpProfile_DelayToMonitorAptxVoicePacketsIsNotZero(void)
{
    /* If HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS is set to non
     * zero value then only this feature will be enabled. Be default this
     * feature is disabled now. */
    return (HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS > 0);
}

bool hfpProfile_AptxVoicePacketsCounterToBeMonitored(const bdaddr *bd_addr, uint16 qce_codec_mode_id)
{
    bool monitor_packets = FALSE;
    if(hfpProfile_DelayToMonitorAptxVoicePacketsIsNotZero() &&
       HfpProfile_HandsetSupportsSuperWideband(qce_codec_mode_id) &&
       !HfpProfile_IsHandsetBlockedForSwb(bd_addr))
    {
        monitor_packets = TRUE;
    }

    DEBUG_LOG("hfpProfile_AptxVoicePacketsCounterToBeMonitored:%d",monitor_packets);
    return monitor_packets;
}

void hfpProfile_HandleHfpAudioConnectComplete(hfpInstanceTaskData* instance, hfp_profile_audio_connect_status_t status, Sink audio_sink, uint16 codec, uint8 wesco, uint8 tesco, uint16 qce_codec_mode_id)
{
    voice_source_t source;
    PanicNull(instance);
    
    source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    
    hfpState state = appHfpGetState(instance);

    instance->bitfields.esco_connecting = FALSE;
    instance->bitfields.esco_disconnecting = FALSE;

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        /* Check if audio connection was successful. */
        if (status == hfp_profile_audio_connect_success)
        {
            appPowerPerformanceProfileRequestDuration(appConfigAudioConnectedCpuBoostDuration());
            TimestampEvent(TIMESTAMP_EVENT_HFP_AUDIO_CONNECTED);


            /* Inform client tasks SCO is active */
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_CONNECTED_IND);

            /* Store sink associated with SCO */
            instance->sco_sink = audio_sink;

            /* Check if SCO is now encrypted (or not) */
            HfpProfile_CheckEncryptedSco(instance);

            /* Update link policy now SCO is active */
            appLinkPolicyUpdatePowerTable(HfpProfile_GetHandsetBdAddr(instance));

            HfpProfile_StoreConnectParams(instance, codec, wesco, tesco, qce_codec_mode_id);

            if(hfpProfile_AptxVoicePacketsCounterToBeMonitored(HfpProfile_GetHandsetBdAddr(instance), qce_codec_mode_id))
            {
                DEBUG_LOG("hfpProfile_HandleHfpAudioConnectCfm:aptX voice packets to be read first time in %d ms.",HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS);
                MessageSendLater(HfpProfile_GetInstanceTask(instance),
                                 HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ,
                                 NULL, HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS);
            }

            Telephony_NotifyCallAudioConnected(source);

            /* Check if in HSP mode, use audio connection as indication of active call */
            if (instance->profile == hfp_headset_profile)
            {
                /* Move to active call state */
                appHfpSetState(instance, HFP_STATE_CONNECTED_ACTIVE);
            }

            /* Play SCO connected tone, only play if state is ConnectedIncoming,
               ConnectedOutgoing or ConnectedActive and not voice recognition */
            if (appHfpIsCallForInstance(instance) && !hfpProfile_IsVoiceRecognitionActive(instance))
            {
                Telephony_NotifyCallAudioRenderedLocal(source);
            }
        }
        else if (status == hfp_profile_audio_connect_in_progress)
        {
            /* This can happen if we have asked to transfer the audio to this device
               multiple times before the first HFP_AUDIO_CONNECT_CFM was received.

               Do nothing here because eventually we should get the CFM for the
               first request with another success or failure status. */
            instance->bitfields.esco_connecting = TRUE;
        }
    }
}

void hfpProfile_HandleAudioDisconnectIndication(hfpInstanceTaskData* instance, bool transferred)
{
    hfpState state = appHfpGetState(instance);

    if(instance && HfpProfile_StateIsInitialised(state))
    {
        instance->bitfields.esco_connecting = FALSE;
        instance->bitfields.esco_disconnecting = FALSE;
        
        /* The SCO has been transferred to the secondary earbud. Ignore this message.
           The SLC disconnection will clean up the hfp state */
        if(!transferred)
        {
            voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
            
            /* Inform client tasks SCO is inactive */
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), APP_HFP_SCO_DISCONNECTED_IND);

            Telephony_NotifyCallAudioRenderedRemote(source);

            Telephony_NotifyCallAudioDisconnected(source);

            /* Check if in HSP mode, if so then end the call */
            if (instance->profile == hfp_headset_profile && appHfpIsConnectedForInstance(instance))
            {
                /* Move to connected state */
                appHfpSetState(instance, HFP_STATE_CONNECTED_IDLE);
            }

            /* Clear SCO sink */
            instance->sco_sink = 0;

            /* Clear any SCO unencrypted reminders */
            HfpProfile_CheckEncryptedSco(instance);

            /* Update link policy now SCO is inactive */
            appLinkPolicyUpdatePowerTable(HfpProfile_GetHandsetBdAddr(instance));

            HfpProfileInstance_ResetAptxVoiceFrameCounts();

            /* Cancel aptX voice counter monitoring msg. */
            MessageCancelAll(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ);
        }
    }
    else
    {
        DEBUG_LOG("hfpProfile_HandleAudioDisconnectIndication for uninitialised instance");
    }
}

void hfpProfile_HandleRingIndication(hfpInstanceTaskData* instance, bool in_band)
{
    hfpState state;
    
    PanicNull(instance);

    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleRingIndication(%p) enum:voice_source_t:%d, enum:hfpState:%d in_band=%d", instance, source, state, in_band);

    switch(state)
    {
        case HFP_STATE_CONNECTED_IDLE:
        {
            /* Check if in HSP mode, use rings as indication of incoming call */
            if (instance->profile == hfp_headset_profile)
            {
                /* Move to incoming call establishment */
                appHfpSetState(instance, HFP_STATE_CONNECTED_INCOMING);

                /* Start HSP incoming call timeout */
                MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT, NULL, D_SEC(5));
            }

            /* Play ring tone if AG doesn't support in band ringing */
            if (in_band && !instance->bitfields.call_accepted)
            {
                Telephony_NotifyCallIncomingOutOfBandRingtone(source);
            }
        }
        break;

        case HFP_STATE_CONNECTED_INCOMING:
        {
            /* Check if in HSP mode, use rings as indication of incoming call */
            if (instance->profile == hfp_headset_profile)
            {
                /* Reset incoming call timeout */
                MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT);
                MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HSP_INCOMING_TIMEOUT, NULL, D_SEC(5));
            }
        }
        /* Fallthrough */

        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_CONNECTED_ACTIVE_WITH_INCOMING:
        {
            /* Play ring tone if AG doesn't support in band ringing or this source is not routed */
            if ((!in_band || instance->source_state != source_state_connected) && !instance->bitfields.call_accepted)
            {
                Telephony_NotifyCallIncomingOutOfBandRingtone(source);
            }
        }
        break;

        case HFP_STATE_DISCONNECTING:
        default:
        break;
    }
}

void hfpProfile_HandleServiceIndication(hfpInstanceTaskData* instance, uint8 service)
{
    DEBUG_LOG("hfpProfile_HandleHfpServiceInd(%p) service=%d", instance, service);
    
    PanicNull(instance);
    hfpState state = appHfpGetState(instance);

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        /* TODO: Handle service/no service */
    }
}

void hfpProfile_HandleHfpCallStateIndication(hfpInstanceTaskData* instance)
{
    PanicNull(instance);

    hfpState current_state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallStateIndication(%p) enum:hfpState:%d enum:hfp_call_state:%d",
              instance, current_state, instance->bitfields.call_state);

    /* Only update the overall state if not in process of connecting/disconnecting */
    if(HfpProfile_StateIsSlcConnected(current_state))
    {
        /* Move to new state, depending on call state */
        hfpState new_state = hfpProfile_GetStateFromCallState(instance->bitfields.call_state);
        if(current_state != new_state)
        {
            appHfpSetState(instance, new_state);
        }
    }
}

void hfpProfile_HandleHfpVoiceRecognitionIndication(hfpInstanceTaskData* instance, bool enabled)
{
    hfpState state = appHfpGetState(instance);
    
    PanicNull(instance);
    
    DEBUG_LOG("hfpProfile_HandleHfpVoiceRecognitionIndication(%p) enum:hfpState:%d enabled=%d", instance, state, enabled);

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        instance->bitfields.voice_recognition_active = enabled;
    }
}

void hfpProfile_HandleHfpVoiceRecognitionEnableComplete(hfpInstanceTaskData* instance, bool complete_success)
{
    hfpState state = appHfpGetState(instance);
    
    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        if (complete_success)
            instance->bitfields.voice_recognition_active = instance->bitfields.voice_recognition_request;
        else
            instance->bitfields.voice_recognition_request = instance->bitfields.voice_recognition_active;
    }
}

void hfpProfile_HandleHfpCallerIdIndication(hfpInstanceTaskData* instance, char* caller_number, phone_number_type_t type, char* caller_name)
{
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallerIdIndication enum:voice_source_t:%d", source);

    if (VoiceSource_IsValid(source))
    {
        Telephony_NotifyCallerId(source, caller_number, type, caller_name);
    }
}

void hfpProfile_HandleHfpVolumeSyncSpeakerGainIndication(hfpInstanceTaskData * instance, uint16 gain)
{
    PanicNull(instance);
    
    hfpState state = appHfpGetState(instance);
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    DEBUG_LOG("hfpProfile_HandleHfpVolumeSyncSpeakerGainIndication(%p) enum:hfpState:%d vol=%d", instance, state, gain);

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        Volume_SendVoiceSourceVolumeUpdateRequest(source, event_origin_external, gain);
    }
}

void hfpProfile_HandleHfpVolumeSyncMicGainIndication(hfpInstanceTaskData * instance, uint16 gain)
{
    PanicNull(instance);

    hfpState state = appHfpGetState(instance);
    voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
    
    DEBUG_LOG("hfpProfile_HandleHfpVolumeSyncMicGainIndication(%p) mic_gain=%d", instance, gain);

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        /* Set input gain */
        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        Device_SetPropertyU8(device, device_property_hfp_mic_gain, gain);

        /* Store new configuration */
        HfpProfile_StoreConfig(device);
        
        if (gain == 0)
        {
            instance->bitfields.mute_active = TRUE;
            Telephony_NotifyMicrophoneMuted(source);
        }
        else if (instance->bitfields.mute_active && gain != 0)
        {
            instance->bitfields.mute_active = FALSE;
            Telephony_NotifyMicrophoneUnmuted(source);
        }

        /* Re-configure audio chain */
        appKymeraScoMicMute(instance->bitfields.mute_active);
    }
}

void hfpProfile_HandleHfpCallAnswerComplete(hfpInstanceTaskData * instance, bool completed_successfully)
{
    voice_source_t source;
    hfpState state;

    PanicNull(instance);

    state = appHfpGetState(instance);
    source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallAnswerComplete(%p) enum:hfpState:%d success:%d",
              instance, state, completed_successfully);

    if(HfpProfile_StateHasIncomingCall(state))
    {
        if (completed_successfully)
        {
            /* Flag call as accepted, so we ignore any ring indications or caller ID */
            instance->bitfields.call_accepted = TRUE;
        }
    }
    
    if(!completed_successfully)
    {
        Telephony_NotifyError(source);
    }
}

void hfpProfile_HandleHfpCallTerminateComplete(hfpInstanceTaskData * instance, bool completed_successfully)
{
    voice_source_t source;
    hfpState state;

    PanicNull(instance);

    state = appHfpGetState(instance);
    source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallTerminateComplete(%p) enum:hfpState:%d success:%d", 
              instance, state, completed_successfully);
    
    if(!completed_successfully)
    {
        Telephony_NotifyError(source);
    }
}

void hfpProfile_HandleHfpCallHoldActionComplete(hfpInstanceTaskData * instance, bool completed_successfully)
{
    voice_source_t source;
    hfpState state;

    PanicNull(instance);

    state = appHfpGetState(instance);
    source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallHoldActionComplete(%p) enum:hfpState:%d success:%d", 
              instance, state, completed_successfully);
    
    if(!completed_successfully)
    {
        Telephony_NotifyError(source);
    }
}

void hfpProfile_HandleHfpUnrecognisedAtCmdIndication(hfpInstanceTaskData* instance, uint8* data, uint16 size_data)
{
    PanicNull(instance);

    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("hfpProfile_HandleHfpUnrecognisedAtCmdIndication(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcDisconnecting(state))
    {
        /* copy the message and send to register AT client */
        MAKE_HFP_MESSAGE_WITH_LEN(APP_HFP_AT_CMD_IND, size_data);
        message->addr = instance->ag_bd_addr;
        message->size_data = size_data;
        memcpy(message->data, data, size_data);
        MessageSend(hfp_profile_task_data.at_cmd_task, APP_HFP_AT_CMD_IND, message);
    }
    else
    {
        uint16 i;
        for(i = 0; i < size_data; ++i)
        {
            DEBUG_LOG("0x%x %c", data[i], data[i]);
        }
    }
}

void hfpProfile_HandleHfpAtCmdComplete(bool success)
{
    MAKE_HFP_MESSAGE(APP_HFP_AT_CMD_CFM);
    message->status = success;
    MessageSend(hfp_profile_task_data.at_cmd_task, APP_HFP_AT_CMD_CFM, message);
}

/*! \brief Handle indication of change in a connection status.

    Some phones will disconnect the ACL without closing any L2CAP/RFCOMM
    connections, so we check the ACL close reason code to determine if this
    has happened.

    If the close reason code was not link-loss and we have an HFP profile
    on that link, mark it as detach pending, so that we can gracefully handle
    the L2CAP or RFCOMM disconnection that will follow shortly.
 */
static void hfpProfile_HandleConManagerConnectionInd(CON_MANAGER_CONNECTION_IND_T *ind)
{
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);

    /* if disconnection and not an connection timeout, see if we need to mark
     * the HFP profile at having a pending detach */
    if (!ind->connected && ind->reason != hci_error_conn_timeout)
    {
        if (instance && !HfpProfile_IsDisconnected(instance) && BdaddrIsSame(&ind->bd_addr, &instance->ag_bd_addr) && !ind->ble)
        {
            DEBUG_LOG("hfpProfile_HandleConManagerConnectionInd, detach pending");
            instance->bitfields.detach_pending = TRUE;
        }
    }
}

static bool hfpProfile_DisconnectInternal(bdaddr *bd_addr)
{
    bool disconnect_request_sent = FALSE;
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    DEBUG_LOG("hfpProfile_DisconnectInternal(%p)", instance);

    if (instance && !HfpProfile_IsDisconnected(instance))
    {
        MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_DISCONNECT_REQ,
                                 NULL, HfpProfileInstance_GetLock(instance));
        disconnect_request_sent = TRUE;
    }
    return disconnect_request_sent;
}

static void hfpProfile_InitTaskData(void)
{
    /* set up common hfp profile task handler. */
    hfp_profile_task_data.task.handler = hfpProfile_TaskMessageHandler;

    /* create list for SLC notification clients */
    TaskList_InitialiseWithCapacity(appHfpGetSlcStatusNotifyList(), HFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create list for general status notification clients */
    TaskList_InitialiseWithCapacity(appHfpGetStatusNotifyList(), HFP_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create lists for connection/disconnection requests */
    TaskList_WithDataInitialise(&hfp_profile_task_data.connect_request_clients);
    TaskList_WithDataInitialise(&hfp_profile_task_data.disconnect_request_clients);

    PanicFalse(BandwidthManager_RegisterFeature(BANDWIDTH_MGR_FEATURE_ESCO, high_bandwidth_manager_priority, NULL));
}


static voice_source_t hfpProfile_GetForegroundVoiceSource(void)
{
    voice_source_t foreground_voice_source = voice_source_none;
    generic_source_t routed_source = Focus_GetFocusedGenericSourceForAudioRouting();

    if (GenericSource_IsVoice(routed_source) && VoiceSource_IsHfp(routed_source.u.voice))
    {
        foreground_voice_source = routed_source.u.voice;
    }
    return foreground_voice_source;
}

bool HfpProfile_Init(Task init_task)
{
    UNUSED(init_task);

    hfpProfile_InitTaskData();

    hfpProfile_InitHfpLibrary();

    VoiceSources_RegisterVolume(voice_source_hfp_1, HfpProfile_GetVoiceSourceVolumeInterface());
    VoiceSources_RegisterVolume(voice_source_hfp_2, HfpProfile_GetVoiceSourceVolumeInterface());

    /* Register to receive notifications of (dis)connections */
    ConManagerRegisterConnectionsClient(&hfp_profile_task_data.task);

    ProfileManager_RegisterProfile(profile_manager_hfp_profile, hfpProfile_Connect, hfpProfile_Disconnect);

    return TRUE;
}

bool HfpProfile_ConnectHandset(void)
{
    bdaddr bd_addr;

    /* Get handset device address */
    if (appDeviceGetHandsetBdAddr(&bd_addr) && BtDevice_IsProfileSupported(&bd_addr, DEVICE_PROFILE_HFP))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(&bd_addr);
        if (device)
        {
            uint8 our_hfp_profile = 0;
            Device_GetPropertyU8(device, device_property_hfp_profile, &our_hfp_profile);
            return HfpProfile_ConnectWithBdAddr(&bd_addr, our_hfp_profile);
        }
    }

    return FALSE;
}

void hfpProfile_Connect(bdaddr *bd_addr)
{
    PanicNull((bdaddr *)bd_addr);
    if (BtDevice_IsProfileSupported(bd_addr, DEVICE_PROFILE_HFP))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
        if (device)
        {
            uint8 our_hfp_profile = 0;
            Device_GetPropertyU8(device, device_property_hfp_profile, &our_hfp_profile);

            ProfileManager_AddToNotifyList(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients), device);
            if (!HfpProfile_ConnectWithBdAddr(bd_addr, our_hfp_profile))
            {
                /* If already connected, send an immediate confirmation */
                ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&hfp_profile_task_data.connect_request_clients),
                                                  bd_addr, profile_manager_success,
                                                  profile_manager_hfp_profile, profile_manager_connect);
            }
        }
    }
}

void hfpProfile_Disconnect(bdaddr *bd_addr)
{
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddToNotifyList(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients), device);
        if (!hfpProfile_DisconnectInternal(bd_addr))
        {
            /* If already disconnected, send an immediate confirmation */
            ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&hfp_profile_task_data.disconnect_request_clients),
                                              bd_addr, profile_manager_success,
                                              profile_manager_hfp_profile, profile_manager_disconnect);
        }
    }
}

bool HfpProfile_ConnectWithBdAddr(const bdaddr *bd_addr, hfp_connection_type_t connection_type)
{
    DEBUG_LOG("HfpProfile_ConnectWithBdAddr");

    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);
    if (!instance)
    {
        instance = HfpProfileInstance_Create(bd_addr, TRUE);
    }

    /* Check if not already connected */
    if (!appHfpIsConnectedForInstance(instance))
    {
        /* Store address of AG */
        instance->ag_bd_addr = *bd_addr;

        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CONNECT_REQ);

        /* Send message to HFP task */
        message->profile = connection_type;
        MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 ConManagerCreateAcl(bd_addr));

        /* Connect will now be handled by HFP task */
        return TRUE;
    }

    /* Already connected */
    return FALSE;
}

void HfpProfile_StoreConfig(device_t device)
{
    DeviceDbSerialiser_SerialiseDeviceLater(device, hfpConfigSerialiseDeviceLaterDelay());
}

void appHfpClientRegister(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(appHfpGetSlcStatusNotifyList()), task);
}

void HfpProfile_RegisterStatusClient(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(appHfpGetStatusNotifyList()), task);
}

uint8 appHfpGetVolume(hfpInstanceTaskData * instance)
{
    volume_t volume = HfpProfile_GetDefaultVolume();
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    PanicNull(device);
    DeviceProperties_GetVoiceVolume(device, volume.config, &volume);
    return volume.value;
}

static void hfpProfile_TaskMessageHandler(Task task, MessageId id, Message message)
{
    DEBUG_LOG("hfpProfile_TaskMessageHandler MESSAGE:HfpMessageId:0x%04X", id);

    /* Handle any platform-specific implementation HFP messaging. */
    if (HfpProfilePort_HandleMessage(task, id, message))
    {
        return;
    }

    /* Handle other messages */
    switch (id)
    {
        case CON_MANAGER_CONNECTION_IND:
            hfpProfile_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T *)message);
            return;
    }

    /* Finally, check for any messages bearing on Battery monitoring. */
    HfpProfile_HandleBatteryMessages(id, message);
}

void hfpProfile_RegisterHfpMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == APP_HFP_MESSAGE_GROUP);
    HfpProfile_RegisterStatusClient(task);
}

void hfpProfile_RegisterSystemMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == SYSTEM_MESSAGE_GROUP);
    HfpProfile_RegisterStatusClient(task);
}

/* \brief Inform hfp profile of current device Primary/Secondary role.
 */
void HfpProfile_SetRole(bool primary)
{
    if (primary)
    {
        /* Register voice source interface for hfp profile */
        VoiceSources_RegisterAudioInterface(voice_source_hfp_1, HfpProfile_GetAudioInterface());
        VoiceSources_RegisterAudioInterface(voice_source_hfp_2, HfpProfile_GetAudioInterface());
    }

}

void HfpProfile_HandleError(hfpInstanceTaskData * instance, MessageId id, Message message)
{
    UNUSED(message);

    DEBUG_LOG_ERROR("HfpProfile_HandleError enum:hfpState:%d, MESSAGE:hfp_profile_internal_messages:%x",
                    instance->state, id);

    /* Check if we are connected */
    if (appHfpIsConnectedForInstance(instance))
    {
        /* Move to 'disconnecting' state */
        appHfpSetState(instance, HFP_STATE_DISCONNECTING);
    }
}

/*! \brief returns hfp task pointer to requesting component

    \return hfp task pointer.
*/
Task HfpProfile_GetInstanceTask(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &instance->task;
}

/*! \brief Get HFP sink */
Sink HfpProfile_GetSink(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return instance->slc_sink;
}

/*! \brief Get current AG address */
bdaddr * HfpProfile_GetHandsetBdAddr(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &(instance->ag_bd_addr);
}

/*! \brief Is HFP SCO active with the specified HFP instance. */
bool HfpProfile_IsScoActiveForInstance(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->sco_sink != 0);
}

/*! \brief Is HFP SCO connecting with the specified HFP instance. */
bool HfpProfile_IsScoConnectingForInstance(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->bitfields.esco_connecting != 0);
}

/*! \brief Is HFP SCO disconnecting with the specified HFP instance. */
bool HfpProfile_IsScoDisconnectingForInstance(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return (instance->bitfields.esco_disconnecting != 0);
}

/*! \brief Is HFP SCO active */
bool HfpProfile_IsScoActive(void)
{
    bool is_sco_active = FALSE;
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        is_sco_active = HfpProfile_IsScoActiveForInstance(instance);
        if (is_sco_active)
            break;
    }
    return is_sco_active;
}

/*! \brief Is microphone muted */
bool HfpProfile_IsMicrophoneMuted(hfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return instance->bitfields.mute_active;
}

hfpInstanceTaskData * HfpProfile_GetInstanceForVoiceSourceWithUiFocus(void)
{
    hfpInstanceTaskData* instance = NULL;
    voice_source_t source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &source))
    {
        instance = HfpProfileInstance_GetInstanceForSource(source);
    }
    return instance;
}

hfpInstanceTaskData * HfpProfile_GetInstanceForVoiceSourceWithAudioFocus(void)
{
    voice_source_t source = hfpProfile_GetForegroundVoiceSource();
    return HfpProfileInstance_GetInstanceForSource(source);
}

uint8 HfpProfile_GetDefaultMicGain(void)
{
    return HFP_MICROPHONE_GAIN;
}

void HfpProfile_SetScoConnectingSyncTask(Task task)
{
    PanicNotNull(hfp_profile_task_data.sco_sync_task);
    hfp_profile_task_data.sco_sync_task = task;
}

void HfpProfile_ScoConnectingSyncResponse(device_t device, Task task, bool accept, sync_link_type link_type)
{
    hfpInstanceTaskData *instance = HfpProfileInstance_GetInstanceForDevice(device);

    if (instance)
    {
        DEBUG_LOG("HfpProfile_ScoConnectingSyncResponse enum:hfpState:%d, connecting:%x link type:%x pkt supp:%x",
                   instance->state, instance->bitfields.esco_connecting, link_type, instance->sco_supported_packets);

        if (instance->bitfields.esco_connecting)
        {
            hfpProfile_SendAudioConnectResponse(instance, (link_type == sync_link_esco), accept);
        }
    }

    /* Only one task currently supported so ignore */
    UNUSED(task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(APP_HFP, hfpProfile_RegisterHfpMessageGroup, NULL);
MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(SYSTEM, hfpProfile_RegisterSystemMessageGroup, NULL);

#endif
