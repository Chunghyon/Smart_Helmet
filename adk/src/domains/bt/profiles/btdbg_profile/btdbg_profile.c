/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      BTDBG profile

It only works with synergy profiles.

*/

#ifdef INCLUDE_BTDBG

#include "btdbg_profile.h"
#include "btdbg_profile_streams.h"
#include "btdbg_profile_data.h"

#include <multidevice.h>

#include <profile_manager.h>

#include <connection_no_ble.h>
#include <bdaddr.h>
#include <bt_device.h>
#include <logging.h>

#include <stream.h>
#include <sink.h>
#include <source.h>

#define SERVER_CHANNEL_OFFSET 38

uint8 btdbg_profile_rfcomm_service_record[] =
{
    0x09, 0x00, 0x01,           /*  0  1  2  ServiceClassIDList(0x0001) */
    0x35,   17,                 /*  3  4     DataElSeq 17 bytes */
    0x1C, BTDBG_PROFILE_UUID,  /*  5 .. 21  UUID BTDBG  */
    BTDBG_PROFILE_ATTR,        /* 22 23 24  ProtocolDescriptorList(0x0004) */
    0x35,   12,                 /* 25 26     DataElSeq 12 bytes */
    0x35,    3,                 /* 27 28     DataElSeq 3 bytes */
    0x19, 0x01, 0x00,           /* 29 30 31  UUID L2CAP(0x0100) */
    0x35,    5,                 /* 32 33     DataElSeq 5 bytes */
    0x19, 0x00, 0x03,           /* 34 35 36  UUID RFCOMM(0x0003) */
    0x08, 0x01,                 /* 37 38     uint8 RFCOMM channel */
    0x09, 0x00, 0x06,           /* 39 40 41  LanguageBaseAttributeIDList(0x0006) */
    0x35,    9,                 /* 42 43     DataElSeq 9 bytes */
    0x09,  'e',  'n',           /* 44 45 46  Language: English */
    0x09, 0x00, 0x6A,           /* 47 48 49  Encoding: UTF-8 */
    0x09, 0x01, 0x00,           /* 50 51 52  ID base: 0x0100 */
    0x09, 0x01, 0x00,           /* 53 54 55  ServiceName 0x0100, base + 0 */
    0x25,   5,                  /* 56 57     String length 5 */
    'B', 'T', 'D', 'B', 'G',    /* 58 59 60 61 62 "BTDBG" */
};

btdbg_profile_data_t btdbg_profile_data;

inline static void btdbgProfile_HandleRegisterCfm(CsrBtResultCode result, uint8 server_channel);
inline static void btdbgProfile_HandleConnection(CsrBtResultCode result, uint32 conn_id, CsrBtDeviceAddr *device_addr);
static void btdbgProfile_RequestDisconnect(bdaddr *bd_addr);
inline static void btdbgProfile_HandleDisconectInd(bool localy_terminated);


static void btdbgProfile_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(id);
    CsrBtCmPrim *prim = (CsrBtCmPrim*) message;
    DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler msg id 0x%04x, id 0x%x", *prim, id);
    switch (*prim)
    {
        case CSR_BT_CM_REGISTER_CFM:
        {
            CsrBtCmRegisterCfm *cfm = (CsrBtCmRegisterCfm*)message;
            DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_CM_REGISTER_CFM rc 0x%x, server channel %d", cfm->resultCode, cfm->serverChannel);

            btdbgProfile_HandleRegisterCfm(cfm->resultCode, cfm->serverChannel);

        }
        break;

        case CSR_BT_CM_RFC_CONNECT_ACCEPT_IND:
        {
            CsrBtCmRfcConnectAcceptInd *ind = (CsrBtCmRfcConnectAcceptInd *)message;
            DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_CM_RFC_CONNECT_ACCEPT_IND conn_id: 0x%x, lap: 0x%x, channel: 0x%x",
                    ind->btConnId, ind->deviceAddr.lap, ind->localServerChannel);

            CmRfcConnectAcceptRspSend(btdbg_profile_data.my_task,
                                        ind->btConnId,
                                        ind->deviceAddr,
                                        TRUE,
                                        ind->localServerChannel,
                                        CSR_BT_MODEM_SEND_CTRL_DTE_DEFAULT,
                                        CSR_BT_DEFAULT_BREAK_SIGNAL,
                                        CSR_BT_DEFAULT_MSC_TIMEOUT);
        }
        break;

        case CSR_BT_CM_CONNECT_ACCEPT_CFM:
        {
            CsrBtCmConnectAcceptCfm *cfm = (CsrBtCmConnectAcceptCfm *)message;


            DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_CM_CONNECT_ACCEPT_CFM status 0x%x, lap 0x%x, channel 0x%x, conn_id 0x%x",
                    cfm->resultCode, cfm->deviceAddr.lap, cfm->serverChannel, cfm->btConnId);

            btdbgProfile_HandleConnection(cfm->resultCode, cfm->btConnId, &cfm->deviceAddr);

        }
        break;

        case CSR_BT_CM_DISCONNECT_IND:
        {
            CsrBtCmDisconnectInd *ind = (CsrBtCmDisconnectInd *)message;

            DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_CM_DISCONNECT_IND conn_id 0x%x, local term 0x%x, reason code 0x%x",
                    ind->btConnId,
                    ind->localTerminated,
                    ind->reasonCode);

            btdbgProfile_HandleDisconectInd(ind->localTerminated);

        }
        break;

        case CSR_BT_CM_SDC_ATTRIBUTE_CFM:
        {
            DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_CM_SDC_ATTRIBUTE_CFM");
            CsrBtCmSdcCloseReqSend(TrapToOxygenTask(btdbg_profile_data.my_task));
        }
        break;

        default:
            DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler unhandled message 0x%x", *prim);
        break;
    }

    CmFreeUpstreamMessageContents((void *) message);
}

