/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice Assistant Session state private header
*/

#ifndef VOICE_UI_SESSION_H_
#define VOICE_UI_SESSION_H_

#include "audio_sources.h"

/*! \brief Init the module (as part of the device init sequence)
 */
void VoiceUi_VaSessionInit(void);

/*! \brief Reset VA session state
 */
void VoiceUi_VaSessionReset(void);

audio_source_t VoiceUi_GetVaAudioSource(audio_source_t source);

#endif // VOICE_UI_SESSION_H_
