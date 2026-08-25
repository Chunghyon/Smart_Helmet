/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Implementation of the handover of (initial) primary device.
*/
#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"
#include "handover_profile_procedures.h"

/*! Primary handover states */
typedef enum handover_profile_primary_states
{
    HO_PRIMARY_STATE_SETUP,
    HO_PRIMARY_STATE_SELF_VETO,
    HO_PRIMARY_STATE_VETO1,
    HO_PRIMARY_STATE_SEND_START_REQ,
    HO_PRIMARY_STATE_WAIT_FOR_START_CFM,
    HO_PRIMARY_STATE_SEND_P1_MARSHAL_DATA,
    HO_PRIMARY_STATE_WAIT_FOR_MARSHAL_DATA_CFM,
    HO_PRIMARY_STATE_VETO2,
    HO_PRIMARY_STATE_PERFORMANCE_REQUEST,
    HO_PRIMARY_STATE_HALT_INACTIVE_ACL_LINKS,
    HO_PRIMARY_STATE_VETO3,
    HO_PRIMARY_STATE_PREPARE_INACTIVE_ACL_LINKS,
    HO_PRIMARY_STATE_VETO4,
    HO_PRIMARY_STATE_SEND_INACTIVE_ACL_LINK_MARSHAL_DATA,
    HO_PRIMARY_STATE_WAIT_FOR_A2DP_PACKET,
    HO_PRIMARY_STATE_HALT_ACTIVE_ACL_LINKS,
    HO_PRIMARY_STATE_VETO5,
    HO_PRIMARY_STATE_SET_PEER_LINK_ACTIVE_MODE,
    HO_PRIMARY_STATE_PREPARE_ACTIVE_ACL_LINKS,
    HO_PRIMARY_STATE_WAIT_FOR_PEER_LINK_ACTIVE_MODE,
    HO_PRIMARY_STATE_VETO6,
    HO_PRIMARY_STATE_SEND_ACTIVE_ACL_LINK_MARSHAL_DATA,
    HO_PRIMARY_STATE_CLEAR_PENDING_PEER_DATA,
    HO_PRIMARY_STATE_COMMIT_TO_SECONDARY_ROLE,
    HO_PRIMARY_STATE_PERFORMANCE_RELINQUISH,
    HO_PRIMARY_STATE_CLEANUP,
    HO_PRIMARY_STATE_COMPLETE

} handover_profile_primary_state_t;

static handover_profile_status_t handoverProfile_PrimarySetup(void);
static handover_profile_status_t handoverProfile_PrimarySelfVeto(void);
static handover_profile_status_t handoverProfile_HaltInactiveAclLinks(void);
static handover_profile_status_t handoverProfile_PrepareInactiveAclLinks(void);
static handover_profile_status_t handoverProfile_SendInactiveLinkMarshalData(void);
static handover_profile_status_t handoverProfile_WaitForA2dpPacket(void);
static handover_profile_status_t handoverProfile_HaltActiveAclLinks(void);
static handover_profile_status_t handoverProfile_SetPeerLinkActiveMode(void);
static handover_profile_status_t handoverProfile_PrepareActiveAclLinks(void);
static handover_profile_status_t handoverProfile_WaitForPeerLinkActiveMode(void);
static handover_profile_status_t handoverProfile_SendActiveLinkMarshalData(void);
static handover_profile_status_t handoverProfile_CommitSecondaryRole(void);
static handover_profile_status_t handoverProfile_PrimaryCleanup(void);

