/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_fw_if.c
\brief      implementation of the interface with case

    Handles the dfu Protocol messages comming from the Case
*/

#ifdef INCLUDE_DFU_CASE


#include <cc_protocol.h>
#include "dfu_case_fw_if.h"

#include <util.h>
#include <service.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include <stdio.h>
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include <byte_utils.h>

#ifdef INCLUDE_DFU_CASE_MOCK
#include "dfu_case_mock.h"
#endif

/* Time limit for the case to respond to a request. */
#define DFU_CASE_FW_IF_CASE_RESPONSE_TIME_LIMIT 2000

/* Max attempts to retry if CaseComms tx fails. */
#define DFU_CASE_FW_IF_MAX_RETRY_ATTEMPTS 2

#define DFU_CASE_FW_MSG_HEADER_ID_FIELD 0
#define DFU_CASE_FW_MSG_HEADER_LENGTH_FIELD 1

/*! \brief Types of dfu channel messages.
    \note These values are used in the protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    /*! Message containing a new Request. */
    DFU_CHANNEL_MID_REQUEST                  = 0,

    /*! Message contatining a Response for the last Request. */
    DFU_CHANNEL_MID_RESPONSE                 = 1,

    /*! Message contatining a Response for the last Request and a new Request for next message. */
    DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST    = 2,
} dfu_channel_mid_t;

/*! \brief State of pending requests from case while we were transmitting a message to case
*/
typedef enum
{
    /*! No pending request. */
    DFU_CASE_FW_IF_PENDING_NONE,

    /*! An ACK message as a response. */
    DFU_CASE_FW_IF_PENDING_ACK,

    /*! An ACK message as a request or response_with_request. */
    DFU_CASE_FW_IF_PENDING_ACK_WITH_REQ,

    /*! An ACK message followed by checksum message. */
    DFU_CASE_FW_IF_PENDING_ACK_WITH_CHECKSUM,
} dfu_case_fw_pending_req_t;

typedef struct
{
    /*! Flag to track outstandig requests from case. */
    bool is_last_request_pending;

    /*! Last received outstandig request. */
    dfu_case_fw_message_t last_message;

    /*! Last sent message to retry. */
    dfu_case_fw_message_t last_sent_message;

    uint8* last_message_payload;

    uint16 last_message_length;

    uint8 retry_attempts;

    /*! TRUE if a message has been sent over caseComms and the tx_status is still pending. */
    bool is_tx_in_progress;

    /*! dfu_case_fw_if has been asked to cleanup. */
    uint8 is_cleanup_scheduled;

    /*! next expected sequence number for ACK message. */
    uint8 nesn;

    /*! state of new pending requests. */
    dfu_case_fw_pending_req_t next_pending_req;
} dfu_case_fw_if_context_t;

LOGGING_PRESERVE_MESSAGE_TYPE(dfu_case_fw_message_t)

static dfu_case_fw_if_context_t dfu_case_fw_if_context = {0};

/*! Get the private context of dfu_case_fw_if */
#define  dfuCase_FWIFGetContext() (&dfu_case_fw_if_context)

