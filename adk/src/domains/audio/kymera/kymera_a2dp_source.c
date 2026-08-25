/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera chain configuration routines common to all A2DP Source chains.
*/

#if defined(INCLUDE_A2DP_USB_SOURCE) || defined(INCLUDE_A2DP_ANALOG_SOURCE)

#include "av_seids.h"
#include "kymera.h"
#include "kymera_a2dp.h"
#include "kymera_a2dp_source.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "connection_manager.h"
#include <audio_aptx_adaptive_encoder_params.h>
#include <message.h>
#include <operators.h>
#include <sink.h>

#define ENCODE_OUTPUT_BUFFER_SIZE_FACTOR 4

/* These SBC parameters are currently fixed. */
#define A2DP_SBC_SUPPORTED_SUBBANDS 8
#define A2DP_SBC_SUPPORTED_BLOCK_LENGTH 16

/* How often to send updated RF signal params to the aptX Adaptive encoder, when running. */
#define APTX_AD_RF_SIGNAL_UPDATE_INTERVAL_MS 100

/* Default RF signal params to send to the aptX Adaptive encoder, in absence of real values. */
#define APTX_AD_SIGNAL_PARAMS_DEFAULT_RSSI -40  /* Assume strong signal until told otherwise. */
#define APTX_AD_SIGNAL_PARAMS_DEFAULT_CQDDR LINK_STATUS_PHY_RATE_3MBPS  /* Assume highest EDR rate. */
#define APTX_AD_SIGNAL_PARAMS_DEFAULT_QUALITY LINK_STATUS_LINK_QUALITY_STANDARD

/* Maximum number of bytes the packetiser will add if RTP headers are enabled. */
#define RTP_HEADER_SIZE_MAX 12

/* Task data structure */
typedef struct
{
    TaskData task;
    bool enable_rf_signal_updates;
    aptxad_96K_encoder_rf_signal_params_t signal_params;
    aptxad_quality_mode_t aptxad_active_mode;
    aptxad_quality_mode_t aptxad_desired_mode;
} kymera_a2dp_source_data_t;

kymera_a2dp_source_data_t kymera_a2dp_source_data;


static unsigned  kymeraA2dpSource_CalculateEncoderOutputBufferSize(unsigned output_rate)
{
    unsigned scaled_rate = output_rate / 1000;
    return (KICK_PERIOD_SLOW * scaled_rate * ENCODE_OUTPUT_BUFFER_SIZE_FACTOR) / 1000;
}

static bool kymeraA2dpSource_SendEncoderSignalParams(void)
{
    Operator aptx_encoder = ChainGetOperatorByRole(KymeraGetTaskData()->chain_input_handle, OPR_APTX_ADAPTIVE_ENCODER);

    if (aptx_encoder)
    {
        DEBUG_LOG_V_VERBOSE("kymeraA2dpSource_SendEncoderSignalParams: RSSI %d, CQDDR %u, Quality %u",
                            kymera_a2dp_source_data.signal_params.rssi,
                            kymera_a2dp_source_data.signal_params.cqddr,
                            kymera_a2dp_source_data.signal_params.quality);

        OperatorsAptxAd96kEncoderRfSignalParams(aptx_encoder, &kymera_a2dp_source_data.signal_params);
        return TRUE;
    }

    /* AptX Adaptive encoder not running. */
    return FALSE;
}

static void kymeraA2dpSource_HandleMessageLinkStatusTracking(MessageLinkStatusTracking *msg)
{
    if (kymera_a2dp_source_data.enable_rf_signal_updates)
    {
        kymera_a2dp_source_data.signal_params.rssi = msg->average_rssi;
        kymera_a2dp_source_data.signal_params.cqddr = msg->phy_rate;
        kymera_a2dp_source_data.signal_params.quality = msg->link_quality;

        kymeraA2dpSource_SendEncoderSignalParams();
    }
}

static void kymeraA2dpSource_TaskHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    UNUSED(msg);

    switch (id)
    {
        case MESSAGE_LINK_STATUS_TRACKING:
            kymeraA2dpSource_HandleMessageLinkStatusTracking((MessageLinkStatusTracking*)msg);
            break;

        default:
            break;
    }
}

