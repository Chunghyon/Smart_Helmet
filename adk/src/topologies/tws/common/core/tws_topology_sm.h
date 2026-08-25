/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Header file for TWS Topology State Machine.
*/

#ifndef TWS_TOPOLOGY_SM_H_
#define TWS_TOPOLOGY_SM_H_

#include "tws_topology_goals.h"
#include "tws_topology_sm_states.h"
#include "procedures.h"
#include <hdma.h>

typedef struct
{
    /*! Task for handling goal messages (from the SM) */
    TaskData                goal_task;

    /*! The role elected by "peer find role" */
    tws_topology_elected_role_t elected_role;

    /*! Current state */
    tws_topology_state_t state;

    /*! Desired target state */
    tws_topology_state_t target_state;

    /*! Flag to indicate handover failed(either handover window expired or handover failed for other reasons) */
    bool handover_failed;

    /*! Handover reason */
    hdma_handover_reason_t  handover_reason;

    /*! Task requesting topology SM stop */
    Task                    stop_task;

    /*! Task requesting topology SM start */
    Task                    start_task;

} tws_topology_sm_t;

/*! \brief Initialise the SM.
    \param sm The sm.
*/
void twsTopology_SmInit(tws_topology_sm_t *sm);

/*! \brief Start the topology SM.
    \param sm The sm.
    \param start_task Task requesting the start.
*/
void twsTopology_SmStart(tws_topology_sm_t *sm, Task start_task);

/*! \brief Stop the topology SM.
    \param sm The sm.
    \param stop_task Task requesting the stop.
*/
void twsTopology_SmStop(tws_topology_sm_t *sm, Task stop_task);

/*! \brief Trigger immediate state machine transition if SM is in a steady state.
    \param sm The sm.
*/
void twsTopology_SmKick(tws_topology_sm_t *sm);

/*! \brief Trigger a queued state machine transition if SM is in a steady state.
    \param sm The sm.
    \note The kick is queued by sending a message
*/
void twsTopology_SmEnqueueKick(tws_topology_sm_t *sm);

/*! \brief Handle the completion of a goal
    \param sm The sm.
    \param completed_goal The goal's ID.
    \param result The goal's result.
 */
void twsTopology_SmGoalComplete(tws_topology_sm_t *sm, tws_topology_goal_id completed_goal, procedure_result_t result);

/*! \brief Handle the peer earbud link disconnecting.
    \param sm The sm.
*/
void twsTopology_SmHandlePeerLinkDisconnected(tws_topology_sm_t *sm);

/*! \brief Handle acting primary election result.
    \param sm The sm.
*/
void twsTopology_SmHandleElectedActingPrimary(tws_topology_sm_t *sm);

/*! \brief Handle primary election result.
    \param sm The sm.
*/
void twsTopology_SmHandleElectedPrimary(tws_topology_sm_t *sm);

/*! \brief Handle secondary election result.
    \param sm The sm.
*/
void twsTopology_SmHandleElectedSecondary(tws_topology_sm_t *sm);

/*! \brief Handle the completion of handover as secondary resulting in a
transition to the primary role.
    \param sm The sm.
*/
void twsTopology_SmHandleHandoverToPrimary(tws_topology_sm_t *sm);

/*! \brief Handle the TWSTOP_INTERNAL_RETRY_HANDOVER message.
    \param sm The sm.
*/
void twsTopology_SmHandleInternalRetryHandover(tws_topology_sm_t *sm);

/*! \brief Handle the TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP message.
    \param sm The sm.
*/
void twsTopology_SmHandleInternalStopTimeout(tws_topology_sm_t *sm);

/*! \brief Handle handover request from HDMA.
    \param sm The sm.
    \param message Decision information.
*/
void twsTopology_SmHandleHDMARequest(tws_topology_sm_t *sm, hdma_handover_decision_t* message);

/*! \brief Handle handover cancel request from HDMA.
    \param sm The sm.
*/
void twsTopology_SmHandleHDMACancelHandover(tws_topology_sm_t *sm);

/*! \brief Set the SM's target state.
    \param sm The sm.
*/
void twsTopology_SetTargetState(tws_topology_sm_t *sm);

#endif /* TWS_TOPOLOGY_SM_H_ */
