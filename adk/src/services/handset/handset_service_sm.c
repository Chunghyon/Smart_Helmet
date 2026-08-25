/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service state machine
*/

#include <bdaddr.h>
#include <device.h>

#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <device_properties.h>
#include <focus_device.h>
#include <profile_manager.h>
#include <timestamp_event.h>
#include <ui_inputs.h>
#include <vm.h>

#include "handset_service_config.h"
#include "handset_service_connectable.h"
#include "handset_service_protected.h"
#include "handset_service_sm.h"
#include "handset_service.h"
#include "power_manager.h"


/*! \brief Cast a Task to a handset_service_state_machine_t.
    This depends on task_data being the first member of handset_service_state_machine_t. */
#define handsetServiceSm_GetSmFromTask(task) ((handset_service_state_machine_t *)task)

/*! \brief Test if the current state is in the "CONNECTING" pseudo-state. */
#define handsetServiceSm_IsConnectingBredrState(state) \
    ((state & HANDSET_SERVICE_CONNECTING_BREDR_STATE_MASK) == HANDSET_SERVICE_CONNECTING_BREDR_STATE_MASK)

/*! \brief Add one mask of profiles to another. */
#define handsetServiceSm_MergeProfiles(profiles, profiles_to_merge) \
    ((profiles) |= (profiles_to_merge))

/*! \brief Remove a set of profiles from another. */
#define handsetServiceSm_RemoveProfiles(profiles, profiles_to_remove) \
    ((profiles) &= ~(profiles_to_remove))

#define handsetServiceSm_ProfileIsSet(profiles, profile) \
    (((profiles) & (profile)) == (profile))

/*! \brief Check if the disconnect was requested by ourselves */
#define handsetService_IsDisconnectLocal(hci_reason) \
    ((hci_reason) == hci_error_conn_term_local_host)

/*! \brief The maximum length of device_property_profiles_disconnect_order.
    Currently room for 9 profiles and a terminator. */
#define PROFILE_LIST_LENGTH 10

/*! \brief The device profiles that imply the use of BR/EDR. */
#define BREDR_PROFILES  (DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP | DEVICE_PROFILE_AVRCP | DEVICE_PROFILE_HIDD | DEVICE_PROFILE_BTDBG)

/* Delay before requesting to connect the profiles. */
#define CONNECT_PROFILES_DELAY D_SEC(5)
/*
    Helper Functions
*/

device_t HandsetServiceSm_GetHandsetDeviceIfValid(handset_service_state_machine_t *sm)
{
    return BtDevice_DeviceIsValid(sm->handset_device) ? sm->handset_device
                                                      : NULL;
}

static uint32 handsetServiceSm_GetConnectedProfiles(handset_service_state_machine_t *sm)
{
    device_t handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    
    if(handset_device)
    {
        return BtDevice_GetConnectedProfiles(handset_device);
    }
    
    return 0;
}

static uint32 handsetServiceSm_GetRequestedProfilesNotConnected(handset_service_state_machine_t *sm)
{
    uint32 connected_profiles = handsetServiceSm_GetConnectedProfiles(sm);
    
    return sm->profiles_requested & ~connected_profiles;
}

static uint32 handsetServiceSm_GetSupportedProfilesNotConnected(handset_service_state_machine_t *sm)
{
    device_t handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    
    if(handset_device)
    {
        return BtDevice_GetSupportedProfilesNotConnected(handset_device);
    }
    
    return 0;
}

static bool handsetServiceSm_AreAllRequestedProfilesConnected(handset_service_state_machine_t *sm)
{
    if (HandsetServiceSm_IsBredrAclConnected(sm) && (handsetServiceSm_GetRequestedProfilesNotConnected(sm) == 0))
    {
        return TRUE;
    }

    return FALSE;
}

static bool handsetServiceSm_AreAllSupportedProfilesConnected(handset_service_state_machine_t *sm)
{
    if (HandsetServiceSm_IsBredrAclConnected(sm) && (handsetServiceSm_GetSupportedProfilesNotConnected(sm) == 0))
    {
        return TRUE;
    }

    return FALSE;
}

static bool handsetServiceSm_CancelProfileConnectionTimeout(handset_service_state_machine_t *sm)
{
    if(MessageCancelFirst(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_PROFILES_REQ))
    {
        HS_LOG("handsetServiceSm_CancelProfileConnectionTimeout");
        return TRUE;
    }
    return FALSE;
}

static void handsetServiceSm_StartProfileConnectionTimeout(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_StartProfileConnectionTimeout %dms", CONNECT_PROFILES_DELAY);
    MessageSendLater(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_PROFILES_REQ, NULL, CONNECT_PROFILES_DELAY);
}

static void handsetServiceSm_RestartProfileConnectionTimeout(handset_service_state_machine_t *sm)
{
    if(handsetServiceSm_CancelProfileConnectionTimeout(sm))
    {
        handsetServiceSm_StartProfileConnectionTimeout(sm);
    }
    else
    {
        HS_LOG("handsetServiceSm_RestartProfileConnectionTimeout - timeout not in progress");
    }
}

/*! Count the number of active BR/EDR handset state machines */
unsigned HandsetServiceSm_GetBredrAclConnectionCount(void)
{
    unsigned active_sm_count = 0;

    FOR_EACH_HANDSET_SM(sm)
    {
        DEBUG_LOG_VERBOSE("HandsetServiceSm_GetBredrAclConnectionCount Check state [%d] addr [%04x,%02x,%06lx]",
                          sm->state, sm->handset_addr.nap, sm->handset_addr.uap, sm->handset_addr.lap);

        if(HandsetServiceSm_IsBredrAclConnected(sm))
        {
            active_sm_count++;
        }
    }
    
    DEBUG_LOG_VERBOSE("HandsetServiceSm_GetBredrAclConnectionCount %u", active_sm_count);
    
    return active_sm_count;
}

/*! Count the number of active BR/EDR handset whose ACL and all supported profiles are connected */
unsigned HandsetServiceSm_GetFullyConnectedBredrHandsetCount(void)
{
    unsigned fully_connected_handset_count = 0;

    FOR_EACH_HANDSET_SM(sm)
    {
        DEBUG_LOG_VERBOSE("HandsetServiceSm_GetFullyConnectedBredrHandsetCount Check state [%d] addr [%04x,%02x,%06lx]",
                          sm->state, sm->handset_addr.nap, sm->handset_addr.uap, sm->handset_addr.lap);

        if (handsetServiceSm_AreAllSupportedProfilesConnected(sm))
        {
            fully_connected_handset_count++;
        }
    }

    DEBUG_LOG_VERBOSE("HandsetServiceSm_GetFullyConnectedBredrHandsetCount %u", fully_connected_handset_count);
    
    return fully_connected_handset_count;
}

/*! Count the number of active LE handset state machines */
unsigned HandsetServiceSm_GetLeAclConnectionCount(void)
{
    unsigned active_sm_count = 0;

    FOR_EACH_HANDSET_SM(sm)
    {
        if(HandsetServiceSm_IsLeAclConnected(sm))
        {
            active_sm_count++;
        }
    }
    
    DEBUG_LOG_VERBOSE("HandsetServiceSm_GetLeAclConnectionCount %u", active_sm_count);
    
    return active_sm_count;
}

/*! Count the number of active handset state machines with either BR/EDR or LE connected or connecting */
unsigned HandsetServiceSm_GetBredrOrLeAclConnectionCount(void)
{
    unsigned active_sm_count = 0;

    FOR_EACH_HANDSET_SM(sm)
    {
        if (   HandsetServiceSm_IsBredrAclConnected(sm)
            || HandsetServiceSm_IsLeAclConnected(sm)
            || handsetServiceSm_IsConnectingBredrState(sm->state))
        {
            active_sm_count++;
        }
    }

    DEBUG_LOG_VERBOSE("HandsetServiceSm_GetBredrOrLeAclConnectionCount %u", active_sm_count);

    return active_sm_count;
}

