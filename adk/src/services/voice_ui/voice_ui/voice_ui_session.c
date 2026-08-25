/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice Assistant Session state
*/

#include "voice_ui_session.h"
#include "voice_ui.h"
#include "voice_ui_container.h"
#include "voice_ui_va_client_if.h"
#include "logging.h"
#include "audio_router.h"
#include <panic.h>

#define VA_SESSION_ENDING_TIMEOUT_MS 2000

static void voiceUi_OverrideSourceContextIfNecessary(audio_source_t source, unsigned *context);

const media_context_override_interface_t media_context_override_if =
{
    .ContextOverride = voiceUi_OverrideSourceContextIfNecessary
};


typedef enum
{
    VA_SESSION_ENDING_TIMEOUT
} va_session_internal_message_ids_t;

static bool va_session_in_progress = FALSE;
static bool va_session_ending = FALSE;

static void voiceUi_VaSessionInternalMsgHandler(Task task, MessageId id, Message message);
static const TaskData msg_handler = { voiceUi_VaSessionInternalMsgHandler };

static void voiceUi_VaSessionInternalMsgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (id == VA_SESSION_ENDING_TIMEOUT)
    {
        DEBUG_LOG_DEBUG("VA_SESSION_ENDING_TIMEOUT");
        va_session_ending = FALSE;
        AudioRouter_Update();
    }
}

static void voiceUi_StartVaSessionEndInProgressTimer(void)
{
    DEBUG_LOG_DEBUG("voiceUi_StartVaSessionEndInProgressTimer");
    va_session_ending = TRUE;
    MessageCancelAll((Task) &msg_handler, VA_SESSION_ENDING_TIMEOUT);
    MessageSendLater((Task)&msg_handler, VA_SESSION_ENDING_TIMEOUT, NULL, VA_SESSION_ENDING_TIMEOUT_MS);
}

static const bdaddr * voiceUi_GetVaSourceBdAddress(void)
{
    const voice_ui_handle_t * va = VoiceUi_GetActiveVa();

    if (va)
    {
        return va->voice_assistant->GetBtAddress();
    }

    return NULL;
}

static void voiceUi_OverrideSourceContextIfNecessary(audio_source_t source, unsigned *context)
{
    bool status = FALSE;

    if (va_session_in_progress && (source == VoiceUi_GetVaAudioSource(source)))
    {
        *context = context_audio_is_va_response;
        status = TRUE;
    }

    if ((status == FALSE) && AudioSource_IsA2dp(source))
    {
        if(va_session_ending && (*context == context_audio_is_playing))
        {
            /* This is to cater the case where va session has ended but a2dp stream for this audio source is still
             * in playing state as AG is streaming silence.So we need to move audio router context to streaming from
             * playing so if hfp call is invoked just after va session then this audio source will not be marked
             * as interuppted which is actually the case as it is not playing anything.Av module will take care of
             * further context movement. */

            *context = context_audio_is_streaming;
            status = TRUE;
        }
    }

    if (status)
    {
        DEBUG_LOG_DEBUG("voiceUi_OverrideSourceContextIfNecessary: enum:audio_source_t:%d "
                        "set context as enum:audio_source_provider_context_t:%d", source, *context);
    }
}

void VoiceUi_VaSessionStarted(voice_ui_handle_t* va_handle)
{
    DEBUG_LOG("VoiceUi_VaSessionStarted");
    if (VoiceUi_IsActiveAssistant(va_handle) && (va_session_in_progress != TRUE))
    {
        va_session_in_progress = TRUE;
        va_session_ending = FALSE;
        AudioRouter_Update();
    }
}

void VoiceUi_VaSessionEnded(voice_ui_handle_t* va_handle)
{
    DEBUG_LOG("VoiceUi_VaSessionEnded");
    if (VoiceUi_IsActiveAssistant(va_handle))
    {
        voiceUi_StartVaSessionEndInProgressTimer();
        VoiceUi_VaSessionReset();
        AudioRouter_Update();
    }
}

bool VoiceUi_IsSessionInProgress(void)
{
    return va_session_in_progress;
}

void VoiceUi_VaSessionReset(void)
{
    va_session_in_progress = FALSE;
}

audio_source_t VoiceUi_GetVaAudioSource(audio_source_t source)
{
    const bdaddr *va_addr = voiceUi_GetVaSourceBdAddress();
    device_t audio_source_device = AudioSources_GetAudioSourceDevice(source);
    
    if ((va_addr != NULL) && (audio_source_device != NULL))
    {
        device_t va_device = BtDevice_GetDeviceForBdAddr(va_addr);

        if ((va_device != NULL) &&
            (audio_source_device == va_device))
        {
            DEBUG_LOG("VoiceUi_GetVaAudioSource enum:audio_source_t:%u", source);
            return source;
        }
    }

    return audio_source_none;
}

void VoiceUi_VaSessionInit(void)
{
    PanicFalse(AudioSources_RegisterMediaControlContextOverrideInterface(&media_context_override_if));
}
