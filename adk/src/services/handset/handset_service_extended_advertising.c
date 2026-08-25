/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage the handset extended advertising set.
*/

#if defined(INCLUDE_ADVERTISING_EXTENSIONS)

#include "handset_service_extended_advertising.h"
#include "handset_service_protected.h"

#include "le_advertising_manager.h"

#include <logging.h>
#include <panic.h>

#ifndef LE_ADVERTISING_MANAGER_NEW_API

static void handsetServiceExtAdv_SetBleAdvertState(handset_service_le_adv_data_set_state_t state)
{
    HandsetService_GetExtendedAdvertisingData()->state = state;
    UNUSED(HandsetServiceExtAdv_UpdateAdvertisingData());
}

static void handsetServiceExtAdv_EnableAdvertising(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetExtendedAdvertisingData();
    le_adv_select_params_t adv_select_params;
    le_adv_data_set_handle adv_handle = NULL;

    DEBUG_LOG("handsetServiceExtAdv_EnableAdvertising, Le Advert State is %d", hs_adv->state);

    adv_select_params.set = le_adv_data_set_extended_handset;

    adv_handle = LeAdvertisingManager_SelectAdvertisingDataSet(HandsetService_GetExtendedAdvertisingTask(), &adv_select_params);

    hs_adv->state = handset_service_le_adv_data_set_state_selecting;
    hs_adv->set = adv_select_params.set;

    if (adv_handle != NULL)
    {
        hs_adv->handle = adv_handle;
        DEBUG_LOG("handsetServiceExtAdv_EnableAdvertising. Selected set with handle=%p", hs_adv->handle);
    }
}

static void handsetServiceExtAdv_DisableAdvertising(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetExtendedAdvertisingData();

    DEBUG_LOG("handsetServiceExtAdv_DisableAdvertising, release set with handle=%p", hs_adv->handle);

    PanicFalse(LeAdvertisingManager_ReleaseAdvertisingDataSet(hs_adv->handle));

    hs_adv->handle = NULL;
    hs_adv->state = handset_service_le_adv_data_set_state_releasing;

    DEBUG_LOG("handsetServiceExtAdv_DisableAdvertising, handle=%p", hs_adv->handle);
}


static void  handsetServiceExtAdv_HandleLamSelectDatasetCfm(const LE_ADV_MGR_SELECT_DATASET_CFM_T *cfm)
{
    DEBUG_LOG("handsetServiceExtAdv_HandleLamSelectDatasetCfm, cfm status is %d", cfm->status);

    PanicFalse(cfm->status == le_adv_mgr_status_success);

    handsetServiceExtAdv_SetBleAdvertState(handset_service_le_adv_data_set_state_selected);
}

static void handsetServiceExtAdv_HandleLamReleaseDatasetCfm(const LE_ADV_MGR_RELEASE_DATASET_CFM_T *cfm)
{
    DEBUG_LOG("handsetServiceExtAdv_HandleLamReleaseDatasetCfm, cfm status is %d", cfm->status);

    PanicFalse(cfm->status == le_adv_mgr_status_success);

    handsetServiceExtAdv_SetBleAdvertState(handset_service_le_adv_data_set_state_not_selected);
}
#endif

static void handsetServiceExtAdv_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
#ifndef LE_ADVERTISING_MANAGER_NEW_API
    /* LE Advertising messages */
    case LE_ADV_MGR_SELECT_DATASET_CFM:
        {
            handsetServiceExtAdv_HandleLamSelectDatasetCfm((const LE_ADV_MGR_SELECT_DATASET_CFM_T *)message);
        }
        break;
    case LE_ADV_MGR_RELEASE_DATASET_CFM:
        {
            handsetServiceExtAdv_HandleLamReleaseDatasetCfm((const LE_ADV_MGR_RELEASE_DATASET_CFM_T *)message);
        }
        break;
#endif
    default:
        {
            UNUSED(message);
            DEBUG_LOG("handsetServiceExtAdv_HandleMessage Unhandled %d", id);
            Panic();
        }
        break;
    }
}

void HandsetServiceExtAdv_Init(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetExtendedAdvertisingData();

    memset(hs_adv, 0, sizeof(*hs_adv));
    hs_adv->task_data.handler = handsetServiceExtAdv_HandleMessage;
}

bool HandsetServiceExtAdv_UpdateAdvertisingData(void)
{
#ifndef LE_ADVERTISING_MANAGER_NEW_API
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetExtendedAdvertisingData();

    if (   (hs_adv->state == handset_service_le_adv_data_set_state_selecting)
        || (hs_adv->state == handset_service_le_adv_data_set_state_releasing))
    {
        HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. Le advertising data set select/release state is enum:handset_service_ext_advert_state_t:%d", 
                    hs_adv->state);
        return TRUE;
    }

    unsigned le_connections = HandsetServiceSm_GetLeAclConnectionCount();
    bool have_spare_le_connections = le_connections < handsetService_LeAclMaxConnections();
    bool is_le_connectable = HandsetService_IsBleConnectable();
    bool pairing_possible = HandsetServiceSm_CouldDevicesPair();

    bool enable_advertising = is_le_connectable && have_spare_le_connections && !pairing_possible;

    HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. State enum:handset_service_ext_advert_state_t:%d. Le Connectable Status is %x. Spare LE conns:%d.", 
            hs_adv->state, is_le_connectable, have_spare_le_connections);

    if (hs_adv->handle)
    {
        HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. There is an active data set with handle=%p. Disable:%d", 
                  hs_adv->handle, !enable_advertising);

        if (!enable_advertising)
        {
            handsetServiceExtAdv_DisableAdvertising();
            return TRUE;
        }
    }
    else
    {
        HS_LOG("HandsetServiceExtAdv_UpdateAdvertisingData. There is no active data set. Enable:%d",
                  enable_advertising);

        if (enable_advertising)
        {
            handsetServiceExtAdv_EnableAdvertising();
            return TRUE;
        }
    }
#endif
    return FALSE;
}

#endif /* defined(INCLUDE_ADVERTISING_EXTENSIONS) */
