/*
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_sig_static_data.c
\brief      Statically defined data may be peer signalled by using this API. As images on both buds are
            identical, the address of static data is the same and data may be peer signalled by simply
            providing address and size. Note: Peer signallng implementation limits single object within
            marshalling descriptor to max. 255 bytes.
            Usage:
            1. PeerSigStaticData_Register
            2. PeerSigStaticData_Transmit (primary)
            ... Data transmitted will be received on Secondary by the registered peer signalling task.
            ... Task receives a PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND message id and calls...
            3. PeerSigStaticData_Receive (secondary) to write received data into memory
*/

#include "peer_sig_static_data.h"
#include "peer_sig_static_data_descriptor.h"

#include <peer_signalling.h>
#include <panic.h>
#include <logging.h>
#include <limits.h>

static uint32 peerSigStaticData_GetStaticDataSize(const void *object,
                             const marshal_member_descriptor_t *member,
                             uint32 arrayElement);

static const marshal_member_descriptor_t member_descriptors_peer_sig_static_data[] =
{
    MAKE_MARSHAL_MEMBER(peer_sig_static_data_t, uint32, data_addr),
    MAKE_MARSHAL_MEMBER(peer_sig_static_data_t, uint8, data_length),
    MAKE_MARSHAL_MEMBER_ARRAY(peer_sig_static_data_t, uint8, data, 1),
};

static const marshal_type_descriptor_dynamic_t marshal_type_descriptor_peer_sig_static_data =
    MAKE_MARSHAL_TYPE_DEFINITION_HAS_DYNAMIC_ARRAY(
        peer_sig_static_data_t,
        member_descriptors_peer_sig_static_data,
        peerSigStaticData_GetStaticDataSize);

/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const descriptor_peer_sig_static_data[NUMBER_OF_MARSHAL_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION

static uint32 peerSigStaticData_GetStaticDataSize(const void *object,
                             const marshal_member_descriptor_t *member,
                             uint32 arrayElement)
{
    UNUSED(member);
    UNUSED(arrayElement);

    peer_sig_static_data_t* peer_sig_static_data = (peer_sig_static_data_t*)object;
    return peer_sig_static_data->data_length;
}

static void* peerSigStaticData_CreateMessage(const uint8* data_addr, uint16 data_size)
{
    PanicFalse(data_size < UINT8_MAX);
    peer_sig_static_data_t* temp = (peer_sig_static_data_t*)PanicUnlessMalloc(sizeof(peer_sig_static_data_t)+data_size);
    temp->data_addr = (uint32)data_addr;
    temp->data_length = data_size;
    memcpy(temp->data, (void*)temp->data_addr, temp->data_length);
    return temp;
}

void PeerSigStaticData_Receive(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch(ind->type)
    {
        case MARSHAL_TYPE_peer_sig_static_data:
        {
            peer_sig_static_data_t* temp = ((peer_sig_static_data_t*)ind->msg);
            DEBUG_LOG("PeerSigStaticData_Receive: PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND %d %p", temp->data_length, temp->data);
            memcpy((void*)temp->data_addr, temp->data, temp->data_length);
        }
        break;

        default:
            Panic();
        break;
    }

    /* Free unmarshalled message after use. */
    free(ind->msg);
}


void PeerSigStaticData_Transmit(Task task, peerSigMsgChannel channel, void* data_addr, uint16 data_size)
{
    PanicFalse(data_size <= UCHAR_MAX);
    if(appPeerSigIsConnected())
    {
        DEBUG_LOG("PeerSigStaticData_Transmit: send peer data addr %p size %d channel %d", data_addr, data_size, channel);
        appPeerSigMarshalledMsgChannelTx(task,
                                            channel,
                                            peerSigStaticData_CreateMessage(data_addr, data_size),
                                            MARSHAL_TYPE_peer_sig_static_data);
    }
}

void PeerSigStaticData_Register(Task task, peerSigMsgChannel channel)
{
    DEBUG_LOG("PeerSigStaticData_Register channel=enum:peerSigMsgChannel:%d", channel);
    appPeerSigClientRegister(task);
    appPeerSigMarshalledMsgChannelTaskRegister(task,
                                               channel,
                                               descriptor_peer_sig_static_data,
                                               NUMBER_OF_MARSHAL_OBJECT_TYPES);
}
