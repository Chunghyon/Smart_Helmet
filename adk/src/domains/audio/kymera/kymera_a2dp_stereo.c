/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP for stereo
*/

#ifdef INCLUDE_STEREO

#include "kymera_a2dp.h"
#include "kymera_a2dp_private.h"
#include "kymera_dsp_clock.h"
#include "kymera_buffer_utils.h"
#include "kymera_state.h"
#include "kymera_output_if.h"
#include "kymera_source_sync.h"
#include "kymera_latency_manager.h"
#include "kymera_music_processing.h"
#include "kymera_leakthrough.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "timestamp_event.h"
#include "av.h"
#include "a2dp_profile_config.h"
#include "handset_service.h"
#include <operators.h>
#include <logging.h>

static bool appKymeraA2dpGetPreferredChainOutput(kymera_output_chain_config *config);
static const output_callbacks_t appKymeraA2dpStereoCallbacks =
{
   .OutputGetPreferredChainConfig = appKymeraA2dpGetPreferredChainOutput,
};

static const output_registry_entry_t output_info =
{
    .user = output_user_a2dp,
    .connection = output_connection_stereo,
    .callbacks = &appKymeraA2dpStereoCallbacks,
};

static void appKymeraA2dpPopulateOutputChainConfig(a2dp_params_getter_t a2dp_params, kymera_output_chain_config *config)
{
    unsigned kick_period = KICK_PERIOD_FAST;
    unsigned block_size = DEFAULT_CODEC_BLOCK_SIZE;
    unsigned kp_multiplier = 5;
    unsigned kp_divider = 2;
    unsigned input_terminal_delta_buffer_size = 0;

    DEBUG_LOG("appKymeraA2dpPopulateOutputChainConfig");

    switch (a2dp_params.seid)
    {
        case AV_SEID_SBC_SNK:
            kick_period = KICK_PERIOD_MASTER_SBC;
            block_size = SBC_CODEC_BLOCK_SIZE;
            break;

        case AV_SEID_AAC_SNK:
            kick_period = KICK_PERIOD_MASTER_AAC;
            block_size = AAC_CODEC_BLOCK_SIZE;
           /* if AEC Ref is included in the audio graph, then there are possibilities that
               AAC audio graph could have MIPS issue when graph is running @ 32MHz.
               So, input terminal buffer for source_sync operator should have some extra delta
               to offset this issue */
            if(Kymera_OutputIsAecAlwaysUsed())
            {
                /* the delta increase in buffer size should be calculated such that the overall
                    terminal buffer size should be smaller than 2*decoder_block_size */
                input_terminal_delta_buffer_size = 500;

                /* on the similar grounds, also increase the output buffer of source_sync by 4times kp */
                kp_multiplier = 4;
                kp_divider = 0;
            }
            break;

        case AV_SEID_APTX_SNK:
        case AV_SEID_APTXHD_SNK:
            kick_period = KICK_PERIOD_MASTER_APTX;
            block_size = APTX_CODEC_BLOCK_SIZE;
        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
            kick_period = KICK_PERIOD_MASTER_APTX_ADAPTIVE;
            block_size = APTX_CODEC_BLOCK_SIZE;
        break;
#endif

        default :
            Panic();
            break;
    }

    if (Kymera_FastKickPeriodInGamingMode() && Kymera_LatencyManagerIsGamingModeEnabled())
    {
        kick_period = KICK_PERIOD_FAST;
    }

    config->rate = a2dp_params.rate;
    config->kick_period = kick_period;
    config->source_sync_kick_back_threshold = block_size;
    if (kick_period == KICK_PERIOD_SLOW)
    {
        config->source_sync_max_period = appKymeraGetSlowKickSourceSyncPeriod(TRUE);
        config->source_sync_min_period = appKymeraGetSlowKickSourceSyncPeriod(FALSE);
    }
    else if (kick_period == KICK_PERIOD_FAST)
    {
        config->source_sync_max_period = appKymeraGetFastKickSourceSyncPeriod(TRUE);
        config->source_sync_min_period = appKymeraGetFastKickSourceSyncPeriod(FALSE);
    }
    config->set_source_sync_min_period = TRUE;
    config->set_source_sync_max_period = TRUE;
    config->set_source_sync_kick_back_threshold = TRUE;

    /* Output buffer is 2.5*KP or 4*KP (if AEC Ref is in the audio chain) */
    appKymeraSetSourceSyncConfigOutputBufferSize(config, kp_multiplier, kp_divider);
    appKymeraSetSourceSyncConfigInputBufferSize(config, (block_size + input_terminal_delta_buffer_size));
    config->chain_type = output_chain_stereo;
}

