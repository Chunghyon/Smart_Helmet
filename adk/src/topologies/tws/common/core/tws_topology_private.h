/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Private header file for the TWS topology.
*/

#ifndef TWS_TOPOLOGY_PRIVATE_H_
#define TWS_TOPOLOGY_PRIVATE_H_

#include "tws_topology_sm.h"
#include "tws_topology_config.h"
#include "tws_topology.h"
#include <goals_engine.h>
#include <task_list.h>
#include <stdlib.h>

#include <message.h>
#include <bdaddr.h>

/*! Defines the roles changed task list initalc capacity */
#define MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY 1

typedef enum
{
    /*! Used by procedure notify role change clients to indicate clients are
    prepared for role change */
    TWSTOP_INTERNAL_ALL_ROLE_CHANGE_CLIENTS_PREPARED = INTERNAL_MESSAGE_BASE,

    /*! Used by procedure notify role change clients to indicate clients have
    rejected the role change.  */
    TWSTOP_INTERNAL_ROLE_CHANGE_CLIENT_REJECTION,

    /*! Used by the procedure nofiy role change clients to handle failure
    of clients to response to role change proposal. */
    TWSTOP_INTERNAL_PROPOSAL_TIMEOUT,

    /*! Used by the procedure nofiy role change clients to handle timing
    after nofiying clients of forced role change. */
    TWSTOP_INTERNAL_FORCE_TIMEOUT,

    /*! Internal message sent if the topology stop command times out */
    TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP,

    /*! Message that triggers the SM re-evaluate its state */
    TWSTOP_INTERNAL_KICK_SM,

    /*! Message to trigger handover retry */
    TWSTOP_INTERNAL_RETRY_HANDOVER,

    /*! Internal message to indicate the handover window period has ended */
    TWSTOP_INTERNAL_HANDOVER_WINDOW_TIMEOUT,

    TWSTOP_INTERNAL_MESSAGE_END
} tws_topology_internal_message_t;

/* Validate that internal message range has not been breached. */
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(TWSTOP_INTERNAL_MESSAGE_END)

/*! Structure holding information for the TWS Topology task */
typedef struct
{
    /*! Task for handling messages */
    TaskData                task;

    /*! TWS topology SM state */
    tws_topology_sm_t       tws_sm;

    /*! List of clients registered to receive TWS_TOPOLOGY_ROLE_CHANGED_IND_T
     * messages */
    TASK_LIST_WITH_INITIAL_CAPACITY(MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY)   message_client_tasks;

    /*! Queue of goals already decided but waiting to be run. */
    TaskData                pending_goal_queue_task;

    /*! The TWS topology goal set */
    goal_set_t              goal_set;

    /*! Whether Handover is allowed or prohibited. controlled by APP */
    bool                    app_prohibit_handover;

    /*! The currently selected advertising parameter set */
    tws_topology_le_adv_params_set_type_t advertising_params;

    /*! Flag to indicate topology to remain active for peer despite earbud is going in the case. */
    bool                    remain_active_for_peer:1;

} twsTopologyTaskData;

/* Make the tws_topology instance visible throughout the component. */
extern twsTopologyTaskData tws_topology;

/*! Get pointer to the task data */
#define TwsTopologyGetTaskData()         (&tws_topology)

/*! Get pointer to the TWS Topology task */
#define TwsTopologyGetTask()             (&tws_topology.task)

/*! Get pointer to the TWS Topology role changed tasks */
#define TwsTopologyGetMessageClientTasks() (task_list_flexible_t *)(&tws_topology.message_client_tasks)

#define TwsTopology_GetSm() (&TwsTopologyGetTaskData()->tws_sm)

/*! Macro to create a TWS topology message. */
#define MAKE_TWS_TOPOLOGY_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))

/*! Private API used for test functionality

    \return TRUE if topology has been started, FALSE otherwise
 */
bool twsTopology_IsRunning(void);

/*! \brief Check whether to remain active or not.

    Tell the topology to remain active for peer even if Earbud is going in the case.

    \return TRUE when remain_active_for_peer enabled, FALSE otherwise.
 */
bool TwsTopology_IsRemainActiveForPeerEnabled(void);

#endif /* TWS_TOPOLOGY_H_ */
