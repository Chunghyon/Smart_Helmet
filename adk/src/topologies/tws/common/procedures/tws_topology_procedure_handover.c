/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      This file contains the procedure and scripts to perform dynamic handover.
*/
#include "tws_topology_private.h"
#include "tws_topology_procedures.h"
#include "handover_profile.h"
#include "script_engine.h"
#include <logging.h>
#include <message.h>

#include "tws_topology_procedure_notify_role_change_clients.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedure_enable_connectable_handset.h"
#include "tws_topology_procedure_enable_le_connectable_handset.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_disconnect_le_connections.h"

void TwsTopology_ProcedureHandoverStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data);

void TwsTopology_ProcedureHandoverCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t proc_handover_fns = {
    TwsTopology_ProcedureHandoverStart,
    TwsTopology_ProcedureHandoverCancel,
};

void TwsTopology_ProcedureHandoverStart(Task result_task,
                                        procedure_start_cfm_func_t proc_start_cfm_fn,
                                        procedure_complete_func_t proc_complete_fn,
                                        Message goal_data)
{
    UNUSED(result_task);
    UNUSED(goal_data);

    handover_profile_status_t status;
    procedure_result_t result;

    DEBUG_LOG("TwsTopology_ProcedureHandoverStart() Started");

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(tws_topology_procedure_dynamic_handover, procedure_result_success);

    status = HandoverProfile_Handover();

    DEBUG_LOG("TwsTopology_ProcedureHandoverStart() enum:handover_profile_status_t:%d", status);

    /* Handle return status from procedure */
    switch (status)
    {
        case HANDOVER_PROFILE_STATUS_SUCCESS:
            result = procedure_result_success;
        break;

        case HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT:
        case HANDOVER_PROFILE_STATUS_HANDOVER_VETOED:
            result = procedure_result_timeout;
        break;

        default:
            result = procedure_result_failed;
        break;
    }

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn, tws_topology_procedure_dynamic_handover, result);
}

void TwsTopology_ProcedureHandoverCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    proc_cancel_cfm_fn(tws_topology_procedure_dynamic_handover, procedure_result_success);
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API
#define PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)
#else
#define PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)   ENTRY(proc_enable_le_connectable_handset_fns, PROC_ENABLE_LE_CONNECTABLE_PARAMS_DISABLE),
#endif

/* For LE handover, remove disconnection of LE connections from the procedures */

#define DYNAMIC_HANDOVER_PREPARE_SCRIPT(ENTRY) \
    ENTRY(proc_notify_role_change_clients_fns, PROC_NOTIFY_ROLE_CHANGE_CLIENTS_FORCE_NOTIFICATION), \
    ENTRY(proc_permit_bt_fns, PROC_PERMIT_BT_DISABLE),\
    ENTRY(proc_disconnect_le_connections_fns, NO_DATA)

#define DYNAMIC_HANDOVER_SCRIPT(ENTRY) \
    ENTRY(proc_disconnect_le_connections_fns, NO_DATA), \
    ENTRY(proc_handover_fns, NO_DATA), \
    ENTRY(proc_enable_connectable_handset_fns, PROC_ENABLE_CONNECTABLE_HANDSET_DATA_DISABLE),\
    PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)\
    ENTRY(proc_permit_bt_fns, PROC_PERMIT_BT_ENABLE)


DEFINE_TOPOLOGY_SCRIPT(dynamic_handover_prepare, DYNAMIC_HANDOVER_PREPARE_SCRIPT);
DEFINE_TOPOLOGY_SCRIPT(dynamic_handover, DYNAMIC_HANDOVER_SCRIPT);

#define DYNAMIC_HANDOVER_UNDO_PREPARE_SCRIPT(ENTRY) \
    ENTRY(proc_permit_bt_fns, PROC_PERMIT_BT_ENABLE), \
    ENTRY(proc_notify_role_change_clients_fns, PROC_NOTIFY_ROLE_CHANGE_CLIENTS_CANCEL_NOTIFICATION)
DEFINE_TOPOLOGY_SCRIPT(dynamic_handover_undo_prepare, DYNAMIC_HANDOVER_UNDO_PREPARE_SCRIPT);
