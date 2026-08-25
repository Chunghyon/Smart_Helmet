/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_send_response.h
\brief  Definition of the APIs to send AMA responses to the phone
*/

#ifndef AMA_SEND_RESPONSE_H
#define AMA_SEND_RESPONSE_H

#include "accessories.pb-c.h"

/*! \brief Send generic command response to the phone
 *  \param command The command type
 *  \param ErrorCode Information on the success or failure of the request
 */
void AmaProtocol_SendGenericResponse(Command command, ErrorCode error_code);

/*! \brief Send COMMAND__GET_LOCALES response to the phone
 *  \param locales Pointer to the locales
 */
void AmaProtocol_SendGetLocalesResponse(Locales * locales);

/*! \brief Send COMMAND__GET_DEVICE_INFORMATION response to the phone
 *  \param device_information Pointer to the device information
 */
void AmaProtocol_SendGetDeviceInformationResponse(DeviceInformation * device_information);

/*! \brief Send COMMAND__GET_DEVICE_CONFIGURATION response to the phone
 *  \param device_configuration Pointer to the device configuration
 */
void AmaProtocol_SendGetDeviceConfigurationResponse(DeviceConfiguration * device_configuration);

/*! \brief Send COMMAND__GET_DEVICE_FEATURES response to the phone
 *  \param device_features Pointer to the device features
 */
void AmaProtocol_SendGetDeviceFeaturesResponse(DeviceFeatures * device_features);

/*! \brief Send COMMAND__UPGRADE_TRANSPORT response to the phone
 *  \param connection_details Pointer to the connecton details
 */
void AmaProtocol_SendUpgradeTransportResponse(ConnectionDetails * connection_details);

/*! \brief Send COMMAND__GET_STATE response to the phone
 *  \param state Pointer to the state data
 *  \param ErrorCode Information of the success or failure of the request
 */
void AmaProtocol_SendGetStateResponse(State * state, ErrorCode error_code);

/*! \brief Send COMMAND__PROVIDE_SPEECH response to the phone
 *  \param accept Accept the request to provide speech
 *  \param dialog_id The new dialog ID
 */
void AmaProtocol_SendProvideSpeechResponse(bool accept, uint32 dialog_id, AudioProfile profile, AudioFormat format, AudioSource source);

#endif // AMA_SEND_RESPONSE_H
