/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama.h
\brief  Interfaces defination of Amazon AVS interfaces
*/

#ifndef AMA_H
#define AMA_H

#include "ama_config.h"
#include "logging.h"
#include <csrtypes.h>
#include <stdio.h>
#include <voice_ui_va_client_if.h>

#define ASSISTANT_OVERRIDE_REQUIRED     TRUE

/* Size of locale string, including terminator */
#define AMA_LOCALE_STR_SIZE (sizeof(AMA_DEFAULT_LOCALE))
/* Length of the locale string */
#define AMA_LOCALE_STR_LEN  (AMA_LOCALE_STR_SIZE-1)

#if defined(INCLUDE_AMA) && !defined(INCLUDE_VOICE_UI)
    #error INCLUDE_VOICE_UI is missing in combination with INCLUDE_AMA
#endif

#if defined(INCLUDE_AMA) && !defined(INCLUDE_KYMERA_AEC)
    #error AMA needs the INCLUDE_KYMERA_AEC compilation switch for this platform
#endif

#ifdef INCLUDE_AMA_WUW
#define Ama_IsWakeUpWordFeatureIncluded() (TRUE)
  #if !defined(INCLUDE_AMA)
      #error INCLUDE_AMA is missing in combination with INCLUDE_AMA_WUW
  #endif
  #if !defined(INCLUDE_WUW)
      #error INCLUDE_WUW is missing in combination with INCLUDE_AMA_WUW
  #endif
#else
#define Ama_IsWakeUpWordFeatureIncluded() (FALSE)
#endif  /* INCLUDE_AMA_WUW */

/*! \brief Initialise the AMA component.

    \param task The init task
    \return TRUE if component initialisation was successful, otherwise FALSE.
*/
bool Ama_Init(Task init_task);

#endif /* AMA_H */
