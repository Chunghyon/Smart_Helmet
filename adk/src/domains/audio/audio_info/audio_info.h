/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides audio info to the context framework
*/

#ifndef AUDIO_INFO_H_
#define AUDIO_INFO_H_

#include "source_param_types.h"

/*! \brief Get the currently routed generic source

    \return The currently routed generic source
*/
generic_source_t AudioInfo_GetRoutedGenericSource(void);

/*! \brief Initialise audio_info component

    Registers as a provider to the context framework
*/
void AudioInfo_Init(void);

#endif /* AUDIO_INFO_H_ */
