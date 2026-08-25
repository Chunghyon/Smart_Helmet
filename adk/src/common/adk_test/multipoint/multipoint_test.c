/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of common multipoint specifc testing functions.
*/

#include "multipoint_test.h"

#include <logging.h>
#include <device_properties.h>
#include <device_list.h>
#include <handset_service.h>
#include <connection_manager.h>
#include <focus_select.h>
#include <ui.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

void MultipointTest_EnableMultipoint(void)
{
    DEBUG_LOG_ALWAYS("MultipointTest_EnableMultipoint");
    Ui_InjectUiInput(ui_input_enable_multipoint);
}

void MultipointTest_DisableMultipoint(void)
{
    DEBUG_LOG_ALWAYS("MultipointTest_DisableMultipoint");
    Ui_InjectUiInput(ui_input_disable_multipoint);
}

bool MultipointTest_IsMultipointEnabled(void)
{
    bool enabled = HandsetService_IsBrEdrMultipointEnabled();
    DEBUG_LOG_ALWAYS("MultipointTest_IsMultipointEnabled:%d",enabled);
    return enabled;
}

static uint8 num_handsets_connected; 

static void multipointTest_IncrementConnectedHandsetCount(device_t device, void *data)
{
    UNUSED(data);
    bdaddr handset_addr = {0};
    bool is_acl_connected = FALSE;
    uint8 connected_profiles_mask;

    if (device && (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET))
    {
        handset_addr = DeviceProperties_GetBdAddr(device);

        is_acl_connected = ConManagerIsConnected(&handset_addr);
        connected_profiles_mask = BtDevice_GetConnectedProfiles(device);

        DEBUG_LOG_ALWAYS("multipointTest_IncrementConnectedHandsetCount handset %04x,%02x,%06lx connected_profiles 0x%x is_acl_conn %d",
                                handset_addr.nap,
                                handset_addr.uap,
                                handset_addr.lap,
                                connected_profiles_mask,
                                is_acl_connected);

        if (is_acl_connected &&
            (connected_profiles_mask & DEVICE_PROFILE_A2DP ||
             connected_profiles_mask & DEVICE_PROFILE_HFP))
        {
            num_handsets_connected += 1;
        }
    }
}

uint8 MultipointTest_NumberOfHandsetsConnected(void)
{
    DEBUG_LOG_ALWAYS("MultipointTest_NumberOfHandsetsConnected");
    num_handsets_connected = 0;

    DeviceList_Iterate(multipointTest_IncrementConnectedHandsetCount, NULL);

    DEBUG_LOG_ALWAYS("MultipointTest_NumberOfHandsetsConnected num_handsets_connected %u",num_handsets_connected);
    return num_handsets_connected;
}

void MultipointTest_EnableA2dpBargeIn(bool enable)
{
    FocusSelect_EnableA2dpBargeIn(enable);
}

bool MultipointTest_IsA2dpBargeInEnabled(void)
{
    return FocusSelect_IsA2dpBargeInEnabled();
}
