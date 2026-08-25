/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Adaptation layer providing a generalised API into the world of kymera audio
*/

#include "kymera_adaptation.h"
#include "kymera_adaptation_audio_protected.h"
#include "kymera_config.h"
#include "kymera_adaptation_voice_protected.h"
#include "kymera.h"
#include "kymera_volume.h"
#include "volume_utils.h"
#include "audio_sources.h"
#include "voice_sources.h"
#ifdef INCLUDE_AUDIO_DRIVER
#include <audio_driver.h>
#include <audio_driver_a2dp.h>
#include <audio_use_case_instance.h>
#endif
#include <logging.h>

static appKymeraScoMode kymeraAdaptation_ConvertToScoMode(hfp_codec_mode_t codec_mode)
{
    appKymeraScoMode sco_mode = NO_SCO;
    switch(codec_mode)
    {

        case hfp_codec_mode_narrowband:
            sco_mode = SCO_NB;
            break;
        case hfp_codec_mode_wideband:
            sco_mode = SCO_WB;
            break;
        case hfp_codec_mode_ultra_wideband:
            sco_mode = SCO_UWB;
            break;
        case hfp_codec_mode_super_wideband:
            sco_mode = SCO_SWB;
            break;
        case hfp_codec_mode_none:
        default:
            break;
    }
    return sco_mode;
}

static void kymeraAdaptation_ConnectSco(connect_parameters_t * params)
{
    if(params->source_params.data_length == sizeof(voice_connect_parameters_t))
    {
        voice_connect_parameters_t * voice_params = (voice_connect_parameters_t *)params->source_params.data;
        appKymeraScoMode sco_mode = kymeraAdaptation_ConvertToScoMode(voice_params->codec_mode);
        int16 volume_in_db = VolumeUtils_GetVolumeInDb(voice_params->volume);
        appKymeraScoStart(voice_params->audio_sink, sco_mode, voice_params->wesco, volume_in_db,
                          voice_params->pre_start_delay, voice_params->synchronised_start,
                          voice_params->started_handler);
    }
    else
    {
        Panic();
    }
}

static void kymeraAdaptation_ConnectUsbVoice(connect_parameters_t * params)
{
    usb_voice_connect_parameters_t * usb_voice =
            (usb_voice_connect_parameters_t *)params->source_params.data;
    int16 volume_in_db = VolumeUtils_GetVolumeInDb(usb_voice->volume);

    appKymeraUsbVoiceStart(usb_voice->mode, usb_voice->spkr_channels, usb_voice->spkr_frame_size,
                           usb_voice->spkr_sample_rate, usb_voice->mic_sample_rate, usb_voice->spkr_src,
                           usb_voice->mic_sink, volume_in_db, usb_voice->min_latency_ms, usb_voice->max_latency_ms,
                           usb_voice->target_latency_ms, usb_voice->kymera_stopped_handler);
}


static void kymeraAdaptation_ConnectVoice(connect_parameters_t * params)
{
    switch(params->source.u.voice)
    {
        case voice_source_hfp_1:
        case voice_source_hfp_2:
            kymeraAdaptation_ConnectSco(params);
            break;

        case voice_source_usb:
            kymeraAdaptation_ConnectUsbVoice(params);
            break;
        default:
            Panic();
            break;
    }
}

#ifdef INCLUDE_AUDIO_DRIVER
static audio_a2dp_start_params_t kymeraAdaptation_GetA2dpStartParams(const a2dp_connect_parameters_t * connect_params)
{
    audio_a2dp_start_params_t params =
    {
        .lock = connect_params->client_lock,
        .lock_mask = connect_params->client_lock_mask,
        .codec_settings =
        {
            .rate = connect_params->rate, .channel_mode = connect_params->channel_mode,
            .seid = connect_params->seid, .sink = connect_params->sink,
            .codecData =
            {
                .content_protection = connect_params->content_protection, .bitpool = connect_params->bitpool,
                .format = connect_params->format, .packet_size = connect_params->packet_size,
                .aptx_ad_params.features = connect_params->aptx_features
            }
        },
        .volume_in_db = VolumeUtils_GetVolumeInDb(connect_params->volume),
        .master_pre_start_delay = connect_params->master_pre_start_delay,
        .max_bitrate = connect_params->max_bitrate,
        .q2q_mode = connect_params->q2q_mode,
        .nq2q_ttp = connect_params->nq2q_ttp
    };
    return params;
}

static void kymeraAdaptation_PrepareA2DP(audio_use_case_instance_t instance, const a2dp_connect_parameters_t *connect_params)
{
    audio_driver_a2dp_prepare_t prepare_params = kymeraAdaptation_GetA2dpStartParams(connect_params);
    data_blob_t params = {.data = &prepare_params, .data_length = sizeof(prepare_params)};
    AudioDriver_PrepareUseCase(instance, &params);
}

