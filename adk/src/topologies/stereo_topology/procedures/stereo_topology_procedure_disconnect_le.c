/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to handle BLE disconnection.
*/

#include "stereo_topology_procedure_disconnect_le.h"
#include "stereo_topology_client_msgs.h"
#include "stereo_topology_procedures.h"

#include <connection_manager.h>
#include <logging.h>
#include <message.h>

static void stereoTopology_ProcDisconnectLeHandleMessage(Task task, MessageId id, Message message);
static void StereoTopology_ProcedureDisconnectLeStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data);
static void StereoTopology_ProcedureDisconnectLeCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t stereo_proc_disconnect_le_fns = {
    StereoTopology_ProcedureDisconnectLeStart,
    StereoTopology_ProcedureDisconnectLeCancel,
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
} stereotop_proc_disconnect_le_task_data_t;

stereotop_proc_disconnect_le_task_data_t handsettop_proc_disconnect_le = {stereoTopology_ProcDisconnectLeHandleMessage};

#define StereoTopProcDisconnectLeGetTaskData()     (&handsettop_proc_disconnect_le)
#define StereoTopProcDisconnectLeGetTask()         (&handsettop_proc_disconnect_le.task)

static void stereoTopology_ProcDisconnectLeResetProc(void)
{
    stereotop_proc_disconnect_le_task_data_t *td = StereoTopProcDisconnectLeGetTaskData();
    td->complete_fn = NULL;
}

static void StereoTopology_ProcedureDisconnectLeStart(Task result_task,
                                                            procedure_start_cfm_func_t proc_start_cfm_fn,
                                                            procedure_complete_func_t proc_complete_fn,
                                                            Message goal_data)
{
    stereotop_proc_disconnect_le_task_data_t *td = StereoTopProcDisconnectLeGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureDisconnectLeStart");

    ConManagerDisconnectAllLeConnectionsRequest(StereoTopProcDisconnectLeGetTask());

    td->complete_fn = proc_complete_fn;

    proc_start_cfm_fn(stereo_topology_procedure_disconnect_le, procedure_result_success);
}

static void StereoTopology_ProcedureDisconnectLeCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureDisconnectLeCancel");

    stereoTopology_ProcDisconnectLeResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        stereo_topology_procedure_disconnect_le,
                                        procedure_result_success);
}

static void stereoTopology_ProcDisconnectLeHandleLeDisconnectCfm(void)
{
    stereotop_proc_disconnect_le_task_data_t* td = StereoTopProcDisconnectLeGetTaskData();

    DEBUG_LOG_VERBOSE("stereoTopology_ProcDisconnectLeHandleLeDisconnectCfm, all LE ACL (if present) is disconnected");

    td->complete_fn(stereo_topology_procedure_disconnect_le, procedure_result_success);
    stereoTopology_ProcDisconnectLeResetProc();
}

static void stereoTopology_ProcDisconnectLeHandleMessage(Task task, MessageId id, Message message)
{
    stereotop_proc_disconnect_le_task_data_t* td = StereoTopProcDisconnectLeGetTaskData();

    UNUSED(task);
    UNUSED(message);

    if(td->complete_fn == NULL)
    {
        DEBUG_LOG_VERBOSE("stereoTopology_ProcDisconnectLeHandleMessage: Ignore because the procedure already completed/or cancelled");
        return;
    }

    switch (id)
    {
        case CON_MANAGER_DISCONNECT_ALL_LE_CONNECTIONS_CFM:
            stereoTopology_ProcDisconnectLeHandleLeDisconnectCfm();
            break;

        default:
            DEBUG_LOG_VERBOSE("stereoTopology_ProcDisconnectLeHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
    }
}

