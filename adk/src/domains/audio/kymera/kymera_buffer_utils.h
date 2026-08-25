/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Helper definitions for configuring audio buffers
*/

#ifndef KYMERA_BUFFER_UTILS_H_
#define KYMERA_BUFFER_UTILS_H_

#include <rtime.h>

#define SAMPLE_SIZE (16)

/*! @{ \name Macros to calculate buffer sizes required to hold a specific (timed) amount of audio. */
#define US_TO_BUFFER_SIZE_MONO_PCM(time_us, sample_rate) ((((time_us) * (sample_rate)) + (US_PER_SEC-1)) / US_PER_SEC)
#define MS_TO_BUFFER_SIZE_CODEC(time_ms, codec_rate_kbps) ((((time_ms) * (codec_rate_kbps)) + (SAMPLE_SIZE-1)) / SAMPLE_SIZE)
/*! @} */

/*! \brief Get the audio buffer size (in number of samples) required based on bitrate and latency.
    \max_bitrate The maximum bitrate of the audio stream (in bps).
    \latency_in_ms Milliseconds of audio the buffer should be able to hold.
    \return Audio buffer size in number of samples.
 */
unsigned Kymera_GetAudioBufferSize(uint32 max_bitrate, uint32 latency_in_ms);

#endif /* KYMERA_BUFFER_UTILS_H_ */
