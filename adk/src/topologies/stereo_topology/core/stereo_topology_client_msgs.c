/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Stereo Topology utility functions for sending messages to clients.
*/

#include "stereo_topology.h"
#include "stereo_topology_private.h"
#include "stereo_topology_client_msgs.h"

#include <logging.h>
#include <task_list.h>
#include <panic.h>

void StereoTopology_SendHandsetDisconnectedIndication(const handset_service_status_t status)
{
    DEBUG_LOG_VERBOSE("StereoTopology_SendHandsetDisconnectedIndication");
    MESSAGE_MAKE(msg, STEREO_TOPOLOGY_HANDSET_DISCONNECTED_IND_T);
    msg->status = status;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(StereoTopologyGetMessageClientTasks()), STEREO_TOPOLOGY_HANDSET_DISCONNECTED_IND, msg);
}


void StereoTopology_SendStopCfm(stereo_topology_status_t status)
{
    stereo_topology_task_data_t *stereo_taskdata = StereoTopologyGetTaskData();
    MAKE_STEREO_TOPOLOGY_MESSAGE(STEREO_TOPOLOGY_STOP_CFM);

    DEBUG_LOG_VERBOSE("StereoTopology_SendStopCfm status %u", status);

    MessageCancelAll(StereoTopologyGetTask(), STEREOTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP);

    message->status = status;
    MessageSend(stereo_taskdata->app_task, STEREO_TOPOLOGY_STOP_CFM, message);
}

