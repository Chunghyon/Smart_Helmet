/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_mock.c
\brief      Implementation of mock API for stm32 chargercase and caseComms.
*/

#ifdef INCLUDE_DFU_CASE
#ifdef INCLUDE_DFU_CASE_MOCK

#include "dfu_case.h"
#include "dfu_case_mock.h"

#include <util.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include <stdio.h>
#include "phy_state.h"
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include <byte_utils.h>

/*! details of S-records used in dfu protocol  */
#define DFU_CASE_MOCK_MIN_MESSAGE_LENGTH 3

#define DFU_CASE_MOCK_SREC_PREFIX_LENGTH 2
#define DFU_CASE_MOCK_SREC_S0_PREFIX "S0"
#define DFU_CASE_MOCK_SREC_S3_PREFIX "S3"
#define DFU_CASE_MOCK_SREC_S7_PREFIX "S7"
#define DFU_CASE_MOCK_SREC_TERMINATOR 0x0

#define DFU_CASE_MOCK_GENERAL_COMMAND_LENGTH 2
#define DFU_CASE_MOCK_START_COMMAND "DS"
#define DFU_CASE_MOCK_REBOOT_COMMAND "DR"
#define DFU_CASE_MOCK_COMMIT_COMMAND "DC"
#define DFU_CASE_MOCK_ABORT_COMMAND "DA"

#define DFU_CASE_MOCK_BUILD_VARIANT_BALI "ST2"

#define DFU_CASE_MOCK_MESSAGE_BUFFER_SIZE 11

#define DFU_CASE_MOCK_DUMMY_CHECKSUM 0x7C93C6FD

#define DFU_CASE_MOCK_TEST_RESPONSE_TIMEOUT_DELAY 2010
#define DFU_CASE_MOCK_TEST_NEXT_STAGE_REQUEST_TIMEOUT_DELAY 3050
#define DFU_CASE_MOCK_TEST_TX_STS_DELAY 10

/*! Internal messages used by the dfu_case_mock */
typedef enum {
    /*! Send CHECK message to registered callback function */
    DFU_CASE_MOCK_INTERNAL_SEND_CHECK,

    /*! Send a mock tx status */
    DFU_CASE_MOCK_INTERNAL_SEND_TX_STATUS,

    /*! Send the READY message */
    DFU_CASE_MOCK_INTERNAL_SEND_READY,

    /*! Send the START message */
    DFU_CASE_MOCK_INTERNAL_SEND_START,

    /*! Send the ACK message */
    DFU_CASE_MOCK_INTERNAL_SEND_ACK,

    /*! Send the checksum message */
    DFU_CASE_MOCK_INTERNAL_SEND_CHECKSUM,

    /*! Send the verify message after reboot */
    DFU_CASE_MOCK_INTERNAL_SEND_VERIFY,

    /*! Send the dfu complete message */
    DFU_CASE_MOCK_INTERNAL_SEND_COMPLETE,

    /*! Send the dfu sync message */
    DFU_CASE_MOCK_INTERNAL_SEND_SYNC,

    /*! Send the dfu error message */
    DFU_CASE_MOCK_INTERNAL_SEND_ERROR,

    /*! This must be the final message */
    DFU_CASE_MOCK_INTERNAL_MESSAGE_END
} dfu_case_mock_internal_messages_t;

/*! dfu protocol messages that dfu_case_mock sends in place of stm32 case */
typedef enum {
    DFU_CASE_MOCK_DFU_MESSAGE_CHECK,
    DFU_CASE_MOCK_DFU_MESSAGE_BUSY,
    DFU_CASE_MOCK_DFU_MESSAGE_READY,
    DFU_CASE_MOCK_DFU_MESSAGE_START,
    DFU_CASE_MOCK_DFU_MESSAGE_ACK,
    DFU_CASE_MOCK_DFU_MESSAGE_NACK,
    DFU_CASE_MOCK_DFU_MESSAGE_CHECKSUM,
    DFU_CASE_MOCK_DFU_MESSAGE_VERIFY,
    DFU_CASE_MOCK_DFU_MESSAGE_SYNC,
    DFU_CASE_MOCK_DFU_MESSAGE_COMPLETE,
    DFU_CASE_MOCK_DFU_MESSAGE_ERROR,
    DFU_CASE_MOCK_DFU_MESSAGE_INITIATE,
    DFU_CASE_MOCK_DFU_MESSAGE_REBOOT,
    DFU_CASE_MOCK_DFU_MESSAGE_COMMIT,
    DFU_CASE_MOCK_DFU_MESSAGE_ABORT,
    DFU_CASE_MOCK_DFU_MESSAGE_DATA,
    DFU_CASE_MOCK_DFU_MESSAGE_TOP,
} dfu_case_mock_dfu_message_t;

