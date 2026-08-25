/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Implementation of handover in the acceptor device.
*/

#ifdef INCLUDE_MIRRORING

#include "handover_profile_private.h"
#include "tws_topology.h"

typedef enum handover_profile_acceptor_states
{
    HO_ACCEPTOR_STATE_IDLE,
    HO_ACCEPTOR_STATE_SETUP,
    HO_ACCEPTOR_STATE_RECEIVED_APPSP1_MARSHAL_DATA,
    HO_ACCEPTOR_STATE_APPSP1_UNMARSHAL_COMPLETE,
    HO_ACCEPTOR_STATE_RECEIVED_BTSTACK_MARSHAL_DATA,
} handover_profile_acceptor_state_t;

static handover_profile_acceptor_state_t acceptor_state;

static void handoverProfile_SecondaryBecomePrimary(void);
static void handoverProfile_AcceptorCleanup(void);
static bool handoverProfile_AcceptorVeto(uint8 pri_tx_seq, uint8 pri_rx_seq, uint16 mirror_state);
static void handoverProfile_PopulateDeviceList(const HANDOVER_PROTOCOL_START_REQ_T *req);
static bool handoverProfile_ApplyBtStackData(Source source, uint16 len, bool mirrored);
static void handoverProfile_CommitDevice(handover_device_t *device, bool wait_for_data_ack_transfer);
static void handoverProfile_AcceptorWaitForBtStackDataAckTransfer(void);
static void handoverProfile_AcceptorSetPeerLinkSniffMode(void);
static bool handoverProfile_AllDeviceBtStackDataReceived(void);

handover_profile_status_t handoverProfile_AcceptorStart(const HANDOVER_PROTOCOL_START_REQ_T *req)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    if(ho_inst->is_primary)
    {
        DEBUG_LOG_INFO("handoverProfile_AcceptorStart not secondary");
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
    if (ho_inst->state != HANDOVER_PROFILE_STATE_CONNECTED)
    {
        DEBUG_LOG_INFO("handoverProfile_AcceptorStart not connected");
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }

    ho_inst->handover_type = handover_type_standard_acceptor;

    handoverProfile_AcceptorCleanup();
    handoverProfile_PopulateDeviceList(req);

    if (handoverProfile_AcceptorVeto(req->last_tx_seq, req->last_rx_seq, req->mirror_state))
    {
        handoverProfile_AcceptorCleanup();
        return HANDOVER_PROFILE_STATUS_HANDOVER_VETOED;
    }

    acceptor_state = HO_ACCEPTOR_STATE_SETUP;

    return HANDOVER_PROFILE_STATUS_SUCCESS;
}

void handoverProfile_AcceptorCancel(void)
{
    handoverProfile_AcceptorCleanup();
}

