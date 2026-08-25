/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "gatt_server_gap_advertising.h"
#include "gatt_server_gap.h"

#include "le_advertising_manager.h"
#include "local_name.h"

#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#define GAP_ADVERT_FLAGS        (BLE_FLAGS_GENERAL_DISCOVERABLE_MODE | BLE_FLAGS_DUAL_CONTROLLER | BLE_FLAGS_DUAL_HOST)
#define GAP_ADVERT_FLAGS_LENGTH 3

static const uint8 gap_adv_flags_data[GAP_ADVERT_FLAGS_LENGTH] =
{
    GAP_ADVERT_FLAGS_LENGTH - 1,
    ble_ad_type_flags,
    GAP_ADVERT_FLAGS
};

static le_adv_item_data_t gap_name_item = {0};
static const le_adv_item_data_t gap_flags_item = {.data = gap_adv_flags_data, .size = sizeof(gap_adv_flags_data)};
static bool name_legacy_in_use = FALSE;
static bool name_extended_in_use = FALSE;

static inline unsigned gettServerGap_GetItemDataNameSize(void)
{
    uint16 name_length = 0;
    LocalName_GetPrefixedName(&name_length);
    return (unsigned)(name_length + AD_DATA_HEADER_SIZE);
}

static bool gattServerGap_GetItemDataName(le_adv_item_data_t * item)
{
    PanicNull(item);
    DEBUG_LOG("gattServerGap_GetAdvertisingItemName gap_name_item.data:%d", gap_name_item.data);

    if(gap_name_item.data)
    {
        uint16 name_len;
        const char* name = (const char*)LocalName_GetPrefixedName(&name_len);
        if(memcmp(&gap_name_item.data[AD_DATA_HEADER_SIZE], name, name_len))
        {
            free((void *)gap_name_item.data);
            gap_name_item.data = NULL;
        }
    }
    if(gap_name_item.data == NULL)
    {
        uint16 name_len;
        const char* name = (const char*)LocalName_GetPrefixedName(&name_len);
        PanicNull((void*)name);

        uint16 data_len = gettServerGap_GetItemDataNameSize();
        uint8* data = PanicUnlessMalloc(data_len);

        data[AD_DATA_LENGTH_OFFSET] = name_len + 1;
        data[AD_DATA_TYPE_OFFSET] = ble_ad_type_complete_local_name;
        memcpy(&data[AD_DATA_HEADER_SIZE], name, name_len);

        gap_name_item.size = data_len;
        gap_name_item.data = data;
    }
    *item = gap_name_item;
    return TRUE;
}

static void gattServerGap_ReleaseItemDataName(void)
{
    if(gap_name_item.data && !name_legacy_in_use && !name_extended_in_use)
    {
        free((void*)gap_name_item.data);
        gap_name_item.data = NULL;
        gap_name_item.size = 0;
    }
}

static bool gattServerGap_GetItemDataFlags(le_adv_item_data_t * data)
{
    PanicNull(data);
    *data = gap_flags_item;
    return TRUE;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static le_adv_mgr_register_handle gatt_server_gap_registered_item_name_handle = NULL;
static le_adv_mgr_register_handle gatt_server_gap_registered_item_name_extended_handle = NULL;
static le_adv_mgr_register_handle gatt_server_gap_registered_item_flags_handle = NULL;

static bool gattServerGap_GetItemInfoFlags(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                     .data_size = gap_flags_item.size,
                                    .include_if_connectable = TRUE };
    return TRUE;
}

le_adv_item_callback_t gattServerGap_AdvertisingManagerItemFlagsCallback =
{
    .GetItemData = &gattServerGap_GetItemDataFlags,
    .GetItemInfo = &gattServerGap_GetItemInfoFlags
};

static bool gattServerGap_GetItemDataNameLegacyAdvertising(le_adv_item_data_t * item)
{
    name_legacy_in_use = TRUE;
    return gattServerGap_GetItemDataName(item);
}

static void gattServerGap_ReleaseItemDataNameLegacyAdvertising(void)
{
    name_legacy_in_use = FALSE;
    gattServerGap_ReleaseItemDataName();
}

static bool gattServerGap_GetItemInfoNameLegacyAdvertising(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = gettServerGap_GetItemDataNameSize() };
    return TRUE;
}

le_adv_item_callback_t gattServerGap_AdvertisingManagerItemNameCallback =
{
    .GetItemData = &gattServerGap_GetItemDataNameLegacyAdvertising,
    .ReleaseItemData = &gattServerGap_ReleaseItemDataNameLegacyAdvertising,
    .GetItemInfo = &gattServerGap_GetItemInfoNameLegacyAdvertising
};

static bool gattServerGap_GetItemDataNameExtendedAdvertising(le_adv_item_data_t * item)
{
    name_extended_in_use = TRUE;
    return gattServerGap_GetItemDataName(item);
}

static void gattServerGap_ReleaseItemDataNameExtendedAdvertising(void)
{
    name_extended_in_use = FALSE;
    gattServerGap_ReleaseItemDataName();
}

