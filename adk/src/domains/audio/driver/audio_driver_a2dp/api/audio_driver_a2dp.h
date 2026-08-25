/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_driver_a2dp
\ingroup    audio_driver
\brief      Audio Driver A2DP handler interface

*/

#ifndef AUDIO_DRIVER_A2DP_H_
#define AUDIO_DRIVER_A2DP_H_

#include <audio_driver_ioctl_types.h>
#include <audio_a2dp_types.h>

/*! @{ */

/*! \brief Used in IOCTL Audio Driver API to specify the request
 */
typedef enum
{
    audio_driver_a2dp_set_volume = audio_driver_set_volume,
} audio_driver_a2dp_ioctl_ids_t;

typedef audio_a2dp_start_params_t audio_driver_a2dp_prepare_t;
typedef audio_a2dp_start_params_t audio_driver_a2dp_start_t;
typedef audio_a2dp_stop_params_t audio_driver_a2dp_stop_t;
typedef audio_driver_set_volume_t audio_driver_a2dp_set_volume_t;

/*! \brief Init A2DP audio driver handler
 */
void AudioDriverA2dp_Init(void);

/*! @} */

#endif /* AUDIO_DRIVER_A2DP_H_ */
