#ifdef INCLUDE_AMA
/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides TWS support in the accessory domain
*/

#include "ama.h"
#include "ama_tws.h"

#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stdio.h>

#include "bt_device.h"
#include "ama_debug.h"
#include "ama_transport.h"


typedef struct
{
    Task serverTask;
    int32 min_reconnection_delay;
    tws_topology_role current_role;
} ama_tws_data_t;

static ama_tws_data_t ama_tws_data =
{
    .serverTask = NULL,
    .min_reconnection_delay = D_SEC(2),
    .current_role = tws_topology_role_none
};

static bool pending_role_change_expected = FALSE;

static void amaTws_SendRoleChangePrepareResponse(void)
{
    DEBUG_LOG("ama_tws_SendRoleChangePrepareResponse");
    MessageSend(PanicNull(ama_tws_data.serverTask), TWS_ROLE_CHANGE_PREPARATION_CFM, NULL);
}

void AmaTws_HandleLocalDisconnectionCompleted(void)
{
    DEBUG_LOG("AmaTws_HandleLocalDisconnectionCompleted");
    amaTws_SendRoleChangePrepareResponse();
}

static void amaTws_AllowReconnections(void)
{
    DEBUG_LOG("amaTws_AllowReconnections current_role=enum:tws_topology_role:%d", ama_tws_data.current_role);
    if(ama_tws_data.current_role == tws_topology_role_primary)
    {
        AmaTransport_AllowConnections();
    }
}

static void amaTws_BlockReconnections(void)
{
    DEBUG_LOG("amaTws_BlockReconnections current_role=enum:tws_topology_role:%d", ama_tws_data.current_role);
    AmaTransport_BlockConnections();
}

bool AmaTws_IsDisconnectRequired(void)
{
    bool disconnect_required = (ama_tws_data.current_role == tws_topology_role_primary);
    DEBUG_LOG("AmaTws_IsDisconnectRequired: disconnect_required = %u", disconnect_required);
    return disconnect_required;
}

void AmaTws_DisconnectIfRequired(ama_local_disconnect_reason_t reason)
{
    DEBUG_LOG("AmaTws_DisconnectIfRequired");
    if(AmaTws_IsDisconnectRequired())
    {
       AmaTransport_RequestDisconnect(reason);
    }
}

static void amaTws_SendRoleChangeRequestResponse(void)
{
    DEBUG_LOG("amaTws_SendRoleChangeRequestResponse");
    MAKE_TWS_ROLE_CHANGE_ACCEPTANCE_MESSAGE(TWS_ROLE_CHANGE_ACCEPTANCE_CFM);
    message->role_change_accepted = TRUE;

    MessageSend(PanicNull(ama_tws_data.serverTask), TWS_ROLE_CHANGE_ACCEPTANCE_CFM, message);
}

static void amaTws_Initialise(Task server, int32_t reconnect_delay)
{
    DEBUG_LOG("amaTws_Initialise server, reconnect delay =%d",reconnect_delay);
    ama_tws_data.min_reconnection_delay = reconnect_delay;
    ama_tws_data.serverTask = server;
}

static void amaTws_RoleChangeIndication(tws_topology_role new_role)
{
    DEBUG_LOG_FN_ENTRY("amaTws_RoleChangeIndication role=enum:tws_topology_role:%d expected %d", new_role, pending_role_change_expected);

    tws_topology_role old_role = ama_tws_data.current_role;
    ama_tws_data.current_role = new_role;

    AmaTransport_SetProfileDisconnectRequired(new_role == tws_topology_role_primary);

    if(old_role == tws_topology_role_primary && new_role != tws_topology_role_primary)
    {
        amaTws_BlockReconnections();
    }
    else if(old_role != tws_topology_role_primary && new_role == tws_topology_role_primary)
    {
        amaTws_AllowReconnections();
    }

    if(pending_role_change_expected &&
      ((old_role != tws_topology_role_primary && new_role == tws_topology_role_primary) ||
       (old_role != tws_topology_role_secondary && new_role == tws_topology_role_secondary)))
    {
        pending_role_change_expected = FALSE;
        AmaTws_RoleChangedIndication(new_role);
    }
}

static void amaTws_ProposeRoleChange(void)
{
    DEBUG_LOG_FN_ENTRY("amaTws_ProposeRoleChange");
    amaTws_SendRoleChangeRequestResponse();
}

static void amaTws_ForceRoleChange(void)
{
    DEBUG_LOG_FN_ENTRY("amaTws_ForceRoleChange");
    pending_role_change_expected = TRUE;
    AmaTws_DisconnectIfRequired(ama_local_disconnect_reason_forced);
}

static void amaTws_PrepareRoleChange(void)
{
    DEBUG_LOG_FN_ENTRY("amaTws_PrepareRoleChange");
    AmaTws_DisconnectIfRequired(ama_local_disconnect_reason_normal);
    pending_role_change_expected = TRUE;
    amaTws_SendRoleChangePrepareResponse();
}

static void amaTws_CancelRoleChange(void)
{
    DEBUG_LOG_FN_ENTRY("amaTws_CancelRoleChange");
    pending_role_change_expected = FALSE;
    amaTws_AllowReconnections();
}



TWS_ROLE_CHANGE_CLIENT_REGISTRATION_MAKE(amaTws,
                                         amaTws_Initialise,
                                         amaTws_RoleChangeIndication,
                                         amaTws_ProposeRoleChange,
                                         amaTws_ForceRoleChange,
                                         amaTws_PrepareRoleChange,
                                         amaTws_CancelRoleChange);

#ifdef HOSTED_TEST_ENVIRONMENT

const role_change_client_callback_t * ama_tws_GetClientCallbacks(void)
{
    return &role_change_client_registrations_ama_tws;
}

void Ama_tws_Reset(void)
{
    memset(&ama_tws_data, 0, sizeof(ama_tws_data));
}

#endif /* HOSTED_TEST_ENVIRONMENT */
#endif /* INCLUDE_AMA */
