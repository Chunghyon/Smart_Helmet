/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       state_machine.c
\brief      Implementation of APIs for the state machine module
*/

#include "state_machine.h"
#include <logging.h>
#include <panic.h>

static const state_transition_t * stateMachine_GetStateTransition(const state_transitions_t * transition_table, state_machine_events_t event)
{
    const state_transition_t * transition = NULL;
    unsigned i;
    
    for(i=0; i<transition_table->number_of_transitions; i++)
    {
        if(transition_table->transitions[i].event == event)
        {
            transition = &(transition_table->transitions[i]);
        }
    }
    
    return transition;
}

static const state_transitions_t * stateMachine_GetStateTransitionTable(const state_machine_t * state_machine)
{
    const state_transitions_t * transition_table = NULL;
    unsigned i;
    
    for(i=0; i<state_machine->transition_table_size; i++)
    {
        if(state_machine->transition_table[i].state == *(state_machine->current_state))
        {
            transition_table = &(state_machine->transition_table[i]);
        }
    }
    
    return transition_table;
}

static void stateMachine_ExecuteActions(const void * event_params, const stateTransitionAction * actions, unsigned number_of_actions)
{
    unsigned i;
    
    for(i=0; i<number_of_actions; i++)
    {
        if (actions[i])
        {
            actions[i](event_params);
        }
    }
}

void StateMachine_Update(const state_machine_t * state_machine, state_machine_events_t event, const void * event_params)
{
    PanicNull((void *)state_machine);
    PanicNull((void *)state_machine->current_state);
    PanicNull((void *)state_machine->event_bounds);
    
    if(state_machine->transition_table_size > 0)
    {
        PanicNull((void *)state_machine->transition_table);
    }

    if((state_machine->event_bounds->base_event <= event) && (state_machine->event_bounds->top_event >= event))
    {
        bool transition_successful = FALSE;
        
        const state_transitions_t * transition_table = stateMachine_GetStateTransitionTable(state_machine);
        
        if(transition_table)
        {
            const state_transition_t * transition = stateMachine_GetStateTransition(transition_table, event);
            
            if(transition)
            {
                DEBUG_LOG_STATE("StateMachine_UpdateState event=%d, old state=%d to new state=%d",
                                event, *(state_machine->current_state), transition->new_state);
                    
                /* Updating the state first since the new state may be used by the executed actions */
                *(state_machine->current_state) = transition->new_state;
                stateMachine_ExecuteActions(event_params, transition->actions, transition->number_of_actions);
                transition_successful = TRUE;
            }
        }
        
        /* A valid event was received, but no transition was performed. This is an unexpected or unconsidered event. */
        PanicFalse(transition_successful);
    }
    else
    {
        DEBUG_LOG_V_VERBOSE("StateMachine_UpdateState event not in bounds for state_machine=%p", state_machine);
        Panic();
    }
}
