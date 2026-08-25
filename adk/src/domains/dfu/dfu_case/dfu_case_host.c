/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_host.c
\brief      host state machine of the dfu_case

    Handles the Upgrade Protocol messages comming from the Host
*/

#ifdef INCLUDE_DFU_CASE

#include "dfu_case_host.h"
#include "dfu_case_host_private.h"
#include "dfu_case_private.h"
#include "dfu_case_fw.h"
#include "dfu_case_data.h"
#include "dfu_case.h"
#include "../dfu_config.h"

#include <util.h>
#include <service.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stream.h>
#include <source.h>
#include <sink.h>

static void dfuCase_HostHandleMessage(Task task, MessageId id, Message message);

TaskData dfu_case_host = { dfuCase_HostHandleMessage };

static dfu_case_host_context_t dfu_case_host_context;

/*! Get the private context of dfu_case_host */
#define  dfuCase_HostGetContext() (&dfu_case_host_context)

/*! \brief Get the dfu_case_host FSM state
    \return current state
*/
static dfu_case_host_state_t dfuCase_HostGetState(void)
{
    return dfuCase_HostGetContext()->state;
}

/*! \brief Set the dfu_case_host FSM state
    \param new_state state to change
*/
static void dfuCase_HostSetState(dfu_case_host_state_t new_state)
{
    dfuCase_HostGetContext()->state = new_state;
}

/*! \brief TRUE if the queue is not empty.
*/
static bool dfuCase_HostIsDataQueued(void)
{
    return dfuCase_HostGetContext()->queue_size;
}

/*! \brief Add new element in the queue at the tail
    \param msg new message to store
*/
static void dfuCase_HostQueueData(UPGRADE_HOST_DATA_WITH_ACK_T* msg)
{
    dfu_case_host_context_t* ctx = dfuCase_HostGetContext();

    DEBUG_LOG("dfuCase_HostQueueData data queued");

    /* Check the capacity of the queue and increase it if needed. */
    if(!ctx->queue_max_size || ctx->queue_max_size == ctx->queue_size)
    {
        uint16 cnt_new = 0;
        uint16 cnt_old;
        UPGRADE_HOST_DATA_WITH_ACK_T** new_queue;

        ctx->queue_max_size = ctx->queue_max_size? ctx->queue_max_size*2: 4;
        new_queue = PanicUnlessMalloc(sizeof(UPGRADE_HOST_DATA_WITH_ACK_T*)*ctx->queue_max_size);

        DEBUG_LOG("dfuCase_HostQueueData max size increased to %d", ctx->queue_max_size);

        /* Copy the elements from the old queue head to the end of the array in the new queue. */
        for(cnt_old = ctx->queue_head; cnt_old < ctx->queue_size; cnt_old++, cnt_new++)
        {
            new_queue[cnt_new] = ctx->data_queue[cnt_old];
        }

        /* Copy the elements from start of the old array to the tail of queue in the new queue. */
        for(cnt_old = 0; cnt_old < ctx->queue_tail; cnt_old++, cnt_new++)
        {
            new_queue[cnt_new] = ctx->data_queue[cnt_old];
        }

        ctx->queue_head = 0;
        ctx->queue_tail = ctx->queue_size;
        if(ctx->data_queue)
        {
            free(ctx->data_queue);
        }
        ctx->data_queue = new_queue;
    }

    ctx->data_queue[ctx->queue_tail] = msg;
    ctx->queue_tail = (ctx->queue_tail +1) % ctx->queue_max_size;
    ctx->queue_size++;
}

/*! \brief Remove and return an element from the head of the queue
    \return stored message
*/
static UPGRADE_HOST_DATA_WITH_ACK_T* dfuCase_HostDequeueData(void)
{
    dfu_case_host_context_t* ctx = dfuCase_HostGetContext();
    UPGRADE_HOST_DATA_WITH_ACK_T* msg;
    if(ctx->queue_size)
    {
        msg = ctx->data_queue[ctx->queue_head];
        ctx->queue_head = (ctx->queue_head + 1) % ctx->queue_max_size;
        ctx->queue_size--;
        return msg;
    }
    return NULL;
}

