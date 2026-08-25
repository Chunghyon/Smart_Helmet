/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_sm.c
\brief      Implemetation of the state machine for the dfu_protocol module
*/

#include "dfu_protocol_sm.h"
#include "dfu_protocol_sm_actions.h"
#include "state_machine.h"

#define IGNORE_EVENT_IN_STATE(event,state) {event, state, NULL, 0}

static const event_bounds_t event_bounds =
{
     .base_event = DFU_PROTOCOL_EVENT_BASE,
     .top_event = DFU_PROTOCOL_EVENT_TOP
};

static const stateTransitionAction common_abort_event_action[] = { DfuProtocol_CommonAbortEventAction };
static const stateTransitionAction generic_complete_event_action[] = { DfuProtocol_GenericCompleteEventAction };
static const stateTransitionAction transport_disconnect_event_action[] = { DfuProtocol_TransportDisconnectEventAction };
static const stateTransitionAction generic_validate_event_action[] = { DfuProtocol_ValidateEventAction };

/*! dfu_protocol_init_state transitions */
static const stateTransitionAction init_state_start_event_actions[] = { DfuProtocol_StartEventAction };
static const stateTransitionAction init_state_start_post_reboot_event_actions[] = { DfuProtocol_StartPostRebootEventAction };
static const stateTransitionAction init_state_abort_event_action[] = { DfuProtocol_ConnectTransportForAbortEvent };
static const state_transition_t dfu_protocol_init_transitions[] = 
{
	{ 
        .event = dfu_protocol_start_event, 
        .new_state = dfu_protocol_transport_connecting_state, 
        .actions = init_state_start_event_actions, 
        ARRAY_DIM(init_state_start_event_actions) 
    },
    { 
        .event = dfu_protocol_start_post_reboot_event, 
        .new_state = dfu_protocol_transport_connecting_state, 
        .actions = init_state_start_post_reboot_event_actions, 
        ARRAY_DIM(init_state_start_post_reboot_event_actions) 
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = init_state_abort_event_action, 
        ARRAY_DIM(init_state_abort_event_action) 
    },
    IGNORE_EVENT_IN_STATE(dfu_protocol_transport_disconnect_event, dfu_protocol_init_state)
};

