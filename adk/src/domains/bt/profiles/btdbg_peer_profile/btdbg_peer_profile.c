/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    btdbg_peer_profile
\brief      BTDBG peer profile

It only works with synergy profiles.

*/

#ifdef INCLUDE_BTDBG

#include "btdbg_peer_profile.h"
#include "btdbg_profile.h"
#include "btdbg_profile_streams.h"
#include "btdbg_profile_data.h"
#include "btdbg_peer_profile_data.h"

#include <multidevice.h>

#include <connection_no_ble.h>
#include <bdaddr.h>
#include <bt_device.h>
#include <logging.h>

#include <stream.h>
#include <sink.h>
#include <source.h>

btdbg_peer_profile_data_t btdbg_peer_profile_data;

static const uint8 btdbg_profile_attribute_list[] =
{
    0x35,    3,
    BTDBG_PROFILE_ATTR
};

static const CsrBtUuid128 service_uuid = {BTDBG_PROFILE_UUID};

inline static void btdbgProfile_HandleRegisterCfm(CsrBtResultCode result, uint8 server_channel);
static bool btdbgProfile_EvaluateConnectionRequest(void);
inline static void BtdbgPeerProfile_ConnectForReal(uint8 server_channel);
inline static void btdbgProfile_HandleConnection(CsrBtResultCode result, uint32 conn_id);
inline static void btdbgProfile_RequestDisconnect(void);
inline static void btdbgProfile_HandleDisconectInd(bool localTerminated);

inline static void btdbgProfile_SdpResultHandler(void *inst,
                                       CmnCsrBtLinkedListStruct *sdpTagList,
                                       CsrBtDeviceAddr deviceAddr,
                                       CsrBtResultCode resultCode,
                                       CsrBtSupplier resultSupplier)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_SdpResultHandler rc enum:CsrBtResultCode:0x%x", resultCode);

    CsrBtUtilBllFreeLinkedList(&sdpTagList, CsrBtUtilBllPfreeWrapper);

    UNUSED(inst);
    UNUSED(deviceAddr);
    UNUSED(resultSupplier);
}

static void btdbgProfile_ResultHandler(CsrSdcOptCallbackType callback_type, void *context)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_ResultHandler callback_type enum:CsrSdcOptCallbackType:0x%x", callback_type);

    switch(callback_type)
    {
        case CSR_SDC_OPT_CB_SEARCH_RESULT:
        {
            CsrSdcResultFuncType *params = (CsrSdcResultFuncType *)context;
            btdbgProfile_SdpResultHandler(params->instData, params->sdpTagList, params->deviceAddr, params->resultCode, params->resultSupplier);
        }
        break;

        case CSR_SDC_OPT_RFC_CON_RESULT:
        {
            CsrRfcConResultType *params = (CsrRfcConResultType *) context;

            btdbgProfile_HandleConnection(params->resultCode, params->btConnId);
        }
        break;

        default:
            break;
    }
}

static void btdbgProfile_PrimHandler(Message message)
{
    CsrBtCmPrim *prim = (CsrBtCmPrim*) message;

    switch (*prim)
    {
        case CSR_BT_CM_REGISTER_CFM:
        {
            CsrBtCmRegisterCfm *cfm = (CsrBtCmRegisterCfm*)message;
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_CM_REGISTER_CFM rc 0x%x, server channel %d", cfm->resultCode, cfm->serverChannel);

            btdbgProfile_HandleRegisterCfm(cfm->resultCode, cfm->serverChannel);

        }
        break;

        case CSR_BT_CM_SDS_REGISTER_CFM:
        {

            CsrBtCmSdsRegisterCfm *cfm = (CsrBtCmSdsRegisterCfm *)message;
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_CM_SDS_REGISTER_CFM result 0x%x", cfm->resultCode);
        }
        break;

        default:
        {
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler 1st unhandled message 0x%x", *prim);

            if(btdbg_peer_profile_data.sdp_search_data && CsrBtUtilRfcConVerifyCmMsg(prim))
            {
                CsrBtUtilRfcConCmMsgHandler(NULL, btdbg_peer_profile_data.sdp_search_data, prim);
            }
        }

    }

    switch(*prim)
    {
        case CSR_BT_CM_RFC_CONNECT_ACCEPT_IND:
        {
            CsrBtCmRfcConnectAcceptInd *ind = (CsrBtCmRfcConnectAcceptInd *)message;
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_CM_RFC_CONNECT_ACCEPT_IND conn_id: 0x%x, lap: 0x%x, channel: 0x%x",
                    ind->btConnId, ind->deviceAddr.lap, ind->localServerChannel);

            CmRfcConnectAcceptRspSend(btdbg_peer_profile_data.rfc_task,
                                        ind->btConnId,
                                        ind->deviceAddr,
                                        TRUE,
                                        ind->localServerChannel,
                                        CSR_BT_MODEM_SEND_CTRL_DTE_DEFAULT,
                                        CSR_BT_DEFAULT_BREAK_SIGNAL,
                                        CSR_BT_DEFAULT_MSC_TIMEOUT);
        }
        break;

        case CSR_BT_CM_DISCONNECT_IND:
        {
            CsrBtCmDisconnectInd *ind = (CsrBtCmDisconnectInd *)message;
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_CM_DISCONNECT_IND local %d", ind->localTerminated);

            btdbgProfile_HandleDisconectInd(ind->localTerminated);
        }
        break;

        case CSR_BT_CM_SDC_RELEASE_RESOURCES_CFM:
        {
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_CM_SDC_RELEASE_RESOURCES_CFM");
        }
        break;


        default:
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler unhandled message 0x%x", *prim);
        break;
    }

    CmFreeUpstreamMessageContents((void *) message);
}