handover_profile_status_t handoverProfile_AcceptorHandleAppsP1Data(Source source, uint16 len)
{
    switch (acceptor_state)
    {
        case HO_ACCEPTOR_STATE_SETUP:
        case HO_ACCEPTOR_STATE_RECEIVED_APPSP1_MARSHAL_DATA:
        {
            FOR_EACH_HANDOVER_DEVICE(device)
            {
                if (!device->u.s.appsp1_unmarshal_complete)
                {
                    while (len)
                    {
                        const uint8 *address = SourceMap(source);
                        uint16 consumed = 0;
                        PanicFalse(handoverProfile_UnmarshalP1Client(&device->addr, address, len, &consumed));
                        PanicFalse(consumed <= len);
                        SourceDrop(source, consumed);
                        len -= consumed;
                    }
                    device->u.s.appsp1_unmarshal_complete = TRUE;
                    break;
                }
            }
            /* Failed to consume all the marshal data */
            PanicNotZero(len);

            FOR_EACH_HANDOVER_DEVICE(device)
            {
                if (!device->u.s.appsp1_unmarshal_complete)
                {
                    /* Successful unmarshalling but not yet complete */
                    acceptor_state = HO_ACCEPTOR_STATE_RECEIVED_APPSP1_MARSHAL_DATA;
                    return HANDOVER_PROFILE_STATUS_SUCCESS;
                }
            }
            /* All devices now unmarshalled */
            acceptor_state = HO_ACCEPTOR_STATE_APPSP1_UNMARSHAL_COMPLETE;
            appPowerPerformanceProfileRequest();
        }
        return HANDOVER_PROFILE_STATUS_SUCCESS;

        default:
        {
            DEBUG_LOG_WARN("handoverProfile_AcceptorHandleAppsP1Data invalid state %d", acceptor_state);
            SourceDrop(source, len);
        }
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
}

handover_profile_status_t handoverProfile_AcceptorHandleBtStackData(Source source, uint16 len)
{
    DEBUG_LOG_WARN("handoverProfile_AcceptorHandleBtStackData state enum:handover_profile_acceptor_state_t:%d", acceptor_state);

    switch (acceptor_state)
    {
        case HO_ACCEPTOR_STATE_RECEIVED_BTSTACK_MARSHAL_DATA:
        case HO_ACCEPTOR_STATE_APPSP1_UNMARSHAL_COMPLETE:
        {
            acceptor_state = HO_ACCEPTOR_STATE_RECEIVED_BTSTACK_MARSHAL_DATA;

            /* Marshal data is either for mirrored or non-mirrored device.
               Try unmarshalling the non-mirrored device first */
            bool non_mirrored_done = handoverProfile_ApplyBtStackData(source, len, FALSE);
            if (!non_mirrored_done)
            {
                /* If the non mirrored didn't complete (or it may already have completed
                   when receiving a previous packet, this must be for the mirrored device */
                bool mirrored_done = handoverProfile_ApplyBtStackData(source, len, TRUE);
                PanicFalse(mirrored_done);
            }

            /* If all handover devices' BT stack data is received and applied */
            if (handoverProfile_AllDeviceBtStackDataReceived())
            {
                {
                    /* Commit to new Primay role */
                    handoverProfile_SecondaryBecomePrimary();
                }
                handoverProfile_AcceptorCleanup();
            }
        }
        return HANDOVER_PROFILE_STATUS_SUCCESS;

        default:
        {
            DEBUG_LOG_WARN("handoverProfile_AcceptorHandleBtStackData invalid state %d", acceptor_state);
            SourceDrop(source, len);
        }
        return HANDOVER_PROFILE_STATUS_HANDOVER_FAILURE;
    }
}

bool handoverProfile_AcceptorIsAppsP1UnmarshalComplete(void)
{
    return (acceptor_state == HO_ACCEPTOR_STATE_APPSP1_UNMARSHAL_COMPLETE);
}

static bool handoverProfile_AllDeviceBtStackDataReceived(void)
{
    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (!device->u.s.btstack_unmarshal_complete)
        {
            return FALSE;
        }
    }
    return TRUE;
}

static void handoverProfile_AcceptorCleanup(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    handover_device_t *next;

    switch (acceptor_state)
    {
        case HO_ACCEPTOR_STATE_RECEIVED_BTSTACK_MARSHAL_DATA:
        // fallthrough

        case HO_ACCEPTOR_STATE_APPSP1_UNMARSHAL_COMPLETE:
            appPowerPerformanceProfileRelinquish();
        // fallthrough

        case HO_ACCEPTOR_STATE_RECEIVED_APPSP1_MARSHAL_DATA:
            /* Call abort to free up any P1 unmarshalled data */
            {
                handoverProfile_AbortP1Clients();
            }
        // fallthrough

        case HO_ACCEPTOR_STATE_SETUP:
        case HO_ACCEPTOR_STATE_IDLE:
        break;

        default:
            Panic();
            break;
    }

    next = ho_inst->device_list;
    while (next)
    {
        handover_device_t *current = next;
        SinkClose(current->u.s.btstack_sink);
        next = current->next;
        current->next = NULL;
        free(current);
    }
    ho_inst->device_list = NULL;

    ho_inst->handover_type = handover_type_not_in_progress;

    acceptor_state = HO_ACCEPTOR_STATE_IDLE;
}

static bool handoverProfile_AcceptorVeto(uint8 pri_tx_seq, uint8 pri_rx_seq, uint16 mirror_state)
{
    uint8 sec_tx_seq = appPeerSigGetLastTxMsgSequenceNumber();
    uint8 sec_rx_seq = appPeerSigGetLastRxMsgSequenceNumber();

    /* Veto handover if current topology state is not secondary. There can be instances where 
    secondary topology just decided to become idle state (Eg. due to an incase) and before it
    proceeds, primary triggers a handover. In such cases handover needs to be vetoed. */
    if(!TwsTopology_IsSecondary())
    {
        return TRUE;
    }

    /* Validate if the received and the transmitted peer signalling messages are
    same on both earbuds. If the same, this means there
    are no in-flight peer signalling message. If not, veto to allow time for the
    messages to be cleared. */
    if(sec_rx_seq != pri_tx_seq || pri_rx_seq != sec_tx_seq)
    {
        DEBUG_LOG_INFO("HandoverProfile_AcceptorVeto: PriTx:%x PriRx:%x SecTx:%x SecRx:%x",
                    pri_tx_seq, pri_rx_seq, sec_tx_seq, sec_rx_seq);
        return TRUE;
    }

    if (mirror_state != MirrorProfile_GetMirrorState())
    {
        DEBUG_LOG_INFO("HandoverProfile_AcceptorVeto: Mirror state mismatch: 0x%x 0x%x",
                        mirror_state, MirrorProfile_GetMirrorState());
        return TRUE;
    }

    if (MirrorProfile_Veto() || kymera_a2dp_mirror_handover_if.pFnVeto())
    {
        return TRUE;
    }

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (appAvInstanceFindFromBdAddr(&device->addr.taddr.addr))
        {
            /* AV instance is present on Acceptor. This is only possible if disconnection caused by
            previous handover is not complete yet. */
            DEBUG_LOG_INFO("HandoverProfile_AcceptorVeto: AV instance exists");
            return TRUE;
        }
    }
    return FALSE;
}

