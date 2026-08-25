/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_anc_common.c
\brief      Implementation of common anc kymera realted functionality
*/
#include <app/audio/audio_if.h>
#include <operators.h>
#include "kymera_anc_common.h"
#include "kymera_adaptive_anc.h"
#include "kymera.h"
#include "macros.h"

#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAncCommon_EnableQuietMode(void)
{
    KymeraAdaptiveAnc_EnableQuietMode();
}

void KymeraAncCommon_DisableQuietMode(void)
{
    KymeraAdaptiveAnc_DisableQuietMode();
}

void KymeraAncCommon_GetFFGain(uint8 *gain)
{
    KymeraAdaptiveAnc_GetFFGain(gain);
}

void KymeraAncCommon_AdaptiveAncSetGainValues(uint32 mantissa, uint32 exponent)
{
    KymeraAdaptiveAnc_SetGainValues(mantissa,exponent);
}

void KymeraAncCommon_UpdateInEarStatus(void)
{
    KymeraAdaptiveAnc_UpdateInEarStatus();
}

void KymeraAncCommon_UpdateOutOfEarStatus(void)
{
    KymeraAdaptiveAnc_UpdateOutOfEarStatus();
}

void KymeraAncCommon_ExitAdaptiveAncTuning(const adaptive_anc_tuning_disconnect_parameters_t *param)
{
    KymeraAdaptiveAnc_ExitAdaptiveAncTuning(param);
}

void KymeraAncCommon_ApplyModeChange(anc_mode_t mode, audio_anc_path_id anc_path,adaptive_anc_hw_channel_t anc_hw_channel)
{
    KymeraAdaptiveAnc_ApplyModeChange(mode,anc_path,anc_hw_channel);
}


void KymeraAncCommon_AdaptiveAncEnableGentleMute(void)
{
    KymeraAdaptiveAnc_EnableGentleMute();
}

void KymeraAncCommon_EnterAdaptiveAncTuning(const adaptive_anc_tuning_connect_parameters_t *param)
{
    KymeraAdaptiveAnc_EnterAdaptiveAncTuning(param);
}

void KymeraAncCommon_EnableAdaptivity(void)
{
    KymeraAdaptiveAnc_EnableAdaptivity();
}

void KymeraAncCommon_DisableAdaptivity(void)
{
    KymeraAdaptiveAnc_DisableAdaptivity();
}

void KymeraAncCommon_AdaptiveAncSetUcid(anc_mode_t mode)
{
    KymeraAdaptiveAnc_SetUcid(mode);
}

void KymeraAncCommon_AncEnable(const KYMERA_INTERNAL_AANC_ENABLE_T *msg)
{
    KymeraAdaptiveAnc_Enable(msg);
}

void KymeraAncCommon_AdaptiveAncDisable(void)
{
    KymeraAdaptiveAnc_Disable();
}

void KymeraAncCommon_Init(void)
{
    KymeraAdaptiveAnc_Init();
}

bool KymeraAncCommon_AdaptiveAncIsEnabled(void)
{
    return KymeraAdaptiveAnc_IsEnabled();
}

bool KymeraAncCommon_GetApdativeAncCurrentMode(adaptive_anc_mode_t *aanc_mode)
{
    PanicNull(aanc_mode);
    return KymeraAdaptiveAnc_GetCurrentAancMode(aanc_mode);
}

bool KymeraAncCommon_GetApdativeAncV2CurrentMode(adaptive_ancv2_sysmode_t *aancv2_mode)
{
    UNUSED(aancv2_mode);
    return FALSE;
}

bool KymeraAncCommon_GetAhmMode(ahm_sysmode_t *ahm_mode)
{
    UNUSED(ahm_mode);
    return FALSE;
}

bool KymeraAncCommon_AdaptiveAncIsNoiseLevelBelowQmThreshold(void)
{
    return KymeraAdaptiveAnc_IsNoiseLevelBelowQmThreshold();
}

bool KymeraAncCommon_AdaptiveAncIsConcurrencyActive(void)
{
    return KymeraAdaptiveAnc_IsConcurrencyActive();
}

void KymeraAncCommon_CreateAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *msg)
{
    KymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(msg);
}

void KymeraAncCommon_DestroyAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T *msg)
{
    KymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain(msg);
}

audio_dsp_clock_type KymeraAncCommon_GetOptimalAudioClockAancScoConcurrency(appKymeraScoMode mode)
{
    DEBUG_LOG("KymeraAncCommon_GetOptimalAudioClockAancScoConcurrency");
    UNUSED(mode);
    return AUDIO_DSP_TURBO_CLOCK;
}

audio_dsp_clock_type KymeraAncCommon_GetOptimalAudioClockAancMusicConcurrency(int seid)
{
    DEBUG_LOG("KymeraAncCommon_GetOptimalAudioClockAancMusicConcurrency");
    UNUSED(seid);
    return AUDIO_DSP_TURBO_CLOCK;
}
/* Dummy implementation of the functions not applicable for Pre QCC517x Chipsets */
void KymeraAncCommon_RampCompleteAction(void)
{

}

void KymeraAncCommon_AncCompanderMakeupGainVolumeUp(void)
{

}

void KymeraAncCommon_AncCompanderMakeupGainVolumeDown(void)
{

}

void KymeraAncCommon_PreAncDisable(void)
{
}

void KymeraAncCommon_AhmRampExpiryAction(void)
{
	
}

void KymeraAncCommon_PreAncModeChange(void)
{
}

void KymeraAncCommon_AhmGetFFGain(uint16* gain)
{
    UNUSED(gain);
}

bool KymeraAncCommon_UpdateAhmFfPathFineTargetGain(uint8 ff_fine_gain)
{
    UNUSED(ff_fine_gain);
    return FALSE;
}

bool KymeraAncCommon_UpdateAncCompanderMakeupGain(int32 makeup_gain_fixed_point)
{
    UNUSED(makeup_gain_fixed_point);
    return FALSE;
}

bool KymeraAncCommon_GetAncCompanderMakeupQGain(int32* makeup_gain_fixed_point)
{
    UNUSED(makeup_gain_fixed_point);
    return FALSE;
}

bool KymeraAncCommon_IsHowlingDetectionSupported(void)
{
	return FALSE;
}

bool KymeraAncCommon_IsHowlingDetectionEnabled(void)
{
	return FALSE;
}

void KymeraAncCommon_UpdateHowlingDetectionState(bool enable)
{
    UNUSED(enable);	
}

void KymeraAncCommon_GetFBGain(uint8 *gain)
{
	UNUSED(gain);
}

bool KymeraAncCommon_IsAancActive(void)
{
    return FALSE;
}

/* Dummy implementation of the functions not applicable for Pre QCC517x Chipsets */
void KymeraAncCommon_TransitionCompleteAction(void)
{

}

/* Dummy implementation of the functions not applicable for Pre QCC517x Chipsets */
void KymeraAncCommon_SetFilterCoefficients(void)
{

}

void KymeraAncCommon_HandleConcurrencyUpdate(bool is_concurrency_active)
{
	UNUSED(is_concurrency_active);
}

#endif /* ENABLE_ADAPTIVE_ANC */
