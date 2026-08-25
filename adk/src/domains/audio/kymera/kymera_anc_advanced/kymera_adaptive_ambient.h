/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_adaptive_ambient.h
\brief      Header for Adaptive Ambient mode of ANC
*/
#ifndef KYMERA_ADAPTIVE_AMBIENT_H_
#define KYMERA_ADAPTIVE_AMBIENT_H_

#include "kymera.h"

/*!
    \brief Creates Adaptive Ambient Chain. Adaptive Ambient chain consist of splitter, 
	       compander and howling control operator. ANC hardware manager is not created here.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_CreateChain(void);
#else
#define KymeraAdaptiveAmbient_CreateChain() ((void)(0))
#endif

/*!
    \brief Configures the different operators invloved in the adaptive ambient chain.
    \param configuration parameters.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_ConfigureChain(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraAdaptiveAmbient_ConfigureChain(x) (UNUSED(x))
#endif

/*!
    \brief Connects adaptive ambient chain operators.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_ConnectChain(void);
#else
#define KymeraAdaptiveAmbient_ConnectChain() ((void)(0))
#endif

/*!
    \brief Starts adaptive ambient chain operators.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_StartChain(void);
#else
#define KymeraAdaptiveAmbient_StartChain() ((void)(0))
#endif

/*!
    \brief Stops adaptive ambient chain.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_StopChain(void);
#else
#define KymeraAdaptiveAmbient_StopChain() ((void)(0))
#endif

/*!
    \brief Disconnects operators of adaptive ambient chain.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_DisconnectChain(void);
#else
#define KymeraAdaptiveAmbient_DisconnectChain() ((void)(0))
#endif

/*!
    \brief Destroys the operatos involved in adaptive ambient chain.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_DestroyChain(void);
#else
#define KymeraAdaptiveAmbient_DestroyChain() ((void)(0))
#endif

/*!
    \brief Returns the chain handler.
    \param None.
    \return chain handler of type kymera_chain_handle_t.
*/
#ifdef ENABLE_ADAPTIVE_ANC
kymera_chain_handle_t KymeraAdaptiveAmbient_GetChain(void);
#else
#define KymeraAdaptiveAmbient_DestroyChain() ((void)(0))
#endif

/*!
    \brief Returns Feedforward mic path sink.
    \param None.
    \return FF mic path sink.
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraAdaptiveAmbient_GetFFMicPathSink(void);
#else
#define KymeraAdaptiveAmbient_GetFFMicPathSink() ((void)(0))
#endif

/*!
    \brief Configures the howling control operator system mode.
    \param mode to be set.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_SetHowlingControlSysMode(hc_sysmode_t mode);
#else
#define KymeraAdaptiveAmbient_SetHowlingControlSysMode(x) (UNUSED(x))
#endif

/*!
    \brief Sets the howling control ucid.
    \param ucid to be applied.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_SetHowlingControlUcid(unsigned ucid);
#else
#define KymeraAdaptiveAmbient_SetHowlingControlUcid(x) (UNUSED(x))
#endif

/*!
    \brief Sets the ANC compander system mode.
    \param mode to be set.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_SetAncCompanderSysMode(anc_compander_sysmode_t mode);
#else
#define KymeraAdaptiveAmbient_SetAncCompanderSysMode(x) (UNUSED(x))
#endif

/*!
    \brief Starts the compander gain adaptation.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_StartAncCompanderAdaptation(void);
#else
#define KymeraAdaptiveAmbient_StartAncCompanderAdaptation() ((void)(0))
#endif

/*!
    \brief Sets ANC compander ucid.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_SetAncCompanderUcid(unsigned ucid);
#else
#define KymeraAdaptiveAmbient_SetAncCompanderUcid(x) (UNUSED(x))
#endif

/*!
    \brief Used for increasing the compander makeup gain.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_AncCompanderMakeupGainVolumeUp(void);
#else
#define KymeraAdaptiveAmbient_AncCompanderMakeupGainVolumeUp() ((void)(0))
#endif

/*! \brief Obtain Current makeup gain in fixed point format(2.N) from ANC compander operator.
    \param makeup_gain_fixed_point pointer to get the makeup gain value in fixed point.
    \return TRUE if current makeup gain is stored in makeup_gain_fixed_point, else FALSE.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAmbient_GetCompanderGain(int32* makeup_gain_fixed_point);
#else
#define KymeraAdaptiveAmbient_GetCompanderGain(x) ((FALSE))
#endif

/*! \brief Custom message for getting compander adjusted gain    
     \param None
    \return The adjusted gain is returned in the response.
*/
#ifdef ENABLE_ADAPTIVE_ANC
uint16 KymeraAdaptiveAmbient_GetCompanderAdjustedGain(void);
#else
#define KymeraAdaptiveAmbient_GetCompanderAdjustedGain() ((void)(0))
#endif


/*!
    \brief Used for decreasing the compander makeup gain.
    \param None.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_AncCompanderMakeupGainVolumeDown(void);
#else
#define KymeraAdaptiveAmbient_AncCompanderMakeupGainVolumeDown() ((void)(0))
#endif

/*!
    \brief Configure adaptive ambient chain after mode change.
    \param configuration paramters.
    \return None.
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraAdaptiveAmbient_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param);
#else
#define KymeraAdaptiveAmbient_ApplyModeChange(x) (UNUSED(x))
#endif

/*!
    \brief Update makeup gain to ANC compander operator.
    \param makeup_gain_fixed_point Makeup gain in fixed point format(2.N) to be updated.
    \return TRUE if makeup gain is updated to ANC compander operator, else FALSE.
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraAdaptiveAmbient_UpdateAncCompanderMakeupGain(int32 makeup_gain_fixed_point);
#else
#define KymeraAdaptiveAmbient_UpdateAncCompanderMakeupGain(x) ((0*x))
#endif


#endif /*KYMERA_ADAPTIVE_AMBIENT_H_*/
