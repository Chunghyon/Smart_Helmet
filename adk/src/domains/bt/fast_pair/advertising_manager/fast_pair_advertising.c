/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file        fast_pair_advertising.c
\brief      Handles Fast Pair Advertising Data
*/

/*! Firmware and Library Headers */
#include <util.h>
#include <stdlib.h>
#include <message.h>
#include <logging.h>
#include <panic.h>
#include <stdio.h>
#include <connection_manager.h>
#include <connection.h>
#include <string.h>
#include <cryptovm.h>

/*! Application Headers */
#include "le_advertising_manager.h"
#include "fast_pair_advertising.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair.h"
#include "fast_pair_config.h"
#include "fast_pair_session_data.h"
#include "tx_power.h"
#include "fast_pair_battery_notifications.h"
#include "user_accounts.h"
#include "fast_pair_adv_sass.h"

/*! \brief Global data structure for fastpair adverts */
typedef struct
{
    le_adv_mgr_register_handle adv_register_handle;
    uint8   *account_key_filter_adv_data;
    bool    identifiable;
    bool    in_use_account_key_active;
}fastpair_advert_data_t;

/*! Global Instance of fastpair advertising data */
fastpair_advert_data_t fastpair_advert;

/*! Module Constants */
#define FAST_PAIR_GFPS_IDENTIFIER 0xFE2C

#define FAST_PAIR_ADV_ITEM_MODEL_ID 0 /*During BR/EDR Connectable and Discoverable*/
#define FAST_PAIR_ADV_ITEM_BLOOM_FILTER_ID 0 /*During BR/EDR Connectable and Non-Discoverable*/

/*GFPS Identifier, Model ID in Identifiable mode and GFPS Identifier, Hashed Account key (Bloom Filter) including Salt in Unidentifiable mode*/
/*Note transmit power needed for fastpair adverts will go as part of Tx_power module*/
#define FAST_PAIR_AD_ITEMS_IDENTIFIABLE 1
#define FAST_PAIR_AD_ITEMS_UNIDENTIFIABLE_WITH_ACCOUNT_KEYS 1

/*All Adv Interval values are in ms (units of 0.625)*/
/* Adv interval when BR/EDR is discoverable should be <=100ms*/
#define FP_ADV_INTERVAL_IDENTIFIABLE_MIN     128
#define FP_ADV_INTERVAL_IDENTIFIABLE_MAX     160

/*FP when BR/EDR non-discoverable/Silent Pairing*/
#define FP_ADV_INTERVAL_UNIDENTIFIABLE_MIN   320
#define FP_ADV_INTERVAL_UNIDENTIFIABLE_MAX   400

/*! Const fastpair advertising data in Length, Tag, Value format*/
#define FP_SIZE_AD_TYPE_FIELD 1 
#define FP_SIZE_LENGTH_FIELD 1

/* Flags in Advertising Payload when in non-discoverable mode(All bits are reserved for future use)*/
#define FP_ADV_PAYLOAD_FLAGS 0x00
/* Account Key data when in non-discoverable mode and no acocunt keys are present*/
#define FP_ADV_PAYLOAD_ACCOUNT_KEY_DATA 0x00

#define SIZE_GFPS_ID 2
/* Length of account key data when non-discoverable and no account keys are present*/
#define ACCOUNT_DATA_LEN 2
#define SIZE_GFPS_ID_ADV (FP_SIZE_LENGTH_FIELD+FP_SIZE_AD_TYPE_FIELD+SIZE_GFPS_ID)
/* Size of adverising data when non-discoverable and no account keys are present*/
#define SIZE_ACCOUNT_DATA_ADV (SIZE_GFPS_ID_ADV+ACCOUNT_DATA_LEN)

static bool IsHandsetConnAllowed = FALSE;

static const uint8 fp_account_data_adv[SIZE_ACCOUNT_DATA_ADV] = 
{ 
    SIZE_ACCOUNT_DATA_ADV - 1,
    ble_ad_type_service_data, 
    FAST_PAIR_GFPS_IDENTIFIER & 0xFF, 
    (FAST_PAIR_GFPS_IDENTIFIER >> 8) & 0xFF,
    FP_ADV_PAYLOAD_FLAGS,
    FP_ADV_PAYLOAD_ACCOUNT_KEY_DATA
};

