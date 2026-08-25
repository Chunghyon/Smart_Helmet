/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_adaptive_anc.h
\brief     Header file for Adaptive ANC kymera related functionality
*/
#ifndef KYMERA_ADAPTIVE_ANC_H_
#define KYMERA_ADAPTIVE_ANC_H_

#include <operators.h>
#include <anc.h>
#include <chain.h>
#include <kymera.h>

/*! \brief Get the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
kymera_chain_handle_t KymeraAdaptiveAnc_GetChain(void);
#else
#define KymeraAdaptiveAnc_GetChain() ((void)(0))
#endif

/*! \brief Create the Adaptive ANC standalone chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_CreateChain(void);
#else
#define KymeraAdaptiveAnc_CreateChain() ((void)(0))
#endif

/*! \brief Configure the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_ConfigureChain(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraAdaptiveAnc_ConfigureChain(param) ((void)(0))
#endif

/*! \brief Connect the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_ConnectChain(void);
#else
#define KymeraAdaptiveAnc_ConnectChain() ((void)(0))
#endif

/*! \brief Start the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_StartChain(void);
#else
#define KymeraAdaptiveAnc_StartChain() ((void)(0))
#endif

/*! \brief Stop the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_StopChain(void);
#else
#define KymeraAdaptiveAnc_StopChain() ((void)(0))
#endif

/*! \brief Disconnect the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DisconnectChain(void);
#else
#define KymeraAdaptiveAnc_DisconnectChain() ((void)(0))
#endif

/*! \brief Destroy the Adaptive ANC chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_DestroyChain(void);
#else
#define KymeraAdaptiveAnc_DestroyChain() ((void)(0))
#endif

/*! \brief Get Adaptive ANC sys mode 
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_SetSysMode(adaptive_ancv2_sysmode_t mode);
#else
#define KymeraAdaptiveAnc_SetSysMode(mode) ((void)(0))
#endif

/*! \brief Apply mode change to Adaptive ANC mode
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAnc_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraAdaptiveAnc_ApplyModeChange(param) ((void)(0))
#endif

/*! \brief Returns true if current noise level is below Quiet mode threshold.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_IsNoiseLevelBelowQuietModeThreshold(void);
#else
#define KymeraAdaptiveAnc_IsNoiseLevelBelowQuietModeThreshold() (FALSE)
#endif

/*! \brief Get the status if Adaptive ANC chain active
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_IsActive(void);
#else
#define KymeraAdaptiveAnc_IsActive() ((void)(0))
#endif

/*! \brief Get current AANC mode
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAnc_GetSysMode(adaptive_ancv2_sysmode_t *aancv2_mode);
#else
#define KymeraAdaptiveAnc_GetSysMode(aancv2_mode) ((void)(0))
#endif

/*! \brief Get current adapted freezed gain
*/
#ifdef ENABLE_ADAPTIVE_ANC
uint16 KymeraAdaptiveAnc_GetFreezedGain(void);
#else
#define KymeraAdaptiveAnc_GetFreezedGain(void) ((void)(0))
#endif

#endif /* KYMERA_ADAPTIVE_ANC_H_ */

