/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP Source for USB Wired Audio.
*/

#if defined(INCLUDE_A2DP_USB_SOURCE)

#include "kymera.h"
#include "kymera_a2dp.h"
#include "kymera_a2dp_source.h"
#include "kymera_chain_roles.h"
#include "kymera_usb_audio.h"
#include "kymera_dsp_clock.h"
#include "kymera_state.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "kymera_config.h"
#include "av.h"
#include "a2dp_profile_caps.h"


static void kymeraA2dpUsbSource_CreateInputChain(uint8 seid)
{
    const chain_config_t *config = NULL;
    DEBUG_LOG_FN_ENTRY("kymeraA2dpUsbSource_CreateInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_SBC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_sbc_encode_config;
            break;
        }
        case AV_SEID_APTX_CLASSIC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_CLASSIC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_aptx_classic_encode_config;
            break;
        }
        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_ADAPTIVE_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_aptx_adaptive_encode_config;
            break;
        }
        case AV_SEID_APTXHD_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTXHD_SRC");
            config = Kymera_GetChainConfigs()->chain_input_usb_aptxhd_encode_config;
            break;
        }
        default:
            Panic();
        break;
    }

    /* Create input chain */
    KymeraGetTaskData()->chain_input_handle = PanicNull(ChainCreate(config));
}

static void kymeraA2dpUsbSource_ConfigureUsbRx(const KYMERA_INTERNAL_USB_AUDIO_START_T *usb_params)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpUsbSource_ConfigureUsbRx");

    DEBUG_LOG_V_VERBOSE(" usb_params->sample_freq: %u", usb_params->sample_freq);
    DEBUG_LOG_V_VERBOSE(" usb_params->frame_size: %u", usb_params->frame_size);
    DEBUG_LOG_V_VERBOSE(" usb_params->channels: %u", usb_params->channels);
    DEBUG_LOG_V_VERBOSE(" usb_params->min_latency_ms: %u", usb_params->min_latency_ms);
    DEBUG_LOG_V_VERBOSE(" usb_params->max_latency_ms: %u", usb_params->max_latency_ms);
    DEBUG_LOG_V_VERBOSE(" usb_params->target_latency_ms: %u", usb_params->target_latency_ms);

    Operator usb_rx_op = ChainGetOperatorByRole(KymeraGetTaskData()->chain_input_handle, OPR_USB_AUDIO_RX);

    usb_config_t config;
    config.sample_rate = usb_params->sample_freq;
    config.sample_size = usb_params->frame_size;
    config.number_of_channels = usb_params->channels;

    OperatorsConfigureUsbAudio(usb_rx_op, config);

    /* In USB chains, the USB RX operator acts as the Time-To-Play buffer. */
    kymeraA2dpSource_ConfigureTtpBufferParams(usb_rx_op,
                                              usb_params->min_latency_ms,
                                              usb_params->max_latency_ms,
                                              usb_params->target_latency_ms);
}

static void kymeraA2dpUsbSource_ConfigureResampler(uint32 usb_sample_rate)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpUsbSource_ConfigureResampler");

    Operator resampler_op = ChainGetOperatorByRole(KymeraGetTaskData()->chain_input_handle, OPR_SPEAKER_RESAMPLER);

    /* USB may be running at a different sample rate to that negotiated for the
       A2DP media channel. Resample to match the two rates. */
    uint32 input_rate = usb_sample_rate;
    uint32 output_rate = KymeraGetTaskData()->a2dp_output_params->rate;

    DEBUG_LOG_INFO("KymeraA2dpUsbSource: Resampling %u -> %u", input_rate, output_rate);

    OperatorsResamplerSetConversionRate(resampler_op, input_rate, output_rate);
}

static void kymeraA2dpUsbSource_ConfigureInputChain(const KYMERA_INTERNAL_USB_AUDIO_START_T *usb_params)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpUsbSource_ConfigureInputChain");

    kymeraA2dpUsbSource_ConfigureUsbRx(usb_params);
    kymeraA2dpUsbSource_ConfigureResampler(usb_params->sample_freq);
    kymeraA2dpSource_ConfigureEncoder();
    kymeraA2dpSource_ConfigureSwitchedPassthrough();

    ChainConnect(KymeraGetTaskData()->chain_input_handle);
}

