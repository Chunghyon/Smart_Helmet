/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_mock.h
\brief      Mock API for stm32 chargercase and caseComms.

    This API imitates the behavior of a stm32 based chagercase.  
*/

#ifndef DFU_CASE_MOCK_H_
#define DFU_CASE_MOCK_H_

#ifdef INCLUDE_DFU_CASE
#ifdef INCLUDE_DFU_CASE_MOCK

#include <message.h>
#include "domain_message.h"
#include <cc_protocol.h>

#include <rtime.h>
#include <task_list.h>

/*! Command from Earbud to Case to perform requested operation. */
#define DFU_CASE_MOCK_CHANNEL_MID_REQUEST                    0

/*! Response to command by Case to Earbud, for requested operation. */
#define DFU_CASE_MOCK_CHANNEL_MID_RESPONSE                   1

/*! Case notification message, not related to any command. */
#define DFU_CASE_MOCK_CHANNEL_MID_RESPONSE_WITH_REQUEST      2

/*! indentifiers for the image banks in the case flash */
#define DFU_CASE_MOCK_CASE_BANK_A 'A'
#define DFU_CASE_MOCK_CASE_BANK_B 'B'

typedef enum {
    DFU_CASE_MOCK_TEST_NONE =                       0,
    DFU_CASE_MOCK_TEST_SYNC =                       (1 << 0), /* 1 */
    DFU_CASE_MOCK_TEST_SPURIOUS_SYNC =              (1 << 1), /* 2 */
    DFU_CASE_MOCK_TEST_DUPLICATE =                  (1 << 2), /* 4 */
    DFU_CASE_MOCK_TEST_RESPONSE_TIMEOUT =           (1 << 3), /* 8 */
    DFU_CASE_MOCK_TEST_NEXT_STAGE_REQUEST_TIMEOUT = (1 << 4), /* 16 */
    DFU_CASE_MOCK_TEST_ERROR_MESSAGE =              (1 << 5), /* 32 */
    DFU_CASE_MOCK_TEST_RETRY_ATTEMPTS =             (1 << 6), /* 64 */
    DFU_CASE_MOCK_TEST_DELAYED_TX_STS =             (1 << 7), /* 128 */
} dfu_case_mock_test_t;

/*! Messages that are sent by the dfu_case module 
    To Do : remove if not used
*/
typedef enum {
    /*! This must be the final message */
    DFU_CASE_MOCK_MESSAGE_END
} dfu_case_mock_messages_t;

typedef enum
{
    DFU_CASE_MOCK_STATE_NO_EB,

    /*! dfu check message sent */
    DFU_CASE_MOCK_STATE_CHECK_SENT,

    /*! dfu ready message sent */
    DFU_CASE_MOCK_STATE_READY_SENT,

    DFU_CASE_MOCK_STATE_DATA_TRANSFER,

    DFU_CASE_MOCK_STATE_REBOOT_AND_COMMIT,
} dfu_case_mock_state_t;

typedef struct
{
    TaskData        task;

    /*! Current state of the state machine */
    dfu_case_mock_state_t     state;

    /*! send transmit status to registered module */
    CcProtocol_TxStatus_fn tx_sts;

    /*! send new message to registered module */
    CcProtocol_RxInd_fn rx_ind;

    /*! set this using pydbg */
    uint16 major_version;

    /*! set this using pydbg */
    uint16 minor_version;

    /*! set this using pydbg */
    char current_bank;

    /*! set this using pydbg */
    uint16 mock_processing_delay;

    /*! Used for messages that uses different MIDs for different scenarios. */
    uint16 mid_to_set;

    uint16 received_mid;

    /*! set this using pydbg */
    uint8 tests_to_perform;

    uint8 retry_attempts;

    /*! set this using pydbg */
    uint8 max_retry_attempts;

    /*! set this using pydbg */
    uint16 tx_sts_delay;

    /*! Boolean Sequence number to be sent with ACK messages. */ 
    uint8 sn;
} dfu_case_mock_task_data_t;

/*! Make the dfu_case_mock taskdata visible throughout the component. */
extern dfu_case_mock_task_data_t dfu_case_mock;

/*! Get pointer to dfu_case_mock taskdata. */
#define dfuCase_MockGetTaskData()    (&dfu_case_mock)

/*! Get pointer to dfu_case_mock task. */
#define dfuCase_MockGetTask()  (&(dfuCase_MockGetTaskData()->task))

typedef void (*verify_callback_t)(uint8 message, uint8* payload, uint16 len);

/*! \brief Test framework can register a callback to be called when dfu_case_mock receives any message. 

    \param callback function to register for callback
*/
void DfuCase_MockSetVerificationCallback(verify_callback_t callback);

/*! \brief Initialize the mock interface
*/
bool DfuCase_MockInit(void);

/*! \brief Register a new caseComms channel

        This mock interface only supports 'dfu' channel

    \param config configuration for new channel
*/
void DfuCase_MockRegisterChannel(const cc_chan_config_t* config);

/*! \brief Send a message over caseComms without expecting response

        Only the case as destination and 'dfu' channel is supported

    \param dest destination device (supported case)
    \param cid channel Id (supported 'dfu')
    \param mid message Id
    \param data message payload
    \param len message payload size
    \return TRUE always for mock API
*/
bool DfuCase_MockTransmitNotification(cc_dev_t dest, cc_cid_t cid, unsigned mid, 
                                     uint8* data, uint16 len);


#endif /* INCLUDE_DFU_CASE_MOCK */

#endif /* INCLUDE_DFU_CASE */

#endif /* DFU_CASE_MOCK_H_ */
