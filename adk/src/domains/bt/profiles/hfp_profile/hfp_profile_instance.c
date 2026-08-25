/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain HFP dynamic instance management.
*/

#include "hfp_profile_instance.h"

#include "system_state.h"
#include "bt_device.h"
#include "device_properties.h"
#include "hfp_profile_config.h"
#include "link_policy.h"
#include "voice_sources.h"
#include "hfp_profile_audio.h"
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
#include "ui.h"
#include "hfp_profile.h"

#include <profile_manager.h>
#include <av.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <ps.h>
#include <device_db_serialiser.h>

#include <stdio.h>

#define INDEX_OF_TOTAL_FRAME_COUNTER   2
#define INDEX_OF_ERROR_FRAME_COUNTER   3
#define NUMBER_OF_PARAMS              12

/*! Structure holding counts of previously read good aptx
 *  voice encoded frames and error frames. */
typedef struct
{
    /*! Correct encoded good frame counts */
    uint32 previous_good_frame_counts;

    /*! Error frame counts */
    uint32 previous_error_frame_counts;

} aptx_voice_frames_counts_info_t;

aptx_voice_frames_counts_info_t aptx_voice_frames_counts_info = {0, 0};

/* Local Function Prototypes */
static void hfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message);

#define HFP_MAX_NUM_INSTANCES 2

#define for_all_hfp_instances(hfp_instance, iterator) for(hfp_instance = HfpInstance_GetFirst(iterator); hfp_instance != NULL; hfp_instance = HfpInstance_GetNext(iterator))

static void HfpInstance_AddDeviceHfpInstanceToIterator(device_t device, void * iterator_data)
{
    hfpInstanceTaskData* hfp_instance = HfpProfileInstance_GetInstanceForDevice(device);

    if(hfp_instance)
    {
        hfp_instance_iterator_t * iterator = (hfp_instance_iterator_t *)iterator_data;
        iterator->instances[iterator->index] = hfp_instance;
        iterator->index++;
    }
}

hfpInstanceTaskData * HfpInstance_GetFirst(hfp_instance_iterator_t * iterator)
{
    memset(iterator, 0, sizeof(hfp_instance_iterator_t));

    DeviceList_Iterate(HfpInstance_AddDeviceHfpInstanceToIterator, iterator);

    iterator->index = 0;

    return iterator->instances[iterator->index];
}

hfpInstanceTaskData * HfpInstance_GetNext(hfp_instance_iterator_t * iterator)
{
    iterator->index++;

    if(iterator->index >= HFP_MAX_NUM_INSTANCES)
        return NULL;

    return iterator->instances[iterator->index];
}

/*! \brief Handle connect HFP SLC request
*/
static void appHfpHandleInternalHfpConnectRequest(hfpInstanceTaskData *instance, const HFP_INTERNAL_HFP_CONNECT_REQ_T *req)
{
    hfpState state = appHfpGetState(instance);
    bdaddr *addr = &instance->ag_bd_addr;

    DEBUG_LOG("appHfpHandleInternalHfpConnectRequest(%p), enum:hfpState:%d %04x,%02x,%06lx",
              instance, state, addr->nap, addr->uap, addr->lap);

    if(HfpProfile_StateIsSlcDisconnected(state))
    {
        if (ConManagerIsConnected(addr))
        {
            /* Store AG profile type */
            instance->profile = req->profile;

            /* Move to connecting local state */
            appHfpSetState(instance, HFP_STATE_CONNECTING_LOCAL);
        }
        else
        {
            DEBUG_LOG("appHfpHandleInternalHfpConnectRequest, no ACL");

            /* Set disconnect reason */
            instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;

            /* Move to 'disconnected' state */
            appHfpSetState(instance, HFP_STATE_DISCONNECTED);

            HfpProfileInstance_Destroy(instance);
        }
    }
    else if(HfpProfile_StateIsSlcDisconnecting(state))
    {
        MAKE_HFP_MESSAGE(HFP_INTERNAL_HFP_CONNECT_REQ);

        /* repost the connect message pending final disconnection of the profile
         * via the lock */
        message->profile = req->profile;
        MessageSendConditionally(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_HFP_CONNECT_REQ, message,
                                 HfpProfileInstance_GetLock(instance));
    }
    else if(HfpProfile_StateIsSlcConnectedOrConnecting(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpConnectRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_CONNECT_REQ, req);
    }
}