/*! \brief Start the Abort sequence from dfu_case_host.

            Informs dfu_case_fw along with aborting case DFU on host side.
*/
static void DfuCase_HostAbort(UpgradeHostErrorCode error_code, bool inform_case)
{
    if(dfuCase_HostGetState() != DFU_CASE_HOST_STATE_ABORT)
    {
        MESSAGE_MAKE(msg, DFU_CASE_INTERNAL_FW_ABORT_T);
        msg->inform_case = inform_case;

        /* Inform dfu_case_fw to abort on case side. */
        MessageFlushTask(dfuCase_FWGetTask());
        MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_FW_ABORT, msg);
        DfuCase_HostRequestAbort(error_code);
    }

}

/*! \brief dfuCase_HostRequestEarbudsInCase

        Request host to put earbuds in case, inform application about this request and start a timer to abort
        if host fails to take action in the time limit.
*/
static void dfuCase_HostRequestEarbudsInCase(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(dfuCase_GetClientList()), DFU_CASE_EARBUDS_IN_CASE_REQUSETED);
    dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_PUT_EARBUDS_IN_CASE_REQ);
    dfuCase_HostGetContext()->is_in_case_cfm_required = TRUE;
#ifndef HOSTED_TEST_ENVIRONMENT
    MessageSendLater(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_TIMEOUT_OUT_CASE_DFU,
                        NULL, dfuConfigCaseDfuTimeoutToPutInCaseMs());
#endif
}

/*! \brief dfuCase_HostConfirmEarbudsInCase

        Confrirm to host that earbuds are now in case, inform application about the same and cancel the timer.
*/
static void dfuCase_HostConfirmEarbudsInCase(void)
{
    if(dfuCase_HostGetContext()->is_in_case_cfm_required)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(dfuCase_GetClientList()), DFU_CASE_EARBUDS_IN_CASE_CONFIRM);
        dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_EARBUDS_IN_CASE_CFM);
        dfuCase_HostGetContext()->is_in_case_cfm_required = FALSE;
        MessageCancelAll(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_TIMEOUT_OUT_CASE_DFU);
    }
}

