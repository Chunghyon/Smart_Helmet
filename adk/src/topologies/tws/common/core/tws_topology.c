/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      TWS Topology component core.
*/

#include "tws_topology.h"
#include "tws_topology_private.h"
#include "tws_topology_role_change_client_notifier.h"
#include "tws_topology_peer_sig.h"
#include "tws_topology_config.h"
#include "tws_topology_sdp.h"
#include "tws_topology_peer_sig.h"
#include "tws_topology_advertising.h"
#include <hdma.h>
#include <peer_find_role.h>
#include <state_proxy.h>
#include <phy_state.h>
#include <bt_device.h>
#include <mirror_profile.h>
#include <peer_signalling.h>
#include <le_advertising_manager.h>
#include <cc_with_case.h>

#include <task_list.h>
#include <logging.h>
#include <message.h>

#include <panic.h>
#include <bdaddr.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_internal_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(tws_topology_client_notifier_message_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TWS_TOPOLOGY, TWS_TOPOLOGY_MESSAGE_END)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TWS_TOPOLOGY_CLIENT_NOTIFIER, TWS_TOPOLOGY_CLIENT_NOTIFIER_MESSAGE_END)


/*! Instance of the TWS Topology. */
twsTopologyTaskData tws_topology = {0};

/*! \brief Handle ACL (dis)connections. */
static void twsTopology_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T* ind)
{
    if(!ind->ble)
    {
        twsTopology_UpdateAdvertisingParams();

        if (appDeviceIsPeer(&ind->bd_addr))
        {
            if (!ind->connected)
            {
                twsTopology_SmHandlePeerLinkDisconnected(TwsTopology_GetSm());
            }
            else
            {
               /* The earbud should switch to become primary if it is currently in acting primary state.
                * There can be rare instances when we perform quick incase/outcase of the earbuds,
                * one earbud which was elected secondary in the earlier peer find role stops
                * advertising/scanning and starts paging.Now the other earbud when kept in case and
                * brought back again moves to acting primary state since it won't be able to find the peer and
                * starts page scan.Now, the secondary earbud connects to the acting primary.In such cases, the earbud
                * which is in acting primary state should switch to become primary once it is connected to the secondary.*/
                if(TwsTopology_IsActingPrimary())
                {
                    twsTopology_SmHandleElectedPrimary(TwsTopology_GetSm());
                }
            }
        }
        twsTopology_SmKick(TwsTopology_GetSm());
    }
}

static void twsTopology_HandlePairingActivity(const PAIRING_ACTIVITY_T *message)
{
    DEBUG_LOG("twsTopology_HandlePairingActivity status=enum:pairingActivityStatus:%d",
                message->status);

    switch(message->status)
    {
        case pairingActivitySuccess:
            /* just completed pairing, kick the SM, because it might need to start HDMA,
            * necessary because the normal checks to start HDMA triggered by
            * CON_MANAGER_CONNECTION_INDs will not succeed immediately after pairing
            * because the link type would not have been known to be a handset */
            twsTopology_SmKick(TwsTopology_GetSm());
            break;

        case pairingActivityInProgress:
        case pairingActivityNotInProgress:
        {
            twsTopology_UpdateAdvertisingParams();
        }
        break;
        default:
            break;
    }
}


/*! \brief TWS Topology message handler.
 */
