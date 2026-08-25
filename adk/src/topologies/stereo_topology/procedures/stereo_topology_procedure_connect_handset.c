/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Procedure for stereo to connect BR/EDR ACL to Handset.

\note       Whilst the procedure is running, if audio streaming is started the handset 
            connection is stopped but the procedure continues to be active.
            If the streaming stops within PROC_CONNECT_HANDSET_STREAMING_STOP_TIMEOUT_MS,
            the handset connection is resumed.
            If the streaming continues beyond 30s, the procedure completes returning failure status.
*/

#include "stereo_topology_procedure_connect_handset.h"
#include "stereo_topology_config.h"
#include "core/stereo_topology_rules.h"
#include "procedures.h"
#include "stereo_topology_procedures.h"

#include <av.h>
#include <handset_service.h>
#include <handset_service_config.h>
#include <connection_manager.h>
#include <logging.h>

#include <message.h>
#include <panic.h>

/*! Parameter definition for link loss reconnection */
const CONNECT_HANDSET_PARAMS_T stereo_topology_procedure_connect_handset_link_loss = { .link_loss = TRUE };

/*! Parameter definition for normal handset connection */
const CONNECT_HANDSET_PARAMS_T stereo_topology_procedure_connect_handset_normal = { .link_loss = FALSE };

void StereoTopology_ProcedureConnectHandsetStart(Task result_task,
                                                  procedure_start_cfm_func_t proc_start_cfm_fn,
                                                  procedure_complete_func_t proc_complete_fn,
                                                  Message goal_data);
static void StereoTopology_ProcedureConnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t stereo_proc_connect_handset_fns = {
    StereoTopology_ProcedureConnectHandsetStart,
    StereoTopology_ProcedureConnectHandsetCancel,
};


typedef struct
{
    TaskData task;
    procedure_complete_func_t complete_fn;
    procedure_cancel_cfm_func_t cancel_fn;
} stereotop_proc_connect_handset_task_data_t;

stereotop_proc_connect_handset_task_data_t stereotop_proc_connect_handset;

#define StereoTopProcConnectHandsetGetTaskData() (&stereotop_proc_connect_handset)
#define StereoTopProcConnectHandsetGetTask()     (&stereotop_proc_connect_handset.task)

static void stereoTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message);

stereotop_proc_connect_handset_task_data_t stereotop_proc_connect_handset = {stereoTopology_ProcConnectHandsetHandleMessage};

static void stereoTopology_ProcConnectHandsetResetTaskData(void)
{
    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();

    memset(td, 0, sizeof(*td));
}

static void stereoTopology_ProcConnectHandsetResetCompleteFunc(void)
{
    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();
    td->complete_fn = NULL;

}

static void stereoTopology_ProcConnectHandsetResetCancelFunc(void)
{
    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();
    td->cancel_fn = NULL;

}

void StereoTopology_ProcedureConnectHandsetStart(Task result_task,
                                                  procedure_start_cfm_func_t proc_start_cfm_fn,
                                                  procedure_complete_func_t proc_complete_fn,
                                                  Message goal_data)
{
    UNUSED(result_task);

    CONNECT_HANDSET_PARAMS_T* params = (CONNECT_HANDSET_PARAMS_T*)goal_data;

    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();

    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureConnectHandsetStart link_loss=%d", params->link_loss);

    stereoTopology_ProcConnectHandsetResetTaskData();

    td->task.handler = stereoTopology_ProcConnectHandsetHandleMessage;
    td->complete_fn = proc_complete_fn;

    /* start the procedure */
    bdaddr handset_addr;
    if (appDeviceGetHandsetBdAddr(&handset_addr))
    {
        if (params->link_loss)
        {
            HandsetService_ReconnectLinkLossRequest(StereoTopProcConnectHandsetGetTask());
        }
        else
        {
            HandsetService_ReconnectRequest(StereoTopProcConnectHandsetGetTask());
        }

        proc_start_cfm_fn(stereo_topology_procedure_connect_handset, procedure_result_success);
    }
    else
    {
        DEBUG_LOG_ERROR("StereoTopology_ProcedureConnectHandsetStart shouldn't be called with no paired handset");
        Panic();
    }
}

void StereoTopology_ProcedureConnectHandsetCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();
    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureConnectHandsetCancel ");

    td->complete_fn = NULL;
    td->cancel_fn = proc_cancel_cfm_fn;

    HandsetService_StopReconnect(StereoTopProcConnectHandsetGetTask());
}

static void stereoTopology_ProcConnectHandsetHandleHandsetMpConnectCfm(const HANDSET_SERVICE_MP_CONNECT_CFM_T *cfm)
{
    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();

    DEBUG_LOG_VERBOSE("stereoTopology_ProcConnectHandsetHandleHandsetMpConnectCfm status enum:handset_service_status_t:%d", cfm->status);

    /* Topology shall rely on handset service's responsibilty to establish handset connection
     * and notify CONNECT_CFM after what constitues to be connection as per handset service.
     * Topology shall not inspect and match b/w connected and requested profiles.
     * Reason: All requested profiles may not have to be connected for service to treat as
     * handset connection success */
    if (cfm->status != handset_service_status_cancelled)
    {
        /* The procedure could be finished by either HANDSET_SERVICE_MP_CONNECT_CFM or
         * HANDSET_SERVICE_MP_CONNECT_STOP_CFM but there is no guarantee which order they
         * arrive in so it has to handle them arriving in either order. */
        if(cfm->status == handset_service_status_success)
        {
            if (td->complete_fn)
            {
                td->complete_fn(stereo_topology_procedure_connect_handset, procedure_result_success);
            }
            else if (td->cancel_fn)
            {
                td->cancel_fn(stereo_topology_procedure_connect_handset, procedure_result_success);
            }
        }
        else
        {
            if (td->complete_fn)
            {
                td->complete_fn(stereo_topology_procedure_connect_handset, procedure_result_failed);
            }
            else if (td->cancel_fn)
            {
                td->cancel_fn(stereo_topology_procedure_connect_handset, procedure_result_success);
                stereoTopology_ProcConnectHandsetResetCancelFunc();
            }
        }
        stereoTopology_ProcConnectHandsetResetCompleteFunc();
    }
    else
    {
        DEBUG_LOG("stereoTopology_ProcConnectHandsetHandleHandsetMpConnectCfm, connect procedure has been cancelled");
    }
}

static void stereoTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm(const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T* cfm)
{
    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();

    DEBUG_LOG("stereoTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm status enum:handset_service_status_t:%d", cfm->status);

    /* If the procedure was cancelled, let the topology know and tidy up
       this procedure. If not cancelled, wait for the
       HANDSET_SERVICE_MP_CONNECT_CFM instead. */
    if (td->cancel_fn)
    {
        td->cancel_fn(stereo_topology_procedure_connect_handset, procedure_result_success);
        stereoTopology_ProcConnectHandsetResetCancelFunc();
    }
}

static void stereoTopology_ProcConnectHandsetHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    stereotop_proc_connect_handset_task_data_t* td = StereoTopProcConnectHandsetGetTaskData();

    /* ignore any delivered messages if no longer active */
    if ((td->complete_fn == NULL) && (td->cancel_fn == NULL))
    {
        return;
    }

    switch (id)
    {
        case HANDSET_SERVICE_MP_CONNECT_CFM:
            stereoTopology_ProcConnectHandsetHandleHandsetMpConnectCfm((const HANDSET_SERVICE_MP_CONNECT_CFM_T *)message);
            break;

        case HANDSET_SERVICE_MP_CONNECT_STOP_CFM:
            stereoTopology_ProcConnectHandsetHandleHandsetMpConnectStopCfm((const HANDSET_SERVICE_MP_CONNECT_STOP_CFM_T *)message);
            break;

        default:
            DEBUG_LOG_VERBOSE("stereoTopology_ProcConnectHandsetHandleMessage unhandled id MESSAGE:0x%x", id);
            break;
    }
}

