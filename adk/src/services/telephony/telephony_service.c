/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Telephony Service
*/

#include "telephony_service.h"
#include "telephony_service_call_control.h"
#include "telephony_service_private.h"

#include "kymera_adaptation.h"
#include "ui.h"
#include "ui_inputs.h"
#include "ui_user_config.h"
#include "telephony_messages.h"
#include "voice_sources.h"
#include "voice_sources_list.h"
#include "usb_audio.h"
#include "audio_router.h"
#include "device_properties.h"

#include <focus_voice_source.h>
#include <panic.h>
#include <logging.h>

static void telephonyService_CallStateNotificationMessageHandler(Task task, MessageId id, Message message);
static void telephonyService_HandleUiInput(Task task, MessageId ui_input, Message message);

const TaskData telephony_message_handler_task = { telephonyService_CallStateNotificationMessageHandler };
static const TaskData ui_handler_task = { telephonyService_HandleUiInput };

static const message_group_t ui_inputs[] =
{
    UI_INPUTS_TELEPHONY_MESSAGE_GROUP,
};

#ifdef INCLUDE_UI_USER_CONFIG
static const ui_user_config_context_id_map_t telephony_map[] =
{
    { .context_id=ui_context_voice_outgoing, .context=context_voice_ringing_outgoing },
    { .context_id=ui_context_voice_outgoing, .context=context_voice_in_call_with_outgoing },
    { .context_id=ui_context_voice_incoming, .context=context_voice_in_call_with_incoming },
    { .context_id=ui_context_voice_incoming, .context=context_voice_ringing_incoming },
    { .context_id=ui_context_voice_in_call, .context=context_voice_in_call },
    { .context_id=ui_context_voice_in_call, .context=context_voice_in_multiparty_call },
    { .context_id=ui_context_voice_in_call, .context=context_voice_in_call_with_held },
    { .context_id=ui_context_voice_held_call, .context=context_voice_call_held },
};
#endif

telephony_service_data_t telephony_service;

static void telephonyService_AddVoiceSource(voice_source_t source)
{
    generic_source_t voice_source = {.type = source_type_voice, .u.voice = source};

    AudioRouter_AddSource(voice_source);
}

static void telephonyService_handleTelephonyConnected(const TELEPHONY_CONNECTED_T *message)
{
    if(message)
    {
        DEBUG_LOG_INFO("telephonyService_handleTelephonyConnected enum:voice_source_t:%d", message->voice_source);

        VoiceSources_EnableCallerId(message->voice_source, telephony_service.caller_id_enabled);
    }
}

static void telephonyService_handleTelephonyAudioConnected(const TELEPHONY_AUDIO_CONNECTED_T *message)
{
    if(message)
    {
        DEBUG_LOG_INFO("telephonyService_handleTelephonyAudioConnected enum:voice_source_t:%d", message->voice_source);

        telephonyService_AddVoiceSource(message->voice_source);
    }
}

static void telephonyService_handleTelephonyAudioDisconnected(const TELEPHONY_AUDIO_DISCONNECTED_T *message)
{
    if(message)
    {
        generic_source_t voice_source = {.type = source_type_voice, .u.voice = message->voice_source};

        DEBUG_LOG_INFO("telephonyService_handleTelephonyAudioDisconnected enum:voice_source_t:%d", message->voice_source);

        AudioRouter_RemoveSource(voice_source);
    }
}

static void telephonyService_RefreshCallStates(voice_source_t source)
{
    voice_source_t source_to_refresh = voice_source_none;

    /* Get the device for the source for which the call state indication is recieved */
    device_t device = VoiceSources_GetDeviceForSource(source);
    if (device != NULL)
    {
        /* Find the voice source to refresh for this device */
        if (source == voice_source_le_audio_unicast)
        {
            source_to_refresh = DeviceProperties_GetVoiceSource(device);
        }
        else if (source == voice_source_hfp_1 || source == voice_source_hfp_2)
        {
            source_to_refresh = DeviceProperties_GetLeVoiceSource(device);
        }

        if (source_to_refresh > voice_source_none && source_to_refresh < max_voice_sources)
        {
            DEBUG_LOG_INFO("telephonyService_RefreshCallStates source : enum:voice_source_t:%d, source_to_refresh : enum:voice_source_t:%d, device:%p",
                            source, source_to_refresh, device);
            VoiceSources_RefreshCallState(source_to_refresh);
        }
    }
}

