/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		wired_audio_source_interface.h
\addtogroup wired_source
\brief      The wired audio source interface implementations.
*/

#ifndef WIRED_AUDIO_SOURCE_INTERFACE_H_
#define WIRED_AUDIO_SOURCE_INTERFACE_H_

#include "audio_sources_audio_interface.h"
#include "source_param_types.h"

/*! @{ */

/*! \brief Gets the Wired audio interface.

    \return The audio source audio interface for an Wired audio source
 */
const audio_source_audio_interface_t * WiredAudioSource_GetWiredAudioInterface(void);

/*! @} */

#endif /* WIRED_AUDIO_SOURCE_INTERFACE_H_ */