static void kymeraAdaptation_StartA2DP(audio_use_case_instance_t instance, const a2dp_connect_parameters_t * connect_params)
{
    audio_driver_a2dp_start_t start_params = kymeraAdaptation_GetA2dpStartParams(connect_params);
    data_blob_t params = {.data = &start_params, .data_length = sizeof(start_params)};
    AudioDriver_StartUseCase(instance, &params);
}

static void kymeraAdaptation_ConnectA2DP(connect_parameters_t * params)
{
    const a2dp_connect_parameters_t *connect_params = (a2dp_connect_parameters_t *)params->source_params.data;
    audio_use_case_instance_t instance = AudioUseCase_GetInstanceForSource(params->source);
    kymeraAdaptation_PrepareA2DP(instance, connect_params);
    kymeraAdaptation_StartA2DP(instance, connect_params);
}
#else
static void kymeraAdaptation_ConnectA2DP(connect_parameters_t * params)
{
    a2dp_connect_parameters_t * connect_params = (a2dp_connect_parameters_t *)params->source_params.data;
    a2dp_codec_settings codec_settings =
    {
        .rate = connect_params->rate, .channel_mode = connect_params->channel_mode,
        .seid = connect_params->seid, .sink = connect_params->sink,
        .codecData =
        {
            .content_protection = connect_params->content_protection, .bitpool = connect_params->bitpool,
            .format = connect_params->format, .packet_size = connect_params->packet_size,
            .aptx_ad_params.features = connect_params->aptx_features
        }
    };

    int16 volume_in_db = VolumeUtils_GetVolumeInDb(connect_params->volume);
    appKymeraA2dpStart(connect_params->client_lock, connect_params->client_lock_mask,
                        &codec_settings, connect_params->max_bitrate,
                        volume_in_db, connect_params->master_pre_start_delay, connect_params->q2q_mode,
                        connect_params->nq2q_ttp);
}
#endif

static void kymeraAdaptation_ConnectLineIn(connect_parameters_t * params)
{
    wired_analog_connect_parameters_t * connect_params = (wired_analog_connect_parameters_t *)params->source_params.data;
    int16 volume_in_db = VolumeUtils_GetVolumeInDb(connect_params->volume);
    Kymera_StartWiredAnalogAudio(volume_in_db, connect_params->rate, connect_params->min_latency, connect_params->max_latency, connect_params->target_latency);
}

static void kymeraAdaptation_ConnectUsbAudio(connect_parameters_t * params)
{
    usb_audio_connect_parameters_t * usb_audio =
            (usb_audio_connect_parameters_t *)params->source_params.data;
    int16 volume_in_db = VolumeUtils_GetVolumeInDb(usb_audio->volume);

    appKymeraUsbAudioStart(usb_audio->channels, usb_audio->frame_size,
                           usb_audio->spkr_src, volume_in_db,
                           usb_audio->sample_freq, usb_audio->min_latency_ms,
                           usb_audio->max_latency_ms, usb_audio->target_latency_ms);
}


static void kymeraAdaptation_ConnectAudio(connect_parameters_t * params)
{
    switch(params->source.u.audio)
    {
        case audio_source_a2dp_1:
        case audio_source_a2dp_2:
            kymeraAdaptation_ConnectA2DP(params);
            break;

        case audio_source_line_in:
            kymeraAdaptation_ConnectLineIn(params);
            break;

        case audio_source_usb:
            kymeraAdaptation_ConnectUsbAudio(params);
            break;
        default:
            Panic();
            break;
    }
}

static void kymeraAdaptation_DisconnectSco(disconnect_parameters_t * params)
{
    if(params->source_params.data_length == 0)
    {
        appKymeraScoStop();
    }
    else
    {
        Panic();
    }
}

static void kymeraAdaptation_DisconnectUsbVoice(disconnect_parameters_t * params)
{
    usb_voice_disconnect_parameters_t * voice_params =
            (usb_voice_disconnect_parameters_t *)params->source_params.data;

    appKymeraUsbVoiceStop(voice_params->spkr_src, voice_params->mic_sink,
                          voice_params->kymera_stopped_handler);
}


static void kymeraAdaptation_DisconnectVoice(disconnect_parameters_t * params)
{
    switch(params->source.u.voice)
    {
        case voice_source_hfp_1:
        case voice_source_hfp_2:
            kymeraAdaptation_DisconnectSco(params);
            break;

        case voice_source_usb:
            kymeraAdaptation_DisconnectUsbVoice(params);
            break;
        default:
            Panic();
            break;
    }
}

