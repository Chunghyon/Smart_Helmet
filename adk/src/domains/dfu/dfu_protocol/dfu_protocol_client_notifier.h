/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_client_notifier.h
\brief      Definition of the client notifier APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_CLIENT_NOTIFIER_H
#define DFU_PROTOCOL_CLIENT_NOTIFIER_H

#include "dfu_protocol.h"
#include <upgrade_protocol.h>

/*! \brief Sends a DFU_PROTOCOL_COMMIT_REQ to the active DFU protocol client */
void DfuProtocol_SendCommitReqToActiveClient(void);

/*! \brief Sends a DFU_PROTOCOL_ERROR_IND to the active DFU protocol client
    \param error_code The error code */
void DfuProtocol_SendErrorIndToActiveClient(UpgradeHostErrorCode error_code);

/*! \brief Sends a DFU_PROTOCOL_CACHE_CLEARED_IND to the active DFU protocol client */
void DfuProtocol_SendCacheClearedIndToActiveClient(void);

/*! \brief Sends a DFU_PROTOCOL_TRANSPORT_DISCONNECTED_IND to the active DFU protocol client
    \param task Task to send the message
    \param reason The reason for the transport disconnection */
void DfuProtocol_SendTransportDisconnectedIndToTask(Task task, dfu_protocol_disconnect_reason_t reason);

/*! \brief Sends a DFU_PROTOCOL_PERMISSION_DENIED_IND to the active DFU protocol client */
void DfuProtocol_SendPermissionDeniedIndToActiveClient(void);

/*! \brief Sends a DFU_PROTOCOL_TRANSFER_COMPLETE_IND to the active DFU protocol client */
void DfuProtocol_SendTransferCompleteIndToActiveClient(void);

#endif // DFU_PROTOCOL_CLIENT_NOTIFIER_H
