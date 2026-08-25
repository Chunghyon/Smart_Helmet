/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AGHFP component.
*/

#include <panic.h>
#include <logging.h>
#include <bandwidth_manager.h>
#include <device_properties.h>
#include <profile_manager.h>
#include <system_state.h>
#include <telephony_messages.h>
#include <device_list.h>
#include <ui.h>
#include <feature.h>
#include <stdio.h>
#include <stream.h>
#include <string.h>
#include <volume_messages.h>

#include "connection_manager.h"
#include "aghfp_profile.h"
#include "aghfp_profile_instance.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile_audio.h"
#include "aghfp_profile_config.h"
#include "aghfp_profile_volume.h"
#include "aghfp_profile_abstraction.h"
#include "aghfp_profile_port_protected.h"

/***************************** data defines ****************************************/


/*! \brief Application HFP component main data structure. */
agHfpTaskData aghfp_profile_task_data;

dialed_number last_dialed_number;

const aghfpState aghfp_call_status_table[2] =
{
    /* aghfp_call_none */   AGHFP_STATE_CONNECTED_IDLE,
    /* aghfp_call_active */ AGHFP_STATE_CONNECTED_ACTIVE,
};

const aghfpState aghfp_call_setup_table[4] =
{
    /* aghfp_call_setup_none */         AGHFP_STATE_CONNECTED_IDLE,
    /* aghfp_call_setup_incoming */     AGHFP_STATE_CONNECTED_INCOMING,
    /* aghfp_call_setup_outgoing */     AGHFP_STATE_CONNECTED_OUTGOING,
    /* aghfp_call_setup_remote_alert */ AGHFP_STATE_CONNECTED_IDLE,
};

#define AG_DEBUG

#define MAX_CALL_HISTORY 1

static void aghfpProfile_TaskMessageHandler(Task task, MessageId id, Message message);

static void aghfpProfile_InitTaskData(void)
{
    /* set up common hfp profile task handler. */
    aghfp_profile_task_data.task.handler = aghfpProfile_TaskMessageHandler;

    /* create list for SLC notification clients */
    TaskList_InitialiseWithCapacity(aghfpProfile_GetSlcStatusNotifyList(), AGHFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create list for general status notification clients */
    TaskList_InitialiseWithCapacity(aghfpProfile_GetStatusNotifyList(), AGHFP_STATUS_NOTIFY_LIST_INIT_CAPACITY);

    /* create lists for connection/disconnection requests */
    TaskList_WithDataInitialise(&aghfp_profile_task_data.connect_request_clients);
    TaskList_WithDataInitialise(&aghfp_profile_task_data.disconnect_request_clients);
}

/*! \brief Entering `Initialising HFP` state
*/
static void aghfpProfile_InitAghfpLibrary(void)
{
    uint16 supported_features = 0;
    uint16 supported_qce_codecs = CODEC_MODE_ID_UNSUPPORTED;

    supported_features = AghfpProfilePort_GetSupportedFeatures();

#ifdef INCLUDE_SWB
    if (FeatureVerifyLicense(APTX_ADAPTIVE_MONO_DECODE))
    {
        DEBUG_LOG("License found for AptX adaptive mono decoder");
        supported_qce_codecs = AghfpProfilePort_GetSupportedQceCodec();
    }
    else
    {
        DEBUG_LOG("No license found for AptX adaptive mono decoder");
    }
#endif /* INCLUDE_SWB */

    last_dialed_number.number = NULL;
    last_dialed_number.number_len = 0;

    AghfpProfilePort_InitLibrary(supported_qce_codecs, supported_features);
}

/*! \brief Handle HF answering an incoming call
*/
void aghfpProfile_HandleCallAnswerInd(uint16 id, AGHFP_ANSWER_IND_T *ind)
{
    aghfpInstanceTaskData *instance = NULL;

    instance = AghfpProfilePort_GetInstance(id, ind);
    PanicNull(instance);

    DEBUG_LOG("aghfpProfile_HandleCallAnswerInd(%p)", instance);

    AghfpProfileAbstract_SendOk(instance, TRUE);

    aghfpState state = AghfpProfile_GetState(instance);

    if (state == AGHFP_STATE_CONNECTED_INCOMING)
    {
        instance->bitfields.call_status = aghfp_call_active;
        AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_ACTIVE);
        AghfpProfileCallList_AnswerIncomingCall(instance->call_list);
    }
    else if (state == AGHFP_STATE_CONNECTED_ACTIVE && instance->bitfields.call_setup == aghfp_call_setup_incoming)
    {
        instance->bitfields.call_hold = aghfp_call_held_active;
        instance->bitfields.call_setup = aghfp_call_setup_none;
        AghfpProfileCallList_HoldActiveCall(instance->call_list);
        AghfpProfileCallList_AnswerIncomingCall(instance->call_list);
    }
}

