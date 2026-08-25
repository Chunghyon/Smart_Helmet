/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       ama_ble.c
\brief      Implementation for AMA BLE
*/

#include <panic.h>
#include <logging.h>
#include <connection_manager.h>
#include <connection.h>

#include "ama_setup_tracker.h"
#include "le_advertising_manager.h"
#include "ama_ble.h"


#define AMA_SERVICE_ADV                     (0xFE03)
#define AMA_SERVICE_DATA_LENGTH             (13)
#define AMA_UUID_DATA_LENGTH                (3)
#define AMA_VENDOR_ID                       (0x000A)
#define AMA_PRODUCT_ID                      (0x0001)
#define AMA_ACCESSORY_COLOR                 (0x00)
#define AMA_DEVICE_STATE_DISCOVERABLE       (0x02)
#define AMA_DEVICE_STATE_NON_DISCOVERABLE   (0x00)
#define AMA_DEVICE_STATE_OOBE_COMPLETED     (0x00)
#define AMA_DEVICE_STATE_OOBE_NEEDS_TO_RUN  (0x01)
#define AMA_ACCESSORY_PREFERRED             (0x02)
#define AMA_LE_PREFERRED                    (0x00)
#define AMA_RFCOMM_PREFERRED                (0x01)
#define AMA_IAP_PREFERRED                   (0x02)
#define AMA_RESERVED                        (0x00)
#define AMA_DEVICE_STATE_OFFSET             (9)
#define AMA_PREFERRED_TRANSPORT_OFFSET      (10)

#ifdef INCLUDE_ACCESSORY
    #define AMA_PREFERRED_TRANSPORT AMA_RFCOMM_PREFERRED + AMA_IAP_PREFERRED;
#else
    #define AMA_PREFERRED_TRANSPORT AMA_RFCOMM_PREFERRED
#endif


/*********************************************************************/


/********************************************************************
* Advertising packet prototypes:
*/
static uint8 ama_full_service_adv_data[] = {
    (uint8)AMA_SERVICE_DATA_LENGTH,         /*Length for Service Data AD Type (23 bytes)*/
    (uint8)ble_ad_type_service_data,        /*Service Data AD Type Identifier*/
    (uint8)(AMA_SERVICE_ADV & 0xFF),        /*AMA Service ID*/
    (uint8)((AMA_SERVICE_ADV >> 8) & 0xFF),
    (uint8)(AMA_VENDOR_ID & 0xFF),          /*Vendor Id assigned by BT*/
    (uint8)((AMA_VENDOR_ID >> 8) & 0xFF),
    (uint8)(AMA_PRODUCT_ID & 0xFF),         /*Product Id for Alexa-enabled Headphones*/
    (uint8)((AMA_PRODUCT_ID >> 8) & 0xFF),
    (uint8)AMA_ACCESSORY_COLOR,             /*Color of the Accessory*/
    0x00,                                   /* Device State bit mask.  Bit 1: 1, if classic bluetooth is discoverable*/
    0x00,                                   /* Preferred Transport */
    (uint8)AMA_RESERVED,
    (uint8)AMA_RESERVED,
    (uint8)AMA_RESERVED
};

static const uint8 ama_uuid_adv_data[] = {
    (uint8)AMA_UUID_DATA_LENGTH,
    (uint8)ble_ad_type_complete_uuid16,
    (uint8)(AMA_SERVICE_ADV & 0xFF),
    (uint8)((AMA_SERVICE_ADV >> 8) & 0xFF)
};

#define SIZE_AMA_FULL_SERVICE_DATA ARRAY_DIM(ama_full_service_adv_data)
#define SIZE_AMA_UUID_DATA ARRAY_DIM(ama_uuid_adv_data)

static le_adv_item_data_t ama_adv_data = {0};

/********************************************************************
* Local functions:
*/
/*********************************************************************/
static bool amaBle_GetItemDataServiceData(le_adv_item_data_t * item)
{
    DEBUG_LOG("amaBle_GetItemDataServiceData");
    PanicNull(item);
    uint8 oobe_state = Ama_IsSetupComplete() ? AMA_DEVICE_STATE_OOBE_COMPLETED : AMA_DEVICE_STATE_OOBE_NEEDS_TO_RUN;
    ama_full_service_adv_data[AMA_DEVICE_STATE_OFFSET] = AMA_DEVICE_STATE_DISCOVERABLE | oobe_state;
    ama_full_service_adv_data[AMA_PREFERRED_TRANSPORT_OFFSET] = AMA_PREFERRED_TRANSPORT;

    ama_adv_data.size = SIZE_AMA_FULL_SERVICE_DATA;
    ama_adv_data.data = ama_full_service_adv_data;
    *item = ama_adv_data;
    return TRUE;
}

static bool amaBle_GetItemDataUuid(le_adv_item_data_t * item)
{
    DEBUG_LOG("amaBle_GetAdvertisingDataItemUuidDataset");
    PanicNull(item);
    ama_adv_data.size = SIZE_AMA_UUID_DATA;
    ama_adv_data.data = ama_uuid_adv_data;
    *item = ama_adv_data;
    return TRUE;
}

#ifdef LE_ADVERTISING_MANAGER_NEW_API

static le_adv_mgr_register_handle le_adv_data_handle_service_data = NULL;
static le_adv_mgr_register_handle le_adv_data_handle_uuid = NULL;