/*! This table maps a function to perform the actions required of the associated state */
handover_profile_status_t (* const state_handler_map[])(void) =
{
    [HO_PRIMARY_STATE_SETUP] =                                  handoverProfile_PrimarySetup,
    [HO_PRIMARY_STATE_SELF_VETO] =                              handoverProfile_PrimarySelfVeto,
    [HO_PRIMARY_STATE_VETO1] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SEND_START_REQ] =                         handoverProtocol_SendStartReq,
    [HO_PRIMARY_STATE_WAIT_FOR_START_CFM] =                     handoverProtocol_WaitForStartCfm,
    [HO_PRIMARY_STATE_SEND_P1_MARSHAL_DATA] =                   handoverProtocol_SendP1MarshalData,
    [HO_PRIMARY_STATE_WAIT_FOR_MARSHAL_DATA_CFM] =              handoverProtocol_WaitForUnmarshalP1Cfm,
    [HO_PRIMARY_STATE_VETO2] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_PERFORMANCE_REQUEST] =                    handoverProfile_PerformanceRequest,
    [HO_PRIMARY_STATE_HALT_INACTIVE_ACL_LINKS] =                handoverProfile_HaltInactiveAclLinks,
    [HO_PRIMARY_STATE_VETO3] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_PREPARE_INACTIVE_ACL_LINKS] =             handoverProfile_PrepareInactiveAclLinks,
    [HO_PRIMARY_STATE_VETO4] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SEND_INACTIVE_ACL_LINK_MARSHAL_DATA] =    handoverProfile_SendInactiveLinkMarshalData,
    [HO_PRIMARY_STATE_WAIT_FOR_A2DP_PACKET] =                   handoverProfile_WaitForA2dpPacket,
    [HO_PRIMARY_STATE_HALT_ACTIVE_ACL_LINKS] =                  handoverProfile_HaltActiveAclLinks,
    [HO_PRIMARY_STATE_VETO5] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SET_PEER_LINK_ACTIVE_MODE] =              handoverProfile_SetPeerLinkActiveMode,
    [HO_PRIMARY_STATE_PREPARE_ACTIVE_ACL_LINKS] =               handoverProfile_PrepareActiveAclLinks,
    [HO_PRIMARY_STATE_WAIT_FOR_PEER_LINK_ACTIVE_MODE] =         handoverProfile_WaitForPeerLinkActiveMode,
    [HO_PRIMARY_STATE_VETO6] =                                  handoverProfile_VetoP1Clients,
    [HO_PRIMARY_STATE_SEND_ACTIVE_ACL_LINK_MARSHAL_DATA] =      handoverProfile_SendActiveLinkMarshalData,
    [HO_PRIMARY_STATE_CLEAR_PENDING_PEER_DATA] =                handoverProfile_ClearPendingPeerData,
    [HO_PRIMARY_STATE_COMMIT_TO_SECONDARY_ROLE] =               handoverProfile_CommitSecondaryRole,
    [HO_PRIMARY_STATE_PERFORMANCE_RELINQUISH] =                 handoverProfile_PerformanceRelinquish,
    [HO_PRIMARY_STATE_CLEANUP] =                                handoverProfile_PrimaryCleanup,
    [HO_PRIMARY_STATE_COMPLETE] =                               NULL,
};

static handover_profile_status_t handoverProfile_CancelPrepareActiveAclLinks(void);
static handover_profile_status_t handoverProfile_SetPeerLinkSniffMode(void);
static handover_profile_status_t handoverProfile_ResumeActiveAclLinks(void);
static handover_profile_status_t handoverProfile_CancelPrepareInactiveAclLinks(void);
static handover_profile_status_t handoverProfile_ResumeInactiveAclLinks(void);
static bool handoverProfile_IsAnyMirroredDevice(void);

/*! This table maps a function to handle the failure of handover in the
    associated state. Each handler undoes the actions of the corresponding
    function in the state_handler_map table */