/*! \brief handle the status of the last transmitted packet over caseComms

    \param status status of packet transmission over caseComms
    \param mid message ID of transmitted packet for which this status has been sent
*/
static void dfuCase_FWIFHandleTxStatus(cc_tx_status_t status, unsigned mid)
{
    DEBUG_LOG("dfuCase_FWHandleTxStatus sts enum:cc_tx_status_t:%d mid:%d retry_attempts %d", status, mid, dfuCase_FWIFGetContext()->retry_attempts);

    if(status != CASECOMMS_TX_SUCCESS)
    {
        if(dfuCase_FWIFGetContext()->retry_attempts >= DFU_CASE_FW_IF_MAX_RETRY_ATTEMPTS)
        {
            dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, FALSE);
        }
        else
        {
            dfuCase_FWIFGetContext()->retry_attempts++;
            if(dfuCase_FWIFGetContext()->last_sent_message == DFU_CASE_FW_DATA)
            {
                dfuCase_FWIFSendSRecToCase(dfuCase_FWIFGetContext()->last_message_payload, dfuCase_FWIFGetContext()->last_message_length);
            }
            else
            {
                DfuCase_FWIFSendGeneralCommand(dfuCase_FWIFGetContext()->last_sent_message);
            }
            return;
        }
    }
    else
    {
        dfuCase_FWIFGetContext()->retry_attempts = 0;

        /* We have sent a message to case which would contain the response to last request. */
        dfuCase_FWIFGetContext()->is_last_request_pending = FALSE;

        if(dfuCase_FWIFGetContext()->last_sent_message == DFU_CASE_FW_DATA)
        {
            /* flip the nesn as we have sent an S-Record and waiting for next ACK. */
            dfuCase_FWIFGetContext()->nesn = !dfuCase_FWIFGetContext()->nesn;

            /* Handle if we had received any new requests while we were sending last S-Record. */
            switch(dfuCase_FWIFGetContext()->next_pending_req)
            {
                case DFU_CASE_FW_IF_PENDING_ACK_WITH_REQ:
                    dfuCase_FWIFGetContext()->is_last_request_pending = TRUE;

                case DFU_CASE_FW_IF_PENDING_ACK:
                    MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE);
                    dfuCase_FWIFGetContext()->last_message = DFU_CASE_FW_ACK;
                    MessageSend(dfuCase_FWGetTask(), DFU_CASE_FW_ACK, NULL);
                    break;

                case DFU_CASE_FW_IF_PENDING_ACK_WITH_CHECKSUM:
                    MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE);
                    dfuCase_FWIFGetContext()->is_last_request_pending = TRUE;
                    dfuCase_FWIFGetContext()->last_message = DFU_CASE_FW_CHECKSUM;
                    MessageSend(dfuCase_FWGetTask(), DFU_CASE_FW_ACK, NULL);
                    MessageSend(dfuCase_FWGetTask(), DFU_CASE_FW_CHECKSUM, NULL);
                    break;

                default:
                    break;
            }
            dfuCase_FWIFGetContext()->next_pending_req = DFU_CASE_FW_IF_PENDING_NONE;
        }
    }

    /* Earbud has successfully trasmitted a message to case. */
    dfuCase_FWIFGetContext()->is_tx_in_progress = FALSE;

    /* If the cleanup was scheduled during the Tx then clean it up now. */
    if(dfuCase_FWIFGetContext()->is_cleanup_scheduled)
    {
        dfuCase_FWIFCleanUp();
    }
}