static const le_adv_item_data_t fp_account_data_data_item =
{
    SIZE_ACCOUNT_DATA_ADV,
    fp_account_data_adv
};

/*! Const fastpair advertising data in Length, Tag, Value format*/
#define SIZE_MODEL_ID 3
#define SIZE_MODEL_ID_ADV (FP_SIZE_LENGTH_FIELD+FP_SIZE_AD_TYPE_FIELD+SIZE_MODEL_ID + SIZE_GFPS_ID)

static le_adv_item_data_t fp_model_id_data_item;
static uint8 fp_model_id_adv[SIZE_MODEL_ID_ADV];


/*! \brief Provide fastpair advert data when Identifiable (i.e. BR/EDR discoverable)

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.
*/
static inline le_adv_item_data_t fastPair_GetDataIdentifiable(void)
{
    DEBUG_LOG("FP ADV: fastPair_GetDataIdentifiable: Model Id data item \n");
    return fp_model_id_data_item;
}

/*! \brief Get the advert data for account key filter
*/
static le_adv_item_data_t fastPairGetAccountKeyFilterAdvData(bool compute_size_only)
{
    le_adv_item_data_t data_item;
    uint16 bloom_filter_size = fastPairGetBloomFilterLen();
    uint16 adv_size=0;

    if (fastpair_advert.account_key_filter_adv_data)
    {
        free(fastpair_advert.account_key_filter_adv_data);
        fastpair_advert.account_key_filter_adv_data=NULL;
    }

    if (bloom_filter_size)
    {
        /* calculate total size of advert data
            - service type and length
            - bloom filter
            - optionally battery notifications (may be 0)
            - mandatory random resovable data (encrypted connection status field for SASS)
         */
        adv_size = SIZE_GFPS_ID_ADV + bloom_filter_size + FP_BATTERY_NOTFICATION_SIZE + SASS_RANDOM_RESOLVABLE_DATA_SIZE;

        if(!compute_size_only)
        {
            fastpair_advert.account_key_filter_adv_data = PanicUnlessMalloc(adv_size);
            fastpair_advert.account_key_filter_adv_data[0] = adv_size-FP_SIZE_LENGTH_FIELD;
            fastpair_advert.account_key_filter_adv_data[1] = ble_ad_type_service_data;
            fastpair_advert.account_key_filter_adv_data[2] = (uint8)(FAST_PAIR_GFPS_IDENTIFIER & 0xFF);
            fastpair_advert.account_key_filter_adv_data[3] = (uint8)((FAST_PAIR_GFPS_IDENTIFIER >> 8) & 0xFF);
            memcpy(&fastpair_advert.account_key_filter_adv_data[4], fastPairGetBloomFilterData(), bloom_filter_size);
            /* add optional battery state data if available */
            if (FP_BATTERY_NOTFICATION_SIZE)
            {
                memcpy(&fastpair_advert.account_key_filter_adv_data[bloom_filter_size+4], fastPair_BatteryGetData(), FP_BATTERY_NOTFICATION_SIZE);
            }
            if(fastPair_SASSGetAdvDataSize())
            {
                /* Add mandatory Random Resolvable Data for SASS feature */
                memcpy(&fastpair_advert.account_key_filter_adv_data[bloom_filter_size+4+FP_BATTERY_NOTFICATION_SIZE], fastPair_SASSGetAdvData(), SASS_RANDOM_RESOLVABLE_DATA_SIZE);
            }
        }
        DEBUG_LOG("FP ADV: fastPairGetAccountKeyFilterAdvData: bloom_filter_size %d adv_size %d\n", bloom_filter_size, adv_size);
    }

    data_item.size = adv_size;
    data_item.data = fastpair_advert.account_key_filter_adv_data;

    return data_item;
}

