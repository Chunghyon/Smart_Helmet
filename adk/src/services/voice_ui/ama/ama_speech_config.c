/*!
\copyright  Copyright (c) 2018- 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_speech_config.c
\brief  Implementation of the APIs to manage the AMA speech configuration
*/

#ifdef INCLUDE_AMA
#include "speech.pb-c.h"
#include "ama_speech_config.h"
#include "voice_ui_va_client_if.h"

typedef struct{
    SpeechInitiator__Type initiator;
    uint32 pre_roll;
    /* start timestamp for trigger phrase of buffered data */
    uint32 start_timestamp;
    /* end timestamp for trigger phrase of buffered data */
    uint32 end_timestamp;
}ama_speech_msg_start_t;

typedef struct{
    SpeechInitiator__Type speech_initiator;
    AudioProfile audio_profile;
    AudioFormat audio_format;
    AudioSource audio_source;
    uint32 new_dialog_id;
    uint32 current_dialog_id;
}speech_settings_t;

#define MAX_START_DIALOG_ID 0x7fffffff

static speech_settings_t speech_settings = {
    AMA_SPEECH_INITIATOR_DEFAULT,
    AMA_SPEECH_AUDIO_PROFILE_DEFAULT,
    AMA_SPEECH_AUDIO_FORMAT_DEFAULT,
    AMA_SPEECH_AUDIO_SOURCE_DEFAULT,
    MAX_START_DIALOG_ID,
    0
};

void Ama_NewSpeechDialogId(void)
{
    if(speech_settings.new_dialog_id == MAX_START_DIALOG_ID)
    {
        speech_settings.new_dialog_id = 0;
    }
    else
    {
        speech_settings.new_dialog_id++;
    }
    speech_settings.current_dialog_id = speech_settings.new_dialog_id;
}

void Ama_SetSpeechAudioSource(AudioSource source)
{
    speech_settings.audio_source = source;
}

void Ama_SetSpeechAudioProfile(AudioProfile profile)
{
    speech_settings.audio_profile = profile;
}

void Ama_SetSpeechInitiator(SpeechInitiator__Type initiator)
{
    speech_settings.speech_initiator = initiator;
}

void Ama_SetSpeechAudioFormat(AudioFormat format)
{
    speech_settings.audio_format = format;
}

SpeechInitiator__Type Ama_GetSpeechInitiatorType(void)
{
    return speech_settings.speech_initiator;
}

AudioSource Ama_GetSpeechAudioSource(void)
{
    return speech_settings.audio_source;
}

AudioFormat Ama_GetSpeechAudioFormat(void)
{
    return speech_settings.audio_format;
}

AudioProfile Ama_GetSpeechAudioProfile(void)
{
    return speech_settings.audio_profile;
}

uint32 Ama_GetCurrentSpeechDialogId(void)
{
    return speech_settings.current_dialog_id;
}

void Ama_UpdateSpeechDialogId(uint32 provided_id)
{
    if(provided_id > MAX_START_DIALOG_ID)
    {
        speech_settings.current_dialog_id = provided_id;
    }
}

void Ama_SetSpeechSettingsToDefault(void)
{
    Ama_SetSpeechAudioSource(AMA_SPEECH_AUDIO_SOURCE_DEFAULT);
    Ama_SetSpeechAudioFormat(AMA_SPEECH_AUDIO_FORMAT_DEFAULT);
    Ama_SetSpeechAudioProfile(AMA_SPEECH_AUDIO_PROFILE_DEFAULT);
    Ama_SetSpeechInitiator(AMA_SPEECH_INITIATOR_DEFAULT);
    speech_settings.new_dialog_id = MAX_START_DIALOG_ID;
    speech_settings.current_dialog_id = 0;
}

bool Ama_IsWakeUpWordDetectionAllowedToStart(void)
{
    return (!VoiceUi_IsPrivacyModeEnabled()) && VoiceUi_IsWakeUpWordDetectionEnabled();
}

bool Ama_IsLiveCaptureAllowedToStart(void)
{
    bool allowed_to_start = TRUE;

    if(PRIVACY_MODE_BLOCKS_LIVE_CAPTURE)
    {
        allowed_to_start = !VoiceUi_IsPrivacyModeEnabled();
    }

    return allowed_to_start;
}


#endif /* INCLUDE_AMA */
