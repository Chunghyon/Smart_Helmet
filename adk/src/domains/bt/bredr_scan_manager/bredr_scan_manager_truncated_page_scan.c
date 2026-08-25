/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       bredr_scan_manager_truncated_scan.c
\brief	    Implementation of module managing enabling and disabling truncated page scanning.
*/

#include <message.h>
#include "bredr_scan_manager.h"
#include "bredr_scan_manager_private.h"
#include "bredr_scan_manager_truncated_page_scan.h"

static void bredrScanManager_HandleTruncatedPageScanRequest(Task client, bredr_scan_manager_scan_type_t type)
{
    DEBUG_LOG_VERBOSE("bredrScanManager_HandleTruncatedPageScanRequest");

    bredrScanManager_InstanceClientAddOrUpdate(bredrScanManager_TruncatedPageScanContext(), client, type);
    bredrScanManager_InstanceRefresh(bredrScanManager_TruncatedPageScanContext());
}

static void bredrScanManager_TruncatedPageScanMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        case BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST:
            bredrScanManager_HandleTruncatedPageScanRequest(\
                        ((const BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST_T*)message)->client, \
                        ((const BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST_T*)message)->type);
            break;

        default:
            break;
    }
}

TaskData truncated_scan_task_data = {.handler  = bredrScanManager_TruncatedPageScanMessageHandler};

void BredrScanManager_TruncatedPageScanRequest(Task client, bredr_scan_manager_scan_type_t page_type)
{
    if(page_type <= SCAN_MAN_PARAMS_TYPE_MAX)
    {
        MESSAGE_MAKE(msg, BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST_T);
        msg->client = client;
        msg->type = page_type;
        MessageSendConditionally(&truncated_scan_task_data, BREDR_SCAN_MANAGER_INTERNAL_MESSAGE_TRUNCATED_PAGE_SCAN_REQUEST, msg, &bredrScanManager_GetTaskData()->page_scan.lock);
    }
}

void BredrScanManager_TruncatedPageScanRelease(Task client)
{
    bredrScanManager_InstanceClientRemove(bredrScanManager_TruncatedPageScanContext(), client);
}
