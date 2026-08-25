/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_config.c
\brief  Implementation of AMA configuration APIs
*/

#include "ama_config.h"

#ifdef INCLUDE_AMA
#include <stdlib.h>
#include "voice_ui_va_client_if.h"
#include <logging.h>
#include <message.h>
#include <vm.h>
#include <vmal.h>
#include <bt_device.h>
#include <device_info.h>
#include <md5.h>
#include <source.h>
#include "ama_state.h"
#include "ama_send_command.h"
#include <stdio.h>

#include "speech.pb-c.h"

#include "ama_send_command.h"
#include "ama_log.h"
#include "ama_battery.h"
#include "ama_setup_tracker.h"
#include "ama_transport.h"
#include "ama_voice_ui_handle.h"
#include <panic.h>
#include <source.h>
#include "ama_debug.h"
#include "message.h"
#include "local_name.h"

static bdaddr local_addr = {0};

static const SpeechInitiationType speech_initiations[] =
{
    SPEECH_INITIATION_TYPE__TAP,
#ifdef INCLUDE_AMA_WUW
    SPEECH_INITIATION_TYPE__WAKEWORD,
#endif
};

#define SPEECH_INITIATIONS_SIZE ARRAY_DIM(speech_initiations)

#ifdef INCLUDE_AMA_WUW
static const char * wakewords[] =
{
    "alexa",
};

#define WAKEWORDS_SIZE ARRAY_DIM(wakewords)
#endif

static Transport supported_transports[NUMBER_OF_SUPPORTED_TRANSPORTS];

uint8 Ama_GetNumTransportSupported(void)
{
    return NUMBER_OF_SUPPORTED_TRANSPORTS;
}

bdaddr * Ama_GetLocalAddress(void)
{
    return &local_addr;
}

void Ama_PopulateDeviceInformation(DeviceInformation *device_information, uint32 device_id)
{
    unsigned index;
    uint16 name_len = 0;
    DEBUG_LOG_INFO("Ama_PopulateDeviceInformation: device_id:%d", device_id);

    device_information->n_supported_transports = 0;
    device_information->name = (char*)LocalName_GetName(&name_len);
    device_information->device_id = Ama_GetDeviceId(device_id);
    device_information->device_type = Ama_GetDeviceType(device_id);
    device_information->serial_number = Ama_GetSerialNumber(device_id);
    device_information->n_associated_devices = Ama_GetNumberAssociatedDevices();
    device_information->associated_devices = Ama_GetAssociatedDevices(device_id);

    supported_transports[device_information->n_supported_transports++] = TRANSPORT__BLUETOOTH_RFCOMM;
#ifdef INCLUDE_ACCESSORY
    supported_transports[device_information->n_supported_transports++] = TRANSPORT__BLUETOOTH_IAP;
#endif

    device_information->supported_transports = &supported_transports[0];
    device_information->battery = AmaBattery_GetDeviceBattery();

    device_information->n_supported_speech_initiations = SPEECH_INITIATIONS_SIZE;
    device_information->supported_speech_initiations = (SpeechInitiationType *)speech_initiations;

#ifdef INCLUDE_AMA_WUW
    device_information->n_supported_wakewords = WAKEWORDS_SIZE;
    device_information->supported_wakewords = (char **)wakewords;
#else
    device_information->n_supported_wakewords = 0;
    device_information->supported_wakewords = (char **) NULL;
#endif

    if (debug_log_level__global >= DEBUG_LOG_LEVEL_VERBOSE)
    {
        AmaLog_LogVaArg("ama_PopulateDeviceInformation name %s\n", device_information->name);
        AmaLog_LogVaArg("ama_PopulateDeviceInformation device_type %s\n", device_information->device_type);
        AmaLog_LogVaArg("ama_PopulateDeviceInformation serial_number %s\n", device_information->serial_number);
    }

    DEBUG_LOG_VERBOSE("ama_PopulateDeviceInformation number of supported transports %lu",
        device_information->n_supported_transports);
    for (index = 0; index < device_information->n_supported_transports; index++)
    {
        DEBUG_LOG_VERBOSE("ama_PopulateDeviceInformation supported transport[%i]: enum:Transport:%d",
            index, device_information->supported_transports[index]);
    }

    DEBUG_LOG_VERBOSE("ama_PopulateDeviceInformation battery: level %lu, scale %lu, status enum:DeviceBattery__Status:%d",
        device_information->battery->level, device_information->battery->scale, device_information->battery->status);

    DEBUG_LOG_VERBOSE("ama_PopulateDeviceInformation number of supported speech initiations %lu",
        device_information->n_supported_speech_initiations);
    for (index = 0; index < device_information->n_supported_speech_initiations; index++)
    {
        DEBUG_LOG_VERBOSE("ama_PopulateDeviceInformation speech initiation[%i]: enum:SpeechInitiationType:%d",
            index, device_information->supported_speech_initiations[index]);
    }

    DEBUG_LOG_VERBOSE("ama_PopulateDeviceInformation number of supported wakewords %lu",
        device_information->n_supported_wakewords);
    if (debug_log_level__global >= DEBUG_LOG_LEVEL_VERBOSE)
    {
        for (index = 0; index < device_information->n_supported_wakewords; index++)
        {
            AmaLog_LogVaArg("ama_PopulateDeviceInformation wakeword[%i]: %s\n", index,
                device_information->supported_wakewords[index]);
        }
    }
}

