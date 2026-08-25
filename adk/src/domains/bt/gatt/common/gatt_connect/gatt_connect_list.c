/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Tracking GATT connections
*/

#include "gatt_connect.h"
#include "gatt_connect_list.h"
#include "connection_manager.h"

#include <bdaddr.h>

static gatt_connection_t connections[GATT_CONNECT_MAX_CONNECTIONS];

gatt_connection_t* GattConnect_FindConnectionFromCid(unsigned cid)
{
    gatt_connection_t* result;
    
    for(result = &connections[0]; result <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; result++)
    {
        if(result->cid == cid)
            return result;
    }
    
    return NULL;
}

gatt_connection_t* GattConnect_CreateConnection(unsigned cid)
{
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(0);
    tp_bdaddr tpaddr;
    
    if(connection)
    {
        connection->cid = cid;
        if (VmGetBdAddrtFromCid(gattConnect_GetCid(cid), &tpaddr))
        {
            if (tpaddr.taddr.type != TYPED_BDADDR_PUBLIC)
            {
                tp_bdaddr public_tpaddr;

                if (VmGetPublicAddress(&tpaddr, &public_tpaddr))
                {
                    tpaddr = public_tpaddr;
                }
            }

            connection->tpaddr = tpaddr;
        }
        else
        {
            BdaddrTpSetEmpty(&connection->tpaddr);
        }
    }
    
    return connection;
}

void GattConnect_DestroyConnection(unsigned cid)
{
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(cid);
    
    if(connection)
    {
        memset(connection, 0, sizeof(gatt_connection_t));
        BdaddrTpSetEmpty(&connection->tpaddr);
    }
}

void GattConnect_ListInit(void)
{
    gatt_connection_t* connection;

    memset(connections, 0, sizeof(connections));
    for(connection = &connections[0]; connection <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; connection++)
    {
        BdaddrTpSetEmpty(&connection->tpaddr);
    }
}

gatt_connection_t* GattConnect_FindConnectionFromTpaddr(const tp_bdaddr *tpaddr_in)
{
    gatt_connection_t* result;
    tp_bdaddr tpaddr_for_cid;
    
    for(result = &connections[0]; result <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; result++)
    {
        if (VmGetBdAddrtFromCid(gattConnect_GetCid(result->cid), &tpaddr_for_cid))
        {
            if (BdaddrTpIsSame(tpaddr_in, &tpaddr_for_cid))
            {
                return result;
            }
        }
    }
    
    return NULL;
}

bool GattConnect_FindTpaddrFromCid(unsigned cid, tp_bdaddr * tpaddr)
{
    bool result = FALSE;
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(cid);
    
    if (connection)
    {
        if (!BdaddrTpIsEmpty(&connection->tpaddr))
        {
            *tpaddr = connection->tpaddr;
            
            result = TRUE;
        }
    }
    
    return result;
}

bool GattConnect_IsDisconnectPending(void)
{
    gatt_connection_t* result;
    for(result = &connections[0]; result <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; result++)
    {
        if(result->pending_disconnects)
            return TRUE;
    }
    return FALSE;
}

gatt_connection_t* GattConnect_ResolveAndFindConnection(const tp_bdaddr *in_tp_bd_addr)
{
    gatt_connection_t *result;
    tp_bdaddr resolved_tpaddr;
    tp_bdaddr in_resolved_tp_bd_addr;

    (void) ConManagerResolveTpaddr(in_tp_bd_addr, &in_resolved_tp_bd_addr);

    for(result = &connections[0]; result <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; result++)
    {
        (void) ConManagerResolveTpaddr(&result->tpaddr, &resolved_tpaddr);

        if (BdaddrIsSame(&resolved_tpaddr.taddr.addr, &in_resolved_tp_bd_addr.taddr.addr))
        {
            return result;
        }
    }

    return NULL;
}

uint8 GattConnect_GetConnectionCount(void)
{
    gatt_connection_t* connection;
    uint8 count = 0;

    for(connection = &connections[0]; connection <= &connections[GATT_CONNECT_MAX_CONNECTIONS - 1]; connection++)
    {
        if (!BdaddrTpIsEmpty(&connection->tpaddr))
        {
            count++;
        }
    }

    return count;
}
