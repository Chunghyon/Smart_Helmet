/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "gatt_server_battery_advertising.h"

#include "le_advertising_manager.h"

#include <gatt.h>
#include <panic.h>

#define SIZE_BATTERY_ADVERT                 4

static const uint8 gatt_battery_advert_data[SIZE_BATTERY_ADVERT] = { \
    SIZE_BATTERY_ADVERT - 1, \
    ble_ad_type_complete_uuid16, \
    GATT_SERVICE_UUID_BATTERY_SERVICE & 0xFF, \
    GATT_SERVICE_UUID_BATTERY_SERVICE >> 8 \
};

static le_adv_mgr_register_handle gatt_server_battery_registered_handle = NULL;
static le_adv_item_data_t gatt_battery_advert;

static bool gattBatteryServer_GetItemData(le_adv_item_data_t * data)
{
    PanicNull(data);
    *data = gatt_battery_advert;
    return TRUE;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static bool gattBatteryServer_GetItemInfo(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = gatt_battery_advert.size };
    return TRUE;
}

le_adv_item_callback_t gattBatteryServer_AdvertisingManagerCallback =
{
    .GetItemData = &gattBatteryServer_GetItemData,
    .GetItemInfo = &gattBatteryServer_GetItemInfo,
};

#else

#define NUMBER_OF_ADVERT_DATA_ITEMS         1

static bool gattBatteryServer_CanAdvertiseService(const le_adv_data_params_t * params)
{
    bool can_advertise = FALSE;
    if(params->data_set == le_adv_data_set_handset_identifiable
            && params->completeness == le_adv_data_completeness_can_be_skipped
            && params->placement == le_adv_data_placement_advert)
    {
        can_advertise = TRUE;
    }
    return can_advertise;
}

static unsigned int gattServerBattery_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    if(gattBatteryServer_CanAdvertiseService(params))
    {
        return NUMBER_OF_ADVERT_DATA_ITEMS;
    }

    return 0;
}

static le_adv_data_item_t gattServerBattery_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(id);
    le_adv_data_item_t item = { .size = 0, .data = NULL };
    if(gattBatteryServer_CanAdvertiseService(params) == FALSE)
    {
        Panic();
    }
    gattBatteryServer_GetItemData(&item);
    return item;
}

static void gattServerBattery_ReleaseAdvDataItems(const le_adv_data_params_t * params)
{
    UNUSED(params);
    return;
}

static const le_adv_data_callback_t gattBatteryServer_AdvertisingManagerCallback =
{
    .GetNumberOfItems = gattServerBattery_NumberOfAdvItems,
    .GetItem = gattServerBattery_GetAdvDataItems,
    .ReleaseItems = gattServerBattery_ReleaseAdvDataItems
};

#endif

bool GattServerBattery_SetupLeAdvertisingData(void)
{
    gatt_battery_advert.size = SIZE_BATTERY_ADVERT;
    gatt_battery_advert.data = gatt_battery_advert_data;
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    gatt_server_battery_registered_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &gattBatteryServer_AdvertisingManagerCallback);
#else
    gatt_server_battery_registered_handle = LeAdvertisingManager_Register(NULL, &gattBatteryServer_AdvertisingManagerCallback);
#endif
    return (gatt_server_battery_registered_handle ? TRUE : FALSE);
}

bool GattServerBattery_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return (gatt_server_battery_registered_handle ? LeAdvertisingManager_UpdateAdvertisingItem(gatt_server_battery_registered_handle) : FALSE);
#else
    return (gatt_server_battery_registered_handle ? LeAdvertisingManager_NotifyDataChange(NULL, gatt_server_battery_registered_handle) : FALSE);
#endif
}