/*! \brief Provide fastpair advert data when Unidentifiable (i.e. BR/EDR non-discoverable)

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.

    \param  compute_size_only TRUE means the caller is only interested in the size of data, so avoid any
            allocating memory for actual data
*/
static le_adv_item_data_t fastPair_GetDataUnIdentifiable(bool compute_size_only)
{
    le_adv_item_data_t data_item={0};

    if(!UserAccounts_GetNumAccountKeys())
    {
        DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: GFPS Id data item with empty Account Key\n");
        data_item = fp_account_data_data_item;
    }
    else
    {
        fastPairTaskData *theFastPair = fastPair_GetTaskData();
        DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: GFPS Id data item with Account Key\n");
        data_item = fastPairGetAccountKeyFilterAdvData(compute_size_only);
        fastpair_advert.in_use_account_key_active = FastPair_SASSGetInUseAccountKeyActiveFlag();
        /*Generate new bloom filter and keep it ready for advertisements in BR/EDR Connectable and non-discoverable mode
        This will ensure next callback would have new Salt*/
        DEBUG_LOG("FP ADV: fastPair_GetDataUnIdentifiable: fastPair_GenerateBloomFilter\n");
        /* Generating Bloom filter only In Idle state(fresh pair) and Wait Account Key state(Subsequent pair)*/
        if((fastPair_GetState(theFastPair) == FAST_PAIR_STATE_IDLE) || (fastPair_GetState(theFastPair) == FAST_PAIR_STATE_WAIT_ACCOUNT_KEY))
        {
            fastPair_GenerateBloomFilter();
        }
    }
    return data_item;
}

static inline void fastPair_ReleaseAdvertisingDataItem(void)
{
    if (fastpair_advert.account_key_filter_adv_data)
    {
        free(fastpair_advert.account_key_filter_adv_data);
        fastpair_advert.account_key_filter_adv_data=NULL;
    }
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static inline bool fastPair_ShouldAdvertiseIdentifiable(void)
{
    return fastpair_advert.identifiable;
}

static unsigned fastPair_GetItemDataSize(void)
{
    return fastPair_ShouldAdvertiseIdentifiable()
            ? fastPair_GetDataIdentifiable().size
                : fastPair_GetDataUnIdentifiable(TRUE).size;
}

static bool fastPair_GetItemData(le_adv_item_data_t * data)
{
    PanicNull(data);
    data->data = NULL;
    data->size = 0;

    *data = fastPair_ShouldAdvertiseIdentifiable()
            ? fastPair_GetDataIdentifiable()
                : fastPair_GetDataUnIdentifiable(FALSE);
    return TRUE;
}

static void fastPair_ReleaseItemData(void)
{
    fastPair_ReleaseAdvertisingDataItem();
}

static bool fastPair_GetItemInfo(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                        .type = le_adv_type_legacy_connectable_scannable,
                                        .data_size = fastPair_GetItemDataSize()};
    return TRUE;
}

static const le_adv_item_callback_t fastPair_advertising_callback = {
    .GetItemData = &fastPair_GetItemData,
    .ReleaseItemData = &fastPair_ReleaseItemData,
    .GetItemInfo = &fastPair_GetItemInfo
};

#else

/*! \brief Query the advertisement interval and check if it in expected range*/
static void fastpair_CheckAdvIntervalInRange(le_adv_data_set_t data_set)
{
    le_adv_common_parameters_t adv_int = {0};

    /* Get the currently used advertising parameters */
    bool status = LeAdvertisingManager_GetAdvertisingInterval(&adv_int);
    DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange adv_int_min = %d\n, adv_int_max = %d\n", adv_int.le_adv_interval_min, adv_int.le_adv_interval_max);

    if(status)
    {
        switch(data_set)
        {
            case le_adv_data_set_handset_identifiable:
            /*  This is more stringent check as the max value returned might be above 100ms but actual adv interval
                might still be conforming to FASTPAIR discoverable requirement*/
                if(adv_int.le_adv_interval_max > FP_ADV_INTERVAL_IDENTIFIABLE_MAX)
                {
                    DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Adv interval might be non compliant with Fastpair Standard \n");
                }
            break;

            case le_adv_data_set_handset_unidentifiable:
            /*  This is more stringent check as the max value returned might be above 250ms but actual adv interval
                might still be conforming to FASTPAIR non-discoverable requirement*/
                if(adv_int.le_adv_interval_max > FP_ADV_INTERVAL_UNIDENTIFIABLE_MAX)
                {
                    DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Adv interval might be non compliant with Fastpair Standard \n");
                }
            break;

            case le_adv_data_set_peer:
                DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Non-connectable \n");
            break;

            default:
                DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Invalid advertisement dataset\n");
            break;
        }
    }
    else
    {
        DEBUG_LOG("FP ADV: fastpair_CheckAdvIntervalInRange: Failed to get the LE advertising interval values \n");
    }
}


