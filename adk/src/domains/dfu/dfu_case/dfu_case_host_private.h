/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_host_private.h
\brief      Private header file for the host state machine of the dfu_case.
*/

#ifndef DFU_CASE_HOST_PRIVATE_H_
#define DFU_CASE_HOST_PRIVATE_H_

#ifdef INCLUDE_DFU_CASE

#include <upgrade.h>
#include <upgrade_protocol.h>

typedef enum
{
    DFU_CASE_HOST_STATE_IDLE,

    /*! Awaiting transport connection */
    DFU_CASE_HOST_STATE_WAITING_FOR_TRANSPORT,

    /*! transport connected and header first part available */
    DFU_CASE_HOST_STATE_CONNECTED,

    /*! data transfer started */
    DFU_CASE_HOST_STATE_DATA_TRANSFER,

    /*! data transfer complete and waiting for user confirmation */
    DFU_CASE_HOST_STATE_CONFIRM_REBOOT,

    /*! post reboot commit state */
    DFU_CASE_HOST_STATE_CONFIRM_COMMIT,

    /*! abort has been requested */
    DFU_CASE_HOST_STATE_ABORT,
} dfu_case_host_state_t;

typedef struct
{
    /*! Utility functions to send response to the host for received upgrade commands */
    const upgrade_response_functions_t* resp_funcs;

    /*! Current state of the state machine */
    dfu_case_host_state_t     state;

    /*! TRUE if upgrade lib has confirmed that dfu file contains case image */
    bool is_case_file_detected;

    /*! TRUE if we have requested the host to put earbuds in case and haven't yet confirmed it. */
    bool is_in_case_cfm_required;

    /*! TRUE if we have sent data to dfu_case_fw to process and haven't received reply to indicate it completed. */
    bool is_data_in_process;

    /*! Queue to store incoming DATA messages from host if they arrive while we are processing previous message. */
    UPGRADE_HOST_DATA_WITH_ACK_T** data_queue;

    /*! Current size of queue (elements stored in it). */
    uint16 queue_size;

    /*! Max capacity of the queue. */
    uint16 queue_max_size;

    /* Points to the first available element at the head of queue. */
    uint16 queue_head;

    /* Points to next available position at the tail of queue */
    uint16 queue_tail;
} dfu_case_host_context_t;

#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_HOST_PRIVATE_H_ */
