/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_notify_app_msg.c
\brief  Implementation of the APIs to notify the AMA client of events
*/

#ifdef INCLUDE_AMA
#include "ama_notify_app_msg.h"
#include "logging.h"
#include "panic.h"

#define CARRIAGE_RETURN      (1)

static Task app_task = NULL;

static void amaProtocol_SendAppMsg(ama_message_type_t id, void* data)
{
    DEBUG_LOG("amaProtocol_SendAppMsg enum:ama_message_type_t:%d", id);

    if(app_task)
    {
        MessageSend(app_task, id, data);
    }
}

void AmaProtocol_InitialiseAppNotifier(Task task)
{
    app_task = task;
}

void AmaProtocol_NotifyAppTransportSwitch(ama_transport_type_t transport)
{
    MAKE_AMA_MESSAGE(AMA_SWITCH_TRANSPORT_IND);

    DEBUG_LOG(" AmaNotifyAppMsg_TransportSwitch %d", transport );
    message->transport = transport;
    amaProtocol_SendAppMsg(AMA_SWITCH_TRANSPORT_IND, message);
}

void AmaProtocol_NotifyAppSpeechState(SpeechState state)
{
    MAKE_AMA_MESSAGE(AMA_NOTIFY_SPEECH_STATE_IND);
    message->state = state;
    amaProtocol_SendAppMsg(AMA_NOTIFY_SPEECH_STATE_IND, message);
}

void AmaProtocol_NotifyAppStopSpeech(uint32 dialog_id)
{
    MAKE_AMA_MESSAGE(AMA_SPEECH_STOP_IND);
    message->dialog_id = dialog_id;
    amaProtocol_SendAppMsg(AMA_SPEECH_STOP_IND, message);
}

void AmaProtocol_NotifyAppProvideSpeech(uint32 dialog_id)
{
    MAKE_AMA_MESSAGE(AMA_SPEECH_PROVIDE_IND);
    message->dialog_id = dialog_id;
    amaProtocol_SendAppMsg(AMA_SPEECH_PROVIDE_IND, message);
}

void AmaProtocol_NotifyAppSynchronizeSetting(void)
{
    amaProtocol_SendAppMsg(AMA_SYNCHRONIZE_SETTING_IND, NULL);
}

void AmaProtocol_NotifyAppOverrideAssistant(void)
{
    amaProtocol_SendAppMsg(AMA_OVERRIDE_ASSISTANT_IND, NULL);
}

void AmaProtocol_NotifyAppAtCommand(ama_at_cmd_t ama_at_cmd, char* forward_at_command)
{
    DEBUG_LOG("AmaNotifyAppMsg_AtCommand len : %d", strlen(forward_at_command));

    MAKE_AMA_MESSAGE(AMA_SEND_AT_COMMAND_IND);
    message->at_command = ama_at_cmd;

    if( ama_at_cmd == ama_at_cmd_atd_ind)
    {
        /* ATD command indication has format "ATD<ph number>" but the atd command message coming from Alexa
         * has a carriage return in the end so we need to remove this also. */
        message->telephony_number.number_of_digits = strlen(forward_at_command) - strlen("ATD") - CARRIAGE_RETURN;
        message->telephony_number.digits = (uint8*)PanicUnlessMalloc(message->telephony_number.number_of_digits);
        memmove(message->telephony_number.digits, &forward_at_command[3], message->telephony_number.number_of_digits);
    }
    amaProtocol_SendAppMsg(AMA_SEND_AT_COMMAND_IND, message);
}

void AmaProtocol_NotifyAppStartSetup(void)
{
    amaProtocol_SendAppMsg(AMA_START_SETUP_IND, NULL);
}

void AmaProtocol_NotifyAppCompleteSetup(void)
{
    amaProtocol_SendAppMsg(AMA_COMPLETE_SETUP_IND, NULL);
}

void AmaProtocol_NotifyAppEnableClassicPairing(void)
{
    amaProtocol_SendAppMsg(AMA_ENABLE_CLASSIC_PAIRING_IND, NULL);
}