static bool gattServerGap_GetItemInfoNameExtendedAdvertising(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_extended_connectable,
                                    .data_size = gettServerGap_GetItemDataNameSize() };
    return TRUE;
}

le_adv_item_callback_t gattServerGap_AdvertisingManagerItemNameExtendedAdvertisingCallback =
{
    .GetItemData = &gattServerGap_GetItemDataNameExtendedAdvertising,
    .ReleaseItemData = &gattServerGap_ReleaseItemDataNameExtendedAdvertising,
    .GetItemInfo = &gattServerGap_GetItemInfoNameExtendedAdvertising
};

#else

static le_adv_mgr_register_handle gatt_server_gap_registered_handle = NULL;

static bool gattServerGap_IsNameReturned(const le_adv_data_params_t * params)
{
    if((params->data_set != le_adv_data_set_handset_unidentifiable) &&
       (params->data_set != le_adv_data_set_extended_handset))
    {
        return FALSE;
    }

    if(params->placement != le_adv_data_placement_dont_care)
    {
        return FALSE;
    }

    if(GattServerGap_IsCompleteLocalNameBeingUsed())
    {
        if(params->completeness != le_adv_data_completeness_full)
        {
            return FALSE;
        }
    }
    else
    {
        if(params->completeness != le_adv_data_completeness_can_be_shortened)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static bool gattServerGap_IsFlagsReturned(const le_adv_data_params_t * params)
{
    if(params->data_set == le_adv_data_set_peer)
    {
        return FALSE;
    }

    if(params->placement != le_adv_data_placement_advert)
    {
        return FALSE;
    }

    if(params->completeness != le_adv_data_completeness_full)
    {
        return FALSE;
    }

    return TRUE;
}

static unsigned gattServerGap_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    unsigned count = 0;

    if(gattServerGap_IsNameReturned(params))
    {
        count++;
    }

    if(gattServerGap_IsFlagsReturned(params))
    {
        count++;
    }

    return count;
}

static le_adv_data_item_t gattServerGap_GetAdvData(const le_adv_data_params_t * params, unsigned index)
{
    PanicFalse(index == 0);

    DEBUG_LOG("gattServerGap_GetAdvData data_set: %d, completeness: %d,  placement:%d",
               params->data_set, params->completeness, params->placement);

    le_adv_data_item_t item = { .size = 0, .data = NULL };
    if(gattServerGap_IsNameReturned(params))
    {
        gattServerGap_GetItemDataName(&item);
        return item;
    }

    if(gattServerGap_IsFlagsReturned(params))
    {
        gattServerGap_GetItemDataFlags(&item);
        return item;
    }

    Panic();
    return item;
}

static void gattServerGap_ReleaseAdvData(const le_adv_data_params_t * params)
{
    DEBUG_LOG("gattServerGap_ReleaseAdvData data_set: %d, completeness: %d,  placement:%d",
               params->data_set, params->completeness, params->placement);

    if(gattServerGap_IsNameReturned(params))
    {
        gattServerGap_ReleaseItemDataName();
    }
}

static const le_adv_data_callback_t gattServerGap_AdvertisingManagerCallback =
{
    .GetNumberOfItems = gattServerGap_NumberOfAdvItems,
    .GetItem = gattServerGap_GetAdvData,
    .ReleaseItems = gattServerGap_ReleaseAdvData
};

#endif

bool GattServerGap_SetupLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    gatt_server_gap_registered_item_name_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &gattServerGap_AdvertisingManagerItemNameCallback);
    gatt_server_gap_registered_item_name_extended_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &gattServerGap_AdvertisingManagerItemNameExtendedAdvertisingCallback);
    gatt_server_gap_registered_item_flags_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &gattServerGap_AdvertisingManagerItemFlagsCallback);
    return ((gatt_server_gap_registered_item_name_handle
            && gatt_server_gap_registered_item_name_extended_handle
            && gatt_server_gap_registered_item_flags_handle)  ? TRUE : FALSE);
#else
    gatt_server_gap_registered_handle = LeAdvertisingManager_Register(NULL, &gattServerGap_AdvertisingManagerCallback);
    return (gatt_server_gap_registered_handle ? TRUE : FALSE);
#endif
}

bool GattServerGap_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return ((gatt_server_gap_registered_item_name_handle ? LeAdvertisingManager_UpdateAdvertisingItem(gatt_server_gap_registered_item_name_handle) : FALSE)
            && (gatt_server_gap_registered_item_name_extended_handle ? LeAdvertisingManager_UpdateAdvertisingItem(gatt_server_gap_registered_item_name_extended_handle) : FALSE)
            && (gatt_server_gap_registered_item_flags_handle ? LeAdvertisingManager_UpdateAdvertisingItem(gatt_server_gap_registered_item_flags_handle) : FALSE));
#else
    return (gatt_server_gap_registered_handle ? LeAdvertisingManager_NotifyDataChange(NULL, gatt_server_gap_registered_handle) : FALSE);
#endif
}
