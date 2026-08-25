/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_fw.c
\brief      FW state machine of the dfu_case

    Handles the dfu Protocol messages comming from the Case
*/

#ifdef INCLUDE_DFU_CASE

#include "dfu_case_fw.h"
#include "dfu_case_fw_private.h"
#include "dfu_case_fw_if.h"
#include "dfu_case_host.h"
#include <upgrade.h>

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

/* Max S-record size allowed given the malloc-ed buffer size limit */
#define DFU_CASE_FW_MAX_SREC_SIZE (UPGRADE_MAX_FORWARD_PACKET_SIZE - DFU_CASE_FW_MSG_HEADER_SIZE)

static void dfuCase_FWHandleMessage(Task task, MessageId id, Message message);

TaskData dfu_case_fw = { dfuCase_FWHandleMessage };

static dfu_case_fw_context_t dfu_case_fw_context;

/*! Get the private context of dfu_case_fw */
#define dfuCase_FWGetContext() (&dfu_case_fw_context)

static void dfuCase_FWHandleMessage(Task task, MessageId id, Message message);

/*! \brief Get the dfu_case_fw FSM state
*/
static dfu_case_fw_state_t dfuCase_FWGetState(void)
{
    return dfuCase_FWGetContext()->state;
}

/*! \brief Set the dfu_case_fw FSM state
*/
static void dfuCase_FWSetState(dfu_case_fw_state_t new_state)
{
    dfuCase_FWGetContext()->state = new_state;
}

/*! \brief Update the context variables to process new incoming data

    \param msg Internal message containing the new incoming data
*/
static void dfuCase_FWResetNewDataBuffer(DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE_T* msg)
{
    dfuCase_FWGetContext()->new_data_buffer = msg->data;
    dfuCase_FWGetContext()->new_data_length = msg->len;
    dfuCase_FWGetContext()->new_data_buffer_offset = 0;
}

/*! \brief Search the first occurence of the S-record delimiter in the unporcessed data.

    \param record_length Buffer to return the length of the first S-record
    \return TRUE if an S-record delimiter is present in the unprocessed data
*/
static bool dfuCase_FWFindRecordEndInBuffer(uint16* record_length)
{

    uint8* location;
    location= (uint8*)memchr(&(dfuCase_FWGetContext()->new_data_buffer[dfuCase_FWGetContext()->new_data_buffer_offset]), 
        DFU_CASE_FW_SREC_DELIMITER,
        dfuCase_FWGetContext()->new_data_length - dfuCase_FWGetContext()->new_data_buffer_offset);
    if(location)
    {
        *record_length = 1 + (uint16)(location - &(dfuCase_FWGetContext()->new_data_buffer[dfuCase_FWGetContext()->new_data_buffer_offset]));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*! \brief Parse a new S-record and send it to case if a complete record exists

        We parse both complete and partial records in this fn. If we have already existing
        data in the incomplete data buffer then we copy the partial record from new data.
        If we don't have enough data then we copy it to a buffer and request more data.

    \return FALSE if any error occurs
*/
static bool dfuCase_FWParseRecord(void)
{
    uint16 record_length, total_packet_length;

    if(dfuCase_FWGetContext()->new_data_buffer_offset == dfuCase_FWGetContext()->new_data_length)
    {
        /* We are not able to parse a complete record from the available data
         * so request more data. */
        MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_REQUEST_MORE_DATA, NULL);

        return TRUE;
    }

    if(dfuCase_FWFindRecordEndInBuffer(&record_length))
    {
        /* Verify if there is enough buffer left to store the current (partial or complete) S-record */
        if(record_length > (DFU_CASE_FW_MAX_SREC_SIZE - dfuCase_FWGetContext()->incomplete_data_length))
        {
            /*wrong S-Record size */
            return FALSE;
        }
        memcpy(&(dfuCase_FWGetContext()->srec_buffer[dfuCase_FWGetContext()->incomplete_data_length]), 
                &(dfuCase_FWGetContext()->new_data_buffer[dfuCase_FWGetContext()->new_data_buffer_offset]), 
                record_length);

        DEBUG_LOG("dfuCase_FWParseRecord S%c",dfuCase_FWGetContext()->srec_buffer[DFU_CASE_FW_SREC_TYPE_FIELD_OFFSET]);

        /* Check if current S-record is the last one(S7). */
        dfuCase_FWGetContext()->is_last_srec_sent = 
            (dfuCase_FWGetContext()->srec_buffer[DFU_CASE_FW_SREC_TYPE_FIELD_OFFSET] == DFU_CASE_FW_LAST_SREC_TYPE);

        total_packet_length  = DFU_CASE_FW_MSG_HEADER_SIZE + record_length + dfuCase_FWGetContext()->incomplete_data_length;
        dfuCase_FWIFSendSRecToCase(dfuCase_FWGetContext()->dfu_data_buffer, total_packet_length);

        dfuCase_FWGetContext()->srec_size_in_process = record_length;
    }
    /* We have reached the end of available data and couldn't find the record end so, store the partial record and
     * request for more data. */
    else
    {
        uint16 remaining_size  = dfuCase_FWGetContext()->new_data_length - dfuCase_FWGetContext()->new_data_buffer_offset;

        /* Verify if there is enough buffer left to store the current partial S-record */
        if(remaining_size > (DFU_CASE_FW_MAX_SREC_SIZE - dfuCase_FWGetContext()->incomplete_data_length))
        {
            /*wrong S-Record size */
            return FALSE;
        }
        else
        {
            memcpy(&(dfuCase_FWGetContext()->srec_buffer[dfuCase_FWGetContext()->incomplete_data_length]), 
                &(dfuCase_FWGetContext()->new_data_buffer[dfuCase_FWGetContext()->new_data_buffer_offset]), 
                remaining_size);
            dfuCase_FWGetContext()->incomplete_data_length += remaining_size;

            /* We are not able to parse a complete record from the available data
             * so request more data. */
            MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_REQUEST_MORE_DATA, NULL);
        }
    }
    return TRUE;
}

