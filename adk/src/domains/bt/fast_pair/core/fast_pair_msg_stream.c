/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_msg_stream.c
\brief      Implementation of Fast Pair Message Stream
*/

#include <panic.h>

#include "fast_pair.h"
#include "fast_pair_rfcomm.h"
#include "fast_pair_profile.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair_msg_stream_dev_info.h"
#include "fast_pair_msg_stream_dev_action.h"
#include "sass.h"

/********* MESSAGE STREAM PROTOCOL **************/
/*
Octet   Data Type      Description               Mandatory?
0       uint8          Message group             Mandatory
1       uint8          Message code              Mandatory
2 - 3   uint16         Additional data length    Mandatory
4 - n                  Additional data           Optional
The additional data length and additional data fields should be big endian.
*/

#define FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX 0
#define FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX  1
#define FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX  2
#define FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX  3
#define FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_INDEX 4
#define FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_INDEX
#define FASTPAIR_MESSAGESTREAM_SESSION_NONCE_LEN 8

/* Message group data */
typedef struct
{
    fastPair_MsgStreamMsgCallBack bluetooth_event_msgs_callback;
    fastPair_MsgStreamMsgCallBack companion_app_event_msgs_callback;
    fastPair_MsgStreamMsgCallBack dev_info_event_msgs_callback;
    fastPair_MsgStreamMsgCallBack dev_action_event_callback;
    fastPair_MsgStreamMsgCallBack sass_event_callback;

    bool is_msg_stream_busy_incoming_data;
    bool is_msg_stream_busy_outgoing_data;
    uint8* sass_session_nonce[FASTPAIR_RFCOMM_CONNECTIONS_MAX];
} fast_pair_msg_stream_data_t;

static fast_pair_msg_stream_data_t fast_pair_msg_stream_data;

void fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP group, uint8 msg_code, uint8 *add_data, uint16 add_data_len, uint8 fp_seeker_number)
{
    uint8 *msg_stream_data;
    uint16 msg_stream_data_len = (FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM+add_data_len);

    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = TRUE;

    msg_stream_data = (uint8 *)PanicUnlessMalloc(msg_stream_data_len*sizeof(uint8));
    if(NULL == msg_stream_data)
    {
        DEBUG_LOG_WARN("fastPair_MsgStreamSendData: Could not allocate memory");
        fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = FALSE;
        return;
    }

    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX] = group;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX]  = msg_code;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX] = ((add_data_len&0xFF00)>>8)&0xFF;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX] = add_data_len&0xFF;
    if(add_data_len > 0)
    {
        memcpy(msg_stream_data+FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM,add_data,add_data_len);
    }

    DEBUG_LOG("fastPair_MsgStreamSendData: Length %d Data is ",msg_stream_data_len);
    for(int i=0;i < msg_stream_data_len;++i)
    {
        DEBUG_LOG_V_VERBOSE(" %02X",msg_stream_data[i]);
    }

    fastPair_RfcommSendData(msg_stream_data,msg_stream_data_len, fp_seeker_number);
    free(msg_stream_data);
    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = FALSE;
}

void fastPair_MsgStreamSendDataToAll(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP group, uint8 msg_code, uint8 *add_data, uint16 add_data_len)
{
    uint8 *msg_stream_data;
    uint8 rfcomm_instance;
    uint16 msg_stream_data_len = (FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM+add_data_len);

    /* No connected RFCOMMs, return */
    if(fastPair_RfcommGetRFCommConnectedInstances() == 0)
    {
        return;
    }
    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = TRUE;

    msg_stream_data = (uint8 *)PanicUnlessMalloc(msg_stream_data_len*sizeof(uint8));
    if(msg_stream_data == NULL)
    {
        DEBUG_LOG_WARN("fastPair_MsgStreamSendDataToAll: Could not allocate memory");
        fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = FALSE;
        return;
    }

    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX] = group;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX]  = msg_code;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX] = ((add_data_len&0xFF00)>>8)&0xFF;
    msg_stream_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX] = add_data_len&0xFF;
    if(add_data_len > 0)
    {
        memcpy(msg_stream_data+FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM,add_data,add_data_len);
    }

    DEBUG_LOG("fastPair_MsgStreamSendDataToAll: Length %d Data is ",msg_stream_data_len);
    for(uint8 i = 0;i < msg_stream_data_len; i++)
    {
        DEBUG_LOG_V_VERBOSE(" %02X", msg_stream_data[i]);
    }

    for(rfcomm_instance = 0; rfcomm_instance < FASTPAIR_RFCOMM_CONNECTIONS_MAX; rfcomm_instance++)
    {
        if(fastPair_RfcommInstanceNumberIsConnected(rfcomm_instance))
        {
           fastPair_RfcommSendData(msg_stream_data, msg_stream_data_len, rfcomm_instance + 1);
           DEBUG_LOG("fastPair_MsgStreamSendDataToAll: Sent data of length %d to instance %d", msg_stream_data_len, rfcomm_instance + 1);
        }
    }

    free(msg_stream_data);
    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data = FALSE;
}

