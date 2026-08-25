/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice source specific parameters used in the kymera adaptation layer
*/

#ifndef KYMERA_VOICE_PROTECTED_H_
#define KYMERA_VOICE_PROTECTED_H_

#include "volume_types.h"
#include <stream.h>

typedef enum
{
    hfp_codec_mode_none,
    hfp_codec_mode_narrowband,
    hfp_codec_mode_wideband,
    hfp_codec_mode_ultra_wideband,
    hfp_codec_mode_super_wideband
} hfp_codec_mode_t;

typedef struct
{
    Sink audio_sink;
    hfp_codec_mode_t codec_mode;
    uint8 wesco;
    volume_t volume;
    uint8 pre_start_delay;
    uint8 tesco;
    bool synchronised_start;
    void (*started_handler)(void);
} voice_connect_parameters_t;

typedef struct
{
    uint8 mode;
    uint8 spkr_channels;
    uint8 spkr_frame_size;
    Source spkr_src;
    Sink mic_sink;
    uint32 spkr_sample_rate;
    uint32 mic_sample_rate;
    volume_t volume;
    bool mute_status;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
    void (*kymera_stopped_handler)(Source source);
} usb_voice_connect_parameters_t;

typedef struct
{
     Source spkr_src;
     Sink mic_sink;
     void (*kymera_stopped_handler)(Source source);
} usb_voice_disconnect_parameters_t;

typedef struct
{
    uint16 frame_length;
    uint16 stream_type;
    uint8 codec_frame_blocks_per_sdu;
    uint32 presentation_delay;
} stream_config;

typedef struct
{
    uint16 sample_rate;
    uint16 frame_duration;
    uint8 codec_type;
    uint8 codec_version;
    uint16 source_iso_handle;
    uint16 source_iso_handle_right;
    uint16 sink_iso_handle;
    stream_config speaker_stream;
    stream_config mic_stream;
} le_voice_config_t;

typedef struct
{
    volume_t volume;
    le_voice_config_t le_voice_config;
} le_voice_connect_parameters_t;

#endif /* KYMERA_VOICE_PROTECTED_H_ */