/*! \brief Handle disconnect HFP SLC request
*/
static void appHfpHandleInternalHfpDisconnectRequest(hfpInstanceTaskData *instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpDisconnectRequest enum:hfpState:%d", state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        /* Move to disconnecting state */
        appHfpSetState(instance, HFP_STATE_DISCONNECTING);
    }
    else if(HfpProfile_StateIsSlcDisconnectedOrDisconnecting(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpDisconnectRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_DISCONNECT_REQ, NULL);
    }
}

/*! \brief Handle last number redial request
*/
static void appHfpHandleInternalHfpLastNumberRedialRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpLastNumberRedialRequest enum:hfpState:%d", state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        hfpProfile_LastNumberRedial(instance);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpLastNumberRedialRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ, NULL);
    }
}

/*! \brief Handle voice dial request
*/
static void appHfpHandleInternalHfpVoiceDialRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpVoiceDialRequest(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        hfpProfile_VoiceDialEnable(instance);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpVoiceDialRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_VOICE_DIAL_REQ, NULL);
    }
}

/*! \brief Handle voice dial disable request
*/
static void appHfpHandleInternalHfpVoiceDialDisableRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    
    DEBUG_LOG("appHfpHandleInternalHfpVoiceDialDisableRequest(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        hfpProfile_VoiceDialDisable(instance);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpVoiceDialDisableRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ, NULL);
    }
}

static void appHfpHandleInternalNumberDialRequest(hfpInstanceTaskData * instance, HFP_INTERNAL_NUMBER_DIAL_REQ_T * message)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalNumberDialRequest(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        hfpProfile_DialNumber(instance, message->number, message->length);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalNumberDialRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_NUMBER_DIAL_REQ, NULL);
    }
}

/*! \brief Handle accept call request
*/
static void appHfpHandleInternalHfpCallAcceptRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallAcceptRequest(%p) enum:hfpState:%d", instance, state);

    if(state == HFP_STATE_CONNECTED_INCOMING)
    {
        hfpProfile_AnswerCall(instance);
    }
    else
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
        DEBUG_LOG("appHfpHandleInternalHfpCallAcceptRequest, ignored");
    }
}

/*! \brief Handle reject call request
*/
static void appHfpHandleInternalHfpCallRejectRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallRejectRequest(%p) enum:hfpState:%d", instance, state);

    if(state == HFP_STATE_CONNECTED_INCOMING)
    {
        if (instance->profile == hfp_headset_profile)
        {
            Telephony_NotifyError(HfpProfileInstance_GetVoiceSourceForInstance(instance));
        }
        else
        {
            hfpProfile_RejectCall(instance);
        }
    }
    else
    {
        DEBUG_LOG("appHfpHandleInternalHfpCallRejectRequest, ignored");
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
    }
}

/*! \brief Handle hangup call request
*/
static void appHfpHandleInternalHfpCallHangupRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpCallHangupRequest(%p) enum:hfpState:%d", instance, state);

    switch(state)
    {
        case HFP_STATE_CONNECTED_ACTIVE:
        case HFP_STATE_CONNECTED_MULTIPARTY:
        case HFP_STATE_CONNECTED_OUTGOING:
            hfpProfile_TerminateCall(instance);
        break;
        
        default:
            DEBUG_LOG("appHfpHandleInternalHfpCallHangupRequest, ignored");
            voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
            Telephony_NotifyError(source);
        break;
    }
}

