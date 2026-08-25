/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_notify_app_msg.h
\brief  Definition of the APIs to notify the AMA client of events
*/

#ifndef AMA_NOTIFY_APP_MSG_H
#define AMA_NOTIFY_APP_MSG_H

#include "ama_msg_types.h"
#include "ama_transport_types.h"
#include "state.pb-c.h"

#include <csrtypes.h>

void AmaProtocol_InitialiseAppNotifier(Task task);

/*! \brief Notfiy AMA client level of transport switch confirmation
 *  \param transport The AMA transport
 */
void AmaProtocol_NotifyAppTransportSwitch(ama_transport_type_t transport);

/*! \brief Notfiy AMA client level of a change to the speech state
 */
void AmaProtocol_NotifyAppSpeechState(SpeechState state);

/*! \brief Notfiy AMA client level of stop speech confirmation
 */
void AmaProtocol_NotifyAppStopSpeech(uint32 dialog_id);

/*! \brief Notfiy AMA client level of provide speech request
 *  \param dialog_id The dialog ID for the speech session
 */
void AmaProtocol_NotifyAppProvideSpeech(uint32 dialog_id);

/*! \brief Notfiy AMA client level of synchronize settings request
 */
void AmaProtocol_NotifyAppSynchronizeSetting(void);

/*! \brief Notfiy AMA client level of override assistant request
 */
void AmaProtocol_NotifyAppOverrideAssistant(void);

/*! \brief Notfiy AMA client level of AT command request
 *  \param at_cmd The AT command
 *  \param forward_at_command AT command string
 */
void AmaProtocol_NotifyAppAtCommand(ama_at_cmd_t ama_at_cmd, char* forward_at_command);

void AmaProtocol_NotifyAppStartSetup(void);

/*! \brief Notfiy AMA client level that setup has been completed
 */
void AmaProtocol_NotifyAppCompleteSetup(void);

/*! \brief Notfiy AMA client level to enable classic pairing
 */
void AmaProtocol_NotifyAppEnableClassicPairing(void);

/*! \brief Notfiy AMA client level of upgrade transport
 */
void AmaProtocol_NotifyAppUpgradeTransport(void);

/*! \brief Notfiy AMA client level to issue a media control
 */
void AmaProtocol_NotifyAppMediaControl(void);

/*! \brief Notfiy AMA client level to send supported device features
 */
void AmaProtocol_NotifyAppGetDeviceFeatures(void);

/*! \brief Notfiy AMA client level to send the state of a specifed feature
 *  \param feature_id The ID of the feature whose state has been requested
 */
void AmaProtocol_NotifyAppGetState(uint32 feature_id);

/*! \brief Notfiy AMA client level to set the state of a specifed feature
 *  \param feature_id The ID of the feature whose state has been requested to be set
 *  \param value_case The type of the state to be state (i.e. boolean, integer, etc)
 *  \param value The value to set
 */
void AmaProtocol_NotifyAppSetState(uint32 feature_id, State__ValueCase value_case, uint32 value);

/*! \brief Notfiy AMA client level that speech has reached its endpoint
 */
void AmaProtocol_NotifyAppSpeechEndpoint(void);

/*! \brief Notfiy AMA client level to synchronize states
 *  \param feature_id The ID of the feature to be synchronized
 *  \param value_case The type of the state (i.e. boolean, integer, etc)
 *  \param value The value to set
 */
void AmaProtocol_NotifyAppSynchronizeState(uint32 feature_id, State__ValueCase value_case, uint32 value);

/*! \brief Notfiy AMA client level to keep alive
 */
void AmaProtocol_NotifyAppKeepAlive(void);

/*! \brief Notfiy AMA client level that we did not handle a command
 *  \param command The command
 */
void AmaProtocol_NotifyAppUnhandledCommand(Command command);

/*! \brief Notfiy AMA client level to send its device information
 */
void AmaProtocol_NotifyAppGetDeviceInformation(uint32 device_id);

/*! \brief Notfiy AMA client level to send its device configuration
 */
void AmaProtocol_NotifyAppGetDeviceConfiguration(void);

/*! \brief Notfiy AMA client level to launch phone application
 *  \param app_id ID string for the phone application
 */
void AmaProtocol_NotifyAppLaunchApp(char * app_id);

/*! \brief Notfiy AMA client level to send locales
 */
void AmaProtocol_NotifyAppGetLocales(void);

/*! \brief Notfiy AMA client level to set the locale
 *  \param locale A string of the locale to set
 */
void AmaProtocol_NotifyAppSetLocale(char * locale);

#endif // AMA_NOTIFY_APP_MSG_H
