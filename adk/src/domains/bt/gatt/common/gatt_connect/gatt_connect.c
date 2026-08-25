/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application support for GATT connect/disconnect
*/

#include "gatt_connect.h"
#include "gatt_connect_observer.h"
#include "gatt_connect_list.h"
#include "gatt_connect_mtu.h"
#include <gatt_manager.h>
#include <gatt.h>
#include <logging.h>

#include <panic.h>
#include <l2cap_prim.h>

#include <connection_manager.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(gatt_connect_message_t)

#define GATT_RESULT_SUCCESS_ID         gatt_status_success
#define GATT_RESULT_LINK_TRANSFERRED   (0)  /* @ToDo: Not yet supported in legacy library */ 

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(APP_GATT, APP_GATT_MESSAGE_END)

#endif

static void gattConnect_MessageHandler(Task task, MessageId id, Message msg);

TaskData gatt_task_data = {gattConnect_MessageHandler};
Task gatt_connect_init_task;

static void gattConnect_HandleConnectInd(GATT_MANAGER_CONNECT_IND_T* ind)
{
    gatt_connection_t* connection;
    
    DEBUG_LOG("gattConnect_HandleConnectInd, cid 0x%04x flags 0x%04d", ind->cid, ind->flags);
    
    if(L2CA_CONFLAG_IS_LE(ind->flags))
    {
        connection = GattConnect_CreateConnection(ind->cid);
        
        if(connection)
        {
            GattConnect_SetMtu(connection, ind->mtu);
            GattConnect_SendExchangeMtuReq(&gatt_task_data, ind->cid);
            GattConnect_ObserverNotifyOnConnection(ind->cid);
            GattManagerConnectResponse(ind->cid, ind->flags, TRUE);
            return;
        }
    }
    
    GattManagerConnectResponse(ind->cid, ind->flags, FALSE);
}


static void gattConnect_UpdateDisconnectReasonCode(gatt_connection_t *connection, uint16 reason_code)
{
    DEBUG_LOG("gattConnect_UpdateDisconnectReasonCode %d", reason_code);

    if (connection != NULL)
    {
        switch (reason_code)
        {
            case GATT_RESULT_SUCCESS_ID:
                connection->disconnect_reason_code = gatt_connect_disconnect_reason_success;
            break;


            default:
                connection->disconnect_reason_code = gatt_connect_disconnect_reason_other;
            break;
        }
    }
}

static void gattConnect_HandleDisconnectInd(GATT_MANAGER_DISCONNECT_IND_T* ind)
{
    uint16 cid = ind->cid;
    gatt_connection_t *connection = GattConnect_FindConnectionFromCid(cid);

    gattConnect_UpdateDisconnectReasonCode(connection, ind->status);
    GattConnect_ObserverNotifyOnDisconnection(cid);
    GattConnect_DestroyConnection(cid);
}

static void gattConnect_HandleRegisterWithGattCfm(GATT_MANAGER_REGISTER_WITH_GATT_CFM_T* cfm)
{
    PanicFalse(cfm->status == gatt_manager_status_success);
    if(gatt_connect_init_task)
    {
        MessageSend(gatt_connect_init_task, GATT_CONNECT_SERVER_INIT_COMPLETE_CFM, NULL);
    }
}

static void gattConnect_HandleConManagerConnection(const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    if (ConManagerIsTpAclLocal(&ind->tpaddr))
    {
        GattManagerConnectAsCentral(&gatt_task_data, &ind->tpaddr.taddr, gatt_connection_ble_master_directed, TRUE);
    }
}

static void gattConnect_HandleConManagerDisconnection(const CON_MANAGER_TP_DISCONNECT_IND_T *ind)
{
    UNUSED(ind);
    GattManagerCancelConnectAsCentral(&gatt_task_data);
}

static void gattConnect_DisconnectRequestedResponse(gatt_cid_t cid)
{
    gatt_connection_t* connection = GattConnect_FindConnectionFromCid(cid);
    if (connection)
    {
        DEBUG_LOG("gattConnect_DisconnectRequestedResponse, cid 0x%04x pending_disconnects %d", cid, connection->pending_disconnects);
        if (connection->pending_disconnects)
        {
            connection->pending_disconnects--;
            if (connection->pending_disconnects == 0)
            {
                /* Synergy library relies on CM ACL close request for disconnection of GATT links */
                GattManagerDisconnectRequest(connection->cid);
            }
        }
    }
}