void kymeraA2dpSource_Init(void)
{
    kymera_a2dp_source_data.task.handler = kymeraA2dpSource_TaskHandler;

    kymera_a2dp_source_data.enable_rf_signal_updates = TRUE;
    kymera_a2dp_source_data.signal_params.rssi = APTX_AD_SIGNAL_PARAMS_DEFAULT_RSSI;
    kymera_a2dp_source_data.signal_params.cqddr = APTX_AD_SIGNAL_PARAMS_DEFAULT_CQDDR;
    kymera_a2dp_source_data.signal_params.quality = APTX_AD_SIGNAL_PARAMS_DEFAULT_QUALITY;
    kymera_a2dp_source_data.aptxad_active_mode = aptxad_mode_high_quality;
    kymera_a2dp_source_data.aptxad_desired_mode = aptxad_mode_high_quality;

    MessageLinkStatusTrackingTask(&kymera_a2dp_source_data.task);
}

void kymeraA2dpSource_ConfigureTtpBufferParams(Operator operator, uint32 min_latency_ms,
                                               uint32 max_latency_ms, uint32 target_latency_ms)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpSource_ConfigureTtpBufferParams");

    OperatorsStandardSetLatencyLimits(operator, MS_TO_US(min_latency_ms), MS_TO_US(max_latency_ms));
    OperatorsStandardSetTimeToPlayLatency(operator, MS_TO_US(target_latency_ms));
    OperatorsStandardSetBufferSizeWithFormat(operator, TTP_BUFFER_SIZE, operator_data_format_pcm);
}

