/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_setup_audio.c
\brief      Module Conifgure Audio chains for headset application
*/

#include "kymera.h"
#include "source_prediction.h"
#include "kymera_setup.h"
#include "wired_audio_source.h"
#include "cap_id_prim.h"

#include "headset_cap_ids.h"
#include "headset_setup_audio.h"

#ifdef INCLUDE_AUDIO_DRIVER
#include "audio_use_case_instance.h"
#include "audio_driver_a2dp.h"
#endif

#include "chains/chain_sco_nb.h"
#include "chains/chain_sco_wb.h"
#include "chains/chain_sco_swb.h"
#include "chains/chain_sco_nb_2mic.h"
#include "chains/chain_sco_wb_2mic.h"
#include "chains/chain_sco_swb_2mic.h"
#include "chains/chain_sco_nb_2mic_binaural.h"
#include "chains/chain_sco_wb_2mic_binaural.h"
#include "chains/chain_sco_swb_2mic_binaural.h"
#include "chains/chain_output_volume_stereo.h"
#include "chains/chain_output_volume_mono.h"
#include "chains/chain_output_volume_common.h"
#include "chains/chain_prompt_sbc.h"
#include "chains/chain_prompt_aac.h"
#include "chains/chain_prompt_pcm.h"
#include "chains/chain_tone_gen.h"
#include "chains/chain_aec.h"
#include "chains/chain_va_encode_msbc.h"
#include "chains/chain_va_encode_opus.h"
#include "chains/chain_va_encode_sbc.h"
#include "chains/chain_va_mic_1mic.h"
#include "chains/chain_va_mic_1mic_cvc.h"
#include "chains/chain_va_mic_1mic_cvc_no_vad_wuw.h"
#include "chains/chain_va_mic_1mic_cvc_wuw.h"
#include "chains/chain_va_mic_2mic_cvc.h"
#include "chains/chain_va_mic_2mic_cvc_wuw.h"
#include "chains/chain_va_wuw_qva.h"
#include "chains/chain_va_wuw_gva.h"
#include "chains/chain_va_wuw_apva.h"
#include "chains/chain_anc.h"

#include "chains/chain_input_sbc_stereo.h"
#include "chains/chain_input_aptx_stereo.h"
#include "chains/chain_input_aptxhd_stereo.h"
#include "chains/chain_input_aptx_adaptive_stereo.h"
#include "chains/chain_input_aptx_adaptive_stereo_q2q.h"
#include "chains/chain_input_aac_stereo.h"
#include "chains/chain_input_wired_analog_stereo.h"
#include "chains/chain_input_usb_stereo.h"
#include "chains/chain_usb_voice_rx_mono.h"
#include "chains/chain_usb_voice_rx_stereo.h"
#include "chains/chain_usb_voice_wb.h"
#include "chains/chain_usb_voice_wb_2mic.h"
#include "chains/chain_usb_voice_wb_2mic_binaural.h"
#include "chains/chain_usb_voice_nb.h"
#include "chains/chain_usb_voice_nb_2mic.h"
#include "chains/chain_usb_voice_nb_2mic_binaural.h"

#include "chains/chain_music_processing.h"
#ifdef INCLUDE_MUSIC_PROCESSING
        #include "chains/chain_music_processing_user_eq.h"
#endif



#include "chains/chain_mic_resampler.h"

#include "chains/chain_va_graph_manager.h"

#ifdef ENABLE_SIMPLE_SPEAKER
#include "chains/chain_spk_sco_nb.h"
#include "chains/chain_spk_sco_wb.h"
#include "chains/chain_spk_sco_swb.h"
#include "chains/chain_spk_usb_voice_nb.h"
#include "chains/chain_spk_usb_voice_wb.h"
#if defined(KYMERA_SCO_USE_2MIC) || defined(KYMERA_SCO_USE_2MIC_BINAURAL)
      #error KYMERA_SCO_USE_2MIC, KYMERA_SCO_USE_2MIC_BINAURAL is not supported in Simple Speaker build
#endif
#endif  /* ENABLE_SIMPLE_SPEAKER */

#ifdef INCLUDE_DECODERS_ON_P1
#define DOWNLOAD_TYPE_DECODER capability_load_to_p0_use_on_both
#else
#define DOWNLOAD_TYPE_DECODER capability_load_to_p0_use_on_p0_only
#endif