uint16 dfu_case_mock_dfu_message_payload_size[DFU_CASE_MOCK_DFU_MESSAGE_TOP] = {
    4 /* DFU_CASE_MOCK_DFU_MESSAGE_CHECK */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_BUSY */,
    1 /* DFU_CASE_MOCK_DFU_MESSAGE_READY */,
    4 /* DFU_CASE_MOCK_DFU_MESSAGE_START */,
    1 /* DFU_CASE_MOCK_DFU_MESSAGE_ACK */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_NACK */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_CHECKSUM */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_VERIFY */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_SYNC */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_COMPLETE */,
    1 /* DFU_CASE_MOCK_DFU_MESSAGE_ERROR */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_INITIATE */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_REBOOT */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_COMMIT */,
    0 /* DFU_CASE_MOCK_DFU_MESSAGE_ABORT */,
};

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(dfu_case_mock_internal_messages_t)
//LOGGING_PRESERVE_MESSAGE_ENUM(dfu_case_messages_t)

/* Ensure message range is legal */
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(DFU_CASE_MOCK_INTERNAL_MESSAGE_END)
//ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(DFU_CASE, DFU_CASE_MESSAGE_END) To DO : create the group when first message is added to list

dfu_case_mock_task_data_t dfu_case_mock = {0};
static verify_callback_t verify_callback;

/*! \brief Get the dfu_case_mock FSM state
*/
static dfu_case_mock_state_t dfuCase_MockGetState(void)
{
    dfu_case_mock_task_data_t *the_dfu_case_mock = dfuCase_MockGetTaskData();
    return the_dfu_case_mock->state;
}

/*! \brief Set the dfu_case_mock FSM state
*/
static void dfuCase_MockSetState(dfu_case_mock_state_t new_state)
{
    dfu_case_mock_task_data_t *the_dfu_case_mock = dfuCase_MockGetTaskData();
    the_dfu_case_mock->state = new_state;
}

/******************************************************************************
 * General Definitions
 ******************************************************************************/

/*! \brief Clean up the dfu_case_mock for next DFU.
*/
static void dfuCase_MockCleanUp(void)
{
    dfu_case_mock_task_data_t *the_dfu_case_mock = dfuCase_MockGetTaskData();

    dfuCase_MockSetState(DFU_CASE_MOCK_STATE_NO_EB);
    MessageCancelAll(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_TX_STATUS);

    /* default values */
    the_dfu_case_mock->major_version = 1;
    the_dfu_case_mock->minor_version = 1;
    the_dfu_case_mock->current_bank = DFU_CASE_MOCK_CASE_BANK_A;
    the_dfu_case_mock->mock_processing_delay = 500;
    the_dfu_case_mock->tests_to_perform = 0;
    the_dfu_case_mock->retry_attempts = 0;
    the_dfu_case_mock->max_retry_attempts = 0;
    the_dfu_case_mock->sn = 0;
    the_dfu_case_mock->tx_sts_delay = DFU_CASE_MOCK_TEST_TX_STS_DELAY;
}

