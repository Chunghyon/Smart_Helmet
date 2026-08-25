/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_auto_ambient.h
\brief     ANC Auto transparency: Auto move to ANC ambient mode when self speech detected
*/

#ifndef ANC_AUTO_AMBIENT_H
#define ANC_AUTO_AMBIENT_H

#include <operators.h>

/*Self speech release time configuration in seconds*/     
#define ANC_AUTO_AMBIENT_RELEASE_TIME_NO_ACTION   0
#define ANC_AUTO_AMBIENT_RELEASE_TIME_SHORT       5
#define ANC_AUTO_AMBIENT_RELEASE_TIME_NORMAL      10
#define ANC_AUTO_AMBIENT_RELEASE_TIME_LONG        15

typedef struct
{
    uint16 release_time;
}ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND_T;


/*!
    \brief API to identify auto ambient feature support
    \param None
    \return TRUE for feature supported otherwise FALSE
*/
#ifdef ENABLE_AUTO_AMBIENT
bool AncAutoAmbient_IsSupported(void);
#else
#define AncAutoAmbient_IsSupported() (FALSE)
#endif

/*!
    \brief API to identify if ambient mode is configured in anc_config_data
    \param None
    \return TRUE if Configured otherwise FALSE
*/
#ifdef ENABLE_AUTO_AMBIENT
bool AncAutoAmbient_IsAmbientModeConfigured(void);
#else
#define AncAutoAmbient_IsAmbientModeConfigured() (FALSE)
#endif

/*!
    \brief API to identify if auto ambient feature is enabled by user or not
    \param None
    \return TRUE for feature enabled otherwise FALSE
*/
#ifdef ENABLE_AUTO_AMBIENT
bool AncAutoAmbient_IsEnabled(void);
#else
#define AncAutoAmbient_IsEnabled() (FALSE)
#endif

/*!
    \brief API to enable or disable the auto ambient feature
    \param Enable/Disable TRUE for feature enabled 
    \return None
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_Enable(bool enable);
#else
#define AncAutoAmbient_Enable(enable) (UNUSED(enable))
#endif

/*!
    \brief API to action ANC to mode switch to Ambient on Self Speech detect trigger
    \param None
    \return None
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_Trigger(void);
#else
#define AncAutoAmbient_Trigger() ((void)(0))
#endif


/*!
    \brief API to action ANC to move back to previous mode on Self Speech release
    Function called Either on a self speech release from ATR VAD capability
    Or feature disable request from user 
    Or handover of primary
    Or A2DP absolute volume change
    \param None
    \return None
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_Release(void);
#else
#define AncAutoAmbient_Release() ((void)(0))
#endif

/*!
    \brief API to return the current release time config for self speech clear
    \param None
    \return release time
*/
#ifdef ENABLE_AUTO_AMBIENT
uint16 AncAutoAmbient_GetReleaseTimeConfig(void);
#else
#define AncAutoAmbient_GetReleaseTimeConfig() (0)
#endif

/*!
    \brief API to store the requested release time config in global, used for self speech clear
    \param release time
    \return None
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_StoreReleaseConfig(uint16 release_time);
#else
#define AncAutoAmbient_StoreReleaseConfig(x) ((void)(0))
#endif

/*!
    \brief API to update the requested release time config in VAD for self speech clear
    This is peer synchronised.
    \param release time
    \return None
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_UpdateReleaseTime(void);
#else
#define AncAutoAmbient_UpdateReleaseTime(x) ((void)(0))
#endif

/*!
    \brief Updates the task data with the session data stored in PS.
           If PS is empty, uses default values in session_data
    \param ATR Enable and Release config data read from PS
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_UpdateTaskData(bool atr_enabled, atr_vad_release_duration_t release_config);
#else
#define AncAutoAmbient_UpdateTaskData(x, y) ((void)(0*x), (void)(0*y))
#endif

/*!
    \brief Stores ATR session data in PS.
    \param ATR Enable and Release config data to be stored in PS
*/
#ifdef ENABLE_AUTO_AMBIENT
void AncAutoAmbient_UpdateSessionData(bool *atr_enabled, atr_vad_release_duration_t *release_config);
#else
#define AncAutoAmbient_UpdateSessionData(x, y) ((void)(0*x), (void)(0*y))
#endif

#endif /* ANC_AUTO_AMBIENT_H */
