/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_audio.c
\brief  Implementation of audio functionality for Amazon Voice Service
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_config.h"
#include "ama_setup_tracker.h"
#include "ama_audio.h"
#include "ama_speech_config.h"
#include "ama_voice_ui_handle.h"
#include "ama_transport.h"
#include "ama_transport_version.h"
#include <voice_ui_container.h>
#include <voice_ui_va_client_if.h>
#include <voice_ui_peer_sig.h>
#include "ama_ble.h"
#include "ama_log.h"
#include <kymera.h>
#include <source.h>
#include <stdlib.h>
#include <system_clock.h>
#include <voice_ui.h>
#include <voice_ui_audio.h>
#include <ui_indicator_prompts.h>
#include "ama_send_command.h"

#ifdef INCLUDE_ACCESSORY
#include "bt_device.h"
#include "request_app_launch.h"
#define amaAudio_HaveAppLaunch() (TRUE)
#define AMA_PROTOCOL_NAME "com.amazon.echo"
#else
#define amaAudio_HaveAppLaunch() (FALSE)
#endif

#define PRE_ROLL_US 500000UL
#define AmaAudio_GetOpusFrameSize(config)  ((config->u.opus_format == AUDIO_FORMAT__OPUS_16KHZ_16KBPS_CBR_0_20MS) ? 40 : 80);
#define AMA_LOCALE_FILENAME_STR_SIZE    32

#define AMA_BUTTON_ACTIVATE_RETRY_MILLIS (3000)
#define AMA_AUDIO_START_SESSION_TIMEOUT_MILLIS (2000)

typedef enum
{
    AMA_BUTTON_ACTIVATE_RETRY_START = 1,
    AMA_BUTTON_ACTIVATE_RETRY_TIMEOUT,
    AMA_AUDIO_SESSION_START_TIMEOUT,
} ama_audio_events_t;

typedef enum
{
    ama_audio_prompt_unregistered,
    ama_audio_prompt_not_connected
} ama_audio_prompt_t;

typedef struct _ama_current_locale
{
    char name[AMA_LOCALE_STR_SIZE];
    FILE_INDEX file_index;
}ama_current_locale_t;

typedef struct{
    char* locale;   /* Name of locale */
    char* model;    /* Name of model that supports locale */
}locale_to_model_t;

typedef struct{
    SpeechInitiator__Type initiator;
    uint32 pre_roll;
    /* start timestamp for trigger phrase of buffered data */
    uint32 start_timestamp;
    /* end timestamp for trigger phrase of buffered data */
    uint32 end_timestamp;
    /* Read-Only blob of Wake-Up-Word metadata */
    data_blob_t metadata;
}ama_start_speech_t;

typedef struct
{
    SpeechInitiator__Type type;
} AMA_BUTTON_ACTIVATE_RETRY_START_T;

/* Function pointer to send the encoded voice data */
static bool (*amaAudio_SendVoiceData)(Source src);

static ama_current_locale_t current_locale = {.file_index = FILE_NONE, .name = AMA_DEFAULT_LOCALE};

static const char *locale_ids[] = {AMA_AVAILABLE_LOCALES};
static const locale_to_model_t locale_to_model[] =
{
    AMA_LOCALE_TO_MODEL_OVERRIDES
};

static char locale_filename[AMA_LOCALE_FILENAME_STR_SIZE];
static const ui_event_indicator_table_t locale_prompt_data[] =
{
    {.sys_event=VOICE_UI_AMA_UNREGISTERED,
        { .prompt.filename = locale_filename,
          .prompt.rate = 48000,
          .prompt.format = PROMPT_FORMAT,
          .prompt.interruptible = TRUE,
          .prompt.queueable = FALSE,
          .prompt.requires_repeat_delay = FALSE }
    },
    {.sys_event=VOICE_UI_AMA_NOT_CONNECTED,
        { .prompt.filename = locale_filename,
          .prompt.rate = 48000,
          .prompt.format = PROMPT_FORMAT,
          .prompt.interruptible = FALSE,
          .prompt.queueable = TRUE,
          .prompt.requires_repeat_delay = TRUE }
    }
};

static bool ama_button_activate_retry = amaAudio_HaveAppLaunch();
static uint16 ama_audio_lock;

static void amaAudio_MessageHandler(Task task, MessageId id, Message message);
static void amaAudio_PlayPrompt(ama_audio_prompt_t prompt);

static const TaskData ama_audio_task = { .handler = amaAudio_MessageHandler };

