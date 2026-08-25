/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP Source for analog wired audio.
*/

#ifdef INCLUDE_A2DP_ANALOG_SOURCE
#include "kymera_wired_analog.h"
#include "kymera_a2dp.h"
#include "kymera_a2dp_source.h"
#include "kymera_state.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "av.h"
#include "kymera.h"
#include "kymera_chain_roles.h"
#include "a2dp_profile_caps.h"
#include "wired_audio_source.h"
#include "kymera_dsp_clock.h"

#include <logging.h>


static Source kymeraA2dpAnalogSource_GetSource(audio_channel channel, uint8 inst, uint32 rate)
{
#define SAMPLE_SIZE 24
    Source source;
    analogue_input_params params = {
        .pre_amp = FALSE,
        .gain = 0x09, /* for line-in set to 0dB */
        .instance = 0, /* Place holder */
        .enable_24_bit_resolution = (SAMPLE_SIZE == 24)
        };

    DEBUG_LOG_VERBOSE("SourcekymeraWiredAnalog_GetSource, Get source for Channel: %u, Instance: %u, Sample Rate: %u and Bits: %u", channel, inst, rate, SAMPLE_SIZE);
    params.instance = inst;
    source = AudioPluginAnalogueInputSetup(channel, params, rate);
    PanicFalse(SourceConfigure(source, STREAM_AUDIO_SAMPLE_SIZE, SAMPLE_SIZE));

    return source;
}


static void kymeraA2dpAnalogSource_CreateInputChain(uint8 seid)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_CreateInputChain");

    const chain_config_t *config = NULL;

    switch (seid)
    {
        case AV_SEID_SBC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_SBC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_sbc_encode_config;
            break;
        }
        case AV_SEID_APTX_CLASSIC_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_CLASSIC_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_aptx_classic_encode_config;
            break;
        }
        case AV_SEID_APTXHD_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTXHD_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_aptxhd_encode_config;
            break;
        }
        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            DEBUG_LOG_DEBUG("Encoder Config: AV_SEID_APTX_ADAPTIVE_SRC");
            config = Kymera_GetChainConfigs()->chain_input_wired_aptx_adaptive_encode_config;
            break;
        }
        default:
            Panic();
        break;
    }

    /* Create input chain */
    KymeraGetTaskData()->chain_input_handle = PanicNull(ChainCreate(config));
}

static void kymeraA2dpAnalogSource_ConfigureLatencyBuffer(const wired_audio_config_t *audio_config)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_ConfigureLatencyBuffer");

    Operator ttp_passthrough = PanicZero(ChainGetOperatorByRole(KymeraGetTaskData()->chain_input_handle, OPR_LATENCY_BUFFER));

    OperatorsStandardSetSampleRate(ttp_passthrough, audio_config->rate);
    OperatorsSetPassthroughDataFormat(ttp_passthrough, operator_data_format_pcm);

    kymeraA2dpSource_ConfigureTtpBufferParams(ttp_passthrough,
                                              audio_config->min_latency,
                                              audio_config->max_latency,
                                              audio_config->target_latency);
}

static void kymeraA2dpAnalogSource_ConfigureInputChain(const wired_audio_config_t *audio_config)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_ConfigureInputChain");

    kymeraA2dpAnalogSource_ConfigureLatencyBuffer(audio_config);
    kymeraA2dpSource_ConfigureEncoder();
    kymeraA2dpSource_ConfigureSwitchedPassthrough();

    ChainConnect(KymeraGetTaskData()->chain_input_handle);
}

