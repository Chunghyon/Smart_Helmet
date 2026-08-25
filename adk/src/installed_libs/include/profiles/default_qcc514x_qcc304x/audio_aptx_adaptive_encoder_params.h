/****************************************************************************
Copyright (c) 2018-2022 Qualcomm Technologies International, Ltd.


FILE NAME
    audio_aptx_adaptive_encoder_params.h

DESCRIPTION
    Definitions of aptX Adaptive Encoder Parameters.
*/

#ifndef AUDIO_APTX_ADAPTIVE_ENCODER_PARAMS_H_
#define AUDIO_APTX_ADAPTIVE_ENCODER_PARAMS_H_

#include <audio_plugin_if.h>

/* Sample rates required by the aptx Adaptive encoder for various modes */
#define APTXAD_REQUIRED_SAMPLE_RATE_LL_MODE     (SAMPLE_RATE_48000)
#define APTXAD_REQUIRED_SAMPLE_RATE_HQ_SPLIT_TX (SAMPLE_RATE_96000)

/* Quality mode used by the aptX Adaptive encoder */
typedef enum
{
    aptxad_mode_low_latency = 2,
    aptxad_mode_high_quality = 3,
} aptxad_quality_mode_t;

/* Legacy aptX Adaptive encoder parameters */
typedef struct
{
    unsigned quality;
    unsigned dh5_dh3;
    unsigned channel;
    unsigned bitrate;
    unsigned sample_rate;
    unsigned compatibility;
} aptxad_encoder_params_t;

/* Parameters used for configuring the aptX Adaptive encoder for 96K */
typedef struct
{
    uint16 quality_mode;    /*!< main quality mode: HQ, Low Latency */
    uint32 sample_rate;     /*!< Sample rate for the encoder */
    uint16 compatibility;   /*!< version of aptX adaptive that the encoder should be configured for */
    uint16 mtu;             /*!< MTU size */
    uint16 split_tx;        /*!< split Tx is enabled or not */
    uint16 qhs;             /*!< QHS is enabled or not */
    uint16 twm;             /*!< TWM is enabled or not */
} aptxad_96k_encoder_config_params_t;

/* Parameters used for updating the aptX Adaptive encoder for 96K */
typedef struct
{
    int16 rssi;             /*!< RSSI */
    uint16 cqddr;           /*!< CQDRR */
    uint16 quality;         /*!< RF quality 1 to 5 (best) */
} aptxad_96K_encoder_rf_signal_params_t;

#endif /* AUDIO_APTX_ADAPTIVE_ENCODER_PARAMS_H_ */

