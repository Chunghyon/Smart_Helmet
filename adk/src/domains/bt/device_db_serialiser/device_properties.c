/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Contains helper functions for accessing the device properties data.
*/

#include "device_properties.h"

#include <bdaddr.h>
#include <csrtypes.h>
#include <device.h>
#include <panic.h>
#include <audio_sources_list.h>
#include <voice_sources_list.h>
#include <volume_utils.h>
#include <logging.h>


PRESERVE_TYPE_FOR_DEBUGGING(earbud_device_property_t)

bdaddr DeviceProperties_GetBdAddr(device_t device)
{
    void * object = NULL;
    size_t size = 0;
    PanicFalse(Device_GetProperty(device, device_property_bdaddr, &object, &size));
    PanicFalse(size==sizeof(bdaddr));
    return *((bdaddr *)object);
}

void DeviceProperties_SanitiseBdAddr(bdaddr *bd_addr)
{
    // Sanitise bluetooth address VMCSA-1007
    bdaddr sanitised_bdaddr = {0};
    sanitised_bdaddr.uap = bd_addr->uap;
    sanitised_bdaddr.lap = bd_addr->lap;
    sanitised_bdaddr.nap = bd_addr->nap;
    memcpy(bd_addr, &sanitised_bdaddr, sizeof(bdaddr));
}

void DeviceProperties_SetBdAddr(device_t device, bdaddr *bd_addr)
{
    DeviceProperties_SanitiseBdAddr((bdaddr *)bd_addr);
    Device_SetProperty(device, device_property_bdaddr, (void*)bd_addr, sizeof(bdaddr));
}

audio_source_t DeviceProperties_GetAudioSource(device_t device)
{
    audio_source_t source = audio_source_none;
    if (device)
    {
        audio_source_t *sourcep = NULL;
        size_t size;
        if (Device_GetProperty(device, device_property_audio_source, (void *)&sourcep, &size))
        {
            if (sizeof(audio_source_t) == size)
            {
                source = *sourcep;
            }
        }
    }
    return source;
}

voice_source_t DeviceProperties_GetVoiceSource(device_t device)
{
    voice_source_t source = voice_source_none;
    if (device)
    {
        voice_source_t *sourcep = NULL;
        size_t size;
        if (Device_GetProperty(device, device_property_voice_source, (void *)&sourcep, &size))
        {
            if (sizeof(voice_source_t) == size)
            {
                source = *sourcep;
            }
        }
    }
    return source;
}

voice_source_t DeviceProperties_GetLeVoiceSource(device_t device)
{
    voice_source_t source = voice_source_none;
    voice_source_t *sourcep = NULL;
    size_t size;

    if (device != NULL)
    {
        if (Device_GetProperty(device, device_property_le_voice_source, (void *)&sourcep, &size))
        {
            if (sizeof(voice_source_t) == size)
            {
                source = *sourcep;
            }
        }
    }

    return source;
}

void DeviceProperties_SetAudioSource(device_t device, audio_source_t source)
{
    Device_SetProperty(device, device_property_audio_source, &source, sizeof(audio_source_t));
}

void DeviceProperties_SetVoiceSource(device_t device, voice_source_t source)
{
    Device_SetProperty(device, device_property_voice_source, &source, sizeof(audio_source_t));
}

void DeviceProperties_SetLeVoiceSource(device_t device, voice_source_t source)
{
    Device_SetProperty(device, device_property_le_voice_source, &source, sizeof(voice_source_t));
}

void DeviceProperties_RemoveAudioSource(device_t device)
{
    Device_RemoveProperty(device, device_property_audio_source);
}

void DeviceProperties_RemoveVoiceSource(device_t device)
{
    Device_RemoveProperty(device, device_property_voice_source);
}

void DeviceProperties_RemoveLeVoiceSource(device_t device)
{
    Device_RemoveProperty(device, device_property_le_voice_source);
}

/*! \brief Volume config for the audio volume stored in the per-device property.

    When storing the audio volume it shall be converted from the profile
    volume config to this config. For example: converting the AVRCP absolute
    volume (0-127) to this one (0-255).
*/
#define DEVICE_AUDIO_VOLUME_CONFIG      { .range = { .min = 0, .max = 255 }, .number_of_steps = 256 }
#define DEVICE_AUDIO_VOLUME(volume)     { .config = DEVICE_AUDIO_VOLUME_CONFIG, .value = (volume) }

bool DeviceProperties_SetAudioVolume(device_t device, volume_t volume)
{
    bool was_set = FALSE;

    if (device)
    {
        volume_t audio_volume = DEVICE_AUDIO_VOLUME(0);
        audio_volume.value = VolumeUtils_ConvertToVolumeConfig(volume, audio_volume.config);
        was_set = Device_SetPropertyU8(device, device_property_audio_volume, audio_volume.value);

        DEBUG_LOG_VERBOSE("DeviceProperties_SetAudioVolume device 0x%x vol %d/%d device vol:%d was_set %d",
                          device, 
                          volume.value, volume.config.range.max, 
                          audio_volume.value, was_set);
    }

    return was_set;
}

