/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_fw_if.h
\brief      Header file for the interface with the stm32 case.
*/

#ifndef DFU_CASE_FW_IF_H_
#define DFU_CASE_FW_IF_H_

#ifdef INCLUDE_DFU_CASE

#include "dfu_case_private.h"
#include "dfu_case_fw.h"

#include <message.h>
#include <rtime.h>

#ifndef DFU_CASE_FW_MSG_BASE
#define DFU_CASE_FW_MSG_BASE 0x400
#endif

/* header size of the 'dfu protocol packet from the case */
#define DFU_CASE_FW_MSG_HEADER_SIZE (sizeof(uint8) + sizeof(uint16))

/*! \brief messages received from the case.
    \note These values are used in the dfu protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    DFU_CASE_FW_CHECK            = DFU_CASE_FW_MSG_BASE,
    DFU_CASE_FW_BUSY,
    DFU_CASE_FW_READY,
    DFU_CASE_FW_START,
    DFU_CASE_FW_ACK,
    DFU_CASE_FW_NACK,
    DFU_CASE_FW_CHECKSUM,
    DFU_CASE_FW_VERIFY,
    DFU_CASE_FW_SYNC,
    DFU_CASE_FW_COMPLETE,
    DFU_CASE_FW_ERROR,
    DFU_CASE_FW_INITIATE,
    DFU_CASE_FW_REBOOT,
    DFU_CASE_FW_COMMIT,
    DFU_CASE_FW_ABORT,
    DFU_CASE_FW_DATA,
} dfu_case_fw_message_t;

/*! \brief error codes received from the case.
    \note These values are used in the dfu protocol with the case
          and must remain in sync with case software.
*/
typedef enum
{
    DFU_CASE_FW_ERROR_INCOMPATIBLE,
    DFU_CASE_FW_ERROR_SPURIOUS_NACK,
    DFU_CASE_FW_ERROR_RECORD_TOO_BIG,
    DFU_CASE_FW_ERROR_RECORD_TOO_SMALL,
    DFU_CASE_FW_ERROR_RECORD_CHECKSUM,
    DFU_CASE_FW_ERROR_LENGTH_INCONSISTENT,
    DFU_CASE_FW_ERROR_FLASH_FAILED,
    DFU_CASE_FW_ERROR_CHECKSUM,
    DFU_CASE_FW_ERROR_TIMEOUT,
    DFU_CASE_FW_ERROR_WRITE_COUNT_FAILED,
    DFU_CASE_FW_ERROR_ACTIVITY_TIMEOUT,
    DFU_CASE_FW_ERROR_OUT_OF_ORDER,
    DFU_CASE_FW_ERROR_LID_OPEN,
} dfu_case_fw_error_code_t;

/*! \brief commands to send to the case.
*/
typedef enum
{
    DFU_CASE_FW_IF_COMMAND_START,
    DFU_CASE_FW_IF_COMMAND_REBOOT,
    DFU_CASE_FW_IF_COMMAND_COMMIT,
    DFU_CASE_FW_IF_COMMAND_ABORT,
} dfu_case_fw_if_command_t;

typedef struct {
    uint16 major_version;
    uint16 minor_version;
} DFU_CASE_FW_CHECK_T;
#define DFU_CASE_FW_CHECK_BYTE_SIZE (4)

typedef struct {
    dfu_case_fw_bank_t current_bank;
} DFU_CASE_FW_READY_T;
#define DFU_CASE_FW_READY_BYTE_SIZE (1)

typedef struct {
    char build_variant[4];
} DFU_CASE_FW_START_T;
#define DFU_CASE_FW_START_BYTE_SIZE (4)

typedef struct {
    dfu_case_fw_error_code_t error_code;
} DFU_CASE_FW_ERROR_T;
#define DFU_CASE_FW_ERROR_BYTE_SIZE (1)

/*! \brief register a new channel with the caseComms for dfu.
*/
void DfuCase_FWIFRegisterDFUChannel(void);

/*! \brief Clean up after DFU comlpletes or aborts.
*/
void dfuCase_FWIFCleanUp(void);

/*! \brief send an S-Record to the case over caseComms 'dfu' channel

    \param data buffer containing the message
    \param len size of the message in the buffer
*/
void dfuCase_FWIFSendSRecToCase(uint8* data, uint16 len);

/*! \brief send the dfu protocol commands to the case.
*/
void DfuCase_FWIFSendGeneralCommand(dfu_case_fw_message_t command);

/*! \brief Handle if we fail to receive the reply from case for last transmitted message.
*/
void DfuCase_FWIFHandleTimeoutCaseResponse(void);

#endif /* INCLUDE_DFU_CASE */
#endif /* DFU_CASE_FW_IF_H_ */
