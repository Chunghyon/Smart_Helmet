/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sink service state machine instance implementation
*/

/* local logging */
#include "sink_service_logging.h"

/* local includes */
#include "sink_service_protected.h"
#include "sink_service_private.h"
#include "sink_service_sm.h"
#include "sink_service_config.h"

/* framework includes */

#include <bt_device.h>
#include <rssi_pairing.h>
#include <connection_manager.h>
#include <device_properties.h>
#include <profile_manager.h>
#include <device_list.h>
#include <task_list.h>
#include <device_db_serialiser.h>
#include <unexpected_message.h>

/* system includes */
#include <bdaddr.h>
#include <stdlib.h>
#include <panic.h>

#define SINK_SERVICE_CLIENT_TASKS_LIST_INIT_CAPACITY 1

/*! \brief Cast a Task to a sink_service_state_machine_t.
    This depends on task_data being the first member of sink_service_state_machine_t. */
#define sinkServiceSm_GetSmFromTask(task) ((sink_service_state_machine_t *)task)
#define sinkServiceSm_GetTaskForSm(_sm) ((&_sm->task_data))
#define sinkServiceSm_GetStateForSm(_sm) ((_sm->state))

#define PROFILE_LIST_LENGTH 10

/* Delay before requesting to connect the profiles. */
#define CONNECT_PROFILES_DELAY D_SEC(5)

#define sinkServiceSm_ProfileIsSet(profiles, profile) \
    (((profiles) & (profile)) == (profile))


/* Check that a profile bitfield is a power of 2 (i.e. only one bit is set) and is non zero */
#define OnlyOneProfileConnected(profiles) (profiles && (!(profiles & (profiles - 1))))

static void sinkServiceSm_SetState(sink_service_state_machine_t *sm, sink_service_state_t state);

static bool sinkServiceSm_CancelProfileConnectionTimeout(sink_service_state_machine_t *sm)
{
    if(MessageCancelFirst(&sm->task_data, SINK_SERVICE_INTERNAL_CONNECT_PROFILES_REQ))
    {
        DEBUG_LOG("sinkServiceSm_CancelProfileConnectionTimeout");
        return TRUE;
    }
    return FALSE;
}

static void sinkServiceSm_StartProfileConnectionTimeout(sink_service_state_machine_t *sm)
{
    DEBUG_LOG("sinkServiceSm_StartProfileConnectionTimeout %dms", CONNECT_PROFILES_DELAY);
    MessageSendLater(&sm->task_data, SINK_SERVICE_INTERNAL_CONNECT_PROFILES_REQ, NULL, CONNECT_PROFILES_DELAY);
}

static void sinkServiceSm_RestartProfileConnectionTimeout(sink_service_state_machine_t *sm)
{
    if(sinkServiceSm_CancelProfileConnectionTimeout(sm))
    {
        sinkServiceSm_StartProfileConnectionTimeout(sm);
    }
    else
    {
        DEBUG_LOG("sinkServiceSm_RestartProfileConnectionTimeout - timeout not in progress");
    }
}

static bool sinkServiceSm_DeviceIsPaired(sink_service_state_machine_t *sm)
{
    uint16 flags = DEVICE_FLAGS_NO_FLAGS;

    bdaddr device_addr = DeviceProperties_GetBdAddr(sm->sink_device);
    appDeviceGetFlags(&device_addr, &flags);

    if(flags & DEVICE_FLAGS_NOT_PAIRED)
    {
        return FALSE;
    }
    return TRUE;
}

static void sinkServiceSm_DeleteDeviceIfNotPaired(sink_service_state_machine_t *sm)
{
    if (!sm->sink_device)
    {
        return;
    }

    if(!sinkServiceSm_DeviceIsPaired(sm))
    {
        bdaddr device_addr = DeviceProperties_GetBdAddr(sm->sink_device);
        appDeviceDelete(&device_addr);
        SinkServiceSm_SetDevice(sm, (device_t)0);
    }
}

static device_t sinkServiceSm_DetermineSinkDevice(void)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_DetermineSinkDevice");

    device_t sink = NULL;

    /* Try most recently used (MRU) device first */
    sink = BtDevice_GetMruDevice();

    if (!sink)
    {
        DEBUG_LOG_DEBUG("sinkServiceSm_DetermineSinkDevice: No MRU device, fall back to first entry on the device list instead");

        /* No MRU device, fall back to first entry on the device list instead */
        deviceType type = DEVICE_TYPE_SINK;
        sink = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type,
                                                          &type, sizeof(deviceType));
    }
    return sink;
}

