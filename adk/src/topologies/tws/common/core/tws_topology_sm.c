/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Implementation of state machine transitions for the TWS topology.
*/

#include <logging.h>
#include <panic.h>
#include <phy_state.h>
#include <watchdog.h>

#include "tws_topology.h"
#include "tws_topology_private.h"
#include "tws_topology_client_msgs.h"
#include "tws_topology_advertising.h"
#include "tws_topology_config.h"
#include "tws_topology_sm.h"
#include "tws_topology_goals.h"

/*! \brief Function pointer type handling state entry.
    \param sm The sm.
    \param arg Optional value that may be passed to the entry function.
*/
typedef void (*twsTopology_SmEnterState)(tws_topology_sm_t *sm, unsigned arg);

/*! \brief Function pointer type handling state exit.
    \param sm The sm.
    \param arg Optional value that may be passed to the exit function.
*/
typedef void (*twsTopology_SmExitState)(tws_topology_sm_t *sm, unsigned arg);

/*! \brief Function pointer type handling goal completion.
    \param sm The sm.
    \param result The result of the goal.
    \return The SM's next state.
*/
typedef tws_topology_state_t (*twsTopology_SmOnGoalComplete)(tws_topology_sm_t *sm, procedure_result_t result);

/*! Structure to store each SM state's entry and exit function config */
typedef struct
{
    /*! Function to call on exiting state. */
    twsTopology_SmExitState exit;
    /*! Argument to pass to exit state function. */
    unsigned exit_arg;
    /*! Function to call on entering state. */
    twsTopology_SmEnterState enter;
    /*! Argument to pass to enter state function. */
    unsigned enter_arg;
} tws_topology_sm_state_config_t;

static tws_topology_state_t twsTopology_SmGetNextState(tws_topology_state_t current, tws_topology_state_t target);
static void twsTopology_SetState(tws_topology_sm_t *sm, tws_topology_state_t new_state);
static void twsTopology_SetElectedRole(tws_topology_sm_t *sm, tws_topology_elected_role_t new_elected_role);
static void twsTopology_RelinquishElectedRole(tws_topology_sm_t *sm);
static void twsTopology_SmSendStartCfm(tws_topology_sm_t *sm, tws_topology_status_t sts);
static void twsTopology_SmSendStopCfm(tws_topology_sm_t *sm, tws_topology_status_t sts);
static void twsTopology_WatchdogKick(unsigned timeout_seconds);
static void twsTopology_WatchdogStop(unsigned timeout_seconds);
static void twsTopology_StartHdma(tws_topology_sm_t *sm);
static void twsTopology_StopHdma(tws_topology_sm_t *sm);