/*! \brief Convert a profile bitmask to Profile Manager profile connection list. */
static void handsetServiceSm_ConvertProfilesToProfileList(uint32 profiles, profile_t *profile_list, size_t profile_list_count)
{
    int entry = 0;
    profile_t pm_profile = profile_manager_hfp_profile;

    /* Loop over the profile manager profile_t enum values and if the matching
       profile mask from bt_device.h is set, add it to profile_list.

       Write up to (profile_list_count - 1) entries and leave space for the
       'last entry' marker at the end. */
    while ((pm_profile < profile_manager_max_number_of_profiles) && (entry < (profile_list_count - 1)))
    {
        switch (pm_profile)
        {
        case profile_manager_hfp_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_HFP))
            {
                profile_list[entry++] = profile_manager_hfp_profile;
            }
            break;

        case profile_manager_a2dp_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_A2DP))
            {
                profile_list[entry++] = profile_manager_a2dp_profile;
            }
            break;

        case profile_manager_avrcp_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_AVRCP))
            {
                profile_list[entry++] = profile_manager_avrcp_profile;
            }
            break;
#ifdef INCLUDE_HIDD_PROFILE
        case profile_manager_hidd_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_HIDD))
            {
                profile_list[entry++] = profile_manager_hidd_profile;
            }
            break;
#endif
        case profile_manager_ama_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_AMA))
            {
                profile_list[entry++] = profile_manager_ama_profile;
            }
            break;

        case profile_manager_gaa_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_GAA))
            {
                profile_list[entry++] = profile_manager_gaa_profile;
            }
            break;

        case profile_manager_gaia_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_GAIA))
            {
                profile_list[entry++] = profile_manager_gaia_profile;
            }
            break;

        case profile_manager_peer_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_PEER))
            {
                profile_list[entry++] = profile_manager_peer_profile;
            }
            break;

        case profile_manager_accessory_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_ACCESSORY))
            {
                profile_list[entry++] = profile_manager_accessory_profile;
            }
            break;

        case profile_manager_btdbg_profile:
            if (handsetServiceSm_ProfileIsSet(profiles, DEVICE_PROFILE_BTDBG))
            {
                profile_list[entry++] = profile_manager_btdbg_profile;
            }
            break;

        default:
            break;
        }

        pm_profile++;
    }

    /* The final entry in the list is the 'end of list' marker */
    profile_list[entry] = profile_manager_max_number_of_profiles;
}

bool HandsetServiceSm_AllConnectionsDisconnected(handset_service_state_machine_t *sm, bool bredr_only)
{
    bool bredr_connected = FALSE;
    bool ble_connected = FALSE;
    uint32 connected_profiles = handsetServiceSm_GetConnectedProfiles(sm);

    if (!BdaddrIsZero(&sm->handset_addr))
    {
        bredr_connected = ConManagerIsConnected(&sm->handset_addr);
    }

    if (!bredr_only)
    {
        ble_connected = HandsetServiceSm_IsLeAclConnected(sm);

        DEBUG_LOG("HandsetServiceSm_AllConnectionsDisconnected bredr %d profiles 0x%x le %d",
                  bredr_connected, connected_profiles, ble_connected);
    }
    else
    {
        DEBUG_LOG("HandsetServiceSm_AllConnectionsDisconnected bredr %d profiles 0x%x le Ignored (connected:%d)",
                  bredr_connected, connected_profiles, ble_connected);
    }

    return (!bredr_connected
            && (connected_profiles == 0)
            && !ble_connected
            );
}

/*  Helper to request a BR/EDR connection to the handset from connection manager. */
static void handsetService_ConnectAcl(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetService_ConnectAcl");

    ConManager_SetPageTimeout(handsetService_GetPageTimeout());

    /* Post message back to ourselves, blocked on creating ACL */
    MessageSendConditionally(&sm->task_data,
                             HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE,
                             NULL, ConManagerCreateAcl(&sm->handset_addr));

    sm->acl_create_called = TRUE;
    sm->acl_attempts++;
}

/*! \brief Get the client facing address for a state machine

    This function returns the bdaddr of the handset this state machine
    represents. Typically this would be the bdaddr a client uses to
    refer to this particular handset sm.

    A handset can be connected by BR/EDR and / or LE, so this function will
    first try to return the BR/EDR address but if that is invalid it will
    return the LE address.

    \param[in] sm Handset state machine to get the address for.
    \param[out] addr Client facing address of the handset sm.
*/
static void handsetServiceSm_GetBdAddr(handset_service_state_machine_t *sm, bdaddr *addr)
{
    if (!BdaddrIsZero(&sm->handset_addr))
    {
        *addr = sm->handset_addr;
    }
    else
    {
        *addr = HandsetServiceSm_GetLeTpBdaddr(sm).taddr.addr;
    }
}

static bool handsetServiceSm_BredrConnectionWasCompleted(handset_service_state_machine_t *sm, handset_service_state_t old_state)
{
    bool was_fully_connected = FALSE;
    if ((old_state == HANDSET_SERVICE_STATE_CONNECTED_BREDR) || 
        (old_state == HANDSET_SERVICE_STATE_DISCONNECTING_BREDR && !sm->connection_was_not_complete_at_disconnect_request))
    {
        was_fully_connected = TRUE;
    }
    return was_fully_connected;
}

static bool handsetServiceSm_BredrConnectionWasStopped(handset_service_state_machine_t *sm, handset_service_state_t old_state)
{
    bool was_stopped = FALSE;

    UNUSED(sm);

    if (old_state == HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES)
    {
        was_stopped = TRUE;
    }
    return was_stopped;
}

static void handsetServiceSm_ConnectRequestedProfiles(handset_service_state_machine_t *sm)
{
    device_t handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    if (handset_device)
    {
        if (handsetServiceSm_AreAllRequestedProfilesConnected(sm))
        {
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
        }
        else
        {
            profile_t profile_list[PROFILE_LIST_LENGTH];

            HS_LOG("handsetServiceSm_ConnectRequestedProfiles connect 0x%08x for device %p",
                   sm->profiles_requested,
                   handset_device);

            /* Connect the requested profiles.
               The requested profiles bitmask needs to be converted to the format of
               the profiles_connect_order device property and set on the device before
               calling profile manager to do the connect. */
            handsetServiceSm_ConvertProfilesToProfileList(sm->profiles_requested, profile_list, ARRAY_DIM(profile_list));

            Device_SetProperty(handset_device, device_property_profiles_connect_order, profile_list, sizeof(profile_list));

            PanicFalse(ProfileManager_ConnectProfilesRequest(&sm->task_data, handset_device));
        }
    }
    else
    {
        HS_LOG("handsetServiceSm_ConnectRequestedProfiles, can't connect profiles, device NULL");
    }

}

static bool handsetServiceSm_DeviceIsPaired(handset_service_state_machine_t *sm)
{
    uint16 flags = DEVICE_FLAGS_NO_FLAGS;
    
    appDeviceGetFlags(&sm->handset_addr, &flags);
    
    if(flags & DEVICE_FLAGS_NOT_PAIRED)
    {
        return FALSE;
    }
    return TRUE;
}

static void handsetServiceSm_DeleteDeviceIfNotPaired(handset_service_state_machine_t *sm)
{
    if(!handsetServiceSm_DeviceIsPaired(sm))
    {
        appDeviceDelete(&sm->handset_addr);
        HandsetServiceSm_SetDevice(sm, (device_t)0);
    }
}

/*
    State Enter & Exit functions.
*/

