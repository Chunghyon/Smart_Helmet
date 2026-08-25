/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Audio Driver core interface implementation

*/

#include "audio_driver.h"
#include "audio_driver_indications.h"
#include <audio_use_case_handler.h>
#include <kymera_data.h>
#include <logging.h>
#include <message.h>

#define ASSERT_HANDLER_CALLBACK(instance, callback_name) \
    do { \
        if (AudioUseCase_GetInstanceHandler(instance)->callback_name == NULL) \
        { \
            /* Stringification doesn't seem to work on target when it is in quotes, it will work as follows */ \
            DEBUG_LOG_PANIC("Audio use case instance %p has no " #callback_name " callback", instance); \
        } \
    } while(0)

#define ALLOCATE_MESSAGE_WITH_DATA_BLOB(message, data_blob) \
    do { \
        size_t data_length = (data_blob != NULL) ? data_blob->data_length : 0; \
        message = PanicUnlessMalloc(sizeof(*message) + data_length); \
    } while(0)

typedef enum
{
    AUDIO_DRIVER_PREPARE,
    AUDIO_DRIVER_START,
    AUDIO_DRIVER_STOP,
    AUDIO_DRIVER_IOCTL
} internal_message_ids;

typedef struct
{
    audio_use_case_instance_t instance;
    data_blob_t params;
} INTERNAL_COMMON_MESSAGE_T;

typedef INTERNAL_COMMON_MESSAGE_T AUDIO_DRIVER_PREPARE_T;
typedef INTERNAL_COMMON_MESSAGE_T AUDIO_DRIVER_START_T;
typedef INTERNAL_COMMON_MESSAGE_T AUDIO_DRIVER_STOP_T;
typedef struct
{
    unsigned ioctl_id;
    // Must remain as last element in struct since it points to memory beyond this bound
    INTERNAL_COMMON_MESSAGE_T common;
} AUDIO_DRIVER_IOCTL_T;

static void audioDriver_MessageHandler(Task task, MessageId id, Message message);
static const TaskData msg_handler = {audioDriver_MessageHandler};

static void audioDriver_MessageHandler(Task task, MessageId id, Message message)
{
    audio_use_case_request_status_t status = audio_use_case_request_pending;
    UNUSED(task);

    switch(id)
    {
        case AUDIO_DRIVER_PREPARE:
        {
            const AUDIO_DRIVER_PREPARE_T *msg = message;
            AudioDriver_PreparingInd(msg->instance);
            status = AudioUseCase_GetInstanceHandler(msg->instance)->Prepare(msg->instance, &msg->params);
            AudioDriver_PreparedInd(msg->instance);
        }
        break;
        case AUDIO_DRIVER_START:
        {
            const AUDIO_DRIVER_START_T *msg = message;
            AudioDriver_StartingInd(msg->instance);
            status = AudioUseCase_GetInstanceHandler(msg->instance)->Start(msg->instance, &msg->params);
            AudioDriver_StartedInd(msg->instance);
        }
        break;
        case AUDIO_DRIVER_STOP:
        {
            const AUDIO_DRIVER_STOP_T *msg = message;
            AudioDriver_StoppingInd(msg->instance);
            status = AudioUseCase_GetInstanceHandler(msg->instance)->Stop(msg->instance, &msg->params);
            AudioDriver_StoppedInd(msg->instance);
        }
        break;
        case AUDIO_DRIVER_IOCTL:
        {
            const AUDIO_DRIVER_IOCTL_T *msg = message;
            AudioDriver_BeginIoctlInd(msg->common.instance, msg->ioctl_id);
            status = AudioUseCase_GetInstanceHandler(msg->common.instance)->Ioctl(msg->common.instance, msg->ioctl_id, &msg->common.params);
            AudioDriver_EndIoctlInd(msg->common.instance, msg->ioctl_id);
        }
        break;
        default:
            DEBUG_LOG_PANIC("audioDriver_MessageHandler: Unknown message ID %d", id);
    }

    if (status != audio_use_case_request_success)
    {
        DEBUG_LOG_PANIC("audioDriver_MessageHandler: Only synchronous successful requests are supported");
    }
}

static void audioDriver_SendInternalMessage(MessageId id, void *message)
{
    /*
     * Messages must be kept in the same ordering as ones created using kymera APIs
     * This can be removed when all kymera handling can be done via Audio Driver API
     */
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageSendConditionally((Task)&msg_handler, id, message, &theKymera->lock);
}

static void audioDriver_PopulateCommonMsg(INTERNAL_COMMON_MESSAGE_T *msg, audio_use_case_instance_t instance, const data_blob_t *params)
{
    msg->instance = instance;
    msg->params.data_length = (params != NULL) ? params->data_length : 0;
    if (msg->params.data_length)
    {
        // Copy data at the end of the message struct, the caller must ensure that this much memory is allocated for this purpose
        uint8 *memory = (void*)msg;
        msg->params.data = &memory[sizeof(*msg)];
        memcpy(msg->params.data, params->data, params->data_length);
    }
    else
    {
        msg->params.data = NULL;
    }
}

static INTERNAL_COMMON_MESSAGE_T * audioDriver_CreateCommonMsg(audio_use_case_instance_t instance, const data_blob_t *params)
{
    INTERNAL_COMMON_MESSAGE_T *msg;
    ALLOCATE_MESSAGE_WITH_DATA_BLOB(msg, params);
    audioDriver_PopulateCommonMsg(msg, instance, params);
    return msg;
}

static AUDIO_DRIVER_IOCTL_T * audioDriver_CreateIoctlMsg(audio_use_case_instance_t instance, unsigned ioctl_id, const data_blob_t *params)
{
    AUDIO_DRIVER_IOCTL_T *msg;
    ALLOCATE_MESSAGE_WITH_DATA_BLOB(msg, params);
    msg->ioctl_id = ioctl_id;
    audioDriver_PopulateCommonMsg(&msg->common, instance, params);
    return msg;
}

void AudioDriver_PrepareUseCase(audio_use_case_instance_t instance, const data_blob_t *params)
{
    ASSERT_HANDLER_CALLBACK(instance, Prepare);
    AUDIO_DRIVER_PREPARE_T *msg = audioDriver_CreateCommonMsg(instance, params);
    audioDriver_SendInternalMessage(AUDIO_DRIVER_PREPARE, msg);
}

void AudioDriver_StartUseCase(audio_use_case_instance_t instance, const data_blob_t *params)
{
    ASSERT_HANDLER_CALLBACK(instance, Start);
    AUDIO_DRIVER_START_T *msg = audioDriver_CreateCommonMsg(instance, params);
    audioDriver_SendInternalMessage(AUDIO_DRIVER_START, msg);
}

void AudioDriver_StopUseCase(audio_use_case_instance_t instance, const data_blob_t *params)
{
    ASSERT_HANDLER_CALLBACK(instance, Stop);
    AUDIO_DRIVER_STOP_T *msg = audioDriver_CreateCommonMsg(instance, params);
    audioDriver_SendInternalMessage(AUDIO_DRIVER_STOP, msg);
}

void AudioDriver_IoctlUseCase(audio_use_case_instance_t instance, unsigned ioctl_id, const data_blob_t *params)
{
    ASSERT_HANDLER_CALLBACK(instance, Ioctl);
    AUDIO_DRIVER_IOCTL_T *msg = audioDriver_CreateIoctlMsg(instance, ioctl_id, params);
    audioDriver_SendInternalMessage(AUDIO_DRIVER_IOCTL, msg);
}

#ifdef HOSTED_TEST_ENVIRONMENT
extern void AudioDriver_ObserverTestReset(void);
void AudioDriver_TestReset(void)
{
    AudioDriver_ObserverTestReset();
}
#endif
