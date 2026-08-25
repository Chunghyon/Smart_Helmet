/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice ui async req APIs
*/

#include "voice_ui_async_req.h"
#include <link_policy.h>
#include <panic.h>

typedef enum
{
    LINK_POLICY_UPDATE_REQ
} message_ids_t;

typedef bdaddr LINK_POLICY_UPDATE_REQ_T;

static void voiceUi_MsgHandler(Task task, MessageId id, Message message);
static const TaskData msg_handler = { voiceUi_MsgHandler };

static void voiceUi_MsgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    switch(id)
    {
        case LINK_POLICY_UPDATE_REQ:
        {
            const LINK_POLICY_UPDATE_REQ_T *params = message;
            appLinkPolicyUpdatePowerTable(params);
        }
        break;
        default:
            Panic();
        break;
    }
}

static inline void voiceUi_SendMessage(MessageId id, void *message)
{
    MessageSend((Task)&msg_handler, id, message);
}

void VoiceUi_SendLinkPolicyUpdateReq(const bdaddr *address)
{
    MESSAGE_MAKE(msg, LINK_POLICY_UPDATE_REQ_T);
    *msg = *address;
    voiceUi_SendMessage(LINK_POLICY_UPDATE_REQ, msg);
}