/*! \brief Handle mute/unmute request
*/
static void appHfpHandleInternalHfpMuteRequest(hfpInstanceTaskData *instance, const HFP_INTERNAL_HFP_MUTE_REQ_T *req)
{
    hfpState state = appHfpGetState(instance);
    uint8 hfp_mic_gain = 0;
    
    DEBUG_LOG("appHfpHandleInternalHfpMuteRequest(%p) enum:hfpState:%d", instance, state);

    if(HfpProfile_StateHasActiveCall(state))
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        if (req->mute)
        {
            Telephony_NotifyMicrophoneMuted(source);
        }
        else
        {
            Telephony_NotifyMicrophoneUnmuted(source);
            hfp_mic_gain = HFP_MICROPHONE_GAIN;
        }
        
        hfpProfile_SetMicrophoneGain(instance, hfp_mic_gain);

        /* Set input gain */
        device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
        Device_SetPropertyU8(device, device_property_hfp_mic_gain, hfp_mic_gain);

        /* Store new configuration */
        HfpProfile_StoreConfig(device);
        
        /* Set mute flag */
        instance->bitfields.mute_active = req->mute;

        /* Re-configure audio chain */
        appKymeraScoMicMute(req->mute);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpMuteRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_MUTE_REQ, NULL);
    }
}

/*! \brief Handle audio transfer request
*/
static void appHfpHandleInternalHfpTransferRequest(hfpInstanceTaskData *instance, const HFP_INTERNAL_HFP_TRANSFER_REQ_T *req)
{
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("appHfpHandleInternalHfpTransferRequest state enum:hfpState:%u direction enum:voice_source_audio_transfer_direction_t:%d",
               state,
               req->direction);

    if(HfpProfile_StateIsSlcConnected(state))
    {
        hfpProfile_HandleAudioTransferRequest(instance, req->direction);
    }
    else if(HfpProfile_StateIsInitialised(state))
    {
        DEBUG_LOG("appHfpHandleInternalHfpTransferRequest, ignored");
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_INTERNAL_HFP_TRANSFER_REQ, NULL);
    }
}

/*! \brief Handle HSP incoming call timeout

    We have had a ring indication for a while, so move back to 'connected
    idle' state.
*/
static void appHfpHandleHfpHspIncomingTimeout(hfpInstanceTaskData* instance)
{
    DEBUG_LOG("appHfpHandleHfpHspIncomingTimeout(%p)", instance);

    switch (appHfpGetState(instance))
    {
        case HFP_STATE_CONNECTED_INCOMING:
        {
            /* Move back to connected idle state */
            appHfpSetState(instance, HFP_STATE_CONNECTED_IDLE);
        }
        return;

        default:
            HfpProfile_HandleError(instance, HFP_INTERNAL_HSP_INCOMING_TIMEOUT, NULL);
            return;
    }
}

static void appHfpHandleInternalOutOfBandRingtoneRequest(hfpInstanceTaskData * instance)
{
    if(HfpProfile_StateHasIncomingCall(appHfpGetState(instance)))
    {
        Telephony_NotifyCallIncomingOutOfBandRingtone(HfpProfileInstance_GetVoiceSourceForInstance(instance));

        MessageCancelFirst(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ);
        MessageSendLater(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ, NULL, D_SEC(5));
    }
}

static void appHfpHandleInternalReleaseWaitingRejectIncomingRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalReleaseWaitingRejectIncomingRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasActiveAndIncomingCall(state) || HfpProfile_StateHasHeldCall(state))
    {
        hfpProfile_ReleaseWaitingRejectIncoming(instance);
    }
    else
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
    }
}

static void appHfpHandleInternalAcceptWaitingReleaseActiveRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalAcceptWaitingReleaseActiveRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasEstablishedCall(state))
    {
        hfpProfile_AcceptWaitingReleaseActive(instance);
    }
    else
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
    }
}

static void appHfpHandleInternalAcceptWaitingHoldActiveRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalAcceptWaitingHoldActiveRequest %p enum:hfpState:%d", instance, state);
    
    /* Allow in active or held call states to enable putting an active call on hold or resuming held call */
    if(HfpProfile_StateHasEstablishedCall(state))
    {
        hfpProfile_AcceptWaitingHoldActive(instance);
    }
    else
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
    }
}

