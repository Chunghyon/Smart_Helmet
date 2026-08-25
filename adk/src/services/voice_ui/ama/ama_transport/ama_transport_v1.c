/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_v1.c
\brief      Implementation of transport version API (see ama_transport_version.h)
*/

#ifdef INCLUDE_AMA

#include "ama_transport_version.h"
#include "ama_transport_v1.h"
#include "ama_transport.h"
#include "logging.h"
#include "ama_receive_command.h"
#include <stdlib.h>

#define AMA_VERSION_MAJOR 1
#define AMA_VERSION_MINOR 0
#define AMA_VERSION_EXCHANGE_SIZE 20
#define MAX_SMALL_PACKET_SIZE   0xFF

typedef enum
{
    amaParseTransportStateIdle,
    amaParseTransportStateHeader,
    amaParseTransportStateLength,
    amaParseTransportStateBody,
    amaParseTransportLast
}amaParseTransportState_t;

static uint16 header = 0;
static uint16 protoPktLength = PACKET_INVALID_LENGTH;
static uint8* protoBody = NULL;
static uint16 bytesCopied = 0;
static amaParseTransportState_t amaParseState = amaParseTransportStateIdle;
static uint16 amaVersion = 0;

uint16 AmaTransport_AddVersionInformation(uint8* packet)
{
    packet[1] = 0x03;
    packet[0] = 0xFE;
    packet[2] = AMA_VERSION_MAJOR;
    packet[3] = AMA_VERSION_MINOR;

    /* Transport packet size */
    packet[4] = 0;
    packet[5] = 0;

    /* Maximum transactional data size */
    packet[6] = 0;
    packet[7] = 0;

    return sizeof(uint8) * AMA_VERSION_EXCHANGE_SIZE;
}

static uint16 amaTransport_AddPacketHeader(uint16 stream_id, uint8 *packet, uint16 length)
{
    uint16 streamHeader = 0;

    streamHeader = (amaVersion <<AMA_HEADER_VERSION_OFFSET) & AMA_HEADER_VERSION_MASK;
    streamHeader |= (stream_id<<AMA_HEADER_STREAM_ID_OFFSET) & AMA_HEADER_STREAM_ID_MASK;

    if(length > 255)
    {
        streamHeader |= AMA_HEADER_LENTGH_MASK;

        packet[2] = (uint8) (length>>8);
        packet[3] = (uint8) (length & 0xFF);

        length += 4;
    }
    else
    {
        packet[2] = length;
        length += 3;
    }

    packet[0] = (uint8) (streamHeader>>8);
    packet[1] = (uint8) (streamHeader & 0xFF);

    return length;
}

bool AmaTransport_TransmitData(ama_stream_type_t stream_type, uint8 * payload, uint16 payload_length)
{
    /* Memory for tx data is allocated so as to include header, but client of memory allocation only knows about payload, so
     * adjustment has to be made to negatively offset memory pointer by size of header */
    uint8* packet = payload - AmaTransport_VersionGetHeaderSize(payload_length);
    uint16 packet_size = amaTransport_AddPacketHeader(stream_type, packet, payload_length);
    return AmaTransport_SendData(packet, packet_size);
}

uint16 AmaTransport_VersionGetHeaderSize(const uint16 payload_len)
{
    return payload_len > MAX_SMALL_PACKET_SIZE ? 4 : 3;
}

void AmaTransport_PacketInit(void)
{
}

bool AmaTransport_ParseRxData(const uint8 * stream, uint16 size)
{

    DEBUG_LOG("AmaTransport_ParseRxData, size is %d", size );

    while(size)
    {
        switch(amaParseState)
        {
            case amaParseTransportStateIdle:
                if(size > 0)
                {
                    header = (uint16)stream[0]<<8;
                    amaParseState = amaParseTransportStateHeader;
                    stream++;
                    size--;
                }

                if(size > 0)
                {
                    header |= (uint16)stream[0];
                    amaParseState = amaParseTransportStateLength;
                    stream++;
                    size--;
                }
                break;

            case amaParseTransportStateHeader:
                if(size > 0)
                {
                    header |= (uint16)stream[0];
                    amaParseState = amaParseTransportStateLength;
                    stream++;
                    size--;
                }
                break;

            case amaParseTransportStateLength:
                if(size > 0)
                {
                    if(header & AMA_HEADER_LENTGH_MASK)
                    {
                        if(protoPktLength == PACKET_INVALID_LENGTH)
                        {
                            protoPktLength = (uint16)stream[0]<<8;
                        }
                        else
                        {
                            protoPktLength |= (uint16)stream[0];
                            amaParseState = amaParseTransportStateBody;
                        }
                        stream++;
                        size--;
                    }
                    else
                    {
                        protoPktLength = (uint16)stream[0];
                        amaParseState = amaParseTransportStateBody;
                        stream++;
                        size--;
                    }

                    if(amaParseState == amaParseTransportStateBody)
                    {
                        if(protoBody != NULL)
                            Panic();
                        protoBody = PanicUnlessMalloc(protoPktLength);
                    }
                }
                break;

            case amaParseTransportStateBody:
                {
                    uint16 remainToCopy = protoPktLength - bytesCopied;
                    uint16 bytesToCopy = MIN(size,remainToCopy);

                    DEBUG_LOG("AMA TRANSPORT amaParseTransportStateBody bytes to Copy is %d", bytesToCopy);

                    if(bytesToCopy)
                    {
                        memcpy(protoBody + bytesCopied, stream, bytesToCopy);
                        bytesCopied += bytesToCopy;
                        stream += bytesToCopy;
                        size -= bytesToCopy;
                    }

                    remainToCopy = protoPktLength - bytesCopied;

                    if(remainToCopy == 0)
                    {
                        /* received a complete protobuff packet */
                        amaVersion = (header & AMA_HEADER_VERSION_MASK)>>AMA_HEADER_VERSION_OFFSET;

                        AmaReceive_Command((char*)protoBody, protoPktLength);

                        AmaTransport_RxDataReset();
                    }
                }
                break;

            default:
                break;

        }
    } /* end if while(size) */

    return (amaParseState == amaParseTransportStateIdle);
}

void AmaTransport_RxDataReset(void)
{
    header = 0;
    protoPktLength = PACKET_INVALID_LENGTH;

    free(protoBody);
    protoBody = NULL;

    bytesCopied = 0;
    amaParseState = amaParseTransportStateIdle;
}

#endif /* INCLUDE_AMA */