static void twsTopology_SmSetGoal(tws_topology_sm_t *sm, unsigned id);
static void twsTopology_EnterStopped(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterStarted(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterHandoverPrepared(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterPrimary(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterSecondary(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterActingPrimary(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterIdle(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_ExitIdle(tws_topology_sm_t *sm, unsigned dummy);
static void twsTopology_EnterHandoverRetry(tws_topology_sm_t *sm, unsigned delay);
static void twsTopology_ExitHandoverRetry(tws_topology_sm_t *sm, unsigned delay);

static tws_topology_state_t twsTopology_SmHandleGoalPairPeerComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalNoRoleIdleComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalSecondaryConnectPeerComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalPrimaryConnectablePeerComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalPrimaryConnectPeerProfilesComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalDynamicHandoverPrepareComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalDynamicHandoverComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalDynamicHandoverUndoPrepareComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalBecomeActingPrimaryComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalBecomePrimaryComplete(tws_topology_sm_t *sm, procedure_result_t result);
static tws_topology_state_t twsTopology_SmHandleGoalBecomeSecondaryComplete(tws_topology_sm_t *sm, procedure_result_t result);

/*! This array defines the enter/exit functions for each SM state */
static const tws_topology_sm_state_config_t state_config[TWS_TOPOLOGY_STATES_END] =
    {
        [TWS_TOPOLOGY_STATE_PEER_PAIRING] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_pair_peer},
        [TWS_TOPOLOGY_STATE_BECOME_PRIMARY] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_become_primary},
        [TWS_TOPOLOGY_STATE_BECOME_PRIMARY_FROM_SECONDARY] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_become_primary},
        [TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_become_acting_primary},
        [TWS_TOPOLOGY_STATE_PRIMARY_CONNECT_PEER_PROFILES] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_primary_connect_peer_profiles},
        [TWS_TOPOLOGY_STATE_HANDOVER] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_dynamic_handover},
        [TWS_TOPOLOGY_STATE_HANDOVER_PREPARE] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_dynamic_handover_prepare},
        [TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_dynamic_handover_undo_prepare},
        [TWS_TOPOLOGY_STATE_BECOME_SECONDARY] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_become_secondary},
        [TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_secondary_connect_peer},
        [TWS_TOPOLOGY_STATE_BECOME_IDLE] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_no_role_idle},
        [TWS_TOPOLOGY_STATE_CANDIDATE] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_find_role},
        [TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE] = {NULL, 0, twsTopology_SmSetGoal, tws_topology_goal_primary_connectable_peer},
        [TWS_TOPOLOGY_STATE_HANDOVER_RETRY] = {twsTopology_ExitHandoverRetry, 0, twsTopology_EnterHandoverRetry, 0},
        [TWS_TOPOLOGY_STATE_STOPPED] = {NULL, 0, twsTopology_EnterStopped, 0},
        [TWS_TOPOLOGY_STATE_STARTED] = {NULL, 0, twsTopology_EnterStarted, 0},
        [TWS_TOPOLOGY_STATE_HANDOVER_PREPARED] = {NULL, 0, twsTopology_EnterHandoverPrepared, 0},
        [TWS_TOPOLOGY_STATE_PRIMARY] = {NULL, 0, twsTopology_EnterPrimary, 0},
        [TWS_TOPOLOGY_STATE_ACTING_PRIMARY] = {NULL, 0, twsTopology_EnterActingPrimary, 0},
        [TWS_TOPOLOGY_STATE_SECONDARY] = {NULL, 0, twsTopology_EnterSecondary, 0},
        [TWS_TOPOLOGY_STATE_IDLE] = {twsTopology_ExitIdle, 0, twsTopology_EnterIdle, 0},
};

/*! This array defines the goal complete handlers for each goal */
static const twsTopology_SmOnGoalComplete on_goal_complete_handlers[tws_topology_goals_end] =
    {
        [tws_topology_goal_pair_peer] = twsTopology_SmHandleGoalPairPeerComplete,
        [tws_topology_goal_no_role_idle] = twsTopology_SmHandleGoalNoRoleIdleComplete,
        [tws_topology_goal_secondary_connect_peer] = twsTopology_SmHandleGoalSecondaryConnectPeerComplete,
        [tws_topology_goal_primary_connectable_peer] = twsTopology_SmHandleGoalPrimaryConnectablePeerComplete,
        [tws_topology_goal_primary_connect_peer_profiles] = twsTopology_SmHandleGoalPrimaryConnectPeerProfilesComplete,
        [tws_topology_goal_dynamic_handover_prepare] = twsTopology_SmHandleGoalDynamicHandoverPrepareComplete,
        [tws_topology_goal_dynamic_handover] = twsTopology_SmHandleGoalDynamicHandoverComplete,
        [tws_topology_goal_dynamic_handover_undo_prepare] = twsTopology_SmHandleGoalDynamicHandoverUndoPrepareComplete,
        [tws_topology_goal_become_acting_primary] = twsTopology_SmHandleGoalBecomeActingPrimaryComplete,
        [tws_topology_goal_become_primary] = twsTopology_SmHandleGoalBecomePrimaryComplete,
        [tws_topology_goal_become_secondary] = twsTopology_SmHandleGoalBecomeSecondaryComplete,
};

CREATE_WATCHDOG(topology_watchdog);

