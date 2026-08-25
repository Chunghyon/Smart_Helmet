/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_client_config.h
\brief      Definition of the client config APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_CLIENT_CONFIG_H
#define DFU_PROTOCOL_CLIENT_CONFIG_H

#include <bdaddr.h>
#include <upgrade.h>

typedef struct
{
    Task task;
    upgrade_context_t context;
    const bdaddr * (*GetBtAddress)(void);
    uint32 in_progress_id;
    bool supports_silent_commit;
} dfu_protocol_client_config_t;

/*! \brief Initialise the client config module */
void DfuProtocol_InitialiseClientConfig(void);

/*! \brief Stores a pointer to the clients configuration for future reference
    \param config A pointer to the populated client config structure */
void DfuProtocol_RegisterClientConfig(const dfu_protocol_client_config_t * config);

/*! \brief Sets the active cient
    \param context The context identifier for the client */
void DfuProtocol_SetActiveClient(upgrade_context_t context);

/*! \brief Gets the client task
    \return The client task */
Task DfuProtocol_GetClientTask(void);

/*! \brief Gets the active client context identifier
    \return The active client context identifier */
upgrade_context_t DfuProtocol_GetActiveClientContext(void);

/*! \brief Gets the active client handset BT address
    \return The active client handset BT address */
const bdaddr * DfuProtocol_GetClientHandsetBtAddress(void);

/*! \brief Gets the active client in progress ID
    \return The active client in progress ID */
uint32 DfuProtocol_GetClientInProgressId(void);

/*! \brief Checks whether the active client supports silent commit
    \return TRUE if the active client supports silent commit, otherwise FALSE */
bool DfuProtocol_GetClientSupportSilentCommit(void);

/*! \brief Checks whether the active client caused a reboot
    \return TRUE if the active client caused a reboot, otherwise FALSE */
bool DfuProtocol_DidActiveClientCauseReboot(void);

#endif // DFU_PROTOCOL_CLIENT_CONFIG_H
