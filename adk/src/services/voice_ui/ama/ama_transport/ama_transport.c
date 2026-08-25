/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport.c
\brief  Implementation of AMA transport
*/

#ifdef INCLUDE_AMA
#include "ama_transport_version.h"
#include "ama_transport.h"
#include "ama_transports.h"
#include "ama_transport_profile.h"
#include "ama_transport_notify_app.h"
#include <logging.h>

static data_received_callback_t data_received_callback;

static bool amaTransport_ReceivedData(const uint8 * data, uint16 length)
{
    PanicNull((void *)data_received_callback);
    data_received_callback(data, length);
    return TRUE;
}

void AmaTransport_RegisterParser(data_received_callback_t callback)
{
    data_received_callback = callback;
}

bool AmaTransport_SendData(uint8 * data, uint16 length)
{
    bool data_sent = FALSE;

    ama_transport_type_t active_transport = AmaTransport_GetActiveTransport();
    
    DEBUG_LOG_V_VERBOSE("AmaTransport_SendData enum:ama_transport_type_t:%d", active_transport);   

    if(active_transport != ama_transport_none)
    {
        ama_transport_t * transport_list = AmaTransport_GetTransportList();

        PanicNull((void *)transport_list[active_transport].interface);
        PanicNull((void *)transport_list[active_transport].interface->send_data);
        data_sent = transport_list[active_transport].interface->send_data(data, length);
    }

    return data_sent;
}

void AmaTransport_RequestDisconnect(ama_local_disconnect_reason_t reason)
{
    DEBUG_LOG("AmaTransport_RequestDisconnect");                                            
    ama_transport_type_t active_transport = AmaTransport_GetActiveTransport();

    if(active_transport != ama_transport_none)
    {
        ama_transport_t * transport_list = AmaTransport_GetTransportList();

        PanicNull((void *)transport_list[active_transport].interface);

        if(transport_list[active_transport].interface->handle_disconnect_request)
        {
            transport_list[active_transport].interface->handle_disconnect_request(reason);
        }
        else
        {
            DEBUG_LOG_WARN("AmaTransport_RequestDisconnect enum:ama_transport_type_t:%d does not handle user disconnection requests", active_transport);
        }
    }
}

void AmaTransport_AllowConnections(void)
{
    ama_transport_t * transport_list = AmaTransport_GetTransportList();

    for(ama_transport_type_t current_transport=0; current_transport<ama_transport_max; current_transport++)
    {
        if(transport_list[current_transport].interface && transport_list[current_transport].interface->allow_connections)
        {
            transport_list[current_transport].interface->allow_connections();
            DEBUG_LOG("AmaTransport_AllowConnections enum:ama_transport_type_t:%d", current_transport);
        }
        else
        {
            DEBUG_LOG("AmaTransport_AllowConnections enum:ama_transport_type_t:%d does not support allowing/blocking connections", current_transport);
        }
    }
}

void AmaTransport_BlockConnections(void)
{
    ama_transport_t * transport_list = AmaTransport_GetTransportList();

    for(ama_transport_type_t current_transport=0; current_transport<ama_transport_max; current_transport++)
    {
        if(transport_list[current_transport].interface && transport_list[current_transport].interface->block_connections)
        {
            transport_list[current_transport].interface->block_connections();
            DEBUG_LOG("AmaTransport_BlockConnections enum:ama_transport_type_t:%d", current_transport);
        }
        else
        {
            DEBUG_LOG("AmaTransport_BlockConnections enum:ama_transport_type_t:%d does not support allowing/blocking connections", current_transport);
        }
    }
}

void AmaTransport_SetActiveTransport(ama_transport_type_t type)
{
    DEBUG_LOG("AmaTransport_SetActiveTransport enum:ama_transport_type_t:%d", type);                                                                           
    AmaTransport_InternalSetActiveTransport(type);
}

ama_transport_type_t AmaTransport_GetActiveTransport(void)
{
    return AmaTransport_InternalGetActiveTransport();
}

bool AmaTransport_IsConnected(void)
{
    return AmaTransport_InternalIsConnected();
}

const bdaddr * AmaTransport_GetBtAddress(void)
{
    return AmaTransport_InternalGetBtAddress();
}

void AmaTransport_SetProfileDisconnectRequired(bool disconnect_required)
{
    AmaTransport_InternalSetProfileDisconnectRequired(disconnect_required);
}

void AmaTransport_Register(Task task)
{
    if(task)
    {
        AmaTransport_RegisterAppTask(task);
    }
    else
    {
        DEBUG_LOG_WARN("AmaTransport_Register task is NULL");
    }
}

void AmaTransport_Init(void)
{
    DEBUG_LOG("AmaTransport_Init");
    AmaTransport_InitialiseTransports();
    AmaTransport_InitAppTaskList();
    AmaTransport_RegisterDataReceivedClient(amaTransport_ReceivedData);
    AmaTransport_ProfileInit();
    AmaTransport_RegisterParser(AmaTransport_ParseRxData);
    AmaTransport_PacketInit();
}

#endif /* INCLUDE_AMA */