/*! \brief Send a dfu protocol message to the component registered over dfu CC channel

    \param id Id of the message to send
*/
static void dfuCase_MockSendDfuProtocolMessage(dfu_case_mock_dfu_message_t id)
{
    uint8 message_buffer[DFU_CASE_MOCK_MESSAGE_BUFFER_SIZE];
    unsigned message_length;
    unsigned mid;

    memset(message_buffer, 0, sizeof(message_buffer));
    ByteUtilsSet1Byte(message_buffer, 0, id);
    ByteUtilsSet2Bytes(message_buffer, 1, dfu_case_mock_dfu_message_payload_size[id]);
    message_length = dfu_case_mock_dfu_message_payload_size[id] + 3;
    uint8* payload = &message_buffer[3];

    switch (id)
    {
        case DFU_CASE_MOCK_DFU_MESSAGE_CHECK:
        {
            ByteUtilsSet2Bytes(payload, 0, dfuCase_MockGetTaskData()->major_version);
            ByteUtilsSet2Bytes(payload, 2, dfuCase_MockGetTaskData()->minor_version);
            mid = DFU_CASE_MOCK_CHANNEL_MID_REQUEST;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_READY:
        {
            ByteUtilsSet1Byte(payload, 0, dfuCase_MockGetTaskData()->current_bank);
            mid = DFU_CASE_MOCK_CHANNEL_MID_RESPONSE_WITH_REQUEST;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_ACK:
        {
            ByteUtilsSet1Byte(payload, 0, dfuCase_MockGetTaskData()->sn);
            mid = dfuCase_MockGetTaskData()->mid_to_set;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_START:
        {
            snprintf((char*) payload, message_length-3, DFU_CASE_MOCK_BUILD_VARIANT_BALI);
            mid = DFU_CASE_MOCK_CHANNEL_MID_REQUEST;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_CHECKSUM:
        {
            mid = DFU_CASE_MOCK_CHANNEL_MID_REQUEST;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_VERIFY:
        {
            mid = DFU_CASE_MOCK_CHANNEL_MID_REQUEST;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_COMPLETE:
        {
            mid = DFU_CASE_MOCK_CHANNEL_MID_RESPONSE;
            dfuCase_MockCleanUp();
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_SYNC:
        {
            mid = DFU_CASE_MOCK_CHANNEL_MID_REQUEST;
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_ERROR:
        {
            mid = DFU_CASE_MOCK_CHANNEL_MID_RESPONSE;
            ByteUtilsSet1Byte(payload, 0, 1);
            dfuCase_MockCleanUp();
        }
        break;

        default:
            DEBUG_LOG("dfuCase_MockSendDfuProtocolMessage UNHANDLED Message 0x%x", id);
            return;
    }
    DEBUG_LOG("dfuCase_MockSendDfuProtocolMessage send Message enum:dfu_case_mock_dfu_message_t:%d", id);
    dfuCase_MockGetTaskData()->rx_ind(mid, message_buffer, message_length, CASECOMMS_DEVICE_CASE);

    if(DFU_CASE_MOCK_TEST_DUPLICATE & dfuCase_MockGetTaskData()->tests_to_perform)
    {
        dfuCase_MockGetTaskData()->rx_ind(mid, message_buffer, message_length, CASECOMMS_DEVICE_CASE);
    }

    if(DFU_CASE_MOCK_TEST_SYNC & dfuCase_MockGetTaskData()->tests_to_perform)
    {
        memset(message_buffer, 0, sizeof(message_buffer));
        ByteUtilsSet1Byte(message_buffer, 0, DFU_CASE_MOCK_DFU_MESSAGE_SYNC);
        ByteUtilsSet2Bytes(message_buffer, 1, dfu_case_mock_dfu_message_payload_size[DFU_CASE_MOCK_DFU_MESSAGE_SYNC]);

        DEBUG_LOG("dfuCase_MockSendDfuProtocolMessage send sync message", id);
        dfuCase_MockGetTaskData()->rx_ind(DFU_CASE_MOCK_CHANNEL_MID_REQUEST, message_buffer, 3, CASECOMMS_DEVICE_CASE);
    }
}

/*! \brief Parse a incoming dfu protocol command 

    \param data packet buffer containing the incoming command
    \param len size of the buffer
*/
static void dfuCase_MockParseDfuProtocolCommand(uint8* data, uint16 len)
{
    DEBUG_LOG("dfuCase_MockParseDfuProtocolCommand");

    bool is_delay_res = DFU_CASE_MOCK_TEST_RESPONSE_TIMEOUT & dfuCase_MockGetTaskData()->tests_to_perform;
    bool is_delay_next_req = DFU_CASE_MOCK_TEST_NEXT_STAGE_REQUEST_TIMEOUT & dfuCase_MockGetTaskData()->tests_to_perform;

    if(len < DFU_CASE_MOCK_MIN_MESSAGE_LENGTH)
    {
        DEBUG_LOG_ERROR("dfuCase_MockParseDfuProtocolCommand command header not complete");
        return;
    }

    if(DFU_CASE_MOCK_TEST_ERROR_MESSAGE & dfuCase_MockGetTaskData()->tests_to_perform)
    {
        MessageSend(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_ERROR, NULL);
        return;
    }

    if(DFU_CASE_MOCK_TEST_SYNC & dfuCase_MockGetTaskData()->tests_to_perform)
    {
        MessageSend(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_SYNC, NULL);
    }

    switch(data[0])
    {
        case DFU_CASE_MOCK_DFU_MESSAGE_INITIATE:
        {
            DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand DFU_CASE_MOCK_DFU_MESSAGE_INITIATE");
            MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_READY, NULL, 
                        is_delay_res? DFU_CASE_MOCK_TEST_RESPONSE_TIMEOUT_DELAY: dfuCase_MockGetTaskData()->mock_processing_delay*2);
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_REBOOT:
        {
            DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand DFU_CASE_MOCK_DFU_MESSAGE_REBOOT");
            MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_VERIFY, NULL, 
                        is_delay_next_req? 
                        dfuCase_MockGetTaskData()->mock_processing_delay+DFU_CASE_MOCK_TEST_NEXT_STAGE_REQUEST_TIMEOUT_DELAY: 
                        dfuCase_MockGetTaskData()->mock_processing_delay*2);
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_COMMIT:
        {
            DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand DFU_CASE_MOCK_DFU_MESSAGE_COMMIT");
            MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_COMPLETE, NULL, 
                        is_delay_res? DFU_CASE_MOCK_TEST_RESPONSE_TIMEOUT_DELAY: dfuCase_MockGetTaskData()->mock_processing_delay*2);
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_ABORT:
        {
            DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand DFU_CASE_MOCK_DFU_MESSAGE_ABORT");
            dfuCase_MockCleanUp();
        }
        break;

        case DFU_CASE_MOCK_DFU_MESSAGE_DATA:
        {
            uint16 payload_length;
            uint8* payload = &data[3];
            payload_length = ByteUtilsGet2BytesFromStream(&data[1]);
            dfuCase_MockGetTaskData()->sn = !dfuCase_MockGetTaskData()->sn;

            /* S0 */
            if(payload_length > DFU_CASE_MOCK_SREC_PREFIX_LENGTH &&
                    0 == strncmp((char *)payload, DFU_CASE_MOCK_SREC_S0_PREFIX, DFU_CASE_MOCK_SREC_PREFIX_LENGTH))
            {
                DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand %d", payload_length);
                DEBUG_LOG_DATA_INFO(payload, payload_length);
                MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_ACK, NULL, dfuCase_MockGetTaskData()->mock_processing_delay);
        
                dfuCase_MockGetTaskData()->mid_to_set = DFU_CASE_MOCK_CHANNEL_MID_RESPONSE;
                MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_START, NULL, 
                            is_delay_next_req? 
                            dfuCase_MockGetTaskData()->mock_processing_delay+DFU_CASE_MOCK_TEST_NEXT_STAGE_REQUEST_TIMEOUT_DELAY: 
                            dfuCase_MockGetTaskData()->mock_processing_delay*2);
            }
            /* S3 */
            else if(payload_length > DFU_CASE_MOCK_SREC_PREFIX_LENGTH &&
                0 == strncmp((char *)payload, DFU_CASE_MOCK_SREC_S3_PREFIX, DFU_CASE_MOCK_SREC_PREFIX_LENGTH))
            {
                DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand %d", payload_length);
                DEBUG_LOG_DATA_INFO(payload, payload_length);
                MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_ACK, NULL, 
                            is_delay_res? DFU_CASE_MOCK_TEST_RESPONSE_TIMEOUT_DELAY: dfuCase_MockGetTaskData()->mock_processing_delay);
                dfuCase_MockGetTaskData()->mid_to_set = DFU_CASE_MOCK_CHANNEL_MID_RESPONSE_WITH_REQUEST;
            }
            /* S7 */
            else if(payload_length > DFU_CASE_MOCK_SREC_PREFIX_LENGTH &&
                0 == strncmp((char *)payload, DFU_CASE_MOCK_SREC_S7_PREFIX, DFU_CASE_MOCK_SREC_PREFIX_LENGTH))
            {
                DEBUG_LOG_INFO("dfuCase_MockParseDfuProtocolCommand %d", payload_length);
                DEBUG_LOG_DATA_INFO(payload, payload_length);
                MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_ACK, NULL, dfuCase_MockGetTaskData()->mock_processing_delay);

                dfuCase_MockGetTaskData()->mid_to_set = DFU_CASE_MOCK_CHANNEL_MID_RESPONSE;
                MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_CHECKSUM, NULL, 
                            is_delay_next_req? 
                            dfuCase_MockGetTaskData()->mock_processing_delay+DFU_CASE_MOCK_TEST_NEXT_STAGE_REQUEST_TIMEOUT_DELAY: 
                            dfuCase_MockGetTaskData()->mock_processing_delay*2);
            }
            else
            {
                Panic();
            }

        }
        break;
        default:
        {
            DEBUG_LOG_ERROR("dfuCase_MockParseDfuProtocolCommand Unknown command");
            Panic();
        }
    }
    
    if(verify_callback)
    {
        uint16 payload_len = len - DFU_CASE_MOCK_MIN_MESSAGE_LENGTH;
        verify_callback(data[0], (payload_len? &data[3]: NULL), payload_len);
    }
}

static bool dfuCase_MockHandleNoEB(MessageId id, Message message)
{
    UNUSED(message);

    switch (id)
    {
        /* Physical state changes */
        case PHY_STATE_CHANGED_IND:
        {
            PHY_STATE_CHANGED_IND_T* msg = (PHY_STATE_CHANGED_IND_T*) message;
            if( phy_state_event_in_case == msg->event && DfuCase_IsDfuNeeded())
            {
                /* small delay to indicate the time taken by case. */
                MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_CHECK, NULL, dfuCase_MockGetTaskData()->mock_processing_delay);
            }
        }
        break;

        case DFU_CASE_MOCK_INTERNAL_SEND_CHECK:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_CHECK);
            dfuCase_MockSetState(DFU_CASE_MOCK_STATE_CHECK_SENT);
        }
        break;

        default:
        {
            DEBUG_LOG("dfuCase_MockHandleNoEB. UNHANDLED Message 0x%x", id);
            return FALSE;
        }
    }

    return TRUE;
}