static void handsetServiceSm_EnterDisconnected(handset_service_state_machine_t *sm, handset_service_state_t old_state)
{
    bdaddr addr;
    
    bool all_sm_transports_disconnected = HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_AND_BLE);

    /* Notify registered clients of this disconnect event. */
    handsetServiceSm_GetBdAddr(sm, &addr);

    /* Complete any outstanding connect stop request */
    HandsetServiceSm_CompleteConnectStopRequests(sm, handset_service_status_disconnected);

    /* Complete any outstanding connect requests. */
    HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_failed);
    
    if(all_sm_transports_disconnected)
    {
        HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_success);
    }

    device_t handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    if (sm->disconnect_reason == hci_error_conn_timeout || sm->disconnect_reason == hci_error_lmp_response_timeout)
    {
        DeviceProperties_SetHandsetContext(handset_device, handset_context_link_loss);
        HandsetService_SendDisconnectedIndNotification(&addr, handset_service_status_link_loss);
    }
    else
    {
        if (DeviceProperties_GetHandsetContext(handset_device) != handset_context_reconnecting)
        {
            DeviceProperties_SetHandsetContext(handset_device, handset_context_disconnected);
        }
        else
        {
            DeviceProperties_SetHandsetContext(handset_device, handset_context_link_loss);
        }

        /* Don't send a disconnected indication for an intentional disconnect if we hadn't yet fully
           established the connection with the device (i.e. if not yet completed profile connection). */
        if (handsetServiceSm_BredrConnectionWasCompleted(sm, old_state))
        {
            HandsetService_SendDisconnectedIndNotification(&addr, handset_service_status_disconnected);
        }
        else if (handsetServiceSm_BredrConnectionWasStopped(sm, old_state))
        {
            HandsetService_SendDisconnectedIndNotification(&addr, handset_service_status_cancelled);
        }
    }
    /* clear the connection status flag after sending the disconnected indication. */
    sm->connection_was_not_complete_at_disconnect_request = FALSE;
    
    handsetServiceSm_DeleteDeviceIfNotPaired(sm);
    
    HandsetServiceSm_EnableConnectableIfMaxConnectionsNotActive();
    
    /* If there are no open connections to this handset, destroy this state machine. */
    if (all_sm_transports_disconnected)
    {
        HS_LOG("handsetServiceSm_EnterDisconnected destroying sm for dev 0x%x", sm->handset_device);
        HandsetServiceSm_DeInit(sm);
    }
}


static void handsetServiceSm_EnterConnectingBredrAcl(handset_service_state_machine_t *sm)
{
    handsetService_ConnectAcl(sm);

    device_t handset = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    handset_context_t context = handset_context_connecting;
    handset_context_t current_context = DeviceProperties_GetHandsetContext(handset);
    if (current_context == handset_context_link_loss) {
        context = handset_context_reconnecting;
    }
    DeviceProperties_SetHandsetContext(handset, context);
}

static void handsetServiceSm_ExitConnectingBredrAcl(handset_service_state_machine_t *sm)
{
    /* Cancel any queued internal ACL connect retry requests */
    MessageCancelAll(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ);

    /* Reset ACL connection attempt count. */
    sm->acl_attempts = 0;
}

static void handsetServiceSm_EnterConnectingBredrProfiles(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_EnterConnectingBredrProfiles connect 0x%08x enum:handset_service_state_t:%d addr [%04x,%02x,%06lx]",
           sm->profiles_requested,
           sm->state,
           sm->handset_addr.nap,
           sm->handset_addr.uap,
           sm->handset_addr.lap);

    /* Request performance profile during profile connection */
    appPowerPerformanceProfileRequest();

    /* Don't request any profiles that have already been connected */
    sm->profiles_requested = handsetServiceSm_GetRequestedProfilesNotConnected(sm);
    
    if(ConManagerIsAclLocal(&sm->handset_addr))
    {
        /* Start profile connections immediately if the ACL was locally initiated */
        handsetServiceSm_ConnectRequestedProfiles(sm);
    }
    else
    {
        /* Delay profile connections to allow handset to initiate remotely first */
        handsetServiceSm_CancelProfileConnectionTimeout(sm);
        handsetServiceSm_StartProfileConnectionTimeout(sm);
        
        /* Attempt to connect supported profiles if there were no profiles requested locally */
        if(sm->profiles_requested == 0)
        {
            sm->profiles_requested = handsetServiceSm_GetSupportedProfilesNotConnected(sm);
        }
    }

    HandsetServiceSm_DisableConnectableIfMaxConnectionsActive();

    DeviceProperties_SetHandsetContext(HandsetServiceSm_GetHandsetDeviceIfValid(sm), handset_context_acl_connected);
}

static void handsetServiceSm_EnterStopConnectingBredrProfiles(handset_service_state_machine_t *sm)
{
    bool acl_is_connected = FALSE;
    device_t cached_handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    
    if (cached_handset_device)
    {
        if(ProfileManager_StopConnectProfilesRequest(&sm->task_data, cached_handset_device))
        {
            HS_LOG("handsetServiceSm_EnterStopConnectingBredrProfiles enum:handset_service_state_t:%d addr [%04x,%02x,%06lx]",
                   sm->state,
                   sm->handset_addr.nap,
                   sm->handset_addr.uap,
                   sm->handset_addr.lap);
            return;
        }
        
        acl_is_connected = ConManagerIsConnected(&sm->handset_addr);
    }
    
    HS_LOG("handsetServiceSm_EnterStopConnectingBredrProfiles no profile connection to stop, ACL connected %d", acl_is_connected);
    
    if(acl_is_connected)
    {
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
    }
    else
    {
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
    }
}

static void handsetServiceSm_ExitConnectingBredrProfiles(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_ExitConnectingBredrProfiles enum:handset_service_state_t:%d", sm->state);

    /* Relinquish perform profile now */
    appPowerPerformanceProfileRelinquish();

    handsetServiceSm_CancelProfileConnectionTimeout(sm);
}

static void handsetServiceSm_ExitStopConnectingBredrProfiles(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_ExitStopConnectingBredrProfiles enum:handset_service_state_t:%d", sm->state);
    MessageCancelFirst(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ);
}

/* Enter the CONNECTING pseudo-state */
static void handsetServiceSm_EnterConnectingBredr(handset_service_state_machine_t *sm)
{
    sm->acl_create_called = FALSE;
}

/* Exit the CONNECTING pseudo-state */
static void handsetServiceSm_ExitConnectingBredr(handset_service_state_machine_t *sm)
{
    if (sm->acl_create_called)
    {
        /* We have finished (successfully or not) attempting to connect, so
        we can relinquish our lock on the ACL.  Bluestack will then close
        the ACL when there are no more L2CAP connections */
        ConManagerReleaseAcl(&sm->handset_addr);
    }
}

static void handsetServiceSm_EnterConnectedBredr(handset_service_state_machine_t *sm)
{
    device_t handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    uint32 connected_profiles = handsetServiceSm_GetConnectedProfiles(sm);

    /* Complete any outstanding stop connect request */
    HandsetServiceSm_CompleteConnectStopRequests(sm, handset_service_status_connected);

    /* Complete outstanding connect requests */
    HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_success);

    /* Complete any outstanding disconnect requests. */
    HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_failed);

    /* Notify registered clients about this connection */
    HandsetService_SendConnectedIndNotification(handset_device, connected_profiles);

    HandsetServiceSm_DisableConnectableIfMaxConnectionsActive();

    DeviceProperties_SetHandsetContext(handset_device, handset_context_profiles_connected);
}