static bool dfuCase_HostHandleIdle(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_INTERNAL_TRANSPORT_CONNECTED:
        {
            /* Transport has connected and case DFU has started while earbuds are out of case
             * so, we need them in case to continue. */
            dfuCase_HostRequestEarbudsInCase();

            dfuCase_HostSetState(DFU_CASE_HOST_STATE_CONNECTED);
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostHandleWaitingForTransport(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_INTERNAL_TRANSPORT_CONNECTED:
        {
            if(DfuCase_FWIsDfuCheckReceived())
            {
                dfuCase_DataStartDataTransfer();
                if(UpgradeTransportInUse())
                {
                    dfuCase_HostGetContext()->resp_funcs->SendBytesReq(DfuCase_DataGetNextReqSize(), DfuCase_DataGetNextOffset());
                }
                dfuCase_HostSetState(DFU_CASE_HOST_STATE_DATA_TRANSFER);
            }
            else
            {
                /* User had enabled the in case DFU mode but user started the case DFU before 
                 * putting earbuds in case. */
                dfuCase_HostRequestEarbudsInCase();

                dfuCase_HostSetState(DFU_CASE_HOST_STATE_CONNECTED);
            }
        }
        break;

        case DFU_CASE_INTERNAL_CHECK_RECEIVED:
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostHandleConnected(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case UPGRADE_HOST_START_DATA_REQ:
            UpgradeTransportDataCfm();
            dfuCase_DataCalculateResumeOffset();
            break;

        case DFU_CASE_INTERNAL_CHECK_RECEIVED:
            dfuCase_DataStartDataTransfer();
            if(UpgradeTransportInUse())
            {
                /* We had requested the user to put earbuds in case when DFU started and now earbuds 
                 * have been put in case so, confirm this to host */
                dfuCase_HostConfirmEarbudsInCase();

                dfuCase_HostGetContext()->resp_funcs->SendBytesReq(DfuCase_DataGetNextReqSize(), DfuCase_DataGetNextOffset());
            }
            dfuCase_HostSetState(DFU_CASE_HOST_STATE_DATA_TRANSFER);
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostHandleDataTransfer(MessageId id, Message message)
{
    switch(id)
    {
        case UPGRADE_HOST_DATA:
            if(message)
            {
                UPGRADE_HOST_DATA_WITH_ACK_T *msg = (UPGRADE_HOST_DATA_WITH_ACK_T *)message;
                UpgradeHostErrorCode rc;

                if(msg->dataCfm)
                {
                    /* If dataCfm field is non-zero that means the message is coming from the library and not the
                     * queue. We need to schedule the UPGRADE_TRANSPORT_DATA_CFM message. */
                    UpgradeTransportScheduleDataCfm(msg->dataCfm, msg->transportTask);
                }

                /* If there is already some data in process then we can't process this new packet right away. */
                if(dfuCase_HostGetContext()->is_data_in_process)
                {
                    if(UpgradeTransportInUse())
                    {
                        /* If transport is still connected then we can queue the incoming data packet to be processed 
                         * next. Otherwise, we can ignore the packet. It will be requested again when transport reconnects. */
                        MESSAGE_MAKE(q_msg, UPGRADE_HOST_DATA_WITH_ACK_T);
                        q_msg->length = msg->length;
                        q_msg->lastPacket = msg->lastPacket;
                        q_msg->data = msg->data;

                        /* The Cfm has already been scheduled and message will be added to the queue so, clear the dataCfm field. */
                        q_msg->dataCfm = NULL;

                        dfuCase_HostQueueData(q_msg);
                    }
                    break;
                }
                /* If there is no data in process and the transport has been disconnected then just send the confirmation. */
                else if(!UpgradeTransportInUse())
                {
                    UpgradeTransportDataCfm();
                    break;
                }

                dfuCase_HostGetContext()->is_data_in_process = TRUE;

                rc = dfuCase_DataParse(msg->data, msg->length);
                DEBUG_LOG("dfuCase_HostHandleDataTransfer rc enum:UpgradeHostErrorCode:%u", rc);

                /* Check for upgrade file size errors */
                if(rc == UPGRADE_HOST_SUCCESS && msg->lastPacket)
                {
                    rc = UPGRADE_HOST_ERROR_FILE_TOO_SMALL;
                }
                else if(rc == UPGRADE_HOST_DATA_TRANSFER_COMPLETE && !msg->lastPacket)
                {
                    rc = UPGRADE_HOST_ERROR_FILE_TOO_BIG;
                }

                if(rc == UPGRADE_HOST_DATA_TRANSFER_COMPLETE)
                {
                    DEBUG_LOG_INFO("dfuCase_HostHandleDataTransfer transfer complete");
                    /* Confirm the successful reception of previous message. */
                    UpgradeTransportDataCfm();

                    /* Data transfer has completed so change the resume point for validation. */
                    UpgradeSetResumePoint(UPGRADE_RESUME_POINT_PRE_VALIDATE);
                }
                else if(rc != UPGRADE_HOST_SUCCESS)
                {
                    DfuCase_HostAbort(rc, TRUE);
                    return TRUE;
                }
            }
            break;

        case UPGRADE_HOST_START_DATA_REQ:
            /* This confirmation might be required if a link-loss happened after the host 
             * started the case DFU and if, user put earbuds in case during that time. */
            dfuCase_HostConfirmEarbudsInCase();

            dfuCase_DataCalculateResumeOffset();
            /* Let it fall through to start data transfer after transport reconnection. */

        case DFU_CASE_INTERNAL_REQUEST_MORE_DATA:

        /* Case has received all data and verified the checksum so, we need to request the
         * footer to inform the host that data transfer is complete. 
         */
        case DFU_CASE_INTERNAL_CHECKSUM_VERIFIED:
        {
            dfuCase_HostGetContext()->is_data_in_process = FALSE;
            if(dfuCase_HostIsDataQueued())
            {
                /* Handle the next data message from queue. */
                UPGRADE_HOST_DATA_WITH_ACK_T* q_msg = dfuCase_HostDequeueData();
                dfuCase_HostHandleMessage(dfuCase_HostGetTask(), UPGRADE_HOST_DATA, q_msg);
                free(q_msg);
                break;
            }
            uint32 req_size = DfuCase_DataGetNextReqSize();
            
            /* Confirm the successful reception of previous message. */
            UpgradeTransportDataCfm();

            if(req_size && UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendBytesReq(req_size, DfuCase_DataGetNextOffset());
            }
            else
            {
                DEBUG_LOG_VERBOSE("dfuCase_HostHandleDataTransfer, no more bytes to request");
            }
        }
        break;

        case DFU_CASE_INTERNAL_CASE_READY_FOR_DATA:
        {
            DfuCase_DataHandleCaseReady();
        }
        break;

        case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
        {
            UpgradeTransportDataCfm();
            UpgradeSetResumePoint(UPGRADE_RESUME_POINT_PRE_REBOOT);

            /* No validation is required for the case DFU so we can inform the host of transfer complete. */
            if(UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
            }
            dfuCase_HostSetState(DFU_CASE_HOST_STATE_CONFIRM_REBOOT);
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostHandleConfirmReboot(MessageId id, Message message)
{
    switch(id)
    {
        case UPGRADE_HOST_TRANSFER_COMPLETE_RES:
        {
            UPGRADE_HOST_TRANSFER_COMPLETE_RES_T *msg = (UPGRADE_HOST_TRANSFER_COMPLETE_RES_T *)message;

            UpgradeTransportDataCfm();

            /* Interactive Commit */
            if(msg !=NULL && msg->action == UPGRADE_COMMIT_INTERACTIVE)
            {
                DEBUG_LOG("dfuCase_HostHandleConfirmReboot Interactive Commit");
                MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_REBOOT_CASE, NULL);
                UpgradeSetResumePoint(UPGRADE_RESUME_POINT_POST_REBOOT);
                dfuCase_HostSetState(DFU_CASE_HOST_STATE_CONFIRM_COMMIT);
            }
            /* Silent Commit */
            else if(msg !=NULL && msg->action == UPGRADE_COMMIT_SILENT)
            {
                /* Silent commit is not supported currently for case DFU. */
                DfuCase_HostAbort(UPGRADE_HOST_ERROR_SILENT_COMMIT_NOT_SUPPORTED, TRUE);
                return TRUE;
            }
            /* Action ABORT will be followed by an ABORT_REQ so, no need to handle here. */
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostHandleConfirmCommit(MessageId id, Message message)
{
    switch(id)
    {
        case UPGRADE_HOST_IN_PROGRESS_RES:
            UpgradeTransportDataCfm();
            if(!DfuCase_FWIsCaseRebooted())
            {
                /* We can arrive here if transport reconnects while case is rebooting.
                 * Wait for case to reboot. */
                break;
            }
            /* Case has rebooted and transport has reconnected so let it fall through to move forward. */

        case DFU_CASE_INTERNAL_CASE_REBOOTED:
            if(UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_COMMIT_REQ);
            }
            break;


        case UPGRADE_HOST_COMMIT_CFM:
        {
            UPGRADE_HOST_COMMIT_CFM_T *cfm = (UPGRADE_HOST_COMMIT_CFM_T *)message;

            uint8 action = (uint8)cfm->action;
            DEBUG_LOG("dfuCase_HostHandleConfirmCommit UPGRADE_HOST_COMMIT_CFM action %d", action);

            UpgradeTransportDataCfm();
            if(cfm->action == UPGRADE_HOSTACTION_YES)
            {
                MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_COMMIT_UPGRADE, NULL);
            }
            /* Action NO will be followed by an ABORT_REQ so, no need to handle here. */
        }
        break;

        case DFU_CASE_INTERNAL_UPGRADE_COMPLETE:
            if(UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_COMPLETE_IND);
            }
            DfuCase_HostCleanUp(FALSE);
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostHandleAbort(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case UPGRADE_HOST_ERRORWARN_RES:
            UpgradeTransportDataCfm();
            break;

        case UPGRADE_HOST_ABORT_REQ:
        {
            UpgradeTransportDataCfm();
            if(UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
            }
            DfuCase_HostCleanUp(TRUE);
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_HostDefaultHandler(MessageId id, Message message)
{
    DEBUG_LOG("dfuCase_HostDefaultHandler message_id %d", id);

    switch(id)
    {
        case UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_REQ:
        {
            UpgradeTransportDataCfm();

            /* Silent commit is not supported by case dfu yet. */
            if(UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendSilentCommitSupportedCfm(FALSE);
            }
        }
        break;

        case DFU_CASE_INTERNAL_TIMEOUT_OUT_CASE_DFU:
        {
            DfuCase_HostAbort(UPGRADE_HOST_ERROR_TIME_OUT, TRUE);
        }
        break;

        case UPGRADE_HOST_ABORT_REQ:
        {
            UpgradeTransportDataCfm();
            if(UpgradeTransportInUse())
            {
                dfuCase_HostGetContext()->resp_funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
            }

            MESSAGE_MAKE(msg, DFU_CASE_INTERNAL_FW_ABORT_T);
            msg->inform_case = TRUE;

            MessageFlushTask(dfuCase_FWGetTask());
            MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_FW_ABORT, msg);
            DfuCase_HostCleanUp(TRUE);
        }
        break;

        case DFU_CASE_INTERNAL_HOST_ABORT:
        {
            DFU_CASE_INTERNAL_HOST_ABORT_T* msg = (DFU_CASE_INTERNAL_HOST_ABORT_T *) message;
            DfuCase_HostRequestAbort(msg->error_code);
        }
        break;

        case UPGRADE_HOST_DATA:
        {
            /* dfu_case_host is not in a state to handle this message so, send confirmation for this data 
             * to transport and ignore the data. */
            UPGRADE_HOST_DATA_WITH_ACK_T *msg = (UPGRADE_HOST_DATA_WITH_ACK_T *)message;
            if(msg->dataCfm)
            {
                UpgradeTransportScheduleDataCfm(msg->dataCfm, msg->transportTask);
            }
            UpgradeTransportDataCfm();
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

/*! \brief Message Handler for the dfu_case_host FSM
*/
static void dfuCase_HostHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    bool handled = FALSE;

    if(id < DFU_CASE_INTERNAL_MESSAGE_END)
    {
        DEBUG_LOG("dfuCase_HostHandleMessage, state enum:dfu_case_host_state_t:%u, message_id enum:dfu_case_internal_messages_t:%d", dfuCase_HostGetState(), id);
    }
    else
    {
        DEBUG_LOG("dfuCase_HostHandleMessage, state enum:dfu_case_host_state_t:%u, message_id enum:UpgradeMsgHost:%d", dfuCase_HostGetState(), id);
    }

    switch (dfuCase_HostGetState())
    {
    case DFU_CASE_HOST_STATE_IDLE:
        handled = dfuCase_HostHandleIdle(id, message);
        break;

    case DFU_CASE_HOST_STATE_WAITING_FOR_TRANSPORT:
        handled = dfuCase_HostHandleWaitingForTransport(id, message);
        break;

    case DFU_CASE_HOST_STATE_CONNECTED:
        handled = dfuCase_HostHandleConnected(id, message);
        break;

    case DFU_CASE_HOST_STATE_DATA_TRANSFER:
        handled = dfuCase_HostHandleDataTransfer(id, message);
        break;

    case DFU_CASE_HOST_STATE_CONFIRM_REBOOT:
        handled = dfuCase_HostHandleConfirmReboot(id, message);
        break;

    case DFU_CASE_HOST_STATE_CONFIRM_COMMIT:
        handled = dfuCase_HostHandleConfirmCommit(id, message);
        break;

    case DFU_CASE_HOST_STATE_ABORT:
        handled = dfuCase_HostHandleAbort(id, message);
        break;

    default:
        DEBUG_LOG("dfuCase_HostHandleMessage, unknown state %u", dfuCase_HostGetState());
        break;
    }

    if (!handled)
    {
        handled = dfuCase_HostDefaultHandler(id, message);
    }

    if (!handled)
    {
        DEBUG_LOG("dfuCase_HostHandleMessage: MESSAGE:0x%04x not handled", id);
    }

    DEBUG_LOG("dfuCase_HostHandleMessage, new state enum:dfu_case_host_state_t:%u", dfuCase_HostGetState());
}

/*! \brief Clean up the case DFU and abort DFU process in charger case side

        Clean up the case DFU along with aborting the DFU process in 
        charger case side
*/
void DfuCase_HostAbortWithCase(void)
{
    DEBUG_LOG_INFO("DfuCase_HostAbortWithCase Abort and Cleanup");

    /* Prepare the Abort Msg for aborting the charger case DFU */
    MESSAGE_MAKE(msg, DFU_CASE_INTERNAL_FW_ABORT_T);
    msg->inform_case = TRUE;
    MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_FW_ABORT, msg);

    DfuCase_HostCleanUp(TRUE);
}

/*! \brief Initialise dfu_case_host
*/
bool DfuCase_HostInit(void)
{
    memset(dfuCase_HostGetContext(), 0, sizeof(dfu_case_host_context_t));

    /* Get the utility functions related to host interface. */
    dfuCase_HostGetContext()->resp_funcs = UpgradeGetFPtr();

    dfuCase_HostSetState(DFU_CASE_HOST_STATE_IDLE);

    return TRUE;
}

/*! \brief Start the Case DFU on Host side

        Setup the upgrade lib for case DFU and inject the event for
        transport connection
*/
void DfuCase_HostStartCaseDfu(void)
{
    dfuCase_HostGetContext()->is_case_file_detected = TRUE;
    UpgradeClientConnect(dfuCase_HostGetTask());
    DfuCase_DataInit();
    MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_TRANSPORT_CONNECTED, NULL);
}

/*! \brief Set the state to wait for transport connection.
*/
void DfuCase_HostHandleDfuMode(void)
{
    dfuCase_HostSetState(DFU_CASE_HOST_STATE_WAITING_FOR_TRANSPORT);
}

/*! \brief User selected earbud file during in case DFU.
*/
void DfuCase_HostHandleEarbudDfu(void)
{
    dfuCase_HostSetState(DFU_CASE_HOST_STATE_IDLE);
}

/*! \brief  Pause the data transfer with the host due to transport linkloss.
*/
void DfuCase_HostPauseCaseDfu(void)
{
    UpgradeClientReconnectLib();
}

/*! \brief Resume the data transfer with the host after transport reconnection.
*/
void DfuCase_HostResumeCaseDfu(void)
{
    UpgradeClientConnect(dfuCase_HostGetTask());
}

/*! \brief Confirm that DFU file indeed contains a case image.
*/
bool DfuCase_HostIsCaseDfuConfirmed(void)
{
    return dfuCase_HostGetContext()->is_case_file_detected;
}

/*! \brief TRUE if we have requested the host to put earbuds in case and not confirmed .
*/
bool DfuCase_HostIsEarbudsInCaseRequested(void)
{
    return dfuCase_HostGetContext()->is_in_case_cfm_required;
}

/*! \brief Clean up after DFU completes or aborts
*/
void DfuCase_HostCleanUp(bool isError)
{
    MessageCancelAll(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_TIMEOUT_OUT_CASE_DFU);
    DfuCase_DataCleanUp();
    UpgradeCleanUpCaseDfu(isError);

    if(dfuCase_HostGetContext()->data_queue)
    {
        free(dfuCase_HostGetContext()->data_queue);
    }

    memset(dfuCase_HostGetContext(), 0, sizeof(dfu_case_host_context_t));

    /* Get the utility functions related to host interface. */
    dfuCase_HostGetContext()->resp_funcs = UpgradeGetFPtr();

    dfuCase_HostSetState(DFU_CASE_HOST_STATE_IDLE);
}

/*! \brief Send error indication to host if present otherwise cleanup on host side.

            If error indication is sent than cleanup will happen when host replies with an ABORT_REQ.
*/
void DfuCase_HostRequestAbort(UpgradeHostErrorCode error_code)
{
    DEBUG_LOG("DfuCase_HostRequestAbort enum:UpgradeHostErrorCode:%d ", error_code);

    if(dfuCase_HostGetState() != DFU_CASE_HOST_STATE_ABORT)
    {
        UpgradeTransportDataCfm();

        if(UpgradeTransportInUse())
        {
            dfuCase_HostGetContext()->resp_funcs->SendErrorInd((uint16)error_code);
            dfuCase_HostSetState(DFU_CASE_HOST_STATE_ABORT);

            UpgradeSetResumePoint(UPGRADE_RESUME_POINT_ERROR);
        }
        else
        {
            DfuCase_HostCleanUp(TRUE);
        }
    }
}

#endif /* DFU_CASE_HOST_H_ */
