/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice UI audio private header
*/

#ifndef VOICE_UI_AUDIO_H_
#define VOICE_UI_AUDIO_H_

#include "voice_ui_va_client_if.h"

/*! \brief Init audio module
 */
void VoiceUi_AudioInit(void);

/*! \brief Enter suspended state (Unroute  VA related audio, don't allow VA related audio to be routed).
 */
void VoiceUi_SuspendAudio(void);

/*! \brief Exist suspended state (Resume VA related audio such as Wake-Up-Word detection).
 */
void VoiceUi_ResumeAudio(void);

/*! \brief Unroute all VA related audio (unlike suspend it doesn't stay in that state and nothing is resumed afterwards).
 */
void VoiceUi_UnrouteAudio(void);

/*! \brief Check if VA is active.
 *  \return bool TRUE if detection or capture are active, otherwise FALSE.
 */
bool VoiceUi_IsVaActive(void);

/*! \brief Check if WuW detection suspended for a given Voice Assistant.
    \param va_handle Voice assistant handle provided at registration.
    \return TRUE if WuW detection is suspended, otherwise FALSE
 */
bool VoiceUi_IsDetectionSuspended(voice_ui_handle_t * va_handle);

#ifdef HOSTED_TEST_ENVIRONMENT
#include "voice_audio_manager.h"
bool VoiceUi_CaptureDataReceived(Source source);
va_audio_wuw_detected_response_t VoiceUi_WakeUpWordDetected(const va_audio_wuw_detection_info_t *wuw_info);
void VoiceUi_TestResetAudio(void);
void VoiceUi_AdjustBtBandwidthUsage(uint8 level);
#endif

#endif /* VOICE_UI_AUDIO_H_ */