static const capability_bundle_t capability_bundle[] =
{
#ifdef DOWNLOAD_AEC_REF
    {
        "download_aec_reference.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_VOLUME_CONTROL
    {
        "download_volume_control.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_VA_GRAPH_MANAGER
    {
        "download_va_graph_manager.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_CVC_FBC
    {
        "download_cvc_fbc.edkcs",
#ifdef __QCC514X__
        capability_load_to_p1_use_on_both
#else
        capability_load_to_p0_use_on_p0_only
#endif
    },
#endif
#ifdef DOWNLOAD_GVA
    {
#if defined(__QCC305X__) || defined(__QCC307X__)
        "download_gva.edkcs",
#else
        "download_gva.dkcs",
#endif
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_APVA
    {
#if defined(__QCC305X__) || defined(__QCC307X__)
        "download_apva.edkcs",
#else
        "download_apva.dkcs",
#endif
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_OPUS_CELT_ENCODE
    {
        "download_opus_celt_encode.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_APTX_ADAPTIVE_DECODE
    {
        "download_aptx_adaptive_decode.edkcs",
        DOWNLOAD_TYPE_DECODER
    },
#endif
#ifdef DOWNLOAD_SWITCHED_PASSTHROUGH
    {
        "download_switched_passthrough_consumer.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_SWBS
    {
        "download_swbs.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_LC3_ENCODE_SCO_ISO
    {
        "download_lc3_encode_sco_iso.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#ifdef DOWNLOAD_LC3_DECODE_SCO_ISO
    {
        "download_lc3_decode_sco_iso.edkcs",
#ifdef INCLUDE_RESTRICT_LC3_TO_P0
        capability_load_to_p0_use_on_p0_only
#else
        DOWNLOAD_TYPE_DECODER
#endif
    },
#ifdef SPLITTER_DOWNLOAD
    {
        "download_splitter.edkcs",
        capability_load_to_p0_use_on_p0_only
    },
#endif
#endif
    {
        0, 0
    }
};

static const capability_bundle_config_t bundle_config = {capability_bundle, ARRAY_DIM(capability_bundle) - 1};

static const kymera_chain_configs_t chain_configs = {
    .chain_output_volume_mono_config = &chain_output_volume_mono_config,
    .chain_output_volume_stereo_config = &chain_output_volume_stereo_config,
    .chain_output_volume_common_config = &chain_output_volume_common_config,
    .chain_tone_gen_config = &chain_tone_gen_config,
    .chain_aec_config = &chain_aec_config,
    .chain_prompt_pcm_config = &chain_prompt_pcm_config,
    .chain_anc_config = &chain_anc_config,
#ifdef INCLUDE_DECODERS_ON_P1
    .chain_input_sbc_stereo_config = &chain_input_sbc_stereo_config_p1,
    .chain_input_aptx_stereo_config = &chain_input_aptx_stereo_config_p1,
    .chain_input_aptxhd_stereo_config = &chain_input_aptxhd_stereo_config_p1,
    .chain_input_aac_stereo_config = &chain_input_aac_stereo_config_p1,
    .chain_input_aptx_adaptive_stereo_config = &chain_input_aptx_adaptive_stereo_config_p1,
    .chain_input_aptx_adaptive_stereo_q2q_config = &chain_input_aptx_adaptive_stereo_q2q_config_p1,
    .chain_prompt_sbc_config = &chain_prompt_sbc_config_p1,
    .chain_prompt_aac_config = &chain_prompt_aac_config_p1,
#else
#if defined(INCLUDE_WUW) && (defined(__QCC304X__) || defined(__QCC514X__) || defined(__QCC305X__) || defined(__QCC515X__))
#error Wake-up word requires decoders to be on P1
#endif
    .chain_input_sbc_stereo_config = &chain_input_sbc_stereo_config_p0,
    .chain_input_aptx_stereo_config = &chain_input_aptx_stereo_config_p0,
    .chain_input_aptxhd_stereo_config = &chain_input_aptxhd_stereo_config_p0,
    .chain_input_aac_stereo_config = &chain_input_aac_stereo_config_p0,
    .chain_prompt_sbc_config = &chain_prompt_sbc_config_p0,
    .chain_prompt_aac_config = &chain_prompt_aac_config_p0,
    .chain_input_aptx_adaptive_stereo_config = &chain_input_aptx_adaptive_stereo_config_p0,
    .chain_input_aptx_adaptive_stereo_q2q_config = &chain_input_aptx_adaptive_stereo_q2q_config_p0,
#endif

#ifdef INCLUDE_SPEAKER_EQ
#ifdef INCLUDE_MUSIC_PROCESSING
    .chain_music_processing_config_p0 = &chain_music_processing_user_eq_config_p0,
    .chain_music_processing_config_p1 = &chain_music_processing_user_eq_config_p1,
#else
    .chain_music_processing_config_p0 = &chain_music_processing_config_p0,
    .chain_music_processing_config_p1 = &chain_music_processing_config_p1,
#endif
#endif

    .chain_input_wired_analog_stereo_config = &chain_input_wired_analog_stereo_config,
    .chain_input_usb_stereo_config = &chain_input_usb_stereo_config,
    .chain_usb_voice_rx_mono_config = &chain_usb_voice_rx_mono_config,
    .chain_usb_voice_rx_stereo_config = &chain_usb_voice_rx_stereo_config,
#ifdef ENABLE_SIMPLE_SPEAKER
    .chain_usb_voice_nb_config = &chain_spk_usb_voice_nb_config,
    .chain_usb_voice_wb_config = &chain_spk_usb_voice_wb_config,
#else
    .chain_usb_voice_nb_config = &chain_usb_voice_nb_config,
    .chain_usb_voice_wb_config = &chain_usb_voice_wb_config,
#endif /* ENABLE_SIMPLE_SPEAKER */
    .chain_usb_voice_wb_2mic_config = &chain_usb_voice_wb_2mic_config,
    .chain_usb_voice_wb_2mic_binaural_config = &chain_usb_voice_wb_2mic_binaural_config,
    .chain_usb_voice_nb_2mic_config = &chain_usb_voice_nb_2mic_config,
    .chain_usb_voice_nb_2mic_binaural_config = &chain_usb_voice_nb_2mic_binaural_config,
    .chain_mic_resampler_config = &chain_mic_resampler_config,
    .chain_va_graph_manager_config = &chain_va_graph_manager_config,
};

static const appKymeraVaEncodeChainInfo va_encode_chain_info[] =
{
    {{va_audio_codec_sbc}, &chain_va_encode_sbc_config},
    {{va_audio_codec_msbc}, &chain_va_encode_msbc_config},
    {{va_audio_codec_opus}, &chain_va_encode_opus_config}
};

static const appKymeraVaEncodeChainTable va_encode_chain_table =
{
    .chain_table = va_encode_chain_info,
    .table_length = ARRAY_DIM(va_encode_chain_info)
};

static const appKymeraVaMicChainInfo va_mic_chain_info[] =
{
  /*{{  WuW,   CVC, mics}, chain_to_use}*/
#ifdef KYMERA_VA_USE_CHAIN_WITHOUT_CVC
    {{FALSE,  FALSE,   1}, &chain_va_mic_1mic_config},
#else
#ifdef INCLUDE_WUW
#ifdef KYMERA_VA_USE_CHAIN_WITHOUT_VAD
    {{ TRUE,  TRUE,    1}, &chain_va_mic_1mic_cvc_no_vad_wuw_config},
#else
    {{ TRUE,  TRUE,    1}, &chain_va_mic_1mic_cvc_wuw_config},
    {{ TRUE,  TRUE,    2}, &chain_va_mic_2mic_cvc_wuw_config},
#endif /* KYMERA_VA_USE_CHAIN_WITHOUT_VAD */
#endif /* INCLUDE_WUW */
    {{FALSE,  TRUE,    1}, &chain_va_mic_1mic_cvc_config},
    {{FALSE,  TRUE,    2}, &chain_va_mic_2mic_cvc_config}
#endif /* KYMERA_VA_USE_CHAIN_WITHOUT_CVC */
};

static const kymera_callback_configs_t callback_configs = {
    .GetA2dpParametersPrediction = &SourcePrediction_GetA2dpParametersPrediction,
};

static const appKymeraVaMicChainTable va_mic_chain_table =
{
    .chain_table = va_mic_chain_info,
    .table_length = ARRAY_DIM(va_mic_chain_info)
};

#ifdef INCLUDE_WUW
static const appKymeraVaWuwChainInfo va_wuw_chain_info[] =
{
    {{va_wuw_engine_qva}, &chain_va_wuw_qva_config},
#ifdef INCLUDE_GAA_WUW
    {{va_wuw_engine_gva}, &chain_va_wuw_gva_config},
#endif /* INCLUDE_GAA_WUW */
#ifdef INCLUDE_AMA_WUW
    {{va_wuw_engine_apva}, &chain_va_wuw_apva_config}
#endif /* INCLUDE_AMA_WUW */
};

static const appKymeraVaWuwChainTable va_wuw_chain_table =
{
    .chain_table = va_wuw_chain_info,
    .table_length = ARRAY_DIM(va_wuw_chain_info)
};
#endif /* INCLUDE_WUW */

const appKymeraScoChainInfo kymera_sco_chain_table[] =
{
#ifdef KYMERA_SCO_USE_2MIC
#ifdef KYMERA_SCO_USE_2MIC_BINAURAL

    /* 2-mic cVc Binaural configurations
     sco_mode   mic_cfg   chain                            rate */
    { SCO_NB,   2,    &chain_sco_nb_2mic_binaural_config,  8000 },
    { SCO_WB,   2,    &chain_sco_wb_2mic_binaural_config, 16000 },
    { SCO_SWB,  2,    &chain_sco_swb_2mic_binaural_config, 32000 },
#else  /* KYMERA_SCO_USE_2MIC_BINAURAL */

    /* 2-mic cVc Endfire configurations
     sco_mode  mic_cfg   chain                      rate */
    { SCO_NB,  2,       &chain_sco_nb_2mic_config,  8000 },
    { SCO_WB,  2,       &chain_sco_wb_2mic_config,  16000 },
    { SCO_SWB, 2,       &chain_sco_swb_2mic_config, 32000 },
#endif /* KYMERA_SCO_USE_2MIC_BINAURAL */
#else   /* KYMERA_SCO_USE_2MIC */

    /* 1-mic CVC configurations
    sco_mode   mic_cfg   chain                rate */
#ifdef ENABLE_SIMPLE_SPEAKER
    { SCO_NB,  1,      &chain_spk_sco_nb_config,  8000 },
    { SCO_WB,  1,      &chain_spk_sco_wb_config,  16000 },
    { SCO_SWB, 1,      &chain_spk_sco_swb_config, 32000 },
#else
    { SCO_NB,  1,      &chain_sco_nb_config,  8000 },
    { SCO_WB,  1,      &chain_sco_wb_config,  16000 },
    { SCO_SWB, 1,      &chain_sco_swb_config, 32000 },
#endif /* ENABLE_SIMPLE_SPEAKER */
#endif /* KYMERA_SCO_USE_2MIC */

    { NO_SCO }
};


const wired_audio_config_t wired_audio_config =
{
    .rate = 48000,
    .min_latency = 10,/*! in milli-seconds */
    .max_latency = 40,/*! in milli-seconds */
    .target_latency = 30 /*! in milli-seconds */
};

const audio_output_config_t audio_hw_output_config =
{
#ifdef ENABLE_I2S_OUTPUT
    .mapping = {
        {audio_output_type_i2s, audio_output_hardware_instance_0, audio_output_channel_a },
        {audio_output_type_i2s, audio_output_hardware_instance_0, audio_output_channel_b }
    },
    .gain_type = {audio_output_gain_digital, audio_output_gain_digital},
#ifdef ENABLE_I2S_OUTPUT_24BIT
    .output_resolution_mode = audio_output_24_bit,
#else
    .output_resolution_mode = audio_output_16_bit,
#endif
    .fixed_hw_gain = -1440 /* -24dB */
#else
    .mapping = {
        {audio_output_type_dac, audio_output_hardware_instance_0, audio_output_channel_a },
        {audio_output_type_dac, audio_output_hardware_instance_0, audio_output_channel_b }
    },
    .gain_type = {audio_output_gain_none, audio_output_gain_none},
    .output_resolution_mode = audio_output_24_bit,
    .fixed_hw_gain = 0
#endif
};

static const kymera_mic_config_t kymera_mic_config =
{
#ifdef SPLITTER_DOWNLOAD
    .cap_id_splitter = (capability_id_t)CAP_ID_DOWNLOAD_SPLITTER
#else
    .cap_id_splitter = (capability_id_t)CAP_ID_SPLITTER
#endif
};



static void headset_SetKymeraChains(void)
{
    Kymera_SetChainConfigs(&chain_configs);
    Kymera_SetScoChainTable(kymera_sco_chain_table);
    Kymera_SetVaMicChainTable(&va_mic_chain_table);
    Kymera_SetVaEncodeChainTable(&va_encode_chain_table);
#ifdef INCLUDE_WUW
    Kymera_SetVaWuwChainTable(&va_wuw_chain_table);
#endif /* INCLUDE_WUW */
}

static void headset_InitAudioDriver(void)
{
#ifdef INCLUDE_AUDIO_DRIVER
    AudioDriverA2dp_Init();
    generic_source_t source = {.type = source_type_audio, .u = {.audio = audio_source_a2dp_1}};
    AudioUseCase_CreateInstanceForSource(audio_use_case_a2dp, source);
    source.u.audio = audio_source_a2dp_2;
    AudioUseCase_CreateInstanceForSource(audio_use_case_a2dp, source);
#endif
}

bool Headset_InitAudio(Task init_task)
{
    bool status;
    Kymera_SetBundleConfig(&bundle_config);
    status = appKymeraInit(init_task);
    if (status)
    {
        headset_SetKymeraChains();
        Kymera_MicInit(&kymera_mic_config);
        Kymera_SetCallbackConfigs(&callback_configs);
#ifdef INCLUDE_WUW
        Kymera_StoreLargestWuwEngine();
#endif
        WiredAudioSource_Configure(&wired_audio_config);
        AudioOutputInit(&audio_hw_output_config);
        headset_InitAudioDriver();
    }
    return status;
}