#define FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT 0xFF
/* Message Code for Acknowledgement group */
typedef enum
{
    FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK = 0x01,
    FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_NAK = 0x02
} FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE;

#define MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN (2)
#define MESSAGE_STREAM_ACKNOWLEDGEMENT_NAK_DATA_LEN (3)

/* Send acknowledge message stream packet */

void fastPair_MsgStreamSendACK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code, uint8 fp_seeker_number)
{
    uint8 data_ack[MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN];
    data_ack[0] = (uint8)msg_group;
    data_ack[1] = msg_code;
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK,
                               data_ack,MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN, fp_seeker_number);
}

void fastPair_MsgStreamSendACKWithAdditionalData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code,
                                                             uint8* additional_data, uint16 additional_data_len, uint8 remote_device_id)
{

    uint8* add_data = NULL;
    /* Additional data will include msg group and msg code along with sass additional data */
    uint16 add_data_len = MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN + additional_data_len;
    add_data = (uint8*)PanicUnlessMalloc(add_data_len*sizeof(uint8));

    add_data[0] = (uint8)msg_group;
    add_data[1] = (uint8)msg_code;
    memcpy(add_data + MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN, additional_data, additional_data_len);

    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,
                               FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK,
                               add_data, add_data_len,
                               remote_device_id);

    free(add_data);
}

/* Send NAK message stream packet */
void fastPair_MsgStreamSendNAK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code,FASTPAIR_MESSAGESTREAM_NAK_REASON nak_reason, uint8 fp_seeker_number)
{
    uint8 data_nak[MESSAGE_STREAM_ACKNOWLEDGEMENT_NAK_DATA_LEN];
    data_nak[0] = (uint8)nak_reason;
    data_nak[1] = (uint8)msg_group;
    data_nak[2] = msg_code;
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_NAK,
                               data_nak,MESSAGE_STREAM_ACKNOWLEDGEMENT_NAK_DATA_LEN, fp_seeker_number);
}

void fastPair_MsgStreamSendRsp(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, uint8 msg_code, uint8 *data, uint8 data_len, uint8 fp_seeker_number)
{
    uint8 data_ack[MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN+data_len];
    data_ack[0]=(uint8)msg_group;
    data_ack[1]=msg_code;
    if(data_len > 0)
    {
        memcpy(&data_ack[2], data, data_len);
    }
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_ACKNOWLEDGEMENT,FASTPAIR_MESSAGESTREAM_ACKNOWLEDGEMENT_CODE_ACK,
                               data_ack,MESSAGE_STREAM_ACKNOWLEDGEMENT_ACK_DATA_LEN+data_len, fp_seeker_number);
}
 
uint8* fastPair_MsgStream_GenerateNonce(void)
{
    unsigned valid_nonce_idx,i;
    uint8 rand1;
    uint8* nonce = (uint8*)PanicUnlessMalloc(FASTPAIR_MESSAGESTREAM_SESSION_NONCE_LEN);

    nonce[0] = UtilRandom()&0xFF;
    valid_nonce_idx = 1;
    while(valid_nonce_idx < FASTPAIR_MESSAGESTREAM_SESSION_NONCE_LEN)
    {
        rand1 = UtilRandom()&0xFF;
        /* Check for duplicates */
        for(i=0;i<valid_nonce_idx;i++)
        {
            if(rand1 == nonce[i])
                 break;
        }
        /* No duplicates found*/
        if(i == valid_nonce_idx)
        {
            nonce[valid_nonce_idx] = rand1;
            valid_nonce_idx++;
        }
    }

    return nonce;
}

