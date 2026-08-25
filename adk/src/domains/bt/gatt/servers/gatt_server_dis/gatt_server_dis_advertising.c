/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "gatt_server_dis_advertising.h"

#include "le_advertising_manager.h"
#include <gatt.h>
#include <panic.h>

#define SIZE_DEVICE_INFO_ADVERT             4

static const uint8 gatt_device_info_advert_data[SIZE_DEVICE_INFO_ADVERT] = { \
    SIZE_DEVICE_INFO_ADVERT - 1, \
    ble_ad_type_complete_uuid16, \
    GATT_SERVICE_UUID_DEVICE_INFORMATION & 0xFF, \
    GATT_SERVICE_UUID_DEVICE_INFORMATION >> 8 \
};

static le_adv_mgr_register_handle gatt_server_dis_registered_handle = NULL;
static le_adv_item_data_t gatt_device_info_advert;

static bool gattServerDeviceInfo_GetItemData(le_adv_item_data_t * item)
{
    PanicNull(item);
    *item = gatt_device_info_advert;
    return TRUE;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static bool gattServerDeviceInfo_GetItemInfo(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = gatt_device_info_advert.size };
    return TRUE;
}

le_adv_item_callback_t gattServerDeviceInfo_AdvertisingManagerCallback =
{
    .GetItemData = &gattServerDeviceInfo_GetItemData,
    .GetItemInfo = &gattServerDeviceInfo_GetItemInfo,
};

#else

#define NUMBER_OF_ADVERT_DATA_ITEMS         1

static bool gattDeviceInfoServer_CanAdvertiseService(const le_adv_data_params_t * params)
{
    /* DIS will not worry if LE Advertising Manager(LEAM) can't fit advert data into the
    advertising/scan response space.
    DIS advertising data is skippable (optional data item) meaning if LEAM can ignore it
    if it can not fit into advert space.*/
    return ((params->data_set == le_adv_data_set_handset_identifiable) &&
            (params->placement == le_adv_data_placement_dont_care) &&
            (params->completeness == le_adv_data_completeness_can_be_skipped)
           );
}

static unsigned int gattServerDeviceInfo_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    if(gattDeviceInfoServer_CanAdvertiseService(params))
    {
        return NUMBER_OF_ADVERT_DATA_ITEMS;
    }

    return 0;
}

static le_adv_data_item_t gattServerDeviceInfo_GetAdvData(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(params);

    /* Safety check to make sure advert data items 1 */
    COMPILE_TIME_ASSERT(NUMBER_OF_ADVERT_DATA_ITEMS == 1, AdvertDataItemExceeds);

    /* id is an index to the Number of Adv items i.e.gattServerDeviceInfo_NumberOfAdvItems()
    id is 0 indexed, so if 1 item in AdvItems, LE advertising manager will pass id = 0
    if 2 items in AdvItems, LE advertising manager will pass id = 0 and 1
    i.e this function will be called twice, first called with id=0 and and then id= 1.
    As gattServerDeviceInfo_NumberOfAdvItems() can't be more than 1, expectation that
    id should 0 and never be 1 or greater than 1.*/
    PanicFalse(id == 0);

    le_adv_data_item_t item;
    gattServerDeviceInfo_GetItemData(&item);
    return item;
}

static void gattServerDeviceInfo_ReleaseAdvData(const le_adv_data_params_t * params)
{
    UNUSED(params);
    return;
}

static const le_adv_data_callback_t gattServerDeviceInfo_AdvertisingManagerCallback =
{
    .GetNumberOfItems = gattServerDeviceInfo_NumberOfAdvItems,
    .GetItem = gattServerDeviceInfo_GetAdvData,
    .ReleaseItems = gattServerDeviceInfo_ReleaseAdvData
};

#endif

bool GattServerDis_SetupLeAdvertisingData(void)
{
    gatt_device_info_advert.size = SIZE_DEVICE_INFO_ADVERT;
    gatt_device_info_advert.data = gatt_device_info_advert_data;
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    gatt_server_dis_registered_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &gattServerDeviceInfo_AdvertisingManagerCallback);
#else
    gatt_server_dis_registered_handle = LeAdvertisingManager_Register(NULL, &gattServerDeviceInfo_AdvertisingManagerCallback);
#endif
    return (gatt_server_dis_registered_handle ? TRUE : FALSE);
}

bool GattServerDis_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return (gatt_server_dis_registered_handle ? LeAdvertisingManager_UpdateAdvertisingItem(gatt_server_dis_registered_handle) : FALSE);
#else
    return (gatt_server_dis_registered_handle ? LeAdvertisingManager_NotifyDataChange(NULL, gatt_server_dis_registered_handle) : FALSE);
#endif
}
