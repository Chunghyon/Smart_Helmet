/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		audio_use_case_instance.h
\addtogroup audio_use_case
\brief      APIs related to audio use case instances

*/

#ifndef AUDIO_USE_CASE_INSTANCE_H_
#define AUDIO_USE_CASE_INSTANCE_H_

#include "audio_use_case_types.h"
#include <source_param_types.h>

/*! @{ */

/*! \brief Create audio use case instance
           Supports a single instance per use_case and will panic otherwise
    \param use_case The use case type of the instance
    \return The audio use case instance handler
*/
audio_use_case_instance_t AudioUseCase_CreateInstance(audio_use_case_t use_case);

/*! \brief Get instance for use case type
           Will Panic if the use_case is not associated with a unique instance
    \param use_case The audio use case type
    \return The audio use case instance handler
*/
audio_use_case_instance_t AudioUseCase_GetInstanceForUseCase(audio_use_case_t use_case);

/*! \brief Create audio use case instance for the specified source
           Supports a single instance per source and will panic otherwise
    \param use_case The use case type of the instance
    \param source The generic audio source to associate with the instance
    \return The audio use case instance handler
*/
audio_use_case_instance_t AudioUseCase_CreateInstanceForSource(audio_use_case_t use_case, generic_source_t source);

/*! \brief Get audio use case instance associated with a source
           Will Panic if the source is not associated with a unique instance
    \param source The generic audio source
    \return The audio use case instance handler
*/
audio_use_case_instance_t AudioUseCase_GetInstanceForSource(generic_source_t source);

/*! \brief Get audio use case type associated with an audio use case instance
    \param instance The audio use case instance handler
    \return The audio use case type
*/
audio_use_case_t AudioUseCase_GetUseCaseFromInstance(audio_use_case_instance_t instance);

/*! @} */

#endif /* AUDIO_USE_CASE_INSTANCE_H_ */
