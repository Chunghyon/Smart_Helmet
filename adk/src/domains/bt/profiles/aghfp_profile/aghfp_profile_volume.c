/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   aghfp_profile
\brief      The voice source volume interface implementation for HFP sources
*/

#include "aghfp_profile_volume.h"
#include "aghfp.h"

#include "aghfp_profile.h"
#include "aghfp_profile_instance.h"
#include "voice_sources.h"
#include "voice_sources_list.h"
#include "volume_messages.h"
#include "volume_utils.h"
#include "volume_types.h"
#include "aghfp_profile_abstraction.h"
#include <logging.h>

#define AGHFP_VOLUME_MIN      0
#define AGHFP_VOLUME_MAX      15
#define AGHFP_VOLUME_CONFIG   { .range = { .min = AGHFP_VOLUME_MIN, .max = AGHFP_VOLUME_MAX }, .number_of_steps = ((AGHFP_VOLUME_MAX - AGHFP_VOLUME_MIN) + 1) }
#define AGHFP_VOLUME(step)    { .config = AGHFP_VOLUME_CONFIG, .value = step }
#define AGHFP_SPEAKER_GAIN    (10)

static volume_t aghfpProfile_GetVolume(voice_source_t source);
static void aghfpProfile_SetVolume(voice_source_t source, volume_t volume);
static mute_state_t aghfpProfile_GetMuteState(voice_source_t source);
static void aghfpProfile_SetMuteState(voice_source_t source, mute_state_t state);

static const voice_source_volume_interface_t aghfp_volume_interface =
{
    .GetVolume = aghfpProfile_GetVolume,
    .SetVolume = aghfpProfile_SetVolume,
    .GetMuteState = aghfpProfile_GetMuteState,
    .SetMuteState = aghfpProfile_SetMuteState
};

static mute_state_t aghfpProfile_GetMuteState(voice_source_t source)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_GetMuteState enum:voice_source_t:%d", source);

    /* Set to a default volume */
    mute_state_t state = unmute;

    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if (instance != NULL)
    {
        state = instance->bitfields.mic_mute ? mute : unmute;
    }
    else
    {
        DEBUG_LOG_INFO("aghfpProfile_GetMuteState: No instance available");
    }

    return state;
}

static void aghfpProfile_SetMuteState(voice_source_t source, mute_state_t state)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_SetMuteState enum:voice_source_t:%d %d",
                      source, state);
    uint8 gain ;
    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if (instance != NULL)
    {
        if (instance->bitfields.mic_mute != state == mute)
        {
            instance->bitfields.mic_mute = state == mute;
            gain = instance->bitfields.mic_mute ? 0 : instance->mic_gain;
            AghfpProfileAbstract_SetRemoteMicrophoneGain(instance, gain);
        }
    }
    else
    {
        DEBUG_LOG_INFO("aghfpProfile_SetMuteState: No instance available");
    }
}


static volume_t aghfpProfile_GetVolume(voice_source_t source)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_GetVolume enum:voice_source_t:%d", source);

    /* Set to a default volume */
    volume_t volume = AGHFP_VOLUME(AGHFP_SPEAKER_GAIN);

    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if (instance != NULL)
    {
        volume.value = instance->speaker_volume;
    }
    else
    {
        DEBUG_LOG_INFO("aghfpProfile_GetVolume: No instance available");
    }

    return volume;
}

static void aghfpProfile_SetVolume(voice_source_t source, volume_t volume)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_SetVolume enum:voice_source_t:%d %d",
                      source, volume.value);

    aghfpInstanceTaskData * instance = AghfpProfileInstance_GetInstanceForSource(source);

    if (instance != NULL)
    {
        if (instance->speaker_volume != volume.value)
        {
            instance->speaker_volume = volume.value;
            AghfpProfileAbstract_SetRemoteSpeakerVolume(instance, volume.value);
        }
    }
    else
    {
        DEBUG_LOG_INFO("aghfpProfile_SetVolume: No instance available");
    }
}

const voice_source_volume_interface_t * AghfpProfile_GetVoiceSourceVolumeInterface(void)
{
    return &aghfp_volume_interface;
}