static bool dfuCase_MockHandleCheckSent(MessageId id, Message message)
{
    UNUSED(message);

    switch (id)
    {
        case DFU_CASE_MOCK_INTERNAL_SEND_READY:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_READY);
            dfuCase_MockSetState(DFU_CASE_MOCK_STATE_READY_SENT);
        }
        break;

        default:
        {
            DEBUG_LOG("dfuCase_MockHandleCheckSent. UNHANDLED Message 0x%x", id);
            return FALSE;
        }
    }

    return TRUE;
}

static bool dfuCase_MockHandleReadySent(MessageId id, Message message)
{
    UNUSED(message);

    switch (id)
    {
        case DFU_CASE_MOCK_INTERNAL_SEND_ACK:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_ACK);
        }
        break;

        case DFU_CASE_MOCK_INTERNAL_SEND_START:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_START);
            dfuCase_MockSetState(DFU_CASE_MOCK_STATE_DATA_TRANSFER);
        }
        break;

        default:
        {
            DEBUG_LOG("dfuCase_MockHandleReadySent. UNHANDLED Message 0x%x", id);
            return FALSE;
        }
    }

    return TRUE;
}

static bool dfuCase_MockHandleDataTransfer(MessageId id, Message message)
{
    UNUSED(message);

    switch (id)
    {
        case DFU_CASE_MOCK_INTERNAL_SEND_ACK:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_ACK);
        }
        break;

        case DFU_CASE_MOCK_INTERNAL_SEND_CHECKSUM:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_CHECKSUM);
            dfuCase_MockSetState(DFU_CASE_MOCK_STATE_REBOOT_AND_COMMIT);
        }
        break;

        default:
        {
            DEBUG_LOG("dfuCase_MockHandleDataTransfer. UNHANDLED Message 0x%x", id);
            return FALSE;
        }
    }

    return TRUE;
}