void twsTopology_SmInit(tws_topology_sm_t *sm)
{
    sm->state = TWS_TOPOLOGY_STATE_STOPPED;
    sm->goal_task.handler = TwsTopology_HandleGoalDecision;
    TwsTopology_GoalsInit();
}

void twsTopology_SmStart(tws_topology_sm_t *sm, Task start_task)
{
    sm->start_task = start_task;
    sm->stop_task = NULL;

    twsTopology_SmSendStartCfm(sm, tws_topology_status_success);

    sm->state = TWS_TOPOLOGY_STATE_STARTING;
    twsTopology_SmEnqueueKick(sm);
}

void twsTopology_SmStop(tws_topology_sm_t *sm, Task stop_task)
{
    uint32 timeout_ms = D_SEC(TwsTopologyConfig_TwsTopologyStopTimeoutS());

    sm->stop_task = stop_task;
    sm->start_task = NULL;

    twsTopology_SmSendStopCfm(sm, tws_topology_status_success);

    if (timeout_ms)
    {
        MessageSendLater(TwsTopologyGetTask(),
                         TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP, NULL, timeout_ms);
    }
    twsTopology_SmEnqueueKick(sm);
}

void twsTopology_SmKick(tws_topology_sm_t *sm)
{
    if (twsTopology_StateIsSteady(sm->state))
    {
        MessageCancelFirst(TwsTopologyGetTask(), TWSTOP_INTERNAL_KICK_SM);
        twsTopology_SetTargetState(sm);
        if (sm->target_state == TWS_TOPOLOGY_STATE_IDLE ||
            sm->target_state == TWS_TOPOLOGY_STATE_STOPPED)
        {
            /*! Idle target requires a new role election */
            twsTopology_RelinquishElectedRole(sm);
        }

        if (sm->target_state != sm->state)
        {
            tws_topology_state_t next_state = twsTopology_SmGetNextState(sm->state, sm->target_state);
            twsTopology_SetState(sm, next_state);
        }
    }
}

void twsTopology_SmEnqueueKick(tws_topology_sm_t *sm)
{
    UNUSED(sm);
    MessageCancelFirst(TwsTopologyGetTask(), TWSTOP_INTERNAL_KICK_SM);
    MessageSend(TwsTopologyGetTask(), TWSTOP_INTERNAL_KICK_SM, NULL);
}

void twsTopology_SmHandlePeerLinkDisconnected(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandlePeerLinkDisconnected");
    switch (sm->elected_role)
    {
        case tws_topology_elected_role_primary:
            if (appPhyStateIsOutOfCase())
            {
                twsTopology_SetElectedRole(sm, tws_topology_elected_role_acting_primary);
                twsTopology_SmKick(sm);
            }
        break;
        case tws_topology_elected_role_secondary:
            twsTopology_RelinquishElectedRole(sm);
            twsTopology_SmKick(sm);
        break;
        default:
        break;
    }
}

void twsTopology_SmHandleElectedActingPrimary(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleElectedActingPrimary");
    twsTopology_SetElectedRole(sm, tws_topology_elected_role_acting_primary);
    twsTopology_SmKick(sm);
}

void twsTopology_SmHandleElectedPrimary(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleElectedPrimary");
    twsTopology_SetElectedRole(sm, tws_topology_elected_role_primary);
    twsTopology_SmKick(sm);
}

void twsTopology_SmHandleElectedSecondary(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleElectedSecondary");
    twsTopology_SetElectedRole(sm, tws_topology_elected_role_secondary);
    twsTopology_SmKick(sm);
}

void twsTopology_SmHandleHandoverToPrimary(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleHandoverToPrimary");
    twsTopology_SetElectedRole(sm, tws_topology_elected_role_primary);
    twsTopology_SetTargetState(sm);
    twsTopology_SetState(sm, TWS_TOPOLOGY_STATE_BECOME_PRIMARY_FROM_SECONDARY);
}

void twsTopology_SmHandleInternalRetryHandover(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleInternalRetryHandover");
    switch (sm->state)
    {
        case TWS_TOPOLOGY_STATE_HANDOVER_RETRY:
            twsTopology_SetState(sm, TWS_TOPOLOGY_STATE_HANDOVER);
        break;
        default:
            Panic();
        break;
    }
}

