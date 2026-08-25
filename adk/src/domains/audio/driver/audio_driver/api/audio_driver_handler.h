/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\addtogroup audio_driver
\brief      Audio Driver core handler interface

*/

#ifndef AUDIO_DRIVER_HANDLER_H_
#define AUDIO_DRIVER_HANDLER_H_

#include <audio_use_case_types.h>

/*! @{ */

/*! \brief Must be called by an audio driver handler, when a request had to be completed asynchronously, upon completion of the request
    \param instance The use case instance that received the aforementioned request
    \NOTE: Only a placeholder, async handling has not yet been implemented
*/
void AudioDriver_HandlerDone(audio_use_case_instance_t instance);

/*! @} */

#endif /* AUDIO_DRIVER_HANDLER_H_ */
