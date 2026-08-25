/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_hcgr.h
\defgroup   kymera Kymera
\ingroup    audio_domain
\brief      The Kymera HCGR Manager

*/

#ifndef KYMERA_HCGR_H
#define KYMERA_HCGR_H

#include "kymera_anc_common.h"
#include "operators.h"
#include "kymera_ucid.h"

typedef enum
{
    howling_detection_disabled,
    howling_detection_enabled
}howling_detect_user_state_t;

/*!
    \brief API identifying Howling Detection state
    \param None
    \return TRUE for state is Enabled otherwise FALSE
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraHcgr_IsHowlingDetectionEnabled(void);
#else
#define KymeraHcgr_IsHowlingDetectionEnabled() (FALSE)
#endif

/*! \brief Create the HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Create(void);
#else
#define KymeraHcgr_Create() ((void)(0))
#endif

/*! \brief Configure the HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraHcgr_Configure(param) ((void)(0))
#endif

/*! \brief Connect the HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Connect(void);
#else
#define KymeraHcgr_Connect() ((void)(0))
#endif

/*! \brief Start the HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Start(void);
#else
#define KymeraHcgr_Start() ((void)(0))
#endif

/*! \brief Stop the HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Stop(void);
#else
#define KymeraHcgr_Stop() ((void)(0))
#endif

/*! \brief Disconnect HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Disconnect(void);
#else
#define KymeraHcgr_Disconnect() ((void)(0))
#endif

/*! \brief Destroy the HCGR Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_Destroy(void);
#else
#define KymeraHcgr_Destroy() ((void)(0))
#endif

/*! \brief Check if Hcgr is active
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraHcgr_IsActive(void);
#else
#define KymeraHcgr_IsActive() (FALSE)
#endif

/*! \brief Set the UCID for HCGR
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_SetUcid(kymera_operator_ucid_t ucid);
#else
#define KymeraHcgr_SetUcid(ucid) ((void)(0))
#endif

/*!
    \brief Configures the howling control operator system mode.
    \param mode to be set.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_t mode);
#else
#define KymeraHcgr_SetHowlingControlSysMode(x) (UNUSED(x))
#endif

/*!
    \brief Configures the howling control user state.
    \param Parameter to be set if user disabled the Howling Control.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraHcgr_UpdateUserState(howling_detect_user_state_t state);
#else
#define KymeraHcgr_UpdateUserState(x) (UNUSED(x))
#endif

/*! \brief Get the Hcgr mic Sink
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraHcgr_GetFbMicPathSink(void);
#else
#define KymeraHcgr_GetFbMicPathSink() (NULL)
#endif

#endif /* KYMERA_HCGR_H */
