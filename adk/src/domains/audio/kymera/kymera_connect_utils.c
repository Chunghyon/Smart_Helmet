/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Helper APIs for audio connections
*/

#include "kymera_connect_utils.h"
#include "kymera_config.h"
#include <audio_output.h>

void Kymera_ConnectIfValid(Source source, Sink sink)
{
    if (source && sink)
    {
        PanicNull(StreamConnect(source, sink));
    }
}

void Kymera_DisconnectIfValid(Source source, Sink sink)
{
    if (source || sink)
    {
        StreamDisconnect(source, sink);
    }
}

void Kymera_ConnectOutputSource(Source left, Source right, uint32 output_sample_rate)
{
    audio_output_params_t output_params;

    memset(&output_params, 0, sizeof(audio_output_params_t));
    output_params.sample_rate = output_sample_rate;
    output_params.transform = audio_output_tansform_connect;

    AudioOutputAddSource(left, audio_output_primary_left);

    /*In earbud application, second DAC path needs to be activated to support Parallel ANC topology*/
    if(appConfigOutputIsStereo() || appKymeraEnhancedAncRequiresSecondDAC())
    {
        AudioOutputAddSource(right, audio_output_primary_right);
    }
    /* Connect the sources to their appropriate hardware outputs. */
    AudioOutputConnect(&output_params);

    AudioOutputGainApplyConfiguredLevels(audio_output_group_main, 0, NULL);
}
