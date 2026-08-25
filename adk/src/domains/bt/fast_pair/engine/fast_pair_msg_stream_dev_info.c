/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream_dev_info.c
\brief      Implementation of Fast Pair Device Information Message Stream
*/

#include <bdaddr.h>
#include <panic.h>

#include "fast_pair.h"
#include "fast_pair_config.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_msg_stream_dev_info.h"
#include "fast_pair_battery_notifications.h"
#include "multidevice.h"
#include "fast_pair_msg_stream_dev_action.h"

typedef struct
{
    fast_pair_msg_stream_dev_info dev_info;
    bdaddr fast_pair_bdaddr;
    bool is_fast_pair_bdaddr_received;
    uint8 fp_seeker_index;
} fast_pair_msg_stream_dev_info_data_t;

static fast_pair_msg_stream_dev_info_data_t fast_pair_msg_stream_dev_info_data;
static void fastPair_DevInfo_SysMessageHandler(Task task, MessageId id, Message message);
static const TaskData dev_info_msg_stream_task = {fastPair_DevInfo_SysMessageHandler};


static void fastPair_DevInfo_SysMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {

        case CL_SM_BLE_READ_RANDOM_ADDRESS_CFM:
        {
            CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T *msg =  (CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T *)message;
            tp_bdaddr  fast_pair_tp_bdaddr = msg->random_tpaddr;
            DEBUG_LOG("CL_SM_BLE_READ_RANDOM_ADDRESS_CFM: Addr %04x,%02x,%06x,type %d",fast_pair_tp_bdaddr.taddr.addr.nap,
                  fast_pair_tp_bdaddr.taddr.addr.uap,fast_pair_tp_bdaddr.taddr.addr.lap,fast_pair_tp_bdaddr.taddr.type );
            fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr = fast_pair_tp_bdaddr.taddr.addr;
            fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received = TRUE;
        }
        break;

        default:
        {
            DEBUG_LOG_WARN("fastPair_DevInfo_SysMessageHandler: UNHANDLED msg id %d. ",id);
        }
    }
}

/* Send Model-Id to Seeker */
static void fastPair_DevInfo_SendModelId(uint8 fp_seeker_number)
{
    uint8 data_model_id[FASTPAIR_DEV_INFO_MODEL_ID_ADD_DATA_LEN];
    uint32 model_id = fastPair_GetModelId();

    data_model_id[0]= (model_id >> 16) & 0xFF;
    data_model_id[1]= (model_id >> 8) & 0xFF;
    data_model_id[2]= model_id & 0xFF;

    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
         FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_MODEL_ID,data_model_id,FASTPAIR_DEV_INFO_MODEL_ID_ADD_DATA_LEN, fp_seeker_number);
}

/* Send BLE-address to Seeker */
static void fastPair_DevInfo_Send_BLEAddress(uint8 fp_seeker_number)
{
    uint8 data_addr[FASTPAIR_DEV_INFO_BLE_ADDRESS_ADD_DATA_LEN];

    data_addr[0] = ((fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.nap)>> 8) & 0xFF;
    data_addr[1] =  (fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.nap) & 0xFF;
    data_addr[2] =  (fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.uap) & 0xFF;
    data_addr[3] = ((fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.lap)>> 16) & 0xFF;
    data_addr[4] = ((fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.lap)>> 8) & 0xFF;
    data_addr[5] =  (fast_pair_msg_stream_dev_info_data.fast_pair_bdaddr.lap) & 0xFF;

    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
         FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_BLE_ADDRESS_UPDATED,data_addr,FASTPAIR_DEV_INFO_BLE_ADDRESS_ADD_DATA_LEN, fp_seeker_number);
}

/*! \brief Send session nonce to Seeker */
static void fastPair_DevInfo_SendSessionNonce(uint8 remote_device_id)
{

    DEBUG_LOG("fastPair_DevInfo_SendSessionNonce: Sending session nonce to remote device id: %d", remote_device_id);
    uint8* session_nonce = FastPair_MsgStream_GetSessionNonce(remote_device_id);

    if(session_nonce != NULL) 
    {
        fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
                                   FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_SESSION_NOUNCE,
                                   session_nonce,
                                   FASTPAIR_DEV_INFO_SESSION_NONCE_LEN,
                                   remote_device_id);
    }
}