static bool dfuCase_MockHandleRebootAndCommit(MessageId id, Message message)
{
    UNUSED(message);

    switch (id)
    {
        case DFU_CASE_MOCK_INTERNAL_SEND_VERIFY:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_VERIFY);
        }
        break;

        case DFU_CASE_MOCK_INTERNAL_SEND_COMPLETE:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_COMPLETE);
        }
        break;

        default:
        {
            DEBUG_LOG("dfuCase_MockHandleReadySent. UNHANDLED Message 0x%x", id);
            return FALSE;
        }
    }

    return TRUE;
}

static bool dfuCase_MockDefaultHandler(MessageId id, Message message)
{
    UNUSED(message);

    switch (id)
    {
        case DFU_CASE_MOCK_INTERNAL_SEND_TX_STATUS:
        {
            if(DFU_CASE_MOCK_TEST_RETRY_ATTEMPTS && dfuCase_MockGetTaskData()->max_retry_attempts > dfuCase_MockGetTaskData()->retry_attempts)
            {
                dfuCase_MockGetTaskData()->retry_attempts++;
                dfuCase_MockGetTaskData()->tx_sts(CASECOMMS_TX_FAIL, dfuCase_MockGetTaskData()->received_mid);
            }
            else
            {
                dfuCase_MockGetTaskData()->retry_attempts=0;
                dfuCase_MockGetTaskData()->tx_sts(CASECOMMS_TX_SUCCESS, dfuCase_MockGetTaskData()->received_mid);
            }
        }
        break;

        case DFU_CASE_MOCK_INTERNAL_SEND_SYNC:
        {
             dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_SYNC);
        }
        break;

        case DFU_CASE_MOCK_INTERNAL_SEND_ERROR:
        {
            dfuCase_MockSendDfuProtocolMessage(DFU_CASE_MOCK_DFU_MESSAGE_ERROR);
        }
        break;

        default:
        {
            DEBUG_LOG("dfuCase_MockDefaultHandler. UNHANDLED Message 0x%x", id);
            return FALSE;
        }
    }

    return TRUE;
}

