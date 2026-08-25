/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_echo_canceller.h
\brief     Header file for Echo canceller kymera related functionality
*/
#ifndef KYMERA_ECHO_CANCELLER_H_
#define KYMERA_ECHO_CANCELLER_H_

#include <chain.h>

/* Supported FBC configurations */
typedef enum
{
    fb_path_only = 0,
    ff_fb_paths
} fbc_config_t;

/*! \brief Returns true if echo canceller active
*/
#ifdef ENABLE_ADAPTIVE_ANC
bool KymeraEchoCanceller_IsActive(void);
#else
#define KymeraEchoCanceller_IsActive() (FALSE)
#endif

/*! \brief Create echo canceller audio chain for Adaptive AANC configuration
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Create(fbc_config_t fbc_config);
#else
#define KymeraEchoCanceller_Create(x) (UNUSED(x))
#endif

/*! \brief Configure echo canceller audio chain for Adaptive AANC configuration
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Configure(void);
#else
#define KymeraEchoCanceller_Configure() ((void)(0))
#endif

/*! \brief Connect echo canceller FBC and Splitter streams for Adaptive AANC configuration
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Connect(void);
#else
#define KymeraEchoCanceller_Connect() ((void)(0))
#endif

/*! \brief Start echo canceller audio chain for Adaptive AANC configuration
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Start(void);
#else
#define KymeraEchoCanceller_Start() ((void)(0))
#endif

/*! \brief Stop echo canceller audio chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Stop(void);
#else
#define KymeraEchoCanceller_Stop() ((void)(0))
#endif

/*! \brief Disconnect echo canceller FBC and Splitter streams for Adaptive AANC configuration
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Disconnect(void);
#else
#define KymeraEchoCanceller_Disconnect() ((void)(0))
#endif

/*! \brief Destroy echo canceller audio chain
*/
#ifdef ENABLE_ADAPTIVE_ANC
void KymeraEchoCanceller_Destroy(void);
#else
#define KymeraEchoCanceller_Destroy() ((void)(0))
#endif

/*! \brief Get the Echo canceller reference path speaker Sink for Adaptive AANC configuration
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraEchoCanceller_GetSpkRefPathSink(void);
#else
#define KymeraEchoCanceller_GetSpkRefPathSink() ((void)(0))
#endif

/*! \brief Get the Echo canceller Feed Forward mic Source
*/
#ifdef ENABLE_ADAPTIVE_ANC
Source KymeraEchoCanceller_GetFFMicPathSource(void);
#else
#define KymeraEchoCanceller_GetFFMicPathSource() (NULL)
#endif

/*! \brief Get the Echo canceller Feed back mic Source
*/
#ifdef ENABLE_ADAPTIVE_ANC
Source KymeraEchoCanceller_GetFBMicPathSource(void);
#else
#define KymeraEchoCanceller_GetFBMicPathSource() (NULL)
#endif

/*! \brief Get the Echo canceller Feed Forward mic Sink
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraEchoCanceller_GetFFMicPathSink(void);
#else
#define KymeraEchoCanceller_GetFFMicPathSink() (NULL)
#endif

/*! \brief Get the Echo canceller Feed back mic Sink
*/
#ifdef ENABLE_ADAPTIVE_ANC
Sink KymeraEchoCanceller_GetFBMicPathSink(void);
#else
#define KymeraEchoCanceller_GetFBMicPathSink() (NULL)
#endif

#endif /* KYMERA_ECHO_CANCELLER_H_ */

