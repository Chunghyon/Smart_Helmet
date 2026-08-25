/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_version.h
\brief      Public API for transport version.
*/

#ifndef AMA_TRANSPORT_VERSION_H
#define AMA_TRANSPORT_VERSION_H

#include "ama_transport_version_types.h"
#include "ama_transport_v1.h"

/*! \brief Initialises the transport version modules
*/
void AmaTransport_PacketInit(void);

/*! \brief Add the transport version information
    \param packet - packet to add version info to
    \return packet length
*/
uint16 AmaTransport_AddVersionInformation(uint8* packet);

/*! \brief Parses an incoming AMA packet
    \param stream - packet data stream
    \param size - size of packet data stream
    \return TRUE if successful, otherwise FALSE
*/
bool AmaTransport_ParseRxData(const uint8 * stream, uint16 size);

/*! \brief Resets packet parsing state
*/
void AmaTransport_RxDataReset(void);

/*! \brief Transmits data to the gateway
    \param stream_type - control or voice
    \param payload - payload of packet
    \param payload_length - length of payload
    \return TRUE if successful, otherwise FALSE
*/
bool AmaTransport_TransmitData(ama_stream_type_t stream_type, uint8 * payload, uint16 payload_length);

/*! \brief Allocates memory for the payload
    \param payload_length - length of payload
    \return pointer to new payload memory
*/
uint8* AmaTransport_AllocatePacketData(uint16 payload_length);

/*! \brief Frees memory of the payload
 *  \param payload - payload of packet
    \param payload_length - length of payload
*/
void AmaTransport_FreePacketData(uint8* payload, uint16 payload_length);

#endif // AMA_TRANSPORT_VERSION_H