/*! \brief handle the message sent by the case

        This indication will be sent for all incoming message over the registered
        caseComms channel ('dfu')

    \param mid message ID of received packet
    \param msg buffer containing the message
    \param length size of the message in the buffer
    \param source_dev caseComms source from which this message is coming
*/
static void dfuCase_FWIFHandleRxInd(unsigned mid, const uint8* msg, unsigned length, cc_dev_t source_dev)
{
    if(source_dev != CASECOMMS_DEVICE_CASE)
    {
        DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, unknown sournce %d", source_dev);
        return;
    }

    if ((length < DFU_CASE_FW_MSG_HEADER_SIZE) || (msg == NULL))
    {
        DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, size %d too small for header", length);
        return;
    }

    uint16 msgId = ByteUtilsGet1ByteFromStream(msg) + DFU_CASE_FW_MSG_BASE;
    uint16 msgLen = ByteUtilsGet2BytesFromStream(msg + 1);

    /* Validate message has enough data as specified in message length field */
    if (length < (msgLen + DFU_CASE_FW_MSG_HEADER_SIZE))
    {
        DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, size %d too small, ", length);
        return;
    }

    if(dfuCase_FWIFGetContext()->is_last_request_pending)
    {
        if(msgId == DFU_CASE_FW_ACK)
        {
            /* compare the sn of the ACK message to determine if its just a retry or a new request. */
            uint8 sn = ByteUtilsGet1ByteFromStream(msg + DFU_CASE_FW_MSG_HEADER_SIZE);
            if(sn != dfuCase_FWIFGetContext()->nesn)
            {
                /* If we have received a new request while the previous request is still pending then just update the
                 * state of pending requests accordingly. */
                dfuCase_FWIFGetContext()->next_pending_req = 
                    (mid == DFU_CHANNEL_MID_RESPONSE? DFU_CASE_FW_IF_PENDING_ACK: DFU_CASE_FW_IF_PENDING_ACK_WITH_REQ);
                DEBUG_LOG("dfuCase_FWHandleRxInd, next ACK received sn %d", sn);
            }
            else
            {
                DEBUG_LOG("dfuCase_FWHandleRxInd, case retrying with ACK message %d", sn);
            }
        }
        else if(msgId == DFU_CASE_FW_CHECKSUM && dfuCase_FWIFGetContext()->next_pending_req == DFU_CASE_FW_IF_PENDING_ACK)
        {
            /* If we have received a new CHECKSUM message while the previous request is still pending then just update the
             * state of pending requests accordingly. */
            dfuCase_FWIFGetContext()->next_pending_req = DFU_CASE_FW_IF_PENDING_ACK_WITH_CHECKSUM;
            DEBUG_LOG("dfuCase_FWHandleRxInd, next CHECKSUM received");
        }
        else if(msgId == dfuCase_FWIFGetContext()->last_message || msgId == DFU_CASE_FW_SYNC)
        {
            /* Case is retrying for the reply, ignore these messages. */
            DEBUG_LOG("dfuCase_FWHandleRxInd, case retrying with message %d", msgId);
        }
        else if(mid == DFU_CHANNEL_MID_REQUEST || mid == DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST)
        {
            /* Abort, case has sent a new request before earbud replied to the last one. */
            DEBUG_LOG("dfuCase_FWHandleRxInd, request out of order message %d", msgId);
            dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, TRUE);
        }
        return;
    }
    else
    {
        if(msgId == DFU_CASE_FW_SYNC)
        {
            /* Abort, case is retrying without sending any request. */
            DEBUG_LOG("dfuCase_FWHandleRxInd, sync message out of order message %d", msgId);
            dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, TRUE);
            return;
        }
        else if(mid == DFU_CHANNEL_MID_REQUEST || mid == DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST)
        {
            dfuCase_FWIFGetContext()->last_message = msgId;
            dfuCase_FWIFGetContext()->is_last_request_pending = TRUE;
        }
    }

    /* We have received a valid response from case so cancel the timer for case to respond. */
    MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE);

    /* For convenience get pointer to message payload */
    const uint8 *msgPayload = msg + DFU_CASE_FW_MSG_HEADER_SIZE;

    /* Extract message data and pass to state machine via message send or directly */
    switch (msgId)
    {
        case DFU_CASE_FW_CHECK:
        {
            if (msgLen == DFU_CASE_FW_CHECK_BYTE_SIZE)
            {
                MESSAGE_MAKE(dfu_msg, DFU_CASE_FW_CHECK_T);
                dfu_msg->major_version = ByteUtilsGet2BytesFromStream(msgPayload);
                dfu_msg->minor_version = ByteUtilsGet2BytesFromStream(msgPayload+2);
                MessageSend(dfuCase_FWGetTask(), msgId, dfu_msg);
                DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_CHECK version %d.%d", dfu_msg->major_version, dfu_msg->minor_version);
            }
            else
            {
                DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, DFU_CASE_FW_CHECK_BYTE_SIZE, message size %d incorrect", msgLen);
            }
        }
        break;

        case DFU_CASE_FW_BUSY:
        {
            MessageSend(dfuCase_FWGetTask(), msgId, NULL);
            DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_BUSY");
        }
        break;

        case DFU_CASE_FW_READY:
        {
            if (msgLen == DFU_CASE_FW_READY_BYTE_SIZE)
            {
                MESSAGE_MAKE(dfu_msg, DFU_CASE_FW_READY_T);
                dfu_msg->current_bank = ByteUtilsGet1ByteFromStream(msgPayload);

                MessageSend(dfuCase_FWGetTask(), msgId, dfu_msg);
                DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_READY current bank %c", dfu_msg->current_bank);
            }
            else
            {
                DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, DFU_CASE_FW_READY_BYTE_SIZE, message size %d incorrect", msgLen);
            }
        }
        break;

        case DFU_CASE_FW_ACK:
        {
            MessageSend(dfuCase_FWGetTask(), msgId, NULL);
            DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_ACK sn %d", ByteUtilsGet1ByteFromStream(msgPayload));
        }
        break;

        case DFU_CASE_FW_START:
        {
            if (msgLen == DFU_CASE_FW_START_BYTE_SIZE)
            {
                MESSAGE_MAKE(dfu_msg, DFU_CASE_FW_START_T);
                memcpy(dfu_msg->build_variant, msgPayload, DFU_CASE_FW_START_BYTE_SIZE);

                MessageSend(dfuCase_FWGetTask(), msgId, dfu_msg);

                DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_START build variant %c%c%c%c", dfu_msg->build_variant[0],
                    dfu_msg->build_variant[1],dfu_msg->build_variant[2],dfu_msg->build_variant[3]);
            }
            else
            {
                DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, DFU_CASE_FW_START_BYTE_SIZE, message size %d incorrect", msgLen);
            }
        }
        break;

        case DFU_CASE_FW_CHECKSUM:
        {
            MessageSend(dfuCase_FWGetTask(), msgId, NULL);
            DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_CHECKSUM");
        }
        break;

        case DFU_CASE_FW_VERIFY:
        {
            MessageSend(dfuCase_FWGetTask(), msgId, NULL);
            DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_VERIFY");
        }
        break;

        case DFU_CASE_FW_COMPLETE:
        {
            MessageSend(dfuCase_FWGetTask(), msgId, NULL);
            DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_COMPLETE");
        }
        break;

        case DFU_CASE_FW_ERROR:
        {
            MESSAGE_MAKE(dfu_msg, DFU_CASE_FW_ERROR_T);
            dfu_msg->error_code = ByteUtilsGet1ByteFromStream(msgPayload);
            DEBUG_LOG_INFO("dfuCase_FWHandleRxInd, DFU_CASE_FW_ERROR code %x", dfu_msg->error_code);
            /* Abort the DFU synchronously as case has stopped the polling and we don't want dfu_case_fw to send 
             * any more data. */
            dfuCase_FWAbort(UPGRADE_HOST_ERROR_INTERNAL_ERROR_1, FALSE);
            if(dfuCase_FWIFGetContext()->is_last_request_pending)
            {
                MessageCancelAll(dfuCase_FWGetTask(), dfuCase_FWIFGetContext()->last_message);
            }
        }
        break;

        default:
            DEBUG_LOG_ERROR("dfuCase_FWHandleRxInd, unhandled message, id MESSAGE:0x%x", msgId);
            return;
    }
}