void twsTopology_SmHandleInternalStopTimeout(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleInternalStopTimeout");
    twsTopology_SmSendStopCfm(sm, tws_topology_status_fail);
    sm->stop_task = NULL;
}

void twsTopology_SmHandleHDMARequest(tws_topology_sm_t *sm, hdma_handover_decision_t* message)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleHDMARequest");
    sm->handover_reason = message->reason;
    twsTopology_SmKick(sm);
}

void twsTopology_SmHandleHDMACancelHandover(tws_topology_sm_t *sm)
{
    DEBUG_LOG_FN_ENTRY("twsTopology_SmHandleHDMACancelHandover");
    sm->handover_reason = HDMA_HANDOVER_REASON_INVALID;
    twsTopology_SmKick(sm);
}

void twsTopology_SmGoalComplete(tws_topology_sm_t *sm, tws_topology_goal_id completed_goal, procedure_result_t result)
{
    twsTopology_SmOnGoalComplete handler = on_goal_complete_handlers[completed_goal];
    if (handler)
    {
        tws_topology_state_t next_state = handler(sm, result);
        twsTopology_SetState(sm, next_state);
    }
}

/*! \brief Relinquish the currently elected role.

    The SM sometimes makes a decision or handles a failure that requires a new
    role to be elected. This function relinquishes the last elected role by
    setting the elected_role back to none. This will force the SM to transition
    back through the candidate state to obtain a new role because the target
    state is always updated before the SM transitions away from a steady state,
    and the elected_role influences the target state. */
static void twsTopology_RelinquishElectedRole(tws_topology_sm_t *sm)
{
    twsTopology_SetElectedRole(sm, tws_topology_elected_role_none);
}

/*! \brief Set the elected role. */
static void twsTopology_SetElectedRole(tws_topology_sm_t *sm, tws_topology_elected_role_t new_elected_role)
{
    DEBUG_LOG_STATE("twsTopology_SetElectedRole "
                    "enum:tws_topology_elected_role_t:%d ->"
                    "enum:tws_topology_elected_role_t:%d",
                    sm->elected_role, new_elected_role);
    sm->elected_role = new_elected_role;
}

/*! \brief Kick the watchdog.
    \param timeout_seconds The watchdog timeout
*/
static void twsTopology_WatchdogKick(unsigned timeout_seconds)
{
    if (timeout_seconds)
    {
        Watchdog_Kick(&topology_watchdog, timeout_seconds);
    }
}

/*! \brief Stop the watchdog.
    \param timeout_seconds The watchdog timeout
*/
static void twsTopology_WatchdogStop(unsigned timeout_seconds)
{
    if (timeout_seconds)
    {
        Watchdog_Stop(&topology_watchdog);
    }
}

/*! \brief Start the handover decision making algorithm */
static void twsTopology_StartHdma(tws_topology_sm_t *sm)
{
    sm->handover_reason = HDMA_HANDOVER_REASON_INVALID;
    Hdma_Init(TwsTopologyGetTask());
}