static uint32 sinkServiceSm_GetConnectedProfiles(sink_service_state_machine_t *sm)
{
    if (!BtDevice_DeviceIsValid(sm->sink_device))
    {
        return 0;
    }

    return BtDevice_GetConnectedProfiles(sm->sink_device);
}

static uint32 sinkServiceSm_GetRequestedProfilesNotConnected(sink_service_state_machine_t *sm)
{
    uint32 connected_profiles = sinkServiceSm_GetConnectedProfiles(sm);

    return sm->profiles_requested & ~connected_profiles;
}

static bool sinkServiceSm_AreAllRequestedProfilesConnected(sink_service_state_machine_t *sm)
{

    bdaddr device_addr = DeviceProperties_GetBdAddr(sm->sink_device);
    if (ConManagerIsConnected(&device_addr) && (sinkServiceSm_GetRequestedProfilesNotConnected(sm) == 0))
    {
        return TRUE;
    }

    return FALSE;
}

static void sinkServiceSm_PrintProfileBitmask(uint32 bitmask)
{
    for (profile_t profile_index = 0; profile_index <= profile_manager_max_number_of_profiles; profile_index++)
    {
        if (bitmask & (1 << profile_index))
        {
            DEBUG_LOG("    enum:profile_t:%d", profile_index);
        }
    }
}

static uint32 sinkServiceSm_ConvertProfileListToBitmask(const profile_t * list, size_t length)
{
    uint32 profile_bitmask = 0;
    for (int i = 0; i < length ; i++)
    {
        profile_bitmask |= (1 << list[i]);
    }
    DEBUG_LOG("sinkServiceSm_ConvertProfileListToBitmask profile_bitmask:");
    sinkServiceSm_PrintProfileBitmask(profile_bitmask);
    return profile_bitmask;
}

static size_t sinkServiceSm_ConvertBitmaskToProfileList(uint32 profile_bitmask, profile_t * profile_list)
{
    size_t list_length = 0;

    /* loop through the configured profile list. If that bit is set in the profile bitmask add it to the profile list
       This way the profiles are added to the list in the configured connection order*/
    for (int i = 0; i <=  SinkServiceConfig_GetConfig()->profile_list_length ; i++)
    {
        if (profile_bitmask & (1<<SinkServiceConfig_GetConfig()->profile_list[i]))
        {
            profile_list[list_length] = SinkServiceConfig_GetConfig()->profile_list[i];
            list_length++;
        }
    }

    return list_length;
}

static bool sinkServiceSm_ConnectRequestedProfiles(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ConnectRequestedProfiles");

    profile_t profile_list[profile_manager_max_number_of_profiles];

    /* Connect the profiles_requested. If a profile has already been connected by the remote it does not need to be conneced */
    size_t list_length = sinkServiceSm_ConvertBitmaskToProfileList(sm->profiles_requested, profile_list);

    /* Print out connecting profile list */
    DEBUG_LOG_INFO("Sink Service: Connecting Profiles:");

    for (int i = 0; i < list_length ; i++)
    {
        DEBUG_LOG_INFO("    enum:profile_t:%d",profile_list[i]);
    }

    /* Set the connect profile order as this has to be done for each profile connect request */
    Device_SetProperty(sm->sink_device,
                       device_property_profiles_connect_order,
                       profile_list,
                       sizeof(profile_list));

    if (!ProfileManager_ConnectProfilesRequest(sinkServiceSm_GetTaskForSm(sm), sm->sink_device))
    {
        DEBUG_LOG_ERROR("SinkService: No profiles to connect!");
        return FALSE;
    }
    return TRUE;
}

static void sinkServiceSm_ReleaseAcl(sink_service_state_machine_t * sm)
{
    /* Release the ACL if there is an ACL */
    if (!BdaddrIsZero(&sm->acl_hold_addr))
    {
        DEBUG_LOG_DEBUG("SinkService: Release pairing ACL");
        ConManagerReleaseAcl(&sm->acl_hold_addr);
        BdaddrSetZero(&sm->acl_hold_addr);
    }
}