static void aghfpProfile_UpdateCallListAfterHangUp(aghfpState state, call_list_t *call_list)
{
    switch (state)
    {
    case AGHFP_STATE_CONNECTED_ACTIVE:
        AghfpProfileCallList_TerminateActiveCall(call_list);
        break;
    case AGHFP_STATE_CONNECTED_INCOMING:
        AghfpProfileCallList_RejectIncomingCall(call_list);
        break;
    case AGHFP_STATE_CONNECTED_OUTGOING:
        AghfpProfileCallList_OutgoingCallRejected(call_list);
        break;
    default:
       DEBUG_LOG("aghfpProfile_UpdateCallListAfterHangUp: Invalid state");
    }
}

/*! \brief Handle incoming calls in different states
*/
static void aghfpProfile_HandleIncomingInd(aghfpInstanceTaskData *instance)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleIncomingInd");

    aghfpState state = AghfpProfile_GetState(instance);

    if (instance->bitfields.call_setup != aghfp_call_setup_incoming)
    {
        AghfpProfileCallList_AddIncomingCall(instance->call_list);
    }

    if (AGHFP_STATE_CONNECTED_IDLE == state)
    {
        AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_INCOMING);
    }
    else if (AGHFP_STATE_DISCONNECTED == state)
    {
        /* If we're not connected then update the call_setup
           so it can be transferred on SLC set-up
        */
        instance->bitfields.call_setup = aghfp_call_setup_incoming;
    }
    else if (AGHFP_STATE_CONNECTED_ACTIVE == state)
    {
        instance->bitfields.call_setup = aghfp_call_setup_incoming;
        AghfpProfileAbstract_SendCallSetupIndication(instance);
        AghfpProfileAbstract_SendCallWaiting(instance, instance->bitfields.caller_id_active_host, instance->bitfields.caller_id_active_remote);
    }
}

/*! \brief Handle HF rejecting an incoming call or ending an ongoing call
*/
void aghfpProfile_HandleCallHangUpInd(uint16 id, AGHFP_CALL_HANG_UP_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallHangUpInd");

    aghfpInstanceTaskData *instance = NULL;

    instance = AghfpProfilePort_GetInstance(id, ind);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleCallHangUpInd);
        return;
    }

    AghfpProfileAbstract_SendOk(instance, TRUE);

    aghfpState state = AghfpProfile_GetState(instance);

    if (state == AGHFP_STATE_CONNECTED_ACTIVE ||
        state == AGHFP_STATE_CONNECTED_INCOMING ||
        state == AGHFP_STATE_CONNECTED_OUTGOING)
    {
        Ui_InformContextChange(ui_provider_telephony, context_voice_connected);
        aghfpProfile_UpdateCallListAfterHangUp(state, instance->call_list);
        if (instance->bitfields.call_hold == aghfp_call_held_none)
        {
            instance->bitfields.call_status = aghfp_call_none;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_IDLE);
        }
        else if (instance->bitfields.call_hold == aghfp_call_held_active)
        {
            instance->bitfields.call_hold = aghfp_call_held_remaining;
            AghfpProfileAbstract_SendCallHeldIndication(instance);
        }
    }
}

