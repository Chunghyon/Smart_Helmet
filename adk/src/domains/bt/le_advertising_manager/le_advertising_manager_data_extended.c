/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct extended adverts and scan response
*/

#ifndef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_private.h"

#if defined(INCLUDE_ADVERTISING_EXTENSIONS)

#include "le_advertising_manager_data_extended.h"

#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_data_packet.h"
#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_uuid.h"
#include "le_advertising_manager_local_name.h"

#include <stdlib.h>
#include <panic.h>



static le_advertising_manager_data_packet_t *le_ext_adv_data_packets[le_adv_manager_data_packet_max] = {0};


static bool leAdvertisingManager_createNewExtendedDataPacket(le_adv_manager_data_packet_type_t type)
{
    le_advertising_manager_data_packet_t* new_packet;

    if (le_ext_adv_data_packets[type])
    {
        new_packet = le_ext_adv_data_packets[type];
    }
    else
    {
        new_packet = LeAdvertisingManager_DataPacketCreateDataPacket(MAX_EXT_AD_DATA_SIZE_IN_OCTETS);
        le_ext_adv_data_packets[type] = new_packet;
        DEBUG_LOG_VERBOSE("leAdvertisingManager_createNewExtendedDataPacket type: enum:le_adv_manager_data_packet_type_t:%d new_ptr: %p prev_ptr: %p",
                          type, new_packet, le_ext_adv_data_packets[type]);
    }
    
    return TRUE;
}

static bool leAdvertisingManager_destroyExtendedDataPacket(le_adv_manager_data_packet_type_t type)
{
    DEBUG_LOG_VERBOSE("leAdvertisingManager_destroyExtendedDataPacket type: enum:le_adv_manager_data_packet_type_t:%d ptr: %p",
                      type, le_ext_adv_data_packets[type]);

    LeAdvertisingManager_DataPacketDestroy(le_ext_adv_data_packets[type]);
    le_ext_adv_data_packets[type] = NULL;

    return TRUE;
}

static unsigned leAdvertisingManager_getSizeExtendedDataPacket(le_adv_manager_data_packet_type_t type)
{
    return (le_ext_adv_data_packets[type] ? LeAdvertisingManager_DataPacketGetSize(le_ext_adv_data_packets[type]) : 0);
}

static bool leAdvertisingManager_addItemToExtendedDataPacket(le_adv_manager_data_packet_type_t type, const le_adv_data_item_t* item)
{
    return LeAdvertisingManager_DataPacketAddDataItem(le_ext_adv_data_packets[type], item);
}

static void leAdvertisingManager_setupExtendedAdvertData(Task task, uint8 adv_handle)
{
    uint8 size_advert = leAdvertisingManager_getSizeExtendedDataPacket(le_adv_manager_data_packet_advert);
    uint8* advert_start[MAX_EXT_AD_DATA_BUFFER_COUNT] = {0};
    le_advertising_manager_data_packet_t *packet = le_ext_adv_data_packets[le_adv_manager_data_packet_advert];

    if (size_advert)
    {
        for (uint8 i = 0; i < ARRAY_DIM(advert_start); i++)
        {
            advert_start[i] = packet->data[i];
        }
    }

    DEBUG_LOG_VERBOSE("leAdvertisingManager_setupExtendedAdvertData, Size is %d data:%p", size_advert, advert_start[0]);

    LeAdvertisingManager_DataPacketDebugLogData(packet);

    ConnectionDmBleExtAdvSetDataReq(task, adv_handle, complete_data, size_advert, advert_start);

    /* ConnectionDmBleExtAdvSetDataReq takes ownership of the buffers passed
       in via advert_start, so set the pointers to NULL in the packet data. */
    LeAdvertisingManager_DataPacketReset(packet);
}

static void leAdvertisingManager_setupExtendedScanResponseData(Task task, uint8 adv_handle)
{
    uint8 size_scan_rsp = leAdvertisingManager_getSizeExtendedDataPacket(le_adv_manager_data_packet_scan_response);
    uint8* scan_rsp_start[MAX_EXT_AD_DATA_BUFFER_COUNT] = {0};
    le_advertising_manager_data_packet_t *packet = le_ext_adv_data_packets[le_adv_manager_data_packet_scan_response];

    if (size_scan_rsp)
    {
        for (uint8 i = 0; i < ARRAY_DIM(scan_rsp_start); i++)
        {
            scan_rsp_start[i] = packet->data[i];
        }
    }
    
    DEBUG_LOG("leAdvertisingManager_setupExtendedScanResponseData, Size is %d", size_scan_rsp);

    LeAdvertisingManager_DataPacketDebugLogData(packet);

    ConnectionDmBleExtAdvSetScanRespDataReq(task, adv_handle, complete_data, size_scan_rsp, scan_rsp_start);

    /* ConnectionDmBleExtAdvSetScanRespDataReq takes ownership of the buffers passed
       in via scan_rsp_start, so set the pointers to NULL in the packet data. */
    LeAdvertisingManager_DataPacketReset(packet);
}

static bool leAdvertisingManager_getExtendedDataPacket(le_adv_manager_data_packet_type_t type, le_advertising_manager_data_packet_t *packet)
{
    if (le_ext_adv_data_packets[type])
    {
        *packet = *le_ext_adv_data_packets[type];
        return TRUE;
    }

    return FALSE;
}

static const le_advertising_manager_data_packet_if_t le_advertising_manager_extended_data_fns = 
{
    .createNewDataPacket = leAdvertisingManager_createNewExtendedDataPacket,
    .destroyDataPacket = leAdvertisingManager_destroyExtendedDataPacket,
    .getSizeDataPacket = leAdvertisingManager_getSizeExtendedDataPacket,
    .addItemToDataPacket = leAdvertisingManager_addItemToExtendedDataPacket,
    .setupAdvertData = leAdvertisingManager_setupExtendedAdvertData,
    .setupScanResponseData = leAdvertisingManager_setupExtendedScanResponseData,
    .getDataPacket = leAdvertisingManager_getExtendedDataPacket
};


void leAdvertisingManager_RegisterExtendedDataIf(le_adv_data_set_t set)
{
    leAdvertisingManager_RegisterDataClient(set,
                                            &le_advertising_manager_extended_data_fns);

    for (unsigned index = 0; index < le_adv_manager_data_packet_max; index++)
    {
        le_ext_adv_data_packets[index] = NULL;
    }
}

#endif /* INCLUDE_ADVERTISING_EXTENSIONS*/
#endif