static void handsetServiceSm_TryDisconnectingBredr(handset_service_state_machine_t *sm)
{
    uint32 profiles_connected_or_requested = sm->profiles_requested | handsetServiceSm_GetConnectedProfiles(sm);
    device_t cached_handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);

    DEBUG_LOG("handsetServiceSm_TryDisconnectingBredr device %p profiles connected or requested 0x%x",
              cached_handset_device, profiles_connected_or_requested);

    /* Disconnect any profiles that were either requested or are currently connected. */
    if(cached_handset_device)
    {
        if(profiles_connected_or_requested)
        {
            uint32 profiles_to_disconnect = profiles_connected_or_requested & ~sm->disconnection_profiles_excluded;

            HS_LOG("handsetServiceSm_TryDisconnectingBredr to_disconnect 0x%x, excluded 0x%x",
                    profiles_to_disconnect, sm->disconnection_profiles_excluded);

            if (profiles_to_disconnect)
            {
                profile_t profile_list[PROFILE_LIST_LENGTH];

                handsetServiceSm_ConvertProfilesToProfileList(profiles_to_disconnect, profile_list, ARRAY_DIM(profile_list));

                Device_SetProperty(cached_handset_device, device_property_profiles_disconnect_order, profile_list, sizeof(profile_list));
                PanicFalse(ProfileManager_DisconnectProfilesRequest(&sm->task_data, cached_handset_device));
                /* Do not request disconnect these profiles again until a new connection is made */
                handsetServiceSm_RemoveProfiles(sm->profiles_requested, profiles_to_disconnect);
            }
            else
            {
                sm->disconnection_profiles_excluded = 0;
                /* All profiles requested to be disconnect are disconnected */
                HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_success);
                /* Return to connected state as we still have BR/EDR profiles connected */
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
            }
        }
        else
        {
            sm->disconnection_profiles_excluded = 0;
            bool acl_is_connected = ConManagerIsConnected(&sm->handset_addr);
            HS_LOG("handsetServiceSm_TryDisconnectingBredr ACL connected %d", acl_is_connected);

            if(acl_is_connected)
            {
                /* No profiles connected, close the ACL */
                ConManagerSendCloseAclRequest(&sm->handset_addr, TRUE);
            }
            else
            {
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
    }
    else
    {
        /* If the cached_device is invalid, then we never sent a Connect Profiles request,
        it is therefore safe to go straight to the disconnected state. */
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
    }
}

static void handsetServiceSm_EnterDisconnectingBredr(handset_service_state_machine_t *sm)
{
    DEBUG_LOG("handsetServiceSm_EnterDisconnectingBredr");
    handsetServiceSm_TryDisconnectingBredr(sm);
}

static void handsetServiceSm_ExitDisconnectingBredr(handset_service_state_machine_t *sm, handset_service_state_t new_state)
{
    if(new_state == HANDSET_SERVICE_STATE_DISCONNECTED)
    {
        HandsetService_BleDisconnectIfConnected(&sm->ble_sm);
    }
}


/*
    Public functions
*/

void HandsetServiceSm_SetDevice(handset_service_state_machine_t *sm, device_t device)
{
    if (sm)
    {
        if (device)
        {
            sm->handset_addr = DeviceProperties_GetBdAddr(device);
        }
        else
        {
            BdaddrSetZero(&sm->handset_addr);
        }
        sm->handset_device = device;
    }
}

/* */
void HandsetServiceSm_SetState(handset_service_state_machine_t *sm, handset_service_state_t state)
{
    handset_service_state_t old_state = sm->state;

    /* It is not valid to re-enter the same state */
    assert(old_state != state);

    DEBUG_LOG_STATE("HandsetServiceSm_SetState 0x%p enum:handset_service_state_t:%d -> enum:handset_service_state_t:%d", sm, old_state, state);

    /* Handle state exit functions */
    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_NULL:
    case HANDSET_SERVICE_STATE_DISCONNECTED:
    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        break;
    
    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        handsetServiceSm_ExitDisconnectingBredr(sm, state);
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        handsetServiceSm_ExitConnectingBredrAcl(sm);
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        handsetServiceSm_ExitConnectingBredrProfiles(sm);
        break;

    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
        handsetServiceSm_ExitStopConnectingBredrProfiles(sm);
        break;
    }

    /* Check for a exit transition from the CONNECTING pseudo-state */
    if (handsetServiceSm_IsConnectingBredrState(old_state) && !handsetServiceSm_IsConnectingBredrState(state))
    {
        handsetServiceSm_ExitConnectingBredr(sm);
    }

    /* Set new state */
    sm->state = state;

    /* Check for a transition to the CONNECTING pseudo-state */
    if (!handsetServiceSm_IsConnectingBredrState(old_state) && handsetServiceSm_IsConnectingBredrState(state))
    {
        handsetServiceSm_EnterConnectingBredr(sm);
    }

    /* Handle state entry functions */
    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        if (old_state != HANDSET_SERVICE_STATE_NULL)
        {
            handsetServiceSm_EnterDisconnected(sm, old_state);
        }
        break;
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        handsetServiceSm_EnterConnectingBredrAcl(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        handsetServiceSm_EnterConnectingBredrProfiles(sm);
        break;
    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
        handsetServiceSm_EnterStopConnectingBredrProfiles(sm);
        break;
    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        handsetServiceSm_EnterConnectedBredr(sm);
        break;
    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        handsetServiceSm_EnterDisconnectingBredr(sm);
        break;
    case HANDSET_SERVICE_STATE_NULL:
        /* NULL state is only "entered" when resetting a sm */
        DEBUG_LOG_ERROR("HandsetServiceSm_SetState. Attempt to enter NULL state");
        Panic();
        break;
    }
}

/*
    Message handler functions
*/

