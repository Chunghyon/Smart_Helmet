/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service multipoint feature registration and handling.
*/

#include "handset_service_feature_multipoint.h"
#include "handset_service_config.h"
#include "handset_service_protected.h"
#include "handset_service_sm.h"

#include <device_list.h>
#include <device_properties.h>
#include <feature_manager.h>
#include <focus_device.h>
#include <ui.h>

#include <logging.h>


#ifdef ENABLE_LE_AUDIO_RESTRICTED_MULTIPOINT

static feature_manager_handle_t multipoint_feature_handle = NULL;
static feature_state_t multipoint_feature_state = feature_state_idle;


static unsigned handsetService_FeatureMultipointGetLinkLossHandsetCount(void)
{
    unsigned linkloss_handset_count = 0;
    device_t* devices = NULL;
    unsigned num_devices = 0;
    handset_context_t context = handset_context_link_loss;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_handset_context, &context, sizeof(context), &devices, &num_devices);

    if (devices && num_devices)
    {
        for (unsigned i = 0; i < num_devices; i++)
        {
            if (DEVICE_TYPE_HANDSET == BtDevice_GetDeviceType(devices[i]))
            {
                linkloss_handset_count++;
            }
        }
    }

    free(devices);

    DEBUG_LOG_VERBOSE("handsetService_FeatureMultipointGetLinkLossHandsetCount count %u", linkloss_handset_count);

    return linkloss_handset_count;
}

static void handsetService_FeatureMultipointSetState(feature_state_t state)
{
    DEBUG_LOG("handsetService_FeatureMultipointSetState state enum:feature_state_t:%u->enum:feature_state_t:%u", multipoint_feature_state, state);
    multipoint_feature_state = state;
}

static feature_state_t handsetService_FeatureMultipointGetState(void)
{
    DEBUG_LOG("handsetService_FeatureMultipointGetState state enum:feature_state_t:%u", multipoint_feature_state);
    return multipoint_feature_state;
}

static void handsetService_FeatureMultipointSuspend(void)
{
    DEBUG_LOG("handsetService_FeatureMultipointSuspend state enum:feature_state_t:%u", multipoint_feature_state);

    if (feature_state_running == handsetService_FeatureMultipointGetState())
    {
        handsetService_FeatureMultipointSetState(feature_state_suspended);

        /* Decide if a handset needs to be disconnected to keep the total
           number of handsets < max number of handsets allowed.

           Count both connected handsets and handsets that are disconnected due
           to link-loss (because they could be re-connected soon). */
        unsigned num_active_handsets = HandsetServiceSm_GetBredrOrLeAclConnectionCount();
        num_active_handsets += handsetService_FeatureMultipointGetLinkLossHandsetCount();

        if (num_active_handsets > HandsetService_GetMaxNumberOfConnectedBredrHandsets())
        {
            /* If multipoint was disabled but 2 handsets are connected or
               connecting then one of them needs to be disconnected. */
            Ui_InjectUiInput(ui_input_disconnect_lru_handset);
        }
    }
}

static void handsetService_FeatureMultipointResume(void)
{
    DEBUG_LOG("handsetService_FeatureMultipointResume state enum:feature_state_t:%u", multipoint_feature_state);

    if (feature_state_suspended == handsetService_FeatureMultipointGetState())
    {
        handsetService_FeatureMultipointSetState(feature_state_running);

        /* reconnect handset(s) if less than the maximum allowed are connected
           or connecting. */
        if (HandsetServiceSm_GetBredrOrLeAclConnectionCount() < HandsetService_GetMaxNumberOfConnectedBredrHandsets())
        {
            Ui_InjectUiInput(ui_input_connect_handset);
        }
    }
}

static const feature_interface_t handset_config_feature_manager_if = {
    .GetState = handsetService_FeatureMultipointGetState,
    .Suspend = handsetService_FeatureMultipointSuspend,
    .Resume = handsetService_FeatureMultipointResume
};

void HandsetService_FeatureMultipointInit(void)
{
    multipoint_feature_handle = FeatureManager_Register(feature_id_handset_multipoint, &handset_config_feature_manager_if);

    handsetService_FeatureMultipointSetState(feature_state_idle);

    DEBUG_LOG("HandsetService_FeatureMultipointInit hdl 0x%x state enum:feature_state_t:%u",
              multipoint_feature_handle, multipoint_feature_state);
}

void HandsetService_FeatureMultipointUpdateState(const handset_service_config_t *config)
{
    DEBUG_LOG("HandsetService_FeatureMultipointUpdateState state enum:feature_state_t:%u", handsetService_FeatureMultipointGetState());

    if (config->max_bredr_connections > 1)
    {
        if (feature_state_idle == handsetService_FeatureMultipointGetState())
        {
            /* If multipoint is enabled try to start the multipoint feature */
            if (FeatureManager_StartFeatureRequest(multipoint_feature_handle))
            {
                handsetService_FeatureMultipointSetState(feature_state_running);
            }
            else
            {
                handsetService_FeatureMultipointSetState(feature_state_suspended);
            }
        }
    }
    else
    {
        if (feature_state_idle != handsetService_FeatureMultipointGetState())
        {
            handsetService_FeatureMultipointSetState(feature_state_idle);
            FeatureManager_StopFeatureIndication(multipoint_feature_handle);  
        }
    }
}

bool HandsetService_FeatureMultipointAllowed(void)
{
   return (handsetService_FeatureMultipointGetState() == feature_state_running);
}

#endif /* ENABLE_LE_AUDIO_RESTRICTED_MULTIPOINT */