static void kymeraA2dpUsbSource_ConnectInput(Source media_source)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpUsbSource_ConnectInput");
    /* The media source may fail to connect to the input chain if the source
    disconnects between the time A2DP asks Kymera to start and this
    function being called. A2DP will subsequently ask Kymera to stop. */
    PanicFalse(ChainConnectInput(KymeraGetTaskData()->chain_input_handle, media_source, EPR_USB_FROM_HOST));
}

static void kymeraA2dpUsbSource_DisconnectInput(Source usb_source)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpUsbSource_DisconnectInput");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Disconnect USB input to chain, and dispose USB source */
    Sink usb_from_host = ChainGetInput(theKymera->chain_input_handle, EPR_USB_FROM_HOST);

    StreamDisconnect(usb_source, 0);
    StreamConnectDispose(usb_source);
    StreamDisconnect(NULL, usb_from_host);

    theKymera->output_rate = 0;
    theKymera->usb_rx = 0;
}

void KymeraUsbAudio_Start(KYMERA_INTERNAL_USB_AUDIO_START_T *msg)
{
    DEBUG_LOG_FN_ENTRY("KymeraUsbAudio_Start");

    const a2dp_codec_settings *codec_settings = KymeraGetTaskData()->a2dp_output_params;
    if (codec_settings == NULL)
    {
        DEBUG_LOG_ERROR("KymeraUsbAudio_Start: A2DP output params not set");
        return;
    }

    Source usb_audio_source = msg->spkr_src;
    /* We have to disconnect the previous source stream. This may be hiding
       an underlying issue */
    StreamDisconnect(usb_audio_source, NULL);

    kymeraA2dpUsbSource_CreateInputChain(codec_settings->seid);
    kymeraA2dpUsbSource_ConfigureInputChain(msg);

    PanicFalse(kymeraA2dpSource_ConfigurePacketiser());

    appKymeraSetState(KYMERA_STATE_USB_AUDIO_ACTIVE);
    appKymeraConfigureDspPowerMode();

    switch (codec_settings->seid)
    {
        case AV_SEID_APTX_CLASSIC_SRC:
        DEBUG_LOG_INFO("Starting USB audio aptX Classic, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        case AV_SEID_APTX_ADAPTIVE_SRC:
        DEBUG_LOG_INFO("Starting USB audio aptX Adaptive, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        case AV_SEID_SBC_SRC:
        DEBUG_LOG_INFO("Starting USB audio SBC, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        case AV_SEID_APTXHD_SRC:
        DEBUG_LOG_INFO("Starting USB audio aptX HD, Latencies: target %u, min %u, max %u",
                       msg->target_latency_ms,
                       msg->min_latency_ms,
                       msg->max_latency_ms);
        break;
        default:
        break;
    }

    /* Connect USB audio source to chain input. */
    kymeraA2dpUsbSource_ConnectInput(usb_audio_source);

    kymeraA2dpSource_StartChain();
}

void KymeraUsbAudio_Stop(KYMERA_INTERNAL_USB_AUDIO_STOP_T *audio_params)
{
    DEBUG_LOG_FN_ENTRY("KymeraUsbAudio_Stop");
    switch (appKymeraGetState())
    {
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
            /* Stop chain before disconnecting */
            kymeraA2dpSource_StopChain();
            kymeraA2dpUsbSource_DisconnectInput(audio_params->source);
            kymeraA2dpSource_DestroyChain();
            appKymeraSetState(KYMERA_STATE_IDLE);
            PanicZero(audio_params->kymera_stopped_handler);
            audio_params->kymera_stopped_handler(audio_params->source);
        break;

        case KYMERA_STATE_IDLE:
        break;

        default:
            /* Report, but ignore attempts to stop in invalid states */
            DEBUG_LOG("KymeraUsbAudio_Stop, invalid state %u", appKymeraGetState());
        break;
    }
}

void KymeraUsbAudio_SetVolume(int16 volume_in_db)
{
    UNUSED(volume_in_db);
    DEBUG_LOG_V_VERBOSE("KymeraUsbAudio_SetVolume: Not Handled in kymera_a2dp_usb_source");
}

#endif
