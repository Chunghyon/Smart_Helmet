/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions for generating telephony notification messages
*/

#include "telephony_messages.h"

#include <task_list.h>
#include <panic.h>
#include <logging.h>
#include <stdlib.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(telephony_domain_messages)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TELEPHONY, TELEPHONY_MESSAGE_END)

#endif

static task_list_t * client_list;

static task_list_t * telephony_GetMessageClients(void)
{
    return client_list;
}

void Telephony_NotifyMessage(MessageId id, voice_source_t source)
{
    telephony_message_t * const message = (telephony_message_t *)PanicNull(calloc(1, sizeof(telephony_message_t)));
    message->voice_source = source;
    TaskList_MessageSendWithSize(telephony_GetMessageClients(), id, message, sizeof(telephony_message_t));
}

void Telephony_NotifyCallerId(
        voice_source_t source,
        char * caller_number,
        phone_number_type_t type,
        char * caller_name)
{
    if (caller_number != NULL)
    {
        /* +1 to account for null termination. */
        uint8 number_len = strlen(caller_number) + 1;
        size_t name_len = 1;
        if (caller_name != NULL)
        {
            name_len += strlen(caller_name);
        }

        /* N.b. Memset to 0 ensures caller_name is an empty null terminated string, if the name is not present. */
        size_t size = sizeof(telephony_caller_id_notification_t) + name_len + number_len;
        telephony_caller_id_notification_t* caller_id = PanicUnlessMalloc(size);
        memset(caller_id, 0, size);

        caller_id->voice_source = source;
        caller_id->number_type = type;

        /* Calculate String offsets in the message data buffer. */
        caller_id->caller_number = &caller_id->data[0];
        caller_id->caller_name = &caller_id->data[number_len];

        /* Assign ASCII encoded caller number string. */
        caller_id->caller_number = strcpy(caller_id->caller_number, caller_number);

        /* Assign caller name, if present. */
        if (caller_name != NULL)
        {
            caller_id->caller_name = strcpy(caller_id->caller_name, caller_name);
        }

        TaskList_MessageSendWithSize(
                    telephony_GetMessageClients(),
                    TELEPHONY_CALLER_ID_NOTIFICATION,
                    caller_id,
                    sizeof(telephony_caller_id_notification_t));
    }
}

bool Telephony_InitMessages(Task init_task)
{
    UNUSED(init_task);
    client_list = TaskList_Create();
    return TRUE;
}

void Telephony_RegisterForMessages(Task task_to_register)
{
    TaskList_AddTask(telephony_GetMessageClients(), task_to_register);
}

static void Telephony_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == TELEPHONY_MESSAGE_GROUP);
    TaskList_AddTask(telephony_GetMessageClients(), task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(TELEPHONY, Telephony_RegisterMessageGroup, NULL);

