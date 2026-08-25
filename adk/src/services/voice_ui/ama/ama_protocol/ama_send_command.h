/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_send_command.h
\brief  Definition of the APIs to send AMA commands to the phone
*/

#ifndef _AMA_SEND_COMMAND_H
#define _AMA_SEND_COMMAND_H

#include <csrtypes.h>
#include "speech.pb-c.h"
#include "accessories.pb-c.h"
#include <data_blob_types.h>

/*! \brief Send COMMAND__START_SPEECH to phone
 *  \param speech_initiator The initiation method for the speech
 *  \param audio_profile The audio profile
 *  \param audio_format The speech data encoding format
 *  \param audio_source The audio source type
 *  \param start_sample The wake-up word start index in samples
 *  \param wuw_metadata The metadata associated with the wake-up word
 *  \param end_sample The wake-up word end index in samples
 */
void AmaSendCommand_StartSpeech(SpeechInitiator__Type speech_initiator,
                                             AudioProfile audio_profile,
                                             AudioFormat audio_format,
                                             AudioSource audio_source,
                                             uint32 start_sample,
                                             uint32 end_sample,
                                             data_blob_t wuw_metadata,
                                             uint32 current_dialog_id);

/*! \brief Send COMMAND__STOP_SPEECH to phone
 *  \param reason The reason for stopping speech
 */
void AmaSendCommand_StopSpeech(ErrorCode reason, uint32 current_dialog_id);

/*! \brief Send COMMAND__ENDPOINT_SPEECH to phone
 */
void AmaSendCommand_EndSpeech(uint32 current_dialog_id);

/*! \brief Send COMMAND__INCOMING_CALL to phone
 *  \param caller_number Pointer to null terminated string containing the caller's number
 */
void AmaSendCommand_IncomingCall(char * caller_number);

/*! \brief Send COMMAND__KEEP_ALIVE to phone
 */
void AmaSendCommand_KeepAlive(void);

/*! \brief Send COMMAND__SYNCHRONIZE_STATE to phone
 *  \param feature The ID for the feature to be synced
 *  \param value_case The type of value associated with the feature
 *  \param value The value
 */
void AmaSendCommand_SyncState(uint32 feature, State__ValueCase value_case, uint16 value);

/*! \brief Send COMMAND__GET_STATE to phone
 *  \param feature The ID for the feature to be get
 */
void AmaSendCommand_GetState(uint32 feature);

/*! \brief Send COMMAND__RESET_CONNECTION to phone
 *  \param timeout Timeout in seconds
 *  \param force_disconnect Whether disconnection should be forced
 */
void AmaSendCommand_ResetConnection(uint32 timeout, bool force_disconnect);

/*! \brief Send COMMAND__GET_CENTRAL_INFORMATION to phone
 */
void AmaSendCommand_GetCentralInformation(void);

/*! \brief Send COMMAND__NOTIFY_DEVICE_CONFIGURATION to phone
 *  \param require_va_override TRUE if voice assitant override is requied, otherwise FALSE
 */
void AmaSendCommand_NotifyDeviceConfig(bool require_va_override);

/*! \brief Send COMMAND__NOTIFY_DEVICE_INFORMATION to phone
 */
void AmaSendCommand_NotifyDeviceInformation(DeviceInformation * device_information);

#endif /* _AMA_SEND_COMMAND_H */