static bool amaBle_GetItemInfoServiceData(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = SIZE_AMA_FULL_SERVICE_DATA };
    return TRUE;
}

static bool amaBle_GetItemInfoUuid(le_adv_item_info_t * info)
{
    PanicNull(info);
    *info = (le_adv_item_info_t){ .placement = le_adv_item_data_placement_advert,
                                    .type = le_adv_type_legacy_connectable_scannable,
                                    .data_size = SIZE_AMA_UUID_DATA };
    return TRUE;
}

le_adv_item_callback_t amaBle_AdvertisingManagerCallbacksForFullDatasetItem =
{
    .GetItemData = &amaBle_GetItemDataServiceData,
    .GetItemInfo = &amaBle_GetItemInfoServiceData
};

le_adv_item_callback_t amaBle_AdvertisingManagerCallbacksForUuidItem =
{
    .GetItemData = &amaBle_GetItemDataUuid,
    .GetItemInfo = &amaBle_GetItemInfoUuid
};

#else

#define NUMBER_OF_ADVERT_DATA_ITEMS         (1)

static le_adv_mgr_register_handle le_adv_data_handle = NULL;
static void ama_BleMessageHandler(Task task, MessageId id, Message message);
static TaskData ama_ble_task = {ama_BleMessageHandler};

static void ama_BleMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    if(id == LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM)
    {
        DEBUG_LOG("ama_BleMessageHandler LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM");
    }
}

static bool amaBle_IsRequestValidForAmaFullDataSet(const le_adv_data_params_t * params)
{
    return ((params->data_set == le_adv_data_set_handset_identifiable) || (params->data_set == le_adv_data_set_handset_unidentifiable)) &&
            (params->completeness == le_adv_data_completeness_can_be_skipped) &&
            (params->placement == le_adv_data_placement_dont_care);
}

static bool amaBle_IsRequestValidForAmaUuidDataSet(const le_adv_data_params_t * params)
{
    return ((params->data_set == le_adv_data_set_handset_identifiable) || (params->data_set == le_adv_data_set_handset_unidentifiable)) &&
            (params->completeness == le_adv_data_completeness_full) &&
            (params->placement == le_adv_data_placement_advert);
}


/*********************************************************************/
static unsigned int amaBle_NumberOfAdvItems(const le_adv_data_params_t * params)
{
    unsigned int number_of_items = 0;

    if(amaBle_IsRequestValidForAmaFullDataSet(params) || amaBle_IsRequestValidForAmaUuidDataSet(params))
    {
        number_of_items = NUMBER_OF_ADVERT_DATA_ITEMS;
    }

    return number_of_items;
}

/*********************************************************************/
static le_adv_data_item_t amaBle_GetAdvDataItems(const le_adv_data_params_t * params, unsigned int id)
{
    UNUSED(id);

    DEBUG_LOG("amaBle_GetAdvDataItems enum:le_adv_data_set_t:%d enum:le_adv_data_completeness_t:%d enum:le_adv_data_placement_t:%d",
               params->data_set, params->completeness, params->placement);

    ama_adv_data.size = 0;
    ama_adv_data.data = NULL;

    if(amaBle_IsRequestValidForAmaFullDataSet(params))
    {
        amaBle_GetItemDataServiceData(&ama_adv_data);
    }
    else if(amaBle_IsRequestValidForAmaUuidDataSet(params))
    {
        amaBle_GetItemDataUuid(&ama_adv_data);
    }

    return ama_adv_data;
}

/*********************************************************************/
static void amaBle_ReleaseAdvDataItems(const le_adv_data_params_t * params)
{
    UNUSED(params);
    return;
}

static le_adv_data_callback_t amaBle_AdvertisingManagerCallback =
{
    .GetNumberOfItems   = amaBle_NumberOfAdvItems,
    .GetItem            = amaBle_GetAdvDataItems,
    .ReleaseItems       = amaBle_ReleaseAdvDataItems
};

#endif

/*********************************************************************/
bool AmaBle_SetupLeAdvertisingData(void)
{
    DEBUG_LOG("AmaBle_RegisterAdvertising");
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    le_adv_data_handle_service_data = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &amaBle_AdvertisingManagerCallbacksForFullDatasetItem);
    le_adv_data_handle_uuid = LeAdvertisingManager_RegisterAdvertisingItemCallback(NULL, &amaBle_AdvertisingManagerCallbacksForUuidItem);
    return ((le_adv_data_handle_service_data && le_adv_data_handle_uuid) ? TRUE : FALSE);
#else
    le_adv_data_handle = LeAdvertisingManager_Register(NULL, &amaBle_AdvertisingManagerCallback);
    return (le_adv_data_handle ? TRUE : FALSE);
#endif
}

bool AmaBle_UpdateLeAdvertisingData(void)
{
#ifdef LE_ADVERTISING_MANAGER_NEW_API
    return ((le_adv_data_handle_service_data ? LeAdvertisingManager_UpdateAdvertisingItem(le_adv_data_handle_service_data) : FALSE)
            && (le_adv_data_handle_uuid ? LeAdvertisingManager_UpdateAdvertisingItem(le_adv_data_handle_uuid) : FALSE));
#else
    return (le_adv_data_handle ? LeAdvertisingManager_NotifyDataChange(&ama_ble_task, le_adv_data_handle) : FALSE);
#endif
}

