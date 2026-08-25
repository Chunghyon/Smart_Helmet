/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_send_command.c
\brief  Implementation of the APIs to send AMA commands to the phone
*/

#ifdef INCLUDE_AMA
#include "logging.h"
#include "accessories.pb-c.h"
#include "ama_notify_app_msg.h"
#include "ama_send_envelope.h"
#include "ama_send_command.h"
#include "ama_command_handlers.h"
#include "ama_log.h"
#include "ama_data.h"

void AmaSendCommand_StartSpeech(SpeechInitiator__Type speech_initiator,
                                             AudioProfile audio_profile,
                                             AudioFormat audio_format,
                                             AudioSource audio_source,
                                             uint32 start_sample,
                                             uint32 end_sample,
                                             data_blob_t wuw_metadata,
                                             uint32 current_dialog_id
)
{
    ControlEnvelope  control_envelope_out = CONTROL_ENVELOPE__INIT;
    control_envelope_out.command = COMMAND__START_SPEECH;
    control_envelope_out.payload_case = CONTROL_ENVELOPE__PAYLOAD_START_SPEECH;
    StartSpeech start_speech = START_SPEECH__INIT;
    SpeechSettings settings = SPEECH_SETTINGS__INIT;

    settings.audio_profile = audio_profile;
    settings.audio_format = audio_format;
    settings.audio_source = audio_source;

    start_speech.settings = &settings;

    SpeechInitiator  initiator = SPEECH_INITIATOR__INIT;

    initiator.type = speech_initiator;

    SpeechInitiator__WakeWord wakeWord = SPEECH_INITIATOR__WAKE_WORD__INIT;

    wakeWord.start_index_in_samples = start_sample;
    wakeWord.end_index_in_samples = end_sample;
    wakeWord.metadata.len = wuw_metadata.data_length;
    wakeWord.metadata.data = wuw_metadata.data;

    initiator.wake_word = &wakeWord;

    start_speech.initiator = &initiator;

    Dialog dialog = DIALOG__INIT;
    dialog.id = current_dialog_id;

    start_speech.dialog = &dialog;

    control_envelope_out.u.start_speech = &start_speech;

    DEBUG_LOG("AMA Send COMMAND__START_SPEECH: Dialog Id=%d", start_speech.dialog->id);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: settings: audio_profile enum:AudioProfile:%d",
        start_speech.settings->audio_profile);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: settings: audio_format enum:AudioFormat:%d",
        start_speech.settings->audio_format);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: settings: audio_source enum:AudioSource:%d",
        start_speech.settings->audio_source);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: initiator: type enum:SpeechInitiator__Type:%d",
        start_speech.initiator->type);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: initiator: wake_word: start_index_in_samples:%lu",
        start_speech.initiator->wake_word->start_index_in_samples);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: initiator: wake_word: end_index_in_samples:%lu",
        start_speech.initiator->wake_word->end_index_in_samples);
    DEBUG_LOG_VERBOSE("AMA Send COMMAND__START_SPEECH: initiator: wake_word: metadata_size:%lu",
        start_speech.initiator->wake_word->metadata.len);

    AmaSendEnvelope_Send(&control_envelope_out);
}

void AmaSendCommand_StopSpeech(ErrorCode reason, uint32 current_dialog_id)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;
    controlEnvelopeOut.command = COMMAND__STOP_SPEECH;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_STOP_SPEECH;

    Dialog dialog = DIALOG__INIT;

    dialog.id = current_dialog_id;


    StopSpeech stop_speech = STOP_SPEECH__INIT;

    stop_speech.dialog = &dialog;
    stop_speech.error_code = reason;
    DEBUG_LOG("AMA Send COMMAND__STOP_SPEECH: error_code enum:ErrorCode:%d,  Dialog Id=%ld",
        stop_speech.error_code, stop_speech.dialog->id);
    controlEnvelopeOut.u.stop_speech = &stop_speech;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_EndSpeech(uint32 current_dialog_id)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;
    controlEnvelopeOut.command = COMMAND__ENDPOINT_SPEECH;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_ENDPOINT_SPEECH;

    Dialog dialog = DIALOG__INIT;

    dialog.id = current_dialog_id;

    EndpointSpeech endpoint_speech = ENDPOINT_SPEECH__INIT;

    endpoint_speech.dialog = &dialog;

    DEBUG_LOG("AMA Send COMMAND__ENDPOINT_SPEECH: Dialog Id=%d", endpoint_speech.dialog->id);
    controlEnvelopeOut.u.endpoint_speech = &endpoint_speech;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_IncomingCall(char* caller_number)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;

    DEBUG_LOG("AMA Send COMMAND__INCOMING_CALL");

    controlEnvelopeOut.command = COMMAND__INCOMING_CALL;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_INCOMING_CALL;

    IncomingCall incoming_call = INCOMING_CALL__INIT;

    incoming_call.caller_number = caller_number;  /* AMA_TODO Revisit for future improvement */

    if (debug_log_level__global >= DEBUG_LOG_LEVEL_VERBOSE)
    {
        AmaLog_LogVaArg("AMA Send COMMAND__INCOMING_CALL: %s\n", incoming_call.caller_number);
    }
    controlEnvelopeOut.u.incoming_call = &incoming_call;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}


