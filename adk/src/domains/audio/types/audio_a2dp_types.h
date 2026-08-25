/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Definitions used for routing a2dp audio use case
*/

#ifndef AUDIO_A2DP_TYPES_H_
#define AUDIO_A2DP_TYPES_H_

#include <a2dp.h>

/*! \brief Parameters used when preparing/starting a2dp audio use case */
typedef struct
{
    /*! The client's lock. Bits set in lock_mask will be cleared when A2DP is started. */
    uint16 *lock;
    /*! The bits to clear in the client lock. */
    uint16 lock_mask;
    /*! The A2DP codec settings */
    a2dp_codec_settings codec_settings;
    /*! The starting volume */
    int16 volume_in_db;
    /*! The number of times remaining the kymera module will resend this message to
        itself (having entered the locked KYMERA_STATE_A2DP_STARTING) state before
        proceeding to commence starting kymera. Starting will commence when received
        with value 0. Only applies to starting the master. */
    uint8 master_pre_start_delay;
    /*! The max bitrate for the input stream (in bps). Ignored if zero. */
    uint32 max_bitrate;
    uint8 q2q_mode; /* 1 = Q2Q mode enabled, 0 = Generic Mode */
    aptx_adaptive_ttp_latencies_t nq2q_ttp;
} audio_a2dp_start_params_t;

/*! \brief Parameters used when stopping a2dp audio use case */
typedef struct
{
    /*! The A2DP seid */
    uint8 seid;
    /*! The media sink */
    Source source;
} audio_a2dp_stop_params_t;

/*! \brief Parameters used when setting a2dp audio use case volume */
typedef struct
{
    int16 volume_in_db;
} audio_a2dp_set_volume_t;

#endif /* AUDIO_A2DP_TYPES_H_ */
