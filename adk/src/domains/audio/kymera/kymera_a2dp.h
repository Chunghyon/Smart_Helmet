/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP
*/

#ifndef KYMERA_A2DP_H
#define KYMERA_A2DP_H

#include <source.h>
#include <message.h>
#include <a2dp.h>
#include <audio_a2dp_types.h>

/* AptX Adaptive encoder version defines */
#define APTX_AD_ENCODER_R2_1 21
#define APTX_AD_ENCODER_R1_1 11

#define APTX_MONO_CODEC_RATE_KBPS     (192)
#define APTX_STEREO_CODEC_RATE_KBPS   (384)
#define APTXHD_STEREO_CODEC_RATE_KBPS (576)
#define APTX_AD_CODEC_RATE_KBPS       (500)

/* Maximum bitrates for aptX adaptive */
/* Bitrates for 48K modes HS and TWM are the same */
#define APTX_AD_CODEC_RATE_NQHS_48K_KBPS     (427)
#define APTX_AD_CODEC_RATE_QHS_48K_KBPS      (430)

/* Maxium bitrates for 96K modes */
#define APTX_AD_CODEC_RATE_HS_QHS_96K_KBPS   (820) /* QHS Headset mode */
#define APTX_AD_CODEC_RATE_HS_NQHS_96K_KBPS  (646) /* Non-QHS Headset mode */

#define APTX_AD_CODEC_RATE_TWM_QHS_96K_KBPS  (650)  /* QHS TWM mode */
#define APTX_AD_CODEC_RATE_TWM_NQHS_96K_KBPS (510)  /* Non-QHS TWM mode */

#define APTX_AD_CODEC_RATE_TWM_QHS_SPLIT_TX_96K_KBPS  (325)  /* QHS TWM mode for split tx is half stereo mode */
#define APTX_AD_CODEC_RATE_TWM_NQHS_SPLIT_TX_96K_KBPS (265)  /* Non-QHS TWM mode for split tx is half stereo mode */

/*! Maximum codec rate expected by this application */
#define MAX_CODEC_RATE_KBPS (APTXHD_STEREO_CODEC_RATE_KBPS)

/*!@{ \name Buffer sizes required to hold enough audio to achieve the TTP latency */
#define PRE_DECODER_BUFFER_SIZE     (MS_TO_BUFFER_SIZE_CODEC(PRE_DECODER_BUFFER_MS, MAX_CODEC_RATE_KBPS))

/*! \brief The KYMERA_INTERNAL_A2DP_SET_VOL message content. */
typedef audio_a2dp_set_volume_t KYMERA_INTERNAL_A2DP_SET_VOL_T;

/*! \brief The KYMERA_INTERNAL_A2DP_START and KYMERA_INTERNAL_A2DP_STARTING message content. */
typedef audio_a2dp_start_params_t KYMERA_INTERNAL_A2DP_START_T;

/*! \brief The KYMERA_INTERNAL_A2DP_STOP and KYMERA_INTERNAL_A2DP_STOP_FORWARDING message content. */
typedef audio_a2dp_stop_params_t KYMERA_INTERNAL_A2DP_STOP_T;

/*! \brief Initialise a2dp module.
*/
void Kymera_A2dpInit(void);

/*! \brief Handle request to start A2DP (prepare + start A2DP).
    \param msg The request message.
    \return TRUE if A2DP start is complete. FALSE if A2DP start is incomplete.
*/
bool Kymera_A2dpHandleInternalStart(const KYMERA_INTERNAL_A2DP_START_T *msg);

/*! \brief Handle request to prepare A2DP.
    \params Parameters needed to prepare A2DP.
*/
void Kymera_A2dpHandlePrepareReq(const audio_a2dp_start_params_t *params);

/*! \brief Handle request to start A2DP (Has to be prepared first).
    \params Parameters needed to start A2DP.
*/
void Kymera_A2dpHandleStartReq(const audio_a2dp_start_params_t *params);

/*! \brief Handle request to stop A2DP.
    \param msg The request message.
*/
void Kymera_A2dpHandleInternalStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg);

/*! \brief Handle request to set A2DP volume.
    \param volume_in_db The requested volume.
*/
void Kymera_A2dpHandleInternalSetVolume(int16 volume_in_db);

/*! \brief Shows if the use case is routed or about to be routed (routing basically means connected to output chain)
    \return TRUE when routed/about to be, FALSE otherwise
*/
bool Kymera_A2dpIsBeingRouted(void);

/*! \brief Shows if the use case is routed (routing basically means connected to output chain)
    \return TRUE when routed, FALSE otherwise
*/
bool Kymera_A2dpIsRouted(void);

/*! \brief Shows if the use case is active (therefore DSP clock needs to be adjusted accordingly)
    \return TRUE when active, FALSE otherwise
*/
bool Kymera_A2dpIsActive(void);

/*! \brief Shows if a2dp is streaming (chain started and processing input)
    \return TRUE when streaming, FALSE otherwise
*/
bool Kymera_A2dpIsStreaming(void);

/*! \brief Shows if a2dp is forwarding (streaming data to peer)
    \return TRUE when forwarding, FALSE otherwise
*/
bool Kymera_A2dpIsForwarding(void);

#ifdef INCLUDE_MIRRORING
void appKymeraA2dpHandleDataSyncIndTimeout(void);
void appKymeraA2dpHandleMessageMoreDataTimeout(void);
void appKymeraA2dpHandleAudioSyncStreamInd(MessageId id, Message msg);
void appKymeraA2dpHandleAudioSynchronisedInd(void);
void appKymeraA2dpHandleMessageMoreData(const MessageMoreData *mmd);
#endif /* INCLUDE_MIRRORING */

#endif // KYMERA_A2DP_H
