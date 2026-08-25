/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile HFP Profile
\ingroup    profiles
\brief      Interface to HFP domain platform specific functionality.
*/

#ifndef HFP_PROFILE_PORT_H_
#define HFP_PROFILE_PORT_H_

#include "hfp_profile_instance.h"
#include <message.h>

/*! \brief Interface to the platform-specific implementation of the HFP domain.

    \param task - The task that will receive the messages.
    \param id - The message Id.
    \param message - If the message Id specified has a data struct associated, this is provided here.

    \return a bool indicating whether the message was handed or not
*/
bool HfpProfilePort_HandleMessage(Task task, MessageId id, Message message);

/*! \brief Initialise the library layer
*/
void hfpProfile_InitHfpLibrary(void);

/*! \brief Send an audio connect response

    \param instance The instance on which the request was received
    \param is_esco Whether or not the request was for eSCO
    \param accept Whether to accept the connection
*/
void hfpProfile_SendAudioConnectResponse(hfpInstanceTaskData* instance, bool is_esco, bool accept);

/*! \brief Initialise any instance data which is port specific

    \param instance HFP instance
*/
void hfpProfile_InitialiseInstancePortSpecificData(hfpInstanceTaskData * instance);

/*! \brief Copy any instance data which is port specific

    \param target_instance HFP instance to copy to
    \param source_instance HFP instance to copy from
*/
void hfpProfile_CopyInstancePortSpecificData(hfpInstanceTaskData * target_instance, hfpInstanceTaskData * source_instance);

/*! \brief Connect SLC

    \param instance HFP instance
*/
void hfpProfile_ConnectSlc(hfpInstanceTaskData* instance);

/*! \brief Disconnect SLC

    \param instance HFP instance
*/
void hfpProfile_DisconnectSlc(hfpInstanceTaskData* instance);

/*! \brief Read remote supported features

    \param instance HFP instance
*/
void hfpProfile_ReadRemoteSupportedFeatures(hfpInstanceTaskData* instance);

/*! \brief Send AT+BIEV battery update to specific instance

    \param instance HFP instance to which AT+BIEV should be sent.
    \param percent Battery level as percentage of full
*/
void hfpProfile_SendBievCommandToInstance(hfpInstanceTaskData* instance, uint8 percent);

/*! \brief Request all call status information from the AG 

    \param instance HFP instance
*/
void hfpProfile_RefreshCallStateRequest(hfpInstanceTaskData* instance);

/*! \brief Request to enable or disable caller ID

    \param instance HFP instance
*/
void hfpProfile_CallerIdEnable(hfpInstanceTaskData* instance, bool enable);

/*! \brief Last number redial

    \param instance HFP instance
*/
void hfpProfile_LastNumberRedial(hfpInstanceTaskData* instance);

/*! \brief Enable voice dial

    \param instance HFP instance
*/
void hfpProfile_VoiceDialEnable(hfpInstanceTaskData* instance);

/*! \brief Disable voice dial

    \param instance HFP instance
*/
void hfpProfile_VoiceDialDisable(hfpInstanceTaskData* instance);

/*! \brief Dial a number

    \param instance HFP instance
    \param number The number to dial
    \param length_number The length of the number to dial
*/
void hfpProfile_DialNumber(hfpInstanceTaskData* instance, uint8* number, unsigned length_number);

/*! \brief Answer a call

    \param instance HFP instance
*/
void hfpProfile_AnswerCall(hfpInstanceTaskData* instance);

/*! \brief Reject a call

    \param instance HFP instance
*/
void hfpProfile_RejectCall(hfpInstanceTaskData* instance);

/*! \brief Terminate a call

    \param instance HFP instance
*/
void hfpProfile_TerminateCall(hfpInstanceTaskData* instance);

/*! \brief Set microphone gain

    \param instance HFP instance
    \param gain The gain
*/
void hfpProfile_SetMicrophoneGain(hfpInstanceTaskData *instance, uint8 gain);

/*! \brief Set speaker gain

    \param instance HFP instance
    \param gain The gain
*/
void hfpProfile_SetSpeakerGain(hfpInstanceTaskData *instance, uint8 gain);

/*! \brief Handle audio transfer request

    \param instance HFP instance
    \param direction The direction of transfer
*/
void hfpProfile_HandleAudioTransferRequest(hfpInstanceTaskData *instance, voice_source_audio_transfer_direction_t direction);

/*! \brief Release waiting or reject incoming call

    \param instance HFP instance
*/
void hfpProfile_ReleaseWaitingRejectIncoming(hfpInstanceTaskData* instance);

/*! \brief Accept waiting call, release active call

    \param instance HFP instance
*/
void hfpProfile_AcceptWaitingReleaseActive(hfpInstanceTaskData* instance);

/*! \brief Accept waiting call, hold active call

    \param instance HFP instance
*/
void hfpProfile_AcceptWaitingHoldActive(hfpInstanceTaskData* instance);

/*! \brief Add held call to a multiparty call

    \param instance HFP instance
*/
void hfpProfile_AddHeldToMultiparty(hfpInstanceTaskData* instance);

/*! \brief Send join calls and hang up AT command to instance

    \param instance HFP instance
*/
void hfpProfile_JoinCallsAndHangUp(hfpInstanceTaskData* instance);

/*! \brief Gets the SCO and SLC Sinks

    This function retrieves the SLC and SCO Sinks and sets the correspoding instance fields.

    \param[in] instance   HFP instance task data
*/
void hfpProfile_GetSinks(hfpInstanceTaskData *instance);

#endif /* HFP_PROFILE_PORT_H_ */
