/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\addtogroup audio_driver
\brief      Audio Driver common IOCTL types

*/

#ifndef AUDIO_DRIVER_IOCTL_TYPES_H_
#define AUDIO_DRIVER_IOCTL_TYPES_H_

#include <volume_types.h>

/*! @{ */

/*! \brief Common IOCTL ID definitions (check respective use case handler API to ensure it's supported)
 */
typedef enum
{
    audio_driver_set_volume,
} audio_driver_ioctl_ids_t;

/*! \brief Parameters used for Set Volume IOCTL
 */
typedef volume_t audio_driver_set_volume_t;

/*! @} */

#endif /* AUDIO_DRIVER_IOCTL_TYPES_H_ */