handover_profile_status_t (*const state_failure_map[])(void) =
{
    /* Handle failure by undoing each action performed durign the handover procedure. */
    [HO_PRIMARY_STATE_SETUP] =                                  handoverProfile_PrimaryCleanup,
    [HO_PRIMARY_STATE_SELF_VETO] =                              NULL,
    [HO_PRIMARY_STATE_VETO1] =                                  NULL,
    [HO_PRIMARY_STATE_SEND_START_REQ] =                         handoverProtocol_SendCancelInd,
    [HO_PRIMARY_STATE_WAIT_FOR_START_CFM] =                     NULL,
    [HO_PRIMARY_STATE_SEND_P1_MARSHAL_DATA] =                   handoverProfile_AbortP1Clients,
    [HO_PRIMARY_STATE_WAIT_FOR_MARSHAL_DATA_CFM] =              NULL,
    [HO_PRIMARY_STATE_VETO2] =                                  NULL,
    [HO_PRIMARY_STATE_PERFORMANCE_REQUEST] =                    handoverProfile_PerformanceRelinquish,
    [HO_PRIMARY_STATE_HALT_INACTIVE_ACL_LINKS] =                handoverProfile_ResumeInactiveAclLinks,
    [HO_PRIMARY_STATE_VETO3] =                                  NULL,
    [HO_PRIMARY_STATE_PREPARE_INACTIVE_ACL_LINKS] =             handoverProfile_CancelPrepareInactiveAclLinks,
    [HO_PRIMARY_STATE_VETO4] =                                  NULL,
    [HO_PRIMARY_STATE_SEND_INACTIVE_ACL_LINK_MARSHAL_DATA] =    NULL,
    [HO_PRIMARY_STATE_WAIT_FOR_A2DP_PACKET] =                   NULL,
    [HO_PRIMARY_STATE_HALT_ACTIVE_ACL_LINKS] =                  handoverProfile_ResumeActiveAclLinks,
    [HO_PRIMARY_STATE_VETO5] =                                  NULL,
    [HO_PRIMARY_STATE_SET_PEER_LINK_ACTIVE_MODE] =              handoverProfile_SetPeerLinkSniffMode,
    [HO_PRIMARY_STATE_PREPARE_ACTIVE_ACL_LINKS] =               handoverProfile_CancelPrepareActiveAclLinks,
    [HO_PRIMARY_STATE_WAIT_FOR_PEER_LINK_ACTIVE_MODE] =         NULL,
    [HO_PRIMARY_STATE_VETO6] =                                  NULL,
    [HO_PRIMARY_STATE_SEND_ACTIVE_ACL_LINK_MARSHAL_DATA] =      NULL,
    /* States not expected to fail under any circumstances */
    [HO_PRIMARY_STATE_CLEAR_PENDING_PEER_DATA] =                handoverProfile_Panic,
    [HO_PRIMARY_STATE_COMMIT_TO_SECONDARY_ROLE] =               handoverProfile_Panic,
    [HO_PRIMARY_STATE_PERFORMANCE_RELINQUISH] =                 handoverProfile_Panic,
    [HO_PRIMARY_STATE_CLEANUP] =                                handoverProfile_Panic,
    [HO_PRIMARY_STATE_COMPLETE] =                               handoverProfile_Panic,
};

static void handoverProfile_HandleFailureAsPrimary(handover_profile_primary_state_t last_completed_state);

handover_profile_status_t handoverProfile_HandoverAsPrimary(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    handover_profile_status_t status = HANDOVER_PROFILE_STATUS_SUCCESS;
    handover_profile_primary_state_t primary_state;

    ho_inst->handover_type = handover_type_standard_initiator;

    for (primary_state = HO_PRIMARY_STATE_SETUP; primary_state != HO_PRIMARY_STATE_COMPLETE; primary_state++)
    {
        DEBUG_LOG_INFO("handoverProfile_HandoverAsPrimary enum:handover_profile_primary_state_t:%d", primary_state);

        if (state_handler_map[primary_state])
        {
            status = state_handler_map[primary_state]();

            /* Break out at the point a procedure is failed */
            if (status != HANDOVER_PROFILE_STATUS_SUCCESS)
            {
                break;
            }
            else
            {
                /*Process the active ACL link states if CIS is deligated 
                  else skip the processing of active ACL link states if there are no active BR/EDR mirrored device */
                if (primary_state == HO_PRIMARY_STATE_SEND_INACTIVE_ACL_LINK_MARSHAL_DATA &&
                    !handoverProfile_IsAnyMirroredDevice())
                {
                    primary_state = HO_PRIMARY_STATE_SEND_ACTIVE_ACL_LINK_MARSHAL_DATA;
                }
            }
        }
    }

    if (status != HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        /* Pass in the last successful state */
        handoverProfile_HandleFailureAsPrimary(primary_state-1);
    }

    ho_inst->handover_type = handover_type_not_in_progress;

    return status;
}

/*! \brief Iterate back through the completed states undoing actions to handle the failure */
static void handoverProfile_HandleFailureAsPrimary(handover_profile_primary_state_t last_completed_state)
{
    do
    {
        /* Skip failure handling of active ACL links if there is no BR/EDR mirrored device */
        if (last_completed_state == HO_PRIMARY_STATE_SEND_ACTIVE_ACL_LINK_MARSHAL_DATA &&
            !handoverProfile_IsAnyMirroredDevice())
        {
            last_completed_state = HO_PRIMARY_STATE_SEND_INACTIVE_ACL_LINK_MARSHAL_DATA;
        }

        if (state_failure_map[last_completed_state])
        {
            DEBUG_LOG_INFO("handoverProfile_HandleFailureAsPrimary enum:handover_profile_primary_state_t:%d", last_completed_state);
            state_failure_map[last_completed_state]();
        }
    } while (last_completed_state-- != HO_PRIMARY_STATE_SETUP);
}

