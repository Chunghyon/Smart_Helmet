/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_message_handler.c
\brief  Implementation of the APIs for handling AMA events
*/

#ifdef INCLUDE_AMA

#include "ama_message_handlers_init.h"

#include "ama.h"
#include "ama_audio.h"
#include "ama_config.h"
#include "ama_log.h"
#include "ama_msg_types.h"
#include "ama_notify_app_msg.h"
#include "ama_send_command.h"
#include "ama_send_response.h"
#include "ama_setup_tracker.h"
#include "ama_speech_config.h"
#include "ama_state.h"
#include "ama_transport.h"
#include "ama_transport_version.h"
#include "ama_tws.h"
#include "ama_voice_ui_handle.h"

#include "voice_ui_va_client_if.h"
#include "voice_sources.h"
#include "bt_device.h"
#include "device_properties.h"


#include <logging.h>
#include <message.h>
#include <stdlib.h>

#ifdef INCLUDE_ACCESSORY
#include "ama_transport_accessory.h"
#include "request_app_launch.h"
#endif

#define AMA_SEND_NOTIFY_DEVICE_CONFIG_DELAY D_SEC(1)


static void ama_MessageHandler(Task task, MessageId id, Message message);
static const TaskData ama_task = { ama_MessageHandler };

static void ama_HandleGetDeviceInformationInd(uint16 device_id)
{
    DeviceInformation device_information = DEVICE_INFORMATION__INIT;
    Ama_PopulateDeviceInformation(&device_information, device_id);
    AmaProtocol_SendGetDeviceInformationResponse(&device_information);
}

static void ama_HandleGetDeviceConfigurationInd(void)
{
    DeviceConfiguration device_config = DEVICE_CONFIGURATION__INIT;
    Ama_PopulateDeviceConfiguration(&device_config);
    AmaProtocol_SendGetDeviceConfigurationResponse(&device_config);
}

static void ama_HandleTransportSwitched(ama_transport_type_t transport)
{
    DEBUG_LOG_FN_ENTRY("ama_HandleTransportSwitched enum:ama_transport_type_t:%d", transport);

    AmaTransport_SetActiveTransport(transport);

    switch(transport)
    {
        case ama_transport_rfcomm:
            DEBUG_LOG("Ama_SwitchedTransport ama_transport_rfcomm with (%u) codec (1->mSBC, 2->OPUS)", (ama_codec_t)AMA_DEFAULT_CODEC_OVER_RFCOMM);
            Ama_ConfigureCodec((ama_codec_t)AMA_DEFAULT_CODEC_OVER_RFCOMM);
            AmaData_SetState(ama_state_idle);
            break;

        case ama_transport_iap:
            DEBUG_LOG("Ama_SwitchedTransport ama_transport_iap with (%u) codec (1->mSBC, 2->OPUS)", (ama_codec_t)AMA_DEFAULT_CODEC_OVER_IAP2);
            Ama_ConfigureCodec((ama_codec_t)AMA_DEFAULT_CODEC_OVER_IAP2);
            AmaData_SetState(ama_state_idle);
            break;

        case ama_transport_none:
        {
            AmaTransport_RxDataReset();
            AmaData_SetState(ama_state_initialized);
            Ama_SetSpeechSettingsToDefault();
            AmaAudio_StopCapture();

            if(Ama_IsWakeUpWordFeatureIncluded())
            {
                AmaAudio_StopWakeWordDetection();
            }
            
            VoiceUi_VaSessionEnded(Ama_GetVoiceUiHandle());
        }
        break;

        default:
            DEBUG_LOG("Ama_SwitchedTransport UNKNOWN transport");
            break;
    }
}

static void ama_HandleSwitchTransportInd(AMA_SWITCH_TRANSPORT_IND_T * ind)
{
    AmaProtocol_SendGenericResponse(COMMAND__SWITCH_TRANSPORT, ERROR_CODE__SUCCESS);
    AmaSendCommand_GetCentralInformation();
    ama_HandleTransportSwitched(ind->transport);
}