static void amaAudio_MessageHandler(Task task, MessageId id, Message message)
{
    DEBUG_LOG("amaAudio_MessageHandler: enum:ama_audio_events_t:%d", id);

    switch (id)
    {
    case AMA_BUTTON_ACTIVATE_RETRY_START:
        MessageCancelAll(task, AMA_BUTTON_ACTIVATE_RETRY_TIMEOUT);
        AmaAudio_StartButtonActivatedCapture(((AMA_BUTTON_ACTIVATE_RETRY_START_T *) message)->type);
        break;

    case AMA_BUTTON_ACTIVATE_RETRY_TIMEOUT:
        MessageCancelAll(task, AMA_BUTTON_ACTIVATE_RETRY_START);
        amaAudio_PlayPrompt(ama_audio_prompt_not_connected);
        break;

    case AMA_AUDIO_SESSION_START_TIMEOUT:
        VoiceUi_VaSessionEnded(Ama_GetVoiceUiHandle());
        break;

    default:
        DEBUG_LOG_ERROR("amaAudio_MessageHandler: unhandled 0x%04X", id);
        break;
    }
}

static void amaAudio_StartSession(void)
{
    VoiceUi_VaSessionStarted(Ama_GetVoiceUiHandle());
    MessageCancelAll((Task) &ama_audio_task, AMA_AUDIO_SESSION_START_TIMEOUT);
    MessageSendLater((Task) &ama_audio_task, AMA_AUDIO_SESSION_START_TIMEOUT, NULL, AMA_AUDIO_START_SESSION_TIMEOUT_MILLIS);
}

static bool amaAudio_RequestAlexaLaunch(void)
{
    bool launched = FALSE;
#ifdef INCLUDE_ACCESSORY
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        launched = AccessoryFeature_RequestAppLaunch(bd_addr, AMA_PROTOCOL_NAME, launch_without_user_alert);
    }
    else
    {
        DEBUG_LOG_ERROR("amaAudio_RequestAlexaLaunch: no handset");
    }
#endif
    return launched;
}

static bool amaAudio_RetryStart(SpeechInitiator__Type type)
{
    bool started = FALSE;
    MESSAGE_MAKE(msg, AMA_BUTTON_ACTIVATE_RETRY_START_T);
    msg->type = type;

    started = amaAudio_RequestAlexaLaunch();

    if (started)
    {
        ama_audio_lock = 1;
        MessageSendConditionally((Task) &ama_audio_task, AMA_BUTTON_ACTIVATE_RETRY_START, msg, &ama_audio_lock);
        MessageSendLater((Task) &ama_audio_task, AMA_BUTTON_ACTIVATE_RETRY_TIMEOUT, NULL, AMA_BUTTON_ACTIVATE_RETRY_MILLIS);
    }

    return started;
}

static const char *amaAudio_GetPromptFileSuffix(ama_audio_prompt_t prompt)
{
    const char *result = NULL;
    switch (prompt)
    {
    case ama_audio_prompt_unregistered:
        result = PROMPT_FILENAME("_ama_unregistered");
        break;

    case ama_audio_prompt_not_connected:
        result = PROMPT_FILENAME("_ama_not_connected");
        break;
    }
    return result;
}

static void amaAudio_CreatePromptFilename(const char *locale, const char *suffix,
                                          char *filename, size_t size)
{
    size_t needed = strlen(locale) + strlen(suffix) + 1;
    PanicFalse(size >= needed);
    strcpy(filename, locale);
    strcat(filename, suffix);
}

static FILE_INDEX amaAudio_CheckFileExists(const char *locale, const char *suffix,
                                           char *filebuf, size_t bufsize)
{
    char filename[AMA_LOCALE_FILENAME_STR_SIZE];
    amaAudio_CreatePromptFilename(locale, suffix, filename, sizeof(filename));
    FILE_INDEX file_index = FileFind(FILE_ROOT, filename, strlen(filename));
    if ( file_index != FILE_NONE && filebuf != NULL)
    {
        PanicFalse(bufsize > strlen(filename));
        strcpy(filebuf,filename);
    }
    return file_index;
}

static FILE_INDEX amaAudio_GetLocalePromptFilenameAndIndex(const char *locale, ama_audio_prompt_t prompt,
                                                           char *filebuf, size_t bufsize)
{
    FILE_INDEX file_index = FILE_NONE;
    const char *prompt_name = amaAudio_GetPromptFileSuffix(prompt);

    if( prompt_name == NULL)
        return FILE_NONE;

    file_index = amaAudio_CheckFileExists(locale, prompt_name, filebuf, bufsize);

    if (file_index == FILE_NONE)
    {
        const char *model = AmaAudio_GetModelFromLocale(locale);
        if (strcmp(model, locale) != 0)
        {
            /* The model for the locale is different to the locale */
            file_index = amaAudio_CheckFileExists(model, prompt_name, filebuf, bufsize);
        }
    }
    return file_index;
}

static FILE_INDEX amaAudio_FindLocalePromptFileIndex(const char *locale, ama_audio_prompt_t prompt)
{
    return amaAudio_GetLocalePromptFilenameAndIndex(locale, prompt, NULL, 0);
}

