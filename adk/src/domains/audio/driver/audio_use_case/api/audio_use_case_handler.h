/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		audio_use_case_handler.h
\addtogroup audio_use_case
\brief      APIs related to audio use case handlers

*/

#ifndef AUDIO_USE_CASE_HANDLER_H_
#define AUDIO_USE_CASE_HANDLER_H_

#include "audio_use_case_types.h"
#include <data_blob_types.h>

/*! @{ */

/*! Status returned for audio use case handler requests */
typedef enum
{
    /*! Returned upon successful completion of the request */
    audio_use_case_request_success,
    /*! Returned when the request has to be completed asynchronously */
    audio_use_case_request_pending,
} audio_use_case_request_status_t;

/*! Interface maps to that of audio driver core (audio_driver.h) */
typedef struct
{
    audio_use_case_request_status_t (*Prepare)(audio_use_case_instance_t instance, const data_blob_t *params);
    audio_use_case_request_status_t (*Start)(audio_use_case_instance_t instance, const data_blob_t *params);
    audio_use_case_request_status_t (*Stop)(audio_use_case_instance_t instance, const data_blob_t *params);
    audio_use_case_request_status_t (*Ioctl)(audio_use_case_instance_t instance, unsigned ioctl_id, const data_blob_t *params);
} audio_use_case_handler_if;

/*! \brief Associate an audio use case with a handlers interface
    \param use_case The use case type
    \param handler The handlers interface
*/
void AudioUseCase_RegisterHandler(audio_use_case_t use_case, const audio_use_case_handler_if *handler);

/*! \brief Get handler interface associated with an audio use case instance
           Will panic if the instance is not associated with a handler interface
    \param use_case The audio use case instance
    \return The handler interface associated with it
*/
const audio_use_case_handler_if * AudioUseCase_GetInstanceHandler(audio_use_case_instance_t instance);

/*! @} */

#endif /* AUDIO_USE_CASE_HANDLER_H_ */