static bool handoverProfile_IsAnyMirroredDevice(void)
{
    /* Check if there is any mirrored device, specifically BR/EDR ACL mirrored device */
    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->is_mirrored)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static handover_profile_status_t handoverProfile_PrimaryCleanup(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    handover_device_t *next = ho_inst->device_list;
    while (next)
    {
        handover_device_t *current = next;
        Source source = current->u.p.btstack_source;
        next = current->next;
        current->next = NULL;
        SourceEmpty(source);
        SourceClose(source);
        free(current);
    }
    ho_inst->device_list = NULL;

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_PrimarySetup(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    cm_connection_iterator_t iterator;
    tp_bdaddr addr;

    /* Clean any stale state */
    handoverProfile_PrimaryCleanup();

    /* Note that the order of devices in the device_list is used to set the
       order of devices in the start request protocol message and the order in
       which devices are prepared and marshalled/unmarshalled during handover
       procedure.
    */
    if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
    {
        bool create_device;
        do
        {
            create_device = FALSE;
            if (addr.transport == TRANSPORT_BREDR_ACL)
            {
                /* Interested in handset BREDR connections */
                if (!BdaddrIsSame(&addr.taddr.addr, &ho_inst->peer_addr))
                {
                    create_device = TRUE;
                }
            }

            if (create_device)
            {
                handover_device_t *device = PanicUnlessMalloc(sizeof(*device));
                memset(device, 0, sizeof(*device));
                device->next = ho_inst->device_list;
                device->addr = addr;
                /* Check if this device is the mirrored device. Only BR/EDR ACL is considered to be the mirrored device.
                   Because LE ACL will never be mirrored, only CISes are mirrored/delegated for LE-Audio 
                */
                device->is_mirrored = MirrorProfile_IsSameMirroredDevice(&addr);
                ho_inst->device_list = device;
            }
        } while (ConManager_IterateNextActiveConnection(&iterator, &addr));
    }

    SourceEmpty(ho_inst->link_source);
    TimestampEvent(TIMESTAMP_EVENT_PRI_HANDOVER_STARTED);

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_PrimarySelfVeto(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();

    switch (ho_inst->peer_firmware)
    {
        case HANDOVER_PROFILE_PEER_FIRMWARE_UNKNOWN:
            DEBUG_LOG_INFO("handoverProfile_PrimarySelfVeto secondary firmware unknown - veto");
            return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;

        case HANDOVER_PROFILE_PEER_FIRMWARE_MISMATCHED:
            DEBUG_LOG_INFO("handoverProfile_PrimarySelfVeto secondary firmware mismatched");
            return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;

        default:
            break;
    }

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        /*! Handover of LE devices is not supported, all LE links should be
            disconnect before attempting handover. */
        if (device->addr.transport == TRANSPORT_BLE_ACL)
        {
            DEBUG_LOG_INFO("handoverProfile_PrimarySelfVeto unexpected LE ACL");
            return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
        }
    }

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

/* Iterates through the handover device list and halts/resumes them one by one. If halt request failed for any device, 
   this function will recursively find all the previously halted devices and resumes them before returning a failure.
*/
static handover_profile_status_t handoverProfile_HaltOrResumeAclLinks(bool halt, bool mirrored, handover_device_t *end_device)
{
    handover_profile_status_t result = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        /* If device in the handover list matches with end_device, either we have resumed the halted devices OR there 
           are no previously halted devices to resume.
        */
        if (end_device == device)
        {
            break;
        }

        if (device->is_mirrored == mirrored)
        {
            DEBUG_LOG_INFO("handoverProfile_HaltOrResumeAclLinks lap:0x%x", device->addr.taddr.addr.lap);
            if (halt)
            {
                result = handoverProfile_HaltLink(device);
                if (result != HANDOVER_PROFILE_STATUS_SUCCESS)
                {
                    /* Halt request failed for this device. Recursively try to find already halted devices and
                       ensure those devices are resumed, before returning failure.
                    */
                    handoverProfile_HaltOrResumeAclLinks(FALSE, mirrored, device);
                    return result;
                }
            }
            else
            {
                result = handoverProfile_ResumeLink(device);
            }
        }
    }

    return result;
}

/* Iterates through the handover device list and prepares/cancels prepared devices one by one. If prepare request for a  
   device veteod/timedout, this function will recursively find all the previously prepared devices and issues a cancel
   prepare request before returning a failure.
*/
static handover_profile_status_t handoverProfile_PrepareAclLinks(bool prepare, bool mirrored, handover_device_t *end_device)
{
    handover_profile_status_t result = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        /* If device in the handover list matches with end_device, either we have cancelled the prepare request for
           all the previously prepared devices OR there are no previously prepared devices to issue a cancel prepare.
        */
        if (end_device == device)
        {
            break;
        }

        if (device->is_mirrored == mirrored)
        {
            DEBUG_LOG_INFO("handoverProfile_PrepareAclLinks lap:0x%x", device->addr.taddr.addr.lap);
            if (prepare)
            {
                result = handoverProfile_PrepareLink(device);
                if (result != HANDOVER_PROFILE_STATUS_SUCCESS)
                {
                    /* Prepare request failed for this device. Recursively try to find already prepared devices and
                       ensure cancel handover is called for those devices, before returning failure.
                    */
                    handoverProfile_PrepareAclLinks(FALSE, mirrored, device);
                    return result;
                }
            }
            else
            {
                result = handoverProfile_CancelPrepareLink(device);
            }
        }
    }

    return result;
}

