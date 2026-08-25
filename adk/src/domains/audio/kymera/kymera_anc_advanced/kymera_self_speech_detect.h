/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_self_speech_detect.h
\brief     Implementation of Kymera Self Speech Detect and related functionality
*/

#ifndef KYMERA_SELF_SPEECH_DETECT_H_
#define KYMERA_SELF_SPEECH_DETECT_H_

#include "operators.h"
#include "kymera_ucid.h"


/*! \brief Get the  Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
kymera_chain_handle_t KymeraSelfSpeechDetect_GetChain(void);
#else
#define KymeraSelfSpeechDetect_GetChain() ((void)(0))
#endif

/*! \brief Create the Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Create(void);
#else
#define KymeraSelfSpeechDetect_Create() ((void)(0))
#endif

/*! \brief Configure the Self Speech detect  Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Configure(void);
#else
#define KymeraSelfSpeechDetect_Configure() ((void)(0))
#endif

/*! \brief Connect the Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Connect(void);
#else
#define KymeraSelfSpeechDetect_Connect() ((void)(0))
#endif

/*! \brief Disconnect the Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Disconnect(void);
#else
#define KymeraSelfSpeechDetect_Disconnect() ((void)(0))
#endif


/*! \brief Start the Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Start(void);
#else
#define KymeraSelfSpeechDetect_Start() ((void)(0))
#endif

/*! \brief Stop the Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Stop(void);
#else
#define KymeraSelfSpeechDetect_Stop() ((void)(0))
#endif

/*! \brief Destroy the Self Speech detect Chain
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Destroy(void);
#else
#define KymeraSelfSpeechDetect_Destroy() ((void)(0))
#endif

/*! \brief Check if  Self Speech Detect is active
*/
#if defined(ENABLE_SELF_SPEECH)
bool KymeraSelfSpeechDetect_IsActive(void);
#else
#define KymeraSelfSpeechDetect_IsActive() (FALSE)
#endif

/*! \brief Get the PEQ/VAD mic path sink for  Self Speech Detect
*/
#if defined(ENABLE_SELF_SPEECH)
Sink KymeraSelfSpeechDetect_GetMicPathSink(void);
#else
#define KymeraSelfSpeechDetect_GetMicPathSink() ((void)(0))
#endif

/*! \brief Enable the Kymera audio chain for Self speech
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Enable(void);
#else
#define KymeraSelfSpeechDetect_Enable() ((void)(0))
#endif

/*! \brief Disable the Kymera audio chain for Self speech
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_Disable(void);
#else
#define KymeraSelfSpeechDetect_Disable() ((void)(0))
#endif

/*! \brief Set Self Speech ATR VAD sys mode
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_SetSysMode(atr_vad_sysmode_t mode);
#else
#define KymeraSelfSpeechDetect_SetSysMode(mode) ((void)(0))
#endif

/*! \brief Set Self Speech ATR VAD Release Duration
*/
#if defined(ENABLE_SELF_SPEECH)
void KymeraSelfSpeechDetect_SetReleaseDuration(atr_vad_release_duration_t duration);
#else
#define KymeraSelfSpeechDetect_SetReleaseDuration(duration) ((void)(0))
#endif


#endif /*KYMERA_SELF_SPEECH_DETECT_H_*/