uint8* FastPair_MsgStream_GetSessionNonce(uint8 remote_device_id)
{
    if(fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1] != NULL)
    {
        return fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1];
    }
    return NULL;
}

void FastPair_MsgStream_SetSessionNonce(uint8* session_nonce, uint8 remote_device_id)
{
    DEBUG_LOG("FastPair_MsgStream_SetSessionNonce: remote device id: %d", remote_device_id);
    fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1] = (uint8*)PanicUnlessMalloc(FASTPAIR_MESSAGESTREAM_SESSION_NONCE_LEN);
    memcpy(fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1],
           session_nonce, FASTPAIR_MESSAGESTREAM_SESSION_NONCE_LEN);
}

/*! \brief Generate session nonce to seeker when RFCOMM connects.
 */
static void fastPair_MsgStream_SessionNonce_HandleConnectInd(uint8 remote_device_id)
{
    DEBUG_LOG("fastPair_MsgStream_SessionNonce_HandleConnectInd from remote_device_id %d", remote_device_id);

    if( (remote_device_id > 0) && (remote_device_id <= FASTPAIR_RFCOMM_CONNECTIONS_MAX))
    {
        fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1] = fastPair_MsgStream_GenerateNonce();
    }
    else
    {
        DEBUG_LOG("fastPair_MsgStream_SessionNonce_HandleConnectInd: invalid remote device id received %d", remote_device_id);
    }
}

/*! \brief Clear session nonce when RFCOMM disconnects.
 */
static void fastPair_MsgStream_SessionNonce_HandleDisconnectInd(uint8 remote_device_id)
{
    DEBUG_LOG("fastPair_MsgStream_SessionNonce_HandleDisconnectInd from remote_device_id %d", remote_device_id);

    if((remote_device_id == 1 || remote_device_id == 2) && fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1])
    {
        free(fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1]);
        fast_pair_msg_stream_data.sass_session_nonce[remote_device_id-1] = NULL;
    }
}

static void msgStream_InitData(void)
{
    fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data=FALSE;
    fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data=FALSE;
}

static void msgStream_InitCallbacks(void)
{
    fast_pair_msg_stream_data.bluetooth_event_msgs_callback = NULL;
    fast_pair_msg_stream_data.companion_app_event_msgs_callback = NULL;
    fast_pair_msg_stream_data.dev_info_event_msgs_callback = NULL;
    fast_pair_msg_stream_data.dev_action_event_callback = NULL;
    fast_pair_msg_stream_data.sass_event_callback = NULL;
}