static bool appKymeraA2dpGetA2dpParametersPrediction(uint32 *rate, uint8 *seid)
{
    const kymera_callback_configs_t *config = Kymera_GetCallbackConfigs();
    DEBUG_LOG("appKymeraA2dpGetA2dpParametersPrediction");
    if ((config) && (config->GetA2dpParametersPrediction))
    {
        return config->GetA2dpParametersPrediction(rate, seid);
    }
    return FALSE;
}

static bool appKymeraA2dpGetPreferredChainOutput(kymera_output_chain_config *config)
{
    uint32 rate;
    uint8 seid;
    bool a2dp_params_are_valid = appKymeraA2dpGetA2dpParametersPrediction(&rate, &seid);
    if (a2dp_params_are_valid)
    {
        a2dp_params_getter_t a2dp_params;
        a2dp_params.rate = rate;
        a2dp_params.seid = seid;

        appKymeraA2dpPopulateOutputChainConfig(a2dp_params, config);
    }
    return a2dp_params_are_valid;
}

static void appKymeraCreateInputChain(kymeraTaskData *theKymera, uint8 seid)
{
    const chain_config_t *config = NULL;
    DEBUG_LOG("appKymeraCreateInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            DEBUG_LOG("Create SBC input chain");
            config = Kymera_GetChainConfigs()->chain_input_sbc_stereo_config;
        break;

        case AV_SEID_AAC_SNK:
            DEBUG_LOG("Create AAC input chain");
            config = Kymera_GetChainConfigs()->chain_input_aac_stereo_config;
        break;

        case AV_SEID_APTX_SNK:
            DEBUG_LOG("Create aptX Classic input chain");
            config = Kymera_GetChainConfigs()->chain_input_aptx_stereo_config;
        break;

        case AV_SEID_APTXHD_SNK:
            DEBUG_LOG("Create aptX HD input chain");
            config = Kymera_GetChainConfigs()->chain_input_aptxhd_stereo_config;

        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
             DEBUG_LOG("Create aptX Adaptive input chain");
             if (theKymera->q2q_mode)
                 config =  Kymera_GetChainConfigs()->chain_input_aptx_adaptive_stereo_q2q_config;
             else
                 config =  Kymera_GetChainConfigs()->chain_input_aptx_adaptive_stereo_config;
        break;
#endif
        default:
            Panic();
        break;
    }

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static void appKymeraConfigureInputChain(kymeraTaskData *theKymera,
                                         uint8 seid, uint32 rate,
                                         bool cp_header_enabled,
                                         aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymera_chain_handle_t chain_handle = theKymera->chain_input_handle;
    rtp_codec_type_t rtp_codec = -1;
    rtp_working_mode_t mode = rtp_decode;
    Operator op_aac_decoder;
#ifdef INCLUDE_APTX_ADAPTIVE
    Operator op;
#endif
    Operator op_rtp_decoder = ChainGetOperatorByRole(chain_handle, OPR_RTP_DECODER);
    uint32_t rtp_buffer_size = PRE_DECODER_BUFFER_SIZE;
    uint32_t max_aptx_bitrate = 0;
    DEBUG_LOG("appKymeraConfigureInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            DEBUG_LOG("configure SBC input chain");
            rtp_codec = rtp_codec_type_sbc;
        break;

        case AV_SEID_AAC_SNK:
            DEBUG_LOG("configure AAC input chain");
            rtp_codec = rtp_codec_type_aac;
            op_aac_decoder = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_AAC_DECODER));
            OperatorsRtpSetAacCodec(op_rtp_decoder, op_aac_decoder);
        break;
		
        case AV_SEID_APTX_SNK:
            DEBUG_LOG("configure aptX Classic input chain");
            rtp_codec = rtp_codec_type_aptx;
            if (!cp_header_enabled)
            {
                mode = rtp_ttp_only;
            }
        break;

        case AV_SEID_APTXHD_SNK:
            DEBUG_LOG("configure aptX HD input chain");
            rtp_codec = rtp_codec_type_aptx_hd;
        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
            DEBUG_LOG("configure aptX adaptive input chain");
            aptx_adaptive_ttp_in_ms_t aptx_ad_ttp;

            uint32_t max_aptx_latency = APTX_ADAPTIVE_HQ_LATENCY_MS;

            if (theKymera->q2q_mode)
            {
                max_aptx_bitrate = (rate == SAMPLE_RATE_96000) ? APTX_AD_CODEC_RATE_HS_QHS_96K_KBPS * 1000: APTX_AD_CODEC_RATE_QHS_48K_KBPS * 1000;
                rtp_buffer_size = Kymera_GetAudioBufferSize(max_aptx_bitrate, max_aptx_latency);

                op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
                OperatorsSetSwitchedPassthruEncoding(op, spc_op_format_encoded);
                OperatorsStandardSetBufferSizeWithFormat(op, rtp_buffer_size, operator_data_format_encoded);
                OperatorsSetSwitchedPassthruMode(op, spc_op_mode_passthrough);
            }
            else
            {
                convertAptxAdaptiveTtpToOperatorsFormat(nq2q_ttp, &aptx_ad_ttp);
                getAdjustedAptxAdaptiveTtpLatencies(&aptx_ad_ttp);
                OperatorsRtpSetAptxAdaptiveTTPLatency(op_rtp_decoder, aptx_ad_ttp);
                rtp_codec = rtp_codec_type_aptx_ad;

                max_aptx_bitrate = (rate == SAMPLE_RATE_96000) ? APTX_AD_CODEC_RATE_HS_NQHS_96K_KBPS * 1000 : APTX_AD_CODEC_RATE_NQHS_48K_KBPS *1000;
                max_aptx_latency = aptx_ad_ttp.high_quality ;
                rtp_buffer_size = Kymera_GetAudioBufferSize(max_aptx_bitrate, max_aptx_latency);
            }

            op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_APTX_ADAPTIVE_DECODER));
            OperatorsStandardSetSampleRate(op, rate);

        break;
