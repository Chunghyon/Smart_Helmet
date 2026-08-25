/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "tx_power_advertising.h"

#include "le_advertising_manager.h"
#include "tx_power.h"
#include <logging.h>

#define TX_POWER_ADV_SIZE    (3)

static le_adv_mgr_register_handle tx_power_registered_handle = NULL;
static le_adv_item_data_t tx_power_advert;
static uint8 tx_power_adv_data[TX_POWER_ADV_SIZE];

static bool txPower_GetItemData(le_adv_item_data_t * item)
{
    DEBUG_LOG("txPower_GetAdvertisingDataItem tx_power_advert.data:%d", tx_power_advert.data);

    tx_power_advert.size = TX_POWER_ADV_SIZE;

    /* Set the data field in the format of advertising packet format. Adhering to le_advertising_mgr */
    tx_power_adv_data[0] = TX_POWER_ADV_SIZE - 1 ;
    tx_power_adv_data[1] = (uint8)ble_ad_type_tx_power_level;
    tx_power_adv_data[2] = TxPower_LEGetData();
    tx_power_advert.data = tx_power_adv_data;

    *item = tx_power_advert;
    return TRUE;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static bool txPower_GetItemInfo(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_scan_response,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = TX_POWER_ADV_SIZE };
    return TRUE;
}

le_adv_item_callback_t txPower_AdvertisingManagerCallback =
{
    .GetItemData = &txPower_GetItemData,
    .GetItemInfo = &txPower_GetItemInfo
};

#else

#define TX_POWER_NUM_ITEMS   (1)

static bool txPower_CanAdvertise(const le_adv_data_params_t * params)
{
    /* Can advertise under these conditions
       1. TxPower is Mandatory and completeness_full.
       2. TxPower is Optional and completeness_skip
    */
    bool can_advertise = FALSE;
    if(((params->completeness == le_adv_data_completeness_full && TxPower_GetMandatoryStatus()) ||
           (params->completeness == le_adv_data_completeness_can_be_skipped && !TxPower_GetMandatoryStatus())) &&
           (params->placement == le_adv_data_placement_dont_care) &&
           (params->data_set != le_adv_data_set_peer))
    {
        can_advertise = TRUE;
    }
    return can_advertise;
}

static unsigned int txPower_AdvGetNumberOfItems(const le_adv_data_params_t * params)
{
    DEBUG_LOG_V_VERBOSE("txPower_AdvGetNumberOfItems: Completeness=%d", params->completeness);
    if(txPower_CanAdvertise(params))
    {
        return TX_POWER_NUM_ITEMS;
    }
    else
    {
        return 0;
    }
}

static le_adv_data_item_t txPower_AdvertData(const le_adv_data_params_t * params, unsigned int number)
{
    le_adv_data_item_t adv_data_item={0};

    DEBUG_LOG_V_VERBOSE("txPower_AdvertData: Completeness=%d, number=%d", params->completeness, number);

    if(txPower_CanAdvertise(params))
    {
        txPower_GetItemData(&adv_data_item);
    }
    else
    {
        adv_data_item.size = 0;
        adv_data_item.data = NULL;
    }

    return adv_data_item;
}

static void txPower_ReleaseItems(const le_adv_data_params_t * params)
{
    UNUSED(params);
    if(tx_power_advert.data)
    {
        tx_power_advert.size = 0;
        tx_power_advert.data = NULL;
    };
}

le_adv_data_callback_t txPower_AdvertisingManagerCallback =
{
    .GetNumberOfItems = &txPower_AdvGetNumberOfItems,
    .GetItem = &txPower_AdvertData,
    .ReleaseItems = &txPower_ReleaseItems
};

#endif

bool TxPower_SetupLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    tx_power_registered_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &txPower_AdvertisingManagerCallback);
#else
    tx_power_registered_handle = LeAdvertisingManager_Register(NULL, &txPower_AdvertisingManagerCallback);
#endif
    return (tx_power_registered_handle ? TRUE : FALSE);
}

bool TxPower_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return (tx_power_registered_handle ? LeAdvertisingManager_UpdateAdvertisingItem(tx_power_registered_handle) : FALSE);
#else
    return (tx_power_registered_handle ? LeAdvertisingManager_NotifyDataChange(NULL, tx_power_registered_handle) : FALSE);
#endif
}