/*! \brief Message Handler for this mock interface component
*/
static void dfuCase_MockHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    bool handled = FALSE;

    DEBUG_LOG("dfuCase_MockHandleMessage, state enum:dfu_case_mock_state_t:%u, message_id 0x%04x", dfuCase_MockGetState(), id);

    switch (dfuCase_MockGetState())
    {

        case DFU_CASE_MOCK_STATE_NO_EB:
            handled = dfuCase_MockHandleNoEB(id, message);
            break;

        case DFU_CASE_MOCK_STATE_CHECK_SENT:
            handled = dfuCase_MockHandleCheckSent(id, message);
            break;

        case DFU_CASE_MOCK_STATE_READY_SENT:
            handled = dfuCase_MockHandleReadySent(id, message);
            break;

        case DFU_CASE_MOCK_STATE_DATA_TRANSFER:
            handled = dfuCase_MockHandleDataTransfer(id, message);
            break;

        case DFU_CASE_MOCK_STATE_REBOOT_AND_COMMIT:
        handled = dfuCase_MockHandleRebootAndCommit(id, message);
        break;

        default:
            DEBUG_LOG("dfuCase_MockHandleMessage, unknown state %u", dfuCase_MockGetState());
            break;
    }

    if (!handled)
    {
        handled = dfuCase_MockDefaultHandler(id, message);
    }

    if (!handled)
    {
        DEBUG_LOG("dfuCase_MockHandleMessage: MESSAGE:0x%04x not handled", id);
        Panic();
    }

    DEBUG_LOG("dfuCase_MockHandleMessage, new state enum:dfu_case_mock_state_t:%u", dfuCase_MockGetState());
}