static void ama_HandleNotifySpeechStateInd(AMA_NOTIFY_SPEECH_STATE_IND_T * ind)
{
    switch(ind->state)
    {
        case SPEECH_STATE__LISTENING:
        case SPEECH_STATE__PROCESSING:
        case SPEECH_STATE__SPEAKING:
            DEBUG_LOG("ama_HandleNotifySpeechStateInd enum:SpeechState:%d", ind->state);
            VoiceUi_VaSessionStarted(Ama_GetVoiceUiHandle());
            AmaAudio_NotifySessionStarted();
            break;

        case SPEECH_STATE__IDLE:
            DEBUG_LOG("ama_HandleNotifySpeechStateInd enum:SpeechState:%d", ind->state);
            VoiceUi_VaSessionEnded(Ama_GetVoiceUiHandle());
            break;

        default:
            DEBUG_LOG("ama_HandleNotifySpeechStateInd unknown speech state %d", ind->state);
            break;
    }
}

static void ama_HandleSpeechProvideInd(AMA_SPEECH_PROVIDE_IND_T * ind)
{
    bool capture_started = AmaAudio_Provide(ind);

    if(capture_started)
    {
        AmaData_SetState(ama_state_sending);
    }

    AmaProtocol_SendProvideSpeechResponse(capture_started, Ama_GetCurrentSpeechDialogId(),
                                          Ama_GetSpeechAudioProfile(), Ama_GetSpeechAudioFormat(), Ama_GetSpeechAudioSource());
}

static void ama_HandleSpeechStopInd(AMA_SPEECH_STOP_IND_T * ind)
{
    if(ind->dialog_id == Ama_GetCurrentSpeechDialogId())
    {
        AmaAudio_StopCapture();
        AmaData_SetState(ama_state_idle);
    }
    else
    {
        DEBUG_LOG_WARN("ama_HandleSpeechStopInd received dialog id %u does not match current %u", ind->dialog_id, Ama_GetCurrentSpeechDialogId());
    }

    AmaProtocol_SendGenericResponse(COMMAND__STOP_SPEECH, ERROR_CODE__SUCCESS);
}

static void ama_HandleSpeechEndpointInd(void)
{
    AmaProtocol_SendGenericResponse(COMMAND__ENDPOINT_SPEECH, ERROR_CODE__SUCCESS);
}

static void ama_HandleOverrideAssitantInd(void)
{
    VoiceUi_SelectVoiceAssistant(voice_ui_provider_ama, voice_ui_reboot_allowed);

    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        AmaAudio_StartWakeWordDetection();
    }

    AmaProtocol_SendGenericResponse(COMMAND__OVERRIDE_ASSISTANT, ERROR_CODE__SUCCESS);

    MessageSendLater((Task)&ama_task, AMA_NOTIFY_DEVICE_CONFIG_IND, NULL, AMA_SEND_NOTIFY_DEVICE_CONFIG_DELAY);
}

static void ama_HandleNotifyDeviceConfigInd(void)
{
    AmaSendCommand_NotifyDeviceConfig(ASSISTANT_OVERRIDEN);
}

static void ama_HandleSynchronizeSettingsInd(void)
{
    AmaProtocol_SendGenericResponse(COMMAND__SYNCHRONIZE_SETTINGS, ERROR_CODE__SUCCESS);
    AmaSendCommand_GetCentralInformation();

    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        AmaAudio_StartWakeWordDetection();
    }

    if(!Ama_IsSetupComplete() && AmaTransport_IsConnected())
    {
        Ama_CompleteSetup();
    }
}

static void ama_HandleCompleteSetupInd(void)
{
    Ama_CompleteSetup();
    AmaProtocol_SendGenericResponse(COMMAND__COMPLETE_SETUP, ERROR_CODE__SUCCESS);
}

static void ama_HandleGetDeviceFeaturesInd(void)
{
    DeviceFeatures device_features = DEVICE_FEATURES__INIT;
    Ama_PopulateDeviceFeatures(&device_features);
    AmaProtocol_SendGetDeviceFeaturesResponse(&device_features);
}

