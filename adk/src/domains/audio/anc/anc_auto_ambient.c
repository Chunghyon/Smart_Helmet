/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_auto_ambient.c
\brief     ANC Auto transparency: Automatically move to ANC ambient mode when self speech detected
*/

#ifdef ENABLE_AUTO_AMBIENT

#include "anc_auto_ambient.h"
#include "kymera_anc_common.h"
#include "anc_state_manager.h"
#include "kymera_self_speech_detect.h"
#include "self_speech_tws.h"
#include "phy_state.h"

typedef struct
{
    bool enabled;
    atr_vad_release_duration_t release_time_config;
}autoAmbientData;

static autoAmbientData auto_ambient_data;

#define autoAmbientData() (&auto_ambient_data)

static bool ancAutoAmbient_CanProcess(void)
{    
    if (AncAutoAmbient_IsSupported() && 
        (appPhyStateGetState()==PHY_STATE_IN_EAR) && 
        SelfSpeechTws_IsPrimary())
    {
        return (TRUE);
    }
    else
    {
        return FALSE;
    }
}

bool AncAutoAmbient_IsSupported(void)
{
    return appConfigAutoAmbientEnable();
}

bool AncAutoAmbient_IsEnabled(void)
{
    return (AncAutoAmbient_IsSupported() && autoAmbientData()->enabled);
}

bool AncAutoAmbient_IsAmbientModeConfigured(void)
{
    anc_mode_t ambient_mode=appConfigAutoAmbientMode();

    if ((ambient_mode != ANC_INVALID_MODE) && 
        (AncConfig_IsAncModeAdaptiveLeakThrough(ambient_mode) || (AncConfig_IsAncModeStaticLeakThrough(ambient_mode))))
        return TRUE;
    else
        return FALSE;
}

void AncAutoAmbient_Trigger(void)
{        
    if (AncAutoAmbient_IsEnabled()  && ancAutoAmbient_CanProcess())
    {
        AncStateManager_AutoAmbientTrigger();
    }
}

void AncAutoAmbient_Release(void)
{
    if (AncAutoAmbient_IsEnabled())
    {
        AncStateManager_AutoAmbientRelease();
    }
}

void AncAutoAmbient_Enable(bool enable)
{
    if (enable)
    {
        autoAmbientData()->enabled = enable;

        if (ancAutoAmbient_CanProcess())
        {
            DEBUG_LOG("AncAutoAmbient Enable");
            KymeraSelfSpeechDetect_Enable();
        }
    }
    else
    {        
        DEBUG_LOG("AncAutoAmbient Disable");
        KymeraSelfSpeechDetect_Disable();
        
        autoAmbientData()->enabled = FALSE;
    }
    
     /* Send notification to GAIA */
    AncStateManager_AutoAmbientStateInd(enable);
}

uint16 AncAutoAmbient_GetReleaseTimeConfig(void)
{
    DEBUG_LOG("AncAutoAmbient_GetReleaseTimeConfig %d", autoAmbientData()->release_time_config);
    return (uint16) autoAmbientData()->release_time_config;
}

void AncAutoAmbient_UpdateReleaseTime(void)
{
    /*Set release time through operator interface*/
    KymeraSelfSpeechDetect_SetReleaseDuration(autoAmbientData()->release_time_config);
    
     /* Send notification to GAIA */
    AncStateManager_AutoAmbientReleaseTimeInd();
}

void AncAutoAmbient_StoreReleaseConfig(uint16 release_config)
{
    DEBUG_LOG("AncAutoAmbient_StoreReleaseConfig %d ", release_config);
    autoAmbientData()->release_time_config = (atr_vad_release_duration_t) release_config;
}

void AncAutoAmbient_UpdateTaskData(bool atr_enabled, atr_vad_release_duration_t release_config)
{
    /*By default feature is not enabled, enable using GAIA or Pydbg commands, info is persisted*/
    autoAmbientData()->enabled = atr_enabled;
    autoAmbientData()->release_time_config = release_config;
}

void AncAutoAmbient_UpdateSessionData(bool *atr_enabled, atr_vad_release_duration_t *release_config)
{
    *atr_enabled = autoAmbientData()->enabled;
    *release_config = autoAmbientData()->release_time_config;
    
    DEBUG_LOG("AncAutoAmbient_UpdateSessionData Enabled: %d Release time: %d", autoAmbientData()->enabled, autoAmbientData()->release_time_config);
}

#endif /*ENABLE_AUTO_AMBIENT*/
