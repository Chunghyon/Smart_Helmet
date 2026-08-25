/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_notify_app.c
\brief  Implementation of AMA transport app notifications
*/

#ifdef INCLUDE_AMA
#include "ama_transport_notify_app.h"
#include "ama_msg_types.h"
#include "logging.h"
#include "panic.h"
#include "task_list.h"

#define MAX_NUMBER_OF_APP_TASKS 2

static task_list_t * app_tasks = NULL;

void AmaTransport_InitAppTaskList(void)
{
    app_tasks = TaskList_CreateWithCapacity(MAX_NUMBER_OF_APP_TASKS);
    PanicNull(app_tasks);
}

void AmaTransport_RegisterAppTask(Task task)
{
    PanicFalse(TaskList_AddTask(app_tasks, task));
}

void AmaTransport_NotifyAppTransportSwitched(ama_transport_type_t new_transport)
{
    DEBUG_LOG("AmaTransport_NotifyAppTransportSwitched enum:ama_transport_type_t:%d", new_transport);
    MAKE_AMA_MESSAGE(AMA_SWITCH_TRANSPORT_IND);
    message->transport = new_transport;
    TaskList_MessageSend(app_tasks, AMA_SWITCH_TRANSPORT_IND, message);
}

void AmaTransport_NotifyAppLocalDisconnectComplete(void)
{
    TaskList_MessageSendId(app_tasks, AMA_LOCAL_DISCONNECT_COMPLETE_IND);
}

#endif /* INCLUDE_AMA */