static void ama_HandleGetStateInd(AMA_GET_STATE_IND_T * ind)
{
    State state = STATE__INIT;
    ErrorCode error_code = AmaState_GetState(ind->feature_id, &state);
    AmaProtocol_SendGetStateResponse(&state, error_code);
}

static void ama_HandleSetStateInd(AMA_SET_STATE_IND_T * ind)
{
    ErrorCode error_code = AmaState_SetState(ind->feature_id, ind->value_case, ind->value);
    AmaProtocol_SendGenericResponse(COMMAND__SET_STATE, error_code);
}

static void ama_BdaddrToArray(uint8 *array, bdaddr *bdaddr_in)
{
    array[1] = (uint8)(bdaddr_in->nap & 0xff);
    array[0] = (uint8)((bdaddr_in->nap>>8) & 0xff);
    array[2] = bdaddr_in->uap;
    array[5] = (uint8)(bdaddr_in->lap) & 0xff;
    array[4] = (uint8)(bdaddr_in->lap>>8) & 0xff ;
    array[3] = (uint8)(bdaddr_in->lap>>16) & 0xff ;
}

static void ama_HandleUpgradeTransportInd(void)
{
    uint8 bdaddr_array[6];
    ConnectionDetails connection_details = CONNECTION_DETAILS__INIT;

    ama_BdaddrToArray(bdaddr_array, Ama_GetLocalAddress());

    connection_details.identifier.len = 6;
    connection_details.identifier.data = bdaddr_array;

    DEBUG_LOG_VERBOSE("ama_HandleUpgradeTransportInd connection details: len %d, data %02x %02x %02x %02x %02x %02x",
        connection_details.identifier.len,
        connection_details.identifier.data[0],
        connection_details.identifier.data[1],
        connection_details.identifier.data[2],
        connection_details.identifier.data[3],
        connection_details.identifier.data[4],
        connection_details.identifier.data[5]);

    AmaProtocol_SendUpgradeTransportResponse(&connection_details);
}

static void ama_HandleLaunchAppInd(AMA_LAUNCH_APP_IND_T * ind)
{
    AmaLog_LogVaArg("ama_HandleLaunchAppInd launch app with ID = %s\n", ind->app_id);

#ifdef INCLUDE_ACCESSORY
    const bdaddr * bd_addr = AmaTransport_GetBtAddress();

    if(bd_addr)
    {
        AccessoryFeature_RequestAppLaunch(*bd_addr, ind->app_id, launch_without_user_alert);
    }
    else
    {
        DEBUG_LOG_ERROR("ama_HandleLaunchAppInd Unable to get handset Bdaddr for launch app");
    }

    AmaProtocol_SendGenericResponse(COMMAND__LAUNCH_APP, ERROR_CODE__SUCCESS);
#else
    DEBUG_LOG("ama_HandleLaunchAppInd launch app not supported");
    AmaProtocol_SendGenericResponse(COMMAND__LAUNCH_APP, ERROR_CODE__UNSUPPORTED);
#endif

    free(ind->app_id);
    ind->app_id = NULL;
}

static void ama_HandleGetLocalesInd(void)
{
    Locales locales = LOCALES__INIT;
    Locale locale[MAX_AMA_LOCALES] = {0};
    Locale * p_locale [MAX_AMA_LOCALES] = {0};
    ama_supported_locales_t supported_locales;
    AmaAudio_GetSupportedLocales(&supported_locales);

    /* Initialise the locale structures before populating */
    if (supported_locales.num_locales == 0)
    {
        const Locale locale_init = LOCALE__INIT;
        locale[0] = locale_init;
        p_locale[0] = &locale[0];
    }
    else
    {
        for(uint8 i=0; i<supported_locales.num_locales; i++)
        {
            const Locale locale_init = LOCALE__INIT;
            locale[i] = locale_init;
            p_locale[i] = &locale[i];
        }
    }

    AmaAudio_PopulateLocales(&locales, locale, p_locale);
    AmaProtocol_SendGetLocalesResponse(&locales);
}