/*! dfu_protocol_transport_connecting_state transitions */
static const stateTransitionAction transport_connecting_state_transport_connected_event_actions[] = { DfuProtocol_TransportConnectedEventAction };
static const stateTransitionAction transport_connecting_state_transport_connected_post_reboot_event_actions[] = { DfuProtocol_TransportConnectedPostRebootEventAction };
static const state_transition_t dfu_protocol_transport_connecting_transitions[] = 
{
	{ 
        .event = dfu_protocol_transport_connected_event, 
        .new_state = dfu_protocol_transport_connected_state, 
        .actions = transport_connecting_state_transport_connected_event_actions, 
        ARRAY_DIM(transport_connecting_state_transport_connected_event_actions) 
    },
    { 
        .event = dfu_protocol_transport_connected_post_reboot_event, 
        .new_state = dfu_protocol_transport_connected_state, 
        .actions = transport_connecting_state_transport_connected_post_reboot_event_actions, 
        ARRAY_DIM(transport_connecting_state_transport_connected_post_reboot_event_actions) 
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_transport_connected_state transitions */
static const stateTransitionAction transport_connected_state_sync_complete_event_actions[] = { DfuProtocol_SyncCompleteEventAction };
static const stateTransitionAction transport_connected_state_sync_complete_post_reboot_event_actions[] = { DfuProtocol_SyncCompletePostRebootEventAction };
static const state_transition_t dfu_protocol_transport_connected_transitions[] = 
{
	{ 
        .event = dfu_protocol_host_sync_complete_event, 
        .new_state = dfu_protocol_host_sync_complete_state, 
        .actions = transport_connected_state_sync_complete_event_actions, 
        ARRAY_DIM(transport_connected_state_sync_complete_event_actions) 
    },
    { 
        .event = dfu_protocol_host_sync_complete_post_reboot_event, 
        .new_state = dfu_protocol_host_sync_complete_state, 
        .actions = transport_connected_state_sync_complete_post_reboot_event_actions, 
        ARRAY_DIM(transport_connected_state_sync_complete_post_reboot_event_actions) 
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_host_sync_complete_state transitions */
static const stateTransitionAction host_sync_complete_state_begin_transfer_event_actions[] = { DfuProtocol_BeginTransferEventAction };
static const stateTransitionAction host_sync_complete_state_commit_request_event_actions[] = { DfuProtocol_CommitRequestEventAction };
static const stateTransitionAction host_sync_complete_state_transfer_complete_event_actions[] = { DfuProtocol_ApplyEventAction };
static const state_transition_t dfu_protocol_host_sync_complete_transitions[] = 
{
	{ 
        .event = dfu_protocol_begin_transfer_event, 
        .new_state = dfu_protocol_data_transfer_state, 
        .actions = host_sync_complete_state_begin_transfer_event_actions, 
        ARRAY_DIM(host_sync_complete_state_begin_transfer_event_actions) 
    },
    { 
        .event = dfu_protocol_commit_request_event,
        .new_state = dfu_protocol_waiting_for_commit_state,
        .actions = host_sync_complete_state_commit_request_event_actions,
        ARRAY_DIM(host_sync_complete_state_commit_request_event_actions)
    },
    {
        .event = dfu_protocol_validate_event,
        .new_state = dfu_protocol_validating_state,
        .actions = generic_validate_event_action,
        ARRAY_DIM(generic_validate_event_action)
    },
    {
        .event = dfu_protocol_transfer_complete_event,
        .new_state = dfu_protocol_applying_state,
        .actions = host_sync_complete_state_transfer_complete_event_actions,
        ARRAY_DIM(host_sync_complete_state_transfer_complete_event_actions)
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_data_transfer_state transitions */
static const state_transition_t dfu_protocol_data_transfer_transitions[] = 
{
    {
        .event = dfu_protocol_validate_event, 
        .new_state = dfu_protocol_validating_state, 
        .actions = generic_validate_event_action,
        ARRAY_DIM(generic_validate_event_action)
    },
    {
        .event = dfu_protocol_apply_event, 
        .new_state = dfu_protocol_data_transfer_apply_pending_state, 
        .actions = NULL
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_data_transfer_apply_pending_state transitions */
static const state_transition_t dfu_protocol_data_transfer_apply_pending_transitions[] = 
{
	{ 
        .event = dfu_protocol_validate_event, 
        .new_state = dfu_protocol_validating_apply_pending_state, 
        .actions = generic_validate_event_action,
        ARRAY_DIM(generic_validate_event_action)
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_validating_state transitions */
static const state_transition_t dfu_protocol_validating_transitions[] = 
{
	{ 
        .event = dfu_protocol_validate_event, 
        .new_state = dfu_protocol_validating_state, 
        .actions = generic_validate_event_action,
        ARRAY_DIM(generic_validate_event_action)
    },
    {
        .event = dfu_protocol_apply_event,
        .new_state = dfu_protocol_validating_apply_pending_state,
        .actions = NULL
    },
    { 
        .event = dfu_protocol_transfer_complete_event, 
        .new_state = dfu_protocol_waiting_for_apply_state, 
        .actions = NULL
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_validating_apply_pending_state transitions */
static const stateTransitionAction validating_apply_pending_state_transfer_complete_event_actions[] = { DfuProtocol_ApplyEventAction };
static const state_transition_t dfu_protocol_validating_apply_pending_transitions[] = 
{
	{ 
        .event = dfu_protocol_validate_event, 
        .new_state = dfu_protocol_validating_apply_pending_state,
        .actions = generic_validate_event_action,
        ARRAY_DIM(generic_validate_event_action)
    },
    { 
        .event = dfu_protocol_transfer_complete_event, 
        .new_state = dfu_protocol_applying_state, 
        .actions = validating_apply_pending_state_transfer_complete_event_actions, 
        ARRAY_DIM(validating_apply_pending_state_transfer_complete_event_actions) 
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};


/*! dfu_protocol_waiting_for_apply_state transitions */
static const stateTransitionAction waiting_for_apply_state_apply_event_actions[] = { DfuProtocol_ApplyEventAction };
static const state_transition_t dfu_protocol_waiting_for_apply_transitions[] = 
{
	{ 
        .event = dfu_protocol_apply_event, 
        .new_state = dfu_protocol_applying_state, 
        .actions = waiting_for_apply_state_apply_event_actions, 
        ARRAY_DIM(waiting_for_apply_state_apply_event_actions) 
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_applying_state transitions */
static const state_transition_t dfu_protocol_applying_transitions[] = 
{
    { 
        .event = dfu_protocol_silent_commit_event, 
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = generic_complete_event_action, 
        ARRAY_DIM(generic_complete_event_action) 
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    },
    IGNORE_EVENT_IN_STATE(dfu_protocol_apply_event, dfu_protocol_applying_state)
};

/*! dfu_protocol_commiting_state transitions */
static const stateTransitionAction commiting_state_commit_event_actions[] = { DfuProtocol_CommitEventAction };
static const state_transition_t dfu_protocol_waiting_for_commit_transitions[] =
{
	{ 
        .event = dfu_protocol_commit_event,
        .new_state = dfu_protocol_transport_disconnecting_state, 
        .actions = commiting_state_commit_event_actions,
        ARRAY_DIM(commiting_state_commit_event_actions)
    },
    { 
        .event = dfu_protocol_abort_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = common_abort_event_action, 
        ARRAY_DIM(common_abort_event_action) 
    },
    {
        .event = dfu_protocol_transport_disconnect_event,
        .new_state = dfu_protocol_transport_disconnecting_state,
        .actions = transport_disconnect_event_action,
        ARRAY_DIM(transport_disconnect_event_action)
    }
};

/*! dfu_protocol_transport_disconnecting_state transitions */
static const stateTransitionAction transport_disconnecting_state_transport_disconnected_event_actions[] = { DfuProtocol_TransportDisconnectedEventAction };
static const state_transition_t dfu_protocol_transport_disconnecting_transitions[] = 
{
    { 
        .event = dfu_protocol_transport_disconnected_event, 
        .new_state = dfu_protocol_init_state, 
        .actions = transport_disconnecting_state_transport_disconnected_event_actions, 
        ARRAY_DIM(transport_disconnecting_state_transport_disconnected_event_actions)
    },
    /* We are disconnecting so ignore all of these events */
    IGNORE_EVENT_IN_STATE(dfu_protocol_transport_connected_event,             dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_transport_connected_post_reboot_event, dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_host_sync_complete_event,              dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_host_sync_complete_post_reboot_event,  dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_begin_transfer_event,                  dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_validate_event,                        dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_transfer_complete_event,               dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_apply_event,                           dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_commit_request_event,                  dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_commit_event,                          dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_silent_commit_event,                   dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_abort_event,                           dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_complete_event,                        dfu_protocol_transport_disconnecting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_transport_disconnect_event,            dfu_protocol_transport_disconnecting_state)
};

/*! dfu_protocol_aborting_state transitions */
static const stateTransitionAction aborting_state_transport_connected_event_actions[] = { DfuProtocol_TransportConnectedAbortEventAction };
static const state_transition_t dfu_protocol_aborting_transitions[] = 
{
    { 
        .event = dfu_protocol_transport_connected_event, 
        .new_state = dfu_protocol_aborting_state, 
        .actions = aborting_state_transport_connected_event_actions, 
        ARRAY_DIM(aborting_state_transport_connected_event_actions) 
    },
    {
        .event = dfu_protocol_complete_event, 
        .new_state = dfu_protocol_transport_disconnecting_state, 
        .actions = generic_complete_event_action, 
        ARRAY_DIM(generic_complete_event_action) 
    },
    /* We are aborting so ignore all of these events */
    IGNORE_EVENT_IN_STATE(dfu_protocol_transport_connected_post_reboot_event, dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_host_sync_complete_event,              dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_host_sync_complete_post_reboot_event,  dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_begin_transfer_event,                  dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_validate_event,                        dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_transfer_complete_event,               dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_apply_event,                           dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_commit_request_event,                  dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_commit_event,                          dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_silent_commit_event,                   dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_abort_event,                           dfu_protocol_aborting_state),
    IGNORE_EVENT_IN_STATE(dfu_protocol_transport_disconnect_event,            dfu_protocol_aborting_state)
};

/*! State transitions */
static const state_transitions_t state_transition_table[] =
{
    {
        .state = dfu_protocol_init_state, 
        .transitions = dfu_protocol_init_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_init_transitions) 
    },
    { 
        .state = dfu_protocol_transport_connecting_state, 
        .transitions = dfu_protocol_transport_connecting_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_transport_connecting_transitions) 
    },
    { 
        .state = dfu_protocol_transport_connected_state, 
        .transitions = dfu_protocol_transport_connected_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_transport_connected_transitions) 
    },
    { 
        .state = dfu_protocol_host_sync_complete_state, 
        .transitions = dfu_protocol_host_sync_complete_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_host_sync_complete_transitions) 
    },
    { 
        .state = dfu_protocol_data_transfer_state, 
        .transitions = dfu_protocol_data_transfer_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_data_transfer_transitions) 
    },
    { 
        .state = dfu_protocol_data_transfer_apply_pending_state, 
        .transitions = dfu_protocol_data_transfer_apply_pending_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_data_transfer_apply_pending_transitions) 
    },
    { 
        .state = dfu_protocol_validating_state, 
        .transitions = dfu_protocol_validating_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_validating_transitions) 
    },
    { 
        .state = dfu_protocol_validating_apply_pending_state, 
        .transitions = dfu_protocol_validating_apply_pending_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_validating_apply_pending_transitions) 
    },
    { 
        .state = dfu_protocol_waiting_for_apply_state, 
        .transitions = dfu_protocol_waiting_for_apply_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_waiting_for_apply_transitions) 
    },
    { 
        .state = dfu_protocol_applying_state, 
        .transitions = dfu_protocol_applying_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_applying_transitions) 
    },
    { 
        .state = dfu_protocol_waiting_for_commit_state,
        .transitions = dfu_protocol_waiting_for_commit_transitions,
        .number_of_transitions = ARRAY_DIM(dfu_protocol_waiting_for_commit_transitions)
    },
    { 
        .state = dfu_protocol_transport_disconnecting_state, 
        .transitions = dfu_protocol_transport_disconnecting_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_transport_disconnecting_transitions) 
    },
    { 
        .state = dfu_protocol_aborting_state, 
        .transitions = dfu_protocol_aborting_transitions, 
        .number_of_transitions = ARRAY_DIM(dfu_protocol_aborting_transitions) 
    }
};

static state_machine_states_t current_state = dfu_protocol_init_state;

/*! The state machine */
static const state_machine_t dfu_protocol_state_machine =
{
     .current_state = &current_state,
     .event_bounds = &event_bounds,
     .transition_table = state_transition_table,
     .transition_table_size = ARRAY_DIM(state_transition_table)
};

const state_machine_t * DfuProtocol_GetStateMachine(void)
{
    return &dfu_protocol_state_machine;
}

bool DfuProtocol_IsSmWaitingForApply(void)
{
    return current_state == dfu_protocol_waiting_for_apply_state;
}