static void gattConnect_HandleConManagerDisconnectRequested(const CON_MANAGER_TP_DISCONNECT_REQUESTED_IND_T *ind)
{
    gatt_connection_t* connection = GattConnect_FindConnectionFromTpaddr(&ind->tpaddr);
    if (connection)
    {
        connection->pending_disconnects = GattConnect_ObserverGetNumberDisconnectReqCallbacksRegistered();
        DEBUG_LOG("gattConnect_HandleConManagerDisconnectRequested, cid 0x%04x pending_disconnects %d", connection->cid, connection->pending_disconnects);
        if (connection->pending_disconnects)
        {
            GattConnect_ObserverNotifyOnDisconnectRequested(connection->cid, gattConnect_DisconnectRequestedResponse);
        }
        else
        {
            /* Synergy library relies on CM ACL close request for disconnection of GATT links */
            GattManagerDisconnectRequest(connection->cid);
        }
    }
}

static void gattConnect_HandleGattManagerConnectAsCentral(const GATT_MANAGER_CONNECT_AS_CENTRAL_CFM_T *cfm)
{
    gatt_connection_t* connection = GattConnect_CreateConnection(cfm->cid);

    DEBUG_LOG("gattConnect_HandleGattManagerConnectAsCentral, cid 0x%04x flags 0x%04d", cfm->cid, cfm->flags);

    if(connection)
    {
        GattConnect_SetMtu(connection, cfm->mtu);
        GattConnect_SendExchangeMtuReq(&gatt_task_data, cfm->cid);
        GattConnect_ObserverNotifyOnConnection(cfm->cid);
    }
}

/*! \brief Handle encryption change indications

    If a LE ACL that is registered with GATT is encrypted notify
    the registered GATT observers.

    \param  ind     The change indication to process
*/
static void gattConnect_HandleClSmEncryptionChangeInd(CL_SM_ENCRYPTION_CHANGE_IND_T *ind)
{
    DEBUG_LOG("gattConnect_HandleClSmEncryptionChangeInd address %x encrypted %u",
              ind->tpaddr.taddr.addr.lap,
              ind->encrypted);

    if (ind->encrypted && (ind->tpaddr.transport == TRANSPORT_BLE_ACL))
    {
        gatt_connection_t *connection = GattConnect_FindConnectionFromTpaddr(&ind->tpaddr);
        if (connection)
        {
            DEBUG_LOG("gattConnect_HandleClSmEncryptionChangeInd cid 0x%4x encrypted %d",
                      connection->cid, ind->encrypted);

            GattConnect_ObserverNotifyOnEncryptionChanged(connection->cid, ind->encrypted);
        }
    }
}

bool GattConnect_HandleConnectionLibraryMessages(MessageId id, Message message,
                                                 bool already_handled)
{
    switch (id)
    {
        case CL_SM_ENCRYPTION_CHANGE_IND:
            gattConnect_HandleClSmEncryptionChangeInd((CL_SM_ENCRYPTION_CHANGE_IND_T *)message);
            return TRUE;

         default:
             break;
    }

    return already_handled;
}

static void gattConnect_GattMessageHandler(MessageId id, Message msg)
{
    switch(id)
    {
        case GATT_MANAGER_CONNECT_IND:
            gattConnect_HandleConnectInd((GATT_MANAGER_CONNECT_IND_T*)msg);
        break;
        
        case GATT_MANAGER_DISCONNECT_IND:
            gattConnect_HandleDisconnectInd((GATT_MANAGER_DISCONNECT_IND_T*)msg);
        break;
        
        case GATT_EXCHANGE_MTU_IND:
            gattConnect_HandleExchangeMtuInd((GATT_EXCHANGE_MTU_IND_T*)msg);
        break;
        
        case GATT_EXCHANGE_MTU_CFM:
            gattConnect_HandleExchangeMtuCfm((GATT_EXCHANGE_MTU_CFM_T*)msg);
        break;
        
        case GATT_MANAGER_REGISTER_WITH_GATT_CFM:
            gattConnect_HandleRegisterWithGattCfm((GATT_MANAGER_REGISTER_WITH_GATT_CFM_T*)msg);
        break;
        
        case GATT_MANAGER_CONNECT_AS_CENTRAL_CFM:
            gattConnect_HandleGattManagerConnectAsCentral((const GATT_MANAGER_CONNECT_AS_CENTRAL_CFM_T *)msg);
        break;
        default:
        break;        
    }
}

