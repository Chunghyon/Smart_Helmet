/*!
\copyright  Copyright (c) 2018 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage execution of callbacks to construct adverts and scan response
*/

#ifndef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_data_legacy.h"

#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_data_packet.h"

#include <stdlib.h>
#include <panic.h>


/*! Maximum data length of an advert if advertising length extensions are not used */
#define MAX_AD_DATA_SIZE_IN_OCTETS  (0x1F)


static le_advertising_manager_data_packet_t* le_adv_data_packet[le_adv_manager_data_packet_max];


static bool leAdvertisingManager_createNewLegacyDataPacket(le_adv_manager_data_packet_type_t type)
{
    le_advertising_manager_data_packet_t *new_packet = LeAdvertisingManager_DataPacketCreateDataPacket(MAX_AD_DATA_SIZE_IN_OCTETS);
    le_adv_data_packet[type] = new_packet;
    
    return TRUE;
}

static bool leAdvertisingManager_destroyLegacyDataPacket(le_adv_manager_data_packet_type_t type)
{
    LeAdvertisingManager_DataPacketDestroy(le_adv_data_packet[type]);
    le_adv_data_packet[type] = NULL;
    
    return TRUE;
}

static unsigned leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_type_t type)
{
    return LeAdvertisingManager_DataPacketGetSize(le_adv_data_packet[type]);
}

static bool leAdvertisingManager_addItemToLegacyDataPacket(le_adv_manager_data_packet_type_t type, const le_adv_data_item_t* item)
{
    return LeAdvertisingManager_DataPacketAddDataItem(le_adv_data_packet[type], item);
}


static void leAdvertisingManager_setupLegacyAdvertData(Task task, uint8 adv_handle)
{
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
    uint8 size_advert = leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_advert);
    uint8* advert_start[MAX_EXT_AD_DATA_BUFFER_COUNT] = {0};
    le_advertising_manager_data_packet_t *packet = le_adv_data_packet[le_adv_manager_data_packet_advert];

    if (size_advert)
    {
        for (uint8 i = 0; i < ARRAY_DIM(advert_start); i++)
        {
            advert_start[i] = packet->data[i];
        }
    }

    DEBUG_LOG_VERBOSE("leAdvertisingManager_setupLegacyAdvertData, Size is %d data:%p", size_advert, advert_start[0]);

    LeAdvertisingManager_DataPacketDebugLogData(packet);

    ConnectionDmBleExtAdvSetDataReq(task, adv_handle, complete_data, size_advert, advert_start);

    /* ConnectionDmBleExtAdvSetDataReq takes ownership of the buffers passed
       in via advert_start, so set the pointers to NULL in the packet data. */
    LeAdvertisingManager_DataPacketReset(packet);
#else
    uint8 size_advert = leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_advert);
    uint8* advert_start = size_advert ? le_adv_data_packet[le_adv_manager_data_packet_advert]->data[0] : NULL;

    UNUSED(task);
    UNUSED(adv_handle);
    
    DEBUG_LOG_VERBOSE("leAdvertisingManager_setupLegacyAdvertData, Size is %d", size_advert);

    leAdvertisingManager_DebugDataItems(size_advert, advert_start);

    ConnectionDmBleSetAdvertisingDataReq(size_advert, advert_start);
#endif
}

static void leAdvertisingManager_setupLegacyScanResponseData(Task task, uint8 adv_handle)
{
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
    uint8 size_scan_rsp = leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_scan_response);
    uint8* scan_rsp_start[MAX_EXT_AD_DATA_BUFFER_COUNT] = {0};
    le_advertising_manager_data_packet_t *packet = le_adv_data_packet[le_adv_manager_data_packet_scan_response];

    if (size_scan_rsp)
    {
        for (uint8 i = 0; i < ARRAY_DIM(scan_rsp_start); i++)
        {
            scan_rsp_start[i] = packet->data[i];
        }
    }

    DEBUG_LOG("leAdvertisingManager_setupLegacyScanResponseData, Size is %d", size_scan_rsp);

    LeAdvertisingManager_DataPacketDebugLogData(packet);

    ConnectionDmBleExtAdvSetScanRespDataReq(task, adv_handle, complete_data, size_scan_rsp, scan_rsp_start);

    /* ConnectionDmBleExtAdvSetScanRespDataReq takes ownership of the buffers passed
       in via scan_rsp_start, so set the pointers to NULL in the packet data. */
    LeAdvertisingManager_DataPacketReset(packet);
#else
    uint8 size_scan_rsp = leAdvertisingManager_getSizeLegacyDataPacket(le_adv_manager_data_packet_scan_response);
    uint8* scan_rsp_start = size_scan_rsp ? le_adv_data_packet[le_adv_manager_data_packet_scan_response]->data[0] : NULL;

    UNUSED(task);
    UNUSED(adv_handle);

    DEBUG_LOG("leAdvertisingManager_setupLegacyScanResponseData, Size is %d", size_scan_rsp);

    leAdvertisingManager_DebugDataItems(size_scan_rsp, scan_rsp_start);

    ConnectionDmBleSetScanResponseDataReq(size_scan_rsp, scan_rsp_start);
#endif
}

static bool leAdvertisingManager_getLegacyDataPacket(le_adv_manager_data_packet_type_t type, le_advertising_manager_data_packet_t *packet)
{
    if (le_adv_data_packet[type])
    {
        *packet = *le_adv_data_packet[type];
        return TRUE;
    }

    return FALSE;
}

static const le_advertising_manager_data_packet_if_t le_advertising_manager_legacy_data_fns = 
{
    .createNewDataPacket = leAdvertisingManager_createNewLegacyDataPacket,
    .destroyDataPacket = leAdvertisingManager_destroyLegacyDataPacket,
    .getSizeDataPacket = leAdvertisingManager_getSizeLegacyDataPacket,
    .addItemToDataPacket = leAdvertisingManager_addItemToLegacyDataPacket,
    .setupAdvertData = leAdvertisingManager_setupLegacyAdvertData,
    .setupScanResponseData = leAdvertisingManager_setupLegacyScanResponseData,
    .getDataPacket = leAdvertisingManager_getLegacyDataPacket
};

void leAdvertisingManager_RegisterLegacyDataIf(void)
{
    leAdvertisingManager_RegisterDataClient(LE_ADV_MGR_ADVERTISING_SET_LEGACY,
                                            &le_advertising_manager_legacy_data_fns);

    for (unsigned index = 0; index < le_adv_manager_data_packet_max; index++)
    {
        le_adv_data_packet[index] = NULL;
    }
}

#endif
