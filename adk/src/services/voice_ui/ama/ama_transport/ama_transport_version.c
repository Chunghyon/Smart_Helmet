/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_version.c
\brief      Implementation of transport version-agnostic functions used by clients
*/

#ifdef INCLUDE_AMA

#include "ama_transport_version.h"

#include "panic.h"
#include "stdlib.h"

uint8* AmaTransport_AllocatePacketData(uint16 payload_length)
{
    uint16 header_len = AmaTransport_VersionGetHeaderSize(payload_length);
    uint8* packet = PanicUnlessMalloc(payload_length + header_len);
    return &packet[header_len];
}

void AmaTransport_FreePacketData(uint8* payload, uint16 payload_length)
{
    if(payload)
    {
        uint16 header_len = AmaTransport_VersionGetHeaderSize(payload_length);
        uint8* packet = payload - header_len;
        free(packet);
    }
}

#endif /* INCLUDE_AMA */