/*! \brief Handle disconnect of the SLC
*/
void aghfpProfile_HandleSlcDisconnectInd(uint16 id, AGHFP_SLC_DISCONNECT_IND_T *ind)
{
    aghfpInstanceTaskData *instance = NULL;
    uint16 status_code ;

    instance = AghfpProfilePort_GetInstance(id, ind);

    status_code = aghfpGetSlcDisconnectStatusCode(ind);
    DEBUG_LOG("aghfpProfile_HandleSlcDisconnectInd(%p) enum:aghfp_disconnect_status:%d",
              instance, status_code);
    if (instance)
    {
        if (AghfpProfile_GetState(instance) == AGHFP_STATE_DISCONNECTING)
        {
            /* Expected, this was a locally initiated disconnection */
            instance->disconnect_reason = APP_AGHFP_DISCONNECT_NORMAL;
        }
        else
        {
            /* Disconnect while connected, possibly remote initiated */
            switch (status_code)
            {
                case aghfp_disconnect_success:
                    instance->disconnect_reason = APP_AGHFP_DISCONNECT_NORMAL;
                    break;
                case aghfp_disconnect_link_loss:
                    instance->disconnect_reason = APP_AGHFP_DISCONNECT_LINKLOSS;
                    break;
                case aghfp_disconnect_no_slc:
                case aghfp_disconnect_timeout:
                case aghfp_disconnect_error:
                default:
                    instance->disconnect_reason = APP_AGHFP_DISCONNECT_ERROR;
                    break;
            }
        }

        AghfpProfile_SendSlcStatus(FALSE, &instance->hf_bd_addr);

        instance->slc_sink = NULL;
        AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);
        /* For link loss we wait for the HF to attempt a reconnection */
        if (instance->bitfields.call_hold == aghfp_call_held_none)
        {
            AghfpProfileInstance_Destroy(instance);
        }
    }
}

/*! \brief Handle audio connect confirmation
*/
void aghfpProfile_HandleAgHfpAudioConnectCfm(uint16 id, AGHFP_AUDIO_CONNECT_CFM_T *cfm)
{
    aghfpInstanceTaskData *instance = NULL;
    uint16_t status_code ;

    instance = AghfpProfilePort_GetInstance(id, cfm);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleAgHfpAudioConnectCfm);
        return;
    }

    voice_source_t voice_source = AghfpProfileInstance_GetVoiceSourceForInstance(instance);

    status_code = aghfpGetAudioConnectStatusCode(cfm);

    /* We only accept remote-initiated SCOs during a call, and only initiate
       them ourselves during call setup. In both cases, we set sco_status to
       "connecting". If we have arrived here *without* passing through that
       state, then the SCO was requested by some other entity/component in
       the system, and we shouldn't interfere with it. */
    bool sco_was_initiated_during_call = (instance->bitfields.sco_status == aghfp_sco_connecting);

    DEBUG_LOG("agHfpProfile_HandleAgHfpAudioConnectCfm(%p) enum:aghfp_audio_connect_status:%d,"
              " initiated_for_call:%d", instance, status_code, sco_was_initiated_during_call);

    switch (status_code)
    {
        case aghfp_audio_connect_success:
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_SCO_CONNECTED_IND);
            instance->bitfields.sco_status = aghfp_sco_connected;

            AghfpProfilePort_HandleAgHfpAudioConnectCfm(instance, cfm);

            Telephony_NotifyCallAudioConnected(voice_source);

            switch (instance->state)
            {
                case AGHFP_STATE_CONNECTED_IDLE:
                    /* No longer in a call. */
                    if (sco_was_initiated_during_call)
                    {
                        /* Call cancelled/terminated/rejected during SCO connect.
                           No longer need the SCO we requested, so disconnect it. */
                        AghfpProfileAudio_DisconnectSco(instance);
                    }
                    break;
                case AGHFP_STATE_CONNECTED_INCOMING:
                    if (instance->bitfields.in_band_ring &&
                        instance->bitfields.call_setup == aghfp_call_setup_incoming)
                    {
                        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RING_REQ);
                        message->addr = instance->hf_bd_addr;
                        MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RING_REQ, message);
                    }
                    break;
                default:
                    break;
            }
            break;

        case aghfp_audio_connect_in_progress:
            /* Duplicate request, ignore. Another AGHFP_AUDIO_CONNECT_CFM
               will come later, with the actual result of the first request. */
            break;

        default:
            DEBUG_LOG("agHfpProfile_HandleAgHfpAudioConnectCfm: Connection failure. Status %d", status_code);
            instance->bitfields.sco_status = (instance->sco_sink) ? aghfp_sco_connected : aghfp_sco_disconnected;
            break;
    }
}


