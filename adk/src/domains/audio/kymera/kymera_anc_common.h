/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_anc_common.h
\brief      Private header to common anc kymera realted functionality
*/
#ifndef KYMERA_ANC_COMMON_H_
#define KYMERA_ANC_COMMON_H_

#include <app/audio/audio_if.h>
#include <operator.h>
#include "av_seids.h"
#include "kymera.h"
#ifdef ENABLE_WIND_DETECT
#include "wind_detect.h"
#endif

/*! Connect parameters for Adaptive ANC tuning  */
typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} adaptive_anc_tuning_connect_parameters_t;

/*! Disconnect parameters for Adaptive ANC tuning  */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} adaptive_anc_tuning_disconnect_parameters_t;

/*! \brief The KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START message content. */
typedef struct
{
    uint32 usb_rate;
    Source spkr_src;
    Sink mic_sink;
    uint8 spkr_channels;
    uint8 mic_channels;
    uint8 frame_size;
} KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T;

/*! \brief The KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP message content. */
typedef struct
{
    Source spkr_src;
    Sink mic_sink;
    void (*kymera_stopped_handler)(Source source);
} KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T;

/*********************************** Quiet Mode related Functionality *****************************/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_EnableQuietMode(void);
#else
#define KymeraAncCommon_EnableQuietMode() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_DisableQuietMode(void);
#else
#define KymeraAncCommon_DisableQuietMode() ((void)(0))
#endif

/*********************************** Quiet Mode related Functionality Ends ****************************/

/*********************************** ANC common Functionality     ****************************/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_Init(void);
#else
#define KymeraAncCommon_Init() ((void)(0))
#endif


#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_GetFFGain(uint8 *gain);
#else
#define KymeraAncCommon_GetFFGain(gain) (UNUSED(gain))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_GetFBGain(uint8 *gain);
#else
#define KymeraAncCommon_GetFBGain(gain) (UNUSED(gain))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
audio_dsp_clock_type KymeraAncCommon_GetOptimalAudioClockAancScoConcurrency(appKymeraScoMode mode);
#else
#define KymeraAncCommon_GetOptimalAudioClockAancScoConcurrency(x) ((FALSE))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
audio_dsp_clock_type KymeraAncCommon_GetOptimalAudioClockAancMusicConcurrency(int seid);
#else
#define KymeraAncCommon_GetOptimalAudioClockAancMusicConcurrency(x) ((FALSE))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AdaptiveAncSetGainValues(uint32 mantissa, uint32 exponent);
#else
#define KymeraAncCommon_AdaptiveAncSetGainValues(mantisaa,exponent) ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_UpdateInEarStatus(void);
#else
#define KymeraAncCommon_UpdateInEarStatus() ((void) (0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_UpdateOutOfEarStatus(void);
#else
#define KymeraAncCommon_UpdateOutOfEarStatus() ((void) (0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_ApplyModeChange(anc_mode_t mode, audio_anc_path_id anc_path,adaptive_anc_hw_channel_t anc_hw_channel);
#else
#define KymeraAncCommon_ApplyModeChange(mode,anc_path,anc_hw_channel) ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AdaptiveAncEnableGentleMute(void);
#else
#define KymeraAncCommon_AdaptiveAncEnableGentleMute() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_EnableAdaptivity(void);
#else
#define KymeraAncCommon_EnableAdaptivity() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_DisableAdaptivity(void);
#else
#define KymeraAncCommon_DisableAdaptivity() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_AdaptiveAncIsNoiseLevelBelowQmThreshold(void);
#else
#define KymeraAncCommon_AdaptiveAncIsNoiseLevelBelowQmThreshold() (FALSE)
#endif

