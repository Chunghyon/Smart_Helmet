/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       cc_with_case_private.h
\defgroup   cc_with_case
\brief      Internal interface for the case domain.
*/
/*! @{ */

#ifndef CC_WITH_CASE_PRIVATE_H
#define CC_WITH_CASE_PRIVATE_H

#include "cc_with_case.h"

#include <task_list.h>

#include <stdlib.h>

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_MODE_EARBUDS

/*! Defines the roles changed task list initalc capacity */
#define STATE_CLIENTS_TASK_LIST_INIT_CAPACITY 2

/*! Structure holding information about the current pending case command */
typedef struct
{
    /*! The task which requested this command */
    Task requester;

    /*! The command type to send to the case. */
    case_cmd_t command;

    /*! Additional data required to execute this command. */
    void *command_params;

    /*! The total size of the command parameters */
    unsigned params_length;
} cc_with_case_current_command_t;

/*! Structure holding information for the Case domain. */
typedef struct
{
    /*! Task for handling messages. */
    TaskData task;

    /*! Current known state of the case lid. */
    case_lid_state_t lid_state;

    /*! Current known state of the case battery level. */
    uint8 case_battery_state;

    /*! Current known state of the peer battery level (learnt via the case). */
    uint8 peer_battery_state;

    /*! Current known state of the case charger connectivity. */
    unsigned case_charger_connected:1;

    /*! TRUE if shipping mode command received and awaiting VCHG disconnect. */
    bool shipping_mode_pending:1;
    
    /*! List of clients registered to receive case state notification messages. */
    TASK_LIST_WITH_INITIAL_CAPACITY(STATE_CLIENTS_TASK_LIST_INIT_CAPACITY)   state_client_tasks;

    /*! The current command to send to the case. */
    cc_with_case_current_command_t *current_command;
} cc_with_case_t;

/* Make the main Case data structure visible throughput the component. */
extern cc_with_case_t cc_with_case;

/*! Get pointer to the case task data. */
#define CcWithCase_GetTaskData()   (&cc_with_case)

/*! Get pointer to the case task. */
#define CcWithCase_GetTask()       (&cc_with_case.task)

/*! Get task list with clients requiring state messages. */
#define CcWithCase_GetStateClientTasks() (task_list_flexible_t *)(&cc_with_case.state_client_tasks)

/*! Create a case message. */
#define MAKE_CCWC_MESSAGE(TYPE) TYPE##_T *message = (TYPE##_T*)PanicNull(calloc(1,sizeof(TYPE##_T)))

#endif /* HAVE_CC_MODE_EARBUDS */
#endif /* INCLUDE_CASE_COMMS */

#endif /* CC_WITH_CASE_PRIVATE_H */
/*! @} End of group documentation */