static void btdbgProfile_RfcConnMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler msg id 0x%04x, id 0x%x", message, id);

    switch(id)
    {
        case BTDBG_PROFILE_HANDSET_CONNECT_IND:
        {
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler BTDBG_PROFILE_HANDSET_CONNECT_IND");
            btdbg_peer_profile_data.connection_allowed = TRUE;
            btdbgProfile_EvaluateConnectionRequest();
        }
        break;

        case BTDBG_PROFILE_HANDSET_DISCONNECT_IND:
        {
            DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler BTDBG_PROFILE_HANDSET_DISCONNECT_IND");
            btdbg_peer_profile_data.connection_allowed = FALSE;
            btdbgProfile_RequestDisconnect();
        }
        break;

        default:
        {
            if(message)
            {
                btdbgProfile_PrimHandler(message);
            }
        }
    }

}

bool BtdbgPeerProfile_Init(Task init_task)
{
    UNUSED(init_task);

    btdbg_peer_profile_data.rfc_task_data.handler = btdbgProfile_RfcConnMessageHandler;
    btdbg_peer_profile_data.rfc_task = (Task)&btdbg_peer_profile_data.rfc_task_data;
    btdbg_peer_profile_data.sdp_search_data = NULL;
    btdbg_peer_profile_data.state = btdbg_peer_profile_state_disconnected;

    BtdbgProfile_RegisterListener(btdbg_peer_profile_data.rfc_task);

    return TRUE;
}

static void btdbgProfile_NotifyListener(void)
{
    if(btdbg_peer_profile_data.listener && btdbg_peer_profile_data.cfm_sent == FALSE)
    {
        DEBUG_LOG_VERBOSE("btdbgProfile_NotifyListener");
        MessageSend(btdbg_peer_profile_data.listener, BTDBG_PROFILE_PEER_CONNECT_CFM, NULL);
        btdbg_peer_profile_data.cfm_sent = TRUE;
    }
}

void BtdbgPeerProfile_Connect(Task task, const bdaddr *addr)
{
    DEBUG_LOG_VERBOSE("BtdbgPeerProfile_Connect");

    if(addr)
    {
        btdbg_peer_profile_data.listener = task;
        memcpy(&btdbg_peer_profile_data.peer_addr, addr, sizeof(*addr));

        btdbg_peer_profile_data.connection_requested = TRUE;
        btdbg_peer_profile_data.cfm_sent = FALSE;
        if(!btdbgProfile_EvaluateConnectionRequest())
        {
            btdbgProfile_NotifyListener();
        }

    }
    else
    {
        DEBUG_LOG_VERBOSE("BtdbgPeerProfile_Connect - No peer");
    }
}