void aghfpProfile_HandleHfpAudioDisconnectInd(uint16 id, AGHFP_AUDIO_DISCONNECT_IND_T *ind)
{
    aghfpInstanceTaskData *instance = NULL;
    uint16 status_code;

    DEBUG_LOG("aghfpProfile_HandleHfpAudioDisconnectInd");
        
    instance = AghfpProfilePort_GetInstance(id, ind);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleHfpAudioDisconnectInd);
        return;
    }
    voice_source_t voice_source = AghfpProfileInstance_GetVoiceSourceForInstance(instance);

    /* If this disconnection was initiated locally, we would have set sco_status
       to "disconnecting". If not, then it must have been initiated remotely by
       the HF, or by some other entity/component in the system. In which case,
       we shouldn't interfere, and so shouldn't attempt any reconnections etc. */
    bool locally_initiated = (instance->bitfields.sco_status == aghfp_sco_disconnecting);

    status_code = aghfpGetAudioDisconnectStatusCode(ind);

    DEBUG_LOG("aghfpProfile_HandleHfpAudioDisconnectInd(%p) enum:aghfp_audio_disconnect_status:%d,"
              " locally_initiated:%d", instance, status_code, locally_initiated);

    switch (status_code)
    {
        case aghfp_audio_disconnect_success:
            TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(appAgHfpGetStatusNotifyList()), APP_AGHFP_SCO_DISCONNECTED_IND);
            instance->sco_sink = NULL;
            instance->bitfields.sco_status = aghfp_sco_disconnected;
            Telephony_NotifyCallAudioDisconnected(voice_source);

            if (locally_initiated)
            {
                switch (instance->state)
                {
                    case AGHFP_STATE_CONNECTED_ACTIVE:
                    case AGHFP_STATE_CONNECTED_OUTGOING:
                        /* We requested a SCO disconnect because there was no
                           longer any call audio to route. However, in the
                           meantime another call has started, so reconnect the
                           SCO to route the new audio. */
                        AghfpProfileAudio_ConnectSco(instance);
                        break;
                    case AGHFP_STATE_CONNECTED_INCOMING:
                        /* Incoming call started during SCO disconnect. Only restart
                           SCO for in-band ringing, otherwise wait for call accept. */
                        if (instance->bitfields.in_band_ring)
                        {
                            AghfpProfileAudio_ConnectSco(instance);
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        case aghfp_audio_disconnect_in_progress:
            /* Duplicate request, ignore. Another AGHFP_AUDIO_DISCONNECT_IND
               will come later, with the actual result of the first request. */
            break;
        default:
            instance->bitfields.sco_status =
                        (instance->sco_sink) ? aghfp_sco_connected : aghfp_sco_disconnected;
            break;
    }
}

/*! \brief Handle unknown NREC command from HF.
           Send ERROR unconditionally since we don't support NR/EC at the moment.
*/
void aghfpProfile_HandleNrecSetupInd(uint16 id, AGHFP_NREC_SETUP_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleNrecSetupInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);
    AghfpProfileAbstract_SendError(instance);
}

/*! \brief Handle dial command from HF.
*/
void aghfpProfile_HandleDialInd(uint16 id, AGHFP_DIAL_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleDialInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleDialInd);
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    if (state != AGHFP_STATE_CONNECTED_IDLE)
    {
        DEBUG_LOG("aghfpProfile_HandleDialInd: HF attempting to dial while not idle. Current state: enum:aghfpState:%d", state);
        return;
    }

    AghfpProfilePort_SetLastDialledNumber(ind);

    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ);
    message->instance = instance;
    MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);
}

/*! \brief Handle HF requesting network operator ind
*/
void aghfpProfile_HandleNetworkOperatorInd(uint16 id, AGHFP_NETWORK_OPERATOR_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleNetworkOperatorInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleNetworkOperatorInd);
        return;
    }

    if (instance->network_operator != NULL)
    {
        AghfpProfileAbstract_SendNetworkOperator(instance);
    }
    else
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleNetworkOperatorInd: No network operator available");
    }
}

/*! \brief Handle HF requesting subscriber number.
*/
void aghfpProfile_HandleSubscriberNumberInd(uint16 id, AGHFP_SUBSCRIBER_NUMBER_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleSubscriberNumberInd");
    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleSubscriberNumberInd);
        return;
    }

    AghfpProfileAbstract_SendSubscriberNumber(instance);
}

/*! \brief Handle AT+CIND message.
*/
void aghfpProfile_HandleCallIndicationsStatusReqInd(uint16 id, AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallIndicationsStatusReqInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleCallIndicationsStatusReqInd);
        return;
    }

    DEBUG_LOG("Call status: enum:aghfp_call_status:%d", instance->bitfields.call_status);
    DEBUG_LOG("Call status: enum:aghfp_call_setup:%d", instance->bitfields.call_setup);

    AghfpProfileAbstract_IndicatorsStatusResponse(instance,
                                      aghfp_service_present,           /* aghfp_service_availability */
                                      5,                               /* Signal level */
                                      aghfp_roam_none,                 /* aghfp_roam_status */
                                      5);                              /* Battery level */
}