static handover_profile_status_t handoverProfile_HaltInactiveAclLinks(void)
{
    return handoverProfile_HaltOrResumeAclLinks(TRUE, FALSE, NULL);
}

static handover_profile_status_t handoverProfile_HaltActiveAclLinks(void)
{
    return handoverProfile_HaltOrResumeAclLinks(TRUE, TRUE, NULL);
}

static handover_profile_status_t handoverProfile_ResumeInactiveAclLinks(void)
{
    return handoverProfile_HaltOrResumeAclLinks(FALSE, FALSE, NULL);
}

static handover_profile_status_t handoverProfile_ResumeActiveAclLinks(void)
{
    return handoverProfile_HaltOrResumeAclLinks(FALSE, TRUE, NULL);
}

static handover_profile_status_t handoverProfile_PrepareInactiveAclLinks(void)
{
    return handoverProfile_PrepareAclLinks(TRUE, FALSE, NULL);
}

static handover_profile_status_t handoverProfile_PrepareActiveAclLinks(void)
{
    return handoverProfile_PrepareAclLinks(TRUE, TRUE, NULL);
}

static handover_profile_status_t handoverProfile_CancelPrepareInactiveAclLinks(void)
{
    return handoverProfile_PrepareAclLinks(FALSE, FALSE, NULL);
}

static handover_profile_status_t handoverProfile_CancelPrepareActiveAclLinks(void)
{
    return handoverProfile_PrepareAclLinks(FALSE, TRUE, NULL);
}

