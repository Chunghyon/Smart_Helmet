/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Header defining the states and state machine design for the TWS topology.
*/

#ifndef TWS_TOPOLOGY_SM_STATES_H_
#define TWS_TOPOLOGY_SM_STATES_H_

#include "tws_topology.h"

/*!
@startuml

title TWS Topology State Machine
hide empty description

skinparam state {
  BackgroundColor<<SteadyState>> Green
}

state "Stopped" as STOPPED : send stop cfm
state "Starting" as STARTING <<SteadyState>>
state "Started" as STARTED : send start cfm
state "Become Idle" as BI : tws_topology_goal_no_role_idle
state "Idle" as I <<SteadyState>>
state "Candidate" as C <<SteadyState>> : tws_topology_goal_find_role
state "Peer Pairing" as PP : tws_topology_goal_pair_peer

state "Become Secondary" as BS : tws_topology_goal_become_secondary
state "Secondary Connecting" as SCP : tws_topology_goal_secondary_connect_peer
state "Secondary" as S <<SteadyState>>

state "Become Primary" as BP : tws_topology_goal_become_primary
state "Become Primary Post Handover" as BPPH : tws_topology_goal_become_primary
state "Become Acting Primary" as BAP : tws_topology_goal_become_acting_primary
state "Acting Primary" as AP <<SteadyState>>
state "Primary Connectable" as PC <<SteadyState>> : tws_topology_goal_primary_connectable_peer
state "Primary Connecting Peer-Profiles" as PCPP : tws_topology_goal_primary_connect_peer_profiles
state "Primary" as P <<SteadyState>>
state "Primary HDMA" as PHDMA <<SteadyState>>
state "Prepare Handover" as PH : tws_topology_goal_dynamic_handover_prepare
state "Handover" as H : tws_topology_goal_dynamic_handover
state "Handover Retry" as HR <<SteadyState>>
state "Undo Prepare Handover" as UPH : tws_topology_goal_dynamic_handover_undo_prepare
state "Prepared For Handover" as HP <<SteadyState>>


STOPPED --> STARTING : twsTopology_Start()
BI --> I : Complete
BS --> SCP : Complete
SCP --> S : Complete
AP --> BP : ElectedRole=Primary
PC --> BAP : Timeout
PC --> PCPP : Complete
PCPP --> P : Complete
PCPP --> BAP : Failed
P --> BAP : Secondary\nLost
PH --> HP : Complete
HP --> H
H --> HR : Timeout
HR --> H : Retry
H --> UPH : Failed/\nCancelled
HP --> UPH : Cancelled
HR --> UPH : Cancelled
UPH --> PHDMA : Complete
P --> PHDMA: RuleHdmaRequired
PHDMA --> P: RuleHdmaRequired
PHDMA --> PH : RuleHandoverStart

STARTING --> PP : RulePairWithPeer
STARTING --> STARTED
PP --> STARTED : Complete
STARTED --> BI
I --> STOPPED : RuleStop
I --> C  : RuleFindRole
C --> BI : ElectedRole=Secondary/\nRuleBecomeIdle
C --> BP : ElectedRole=Primary
C --> BAP : ElectedRole=ActingPrimary
BP --> PC : Complete
BAP --> AP : Complete
I --> BS
H --> S : Complete
AP --> BI : RuleBecomeIdle
P --> BI : RuleBecomeIdle

S --> BPPH : Handover As\nSecondary
BPPH --> P : Complete
S --> BI : RuleBecomeIdle
SCP --> BI : Failed

@enduml

*/

