/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol.c
\brief      Implemetation of the public APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol.h"

#include "dfu_protocol_client_config.h"
#include "dfu_protocol_data.h"
#include "dfu_protocol_message_dispatcher.h"
#include "dfu_protocol_message_handler.h"
#include "dfu_protocol_sm.h"
#include "dfu_protocol_sm_types.h"

#include "dfu.h"

DEBUG_LOG_DEFINE_LEVEL_VAR

bool DfuProtocol_Init(Task task)
{
    UNUSED(task);
    DfuProtocol_InitialiseClientConfig();
    DfuProtocol_MessageHandlerInit();

    return TRUE;
}

void DfuProtocol_RegisterClient(const dfu_protocol_client_config_t * config)
{
    DfuProtocol_RegisterClientConfig(config);
}

dfu_protocol_status_t DfuProtocol_StartOta(upgrade_context_t client_context, uint32 dfu_file_size)
{
    dfu_protocol_status_t status = dfu_protocol_success;
    dfu_protocol_start_params_t start_params = { dfu_file_size, client_context };
    upgrade_context_t active_client_context = Upgrade_GetContext();
    bool upgrade_transport_in_use = UpgradeTransportInUse();
    bool waiting_for_silent_commit = UpgradeIsSilentCommitEnabled();

    DEBUG_LOG_FN_ENTRY("DfuProtocol_StartOta enum:upgrade_context_t:%d size=%u upgrade_transport_in_use=%d waiting_for_silent_commit=%d",
                        client_context, dfu_file_size, upgrade_transport_in_use, waiting_for_silent_commit);

    if(client_context != UPGRADE_CONTEXT_UNUSED)
    {
        if(((active_client_context == UPGRADE_CONTEXT_UNUSED) || (active_client_context == client_context)) && !upgrade_transport_in_use && !waiting_for_silent_commit)
        {
            StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_start_event, (void *)&start_params);
        }
        else if(waiting_for_silent_commit)
        {
            DEBUG_LOG_WARN("DfuProtocol_StartOta enum:upgrade_context_t:%d waiting for silent commit", active_client_context);
            status = dfu_protocol_waiting_for_silent_commit;
        }
        else if((active_client_context == client_context) && upgrade_transport_in_use)
        {
            DEBUG_LOG_WARN("DfuProtocol_StartOta enum:upgrade_context_t:%d is already running a DFU", active_client_context);
            status = dfu_protocol_already_started;
        }
        else if((active_client_context == client_context) && !upgrade_transport_in_use && DfuProtocol_DidActiveClientCauseReboot())
        {
            DEBUG_LOG_WARN("DfuProtocol_StartOta enum:upgrade_context_t:%d is waiting for post reboot sequence to run", active_client_context);
            status = dfu_protocol_waiting_for_post_reboot_sequence;
        }
        else
        {
            DEBUG_LOG_WARN("DfuProtocol_StartOta called when another DFU is in progress enum:upgrade_context_t:%d", active_client_context);
            status = dfu_protocol_another_client_is_running;
        }
    }
    else
    {
        DEBUG_LOG_WARN("DfuProtocol_StartOta not a valid client context");
        status = dfu_protocol_error;
    }

    return status;
}

bool DfuProtocol_IsCacheEmpty(void)
{
    return (DfuProtocol_GetLengthOfAvailableDataInBytes() == 0);
}

dfu_protocol_status_t DfuProtocol_AddDataToOtaBuffer(upgrade_context_t client_context, uint8 * data, uint16 data_length, uint32 data_offset)
{
    DEBUG_LOG_FN_ENTRY("DfuProtocol_AddDataToOtaBuffer enum:upgrade_context_t:%d length=%u offset=%u", client_context, data_length, data_offset);

    dfu_protocol_status_t status = dfu_protocol_success;
    upgrade_context_t active_client_context = DfuProtocol_GetActiveClientContext();

    if(active_client_context == client_context)
    {
        if(DfuProtocol_WriteData(data, data_length, data_offset))
        {
            DfuProtocol_SendUpdateHostData();
        }
        else
        {
            status = dfu_protocol_error;
        }
    }
    else
    {
        DEBUG_LOG_WARN("DfuProtocol_AddDataToOtaBuffer called by non-active client enum:upgrade_context_t:%d", active_client_context);
        status = dfu_protocol_error;
    }

    return status;
}

