/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      simple topology procedure to send a message to the topology task.
*/

#include "stereo_topology_private.h"
#include "stereo_topology_config.h"
#include "stereo_topology.h"
#include "stereo_topology_procedure_send_topology_message.h"
#include "stereo_topology_procedures.h"

#include <logging.h>
#include <message.h>
#include <panic.h>


void StereoTopology_ProcedureSendTopologyMessageStart(Task result_task,
                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                   procedure_complete_func_t proc_complete_fn,
                                                   Message goal_data);

void StereoTopology_ProcedureSendTopologyMessageCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t stereo_proc_send_topology_message_fns = {
    StereoTopology_ProcedureSendTopologyMessageStart,
    StereoTopology_ProcedureSendTopologyMessageCancel,
};

void StereoTopology_ProcedureSendTopologyMessageStart(Task result_task,
                                                      procedure_start_cfm_func_t proc_start_cfm_fn,
                                                      procedure_complete_func_t proc_complete_fn,
                                                      Message goal_data)
{
    STEREOTOP_PROC_SEND_MESSAGE_PARAMS_T *params = (STEREOTOP_PROC_SEND_MESSAGE_PARAMS_T*)goal_data;
    void *message = NULL;

    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureSendTopologyMessageStart MESSAGE:0x%x Msg:%p",
                      params->id, params->message);

    UNUSED(result_task);

    if (params->message && params->message_size)
    {
        message = PanicUnlessMalloc(params->message_size);
        memcpy(message, params->message, params->message_size);
    }

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(stereo_topology_procedure_send_message_to_topology, procedure_result_success);

    MessageSend(StereoTopologyGetTask(), params->id, message);

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          stereo_topology_procedure_send_message_to_topology,
                                          procedure_result_success);

}

void StereoTopology_ProcedureSendTopologyMessageCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    UNUSED(proc_cancel_cfm_fn);

    DEBUG_LOG_ERROR("StereoTopology_ProcedureSendTopologyMessageCancel. Should never be called.");

    Panic();
}