void AmaSendCommand_KeepAlive(void)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;

    DEBUG_LOG("AMA Send COMMAND__KEEP_ALIVE");

    controlEnvelopeOut.command = COMMAND__KEEP_ALIVE;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_KEEP_ALIVE;

    KeepAlive  keep_alive = KEEP_ALIVE__INIT;

    controlEnvelopeOut.u.keep_alive = &keep_alive;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_NotifyDeviceConfig(bool require_va_override)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;

    DEBUG_LOG("AMA Send COMMAND__NOTIFY_DEVICE_CONFIGURATION");

    controlEnvelopeOut.command = COMMAND__NOTIFY_DEVICE_CONFIGURATION;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_NOTIFY_DEVICE_CONFIGURATION;

    NotifyDeviceConfiguration  notify_device_config = NOTIFY_DEVICE_CONFIGURATION__INIT;

    /* AMA_TODO - memory needs to be malloc'd & freed */
    static DeviceConfiguration device_config = DEVICE_CONFIGURATION__INIT;
    notify_device_config.device_configuration = &device_config;
    notify_device_config.device_configuration->needs_assistant_override = require_va_override;

    controlEnvelopeOut.u.notify_device_configuration = &notify_device_config;

    DEBUG_LOG_VERBOSE("AMA Send COMMAND__NOTIFY_DEVICE_CONFIGURATION: needs_assistant_override %u",
        notify_device_config.device_configuration->needs_assistant_override);

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_NotifyDeviceInformation(DeviceInformation * device_information)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;

    DEBUG_LOG("AMA Send COMMAND__NOTIFY_DEVICE_INFORMATION");

    controlEnvelopeOut.command = COMMAND__NOTIFY_DEVICE_INFORMATION;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_NOTIFY_DEVICE_INFORMATION;

    NotifyDeviceInformation  notify_device_information = NOTIFY_DEVICE_INFORMATION__INIT;

    notify_device_information.device_information = device_information;

    controlEnvelopeOut.u.notify_device_information = &notify_device_information;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_SyncState(uint32 feature, State__ValueCase value_case, uint16 value)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;
    controlEnvelopeOut.command = COMMAND__SYNCHRONIZE_STATE;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_SYNCHRONIZE_STATE;

    State state = STATE__INIT;

    DEBUG_LOG("AMA Send COMMAND__SYNCHRONIZE_STATE: feature = 0x%lx, enum:ama_state_value_case_t:%d, value = %u",
        feature, value_case, value);

    state.feature = feature;
    state.value_case = value_case;

    if(value_case == STATE__VALUE_BOOLEAN)
    {
        state.u.boolean = (protobuf_c_boolean)value;
    }
    else  if(value_case == STATE__VALUE_INTEGER)
    {
        state.u.integer = (uint32_t)value;
    }
    else
    {
        return;
    }

    SynchronizeState synchronize_state = SYNCHRONIZE_STATE__INIT;
    synchronize_state.state = &state;

    controlEnvelopeOut.u.synchronize_state = &synchronize_state;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}


void AmaSendCommand_GetState(uint32 feature)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;
    controlEnvelopeOut.command = COMMAND__GET_STATE;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_GET_STATE;

    GetState get_state = GET_STATE__INIT;

    DEBUG_LOG("AMA Send COMMAND__GET_STATE");

    get_state.feature = feature;

    controlEnvelopeOut.u.get_state = &get_state;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_ResetConnection(uint32 timeout, bool force_disconnect)
{
    ControlEnvelope  controlEnvelopeOut = CONTROL_ENVELOPE__INIT;
    controlEnvelopeOut.command = COMMAND__RESET_CONNECTION;
    controlEnvelopeOut.payload_case =  CONTROL_ENVELOPE__PAYLOAD_RESET_CONNECTION;

    ResetConnection reset_connection = RESET_CONNECTION__INIT;

    reset_connection.force_disconnect = force_disconnect;
    reset_connection.timeout = timeout;

    DEBUG_LOG("AMA Send COMMAND__RESET_CONNECTION: timeout=%lu, force_disconnect=%u",
        reset_connection.timeout, reset_connection.force_disconnect);

    controlEnvelopeOut.u.reset_connection = &reset_connection;

    AmaSendEnvelope_Send(&controlEnvelopeOut);
}

void AmaSendCommand_GetCentralInformation(void)
{
    ControlEnvelope  control_envelope_out = CONTROL_ENVELOPE__INIT;
    control_envelope_out.command = COMMAND__GET_CENTRAL_INFORMATION;
    control_envelope_out.payload_case =  CONTROL_ENVELOPE__PAYLOAD_GET_CENTRAL_INFORMATION;

    GetCentralInformation  get_central_information = GET_CENTRAL_INFORMATION__INIT;

    DEBUG_LOG("AMA Send COMMAND__GET_CENTRAL_INFORMATION");

    control_envelope_out.u.get_central_information = &get_central_information;

    AmaSendEnvelope_Send(&control_envelope_out);
}

#endif /* INCLUDE_AMA */
