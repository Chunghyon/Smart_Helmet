/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_private.h
\brief      Private header for dfu_case protocol.

*/

#ifndef DFU_CASE_PRIVATE_H_
#define DFU_CASE_PRIVATE_H_

#ifdef INCLUDE_DFU_CASE


#include <message.h>
#include "domain_message.h"

#include <rtime.h>
#include <task_list.h>

/*! Defines the dfu_case client task list initial capacity */
#define DFU_CASE_CLIENT_LIST_INIT_CAPACITY 1

typedef struct
{
    TaskData        task;

    /*! List of tasks to notify of dfu_case activity. */
    TASK_LIST_WITH_INITIAL_CAPACITY(DFU_CASE_CLIENT_LIST_INIT_CAPACITY) client_list;
    uint16          dummy;        /*!< dummy context variable */
} dfu_case_task_data_t;

/*! Make the dfu_case taskdata visible throughout the component. */
extern dfu_case_task_data_t dfu_case;

/*! Get pointer to dfu_case taskdata. */
#define dfuCase_GetTaskData()    (&dfu_case)

/*! Get pointer to dfu_case task. */
#define dfuCase_GetTask()  (&(dfuCase_GetTaskData()->task))

#define dfuCase_GetClientList()  (task_list_flexible_t *)(&(dfuCase_GetTaskData()->client_list))

/*! Internal messages used by dfu_case . */
typedef enum
{
    /*! upgrade_gaia_plugin has re-connected the transport with dfu_case. */
    DFU_CASE_INTERNAL_TRANSPORT_CONNECTED = INTERNAL_MESSAGE_BASE,

    DFU_CASE_INTERNAL_CHECK_RECEIVED,

    /*! send the request for next block of data. */
    DFU_CASE_INTERNAL_REQUEST_MORE_DATA,

    /*! incoming dfu file and current case firmware are compatible for dfu to start. */
    DFU_CASE_INTERNAL_DFU_COMPATIBLE,

    /*! case is ready for data transfer and has sent the READY message. */
    DFU_CASE_INTERNAL_CASE_READY_FOR_DATA,

    /*! host has sent more data which needs to be processed. */
    DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE,

    /*! case has received all data and verified the checksum. */
    DFU_CASE_INTERNAL_CHECKSUM_VERIFIED,

    /*! We are ready to reboot the case to swap firmware images. */
    DFU_CASE_INTERNAL_REBOOT_CASE,

    /*! Case has rebooted and successfully connected over caseComms. */
    DFU_CASE_INTERNAL_CASE_REBOOTED,

    /*! User has allowed to commit the new firmware image. */
    DFU_CASE_INTERNAL_COMMIT_UPGRADE,

    /*! Commit successful, ugrade complete.. */
    DFU_CASE_INTERNAL_UPGRADE_COMPLETE,

    /*! case hasn't replied for a request in specific time limit. */
    DFU_CASE_INTERNAL_TIMEOUT_CASE_RESPONSE,

    /*! case hasn't sent a request for a new stage. */
    DFU_CASE_INTERNAL_TIMEOUT_NEXT_STAGE_REQUEST,

    /*! Case DFU has timed out bacause user has failed to take required action. */
    DFU_CASE_INTERNAL_TIMEOUT_OUT_CASE_DFU,

    /*! Abort the DFU in dfu_case_host. */
    DFU_CASE_INTERNAL_HOST_ABORT,

    /*! Abort the DFU in dfu_case_fw. */
    DFU_CASE_INTERNAL_FW_ABORT,

    /*! This must be the final message */
    DFU_CASE_INTERNAL_MESSAGE_END
} dfu_case_internal_messages_t;

typedef struct
{
    /*! pointer to data buffer */
    const uint8* data;

    /*! size of the data in the buffer */
    uint16 len;
} DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE_T;

typedef struct
{
    /*! error code will be send to the host. */
    bool error_code;
} DFU_CASE_INTERNAL_HOST_ABORT_T;

typedef struct
{
    /*! TRUE if case needs to be informed about the Abort. */
    bool inform_case;
} DFU_CASE_INTERNAL_FW_ABORT_T;

#endif /* INCLUDE_DFU_CASE */
#endif  /* DFU_CASE_PRIVATE_H_ */
