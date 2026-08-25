/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       state_machine.h
\brief      Definition of APIs for the state machine module
*/

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H
#include <csrtypes.h>

/*! The number of state slots reserved for a given component,
    used to increment state base values, e.g.
    #define STATE_BASE_1 0
    #define STATE_BASE_2 STATE_BASE_1 + MAX_NUMBER_OF_STATES_PER_COMPONENT
    #define STATE_BASE_3 STATE_BASE_2 + MAX_NUMBER_OF_STATES_PER_COMPONENT
    */
#define MAX_NUMBER_OF_STATES_PER_COMPONENT 15

/*! The number of event slots reserved for a given component,
    used to increment event base values, e.g.
    #define EVENT_BASE_1 0
    #define EVENT_BASE_2 EVENT_BASE_1 + MAX_NUMBER_OF_EVENTS_PER_COMPONENT
    #define EVENT_BASE_3 EVENT_BASE_2 + MAX_NUMBER_OF_EVENTS_PER_COMPONENT
    */
#define MAX_NUMBER_OF_EVENTS_PER_COMPONENT 17

/*! State base values to ensure components have unique state values */
#define TEST_STATE_BASE         (0)
#define DFU_PROTOCOL_STATE_BASE (TEST_STATE_BASE + MAX_NUMBER_OF_STATES_PER_COMPONENT)

/*! Event base values to ensure components have unique event values */
#define TEST_EVENT_BASE         (0)
#define DFU_PROTOCOL_EVENT_BASE (TEST_EVENT_BASE + MAX_NUMBER_OF_EVENTS_PER_COMPONENT)

typedef uint32 state_machine_states_t;
typedef uint32 state_machine_events_t;

/*! The event bounds define the range of event values that the state machine will accept */
typedef struct 
{
    state_machine_events_t base_event;
    state_machine_events_t top_event;
} event_bounds_t;

typedef void (* stateTransitionAction)(const void *event_params);

typedef struct
{
    state_machine_events_t        event;
    state_machine_states_t        new_state;
    const stateTransitionAction * actions;
    unsigned                      number_of_actions;
} state_transition_t;

typedef struct
{
    state_machine_states_t     state;
    unsigned                   number_of_transitions;
    const state_transition_t * transitions;
} state_transitions_t;

typedef struct
{
    state_machine_states_t * const    current_state;
    const event_bounds_t * const      event_bounds;
    const state_transitions_t * const transition_table;
    const unsigned                    transition_table_size;
} state_machine_t;

/*! \brief Trigger an event with associated parameters in the state machine
    \param state_machine Pointer to the state machine implementation
    \param event The event triggered
    \param event_params A pointer to the parameters
 */
void StateMachine_Update(const state_machine_t * state_machine, state_machine_events_t event, const void * event_params);

#endif /* STATE_MACHINE_H */