static void appHfpHandleInternalAddHeldToMultipartyRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalAddHeldToMultipartyRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasMultipleCalls(state))
    {
        hfpProfile_AddHeldToMultiparty(instance);
    }
    else
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
    }
}

static void appHfpHandleInternalJoinCallsAndHangUpRequest(hfpInstanceTaskData * instance)
{
    hfpState state = appHfpGetState(instance);
    DEBUG_LOG("appHfpHandleInternalJoinCallsAndHangUpRequest %p enum:hfpState:%d", instance, state);
    
    if(HfpProfile_StateHasMultipleCalls(state))
    {
        hfpProfile_JoinCallsAndHangUp(instance);
    }
    else
    {
        voice_source_t source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        Telephony_NotifyError(source);
    }
}

static void hfpProfileInstance_InitTaskData(hfpInstanceTaskData* instance)
{
    /* Set up instance task handler */
    instance->task.handler = hfpProfile_InstanceHandleMessage;

    /* By default, assume remote device supports all HFP standard packet types.
       This is modified when the remote features are read from the device after
       connection. */
    instance->sco_supported_packets = sync_all_sco | sync_ev3 | sync_2ev3;

    /* Initialise state */
    instance->sco_sink = 0;
    instance->hfp_lock = 0;
    instance->bitfields.disconnect_reason = APP_HFP_CONNECT_FAILED;
    instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;
    instance->bitfields.call_accepted = FALSE;
    instance->codec = hfp_wbs_codec_mask_none;
    instance->wesco = 0;
    instance->tesco = 0;
    instance->qce_codec_mode_id = CODEC_MODE_ID_UNSUPPORTED;

    /* Move to disconnected state */
    instance->state = HFP_STATE_DISCONNECTED;
    
    hfpProfile_InitialiseInstancePortSpecificData(instance);
}

static void hfpProfile_BlockHandsetForSwb(const bdaddr *bd_addr)
{
    device_t device  = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        flags |= DEVICE_FLAGS_SWB_NOT_SUPPORTED;
        Device_SetPropertyU16(device, device_property_flags, flags);
        DeviceDbSerialiser_SerialiseDevice(device);
        DEBUG_LOG("hfpProfile_BlockHandsetForSwb:Handset blocked for swb");
    }
}

static bool appHfpAptxVoiceCurrentGoodFramesCountIsSameAsPreviousGoodFramesCount(uint32 current_good_frame_counts)
{
    return ((current_good_frame_counts - aptx_voice_frames_counts_info.previous_good_frame_counts) == 0);
}

static bool appHfpAptxVoiceErrorFramesCountIncreasedFromPreviousErrorFramesCount(uint32 current_error_frame_counts)
{
     return ((current_error_frame_counts - aptx_voice_frames_counts_info.previous_error_frame_counts) > 0);
}

static bool appHfpAptxVoiceFrameCountersAreNotOk(uint32 current_good_frame_counts, uint32 current_error_frame_counts)
{
    bool aptx_voice_frame_counters_not_ok = FALSE;

    if (appHfpAptxVoiceCurrentGoodFramesCountIsSameAsPreviousGoodFramesCount(current_good_frame_counts) &&
        appHfpAptxVoiceErrorFramesCountIncreasedFromPreviousErrorFramesCount(current_error_frame_counts))
    {
        aptx_voice_frame_counters_not_ok = TRUE;
    }
    return aptx_voice_frame_counters_not_ok;
}