/*! \brief Return a list of all current calls
*/
void aghfpProfile_HandleGetCurrentCallsInd(uint16 id, AGHFP_CURRENT_CALLS_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleGetCurrentCallsInd");
    uint16 index = 0;
    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleGetCurrentCallsInd);
        AghfpProfileAbstract_SendError(instance);
        return;
    }

    index = AghfpProfilePort_GetCurCallIndex(ind);

    AghfpProfileAbstract_SendCallList(instance, index);
}

/*! \brief Handle a request to perform a memory dial from the HF.
*/
void agfhpProfile_HandleMemoryDialInd(uint16 id, AGHFP_MEMORY_DIAL_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("agfhpProfile_HandleMemoryDialInd");

    aghfpInstanceTaskData *instance;
    uint16 number = 1;

    instance = AghfpProfilePort_GetInstance(id, ind);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(agfhpProfile_HandleMemoryDialInd);
        AghfpProfileAbstract_SendDialRes(instance, FALSE, FALSE);
        return;
    }
    
    number = AghfpProfilePort_GetMemDialNumber(ind);
    
    DEBUG_LOG("Call index %d Last dialled number present %d", number, last_dialed_number.number);

    if (number > MAX_CALL_HISTORY || last_dialed_number.number == NULL)
    {
        DEBUG_LOG("agfhpProfile_HandleMemoryDialInd Can not perform memory dial. Call index %d Last dialled number present %d", number,
                                                                                           last_dialed_number.number);
        AghfpProfileAbstract_SendDialRes(instance, FALSE, FALSE);
        return;
    }

    AghfpProfileAbstract_SendDialRes(instance, TRUE, FALSE);

    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ);
    message->instance = instance;
    MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);
}

/*! \brief Handle a request to perform a memory dial from the HF.
*/
void agfhpProfile_HandleRedialLastCall(uint16 id, AGHFP_LAST_NUMBER_REDIAL_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("agfhpProfile_HandleRedialLastCall");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);

    if (!instance)
    {
        DEBUG_LOG_ERROR("aghfpProfile_HandleDialInd: No aghfpInstanceTaskData instance for aghfp");
        AghfpProfileAbstract_SendDialRes(instance, FALSE, FALSE);
        return;
    }

    if (last_dialed_number.number == NULL)
    {
        DEBUG_LOG("agfhpProfile_HandleRedialLastCall Can not perform memory dial. Last dialled number present %d", (last_dialed_number.number != NULL));
        AghfpProfileAbstract_SendDialRes(instance, FALSE, FALSE);
        return;
    }

    AghfpProfileAbstract_SendDialRes(instance, TRUE, FALSE);

    MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ);
    message->instance = instance;
    MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_VOICE_DIAL_REQ, message);

}


/*! \brief Handle indication to set microphone gain for HF
*/
void aghfpProfile_HandleSyncMicGain(uint16 id, AGHFP_SYNC_MICROPHONE_GAIN_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleSyncMicGain gain: %d", ind->gain);

    aghfpInstanceTaskData *instance = NULL;
    
    instance = AghfpProfilePort_GetInstance(id, ind);
    
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleSyncMicGain); 
        AghfpProfileAbstract_SendError(instance);
        return;
    }

    /* A gain of 0 indicicates that the microphone has been muted.
       Indicate to the Telephony that the microphone has been muted / unmuted
       if the mute has changed state */
    if (!instance->bitfields.mic_mute && ind->gain == 0)
    {
        instance->bitfields.mic_mute = TRUE;
        Telephony_NotifyMicrophoneMuted(voice_source_hfp_1);
    }
    else if (instance->bitfields.mic_mute && ind->gain != 0)
    {
        instance->bitfields.mic_mute = FALSE;
        Telephony_NotifyMicrophoneUnmuted(voice_source_hfp_1);
    }

    /* Only set the mic gain if it not 0 */
    if (ind->gain != 0)
    {
        instance->mic_gain = ind->gain;
    }
}

/*! \brief Handle indication to set speaker volume for HF
*/
void aghfpProfile_HandleSpeakerVolumeInd(uint16 id, AGHFP_SYNC_SPEAKER_VOLUME_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleSpeakerVolumeInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);
    
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleSyncMicGain);
        AghfpProfileAbstract_SendError(instance);
        return;
    }

    instance->speaker_volume = AghfpProfilePort_GetSpeakerVolume(ind);

    Volume_SendVoiceSourceVolumeUpdateRequest(voice_source_hfp_1, event_origin_external, instance->speaker_volume);
}


