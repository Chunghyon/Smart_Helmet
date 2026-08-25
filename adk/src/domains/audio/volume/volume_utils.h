/*!
\copyright  Copyright (c) 2018-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		volume_utils.h
\defgroup   volume_utils Volume Utils
\ingroup    volume_group
\brief      volume_t related utility functions.
*/

#ifndef VOLUME_UTILS_H_
#define VOLUME_UTILS_H_

#include "volume_types.h"

/*! @{ */

/*! \brief Convert a volume_t from one config to another.

    \param [in] volume_to_convert The input volume to convert from. It must
                                  contain the raw volume value and its config.
    \param [in] out_format The volume config to convert the volume to.

    \return The raw value of the converted volume.
*/
int16 VolumeUtils_ConvertToVolumeConfig(volume_t volume_to_convert, volume_config_t output_format);

bool VolumeUtils_VolumeIsEqual(volume_t volume_1, volume_t volume_2);

unsigned VolumeUtils_GetVolumeInPercent(volume_t volume);
int VolumeUtils_GetLimitedVolume(volume_t volume, volume_t limit);

int VolumeUtils_IncrementVolume(volume_t volume);
int VolumeUtils_DecrementVolume(volume_t volume);

/*! \brief Return the step size calculated for the volume configuration 

    \param config The volume configuration

    \return The calculated step size */
int VolumeUtils_GetStepSize(volume_config_t config);

/*! \brief Return volume limited to the range supplied 

    Helper function that makes sure volume is between range.min and
    range.max

    \param volume   The volume to limit
    \param range    The range to limit to

    \return volume, if within the rane, or one of range.min, range.max
*/
int VolumeUtils_LimitVolumeToRange(int volume, range_t range);

/*! \brief Return volume in DB (limits value to the configured supported range)
 */
int16 VolumeUtils_GetVolumeInDb(volume_t volume);

/*! @} */

#endif /* VOLUME_UTILS_H_ */