static void sinkServiceSm_StatePairingHandler(sink_service_state_machine_t * sm, MessageId id, Message message)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_StatePairingHandler");

    switch(id)
    {
    case RSSI_PAIRING_PAIR_CFM:
    {
        DEBUG_LOG_DEBUG("sinkServiceSm_StatePairingHandler: RSSI_PAIRING_PAIR_CFM");

        const RSSI_PAIRING_PAIR_CFM_T * msg = message;
        const bdaddr * bd_addr = &msg->bd_addr;
        if (msg->status)
        {
            DEBUG_LOG_INFO("SinkService: Sink paired successfully: 0x%04x,%02x,%06lx",
                           bd_addr->nap, bd_addr->uap, bd_addr->lap);

            /* Pairing request is locally initiated, so the device is a sink. */
            device_t sink = BtDevice_GetDeviceCreateIfNew(bd_addr, DEVICE_TYPE_SINK);
            BtDevice_SetDefaultProperties(sink);

            /* The device has paired. Clear the NOT_PAIRED Flag */
            PanicFalse(BtDevice_SetFlags(sink, DEVICE_FLAGS_NOT_PAIRED, 0));

            /* Now that we have successfully paired,
             * we can set the link behavior within bluestack to disable connection retries */
            BtDevice_SetLinkBehavior(bd_addr);

            BtDevice_AddSupportedProfilesToDevice(sink, SinkServiceConfig_GetConfig()->supported_profile_mask);

            DeviceDbSerialiser_SerialiseDevice(sink);

            if (ConManagerIsConnected(bd_addr))
            {
                DEBUG_LOG_INFO("SinkService: Connecting profiles...");

                /* Set the newly created device as the current connected_sink. */
                sm->sink_device = sink;

                /* Pairing is finished, ACL up, so move to the connecting profiles state.*/
                sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTING_PROFILES);
            }
            else
            {
                /* If there is a link loss or remote disconnection while the RSSI Pairing
                 * is sending its confirmation the SM needs to move to disconnected */
                DEBUG_LOG_WARN("SinkService: ACL unexpectedly dropped after pairing");
                sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
            }
        }
        else
        {
            DEBUG_LOG_ERROR("SinkService: Pairing failed");
            SinkService_SendConnectedCfm(sm->sink_device, sink_service_status_failed);
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);

        }
    }
        break;

    case CON_MANAGER_CONNECTION_IND:
    {
        DEBUG_LOG_DEBUG("sinkServiceSm_StatePairingHandler: CON_MANAGER_CONNECTION_IND");
        const CON_MANAGER_CONNECTION_IND_T *msg = message;
        if (msg->connected && ConManagerIsAclLocal(&msg->bd_addr))
        {
            /* DeviceType isn't known during pairing (device entry not created yet),
               but if we initiated the connection, then it must be a sink device we
               are about to pair with. Hold the ACL open to allow time for profiles
               to connect after pairing completes. */
            DEBUG_LOG_INFO("SinkService: Connected, hold pairing ACL: %04x,%02x,%06lx",
                            msg->bd_addr.nap, msg->bd_addr.uap, msg->bd_addr.lap);

            if (BdaddrIsZero(&sm->acl_hold_addr))
            {
                ConManagerCreateAcl(&msg->bd_addr);
                sm->acl_hold_addr = msg->bd_addr;
            }
        }
    }
        break;
    }
}