/*! \brief Handles messages into the AGHFP profile
*/
static void aghfpProfile_TaskMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    AghfpProfilePort_HandleHfgMessages(task, id, message);
}

/*! \brief Called by Profile Manager to request profile connection */
static void aghfpProfile_ConnectRequestCb(bdaddr *bd_addr)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (instance)
    {
        /* If already connected, send an immediate confirmation */
        if (AghfpProfile_IsConnectedForInstance(instance))
        {
            ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&aghfp_profile_task_data.connect_request_clients),
                                              bd_addr, profile_manager_success,
                                              profile_manager_hfp_profile, profile_manager_connect);
            return;
        }
    }

    if (BtDevice_IsProfileSupported(bd_addr, DEVICE_PROFILE_HFP))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
        if (device)
        {
            /* Add Profile Manager to client list so we can notify connect success/failure later */
            ProfileManager_AddToNotifyList(TaskList_GetBaseTaskList(&aghfp_profile_task_data.connect_request_clients), device);
            AghfpProfile_Connect(bd_addr);
        }
    }
}

/*! \brief Called by Profile Manager to request profile disconnection */
static void aghfpProfile_DisconnectRequestCb(bdaddr *bd_addr)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (instance)
    {
        /* If already disconnected, send an immediate confirmation */
        if (AghfpProfile_IsDisconnected(instance))
        {
            ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&aghfp_profile_task_data.disconnect_request_clients),
                                              bd_addr, profile_manager_success,
                                              profile_manager_hfp_profile, profile_manager_disconnect);
            return;
        }
    }

    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        /* Add Profile Manager to client list so we can notify disconnect confirmation later */
        ProfileManager_AddToNotifyList(TaskList_GetBaseTaskList(&aghfp_profile_task_data.disconnect_request_clients), device);
        AghfpProfile_Disconnect(bd_addr);
    }
}

/*! \brief Retrieve the device object using its source
*/
static device_t aghfpProfileInstance_FindDeviceFromVoiceSource(voice_source_t source)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(voice_source_t));
}

/***************************************************************************************/

/* The below APIs will be called directly from the application */

/*********************** Public APIs ***************************************************/
/*! \brief Connect HFP to the HF
*/
void AghfpProfile_Connect(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Connect");

    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        instance = PanicNull(AghfpProfileInstance_Create(bd_addr, TRUE));
    }

    /* Check if not already connected */
    if (!AghfpProfile_IsConnectedForInstance(instance))
    {
        /* Store address of AG */
        instance->hf_bd_addr = *bd_addr;

        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_CONNECT_REQ);

        /* Send message to HFP task */
        message->addr = *bd_addr;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 ConManagerCreateAcl(bd_addr));
    }
}

/*! \brief Disconnect from the HF
*/
void AghfpProfile_Disconnect(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Disconnect");

    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(AghfpProfile_Disconnect);
        return;
    }

    if (!AghfpProfile_IsDisconnected(instance))
    {
        MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_DISCONNECT_REQ);

        message->instance = instance;
        MessageSendConditionally(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_DISCONNECT_REQ,
                                 message, AghfpProfileInstance_GetLock(instance));
    }
}

/*! \brief Is HFP connected */
bool AghfpProfile_IsConnectedForInstance(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) >= AGHFP_STATE_CONNECTED_IDLE) && (AghfpProfile_GetState(instance) <= AGHFP_STATE_CONNECTED_ACTIVE));
}

/*! \brief Is HFP active*/
bool AghfpProfile_IsCallActiveForInstance(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) >= AGHFP_STATE_CONNECTED_OUTGOING) && (AghfpProfile_GetState(instance) <= AGHFP_STATE_CONNECTED_ACTIVE));
}
/*! \brief Is HFP connected */
Task AghfpProfile_GetInstanceTask(aghfpInstanceTaskData * instance)
{
    PanicNull(instance);
    return &instance->task;
}

/*! \brief Is HFP disconnected */
bool AghfpProfile_IsDisconnected(aghfpInstanceTaskData * instance)
{
    return ((AghfpProfile_GetState(instance) < AGHFP_STATE_CONNECTING_LOCAL) || (AghfpProfile_GetState(instance) > AGHFP_STATE_DISCONNECTING));
}