static void twsTopology_HandleMessage(Task task, MessageId id, Message message)
{
    tws_topology_sm_t *sm = TwsTopology_GetSm();

    UNUSED(task);

    switch (id)
    {
        case PEER_FIND_ROLE_ACTING_PRIMARY:
            twsTopology_SmHandleElectedActingPrimary(sm);
            break;
        case PEER_FIND_ROLE_PRIMARY:
            twsTopology_SmHandleElectedPrimary(sm);
            break;
        case PEER_FIND_ROLE_SECONDARY:
            twsTopology_SmHandleElectedSecondary(sm);
            break;

        case PAIRING_ACTIVITY:
            twsTopology_HandlePairingActivity(message);
            break;

        case STATE_PROXY_EVENT_INITIAL_STATE_RECEIVED:
            twsTopology_SmKick(sm);
            break;

        case MIRROR_PROFILE_CONNECT_IND:
            /* This message indicates the mirroring ACL is setup, this may have
            occurred after HDMA has issued a handover decision that was ignored
            by the SM's rules (due to mirroring not being setup yet), so need to
            kick the SM to reevaluate if a handover needs to be started */
            twsTopology_SmKick(sm);
            break;

        case PHY_STATE_CHANGED_IND:
            twsTopology_SmKick(sm);
            break;

        case CON_MANAGER_CONNECTION_IND:
            twsTopology_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            TwsTopology_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            TwsTopology_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        case TWSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP:
            twsTopology_SmHandleInternalStopTimeout(sm);
            break;

        case TWSTOP_INTERNAL_KICK_SM:
            twsTopology_SmKick(sm);
            break;

        case TWSTOP_INTERNAL_RETRY_HANDOVER:
            twsTopology_SmHandleInternalRetryHandover(sm);
            break;

        case CL_SDP_REGISTER_CFM:
        {
            CL_SDP_REGISTER_CFM_T *cfm = (CL_SDP_REGISTER_CFM_T*)message;
            TwsTopology_HandleSdpRegisterCfm(TwsTopologyGetTask(), cfm->status == sds_status_success, cfm->service_handle);
            break;
        }
        case CL_SDP_UNREGISTER_CFM:
	    {
			CL_SDP_UNREGISTER_CFM_T * cfm = (CL_SDP_UNREGISTER_CFM_T*)message;
			if(cfm->status != sds_status_pending)
			{
					TwsTopology_HandleSdpUnregisterCfm(TwsTopologyGetTask(), cfm->status == sds_status_success, cfm->service_handle);
			}
			else
			{
			/* Wait for final confirmation message */
			}
			break;
		}

        case HDMA_HANDOVER_NOTIFICATION:
            twsTopology_SmHandleHDMARequest(sm, (hdma_handover_decision_t*)message);
            break;

        case HDMA_CANCEL_HANDOVER_NOTIFICATION:
            twsTopology_SmHandleHDMACancelHandover(sm);
            break;

#ifdef INCLUDE_CASE_COMMS
        case CASE_LID_STATE:
            twsTopology_SmKick(sm);
            break;
#endif

        default:
            break;
    }
}

static void twsTopology_RegisterForStateProxyEvents(void)
{
    if (TwsTopologyConfig_StateProxyRegisterEvents())
    {
        StateProxy_EventRegisterClient(TwsTopologyGetTask(), TwsTopologyConfig_StateProxyRegisterEvents());
    }
}

static void twsTopology_RegisterBtParameters(void)
{
    BredrScanManager_PageScanParametersRegister(&page_scan_params);
    BredrScanManager_InquiryScanParametersRegister(&inquiry_scan_params);
    PanicFalse(LeAdvertisingManager_ParametersRegister(&le_adv_params));
}

static void twsTopology_SelectBtParameters(void)
{
    PanicFalse(LeAdvertisingManager_ParametersSelect(LE_ADVERTISING_PARAMS_SET_TYPE_FAST));
}