bool DeviceProperties_GetAudioVolume(device_t device, volume_config_t config, volume_t *volume)
{
    bool got_volume = FALSE;
    volume_t audio_volume = DEVICE_AUDIO_VOLUME(0);

    if (device && Device_GetPropertyU8(device, device_property_audio_volume, (uint8 *)&audio_volume.value))
    {
        volume->config = config;
        volume->value = VolumeUtils_ConvertToVolumeConfig(audio_volume, volume->config);
        got_volume = TRUE;

        DEBUG_LOG_VERBOSE("DeviceProperties_GetAudioVolume device 0x%x vol %d/%d (device vol:%d)",
                          device,
                          volume->value, volume->config.range.max,
                          audio_volume.value);
    }
    else
    {
        DEBUG_LOG_VERBOSE("DeviceProperties_GetAudioVolume device 0x%x No volume", device);
    }

    return got_volume;
}

/*! \brief Volume config for the voice volume stored in the per-device property.

    When storing the voice volume it shall be converted from the profile
    volume config to this config. For example: converting the HFP volume
    (0-15) to this one (0-255).
*/
#define DEVICE_VOICE_VOLUME_CONFIG      { .range = { .min = 0, .max = 255 }, .number_of_steps = 256 }
#define DEVICE_VOICE_VOLUME(volume)     { .config = DEVICE_VOICE_VOLUME_CONFIG, .value = (volume) }

bool DeviceProperties_SetVoiceVolume(device_t device, volume_t volume)
{
    bool was_set = FALSE;

    if (device)
    {
        volume_t voice_volume = DEVICE_VOICE_VOLUME(0);
        voice_volume.value = VolumeUtils_ConvertToVolumeConfig(volume, voice_volume.config);
        was_set = Device_SetPropertyU8(device, device_property_voice_volume, voice_volume.value);

        DEBUG_LOG_VERBOSE("DeviceProperties_SetVoiceVolume device 0x%x vol %d/%d voice_vol %d",
                          device, 
                          volume.value, volume.config.range.max,
                          voice_volume.value);
    }

    return was_set;
}

bool DeviceProperties_GetVoiceVolume(device_t device, volume_config_t config, volume_t *volume)
{
    bool got_volume = FALSE;
    volume_t voice_volume = DEVICE_VOICE_VOLUME(0);

    if (device && Device_GetPropertyU8(device, device_property_voice_volume, (uint8 *)&voice_volume.value))
    {
        volume->config = config;
        volume->value = VolumeUtils_ConvertToVolumeConfig(voice_volume, volume->config);
        got_volume = TRUE;

        DEBUG_LOG_VERBOSE("DeviceProperties_GetVoiceVolume device 0x%x volume %d/%d voice_vol %d", 
                          device,
                          volume->value, volume->config.range.max,
                          voice_volume.value);
    }
    else
    {
        DEBUG_LOG_VERBOSE("DeviceProperties_GetVoiceVolume device 0x%x No volume",
                          device);
    }

    return got_volume;
}

void DeviceProperties_SetAudioStatus(device_t device, audio_status_t audio_status)
{
    if(device)
    {
        Device_SetPropertyU8(device, device_property_audio_status, audio_status);
    }
}

audio_status_t DeviceProperties_GetAudioStatus(device_t device)
{
    audio_status_t audio_status = audio_status_disconnected;
    if(device)
    {
        audio_status_t audio = audio_status_disconnected;
        if(Device_GetPropertyU8(device, device_property_audio_status, (uint8*)&audio))
        {
            audio_status = audio;
        }
    }

    return audio_status;
}

void DeviceProperties_SetHandsetContext(device_t device, handset_context_t context)
{
    if (device)
    {
        DEBUG_LOG_VERBOSE("DeviceProperties_SetHandsetContext lap=%d enum:handset_context_t:%d",
                  DeviceProperties_GetBdAddr(device).lap, context);

        Device_SetProperty(device, device_property_handset_context, &context, sizeof(context));
    }
}

handset_context_t DeviceProperties_GetHandsetContext(device_t device)
{
    handset_context_t context = handset_context_none;

    if (device)
    {
        handset_context_t *contextp = NULL;
        size_t size = 0;

        if (   Device_GetProperty(device, device_property_handset_context, (void *)&contextp, &size)
            && sizeof(context) == size)
        {
            context = *contextp;
        }

    }

    return context;
}

bool DeviceProperties_IsUpgradeTransportConnected(device_t device)
{
    uint8 connected;
    if(Device_GetPropertyU8(device, device_property_upgrade_transport_connected, &connected) && connected)
    {
        return TRUE;
    }
    return FALSE;
}

bool DeviceProperties_DeviceIsMruHandset(device_t device)
{
    uint8 mru_handset;
    if(Device_GetPropertyU8(device, device_property_mru, &mru_handset) && mru_handset)
    {
        return TRUE;
    }
    return FALSE;
}