void AghfpProfile_HandleError(aghfpInstanceTaskData * instance, MessageId id, Message message)
{
    UNUSED(message);
    UNUSED(id);

    /* Check if we are connected */
    if (AghfpProfile_IsConnectedForInstance(instance))
    {
        /* Move to 'disconnecting' state */
        AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTING);
    }
}

/*! \brief Is HFP SCO active with the specified HFP instance. */
bool AghfpProfile_IsScoActiveForInstance(aghfpInstanceTaskData * instance)
{
    return (instance && instance->sco_sink);
}

bool AghfpProfile_Init(Task init_task)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_Init");
    UNUSED(init_task);

    aghfpProfile_InitTaskData();

    aghfpProfile_InitAghfpLibrary();

    ConManagerRegisterConnectionsClient(&aghfp_profile_task_data.task);

    VoiceSources_RegisterVolume(voice_source_hfp_1, AghfpProfile_GetVoiceSourceVolumeInterface());

    ProfileManager_RegisterProfile(profile_manager_hfp_profile,
                                   aghfpProfile_ConnectRequestCb,
                                   aghfpProfile_DisconnectRequestCb);
    return TRUE;
}

void AghfpProfile_CallIncomingInd(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_CallIncomingInd");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        instance = PanicNull(AghfpProfileInstance_Create(bd_addr, TRUE));
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_DISCONNECTED:
            aghfpProfile_HandleIncomingInd(instance);
        break;
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return;
    }
}

bool AghfpProfile_HoldActiveCall(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_HoldActiveCall");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(AghfpProfile_HoldActiveCall);
        return FALSE;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_DISCONNECTED:
        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_HOLD_CALL_REQ);
            message->instance = instance;
            MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_HOLD_CALL_REQ, message);
        }
        break;
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return FALSE;
    }

    return TRUE;
}

bool AghfpProfile_ReleaseHeldCall(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ReleaseHeldCall");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(AghfpProfile_ReleaseHeldCall);
        return FALSE;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_DISCONNECTED:
        {
            MAKE_AGHFP_MESSAGE(AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ);
            message->instance = instance;
            MessageSend(AghfpProfile_GetInstanceTask(instance), AGHFP_INTERNAL_HFP_RELEASE_HELD_CALL_REQ, message);
        }
        break;
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return FALSE;
    }

    return TRUE;
}

void AghfpProfile_CallOutgoingInd(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_CallOutgoingInd Address: nap %#x lap %#x uap %#x", bd_addr->nap,
                                                                                        bd_addr->lap,
                                                                                        bd_addr->uap);

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(AghfpProfile_CallOutgoingInd);
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    DEBUG_LOG("AghfpProfile_CallOutgoingInd: State enum:aghfpState:%d", state);

    /* Cancel any pending hang up requests */
    MessageCancelAll(&instance->task, AGHFP_INTERNAL_HFP_CALL_HANGUP_REQ);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
            AghfpProfileCallList_AddOutgoingCall(instance->call_list);
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_OUTGOING);
        break;
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_OutgoingCallAnswered(const bdaddr *bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_OutgoingCallAnswered");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(AghfpProfile_OutgoingCallAnswered);
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    AghfpProfileCallList_OutgoingCallAnswered(instance->call_list);

    switch(state)
    {
        case AGHFP_STATE_CONNECTED_IDLE:
        case AGHFP_STATE_DISCONNECTED:
        case AGHFP_STATE_DISCONNECTING:
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
            return;
        case AGHFP_STATE_CONNECTED_OUTGOING:
            instance->bitfields.call_status = aghfp_call_active;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTED_ACTIVE);
        case AGHFP_STATE_NULL:
            return;
    }
}

void AghfpProfile_EnableInBandRinging(const bdaddr *bd_addr, bool enable)
{
    DEBUG_LOG("AghfpProfile_EnableInBandRinging");

    aghfpInstanceTaskData *instance = AghfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(AghfpProfile_EnableInBandRinging);
        return;
    }

    aghfpState state = AghfpProfile_GetState(instance);

    switch(state)
    {
    case AGHFP_STATE_CONNECTED_IDLE:
        instance->bitfields.in_band_ring = enable;
        AghfpProfileAbstract_EnableInbandRingTone(instance);

    default:
        return;
    }
}