void kymeraA2dpSource_ConfigureEncoder(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraA2dpUsbSource_ConfigureEncoder");

    kymeraTaskData *theKymera = KymeraGetTaskData();
    const a2dp_codec_settings *codec_settings = theKymera->a2dp_output_params;

    switch (codec_settings->seid)
    {
        case AV_SEID_SBC_SRC:
        {
            Operator sbc_encoder = PanicZero(ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_SBC_ENCODER));

            sbc_encoder_params_t sbc_encoder_params;

            sbc_encoder_params.channel_mode = (sbc_encoder_channel_mode_t)codec_settings->channel_mode;
            sbc_encoder_params.bitpool_size = codec_settings->codecData.bitpool;
            sbc_encoder_params.sample_rate = codec_settings->rate;
            sbc_encoder_params.number_of_subbands = A2DP_SBC_SUPPORTED_SUBBANDS;
            sbc_encoder_params.number_of_blocks = A2DP_SBC_SUPPORTED_BLOCK_LENGTH;
            sbc_encoder_params.allocation_method = sbc_encoder_allocation_method_loudness;

            OperatorsSbcEncoderSetEncodingParams(sbc_encoder, &sbc_encoder_params);
        }
        break;

        case AV_SEID_APTX_CLASSIC_SRC:
        {
            /* No parameters needed for aptX Classic */
        }
        break;

        case AV_SEID_APTXHD_SRC:
        {
            /* No parameters needed for aptX HD */
        }
        break;

        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            Operator aptx_encoder = PanicZero(ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_APTX_ADAPTIVE_ENCODER));

            DEBUG_LOG_INFO("kymeraA2dpSource: AptX Adaptive enum:aptx_ad_version_t:%d",
                           codec_settings->codecData.aptx_ad_params.version);

            tp_bdaddr tp_sink_addr;
            SinkGetBdAddr(codec_settings->sink, &tp_sink_addr);

            /* Update encoder quality mode, ensuring a valid combination of mode & sample rate. */
            if ((kymera_a2dp_source_data.aptxad_desired_mode == aptxad_mode_low_latency) &&
                (codec_settings->rate != APTXAD_REQUIRED_SAMPLE_RATE_LL_MODE))
            {
                DEBUG_LOG_WARN("kymeraA2dpSource: LL mode requested, but not supported at %ldkHz",
                               codec_settings->rate/1000);
                kymera_a2dp_source_data.aptxad_active_mode = aptxad_mode_high_quality;
            }
            else
            {
                kymera_a2dp_source_data.aptxad_active_mode = kymera_a2dp_source_data.aptxad_desired_mode;
            }

            /* Only enable split tx if supported and in 96kHz HQ mode (48kHz is always non-split tx). */
            theKymera->split_tx_mode = (codec_settings->codecData.aptx_ad_params.features & aptx_ad_split_streaming) &&
                                       (codec_settings->rate == APTXAD_REQUIRED_SAMPLE_RATE_HQ_SPLIT_TX);

            /* Configure the encoder with the initial parameters. */
            aptxad_96k_encoder_config_params_t encoder_config;
            DEBUG_LOG_INFO("kymeraA2dpSource: Configuring encoder for enum:aptxad_quality_mode_t:%d @ %ldkHz",
                           kymera_a2dp_source_data.aptxad_active_mode, codec_settings->rate/1000);

            encoder_config.quality_mode = kymera_a2dp_source_data.aptxad_active_mode;
            encoder_config.sample_rate = codec_settings->rate;
            encoder_config.mtu = codec_settings->codecData.packet_size;
            encoder_config.split_tx = theKymera->split_tx_mode;
            encoder_config.qhs = ConManagerGetQhsConnectStatus(&tp_sink_addr.taddr.addr);
            encoder_config.twm = (codec_settings->codecData.aptx_ad_params.features & aptx_ad_twm_support)? 1:0;

            /*
             * Workaround for issue found with some older Sinks that negotiated
             * JOINT_STEREO and advertised aptX Adaptive v2.0 but only worked
             * properly with aptX Adaptive v1.0
             */
            if ( codec_settings->channel_mode == a2dp_joint_stereo &&
                 codec_settings->codecData.aptx_ad_params.version == aptx_ad_version_2_0)
            {
                DEBUG_LOG_INFO("kymeraA2dpSource: A2DP joint stereo workaround applied, using aptX Adaptive v1.1");
                encoder_config.compatibility = aptx_ad_version_1_1;
            } else  /* use the aptX Adaptive version that was negotiated */
            {
                encoder_config.compatibility = codec_settings->codecData.aptx_ad_params.version;
            }

            if (encoder_config.compatibility == aptx_ad_version_unknown)
            {
                /* Try 2.1 */
                DEBUG_LOG_ERROR("Unknown aptX Adaptive enum:aptx_ad_version_t:%d. Trying Version 2.1",
                                codec_settings->codecData.aptx_ad_params.version);
                encoder_config.compatibility = aptx_ad_version_2_1;
            }

            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.quality_mode %d", encoder_config.quality_mode);
            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.sample_rate %ld", encoder_config.sample_rate);
            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.compatibility %d", encoder_config.compatibility);
            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.mtu %d", encoder_config.mtu);
            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.split_tx %d", encoder_config.split_tx);
            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.qhs %d", encoder_config.qhs);
            DEBUG_LOG_DEBUG("kymeraA2dpSource:  encoder_config.twm %d", encoder_config.twm);

            OperatorsAptxAd96kEncoderConfigParams(aptx_encoder, &encoder_config);

            DEBUG_LOG_DEBUG("kymeraA2dpSource: Initial RF params: RSSI %d, CQDDR %u, Quality %u",
                            kymera_a2dp_source_data.signal_params.rssi,
                            kymera_a2dp_source_data.signal_params.cqddr,
                            kymera_a2dp_source_data.signal_params.quality);

            /* Send the first set of RF signal quality parameters immediately,
               to prevent the encoder stalling. */
            kymeraA2dpSource_SendEncoderSignalParams();
        }
        break;

        default:
            Panic();
        break;
    }

    /* Configure the size of the encoder output buffer, via the operator that
       immediately follows it (a basic passthrough). */
    Operator passthrough_buffer = PanicZero(ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_ENCODER_OUTPUT_BUFFER));

    OperatorsSetPassthroughDataFormat(passthrough_buffer, operator_data_format_encoded);
    OperatorsStandardSetBufferSize(passthrough_buffer, kymeraA2dpSource_CalculateEncoderOutputBufferSize(codec_settings->rate));
}

