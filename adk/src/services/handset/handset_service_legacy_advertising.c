/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Manage the handset legacy advertising set.
*/

#include "handset_service_protected.h"
#include "handset_service_legacy_advertising.h"

#include "le_advertising_manager.h"

#include <local_addr.h>

#ifndef LE_ADVERTISING_MANAGER_NEW_API

/*! \brief Disable advertising by releasing the LE advertising data set */
static void handsetService_DisableAdvertising(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetLegacyAdvertisingData();

    HS_LOG("handsetService_DisableAdvertising, release set with handle=%p", hs_adv->handle);

    PanicFalse(LeAdvertisingManager_ReleaseAdvertisingDataSet(hs_adv->handle));
    
    hs_adv->handle = NULL;
    hs_adv->state = handset_service_le_adv_data_set_state_releasing;
    
    HS_LOG("handsetService_DisableAdvertising,  handle=%p", hs_adv->handle);
}

/*! \brief Get advertising data set which needs to be selected */
static le_adv_data_set_t handsetService_GetLeAdvDataSetToBeSelected(void)
{    
    bool is_pairing = HandsetService_IsPairing();
    bool is_local_addr_public = LocalAddr_IsPublic();

    HS_LOG("handsetService_GetLeAdvDataSetToBeSelected, Is in pairing:%d, Is local address public:%d", is_pairing, is_local_addr_public);

    le_adv_data_set_t set;

    if(is_pairing || is_local_addr_public)
    {
        set = le_adv_data_set_handset_identifiable;
    }
    else
    {
        set = le_adv_data_set_handset_unidentifiable;
    }

    return set;
}

/*! \brief Enable advertising by selecting the LE advertising data set */
static void handsetService_EnableAdvertising(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetLegacyAdvertisingData();

    le_adv_select_params_t adv_select_params;
    le_adv_data_set_handle adv_handle = NULL;

    HS_LOG("handsetService_EnableAdvertising, Le Adv State is enum:handset_service_le_adv_data_set_state_t:%x, Le Adv Selected Data Set is enum:le_adv_data_set_t:%x",
           hs_adv->state, hs_adv->set);

    adv_select_params.set = handsetService_GetLeAdvDataSetToBeSelected();

    adv_handle = LeAdvertisingManager_SelectAdvertisingDataSet(HandsetService_GetLegacyAdvertisingTask(), &adv_select_params);
    
    hs_adv->state = handset_service_le_adv_data_set_state_selecting;
    hs_adv->set = adv_select_params.set;

    if (adv_handle != NULL)
    {
        hs_adv->handle = adv_handle;

        HS_LOG("handsetService_EnableAdvertising. Selected set with handle=%p", hs_adv->handle);
    }
}

/*! \brief Updates the BLE advertising data 
    \return TRUE if Advertising Data shall be updated, FALSE otherwise.  
*/
bool HandsetService_UpdateLegacyAdvertisingData(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetLegacyAdvertisingData();

    if(   (handset_service_le_adv_data_set_state_releasing == hs_adv->state)
       || (handset_service_le_adv_data_set_state_selecting == hs_adv->state) )
    {
        HS_LOG("HandsetService_UpdateLegacyAdvertisingData. Le advertising data set select/release state is enum:handset_service_le_adv_data_set_state_t:%x",
               hs_adv->state);
        return TRUE;
    }

    bool is_le_connectable = HandsetService_IsBleConnectable();
    unsigned le_connections = HandsetServiceSm_GetLeAclConnectionCount();
    bool have_spare_le_connections = le_connections < handsetService_LeAclMaxConnections();
    le_adv_data_set_t data_set = handsetService_GetLeAdvDataSetToBeSelected();
    bool is_le_adv_data_set_update_needed = hs_adv->set != data_set;
    bool pairing_possible = HandsetServiceSm_CouldDevicesPair();

    HS_LOG("HandsetService_UpdateLegacyAdvertisingData. state enum:handset_service_le_adv_data_set_state_t:%x Connectable:%d Spare LE:%d PairingPossible:%d",
              hs_adv->state, is_le_connectable, have_spare_le_connections, pairing_possible);

    if(hs_adv->handle)
    {
        bool disable_advertising = !is_le_connectable || is_le_adv_data_set_update_needed || !have_spare_le_connections || pairing_possible;

        HS_LOG("HandsetService_UpdateLegacyAdvertisingData. Active data set is 0x%x. Requested data set is 0x%x. Disable:%d",
               hs_adv->set, data_set, disable_advertising);

        if (disable_advertising)
        {
            handsetService_DisableAdvertising();
            return TRUE;
        }
    }
    else
    {
        bool enable_advertising = is_le_connectable && have_spare_le_connections && !pairing_possible;

        HS_LOG("HandsetService_UpdateLegacyAdvertisingData. There is no active data set. Enable:%d",
                enable_advertising);

        if (enable_advertising)
        {
            handsetService_EnableAdvertising();
            return TRUE;
        }
    }
    return FALSE;
}

/*! \brief Update the state of LE advertising data set select/release operation */
static void handsetService_UpdateLeAdvertisingDataSetState(handset_service_le_adv_data_set_state_t state)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetLegacyAdvertisingData();

    hs_adv->state = state;
    HandsetService_UpdateLegacyAdvertisingData();
}

static void handsetService_HandleLeAdvMgrSelectDatasetCfm(const LE_ADV_MGR_SELECT_DATASET_CFM_T *cfm)
{
    HS_LOG("handsetService_HandleLeAdvMgrSelectDatasetCfm, cfm status is enum:le_adv_mgr_status_t:%u", cfm->status );

    if (cfm->status == le_adv_mgr_status_success)
    {
        handsetService_UpdateLeAdvertisingDataSetState(handset_service_le_adv_data_set_state_selected);
        HandsetService_SendLeConnectableIndication(TRUE);
    }
    else
    {
        Panic();
    }

}

static void handsetService_HandleLeAdvMgrReleaseDatasetCfm(const LE_ADV_MGR_RELEASE_DATASET_CFM_T *cfm)
{
    HS_LOG("handsetService_HandleLeAdvMgrReleaseDatasetCfm, cfm status is %x", cfm->status );

    if (cfm->status == le_adv_mgr_status_success)
    {
        handsetService_UpdateLeAdvertisingDataSetState(handset_service_le_adv_data_set_state_not_selected);
        HandsetService_SendLeConnectableIndication(FALSE);
    }
    else
    {
        Panic();
    }
}

static void handsetService_LegacyAdvertisingHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* LE Advertising messages */
    case LE_ADV_MGR_SELECT_DATASET_CFM:
        {
            handsetService_HandleLeAdvMgrSelectDatasetCfm((const LE_ADV_MGR_SELECT_DATASET_CFM_T *)message);
        }
        break;
    case LE_ADV_MGR_RELEASE_DATASET_CFM:
        {
            handsetService_HandleLeAdvMgrReleaseDatasetCfm((const LE_ADV_MGR_RELEASE_DATASET_CFM_T *)message);
        }
        break;
    default:
        {
            UNUSED(message);
            DEBUG_LOG("handsetService_LegacyAdvertisingHandleMessage Unhandled %d", id);
            Panic();
        }
        break;
    }
}

void HandsetService_LegacyAdvertisingInit(void)
{
    handset_service_advert_task_data_t *hs_adv = HandsetService_GetLegacyAdvertisingData();

    memset(hs_adv, 0, sizeof(*hs_adv));
    hs_adv->task_data.handler = handsetService_LegacyAdvertisingHandleMessage;
}

#endif
