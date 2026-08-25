/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Telephony Service Call Control
*/

#include "telephony_service.h"
#include "telephony_service_call_control.h"
#include "telephony_service_private.h"
#include <telephony_messages.h>

#include <voice_sources.h>

#include <focus_voice_source.h>
#include <panic.h>
#include <logging.h>

#include <stdlib.h>

static const unsigned held_remaining_contexts[] = 
{
    context_voice_call_held
};

static const unsigned active_contexts[] = 
{
    context_voice_in_call,
    context_voice_in_call_with_incoming,
    context_voice_in_call_with_outgoing,
    context_voice_in_call_with_held,
    context_voice_in_multiparty_call
};

static const unsigned incoming_outgoing_or_held_contexts[] = 
{
    context_voice_ringing_incoming,
    context_voice_ringing_outgoing,
    context_voice_in_call_with_incoming,
    context_voice_in_call_with_outgoing,
    context_voice_in_call_with_held,
    context_voice_call_held
};

typedef enum
{
    telephony_service_accept,
    telephony_service_reject,
    telephony_service_hang_up,
    telephony_service_hold_active,
    telephony_service_resume_held,
    telephony_service_release_held_reject_waiting,
    telephony_service_release_active_accept_other,
    telephony_service_hold_active_accept_other,
    telephony_service_add_held_to_multiparty,
    telephony_service_join_calls_and_hang_up,
    telephony_service_transfer_to_ag,
    telephony_service_transfer_to_hf,
    telephony_service_transfer_toggle
} telephony_service_call_control_t;

static void telephonyService_RestartTimeout(voice_source_t source)
{
    telephony_message_t * message;
    
    MessageCancelFirst((Task)&telephony_message_handler_task, TELEPHONY_TIMEOUT);
    
    message = (telephony_message_t *)PanicNull(calloc(1, sizeof(telephony_message_t)));
    message->voice_source = source;
    MessageSendLater((Task)&telephony_message_handler_task, TELEPHONY_TIMEOUT, message, D_SEC(5));
}

static void telephonyService_LockCommandsUntilAudioTransfer(voice_source_t source)
{
    telephonyService_SetAudioTransferLock(source);
    telephonyService_RestartTimeout(source);
}

static void telephonyService_LockCommandsUntilStateChange(voice_source_t source)
{
    telephonyService_SetStateChangeLock(source);
    telephonyService_RestartTimeout(source);
}

static void telephonyService_CallControl(voice_source_t source, telephony_service_call_control_t control)
{
    DEBUG_LOG("telephonyService_CallControl enum:voice_source_t:%d enum:telephony_service_call_control_t:%d", source, control);
    
    switch(control)
    {
        case telephony_service_accept:
            VoiceSources_AcceptIncomingCall(source);
            telephonyService_LockCommandsUntilStateChange(source);
        break;
    
        case telephony_service_reject:
            VoiceSources_RejectIncomingCall(source);
            telephonyService_LockCommandsUntilStateChange(source);
        break;
    
        case telephony_service_hang_up:
            VoiceSources_TerminateOngoingCall(source);
            telephonyService_LockCommandsUntilStateChange(source);
        break;
        
        case telephony_service_hold_active:
        case telephony_service_resume_held:
            VoiceSources_TwcControl(source, voice_source_hold_active_accept_other);
            telephonyService_LockCommandsUntilStateChange(source);
        break;
    
        case telephony_service_release_held_reject_waiting:
            VoiceSources_TwcControl(source, voice_source_release_held_reject_waiting);
            telephonyService_LockCommandsUntilStateChange(source);
        break;
    
        case telephony_service_release_active_accept_other:
            VoiceSources_TwcControl(source, voice_source_release_active_accept_other);
            telephonyService_LockCommandsUntilStateChange(source);
        break;
    
        case telephony_service_hold_active_accept_other:
            VoiceSources_TwcControl(source, voice_source_hold_active_accept_other);
        break;
    
        case telephony_service_add_held_to_multiparty:
            VoiceSources_TwcControl(source, voice_source_add_held_to_multiparty);
        break;
    
        case telephony_service_join_calls_and_hang_up:
            VoiceSources_TwcControl(source, voice_source_join_calls_and_hang_up);
        break;
        
        case telephony_service_transfer_to_ag:
            if(VoiceSources_IsVoiceChannelAvailable(source))
            {
                VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_ag);
                telephonyService_LockCommandsUntilAudioTransfer(source);
            }
        break;
        
        case telephony_service_transfer_to_hf:
            if(!VoiceSources_IsVoiceChannelAvailable(source))
            {
                VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_to_hfp);
                telephonyService_LockCommandsUntilAudioTransfer(source);
            }
        break;
        
        case telephony_service_transfer_toggle:
            VoiceSources_TransferOngoingCallAudio(source, voice_source_audio_transfer_toggle);
            telephonyService_LockCommandsUntilAudioTransfer(source);
        break;
    }
}