/*! Definition of TWS Topology states */
typedef enum
{
    /*
    -- Steady states MUST be listed first. --

    A steady state is one in which a state transition may occur towards the
    target state. Non-steady states are ones which must wait for a goal to
    complete prior to transitioning towards the target state.

    TWS_TOPOLOGY_STATE_LAST_STEADY_STATE must be updated if extra steady states
    are added.

    */
    /*! Topology is no longer stopped and can transition to next state */
    TWS_TOPOLOGY_STATE_STARTING,

    /*! Topology has started  */
    TWS_TOPOLOGY_STATE_STARTED,

    /*! Idle */
    TWS_TOPOLOGY_STATE_IDLE,

    /*! In the candidate state, the earbud finds its role using the "peer find
    role" component. The elected role (acting primary, primary, secondary)
    determines the subsequent address and behaviour of the earbud. Both earbuds
    use the primary address in the candidate state. */
    TWS_TOPOLOGY_STATE_CANDIDATE,

    /*! The acting primary state is used when the earbud is unable to connect to
    its peer during role selection in the TWS_TOPOLOGY_STATE_CANDIDATE state.
    After a timeout, the earbud self-elects itself to the primary role but with
    "acting" prefix. The acting primary behaviour is generally the same as the
    primary behaviour, with the difference that the earbud is continuing in the
    background to attempt to reconnect to its peer. */
    TWS_TOPOLOGY_STATE_ACTING_PRIMARY,

    /*! When the primary role is elected in the TWS_TOPOLOGY_STATE_CANDIDATE
    state, the earbud becomes connectable (page scanning) allowing the secondary
    earbud to initiate the ACL connection. The earbud remains connectable for a
    finite period. If the secondary fails to connect during this period, the
    earbud falls-back to acting-primary role. This is a "steady" state because
    the earbud may transition back to idle state which immediately stops the
    connectable mode.
    */
    TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE,

    /*! After the secondary connects in the
    TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE state and the primary has connected
    the peer profiles in the TWS_TOPOLOGY_STATE_PRIMARY_CONNECT_PEER_PROFILES
    state, the earbud enters this state. This is a long term steady-state of
    the primary earbud. */
    TWS_TOPOLOGY_STATE_PRIMARY,

    /*! Having become primary, the earbud activates the handover decision making
    algorithm (HDMA) in this state when a handset is connected. This is a long
    term steady-state of the primary earbud. */
    TWS_TOPOLOGY_STATE_PRIMARY_HDMA,

    /*! After the secondary has connected to the primary in the
    TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING state, it enters this state. This is
    the long term steady-state of the secondary earbud.
    */
    TWS_TOPOLOGY_STATE_SECONDARY,

    /*! When the primary earbud determines it should handover connections and
    become secondary, it prepares to handover in the
    TWS_TOPOLOGY_STATE_HANDOVER_PREPARE state. Once prepared, it enters this
    state before starting the handover procedure in the
    TWS_TOPOLOGY_STATE_HANDOVER state. This short-term steady-state exists so
    that handover may be cancellled post-prepare allowing the earbud to
    transition back to the primary role via the
    TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE state. */
    TWS_TOPOLOGY_STATE_HANDOVER_PREPARED,

    /*! If handover is vetoed in the TWS_TOPOLOGY_STATE_HANDOVER state, the SM
    enters this state. Here, it waits for a short period before re-attempting
    handover. This steady state exists so that handover may be cancelled
    allowing the earbud to transition back to the primary role via the
    TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE state. */
    TWS_TOPOLOGY_STATE_HANDOVER_RETRY,

    /* TWS_TOPOLOGY_STATE_LAST_STEADY_STATE must be updated if extra steady
    states are added. */

    /*
    -- Non-steady-states MUST be placed after this comment --
    */

    /*! Topology is stopped, this is not a steady state meaning the state
    must explicity be set to a steady state to allow the SM to operate.  */
    TWS_TOPOLOGY_STATE_STOPPED,

    /*! Before the earbuds can connect to handsets, the two earbuds _must_ pair.
    During peer pairing, the two earbuds pair, select which of their addresses
    is used as the primary and secondary and synchronise their root keys. */
    TWS_TOPOLOGY_STATE_PEER_PAIRING,

    /*! After election to primary role in the TWS_TOPOLOGY_STATE_CANDIDATE
    state, this state performs the procedures required for the primary role. */
    TWS_TOPOLOGY_STATE_BECOME_PRIMARY,

    /*! When handover completes on the secondary, it needs to become primary. */
    TWS_TOPOLOGY_STATE_BECOME_PRIMARY_FROM_SECONDARY,

    /*! After election to acting-primary role in the
    TWS_TOPOLOGY_STATE_CANDIDATE state, this state performs the procedures
    required for the acting-primary role */
    TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY,

    /*! After election to secondary role in the TWS_TOPOLOGY_STATE_CANDIDATE
    state, this state performs the procedures required for the secondary role.
    This includes swapping addresses from primary to secondary.
    */
    TWS_TOPOLOGY_STATE_BECOME_SECONDARY,

    /*! After election to secondary role in the TWS_TOPOLOGY_STATE_CANDIDATE
    state and becoming secondary in the TWS_TOPOLOGY_STATE_BECOME_SECONDARY
    state, the secondary earbud initiates the connection to the primary earbud
    in this state. */
    TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING,

    /*! After the secondary has connected to the primary earbud in the
    TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING state, the primary _must_ connect
    all peer profiles. The secondary must _not_ connect any profiles. This
    requirement exists due to the way L2CAP CIDs are managed by the upper stack
    during in order to support handover. */
    TWS_TOPOLOGY_STATE_PRIMARY_CONNECT_PEER_PROFILES,

    /*! In this state the primary earbud prepares the system for handover. */
    TWS_TOPOLOGY_STATE_HANDOVER_PREPARE,

    /*! In this state the handover is performed where all handsets are
    transferred to the secondary earbud and the roles and addresses are swapped.
    */
    TWS_TOPOLOGY_STATE_HANDOVER,

    /*! If handover is cancelled (either after preparing in the
    TWS_TOPOLOGY_STATE_HANDOVER_PREPARED state, or after a vetoed handover in
    the TWS_TOPOLOGY_STATE_HANDOVER_RETRY state, the SM un-does the actions
    performed in the TWS_TOPOLOGY_STATE_HANDOVER_PREPARE state. This allows the
    earbud to resume its primary role. */
    TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE,

    /*! In numerous scenarios, the SM needs to return to the idle state.
    In this state the SM performs the actions required to transtion from active
    (in any role) to idle. */
    TWS_TOPOLOGY_STATE_BECOME_IDLE,

    /*! Always the final state, only used to determine the number of states in
    the SM */
    TWS_TOPOLOGY_STATES_END,

} tws_topology_state_t;