static void handsetServiceSm_HandleInternalConnectReq(handset_service_state_machine_t *sm,
    const HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T *req)
{
    HS_LOG("handsetServiceSm_HandleInternalConnectReq state enum:handset_service_state_t:%d device 0x%p profiles 0x%x",
           sm->state, req->device, req->profiles);

    /* Confirm requested addr is actually for this instance. */
    assert(HandsetServiceSm_GetHandsetDeviceIfValid(sm) == req->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR: /* Allow a new connect req to cancel an in-progress disconnect. */
        {
            bdaddr handset_addr = sm->handset_addr;

            HS_LOG("handsetServiceSm_HandleInternalConnectReq bdaddr %04x,%02x,%06lx",
                    handset_addr.nap, handset_addr.uap, handset_addr.lap);

            /* Store profiles to be connected */
            sm->profiles_requested = req->profiles;

            if (ConManagerIsConnected(&handset_addr))
            {
                HS_LOG("handsetServiceSm_HandleInternalConnectReq, ACL connected");

                if (sm->profiles_requested)
                {
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES);
                }
                else
                {
                    HS_LOG("handsetServiceSm_HandleInternalConnectReq, no profiles to connect");
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
                }
            }
            else
            {
                HS_LOG("handsetServiceSm_HandleInternalConnectReq, ACL not connected, attempt to open ACL");
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        /* Already connecting ACL link - nothing more to do but wait for that to finish. */
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        /* Profiles already being connected.
           TBD: Too late to merge new profile mask with in-progress one so what to do? */
        break;

    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
        /* In the middle of stopping the profile(s) connection for given handset. Therefore
           once in stable state the connect requester will be informed with the status. */
        break;

    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        /* Check requested profiles are all connected;
           if not go back to connecting the missing ones */
        {
            uint32 connected_profiles = handsetServiceSm_GetConnectedProfiles(sm);
            if((connected_profiles & req->profiles) != req->profiles)
            {
                sm->profiles_requested |= (req->profiles & ~connected_profiles);
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES);

                /* let Multipoint sm connect next handset if any.*/
                HandsetServiceMultipointSm_SetStateToGetNextDevice();
            }
            else
            {
                /* Already connected, so complete the request immediately. */
                HandsetServiceSm_CompleteConnectRequests(sm, handset_service_status_success);

                DeviceProperties_SetHandsetContext(HandsetServiceSm_GetHandsetDeviceIfValid(sm), handset_context_profiles_connected);
            }
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleInternalConnectReq, unhandled");
        break;
    }
}

static void handsetServiceSm_HandleInternalDisconnectReq(handset_service_state_machine_t *sm,
    const HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T *req)
{
    HS_LOG("handsetServiceSm_HandleInternalDisconnectReq state 0x%x addr [%04x,%02x,%06lx]",
            sm->state, req->addr.nap, req->addr.uap, req->addr.lap);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        {
            if(!HandsetService_BleIsConnected(&sm->ble_sm))
            {
                /* If BR/EDR and BLE are disconnected, complete disconnect requests */
                HandsetServiceSm_CompleteDisconnectRequests(sm, handset_service_status_success);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        /* Cancelled before profile connect was requested; go to disconnected */
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        break;

    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        /* Cancelled in-progress connect/stop connect; go to disconnecting to wait for CFM */
        sm->disconnection_profiles_excluded = req->exclude;
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTING_BREDR);
        break;

    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        {
            if (!HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY))
            {
                sm->disconnection_profiles_excluded = req->exclude;
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTING_BREDR);
            }
            else
            {
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        /* Already in the process of disconnecting so nothing more to do. */
        break;

    default:
        HS_LOG("handsetServiceSm_HandleInternalDisconnectReq, unhandled");
        break;
    }
    
    HandsetService_BleDisconnectIfConnected(&sm->ble_sm);
}

static void handsetServiceSm_HandleInternalConnectAclComplete(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete state enum:handset_service_state_t:%d", sm->state);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        {
            device_t cached_handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
            if (cached_handset_device)
            {
                if (handsetService_CheckHandsetCanConnect(&sm->handset_addr))
                {
                    if (ConManagerIsConnected(&sm->handset_addr))
                    {
                        HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, ACL connected");

                        TimestampEvent(TIMESTAMP_EVENT_HANDSET_CONNECTED_ACL);

                        if (sm->profiles_requested)
                        {
                            /* As handset just connected it cannot have profile connections, so clear flags */
                            BtDevice_SetConnectedProfiles(cached_handset_device, 0);

                            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES);
                        }
                        else
                        {
                            HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, no profiles to connect");
                            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
                        }

                        DeviceProperties_SetHandsetContext(cached_handset_device, handset_context_acl_connected);

                        /* handset is connected,let Multipoint sm connect next handset if any.*/
                        HandsetServiceMultipointSm_SetStateToGetNextDevice();
                    }
                    else
                    {
                        if (sm->acl_attempts < handsetService_BredrAclConnectAttemptLimit())
                        {
                            HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, ACL not connected, retrying");

                            /* Send a delayed message to re-try the ACL connection */
                            MessageSendLater(&sm->task_data,
                                     HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ,
                                     NULL, handsetService_GetPageInterval());
                        }
                        else if ((DeviceProperties_GetHandsetContext(cached_handset_device) == handset_context_reconnecting) &&
                                 handsetService_IsUnlimitedAclReconnectionEnabled())
                        {
                            HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, Reconnecting link loss device with unlimited attempts, retrying");
                            MessageSendLater(&sm->task_data,
                                     HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ,
                                     NULL, handsetService_GetUnlimitedReconnectionPageInterval());
                        }
                        else
                        {
                            /* The handset device shall not attempt to be reconnected until it next enters into
                               the DISCONNECTED state. This means that it will need to be manually reconnected
                               from the phone UI. */

                            HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, ACL failed to connect");
                            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);

                            DeviceProperties_SetHandsetContext(cached_handset_device, handset_context_not_available);

                            /* ACL connection failed, let Multipoint sm connect next handset if any.*/
                            HandsetServiceMultipointSm_SetStateToGetNextDevice();
                        }
                    }
                }
                else
                {
                    /* Not allowed to connect this handset so disconnect it now
                       before the profiles are connected. */
                    HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, new handset connection not allowed");
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
                }
            }
            else
            {
                /* Handset device is no longer valid - usually this is because
                   it was deleted from the device database before it was
                   disconnected. Reject this ACL connection */
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleInternalConnectAclComplete, unhandled");
        break;
    }
}

/*! \brief Handle a HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ */
static void handsetService_HandleInternalConnectStop(handset_service_state_machine_t *sm,
    const HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T *req)
{
    HS_LOG("handsetService_HandleInternalConnectStop state enum:handset_service_state_t:%d", sm->state);

    /* Confirm requested device is actually for this instance. */
    assert(HandsetServiceSm_GetHandsetDeviceIfValid(sm) == req->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        /* ACL has not connected yet so go to disconnected to stop it */
        HS_LOG("handsetService_HandleInternalConnectStop, Cancel ACL connecting");
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        /* Requesting to stop the connection while waiting for profiles to be connected;
        there will be an outstanding profile manager connect request that we must cancel by
        sending a stop connect request, so go to STOP_CONNECTING_BREDR_PROFILES to send 
        the stop connect request. */
        sm->connection_was_not_complete_at_disconnect_request = TRUE;
        HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES);
        break;

    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
        /* still stoppping the profile connection. */
        HS_LOG("handsetService_HandleInternalConnectStop, ignored as stop already in progress");
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTED:
    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        /* Already in a stable state, so send a CFM back immediately. */
        HandsetServiceSm_CompleteConnectStopRequests(sm, handset_service_status_connected);
        break;

    default:
        break;
    }
}

/*! \brief Handle a HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ */
static void handsetService_HandleInternalConnectAclRetryReq(handset_service_state_machine_t *sm)
{
    HS_LOG("handsetService_HandleInternalConnectAclRetryReq state 0x%x", sm->state);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        {
            /* Retry the ACL connection */
            handsetService_ConnectAcl(sm);
        }
        break;

    default:
        break;
    }
}

static void handsetService_HandleInternalConnectProfilesReq(handset_service_state_machine_t *sm)
{
    device_t handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);
    bool handset_is_paired = handsetServiceSm_DeviceIsPaired(sm);

    HS_LOG("handsetService_HandleInternalConnectProfilesReq enum:handset_service_state_t:%d device 0x%p profiles_requested 0x%x is_paired %d",
           sm->state, handset_device, sm->profiles_requested, handset_is_paired);

    if (sm->state == HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES)
    {
        if(handset_is_paired)
        {
            handsetServiceSm_ConnectRequestedProfiles(sm);
        }
        else
        {
            handsetServiceSm_StartProfileConnectionTimeout(sm);
        }
    }

    if(sm->state == HANDSET_SERVICE_STATE_CONNECTED_BREDR)
    {
        HS_LOG("handsetService_HandleInternalConnectProfilesReq HANDSET_SERVICE_STATE_CONNECTED_BREDR no bother");
    }
}

/*! \brief Determine if a profile implies BR/EDR */
static bool handsetServiceSm_ProfileImpliesBrEdr(uint32 profile)
{
    return ((profile & BREDR_PROFILES) != 0);
}

/*! \brief Determine if very first BR-EDR profile is SET. */
static bool handsetServiceSm_FirstBrEdrProfileConnected(uint32 profiles)
{
    /* Only consider BREDR profiles directly related to handset use cases; mask out VA, Peer-related etc */
    profiles &= BREDR_PROFILES;
    return (   (profiles == DEVICE_PROFILE_HFP)
            || (profiles == DEVICE_PROFILE_A2DP)
            || (profiles == DEVICE_PROFILE_AVRCP));
}

/*! \brief Handle a CONNECT_PROFILES_CFM */
static void handsetServiceSm_HandleProfileManagerConnectCfm(handset_service_state_machine_t *sm,
    const CONNECT_PROFILES_CFM_T *cfm)
{
    HS_LOG("handsetServiceSm_HandleProfileManagerConnectCfm enum:handset_service_state_t:%d enum:profile_manager_request_cfm_result_t:%d [%04x,%02x,%06lx]",
           sm->state,
           cfm->result,
           sm->handset_addr.nap,
           sm->handset_addr.uap,
           sm->handset_addr.lap);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        {
            /* Timestamp at this point so that failures could be timed */
            TimestampEvent(TIMESTAMP_EVENT_HANDSET_CONNECTED_PROFILES);

            if (cfm->result == profile_manager_success)
            {
                /* Assume all requested profiles were connected. */
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
            }
            else
            {
                uint32 connected_profiles = handsetServiceSm_GetConnectedProfiles(sm);
                if (handsetServiceSm_ProfileImpliesBrEdr(connected_profiles))
                {
                    HS_LOG("handsetServiceSm_HandleProfileManagerConnectCfm profile(s) still connected");
                    /* some of the BREDR profiles are still connected. */
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
                }
                else if (HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY))
                {
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
                }
                else
                {
                    HS_LOG("handsetServiceSm_HandleProfileManagerConnectCfm only ACL connected");
                    /* Keep the ACL connected so 
                    1. if handover occurs, handset service doesn't need to connect the ACL again.
                    2. if earbud goes into case, as part of NO_ROLE_IDLE, it will be disconnected. */
                    HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
                }
            }
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        /* Nothing more to do as we are already connected.*/
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        /* Connect has been cancelled already but this CFM may have been
           in-flight already. */
        if (HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY))
        {
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
        }
        break;

    default:
        HS_LOG("handsetServiceSm_HandleProfileManagerConnectCfm, unhandled");
        break;
    }

    /* Some profiles like GAIA cannot be told to connect by the profile manager.
     * So there could be some profiles which were requested but not yet connected.
     * profiles_requested is set to 0 as the above info is not relevant for now */
    sm->profiles_requested = 0;
}