/*! \brief send a command to the case over caseComms 'dfu' channel

    \param message Id
    \param data buffer containing the message
    \param len size of the message in the buffer
*/
static void dfuCase_FWIFSendCommandToCase(unsigned mid, uint8* data, uint16 len)
{
    /* Start the timer for the case to reply while sending a request. */
    MessageCancelAll(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE);
    if(mid == DFU_CHANNEL_MID_REQUEST || mid == DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST)
    {
        DEBUG_LOG("dfuCase_FWIFSendCommandToCase DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE timer set");
#ifndef HOSTED_TEST_ENVIRONMENT
        MessageSendLater(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE, NULL, DFU_CASE_FW_IF_CASE_RESPONSE_TIME_LIMIT);
#endif
    }

    dfuCase_FWIFGetContext()->is_tx_in_progress = TRUE;
#ifdef INCLUDE_DFU_CASE_MOCK
    DfuCase_MockTransmitNotification(CASECOMMS_DEVICE_CASE, CASECOMMS_CID_DFU, mid, data, len);
#else
    CcProtocol_TransmitNotification(CASECOMMS_DEVICE_CASE, CASECOMMS_CID_DFU, mid, data, len);
#endif
}

/*! \brief register the 'dfu' channel with the case_comms domain
*/
void DfuCase_FWIFRegisterDFUChannel(void)
{
    cc_chan_config_t cfg;

    cfg.cid = CASECOMMS_CID_DFU;
    cfg.tx_sts = dfuCase_FWIFHandleTxStatus;
    cfg.rx_ind = dfuCase_FWIFHandleRxInd;

#ifdef INCLUDE_DFU_CASE_MOCK
    DfuCase_MockRegisterChannel(&cfg);
#else
    CcProtocol_RegisterChannel(&cfg);
#endif
}