#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_AdaptiveAncIsEnabled(void);
#else
#define KymeraAncCommon_AdaptiveAncIsEnabled() (FALSE)
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AdaptiveAncDisable(void);
#else
#define KymeraAncCommon_AdaptiveAncDisable() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AncEnable(const KYMERA_INTERNAL_AANC_ENABLE_T * msg);
#else
#define KymeraAncCommon_AncEnable(x) (UNUSED(x))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_PreAncDisable(void);
#else
#define KymeraAncCommon_PreAncDisable() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_PreAncModeChange(void);
#else
#define KymeraAncCommon_PreAncModeChange() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_AdaptiveAncIsConcurrencyActive(void);
#else
#define KymeraAncCommon_AdaptiveAncIsConcurrencyActive() (FALSE)
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AdaptiveAncSetUcid(anc_mode_t mode);
#else
#define KymeraAncCommon_AdaptiveAncSetUcid(x) (UNUSED(x))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_RampCompleteAction(void);
#else
#define KymeraAncCommon_RampCompleteAction() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AhmRampExpiryAction(void);
#else
#define KymeraAncCommon_AhmRampExpiryAction() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_ExitAdaptiveAncTuning(const adaptive_anc_tuning_disconnect_parameters_t *param);
#else
#define KymeraAncCommon_ExitAdaptiveAncTuning(x) (UNUSED(x))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_EnterAdaptiveAncTuning(const adaptive_anc_tuning_connect_parameters_t *param);
#else
#define KymeraAncCommon_EnterAdaptiveAncTuning(x) (UNUSED(x))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_CreateAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *msg);
#else
#define KymeraAncCommon_CreateAdaptiveAncTuningChain(x) (UNUSED(x))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_DestroyAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T *msg);
#else
#define KymeraAncCommon_DestroyAdaptiveAncTuningChain(x) (UNUSED(x))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AncCompanderMakeupGainVolumeDown(void);
#else
#define KymeraAncCommon_AncCompanderMakeupGainVolumeDown() ((void) (0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AncCompanderMakeupGainVolumeUp(void);
#else
#define KymeraAncCommon_AncCompanderMakeupGainVolumeUp() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_AhmGetFFGain(uint16 *gain);
#else
#define KymeraAncCommon_AhmGetFFGain(x) (UNUSED(x))
#endif