static void handoverProfile_PopulateDeviceList(const HANDOVER_PROTOCOL_START_REQ_T *req)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    unsigned device_counter;

    /* Make a list of handover devices. The order of devices in the start
       request determines the order in which devices are prepared and
       marshalled/unmarshalled during the handover procedure. Therefore it is
       critical for the devices to be added to the device_list in the same order
       as in the start request message. */
    handover_device_t **next = &ho_inst->device_list;

    for (device_counter = 0; device_counter < req->number_of_devices; device_counter++)
    {
        handover_device_t *device = PanicUnlessMalloc(sizeof(*device));
        memset(device, 0, sizeof(*device));
        device->addr = req->address[device_counter];
        /* Check if this device is the mirrored device. Only BR/EDR ACL is considered to be the mirrored device.
           Because LE ACL will never be mirrored, only CISes are mirrored/delegated for LE-Audio 
        */
        device->is_mirrored = MirrorProfile_IsSameMirroredDevice(&device->addr);

        if (device->is_mirrored)
        {
            device->u.s.btstack_sink = StreamAclMarshalSink(&device->addr);
            device->handle = MirrorProfile_GetMirrorAclHandle();
        }
        else
        {
            device->u.s.btstack_sink = StreamAclEstablishSink(&device->addr);
            device->handle = HANDOVER_PROFILE_INVALID_HANDLE;
        }

        *next = device;
        next = &device->next;
    }
}