void kymeraA2dpSource_ConfigureSwitchedPassthrough(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpSource_ConfigureSwitchedPassthrough");

    Operator spc_op = PanicZero(ChainGetOperatorByRole(KymeraGetTaskData()->chain_input_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER));

    OperatorsSetSwitchedPassthruEncoding(spc_op, spc_op_format_encoded);
    OperatorsSetSwitchedPassthruMode(spc_op, spc_op_mode_passthrough);
    OperatorsStandardSetBufferSize(spc_op, 1024);
}

bool kymeraA2dpSource_ConfigurePacketiser(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpSource_ConfigurePacketiser");

    kymeraTaskData *theKymera = KymeraGetTaskData();
    const a2dp_codec_settings *codec_settings = theKymera->a2dp_output_params;
    uint16 codec_mtu = codec_settings->codecData.packet_size;

    Source source = PanicNull(ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_ENCODE_OUT));

    if (codec_settings->sink == NULL)
    {
        return FALSE;
    }

    Transform packetiser = PanicNull(TransformPacketise(source, codec_settings->sink));

    switch (codec_settings->seid)
    {
        case AV_SEID_APTX_CLASSIC_SRC:
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_RTP));
        }
        break;

        case AV_SEID_APTXHD_SRC:
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_RTP));
        }
        break;

        case AV_SEID_APTX_ADAPTIVE_SRC:
        {
            uint16 codec_ssrc;
            vm_transform_packetise_codec codec_format;

            if (theKymera->split_tx_mode)
            {
                /* Signal to the packetiser to add the extra header bytes required for HQ 96kHz split tx streaming. */
                codec_format = VM_TRANSFORM_PACKETISE_CODEC_APTX_ADAPTIVE;
            }
            else
            {
                /* Non-split tx uses the regular aptX packetiser codec configuration. */
                codec_format = VM_TRANSFORM_PACKETISE_CODEC_APTX;
            }

            switch (kymera_a2dp_source_data.aptxad_active_mode)
            {
                case aptxad_mode_low_latency:
                    codec_ssrc = aptxAdaptiveLowLatencyStreamId_SSRC_Q2Q();
                    codec_mtu = 668;
                    break;
                case aptxad_mode_high_quality:
                default:
                    codec_ssrc = aptxAdaptiveHQStreamId_SSRC();
                    break;
            }

            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, codec_format));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_TWSPLUS));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_RTP_SSRC, codec_ssrc));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_RTP_SSRC_HI, 0));
        }
        break;

        case AV_SEID_SBC_SRC:
        {
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_SBC));
            PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_RTP));
        }
        break;

        default:
        break;
    }

    /* Do not check result of configuring MTU, because this feature is licensed on some platforms, e.g. QCC3056.
       If the license check fails, it will return FALSE, but play silence. This is ok, and we should continue.

       Some codecs do not account for the RTP header in their packet_size. So the MTU we pass to the packetiser
       must account for potentially an extra 12 bytes (RTP_HEADER_SIZE_MAX). It does not matter if these bytes
       end up not being utilised - the MTU is simply an upper limit to avoid fragmentation.
    */
    TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MTU, codec_mtu + RTP_HEADER_SIZE_MAX);

    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_SAMPLE_RATE, (uint16)codec_settings->rate));

    /* aptX HD needs a special configuration */
    if (codec_settings->seid == AV_SEID_APTXHD_SRC)
    {
        PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, 1));
        PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_SCMS, 0));
    }
    else
    {
        PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, (uint16)codec_settings->codecData.content_protection));
    }
    PanicFalse(TransformStart(packetiser));

    theKymera->packetiser = packetiser;

    return TRUE;
}

void kymeraA2dpSource_StartChain(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpSource_StartChain");

    ChainStart(KymeraGetTaskData()->chain_input_handle);

    if (KymeraGetTaskData()->a2dp_output_params->seid == AV_SEID_APTX_ADAPTIVE_SRC)
    {
        DEBUG_LOG_DEBUG("kymeraA2dpSource: Starting link status tracking");
        SinkEnableLinkStatusTracking(KymeraGetTaskData()->a2dp_output_params->sink,
                                     TRUE, APTX_AD_RF_SIGNAL_UPDATE_INTERVAL_MS);
    }
}