/*! \brief Handle a DISCONNECT_PROFILES_CFM */
static void handsetServiceSm_HandleProfileManagerDisconnectCfm(handset_service_state_machine_t *sm,
    const DISCONNECT_PROFILES_CFM_T *cfm)
{
    HS_LOG("handsetServiceSm_HandleProfileManagerDisconnectCfm enum:handset_service_state_t:%d enum:profile_manager_request_cfm_result_t:%d [%04x,%02x,%06lx]",
           sm->state,
           cfm->result,
           sm->handset_addr.nap,
           sm->handset_addr.uap,
           sm->handset_addr.lap);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        if (cfm->result == profile_manager_success)
        {
            handsetServiceSm_TryDisconnectingBredr(sm);
        }
        else
        {
            DEBUG_LOG_WARN("handsetServiceSm_HandleProfileManagerDisconnectCfm, failed to disconnect");
        }
        break;

    default:
        DEBUG_LOG_WARN("handsetServiceSm_HandleProfileManagerDisconnectCfm, unhandled");
        break;
    }
}

/*! \brief Test if the current state is in correct state where first profile connect indication 
           can be sent to UI so prompt or tone can be played. */
static bool handsetServiceSm_CanSendFirstProfileConnectInd(handset_service_state_machine_t *sm)
{
    PanicNull(sm);

    switch(sm->state)
    {
        case HANDSET_SERVICE_STATE_DISCONNECTED:
        case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
            return TRUE;

        default:
            return FALSE;
    }
}

/*! \brief Handle a CONNECTED_PROFILE_IND */
void HandsetServiceSm_HandleProfileManagerConnectedInd(handset_service_state_machine_t *sm,
    const CONNECTED_PROFILE_IND_T *ind)
{
    HS_LOG("HandsetServiceSm_HandleProfileManagerConnectedInd device 0x%x enum:handset_service_state_t:%d profile 0x%x [%04x,%02x,%06lx]",
           ind->device,
           sm->state,
           ind->profile,
           sm->handset_addr.nap,
           sm->handset_addr.uap,
           sm->handset_addr.lap);


    device_t cached_handset_device = HandsetServiceSm_GetHandsetDeviceIfValid(sm);

    assert(cached_handset_device == ind->device);

    uint32 connected_profiles = handsetServiceSm_GetConnectedProfiles(sm);

    if (   handsetServiceSm_CanSendFirstProfileConnectInd(sm)
        && handsetServiceSm_FirstBrEdrProfileConnected(connected_profiles))
    { 
        HandsetService_SendFirstProfileConnectedIndNotification(cached_handset_device);
    }

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        if (handsetServiceSm_ProfileImpliesBrEdr(ind->profile))
        {
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
        /* Check if all profiles are connected. */
        if(handsetServiceSm_AreAllRequestedProfilesConnected(sm))
        {
            HS_LOG("HandsetServiceSm_HandleProfileManagerConnectedInd remote no more profile(s) to connect");
            /* Timestamp at this point so that failures could be timed */
            TimestampEvent(TIMESTAMP_EVENT_HANDSET_CONNECTED_PROFILES);

            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTED_BREDR);
        }
        else
        {
            handsetServiceSm_RestartProfileConnectionTimeout(sm);
        }
        
        /* Do not request this profile again until a new connection is made */
        handsetServiceSm_RemoveProfiles(sm->profiles_requested, ind->profile);
        break;

    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        {
            if (handsetServiceSm_ProfileImpliesBrEdr(ind->profile))
            {
                /* Stay in the same state but send an IND with all the profile(s) currently connected. */
                HandsetService_SendConnectedIndNotification(cached_handset_device, connected_profiles);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        /* Although we are disconnecting, if a profile re-connects just ignore and stay
           in the DISCONNECTING state.
           This can happen as profile manager already started connecting the profiles,
           so it has requested respective profile such as A2DP and etc to make connection.
           We have been requested(by topology) to stop/disconnect connection so we moves to
           DISCONNECTING state and we requested to profile manager to disconnect the profiles.
           Profile manager requests to a respective profile to disconnect. While connection is
           in the process, a profile doesn't process disconnect before a profile is connected
           which is why we end up receiving CONNECTED_PROFILE_IND. */

        DEBUG_LOG("HandsetServiceSm_HandleProfileManagerConnectedInd something connected %d",
                  !HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY));
        break;

    default:
        HS_LOG("HandsetServiceSm_HandleProfileManagerConnectedInd, unhandled");
        break;
    }
}

/*! \brief Handle a DISCONNECTED_PROFILE_IND */
void HandsetServiceSm_HandleProfileManagerDisconnectedInd(handset_service_state_machine_t *sm,
    const DISCONNECTED_PROFILE_IND_T *ind)
{
    HS_LOG("HandsetServiceSm_HandleProfileManagerDisconnectedInd device 0x%x enum:handset_service_state_t:%d profile 0x%x enum:profile_manager_disconnected_ind_reason_t:%d [%04x,%02x,%06lx]",
           ind->device,
           sm->state,
           ind->profile,
           ind->reason,
           sm->handset_addr.nap,
           sm->handset_addr.uap,
           sm->handset_addr.lap);

    assert(HandsetServiceSm_GetHandsetDeviceIfValid(sm) == ind->device);

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
        /* If a profile disconnects for any reason the handset may be fully
           disconnected so we need to check that and go to a disconnected
           state if necessary. */
        /*  Intentional fall-through */
    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        {
            /* Note: don't remove the profile from the 'last connected'
               profiles because we don't have enough information to know if the
               handset disconnected the profile on its own, or as part of
               a full disconnect. */

            /* Only go to disconnected state if there are no other handset connections. */
            if (HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY))
            {
                HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            }
        }
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        /* A disconnect request to the profile manager is in progress, so wait
           for the DISCONNECT_PROFILES_CFM and the ACL to be disconnected. */
        DEBUG_LOG("HandsetServiceSm_HandleProfileManagerDisconnectedInd something connected %d",
                  HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY));
        break;

    default:
        HS_LOG("HandsetServiceSm_HandleProfileManagerDisconnectedInd, unhandled");
        break;
    }
}

void HandsetServiceSm_HandleConManagerBleTpConnectInd(handset_service_state_machine_t *sm,
    const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    tp_bdaddr tpbdaddr;
    HandsetService_ResolveTpaddr(&ind->tpaddr, &tpbdaddr);
    bool was_resolved = ((tpbdaddr.taddr.type == TYPED_BDADDR_PUBLIC) && (ind->tpaddr.taddr.type == TYPED_BDADDR_RANDOM));
    
    HS_LOG("HandsetServiceSm_HandleConManagerBleTpConnectInd enum:handset_service_state_t:%d address resolved:%d, enum:TRANSPORT_T:%d type %d [%04x,%02x,%06lx] ",
        sm->state,
        was_resolved,
        tpbdaddr.transport,
        tpbdaddr.taddr.type,
        tpbdaddr.taddr.addr.nap,
        tpbdaddr.taddr.addr.uap,
        tpbdaddr.taddr.addr.lap);

    /* If we have no handset device but have an entry for this address then
    populate the field. Do not create a new device if the device is not
    known. This will be done if we pair */
    if(!sm->handset_device)
    {
        device_t device = NULL;

        device = BtDevice_GetDeviceForTpbdaddr(&tpbdaddr);

        if(device)
        {
            HS_LOG("HandsetServiceSm_HandleConManagerBleTpConnectInd Have existing device in database");
            HandsetServiceSm_SetDevice(sm, device);
        }
    }
    
    HandsetService_BleHandleConnected(&sm->ble_sm, &tpbdaddr);
}

