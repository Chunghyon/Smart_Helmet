/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functionality to include GAIA in LE advertising data
*/

#include "gaia_framework_advertising.h"
#include "le_advertising_manager.h"

#include <logging.h>
#include <panic.h>

le_adv_mgr_register_handle gaia_registered_handle = NULL;

#define GAIA_SERVICE_NUMBER_ADVERT_DATA_ITEMS   1
#define GAIA_SERVICE_ADVERT_SIZE                18

static const uint8 gaia_advertising_data[GAIA_SERVICE_ADVERT_SIZE] = {
    GAIA_SERVICE_ADVERT_SIZE - 1,
    ble_ad_type_service_128bit_uuid,

    /*
     * It is strongly recommended that the following GAIA UUIDs are changed to be manufacturer
     * specific to prevent potential unintended operation between different products
     * available in the market.
     */
    /* GAIA service UUID: 0x1100D10211E19B2300025B00A5A5 */
    0xA5,
    0xA5,
    0x00,
    0x5B,
    0x02,
    0x00,
    0x23,
    0x9B,
    0xE1,
    0x11,
    0x02,
    0xD1,
    0x00,
    0x11,
    0x00,
    0x00,
};

static const le_adv_item_data_t gaia_advert = { .data = gaia_advertising_data, .size = GAIA_SERVICE_ADVERT_SIZE };

static inline bool gaiaFrameworkAdvertising_GetItemData(le_adv_item_data_t * item)
{
    PanicNull(item);
    *item = gaia_advert;
    return TRUE;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static bool gaiaFrameworkAdvertising_GetItemInfo(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_extended_connectable,
                                    .data_size = GAIA_SERVICE_ADVERT_SIZE };
    return TRUE;
}

le_adv_item_callback_t gaiaFrameworkAdvertising_AdvertisingManagerCallback =
{
    .GetItemData = &gaiaFrameworkAdvertising_GetItemData,
    .GetItemInfo = &gaiaFrameworkAdvertising_GetItemInfo
};


#else

static bool gaiaFrameworkAdvertising_CanAdvertise(const le_adv_data_params_t * params)
{
    bool can_advertise = FALSE;
    if(params->data_set == le_adv_data_set_extended_handset
            && params->completeness == le_adv_data_completeness_full
            && params->placement == le_adv_data_placement_advert)
    {
        can_advertise = TRUE;
    }
    return can_advertise;
}

static unsigned int gaiaFrameworkAdvertising_GetNumberOfAdvertisingItems(const le_adv_data_params_t * params)
{
    if(gaiaFrameworkAdvertising_CanAdvertise(params))
    {
        return GAIA_SERVICE_NUMBER_ADVERT_DATA_ITEMS;
    }
    return 0;
}

static le_adv_data_item_t gaiaFrameworkAdvertising_GetAdvertisingData(const le_adv_data_params_t * params, unsigned int index)
{
    UNUSED(index);
    le_adv_data_item_t item = { .size = 0, .data = NULL };
    if(gaiaFrameworkAdvertising_CanAdvertise(params))
    {
        gaiaFrameworkAdvertising_GetItemData(&item);
        return item;
    }

    Panic();
    return item;
}

static void gaiaFrameworkAdvertising_ReleaseAdvertisingData(const le_adv_data_params_t * params)
{
    UNUSED(params);
    return;
}

le_adv_data_callback_t gaiaFrameworkAdvertising_AdvertisingManagerCallback =
{
    .GetNumberOfItems = &gaiaFrameworkAdvertising_GetNumberOfAdvertisingItems,
    .GetItem = &gaiaFrameworkAdvertising_GetAdvertisingData,
    .ReleaseItems = &gaiaFrameworkAdvertising_ReleaseAdvertisingData
};

#endif

bool GaiaFramework_SetupLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    gaia_registered_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &gaiaFrameworkAdvertising_AdvertisingManagerCallback);
#else
    gaia_registered_handle = LeAdvertisingManager_Register(NULL, &gaiaFrameworkAdvertising_AdvertisingManagerCallback);
#endif
    return (gaia_registered_handle ? TRUE : FALSE);
}


bool GaiaFramework_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return (gaia_registered_handle ? LeAdvertisingManager_UpdateAdvertisingItem(gaia_registered_handle) : FALSE);
#else
    return (gaia_registered_handle ? LeAdvertisingManager_NotifyDataChange(NULL, gaia_registered_handle) : FALSE);
#endif
}