static bool appHfpHandleNoAudioInSwbCall(get_status_data_t * operator_status)
{
    /* OPR_SCO_RECEIVE operator decodes aptX voice packets in swb call.In no audio swb call scenario,
     * good frames count will not increase but error frames count will keep increasing.Good frames count
     * is calculated by substracting error frames count(fourth word in operator status data) from total
     * frame counts(third word in operator status data). */

      bool swb_call_no_audio = FALSE;
      uint32 current_total_frame_counts = operator_status->value[INDEX_OF_TOTAL_FRAME_COUNTER];
      uint32 current_error_frame_counts = operator_status->value[INDEX_OF_ERROR_FRAME_COUNTER];

      uint32 current_good_frame_counts = current_total_frame_counts - current_error_frame_counts;

    if(appHfpAptxVoiceFrameCountersAreNotOk(current_good_frame_counts, current_error_frame_counts))
    {
        swb_call_no_audio = TRUE;
    }

    aptx_voice_frames_counts_info.previous_good_frame_counts = current_good_frame_counts;
    aptx_voice_frames_counts_info.previous_error_frame_counts = current_error_frame_counts;

    DEBUG_LOG("appHfpHandleNoAudioInSwbCall:%d",swb_call_no_audio);
    return swb_call_no_audio;
}

void HfpProfileInstance_ResetAptxVoiceFrameCounts(void)
{
    aptx_voice_frames_counts_info.previous_good_frame_counts = 0;
    aptx_voice_frames_counts_info.previous_error_frame_counts = 0;
}

static void appHfpRecheckAptxVoicePacketsCounterAfterSometime(hfpInstanceTaskData* instance)
{
    MessageSendLater(&instance->task, HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ, NULL, HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS);
}

static bool appHfpSwbCallActive(hfp_call_state call_state, uint16 qce_codec_mode_id)
{
    return ((call_state == hfp_call_state_active) && HfpProfile_HandsetSupportsSuperWideband(qce_codec_mode_id));
}

void HfpProfileInstance_StartCheckingAptxVoicePacketsCounterImmediatelyIfSwbCallActive(hfpInstanceTaskData * instance)
{
    if (appHfpSwbCallActive(instance->bitfields.call_state, instance->qce_codec_mode_id))
    {
        DEBUG_LOG("HfpProfileInstance_StartCheckingAptxVoicePacketsCounterImmediatelyIfSwbCallActive:(%p)Handover in swb call",instance);
        MessageSend(&instance->task, HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ, NULL);
    }
}

static void appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter(hfpInstanceTaskData* instance)
{
    get_status_data_t * operator_status = Kymera_GetOperatorStatusDataInScoChain(OPR_SCO_RECEIVE, NUMBER_OF_PARAMS);

    /* It may be possible that sco chain is not loaded yet and we may be trying to read OPR_SCO_RECEIVE
     * status data earlier which will fail. So we will read it again after HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS delay. */

    if (operator_status == NULL)
    {
        appHfpRecheckAptxVoicePacketsCounterAfterSometime(instance);
        return;
    }

    DEBUG_LOG_VERBOSE("appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter, result=%d, num of params=%d",
                       operator_status->result,operator_status->number_of_params);

    for(uint8 index=0;index<operator_status->number_of_params;index++)
    {
       DEBUG_LOG("%d ",operator_status->value[index]);
    }

    if (appHfpHandleNoAudioInSwbCall(operator_status))
    {
         DEBUG_LOG_VERBOSE("appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter:No Swb Audio Detected.Disconnecting SLC.");
         hfpProfile_BlockHandsetForSwb(&instance->ag_bd_addr);

         /* Disconnect slc to re negotiate hfp codec again and will reconnect once slc disconnect is complete. */
         instance->bitfields.reconnect_handset = TRUE;
         hfpProfile_Disconnect(&instance->ag_bd_addr);
    }
    else
    {
         DEBUG_LOG_VERBOSE("appHfpHandleInternalHfpMonitorAptxVoicePacketsCountere:Swb packets ok.");
         appHfpRecheckAptxVoicePacketsCounterAfterSometime(instance);
    }

    free(operator_status);
}

