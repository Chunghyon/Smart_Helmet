/****************************************************************************
Copyright (c) 2017 - 2022 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_i2s_mclk.c

DESCRIPTION
    Subroutines related to I2S MCLK
*/

#include "audio_i2s_common.h"

/******************************************************************************/
void AudioI2SMclkConfigureMasterClockIfRequired(Sink sink, uint32 sample_rate)
{
    UNUSED(sink);
    UNUSED(sample_rate);
}

/******************************************************************************/
void AudioI2SMclkSourceConfigureMasterClockIfRequired(Source source, uint32 sample_rate)
{
    UNUSED(source);
    UNUSED(sample_rate);
}

/******************************************************************************/
void AudioI2SMclkEnableMasterClockIfRequired(Sink sink, bool enable)
{
    UNUSED(sink);
    UNUSED(enable);
}

/******************************************************************************/
void AudioI2SMclkSourceEnableMasterClockIfRequired(Source source, bool enable)
{
    UNUSED(source);
    UNUSED(enable);
}