static FILE_INDEX amaAudio_ResolveLocaleFilename(ama_audio_prompt_t prompt, char *filebuf, size_t bufsize)
{
    FILE_INDEX file_index = FILE_NONE;
    char locale[AMA_LOCALE_STR_SIZE];
    if (AmaAudio_GetDeviceLocale(locale, AMA_LOCALE_STR_SIZE))
    {
        file_index = amaAudio_GetLocalePromptFilenameAndIndex(locale, prompt, filebuf, bufsize);
    }

    if (file_index == FILE_NONE)
    {
        DEBUG_LOG_WARN("amaAudio_ResolveLocaleFilename: localised file not found, trying default locale");
        file_index = amaAudio_GetLocalePromptFilenameAndIndex(AMA_DEFAULT_LOCALE, prompt, filebuf, bufsize);
    }
    return file_index;
}

static const ui_event_indicator_table_t *amaAudio_GetPromptData(MessageId id)
{
    FILE_INDEX file_index;
    const ui_event_indicator_table_t *result = NULL;
    switch (id)
    {
        case VOICE_UI_AMA_UNREGISTERED:
            file_index = amaAudio_ResolveLocaleFilename(ama_audio_prompt_unregistered,
                                                        locale_filename, sizeof(locale_filename));
            PanicFalse(file_index != FILE_NONE);
            PanicFalse(locale_prompt_data[0].sys_event == id);
            result = &locale_prompt_data[0];
        break;
        case VOICE_UI_AMA_NOT_CONNECTED:
            file_index = amaAudio_ResolveLocaleFilename(ama_audio_prompt_not_connected,
                                                        locale_filename, sizeof(locale_filename));
            PanicFalse(file_index != FILE_NONE);
            PanicFalse(locale_prompt_data[1].sys_event == id);
            result = &locale_prompt_data[1];
        break;
    }
    PanicFalse(result != NULL);
    return result;
}

void AmaAudio_RegisterLocalePrompts(void)
{
    DEBUG_LOG("AmaAudio_RegisterLocalePrompts for each handled event");
    for (size_t i = 0 ; i < ARRAY_DIM(locale_prompt_data); i++)
    {
        UiPrompts_SetUserPromptDataFunction(amaAudio_GetPromptData, locale_prompt_data[i].sys_event);
    }
}

void AmaAudio_DeregisterLocalePrompts(void)
{
    DEBUG_LOG("AmaAudio_DeregisterLocalePrompts for each handled event");
    for (size_t i = 0 ; i < ARRAY_DIM(locale_prompt_data); i++)
    {
        UiPrompts_ClearUserPromptDataFunction(locale_prompt_data[i].sys_event);
    }
}

bool AmaAudio_ValidateLocale(const char *locale)
{
    FILE_INDEX file_index = amaAudio_FindLocalePromptFileIndex(locale, ama_audio_prompt_unregistered);

    if (file_index == FILE_NONE)
    {
        /* There is no "unregistered" prompt */
#ifndef AMA_LOCALES_NEED_UNREGISTERED_PROMPT
        /* The default locale must have an "unregistered" prompt */
        if (strcmp(locale, AMA_DEFAULT_LOCALE) == 0)
#else
        /* All locales must have an "unregistered" prompt */
#endif
        {
            return FALSE;
        }
    }

    file_index = amaAudio_FindLocalePromptFileIndex(locale, ama_audio_prompt_not_connected);

    if (file_index != FILE_NONE)
    {
        /* The locale has a "not connected" prompt */
        if(Ama_IsWakeUpWordFeatureIncluded())
        {
            /* For wake-up-word, the local must have a locale file */
            file_index = FileFind(FILE_ROOT, locale, strlen(locale));
        }
    }

    return (file_index != FILE_NONE);
}

static void ama_GetLocalesInFileSystem(ama_supported_locales_t* supported_locales)
{
    for(uint8 i = 0; i < ARRAY_DIM(locale_ids); i++)
    {
        const char *model = AmaAudio_GetModelFromLocale(locale_ids[i]);
        if (AmaAudio_ValidateLocale(model))
        {
            supported_locales->name[supported_locales->num_locales] = locale_ids[i];
            supported_locales->num_locales++;
        }
    }
}

#define ValidateLocaleSize()   PanicFalse(MAX_AMA_LOCALES >= ARRAY_DIM(locale_ids))

static void amaAudio_SetDeviceLocale(const char *locale);

inline static void amaAudio_SetCurrentLocaleFileIndex(void)
{
    const char* model = AmaAudio_GetModelFromLocale(current_locale.name);
    current_locale.file_index = FileFind(FILE_ROOT, model, strlen(model));
    PanicFalse(current_locale.file_index != FILE_NONE);
}

