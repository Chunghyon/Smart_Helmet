/*!
\copyright  Copyright (c) 2018-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_command_handlers.c
\brief  Implementation of the APIs to handle AMA commands from the phone
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_setup_tracker.h"
#include "ama_voice_ui_handle.h"
#include "ama_audio.h"
#include "ama_state.h"
#include "ama_command_handlers.h"
#include "common.pb-c.h"
#include "speech.pb-c.h"
#include "logging.h"
#include "ama_notify_app_msg.h"
#include "ama_battery.h"
#include "ama_log.h"
#include "ama_transport.h"
#include <string.h>

#if defined (INCLUDE_ACCESSORY)
#include"bt_device.h"
#include "request_app_launch.h"
#endif

void AmaCommandHandlers_NotifySpeechState(ControlEnvelope *control_envelope_in)
{
    NotifySpeechState *notify_speech_state = control_envelope_in->u.notify_speech_state;
    DEBUG_LOG("AMA COMMAND__NOTIFY_SPEECH_STATE received. state enum:SpeechState:%d", notify_speech_state->state);
    AmaProtocol_NotifyAppSpeechState(notify_speech_state->state);
}


void AmaCommandHandlers_StopSpeech(ControlEnvelope *control_envelope_in)
{
    Dialog *dialog = control_envelope_in->u.stop_speech->dialog;
    DEBUG_LOG("AMA COMMAND__STOP_SPEECH received. Error code enum:ErrorCode:%d, id %ld",
                    control_envelope_in->u.stop_speech->error_code, dialog->id);
    AmaProtocol_NotifyAppStopSpeech(dialog->id);
}

void AmaCommandHandlers_GetLocales(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__GET_LOCALES received");
    AmaProtocol_NotifyAppGetLocales();
}

void AmaCommandHandlers_SetLocale(ControlEnvelope *control_envelope_in)
{
    char * locale = control_envelope_in->u.set_locale->locale->name;
    DEBUG_LOG("AMA COMMAND__SET_LOCALE received");
    AmaLog_LogVaArg("\t%s\n", locale);
    AmaProtocol_NotifyAppSetLocale(locale);
}

void AmaCommandHandlers_LaunchApp(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    char* app_id = control_envelope_in->u.launch_app->app_id;
    DEBUG_LOG("AMA COMMAND__LAUNCH_APP received");

    if (app_id && strlen(app_id))
    {
        AmaLog_LogVaArg("\t%s\n", app_id);
        AmaProtocol_NotifyAppLaunchApp(app_id);
    }
}

void AmaCommandHandlers_GetDeviceInformation(ControlEnvelope *control_envelope_in)
{
    uint32 device_id = control_envelope_in->u.get_device_information->device_id;
    DEBUG_LOG("AMA COMMAND__GET_DEVICE_INFORMATION for device %d received", device_id);
    AmaProtocol_NotifyAppGetDeviceInformation(device_id);
}


void AmaCommandHandlers_GetDeviceConfiguration(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__GET_DEVICE_CONFIGURATION received");
    AmaProtocol_NotifyAppGetDeviceConfiguration();
}

void AmaCommandHandlers_GetDeviceFeatures(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__GET_DEVICE_FEATURES received");
    AmaProtocol_NotifyAppGetDeviceFeatures();
}

void AmaCommandHandlers_StartSetup(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__START_SETUP received");
    AmaProtocol_NotifyAppStartSetup();
}

void AmaCommandHandlers_CompleteSetup(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__COMPLETE_SETUP received");
    AmaProtocol_NotifyAppCompleteSetup();
}

void AmaCommandHandlers_UpgradeTransport(ControlEnvelope *control_envelope_in)
{
    UpgradeTransport * upgrade_transport = control_envelope_in->u.upgrade_transport;
    DEBUG_LOG("AMA COMMAND__UPGRADE_TRANSPORT received. Transport enum:Transport:%d", upgrade_transport->transport);
    AmaProtocol_NotifyAppEnableClassicPairing();
    AmaProtocol_NotifyAppUpgradeTransport();
}

void AmaCommandHandlers_SwitchTransport(ControlEnvelope *control_envelope_in)
{
    SwitchTransport * switch_transport = control_envelope_in->u.switch_transport;
    DEBUG_LOG("AMA COMMAND__SWITCH_TRANSPORT received. Transport enum:Transport:%d", switch_transport->new_transport);
    AmaProtocol_NotifyAppTransportSwitch((ama_transport_type_t)switch_transport->new_transport);
}

void AmaCommandHandlers_SynchronizeSettings(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__SYNCHRONIZE_SETTINGS received");
    AmaProtocol_NotifyAppSynchronizeSetting();
}

void AmaCommandHandlers_GetState(ControlEnvelope *control_envelope_in)
{
    GetState *get_state = control_envelope_in->u.get_state;
    uint32 feature = (uint32)get_state->feature;
    AmaProtocol_NotifyAppGetState(feature);
}


void AmaCommandHandlers_SetState(ControlEnvelope *control_envelope_in)
{
    SetState *set_state = control_envelope_in->u.set_state;
    uint32 feature = (uint32)set_state->state->feature;
    State__ValueCase value_case = set_state->state->value_case;
    uint32 state_value = 0xFFFF;

    if(value_case == STATE__VALUE_BOOLEAN)
    {
        state_value = (uint32)set_state->state->u.boolean;
    }
    else if (value_case == STATE__VALUE_INTEGER)
    {
        state_value = (uint32)set_state->state->u.integer;
    }

    DEBUG_LOG("AMA COMMAND__SET_STATE feature %x, value case enum:State__ValueCase:%d, value %d", feature, value_case, state_value);
    AmaProtocol_NotifyAppSetState(feature, value_case, state_value);
}

void AmaCommandHandlers_MediaControl(ControlEnvelope *control_envelope_in)
{

    IssueMediaControl *issue_media_control = control_envelope_in->u.issue_media_control;
    MediaControl control =  issue_media_control->control;
    DEBUG_LOG("AMA COMMAND__ISSUE_MEDIA_CONTROL received. control enum:MediaControl:%d", control);
    AmaProtocol_NotifyAppMediaControl();
}

void AmaCommandHandlers_OverrideAssistant(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__OVERRIDE_ASSISTANT received");
    AmaProtocol_NotifyAppOverrideAssistant();
}

void AmaCommandHandlers_SynchronizeState(ControlEnvelope *control_envelope_in)
{
    State *state = control_envelope_in->u.synchronize_state->state;
    uint32 feature = (uint32)state->feature;
    State__ValueCase value_case = state->value_case;
    uint32 value = value_case == STATE__VALUE_BOOLEAN ? state->u.boolean != 0 : state->u.integer;

    DEBUG_LOG("AmaCommandHandlers_SynchronizeState: feature=0x%04X value=%u", feature, value);
    AmaProtocol_NotifyAppSynchronizeState(feature, value_case, value);
}

void AmaCommandHandlers_ProvideSpeech(ControlEnvelope *control_envelope_in)
{
    ProvideSpeech *provide_speech = control_envelope_in->u.provide_speech;
    Dialog* dialog = provide_speech->dialog;

    DEBUG_LOG("AMA COMMAND__PROVIDE_SPEECH - dialog id =%d", dialog->id);

    AmaProtocol_NotifyAppProvideSpeech(dialog->id);
}

void AmaCommandHandlers_EndpointSpeech(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA COMMAND__ENDPOINT_SPEECH received");

    if(control_envelope_in->payload_case == CONTROL_ENVELOPE__PAYLOAD_ENDPOINT_SPEECH)
    {
        EndpointSpeech * endpoint_speech = control_envelope_in->u.endpoint_speech;
        Dialog *dialog = endpoint_speech->dialog;

        DEBUG_LOG("AMA COMMAND__ENDPOINT_SPEECH: ENDPOINT_SPEECH: Dialog ID %d", dialog->id);
        AmaProtocol_NotifyAppStopSpeech(dialog->id);
    }
    else if(control_envelope_in->payload_case == CONTROL_ENVELOPE__PAYLOAD_NOTIFY_SPEECH_STATE)
    {
        /* probably we get this case if send end speech when there is no speech going on */
        NotifySpeechState * notify_speech_state = control_envelope_in->u.notify_speech_state;
        SpeechState state = notify_speech_state->state;
        DEBUG_LOG("AMA COMMAND__ENDPOINT_SPEECH: NOTIFY_SPEECH_STATE: state %d", state);
        AmaProtocol_NotifyAppSpeechState(state);
    }
    else
    {
        DEBUG_LOG_ERROR("AMA COMMAND__ENDPOINT_SPEECH: Unexpected payload case enum:ControlEnvelope__PayloadCase:%d",
            control_envelope_in->payload_case);
    }

    AmaProtocol_NotifyAppSpeechEndpoint();
}