/*! \brief Wait for a packet to be processed by the transform connecting the
           A2DP media source to the audio subsystem

    \note If A2DP is streaming, the effective handover time can be reduced by
    starting handover immediately after a packet is received from the handset. A
    proportion of the handover time will then occur in the gap before the next
    packet. This increases the overall handover time from the perspective of the
    procedure that initiates the handover (since the software waits for a packet
    before even starting to handover), but reduces the chance of there being a
    audio glitch as the packet that is received can be decoded and rendered
    whilst the handover is performed.
*/
static handover_profile_status_t handoverProfile_WaitForA2dpPacket(void)
{
    Transform trans;

    HandoverPioSet();

    trans =  Kymera_GetA2dpMediaStreamTransform();
    if (trans && HANDOVER_PROFILE_A2DP_HANDOVER_WAIT_FOR_PACKET_TIMEOUT_MS)
    {
        uint32 timeout = VmGetClock() + HANDOVER_PROFILE_A2DP_HANDOVER_WAIT_FOR_PACKET_TIMEOUT_MS;

        /* Read once to clear flag */
        TransformPollTraffic(trans);

        while (VmGetClock() < timeout)
        {
            if (TransformPollTraffic(trans))
            {
                DEBUG_LOG("handoverProfile_WaitForPacket received packet");
                break;
            }
        }
    }
    HandoverPioClr();

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_SetPeerLinkActiveMode(void)
{
    /* For A2DP handover the peer link policy may be changed to active mode at
       the same time as the controller prepares for handover. This reduces the
       handover time. For other handover types the link policy must be changed
       before the controller prepares for handover.
    */
    if (MirrorProfile_IsA2dpActive())
    {
        MirrorProfile_UpdatePeerLinkPolicy(lp_active);
    }
    else
    {
        if(!MirrorProfile_UpdatePeerLinkPolicyBlocking(lp_active, HANDOVER_PROFILE_EXIT_SNIFF_TIMEOUT_MSEC))
        {
            DEBUG_LOG_INFO("handoverProfile_SetPeerLinkActiveMode Could not exit sniff mode");
            return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_SetPeerLinkSniffMode(void)
{
    if (MirrorProfile_IsA2dpActive())
    {
        /* For A2DP handover, first wait for the link to go active as it was previously
           set into sniff mode, but did not wait for the link mode to change */
        if(!MirrorProfile_WaitForPeerLinkMode(lp_active, HANDOVER_PROFILE_EXIT_SNIFF_TIMEOUT_MSEC))
        {
            /* Ignore failure */
            DEBUG_LOG_INFO("handoverProfile_SetPeerLinkSniffMode timeout waiting to enter active mode");
        }
    }

    if(!MirrorProfile_UpdatePeerLinkPolicyBlocking(lp_sniff, HANDOVER_PROFILE_REENTER_SNIFF_TIMEOUT_MSEC))
    {
        /* Ignore failure */
        DEBUG_LOG_INFO("handoverProfile_SetPeerLinkSniffMode timeout wait to re-enter sniff mode");
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

static handover_profile_status_t handoverProfile_WaitForPeerLinkActiveMode(void)
{
    if(!MirrorProfile_WaitForPeerLinkMode(lp_active, HANDOVER_PROFILE_EXIT_SNIFF_TIMEOUT_MSEC))
    {
        DEBUG_LOG_INFO("HandoverProfile_PrepareForMarshal Could not exit sniff mode");
        return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
    }
    MirrorProfile_HandoverRefreshSubrate();
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

handover_profile_status_t handoverProfile_SendInactiveLinkMarshalData(void)
{
    handover_profile_status_t status = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE_CONDITIONAL(device, status == HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        if (!device->is_mirrored)
        {
            status = handoverProtocol_SendBtStackMarshalData(device);
        }
    }
    return status;
}

handover_profile_status_t handoverProfile_SendActiveLinkMarshalData(void)
{
    handover_profile_status_t status = HANDOVER_PROFILE_STATUS_SUCCESS;

    FOR_EACH_HANDOVER_DEVICE_CONDITIONAL(device, status == HANDOVER_PROFILE_STATUS_SUCCESS)
    {
        if (device->is_mirrored)
        {
            status = handoverProtocol_SendBtStackMarshalData(device);
        }
    }
    return status;
}

static handover_profile_status_t handoverProfile_CommitSecondaryRole(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    const bool secondary_role = FALSE;
    uint8 mirrored_dev_count = 0;

    /* Set mirror profile to new role and swap their peer address */
    MirrorProfile_SetRoleAndSwapPeerAddress(secondary_role);

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        /* Commit P1 Clients */
        handoverProfile_CommitP1Clients(&device->addr, secondary_role);

        if (device->is_mirrored)
        {
            /* Commit BT stack only for mirrored device */
            PanicFalse(AclHandoverRoleCommit(device->handle, ROLE_TYPE_SECONDARY));
            mirrored_dev_count++;
        }
    }

    /* When there are no BR/EDR mirrored devices, AclHandoverCommit shall be called with HANDOVER_PROFILE_INVALID_HANDLE and mirror role type.
       This makes P0/BTSS to exchange peer address and respective roles. Also may trigger interchange of CISes ownership.
       Note: LE ACL are never mirrored for LE-Audio(only CISes are mirrored/delegated)
    */
    if (!mirrored_dev_count)
    {
       PanicFalse(AclHandoverRoleCommit(HANDOVER_PROFILE_INVALID_HANDLE, ROLE_TYPE_SECONDARY));
    }

    /* Wait for sniff mode which should have been triggered from new primary after it's commit phase */
    if (!MirrorProfile_WaitForPeerLinkMode(lp_sniff, HANDOVER_PROFILE_REENTER_SNIFF_TIMEOUT_MSEC))
    {
        DEBUG_LOG_INFO("handoverProfile_CommitSecondaryRole timeout waiting to re-enter sniff mode");
    }

    TimestampEvent(TIMESTAMP_EVENT_PRI_HANDOVER_COMPLETED);

    /* Call P1 complete() */
    handoverProfile_CompleteP1Clients(secondary_role);

    /* Update the new peer address */
    PanicFalse(appDeviceGetPeerBdAddr(&ho_inst->peer_addr));
    ho_inst->is_primary = secondary_role;

    DEBUG_LOG_INFO("handoverProfile_CommitSecondaryRole: I am new Secondary");

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

#endif
