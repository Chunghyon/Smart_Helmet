/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_send_response.c
\brief  Implementation of the APIs to send AMA responses to the phone
*/

#ifdef INCLUDE_AMA

#include "ama_send_response.h"

#include "ama_send_envelope.h"

#include <logging.h>

#define MAKE_DEFAULT_RESPONSE_ENVELOPE(envelope_name, command_id) \
Response response = RESPONSE__INIT;\
response.error_code = ERROR_CODE__SUCCESS;\
ControlEnvelope  envelope_name = CONTROL_ENVELOPE__INIT;\
amaProtocol_ResponseInit(&envelope_name, command_id , &response);

static void amaProtocol_ResponseInit(ControlEnvelope* envelope_name,Command command_id, Response* response)
{
    envelope_name->command = command_id;
    envelope_name->u.response = response;
    envelope_name->payload_case = CONTROL_ENVELOPE__PAYLOAD_RESPONSE;
}

void AmaProtocol_SendGenericResponse(Command command, ErrorCode error_code)
{
    DEBUG_LOG("AmaProtocol_SendGenericResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, command);
    response.error_code = error_code;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendGetLocalesResponse(Locales * locales)
{
    DEBUG_LOG("AmaProtocol_SendGetLocalesResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, COMMAND__GET_LOCALES);
    response.u.locales = locales;
    response.payload_case = RESPONSE__PAYLOAD_LOCALES;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendGetDeviceInformationResponse(DeviceInformation * device_information)
{
    DEBUG_LOG("AmaProtocol_SendGetDeviceInformationResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, COMMAND__GET_DEVICE_INFORMATION);
    response.u.device_information = device_information;
    response.payload_case = RESPONSE__PAYLOAD_DEVICE_INFORMATION;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendGetDeviceConfigurationResponse(DeviceConfiguration * device_configuration)
{
    DEBUG_LOG("AmaProtocol_SendGetDeviceConfigurationResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, COMMAND__GET_DEVICE_CONFIGURATION);
    response.u.device_configuration = device_configuration;
    response.payload_case = RESPONSE__PAYLOAD_DEVICE_CONFIGURATION;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendGetDeviceFeaturesResponse(DeviceFeatures * device_features)
{
    DEBUG_LOG("AmaProtocol_SendGetDeviceFeaturesResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, COMMAND__GET_DEVICE_FEATURES);
    response.u.device_features = device_features;
    response.payload_case = RESPONSE__PAYLOAD_DEVICE_FEATURES;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendUpgradeTransportResponse(ConnectionDetails * connection_details)
{
    DEBUG_LOG("AmaProtocol_SendUpgradeTransportResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, COMMAND__UPGRADE_TRANSPORT);
    response.u.connection_details = connection_details;
    response.payload_case = RESPONSE__PAYLOAD_CONNECTION_DETAILS;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendGetStateResponse(State * state, ErrorCode error_code)
{
    DEBUG_LOG("AmaProtocol_SendGetStateResponse");
    MAKE_DEFAULT_RESPONSE_ENVELOPE(control_envelope_out, COMMAND__GET_STATE);
    response.error_code = error_code;
    response.u.state = state;
    response.payload_case = RESPONSE__PAYLOAD_STATE;
    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaProtocol_SendProvideSpeechResponse(bool accept, uint32 resp_id, AudioProfile profile, AudioFormat format, AudioSource source)
{
    ControlEnvelope  control_envelope_out = CONTROL_ENVELOPE__INIT;
    Response response = RESPONSE__INIT;
    SpeechProvider speech_provider = SPEECH_PROVIDER__INIT;
    SpeechSettings settings = SPEECH_SETTINGS__INIT;
    Dialog  dialog_response = DIALOG__INIT;

    control_envelope_out.command = COMMAND__PROVIDE_SPEECH;
    control_envelope_out.u.response = &response;
    control_envelope_out.payload_case = CONTROL_ENVELOPE__PAYLOAD_RESPONSE;

    response.error_code = ERROR_CODE__SUCCESS;
    response.payload_case = RESPONSE__PAYLOAD_SPEECH_PROVIDER;
    response.u.speech_provider = &speech_provider;

    speech_provider.speech_settings = &settings;
    speech_provider.dialog = &dialog_response;
    settings.audio_profile = profile;
    settings.audio_format  = format;
    settings.audio_source  = source;

    dialog_response.id = resp_id;

    if(!accept)
    {
        response.error_code = ERROR_CODE__BUSY;
    }

    DEBUG_LOG_VERBOSE("AmaProtocol_SendProvideSpeechResponse enum:ErrorCode:%d enum:AudioProfile:%d enum:AudioFormat:%d enum:AudioSource:%d dialog_id %lu",
                       response.error_code,
                       response.u.speech_provider->speech_settings->audio_profile,
                       response.u.speech_provider->speech_settings->audio_format,
                       response.u.speech_provider->speech_settings->audio_source,
                       response.u.speech_provider->dialog->id);

    AmaSendEnvelope_Send(&control_envelope_out);

}

#endif /* INCLUDE_AMA */