#endif

        default:
            Panic();
        break;
    }

    if (!theKymera->q2q_mode) /* We don't use rtp decoder for Q2Q mode */
        appKymeraConfigureRtpDecoder(op_rtp_decoder, rtp_codec, mode, rate, cp_header_enabled, rtp_buffer_size);

    if(theKymera->chain_config_callbacks && theKymera->chain_config_callbacks->ConfigureA2dpInputChain)
    {
        kymera_a2dp_config_params_t params = {0};
        params.seid = seid;
        params.sample_rate = rate;
        params.max_bitrate = max_aptx_bitrate;
        params.nq2q_ttp = nq2q_ttp;
        theKymera->chain_config_callbacks->ConfigureA2dpInputChain(chain_handle, &params);
    }

    ChainConnect(chain_handle);
}

static void appKymeraCreateOutputChain(uint8 seid, uint32 rate)
{
    kymera_output_chain_config config = {0};
    a2dp_params_getter_t a2dp_params;
    a2dp_params.seid = seid;
    a2dp_params.rate = rate;

    appKymeraA2dpPopulateOutputChainConfig(a2dp_params, &config);
    PanicFalse(Kymera_OutputPrepare(output_user_a2dp, &config));
}

static void appKymeraStartChains(kymeraTaskData *theKymera)
{
    bool connected;
    uint32 start_time;
    
    DEBUG_LOG("appKymeraStartChains");
    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();
    /* In Q2Q mode the media source has already been connected to the input
    chain by the TransformPacketise so the chain can be started immediately */
    if (theKymera->q2q_mode)
    {
        ChainStart(theKymera->chain_input_handle);
    }
    else
    {
        /* The media source may fail to connect to the input chain if the source
        disconnects between the time A2DP asks Kymera to start and this
        function being called. A2DP will subsequently ask Kymera to stop. */
        connected = ChainConnectInput(theKymera->chain_input_handle, theKymera->media_source, EPR_SINK_MEDIA);
        if (connected)
        {
            ChainStart(theKymera->chain_input_handle);
        }
    }
    
    TimestampEvent(TIMESTAMP_EVENT_A2DP_CHAIN_START_CFM);
    start_time = TimestampEvent_Delta(TIMESTAMP_EVENT_A2DP_START_IND,
                                      TIMESTAMP_EVENT_A2DP_CHAIN_START_CFM);
    DEBUG_LOG("appKymeraStartChains: time taken from A2DP_MEDIA_START_IND till Kymera start chain for SEID %d is %d ms", theKymera->a2dp_seid, start_time);
}

