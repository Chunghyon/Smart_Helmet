/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      STEREO Topology goal handling.
*/

#include "stereo_topology.h"
#include "stereo_topology_private.h"

#include "stereo_topology_rules.h"

#include "stereo_topology_goals.h"
#include "stereo_topology_rule_events.h"
#include "stereo_topology_procedures.h"

#include "stereo_topology_procedure_allow_handset_connect.h"
#include "stereo_topology_procedure_connect_handset.h"
#include "stereo_topology_procedure_enable_connectable_handset.h"
#include "stereo_topology_procedure_disconnect_handset.h"
#include "stereo_topology_procedure_allow_le_connection.h"
#include "stereo_topology_procedure_system_stop.h"

#include <logging.h>

#include <message.h>
#include <panic.h>

#pragma unitsuppress Unused


/*! \brief This table defines each goal supported by the topology.

    Each entry links the goal set by a topology rule decision with the procedure required to achieve it.

    Entries also provide the configuration for how the goal should be handled, identifying the following
    characteristics:
     - is the goal exclusive with another, requiring the exclusive goal to be cancelled
     - the contention policy of the goal
        - can cancel other goals
        - can execute concurrently with other goals
        - must wait for other goal completion
     - function pointers to the procedure or script to achieve the goal
     - events to generate back into the role rules engine following goal completion
        - success, failure or timeout are supported
     
    Not all goals require configuration of all parameters so utility macros are used to define a
    goal and set default parameters for unrequired fields.
*/
const goal_entry_t stereogoals[] =
{
    GOAL(stereo_topology_goal_connect_handset, stereo_topology_procedure_connect_handset,
                          &stereo_proc_connect_handset_fns, stereo_topology_goal_disconnect_handset),

    GOAL(stereo_topology_goal_disconnect_handset, stereo_topology_procedure_disconnect_handset,
         &stereo_proc_disconnect_handset_fns, stereo_topology_goal_connect_handset),

    GOAL_WITH_CONCURRENCY(stereo_topology_goal_connectable_handset, stereo_topology_procedure_enable_connectable_handset,
                          &stereo_proc_enable_connectable_handset_fns, stereo_topology_goal_none,
                          CONCURRENT_GOALS_INIT(stereo_topology_goal_allow_handset_connect,
                                                stereo_topology_goal_allow_le_connection)),

    GOAL_WITH_CONCURRENCY(stereo_topology_goal_allow_handset_connect, stereo_topology_procedure_allow_handset_connection,
                          &stereo_proc_allow_handset_connect_fns, stereo_topology_goal_none,
                          CONCURRENT_GOALS_INIT(stereo_topology_goal_connectable_handset,
                                                stereo_topology_goal_allow_le_connection)),

    GOAL_WITH_CONCURRENCY(stereo_topology_goal_allow_le_connection, stereo_topology_procedure_allow_le_connection,
                          &stereo_proc_allow_le_connection_fns, stereo_topology_goal_none,
                          CONCURRENT_GOALS_INIT(stereo_topology_goal_connectable_handset,
                                                stereo_topology_goal_allow_handset_connect)),


    SCRIPT_GOAL_CANCEL(stereo_topology_goal_system_stop, stereo_topology_procedure_system_stop,
                       &stereo_system_stop_script, stereo_topology_goal_none),

    GOAL(stereo_topology_goal_disconnect_lru_handset, stereo_topology_procedure_disconnect_lru_handset,
         &stereo_proc_disconnect_lru_handset_fns, stereo_topology_goal_connect_handset),
};

/******************************************************************************
 * Callbacks for procedure confirmations
 *****************************************************************************/

/*! \brief Handle confirmation of procedure start.
    
    Provided as a callback to procedures.
*/
static void stereoTopology_GoalProcStartCfm(procedure_id proc, procedure_result_t result)
{
    DEBUG_LOG_VERBOSE("stereoTopology_GoalProcStartCfm proc 0x%x", proc);

    UNUSED(result);
}

/*! \brief Handle completion of a goal.
  
    Provided as a callback for procedures to use to indicate goal completion.

    Remove the goal and associated procedure from the lists tracking
    active ones.
    May generate events into the rules engine based on the completion
    result of the goal.
*/
static void stereoTopology_GoalProcComplete(procedure_id proc, procedure_result_t result)
{
    stereo_topology_task_data_t* td = StereoTopologyGetTaskData();
    goal_id completed_goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);
    rule_events_t complete_event = GoalsEngine_GetGoalCompleteEvent(td->goal_set, completed_goal, result);

    DEBUG_LOG_VERBOSE("stereoTopology_GoalProcComplete proc 0x%x for goal %d", proc, completed_goal);

    /* If the goal STEREOTOP_GOAL_CONNECTABLE_HANDSET has completed successfully, then
       transition from STEREO_TOPOLOGY_STATE_STARTING to STEREO_TOPOLOGY_STATE_STARTED*/
    if(StereoTopology_IsStateStarting() && (completed_goal == stereo_topology_goal_allow_handset_connect))
    {
        StereoTopology_SetState(STEREO_TOPOLOGY_STATE_STARTED);
    }

    if (complete_event)
    {
        DEBUG_LOG_VERBOSE("stereoTopology_GoalProcComplete generating event 0x%llx", complete_event);
        StereoTopologyRules_SetEvent(complete_event);
    }

    /* clear the goal from list of active goals, this may cause further
     * goals to be delivered from the pending_goal_queue_task */
    GoalsEngine_ClearGoal(td->goal_set, completed_goal);
}