/*! \brief Handle a handset initiated ACL connection.

    This represents an ACL connection that was initiated by the handset.

    Usually this will happen in a disconnected state, before any profiles have
    connected. In this case go directly to the BR/EDR connected state.

*/
void HandsetServiceSm_HandleConManagerBredrTpConnectInd(handset_service_state_machine_t *sm,
    const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    HS_LOG("HandsetServiceSm_HandleConManagerBredrTpConnectInd enum:handset_service_state_t:%d device 0x%x enum:TRANSPORT_T:%d type %d [%04x,%02x,%06lx] ",
           sm->state,
           sm->handset_device,
           ind->tpaddr.transport,
           ind->tpaddr.taddr.type,
           ind->tpaddr.taddr.addr.nap,
           ind->tpaddr.taddr.addr.uap,
           ind->tpaddr.taddr.addr.lap);

    assert(BdaddrIsSame(&sm->handset_addr, &ind->tpaddr.taddr.addr));

    switch (sm->state)
    {
    case HANDSET_SERVICE_STATE_DISCONNECTED:
       {
            TimestampEvent(TIMESTAMP_EVENT_HANDSET_CONNECTED_ACL);
            HS_LOG("HandsetServiceSm_HandleConManagerBredrTpConnectInd, remote AG connected ACL");

            /* As handset just connected it cannot have profile connections, so connect profiles */
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES);

            DeviceProperties_SetHandsetContext(sm->handset_device, handset_context_acl_connected);

            /* handset ACL connected, let Multipoint sm connect next handset if any.*/
            HandsetServiceMultipointSm_SetStateToGetNextDevice();
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL:
        /* Although we are waiting for the ACL to connect, we use
           HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE to detect when the ACL
           is connected. But if we were waiting to retry to connect the ACL
           after a connection failure, and the device connects, the complete
           message would not be received and nothing would happen, therefore
           send the complete message immediately. */
        if (MessageCancelFirst(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ))
        {
            MessageSend(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE, NULL);
        }
        break;

    case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
    case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
    case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        /* Unexpected but harmless? */
        break;

    case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
        /* It would be unusual to get an ACL re-connecting if the state machine
           was in the process of disconnecting.
           Not sure of the best way to handle this? */

        DEBUG_LOG("HandsetServiceSm_HandleConManagerBredrTpConnectInd something connected %d",
                  !HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY));
        break;

    default:
        break;
    }
}

/*! \brief Handle a BR/EDR CON_MANAGER_TP_DISCONNECT_IND_T

    This represents the handset ACL has disconnected. Check if any other
    handset connections are active and if not, go into a disconnected state.
*/
void HandsetServiceSm_HandleConManagerBredrTpDisconnectInd(handset_service_state_machine_t *sm,
    const CON_MANAGER_TP_DISCONNECT_IND_T *ind)
{
    HS_LOG("HandsetServiceSm_HandleConManagerBredrTpDisconnectInd enum:handset_service_state_t:%d device 0x%x enum:hci_status:%u enum:TRANSPORT_T:%d type %d [%04x,%02x,%06lx] ",
           sm->state,
           sm->handset_device,
           ind->reason,
           ind->tpaddr.transport,
           ind->tpaddr.taddr.type,
           ind->tpaddr.taddr.addr.nap,
           ind->tpaddr.taddr.addr.uap,
           ind->tpaddr.taddr.addr.lap);

    if(HandsetServiceSm_GetHandsetDeviceIfValid(sm) == NULL)
    {
        return;
    }

    assert(BdaddrIsSame(&sm->handset_addr, &ind->tpaddr.taddr.addr));

    /* Store the reason for handset disconnection */
    sm->disconnect_reason = ind->reason;

    /* Act on the indication only if all the profiles are disconnected (N.b. they should be,
       this is a guard) or if the disconnect was started locally.

       Note: if the disconnect was started locally it may not have been started by the handset service,
             e.g. if the ACL was force-disconnected by the topology. */
    if (HandsetServiceSm_AllConnectionsDisconnected(sm, BREDR_ONLY) ||
        handsetService_IsDisconnectLocal(sm->disconnect_reason))
    {
        switch (sm->state)
        {
        case HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES:
            /* Already stopping profile connections, wait for that to complete */
            break;

        case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
            /* Stop any profile connections in progress, if we have started connection. In most cases
               they will have already have been stopped and this will lead to us transiting to the
               disconnected state.*/
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_STOP_CONNECTING_BREDR_PROFILES);
            break;

        case HANDSET_SERVICE_STATE_CONNECTING_BREDR_ACL: /* All BR/EDR profiles and ACL are already disconnected */
        case HANDSET_SERVICE_STATE_CONNECTED_BREDR:      /* Go to the disconnected state */
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            break;

        case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
            BtDevice_SetConnectedProfiles(sm->handset_device, 0);
            HandsetServiceSm_SetState(sm, HANDSET_SERVICE_STATE_DISCONNECTED);
            break;

        default:
            HS_LOG("HandsetServiceSm_HandleConManagerBredrTpDisconnectInd unhandled");
            break;
        }
    }
}

static void handsetServiceSm_MessageHandler(Task task, MessageId id, Message message)
{
    handset_service_state_machine_t *sm = handsetServiceSm_GetSmFromTask(task);

    HS_LOG("handsetServiceSm_MessageHandler id MESSAGE:handset_service_internal_msg_t:0x%x", id);

    switch (id)
    {
    /* connection_manager messages */

    /* profile_manager messages */
    case CONNECT_PROFILES_CFM:
        handsetServiceSm_HandleProfileManagerConnectCfm(sm, (const CONNECT_PROFILES_CFM_T *)message);
        break;

    case DISCONNECT_PROFILES_CFM:
        handsetServiceSm_HandleProfileManagerDisconnectCfm(sm, (const DISCONNECT_PROFILES_CFM_T *)message);
        break;

    /* Internal messages */
    case HANDSET_SERVICE_INTERNAL_CONNECT_REQ:
        handsetServiceSm_HandleInternalConnectReq(sm, (const HANDSET_SERVICE_INTERNAL_CONNECT_REQ_T *)message);
        break;

    case HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ:
        handsetServiceSm_HandleInternalDisconnectReq(sm, (const HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ_T *)message);
        break;

    case HANDSET_SERVICE_INTERNAL_CONNECT_ACL_COMPLETE:
        handsetServiceSm_HandleInternalConnectAclComplete(sm);
        break;

    case HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ:
        handsetService_HandleInternalConnectStop(sm, (const HANDSET_SERVICE_INTERNAL_CONNECT_STOP_REQ_T *)message);
        break;

    case HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ:
        handsetService_HandleInternalConnectAclRetryReq(sm);
        break;

    case HANDSET_SERVICE_INTERNAL_CONNECT_PROFILES_REQ:
        handsetService_HandleInternalConnectProfilesReq(sm);
        break;

    default:
        HS_LOG("handsetService_MessageHandler unhandled msg id MESSAGE:handset_service_internal_msg_t:0x%x", id);
        break;
    }
}

void HandsetServiceSm_Init(handset_service_state_machine_t *sm)
{
    assert(sm != NULL);

    memset(sm, 0, sizeof(*sm));
    sm->state = HANDSET_SERVICE_STATE_NULL;
    BdaddrSetZero(&sm->handset_addr);
    HandsetService_BleSmReset(&sm->ble_sm);
    sm->task_data.handler = handsetServiceSm_MessageHandler;

    TaskList_Initialise(&sm->connect_list);
    TaskList_Initialise(&sm->disconnect_list);
}

