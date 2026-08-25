/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_sm_actions.c
\brief      Implemetation of the state machine action APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol_sm_actions.h"

#include "dfu_protocol.h"
#include "dfu_protocol_client_config.h"
#include "dfu_protocol_client_notifier.h"
#include "dfu_protocol_data.h"
#include "dfu_protocol_message_dispatcher.h"
#include "dfu_protocol_message_handler.h"
#include "dfu_protocol_sm.h"
#include "dfu_protocol_sm_types.h"
#include "dfu_protocol_upgrade_config.h"

#include <dfu.h>
#include <dfu_peer.h>

#include <bt_device.h>
#include <link_policy.h>
#include <panic.h>
#include <upgrade.h>

static upgrade_context_t start_pending_context = UPGRADE_CONTEXT_UNUSED;
static Task disconnecting_client_task = NULL;
static struct
{
    unsigned generic:1;
    unsigned aborting:1;
} disconnecting_reason = {0};

void DfuProtocol_CommonAbortEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_AbortHostRequest();
    disconnecting_reason.aborting = TRUE;
}

void DfuProtocol_TransportConnectedAbortEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_SetActiveClient(start_pending_context);
    start_pending_context = UPGRADE_CONTEXT_UNUSED;
    DfuProtocol_AbortHostRequest();
    disconnecting_reason.aborting = TRUE;
}

void DfuProtocol_ConnectTransportForAbortEvent(const void * params)
{
    dfu_protocol_abort_params_t * abort_params = (dfu_protocol_abort_params_t *)params;
    start_pending_context = abort_params->context;
    UpgradeTransportConnectRequest(DfuProtocol_GetMessageHandlerTask(), UPGRADE_DATA_CFM_ALL, UPGRADE_MAX_REQUEST_SIZE_NO_LIMIT);
}

void DfuProtocol_GenericCompleteEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_DestroyDataInstance();
    disconnecting_client_task = DfuProtocol_GetClientTask();
    DfuProtocol_SetActiveClient(UPGRADE_CONTEXT_UNUSED);
    UpgradeTransportDisconnectRequest();
}

void DfuProtocol_StartEventAction(const void * params)
{
    dfu_protocol_start_params_t * start_params = (dfu_protocol_start_params_t *)params;
    DfuProtocol_CreateDataInstance(start_params->dfu_file_size);
    start_pending_context = start_params->context;
    DfuProtocol_SetUpgradeSupportsSilentCommit(FALSE);
    UpgradeTransportConnectRequest(DfuProtocol_GetMessageHandlerTask(), UPGRADE_DATA_CFM_ALL, UPGRADE_MAX_REQUEST_SIZE_NO_LIMIT);
}

void DfuProtocol_StartPostRebootEventAction(const void * params)
{
    dfu_protocol_start_params_t * start_params = (dfu_protocol_start_params_t *)params;
    start_pending_context = start_params->context;
    UpgradeTransportConnectRequest(DfuProtocol_GetMessageHandlerTask(), UPGRADE_DATA_CFM_ALL, UPGRADE_MAX_REQUEST_SIZE_NO_LIMIT);
}

void DfuProtocol_TransportConnectedEventAction(const void * params)
{
    UNUSED(params);
    DfuPeer_SetRole(TRUE);
    UpgradePermit(upgrade_perm_assume_yes);
    DfuProtocol_SetActiveClient(start_pending_context);
    start_pending_context = UPGRADE_CONTEXT_UNUSED;
    DfuProtocol_SyncOtaHostRequest(DfuProtocol_GetClientInProgressId());
    appLinkPolicyUpdatePowerTable(DfuProtocol_GetClientHandsetBtAddress());
    /* Set the flag for AG as Upgrade Transport has connected */
    BtDevice_SetUpgradeTransportConnected(BtDevice_GetDeviceForBdAddr(DfuProtocol_GetClientHandsetBtAddress()), TRUE);
}

void DfuProtocol_TransportConnectedPostRebootEventAction(const void * params)
{
    UNUSED(params);
    UpgradePermit(upgrade_perm_assume_yes);
    DfuProtocol_SyncOtaHostRequest(DfuProtocol_GetClientInProgressId());
}

void DfuProtocol_SyncCompleteEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_StartOtaHostRequest();

    if(DfuProtocol_GetClientSupportSilentCommit())
    {
        DfuProtocol_SilentCommitSupportedRequest();
    }
}
void DfuProtocol_SyncCompletePostRebootEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_StartOtaHostRequest();
}

void DfuProtocol_BeginTransferEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_StartOtaHostDataRequest();
}

void DfuProtocol_CommitRequestEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_OtaHostInProgressResponse(UPGRADE_HOSTACTION_YES);
}

void DfuProtocol_ValidateEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_IsCsrValidDoneRequest();
}

void DfuProtocol_ApplyEventAction(const void * params)
{
    UNUSED(params);

    if(DfuProtocol_DoesUpgradeSupportSilentCommit())
    {
        DfuProtocol_SilentCommitOtaHostTransferCompleteResponse();
    }
    else
    {
        DfuProtocol_InteractiveOtaHostTransferCompleteResponse();
    }
}

void DfuProtocol_CommitEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_OtaHostCommitConfirm(UPGRADE_HOSTACTION_YES);
    DfuProtocol_DestroyDataInstance();
    disconnecting_client_task = DfuProtocol_GetClientTask();
    DfuProtocol_SetActiveClient(UPGRADE_CONTEXT_UNUSED);
    UpgradeTransportDisconnectRequest();
}

void DfuProtocol_TransportDisconnectEventAction(const void * params)
{
    UNUSED(params);
    DfuProtocol_DestroyDataInstance();
    disconnecting_client_task = DfuProtocol_GetClientTask();
    disconnecting_reason.generic = TRUE;
    UpgradeTransportDisconnectRequest();
}

void DfuProtocol_TransportDisconnectedEventAction(const void * params)
{
    dfu_protocol_transport_disconnected_params_t * transport_disconnect_params = (dfu_protocol_transport_disconnected_params_t *)params;

    if (transport_disconnect_params->is_upgrade_successful)
    {
        appLinkPolicyUpdatePowerTable(DfuProtocol_GetClientHandsetBtAddress());
        /* Reset the flag for AG as Upgrade Transport is disconnected */
        BtDevice_SetUpgradeTransportConnected(BtDevice_GetDeviceForBdAddr(DfuProtocol_GetClientHandsetBtAddress()), FALSE);
    }

    dfu_protocol_disconnect_reason_t reason = dfu_protocol_disconnect_reason_complete;

      if(disconnecting_reason.generic)
    {
        disconnecting_reason.generic = FALSE;
        reason = dfu_protocol_disconnect_reason_disconnected;
    }

    if(disconnecting_reason.aborting)
    {
        disconnecting_reason.aborting = FALSE;
        reason = dfu_protocol_disconnect_reason_aborted;
    }

    DfuProtocol_SendTransportDisconnectedIndToTask(disconnecting_client_task, reason);
    UpgradePermit(upgrade_perm_always_ask);
}
