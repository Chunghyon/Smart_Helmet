/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_driver
\ingroup    audio_domain
\brief      Audio Driver core user interface

*/

#ifndef AUDIO_DRIVER_H_
#define AUDIO_DRIVER_H_

#include <audio_use_case_types.h>
#include <data_blob_types.h>
#include <volume_types.h>

/*! @{ */

/*! \brief Prepares the graph for the specified use case (readying it without starting it)
    \param instance The use case instance
    \param params The parameters to use, see the respective use case handler implementation API for details
*/
void AudioDriver_PrepareUseCase(audio_use_case_instance_t instance, const data_blob_t *params);

/*! \brief Starts the specified use case
    \param instance The use case instance
    \param params The parameters to use, see the respective use case handler implementation API for details
*/
void AudioDriver_StartUseCase(audio_use_case_instance_t instance, const data_blob_t *params);

/*! \brief Stops the specified use case, should also be called to undo a prepare request since this will tear down the graph
    \param instance The use case instance
    \param params The parameters to use, see the respective use case handler implementation API for details
*/
void AudioDriver_StopUseCase(audio_use_case_instance_t instance, const data_blob_t *params);

/*! \brief API to control/configure use case specific functionality
    \param instance The use case instance
    \param ioctl_id The use case specific ID for the ioctl request to process, see the respective use case handler implementation API for details
    \param params The parameters to use, see the respective use case handler implementation API for details
*/
void AudioDriver_IoctlUseCase(audio_use_case_instance_t instance, unsigned ioctl_id, const data_blob_t *params);

/*! @} */

#endif /* AUDIO_DRIVER_H_ */