static bool amaAudio_SendMsbcVoiceData(Source source)
{    
    #define MSBC_ENC_PKT_LEN 60
    #define MSBC_FRAME_LEN 57
    #define MSBC_FRAME_COUNT 5

    uint8 frames_to_send;
    uint16 payload_posn = 0;
    uint16 lengthSourceThreshold;
    uint8 *payload = NULL;
    uint8 no_of_transport_pkt = 0;

    bool sent_if_necessary = FALSE;

    frames_to_send = MSBC_FRAME_COUNT;        

    lengthSourceThreshold = MSBC_ENC_PKT_LEN * frames_to_send;

    while ((SourceSize(source) >= (lengthSourceThreshold + 2)) && no_of_transport_pkt < 3)
    {
        const uint8 *source_ptr = SourceMap(source);
        uint32 frame;

        if(!payload)
            payload = AmaTransport_AllocatePacketData(MSBC_FRAME_LEN * frames_to_send);
        
        payload_posn = 0;

        for (frame = 0; frame < frames_to_send; frame++)
        {
            memmove(&payload[payload_posn], &source_ptr[(frame * MSBC_ENC_PKT_LEN) + 2], MSBC_FRAME_LEN);
            payload_posn += MSBC_FRAME_LEN;
        }
        sent_if_necessary = AmaTransport_TransmitData(ama_stream_voice, payload, MSBC_FRAME_LEN * frames_to_send);

        if(sent_if_necessary)
        {
            SourceDrop(source, lengthSourceThreshold);
        }
        else
        {
            break;
        }

        no_of_transport_pkt++;
    }

    AmaTransport_FreePacketData(payload, MSBC_FRAME_LEN * frames_to_send);

    DEBUG_LOG_V_VERBOSE("amaAudio_SendMsbcVoiceData: %d bytes remaining", SourceSize(source));

    return sent_if_necessary;
}

static bool amaAudio_SendOpusVoiceData(Source source)
{
    /* Parameters used by Opus codec*/
    #define AMA_OPUS_HEADER_LEN         3
    #define OPUS_16KBPS_ENC_PKT_LEN     40
    #define OPUS_32KBPS_ENC_PKT_LEN     80
    #define OPUS_16KBPS_LE_FRAME_COUNT      4
    #define OPUS_16KBPS_RFCOMM_FRAME_COUNT  5
    #define OPUS_32KBPS_RFCOMM_FRAME_COUNT  3
    #define OPUS_32KBPS_LE_FRAME_COUNT      2

    uint16 payload_posn = 0;
    uint16 lengthSourceThreshold;
    uint8 *payload = NULL;
    bool sent_if_necessary = FALSE;
    uint8 no_of_transport_pkt = 0;
    ama_transport_type_t transport;
    uint16 opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN; /* Make complier happy. */
    uint16 opus_frame_count = OPUS_16KBPS_RFCOMM_FRAME_COUNT;

    transport = AmaTransport_GetActiveTransport();

    switch(Ama_GetSpeechAudioFormat())
    {
        case AUDIO_FORMAT__OPUS_16KHZ_16KBPS_CBR_0_20MS :

            if(transport == ama_transport_rfcomm)
            {
                opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_16KBPS_RFCOMM_FRAME_COUNT;
            }
            else
            {
                opus_enc_pkt_len = OPUS_16KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_16KBPS_LE_FRAME_COUNT;
            }
            break;

        case AUDIO_FORMAT__OPUS_16KHZ_32KBPS_CBR_0_20MS :

            if(transport == ama_transport_rfcomm)
            {
                opus_enc_pkt_len = OPUS_32KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_32KBPS_RFCOMM_FRAME_COUNT;
            }
            else
            {
                opus_enc_pkt_len = OPUS_32KBPS_ENC_PKT_LEN;
                opus_frame_count = OPUS_32KBPS_LE_FRAME_COUNT;
            }
            break;

        case AUDIO_FORMAT__PCM_L16_16KHZ_MONO :
        case AUDIO_FORMAT__MSBC:
        default:
            DEBUG_LOG_ERROR("amaAudio_SendOpusVoiceData: Unexpected audio format");
            Panic();
            break;
    }

    lengthSourceThreshold = (opus_frame_count * opus_enc_pkt_len);

    while (SourceSize(source) && (SourceSize(source) >= lengthSourceThreshold) && (no_of_transport_pkt < 3))
    {
        const uint8 *opus_ptr = SourceMap(source);
        uint16 frame;

        if(!payload)
            payload = AmaTransport_AllocatePacketData(lengthSourceThreshold);
        
        payload_posn = 0;

        for (frame = 0; frame < opus_frame_count; frame++)
        {
            memmove(&payload[payload_posn], &opus_ptr[(frame*opus_enc_pkt_len)], opus_enc_pkt_len);
            payload_posn += opus_enc_pkt_len;
        }

        sent_if_necessary = AmaTransport_TransmitData(ama_stream_voice, payload, lengthSourceThreshold);

        if(sent_if_necessary)
        {
            SourceDrop(source, lengthSourceThreshold);
        }
        else
        {
            break;
        }

        no_of_transport_pkt++;
    }

    AmaTransport_FreePacketData(payload, lengthSourceThreshold);

    DEBUG_LOG_V_VERBOSE("amaAudio_SendOpusVoiceData: %d bytes remaining", SourceSize(source));

    return sent_if_necessary;
}

