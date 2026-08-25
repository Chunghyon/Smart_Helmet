/****************************************************************************
Copyright (c) 2022 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_pcm_common.h

DESCRIPTION
    Base functionality for PCM audio input and output devices.
*/

#ifndef _AUDIO_PCM_COMMON_H_
#define _AUDIO_PCM_COMMON_H_

#include <library.h>
#include <stream.h>

typedef struct
{
    unsigned sample_rate;
    /* master or slave operation: 0 = slave, 1 = master */
    unsigned master_mode;
    /* number of bits per sample */
    unsigned sample_size;
    /* number of PCM slots */
    unsigned slot_count;
    unsigned short_sync_enable;
    unsigned lsb_first_enable;
    unsigned sample_rising_edge_enable;
    unsigned sample_format;
    unsigned master_clock_source;
    /* MClock multiplier */
    unsigned master_mclk_mult;
} pcm_config_t;

typedef enum
{
    pcm_user_none = 0,
    pcm_user = (1 << 0),
} pcm_users_t;

/*!
\brief  Callbacks to inform each active user about microphone related events.
*/
typedef struct
{
    const pcm_config_t *(*AudioPcmCommonGetPcmInterfaceSetting)(void);
    void (*AudioPcmCommonInitializeI2cInterface)(void);
    void (*AudioPcmCommonEnableDevice)(void);
    void (*AudioPcmCommonDisableDevice)(void);
} pcm_callbacks_t;

/*!
\brief  User registration structure
*/
typedef struct
{
    /*! registering user */
    pcm_users_t user;
    const pcm_callbacks_t *callbacks;
} pcm_registry_per_user_t;

/*! Registration array for PCM devices */
typedef struct
{
    unsigned nr_entries;
    const pcm_registry_per_user_t* *entry;
} pcm_registry_t;

/****************************************************************************
DESCRIPTION:
    This function configures the supplied source

PARAMETERS:
    Source audio endpoint
    rate sample rate

RETURNS:
    none
*/
void AudioPcmCommonConfigureSource(Source source, uint32 rate);

/****************************************************************************
DESCRIPTION:
    This function configures the PCM device

PARAMETERS:
    enable TRUE: device is enabled, FALSE: device is disabled

RETURNS:
    none
*/
void AudioPcmCommonConfigureDevice(bool enable);

/****************************************************************************
DESCRIPTION:
    This function returns true if PCM has been configured to output an MCLK signal

PARAMETERS:
    none

RETURNS:
    TRUE : PCM MCLK required
    FALSE : PCM MCLK not required
*/
bool AudioPcmCommonIsMasterClockRequired(void);

/****************************************************************************
DESCRIPTION:
    This function returns the PCM operation mode

PARAMETERS:
    none

RETURNS:
    TRUE : Operation mode is PCM Master
    FALSE : Operation mode is PCM Slave

*/
bool AudioPcmCommonIsMasterEnabled(void);

/****************************************************************************
DESCRIPTION:
    This function registers possible PCM interface users at intialization time.

PARAMETERS:
    param info->user: user that register
    param info->callbacks: callback functions

RETURNS:
    none
*/
void AudioPcmCommonRegisterUser(const pcm_registry_per_user_t * const info);

#endif
