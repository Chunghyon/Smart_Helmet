/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Helper APIs for configuring kymera audio buffers
*/

#include "kymera_buffer_utils.h"

static uint32 kymera_DivideAndRoundUp(uint32 dividend, uint32 divisor)
{
    if (dividend == 0)
        return 0;
    else
        return ((dividend - 1) / divisor) + 1;
}

unsigned Kymera_GetAudioBufferSize(uint32 max_bitrate, uint32 latency_in_ms)
{
    uint32 size_in_bits = kymera_DivideAndRoundUp(latency_in_ms * max_bitrate, 1000);
    unsigned size_in_words = kymera_DivideAndRoundUp(size_in_bits, SAMPLE_SIZE);
    return size_in_words;
}