static void fastPair_DevInfo_HandleConnection(uint8 fp_seeker_number)
{
    fastPair_DevInfo_SendModelId(fp_seeker_number);
    if(TRUE == fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received)
    {
        fastPair_DevInfo_Send_BLEAddress(fp_seeker_number);
    }
    else
    {
        DEBUG_LOG_WARN("fastPair_DevInfo_HandleConnection: BLE Addr not yet received. So, not sending it.");
        /* Send BLE address to this seeker number when BLE address is received */
        fast_pair_msg_stream_dev_info_data.fp_seeker_index = fp_seeker_number;
    }

    /* Send Device Battery Information to connected FP Seekers when RFCOMM connects */
    fastPair_SendDeviceBattery();

    /* Send Session Nonce to connected fp seeker */
    fastPair_DevInfo_SendSessionNonce(fp_seeker_number);
}

static void fastPair_DevInfo_HandleIncomingData(uint8 fp_seeker_number, const uint8 *msg_data, uint16 len)
{
    uint8 rsp_data = 0;
    uint8 msg_code;
    uint16 additional_data_len;

    if((NULL == msg_data)||(len < FASTPAIR_DEVINFO_ADD_DATA_INDEX))
    {
        DEBUG_LOG_WARN("fastPair_DevInfo_HandleIncomingData: UNEXPECTED ERROR - Length is %d is less than minimum of %d or data is NULL",len,FASTPAIR_DEVINFO_ADD_DATA_INDEX);
        return;
    }

    additional_data_len = msg_data[FASTPAIR_DEVINFO_ADD_DATA_LEN_UPPER_INDEX];
    additional_data_len = (additional_data_len<<8)+ msg_data[FASTPAIR_DEVINFO_ADD_DATA_LEN_LOWER_INDEX];

    if(len != (FASTPAIR_DEVINFO_ADD_DATA_INDEX + additional_data_len))
    {
        DEBUG_LOG_WARN("fastPair_DevInfo_HandleIncomingData: UNEXPECTED length ERROR. Received data length is %d. Should be %d",len,(FASTPAIR_DEVINFO_ADD_DATA_INDEX + additional_data_len));
        return;
    }
    msg_code = msg_data[FASTPAIR_DEVINFO_CODE_INDEX];
    switch(msg_code)
    {
        case FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_REQ:
        {
             if(additional_data_len != FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_ADD_DATA_LEN)
             {
                 DEBUG_LOG_WARN("fastPair_DevInfo_HandleIncomingData-capabilities: Additional data length is %d, should be %d",
                                 additional_data_len, FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_ADD_DATA_LEN );
                 return;
             }
             if(Multidevice_IsPair())
             {
                DEBUG_LOG("devInfo Active Components Left & Right active EB");

                /* Both Left and Right Buds are active */ 
                rsp_data = FASTPAIR_LEFT_RIGHT_ACTIVE; 
             }
             else
             {
                DEBUG_LOG("devInfo Active Components Single active HS");

                /* A single device component */ 
                rsp_data = FASTPAIR_SINGLE_ACTIVE;
             }
             /* Send response message */
             fastPair_MsgStreamSendRsp(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
                                       FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_ACTIVE_COMPONENTS_RSP,
                                       &rsp_data,
                                       (uint8) FASTPAIR_DEVINFO_ACTIVE_COMPONENTS_RSP_ADD_DATA_LEN,
                                       fp_seeker_number);

             /* When ring device is already initiated from AG1, send ring device message to AG2 when FMA UI
                on that AG is opened for the first time after it has been connected with Headset or Earbud. */
             if (dev_action_data.ring_component != FASTPAIR_DEVICEACTION_STOP_RING)
             {
                 fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT,
                                            FASTPAIR_MESSAGESTREAM_DEVACTION_RING_EVENT,
                                            &dev_action_data.ring_component,
                                            FASTPAIR_DEVICEACTION_RING_RSP_ADD_DATA_LEN,
                                            fp_seeker_number);
             }

        }
        break;
        case FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_CAPABILITIES:
        {
             if(additional_data_len != FASTPAIR_DEVINFO_CAPABILITIES_ADD_DATA_LEN)
             {
                 DEBUG_LOG_WARN("fastPair_DevInfo_HandleIncomingData-capabilities: Additional data length is %d, should be %d",
                                 additional_data_len, FASTPAIR_DEVINFO_CAPABILITIES_ADD_DATA_LEN );
                 return;
             }
             fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities = msg_data[FASTPAIR_DEVINFO_ADD_DATA_INDEX];
             DEBUG_LOG("fastPair_DevInfo_HandleIncomingData-capabilities: Companion App %d, Silence mode %d",
                       fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities&FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_SILENCE_MODE_SUPPORTED,
                       (fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities&FASTPAIR_MESSAGESTREAM_DEVINFO_CAPABILITIES_COMPANION_APP_INSTALLED)>>1);
             /* Acknowledge message */
             fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_CAPABILITIES, fp_seeker_number);
        }
        break;
        case FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_PLATFORM_TYPE:
        {
            if(additional_data_len != FASTPAIR_DEVINFO_PLATFORM_TYPE_ADD_DATA_LEN)
            {
                DEBUG_LOG_WARN("fastPair_DevInfo_HandleIncomingData-platform type: Additional data length is %d, should be %d",
                            additional_data_len, FASTPAIR_DEVINFO_PLATFORM_TYPE_ADD_DATA_LEN );
                return;
            }
            DEBUG_LOG("fastPair_DevInfo_HandleIncomingData-Platrform Type: Platform %d, SDK Ver %d",
                      msg_data[FASTPAIR_DEVINFO_ADD_DATA_INDEX],msg_data[FASTPAIR_DEVINFO_ADD_DATA_INDEX+1]);

            /* Acknowledge message */
            fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,FASTPAIR_MESSAGESTREAM_DEVINFO_EVENT_CODE_PLATFORM_TYPE, fp_seeker_number);
        }
        break;
        default:
        {
            /* Acknowledge message */
            fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,msg_code, fp_seeker_number);

            DEBUG_LOG_WARN("fastPair_DevInfo_HandleIncomingData: UNHANDLED code %d. ",msg_code);
            return;
        }
    }
}