static va_audio_codec_t amaAudio_ConvertCodecType(ama_codec_t codec_type)
{
    switch(codec_type)
    {
        case ama_codec_sbc:
            return va_audio_codec_sbc;
        case ama_codec_msbc:
            return va_audio_codec_msbc;
        case ama_codec_opus:
            return va_audio_codec_opus;
        default:
            DEBUG_LOG_ERROR("amaAudio_ConvertCodecType: Unknown codec");
            Panic();
            return va_audio_codec_last;
    }
}

unsigned AmaAudio_HandleVoiceData(Source src)
{
    bool data_processed = FALSE;

    if(AmaData_IsSendingVoiceData())
    {
        data_processed = amaAudio_SendVoiceData(src);
    }

    return data_processed;
}

static va_audio_encode_config_t amaAudio_GetEncodeConfiguration(void)
{
    va_audio_encode_config_t config = {0};

    ama_audio_data_t *ama_audio_cfg = AmaData_GetAudioData();
    config.encoder = amaAudio_ConvertCodecType(ama_audio_cfg->codec);

    switch(config.encoder)
    {
        case va_audio_codec_msbc:
            amaAudio_SendVoiceData = amaAudio_SendMsbcVoiceData;
            config.encoder_params.msbc.bitpool_size = ama_audio_cfg->u.msbc_bitpool_size;
            break;

        case va_audio_codec_opus:
            amaAudio_SendVoiceData = amaAudio_SendOpusVoiceData;
            config.encoder_params.opus.frame_size = AmaAudio_GetOpusFrameSize(ama_audio_cfg);
            break;

        default:
            DEBUG_LOG_ERROR("amaAudio_GetEncodeConfiguration: Unsupported codec");
            Panic();
            break;
    }

    return config;
}

static void amaAudio_SetAudioFormat(void)
{
    ama_audio_data_t *ama_audio_cfg = AmaData_GetAudioData();

    switch(ama_audio_cfg->codec)
    {
        case ama_codec_msbc:
            Ama_SetSpeechAudioFormat(AUDIO_FORMAT__MSBC);
            break;
        case ama_codec_opus:
            Ama_SetSpeechAudioFormat(AMA_DEFAULT_OPUS_FORMAT);
            break;
        default:
            DEBUG_LOG_ERROR("amaAudio_SetAudioFormat: Unsupported codec");
            Panic();
            break;
    }
}

static uint32 amaAudio_GetStartCaptureTimestamp(const va_audio_wuw_detection_info_t *wuw_info)
{
    return (wuw_info->start_timestamp - (uint32) PRE_ROLL_US);
}

static void amaAudio_StartSpeech(ama_start_speech_t * start_speech_data)
{
    PanicNull(start_speech_data);

    uint32 start_sample = 0;
    uint32 end_sample = 0;

    Ama_SetSpeechInitiator(start_speech_data->initiator);

    Ama_SetSpeechAudioProfile(AMA_SPEECH_AUDIO_PROFILE_DEFAULT);

    Ama_NewSpeechDialogId();

    if(start_speech_data->initiator == SPEECH_INITIATOR__TYPE__WAKEWORD)
    {
        #define SAMPLES_MS 16

        uint32 tp_len;

        start_sample = (start_speech_data->pre_roll/1000) * SAMPLES_MS;

        if(start_speech_data->end_timestamp >= start_speech_data->start_timestamp)
        {
            tp_len = start_speech_data->end_timestamp - start_speech_data->start_timestamp;
        }
        else
        {
            tp_len = start_speech_data->end_timestamp + (~(start_speech_data->start_timestamp)) + 1;
        }

        end_sample = ((tp_len/1000) * SAMPLES_MS) + start_sample;

    }

    AmaSendCommand_StartSpeech(Ama_GetSpeechInitiatorType(),
                              Ama_GetSpeechAudioProfile(),
                              Ama_GetSpeechAudioFormat(),
                              Ama_GetSpeechAudioSource(),
                              start_sample,
                              end_sample,
                              start_speech_data->metadata,
                              Ama_GetCurrentSpeechDialogId());
}