/*! \brief Initialise dfu_case task

    Called at start up to initialise the dfu_case task
*/
bool DfuCase_MockInit(void)
{
    dfu_case_mock_task_data_t *theDfuCaseMock = dfuCase_MockGetTaskData();

    /* Set up task handler */
    theDfuCaseMock->task.handler = dfuCase_MockHandleMessage;

    dfuCase_MockSetState(DFU_CASE_MOCK_STATE_NO_EB);

    /* default values */
    theDfuCaseMock->major_version = 1;
    theDfuCaseMock->minor_version = 1;
    theDfuCaseMock->current_bank = DFU_CASE_MOCK_CASE_BANK_A;
    theDfuCaseMock->tx_sts_delay = DFU_CASE_MOCK_TEST_TX_STS_DELAY;

    /*! delay to add before replying to simulate case and caseComms operations */
    theDfuCaseMock->mock_processing_delay = 500;

    /* register for phy state notifications */
    appPhyStateRegisterClient(dfuCase_MockGetTask());

    return TRUE;
}

/*! \brief Replica of CcProtocol_RegisterChannel 
*/
void DfuCase_MockRegisterChannel(const cc_chan_config_t* config)
{
    dfuCase_MockGetTaskData()->tx_sts = config->tx_sts;
    dfuCase_MockGetTaskData()->rx_ind = config->rx_ind;
}

/*! \brief parse the incoming command and reply with tx success status
*/
bool DfuCase_MockTransmitNotification(cc_dev_t dest, cc_cid_t cid, unsigned mid, uint8* data, uint16 len)
{
    UNUSED(dest);
    if(cid != CASECOMMS_CID_DFU)
    {
        return FALSE;
    }
    dfuCase_MockGetTaskData()->received_mid = mid;

    if(DFU_CASE_MOCK_TEST_DELAYED_TX_STS & dfuCase_MockGetTaskData()->tests_to_perform && 
        data[0] == DFU_CASE_MOCK_DFU_MESSAGE_DATA)
    {
        MessageSendLater(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_TX_STATUS, NULL,
                            dfuCase_MockGetTaskData()->mock_processing_delay+dfuCase_MockGetTaskData()->tx_sts_delay);
    }
    else
    {
        MessageSend(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_TX_STATUS, NULL);
    }

    if(DFU_CASE_MOCK_TEST_RETRY_ATTEMPTS & dfuCase_MockGetTaskData()->tests_to_perform && 
        dfuCase_MockGetTaskData()->max_retry_attempts > dfuCase_MockGetTaskData()->retry_attempts)
    {
        DEBUG_LOG("DfuCase_MockTransmitNotification asked for retry %d", dfuCase_MockGetTaskData()->retry_attempts);
        return TRUE;
    }

    if(DFU_CASE_MOCK_TEST_SPURIOUS_SYNC & dfuCase_MockGetTaskData()->tests_to_perform)
    {
        MessageSend(dfuCase_MockGetTask(), DFU_CASE_MOCK_INTERNAL_SEND_SYNC, NULL);
    }

    dfuCase_MockParseDfuProtocolCommand(data, len);

    return TRUE;
}

/*! \brief Test framework can register a callback to be called when dfu_case_mock receives any message. 

    \param callback function to register for callback
*/
void DfuCase_MockSetVerificationCallback(verify_callback_t callback)
{
    verify_callback = callback;
}

#endif /* INCLUDE_DFU_CASE_MOCK */

#endif /* INCLUDE_DFU_CASE */
