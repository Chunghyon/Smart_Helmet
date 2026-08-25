/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_message_dispatcher.h
\brief      Definition of the message dispatching APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_MESSAGE_DISPATCHER_H
#define DFU_PROTOCOL_MESSAGE_DISPATCHER_H

#include <csrtypes.h>
#include <upgrade_protocol.h>

#define ID_SIZE sizeof(uint8)
#define LENGTH_SIZE sizeof(uint16)
/* Last packet flag is treated as a uint8 in the Upgrade library, despite being uint16 in UPGRADE_HOST_DATA_T */
#define LAST_PACKET_FLAG_SIZE sizeof(uint8)

/* The values used for the parameter in calls to gaa_OtaSendUpgradeHostTransferCompleteRes */
#define UPGRADE_HOST_TRANSFER_COMPLETE_ACTION_INTERACTIVE   0
#define UPGRADE_HOST_TRANSFER_COMPLETE_ACTION_SILENT        2

/*! \brief Send an UPGRADE_HOST_SYNC_REQ message to the Upgrade library
    \param in_progress_id The in progress ID to add to the message */
void DfuProtocol_SyncOtaHostRequest(uint32 in_progress_id);

/*! \brief Send an UPGRADE_HOST_START_REQ message to the Upgrade library */
void DfuProtocol_StartOtaHostRequest(void);

/*! \brief Send an UPGRADE_HOST_START_DATA_REQ message to the Upgrade library */
void DfuProtocol_StartOtaHostDataRequest(void);

/*! \brief Send an UPGRADE_HOST_IS_CSR_VALID_DONE_REQ message to the Upgrade library */
void DfuProtocol_IsCsrValidDoneRequest(void);

/*! \brief Send an UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_REQ message to the Upgrade library */
void DfuProtocol_SilentCommitSupportedRequest(void);

/*! \brief Send an UPGRADE_HOST_ABORT_REQ message to the Upgrade library */
void DfuProtocol_AbortHostRequest(void);

/*! \brief Send an UPGRADE_HOST_IN_PROGRESS_RES message to the Upgrade library
    \param action The action code to add to the message */
void DfuProtocol_OtaHostInProgressResponse(UpgradeHostActionCode action);

/*! \brief Send an UPGRADE_HOST_COMMIT_CFM message to the Upgrade library
    \param action The action code to add to the message */
void DfuProtocol_OtaHostCommitConfirm(UpgradeHostActionCode action);

/*! \brief Send an UPGRADE_HOST_TRANSFER_COMPLETE_RES message to the Upgrade library for interactive DFU */
void DfuProtocol_InteractiveOtaHostTransferCompleteResponse(void);

/*! \brief Send an UPGRADE_HOST_TRANSFER_COMPLETE_RES message to the Upgrade library for silent commit DFU */
void DfuProtocol_SilentCommitOtaHostTransferCompleteResponse(void);

/*! \brief Send an UPGRADE_HOST_DATA message to the Upgrade library attaching available cached data */
void DfuProtocol_SendUpdateHostData(void);

/*! \brief Send an UPGRADE_HOST_ERRORWARN_RES message to the Upgrade library
    \param error_code The error code to add to the message */
void DfuProtocol_ErrorWarnResponse(uint16 error_code);

#endif // DFU_PROTOCOL_MESSAGE_DISPATCHER_H
