/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		audio_use_case_source.h
\defgroup   audio_use_case Audio Use Case
\ingroup    audio_domain
\brief      APIs related to audio use case sources

*/

#ifndef AUDIO_USE_CASE_SOURCE_H_
#define AUDIO_USE_CASE_SOURCE_H_

#include "audio_use_case_types.h"
#include <source_param_types.h>

/*! @{ */

/*! \brief Associate a source with an audio use case instance
    \param source The generic audio source
    \param instance The instance to associate it with
*/
void AudioUseCase_AssociateSourceWithInstance(generic_source_t source, audio_use_case_instance_t instance);

/*! @} */

#endif /* AUDIO_USE_CASE_SOURCE_H_ */