static void amaAudio_StartWakeWordCapture(uint32 pre_roll, uint32 start_timestamp, uint32 end_timestamp, data_blob_t metadata)
{
    ama_start_speech_t start_speech_data = {0};

    start_speech_data.initiator = SPEECH_INITIATOR__TYPE__WAKEWORD;
    start_speech_data.pre_roll = pre_roll;
    start_speech_data.start_timestamp = start_timestamp;
    start_speech_data.end_timestamp = end_timestamp;
    start_speech_data.metadata = metadata;
    amaAudio_StartSpeech(&start_speech_data);
}

bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info)
{
    bool start_capture = FALSE;

    capture_params->start_timestamp = amaAudio_GetStartCaptureTimestamp(wuw_info);

    DEBUG_LOG("AmaAudio_WakeUpWordDetected");

    amaAudio_SetAudioFormat();

    if (AmaData_IsReadyToSendStartSpeech())
    {
        amaAudio_StartWakeWordCapture(PRE_ROLL_US, wuw_info->start_timestamp, wuw_info->end_timestamp, wuw_info->metadata);
        start_capture = TRUE;
        capture_params->encode_config = amaAudio_GetEncodeConfiguration();
        AmaData_SetState(ama_state_sending);
        amaAudio_StartSession();
    }

    return start_capture;
}

static bool amaAudio_StartVoiceCapture(void)
{
    va_audio_voice_capture_params_t audio_cfg = {0};

    audio_cfg.mic_config.sample_rate = 16000;
    audio_cfg.mic_config.max_number_of_mics = AMA_MAX_NUMBER_OF_MICS;
    audio_cfg.mic_config.min_number_of_mics = AMA_MIN_NUMBER_OF_MICS;
    audio_cfg.encode_config = amaAudio_GetEncodeConfiguration();

    voice_ui_audio_status_t status = VoiceUi_StartAudioCapture(Ama_GetVoiceUiHandle(), &audio_cfg);
    if (voice_ui_audio_failed == status)
    {
        DEBUG_LOG_ERROR("amaAudio_StartVoiceCapture: Failed to start capture");
        Panic();
    }

    return (voice_ui_audio_success == status) ;
}

static DataFileID ama_LoadWakeUpWordModel(wuw_model_id_t model)
{
    DEBUG_LOG("ama_LoadWakeUpWordModel %d", model);
    return OperatorDataLoadEx(model, DATAFILE_BIN, STORAGE_INTERNAL, FALSE);
}

static void amaAudio_StartWuwDetection(void)
{
    DEBUG_LOG_DEBUG("amaAudio_StartWuwDetection");

    if (current_locale.file_index == FILE_NONE)
    {
        amaAudio_SetCurrentLocaleFileIndex();
    }

    va_audio_wuw_detection_params_t detection =
    {
        .max_pre_roll_in_ms = 2000,
        .wuw_config =
        {
            .engine = va_wuw_engine_apva,
            .model = current_locale.file_index,
            .LoadWakeUpWordModel = ama_LoadWakeUpWordModel,
            .engine_init_preroll_ms = 500,
        },
        .mic_config =
        {
            .sample_rate = 16000,
            .max_number_of_mics = AMA_MAX_NUMBER_OF_MICS,
            .min_number_of_mics = AMA_MIN_NUMBER_OF_MICS
        }
    };

    if (detection.wuw_config.model == FILE_NONE)
    {
        DEBUG_LOG_ERROR("amaAudio_StartWuWDetection: Failed to find model");
        Panic();
    }
    voice_ui_audio_status_t status = VoiceUi_StartWakeUpWordDetection(Ama_GetVoiceUiHandle(), &detection);
    if (voice_ui_audio_failed == status)
    {
        DEBUG_LOG_ERROR("amaAudio_StartWuWDetection: Failed to start detection");
        Panic();
    }
}

static bool amaAudio_StartButtonActivatedCapture(SpeechInitiator__Type trigger_type)
{
    bool capture_started = FALSE;

    if(AmaData_IsReadyToSendStartSpeech())
    {
        amaAudio_SetAudioFormat();
        capture_started = amaAudio_StartVoiceCapture();

        if (capture_started)
        {
            switch(trigger_type)
            {
                case SPEECH_INITIATOR__TYPE__TAP:
                    AmaAudio_StartTapToTalkCapture();
                    break;
                default:
                    DEBUG_LOG_ERROR("AmaAudio_Start: Unsupported trigger");
                    Panic();
                    break;
            }

            amaAudio_StartSession();
        }
    }

    return capture_started;
}

static void amaAudio_SetLocaleName(const char* name)
{
    amaAudio_SetDeviceLocale(name);

    if (AmaAudio_GetDeviceLocale(current_locale.name, ARRAY_DIM(current_locale.name)) == FALSE)
    {
        strncpy(current_locale.name, name, AMA_LOCALE_STR_LEN);
        current_locale.name[ARRAY_DIM(current_locale.name)-1] = '\0';
    }
}