bool TwsTopology_Init(Task init_task)
{
    twsTopologyTaskData *tws_taskdata = TwsTopologyGetTaskData();

    UNUSED(init_task);

    tws_taskdata->task.handler = twsTopology_HandleMessage;

    /* Handover is allowed by default, app may prohibit handover by calling
    TwsTopology_ProhibitHandover() function with TRUE parameter */
    tws_taskdata->app_prohibit_handover = FALSE;

    tws_taskdata->advertising_params = LE_ADVERTISING_PARAMS_SET_TYPE_UNSET;

    twsTopology_SmInit(&tws_taskdata->tws_sm);

    PeerFindRole_RegisterTask(TwsTopologyGetTask());
    StateProxy_StateProxyEventRegisterClient(TwsTopologyGetTask());

    /* Register to enable interested state proxy events */
    twsTopology_RegisterForStateProxyEvents();

    /* Register for connect / disconnect events from mirror profile */
    MirrorProfile_ClientRegister(TwsTopologyGetTask());

    appPhyStateRegisterClient(TwsTopologyGetTask());
    ConManagerRegisterConnectionsClient(TwsTopologyGetTask());

    Pairing_ActivityClientRegister(TwsTopologyGetTask());

    twsTopology_RegisterBtParameters();
    twsTopology_SelectBtParameters();

    TaskList_InitialiseWithCapacity(TwsTopologyGetMessageClientTasks(), MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY);

    unsigned rc_registrations_array_dim;
    rc_registrations_array_dim = (unsigned)role_change_client_registrations_end -
                              (unsigned)role_change_client_registrations_begin;
    PanicFalse((rc_registrations_array_dim % sizeof(role_change_client_callback_t)) == 0);
    rc_registrations_array_dim /= sizeof(role_change_client_callback_t);

    TwsTopology_RoleChangeClientNotifierInit(role_change_client_registrations_begin, 
                                    rc_registrations_array_dim); 

#ifdef INCLUDE_CASE_COMMS
    CcWithCase_RegisterStateClient(TwsTopologyGetTask());
#endif

    TwsTopology_RegisterServiceRecord(TwsTopologyGetTask());

    return TRUE;
}

void TwsTopology_Start(Task requesting_task)
{
    DEBUG_LOG_FN_ENTRY("TwsTopology_Start");
    twsTopology_SmStart(TwsTopology_GetSm(), requesting_task);
}

void TwsTopology_Stop(Task requesting_task)
{
    DEBUG_LOG_FN_ENTRY("TwsTopology_Stop");
    twsTopology_SmStop(TwsTopology_GetSm(), requesting_task);
}

void TwsTopology_RegisterMessageClient(Task client_task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(TwsTopologyGetMessageClientTasks()), client_task);
}

void TwsTopology_UnRegisterMessageClient(Task client_task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(TwsTopologyGetMessageClientTasks()), client_task);
}

tws_topology_role TwsTopology_GetRole(void)
{
    return twsTopology_RoleFromState(TwsTopology_GetSm()->state);
}

void TwsTopology_EnableRemainActiveForPeer(bool remain_active_for_peer)
{
    DEBUG_LOG("TwsTopology_EnableRemainActiveForPeer %d", remain_active_for_peer);

    TwsTopologyGetTaskData()->remain_active_for_peer = remain_active_for_peer;
    twsTopology_SmEnqueueKick(TwsTopology_GetSm());
}

bool TwsTopology_IsRemainActiveForPeerEnabled(void)
{
    return TwsTopologyGetTaskData()->remain_active_for_peer;
}

bool TwsTopology_IsPrimary(void)
{
    return (tws_topology_role_primary == twsTopology_RoleFromState(TwsTopology_GetSm()->state));
}

bool TwsTopology_IsFullPrimary(void)
{
    return TwsTopology_IsPrimary() &&
           (TwsTopology_GetSm()->state != TWS_TOPOLOGY_STATE_ACTING_PRIMARY);
}

bool TwsTopology_IsSecondary(void)
{
    return (tws_topology_role_secondary == twsTopology_RoleFromState(TwsTopology_GetSm()->state));
}

bool TwsTopology_IsActingPrimary(void)
{
    return (TwsTopology_GetSm()->state == TWS_TOPOLOGY_STATE_ACTING_PRIMARY);
}

void TwsTopology_ProhibitHandover(bool prohibit)
{
    TwsTopologyGetTaskData()->app_prohibit_handover = prohibit;

    DEBUG_LOG_FN_ENTRY("TwsTopology_ProhibitHandover %d", prohibit);
    twsTopology_SmEnqueueKick(TwsTopology_GetSm());
}

bool TwsTopology_IsHandoverProhibited(void)
{
    return TwsTopologyGetTaskData()->app_prohibit_handover;
}

bool twsTopology_IsRunning(void)
{
    tws_topology_sm_t *sm = TwsTopology_GetSm();

    return (sm->state != TWS_TOPOLOGY_STATE_STOPPED);
}
