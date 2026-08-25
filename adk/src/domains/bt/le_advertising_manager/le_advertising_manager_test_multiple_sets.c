/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation file for LE advertising manager test APIs to demonstrate the use of multiple advertising sets
*/

#include "le_advertising_manager_private.h"
#include "le_advertising_manager_test_multiple_sets.h"

#include <bdaddr.h>
#include <panic.h>


#ifdef LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS

static bool multiple_sets_enabled = FALSE;

void LeAdvertisingManager_MultipleSetsEnableAdvertisingMultipleSets(bool enable)
{
    if(enable)
    {
        multiple_sets_enabled = TRUE;
    }
}

static bool leAdvertisingManager_MultipleSetsIsMultipleSetsEnabled(void)
{
    return multiple_sets_enabled;
}

#define ADV_SET_1 1
#define ADV_SET_2 2
#define ADV_SET_3 3
#define ADV_SET_4 4

#define ADV_HANDLE_MIN ADV_SET_1
#define ADV_HANDLE_MAX ADV_SET_4

static bool leAdvertisingManager_MultipleSetsIsSetValid(uint8 set_id)
{
    bool is_valid = TRUE;

    if((set_id < ADV_HANDLE_MIN) || (set_id > ADV_HANDLE_MAX))
        is_valid = FALSE;

    return is_valid;
}

bool LeAdvertisingManager_MultipleSetsGetAdvertisingSetRange(uint8 *min, uint8 *max)
{
    bool is_valid = FALSE;

    if(min && max)
    {
        *min = ADV_HANDLE_MIN;
        *max = ADV_HANDLE_MAX;

        is_valid = TRUE;
    }

    return is_valid;
}

bool LeAdvertisingManager_MultipleSetsRegisterSet(uint8 set_id)
{
    bool is_registered = FALSE;

    if(leAdvertisingManager_MultipleSetsIsMultipleSetsEnabled() && leAdvertisingManager_MultipleSetsIsSetValid(set_id))
    {
        ConnectionDmBleExtAdvRegisterAppAdvSetReq(AdvManagerGetTask(), set_id);
        is_registered = TRUE;
    }

    return is_registered;
}

bool LeAdvertisingManager_MultipleSetsConfigureParamsForSet(uint8 set_id, uint16 events, uint32 interval_min, uint32 interval_max, uint16 sid)
{
    bool is_configured = FALSE;

    if(leAdvertisingManager_MultipleSetsIsMultipleSetsEnabled() && leAdvertisingManager_MultipleSetsIsSetValid(set_id))
    {
        adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

        ConnectionDmBleExtAdvSetParamsReq(&adv_task_data->extended_task,
                                          set_id,
                                          events,
                                          interval_min,
                                          interval_max,
                                          DEFAULT_PRIMARY_ADV_CHANNEL_MAP,
                                          DEFAULT_OWN_ADDR_TYPE,
                                          DEFAULT_PEER_TPADDR,
                                          DEFAULT_ADV_FILTER_POLICY,
                                          DEFAULT_PRIMARY_ADV_PHY,
                                          DEFAULT_SECONDARY_ADV_MAX_SKIP,
                                          DEFAULT_SECONDARY_ADV_PHY,
                                          sid);
        is_configured = TRUE;
    }

    return is_configured;
}

bool LeAdvertisingManager_MultipleSetsConfigureAddressForSet(uint8 set_id)
{
    bool is_configured = FALSE;

    if(leAdvertisingManager_MultipleSetsIsMultipleSetsEnabled() && leAdvertisingManager_MultipleSetsIsSetValid(set_id))
    {
        adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        bdaddr addr = {0};

        ConnectionDmBleExtAdvSetRandomAddressReq(&adv_task_data->extended_task,
                                                 set_id,
                                                 ble_local_addr_use_global,
                                                 addr);
        is_configured = TRUE;
    }

    return is_configured;
}

bool LeAdvertisingManager_MultipleSetsConfigureDataForSet(uint8 set_id)
{
    bool is_configured = FALSE;

    if(leAdvertisingManager_MultipleSetsIsMultipleSetsEnabled() && leAdvertisingManager_MultipleSetsIsSetValid(set_id))
    {

#define SIZE_BUFFER 4
#define SIZE_PTR_ARRAY 8

        adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        uint8 * array = PanicUnlessMalloc(SIZE_BUFFER);
        memset(array,0x0,SIZE_BUFFER);
        array[0] = 0x3;
        array[1] = 0x16;
        array[2] = set_id;
        array[3] = 0xF - set_id;

        uint8 *ptr_array[SIZE_PTR_ARRAY] = {array,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

        ConnectionDmBleExtAdvSetDataReq( &adv_task_data->extended_task,
                                         set_id,
                                         complete_data,
                                         SIZE_BUFFER,
                                         ptr_array);

        is_configured = TRUE;
    }

    return is_configured;
}

bool LeAdvertisingManager_MultipleSetsEnableSet(uint8 set_id, bool enable)
{
    bool is_enabled = FALSE;

    if(leAdvertisingManager_MultipleSetsIsMultipleSetsEnabled() && leAdvertisingManager_MultipleSetsIsSetValid(set_id))
    {

        adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

        ConnectionDmBleExtAdvertiseEnableReq(&adv_task_data->extended_task,
                                             enable,
                                             set_id);

        is_enabled = TRUE;
    }

    return is_enabled;
}

#endif /* LE_ADVERTISING_MANAGER_TEST_MULTIPLE_SETS */
