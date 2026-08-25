/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transports.c
\brief  Implementation of AMA transports list
*/

#ifdef INCLUDE_AMA
#include "ama_transports.h"
#include "ama_transport_profile.h"
#include "ama_transport_notify_app.h"
#include "ama_msg_types.h"
#include "ama_transport_version.h"
#include <logging.h>
#include <stdlib.h>

#define AMA_VERSION_MAJOR 1
#define AMA_VERSION_MINOR 0
#define AMA_VERSION_EXCHANGE_SIZE 20

static data_received_callback_t data_received_callback = NULL;
static ama_transport_t ama_transport_list[ama_transport_max];
static ama_transport_type_t active_transport = ama_transport_none;
static bdaddr ama_bd_addr;

static void amaTransport_Disconnect(void)
{
    DEBUG_LOG("amaTransport_Disconnect");
    
    if(active_transport != ama_transport_none)
    {
        PanicFalse(active_transport < ama_transport_max);
        PanicNull(ama_transport_list[active_transport].interface);

        if(ama_transport_list[active_transport].interface->handle_disconnect_request)
        {
            ama_transport_list[active_transport].interface->handle_disconnect_request(ama_local_disconnect_reason_normal);
        }
        else
        {
            DEBUG_LOG_WARN("AmaTransport_RequestDisconnect enum:ama_transport_type_t:%d does not handle user disconnection requests", active_transport);
        }
    }

    AmaTransport_Disconnected(active_transport);
}

static bool amaTransport_IsTransportConnected(ama_transport_type_t transport)
{
    return (transport == active_transport);
}

void AmaTransport_RegisterDataReceivedClient(data_received_callback_t callback)
{
    data_received_callback = callback;
}

void AmaTransport_RegisterTransport(ama_transport_type_t transport, ama_transport_if_t * transport_if)
{
    ama_transport_list[transport].interface = transport_if;
}

void AmaTransport_DataReceived(const uint8 * data, uint16 length)
{
    DEBUG_LOG_V_VERBOSE("AmaTransport_DataReceived");
    PanicNull((void *)data_received_callback);
    data_received_callback(data, length);
}

ama_transport_t * AmaTransport_GetTransportList(void)
{
    return ama_transport_list;
}

void AmaTransport_InternalSetActiveTransport(ama_transport_type_t transport)
{
    PanicFalse((transport < ama_transport_max) || (transport == ama_transport_none));
    active_transport = transport;
}

ama_transport_type_t AmaTransport_InternalGetActiveTransport(void)
{
    return active_transport;
}

bool AmaTransport_InternalIsConnected(void)
{
    bool connected = FALSE;

    for(ama_transport_type_t transport=0; transport < ama_transport_max; transport++)
    {
        if(amaTransport_IsTransportConnected(transport))
        {
            connected = TRUE;
            break;
        }
    }

    return connected;
}

const bdaddr * AmaTransport_InternalGetBtAddress(void)
{
    return &ama_bd_addr;
}

void AmaTransport_Connected(ama_transport_type_t transport, const bdaddr * bd_addr)
{
    DEBUG_LOG_FN_ENTRY("AmaTransport_Connected");

    if(active_transport == ama_transport_none)
    {
        DEBUG_LOG("AmaTransport_Connected enum:ama_transport_type_t:%d, [%x, %x, %lx]", transport, bd_addr->nap, bd_addr->uap, bd_addr->lap);

        AmaTransport_NotifyAppTransportSwitched(transport);
        memcpy(&ama_bd_addr, bd_addr, sizeof(bdaddr));
        DEBUG_LOG("Ama_TransportConnected sending transport version ID");
        MAKE_AMA_MESSAGE_WITH_LEN(AMA_SEND_TRANSPORT_VERSION_ID, AMA_VERSION_EXCHANGE_SIZE);
        message->pkt_size = AmaTransport_AddVersionInformation(message->packet);
        PanicNull((void *)ama_transport_list[transport].interface);
        PanicNull((void *)ama_transport_list[transport].interface->send_data);
        ama_transport_list[transport].interface->send_data(message->packet, message->pkt_size);
        free(message);
        AmaTransport_SendProfileConnectedInd(&ama_bd_addr);
    }
    else
    {
        DEBUG_LOG_WARN("AmaTransport_Connected IGNORED enum:ama_transport_type_t:%d, [%x, %x, %lx]", transport, bd_addr->nap, bd_addr->uap, bd_addr->lap);
    }
}

void AmaTransport_Disconnected(ama_transport_type_t transport_to_disconnect)
{
    DEBUG_LOG_FN_ENTRY("AmaTransport_Disconnected");

    if(active_transport == transport_to_disconnect)
    {
        AmaTransport_NotifyAppTransportSwitched(ama_transport_none);

        if (active_transport != ama_transport_none)
        {
            if(!BdaddrIsZero(&ama_bd_addr))
            {
                DEBUG_LOG("AmaTransport_Disconnected enum:ama_transport_type_t:%d, [%x, %x, %lx]", active_transport, ama_bd_addr.nap, ama_bd_addr.uap, ama_bd_addr.lap);
                AmaTransport_SendProfileDisconnectedInd(&ama_bd_addr);
                BdaddrSetZero(&ama_bd_addr);
            }
            else
            {
                DEBUG_LOG_WARN("AmaTransport_Disconnected bdaddr is zero");
            }
        }
    }
}

void AmaTransport_InitialiseTransports(void)
{
    AmaTransport_RegisterProfileClient(amaTransport_Disconnect);
}

#endif /* INCLUDE_AMA */
