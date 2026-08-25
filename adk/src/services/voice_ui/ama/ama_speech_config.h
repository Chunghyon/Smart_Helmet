/*!
\copyright  Copyright (c) 2018- 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_speech_config.c
\brief  Definition of the APIs to manage the AMA speech configuration
*/

#ifndef __AMA_SPEECH_H_
#define __AMA_SPEECH_H_

#include "speech.pb-c.h"

#define PRIVACY_MODE_BLOCKS_LIVE_CAPTURE    TRUE

#define AMA_SPEECH_INITIATOR_DEFAULT        SPEECH_INITIATOR__TYPE__TAP
#define AMA_SPEECH_AUDIO_PROFILE_DEFAULT    AUDIO_PROFILE__NEAR_FIELD
#define AMA_SPEECH_AUDIO_FORMAT_DEFAULT     AUDIO_FORMAT__MSBC
#define AMA_SPEECH_AUDIO_SOURCE_DEFAULT     AUDIO_SOURCE__STREAM

/*! \brief Set the dialog ID for a new speech dialog
 */
void Ama_NewSpeechDialogId(void);

/*! \brief Gets the current speech dialog ID
 *  \return The current speech dialog ID
 */
uint32 Ama_GetCurrentSpeechDialogId(void);

/*! \brief Update the speech dialog ID
 *  \param provided_id The dialog ID from the phone
 */
void Ama_UpdateSpeechDialogId(uint32 provided_id);

/*! \brief Set the speech audio source
 *  \param source Audio source to set
 */
void Ama_SetSpeechAudioSource(AudioSource source);

/*! \brief Set the speech audio profile
 *  \param profile Audio profile to set
 */
void Ama_SetSpeechAudioProfile(AudioProfile profile);

/*! \brief Set the speech audio format
 *  \param format Audio format to set
 */
void Ama_SetSpeechAudioFormat(AudioFormat format);

/*! \brief Set the speech initiator type
 *  \param initiator The speech initiator type to set
 */
void Ama_SetSpeechInitiator(SpeechInitiator__Type initiator);

/*! \brief Get the speech audio source
 *  \return The audio source
 */
AudioSource Ama_GetSpeechAudioSource(void);

/*! \brief Get the speech audio profile
 *  \return The audio profile
 */
AudioProfile Ama_GetSpeechAudioProfile(void);

/*! \brief Get the speech audio format
 *  \return The audio format
 */
AudioFormat Ama_GetSpeechAudioFormat(void);

/*! \brief Get the speech initiator type
 *  \return The speech initiator type
 */
SpeechInitiator__Type Ama_GetSpeechInitiatorType(void);

/*! \brief Set the speech settings to the default values
 */
void Ama_SetSpeechSettingsToDefault(void);

/*! \brief Check if wake word detection is allowed to start
 *  \return TRUE if wake word detection is allowed to start, otherwise FALSE
 */
bool Ama_IsWakeUpWordDetectionAllowedToStart(void);

/*! \brief Check if live capture is allowed to start
 *  \return TRUE if live capture is allowed to start, otherwise FALSE
 */
bool Ama_IsLiveCaptureAllowedToStart(void);

#endif /* __AMA_SPEECH_H_ */