static bool telephonyService_FindHeldCallRemainingExcludingSource(voice_source_t* source, voice_source_t source_to_exclude)
{
    *source = source_to_exclude;
    return Focus_GetVoiceSourceInContexts(ui_provider_telephony, source, held_remaining_contexts);
}

static bool telephonyService_FindHeldCallRemaining(voice_source_t* source)
{
    return telephonyService_FindHeldCallRemainingExcludingSource(source, voice_source_none);
}

static bool telephonyService_FindActiveCallExcludingSource(voice_source_t* source, voice_source_t source_to_exclude)
{
    *source = source_to_exclude;
    return Focus_GetVoiceSourceInContexts(ui_provider_telephony, source, active_contexts);
}

bool TelephonyService_FindActiveCall(voice_source_t* source)
{
    return telephonyService_FindActiveCallExcludingSource(source, voice_source_none);
}

bool TelephonyService_FindIncomingOutgoingOrHeldCall(voice_source_t* source)
{
    *source = voice_source_none;
    return Focus_GetVoiceSourceInContexts(ui_provider_telephony, source, incoming_outgoing_or_held_contexts);
}

static void telephonyService_HangUpActiveCallOnOtherSource(voice_source_t source_to_exclude)
{
    voice_source_t source;
    
    if(telephonyService_FindActiveCallExcludingSource(&source, source_to_exclude))
    {
        telephonyService_CallControl(source, telephony_service_transfer_to_ag);
        TelephonyService_HangUpCall(source);
    }
}

static void telephonyService_HoldActiveCallOnOtherSource(voice_source_t source_to_exclude)
{
    voice_source_t source;
    
    if(telephonyService_FindActiveCallExcludingSource(&source, source_to_exclude))
    {
        TelephonyService_HoldCall(source);
    }
}

static void telephonyService_ResumeHeldCallOnOtherSource(voice_source_t source_to_exclude)
{
    voice_source_t source;
    
    if(telephonyService_FindHeldCallRemainingExcludingSource(&source, source_to_exclude))
    {
        TelephonyService_ResumeCall(source);
    }
    else if(telephonyService_FindActiveCallExcludingSource(&source, source_to_exclude))
    {
        /* If a previous call hold command failed we may need to resume an already active call */
        DEBUG_LOG("telephonyService_ResumeHeldCallOnOtherSource transfer enum:voice_source_t:%d to HF", source);
        telephonyService_CallControl(source, telephony_service_transfer_to_hf);
    }
}

void TelephonyService_ResumeHighestPriorityHeldCallRemaining(void)
{
    voice_source_t source;
    if(telephonyService_FindHeldCallRemaining(&source))
    {
        DEBUG_LOG("telephonyService_ResumeHighestPriorityHeldCallRemaining enum:voice_source_t:%d",source);
        TelephonyService_ResumeCall(source);
    }
    else
    {
        DEBUG_LOG("telephonyService_ResumeHighestPriorityHeldCallRemaining no held calls remaining");
    }
}

