/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      simple topology procedure to indicate that a script designed to stop toology has started.
 */

#include "stereo_topology_private.h"
#include "stereo_topology_config.h"
#include "stereo_topology.h"
#include "stereo_topology_procedure_start_stop_script.h"
#include "stereo_topology_procedures.h"

#include <logging.h>
#include <panic.h>


void StereoTopology_ProcedureStartStopScriptStart(Task result_task,
                                                   procedure_start_cfm_func_t proc_start_cfm_fn,
                                                   procedure_complete_func_t proc_complete_fn,
                                                   Message goal_data);

void StereoTopology_ProcedureStartStopScriptCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn);

const procedure_fns_t stereo_proc_start_stop_script_fns = {
    StereoTopology_ProcedureStartStopScriptStart,
    StereoTopology_ProcedureStartStopScriptCancel,
};

void StereoTopology_ProcedureStartStopScriptStart(Task result_task,
                                                                    procedure_start_cfm_func_t proc_start_cfm_fn,
                                                                    procedure_complete_func_t proc_complete_fn,
                                                                    Message goal_data)
{

    DEBUG_LOG_VERBOSE("StereoTopology_ProcedureStartStopScriptStart");

    UNUSED(result_task);
    UNUSED(goal_data);

    /* procedure started synchronously so indicate success */
    proc_start_cfm_fn(stereo_topology_procedure_start_stop_script, procedure_result_success);

    Procedures_DelayedCompleteCfmCallback(proc_complete_fn,
                                          stereo_topology_procedure_start_stop_script,
                                          procedure_result_success);
}


void StereoTopology_ProcedureStartStopScriptCancel(procedure_cancel_cfm_func_t proc_cancel_cfm_fn)
{
    UNUSED(proc_cancel_cfm_fn);

    DEBUG_LOG_ERROR("StereoTopology_ProcedureStartStopScriptCancel. Should never be called.");

    Panic();
}

