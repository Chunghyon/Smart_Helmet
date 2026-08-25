/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      TWS Topology utility functions for sending messages to clients.
*/

#include "tws_topology.h"
#include "tws_topology_private.h"
#include "tws_topology_client_msgs.h"

#include <logging.h>

#include <panic.h>

void TwsTopology_SendRoleChangedInd(tws_topology_role role)
{
    MAKE_TWS_TOPOLOGY_MESSAGE(TWS_TOPOLOGY_ROLE_CHANGED_IND);

    message->role = role;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(TwsTopologyGetMessageClientTasks()), TWS_TOPOLOGY_ROLE_CHANGED_IND, message);
}