void HandsetServiceSm_DeInit(handset_service_state_machine_t *sm)
{
    TaskList_RemoveAllTasks(&sm->connect_list);
    TaskList_RemoveAllTasks(&sm->disconnect_list);

    MessageFlushTask(&sm->task_data);
    HandsetServiceSm_SetDevice(sm, (device_t)0);
    BdaddrSetZero(&sm->handset_addr);
    sm->profiles_requested = 0;
    sm->acl_create_called = FALSE;
    sm->state = HANDSET_SERVICE_STATE_NULL;
    HandsetService_BleSmReset(&sm->ble_sm);
}

void HandsetServiceSm_CancelInternalConnectRequests(handset_service_state_machine_t *sm)
{
    MessageCancelAll(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_REQ);
    MessageCancelAll(&sm->task_data, HANDSET_SERVICE_INTERNAL_CONNECT_ACL_RETRY_REQ);
}

void HandsetServiceSm_CompleteConnectRequests(handset_service_state_machine_t *sm, handset_service_status_t status)
{
    if (TaskList_Size(&sm->connect_list))
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_CFM_T);
        cfm->addr = sm->handset_addr;
        cfm->status = status;

        /* Send HANDSET_SERVICE_CONNECT_CFM to all clients who made a
           connect request, then remove them from the list. */
        TaskList_MessageSend(&sm->connect_list, HANDSET_SERVICE_CONNECT_CFM, cfm);
        TaskList_RemoveAllTasks(&sm->connect_list);
    }

    /* Flush any queued internal connect requests */
    HandsetServiceSm_CancelInternalConnectRequests(sm);
}

void HandsetServiceSm_CompleteDisconnectRequests(handset_service_state_machine_t *sm, handset_service_status_t status)
{
    DEBUG_LOG("HandsetServiceSm_CompleteDisconnectRequests");
    if (TaskList_Size(&sm->disconnect_list))
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_DISCONNECT_CFM_T);
        handsetServiceSm_GetBdAddr(sm, &cfm->addr);
        cfm->status = status;

        /* Send HANDSET_SERVICE_DISCONNECT_CFM to all clients who made a
           disconnect request, then remove them from the list. */
        TaskList_MessageSend(&sm->disconnect_list, HANDSET_SERVICE_DISCONNECT_CFM, cfm);
        TaskList_RemoveAllTasks(&sm->disconnect_list);
    }

    /* Flush any queued internal disconnect requests */
    MessageCancelAll(&sm->task_data, HANDSET_SERVICE_INTERNAL_DISCONNECT_REQ);
}

void HandsetServiceSm_CompleteConnectStopRequests(handset_service_state_machine_t *sm, handset_service_status_t status)
{
    if (sm->connect_stop_task)
    {
        MESSAGE_MAKE(cfm, HANDSET_SERVICE_CONNECT_STOP_CFM_T);
        cfm->addr = sm->handset_addr;
        cfm->status = status;

        MessageSend(sm->connect_stop_task, HANDSET_SERVICE_CONNECT_STOP_CFM, cfm);
        sm->connect_stop_task = (Task)0;
    }
}

bool HandsetServiceSm_IsLeAclConnected(handset_service_state_machine_t *sm)
{
    PanicNull(sm);

    return HandsetService_BleIsConnected(&sm->ble_sm);
}

bool HandsetServiceSm_IsBredrAclConnected(handset_service_state_machine_t *sm)
{
    PanicNull(sm);
    
    switch(sm->state)
    {
        case HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES:
        case HANDSET_SERVICE_STATE_CONNECTED_BREDR:
        case HANDSET_SERVICE_STATE_DISCONNECTING_BREDR:
            return TRUE;
        
        default:
            return FALSE;
    }
}

bool HandsetServiceSm_MaxBredrAclConnectionsReached(void)
{
    unsigned num_bredr_connections = HandsetServiceSm_GetBredrAclConnectionCount();
    unsigned max_bredr_connections = handsetService_BredrAclMaxConnections();
    
    HS_LOG("HandsetServiceSm_MaxBredrAclConnectionsReached  %u of %u BR/EDR connections", num_bredr_connections, max_bredr_connections);
    
    return num_bredr_connections >= max_bredr_connections;
}

bool HandsetServiceSm_MaxFullyConnectedBredrHandsetConnectionsReached(void)
{
    unsigned num_fully_connected_handset = HandsetServiceSm_GetFullyConnectedBredrHandsetCount();
    unsigned max_bredr_acl_connections = handsetService_BredrAclMaxConnections();

    HS_LOG("HandsetServiceSm_MaxFullyConnectedBredrHandsetConnectionsReached %u of %u BR/EDR connections", num_fully_connected_handset, max_bredr_acl_connections);

    return num_fully_connected_handset >= max_bredr_acl_connections;
}

void HandsetServiceSm_EnableConnectableIfMaxConnectionsNotActive(void)
{
    bool max_connections_reached;
    
    HS_LOG("HandsetServiceSm_EnableConnectableIfMaxConnectionsNotActive");
    max_connections_reached = HandsetServiceSm_MaxBredrAclConnectionsReached();
    
    if(!max_connections_reached)
    {
        HS_LOG("HandsetServiceSm_EnableConnectableIfMaxConnectionsNotActive - enable connectable");
        handsetService_ConnectableEnableBredr(TRUE);
        if(HandsetService_IsConnectionBargeInEnabled())
        {
            HandsetService_DisableTruncatedPageScan();
        }
    }
}

void HandsetServiceSm_DisableConnectableIfMaxConnectionsActive(void)
{
    bool max_connections_reached;
    
    HS_LOG("HandsetServiceSm_DisableConnectableIfMaxConnectionsActive");
    max_connections_reached = HandsetServiceSm_MaxBredrAclConnectionsReached();
    
    if(max_connections_reached)
    {
        HS_LOG("HandsetServiceSm_DisableConnectableIfMaxConnectionsActive - disable connectable");
        handsetService_ConnectableEnableBredr(FALSE);
        if(HandsetService_IsConnectionBargeInEnabled())
        {
            HandsetService_RestartBargeInEnableTimer();
        }
    }
}

bool HandsetServiceSm_CouldDevicesPair(void)
{
    FOR_EACH_HANDSET_SM(sm)
    {
        if (HandsetService_BleIsConnected(&sm->ble_sm))
        {
            if (sm->pairing_possible)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

bool HandsetServiceSm_IsConnecting(void)
{
    bool connecting = FALSE;

    FOR_EACH_HANDSET_SM(sm)
    {
        /* check if any task requested to connect handset (stored in connect_list) 
           and also make sure if any AG is connecting profiles (this can be happen
           when AG has connected to earbud where connect_list will be empty)*/
        if (TaskList_Size(&sm->connect_list) ||
            (!ConManagerIsAclLocal(&sm->handset_addr) && (sm->state == HANDSET_SERVICE_STATE_CONNECTING_BREDR_PROFILES)))
        {
            connecting = TRUE;
            break;
        }
    }
    return connecting;
}

bool HandsetServiceSm_IsDisconnecting(void)
{
    bool disconnecting = FALSE;

    FOR_EACH_HANDSET_SM(sm)
    {
        if (TaskList_Size(&sm->disconnect_list))
        {
            disconnecting = TRUE;
            break;
        }
    }
    return disconnecting;
}

handset_service_state_machine_t* HandsetService_GetSmFromBleSm(handset_service_ble_state_machine_t* ble_sm)
{
    PanicNull(ble_sm);
    
    FOR_EACH_HANDSET_SM(sm)
    {
        if (&sm->ble_sm == ble_sm)
        {
            return sm;
        }
    }
    
    /* ble_sm must be invalid if we get here */
    Panic();
    return NULL;
}