static void amaAudio_PlayPrompt(ama_audio_prompt_t prompt)
{
    DEBUG_LOG_DEBUG("amaAudio_PlayPrompt: prompt=enum:ama_audio_prompt_t:%d", prompt);

#ifndef HAVE_RDP_UI
    FILE_INDEX file_index = amaAudio_ResolveLocaleFilename(prompt, NULL, 0);

    if (file_index == FILE_NONE)
    {
        DEBUG_LOG_ERROR("amaAudio_PlayPrompt: file not found for default locale");
        Panic();
    }
#endif
    switch (prompt)
    {
    case ama_audio_prompt_unregistered:
        VoiceUi_Notify(VOICE_UI_AMA_UNREGISTERED);
        break;

    case ama_audio_prompt_not_connected:
        VoiceUi_Notify(VOICE_UI_AMA_NOT_CONNECTED);
        break;
    }
}

void AmaAudio_StartTapToTalkCapture(void)
{
    ama_start_speech_t start_speech_data = {0};

    start_speech_data.initiator = SPEECH_INITIATOR__TYPE__TAP;
    amaAudio_StartSpeech(&start_speech_data);
}

void AmaAudio_StartButtonActivatedCapture(SpeechInitiator__Type type)
{
    bool started = FALSE;

    if (Ama_IsLiveCaptureAllowedToStart())
    {
        if (Ama_IsSetupComplete())
        {
            if (AmaTransport_IsConnected())
            {
                ama_button_activate_retry = amaAudio_HaveAppLaunch();
                started = amaAudio_StartButtonActivatedCapture(type);
            }
            else
            {
                DEBUG_LOG_WARN("AmaAudio_StartButtonActivatedCapture: not connected");
                
                if (ama_button_activate_retry && amaAudio_RetryStart(type))
                {
                    ama_button_activate_retry = FALSE;
                }
                else
                {
                    amaAudio_PlayPrompt(ama_audio_prompt_not_connected);
                }
            }
        }
        else
        {
            amaAudio_PlayPrompt(ama_audio_prompt_unregistered);
        }
    }
    else
    {
        DEBUG_LOG("AmaAudio_StartButtonActivatedCapture: not allowed");
    }

    if (started)
    {
        AmaData_SetState(ama_state_sending);
    }
}

void AmaAudio_StopCapture(void)
{
    DEBUG_LOG("AmaAudio_StopCapture");
    VoiceUi_StopAudioCapture(Ama_GetVoiceUiHandle());
}

bool AmaAudio_Provide(const AMA_SPEECH_PROVIDE_IND_T* ind)
{
    bool capture_started = FALSE;
    uint32 dialog_id = ind->dialog_id;

    if (AmaData_IsReadyToSendStartSpeech())
    {
        capture_started = amaAudio_StartVoiceCapture();
    }

    if(capture_started)
    {
        Ama_UpdateSpeechDialogId(dialog_id);
    }

    return capture_started;
}

void AmaAudio_Cancel(void)
{
    DEBUG_LOG("AmaAudio_Cancel");
    VoiceUi_StopAudioCapture(Ama_GetVoiceUiHandle());
    AmaSendCommand_StopSpeech(ERROR_CODE__USER_CANCELLED, Ama_GetCurrentSpeechDialogId());
    AmaData_SetState(ama_state_idle);
}

void AmaAudio_StartWakeWordDetection(void)
{
    DEBUG_LOG("AmaAudio_StartWakeWordDetection");

    if(Ama_IsWakeUpWordFeatureIncluded() && Ama_IsWakeUpWordDetectionAllowedToStart())
    {
        amaAudio_StartWuwDetection();
    }
    else
    {
        DEBUG_LOG("AmaAudio_StartWakeWordDetection not allowed");
    }
}

void AmaAudio_StopWakeWordDetection(void)
{
    DEBUG_LOG("AmaAudio_StopWakeWordDetection");
    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        VoiceUi_StopWakeUpWordDetection(Ama_GetVoiceUiHandle());
    }
}

const char *AmaAudio_GetModelFromLocale(const char* locale)
{
    const char *model = locale;
    for(uint8 i=0; i<sizeof(locale_to_model)/sizeof(locale_to_model[0]); i++)
    {
        if(strcmp(locale, locale_to_model[i].locale) == 0)
        {
            model = locale_to_model[i].model;
            break;
        }
    }
    return model;
}

const char* AmaAudio_GetCurrentLocale(void)
{
    return current_locale.name;
}

void AmaAudio_GetSupportedLocales(ama_supported_locales_t* supported_locales)
{
    memset(supported_locales, 0, sizeof(ama_supported_locales_t));
    ama_GetLocalesInFileSystem(supported_locales);
}