static void appKymeraJoinChains(kymeraTaskData *theKymera)
{
    output_source_t output = {0};
    output.stereo.left = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    output.stereo.right = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT);

    if(Kymera_IsMusicProcessingPresent())
    {
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.left, EPR_MUSIC_PROCESSING_IN_L));
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle, output.stereo.right, EPR_MUSIC_PROCESSING_IN_R));
        output.stereo.left = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_L);
        output.stereo.right = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_R);
    }

    PanicFalse(Kymera_OutputConnect(output_user_a2dp, &output));
}

static void kymera_A2dpCreateChain(const a2dp_codec_settings *codec_settings, uint32 max_bitrate, aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    UNUSED(max_bitrate);
    bool cp_header_enabled;
    uint32 rate;
    uint8 seid;
    Source media_source;

    appKymeraGetA2dpCodecSettingsCore(codec_settings, &seid, &media_source, &rate, &cp_header_enabled, NULL);

    PanicZero(media_source); /* Force panic at this point as source should never be zero */

    appKymeraBoostDspClockToMax();

    theKymera->cp_header_enabled = cp_header_enabled;

    appKymeraCreateOutputChain(seid, rate);
    appKymeraCreateInputChain(theKymera, seid);
    appKymeraConfigureInputChain(theKymera, seid,
                                 rate, cp_header_enabled,
                                 nq2q_ttp);
    Kymera_CreateMusicProcessingChain();
    Kymera_ConfigureMusicProcessing(rate);
    appKymeraJoinChains(theKymera);
    theKymera->media_source = media_source;
    appKymeraConfigureDspPowerMode();
}

static void kymera_A2dpStartChain(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Source source = theKymera->media_source;
    uint32 rate = KymeraOutput_GetMainSampleRate();
    bool cp_header_enabled = theKymera->cp_header_enabled;

    PanicZero(source); /* Force panic at this point as source should never be zero */
    appKymeraConfigureDspPowerMode();
    KymeraOutput_SetMainVolume(volume_in_db);
    StreamDisconnect(source, 0);

    if (theKymera->q2q_mode)
    {
        Sink sink = ChainGetInput(theKymera->chain_input_handle, EPR_SINK_MEDIA);
        Transform packetiser = PanicNull(TransformPacketise(source, sink));
        int16 hq_latency_adjust = Kymera_LatencyManagerIsGamingModeEnabled() ?
                                      (rate == SAMPLE_RATE_96000)? aptxAdaptiveTTPLatencyAdjustHQStandard() : aptxAdaptiveTTPLatencyAdjustHQGaming() :
                                      aptxAdaptiveTTPLatencyAdjustHQStandard();

        if (packetiser)
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_TWSPLUS));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_SAMPLE_RATE, (uint16) rate));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, (uint16) cp_header_enabled));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_TRIGGER_1, aptxAdaptiveLowLatencyStreamId_SSRC_Q2Q()));
            if (HandsetService_IsBrEdrMultipointEnabled())
            {
                PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_1, aptxAdaptiveTTPLatencyMPAdjustLL()));
            }
            else
            {
                PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_1, aptxAdaptiveTTPLatencyAdjustLL()));
            }

            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_TRIGGER_2, aptxAdaptiveHQStreamId_SSRC()));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_TTP_DELAY_SSRC_2, hq_latency_adjust));
            PanicFalse(TransformStart(packetiser));
            theKymera->packetiser = packetiser;
         }
    }
    appKymeraStartChains(theKymera);
}

bool Kymera_A2dpStart(const a2dp_codec_settings *codec_settings, uint32 max_bitrate, int16 volume_in_db,
                                     aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    PanicNotNull(theKymera->chain_input_handle);
    kymera_A2dpCreateChain(codec_settings, max_bitrate, nq2q_ttp);
    kymera_A2dpStartChain(volume_in_db);
    Kymera_LeakthroughSetAecUseCase(aec_usecase_create_leakthrough_chain);
    return TRUE;
}

static void kymera_A2dpStopChain(Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    ChainStop(theKymera->chain_input_handle);

    /* Disconnect A2DP source then dispose */
    StreamDisconnect(source, 0);
    StreamConnectDispose(source);

    Kymera_StopMusicProcessingChain();
}

static void kymera_A2dpDestroyChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    Kymera_OutputDisconnect(output_user_a2dp);
    Kymera_DestroyMusicProcessingChain();
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;
    theKymera->media_source = 0;
}