/*! \brief Message Handler for a specific HFP Instance

    This function is the main message handler for the HFP instance, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.
*/
static void hfpProfile_InstanceHandleMessage(Task task, MessageId id, Message message)
{
    hfpInstanceTaskData *instance = STRUCT_FROM_MEMBER(hfpInstanceTaskData, task, task);
    DEBUG_LOG("hfpProfile_InstanceHandleMessage id 0x%x", id);

    /* Handle internal messages */
    switch (id)
    {
        case HFP_INTERNAL_HSP_INCOMING_TIMEOUT:
            appHfpHandleHfpHspIncomingTimeout(instance);
            return;

        case HFP_INTERNAL_HFP_CONNECT_REQ:
            appHfpHandleInternalHfpConnectRequest(instance, (HFP_INTERNAL_HFP_CONNECT_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_DISCONNECT_REQ:
            appHfpHandleInternalHfpDisconnectRequest(instance);
            return;

        case HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ:
            appHfpHandleInternalHfpLastNumberRedialRequest(instance);
            return;

        case HFP_INTERNAL_HFP_VOICE_DIAL_REQ:
            appHfpHandleInternalHfpVoiceDialRequest(instance);
            return;

        case HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ:
            appHfpHandleInternalHfpVoiceDialDisableRequest(instance);
            return;

        case HFP_INTERNAL_HFP_CALL_ACCEPT_REQ:
            appHfpHandleInternalHfpCallAcceptRequest(instance);
            return;

        case HFP_INTERNAL_HFP_CALL_REJECT_REQ:
            appHfpHandleInternalHfpCallRejectRequest(instance);
            return;

        case HFP_INTERNAL_HFP_CALL_HANGUP_REQ:
            appHfpHandleInternalHfpCallHangupRequest(instance);
            return;

        case HFP_INTERNAL_HFP_MUTE_REQ:
            appHfpHandleInternalHfpMuteRequest(instance, (HFP_INTERNAL_HFP_MUTE_REQ_T *)message);
            return;

        case HFP_INTERNAL_HFP_TRANSFER_REQ:
            appHfpHandleInternalHfpTransferRequest(instance, (HFP_INTERNAL_HFP_TRANSFER_REQ_T *)message);
            return;

        case HFP_INTERNAL_NUMBER_DIAL_REQ:
            appHfpHandleInternalNumberDialRequest(instance, (HFP_INTERNAL_NUMBER_DIAL_REQ_T *)message);
            return;

        case HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ:
            appHfpHandleInternalHfpMonitorAptxVoicePacketsCounter(instance);
            return;

        case HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ:
            appHfpHandleInternalOutOfBandRingtoneRequest(instance);
            return;
        
        case HFP_INTERNAL_HFP_RELEASE_WAITING_REJECT_INCOMING_REQ:
            appHfpHandleInternalReleaseWaitingRejectIncomingRequest(instance);
            return;
            
        case HFP_INTERNAL_HFP_ACCEPT_WAITING_RELEASE_ACTIVE_REQ:
            appHfpHandleInternalAcceptWaitingReleaseActiveRequest(instance);
            return;
    
        case HFP_INTERNAL_HFP_ACCEPT_WAITING_HOLD_ACTIVE_REQ:
            appHfpHandleInternalAcceptWaitingHoldActiveRequest(instance);
            return;
            
        case HFP_INTERNAL_HFP_ADD_HELD_TO_MULTIPARTY_REQ:
            appHfpHandleInternalAddHeldToMultipartyRequest(instance);
            return;
        
        case HFP_INTERNAL_HFP_JOIN_CALLS_AND_HANG_UP:
            appHfpHandleInternalJoinCallsAndHangUpRequest(instance);
            return;
    }
}

device_t HfpProfileInstance_FindDeviceFromInstance(hfpInstanceTaskData* instance)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_hfp_instance, &instance, sizeof(hfpInstanceTaskData *));
}

static void hfpProfileInstance_SetInstanceForDevice(device_t device, hfpInstanceTaskData* instance)
{
    PanicFalse(Device_SetProperty(device, device_property_hfp_instance, &instance, sizeof(hfpInstanceTaskData *)));
}