/*! \brief Stop the handover decision making algorithm */
static void twsTopology_StopHdma(tws_topology_sm_t *sm)
{
    sm->handover_reason = HDMA_HANDOVER_REASON_INVALID;
    Hdma_Destroy();
    MessageCancelAll(TwsTopologyGetTask(), HDMA_HANDOVER_NOTIFICATION);
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_STOPPED state */
static void twsTopology_EnterStopped(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    /* Clearing task clears message conditional, delivering message */
    sm->stop_task = NULL;
    MessageCancelFirst(TwsTopologyGetTask(), TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP);
}

/*! \brief Exit the TWS_TOPOLOGY_STATE_STOPPED state */
static void twsTopology_EnterStarted(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    /* Clearing task clears message conditional, delivering message */
    sm->start_task = NULL;
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_HANDOVER_PREPARED state */
static void twsTopology_EnterHandoverPrepared(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);

    sm->handover_failed = FALSE;

    /* start the handover window */
    MessageSendLater(TwsTopologyGetTask(),
                     TWSTOP_INTERNAL_HANDOVER_WINDOW_TIMEOUT,
                     NULL,
                     TwsTopologyConfig_HandoverWindowPeriodMs());
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_PRIMARY state */
static void twsTopology_EnterPrimary(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    UNUSED(sm);
    twsTopology_UpdateAdvertisingParams();
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_SECONDARY state */
static void twsTopology_EnterSecondary(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    UNUSED(sm);
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_ACTING_PRIMARY state */
static void twsTopology_EnterActingPrimary(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    UNUSED(sm);
    twsTopology_UpdateAdvertisingParams();
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_IDLE state */
static void twsTopology_EnterIdle(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);

    sm->handover_failed = FALSE;

    twsTopology_WatchdogKick(TwsTopologyConfig_IdleResetDelaySeconds());
}

/*! \brief Exit the TWS_TOPOLOGY_STATE_IDLE state */
static void twsTopology_ExitIdle(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    UNUSED(sm);
    twsTopology_WatchdogStop(TwsTopologyConfig_IdleResetDelaySeconds());
}

/*! \brief Enter the TWS_TOPOLOGY_STATE_HANDOVER_RETRY state */
static void twsTopology_EnterHandoverRetry(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    UNUSED(sm);
    MessageSendLater(TwsTopologyGetTask(), TWSTOP_INTERNAL_RETRY_HANDOVER, NULL, TwsTopologyConfig_HandoverRetryTimeoutMs());
}

/*! \brief Exit the TWS_TOPOLOGY_STATE_HANDOVER_RETRY state */
static void twsTopology_ExitHandoverRetry(tws_topology_sm_t *sm, unsigned dummy)
{
    UNUSED(dummy);
    UNUSED(sm);
    MessageCancelFirst(TwsTopologyGetTask(), TWSTOP_INTERNAL_RETRY_HANDOVER);
}

/*! \brief Set a new state, triggering state exit/enter functions */
static void twsTopology_SetState(tws_topology_sm_t *sm, tws_topology_state_t new_state)
{
    const tws_topology_sm_state_config_t *state;
    tws_topology_state_t old_state = sm->state;
    bool new_state_requires_hdma = twsTopology_StateRequiresHdma(new_state);
    bool old_state_requires_hdma = twsTopology_StateRequiresHdma(old_state);
    tws_topology_role new_role = twsTopology_RoleFromState(new_state);
    tws_topology_role old_role = twsTopology_RoleFromState(old_state);

    if (old_state == new_state)
    {
        return;
    }

    PanicFalse(new_state < TWS_TOPOLOGY_STATES_END);

    DEBUG_LOG_STATE("twsTopology_SetState Transition "
                    "enum:tws_topology_state_t:%d->"
                    "enum:tws_topology_state_t:%d",
                    old_state, new_state);

    state = &state_config[old_state];
    if (state->exit)
    {
        state->exit(sm, state->exit_arg);
    }

    if (old_state_requires_hdma && !new_state_requires_hdma)
    {
        twsTopology_StopHdma(sm);
    }

    sm->state = new_state;

    if (new_state_requires_hdma && !old_state_requires_hdma)
    {
        twsTopology_StartHdma(sm);
    }

    state = &state_config[new_state];
    if (state->enter)
    {
        state->enter(sm, state->enter_arg);
    }

    if (old_role != new_role)
    {
        TwsTopology_SendRoleChangedInd(new_role);
    }

    if (twsTopology_StateIsSteady(new_state))
    {
        twsTopology_SmEnqueueKick(sm);
    }
}

/*! \brief Cancel pending, then send TWS_TOPOLOGY_START_CFM conditionally on the
    start task.
*/
static void twsTopology_SmSendStartCfm(tws_topology_sm_t *sm, tws_topology_status_t sts)
{
    MAKE_TWS_TOPOLOGY_MESSAGE(TWS_TOPOLOGY_START_CFM);
    message->status = sts;
    MessageCancelFirst(sm->start_task, TWS_TOPOLOGY_START_CFM);
    MessageSendConditionallyOnTask(sm->start_task, TWS_TOPOLOGY_START_CFM, message, &sm->start_task);
}

/*! \brief Cancel pending, then send TWS_TOPOLOGY_STOP_CFM conditionally on the
    stop task.
*/
static void twsTopology_SmSendStopCfm(tws_topology_sm_t *sm, tws_topology_status_t sts)
{
    MAKE_TWS_TOPOLOGY_MESSAGE(TWS_TOPOLOGY_STOP_CFM);
    message->status = sts;
    MessageCancelFirst(TwsTopologyGetTask(), TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP);
    MessageCancelFirst(sm->stop_task, TWS_TOPOLOGY_STOP_CFM);
    MessageSendConditionallyOnTask(sm->stop_task, TWS_TOPOLOGY_STOP_CFM, message, &sm->stop_task);
}

/*! \brief Set a goal, by sending the goal id to the goal task */
static void twsTopology_SmSetGoal(tws_topology_sm_t *sm, unsigned id)
{
    MessageSend(&sm->goal_task, (uint16)id, NULL);
}

/*! \brief Default method of handling goal completion.
    \note Assumes 1) the goal can only complete in one state 2) the goal was
    successful.
*/
static tws_topology_state_t twsTopology_SmGoalCompleteDefaultHandler(tws_topology_sm_t *sm, procedure_result_t result, tws_topology_state_t expected, tws_topology_state_t next)
{
    PanicFalse(sm->state == expected);
    PanicFalse(result == procedure_result_success);
    return next;
}

/*! \brief Handle tws_topology_goal_pair_peer */
static tws_topology_state_t twsTopology_SmHandleGoalPairPeerComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    tws_topology_state_t next_state;

    PanicFalse(sm->state == TWS_TOPOLOGY_STATE_PEER_PAIRING);
    if (result == procedure_result_success)
    {
        next_state = TWS_TOPOLOGY_STATE_STARTED;
    }
    else
    {
        next_state = TWS_TOPOLOGY_STATE_STARTING;
    }
    return next_state;
}

/*! \brief Handle tws_topology_goal_no_role_idle */
static tws_topology_state_t twsTopology_SmHandleGoalNoRoleIdleComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    return twsTopology_SmGoalCompleteDefaultHandler(sm, result, TWS_TOPOLOGY_STATE_BECOME_IDLE, TWS_TOPOLOGY_STATE_IDLE);
}

/*! \brief Handle tws_topology_goal_secondary_connect_peer */
static tws_topology_state_t twsTopology_SmHandleGoalSecondaryConnectPeerComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    tws_topology_state_t next_state = sm->state;

    PanicFalse(sm->state == TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING);
    if (result == procedure_result_success)
    {
        next_state = TWS_TOPOLOGY_STATE_SECONDARY;
    }
    else
    {
        twsTopology_RelinquishElectedRole(sm);
        next_state = TWS_TOPOLOGY_STATE_BECOME_IDLE;
    }
    return next_state;
}

/*! \brief Handle tws_topology_goal_primary_connectable_peer */
static tws_topology_state_t twsTopology_SmHandleGoalPrimaryConnectablePeerComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    tws_topology_state_t next_state = sm->state;

    switch (sm->state)
    {
        case TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE:
            if (result == procedure_result_success)
            {
                next_state = TWS_TOPOLOGY_STATE_PRIMARY_CONNECT_PEER_PROFILES;
            }
            else
            {
                /*! The elected role was primary, but the peer failed to
                    connect, so the elected role must be switched to acting
                    primary */
                twsTopology_SetElectedRole(sm, tws_topology_elected_role_acting_primary);
                next_state = TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY;
            }
        break;
        case TWS_TOPOLOGY_STATE_BECOME_IDLE:
            /* tws_topology_goal_no_role_idle is a cancel goal, so it can cancel
               tws_topology_goal_primary_connectable_peer. In that case the SM
               will be in the TWS_TOPOLOGY_STATE_BECOME_IDLE state when the
               cancelled procedure completes and no further action is required. */
            DEBUG_LOG("twsTopology_SmHandleGoalPrimaryConnectablePeerComplete becoming idle, "
                      "enum:procedure_result_t:%d", result);
        break;

        default:
            Panic();
        break;
    }
    return next_state;
}

