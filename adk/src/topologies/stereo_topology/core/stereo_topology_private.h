/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Private header file for the Stereo topology.
*/


#ifndef STEREO_TOPOLOGY_PRIVATE_H_
#define STEREO_TOPOLOGY_PRIVATE_H_

#include <task_list.h>
#include <message.h>
#include <rules_engine.h>
#include <goals_engine.h>
#include <stdlib.h>

/*! Defines the task list initial capacity */
#define MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY 1


/*! @{ */
    /*! The start identifier for general messages used by the topology */
#define STEREOTOP_INTERNAL_MSG_BASE                         (0x0000)

#define STEREOTOP_INTERNAL_RULE_MSG_BASE                    (0x0100)

#define STEREOTOP_INTERNAL_PROCEDURE_RESULTS_MSG_BASE       (0x0200)

/*! @} */

/*! \brief Stereo Topology states.
 */
typedef enum
{
    STEREO_TOPOLOGY_STATE_STOPPED,        /*!< Topology is stopped (default) */
    STEREO_TOPOLOGY_STATE_STOPPING,       /*!< Topology stop is in progress */
    STEREO_TOPOLOGY_STATE_STARTED,        /*!< Topology is started */
    STEREO_TOPOLOGY_STATE_STARTING,       /*!< Topology start is in progress */
} stereo_topology_sm_t;

typedef enum
{
    /*! Internal message sent if the topology stop command times out */
    STEREOTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP = STEREOTOP_INTERNAL_MSG_BASE,
    STEREOTOP_INTERNAL_MSG_MAX,
} stereo_topology_internal_message_t;

/*! Structure holding information for the Stereo Topology task */
typedef struct
{
    /*! Task for handling messages */
    TaskData                task;

    /*! The STEREO topology goal set */
    goal_set_t              goal_set;

    /*! Task for all goal processing */
    TaskData                goal_task;

    /*! Task to be sent all outgoing messages */
    Task                    app_task;

    /*! Queue of goals already decided but waiting to be run. */
    TaskData                pending_goal_queue_task;

    /*! List of clients registered to receive STEREO_TOPOLOGY_ROLE_CHANGED_IND_T
     * messages */
    TASK_LIST_WITH_INITIAL_CAPACITY(MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY)   message_client_tasks;

    /*! Can be used to control whether topology attempts handset connection */
    bool                prohibit_connect_to_handset;

    /*! Used to decide connectability of stereo based on shutdown */
    bool                shutdown_in_progress;

    /*! To track stereo topology sm state */
    stereo_topology_sm_t   stereo_topology_state;
} stereo_topology_task_data_t;

/*! Make the  stereo topology SM instance visible throughout the component */
extern stereo_topology_task_data_t stereo_topology;

/*! Get pointer to the task data */
#define StereoTopologyGetTaskData()         (&stereo_topology)

/*! Get pointer to the Stereo Topology task */
#define StereoTopologyGetTask()             (&stereo_topology.task)

/*! Get pointer to the Stereo Topology goal handling task */
#define StereoTopologyGetGoalTask()         (&stereo_topology.goal_task)

/*! Get pointer to the Stereo Topology client tasks */
#define StereoTopologyGetMessageClientTasks() (task_list_flexible_t *)(&stereo_topology.message_client_tasks)

/*! Macro to create a Stereo topology message. */
#define MAKE_STEREO_TOPOLOGY_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))

/*! Private API used for test functionality
    \return TRUE if Stereo topology has been started, FALSE otherwise
 */
bool stereoTopology_IsRunning(void);

/*! This function is called to get the stereo topology state
*/
stereo_topology_sm_t StereoTopology_GetState(void);

/*! This function is called to change the stereo topology state, it automatically
   calls the entry and exit functions for the new and old states.
*/
void StereoTopology_SetState(stereo_topology_sm_t new_state);

#define StereoTopology_IsStateStopped() (StereoTopology_GetState() == STEREO_TOPOLOGY_STATE_STOPPED)
#define StereoTopology_IsStateStopping() (StereoTopology_GetState() == STEREO_TOPOLOGY_STATE_STOPPING)
#define StereoTopology_IsStateStarted() (StereoTopology_GetState() == STEREO_TOPOLOGY_STATE_STARTED)
#define StereoTopology_IsStateStarting() (StereoTopology_GetState() == STEREO_TOPOLOGY_STATE_STARTING)

#endif /* STEREO_TOPOLOGY_PRIVATE_H_ */

