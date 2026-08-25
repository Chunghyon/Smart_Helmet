/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure to handle handset connection.
*/

#include "stereo_topology_procedure_disconnect_handset.h"
#include "stereo_topology_client_msgs.h"
#include "stereo_topology_procedures.h"

#include <bt_device.h>
#include <device_list.h>
#include <device_properties.h>
#include <handset_service.h>
#include <connection_manager.h>

#include <logging.h>

#include <message.h>

static void stereoTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message);
static void StereoTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data);
static void StereoTopology_ProcedureDisconnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t stereo_proc_disconnect_handset_fns = {
    StereoTopology_ProcedureDisconnectHandsetStart,
    StereoTopology_ProcedureDisconnectHandsetCancel,
};

static void StereoTopology_ProcedureDisconnectLruHandsetStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data);
static void StereoTopology_ProcedureDisconnectLruHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t stereo_proc_disconnect_lru_handset_fns = {
    StereoTopology_ProcedureDisconnectLruHandsetStart,
    StereoTopology_ProcedureDisconnectLruHandsetCancel,
};

typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
} stereotop_proc_disconnect_handset_task_data_t;

stereotop_proc_disconnect_handset_task_data_t handsettop_proc_disconnect_handset = {stereoTopology_ProcDisconnectHandsetHandleMessage};

#define StereoTopProcDisconnectHandsetGetTaskData()     (&handsettop_proc_disconnect_handset)
#define StereoTopProcDisconnectHandsetGetTask()         (&handsettop_proc_disconnect_handset.task)

static void stereoTopology_ProcDisconnectHandsetResetProc(void)
{
    stereotop_proc_disconnect_handset_task_data_t *td = StereoTopProcDisconnectHandsetGetTaskData();
    td->complete_fn = NULL;
}

static void StereoTopology_ProcedureDisconnectLruHandsetStart(Task result_task,
                                                     procedure_start_cfm_func_t proc_start_cfm_fn,
                                                     procedure_complete_func_t proc_complete_fn,
                                                     Message goal_data)
{
    stereotop_proc_disconnect_handset_task_data_t *td = StereoTopProcDisconnectHandsetGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureDisconnectLruHandsetStart");

    HandsetService_DisconnectLruHandsetRequest(StereoTopProcDisconnectHandsetGetTask());

    /* start the procedure */
    td->complete_fn = proc_complete_fn;

    proc_start_cfm_fn(stereo_topology_procedure_disconnect_lru_handset, procedure_result_success);
}

static void StereoTopology_ProcedureDisconnectLruHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureDisconnectLruHandsetCancel");

    stereoTopology_ProcDisconnectHandsetResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        stereo_topology_procedure_disconnect_lru_handset,
                                        procedure_result_success);
}

static void StereoTopology_ProcedureDisconnectHandsetStart(Task result_task,
                                                            procedure_start_cfm_func_t proc_start_cfm_fn,
                                                            procedure_complete_func_t proc_complete_fn,
                                                            Message goal_data)
{
    stereotop_proc_disconnect_handset_task_data_t *td = StereoTopProcDisconnectHandsetGetTaskData();
    UNUSED(result_task);
    UNUSED(goal_data);

    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureDisconnectHandsetStart");

    HandsetService_DisconnectAll(StereoTopProcDisconnectHandsetGetTask());


    /* Request to Handset Services to disconnect Handsets, even it is disconnected.
    Handset Services sends a confirmation message if nothing to do. This
    message is used by topology to send STEREO_TOPOLOGY_HANDSET_DISCONNECTED_IND to
    apps sm. When the stereo has been requested to enter into User Pairing Mode, the apps state
    machine makes the decision of entering into pairing mode after reception of
    STEREO_TOPOLOGY_HANDSET_DISCONNECTED_IND. */


    /* start the procedure */
    td->complete_fn = proc_complete_fn;

    proc_start_cfm_fn(stereo_topology_procedure_disconnect_handset, procedure_result_success);
}

static void StereoTopology_ProcedureDisconnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureDisconnectHandsetCancel");

    stereoTopology_ProcDisconnectHandsetResetProc();
    Procedures_DelayedCancelCfmCallback(proc_cancel_cfm_fn,
                                        stereo_topology_procedure_disconnect_handset,
                                        procedure_result_success);
}

static void stereoTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm(const HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T *cfm)
{
    stereotop_proc_disconnect_handset_task_data_t* td = StereoTopProcDisconnectHandsetGetTaskData();

    DEBUG_LOG_VERBOSE("stereoTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm status enum:handset_service_status_t:%d", cfm->status);

    td->complete_fn(stereo_topology_procedure_disconnect_handset, procedure_result_success);
    stereoTopology_ProcDisconnectHandsetResetProc();
    StereoTopology_SendHandsetDisconnectedIndication(cfm->status);
}

static void stereoTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    stereotop_proc_disconnect_handset_task_data_t* td = StereoTopProcDisconnectHandsetGetTaskData();
    DEBUG_LOG("stereoTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm status enum:handset_service_status_t:%d", cfm->status);

    td->complete_fn(stereo_topology_procedure_disconnect_lru_handset, procedure_result_success);
    stereoTopology_ProcDisconnectHandsetResetProc();
    StereoTopology_SendHandsetDisconnectedIndication(cfm->status);
}

static void stereoTopology_ProcDisconnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    stereotop_proc_disconnect_handset_task_data_t* td = StereoTopProcDisconnectHandsetGetTaskData();

    UNUSED(task);

    if(td->complete_fn == NULL)
    {
        return;
    }
    switch (id)
    {
        case HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM:
            stereoTopology_ProcDisconnectHandsetHandleHandsetMpDisconnectAllCfm((const HANDSET_SERVICE_MP_DISCONNECT_ALL_CFM_T*)message);
            break;

        case HANDSET_SERVICE_DISCONNECT_CFM:
            stereoTopology_ProcDisconnectHandsetHandleHandsetDisconnectCfm((const HANDSET_SERVICE_DISCONNECT_CFM_T *)message);
            break;

        default:
            DEBUG_LOG_VERBOSE("stereoTopology_ProcDisconnectHandsetHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
    }
}