static void sinkServiceSm_StateConnectingBredrAclHandler(sink_service_state_machine_t * sm, MessageId id, Message message)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_StateConnectingBredrAclHandler");

    switch(id)
    {
    case CON_MANAGER_CONNECTION_IND:
    {
        DEBUG_LOG_DEBUG("sinkServiceSm_StateConnectingBredrAclHandler: CON_MANAGER_CONNECTION_IND");

        const CON_MANAGER_CONNECTION_IND_T *msg = message;
        if (msg->ble || !appDeviceTypeIsSink(&msg->bd_addr) || !ConManagerIsAclLocal(&msg->bd_addr))
        {
            DEBUG_LOG_DEBUG("SinkService: Ignore connections from non-sink devices");

            /* Ignore connections from non-sink devices (outside of pairing). These
                   are tracked separately by other components (e.g. handset service). */
            return;
        }

        device_t device = BtDevice_GetDeviceForBdAddr(&msg->bd_addr);
        if (msg->connected)
        {
            DEBUG_LOG_INFO("SinkService: Connected to 0x%04x 0x%02x 0x%06lx",
                              msg->bd_addr.nap,
                              msg->bd_addr.uap,
                              msg->bd_addr.lap);
            sm->sink_device = device;

            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTING_PROFILES);
        }
        else
        {
            DEBUG_LOG_INFO("SinkService: Disconnected from 0x%04x 0x%02x 0x%06lx",
                              msg->bd_addr.nap,
                              msg->bd_addr.uap,
                              msg->bd_addr.lap);

            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
    break;
    case SINK_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE:
    {
        /* If we receive this internal message but the are not connected it means paging failed */
        if (ConManagerIsConnected(&sm->acl_hold_addr))
        {
            DEBUG_LOG_INFO("SinkService: Connected to 0x%04x 0x%02x 0x%06lx",
                              sm->acl_hold_addr.nap,
                              sm->acl_hold_addr.uap,
                              sm->acl_hold_addr.lap);
            device_t device = BtDevice_GetDeviceForBdAddr(&sm->acl_hold_addr);
            sm->sink_device = device;

            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTING_PROFILES);
        }
        else
        {
            DEBUG_LOG_INFO("SinkService: Failed to connect to 0x%04x 0x%02x 0x%06lx",
                              sm->acl_hold_addr.nap,
                              sm->acl_hold_addr.uap,
                              sm->acl_hold_addr.lap);
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
    break;
}
}

static void sinkServiceSm_StateConnectingProfilesHandler(sink_service_state_machine_t * sm, MessageId id, Message message)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_StateConnectingProfilesHandler");

    switch(id)
    {
    case CONNECT_PROFILES_CFM:
    {
        const CONNECT_PROFILES_CFM_T *msg = message;
        DEBUG_LOG_DEBUG("sinkServiceSm_StateConnectingProfilesHandler: CONNECT_PROFILES_CFM status:enum:profile_manager_request_cfm_result_t:%d ",
                       msg->result);
        if (msg->result == profile_manager_success)
        {
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTED);
        }
        else
        {
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
        break;

    case CONNECTED_PROFILE_IND:
    {
        const CONNECTED_PROFILE_IND_T *msg = message;

        uint32 connected_profiles = sinkServiceSm_GetConnectedProfiles(sm);

        /* Check if only one profile is connected */
        if (OnlyOneProfileConnected(connected_profiles))
        {
            DEBUG_LOG("sinkServiceSm_StateConnectingProfilesHandler: First Profile connected");

            SinkService_SendFirstProfileConnectedIndNotification(sm->sink_device);
        }

        /* Check if all profiles are connected. */
        if(sinkServiceSm_AreAllRequestedProfilesConnected(sm))
        {
            DEBUG_LOG_ALWAYS("sinkServiceSm_StateConnectingProfilesHandler remote no more profile(s) to connect");

            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTED);
        }
        else
        {
            sinkServiceSm_RestartProfileConnectionTimeout(sm);
        }

        /* Do not request this profile again until a new connection is made */
        sm->profiles_requested &= ~msg->profile;
    }
        break;

    case SINK_SERVICE_INTERNAL_CONNECT_PROFILES_REQ:
    {
        if (sinkServiceSm_AreAllRequestedProfilesConnected(sm))
        {
            DEBUG_LOG_INFO("Sink Service: Profile connection timer elapsed. All profiles connected. Move to connected state");
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTED);
        }
        else
        {
            DEBUG_LOG_INFO("Sink Service: Profile connection timer elapsed. Connecting remaining profiles");
            if (!sinkServiceSm_ConnectRequestedProfiles(sm))
            {
                sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
            }
        }
    }
        break;

    case DISCONNECT_PROFILES_CFM:
        DEBUG_LOG_DEBUG("sinkServiceSm_StateConnectingProfilesHandler: DISCONNECT_PROFILES_CFM");
        break;

    case DISCONNECTED_PROFILE_IND:
        DEBUG_LOG_DEBUG("sinkServiceSm_StateConnectingProfilesHandler: DISCONNECTED_PROFILE_IND");
        break;
    case CON_MANAGER_CONNECTION_IND:
    {
        const CON_MANAGER_CONNECTION_IND_T *msg = message;
        if (!msg->connected)
        {
            DEBUG_LOG_INFO("SinkService: Disconnected from 0x%04x 0x%02x 0x%06lx",
                           msg->bd_addr.nap,
                           msg->bd_addr.uap,
                           msg->bd_addr.lap);

            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
        break;
    default:
        break;
    }
}

static void sinkServiceSm_StateConnectedHandler(sink_service_state_machine_t * sm, MessageId id, Message message)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_StateConnectedHandler");

    UNUSED(message);
    switch(id)
    {
    case CON_MANAGER_CONNECTION_IND:
    {
        DEBUG_LOG_DEBUG("sinkServiceSm_StateConnectedHandler: CON_MANAGER_CONNECTION_IND");
        const CON_MANAGER_CONNECTION_IND_T *msg = message;


        if (sm->sink_device != BtDevice_GetDeviceForBdAddr(&msg->bd_addr))
        {
            /* Ignore connection messages for devices not belonging to this instance */
            return;
        }

        if (!msg->connected)
        {
            DEBUG_LOG_INFO("SinkService: Disconnected from 0x%04x 0x%02x 0x%06lx",
                           msg->bd_addr.nap,
                           msg->bd_addr.uap,
                           msg->bd_addr.lap);
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
        break;

    default:
        break;
    }
}

static void sinkServiceSm_StateDisabledHandler(sink_service_state_machine_t * sm, MessageId id, Message message)
{
    UNUSED(sm); UNUSED(id); UNUSED(message);
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_StateDisabledHandler");
}

/* State exit functions */

static void sinkServiceSm_ExitDisconnected(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ExitDisconnected %d", sm);
    ConManagerRegisterConnectionsClient(sinkServiceSm_GetTaskForSm(sm));
    SinkService_ConnectableEnableBredr(FALSE);
}

static void sinkServiceSm_ExitConnectingBredrAcl(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ExitConnectingBredrAcl %d", sm);
}

static void sinkServiceSm_ExitPairing(sink_service_state_machine_t *sm)
{
    /* Nothing to do here. ACL is held for connecting profiles */
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ExitPairing %d", sm);
}

static void sinkServiceSm_ExitConnectingProfiles(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ExitConnectingProfiles %d", sm);
    /* Moved out of connecting profiles. Timer for profile connection should be cancelled. */
    sinkServiceSm_CancelProfileConnectionTimeout(sm);
}

static void sinkServiceSm_ExitConnected(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ExitConnected %d", sm);
}

static void sinkServiceSm_ExitDisabled(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_ExitDisabled %d", sm);
}

/* State enter functions */

static void sinkServiceSm_EnterDisconnected(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_EnterDisconnected %p", sm);

    /* Release the ACL */
    sinkServiceSm_ReleaseAcl(sm);

    /* send a Disconnect confirmation if we were connected previously */
    if ( sm->sink_device )  /* this will be cleared further down */
    {
        SinkService_SendDisconnectedCfm(sm->sink_device);
    }

    /* If the device attached to the SM is not paired. Delete it. */
    sinkServiceSm_DeleteDeviceIfNotPaired(sm);

    SinkService_ConnectableEnableBredr(TRUE);

    ProfileManager_ClientUnregister(sinkServiceSm_GetTaskForSm(sm));
    ConManagerUnregisterConnectionsClient(sinkServiceSm_GetTaskForSm(sm));

    /* Reset the State Machine */
    SinkServiceSm_ClearInstance(sm);

    /* Check if we need to enter the DISABLED state */
    if (!SinkService_IsEnabled())
    {
        sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISABLED);
        SinkService_GetTaskData()->pairing_request_pending = FALSE;
    }
    /* Check if there is a pairing request pending to enter the PAIRING state. */
    else if (SinkService_GetTaskData()->pairing_request_pending)
    {
        SinkServiceSm_PairRequest(SinkService_GetTaskData()->state_machines);
        SinkService_GetTaskData()->pairing_request_pending = FALSE;
    }
}

static void sinkServiceSm_EnterConnectingBredrAcl(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_EnterConnectingBredrAcl");

    device_t sink_device = sinkServiceSm_DetermineSinkDevice();
    sm->acl_hold_addr = DeviceProperties_GetBdAddr(sink_device);
    MessageSendConditionally(sinkServiceSm_GetTaskForSm(sm), SINK_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE,
                             NULL, ConManagerCreateAcl(&sm->acl_hold_addr));

}

static void sinkServiceSm_EnterPairing(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_EnterPairing");

    DEBUG_LOG_INFO("SinkService: Starting RSSI Pairing...");

    if(!RssiPairing_Start(sinkServiceSm_GetTaskForSm(sm),
                          SinkServiceConfig_GetConfig()->rssi_pairing_params))
    {
        DEBUG_LOG_ERROR("SinkService: Failed to start RSSI Pairing");
        SinkService_SendConnectedCfm(sm->sink_device, sink_service_status_failed);
        sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        return;
    }
}

static void sinkServiceSm_EnterConnectingProfiles(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_EnterConnectingProfiles");

    ProfileManager_ClientRegister(sinkServiceSm_GetTaskForSm(sm));

    /* Request all profiles configured */
    sm->profiles_requested = sinkServiceSm_ConvertProfileListToBitmask(SinkServiceConfig_GetConfig()->profile_list,
                                                                       SinkServiceConfig_GetConfig()->profile_list_length);

    /* Device is not paired. There is no point trying to connect profiles */
    if (!sinkServiceSm_DeviceIsPaired(sm))
    {
        return;
    }

    bdaddr device_addr = DeviceProperties_GetBdAddr(sm->sink_device);
    if (ConManagerIsAclLocal(&device_addr))
    {
        /* The connection was made by the Source. Attempt to connect all requested profiles */
        if (!sinkServiceSm_ConnectRequestedProfiles(sm))
        {
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
    else
    {
        DEBUG_LOG_INFO("Sink Service: Sink initiated connection. Starting timer for connecting profiles");

        /* Delay profile connections to allow handset to initiate remotely first */
        sinkServiceSm_CancelProfileConnectionTimeout(sm);
        sinkServiceSm_StartProfileConnectionTimeout(sm);
    }

}

static void sinkServiceSm_EnterConnected(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_EnterConnected %d", sm);

    /* sm is connected, release the ACL */
    sinkServiceSm_ReleaseAcl(sm);
    SinkService_SendConnectedCfm(sm->sink_device, sink_service_status_success);
}

static void sinkServiceSm_EnterDisabled(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("sinkServiceSm_EnterDisabled %d", sm);
}

static void sinkServiceSm_SetState(sink_service_state_machine_t *sm, sink_service_state_t state)
{
    sink_service_state_t old_state = sm->state;

    DEBUG_LOG_STATE("SinkService: enum:sink_service_state_t:%d -> "
                    "enum:sink_service_state_t:%d", old_state, state);
    /* Handle state exit functions */
    switch (old_state)
    {
    case SINK_SERVICE_STATE_DISCONNECTED:
        sinkServiceSm_ExitDisconnected(sm);
        break;
    case SINK_SERVICE_STATE_CONNECTING_BREDR_ACL:
        sinkServiceSm_ExitConnectingBredrAcl(sm);
        break;
    case SINK_SERVICE_STATE_PAIRING:
        sinkServiceSm_ExitPairing(sm);
        break;
    case SINK_SERVICE_STATE_CONNECTING_PROFILES:
        sinkServiceSm_ExitConnectingProfiles(sm);
        break;
    case SINK_SERVICE_STATE_CONNECTED:
        sinkServiceSm_ExitConnected(sm);
        break;
    case SINK_SERVICE_STATE_DISABLED:
        sinkServiceSm_ExitDisabled(sm);
        break;
    default:
        break;
    }

    /* Set new state */
    sm->state = state;

    /* Handle state entry functions */
    switch (sm->state)
    {
    case SINK_SERVICE_STATE_DISCONNECTED:
        sinkServiceSm_EnterDisconnected(sm);
        break;
    case SINK_SERVICE_STATE_CONNECTING_BREDR_ACL:
        sinkServiceSm_EnterConnectingBredrAcl(sm);
        break;
    case SINK_SERVICE_STATE_PAIRING:
        sinkServiceSm_EnterPairing(sm);
        break;
    case SINK_SERVICE_STATE_CONNECTING_PROFILES:
        sinkServiceSm_EnterConnectingProfiles(sm);
        break;
    case SINK_SERVICE_STATE_CONNECTED:
        sinkServiceSm_EnterConnected(sm);
        break;
    case SINK_SERVICE_STATE_DISABLED:
        sinkServiceSm_EnterDisabled(sm);
        break;
    default:
        break;
    }
    SinkService_UpdateUi();
}


static void sinkServiceSm_HandleMessage(Task task, MessageId id, Message message)
{
    sink_service_state_machine_t *sm = sinkServiceSm_GetSmFromTask(task);

    DEBUG_LOG_DEBUG("sinkService_HandleMessage state:enum:sink_service_state_t:%d",
                   sinkServiceSm_GetStateForSm(sm));

    switch (sm->state)
    {
    case SINK_SERVICE_STATE_PAIRING:
        sinkServiceSm_StatePairingHandler(sm, id, message);
        break;
    case SINK_SERVICE_STATE_CONNECTING_BREDR_ACL:
        sinkServiceSm_StateConnectingBredrAclHandler(sm, id, message);
        break;
    case SINK_SERVICE_STATE_CONNECTING_PROFILES:
        sinkServiceSm_StateConnectingProfilesHandler(sm, id, message);
        break;
    case SINK_SERVICE_STATE_CONNECTED:
        sinkServiceSm_StateConnectedHandler(sm, id, message);
        break;
    case SINK_SERVICE_STATE_DISABLED:
        sinkServiceSm_StateDisabledHandler(sm, id, message);
        break;
    default:
        UnexpectedMessage_HandleMessage(id);
        break;

    }
}


/*
    Public functions
*/

sink_service_state_machine_t *sinkServiceSm_CreateSm(device_t device)
{
    sink_service_state_machine_t *new_sm = NULL;

    FOR_EACH_SINK_SM(sm)
    {
        if (sm->state == SINK_SERVICE_STATE_DISCONNECTED)
        {
            new_sm = sm;
            SinkServiceSm_ClearInstance(new_sm);
            SinkServiceSm_SetDevice(new_sm, device);
            break;
        }
    }

    return new_sm;
}

sink_service_state_machine_t *sinkServiceSm_GetSmForDevice(device_t device)
{
    sink_service_state_machine_t *sm_match = NULL;

    FOR_EACH_SINK_SM(sm)
    {
        if ((sm->state > SINK_SERVICE_STATE_DISCONNECTED)
            && (sm->sink_device == device))
        {
            sm_match = sm;
            break;
        }
    }

    return sm_match;
}

sink_service_state_machine_t *sinkServiceSm_GetSmForBredrAddr(const bdaddr *addr)
{
    sink_service_state_machine_t *sm_match = NULL;

    DEBUG_LOG_VERBOSE("sinkService_GetSmForBredrAddr Searching for addr [%04x,%02x,%06lx]",
                        addr->nap, addr->uap, addr->lap);

    FOR_EACH_SINK_SM(sm)
    {
        if (sm->sink_device)
        {
            bdaddr bredr_bdaddr;
            bredr_bdaddr = DeviceProperties_GetBdAddr(sm->sink_device);

            DEBUG_LOG_VERBOSE("sinkService_GetSmForBredrAddr Check SM [%p] state [%d] addr [%04x,%02x,%06lx]",
                              sm, sm->state, bredr_bdaddr.nap, bredr_bdaddr.uap, bredr_bdaddr.lap);

            if ((sm->state > SINK_SERVICE_STATE_DISCONNECTED)
                && BdaddrIsSame(&bredr_bdaddr, addr))
            {
                sm_match = sm;
                break;
            }
        }
    }

    return sm_match;
}

sink_service_state_machine_t *sinkServiceSm_FindOrCreateSm(const tp_bdaddr *tp_addr)
{
    sink_service_state_machine_t *sm = NULL;

    if(tp_addr->transport == TRANSPORT_BREDR_ACL)
    {
        /* First try to match the BR/EDR address */
        sm = sinkServiceSm_GetSmForBredrAddr(&tp_addr->taddr.addr);
        if (!sm)
        {
            /* Second try to match the device_t handle */
            device_t dev = BtDevice_GetDeviceForBdAddr(&tp_addr->taddr.addr);
            if (dev)
            {
                sm = sinkServiceSm_GetSmForDevice(dev);
            }
        }
        /* No sm has been found. Create one */
        if(!sm)
        {
                if(!SinkServiceSm_MaxBredrAclConnectionsReached())
                {
                    device_t device = BtDevice_GetDeviceForBdAddr(&tp_addr->taddr.addr);
                    sm = sinkServiceSm_CreateSm(device);
                }
        }
    }
    else
    {
        DEBUG_LOG_DEBUG("sinkService_GetSmForTpBdAddr Unsupported transport type enum:TRANSPORT_T:%d", tp_addr->transport);
    }

    return sm;
}

void SinkServiceSm_SetDevice(sink_service_state_machine_t *sm, device_t device)
{
    if (sm)
    {
        sm->sink_device = device;
    }
}

void SinkServiceSm_ClearInstance(sink_service_state_machine_t *sm)
{
    PanicNull(sm);

    memset(sm, 0, sizeof(*sm));
    sm->state = SINK_SERVICE_STATE_DISCONNECTED;
    sm->task_data.handler = sinkServiceSm_HandleMessage;
}

bool SinkServiceSm_ConnectRequest(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("SinkServiceSm_ConnectRequest");

    /* if the device is empty then move to pair */
    sm->sink_device = sinkServiceSm_DetermineSinkDevice();
    if (sm->sink_device == NULL)
    {
        DEBUG_LOG_INFO("SinkService: No paired devices");

        if (SinkService_IsPairingEnabled())
        {
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_PAIRING);
        }
        else
        {
            DEBUG_LOG_WARN("SinkService: Pairing disabled, ignoring request");
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG_INFO("SinkService: Connecting to existing device...");
        sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTING_BREDR_ACL);
    }
    return TRUE;
}

bool SinkServiceSm_PairRequest(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("SinkServiceSm_PairRequest");

    if (SinkService_IsPairingEnabled())
    {
        sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_PAIRING);
    }
    else
    {
        DEBUG_LOG_WARN("SinkService: Pairing disabled, ignoring request");
        return FALSE;
    }

    return TRUE;
}

bool SinkServiceSm_DisconnectRequest(sink_service_state_machine_t *sm)
{
    DEBUG_LOG_FN_ENTRY("SinkServiceSm_DisconnectRequest");

    if (sinkServiceSm_GetStateForSm(sm) <= SINK_SERVICE_STATE_DISCONNECTED )
    {
        return FALSE;
    }

    /* If there is no device registered with the instance then disregard the disconnect request.
       This is normally due to the state of the instance being SINK_SERVICE_STATE_PAIRING */
    if (!sm->sink_device)
    {
        DEBUG_LOG("SinkServiceSm_DisconnectRequest: No device registered with instance. Ignore request.");
        return FALSE;
    }

    Device_SetProperty(sm->sink_device,
                       device_property_profiles_disconnect_order,
                       SinkServiceConfig_GetConfig()->profile_list,
                       SinkServiceConfig_GetConfig()->profile_list_length);

    return ProfileManager_DisconnectProfilesRequest(sinkServiceSm_GetTaskForSm(sm), sm->sink_device);
}

/*! \brief Handle a sink initiated ACL connection.

    This represents an ACL connection that was initiated by the sink.

    Usually this will happen in a disconnected state, before any profiles have
    connected. In this case go directly to the BR/EDR connected state.

*/
void SinkServiceSm_HandleConManagerBredrTpConnectInd(sink_service_state_machine_t *sm,
    const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("SinkServiceSm_HandleConManagerBredrTpConnectInd enum:sink_service_state_t:%d device 0x%x enum:TRANSPORT_T:%d type %d [%04x,%02x,%06lx] ",
           sm->state,
           sm->sink_device,
           ind->tpaddr.transport,
           ind->tpaddr.taddr.type,
           ind->tpaddr.taddr.addr.nap,
           ind->tpaddr.taddr.addr.uap,
           ind->tpaddr.taddr.addr.lap);

    bdaddr device_address = DeviceProperties_GetBdAddr(sm->sink_device);
    PanicFalse(BdaddrIsSame(&device_address, &ind->tpaddr.taddr.addr));

    switch (sm->state)
    {
    case SINK_SERVICE_STATE_DISCONNECTED:
        ProfileManager_ClientRegister(sinkServiceSm_GetTaskForSm(sm));
        /* Sink has initiated a connection. Move to connecting profiles to connect profiles if Sink does not */
        sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_CONNECTING_PROFILES);
        break;

    case SINK_SERVICE_STATE_CONNECTING_BREDR_ACL:
        /* Although we are waiting for the ACL to connect, we use
           SINK_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE to detect when the ACL
           is connected. But if we were waiting to retry to connect the ACL
           after a connection failure, and the device connects, the complete
           message would not be received and nothing would happen, therefore
           send the complete message immediately. */
        if (MessageCancelFirst(&sm->task_data, SINK_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ))
        {
            MessageSend(&sm->task_data, SINK_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE, NULL);
        }
        break;

        /* Fall-through */
    case SINK_SERVICE_STATE_CONNECTING_PROFILES:
    case SINK_SERVICE_STATE_CONNECTED:
        /* Sink Service is already connected. This message can be ignored. */
        break;
    case SINK_SERVICE_STATE_DISABLED:
        /* ignore any connection requests, but we shouldn't be connectable here anyway */
        break;
    default:
        break;
    }
}

