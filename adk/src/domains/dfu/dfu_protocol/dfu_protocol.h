/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol.h
\brief      Definition of the public APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_H
#define DFU_PROTOCOL_H

#include "dfu_protocol_client_config.h"
#include <upgrade_protocol.h>

typedef enum
{
    dfu_protocol_success,
    dfu_protocol_already_started,
    dfu_protocol_another_client_is_running,
    dfu_protocol_waiting_for_post_reboot_sequence,
    dfu_protocol_waiting_for_silent_commit,
    dfu_protocol_error
} dfu_protocol_status_t;

typedef enum
{
    DFU_PROTOCOL_COMMIT_REQ,
    DFU_PROTOCOL_ERROR_IND,
    DFU_PROTOCOL_CACHE_CLEARED_IND,
    DFU_PROTOCOL_TRANSPORT_DISCONNECTED_IND,
    DFU_PROTOCOL_PERMISSION_DENIED_IND,
    DFU_PROTOCOL_TRANSFER_COMPLETE_IND
} dfu_protocol_client_message_t;

typedef enum
{
    dfu_protocol_disconnect_reason_complete,
    dfu_protocol_disconnect_reason_aborted,
    dfu_protocol_disconnect_reason_disconnected
} dfu_protocol_disconnect_reason_t;

typedef struct
{
    UpgradeHostErrorCode error_code;
} DFU_PROTOCOL_ERROR_IND_T;

typedef struct
{
    dfu_protocol_disconnect_reason_t reason;
} DFU_PROTOCOL_TRANSPORT_DISCONNECTED_IND_T;

/*! \brief Intialise the DFU protocol module */
bool DfuProtocol_Init(Task task);

/*! \brief Stores a pointer to the clients configuration for future reference
    \param config A pointer to the populated client config structure */
void DfuProtocol_RegisterClient(const dfu_protocol_client_config_t * config);

/*! \brief Triggers the dfu_protocol_start state machine event. Connects the transport, registering the client task
    \param client_context The context identifier for the client
    \param dfu_file_size The total size of the DFU file
    \return dfu_protocol_success if success, dfu_protocol_already_started if the client is already connected and running DFU, otherwise failure type */
dfu_protocol_status_t DfuProtocol_StartOta(upgrade_context_t client_context, uint32 dfu_file_size);

/*! \brief Check if the data cache is empty
    \return TRUE if the cache is empty, otherwise FALSE */
bool DfuProtocol_IsCacheEmpty(void);

/*! \brief Add data to the heap mem allocated OTA buffer, return can be used for rewind
    \param client_context The context identifier for the client
    \param data A pointer to the data to add to the cache
    \param data_length The length of the data to add in bytes
    \param data_offset The offset of the total DFU file size
    \return dfu_protocol_success if success, otherwise failure type */
dfu_protocol_status_t DfuProtocol_AddDataToOtaBuffer(upgrade_context_t client_context, uint8 * data, uint16 data_length, uint32 data_offset);

/*! \brief Check if Upgrade is waiting for an apply from the specified client
    \param client_context The context identifier for the client
    \return TRUE if the Upgrade is waiting for an apply from the specfied client, otherwise FALSE */
bool DfuProtocol_IsWaitingForApply(upgrade_context_t client_context);

/*! \brief Triggers the dfu_protocol_apply state machine event. Reboot and apply the OTA
    \param client_context The context identifier for the client
    \return dfu_protocol_success if success, otherwise failure type */
dfu_protocol_status_t DfuProtocol_Apply(upgrade_context_t client_context);

/*! \brief Triggers the dfu_protocol_commit state machine event. Confirm commit and disconnect transport
    \param client_context The context identifier for the client
    \return dfu_protocol_success if success, otherwise failure type */
dfu_protocol_status_t DfuProtocol_CommitOta(upgrade_context_t client_context);

/*! \brief Triggers the dfu_protocol_abort state machine event.
 *         This will cancel the OTA, clear dfu_protocol static data,
 *         erase the banks, and disconnect the transport.
    \param client_context The context identifier for the client
    \return dfu_protocol_success if success, otherwise failure type */
dfu_protocol_status_t DfuProtocol_AbortOta(upgrade_context_t client_context);

/*! \brief Triggers the dfu_protocol_transport_disconnect state machine event.
 *         This will disconnect the Upgrade transport and clear the
 *         DFU protocol internal data without initiating an abort or releasing
 *         the profiles.
    \param client_context The context identifier for the client
    \return dfu_protocol_success if success, otherwise failure type */
dfu_protocol_status_t DfuProtocol_Disconnect(upgrade_context_t client_context);

#endif // DFU_PROTOCOL_H