static bool handoverProfile_ApplyBtStackData(Source source, uint16 len, bool mirrored)
{
    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->is_mirrored == mirrored)
        {
            if (!device->u.s.btstack_unmarshal_complete)
            {
                uint16 moved;
                Sink sink = device->u.s.btstack_sink;
                PanicZero(sink);
                moved = StreamMove(sink, source, len);
                PanicFalse(moved == len);
                device->u.s.btstack_unmarshal_complete = TRUE;
                device->u.s.btstack_data_len = len;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void handoverProfile_CommitDevice(handover_device_t *device, bool wait_for_data_ack_transfer)
{
    /* P1 commit is postponed only for the BREDR device that is mirroring a2dp */
    bool p1_commit_first = !device->is_mirrored ||!MirrorProfile_IsA2dpActive();

    /* Flush BT stack data */
    bool flush;

    /* For LE audio, only the CISes are mirrored. The LE ACL is never mirrored.
       Hence only BR/EDR ACL shall be considered as mirrored device.
       Use SinkFlush for mirrored device and SinkFlushBlocking for non-mirrored device 
    */
    if (device->is_mirrored)
    {
        DEBUG_LOG_INFO("handoverProfile_SecondaryCommitDevice flushed %d", device->u.s.btstack_data_len);
        flush = SinkFlush(device->u.s.btstack_sink, device->u.s.btstack_data_len);
    }
    else
    {
        DEBUG_LOG_INFO("handoverProfile_SecondaryCommitDevice blocking flushed %d", device->u.s.btstack_data_len);
        flush = SinkFlushBlocking(device->u.s.btstack_sink, device->u.s.btstack_data_len);
    }
    PanicFalse(flush);

    /* If esco mirroring is active, there could be chance of delayed ACK transferred to old primary.
     * So make sure to commit ACL little delayed to avoid any link role switch which causes to change DAC(device access code)
     */
    if (wait_for_data_ack_transfer)
    {
        handoverProfile_AcceptorWaitForBtStackDataAckTransfer();
    }

    /* For A2DP mirroring, the earliest the new primary bud may receive
       data from the handset is after the buds re-enter sniff mode. This
       means the P1 commit can be deferred in this mode until after the
       enter sniff command has been sent to the controller */
    if (p1_commit_first)
    {
        /* Commit P1 clients */
        handoverProfile_CommitP1Clients(&device->addr, TRUE);
    }

    /* Commit P0/BTSS clients */
    /* For LE audio, only the CISes are mirrored. The LE ACL is never mirrored.
       Hence only BR/EDR ACL shall be considered as mirrored device.
       Use AclHandoverRoleCommit for mirrored device and AclEstablishCommit for non-mirrored device
    */
     device->is_mirrored ? AclHandoverRoleCommit(device->handle, ROLE_TYPE_PRIMARY) : AclEstablishCommit(&device->addr);

    if (device->is_mirrored)
    {
        /* Prepare mirror profile to have updated with role and swap peer address before setting link policy mode */
        handoverProfile_AcceptorSetPeerLinkSniffMode();
    }

    if (!p1_commit_first)
    {
        /* Commit P1 clients */
        handoverProfile_CommitP1Clients(&device->addr, TRUE);
    }

    if (device->is_mirrored)
    {
        if (!MirrorProfile_WaitForPeerLinkMode(lp_sniff, HANDOVER_PROFILE_REENTER_SNIFF_TIMEOUT_MSEC))
        {
            DEBUG_LOG_INFO("handoverProfile_SecondaryCommitDevice timeout waiting to re-enter sniff mode");
        }
    }
}

static void handoverProfile_AcceptorWaitForBtStackDataAckTransfer(void)
{
    uint32 timeout = VmGetClock() + HANDOVER_PROFILE_STACK_MARSHAL_DATA_ACK_TIMEOUT_MSEC;
    do
    {
       /*do nothing */
    }while (VmGetClock() < timeout);
}

static void handoverProfile_AcceptorSetPeerLinkSniffMode(void)
{
    MirrorProfile_HandoverRefreshSubrate();
    MirrorProfile_UpdatePeerLinkPolicy(lp_sniff);
}

static void handoverProfile_SecondaryBecomePrimary(void)
{
    handover_profile_task_data_t *ho_inst = Handover_GetTaskData();
    uint8 mirrored_dev_count = 0;
    const bool primary_role = TRUE;
    bool wait_for_data_ack_transfer = MirrorProfile_IsEscoActive() || MirrorProfile_IsCisMirroringConnected();

    /* Set mirror profile to new role and swap their peer address */
    MirrorProfile_SetRoleAndSwapPeerAddress(primary_role);

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (device->is_mirrored)
        {
            handoverProfile_CommitDevice(device, wait_for_data_ack_transfer);
            mirrored_dev_count++;
            wait_for_data_ack_transfer = FALSE;
        }
    }

    /* When there are no BR/EDR mirrored devices, AclHandoverRoleCommit shall be called with HANDOVER_PROFILE_INVALID_HANDLE.
       This makes P0/BTSS to exchange peer address and respective roles. Also may trigger interchange of CISes ownership.
       Note: Since LE ACL are never mirrored for LE-Audio(only CISes are mirrored/delegated)
    */
    if (!mirrored_dev_count)
    {
        /* Before we exchange the peer address and respective roles, ensure we give enough time for the BT stack data
           ACK to get transferred.
         */
        if (wait_for_data_ack_transfer)
        {
            handoverProfile_AcceptorWaitForBtStackDataAckTransfer();
            wait_for_data_ack_transfer = FALSE;
        }
        AclHandoverRoleCommit(HANDOVER_PROFILE_INVALID_HANDLE, ROLE_TYPE_PRIMARY);
    }

    FOR_EACH_HANDOVER_DEVICE(device)
    {
        if (!device->is_mirrored)
        {
            handoverProfile_CommitDevice(device, wait_for_data_ack_transfer);
            wait_for_data_ack_transfer = FALSE;
        }
    }
    /* Complete P1 clients*/
    handoverProfile_CompleteP1Clients(TRUE);

    /* Update the new peer address */
    PanicFalse(appDeviceGetPeerBdAddr(&ho_inst->peer_addr));
    ho_inst->is_primary = primary_role;

    /* This ensures Performance mode relinquished in handover success case. For failure case, it is taken care in
     * handoverProfile_AcceptorCleanup.
     */
    appPowerPerformanceProfileRelinquish();

    acceptor_state = HO_ACCEPTOR_STATE_IDLE;

    DEBUG_LOG_INFO("handoverProfile_SecondaryBecomePrimary: I am new Primary");
}


#endif