static void ama_HandleSetLocaleInd(AMA_SET_LOCALE_IND_T * ind)
{
    AmaLog_LogVaArg("ama_HandleSetLocaleInd set locale %s\n", ind->locale);

    const char* model = AmaAudio_GetModelFromLocale(ind->locale);

    if(AmaAudio_ValidateLocale(model))
    {
        DEBUG_LOG("ama_HandleSetLocaleInd Locale = %c%c%c%c%c is valid",
                  ind->locale[0], ind->locale[1], ind->locale[2], ind->locale[3], ind->locale[4]);
        AmaAudio_SetLocale(ind->locale);
        AmaProtocol_SendGenericResponse(COMMAND__SET_LOCALE, ERROR_CODE__SUCCESS);
    }
    else
    {
        DEBUG_LOG("ama_HandleSetLocaleInd Locale = %c%c%c%c%c is NOT valid",
                  ind->locale[0], ind->locale[1], ind->locale[2], ind->locale[3], ind->locale[4]);
        AmaProtocol_SendGenericResponse(COMMAND__SET_LOCALE, ERROR_CODE__NOT_FOUND);
    }

    free(ind->locale);
    ind->locale = NULL;
}

static void ama_HandleSynchronizeStateInd(AMA_SYNCHRONIZE_STATE_IND_T * ind)
{
    ErrorCode error_code = AmaState_SynchronizeState(ind->feature_id, ind->value_case, ind->value);
    AmaProtocol_SendGenericResponse(COMMAND__SYNCHRONIZE_STATE, error_code);
}

static void ama_HandleUnhandledMessageInd(AMA_UNHANDLED_COMMAND_IND_T * ind)
{
    AmaProtocol_SendGenericResponse(ind->command, ERROR_CODE__UNSUPPORTED);
}

static device_t ama_GetDeviceForAmaBtAdress(void)
{
    const bdaddr * bd_addr = AmaTransport_GetBtAddress();
    return BtDevice_GetDeviceForBdAddr(bd_addr);
}

static void ama_InitiateCallUsingNumber(phone_number_t telephony_number)
{ 
    device_t device = ama_GetDeviceForAmaBtAdress();

    if(device)
    {
        DEBUG_LOG("ama_InitiateCallUsingNumber");
        VoiceSources_InitiateCallUsingNumber(DeviceProperties_GetVoiceSource(device), telephony_number);
    }
}

static void ama_InitiateCallLastDialled(void)
{
    device_t device = ama_GetDeviceForAmaBtAdress();

    if(device)
    {
        DEBUG_LOG("ama_InitiateCallLastDialled");
        VoiceSources_InitiateCallLastDialled(DeviceProperties_GetVoiceSource(device));
    }
}

static void ama_HandleAtCommand(AMA_SEND_AT_COMMAND_IND_T* message)
{
    DEBUG_LOG("ama_HandleAtCommand enum:ama_at_cmd_t:%d", message->at_command);

    ErrorCode error_code = ERROR_CODE__SUCCESS;

    if(message->at_command == ama_at_cmd_atd_ind)
    {
        ama_InitiateCallUsingNumber(message->telephony_number);
        free(message->telephony_number.digits);
    }
    else if(message->at_command == ama_at_cmd_at_plus_bldn_ind)
    {
        ama_InitiateCallLastDialled();
    }
    else if(message->at_command == ama_at_cmd_unknown)
    {
        error_code = ERROR_CODE__UNKNOWN;
    }
    AmaProtocol_SendGenericResponse(COMMAND__FORWARD_AT_COMMAND, error_code);
}