void kymeraA2dpSource_StopChain(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpSource_StopChain");

    if (KymeraGetTaskData()->a2dp_output_params->seid == AV_SEID_APTX_ADAPTIVE_SRC)
    {
        DEBUG_LOG_DEBUG("kymeraA2dpSource: Stopping link status tracking");
        SinkEnableLinkStatusTracking(KymeraGetTaskData()->a2dp_output_params->sink,
                                     FALSE, APTX_AD_RF_SIGNAL_UPDATE_INTERVAL_MS);
    }

    ChainStop(KymeraGetTaskData()->chain_input_handle);

    /* Reset stored RF parameters, unless signal updates are disabled. */
    if (kymera_a2dp_source_data.enable_rf_signal_updates)
    {
        kymera_a2dp_source_data.signal_params.rssi = APTX_AD_SIGNAL_PARAMS_DEFAULT_RSSI;
        kymera_a2dp_source_data.signal_params.cqddr = APTX_AD_SIGNAL_PARAMS_DEFAULT_CQDDR;
        kymera_a2dp_source_data.signal_params.quality = APTX_AD_SIGNAL_PARAMS_DEFAULT_QUALITY;
    }
}

void kymeraA2dpSource_DestroyChain(void)
{
    DEBUG_LOG_FN_ENTRY("kymeraA2dpSource_DestroyChain");
    kymeraTaskData *theKymera = KymeraGetTaskData();

    Source from_encode_out = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_ENCODE_OUT);
    DEBUG_LOG_V_VERBOSE("kymeraA2dpSource_DestroyChain, from_encode_out source(%p)", from_encode_out);

    /* Disconnect the chain output */
    StreamDisconnect(from_encode_out, NULL);

    /* Destroy chain now that all inputs & outputs have been disconnected */
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;

    /* Destroy packetiser */
    if (theKymera->packetiser)
    {
        TransformStop(theKymera->packetiser);
        theKymera->packetiser = NULL;
    }
}

void Kymera_AptxAdEncoderEnableRfSignalUpdates(bool enable)
{
    kymera_a2dp_source_data.enable_rf_signal_updates = enable;
}

bool Kymera_AptxAdEncoderSendRfSignalParams(const aptxad_96K_encoder_rf_signal_params_t *params)
{
    PanicZero(params);

    kymera_a2dp_source_data.signal_params.rssi = params->rssi;
    kymera_a2dp_source_data.signal_params.cqddr = params->cqddr;
    kymera_a2dp_source_data.signal_params.quality = params->quality;

    return kymeraA2dpSource_SendEncoderSignalParams();
}

void Kymera_AptxAdEncoderGetRfSignalParams(aptxad_96K_encoder_rf_signal_params_t *params)
{
    PanicZero(params);

    params->rssi = kymera_a2dp_source_data.signal_params.rssi;
    params->cqddr = kymera_a2dp_source_data.signal_params.cqddr;
    params->quality = kymera_a2dp_source_data.signal_params.quality;
}

void Kymera_AptxAdEncoderSetDesiredQualityMode(aptxad_quality_mode_t quality_mode)
{
    kymera_a2dp_source_data.aptxad_desired_mode = quality_mode;
}

aptxad_quality_mode_t Kymera_AptxAdEncoderGetDesiredQualityMode(void)
{
    return kymera_a2dp_source_data.aptxad_desired_mode;
}

aptxad_quality_mode_t Kymera_AptxAdEncoderGetActiveQualityMode(void)
{
    aptxad_quality_mode_t current_mode;

    if (KymeraGetTaskData()->chain_input_handle &&
        KymeraGetTaskData()->a2dp_output_params &&
        KymeraGetTaskData()->a2dp_output_params->seid == AV_SEID_APTX_ADAPTIVE_SRC)
    {
        /* Encoder is currently running, so report actual live value. */
        current_mode = kymera_a2dp_source_data.aptxad_active_mode;
    }
    else
    {
        /* Encoder not running, report preferred mode for next time it starts. */
        current_mode = Kymera_AptxAdEncoderGetDesiredQualityMode();
    }

    return current_mode;
}

#endif /* INCLUDE_A2DP_USB_SOURCE || INCLUDE_A2DP_ANALOG_SOURCE */