/*! \brief Handle tws_topology_goal_primary_connect_peer_profiles */
static tws_topology_state_t twsTopology_SmHandleGoalPrimaryConnectPeerProfilesComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    tws_topology_state_t next_state = sm->state;

    switch (sm->state)
    {
        case TWS_TOPOLOGY_STATE_PRIMARY_CONNECT_PEER_PROFILES:
            if (result == procedure_result_success)
            {
                next_state = TWS_TOPOLOGY_STATE_PRIMARY;
            }
            else
            {
                /*! The elected role was primary, but the peer profiles failed
                    to connect, so the elected role must be switched to acting
                    primary */
                twsTopology_SetElectedRole(sm, tws_topology_elected_role_acting_primary);
                next_state = TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY;
            }
        break;
        default:
            Panic();
        break;
    }
    return next_state;
}

/*! \brief Handle tws_topology_goal_dynamic_handover_prepare */
static tws_topology_state_t twsTopology_SmHandleGoalDynamicHandoverPrepareComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    return twsTopology_SmGoalCompleteDefaultHandler(sm, result, TWS_TOPOLOGY_STATE_HANDOVER_PREPARE, TWS_TOPOLOGY_STATE_HANDOVER_PREPARED);
}

static tws_topology_state_t twsTopology_SmHandleGoalDynamicHandoverComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    tws_topology_state_t next_state = sm->state;

    switch (sm->state)
    {
        case TWS_TOPOLOGY_STATE_HANDOVER:
            switch (result)
            {
                case procedure_result_success:
                    twsTopology_SetElectedRole(sm, tws_topology_elected_role_secondary);
                    next_state = TWS_TOPOLOGY_STATE_SECONDARY;
                break;

                case procedure_result_timeout:
                    if (MessagePendingFirst(TwsTopologyGetTask(), TWSTOP_INTERNAL_HANDOVER_WINDOW_TIMEOUT, NULL))
                    {
                        next_state = TWS_TOPOLOGY_STATE_HANDOVER_RETRY;
                        break;
                    }
                    //fall-through, max attempts

                case procedure_result_failed:
                    next_state = TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE;
                    sm->handover_reason = HDMA_HANDOVER_REASON_INVALID;
                    sm->handover_failed = TRUE;
                break;

                default:
                    Panic();
                break;
            }
        break;

        default:
            Panic();
        break;
    }
    return next_state;
}