void AmaProtocol_NotifyAppUpgradeTransport(void)
{
    amaProtocol_SendAppMsg(AMA_UPGRADE_TRANSPORT_IND, NULL);
}

void AmaProtocol_NotifyAppMediaControl(void)
{
    amaProtocol_SendAppMsg(AMA_ISSUE_MEDIA_CONTROL_IND, NULL);
}

void AmaProtocol_NotifyAppGetDeviceFeatures(void)
{
    amaProtocol_SendAppMsg(AMA_GET_DEVICE_FEATURES_IND, NULL);
}

void AmaProtocol_NotifyAppGetState(uint32 feature_id)
{
    MAKE_AMA_MESSAGE(AMA_GET_STATE_IND);
    message->feature_id = feature_id;
    amaProtocol_SendAppMsg(AMA_GET_STATE_IND, message);
}

void AmaProtocol_NotifyAppSetState(uint32 feature_id, State__ValueCase value_case, uint32 value)
{
    MAKE_AMA_MESSAGE(AMA_SET_STATE_IND);
    message->feature_id = feature_id;
    message->value_case = value_case;
    message->value = value;
    amaProtocol_SendAppMsg(AMA_SET_STATE_IND, message);
}

void AmaProtocol_NotifyAppSpeechEndpoint(void)
{
    amaProtocol_SendAppMsg(AMA_SPEECH_ENDPOINT_IND, NULL);
}

void AmaProtocol_NotifyAppSynchronizeState(uint32 feature_id, State__ValueCase value_case, uint32 value)
{
    MAKE_AMA_MESSAGE(AMA_SYNCHRONIZE_STATE_IND);
    message->feature_id = feature_id;
    message->value_case = value_case;
    message->value = value;
    amaProtocol_SendAppMsg(AMA_SYNCHRONIZE_STATE_IND, message);
}

void AmaProtocol_NotifyAppKeepAlive(void)
{
    amaProtocol_SendAppMsg(AMA_KEEP_ALIVE_IND, NULL);
}

void AmaProtocol_NotifyAppUnhandledCommand(Command command)
{
    MAKE_AMA_MESSAGE(AMA_UNHANDLED_COMMAND_IND);
    message->command = command;
    amaProtocol_SendAppMsg(AMA_UNHANDLED_COMMAND_IND, message);
}

void AmaProtocol_NotifyAppGetDeviceInformation(uint32 device_id)
{
    MAKE_AMA_MESSAGE(AMA_GET_DEVICE_INFORMATION_IND);
    message->device_id = device_id;
    amaProtocol_SendAppMsg(AMA_GET_DEVICE_INFORMATION_IND, message);
}

void AmaProtocol_NotifyAppGetDeviceConfiguration(void)
{
    amaProtocol_SendAppMsg(AMA_GET_DEVICE_CONFIGURATION_IND, NULL);
}

void AmaProtocol_NotifyAppLaunchApp(char * app_id)
{
    MAKE_AMA_MESSAGE(AMA_LAUNCH_APP_IND);
    message->app_id = (char *)PanicUnlessMalloc(strlen(app_id)+1);
    strncpy(message->app_id, app_id, strlen(app_id));
    message->app_id[strlen(app_id)] = '\0';
    amaProtocol_SendAppMsg(AMA_LAUNCH_APP_IND, message);
}

void AmaProtocol_NotifyAppGetLocales(void)
{
    amaProtocol_SendAppMsg(AMA_GET_LOCALES_IND, NULL);
}

void AmaProtocol_NotifyAppSetLocale(char * locale)
{
    MAKE_AMA_MESSAGE(AMA_SET_LOCALE_IND);
    message->locale = (char *)PanicUnlessMalloc(strlen(locale)+1);
    strncpy(message->locale, locale, strlen(locale));
    message->locale[strlen(locale)] = '\0';
    amaProtocol_SendAppMsg(AMA_SET_LOCALE_IND, message);
}

#endif /* INCLUDE_AMA */