/* Handle messages from Message stream */
static void fastPair_DevInfo_MessageHandler(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type, uint8 fp_seeker_number, const uint8 *msg_data, uint16 len)
{
    switch(msg_type)
    {
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND:
             /* Request for BLE address */
             fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received = FALSE;
             fast_pair_msg_stream_dev_info_data.fp_seeker_index = 0xFF;
             ConnectionSmBleReadRandomAddressTaskReq((Task)&dev_info_msg_stream_task, ble_read_random_address_local,NULL);
        break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM:
              fastPair_DevInfo_HandleConnection(fp_seeker_number);
              fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities = 0;
        break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA:
             fastPair_DevInfo_HandleIncomingData(fp_seeker_number, msg_data, len);
        break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND:
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM:
        break;
        default:
             DEBUG_LOG("fastPair_DevInfo_MessageHandler: unknown message=%x", msg_type);
        break;
    }
}

fast_pair_msg_stream_dev_info fastPair_MsgStreamDevInfo_Get(void)
{
    return fast_pair_msg_stream_dev_info_data.dev_info;
}

void fastPair_MsgStreamDevInfo_Set(fast_pair_msg_stream_dev_info dev_info)
{
    fast_pair_msg_stream_dev_info_data.dev_info = dev_info;
}

void fastPair_MsgStreamDevInfo_Init(void)
{
    fast_pair_msg_stream_dev_info_data.dev_info.dev_info_capabilities = 0;
    fast_pair_msg_stream_dev_info_data.is_fast_pair_bdaddr_received = FALSE;

    /* Handle Device Information messages from Message stream */
    fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,fastPair_DevInfo_MessageHandler);
}