bool BtdbgProfile_Init(Task init_task)
{
    UNUSED(init_task);

    btdbg_profile_data.task_data.handler = btdbgProfile_MessageHandler;
    btdbg_profile_data.my_task = (Task)&btdbg_profile_data.task_data;

    btdbg_profile_data.state = btdbg_profile_state_disconnected;

    DEBUG_LOG_VERBOSE("BtdbgProfile_Init");
    ConnectionRfcommAllocateChannel(btdbg_profile_data.my_task, 1);

    ProfileManager_RegisterProfile(profile_manager_btdbg_profile, NULL,
            btdbgProfile_RequestDisconnect);

    return TRUE;
}

void BtdbgProfile_RegisterListener(Task listener)
{
    btdbg_profile_data.listener = listener;
}

inline static void btdbgProfile_HandleRegisterCfm(CsrBtResultCode result, uint8 server_channel)
{
    if(result == CSR_BT_RESULT_CODE_CM_SUCCESS)
    {
        DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_RESULT_CODE_CM_SUCCESS");
        btdbg_profile_rfcomm_service_record[SERVER_CHANNEL_OFFSET] = server_channel;

        CmSdsRegisterReqSend(btdbg_profile_data.my_task, sizeof(btdbg_profile_rfcomm_service_record),
            btdbg_profile_rfcomm_service_record, CSR_BT_CM_CONTEXT_UNUSED);

    }
    else if(result == CSR_BT_RESULT_CODE_CM_SERVER_CHANNEL_ALREADY_USED)
    {
        DEBUG_LOG_VERBOSE("btdbgProfile_MessageHandler CSR_BT_RESULT_CODE_CM_SERVER_CHANNEL_ALREADY_USED");
        ConnectionRfcommAllocateChannel(btdbg_profile_data.my_task, server_channel + 1);
    }
}

inline static void btdbgProfile_HandleConnection(CsrBtResultCode result, uint32 conn_id, CsrBtDeviceAddr *device_addr)
{
    if(result == CSR_BT_RESULT_CODE_CM_SUCCESS)
    {
        btdbg_profile_data.rfcomm_sink = StreamRfcommSink(CM_GET_UINT16ID_FROM_BTCONN_ID(conn_id));

        if(Multidevice_IsPair())
        {
            bool is_primary = BtDevice_IsMyAddressPrimary();

            btdbg_profile_data.isp_role = is_primary ? ISP_ROLE_PRIMARY : ISP_ROLE_SECONDARY;
            btdbg_profile_data.isp_role |= Multidevice_IsLeft() ? ISP_ROLE_LEFT : ISP_ROLE_RIGHT;
        }
        else
        {
            btdbg_profile_data.isp_role = ISP_ROLE_SINGLE;
        }

        btdbg_profile_data.isp_sink = BtdbgProfile_ConnectStreams(btdbg_profile_data.rfcomm_sink, btdbg_profile_data.isp_role);


        DEBUG_LOG_VERBOSE("btdbgProfile_HandleConnection CSR_BT_CM_CONNECT_ACCEPT_CFM isp role 0x%x", btdbg_profile_data.isp_role);

        bdaddr addr = {0};
        BdaddrConvertBluestackToVm(&addr, device_addr);

        ProfileManager_GenericConnectedInd(profile_manager_btdbg_profile, &addr);
        btdbg_profile_data.state = btdbg_profile_state_connected;

        if(btdbg_profile_data.listener)
        {
            MessageSend(btdbg_profile_data.listener, BTDBG_PROFILE_HANDSET_CONNECT_IND, NULL);
        }
    }
}

static void btdbgProfile_RequestDisconnect(bdaddr *bd_addr)
{
    UNUSED(bd_addr);

    DEBUG_LOG_VERBOSE("btdbgProfile_RequestDisconnect lap 0x%x", bd_addr->lap);

    if(btdbg_profile_data.state == btdbg_profile_state_connected)
    {
        btdbg_profile_data.state = btdbg_profile_state_disconnecting;

        ConnectionRfcommDisconnectRequest(btdbg_profile_data.my_task, btdbg_profile_data.rfcomm_sink);
    }
}

inline static void btdbgProfile_HandleDisconectInd(bool localy_terminated)
{
    BtdbgProfile_DisconnectStreams(btdbg_profile_data.rfcomm_sink, btdbg_profile_data.isp_sink);

    btdbg_profile_data.state = btdbg_profile_state_disconnected;

    if(btdbg_profile_data.listener)
    {
        MessageSend(btdbg_profile_data.listener, BTDBG_PROFILE_HANDSET_DISCONNECT_IND, NULL);
    }

    if (!localy_terminated)
    {
        ConnectionRfcommDisconnectResponse(btdbg_profile_data.rfcomm_sink);
    }
}

#endif