static void msgStream_MessageDataToGroup(uint8 message_group,uint8 fp_seeker_number, uint8 *data,uint16 data_len)
{
    switch(message_group)
    {
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_BLUETOOTH_EVENT:
        {
            if(fast_pair_msg_stream_data.bluetooth_event_msgs_callback)
            {
                fast_pair_msg_stream_data.bluetooth_event_msgs_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,fp_seeker_number,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: bluetooth_event_msgs_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_COMPANION_APP_EVENT:
        {
            if(fast_pair_msg_stream_data.companion_app_event_msgs_callback)
            {
                fast_pair_msg_stream_data.companion_app_event_msgs_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,fp_seeker_number,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: companion_app_event_msgs_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT:
        {
            if(fast_pair_msg_stream_data.dev_info_event_msgs_callback)
            {
                fast_pair_msg_stream_data.dev_info_event_msgs_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,fp_seeker_number,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: dev_info_msgs_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT:
        {
            if(fast_pair_msg_stream_data.dev_action_event_callback)
            {
                fast_pair_msg_stream_data.dev_action_event_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,fp_seeker_number,data,data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: dev_action_event_callback not registered");
            }
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT:
        {
            if(fast_pair_msg_stream_data.sass_event_callback)
            {
                fast_pair_msg_stream_data.sass_event_callback(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA,
                                                              fp_seeker_number, data, data_len);
            }
            else
            {
                DEBUG_LOG_WARN("msgStream_MessageDataToGroup: sass_event_callback not registered");
            }
        }
        break;
        default:
        {
            DEBUG_LOG_WARN("msgStream_MessageDataToGroup: Data arrived on Unsupported group no %d",message_group );
        }
    }
}

static bool msgStream_IsValidMeassageGroup(uint8 message_group)
{
    if((message_group > FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_UNKNOWN) && (message_group < FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_MAX))
        return TRUE;
    else
        return FALSE;
}

static uint16 msgStream_HandleIncomingData(uint8 fp_seeker_number, const uint8 *data, uint16 data_len)
{
    uint8 message_group;
    uint8 message_code;
    uint8 additional_data_len;
    int i;
    uint16 processed_len = 0;
    uint8 *received_data;
    uint16 received_data_len;

    if((NULL == data)||(0 == data_len))
    {
        DEBUG_LOG_WARN("msgStream_HandleIncomingData: Length is 0 or data is NULL");
        return processed_len;
    }

    fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = TRUE;
    received_data = (uint8 *)data;
    received_data_len = data_len;

    /* Check if the data fits is good enough*/

    DEBUG_LOG("msgStream_HandleIncomingData: received_data values received_data_len %d ",received_data_len);
    for(i =0;i<received_data_len;++i)
        DEBUG_LOG_V_VERBOSE("%02x",received_data[i]);

    while(received_data_len >= FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM)
    {
        message_group = received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX];           
        message_code  = received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX];
        additional_data_len = (received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_UPPER_INDEX]<<8)+received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_ADD_DATA_LEN_LOWER_INDEX];

        DEBUG_LOG("msgStream_HandleIncomingData: message_group %d, message_code %d, additional_data_len %d",
                   received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_INDEX],received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX],additional_data_len );

        /* If received Message group is invalid dump the entire content in RFCOMM source buffer */
        if (!msgStream_IsValidMeassageGroup(message_group))
        {
            additional_data_len = 0;
        }
        
        if(received_data_len < (additional_data_len+FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM))
        {
            DEBUG_LOG("msgStream_HandleIncomingData: received_data_len %d additional_data_len %d",
                        received_data_len,additional_data_len);
            fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = FALSE;
            return processed_len;
        }

         /* Call data Handler here */
        msgStream_MessageDataToGroup(message_group,fp_seeker_number,&received_data[FASTPAIR_MESSAGESTREAM_MESSAGE_CODE_INDEX],(additional_data_len + FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM-1));

        /* If received Message group is invalid dump the entire content in RFCOMM source buffer */
        if(!msgStream_IsValidMeassageGroup(message_group))
        {
            processed_len = data_len;
            fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = FALSE;
            return processed_len;
        }
        /* Update processed length as we have a valid mesage */
        processed_len += additional_data_len + FASTPAIR_MESSAGESTREAM_MESSAGE_LENGTH_MINIMUM;

        DEBUG_LOG("msgStream_HandleIncomingData: Processed message_group %d message_code %d processed_len %d new_len %d",
                    message_group,message_code,processed_len,(data_len - processed_len) );

        /* As a valid message is processed, data pointer and data length */
        received_data = (uint8 *)data + processed_len;
        received_data_len = data_len - processed_len;

    }
    fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data = FALSE;
    return processed_len;
}

static void msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type, uint8 fp_seeker_number, const uint8 *msg_data, uint16 msg_len)
{
    if(fast_pair_msg_stream_data.bluetooth_event_msgs_callback)
        fast_pair_msg_stream_data.bluetooth_event_msgs_callback(msg_type,fp_seeker_number,msg_data,msg_len);
    if(fast_pair_msg_stream_data.companion_app_event_msgs_callback)
        fast_pair_msg_stream_data.companion_app_event_msgs_callback(msg_type,fp_seeker_number,msg_data,msg_len);
    if(fast_pair_msg_stream_data.dev_info_event_msgs_callback)
        fast_pair_msg_stream_data.dev_info_event_msgs_callback(msg_type,fp_seeker_number,msg_data,msg_len);
    if(fast_pair_msg_stream_data.dev_action_event_callback)
        fast_pair_msg_stream_data.dev_action_event_callback(msg_type,fp_seeker_number,msg_data,msg_len);
    if(fast_pair_msg_stream_data.sass_event_callback)
        fast_pair_msg_stream_data.sass_event_callback(msg_type,fp_seeker_number,msg_data,msg_len);
}