static void ama_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("ama_MessageHandler enum:ama_message_type_t:%d", id);

    switch(id)
    {
        case AMA_GET_DEVICE_INFORMATION_IND:
            ama_HandleGetDeviceInformationInd(((AMA_GET_DEVICE_INFORMATION_IND_T*)message)->device_id);
            break;

        case AMA_GET_DEVICE_CONFIGURATION_IND:
            ama_HandleGetDeviceConfigurationInd();
            break;

        case AMA_SWITCH_TRANSPORT_IND:
            ama_HandleSwitchTransportInd((AMA_SWITCH_TRANSPORT_IND_T *)message);
            break;

		case AMA_NOTIFY_SPEECH_STATE_IND:
            ama_HandleNotifySpeechStateInd((AMA_NOTIFY_SPEECH_STATE_IND_T *)message);
            break;

        case AMA_SPEECH_PROVIDE_IND:
            ama_HandleSpeechProvideInd((AMA_SPEECH_PROVIDE_IND_T * )message);
            break;

        case AMA_SPEECH_STOP_IND:
            ama_HandleSpeechStopInd((AMA_SPEECH_STOP_IND_T *)message);
            break;

        case AMA_SPEECH_ENDPOINT_IND:
            ama_HandleSpeechEndpointInd();
            break;

        case AMA_OVERRIDE_ASSISTANT_IND:
            ama_HandleOverrideAssitantInd();
            break;

        case AMA_NOTIFY_DEVICE_CONFIG_IND:
            ama_HandleNotifyDeviceConfigInd();
            break;
            
        case AMA_SYNCHRONIZE_SETTING_IND:
            ama_HandleSynchronizeSettingsInd();
            break;

        case AMA_START_SETUP_IND:
            AmaProtocol_SendGenericResponse(COMMAND__START_SETUP, ERROR_CODE__SUCCESS);
            break;

        case AMA_COMPLETE_SETUP_IND:
            ama_HandleCompleteSetupInd();
            break;

        case AMA_LOCAL_DISCONNECT_COMPLETE_IND:
            AmaTws_HandleLocalDisconnectionCompleted();
            break;

        case AMA_ISSUE_MEDIA_CONTROL_IND:
            AmaProtocol_SendGenericResponse(COMMAND__ISSUE_MEDIA_CONTROL, ERROR_CODE__SUCCESS);
            break;

        case AMA_SEND_AT_COMMAND_IND:
            ama_HandleAtCommand((AMA_SEND_AT_COMMAND_IND_T *)message);
            break;

        case AMA_GET_DEVICE_FEATURES_IND:
            ama_HandleGetDeviceFeaturesInd();
            break;

        case AMA_GET_STATE_IND:
            ama_HandleGetStateInd((AMA_GET_STATE_IND_T *)message);
            break;

        case AMA_SET_STATE_IND:
            ama_HandleSetStateInd((AMA_SET_STATE_IND_T *)message);
            break;

        case AMA_SYNCHRONIZE_STATE_IND:
            ama_HandleSynchronizeStateInd((AMA_SYNCHRONIZE_STATE_IND_T *)message);
            break;

        case AMA_KEEP_ALIVE_IND:
            AmaProtocol_SendGenericResponse(COMMAND__KEEP_ALIVE, ERROR_CODE__SUCCESS);
            break;

        case AMA_UPGRADE_TRANSPORT_IND:
            ama_HandleUpgradeTransportInd();
            break;

        case AMA_LAUNCH_APP_IND:
            ama_HandleLaunchAppInd((AMA_LAUNCH_APP_IND_T *)message);
            break;

        case AMA_GET_LOCALES_IND:
            ama_HandleGetLocalesInd();
            break;

        case AMA_SET_LOCALE_IND:
            ama_HandleSetLocaleInd((AMA_SET_LOCALE_IND_T *)message); 
            break;

        case AMA_ENABLE_CLASSIC_PAIRING_IND:
        case AMA_START_ADVERTISING_AMA_IND:
        case AMA_STOP_ADVERTISING_AMA_IND:
            /* AMA_TODO: Yet to handle */
            break;

        case AMA_UNHANDLED_COMMAND_IND:
            ama_HandleUnhandledMessageInd((AMA_UNHANDLED_COMMAND_IND_T *)message);
            break;

        default:
            DEBUG_LOG("ama_MessageHandler: unhandled MESSAGE:0x%04X", id);
            break;
    }
}

Task Ama_GetCoreMessageHandlerTask(void)
{
    return (Task)&ama_task;
}

#endif /* INCLUDE_AMA */