/*! \brief Handle tws_topology_goal_dynamic_handover_undo_prepare */
static tws_topology_state_t twsTopology_SmHandleGoalDynamicHandoverUndoPrepareComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    return twsTopology_SmGoalCompleteDefaultHandler(sm, result, TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE, TWS_TOPOLOGY_STATE_PRIMARY_HDMA);
}

/*! \brief Handle tws_topology_goal_become_acting_primary */
static tws_topology_state_t twsTopology_SmHandleGoalBecomeActingPrimaryComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    return twsTopology_SmGoalCompleteDefaultHandler(sm, result, TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY, TWS_TOPOLOGY_STATE_ACTING_PRIMARY);
}

/*! \brief Handle tws_topology_goal_become_primary */
static tws_topology_state_t twsTopology_SmHandleGoalBecomePrimaryComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    tws_topology_state_t next_state = sm->state;

    PanicFalse(result == procedure_result_success);

    switch (sm->state)
    {
        case TWS_TOPOLOGY_STATE_BECOME_PRIMARY:
            next_state = TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE;
        break;
        case TWS_TOPOLOGY_STATE_BECOME_PRIMARY_FROM_SECONDARY:
            next_state = TWS_TOPOLOGY_STATE_PRIMARY;
        break;
        default:
            Panic();
        break;
    }
    return next_state;
}

