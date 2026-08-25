/****************************************************************************
Copyright (c) 2022 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_pcm_common.c
DESCRIPTION
    Base support for external PCM audio devices
NOTES
*/

#include <audio_plugin_if.h>
#include <gain_utils.h>
#include <stdlib.h>
#include <panic.h>
#include <print.h>
#include <file.h>
#include <stream.h> 
#include <sink.h>
#include <source.h>
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <message.h>
#include <transform.h>
#include <string.h>

#include "audio_pcm_common.h"
#include "audio_pcm_mclk.h"

static struct
{
    pcm_registry_t registry;
    unsigned device_initialized:1;
} pcm_state =
{
    .registry = {0, NULL},
    .device_initialized = 0,
};

static const pcm_registry_per_user_t * audioPcmCommonGetRegistryEntry(pcm_users_t user)
{
    unsigned i;
    for(i = 0; i < pcm_state.registry.nr_entries; i++)
    {
        if (pcm_state.registry.entry[i]->user == user)
        {
            return pcm_state.registry.entry[i];
        }
    }
    PRINT(("audioPcmCommonGetRegistryEntry: User not found"));
    Panic();
    return NULL;
}

static const pcm_config_t *audioPcmCommonGetPcmConfig(pcm_users_t user)
{
    const pcm_registry_per_user_t * reg_entry;

    reg_entry = audioPcmCommonGetRegistryEntry(user);
    if(reg_entry->callbacks->AudioPcmCommonGetPcmInterfaceSetting)
    {
        return reg_entry->callbacks->AudioPcmCommonGetPcmInterfaceSetting();
    }
    PRINT(("audioPcmCommonGetPcmConfig: No PCM config found"));
    Panic();
    return NULL;
}

/****************************************************************************
DESCRIPTION:
    This function returns TRUE if PCM has been configured to output an MCLK signal

PARAMETERS:
    none

RETURNS:
    TRUE : PCM MCLK required
    FALSE : PCM MCLK not required
*/
bool AudioPcmCommonIsMasterClockRequired(void)
{
    const pcm_config_t *pcm_config = audioPcmCommonGetPcmConfig(pcm_user);
    if (pcm_config->master_mclk_mult > 0)
        return TRUE;
    else
        return FALSE;
}

/****************************************************************************
DESCRIPTION:
    This function returns the PCM operation mode

PARAMETERS:
    none

RETURNS:
    TRUE : Operation mode is PCM Master
    FALSE : Operation mode is PCM Slave

*/
bool AudioPcmCommonIsMasterEnabled(void)
{
    const pcm_config_t *pcm_config = audioPcmCommonGetPcmConfig(pcm_user);
    if(pcm_config->master_mode)
        return TRUE;
    else
        return FALSE;
}

/****************************************************************************
DESCRIPTION:
    This function configures the PCM interface for the given source

PARAMETERS:
    Source  source audio endpoint
    rate    microphones sample rate. If a PCM device has different requirements
            (e.g. is always using a fix sample rate) the device specific sample rate is used.

RETURNS:
    none
*/
void AudioPcmCommonConfigureSource(Source source, uint32 rate)
{
    const pcm_registry_per_user_t * reg_entry;

    const pcm_config_t *pcm_config = audioPcmCommonGetPcmConfig(pcm_user);
    AudioPcmMclkResetMasterClock();

    uint32 used_sample_rate = rate;
    if(pcm_config->sample_rate)
    {
        used_sample_rate = pcm_config->sample_rate;
    }

    PRINT(("AudioPcmCommonConfigureSource: rate %luHz sample_size %u", used_sample_rate, pcm_config->sample_size));
    PanicFalse(SourceConfigure(source, STREAM_PCM_SYNC_RATE, used_sample_rate));
    PanicFalse(SourceConfigure(source, STREAM_PCM_MASTER_CLOCK_RATE, used_sample_rate * pcm_config->sample_size * pcm_config->slot_count));
    PanicFalse(SourceConfigure(source, STREAM_PCM_MASTER_MODE , pcm_config->master_mode));
    PanicFalse(SourceConfigure(source, STREAM_PCM_SLOT_COUNT , pcm_config->slot_count));
    PanicFalse(SourceConfigure(source, STREAM_PCM_SHORT_SYNC_ENABLE, pcm_config->short_sync_enable));
    PanicFalse(SourceConfigure(source, STREAM_PCM_LSB_FIRST_ENABLE, pcm_config->lsb_first_enable));
    PanicFalse(SourceConfigure(source, STREAM_AUDIO_SAMPLE_SIZE, pcm_config->sample_size));
    PanicFalse(SourceConfigure(source, STREAM_PCM_SAMPLE_RISING_EDGE_ENABLE , pcm_config->sample_rising_edge_enable));
    PanicFalse(SourceConfigure(source, STREAM_PCM_SAMPLE_FORMAT, pcm_config->sample_format));
    PanicFalse(SourceConfigure(source, STREAM_PCM_MASTER_CLK_SOURCE, pcm_config->master_clock_source));
    PanicFalse(SourceConfigure(source, STREAM_PCM_MASTER_MCLK_MULT, pcm_config->master_mclk_mult));
    AudioPcmMclkSourceEnableMasterClockIfRequired(source, TRUE);

    reg_entry = audioPcmCommonGetRegistryEntry(pcm_user);
    if(reg_entry->callbacks->AudioPcmCommonInitializeI2cInterface)
    {
        reg_entry->callbacks->AudioPcmCommonInitializeI2cInterface();
    }
}

/****************************************************************************
DESCRIPTION:
    This function configures the PCM device

PARAMETERS:
    none

RETURNS:
    none
*/
void AudioPcmCommonConfigureDevice(bool enable)
{
    const pcm_registry_per_user_t * reg_entry;

    if(enable)
    {
        if(pcm_state.device_initialized == FALSE)
        {
            reg_entry = audioPcmCommonGetRegistryEntry(pcm_user);
            if(reg_entry->callbacks->AudioPcmCommonEnableDevice)
            {
                reg_entry->callbacks->AudioPcmCommonEnableDevice();
                pcm_state.device_initialized = TRUE;
            }
        }
    }
    else
    {
        if(pcm_state.device_initialized == TRUE)
        {
            reg_entry = audioPcmCommonGetRegistryEntry(pcm_user);
            if(reg_entry->callbacks->AudioPcmCommonEnableDevice)
            {
                reg_entry->callbacks->AudioPcmCommonDisableDevice();
                pcm_state.device_initialized = FALSE;
            }
        }
    }
    PRINT(("AudioPcmCommonConfigureDevice: device_initialized=%u", pcm_state.device_initialized));
}

void AudioPcmCommonRegisterUser(const pcm_registry_per_user_t * const info)
{
    PRINT(("AudioPcmCommonRegisterUser: enum:mic_users_t:%d", info->user));

    pcm_state.registry.entry = PanicNull(realloc(pcm_state.registry.entry, (pcm_state.registry.nr_entries + 1) * sizeof(*pcm_state.registry.entry)));

    PanicNull((void*)info->callbacks);

    pcm_state.registry.entry[pcm_state.registry.nr_entries] = info;
    pcm_state.registry.nr_entries++;
    // Restrict to 1 PCM user
    PanicFalse(pcm_state.registry.nr_entries < 2);
}
