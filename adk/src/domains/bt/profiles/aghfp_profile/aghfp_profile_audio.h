/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      The voice source audio interface implementation for aghfp voice sources
*/


#ifndef AGHFP_PROFILE_AUDIO_H
#define AGHFP_PROFILE_AUDIO_H

#include "aghfp_profile_typedef.h"
#include "voice_sources_audio_interface.h"
#include <aghfp.h>
#include "aghfp_profile_typedef.h"

/*! \brief Gets the AGHFP audio interface.

    \return The voice source audio interface for an AGHFP source
 */
const voice_source_audio_interface_t * AghfpProfile_GetAudioInterface(void);

void AghfpProfile_StoreConnectParams(aghfpInstanceTaskData * instance, 
                                              uint8 codec, uint8 wesco, uint8 tesco, 
                                              uint16 qce_codec_mode_id, 
                                              bool using_wbs);

void AghfpProfileAudio_ConnectSco(aghfpInstanceTaskData* instance);
void AghfpProfileAudio_DisconnectSco(aghfpInstanceTaskData* instance);

#endif // AGHFP_PROFILE_AUDIO_H
