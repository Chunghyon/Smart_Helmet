/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Procedure to stop handset reconnections.
*/

#include "tws_topology_procedure_stop_handset_reconnect.h"

#include <handset_service.h>

#include <logging.h>

#include <message.h>
#include <panic.h>



static void TwsTopology_ProcedureStopHandsetReconnectStart(Task,
                                                           procedure_start_cfm_func_t,
                                                           procedure_complete_func_t,
                                                           Message);

static void TwsTopology_ProcedureStopHandsetReconnectCancel(procedure_cancel_cfm_func_t);

static void TwsTopology_ProcedureStopHandsetReconnectHandler(Task, MessageId, Message);

const TaskData proc_stop_handset_reconnect_task = {
    .handler = TwsTopology_ProcedureStopHandsetReconnectHandler
};

static procedure_complete_func_t proc_stop_handset_reconnect_complete_fn;

const procedure_fns_t proc_stop_handset_reconnect_fns = {
    TwsTopology_ProcedureStopHandsetReconnectStart,
    TwsTopology_ProcedureStopHandsetReconnectCancel,
};

static void TwsTopology_ProcedureStopHandsetReconnectStart(Task result_task,
                                                    procedure_start_cfm_func_t proc_start_cfm_fn,
                                                    procedure_complete_func_t proc_complete_fn,
                                                    Message goal_data)
{
    DEBUG_LOG_FN_ENTRY("TwsTopology_ProcedureStopHandsetReconnectStart");

    UNUSED(result_task);
    UNUSED(goal_data);

    HandsetService_StopReconnect((Task)&proc_stop_handset_reconnect_task);
    proc_stop_handset_reconnect_complete_fn = proc_complete_fn;
    proc_start_cfm_fn(tws_topology_procedure_stop_handset_reconnect,
                      procedure_result_success);
}

static void TwsTopology_ProcedureStopHandsetReconnectCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_FN_ENTRY("TwsTopology_ProcedureStopHandsetReconnectCancel");

    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        tws_topology_procedure_stop_handset_reconnect,
                                        procedure_result_success);
}

static void twsTopology_ProcedureStopHandsetReconnectHandleConnectStopCfm(const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T *cfm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_ProcedureStopHandsetReconnectHandleConnectStopCfm "
                       "enum:handset_service_status_t:%d", cfm->status);
    Procedures_DelayedCompleteCfmCallback(proc_stop_handset_reconnect_complete_fn,
                                          tws_topology_procedure_stop_handset_reconnect,
                                          procedure_result_success);
}

static void TwsTopology_ProcedureStopHandsetReconnectHandler(Task t, MessageId id, Message m)
{
    UNUSED(t);

    switch (id)
    {
        case HANDSET_SERVICE_MP_CONNECT_STOP_CFM:
            twsTopology_ProcedureStopHandsetReconnectHandleConnectStopCfm(m);
        break;

        default:

        break;
    }
}