static void telephonyService_CallStateNotificationMessageHandler(Task task, MessageId id, Message message)
{
    bool handled = TRUE;
    const telephony_message_t* msg = (const telephony_message_t*)message;

    PanicNull((void *)msg);

    UNUSED(task);

    DEBUG_LOG_INFO("telephonyService_CallStateNotificationMessageHandler enum:telephony_domain_messages:%d enum:voice_source_t:%d lock 0x%04x", id, msg->voice_source, telephony_service.command_lock);

    switch(id)
    {
        case TELEPHONY_CONNECTED:
            telephonyService_handleTelephonyConnected((const TELEPHONY_AUDIO_CONNECTED_T*)message);
        break;

        case TELEPHONY_AUDIO_CONNECTED:
            telephonyService_handleTelephonyAudioConnected((const TELEPHONY_AUDIO_CONNECTED_T*)message);
            telephonyService_ClearAudioTransferLock(msg->voice_source);
        break;

        case TELEPHONY_AUDIO_DISCONNECTED:
            telephonyService_handleTelephonyAudioDisconnected((const TELEPHONY_AUDIO_DISCONNECTED_T*)message);
            telephonyService_ClearAudioTransferLock(msg->voice_source);
        break;

        case TELEPHONY_AUDIO_CONNECTING:
            AudioRouter_Update();
        break;

        case TELEPHONY_CALL_ENDED:
            TelephonyService_ResumeHighestPriorityHeldCallRemaining();
            telephonyService_RefreshCallStates(msg->voice_source);
        break;

        case TELEPHONY_CALLER_ID_NOTIFICATION:
            /* Caller ID notfication can be handled here. */
        break;
        
        case TELEPHONY_ERROR:
            telephonyService_ClearStateChangeLock(msg->voice_source);
        break;
        
        case TELEPHONY_DISCONNECTED:
        case TELEPHONY_LINK_LOSS_OCCURRED:
            telephonyService_RefreshCallStates(msg->voice_source);
            telephonyService_ClearStateChangeLock(msg->voice_source);
            telephonyService_ClearAudioTransferLock(msg->voice_source);
        break;
        
        case TELEPHONY_TIMEOUT:
            telephonyService_ClearAllLocks();
        break;
        
        default:
            handled = FALSE;
        break;
    }

    if (TelephonyMessage_IsCallStateChange(id))
    {
        telephonyService_ClearStateChangeLock(msg->voice_source);
        AudioRouter_Update();
        handled = TRUE;
    }

    if (!handled)
    {
        DEBUG_LOG_VERBOSE("telephonyService_CallStateNotificationMessageHandler: Unhandled event MESSAGE:0x%x", id);
    }
}