void AmaAudio_PopulateLocales(Locales * locales, Locale * locale_arr, Locale ** locale_ptr_arr)
{
    ama_supported_locales_t supported_locales;
    AmaAudio_GetSupportedLocales(&supported_locales);;


    for(uint8 i=0; i<supported_locales.num_locales; i++)
    {
        locale_arr[i].name = (char*)supported_locales.name[i];

        if(strcmp(current_locale.name, locale_arr[i].name) == 0)
        {
            locales->current_locale = locale_ptr_arr[i];
        }
    }

    if (supported_locales.num_locales == 0)
    {
        locale_arr[0].name = (char *) current_locale.name;
        supported_locales.num_locales = 1;
        locales->current_locale = locale_ptr_arr[0];
    }

    locales->n_supported_locales = supported_locales.num_locales;
    locales->supported_locales = locale_ptr_arr;

    DEBUG_LOG("AmaAudio_PopulateLocales number of supported locales: %d", locales->n_supported_locales);
    DEBUG_LOG("AmaAudio_PopulateLocales supported locales:");

    for(uint8 i=0; i<supported_locales.num_locales; i++)
    {
        AmaLog_LogVaArg("\t%s\n", locale_ptr_arr[i]->name);
    }
    if (locales->current_locale)
    {
        AmaLog_LogVaArg("AmaAudio_PopulateLocales current locale: %s\n", locales->current_locale->name);
    }
}

void AmaAudio_SetLocale(const char* locale)
{
    amaAudio_SetLocaleName(locale);

    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        amaAudio_SetCurrentLocaleFileIndex();
        AmaAudio_StartWakeWordDetection();
    }
}

bool AmaAudio_GetDeviceLocale(char *locale, uint8 locale_size)
{
    uint8 packed_locale[DEVICE_SIZEOF_VA_LOCALE];
    /* The unpacked form has a hyphen and a \0 terminator */
    PanicFalse(locale_size >= AMA_LOCALE_STR_SIZE);
    VoiceUi_GetPackedLocale(packed_locale);

    if (packed_locale[0] != '\0')
    {
        locale[0] = packed_locale[0];
        locale[1] = packed_locale[1];
        locale[2] = '-';
        locale[3] = packed_locale[2];
        locale[4] = packed_locale[3];
        locale[5] = '\0';

        DEBUG_LOG_DEBUG("AmaAudio_GetDeviceLocale: locale=\"%c%c%c%c%c\"",
                        locale[0], locale[1], locale[2], locale[3], locale[4]);

        return TRUE;
    }

    DEBUG_LOG_WARN("AmaAudio_GetDeviceLocale: no locale");
    return FALSE;
}

static void amaAudio_SetDeviceLocale(const char *locale)
{
    /* The unpacked form has a hyphen and a \0 terminator */
    if (strlen(locale) == AMA_LOCALE_STR_LEN && locale[2] == '-')
    {
        uint8 packed_locale[DEVICE_SIZEOF_VA_LOCALE];

        packed_locale[0] = locale[0];
        packed_locale[1] = locale[1];
        packed_locale[2] = locale[3];
        packed_locale[3] = locale[4];

        VoiceUi_SetPackedLocale(packed_locale);

        DEBUG_LOG_DEBUG("amaAudio_SetDeviceLocale: locale=\"%c%c%c%c%c\"",
                        locale[0], locale[1], locale[2], locale[3], locale[4]);
    }
    else
    {
        DEBUG_LOG_ERROR("amaAudio_SetDeviceLocale: bad ISO language-country");
        Panic();
    }
}

void Ama_ConfigureCodec(ama_codec_t codec)
{
    ama_audio_data_t audio_config;

    audio_config.codec = codec;

    if(audio_config.codec == ama_codec_opus)
    {
        audio_config.u.opus_format = AMA_DEFAULT_OPUS_FORMAT;
    }
    else if(audio_config.codec == ama_codec_msbc)
    {
        audio_config.u.msbc_bitpool_size = MSBC_ENCODER_BITPOOL_VALUE;
    }

    AmaData_SetAudioData(&audio_config);
}

void AmaAudio_SetAlexaReady(bool ready)
{
    DEBUG_LOG("AmaAudio_SetAlexaReady: %u", ready);
    ama_audio_lock = ready ? 0 : 1;
}

void AmaAudio_NotifySessionStarted(void)
{
    MessageCancelAll((Task) &ama_audio_task, AMA_AUDIO_SESSION_START_TIMEOUT);
}

void AmaAudio_Init(void)
{
    /* Check that the internal and external locale representation sizes are compatible */
    PanicFalse((DEVICE_SIZEOF_VA_LOCALE+2)==AMA_LOCALE_STR_SIZE);
    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        ValidateLocaleSize();
    }
}

#endif /* INCLUDE_AMA */