void Ama_PopulateDeviceConfiguration(DeviceConfiguration * device_config)
{
    bool require_assistant_override = !VoiceUi_IsActiveAssistant(Ama_GetVoiceUiHandle());
    device_config->needs_assistant_override = require_assistant_override;
    device_config->needs_setup = Ama_IsSetupComplete() ? FALSE : TRUE;

    DEBUG_LOG_VERBOSE("ama_PopulateDeviceConfiguration needs assistant override %u", device_config->needs_assistant_override);
    DEBUG_LOG_VERBOSE("ama_PopulateDeviceConfiguration needs setup %u", device_config->needs_setup);

}

void Ama_PopulateCommonDeviceFeatures(DeviceFeatures *device_features)
{
    /*
     * The DeviceFeatures device_attributes, n_feature_properties and feature_properties fields are currently unused.
     * Only the DeviceFeatures features field is used and contains a bitmask of the supported features.
     */
    DEBUG_LOG_DEBUG("Ama_PopulateCommonDeviceFeatures: Battery");
    device_features->features |= AMA_DEVICE_FEATURE_BATTERY_LEVEL;
#ifdef ENABLE_ANC
    DEBUG_LOG_DEBUG("Ama_PopulateCommonDeviceFeatures: ANC");
    device_features->features |= AMA_DEVICE_FEATURE_ANC;
    DEBUG_LOG_DEBUG("Ama_PopulateCommonDeviceFeatures: Passthrough");
    device_features->features |= AMA_DEVICE_FEATURE_PASSTHROUGH;
#endif
#ifdef INCLUDE_AMA_WUW
    DEBUG_LOG_DEBUG("Ama_PopulateCommonDeviceFeatures: Wake Word");
    device_features->features |= AMA_DEVICE_FEATURE_WAKE_WORD;
    DEBUG_LOG_DEBUG("Ama_PopulateCommonDeviceFeatures: Wake Word Privacy");
    device_features->features |= AMA_DEVICE_FEATURE_PRIVACY_MODE;
#endif
#ifdef INCLUDE_MUSIC_PROCESSING
    DEBUG_LOG_DEBUG("Ama_PopulateCommonDeviceFeatures: Equalizer");
    device_features->features |= AMA_DEVICE_FEATURE_EQUALIZER;
#endif
}

#endif /* INCLUDE_AMA */
