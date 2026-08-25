/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_wind_detect.h
\brief     Implementation of Kymera Wind Noise Detect and related functionality
*/

#ifndef KYMERA_WIND_DETECT_H_
#define KYMERA_WIND_DETECT_H_

#include "kymera_anc_common.h"
#include "operators.h"
#include "kymera_ucid.h"


/*! \brief Get the Wind detect Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
kymera_chain_handle_t KymeraWindDetect_GetChain(void);
#else
#define KymeraWindDetect_GetChain() ((void)(0))
#endif

/*! \brief Create the Wind noise detect Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_Create(void);
#else
#define KymeraWindDetect_Create() ((void)(0))
#endif

/*! \brief Configure the Wind noise detect  Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraWindDetect_Configure(param) ((void)(0))
#endif

/*! \brief Connect the Wind noise detect Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_Connect(void);
#else
#define KymeraWindDetect_Connect() ((void)(0))
#endif

/*! \brief Start the Wind noise detect Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_Start(void);
#else
#define KymeraWindDetect_Start() ((void)(0))
#endif

/*! \brief Stop the Wind noise detect Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_Stop(void);
#else
#define KymeraWindDetect_Stop() ((void)(0))
#endif

/*! \brief Destroy the Wind noise detect Chain
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_Destroy(void);
#else
#define KymeraWindDetect_Destroy() ((void)(0))
#endif

/*! \brief Check if Wind Detect is active
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
bool KymeraWindDetect_IsActive(void);
#else
#define KymeraWindDetect_IsActive() (FALSE)
#endif

/*! \brief Apply mode change to Wind Detect
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraWindDetect_ApplyModeChange(param) ((void)(0))
#endif

/*! \brief Set the UCID for Wind Detect
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_SetUcid(kymera_operator_ucid_t ucid);
#else
#define KymeraWindDetect_SetUcid(ucid) ((void)(0))
#endif

/*! \brief Set the sys mode as 1mic for Wind Detect
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_SetSysMode1Mic(void);
#else
#define KymeraWindDetect_SetSysMode1Mic() ((void)(0))
#endif

/*! \brief Set the sys mode as 2mic for Wind Detect
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_SetSysMode2Mic(void);
#else
#define KymeraWindDetect_SetSysMode2Mic() ((void)(0))
#endif

/*! \brief Set the sys mode as standby for Wind Detect
        This state will not process the Wind
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
void KymeraWindDetect_SetSysModeStandby(void);
#else
#define KymeraWindDetect_SetSysModeStandby() ((void)(0))
#endif

/*! \brief Get the FF mic path sink for Wind Detect
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
Sink KymeraWindDetect_GetFFMicPathSink(void);
#else
#define KymeraWindDetect_GetFFMicPathSink() ((void)(0))
#endif

/*! \brief Get the Diversity mic path sink for Wind Detect
*/
#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)
Sink KymeraWindDetect_GetDiversityMicPathSink(void);
#else
#define KymeraWindDetect_GetDiversityMicPathSink() ((void)(0))
#endif

#endif /*KYMERA_WIND_DETECT_H_*/

