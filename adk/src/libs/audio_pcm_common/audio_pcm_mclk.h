/****************************************************************************
Copyright (c) 2017 - 2022 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_pcm_mclk.h
    
DESCRIPTION
    Subroutines related to PCM MCLK
*/


#ifndef AUDIO_PCM_MCLK_H
#define AUDIO_PCM_MCLK_H
#include <library.h>
#include <stream.h>

/****************************************************************************
DESCRIPTION:
    This function enables or disables the MCLK signal for an PCM input interface,
    if required. It is separate from source configuration as it must happen
    after all channels for a given interface have been configured, or after
    all channels of the interface have been disconnected, if disabling MCLK.

PARAMETERS:
    Source source - The Source to enable/disable MCLK for
    Bool enable - TRUE to enable MCLK output, FALSE to disable

RETURNS:
    none
*/
void AudioPcmMclkSourceEnableMasterClockIfRequired(Source source, bool enable);

/****************************************************************************
DESCRIPTION:
    This function resets the MCLK to internal

PARAMETERS:
    none

RETURNS:
    none
*/
void AudioPcmMclkResetMasterClock(void);

#endif /* AUDIO_PCM_MCLK_H */