#define FASTPAIR_ADV_PARAMS_REQUESTED(params) ((params->completeness == le_adv_data_completeness_full) && \
            (params->placement == le_adv_data_placement_advert))

#define IS_IDENTIFIABLE(data_set) (data_set == le_adv_data_set_handset_identifiable)
#define IS_UNIDENTIFIABLE(data_set) (data_set == le_adv_data_set_handset_unidentifiable)


/*! \brief Provide the number of items expected to go in adverts for a given mode

      Advertising Manager is expected to retrive the number of items first before the fastPair_AdvGetDataItem() callback

      For fastpair there wont be any adverts in case of le_adv_data_completeness_can_be_shortened/skipped
*/
static unsigned int fastPair_AdvGetNumberOfItems(const le_adv_data_params_t * params)
{
    unsigned int number=0;

    if(params->data_set != le_adv_data_set_peer)
    {
        bool identifiable = (params->data_set == le_adv_data_set_handset_identifiable ? TRUE : FALSE);
        fastpair_advert.identifiable = identifiable;
        /* Add debug logs if existing advertising interval is not in range */
        fastpair_CheckAdvIntervalInRange(params->data_set);

        /*Check for BR/EDR connectable*/
        if (IsHandsetConnAllowed && FASTPAIR_ADV_PARAMS_REQUESTED(params))
        {
            if (IS_IDENTIFIABLE(params->data_set))
            {
                number = FAST_PAIR_AD_ITEMS_IDENTIFIABLE;
            }
            else if (IS_UNIDENTIFIABLE(params->data_set))
            {
                number = FAST_PAIR_AD_ITEMS_UNIDENTIFIABLE_WITH_ACCOUNT_KEYS;
            }
        }
        else
        {
            DEBUG_LOG("FP ADV: fastPair_AdvGetNumberOfItems: Non-connectable \n");
        }
    }

    return number;
}

/*! \brief Provide the advertisement data expected to go in adverts for a given mode

    Each data item in GetItems will be invoked separately by Adv Mgr, more precisely, one item per AD type.
*/
static le_adv_data_item_t fastPair_AdvGetDataItem(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(id);
    le_adv_data_item_t data_item={0};

    if(params->data_set != le_adv_data_set_peer)
    {
        if (IsHandsetConnAllowed && FASTPAIR_ADV_PARAMS_REQUESTED(params))
        {
            if (IS_IDENTIFIABLE(params->data_set))
            {
                return fastPair_GetDataIdentifiable();
            }
            else if (IS_UNIDENTIFIABLE(params->data_set))
            {
                return fastPair_GetDataUnIdentifiable(FALSE);
            }
        }
    }

    return data_item;
}


/*! \brief Release any allocated fastpair data

      Advertising Manager is expected to retrive the number of items first before the fastPair_AdvGetDatatems() callback
*/
static void fastPair_ReleaseItems(const le_adv_data_params_t * params)
{
    if (FASTPAIR_ADV_PARAMS_REQUESTED(params) && IS_UNIDENTIFIABLE(params->data_set))
    {        
        fastPair_ReleaseAdvertisingDataItem();
    }
}

/*! Callback registered with LE Advertising Manager*/
static const le_adv_data_callback_t fastPair_advertising_callback = {
    .GetNumberOfItems = &fastPair_AdvGetNumberOfItems,
    .GetItem = &fastPair_AdvGetDataItem,
    .ReleaseItems = &fastPair_ReleaseItems
};

#endif

/*! \brief Function to initialise the fastpair advertising globals
*/
static void fastPair_InitialiseAdvGlobal(void)
{
    memset(&fastpair_advert, 0, sizeof(fastpair_advert_data_t));
    fastpair_advert.in_use_account_key_active = FALSE;
}

static void fastPair_ModelIdAdvData(void)
{
    uint32 fp_model_id;

    fp_model_id = fastPair_GetModelId();
    DEBUG_LOG("fastPair_ModelIdAdvData %04x", fp_model_id);

    fp_model_id_adv[0] = SIZE_MODEL_ID_ADV - 1;
    fp_model_id_adv[1] = ble_ad_type_service_data;
    fp_model_id_adv[2] = FAST_PAIR_GFPS_IDENTIFIER & 0xFF;
    fp_model_id_adv[3] = (FAST_PAIR_GFPS_IDENTIFIER >> 8) & 0xFF;
    fp_model_id_adv[4] = (fp_model_id >> 16) & 0xFF;
    fp_model_id_adv[5] = (fp_model_id >> 8) & 0xFF;
    fp_model_id_adv[6] = fp_model_id & 0xFF;

    fp_model_id_data_item.size = SIZE_MODEL_ID_ADV;
    fp_model_id_data_item.data = fp_model_id_adv;
}

