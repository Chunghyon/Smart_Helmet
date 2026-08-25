/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Core procedures used by the handover profile to perform a handover.
*/

#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"
#include "handover_profile_procedures.h"

handover_profile_status_t handoverProfile_PerformanceRequest(void)
{
    appPowerPerformanceProfileRequest();
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

handover_profile_status_t handoverProfile_PerformanceRelinquish(void)
{
    appPowerPerformanceProfileRelinquish();
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

/* Check if all outbound data has been transmitted */
static bool handoverProfile_IsAclTransmitPending (const tp_bdaddr *addr, uint32 t)
{
    uint32 timeout = 0;
    bool timedout = TRUE;
    timeout = VmGetClock() + t;
    do
    {
        if(!AclTransmitDataPending(addr))
        {
            timedout = FALSE;
            break;
        }
    } while(VmGetClock() < timeout);

    return timedout;
}

handover_profile_status_t handoverProfile_HaltLink(handover_device_t *device)
{
    tp_bdaddr *addr = &device->addr;

    if (AclReceiveEnable(addr, FALSE, HANDOVER_PROFILE_ACL_RECEIVE_ENABLE_TIMEOUT_USEC))
    {
        if (AclReceivedDataProcessed(addr, HANDOVER_PROFILE_ACL_RECEIVED_DATA_PROCESSED_TIMEOUT_USEC) == ACL_RECEIVE_DATA_PROCESSED_COMPLETE)
        {
            if (!handoverProfile_IsAclTransmitPending(addr, HANDOVER_PROFILE_ACL_TRANSMIT_DATA_PENDING_TIMEOUT_MSEC))
            {
                return HANDOVER_PROFILE_STATUS_SUCCESS;
            }
            else
            {
                DEBUG_LOG_INFO("handoverProfile_HaltDevice AclTransmitDataPending timeout/failed");
            }
        }
        else
        {
            DEBUG_LOG_INFO("handoverProfile_HaltDevice AclReceivedDataProcessed timeout/failed");
        }
    }
    else
    {
        DEBUG_LOG_INFO("handoverProfile_HaltDevice AclReceiveEnable(false) timeout/failed");
    }
    return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
}

handover_profile_status_t handoverProfile_ResumeLink(handover_device_t *device)
{
    tp_bdaddr *addr = &device->addr;

    if (!AclReceiveEnable(addr, TRUE, HANDOVER_PROFILE_ACL_RECEIVE_ENABLE_TIMEOUT_USEC))
    {
        /* Ignore failure to resume as a link. */
        DEBUG_LOG_INFO("handoverProfile_ResumeLink AclReceiveEnable timeout/failed");
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}


handover_profile_status_t handoverProfile_PrepareLink(handover_device_t *device)
{
    lp_power_mode mode;
    uint32 timeout = 0;
    bool timedout = TRUE;
    tp_bdaddr tp_peer_addr;
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();

    PanicFalse(ConManagerGetPowerMode(&device->addr, &mode));
    if (mode == lp_sniff)
    {
        uint16 sniff_slots;
        PanicFalse(ConManagerGetSniffInterval(&device->addr, &sniff_slots));
        timeout = VmGetClock() + (rtime_get_sniff_interval_in_ms(sniff_slots) * HANDOVER_PROFILE_NO_OF_TIMES_SNIFF_INTERVAL);
    }
    else
    {
        timeout = VmGetClock() + HANDOVER_PROFILE_ACL_HANDOVER_PREPARE_TIMEOUT_MSEC;
    }

    BdaddrTpFromBredrBdaddr(&tp_peer_addr, &ho_inst->peer_addr);
    do
    {
        if ((device->handle = AclHandoverPrepare(&device->addr, (const tp_bdaddr *)&tp_peer_addr)) != 0xFFFF)
        {
            acl_handover_prepare_status prepared_status;

             /* Wait until AclHandoverPrepared returns ACL_HANDOVER_PREPARE_COMPLETE or ACL_HANDOVER_PREPARE_FAILED */
            while ((prepared_status = AclHandoverPrepared(device->handle)) == ACL_HANDOVER_PREPARE_IN_PROGRESS)
            {
                /* Do nothing */
            }

            /* Exit loop if handover prepare completed */
            if (prepared_status == ACL_HANDOVER_PREPARE_COMPLETE)
            {
                device->u.p.btstack_source = StreamAclMarshalSource(&device->addr);
                /* Kick the source to marshal the upper stack data */
                SourceDrop(device->u.p.btstack_source, 0);
                device->u.p.btstack_data_len = SourceSize(device->u.p.btstack_source);
                timedout = FALSE;
                break;
            }
        }
    } while (VmGetClock() < timeout);

    /* AclHandoverPrepare didn't succeed equivalent to veto */
    if (device->handle == 0xFFFF)
    {
        DEBUG_LOG_INFO("handoverProfile_PrepareLink vetoed");
        return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
    }

    if (timedout)
    {
        DEBUG_LOG_INFO("handoverProfile_PrepareLink timedout");
        return HANDOVER_PROFILE_STATUS_HANDOVER_TIMEOUT;
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

handover_profile_status_t handoverProfile_CancelPrepareLink(handover_device_t *device)
{
    if (device->handle != 0xffff)
    {
        if (!AclHandoverCancel(device->handle))
        {
            /* Ignore failure to cancel prepare */
            DEBUG_LOG_INFO("handoverProfile_CancelPrepareLink failed");
        }
    }
    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

handover_profile_status_t handoverProfile_ClearPendingPeerData(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    tp_bdaddr peer_tp_addr;

    BdaddrTpFromBredrBdaddr(&peer_tp_addr, &ho_inst->peer_addr);

    if (handoverProfile_IsAclTransmitPending(&peer_tp_addr, HANDOVER_PROFILE_P0_TRANSMIT_DATA_PENDING_TIMEOUT_MSEC))
    {
        DEBUG_LOG_INFO("handoverProfile_ClearPendingPeerData timeout");
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
    else
    {
        return HANDOVER_PROFILE_STATUS_SUCCESS;
    }
}

handover_profile_status_t handoverProfile_Panic(void)
{
    Panic();
    return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
}

#endif
