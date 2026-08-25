/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\addtogroup audio_driver
\brief      Audio Driver core observer interface

*/

#ifndef AUDIO_DRIVER_OBSERVER_H_
#define AUDIO_DRIVER_OBSERVER_H_

#include <audio_use_case_types.h>
#include <data_blob_types.h>

/*! @{ */

/*! Interface maps to that of audio driver core (audio_driver.h) */
typedef struct
{
    void (*PreparingInd)(audio_use_case_instance_t instance);
    void (*PreparedInd)(audio_use_case_instance_t instance);
    void (*StartingInd)(audio_use_case_instance_t instance);
    void (*StartedInd)(audio_use_case_instance_t instance);
    void (*StoppingInd)(audio_use_case_instance_t instance);
    void (*StoppedInd)(audio_use_case_instance_t instance);
    void (*BeginIoctlInd)(audio_use_case_instance_t instance, unsigned ioctl_id);
    void (*EndIoctlInd)(audio_use_case_instance_t instance, unsigned ioctl_id);
} audio_driver_observer_if;

/*! \brief Register as an observer of Audio Driver
    \param use_case The specific audio use case to observe, use audio_use_case_all to observe all use cases
    \param observer_if The observers interface
*/
void AudioDriver_RegisterObserver(audio_use_case_t use_case, const audio_driver_observer_if *observer_if);

/*! @} */

#endif /* AUDIO_DRIVER_OBSERVER_H_ */