/*! \brief Obtain Current Adaptive ANC mode from AANC operator
    \param aanc_mode - pointer to get the value
    \return TRUE if current mode is stored in aanc_mode, else FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_GetApdativeAncCurrentMode(adaptive_anc_mode_t *aanc_mode);
#else
#define KymeraAncCommon_GetApdativeAncCurrentMode(aanc_mode) (FALSE)
#endif


/*! \brief Obtain Current Adaptive ANC V2 mode from AANC operator
    \param aanc_mode - pointer to get the value
    \return TRUE if current mode is stored in aanc_mode, else FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_GetApdativeAncV2CurrentMode(adaptive_ancv2_sysmode_t *aancv2_mode);
#else
#define KymeraAncCommon_GetApdativeAncV2CurrentMode(aancv2_mode) (FALSE)
#endif

/*! \brief Obtain Current AHM mode from AHM operator
    \param ahm_mode - pointer to get the value
    \return TRUE if current mode is stored in ahm_mode, else FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_GetAhmMode(ahm_sysmode_t *ahm_mode);
#else
#define KymeraAncCommon_GetAhmMode(ahm_mode) (FALSE)
#endif

/*! \brief Update FF fine target gain to AHM operator.
    \param ff_fine_gain FF fine target gain to be updated.
    \return TRUE if FF fine target gain is updated to AHM operator, else FALSE.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_UpdateAhmFfPathFineTargetGain(uint8 ff_fine_gain);
#else
#define KymeraAncCommon_UpdateAhmFfPathFineTargetGain(x) ((FALSE))
#endif

/*! \brief Update makeup gain to ANC compander operator.
    \param makeup_gain_fixed_point Makeup gain in fixed point format(2.N) to be updated.
    \return TRUE if makeup gain is updated to ANC compander operator, else FALSE.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_UpdateAncCompanderMakeupGain(int32 makeup_gain_fixed_point);
#else
#define KymeraAncCommon_UpdateAncCompanderMakeupGain(x) ((FALSE))
#endif

/*! \brief Obtain Current makeup gain in fixed point format(2.N) from ANC compander operator.
    \param makeup_gain_fixed_point pointer to get the makeup gain value in fixed point.
    \return TRUE if current makeup gain is stored in makeup_gain_fixed_point, else FALSE.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_GetAncCompanderMakeupQGain(int32* makeup_gain_fixed_point);
#else
#define KymeraAncCommon_GetAncCompanderMakeupQGain(x) ((FALSE))
#endif

/*! \brief Kymera action on Wind detect attack depending on the stage
    \param attack_stage - whether wind detect attack is detected for stage 1 or stage 2
    \return void
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraAncCommon_WindDetectAttack(windDetectStatus_t attack_stage);
#else
#define KymeraAncCommon_WindDetectAttack(attack_stage) (UNUSED(attack_stage))
#endif

/*! \brief Kymera action on Wind detect release depending on the stage
    \param release_stage - whether wind detect release is detected for stage 1 or stage 2
    \return void
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraAncCommon_WindDetectRelease(windDetectStatus_t release_stage);
#else
#define KymeraAncCommon_WindDetectRelease(release_stage) (UNUSED(release_stage))
#endif

/*! \brief Kymera action on request to enable wind detect.
    Move Wind Detect out of Standby mode
    \param void
    \return void
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraAncCommon_EnableWindDetect(void);
#else
#define KymeraAncCommon_EnableWindDetect() ((void)(0))
#endif

/*! \brief Kymera action on request to disable wind detect.
    Move Wind Detect in Standby mode so as not to process Wind
    \param void
    \return void
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraAncCommon_DisableWindDetect(void);
#else
#define KymeraAncCommon_DisableWindDetect() ((void)(0))
#endif

/*!
    \brief API identifying Howling Detection support
    \param None
    \return TRUE for feature supported otherwise FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_IsHowlingDetectionSupported(void);
#else
#define KymeraAncCommon_IsHowlingDetectionSupported() (FALSE)
#endif

/*!
    \brief API identifying Howling Detection state
    \param None
    \return TRUE for state is Enabled otherwise FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_IsHowlingDetectionEnabled(void);
#else
#define KymeraAncCommon_IsHowlingDetectionEnabled() (FALSE)
#endif

/*! \brief Kymera action on request to change hcgr system mode.
    \param mode: whether HCGR running on Standby mode or fullProc mode
    \return void
*/
#if defined(ENABLE_ADAPTIVE_ANC)
void KymeraAncCommon_UpdateHowlingDetectionState(bool enable);
#else
#define KymeraAncCommon_UpdateHowlingDetectionState(enable) (UNUSED(enable))
#endif

/*! \brief Self Speech Enable and ANC mic framework actions
    \param void
    \return void
*/
#if defined(ENABLE_SELF_SPEECH) && defined (ENABLE_AUTO_AMBIENT)
void KymeraAncCommon_SelfSpeechDetectEnable(void);
#else
#define KymeraAncCommon_SelfSpeechDetectEnable() ((void)(0))
#endif

/*! \brief Self Speech Disable and ANC mic framework actions
    \param void
    \return void
*/
#if defined(ENABLE_SELF_SPEECH) && defined (ENABLE_AUTO_AMBIENT)
void KymeraAncCommon_SelfSpeechDetectDisable(void);
#else
#define KymeraAncCommon_SelfSpeechDetectDisable() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_TransitionCompleteAction(void);
#else
#define KymeraAncCommon_TransitionCompleteAction() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_SetFilterCoefficients(void);
#else
#define KymeraAncCommon_SetFilterCoefficients() ((void)(0))
#endif

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_HandleConcurrencyUpdate(bool is_concurrency_active);
#else
#define KymeraAncCommon_HandleConcurrencyUpdate(is_concurrency_active) (UNUSED(is_concurrency_active))
#endif

/*!
    \brief API identifying Current whether AANC state Active or not
    \param None
    \return TRUE for state is Enabled or Enable_Initiated otherwise FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAncCommon_IsAancActive(void);
#else
#define KymeraAncCommon_IsAancActive() (FALSE)
#endif


#endif //KYMERA_ANC_COMMON_H_