/*! @brief Private API to initialise fastpair
 */
bool fastPair_SetUpAdvertising(void)
{
    /*Initialise fastpair advertising globals*/
    fastPair_InitialiseAdvGlobal();

    /* Initialise Fast Pair SASS advertising module */
    fastPair_SetUpSASSAdvertising();

    /* Setup Fast Pair Model ID advertising data */
    fastPair_ModelIdAdvData();

    /*Initialise fastpair bloom filter globals*/
    fastPair_InitBloomFilter();
    
    /*PreCalculate Bloom Filter with available account filters, if any*/
    DEBUG_LOG("FP ADV: fastPair_SetUpAdvertising: fastPair_GenerateBloomFilter\n");
    fastPair_GenerateBloomFilter();

    /*Mandate use of Transmit Power in fastpair adverts*/
    TxPower_Mandatory(TRUE, le_client_fast_pair);

    if (fastpair_advert.adv_register_handle!=NULL)
    {
        DEBUG_LOG("FP ADV: fastPair_SetUpAdvertising: Adv Handle NOT NULL \n");
    }

    /*Register callback with Advertising Manager*/
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    fastpair_advert.adv_register_handle = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &fastPair_advertising_callback);
#else
    fastPairTaskData *fast_pair_task_data = fastPair_GetTaskData();
    fastpair_advert.adv_register_handle = LeAdvertisingManager_Register(&fast_pair_task_data->task, &fastPair_advertising_callback);
#endif
    return (fastpair_advert.adv_register_handle ? TRUE : FALSE);
}

/*! @brief Private API to handle change in Connectable state and notify the LE Advertising Manager
 */
bool fastPair_AdvNotifyChangeInConnectableState(bool connectable)
{
    bool notify = FALSE;
    if(IsHandsetConnAllowed != connectable)
    {
        IsHandsetConnAllowed = connectable;
        DEBUG_LOG("fastPair_AdvNotifyChangeInConnectableState %d", connectable);
        notify = fastPair_AdvNotifyDataChange();
    }
    return notify;
}

bool fastPair_AdvNotifyChangeInIdentifiable(bool identifiable)
{
#ifndef LE_ADVERTISING_MANAGER_NEW_API
    fastpair_advert.identifiable = identifiable;
    DEBUG_LOG("fastPair_AdvNotifyChangeInIdentifiable %d", fastpair_advert.identifiable);
    return TRUE;
#else
    bool notify = FALSE;
    if(fastpair_advert.identifiable != identifiable)
    {
        fastpair_advert.identifiable = identifiable;
        DEBUG_LOG("fastPair_AdvNotifyChangeInIdentifiable %d", fastpair_advert.identifiable);
        notify = fastPair_AdvNotifyDataChange();
    }
    return notify;
#endif
}

/*! @brief Private API to notify the LE Advertising Manager on FP adverts data change
 */
bool fastPair_AdvNotifyDataChange(void)
{
    bool status = FALSE;
    if (fastpair_advert.adv_register_handle)
    {
#ifdef LE_ADVERTISING_MANAGER_NEW_API
        status = LeAdvertisingManager_UpdateAdvertisingItem(fastpair_advert.adv_register_handle);
#else
        fastPairTaskData *fast_pair_task_data = fastPair_GetTaskData();
        status = LeAdvertisingManager_NotifyDataChange(&fast_pair_task_data->task, fastpair_advert.adv_register_handle);
#endif
    }
    else
    {
        DEBUG_LOG("FP ADV: Invalid handle in fastPair_AdvNotifyDataChange");
    }
    return status;
}

/*! @brief Private API to provide BR/EDR discoverablity information
 */
bool fastPair_AdvIsBrEdrDiscoverable(void)
{
    DEBUG_LOG("FP ADV: fastpair_AdvIsBrEdrDiscoverable %d \n", fastpair_advert.identifiable);
    return fastpair_advert.identifiable;
}

bool fastPair_AdvInUseAccountKeyActiveState(void)
{
    return fastpair_advert.in_use_account_key_active;
}