void Kymera_A2dpCommonStop(Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("Kymera_A2dpCommonStop, source(%p)", source);
    PanicNull(theKymera->chain_input_handle);
    Kymera_LeakthroughSetAecUseCase(aec_usecase_default);
    kymera_A2dpStopChain(source);
    kymera_A2dpDestroyChain();
}

void Kymera_A2dpHandlePrepareStage(const audio_a2dp_start_params_t *params)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = params->codec_settings.seid;
    uint32 rate = params->codec_settings.rate;
    uint8 q2q = params->q2q_mode;

    DEBUG_LOG("Kymera_A2dpHandlePrepareStage: enum:kymera_a2dp_state_t:%d, enum:appKymeraState:%d, seid %u, rate %u, q2q %u",
              appKymeraA2dpGetState(), appKymeraGetState(), seid, rate, q2q);

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        /* Only stop Leakthrough chain with non-TWS message */
        Kymera_LeakthroughStopChainIfRunning();
        PanicFalse(appKymeraA2dpGetState() == kymera_a2dp_idle);
        PanicNotNull(theKymera->chain_input_handle);
        theKymera->a2dp_seid = seid;
        theKymera->q2q_mode = q2q;
        appKymeraA2dpSetState(kymera_a2dp_preparing);
        kymera_A2dpCreateChain(&params->codec_settings, params->max_bitrate, params->nq2q_ttp);
        Kymera_LeakthroughSetAecUseCase(aec_usecase_create_leakthrough_chain);
        appKymeraA2dpSetState(kymera_a2dp_prepared);
        Kymera_LatencyManagerA2dpPrepare(params);
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
}

void Kymera_A2dpHandleStartStage(uint8 seid, int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("Kymera_A2dpHandleStartStage: enum:kymera_a2dp_state_t:%d, enum:appKymeraState:%d, seid %u",
              appKymeraA2dpGetState(), appKymeraGetState(), seid);

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        PanicFalse(appKymeraA2dpGetState() == kymera_a2dp_prepared);
        PanicNull(theKymera->chain_input_handle);
        appKymeraA2dpSetState(kymera_a2dp_starting);
        kymera_A2dpStartChain(volume_in_db);
        appKymeraA2dpSetState(kymera_a2dp_streaming);
        Kymera_LatencyManagerA2dpStart(volume_in_db);
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
}

void Kymera_A2dpHandleInternalStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->seid;

    DEBUG_LOG("Kymera_A2dpHandleInternalStop: enum:kymera_a2dp_state_t:%d, enum:appKymeraState:%d, seid %u",
              appKymeraA2dpGetState(), appKymeraGetState(), seid);

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        switch (appKymeraA2dpGetState())
        {
            case kymera_a2dp_streaming:
                kymera_A2dpStopChain(msg->source);
                // Fall-through

            case kymera_a2dp_prepared:
                Kymera_LeakthroughSetAecUseCase(aec_usecase_default);
                /* Keep framework enabled until after DSP clock update */
                OperatorsFrameworkEnable();
                kymera_A2dpDestroyChain();
                theKymera->a2dp_seid = AV_SEID_INVALID;
                appKymeraA2dpSetState(kymera_a2dp_idle);
                /* Update DSP clock */
                appKymeraConfigureDspPowerMode();
                /* Corresponds to the enable used for the DSP clock update above */
                OperatorsFrameworkDisable();

                Kymera_LatencyManagerA2dpStop();
                Kymera_LeakthroughResumeChainIfSuspended();
            break;

            default:
                // Report, but ignore attempts to stop in invalid states
                DEBUG_LOG("Kymera_A2dpHandleInternalStop: Invalid state");
            break;
        }
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }

    TaskList_MessageSendId(theKymera->listeners, KYMERA_NOTIFICATION_EQ_UNAVAILABLE);
}

void Kymera_A2dpHandleInternalSetVolume(int16 volume_in_db)
{
    DEBUG_LOG("Kymera_A2dpHandleInternalSetVolume, vol %d", volume_in_db);

    if (Kymera_A2dpIsStreaming())
    {
        KymeraOutput_SetMainVolume(volume_in_db);
        Kymera_LatencyManagerHandleA2dpVolumeChange(volume_in_db);
    }
}

void Kymera_A2dpInit(void)
{
    Kymera_OutputRegister(&output_info);
}

#endif /* INCLUDE_STEREO */
