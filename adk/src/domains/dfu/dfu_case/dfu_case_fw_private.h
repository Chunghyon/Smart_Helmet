/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_fw_private.h
\brief      Private header file for the fw state machine of the dfu_case.
*/

#ifndef DFU_CASE_FW_PRIVATE_H_
#define DFU_CASE_FW_PRIVATE_H_

#ifdef INCLUDE_DFU_CASE

#include "dfu_case_fw.h"
#include "dfu_case_fw_if.h"

/* ASCII char NULL as the Delimiter to seperate two S-records */
#define DFU_CASE_FW_SREC_DELIMITER 0x0

/* Offset from the starting of the S-record to the byte indicating the record type */
#define DFU_CASE_FW_SREC_TYPE_FIELD_OFFSET 0x1

/* Type of the last S-record (S7) in the dfu file. ASCII value of 7. */
#define DFU_CASE_FW_LAST_SREC_TYPE 0x37

/* Empirically decided time limit for the case to send a request for new stage. */
#define DFU_CASE_FW_IF_NEXT_STAGE_REQUEST_TIME_LIMIT 3000

/* Empirically decided time limit for the case to reboot and respond to EB. */
#define DFU_CASE_FW_IF_REBOOT_REQUEST_TIME_LIMIT 8000


typedef enum
{
    /*! case DFU has not started yet. */
    DFU_CASE_FW_STATE_IDLE,

    /*! Awaiting dfu check message from case */
    DFU_CASE_FW_STATE_DFU_CHECK,

    /*! Awaiting dfu ready message from case */
    DFU_CASE_FW_STATE_DFU_STARTED,

    /*! data transfer started, parse S0 record */
    DFU_CASE_FW_STATE_PARSE_HEADER,

    /*! Handle START message and send the first record. */
    DFU_CASE_FW_STATE_START_DATA_TRANSFER,

    /*! parse S3 and S7 records */
    DFU_CASE_FW_STATE_PARSE_DATA,

    /*! Awaiting dfu checksum message from case */
    DFU_CASE_FW_STATE_VERIFY_CHECKSUM,

    /*! Awaiting case reboot */
    DFU_CASE_FW_STATE_VERIFY_REBOOT,

    /*! post reboot commit state */
    DFU_CASE_FW_STATE_COMMIT,
} dfu_case_fw_state_t;

typedef struct
{
    /*! Current state of the state machine */
    dfu_case_fw_state_t     state;

    /*! major_version */
    uint16 major_version;

    /*! minor_version */
    uint16 minor_version;

    /*! TRUE if we have received the CHECK message from case */
    bool is_dfu_check_received;

    /*! image bank in the case flash which needs to be updated */
    dfu_case_fw_bank_t bank_to_upgrade;

    /*! Stores the pointer of the buffer containing the new incoming data. 
     *  Buffer would be maintained by the tranport itself. 
     */
    const uint8* new_data_buffer;

    /*! Offset of the already processed data in the new data buffer */
    uint16 new_data_buffer_offset;

    /*! Size of the data being sent to the case from the buffer currently */
    uint16 srec_size_in_process;

    /*! Size of the new incoming data in the buffer */
    uint16 new_data_length;

    /*! Buffer to assemble the DATA packets being sent to case */
    uint8* dfu_data_buffer;

    /*! Buffer to store the S-Records as a part of a DATA packet */
    uint8* srec_buffer;

    /*! Size of the partial data stored */
    uint16 incomplete_data_length;

    /*! TRUE if we have sent the last S-record(S7 in this case) */
    bool is_last_srec_sent;

    /*! TRUE if the case rebooted and has sent the VERIFY message */
    bool is_case_rebooted;
} dfu_case_fw_context_t;

#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_FW_PRIVATE_H_ */
