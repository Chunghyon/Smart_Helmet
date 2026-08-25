/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       swift_pair.c
\brief      Source file for Swift Pair
*/

#ifdef INCLUDE_SWIFT_PAIR
#include "swift_pair.h"
#include "swift_pair_advertising.h"
#include <task_list.h>
#include <logging.h>
#include <panic.h>

#include "pairing.h"

typedef struct
{
    /*! The swift pair module task */
    TaskData task;
}swiftPairTaskData;

swiftPairTaskData swift_pair_task_data;

/*! \brief Handle the Pairing activity messages from pairing module */
static void swiftPair_PairingActivity(PAIRING_ACTIVITY_T *message)
{
    switch(message->status)
    {
        case pairingActivityInProgress:
        {
            DEBUG_LOG("swiftPair_PairingActivity: pairingActivityInProgress");
            SwiftPair_UpdateLeAdvertisingData(TRUE);
        }
        break;

        case pairingActivityNotInProgress:
        {
            DEBUG_LOG("swiftPair_PairingActivity: pairingActivityNotInProgress");
            SwiftPair_UpdateLeAdvertisingData(FALSE);
        }
        break;

        default:
            DEBUG_LOG("swiftPair_PairingActivity: Invalid message id ");
        break;
    }

}
/*! \brief Message Handler

    This function is the main message handler for the swift pair module.
*/
static void swiftPair_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case PAIRING_ACTIVITY:
            swiftPair_PairingActivity((PAIRING_ACTIVITY_T*)message);
        break;

        default:
        break;
    }
}

/*! Get pointer to Swift Pair data structure */
static inline swiftPairTaskData* swiftPair_GetTaskData(void)
{
    return (&swift_pair_task_data);
}

/*! Initialization of swift pair module */
bool SwiftPair_Init(Task init_task)
{
    swiftPairTaskData *theSwiftPair = swiftPair_GetTaskData();

    UNUSED(init_task);

    DEBUG_LOG("SwiftPair_Init");

    memset(theSwiftPair, 0, sizeof(*theSwiftPair));

    /* Set up task handler */
    theSwiftPair->task.handler = swiftPair_HandleMessage;

    SwiftPair_SetupLeAdvertisingData();

    /* Register with pairing module to know when device is Br/Edr discoverable */
    Pairing_ActivityClientRegister(&(theSwiftPair->task));

     return TRUE;
}

#ifdef HOSTED_TEST_ENVIRONMENT

const TaskData * SwiftPair_GetMessageHandler(void)
{
    return &swift_pair_task_data.task;
}

#endif

#endif /* INCLUDE_SWIFT_PAIR */
