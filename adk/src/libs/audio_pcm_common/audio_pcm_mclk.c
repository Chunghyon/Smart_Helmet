/****************************************************************************
Copyright (c) 2017 - 2022 Qualcomm Technologies International, Ltd.

FILE NAME
    audio_pcm_mclk.c

DESCRIPTION
    Subroutines related to PCM MCLK
*/

#include <audio_mclk.h>
#include <stdlib.h>
#include <panic.h>
#include <print.h>
#include <stream.h>
#include <sink.h>
#include <source.h>

#include "audio_pcm_mclk.h"
#include "audio_pcm_common.h"


/******************************************************************************/
void AudioPcmMclkSourceEnableMasterClockIfRequired(Source source, bool enable)
{
    if (AudioPcmCommonIsMasterClockRequired())
    {
        if (enable)
        {
            if (AudioPcmCommonIsMasterEnabled())
            {
                /* PCM Master, enable MCLK output */
                PanicFalse(SourceMasterClockEnable(source, MCLK_ENABLE_OUTPUT));
            }
            else
            {
                /* PCM Slave, enable MCLK input from external source */
                PanicFalse(SourceMasterClockEnable(source, MCLK_ENABLE));
            }
        }
        else
        {
            /* Disable MCLK */
            PanicFalse(SourceMasterClockEnable(source, MCLK_DISABLE));
        }
    }
}

/******************************************************************************/
void AudioPcmMclkResetMasterClock(void)
{
    PanicFalse(AudioMasterClockConfigure(FALSE, 0));
}