bool fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP msg_group, fastPair_MsgStreamMsgCallBack msgCallBack)
{
    switch(msg_group)
    {
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_BLUETOOTH_EVENT:
        {
            fast_pair_msg_stream_data.bluetooth_event_msgs_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_COMPANION_APP_EVENT:
        {
            fast_pair_msg_stream_data.companion_app_event_msgs_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVICE_INFORMATION_EVENT:
        {
            fast_pair_msg_stream_data.dev_info_event_msgs_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_DEVCIE_ACTION_EVENT:
        {
            fast_pair_msg_stream_data.dev_action_event_callback = msgCallBack;
        }
        break;
        case FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT:
            fast_pair_msg_stream_data.sass_event_callback = msgCallBack;
        break;
        default:
        {
            DEBUG_LOG_WARN("fastPair_MsgStreamRegisterGroupMessages: Unsupported group no %d",msg_group );
            return FALSE;
        }
    }
    return TRUE;
}

static uint16 msgStream_MessageHandler(FASTPAIR_RFCOMM_MESSAGE_TYPE msg_type, uint8 fp_seeker_number, const uint8 *msg_data, uint16 msg_len)
{
    uint16 ret_val = 0;

    switch(msg_type)
    {
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_CONNECT_IND:
             msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND, fp_seeker_number, NULL, 0);
             fastPair_MsgStream_SessionNonce_HandleConnectInd(fp_seeker_number);
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_SERVER_CONNECT_CFM:
              msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM, fp_seeker_number, msg_data, msg_len);
              msgStream_InitData();
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_INCOMING_DATA:
             ret_val = msgStream_HandleIncomingData(fp_seeker_number, msg_data, msg_len);
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_IND:
             msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND, fp_seeker_number, NULL, 0);
             msgStream_InitData();
             fastPair_MsgStream_SessionNonce_HandleDisconnectInd(fp_seeker_number);
        break;
        case FASTPAIR_RFCOMM_MESSAGE_TYPE_DISCONNECT_CFM:
             msgStream_MessageMulticastToClients(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM, fp_seeker_number, NULL, 0);
             msgStream_InitData();
        break;
        default:
             DEBUG_LOG("msgStream_MessageHandler: Unknown message=%x", msg_type);
        break;
    }
    return ret_val;
}

/*! \brief Init Session Nonce pointers.
 */
static void fastPair_MsgStreamSessionNonceInit(void)
{
    for(unsigned i=0; i < FASTPAIR_RFCOMM_CONNECTIONS_MAX; i++)
    {
        fast_pair_msg_stream_data.sass_session_nonce[i] = NULL;
    }
}

bool fastPair_MsgStreamIsConnected(uint8 fp_seeker_number)
{
    uint8 instance_number = fp_seeker_number - 1;
    return fastPair_RfcommInstanceNumberIsConnected(instance_number);
}

bdaddr fastPair_MsgStreamGetDeviceAddr(uint8 fp_seeker_number)
{
    uint8 instance_number = fp_seeker_number - 1;
    return fastPair_RfcommGetDeviceAddr(instance_number);
}

uint8 FastPair_MsgStreamGetRemoteDeviceId(bdaddr *device_addr)
{
    uint8 remote_device_id = 0;
    uint8 rfcomm_instance_number = 0;

    rfcomm_instance_number = fastPair_RfcommGetInstanceNumber(device_addr);
    remote_device_id = rfcomm_instance_number + 1;

    return remote_device_id;
}

bool fastPair_MsgStreamIsBusy(void)
{
   return (fast_pair_msg_stream_data.is_msg_stream_busy_incoming_data|| fast_pair_msg_stream_data.is_msg_stream_busy_outgoing_data) ;
}

void fastPair_MsgStreamInit(void)
{
    msgStream_InitCallbacks();
    msgStream_InitData();

    fastPair_RfcommInit();
    fastPair_RfcommRegisterMessage(msgStream_MessageHandler);

    fastPair_ProfileInit();

    /* Register Message Group handlers here */
    fastPair_MsgStreamDevInfo_Init();
    fastPair_MsgStreamDevAction_Init();
    Sass_Init();

    fastPair_MsgStreamSessionNonceInit();
}