/*! \brief Count the number of active BR/EDR sink state machines */
unsigned SinkServiceSm_GetBredrAclConnectionCount(void)
{
    unsigned active_sm_count = 0;

    FOR_EACH_SINK_SM(sm)
    {
        if (sm->state > SINK_SERVICE_STATE_DISCONNECTED)
        {
            bdaddr device_address  = sm->acl_hold_addr;

            if (sm->sink_device != NULL)
            {
                device_address = DeviceProperties_GetBdAddr(sm->sink_device);
            }

            DEBUG_LOG_VERBOSE("SinkServiceSm_GetBredrAclConnectionCount Check state [%d] addr [%04x,%02x,%06lx]",
                              sm->state, device_address.nap, device_address.uap, device_address.lap);

            if(ConManagerIsConnected(&device_address))
            {
                active_sm_count++;
            }
        }
    }

    DEBUG_LOG_VERBOSE("SinkServiceSm_GetBredrAclConnectionCount %u", active_sm_count);

    return active_sm_count;
}

bool SinkServiceSm_MaxBredrAclConnectionsReached(void)
{
    unsigned num_bredr_connections = SinkServiceSm_GetBredrAclConnectionCount();
    unsigned max_bredr_connections = sinkService_BredrAclMaxConnections();

    DEBUG_LOG_DEBUG("SinkServiceSm_MaxBredrAclConnectionsReached  %u of %u BR/EDR connections", num_bredr_connections, max_bredr_connections);

    return num_bredr_connections >= max_bredr_connections;
}

void SinkServiceSm_DisableAll(void)
{
    FOR_EACH_SINK_SM(sm)
    {
        if ( sm->state == SINK_SERVICE_STATE_DISCONNECTED )
        {
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISABLED);
        }
    }
}

void SinkServiceSm_EnableAll(void)
{
    FOR_EACH_SINK_SM(sm)
    {
        if ( sm->state == SINK_SERVICE_STATE_DISABLED )
        {
            sinkServiceSm_SetState(sm, SINK_SERVICE_STATE_DISCONNECTED);
        }
    }
}