void AghfpProfile_SetClipInd(clip_data clip)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_SetClipInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    if (clip.size_clip_number < 3)
    {
        DEBUG_LOG_ERROR("AghfpProfile_SetClipInd: Invalid number size %d", clip.size_clip_number);
        return;
    }

    for_all_aghfp_instances(instance, &iterator)
     {
        if (instance->clip.clip_number == NULL)
        {
            instance->clip.clip_number = malloc(clip.size_clip_number);
        }

        memmove(instance->clip.clip_number, clip.clip_number, clip.size_clip_number);

        instance->clip.clip_type = clip.clip_type;
        instance->clip.size_clip_number = clip.size_clip_number;

        instance->bitfields.caller_id_active_host = TRUE;
    }
}

void AghfpProfile_ClearClipInd(void)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ClearClipInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->clip.clip_number != NULL)
        {
            free(instance->clip.clip_number);
            instance->clip.clip_number = NULL;
        }

        instance->clip.clip_type = 0;
        instance->clip.size_clip_number = 0;

        instance->bitfields.caller_id_active_host = FALSE;
    }
}

void AghfpProfile_SetNetworkOperatorInd(char *network_operator)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_SetNetworkOperatorInd");


    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    if (strlen(network_operator) == 0  || strlen(network_operator) > 17)
    {
        DEBUG_LOG_ERROR("AghfpProfile_SetNetworkOperatorInd: Invalid size %d", strlen(network_operator));
        return;
    }

    char network_op_with_quotes[20];
    sprintf(network_op_with_quotes, "\"%s\"\0", network_operator);

    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->network_operator == NULL)
        {
            instance->network_operator = malloc(strlen(network_op_with_quotes) + 1);
        }

        memmove(instance->network_operator, network_op_with_quotes, strlen(network_op_with_quotes) + 1);
    }
}

void AghfpProfile_ClearNetworkOperatorInd(void)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ClearNetworkOperatorInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->network_operator != NULL)
        {
            free(instance->network_operator);
            instance->network_operator = NULL;
        }
    }
}

aghfpInstanceTaskData * AghfpProfileInstance_GetInstanceForSource(voice_source_t source)
{
    aghfpInstanceTaskData* instance = NULL;

    if (source != voice_source_none)
    {
        device_t device = aghfpProfileInstance_FindDeviceFromVoiceSource(source);

        if (device != NULL)
        {
            instance = AghfpProfileInstance_GetInstanceForDevice(device);
        }
    }

    DEBUG_LOG_V_VERBOSE("HfpProfileInstance_GetInstanceForSource(%p) enum:voice_source_t:%d",
                         instance, source);

    return instance;
}

void AghfpProfile_ClientRegister(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), task);
}

void AghfpProfile_RegisterStatusClient(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetStatusNotifyList()), task);
}

void AghfpProfile_ClearCallHistory(void)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_ClearCallHistory");

    if (last_dialed_number.number != NULL)
    {
        free(last_dialed_number.number);
        last_dialed_number.number_len = 0;
        last_dialed_number.number = NULL;
    }
}

/*! \brief Update the last number dialled by the HF
*/
void AghfpProfile_SetLastDialledNumber(uint16 length, uint8* number)
{
    DEBUG_LOG_FN_ENTRY("AghfpProfile_UpdateLastDialledNumber");

    if (last_dialed_number.number != NULL)
    {
        free(last_dialed_number.number);
    }

    last_dialed_number.number = PanicNull(malloc(length));
    memmove(last_dialed_number.number, number, length);
    last_dialed_number.number_len = length;
}

void AghfpProfile_SendServiceIndicator(aghfpInstanceTaskData * instance, aghfp_service_availability avail)
{
    AghfpProfileAbstract_SendServiceIndication(instance, avail);
}

void AghfpProfile_SendSignalStrengthIndicator(aghfpInstanceTaskData * instance, uint16 level)
{
    AghfpProfileAbstract_SendSignalStrengthIndication(instance, level);
}


void AghfpProfile_SendBatteryChgIndicator(aghfpInstanceTaskData * instance, uint16 level)
{
    AghfpProfileAbstract_SendBattChgIndicator(instance, level);
}


/*! \brief Send SLC status indication to all clients on the list.
 */
void AghfpProfile_SendSlcStatus(bool connected, const bdaddr* bd_addr)
{
    Task next_client = NULL;

    while (TaskList_Iterate(TaskList_GetFlexibleBaseTaskList(aghfpProfile_GetSlcStatusNotifyList()), &next_client))
    {
        MAKE_AGHFP_MESSAGE(APP_AGHFP_SLC_STATUS_IND);
        message->slc_connected = connected;
        message->bd_addr = *bd_addr;
        MessageSend(next_client, APP_AGHFP_SLC_STATUS_IND, message);
    }
}
