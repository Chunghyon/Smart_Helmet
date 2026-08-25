/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transports.h
\brief      AMA transport list internal APIs
*/

#ifndef AMA_TRANSPORTS_H
#define AMA_TRANSPORTS_H

#include "ama_transport_types.h"
#include <bdaddr.h>
#include <message.h>

/*! Structure to be populated by a registering transport */
typedef struct
{
    bool(*send_data)(uint8 * data, uint16 length);
    bool(*handle_disconnect_request)(ama_local_disconnect_reason_t reason);
    void(*allow_connections)(void);
    void(*block_connections)(void);
} ama_transport_if_t;

/*! The format of a member of the transport list */
typedef struct
{
    ama_transport_if_t * interface;
} ama_transport_t;

/*! \brief Register callback to allow an AMA transport to pass data to the transport top-level
    \param callback The callback
*/
void AmaTransport_RegisterDataReceivedClient(data_received_callback_t callback);

/*! \brief Register a transport with the transport list
    \param transport The transport type
    \param transport_if A pointer to the transports implementation of the transport interface
*/
void AmaTransport_RegisterTransport(ama_transport_type_t transport, ama_transport_if_t * transport_if);

/*! \brief Forward received data to the relevant modules
    \param data Pointer to the buffered data to be sent
    \param length Length of the buffered data
*/
void AmaTransport_DataReceived(const uint8 * data, uint16 length);

/*! \brief Return the transport list
    \return Pointer to the head of the transport list
*/
ama_transport_t * AmaTransport_GetTransportList(void);

/*! \brief Set the active transport
    \param type The transport type
*/
void AmaTransport_InternalSetActiveTransport(ama_transport_type_t type);

/*! \brief Get the active transport type
    \return The transport type
*/
ama_transport_type_t AmaTransport_InternalGetActiveTransport(void);

/*! \brief Check if any transport is connected
    \return TRUE if any transport is connected, otherwise FALSE
*/
bool AmaTransport_InternalIsConnected(void);

/*! \brief Get the active transport BT address
    \return The BT address
*/
const bdaddr * AmaTransport_InternalGetBtAddress(void);

/*! \brief Handle transport connected
    \param transport The transport type
    \param bd_addr The BT address
*/
void AmaTransport_Connected(ama_transport_type_t transport, const bdaddr * bd_addr);

/*! \brief Handle transport disconnected
    \param transport The transport type
*/
void AmaTransport_Disconnected(ama_transport_type_t transport);

/*! \brief Initialise the AMA transports module
*/
void AmaTransport_InitialiseTransports(void);

#endif // AMA_TRANSPORTS_H
