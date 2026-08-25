/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case.c
\brief      Public API related Macros and Routines of dfu_case
*/

#ifdef INCLUDE_DFU_CASE

#include "dfu_case.h"
#include "dfu_case_private.h"
#include "dfu_case_host.h"
#include "dfu_case_fw.h"
#include "dfu_peer.h"
#include <upgrade.h>
#include <upgrade_peer.h>

#include <util.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include <cc_with_case.h>
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stream.h>
#include <source.h>
#include <sink.h>

#ifdef INCLUDE_DFU_CASE_MOCK
#include "dfu_case_mock.h"
#endif

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(dfu_case_internal_messages_t)
//LOGGING_PRESERVE_MESSAGE_ENUM(dfu_case_messages_t)

/* Ensure message range is legal */
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(DFU_CASE_INTERNAL_MESSAGE_END)
//ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(DFU_CASE, DFU_CASE_MESSAGE_END) To DO : create the group when first message is added to list

dfu_case_task_data_t dfu_case = {0};

/******************************************************************************
 * General Definitions
 ******************************************************************************/

/*! \brief Handle lid state change during case DFU
*/
static void dfuCase_HandleLidStateChangedInd(CASE_LID_STATE_T* message)
{
    if(DfuCase_FWIsDfuStarted() && DfuCase_FWIsDfuCheckReceived() && message->lid_state == CASE_LID_STATE_OPEN)
    {
        DEBUG_LOG("dfuCase_HandleLidStateChangedInd Aborting due to Lid opening");
        DfuCase_HostRequestAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1);
        DfuCase_FWCleanUp();
    }
}

/*! \brief Message Handler

    This function is the main message handler for dfu_case, every
    message is handled in it's own seperate handler function.
*/
static void dfuCase_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* Lid state changes */
        case CASE_LID_STATE:
            dfuCase_HandleLidStateChangedInd((CASE_LID_STATE_T*)message);
            break;

        /* dfu_peer connected the l2cap channel with peer device */
        case DFU_PEER_STARTED:
            if(DfuCase_HostIsCaseDfuConfirmed())
            {
                DEBUG_LOG("dfuCase_HandleMessage DFU_PEER_STARTED Peer Abort req sent");
                DfuPeer_ProcessHostMsg(UPGRADE_PEER_ABORT_REQ, (uint8)UPGRADE_ABORT);
            }
            break;

        default:
            DEBUG_LOG("dfuCase_HandleMessage. UNHANDLED Message 0x%x", id);
            break;
    }
}

/*! \brief Informs dfu_case_host and dfu_case_fw to start dfu and aborts peer DFU
*/
static void DfuCase_StartCaseDfu(void)
{
    DEBUG_LOG("DfuCase_StartCaseDfu");

    /* Peer device has already started the DFU process due to early erase being sent
     * so, if we are not already connected with peer then wait till we connect and
     * explicitly abort the peer DFU. */
    if(UpgradePeerIsConnected())
    {
        DEBUG_LOG("DfuCase_StartCaseDfu  Peer Abort req sent");
        DfuPeer_ProcessHostMsg(UPGRADE_PEER_ABORT_REQ, (uint8)UPGRADE_ABORT);
    }
    DfuCase_HostStartCaseDfu();

    if(!DfuCase_FWIsDfuStarted())
    {
        DfuCase_FWInitiateCaseDfu();
    }
}

bool DfuCase_EarlyInit(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("DfuCase_EarlyInit");
    TaskList_InitialiseWithCapacity(dfuCase_GetClientList(), DFU_CASE_CLIENT_LIST_INIT_CAPACITY);

    return TRUE;
}

/*! \brief Initialise dfu_case task

    Called at start up to initialise the dfu_case task
*/
bool DfuCase_Init(Task init_task)
{
    UNUSED(init_task);
    dfu_case_task_data_t *theDfuCase = dfuCase_GetTaskData();

    /* Set up task handler */
    theDfuCase->task.handler = dfuCase_HandleMessage;

    DfuCase_HostInit();
    DfuCase_FWInit();
    UpgradeSetCaseDfuSupport(TRUE);
    DfuPeer_ClientRegister(dfuCase_GetTask());
    CcWithCase_RegisterStateClient(dfuCase_GetTask());

#ifdef INCLUDE_DFU_CASE_MOCK
        DfuCase_MockInit();
#endif

    return TRUE;
}

/*! \brief Register any Client with dfu_case for messages
*/
void DfuCase_ClientRegister(Task tsk)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(dfuCase_GetClientList()), tsk);
}

/*! \brief Handle the case DFU related op. indication coming from upgrade lib.
*/
void DfuCase_HandleLibOperation(MessageId id, Message message)
{
    UNUSED(message);
    DEBUG_LOG("DfuCase_HandleLibOperation Message %d", id);

    switch (id)
    {
        case UPGRADE_CASE_DFU_CASE_IMAGE_IND:
        {
            DfuCase_StartCaseDfu();
        }
        break;

        case UPGRADE_CASE_DFU_EARBUD_IMAGE_IND:
        {
            DfuCase_FWCleanUp();
            DfuCase_HostHandleEarbudDfu();
        }
        break;

        case UPGRADE_CASE_DFU_RESUME_IND:
        {
            DfuCase_HostResumeCaseDfu();
        }
        break;

        case UPGRADE_CASE_DFU_CASE_ABORT:
        {
            DfuCase_HostAbortWithCase();
        }
        break;

        default:
            break;
    }


}

/*! \brief dfu_case_host should handle the DFU mode and dfu_case_fw should start the DFU.
*/
void DfuCase_HandleDfuMode(void)
{
    DEBUG_LOG("DfuCase_HandleDfuMode");

    DfuCase_HostHandleDfuMode();
    DfuCase_FWInitiateCaseDfu();
}

/*! \brief Handle the transport disconnection and pause the case DFU.
*/
void DfuCase_HandleTransportDisconnect(void)
{
    DEBUG_LOG("DfuCase_HandleTransportDisconnect");
    DfuCase_HostPauseCaseDfu();
}

/*! \brief Abort the case DFU if its on going.
*/
void DfuCase_Abort(void)
{
    DEBUG_LOG("DfuCase_Abort");
    DfuCase_FWCleanUp();
    DfuCase_HostCleanUp(TRUE);
}

bool DfuCase_IsDfuNeeded(void)
{
    return DfuCase_FWIsDfuStarted();
}

/*! \brief TRUE if we have requested the host to put earbuds in case and not confirmed .
*/
bool DfuCase_IsEarbudsInCaseRequested(void)
{
    return DfuCase_HostIsEarbudsInCaseRequested();
}

#endif /* INCLUDE_DFU_CASE */
