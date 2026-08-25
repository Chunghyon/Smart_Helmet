/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      TWS Topology goal handling.
*/

#include "tws_topology.h"
#include "tws_topology_private.h"

#include "tws_topology_goals.h"
#include "tws_topology_config.h"

#include "tws_topology_procedures.h"
#include "tws_topology_procedure_acting_primary_role.h"
#include "tws_topology_procedure_find_role.h"
#include "tws_topology_procedure_no_role_idle.h"
#include "tws_topology_procedure_pair_peer.h"
#include "tws_topology_procedure_pri_connect_peer_profiles.h"
#include "tws_topology_procedure_enable_connectable_peer.h"
#include "tws_topology_procedure_primary_role.h"
#include "tws_topology_procedure_sec_connect_peer.h"
#include "tws_topology_procedure_secondary_role.h"
#include "tws_topology_procedure_handover.h"

#include <logging.h>

#include <message.h>
#include <panic.h>
#include <watchdog.h>

#pragma unitsuppress Unused

LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_procedure)

/*! \brief This table defines each goal supported by the topology.

    Each entry links the goal set by the SM with the procedure required to achieve it.

    Entries also provide the configuration for how the goal should be handled, identifying the following
    characteristics:
     - is the goal exclusive with another, requiring the exclusive goal to be cancelled
     - the contention policy of the goal
        - can cancel other goals
        - can execute concurrently with other goals
        - must wait for other goal completion
     - function pointers to the procedure or script to achieve the goal

    Not all goals require configuration of all parameters so utility macros are used to define a
    goal and set default parameters for unrequired fields.
*/
const goal_entry_t goals[] =
{
    SCRIPT_GOAL(tws_topology_goal_pair_peer, tws_topology_procedure_pair_peer_script,
                &pair_peer_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_find_role, tws_topology_procedure_find_role,
                &find_role_with_timeout_script, tws_topology_goal_none),

    GOAL(tws_topology_goal_secondary_connect_peer, tws_topology_procedure_sec_connect_peer,
         &proc_sec_connect_peer_fns, tws_topology_goal_none),

    GOAL(tws_topology_goal_primary_connect_peer_profiles, tws_topology_procedure_pri_connect_peer_profiles,
         &proc_pri_connect_peer_profiles_fns, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_primary_connectable_peer, tws_topology_procedure_enable_connectable_peer,
                &enable_connectable_peer_fast_script, tws_topology_goal_none),

    SCRIPT_GOAL_CANCEL(tws_topology_goal_no_role_idle, tws_topology_procedure_no_role_idle,
                &no_role_idle_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_become_primary, tws_topology_procedure_become_primary,
                &primary_role_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_become_secondary, tws_topology_procedure_become_secondary,
                &secondary_role_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_become_acting_primary, tws_topology_procedure_become_acting_primary,
                &acting_primary_role_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_dynamic_handover_prepare,
                tws_topology_procedure_dynamic_handover_prepare,
                &dynamic_handover_prepare_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_dynamic_handover,
                tws_topology_procedure_dynamic_handover,
                &dynamic_handover_script, tws_topology_goal_none),

    SCRIPT_GOAL(tws_topology_goal_dynamic_handover_undo_prepare,
                tws_topology_procedure_dynamic_handover_undo_prepare,
                &dynamic_handover_undo_prepare_script, tws_topology_goal_none),
};


/******************************************************************************
 * Callbacks for procedure confirmations
 *****************************************************************************/

/*! \brief Handle confirmation of procedure start.
    
    Provided as a callback to procedures.
*/
static void twsTopology_GoalProcStartCfm(procedure_id proc, procedure_result_t result)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    tws_topology_goal_id tws_goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);

    DEBUG_LOG("twsTopology_GoalProcStartCfm enum:tws_topology_procedure:%d for enum:tws_topology_goal_id:%d", proc, tws_goal);

    UNUSED(result);
}

/*! \brief Handle completion of a goal.
  
    Provided as a callback for procedures to use to indicate goal completion.

    Remove the goal and associated procedure from the lists tracking
    active ones.
*/
static void twsTopology_GoalProcComplete(procedure_id proc, procedure_result_t result)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    goal_id completed_goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);

    DEBUG_LOG("twsTopology_GoalProcComplete enum:tws_topology_procedure:%d for enum:tws_topology_goal_id:%d", proc, completed_goal);

    /* clear the goal from list of active goals, this may cause further
     * goals to be delivered from the pending_goal_queue_task */
    GoalsEngine_ClearGoal(td->goal_set, completed_goal);

    twsTopology_SmGoalComplete(TwsTopology_GetSm(), completed_goal, result);
}

/*! \brief Handle confirmation of goal cancellation.

    Provided as a callback for procedures to use to indicate cancellation has
    been completed.
*/
static void twsTopology_GoalProcCancelCfm(procedure_id proc, procedure_result_t result)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    goal_id goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);

    DEBUG_LOG("twsTopology_GoalProcCancelCfm enum:tws_topology_procedure:%d for enum:tws_topology_goal_id:%d", proc, goal);

    UNUSED(result);

    GoalsEngine_ClearGoal(td->goal_set, goal);
}

/*! \brief Determine if a goal is currently being executed. */
bool TwsTopology_IsGoalActive(tws_topology_goal_id goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return (GoalsEngine_IsGoalActive(td->goal_set, goal));
}

/*! \brief Determine if a goal is currently being queued. */
bool TwsTopology_IsGoalQueued(tws_topology_goal_id goal)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return (GoalsEngine_IsGoalQueued(td->goal_set, goal));
}

/*! \brief Check if there are any pending goals. */
bool TwsTopology_IsAnyGoalPending(void)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();
    return (GoalsEngine_IsAnyGoalPending(td->goal_set));
}

/*! \brief Given a new goal decision from the SM, find the goal and attempt to start it. */
void TwsTopology_HandleGoalDecision(Task task, MessageId id, Message message)
{
    twsTopologyTaskData* td = TwsTopologyGetTaskData();

    PanicFalse(id > tws_topology_goal_none && id < tws_topology_goals_end);

    UNUSED(message);

    DEBUG_LOG_INFO("TwsTopology_HandleGoalDecision enum:tws_topology_goal_id:%d", id);
    GoalsEngine_ActivateGoal(td->goal_set, id, task, id, NULL, 0);
}

void TwsTopology_GoalsInit(void)
{
    twsTopologyTaskData *td = TwsTopologyGetTaskData();
    goal_set_init_params_t init_params;

    td->pending_goal_queue_task.handler = TwsTopology_HandleGoalDecision;

    memset(&init_params, 0, sizeof(init_params));
    init_params.goals = goals;
    init_params.goals_count = ARRAY_DIM(goals);
    init_params.pending_goal_queue_task = &td->pending_goal_queue_task;

    init_params.proc_result_task = TwsTopologyGetTask();
    init_params.proc_start_cfm_fn = twsTopology_GoalProcStartCfm;
    init_params.proc_cancel_cfm_fn = twsTopology_GoalProcCancelCfm;
    init_params.proc_complete_cfm_fn = twsTopology_GoalProcComplete;

    td->goal_set = GoalsEngine_CreateGoalSet(&init_params);
}
