/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "audio_info.h"
#include "audio_sources.h"
#include "bt_device.h"
#include "context_framework.h"
#include "source_param_types.h"
#include "voice_sources.h"
#include <csrtypes.h>
#include <panic.h>
#include <stdio.h>

static device_t audioInfo_GetDeviceForSource(generic_source_t source)
{
    device_t device = NULL;
    if(source.type == source_type_audio)
    {
        device = AudioSources_GetAudioSourceDevice(source.u.audio);
    }
    else if(source.type == source_type_voice)
    {
        // currently no way of retrieving this through the voice sources interface
        device = BtDevice_GetMruDevice();
    }
    return device;
}

static bool audioInfo_DoesSourceHaveControlChannel(generic_source_t source)
{
    bool has_control_channel = FALSE;
    if(source.type == source_type_audio)
    {
        has_control_channel = (bool)(AudioSources_GetMediaControlSourceDevice(source.u.audio) != NULL);
    }
    else if(source.type == source_type_voice)
    {
        has_control_channel = VoiceSources_IsSourceRegisteredForTelephonyControl(source.u.voice);
    }
    return has_control_channel;
}

static bool audioInfo_GetActiveSourceInfo(unsigned * context_data, uint8 context_data_size)
{
    PanicZero(context_data_size >= sizeof(context_active_source_info_t));
    memset(context_data, 0, sizeof(context_active_source_info_t));
    generic_source_t source = AudioInfo_GetRoutedGenericSource();
    ((context_active_source_info_t *)context_data)->active_source = source;
    ((context_active_source_info_t *)context_data)->handset = audioInfo_GetDeviceForSource(source);
    ((context_active_source_info_t *)context_data)->has_control_channel = audioInfo_DoesSourceHaveControlChannel(source);

    return TRUE;
}

generic_source_t AudioInfo_GetRoutedGenericSource(void)
{
    generic_source_t source = {.type = source_type_invalid };

    if(VoiceSources_IsAnyVoiceSourceRouted())
    {
        source.type = source_type_voice;
        source.u.voice = VoiceSources_GetRoutedSource();
    }
    else
    {
        audio_source_t audio_source = AudioSources_GetRoutedSource();
        if(audio_source != audio_source_none)
        {
            source.type = source_type_audio;
            source.u.audio = audio_source;
        }
    }
    return source;
}

void AudioInfo_Init(void)
{
    ContextFramework_RegisterContextProvider(context_active_source_info, audioInfo_GetActiveSourceInfo);
}