static ama_at_cmd_t amaCommandHandlers_SearchATCommandInSupportedAtCommands(char* forward_at_command)
{
    typedef struct{
        char* at_string;
        ama_at_cmd_t ama_at_command;
    }at_lookup_t;

    static const at_lookup_t  at_lookup[] = {
        {"ATA",           ama_at_cmd_ata_ind},
        {"AT+CHUP",       ama_at_cmd_at_plus_chup_ind},
        {"AT+BLDN",       ama_at_cmd_at_plus_bldn_ind},
        {"AT+CHLD=0",     ama_at_cmd_at_plus_chld_eq_0_ind},
        {"AT+CHLD=1",     ama_at_cmd_at_plus_chld_eq_1_ind},
        {"AT+CHLD=2",     ama_at_cmd_at_plus_chld_eq_2_ind},
        {"AT+CHLD=3",     ama_at_cmd_at_plus_chld_eq_3_ind},
        {"ATD",           ama_at_cmd_atd_ind}
    };

    /* First Check if it is ATD cmd as its format will be ATD<PhNumber>*/
    if(strncmp(forward_at_command,"ATD",strlen("ATD")) == 0)
    {
        return ama_at_cmd_atd_ind;
    }

    uint8 num_of_commands = sizeof(at_lookup) / sizeof(at_lookup[0]);
    uint8 index;

    for(index = 0; index < num_of_commands; index++)
    {
        if(strcmp(at_lookup[index].at_string, forward_at_command) == 0)
        {
            return at_lookup[index].ama_at_command;
        }
    }

    return ama_at_cmd_unknown;
}

