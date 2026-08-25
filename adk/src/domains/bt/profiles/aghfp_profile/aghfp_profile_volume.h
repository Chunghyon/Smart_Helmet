/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   aghfp_profile_volume HFP Profile Volume
\ingroup    aghfp_profile
\brief      The voice source volume interface implementation for HFP sources
*/

#ifndef AGHFP_PROFILE_VOLUME_H
#define AGHFP_PROFILE_VOLUME_H

#include "voice_sources.h"
#include "voice_sources_volume_interface.h"

/*! \brief Gets the HFP volume interface.

    \return The voice source volume interface for an HFP source
 */
const voice_source_volume_interface_t * AghfpProfile_GetVoiceSourceVolumeInterface(void);

#endif /* AGHFP_PROFILE_VOLUME_H */
