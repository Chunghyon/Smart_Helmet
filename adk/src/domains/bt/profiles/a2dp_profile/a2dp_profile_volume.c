/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   a2dp_profile
\brief      The audio source volume interface implementation for A2DP sources
*/

#include "a2dp_profile_volume.h"

#include "a2dp_profile.h"
#include "a2dp_profile_config.h"
#include "av.h"
#include "av_instance.h"
#include "audio_sources_list.h"
#include "kymera_config.h"
#include "volume_types.h"
#include "logging.h"

#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <mirror_profile.h>

#define A2DP_VOLUME_MIN      0
#define A2DP_VOLUME_MAX      127
#define A2DP_VOLUME_STEPS    128
#define A2DP_VOLUME_CONFIG   { .range = { .min = A2DP_VOLUME_MIN, .max = A2DP_VOLUME_MAX }, .number_of_steps = A2DP_VOLUME_STEPS }
#define A2DP_VOLUME(step)    { .config = A2DP_VOLUME_CONFIG, .value = step }

static volume_t a2dpProfile_GetVolume(audio_source_t source);
static void a2dpProfile_SetVolume(audio_source_t source, volume_t volume);

static const audio_source_volume_interface_t a2dp_volume_interface =
{
    .GetVolume = a2dpProfile_GetVolume,
    .SetVolume = a2dpProfile_SetVolume
};

/*! Get the device for the specified instance (if any), or the mirrored device */
static device_t a2dpProfile_GetDevice(audio_source_t source)
{
    avInstanceTaskData * theInst = AvInstance_GetSinkInstanceForAudioSource(source);
    device_t device=NULL;

    if (theInst)
    {
        device = BtDevice_GetDeviceForBdAddr(&theInst->bd_addr);
    }
    else if (MirrorProfile_IsBredrMirroringConnected())
    {
        bdaddr * mirror_addr = NULL;
        mirror_addr = MirrorProfile_GetMirroredDeviceAddress();
        device = BtDevice_GetDeviceForBdAddr(mirror_addr);
    }

    return device;
}

static volume_t a2dpProfile_GetVolume(audio_source_t source)
{
    volume_t volume = A2DP_VOLUME(A2DP_VOLUME_MIN);
    device_t device;

    device = a2dpProfile_GetDevice(source);

    if (!DeviceProperties_GetAudioVolume(device, volume.config, &volume))
    {
        /* If the audio volume couldn't be read from the device properties
           fallback to the default A2DP volume. */
        volume = A2dpProfile_GetDefaultVolume();
        DEBUG_LOG("a2dpProfile_GetVolume Using default volume. device:%p volume:%d",
                   device, volume.value);
    }
    else
    {
        DEBUG_LOG("a2dpProfile_GetVolume from device:%p volume:%d", device, volume.value);
    }

    return volume;
}

static void a2dpProfile_SetVolume(audio_source_t source, volume_t volume)
{
    device_t device;

    device = a2dpProfile_GetDevice(source);

    DEBUG_LOG("a2dpProfile_SetVolume enum:audio_source_t:%d volume:%d device:%p",
               source, volume.value, device);

    DeviceProperties_SetAudioVolume(device, volume);
}

const audio_source_volume_interface_t * A2dpProfile_GetAudioSourceVolumeInterface(void)
{
    return &a2dp_volume_interface;
}

volume_t A2dpProfile_GetDefaultVolume(void)
{
    /* Set default volume as set in configuration */
    const int rangeDb = appConfigMaxVolumedB() - appConfigMinVolumedB();
    const uint8 volume = (uint8)((appConfigDefaultVolumedB() - appConfigMinVolumedB()) * A2DP_VOLUME_MAX / rangeDb);
    volume_t default_volume = A2DP_VOLUME(volume);
    return default_volume;
}