static ErrorCode amaCommandHandlers_ProcessForwardAtCommand(char* forward_at_command)
{
    ErrorCode status = ERROR_CODE__UNKNOWN;
    PanicNull(forward_at_command);

    ama_at_cmd_t ama_at_cmd = amaCommandHandlers_SearchATCommandInSupportedAtCommands(forward_at_command);
    if(ama_at_cmd !=  ama_at_cmd_unknown)
    {
        AmaProtocol_NotifyAppAtCommand(ama_at_cmd,forward_at_command);
        status =  ERROR_CODE__SUCCESS;
    }

    return status;
}


void AmaCommandHandlers_ForwardATCommand(ControlEnvelope *control_envelope_in)
{
    ForwardATCommand* forward_at_command = control_envelope_in->u.forward_at_command;
    char* forward_command = forward_at_command->command;
    AmaLog_LogVaArg("AMA COMMAND__FORWARD_AT_COMMAND received. Command %s\n", forward_command);
    amaCommandHandlers_ProcessForwardAtCommand(forward_command);
}

void AmaCommandHandlers_NotHandled(ControlEnvelope *control_envelope_in)
{
    DEBUG_LOG("AMA unhandled command %d", control_envelope_in->command);
    AmaProtocol_NotifyAppUnhandledCommand(control_envelope_in->command);
}

void AmaCommandHandlers_KeepAlive(ControlEnvelope *control_envelope_in)
{
    UNUSED(control_envelope_in);
    DEBUG_LOG("AMA COMMAND__KEEP_ALIVE received");
    AmaProtocol_NotifyAppKeepAlive();
}

#endif /* INCLUDE_AMA */