/*! \brief Handle confirmation of goal cancellation.

    Provided as a callback for procedures to use to indicate cancellation has
    been completed.
*/
static void stereoTopology_GoalProcCancelCfm(procedure_id proc, procedure_result_t result)
{
    stereo_topology_task_data_t* td = StereoTopologyGetTaskData();
    goal_id goal = GoalsEngine_FindGoalForProcedure(td->goal_set, proc);

    DEBUG_LOG_VERBOSE("stereoTopology_GoalProcCancelCfm proc 0x%x", proc);

    UNUSED(result);

    GoalsEngine_ClearGoal(td->goal_set, goal);
}


/******************************************************************************
 * Handlers for converting rules decisions to goals
 *****************************************************************************/

/*! \brief Determine if a goal is currently being executed. */
bool StereoTopology_IsGoalActive(stereo_topology_goal_id_t goal)
{
    stereo_topology_task_data_t* td = StereoTopologyGetTaskData();
    return (GoalsEngine_IsGoalActive(td->goal_set, goal));
}


/*! \brief Check if there are any pending goals. */
bool StereoTopology_IsAnyGoalPending(void)
{
    stereo_topology_task_data_t* td = StereoTopologyGetTaskData();
    return (GoalsEngine_IsAnyGoalPending(td->goal_set));
}


/*! \brief Given a new goal decision from a rules engine, find the goal and attempt to start it. */
void StereoTopology_HandleGoalDecision(Task task, MessageId id, Message message)
{
    stereo_topology_task_data_t* td = StereoTopologyGetTaskData();

    DEBUG_LOG_INFO("StereoTopology_HandleGoalDecision id MESSAGE:stereo_topology_goals:0x%x , stereo_topology_sm_t:0x%x", id, StereoTopology_GetState());

    switch (id)
    {
        case STEREOTOP_GOAL_CONNECTABLE_HANDSET:
            if (StereoTopology_IsStateStarting())
            {
                GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_connectable_handset, task, id, message, sizeof(STEREOTOP_GOAL_CONNECTABLE_HANDSET_T));
            }
            break;

        case STEREOTOP_GOAL_CONNECT_HANDSET:
            if (StereoTopology_IsStateStarted())
            {
                GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_connect_handset, task, id, message, sizeof(STEREOTOP_GOAL_CONNECT_HANDSET_T));
            }
            break;

        case STEREOTOP_GOAL_ALLOW_HANDSET_CONNECT:
            if (StereoTopology_IsStateStarting())
            {
                GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_allow_handset_connect, task, id, message, sizeof(STEREOTOP_GOAL_ALLOW_HANDSET_CONNECT_T));
            }
            break;

        case STEREOTOP_GOAL_DISCONNECT_HANDSET:
            if (StereoTopology_IsStateStarted())
            {
                GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_disconnect_handset, task, id, message, 0);
            }
            break;

        case STEREOTOP_GOAL_DISCONNECT_LRU_HANDSET:
            if (StereoTopology_IsStateStarted())
            {
                GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_disconnect_lru_handset, task, id, message, 0);
            }
            break;

        case STEREOTOP_GOAL_ALLOW_LE_CONNECTION:
            if (StereoTopology_IsStateStarting())
            {
                GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_allow_le_connection, task, id, message, sizeof(STEREOTOP_GOAL_ALLOW_LE_CONNECTION_T));
            }
            break;

        case STEREOTOP_GOAL_SYSTEM_STOP:
            /* Can STOP topology from either STARTING/STARTED/STOPPING states*/
            GoalsEngine_ActivateGoal(td->goal_set, stereo_topology_goal_system_stop, task, id, NULL, 0);
            break; 

        default:
            DEBUG_LOG_VERBOSE("StereoTopology_HandleGoalDecision, unknown goal decision MESSAGE:stereo_topology_goals:0x%x", id);
            break;
    }

    /* Always mark the rule as complete, once the goal has been added. */
    StereoTopologyRules_SetRuleComplete(id);
}


void StereoTopology_GoalsInit(void)
{
    stereo_topology_task_data_t *td = StereoTopologyGetTaskData();
    goal_set_init_params_t init_params;

    td->pending_goal_queue_task.handler = StereoTopology_HandleGoalDecision;

    memset(&init_params, 0, sizeof(init_params));
    init_params.goals = stereogoals;
    init_params.goals_count = ARRAY_DIM(stereogoals);
    init_params.pending_goal_queue_task = &td->pending_goal_queue_task;

    init_params.proc_result_task = StereoTopologyGetTask();
    init_params.proc_start_cfm_fn = stereoTopology_GoalProcStartCfm;
    init_params.proc_cancel_cfm_fn = stereoTopology_GoalProcCancelCfm;
    init_params.proc_complete_cfm_fn = stereoTopology_GoalProcComplete;

    td->goal_set = GoalsEngine_CreateGoalSet(&init_params);
}