/*! \brief Clean up after DFU comlpletes or aborts
*/
void dfuCase_FWIFCleanUp(void)
{
    /* If the Tx of a message is in progress then wait for it to complete before cleaning up. */
    if(dfuCase_FWIFGetContext()->is_tx_in_progress)
    {
        DEBUG_LOG("dfuCase_FWIFCleanUp scheduled");
        dfuCase_FWIFGetContext()->is_cleanup_scheduled = TRUE;
    }
    else
    {
        DEBUG_LOG("dfuCase_FWIFCleanUp cleaned up");
        memset(dfuCase_FWIFGetContext(), 0, sizeof(dfu_case_fw_if_context_t));
    }
}

/*! \brief send an S-Record to the case over caseComms 'dfu' channel

    \param data buffer containing the message
    \param len size of the message in the buffer
*/
void dfuCase_FWIFSendSRecToCase(uint8* data, uint16 len)
{
    ByteUtilsSet1Byte(data, DFU_CASE_FW_MSG_HEADER_ID_FIELD, DFU_CASE_FW_DATA - DFU_CASE_FW_MSG_BASE);
    ByteUtilsSet2Bytes(data, DFU_CASE_FW_MSG_HEADER_LENGTH_FIELD, len - DFU_CASE_FW_MSG_HEADER_SIZE); 

    /* Store the message details to retry if needed. */
    dfuCase_FWIFGetContext()->last_sent_message = DFU_CASE_FW_DATA;
    dfuCase_FWIFGetContext()->last_message_payload = data;
    dfuCase_FWIFGetContext()->last_message_length = len;

    DEBUG_LOG("dfuCase_FWIFSendSRecToCase DFU_CASE_FW_DATA length %d", len);
    dfuCase_FWIFSendCommandToCase(DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST, data, len);
}

/*! \brief build general commands and send them to case
*/
void DfuCase_FWIFSendGeneralCommand(dfu_case_fw_message_t command)
{
    uint8 data[DFU_CASE_FW_MSG_HEADER_SIZE];
    unsigned mid=0;

    DEBUG_LOG("DfuCase_FWIFSendGeneralCommand command enum:dfu_case_fw_message_t:%d", command);

    switch (command)
    {
        case DFU_CASE_FW_INITIATE:
            mid = DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST;
            break;

        case DFU_CASE_FW_REBOOT:
            mid = DFU_CHANNEL_MID_RESPONSE;
            break;

        case DFU_CASE_FW_COMMIT:
            mid = DFU_CHANNEL_MID_RESPONSE_WITH_REQUEST;
            break;

        case DFU_CASE_FW_ABORT:
            mid = DFU_CHANNEL_MID_RESPONSE;
            break;

        default:
            return;
    }

    ByteUtilsSet1Byte(data, DFU_CASE_FW_MSG_HEADER_ID_FIELD, command - DFU_CASE_FW_MSG_BASE);
    ByteUtilsSet2Bytes(data, DFU_CASE_FW_MSG_HEADER_LENGTH_FIELD, 0);

    dfuCase_FWIFGetContext()->last_sent_message = command;

    dfuCase_FWIFSendCommandToCase(mid, data, DFU_CASE_FW_MSG_HEADER_SIZE);
}

/*! \brief Handle if we fail to receive the reply from case for last transmitted message.
*/
void DfuCase_FWIFHandleTimeoutCaseResponse(void)
{
    /* Timeout can happen if message gets stuck on earbud side and we might not have received the 
     * Tx status so clear below flag. */
    dfuCase_FWIFGetContext()->is_tx_in_progress = FALSE;

    /* If the cleanup was scheduled during the Tx then clean it up now. */
    if(dfuCase_FWIFGetContext()->is_cleanup_scheduled)
    {
        dfuCase_FWIFCleanUp();
    }
}

#endif /* INCLUDE_DFU_CASE */
