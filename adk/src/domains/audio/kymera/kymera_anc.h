/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header for ANC functionality
*/

#ifndef KYMERA_ANC_H_
#define KYMERA_ANC_H_

#include <kymera.h>
#include <anc_state_manager.h>
#include "kymera_data.h"
#include "kymera_state.h"

typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} KYMERA_INTERNAL_ANC_TUNING_START_T;

/*! \brief The KYMERA_INTERNAL_ANC_TUNING_STOP message content. */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_ANC_TUNING_STOP_T;

#ifdef INCLUDE_STEREO
#define getAncFeedForwardRightMic()   (appConfigAncFeedForwardRightMic())
#define getAncFeedBackRightMic()      (appConfigAncFeedBackRightMic())
#define getAncFeedForwardLeftMic()    (appConfigAncFeedForwardLeftMic())
#define getAncFeedBackLeftMic()       (appConfigAncFeedBackLeftMic())

#define getAncTuningMonitorLeftMic()  (appConfigAncTuningMonitorLeftMic())
#define getAncTuningMonitorRightMic() (appConfigAncTuningMonitorRightMic())
#else
#define getAncFeedForwardRightMic()   (MICROPHONE_NONE)
#define getAncFeedBackRightMic()      (MICROPHONE_NONE)
#define getAncFeedForwardLeftMic()    (appConfigAncFeedForwardMic())
#define getAncFeedBackLeftMic()       (appConfigAncFeedBackMic())

#define getAncTuningMonitorLeftMic()  (appConfigAncTuningMonitorMic())
#define getAncTuningMonitorRightMic() (MICROPHONE_NONE)
#endif

/*!
 * \brief Makes the support chain ready for ANC hardware. applicable only for QCC512x devices
 * \param appKymeraState state current kymera state.
 *
 */
#if defined INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN && defined ENABLE_ANC
void KymeraAnc_PreStateTransition(appKymeraState state);
#else
#define KymeraAnc_PreStateTransition(x) ((void)(x))
#endif

#define KymeraAnc_IsDspClockUpdateRequired() (appKymeraInConcurrency() || AncStateManager_CheckIfDspClockBoostUpRequired())

/*! \brief Creates the Kymera Tuning Chain
    \param msg internal message which has the anc tuning connect parameters
*/
#ifdef ENABLE_ANC
void KymeraAnc_TuningCreateChain(const KYMERA_INTERNAL_ANC_TUNING_START_T *msg);
#else
#define KymeraAnc_TuningCreateChain(x) ((UNUSED(x)))
#endif


/*! \brief Destroys the Kymera Tuning Chain
    \param msg internal message which has the anc tuning disconnect parameters
*/
#ifdef ENABLE_ANC
void KymeraAnc_TuningDestroyChain(const KYMERA_INTERNAL_ANC_TUNING_STOP_T *msg);
#else
#define KymeraAnc_TuningDestroyChain(x) ((UNUSED(x)))
#endif


/*! \brief  Updates world volume gain to world_volume_gain_dB.
            In static ANC build, ANC HW FF fine gain will be updated.
            In Adaptive ANC build, AHM FF fine gain will be updated for static leakthrough config whereas ANC compander
            makeup gain will be updated for adaptive leakthrough config.
    \param  world_volume_gain_dB  World volume gain to be updated.
            balance_percentage    Configured balance percentage.
    \return TRUE if gain update is succesful, else FALSE.
*/
#ifdef ENABLE_ANC
bool KymeraAnc_UpdateWorldVolumeGain(int8 world_volume_gain_dB, int8 balance_percentage);
#else
#define KymeraAnc_UpdateWorldVolumeGain(x, y) ((FALSE))
#endif


/*! \brief Register mics params and callbacks used in switched passthrogh graph to Mic framework
    \param None
*/
#ifdef ENABLE_ANC
void KymeraAnc_Init(void);
#else
#define KymeraAnc_Init()  ((void)(0))
#endif

#ifdef ENABLE_ANC
/*! \brief Create and destroy switched passthrough dummy graph. This dummy graph is intended to
 *         resolve ANC mics out of sync issue (B-292238).
    \param None
*/
void KymeraAnc_CreateAndDestroySwitchedPassThroughGraph(void);
#else
#define KymeraAnc_CreateAndDestroySwitchedPassThroughGraph()    ((void)(0))
#endif

/*! \brief  Updates configured world volume balance
    \param  balance_percentage Balance percentage to be applied.
    \return TRUE if update is succesful, else FALSE.
*/
#ifdef ENABLE_ANC
bool KymeraAnc_UpdateWorldVolumeBalance(int8 balance_percentage);
#else
#define KymeraAnc_UpdateWorldVolumeBalance(x) ((FALSE))
#endif


/*! \brief  Gets integral part of ANC Compander makeup gain.
    \param  makeup_gain_dB Pointer to hold integral part of makeup gain.
    \return TRUE if makeup gain is stored in makeup_gain_dB, else FALSE.
*/
#ifdef ENABLE_ANC
bool kymeraAnc_GetAncCompanderMakeupdBGain(int8* makeup_gain_dB);
#else
#define kymeraAnc_GetAncCompanderMakeupdBGain(x) ((FALSE))
#endif


/*! \brief  Gets current FF fine gain.
            This API is only applicable for modes which are configured for static leakthrough use-case.
    \return Current FF fine gain.
*/
#ifdef ENABLE_ANC
uint8 kymeraAnc_GetStaticLeakthroughFfFineGain(void);
#else
#define kymeraAnc_GetStaticLeakthroughFfFineGain() ((0))
#endif

/*! \brief Get the optimal DSP clock speed when clock boost-up is required or AANC is a concurrent user
    \param seid is A2DP stream endpoint ID
*/
#ifdef ENABLE_ANC
audio_dsp_clock_type KymeraAnc_GetOptimalDspClockForMusicConcurrency(int seid);
#else
#define KymeraAnc_GetOptimalDspClockForMusicConcurrency(x) ((FALSE))
#endif

/*! \brief Get the optimal DSP clock speed when clock boost-up is required or AANC is a concurrent user
    \param mode is Sco stream endpoint ID
*/
#ifdef ENABLE_ANC
audio_dsp_clock_type KymeraAnc_GetOptimalDspClockForScoConcurrency(appKymeraScoMode mode);
#else
#define KymeraAnc_GetOptimalDspClockForScoConcurrency(x) ((FALSE))
#endif

#endif /* KYMERA_ANC_H_ */