static bool kymeraA2dpAnalogSource_ConnectLineIn(uint32 rate)
{
    Source line_in_l = kymeraA2dpAnalogSource_GetSource(appConfigLeftAudioChannel(), appConfigLeftAudioInstance(), rate /* for now input/output rate are same */);
    Source line_in_r = kymeraA2dpAnalogSource_GetSource(appConfigRightAudioChannel(), appConfigRightAudioInstance(), rate /* for now input/output rate are same */);
    /* if stereo, then synchronize */
    if(line_in_r)
        SourceSynchronise(line_in_l, line_in_r);

    /* The media source may fail to connect to the input chain if the source
    disconnects between the time wired analog audio asks Kymera to start and this
    function being called. wired analog audio will subsequently ask Kymera to stop. */
    if (line_in_l != NULL && line_in_r !=NULL)
    {
        if (ChainConnectInput(KymeraGetTaskData()->chain_input_handle, line_in_l, EPR_WIRED_STEREO_INPUT_L))
        {
            if (ChainConnectInput(KymeraGetTaskData()->chain_input_handle, line_in_r, EPR_WIRED_STEREO_INPUT_R))
            {
                return TRUE;
            }
            else {
                DEBUG_LOG_ERROR("kymeraA2dpAnalogSource_ConnectLineIn: R Could not connect input");
            }
        }
        else {
            DEBUG_LOG_ERROR("kymeraA2dpAnalogSource_ConnectLineIn: L Could not connect input");
        }
    }
    else {
        DEBUG_LOG_ERROR("kymeraA2dpAnalogSource_ConnectLineIn: A Line in Source was NULL");
    }
    return FALSE;
}

static void kymeraA2dpAnalogSource_DisconnectLineIn(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpAnalogSource_DisconnectLineIn");

    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Disconnect line inputs to chain */
    Sink to_ttp_l = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_L);
    Sink to_ttp_r = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_R);
    DEBUG_LOG_V_VERBOSE("kymeraA2dpAnalogSource_DisconnectLineIn, l-sink(%p), r-sink(%p)", to_ttp_l, to_ttp_r);

    StreamDisconnect(NULL, to_ttp_l);
    StreamDisconnect(NULL, to_ttp_r);
}

void KymeraWiredAnalog_StartPlayingAudio(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg)
{
    DEBUG_LOG_FN_ENTRY("KymeraWiredAnalog_StartPlayingAudio");

    const a2dp_codec_settings *codec_settings = KymeraGetTaskData()->a2dp_output_params;

    wired_audio_config_t audio_config;
    audio_config.rate = codec_settings->rate;
    audio_config.min_latency = msg->min_latency;
    audio_config.max_latency = msg->max_latency;
    audio_config.target_latency = msg->target_latency;

    kymeraA2dpAnalogSource_CreateInputChain(codec_settings->seid);
    kymeraA2dpAnalogSource_ConfigureInputChain(&audio_config);

    PanicFalse(kymeraA2dpAnalogSource_ConnectLineIn(codec_settings->rate));
    PanicFalse(kymeraA2dpSource_ConfigurePacketiser());

    appKymeraSetState(KYMERA_STATE_WIRED_A2DP_STREAMING);
    appKymeraConfigureDspPowerMode();

    switch (codec_settings->seid)
    {
        case AV_SEID_APTX_CLASSIC_SRC:
        DEBUG_LOG_INFO("Starting Analog audio aptX Classic, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;

        case AV_SEID_APTXHD_SRC:
        DEBUG_LOG_INFO("Starting Analog audio aptX HD, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;

        case AV_SEID_APTX_ADAPTIVE_SRC:
        DEBUG_LOG_INFO("Starting Analog audio aptX Adaptive, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;

        case AV_SEID_SBC_SRC:
        DEBUG_LOG_INFO("Starting Analog audio SBC, Latencies: target %u, min %u, max %u",
                       msg->target_latency,
                       msg->min_latency,
                       msg->max_latency);
        break;
        default:
        break;
    }
    kymeraA2dpSource_StartChain();
}

void KymeraWiredAnalog_StopPlayingAudio(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraWiredAnalog_StopPlayingAudio");

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_WIRED_A2DP_STREAMING:
            /* Stop chain before disconnecting */
            kymeraA2dpSource_StopChain();
            kymeraA2dpAnalogSource_DisconnectLineIn();
            kymeraA2dpSource_DestroyChain();
            appKymeraSetState(KYMERA_STATE_IDLE);
        break;

        case KYMERA_STATE_IDLE:
        break;

        default:
            /* Report, but ignore attempts to stop in invalid states */
            DEBUG_LOG("KymeraWiredAnalog_StopPlayingAudio, invalid state %u", appKymeraGetState());
        break;
    }
}

#endif
