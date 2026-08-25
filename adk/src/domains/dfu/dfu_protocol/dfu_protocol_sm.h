/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_sm.h
\brief      Definition of the state machine for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_SM_H
#define DFU_PROTOCOL_SM_H

#include "state_machine.h"

typedef enum
{
    dfu_protocol_init_state = DFU_PROTOCOL_STATE_BASE,
    dfu_protocol_transport_connecting_state,
    dfu_protocol_transport_connected_state,
    dfu_protocol_host_sync_complete_state,
    dfu_protocol_data_transfer_state,
    dfu_protocol_validating_state,
    dfu_protocol_waiting_for_apply_state,
    dfu_protocol_data_transfer_apply_pending_state,
    dfu_protocol_validating_apply_pending_state,
    dfu_protocol_applying_state,
    dfu_protocol_waiting_for_commit_state,
    dfu_protocol_transport_disconnecting_state,
    dfu_protocol_aborting_state
} dfu_protocol_states_t;

typedef enum
{
    dfu_protocol_start_event = DFU_PROTOCOL_EVENT_BASE,
    dfu_protocol_start_post_reboot_event,
    dfu_protocol_transport_connected_event,
    dfu_protocol_transport_connected_post_reboot_event,
    dfu_protocol_host_sync_complete_event,
    dfu_protocol_host_sync_complete_post_reboot_event,
    dfu_protocol_begin_transfer_event,
    dfu_protocol_validate_event,
    dfu_protocol_transfer_complete_event,
    dfu_protocol_apply_event,
    dfu_protocol_commit_request_event,
    dfu_protocol_commit_event,
    dfu_protocol_silent_commit_event,
    dfu_protocol_complete_event,
    dfu_protocol_transport_disconnect_event,
    dfu_protocol_transport_disconnected_event,
    dfu_protocol_abort_event,
    DFU_PROTOCOL_EVENT_TOP
} dfu_protocol_events_t;

/*! \brief Get the DFU protocol state machine.
    \return A pointer to the DFU protocol state machine. */
const state_machine_t * DfuProtocol_GetStateMachine(void);

/*! \brief Check if the DFU state machine is waiting for an apply.
    \return TRUE if the DFU is waiting for an apply, otherwise FALSE. */
bool DfuProtocol_IsSmWaitingForApply(void);

#endif // DFU_PROTOCOL_SM_H