static bool btdbgProfile_EvaluateConnectionRequest(void)
{

    DEBUG_LOG_VERBOSE("btdbgProfile_EvaluateConnectionRequest enum:btdbg_peer_profile_state_t:%d, allowed %d, requested %d",
            btdbg_peer_profile_data.state, btdbg_peer_profile_data.connection_allowed, btdbg_peer_profile_data.connection_requested);

    if(btdbg_peer_profile_data.state == btdbg_peer_profile_state_disconnected
            && btdbg_peer_profile_data.connection_allowed
            && btdbg_peer_profile_data.connection_requested)
    {
        ConnectionRfcommAllocateChannel(btdbg_peer_profile_data.rfc_task, 0);

        btdbg_peer_profile_data.state = btdbg_peer_profile_state_connecting;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

inline static void btdbgProfile_HandleRegisterCfm(CsrBtResultCode result, uint8 server_channel)
{
    if(result == CSR_BT_RESULT_CODE_CM_SUCCESS)
    {
        DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_RESULT_CODE_CM_SUCCESS");

        BtdbgPeerProfile_ConnectForReal(server_channel);

    }
    else if(result == CSR_BT_RESULT_CODE_CM_SERVER_CHANNEL_ALREADY_USED)
    {
        DEBUG_LOG_VERBOSE("btdbgProfile_RfcConnMessageHandler CSR_BT_RESULT_CODE_CM_SERVER_CHANNEL_ALREADY_USED");
        ConnectionRfcommAllocateChannel(btdbg_peer_profile_data.rfc_task, server_channel + 1);
    }
}

inline static void BtdbgPeerProfile_ConnectForReal(uint8 server_channel)
{
    DEBUG_LOG_VERBOSE("BtdbgPeerProfile_ConnectForReal");

    CmnCsrBtLinkedListStruct *sdp_tag_list = NULL;
    uint16 sh_index;
    CsrBtDeviceAddr addr;

    if(!btdbg_peer_profile_data.sdp_search_data)
    {
        btdbg_peer_profile_data.sdp_search_data = CsrBtUtilSdpRfcInit(btdbgProfile_ResultHandler,
                CSR_SDC_OPT_CB_RFC_CON_RESULT_MASK,
                TrapToOxygenTask(btdbg_peer_profile_data.rfc_task),
                0);
    }

    sdp_tag_list = CsrBtUtilSdrCreateServiceHandleEntryFromUuid128(sdp_tag_list,
                                                                &service_uuid,
                                                                &sh_index);
    CsrBtUtilSdrCreateAndInsertAttribute(sdp_tag_list,
                                             sh_index,
                                             CSR_BT_PROTOCOL_DESCRIPTOR_LIST_ATTRIBUTE_IDENTIFIER,
                                             CsrMemDup(btdbg_profile_attribute_list,
                                                     sizeof(btdbg_profile_attribute_list)),
                                             ARRAY_DIM(btdbg_profile_attribute_list));

    CsrBtUtilSdrInsertLocalServerChannel(sdp_tag_list,
                                         sh_index,
                                         server_channel);

    BdaddrConvertVmToBluestack(&addr, &btdbg_peer_profile_data.peer_addr);

    CsrBtUtilRfcConStart((void *) btdbg_peer_profile_data.rfc_task,
                                 btdbg_peer_profile_data.sdp_search_data,
                                 sdp_tag_list,
                                 addr,
                                 CSR_BT__DEFAULT_INCOMING_SECURITY_,
                                 FALSE,
                                 NULL,
                                 CSR_BT_RFC_BUILD_RFCOMM_MAX_FRAME_SIZE,
                                 CSR_BT_MODEM_SEND_CTRL_DTE_DEFAULT,
                                 CSR_BT_DEFAULT_MSC_TIMEOUT,
                                 CSR_BT_SC_DEFAULT_ENC_KEY_SIZE);

}

inline static void btdbgProfile_HandleConnection(CsrBtResultCode result, uint32 conn_id)
{
    if (result == CSR_BT_RESULT_CODE_CM_SUCCESS)
    {
        uint16 cid = CM_GET_UINT16ID_FROM_BTCONN_ID(conn_id);
        btdbg_peer_profile_data.rfcomm_sink = StreamRfcommSink(cid);

        btdbg_peer_profile_data.isp_sink = BtdbgProfile_ConnectStreams(btdbg_peer_profile_data.rfcomm_sink, ISP_ROLE_FORWARDING);

        btdbg_peer_profile_data.state = btdbg_peer_profile_state_connected;

        Source src = StreamSourceFromSink(btdbg_peer_profile_data.rfcomm_sink);
        SourceConfigure(src, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);


        btdbgProfile_NotifyListener();
    }
}

inline static void btdbgProfile_RequestDisconnect(void)
{
    if(btdbg_peer_profile_data.state == btdbg_profile_state_connected)
        {
            btdbg_peer_profile_data.state = btdbg_peer_profile_state_disconnecting;

            BtdbgProfile_DisconnectStreams(btdbg_peer_profile_data.rfcomm_sink, btdbg_peer_profile_data.isp_sink);

            ConnectionRfcommDisconnectRequest(btdbg_peer_profile_data.rfc_task, btdbg_peer_profile_data.rfcomm_sink);
        }
}

inline static void btdbgProfile_HandleDisconectInd(bool localy_terminated)
{
    BtdbgProfile_DisconnectStreams(btdbg_peer_profile_data.rfcomm_sink, btdbg_peer_profile_data.isp_sink);

    btdbg_peer_profile_data.state = btdbg_peer_profile_state_disconnected;
    btdbg_peer_profile_data.connection_requested = FALSE;

    if (!localy_terminated)
    {
        ConnectionRfcommDisconnectResponse(btdbg_peer_profile_data.rfcomm_sink);
    }
}

#endif
