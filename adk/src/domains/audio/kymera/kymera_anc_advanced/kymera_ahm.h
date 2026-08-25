/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_ahm.h
\brief     Header file for ANC hardware manager.
*/

#ifndef KYMERA_AHM_H_
#define KYMERA_AHM_H_

#include "kymera_anc_common.h"
#include "operators.h"
#include "kymera_ucid.h"

Operator KymeraAhm_GetOperator(void);

/*! \brief Get the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
kymera_chain_handle_t KymeraAhm_GetChain(void);
#else
#define KymeraAhm_GetChain() ((void)(0))
#endif

/*! \brief Create the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_Create(void);
#else
#define KymeraAhm_Create() ((void)(0))
#endif

/*! \brief Configure the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraAhm_Configure(param) ((void)(0))
#endif

/*! \brief Connect the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_Connect(void);
#else
#define KymeraAhm_Connect() ((void)(0))
#endif

/*! \brief Start the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_Start(void);
#else
#define KymeraAhm_Start() ((void)(0))
#endif

/*! \brief Stop the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_Stop(void);
#else
#define KymeraAhm_Stop() ((void)(0))
#endif

/*! \brief Destroy the ANC Hardware Manager Chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_Destroy(void);
#else
#define KymeraAhm_Destroy() ((void)(0))
#endif

/*! \brief Check if AHM is active
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAhm_IsActive(void);
#else
#define KymeraAhm_IsActive() (FALSE)
#endif

/*! \brief Apply mode change to AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraAhm_ApplyModeChange(param) ((void)(0))
#endif

/*! \brief Set the UCID for AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_SetUcid(kymera_operator_ucid_t ucid);
#else
#define KymeraAhm_SetUcid(ucid) ((void)(0))
#endif

/*! \brief Set the sys mode for AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_SetSysMode(ahm_sysmode_t mode);
#else
#define KymeraAhm_SetSysMode(mode) ((void)(0))
#endif

/*! \brief Get current AHM mode
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAhm_GetSysMode(ahm_sysmode_t *ahm_mode);
#else
#define KymeraAhm_GetSysMode(ahm_mode) (FALSE)
#endif

/*! \brief Get the FF mic path sink for AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraAhm_GetFFMicPathSink(void);
#else
#define KymeraAhm_GetFFMicPathSink() ((void)(0))
#endif

/*! \brief Get the FB mic path sink for AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraAhm_GetFBMicPathSink(void);
#else
#define KymeraAhm_GetFBMicPathSink() ((void)(0))
#endif

/*! \brief Get the FF gain parameter from AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_GetFFGain(uint16* gain);
#else
#define KymeraAhm_GetFFGain(gain) ((void)(0))
#endif

/*! \brief Get the FB gain parameter from AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_GetFBGain(uint16* gain);
#else
#define KymeraAhm_GetFBGain(gain) ((void)(0))
#endif

/*! \brief Update the In Ear status in capability
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_UpdateInEarStatus(bool enable);
#else
#define KymeraAhm_UpdateInEarStatus(enable) ((void)(0))
#endif

/*! \brief Update FF fine gain to AHM operator.
    \param ff_fine_gain FF fine gain to be updated.
    \return TRUE if FF fine gain is updated to AHM operator, else FALSE.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAhm_UpdateFfPathFineGain(uint8 ff_fine_gain);
#else
#define KymeraAhm_UpdateFfPathFineGain(x) ((0*x))
#endif

/*! \brief Get the Disable FF gain range adjust parameter.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAhm_IsFfFineGainRangeAdjustDisabled(void);
#else
#define KymeraAhm_IsFfFineGainRangeAdjustDisabled() (FALSE)
#endif

/*! \brief Set the Disable FF gain range adjust parameter.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_DisableFfFineGainRangeAdjust(void);
#else
#define KymeraAhm_DisableFfFineGainRangeAdjust() ((void)(0))
#endif

/*! \brief Reset the Disable FF gain range adjust parameter.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_EnableFfFineGainRangeAdjust(void);
#else
#define KymeraAhm_EnableFfFineGainRangeAdjust() ((void)(0))
#endif

/*! \brief Set FF target gain in AHM
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAhm_SetTargetGain(uint16 gain);
#else
#define KymeraAhm_SetTargetGain(gain) ((void)(0))
#endif


/*! \brief Get the static FF fine gain
*/
#ifdef ENABLE_ADAPTIVE_ANC
uint16 KymeraAhm_GetStaticFeedForwardFineGain(adaptive_anc_hw_channel_t hw_channel, audio_anc_path_id control_path);
#else
#define KymeraAhm_GetStaticFeedForwardFineGain(hw_channel, control_path) ((void)hw_channel, (void)control_path)
#endif

#if defined ENABLE_ADAPTIVE_ANC && defined ENABLE_ANC_FAST_MODE_SWITCH
void KymeraAhm_TriggerTransition(audio_anc_path_id control_path, adaptive_anc_hw_channel_t hw_channel, ahm_trigger_transition_ctrl_t transition);
#else
#define KymeraAhm_TriggerTransition(control_path, hw_channel, transition) \
    UNUSED(control_path); \
    UNUSED(hw_channel); \
    UNUSED(transition);
#endif

#endif /*KYMERA_AHM_H_*/

