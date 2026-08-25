/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_data_private.h
\brief      Private header file for the dfu file parser of the dfu_case.
*/

#ifndef DFU_CASE_DATA_PRIVATE_H_
#define DFU_CASE_DATA_PRIVATE_H_

#ifdef INCLUDE_DFU_CASE

#define DFU_CASE_DATA_REQUEST_NO_OFFSET 0

/* Max request size we can send to the host for data which won't overwhelm the earbud if host 
 * sends the data without waiting for Ack. Number is decided empirically. */
#define DFU_CASE_DATA_MAX_REQ_SIZE 500

#define DFU_CASE_DATA_INCOMPLETE_DATA_BUFFER_SIZE 48
#define DFU_CASE_HEADER_FIRST_PART_SIZE 12
#define DFU_CASE_SREC_PARTITION_SECOND_HEADER_SIZE 4
#define DFU_CASE_ID_FIELD_SIZE 8
#define DFU_CASE_VERSION_SIZE 4
#define DFU_CASE_NO_OF_COMPATIBLE_UPGRADES_SIZE 2

/* Identifier for the upgrade file partition containing the data in SREC format. */
#define DFU_CASE_SREC_PARTITION_ID "SRECDATA"

/* Identifier for the upgrade file partition containing the footer. */
#define DFU_CASE_FOOTER_ID "APPUPFTR"

typedef enum
{
    DFU_CASE_DATA_STATE_GENERIC_1ST_PART,
    DFU_CASE_DATA_STATE_HEADER,
    DFU_CASE_DATA_STATE_DATA_HEADER,
    DFU_CASE_DATA_STATE_DATA,
} dfu_case_data_state_t;

typedef struct {
    uint8 size;
    uint8 data[DFU_CASE_DATA_INCOMPLETE_DATA_BUFFER_SIZE];
} dfu_case_data_incomplete_data_t;

/*! \brief Context used by dfu_case_data parser
*/
typedef struct {
    dfu_case_data_state_t state;

    /*! size of pending data yet to be received from our last request to the host. */
    uint32 pending_chunk_size;

    /*! total size of all requests */
    uint32 total_req_size;

    /*! total size of data received */
    uint32 total_received_size;

    /*! offset for the next request */
    uint32 offset;

    /*! srec partition length without second header */
    uint32 partition_length;

    /*! buffer to store partial data. i.e. if host sent less than requested */
    dfu_case_data_incomplete_data_t incomplete_data;

    /*! offset to bank B data in SREC partition */
    uint32 bank_b_offset;

    /*! TRUE if case is ready to receive data and has sent the READY message */
    bool is_case_ready_for_data;

    /*! total file offset used during resume. */
    uint32 dfu_file_offset;

} dfu_case_data_context_t;

#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_DATA_PRIVATE_H_ */