static void telephonyService_HandleUiInput(Task task, MessageId ui_input, Message message)
{
    UNUSED(task);
    UNUSED(message);

    voice_source_t source = voice_source_none;
    
    /* Delay UI input handling if a command is already in progress */
    if(telephony_service.command_lock)
    {
        DEBUG_LOG_INFO("telephonyService_HandleUiInput locked 0x%04x", telephony_service.command_lock);
        MessageSendConditionally(task, ui_input, message, &telephony_service.command_lock);
        return;
    }

    Focus_GetVoiceSourceForUiInput(ui_input, &source);
    
    DEBUG_LOG_INFO("telephonyService_HandleUiInput enum:ui_input_t:%d enum:voice_source_t:%d", ui_input, source);

    if(source == voice_source_none)
    {
        return;
    }

    switch(ui_input)
    {
        case ui_input_voice_call_hang_up:
            TelephonyService_HangUpCall(source);
        break;

        case ui_input_voice_call_accept:
            TelephonyService_AnswerCall(source);
        break;

        case ui_input_voice_call_reject:
            TelephonyService_RejectCall(source);
        break;

        case ui_input_voice_transfer:
            TelephonyService_TransferAudioToggle(source);
        break;

        case ui_input_voice_transfer_to_ag:
            TelephonyService_TransferAudioToHandset(source);
        break;

        case ui_input_voice_transfer_to_headset:
            TelephonyService_TransferAudioToHeadset(source);
        break; 

        case ui_input_voice_dial:
            VoiceSources_InitiateVoiceDial(source);
        break;

        case ui_input_voice_call_last_dialed:
            VoiceSources_InitiateCallLastDialled(source);
        break;

        case ui_input_mic_mute_toggle:
            VoiceSources_ToggleMicrophoneMute(source);
        break;
        
        case ui_input_voice_call_cycle:
            TelephonyService_CycleToNextCall(source);
        break;
        
        case ui_input_voice_call_join_calls:
            TelephonyService_JoinCalls(source, telephony_join_calls_and_stay);
        break;
        
        case ui_input_voice_call_join_calls_and_hang_up:
            TelephonyService_JoinCalls(source, telephony_join_calls_and_leave);
        break;

        default:
        break;
    }
}

static unsigned telephonyService_AddSourceContextToInCall(voice_source_t source)
{
    switch(VoiceSources_GetSourceContext(source))
    {
        case context_voice_ringing_incoming:
        case context_voice_in_call_with_incoming:
            return context_voice_in_call_with_incoming;
        
        case context_voice_ringing_outgoing:
        case context_voice_in_call_with_outgoing:
            return context_voice_in_call_with_outgoing;
        
        case context_voice_call_held:
        case context_voice_in_call_with_held:
            return context_voice_in_call_with_held;
        
        default:
            return context_voice_in_call;
    }
}

static unsigned telephonyService_GetMultiCallContext(unsigned focus_context)
{
    voice_source_t background_source = voice_source_none;
    
    switch(focus_context)
    {
        case context_voice_ringing_incoming:
        {
            if(TelephonyService_FindActiveCall(&background_source))
            {
                return context_voice_in_call_with_incoming;
            }
        }
        break;
        
        case context_voice_ringing_outgoing:
        {
            if(TelephonyService_FindActiveCall(&background_source))
            {
                return context_voice_in_call_with_outgoing;
            }
        }
        break;
        
        case context_voice_in_call:
        {
            if(TelephonyService_FindIncomingOutgoingOrHeldCall(&background_source))
            {
                return telephonyService_AddSourceContextToInCall(background_source);
            }
        }
        break;
        
        default:
        break;
    }
    
    return focus_context;
}

static unsigned telephonyService_GetContext(void)
{
    unsigned context = context_voice_disconnected;
    voice_source_t focused_source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        context = VoiceSources_GetSourceContext(focused_source);
    }
    
    context = telephonyService_GetMultiCallContext(context);

    return context;
}

bool TelephonyService_Init(Task init_task)
{
    UNUSED(init_task);
    
    telephony_service.caller_id_enabled = FALSE;
    telephonyService_ClearAllLocks();
    
    Telephony_RegisterForMessages((Task)&telephony_message_handler_task);

    Ui_RegisterUiProvider(ui_provider_telephony, telephonyService_GetContext);

    Ui_RegisterUiInputConsumer((Task)&ui_handler_task, (uint16*)ui_inputs, sizeof(ui_inputs)/sizeof(uint16));

#ifdef INCLUDE_UI_USER_CONFIG
    UiUserConfig_RegisterContextIdMap(ui_provider_telephony, telephony_map, ARRAY_DIM(telephony_map));
#endif

    UsbAudio_ClientRegister((Task)&telephony_message_handler_task,
                              USB_AUDIO_REGISTERED_CLIENT_TELEPHONY);
    return TRUE;
}

void TelephonyService_ConfigureCallerIdNotifications(bool enable_caller_id)
{
    telephony_service.caller_id_enabled = enable_caller_id;
}