typedef struct voice_source_search_data
{
    /*! The voice source associated with the device to find */
    voice_source_t source_to_find;
    /*! Set to TRUE if a device with the source is found */
    bool source_found;
} voice_source_search_data_t;

static void hfpProfileInstance_SearchForHandsetWithVoiceSource(device_t device, void * data)
{
    voice_source_search_data_t *search_data = data;
    if ((DeviceProperties_GetVoiceSource(device) == search_data->source_to_find) &&
        (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET))
    {
        search_data->source_found = TRUE;
    }
}

static voice_source_t hfpProfileInstance_AllocateVoiceSourceToDevice(hfpInstanceTaskData *instance)
{
    voice_source_search_data_t search_data = {voice_source_hfp_1, FALSE};
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    PanicFalse(device != NULL);

    /* Find a free voice source */
    DeviceList_Iterate(hfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    if (search_data.source_found)
    {
        /* If hfp_1 has been allocated, try to allocate hfp_2 */
        search_data.source_found = FALSE;
        search_data.source_to_find = voice_source_hfp_2;
        DeviceList_Iterate(hfpProfileInstance_SearchForHandsetWithVoiceSource, &search_data);
    }
    if (!search_data.source_found)
    {
        /* A free audio_source exists, allocate it to the device with the instance. */
        DeviceProperties_SetVoiceSource(device, search_data.source_to_find);
        DEBUG_LOG_VERBOSE("hfpProfileInstance_AllocateVoiceSourceToDevice inst(%p) device=%p enum:voice_source_t:%d",
                          instance, device, search_data.source_to_find);
    }
    else
    {
        /* It should be impossible to have connected the HFP profile if we have already
           two connected voice sources for HFP, this may indicate a handle was leaked. */
        Panic();
    }

    return search_data.source_to_find;
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForDevice(device_t device)
{
    hfpInstanceTaskData** pointer_to_instance;
    size_t size_pointer_to_instance;

    if(device && Device_GetProperty(device, device_property_hfp_instance, (void**)&pointer_to_instance, &size_pointer_to_instance))
    {
        PanicFalse(size_pointer_to_instance == sizeof(hfpInstanceTaskData*));
        return *pointer_to_instance;
    }
    DEBUG_LOG_VERBOSE("HfpProfileInstance_GetInstanceForDevice device=%p has no device_property_hfp_instance", device);
    return NULL;
}

/*! \brief Get HFP lock */
uint16 * HfpProfileInstance_GetLock(hfpInstanceTaskData* instance)
{
    return &instance->hfp_lock;
}

/*! \brief Set HFP lock */
void HfpProfileInstance_SetLock(hfpInstanceTaskData* instance, uint16 lock)
{
    instance->hfp_lock = lock;
}

/*! \brief Is HFP SCO/ACL encrypted */
bool HfpProfileInstance_IsEncrypted(hfpInstanceTaskData* instance)
{
    return instance->bitfields.encrypted;
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForBdaddr(const bdaddr *bd_addr)
{
    hfpInstanceTaskData* instance = NULL;
    device_t device = NULL;

    PanicNull((void *)bd_addr);

    device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device != NULL)
    {
        instance = HfpProfileInstance_GetInstanceForDevice(device);
    }

    return instance;
}

device_t HfpProfileInstance_FindDeviceFromVoiceSource(voice_source_t source)
{
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_voice_source, &source, sizeof(voice_source_t));
}

hfpInstanceTaskData * HfpProfileInstance_GetInstanceForSource(voice_source_t source)
{
    hfpInstanceTaskData* instance = NULL;

    if (source != voice_source_none)
    {
        device_t device = HfpProfileInstance_FindDeviceFromVoiceSource(source);

        if (device != NULL)
        {
            instance = HfpProfileInstance_GetInstanceForDevice(device);
        }
    }

    DEBUG_LOG_V_VERBOSE("HfpProfileInstance_GetInstanceForSource(%p) enum:voice_source_t:%d",
                         instance, source);

    return instance;
}

voice_source_t HfpProfileInstance_GetVoiceSourceForInstance(hfpInstanceTaskData * instance)
{
    voice_source_t source = voice_source_none;

    PanicNull(instance);

    device_t device = BtDevice_GetDeviceForBdAddr(&instance->ag_bd_addr);
    if (device)
    {
        source = DeviceProperties_GetVoiceSource(device);
    }

    return source;
}

void HfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    /* Register voice source interfaces implementated by HFP profile */
    VoiceSources_RegisterAudioInterface(voice_source, HfpProfile_GetAudioInterface());
    VoiceSources_RegisterTelephonyControlInterface(voice_source, HfpProfile_GetTelephonyControlInterface());
    VoiceSources_RegisterObserver(voice_source, HfpProfile_GetVoiceSourceObserverInterface());
}

void HfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source_t voice_source)
{
    VoiceSources_DeregisterTelephonyControlInterface(voice_source, HfpProfile_GetTelephonyControlInterface());
    VoiceSources_DeregisterObserver(voice_source, HfpProfile_GetVoiceSourceObserverInterface());
}

hfpInstanceTaskData * HfpProfileInstance_Create(const bdaddr *bd_addr, bool allocate_source)
{
    voice_source_t new_source = voice_source_none;

    DEBUG_LOG_FN_ENTRY("HfpProfileInstance_Create");

    device_t device = PanicNull(BtDevice_GetDeviceForBdAddr(bd_addr));

    /* Panic if we have a duplicate instance somehow */
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForDevice(device);
    PanicNotNull(instance);

    /* Allocate new instance */
    instance = PanicUnlessNew(hfpInstanceTaskData);
    memset(instance, 0 , sizeof(*instance));
    hfpProfileInstance_SetInstanceForDevice(device, instance);

    DEBUG_LOG("HfpProfileInstance_Create(%p) device=%p", instance, device);

    /* Initialise instance */
    hfpProfileInstance_InitTaskData(instance);

    /* Set Bluetooth address of remote device */
    instance->ag_bd_addr = *bd_addr;

    /* initialise the routed state */
    instance->source_state = source_state_disconnected;

    if (appDeviceIsHandset(bd_addr))
    {
        if(allocate_source)
        {
            new_source = hfpProfileInstance_AllocateVoiceSourceToDevice(instance);
            HfpProfileInstance_RegisterVoiceSourceInterfaces(new_source);
        }
    }
    else
    {
        Panic(); /* Unexpected device type */
    }

    /* Return pointer to new instance */
    return instance;
}

/*! \brief Destroy HFP instance

    This function should only be called if the instance no longer has HFP connected.
    If HFP is still connected, the function will silently fail.

    The function will panic if the instance is not valid, or if the instance
    is already destroyed.

    \param  instance The instance to destroy

*/
void HfpProfileInstance_Destroy(hfpInstanceTaskData *instance)
{
    DEBUG_LOG("HfpProfileInstance_Destroy(%p)", instance);
    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);

    PanicNull(device);

    /* Destroy instance only if state machine is disconnected. */
    if (HfpProfile_IsDisconnected(instance))
    {
        DEBUG_LOG("HfpProfileInstance_Destroy(%p) permitted", instance);

        /* Flush any messages still pending delivery */
        MessageFlushTask(&instance->task);

        /* Clear entry and free instance */
        hfpProfileInstance_SetInstanceForDevice(device, NULL);
        free(instance);

        voice_source_t source = DeviceProperties_GetVoiceSource(device);
        DeviceProperties_RemoveVoiceSource(device);

        /* Deregister voice source interfaces that were implementated by the instance. */
        HfpProfileInstance_DeregisterVoiceSourceInterfaces(source);
    }
    else
    {
        DEBUG_LOG("HfpProfileInstance_Destroy(%p) HFP (%d) not disconnected, or HFP Lock Pending",
                   instance, !HfpProfile_IsDisconnected(instance));
    }
}