/*! \brief Case has successfully received a record so update the context
*/
static void dfuCase_FWHandleRecordSent(void)
{
    dfuCase_FWGetContext()->incomplete_data_length = 0;
    dfuCase_FWGetContext()->new_data_buffer_offset += dfuCase_FWGetContext()->srec_size_in_process;
}

static bool dfuCase_FWHandleIdle(MessageId id, Message message)
{
    UNUSED(message);

    if(id >= DFU_CASE_FW_MSG_BASE)
    {
        /* Charger Case is sending messages but DFU is not in progress so, inform case to abort. */
        DfuCase_FWIFSendGeneralCommand(DFU_CASE_FW_ABORT);
        return TRUE;
    }
    return FALSE;
}

static bool dfuCase_FWHandleDfuCheck(MessageId id, Message message)
{
    switch(id)
    {
        case DFU_CASE_FW_CHECK:
        {
            DFU_CASE_FW_CHECK_T* msg = (DFU_CASE_FW_CHECK_T *) message;
            dfuCase_FWGetContext()->major_version = msg->major_version;
            dfuCase_FWGetContext()->minor_version = msg->minor_version;
            dfuCase_FWGetContext()->is_dfu_check_received = TRUE;
            MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_CHECK_RECEIVED, NULL);
        }
        break;

        case DFU_CASE_INTERNAL_DFU_COMPATIBLE:
        {
            DfuCase_FWIFSendGeneralCommand(DFU_CASE_FW_INITIATE);
            dfuCase_FWSetState(DFU_CASE_FW_STATE_DFU_STARTED);
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleDfuStarted(MessageId id, Message message)
{
    switch(id)
    {
        case DFU_CASE_FW_BUSY:
            /* Another DFU is already in progress. */
            dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, FALSE);
            return TRUE;

        case DFU_CASE_FW_READY:
        {
            /* Decide the bank image to upgrade and inform dfu_case_host that case is ready for DFU. */
            DFU_CASE_FW_READY_T* msg = (DFU_CASE_FW_READY_T *) message;
            dfuCase_FWGetContext()->bank_to_upgrade = (msg->current_bank == DFU_CASE_FW_BANK_A ? DFU_CASE_FW_BANK_B : DFU_CASE_FW_BANK_A);
            MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_CASE_READY_FOR_DATA, NULL);
            dfuCase_FWSetState(DFU_CASE_FW_STATE_PARSE_HEADER);
        }
        break;
        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleParseHeader(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE:
        {
            dfuCase_FWResetNewDataBuffer((DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE_T *) message);

            /* Parse an S0 record from the available new data */
            if(!dfuCase_FWParseRecord())
            {
                /* Error during parsing. */
                dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, TRUE);
                return TRUE;
            }
        }
        break;

        case DFU_CASE_FW_ACK:
        {
            dfuCase_FWHandleRecordSent();

            /* S0 record has been sent successfully so move to next state. Also, start the
             * timer for case to send the START message (request for data xfer stage). */
#ifndef HOSTED_TEST_ENVIRONMENT
            MessageSendLater(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST, NULL, 
                            DFU_CASE_FW_IF_NEXT_STAGE_REQUEST_TIME_LIMIT);
#endif
            dfuCase_FWSetState(DFU_CASE_FW_STATE_START_DATA_TRANSFER);
            DEBUG_LOG("dfuCase_FWHandleParseHeader Timer started for START message");
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleStartDataTransfer(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_FW_START:
        {
            MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST);

            /* Parse an S3 record. */
            if(!dfuCase_FWParseRecord())
            {
                /* Error during parsing. */
                dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, TRUE);
                return TRUE;
            }

            /* We have either sent the first record or requested more data. In anycase,
             * data transfer has started so, move to next state. */
            dfuCase_FWSetState(DFU_CASE_FW_STATE_PARSE_DATA);
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleParseData(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_FW_ACK:
            dfuCase_FWHandleRecordSent();
            /* If we have sent the last message then move to the next
             * state */
            if(dfuCase_FWGetContext()->is_last_srec_sent)
            {
                /* Start the timer for case to send the CHECKSUM message (request for validation stage). */
#ifndef HOSTED_TEST_ENVIRONMENT
                MessageSendLater(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST, NULL, 
                                DFU_CASE_FW_IF_NEXT_STAGE_REQUEST_TIME_LIMIT);
#endif
                DEBUG_LOG("dfuCase_FWHandleParseData Timer started for CHECKSUM message");

                dfuCase_FWSetState(DFU_CASE_FW_STATE_VERIFY_CHECKSUM);
                return TRUE;
            }
            break;

        case DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE:
            dfuCase_FWResetNewDataBuffer((DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE_T *) message);
            break;

        default:
            return FALSE;
    }

    /* Try to parse a new record on events like ack of a record, more data available etc. */
    if(!dfuCase_FWParseRecord())
    {
        /* Error during parsing. */
        dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, TRUE);
        return TRUE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleVerifyChecksum(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_FW_CHECKSUM:
        {
            MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST);

            /* The checksum has been verified, inform dfu_case_host. */
            MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_CHECKSUM_VERIFIED, NULL);
        }
        break;

        case DFU_CASE_INTERNAL_REBOOT_CASE:
        {
            /* Start the timer for case to send the VERIFY message (request for commit stage). */
#ifndef HOSTED_TEST_ENVIRONMENT
            MessageSendLater(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST, NULL, 
                            DFU_CASE_FW_IF_REBOOT_REQUEST_TIME_LIMIT);
#endif
            DEBUG_LOG("dfuCase_FWHandleVerifyChecksum Timer started for VERIFY message");

            DfuCase_FWIFSendGeneralCommand(DFU_CASE_FW_REBOOT);
            dfuCase_FWSetState(DFU_CASE_FW_STATE_VERIFY_REBOOT);
        }
        break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleVerifyReboot(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_FW_VERIFY:
            MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST);

            MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_CASE_REBOOTED, NULL);
            dfuCase_FWGetContext()->is_case_rebooted = TRUE;
            break;

        case DFU_CASE_INTERNAL_COMMIT_UPGRADE:
            DfuCase_FWIFSendGeneralCommand(DFU_CASE_FW_COMMIT);
            dfuCase_FWSetState(DFU_CASE_FW_STATE_COMMIT);
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWHandleCommit(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_FW_COMPLETE:
            MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_UPGRADE_COMPLETE, NULL);
            DfuCase_FWCleanUp();
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

static bool dfuCase_FWDefaultHandler(MessageId id, Message message)
{
    UNUSED(message);

    switch(id)
    {
        case DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE:
            DfuCase_FWIFHandleTimeoutCaseResponse();
        case DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST:
            /* Time out happened due to case failing to reply so, Abort. */
            dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, FALSE);
            break;

        case DFU_CASE_INTERNAL_FW_ABORT:
        {
            DFU_CASE_INTERNAL_FW_ABORT_T* msg = (DFU_CASE_INTERNAL_FW_ABORT_T *) message;
            if(msg->inform_case && dfuCase_FWGetContext()->is_dfu_check_received)
            {
                DfuCase_FWIFSendGeneralCommand(DFU_CASE_FW_ABORT);
                /* This internal message is coming from dfu_case_host so, there can be queued
                 * messages for this task only from dfu_case_fw_if which are not important now 
                 * as we are aborting so, flush them. */
                MessageFlushTask(dfuCase_FWGetTask());
            }
            DfuCase_FWCleanUp();
        }
        break;

        default:
        {
            if(id >= DFU_CASE_FW_MSG_BASE)
            {
                /* Abort, case has sent a out of sequence message */
                dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, TRUE);
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

/*! \brief Message Handler

    This handler will handle internal and DFU_FW_xx messages.
*/
static void dfuCase_FWHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    bool handled = FALSE;

    if(id < DFU_CASE_INTERNAL_MESSAGE_END)
    {
        DEBUG_LOG("dfuCase_FWHandleMessage, state enum:dfu_case_fw_state_t:%u, message_id enum:dfu_case_internal_messages_t:%d", dfuCase_FWGetState(), id);
    }
    else
    {
        DEBUG_LOG("dfuCase_FWHandleMessage, state enum:dfu_case_fw_state_t:%u, message_id enum:dfu_case_fw_message_t:%d", dfuCase_FWGetState(), id);
    }

    switch (dfuCase_FWGetState())
    {
    case DFU_CASE_FW_STATE_IDLE:
        handled = dfuCase_FWHandleIdle(id, message);
        break;

    case DFU_CASE_FW_STATE_DFU_CHECK:
        handled = dfuCase_FWHandleDfuCheck(id, message);
        break;

    case DFU_CASE_FW_STATE_DFU_STARTED:
        handled = dfuCase_FWHandleDfuStarted(id, message);
        break;

    case DFU_CASE_FW_STATE_PARSE_HEADER:
        handled = dfuCase_FWHandleParseHeader(id, message);
        break;

    case DFU_CASE_FW_STATE_START_DATA_TRANSFER:
        handled = dfuCase_FWHandleStartDataTransfer(id, message);
        break;

    case DFU_CASE_FW_STATE_PARSE_DATA:
        handled = dfuCase_FWHandleParseData(id, message);
        break;

    case DFU_CASE_FW_STATE_VERIFY_CHECKSUM:
        handled = dfuCase_FWHandleVerifyChecksum(id, message);
        break;

    case DFU_CASE_FW_STATE_VERIFY_REBOOT:
        handled = dfuCase_FWHandleVerifyReboot(id, message);
        break;

    case DFU_CASE_FW_STATE_COMMIT:
        handled = dfuCase_FWHandleCommit(id, message);
        break;

    default:
        DEBUG_LOG("dfuCase_FWHandleMessage, unknown state %u", dfuCase_FWGetState());
        break;
    }

    if (!handled)
    {
        handled = dfuCase_FWDefaultHandler(id, message);
    }

    if (!handled)
    {
        DEBUG_LOG("dfuCase_FWHandleMessage: MESSAGE:0x%04x not handled", id);
    }

    DEBUG_LOG("dfuCase_FWHandleMessage, new state enum:dfu_case_fw_state_t:%u", dfuCase_FWGetState());
}

/*! \brief Initialise dfu_case_fw
*/
bool DfuCase_FWInit(void)
{
    memset(dfuCase_FWGetContext(), 0, sizeof(dfu_case_fw_context_t));

    dfuCase_FWSetState(DFU_CASE_FW_STATE_IDLE);

    DfuCase_FWIFRegisterDFUChannel();

    return TRUE;
}

/*! \brief Initialze the dfu_case_fw for an upcoming DFU
*/
void DfuCase_FWInitiateCaseDfu(void)
{
    dfuCase_FWGetContext()->dfu_data_buffer= PanicUnlessMalloc(UPGRADE_MAX_FORWARD_PACKET_SIZE);
    dfuCase_FWGetContext()->srec_buffer = &dfuCase_FWGetContext()->dfu_data_buffer[DFU_CASE_FW_MSG_HEADER_SIZE];
    dfuCase_FWGetContext()->incomplete_data_length = 0;

    dfuCase_FWSetState(DFU_CASE_FW_STATE_DFU_CHECK);
}

/*! \brief Abort the DFU with case if needed and cleanup
*/
void dfuCase_FWAbort(UpgradeHostErrorCode error_code, bool inform_case)
{
    if(inform_case)
    {
        DfuCase_FWIFSendGeneralCommand(DFU_CASE_FW_ABORT);
    }

    /* If state is Idle then DFU is not in progress so no need to inform dfu_case_host. */
    if(dfuCase_FWGetState() > DFU_CASE_FW_STATE_IDLE)
    {
        MESSAGE_MAKE(msg, DFU_CASE_INTERNAL_HOST_ABORT_T);
        msg->error_code = error_code;
        
        MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_HOST_ABORT, msg);
    }

    DfuCase_FWCleanUp();
}

/*! \brief Has case DFU already started
*/
bool DfuCase_FWIsDfuStarted(void)
{
    return (dfuCase_FWGetState() > DFU_CASE_FW_STATE_IDLE);
}

/*! \brief Has case DFU data transfer started
*/
bool DfuCase_FWIsDfuInDataTransfer(void)
{
    return (dfuCase_FWGetState() > DFU_CASE_FW_STATE_DFU_CHECK);
}

/*! \brief Has CHECK message from case been received.
*/
bool DfuCase_FWIsDfuCheckReceived(void)
{
    return (dfuCase_FWGetContext()->is_dfu_check_received);
}

/*! \brief Has VERIFY message from case been received after reboot.
*/
bool DfuCase_FWIsCaseRebooted(void)
{
    return (dfuCase_FWGetContext()->is_case_rebooted);
}

/*! \brief Get the image bank of the case which needs to be updated.

    \return image bank which needs to be updated
*/
dfu_case_fw_bank_t DfuCase_FWGetBankToUpgrade(void)
{
    return (dfuCase_FWGetContext()->bank_to_upgrade);
}

/*! \brief Clean up after DFU comlpletes or aborts
*/
void DfuCase_FWCleanUp(void)
{
    MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE);
    MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST);

    if(dfuCase_FWGetContext()->dfu_data_buffer)
    {
        free(dfuCase_FWGetContext()->dfu_data_buffer);
    }
    memset(dfuCase_FWGetContext(), 0, sizeof(dfu_case_fw_context_t));
    dfuCase_FWIFCleanUp();

    dfuCase_FWSetState(DFU_CASE_FW_STATE_IDLE);
}
#endif /* DFU_CASE_HOST_H_ */
