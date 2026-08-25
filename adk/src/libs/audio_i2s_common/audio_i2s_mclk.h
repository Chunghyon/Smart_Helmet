/****************************************************************************
Copyright (c) 2017 - 2022 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_i2s_mclk.h
    
DESCRIPTION
    Subroutines related to I2S MCLK
*/

#ifndef AUDIO_I2S_MCLK_H
#define AUDIO_I2S_MCLK_H
#include <library.h>
#include <stream.h>

/****************************************************************************
DESCRIPTION:
    This function configures the MCLK signal for an I2S interface, if it is
    required.

PARAMETERS:
    Sink sink           - The Sink to configure MCLK for
    uint32 sample_rate  - Sample rate of the data coming from the DSP
*/
void AudioI2SMclkConfigureMasterClockIfRequired(Sink sink, uint32 sample_rate);

/****************************************************************************
DESCRIPTION:
    This function configures the MCLK signal for an I2S interface, if it is
    required.

PARAMETERS:
    Source source       - The source to configure MCLK for
    uint32 sample_rate  - Sample rate of the data coming from the DSP
*/
void AudioI2SMclkSourceConfigureMasterClockIfRequired(Source source, uint32 sample_rate);

#endif /* AUDIO_I2S_MCLK_H */
