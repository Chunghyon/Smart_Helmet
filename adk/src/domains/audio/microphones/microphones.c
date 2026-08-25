/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Microphone component implementation, responsible for configuration and microphone user tracking
*/

#include "microphones.h"
#include "microphones_config.h"
#include <audio_plugin_common.h>
#include <logging.h>

typedef struct
{
    audio_mic_params config;
    unsigned non_exclusive_users;
    microphone_user_type_t microphone_user;
    uint32 current_sample_rate;
} managed_microphone;

static managed_microphone microphones[MAX_SUPPORTED_MICROPHONES] =
{
    {
        {
            .bias_config = appConfigMic0Bias(),
            .pio = appConfigMic0Pio(),
            .type = appConfigMic0Type(),
            .instance = appConfigMic0AudioInstance(),
            .channel = appConfigMic0AudioChannel(),
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#if (MAX_SUPPORTED_MICROPHONES >= 2)
    {
        {
            .bias_config = appConfigMic1Bias(),
            .pio = appConfigMic1Pio(),
            .type = appConfigMic1Type(),
            .instance = appConfigMic1AudioInstance(),
            .channel = appConfigMic1AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 3)
    {
        {
            .bias_config = appConfigMic2Bias(),
            .pio = appConfigMic2Pio(),
            .type = appConfigMic2Type(),
            .instance = appConfigMic2AudioInstance(),
            .channel = appConfigMic2AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 4)
    {
        {
            .bias_config = appConfigMic3Bias(),
            .pio = appConfigMic3Pio(),
            .type = appConfigMic3Type(),
            .instance = appConfigMic3AudioInstance(),
            .channel = appConfigMic3AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 5)
    {
        {
            .bias_config = appConfigMic4Bias(),
            .pio = appConfigMic4Pio(),
            .type = appConfigMic4Type(),
            .instance = appConfigMic4AudioInstance(),
            .channel = appConfigMic4AudioChannel()
        },
        .non_exclusive_users = 0,
        .microphone_user = invalid_user
    },
#endif
#if (MAX_SUPPORTED_MICROPHONES >= 6)
    {
         {
            .bias_config = appConfigMic5Bias(),
            .pio = appConfigMic5Pio(),
            .type = appConfigMic5Type(),
            .instance = appConfigMic5AudioInstance(),
            .channel = appConfigMic5AudioChannel()
         },
         .non_exclusive_users = 0,
         .microphone_user = invalid_user
    },
#endif
};

static bool microphones_IsValidMicrophoneNumber(uint16 microphone_number)
{
    return (microphone_number < MAX_SUPPORTED_MICROPHONES);
}

audio_mic_params * Microphones_GetMicrophoneConfig(uint16 microphone_number)
{
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    return &microphones[microphone_number].config;
}

void Microphones_SetMicrophoneConfig(uint16 microphone_number, audio_mic_params *config)
{
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    microphones[microphone_number].config = *config;
}

static microphone_user_type_t microphones_GetMicrophoneUserType(uint16 microphone_number)
{
    return (microphones[microphone_number].microphone_user);
}

static inline uint32 microphones_GetCurrentSampleRate(uint16 microphone_number)
{
    return (microphones[microphone_number].current_sample_rate);
}

static inline bool microphones_IsMicrophoneInExclusiveUse(uint16 microphone_number)
{
    return (microphones_GetMicrophoneUserType(microphone_number) == normal_priority_user ||
            microphones_GetMicrophoneUserType(microphone_number) == high_priority_user);
}

static inline bool microphones_IsMicrophoneInNonExclusiveUse(uint16 microphone_number)
{
    return !!(microphones[microphone_number].non_exclusive_users);
}

static inline void microphones_AddUser(uint16 microphone_number, microphone_user_type_t microphone_user_type)
{
    if(microphone_user_type == non_exclusive_user)
    {
        microphones[microphone_number].non_exclusive_users++;
         DEBUG_LOG("microphones_AddUser: microphone[%d] non_exclusive_users=%d", microphone_number,
                   microphones[microphone_number].non_exclusive_users);
    }
    else
    {
        microphones[microphone_number].microphone_user = microphone_user_type;
    }
}

static inline void microphones_RemoveUser(uint16 microphone_number, microphone_user_type_t microphone_user_type)
{
    if(microphone_user_type == non_exclusive_user)
    {
        microphones[microphone_number].non_exclusive_users--;
        DEBUG_LOG("microphones_RemoveUser: microphone[%d] non_exclusive_users=%d", microphone_number,
                  microphones[microphone_number].non_exclusive_users);
    }
    else
    {
        microphones[microphone_number].microphone_user = invalid_user;
    }
}

static inline void microphones_SetCurrentSampleRate(uint16 microphone_number, uint32 sample_rate)
{
    microphones[microphone_number].current_sample_rate = sample_rate;
}

static unsigned microphones_GetMicrophoneBiasVoltage(mic_bias_id id)
{
    unsigned bias = 0;
    if (id == MIC_BIAS_0)
    {
        if (appConfigMic0Bias() == BIAS_CONFIG_MIC_BIAS_0)
            bias =  appConfigMic0BiasVoltage();
        else if (appConfigMic1Bias() == BIAS_CONFIG_MIC_BIAS_0)
            bias = appConfigMic1BiasVoltage();
        else
            Panic();
    }
    else if (id == MIC_BIAS_1)
    {
        if (appConfigMic0Bias() == BIAS_CONFIG_MIC_BIAS_1)
            bias = appConfigMic0BiasVoltage();
        else if (appConfigMic1Bias() == BIAS_CONFIG_MIC_BIAS_1)
            bias = appConfigMic1BiasVoltage();
        else
            Panic();
    }
    else
        Panic();

    DEBUG_LOG("microphones_GetMicrophoneBiasVoltage, id %u, bias %u", id, bias);
    return bias;
}

void Microphones_SetMicRate(uint16 microphone_number, uint32 sample_rate, microphone_user_type_t microphone_user_type)
{
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    PanicFalse(microphone_user_type != invalid_user);

    if(((microphone_user_type > normal_priority_user) && (microphones_GetMicrophoneUserType(microphone_number) != microphone_user_type))
            || (microphones_IsMicrophoneInExclusiveUse(microphone_number) == FALSE))
    {
        const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
        uint32 current_sample_rate = microphones_GetCurrentSampleRate(microphone_number);
        if(current_sample_rate != sample_rate)
        {
            DEBUG_LOG("Microphones_SetMicRate: microphone_number %d, rate=%lu, enum:mic_user_type_t:%d",
                      microphone_number, sample_rate, microphone_user_type);
            AudioPluginSetMicRate(microphone_config->channel, *microphone_config, sample_rate);
            microphones_SetCurrentSampleRate(microphone_number, sample_rate);
        }
    }
}

Source Microphones_TurnOnMicrophone(uint16 microphone_number, microphone_user_type_t microphone_user_type)
{
    Source mic_source = NULL;
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    PanicFalse(microphone_user_type != invalid_user);

    if(((microphone_user_type > normal_priority_user) && (microphones_GetMicrophoneUserType(microphone_number) != microphone_user_type))
            || (microphones_IsMicrophoneInExclusiveUse(microphone_number) == FALSE))
    {
        const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
        bool already_in_use = Microphones_IsMicrophoneInUse(microphone_number);
        if(!already_in_use)
        {
            DEBUG_LOG("Microphones_TurnOnMicrophone: microphone_number %d enum:mic_type_t:%d audio HDW instance %d",
                      microphone_number, microphone_config->type, microphone_config->instance);
            AudioPluginSetMicGain(microphone_config->channel, *microphone_config);
            mic_source =  AudioPluginMicSetup(microphone_config->channel, *microphone_config);
        }
        else
        {
            mic_source = AudioPluginGetMicSource(*microphone_config, microphone_config->channel);
        }
        microphones_AddUser(microphone_number, microphone_user_type);
    }
    DEBUG_LOG("Microphones_TurnOnMicrophone: source=%p", mic_source);
    return mic_source;
}

void Microphones_TurnOffMicrophone(uint16 microphone_number, microphone_user_type_t microphone_user_type)
{
    DEBUG_LOG("Microphones_TurnOffMicrophone: number=%d, user_type=%d", microphone_number, microphone_user_type);
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));
    PanicFalse(microphone_user_type != invalid_user);

    if(Microphones_IsMicrophoneInUse(microphone_number))
    {
        bool close_mic;
        microphones_RemoveUser(microphone_number, microphone_user_type);
        close_mic = (Microphones_IsMicrophoneInUse(microphone_number) == FALSE);
        DEBUG_LOG("Microphones_TurnOffMicrophone: shutting down, close_mic=%u", close_mic);
        if(close_mic)
        {
            const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
            AudioPluginMicShutdown(microphone_config->channel, microphone_config, close_mic);
            microphones_SetCurrentSampleRate(microphone_number, 0);
        }
    }
}

uint8 Microphones_MaxSupported(void)
{
    return MAX_SUPPORTED_MICROPHONES;
}

void Microphones_Init(void)
{
    for(uint16 microphone_number = 0; microphone_number < MAX_SUPPORTED_MICROPHONES; microphone_number++)
    {
        microphones[microphone_number].non_exclusive_users = 0;
        microphones[microphone_number].microphone_user = invalid_user;
        microphones[microphone_number].current_sample_rate = 0;
    }
    AudioPluginCommonRegisterMicBiasVoltageCallback(microphones_GetMicrophoneBiasVoltage);
}

Source Microphones_GetMicrophoneSource(uint16 microphone_number)
{
    Source mic_source = NULL;
    DEBUG_LOG("Microphones_GetMicrophoneSource: number=%d", microphone_number);
    PanicFalse(microphones_IsValidMicrophoneNumber(microphone_number));

    if (Microphones_IsMicrophoneInUse(microphone_number))
    {
        const audio_mic_params * microphone_config = Microphones_GetMicrophoneConfig(microphone_number);
        mic_source = AudioPluginGetMicSource(*microphone_config, microphone_config->channel);
    }

    return mic_source;
}

bool Microphones_IsMicrophoneInUse(uint16 microphone_number)
{
    return (microphones_IsMicrophoneInNonExclusiveUse(microphone_number) || microphones_IsMicrophoneInExclusiveUse(microphone_number));
}

