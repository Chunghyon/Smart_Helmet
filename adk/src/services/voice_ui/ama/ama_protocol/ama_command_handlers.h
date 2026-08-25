/*!
\copyright  Copyright (c) 2018-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_command_handlers.h
\brief  Definition of the APIs to handle AMA commands from the phone
*/

#ifndef _AMA_COMMAND_HANDLERS_H
#define _AMA_COMMAND_HANDLERS_H

#include "accessories.pb-c.h"

/*! \brief Handles COMMAND__NOTIFY_SPEECH_STATE message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_NotifySpeechState(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__STOP_SPEECH message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_StopSpeech(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__GET_DEVICE_INFORMATION message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_GetDeviceInformation(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__GET_DEVICE_CONFIGURATION message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_GetDeviceConfiguration(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__START_SETUP message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_StartSetup(ControlEnvelope * control_envelope_in);

/*! \brief Handles COMMAND__COMPLETE_SETUP message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_CompleteSetup(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__UPGRADE_TRANSPORT message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_UpgradeTransport(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__SWITCH_TRANSPORT message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_SwitchTransport(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__SYNCHRONIZE_SETTINGS message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_SynchronizeSettings(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__GET_STATE message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_GetState(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__SET_STATE message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_SetState(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__MEDIA_CONTROL message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_MediaControl(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__OVERRIDE_ASSISTANT message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_OverrideAssistant(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__PROVIDE_SPEECH message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_ProvideSpeech(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__ENDPOINT_SPEECH message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_EndpointSpeech(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__FORWARD_AT_COMMAND message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_ForwardATCommand(ControlEnvelope *control_envelope_in);

/*! \brief Handles cases where the message ID is not recognised
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_NotHandled(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__KEEP_ALIVE message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_KeepAlive(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__SYNCHRONIZE_STATE message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_SynchronizeState(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__GET_DEVICE_FEATURES message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_GetDeviceFeatures(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__GET_LOCALES message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_GetLocales(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__SET_LOCALES message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_SetLocale(ControlEnvelope *control_envelope_in);

/*! \brief Handles COMMAND__LAUNCH_APP message
 *  \param control_envelope_in the unpacked message from the phone
 */
void AmaCommandHandlers_LaunchApp(ControlEnvelope *control_envelope_in);

/*! \brief Populates a DeviceInformation structure
 *  \param device_information Pointer to the DeviceInformation structure to be populated
 */
void AmaCommandHandlers_PopulateDeviceInformation(DeviceInformation * device_information);

#endif /* _AMA_COMMAND_HANDLERS_H */
