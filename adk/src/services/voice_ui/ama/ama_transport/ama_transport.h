/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport.h
\brief      AMA transport APIs
*/

#ifndef AMA_TRANSPORT_H
#define AMA_TRANSPORT_H

#include "ama_transport_types.h"
#include <bdaddr.h>

/*! \brief Register callback to allow AMA transport to pass data to the parser
    \param callback The callback
*/
void AmaTransport_RegisterParser(data_received_callback_t callback);

/*! \brief Send data over the active transport
    \param data Pointer to the buffered data to be sent
    \param length Length of the buffered data
    \return TRUE if sent successfully, otherwise FALSE
*/
bool AmaTransport_SendData(uint8 * data, uint16 length);

/*! \brief Request transport disconnection
    \param reason The reason for requesting disconnection
*/
void AmaTransport_RequestDisconnect(ama_local_disconnect_reason_t reason);

/*! \brief Allow transport connections to be made
*/
void AmaTransport_AllowConnections(void);

/*! \brief Block transport connections from being made
*/
void AmaTransport_BlockConnections(void);

/*! \brief Set the active transport
    \param type The transport type
*/
void AmaTransport_SetActiveTransport(ama_transport_type_t type);

/*! \brief Get the active transport type
    \return The transport type
*/
ama_transport_type_t AmaTransport_GetActiveTransport(void);

/*! \brief Check if any transport is connected
    \return TRUE if any transport is connected, otherwise FALSE
*/
bool AmaTransport_IsConnected(void);

/*! \brief Get the active transport BT address
    \return The BT address
*/
const bdaddr * AmaTransport_GetBtAddress(void);

/*! \brief Set whether an AMA profile disconnect is required on handset disconnection
    \param TRUE if disconnect_required required, otherwise FALSE
*/
void AmaTransport_SetProfileDisconnectRequired(bool disconnect_required);

/*! \brief Register an app task to receive transport notifications
    \param The task to receive transport notifications
*/
void AmaTransport_Register(Task task);

/*! \brief Initialise the AMA transport module
*/
void AmaTransport_Init(void);

#endif // AMA_TRANSPORT_H