/*! \brief Handle tws_topology_goal_become_secondary */
static tws_topology_state_t twsTopology_SmHandleGoalBecomeSecondaryComplete(tws_topology_sm_t *sm, procedure_result_t result)
{
    return twsTopology_SmGoalCompleteDefaultHandler(sm, result, TWS_TOPOLOGY_STATE_BECOME_SECONDARY, TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING);
}

/*! \brief Get the next state in the path from current state to target state.
    \param current The current SM state.
    \param target The target SM state.
    \return The next state.

    \note This function specifies the transitions from steady states to
    transient states. The transitions from transient states to transient states,
    or transient states to steady states are coded in the goal complete
    handler functions (twsTopology_SmHandleGoal.*)
*/
static tws_topology_state_t twsTopology_SmGetNextState(tws_topology_state_t current, tws_topology_state_t target)
{
    switch (current)
    {
        case TWS_TOPOLOGY_STATE_STARTING:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_PEER_PAIRING:
                    return TWS_TOPOLOGY_STATE_PEER_PAIRING;
                default:
                    return TWS_TOPOLOGY_STATE_STARTED;
            }

        case TWS_TOPOLOGY_STATE_STARTED:
            return TWS_TOPOLOGY_STATE_BECOME_IDLE;

        case TWS_TOPOLOGY_STATE_IDLE:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_STOPPED:
                    return TWS_TOPOLOGY_STATE_STOPPED;
                case TWS_TOPOLOGY_STATE_SECONDARY:
                    return TWS_TOPOLOGY_STATE_BECOME_SECONDARY;
                case TWS_TOPOLOGY_STATE_CANDIDATE:
                    return TWS_TOPOLOGY_STATE_CANDIDATE;
                default:
                break;
            }
        break;

        case TWS_TOPOLOGY_STATE_CANDIDATE:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_ACTING_PRIMARY:
                    return TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY;
                case TWS_TOPOLOGY_STATE_PRIMARY:
                    return TWS_TOPOLOGY_STATE_BECOME_PRIMARY;
                default:
                    return TWS_TOPOLOGY_STATE_BECOME_IDLE;
            }

        case TWS_TOPOLOGY_STATE_SECONDARY:
            return TWS_TOPOLOGY_STATE_BECOME_IDLE;

        case TWS_TOPOLOGY_STATE_ACTING_PRIMARY:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_PRIMARY:
                case TWS_TOPOLOGY_STATE_PRIMARY_HDMA:
                    return TWS_TOPOLOGY_STATE_BECOME_PRIMARY;
                default:
                    return TWS_TOPOLOGY_STATE_BECOME_IDLE;
            }

        case TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_PRIMARY:
                    /* Remain in state until secondary connects */
                    return TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE;
                default:
                    return TWS_TOPOLOGY_STATE_BECOME_IDLE;
            }

        case TWS_TOPOLOGY_STATE_PRIMARY:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_PRIMARY_HDMA:
                    return TWS_TOPOLOGY_STATE_PRIMARY_HDMA;
                case TWS_TOPOLOGY_STATE_ACTING_PRIMARY:
                    return TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY;
                default:
                    return TWS_TOPOLOGY_STATE_BECOME_IDLE;
            }

        case TWS_TOPOLOGY_STATE_PRIMARY_HDMA:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_SECONDARY:
                    return TWS_TOPOLOGY_STATE_HANDOVER_PREPARE;
                default:
                    return TWS_TOPOLOGY_STATE_PRIMARY;
            }

        case TWS_TOPOLOGY_STATE_HANDOVER_PREPARED:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_SECONDARY:
                    return TWS_TOPOLOGY_STATE_HANDOVER;
                default:
                    return TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE;
            }

        case TWS_TOPOLOGY_STATE_HANDOVER_RETRY:
            switch (target)
            {
                case TWS_TOPOLOGY_STATE_SECONDARY:
                    /* Remain in state until retry timer expires */
                    return TWS_TOPOLOGY_STATE_HANDOVER_RETRY;
                default:
                    return TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE;
            }

        default:
        break;
    }
    Panic();
    return 0;
}