void TelephonyService_HangUpCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_ringing_outgoing:
        case context_voice_in_call:
        case context_voice_in_multiparty_call:
            telephonyService_CallControl(source, telephony_service_hang_up);
        break;
        
        case context_voice_in_call_with_outgoing:
        case context_voice_in_call_with_held:
            telephonyService_CallControl(source, telephony_service_release_active_accept_other);
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_HangUpCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_AnswerCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    
    telephonyService_HangUpActiveCallOnOtherSource(source);
    
    switch(context)
    {
        case context_voice_ringing_incoming:
            telephonyService_CallControl(source, telephony_service_accept);
        break;
        
        case context_voice_in_call_with_incoming:
        case context_voice_in_multiparty_call:
            telephonyService_CallControl(source, telephony_service_release_active_accept_other);
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_AnswerCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_RejectCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_ringing_incoming:
            telephonyService_CallControl(source, telephony_service_reject);
        break;
        
        case context_voice_in_call_with_incoming:
        case context_voice_in_call_with_held:
        case context_voice_call_held:
        case context_voice_in_multiparty_call:
            telephonyService_CallControl(source, telephony_service_release_held_reject_waiting);
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_RejectCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_CycleToNextCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    
    switch(context)
    {
        case context_voice_ringing_incoming:
            telephonyService_HoldActiveCallOnOtherSource(source);
            telephonyService_CallControl(source, telephony_service_accept);
        break;
        
        case context_voice_in_call_with_incoming:
        case context_voice_in_call_with_outgoing:
        case context_voice_in_call_with_held:
        case context_voice_in_multiparty_call:
            /* Two calls on one handset, use TWC as normal */
            telephonyService_CallControl(source, telephony_service_hold_active_accept_other);
        break;
        
        case context_voice_call_held:
            /* One call on each handset, hold active and resume held */
            telephonyService_HoldActiveCallOnOtherSource(source);
            TelephonyService_ResumeCall(source);
        break;
        
        case context_voice_in_call:
            /* One call on each handset, hold active and resume held */
            TelephonyService_HoldCall(source);
            telephonyService_ResumeHeldCallOnOtherSource(source);
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_CycleToNextCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_JoinCalls(voice_source_t source, telephony_join_calls_action_t action)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_in_call_with_incoming:
        case context_voice_in_call_with_outgoing:
        case context_voice_in_call_with_held:
        case context_voice_in_multiparty_call:
            if(action == telephony_join_calls_and_leave)
            {
                telephonyService_CallControl(source, telephony_service_join_calls_and_hang_up);
            }
            else
            {
                telephonyService_CallControl(source, telephony_service_add_held_to_multiparty);
            }
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_JoinCalls enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_HoldCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_in_call:
            telephonyService_CallControl(source, telephony_service_transfer_to_ag);
            telephonyService_CallControl(source, telephony_service_hold_active);
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_HoldCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_ResumeCall(voice_source_t source)
{
    unsigned context = VoiceSources_GetSourceContext(source);
    switch(context)
    {
        case context_voice_call_held:
            telephonyService_CallControl(source, telephony_service_transfer_to_hf);
            telephonyService_CallControl(source, telephony_service_resume_held);
        break;
        
        default:
            DEBUG_LOG_INFO("TelephonyService_ResumeCall enum:voice_source_t:%d in unexpected context enum:voice_source_provider_context_t:%d", source, context);
        break;
    }
}

void TelephonyService_TransferAudioToHandset(voice_source_t source)
{
    telephonyService_CallControl(source, telephony_service_transfer_to_ag);
}

void TelephonyService_TransferAudioToHeadset(voice_source_t source)
{
    telephonyService_CallControl(source, telephony_service_transfer_to_hf);
}

void TelephonyService_TransferAudioToggle(voice_source_t source)
{
    telephonyService_CallControl(source, telephony_service_transfer_toggle);
}