#ifdef INCLUDE_AUDIO_DRIVER
static void kymeraAdaptation_StopA2DP(audio_use_case_instance_t instance, a2dp_disconnect_parameters_t * disconnect_params)
{
    audio_driver_a2dp_stop_t stop_params =
    {
        .seid = disconnect_params->seid,
        .source = disconnect_params->source
    };
    data_blob_t params = {.data = &stop_params, .data_length = sizeof(stop_params)};
    AudioDriver_StopUseCase(instance, &params);
}

static void kymeraAdaptation_DisconnectA2DP(disconnect_parameters_t * params)
{
    a2dp_disconnect_parameters_t * disconnect_params = (a2dp_disconnect_parameters_t *)params->source_params.data;
    audio_use_case_instance_t instance = AudioUseCase_GetInstanceForSource(params->source);
    kymeraAdaptation_StopA2DP(instance, disconnect_params);
}
#else
static void kymeraAdaptation_DisconnectA2DP(disconnect_parameters_t * params)
{
    a2dp_disconnect_parameters_t * disconnect_params = (a2dp_disconnect_parameters_t *)params->source_params.data;
    appKymeraA2dpStop(disconnect_params->seid, disconnect_params->source);
}
#endif

static void kymeraAdaptation_DisconnectUsbAudio(disconnect_parameters_t * params)
{
    usb_audio_disconnect_parameters_t * audio_params =
            (usb_audio_disconnect_parameters_t *)params->source_params.data;

    appKymeraUsbAudioStop(audio_params->source,
                          audio_params->kymera_stopped_handler);
}


static void kymeraAdaptation_DisconnectAudio(disconnect_parameters_t * params)
{
    switch(params->source.u.audio)
    {
        case audio_source_a2dp_1:
        case audio_source_a2dp_2:
            kymeraAdaptation_DisconnectA2DP(params);
            break;
        case audio_source_line_in:
            Kymera_StopWiredAnalogAudio();
            break;
        case audio_source_usb:
            kymeraAdaptation_DisconnectUsbAudio(params);
            break;
        default:
            Panic();
            break;
    }
}

void KymeraAdaptation_Connect(connect_parameters_t * params)
{
    switch(params->source.type)
    {
        case source_type_voice:
            kymeraAdaptation_ConnectVoice(params);
            break;
        case source_type_audio:
            kymeraAdaptation_ConnectAudio(params);
            break;
        default:
            Panic();
            break;
    }
}

void KymeraAdaptation_Disconnect(disconnect_parameters_t * params)
{
    switch(params->source.type)
    {
        case source_type_voice:
            kymeraAdaptation_DisconnectVoice(params);
            break;
        case source_type_audio:
            kymeraAdaptation_DisconnectAudio(params);
            break;
        default:
            Panic();
            break;
    }
}

static void kymeraAdaptation_SetVoiceVolume(generic_source_t source, volume_t volume)
{
    voice_source_t voice_source = source.u.voice;

    switch(voice_source)
    {
        case voice_source_hfp_1:
        case voice_source_hfp_2:
            appKymeraScoSetVolume(VolumeUtils_GetVolumeInDb(volume));
            break;

        case voice_source_usb:
            appKymeraUsbVoiceSetVolume(VolumeUtils_GetVolumeInDb(volume));
            break;
        default:
            break;
    }
}

#ifdef INCLUDE_AUDIO_DRIVER
static void kymeraAdaptation_SetVolume(generic_source_t source, audio_driver_set_volume_t volume_params)
{
    audio_use_case_instance_t instance = AudioUseCase_GetInstanceForSource(source);
    data_blob_t params = {.data = &volume_params, .data_length = sizeof(volume_params)};
    AudioDriver_IoctlUseCase(instance, audio_driver_set_volume, &params);
}
#endif

static void kymeraAdaptation_SetAudioVolume(generic_source_t source, volume_t volume)
{
    audio_source_t audio_source = source.u.audio;

    switch(audio_source)
    {
        case audio_source_a2dp_1:
        case audio_source_a2dp_2:
#ifdef INCLUDE_AUDIO_DRIVER
            kymeraAdaptation_SetVolume(source, volume);
#else
            appKymeraA2dpSetVolume(VolumeUtils_GetVolumeInDb(volume));
#endif
            break;

        case audio_source_usb:
            appKymeraUsbAudioSetVolume(VolumeUtils_GetVolumeInDb(volume));
            break;

        case audio_source_line_in:
            appKymeraWiredAudioSetVolume(VolumeUtils_GetVolumeInDb(volume));
            break;
        default:
            break;
    }
}

void KymeraAdaptation_SetVolume(volume_parameters_t * params)
{
    generic_source_t source = params->source;

    switch(params->source.type)
    {
        case source_type_voice:
            kymeraAdaptation_SetVoiceVolume(source, params->volume);
            break;

        case source_type_audio:
            kymeraAdaptation_SetAudioVolume(source, params->volume);
            break;

        default:
            Panic();
            break;
    }
}
