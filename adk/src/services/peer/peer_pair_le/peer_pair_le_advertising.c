/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "peer_pair_le_advertising.h"

#include "le_advertising_manager.h"
#include "peer_pair_le_private.h"
#include <gatt_root_key_server_uuids.h>
#include <uuid.h>
#include <panic.h>

#define SIZE_PEER_PAIR_LE_ADVERT        18

/*! In legacy use of peer pair LE, we do not know (or care) if we are a left
    or right device. That uses a different UUID */
static const uint8 peer_pair_le_advert_data_common[SIZE_PEER_PAIR_LE_ADVERT] = {
    SIZE_PEER_PAIR_LE_ADVERT - 1,
    ble_ad_type_complete_uuid128,
    UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE)
};

static const uint8 peer_pair_le_advert_data_left[SIZE_PEER_PAIR_LE_ADVERT] = {
    SIZE_PEER_PAIR_LE_ADVERT - 1,
    ble_ad_type_complete_uuid128,
    UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE_LEFT)
};

static const uint8 peer_pair_le_advert_data_right[SIZE_PEER_PAIR_LE_ADVERT] = {
    SIZE_PEER_PAIR_LE_ADVERT - 1,
    ble_ad_type_complete_uuid128,
    UUID_128_FORMAT_uint8(UUID128_ROOT_KEY_SERVICE_RIGHT)
};


static le_adv_mgr_register_handle peer_pair_le_registered_handle = NULL;
static le_adv_item_data_t peer_pair_le_advert;

static inline bool peerPairLe_CanAdvertise(void)
{
    return peer_pair_le_is_in_advertising_state();
}

static bool peerPairLe_GetItemData(le_adv_item_data_t * item)
{
    PanicNull(item);
    bool can_advertise = FALSE;
    item->data = NULL;
    item->size = 0;
    if(peerPairLe_CanAdvertise())
    {
        *item = peer_pair_le_advert;
        can_advertise = TRUE;
    }
    return can_advertise;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static unsigned peerPairLe_GetItemDataSize(void)
{
    unsigned data_size = 0;
    if(peerPairLe_CanAdvertise())
    {
        data_size = SIZE_PEER_PAIR_LE_ADVERT;
    }
    return data_size;
}

static bool peerPairLe_GetItemInfo(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t) { .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = peerPairLe_GetItemDataSize(),
                                    .override_connectable_state = TRUE };
    return TRUE;
}

le_adv_item_callback_t peerPairLe_AdvertisingManagerCallback =
{
    .GetItemData = &peerPairLe_GetItemData,
    .GetItemInfo = &peerPairLe_GetItemInfo
};


#else

#define NUMBER_OF_ADVERT_DATA_ITEMS     1

static bool peerPairLe_CanAdvertiseWithTheseParams(const le_adv_data_params_t * params)
{
    bool can_advertise = FALSE;
    if((le_adv_data_set_peer == params->data_set) && \
           (le_adv_data_completeness_full == params->completeness) && \
           (le_adv_data_placement_advert == params->placement))
    {
        can_advertise = TRUE;
    }
    return can_advertise;
}

/* Return the number of items in the advert.
   For simplicity/safety don't make the same check when getting data items.
 */
static unsigned int peer_pair_le_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    unsigned int items = 0;

    if (peerPairLe_CanAdvertise() && peerPairLe_CanAdvertiseWithTheseParams(params))
    {
        items = NUMBER_OF_ADVERT_DATA_ITEMS;
    }

    return items;
}

static le_adv_data_item_t peer_pair_le_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(id);
    le_adv_data_item_t item = { .size = 0, .data = NULL };
    if(peerPairLe_CanAdvertiseWithTheseParams(params))
    {
        peerPairLe_GetItemData(&item);
        return item;
    }
    else
    {
        Panic();
        return item;
    };
}

static void peer_pair_le_ReleaseAdvDataItems(const le_adv_data_params_t * params)
{
    UNUSED(params);
    return;
}

static const le_adv_data_callback_t peerPairLe_AdvertisingManagerCallback =
{
    .GetNumberOfItems = peer_pair_le_NumberOfAdvItems,
    .GetItem = peer_pair_le_GetAdvDataItems,
    .ReleaseItems = peer_pair_le_ReleaseAdvDataItems
};

#endif

static void peerPairLe_PopulateAdvertisingData(void)
{
    peer_pair_le_advert.size = SIZE_PEER_PAIR_LE_ADVERT;

    if (PeerPairLeIsLeft())
    {
        peer_pair_le_advert.data = peer_pair_le_advert_data_left;
    }
    else if (PeerPairLeIsRight())
    {
        peer_pair_le_advert.data = peer_pair_le_advert_data_right;
    }
    else
    {
        peer_pair_le_advert.data = peer_pair_le_advert_data_common;
    }
}

bool PeerPairLe_SetupLeAdvertisingData(void)
{
    peerPairLe_PopulateAdvertisingData();
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    peer_pair_le_registered_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &peerPairLe_AdvertisingManagerCallback);
#else
    peer_pair_le_registered_handle = LeAdvertisingManager_Register(NULL, &peerPairLe_AdvertisingManagerCallback);
#endif
    return (peer_pair_le_registered_handle ? TRUE : FALSE);
}

bool PeerPairLe_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return (peer_pair_le_registered_handle ? LeAdvertisingManager_UpdateAdvertisingItem(peer_pair_le_registered_handle) : FALSE);
#else
    return (peer_pair_le_registered_handle ? LeAdvertisingManager_NotifyDataChange(NULL, peer_pair_le_registered_handle) : FALSE);
#endif
}