bool DfuProtocol_IsWaitingForApply(upgrade_context_t client_context)
{
    bool waiting_for_apply = FALSE;

    if(client_context == DfuProtocol_GetActiveClientContext())
    {
        waiting_for_apply = DfuProtocol_IsSmWaitingForApply();
    }

    return waiting_for_apply;
}

dfu_protocol_status_t DfuProtocol_Apply(upgrade_context_t client_context)
{
    DEBUG_LOG_FN_ENTRY("DfuProtocol_Apply enum:upgrade_context_t:%d", client_context);

    dfu_protocol_status_t status = dfu_protocol_success;
    upgrade_context_t active_client_context = DfuProtocol_GetActiveClientContext();

    if(active_client_context == client_context)
    {
        StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_apply_event, NULL);
    }
    else
    {
        DEBUG_LOG_WARN("DfuProtocol_Apply called by non-active client enum:upgrade_context_t:%d", active_client_context);
        status = dfu_protocol_error;
    }

    return status;
}

dfu_protocol_status_t DfuProtocol_CommitOta(upgrade_context_t client_context)
{
    DEBUG_LOG_FN_ENTRY("DfuProtocol_CommitOta enum:upgrade_context_t:%d", client_context);

    dfu_protocol_status_t status = dfu_protocol_success;
    upgrade_context_t active_client_context = DfuProtocol_GetActiveClientContext();

    if(active_client_context == client_context)
    {
        StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_commit_event, NULL);
    }
    else
    {
        DEBUG_LOG_WARN("DfuProtocol_CommitOta called by non-active client enum:upgrade_context_t:%d", active_client_context);
        status = dfu_protocol_error;
    }

    return status;
}

dfu_protocol_status_t DfuProtocol_AbortOta(upgrade_context_t client_context)
{
    DEBUG_LOG_FN_ENTRY("DfuProtocol_AbortOta enum:upgrade_context_t:%d", client_context);

    dfu_protocol_status_t status = dfu_protocol_success;
    dfu_protocol_abort_params_t abort_params = { client_context };
    /* Use Upgrade library context to catch instances where clients who are not using DFU protocol are performing a DFU */
    upgrade_context_t active_client_context = Upgrade_GetContext();

    if((active_client_context == client_context) || (active_client_context == UPGRADE_CONTEXT_UNUSED))
    {
        StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_abort_event, &abort_params);
    }
    else
    {
        DEBUG_LOG_WARN("DfuProtocol_AbortOta called by non-active client while an active client is running enum:upgrade_context_t:%d", active_client_context);
        status = dfu_protocol_error;
    }

    return status;
}

dfu_protocol_status_t DfuProtocol_Disconnect(upgrade_context_t client_context)
{
    DEBUG_LOG_FN_ENTRY("DfuProtocol_Disconnect enum:upgrade_context_t:%d", client_context);

    dfu_protocol_status_t status = dfu_protocol_success;
    /* Use Upgrade library context to catch instances where clients who are not using DFU protocol are performing a DFU */
    upgrade_context_t active_client_context = Upgrade_GetContext();

    if((active_client_context == client_context) || (active_client_context == UPGRADE_CONTEXT_UNUSED))
    {
        StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_transport_disconnect_event, NULL);
    }
    else
    {
        DEBUG_LOG_WARN("DfuProtocol_Disconnect called by non-active client while an active client is running enum:upgrade_context_t:%d", active_client_context);
        status = dfu_protocol_error;
    }

    return status;
}
