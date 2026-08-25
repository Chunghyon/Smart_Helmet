/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service BLE state machine
*/

#include <bdaddr.h>
#include <device.h>
#include <device_properties.h>

#include "handset_service_ble_sm.h"
#include "handset_service_sm.h"
#include "handset_service_protected.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API
#include "handset_service_advertising.h"
#else
#include "handset_service_legacy_advertising.h"
#include "handset_service_extended_advertising.h"
#endif

#include "context_framework.h"

static void handsetService_BleSmEnterConnected(handset_service_ble_state_machine_t* ble_sm)
{
    handset_service_state_machine_t* sm = HandsetService_GetSmFromBleSm(ble_sm);
    DeviceProperties_SetHandsetContext(HandsetServiceSm_GetHandsetDeviceIfValid(sm), handset_context_none);
}

static void handsetService_BleSmEnterDisconnecting(handset_service_ble_state_machine_t* ble_sm)
{
    /* Remove LE ACL */
    if (!BdaddrTpIsEmpty(&ble_sm->le_addr))
    {
        ConManagerReleaseTpAcl(&ble_sm->le_addr);
    }
    else
    {
        /* We did not have anything to disconnect. 
           May be a disconnect message in flight, or a bug.
           Change state. uses recursion, but one level only */
        HandsetService_BleSmSetState(ble_sm, HANDSET_SERVICE_STATE_BLE_DISCONNECTED);
    }
}

static void handsetService_BleSmDeleteDeviceIfNotPaired(handset_service_ble_state_machine_t *sm)
{
    uint16 flags = DEVICE_FLAGS_NO_FLAGS;

    appDeviceGetFlagsForDevice(BtDevice_GetDeviceFromTpAddr(&sm->le_addr), &flags);

    if (flags & DEVICE_FLAGS_NOT_PAIRED)
    {
        HS_LOG("handsetService_BleSmDeleteDeviceIfNotPaired delete device");
        /* Delete the device as not paired */
        appDeviceDeleteWithTpAddr(&sm->le_addr);
    }
}

static void handsetService_BleSmEnterDisconnected(handset_service_ble_state_machine_t* ble_sm)
{
    handset_service_state_machine_t* sm = HandsetService_GetSmFromBleSm(ble_sm);
    bool all_sm_transports_disconnected = HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_AND_BLE);
    
    if(all_sm_transports_disconnected)
    {
        HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_success);
    }
    
    handsetService_BleSmDeleteDeviceIfNotPaired(ble_sm);

    BdaddrTpSetEmpty(&ble_sm->le_addr);

#ifndef LE_ADVERTISING_MANAGER_NEW_API
    HandsetService_UpdateLegacyAdvertisingData();
    UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
#endif
    
    if(all_sm_transports_disconnected)
    {
        HS_LOG("handsetService_BleSmEnterDisconnected destroying sm for dev 0x%x", sm->handset_device);
        HandsetServiceSm_DeInit(sm);
    }
}

void HandsetService_BleSmReset(handset_service_ble_state_machine_t* ble_sm)
{
    if (!BdaddrIsZero(&ble_sm->le_addr.taddr.addr))
    {
        /* Delete the unpaired device matching with the address if it exists*/
        handsetService_BleSmDeleteDeviceIfNotPaired(ble_sm);
    }
    BdaddrTpSetEmpty(&ble_sm->le_addr);
    ble_sm->le_state = HANDSET_SERVICE_STATE_BLE_DISCONNECTED;
}

void HandsetService_BleSmSetState(handset_service_ble_state_machine_t* ble_sm, handset_service_ble_state_t state)
{
    handset_service_ble_state_t old_state = ble_sm->le_state;

    HS_LOG("HandsetService_BleSmSetState enum:handset_service_ble_state_t:%d -> enum:handset_service_ble_state_t:%d", old_state, state);

    ble_sm->le_state = state;

    if (old_state != HANDSET_SERVICE_STATE_BLE_CONNECTED && state == HANDSET_SERVICE_STATE_BLE_CONNECTED)
    {
        handsetService_BleSmEnterConnected(ble_sm);
    }

    if(old_state != HANDSET_SERVICE_STATE_BLE_DISCONNECTING && state == HANDSET_SERVICE_STATE_BLE_DISCONNECTING)
    {
        handsetService_BleSmEnterDisconnecting(ble_sm);
    }

    if(old_state != HANDSET_SERVICE_STATE_BLE_DISCONNECTED && state == HANDSET_SERVICE_STATE_BLE_DISCONNECTED)
    {
        handsetService_BleSmEnterDisconnected(ble_sm);
    }
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    if(old_state != state && (state == HANDSET_SERVICE_STATE_BLE_CONNECTED || state == HANDSET_SERVICE_STATE_BLE_DISCONNECTED))
    {
        HandsetService_UpdateAdvertising();
    }
#endif
    ContextFramework_NotifyContextUpdate(context_connected_handsets_info);
}

bool HandsetService_BleIsConnected(handset_service_ble_state_machine_t* ble_sm)
{
    PanicNull(ble_sm);
    
    switch(ble_sm->le_state)
    {
        case HANDSET_SERVICE_STATE_BLE_CONNECTED:
        case HANDSET_SERVICE_STATE_BLE_DISCONNECTING:
        {
            if (!BdaddrTpIsEmpty(&ble_sm->le_addr) && ConManagerIsTpConnected(&ble_sm->le_addr))
            {
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
        
        default:
        {
            return FALSE;
        }
    }
}

void HandsetService_BleDisconnectIfConnected(handset_service_ble_state_machine_t* ble_sm)
{
    if (HandsetService_BleIsConnected(ble_sm))
    {
        HandsetService_BleSmSetState(ble_sm, HANDSET_SERVICE_STATE_BLE_DISCONNECTING);
    }
}

void HandsetService_BleHandleConnected(handset_service_ble_state_machine_t* ble_sm, tp_bdaddr* tpaddr)
{
    if (BdaddrTpIsEmpty(&ble_sm->le_addr))
    {
        ble_sm->le_addr = *tpaddr;
    }
    
    HandsetService_BleSmSetState(ble_sm, HANDSET_SERVICE_STATE_BLE_CONNECTED);
}
