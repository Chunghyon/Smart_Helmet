/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_client_notifier.c
\brief      Implementation of the client notifier APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol.h"
#include "dfu_protocol_client_notifier.h"
#include "dfu_protocol_client_config.h"

#include <panic.h>

#define MAKE_DFU_PROTOCOL_CLIENT_MESSAGE(TYPE) MESSAGE_MAKE(message,TYPE##_T);

static void dfuProtocol_SendClientNotification(dfu_protocol_client_message_t id, void * msg)
{
    DEBUG_LOG("dfuProtocol_SendClientNotification enum:dfu_protocol_client_message_t:%d", id);

    Task client_task = DfuProtocol_GetClientTask();

    if(client_task)
    {
        MessageSend(client_task, id, msg);
    }
}

void DfuProtocol_SendCommitReqToActiveClient(void)
{
    dfuProtocol_SendClientNotification(DFU_PROTOCOL_COMMIT_REQ, NULL);
}

void DfuProtocol_SendErrorIndToActiveClient(UpgradeHostErrorCode error_code)
{
    MAKE_DFU_PROTOCOL_CLIENT_MESSAGE(DFU_PROTOCOL_ERROR_IND);
    message->error_code = error_code;
    dfuProtocol_SendClientNotification(DFU_PROTOCOL_ERROR_IND, message);
}

void DfuProtocol_SendCacheClearedIndToActiveClient(void)
{
    dfuProtocol_SendClientNotification(DFU_PROTOCOL_CACHE_CLEARED_IND, NULL);
}

void DfuProtocol_SendTransportDisconnectedIndToTask(Task task, dfu_protocol_disconnect_reason_t reason)
{
    PanicNull(task);
    MAKE_DFU_PROTOCOL_CLIENT_MESSAGE(DFU_PROTOCOL_TRANSPORT_DISCONNECTED_IND);
    message->reason = reason;
    MessageSend(task, DFU_PROTOCOL_TRANSPORT_DISCONNECTED_IND, message);
}

void DfuProtocol_SendPermissionDeniedIndToActiveClient(void)
{
    dfuProtocol_SendClientNotification(DFU_PROTOCOL_PERMISSION_DENIED_IND, NULL);
}

void DfuProtocol_SendTransferCompleteIndToActiveClient(void)
{
    dfuProtocol_SendClientNotification(DFU_PROTOCOL_TRANSFER_COMPLETE_IND, NULL);
}
