/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       microphones.h
\defgroup   microphones Microphones
\ingroup    audio_domain
\brief      Component responsible for interacting with microphones and microphone user tracking
*/

#ifndef MICROPHONES_H_
#define MICROPHONES_H_

#include <audio_plugin_if.h>
#include <stream.h>

/*! @{ */

#define MAX_SUPPORTED_MICROPHONES   6
#define MICROPHONE_NONE 0xF

/*! \brief Enumeration of microphone user types.

    The user type is used to aid tracking of microphone users and grant access based their priority.
    Normal and high priority users require exclusive access to a microphone resource
    and so cannot co-exist with other exclusive users. 
    Existing high priority user > new high priority user > existing normal priority user > new normal priority user
    
    Non exclusive users can access a microphone resource regardless of whether it is currently in use,
    and they do not block access from other exclusive users.
 */
typedef enum
{
    invalid_user,
    normal_priority_user,
    high_priority_user,
    non_exclusive_user,
} microphone_user_type_t;

/*! \brief Gets a specific microphone configuration.

    \param microphone_number

    \return Pointer to the specified microphone config
 */
audio_mic_params * Microphones_GetMicrophoneConfig(uint16 microphone_number);

/*! \brief Change microphone configuration.

	\param microphone_number
	\param config

	\return Void
 */
void Microphones_SetMicrophoneConfig(uint16 microphone_number, audio_mic_params *config);

/*! \brief Set the sample rate of the specified microphone.

    \param microphone_number
    \param sample_rate the sample rate in Hz
    \param microphone_user_type The user type requesting to turn on the microphone
 */
void Microphones_SetMicRate(uint16 microphone_number, uint32 sample_rate, microphone_user_type_t microphone_user_type);

/*! \brief Turn on and configure the specified microphone.
           For a full configuration of the microphone use Microphones_SetMicRate to
           set the sample rate before calling TurnOnMicrophone.

    \param microphone_number
    \param microphone_user_type The user type requesting to turn on the microphone

    \return Source for the configured microphone, NULL if not available
 */
Source Microphones_TurnOnMicrophone(uint16 microphone_number, microphone_user_type_t microphone_user_type);

/*! \brief Turn off the specified microphone.

    \param microphone_number
    \param microphone_user_type The user type requesting to turn off the microphone
 */
void Microphones_TurnOffMicrophone(uint16 microphone_number, microphone_user_type_t microphone_user_type);

/*! \brief Initialises the internal state of the component.

 */
void Microphones_Init(void);

/*! \brief Returns the maximum number of microphones supported.
    \param null
    \return maximum number of microphones supported
 */
uint8 Microphones_MaxSupported(void);

/*! \brief Get the source for a specific microphone

     \param microphone_number

     \return Source for the microphone, NULL if not turned on
 */
Source Microphones_GetMicrophoneSource(uint16 microphone_number);

/*! \brief Check if a microphone is in use
     \param microphone_number
     \return TRUE: Microphone is in use; FALSE: Microphone is not in use
 */
bool Microphones_IsMicrophoneInUse(uint16 microphone_number);

/*! @} */

#endif /* MICROPHONES_H_ */