static void gattConnect_MessageHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    gattConnect_GattMessageHandler(id, msg);

    switch(id)
    {

        case CON_MANAGER_TP_CONNECT_IND:
            gattConnect_HandleConManagerConnection((const CON_MANAGER_TP_CONNECT_IND_T *)msg);
        break;
            
        case CON_MANAGER_TP_DISCONNECT_IND:
            gattConnect_HandleConManagerDisconnection((const CON_MANAGER_TP_DISCONNECT_IND_T *)msg);
        break;
        
        case CON_MANAGER_TP_DISCONNECT_REQUESTED_IND:
            gattConnect_HandleConManagerDisconnectRequested((const CON_MANAGER_TP_DISCONNECT_REQUESTED_IND_T *)msg);
        break;

        default:
        break;
    }
}

bool GattConnect_Init(Task init_task)
{
    UNUSED(init_task);
    GattConnect_MtuInit();
    GattConnect_ListInit();
    GattConnect_ObserverInit();
    ConManagerRegisterTpConnectionsObserver(cm_transport_ble, &gatt_task_data);
    PanicFalse(GattManagerInit(&gatt_task_data));
    return TRUE;
}

device_t GattConnect_GetBtDevice(unsigned cid)
{
    bdaddr public_addr;
    device_t device = NULL;

    if (GattConnect_GetPublicAddrFromConnectionId(cid, &public_addr))
    {
        device = BtDevice_GetDeviceForBdAddr(&public_addr);
    }
    return device;
}

device_t GattConnect_GetBtLeDevice(unsigned cid)
{
    tp_bdaddr tpaddr;

    if (GattConnect_FindTpaddrFromCid(cid, &tpaddr))
    {
        return  BtDevice_GetDeviceFromTpAddr(&tpaddr);
    }

    return NULL;
}

bool GattConnect_IsDeviceTypeOfPeer(unsigned cid)
{
    device_t device = GattConnect_GetBtDevice(cid);
    deviceType device_type = DEVICE_TYPE_MAX;

    if (device != NULL)
    {
        device_type = BtDevice_GetDeviceType(device);
    }

    return (device_type == DEVICE_TYPE_EARBUD || device_type == DEVICE_TYPE_SELF);
}

bool GattConnect_ServerInitComplete(Task init_task)
{
    gatt_connect_init_task = init_task;
    GattManagerRegisterWithGatt();
    return TRUE;
}


bool GattConnect_GetTpaddrFromConnectionId(unsigned cid, tp_bdaddr * tpaddr)
{
    return GattConnect_FindTpaddrFromCid(cid, tpaddr);
}

bool GattConnect_GetPublicAddrFromConnectionId(unsigned cid, bdaddr * addr)
{
    bool result = FALSE;
    tp_bdaddr tpaddr;

    if (GattConnect_FindTpaddrFromCid(cid, &tpaddr))
    {
        if (tpaddr.taddr.type != TYPED_BDADDR_PUBLIC)
        {
            tp_bdaddr public_tpaddr;
            if (VmGetPublicAddress(&tpaddr, &public_tpaddr))
            {
                tpaddr = public_tpaddr;
            }
        }

        if (tpaddr.taddr.type == TYPED_BDADDR_PUBLIC)
        {
            *addr = tpaddr.taddr.addr;
            result = TRUE;
        }
    }

    return result;
}

unsigned GattConnect_GetConnectionIdFromTpaddr(const tp_bdaddr *tpaddr)
{
    gatt_connection_t *connection = GattConnect_ResolveAndFindConnection(tpaddr);

    return connection != NULL ? connection->cid : INVALID_CID;
}

gatt_connect_disconnect_reason_t GattConnect_GetDisconnectReasonCode(gatt_cid_t cid)
{
    gatt_connect_disconnect_reason_t reason_code = gatt_connect_disconnect_reason_no_valid_connection;
    gatt_connection_t *connection = GattConnect_FindConnectionFromCid(cid);

    if (connection != NULL)
    {
        reason_code = connection->disconnect_reason_code;
    }

    return reason_code;
}

