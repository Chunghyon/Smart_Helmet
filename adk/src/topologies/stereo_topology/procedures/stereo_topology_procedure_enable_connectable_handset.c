/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      File containing a procedure that controls whether a handset is 
            allowed to connect over BREDR.

            The procedure takes a parameter that decides whether connection is 
            to be enabled or disabled.

            \note Other procedures can block connection at a lower level, for 
            instance disabling Bluetooth completely. These functions have the 
            effect of suspending the ability of a handset to connect.
*/

#include "stereo_topology_procedure_enable_connectable_handset.h"
#include "stereo_topology_procedures.h"

#include <handset_service.h>

#include <logging.h>

#include <message.h>


/*! Parameter definition for connectable enable */
const ENABLE_CONNECTABLE_HANDSET_PARAMS_T stereo_topology_procedure_connectable_handset_enable = { .enable = TRUE };
/*! Parameter definition for connectable disable */
const ENABLE_CONNECTABLE_HANDSET_PARAMS_T stereo_topology_procedure_connectable_handset_disable = { .enable = FALSE };

static void stereoTopology_ProcEnableConnectableHandsetHandleMessage(Task task, MessageId id, Message message);
static void StereoTopology_ProcedureEnableConnectableHandsetStart(Task result_task,
                                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                                   procedure_complete_func_t proc_complete_fn,
                                                                   Message goal_data);

static void StereoTopology_ProcedureEnableConnectableHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);


typedef struct
{
    TaskData task;
    ENABLE_CONNECTABLE_HANDSET_PARAMS_T params;
} stereotop_proc_enable_connectable_handset_task_data_t;

const procedure_fns_t stereo_proc_enable_connectable_handset_fns =
{
    StereoTopology_ProcedureEnableConnectableHandsetStart,
    StereoTopology_ProcedureEnableConnectableHandsetCancel,
};

stereotop_proc_enable_connectable_handset_task_data_t stereotop_proc_enable_connectable_handset = {stereoTopology_ProcEnableConnectableHandsetHandleMessage};


#define StereoTopProcEnableConnectableHandsetGetTaskData()     (&stereotop_proc_enable_connectable_handset)
#define StereoTopProcEnableConnectableHandsetGetTask()         (&stereotop_proc_enable_connectable_handset.task)


static void StereoTopology_ProcedureEnableConnectableHandsetStart(Task result_task,
                                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                                   procedure_complete_func_t proc_complete_fn,
                                                                   Message goal_data)
{
    stereotop_proc_enable_connectable_handset_task_data_t* td = StereoTopProcEnableConnectableHandsetGetTaskData();
    ENABLE_CONNECTABLE_HANDSET_PARAMS_T* params = (ENABLE_CONNECTABLE_HANDSET_PARAMS_T*)goal_data;

    UNUSED(result_task);

    td->params = *params;

    if (params->enable)
    {
        DEBUG_LOG_VERBOSE("StereoTopology_ProcedureEnableConnectableHandsetStart ENABLE");

        HandsetService_ConnectableRequest(StereoTopProcEnableConnectableHandsetGetTask());
    }
    else
    {
        DEBUG_LOG_VERBOSE("StereoTopology_ProcedureEnableConnectableHandsetStart DISABLE");

        HandsetService_CancelConnectableRequest(StereoTopProcEnableConnectableHandsetGetTask());
    }

    proc_start_cfm_fn(stereo_topology_procedure_enable_connectable_handset, procedure_result_success);

    /* must use delayed cfm callback to prevent script engine recursion */
    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          stereo_topology_procedure_enable_connectable_handset,
                                          procedure_result_success);
}

static void StereoTopology_ProcedureEnableConnectableHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    stereotop_proc_enable_connectable_handset_task_data_t* td = StereoTopProcEnableConnectableHandsetGetTaskData();

    if (td->params.enable)
    {
        DEBUG_LOG_VERBOSE("StereoTopology_ProcedureEnableConnectableHandsetCancel cancel enable");

        HandsetService_CancelConnectableRequest(StereoTopProcEnableConnectableHandsetGetTask());
    }
    else
    {
        DEBUG_LOG_VERBOSE("StereoTopology_ProcedureEnableConnectableHandsetCancel cancel disable");

        HandsetService_ConnectableRequest(StereoTopProcEnableConnectableHandsetGetTask());
    }

    /* must use delayed cfm callback to prevent script engine recursion */
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        stereo_topology_procedure_enable_connectable_handset,
                                        procedure_result_success);

}

static void stereoTopology_ProcEnableConnectableHandsetHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    UNUSED(id);
}