/*! The last steady state from the enumeration */
#define TWS_TOPOLOGY_STATE_LAST_STEADY_STATE TWS_TOPOLOGY_STATE_HANDOVER_RETRY

/*! Definition of the earbud elected roles in the TWS topology SM. */
typedef enum
{
    /*! The earbud has no role. It cannot take any other role without first
    being elected to a role in the TWS_TOPOLOGY_STATE_CANDIDATE state. */
    tws_topology_elected_role_none,
    /*! The earbud has attempted and failed to connect to the other earbud in
    the TWS_TOPOLOGY_STATE_CANDIDATE state. It has self-elected itself to become
    "acting" primary. */
    tws_topology_elected_role_acting_primary,
    /*! The earbud has been elected as the primary earbud in the
    TWS_TOPOLOGY_STATE_CANDIDATE state. */
    tws_topology_elected_role_primary,
    /*! The earbud has been elected as the secondary earbud in the
    TWS_TOPOLOGY_STATE_CANDIDATE state. */
    tws_topology_elected_role_secondary,

} tws_topology_elected_role_t;

/*! \brief Query if the state is a steady one */
static inline bool twsTopology_StateIsSteady(tws_topology_state_t state)
{
    return (state <= TWS_TOPOLOGY_STATE_LAST_STEADY_STATE);
}

/*! \brief Get the topology role from the SM's state. */
static inline tws_topology_role twsTopology_RoleFromState(tws_topology_state_t state)
{
    switch (state)
    {
        case TWS_TOPOLOGY_STATE_BECOME_ACTING_PRIMARY:
        case TWS_TOPOLOGY_STATE_ACTING_PRIMARY:
        case TWS_TOPOLOGY_STATE_BECOME_PRIMARY:
        case TWS_TOPOLOGY_STATE_PRIMARY_CONNECTABLE:
        case TWS_TOPOLOGY_STATE_PRIMARY:
        case TWS_TOPOLOGY_STATE_PRIMARY_HDMA:
        case TWS_TOPOLOGY_STATE_PRIMARY_CONNECT_PEER_PROFILES:
        case TWS_TOPOLOGY_STATE_HANDOVER_PREPARED:
        case TWS_TOPOLOGY_STATE_HANDOVER_RETRY:
        case TWS_TOPOLOGY_STATE_HANDOVER_PREPARE:
        case TWS_TOPOLOGY_STATE_HANDOVER:
        case TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE:
            return tws_topology_role_primary;
        case TWS_TOPOLOGY_STATE_SECONDARY:
        case TWS_TOPOLOGY_STATE_SECONDARY_CONNECTING:
        case TWS_TOPOLOGY_STATE_BECOME_PRIMARY_FROM_SECONDARY:
            return tws_topology_role_secondary;
        default:
            return tws_topology_role_none;
    }
}

/*! \brief Query if the state requires HDMA */
static inline bool twsTopology_StateRequiresHdma(tws_topology_state_t state)
{
    switch (state)
    {
        case TWS_TOPOLOGY_STATE_PRIMARY_HDMA:
        case TWS_TOPOLOGY_STATE_HANDOVER:
        case TWS_TOPOLOGY_STATE_HANDOVER_PREPARE:
        case TWS_TOPOLOGY_STATE_HANDOVER_PREPARED:
        case TWS_TOPOLOGY_STATE_HANDOVER_RETRY:
        case TWS_TOPOLOGY_STATE_HANDOVER_UNDO_PREPARE:
            return TRUE;
        default:
            return FALSE;
    }
}

#endif
