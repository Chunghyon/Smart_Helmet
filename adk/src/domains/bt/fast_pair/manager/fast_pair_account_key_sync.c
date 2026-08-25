/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_account_key_sync.c
\brief      Component handling synchronization of fast pair account keys between peers 

*/

#include "fast_pair_account_key_sync.h"
#include "fast_pair.h"
#include "device_properties.h"
#include "user_accounts.h"

#include <marshal_common.h>
#include <marshal.h>
#include <peer_signalling.h>
#include <bt_device.h>
#include <device_list.h>
#include <task_list.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include <stdio.h>
/*! Global Instance of Account Key Sync Task Data */
account_key_sync_task_data_t account_key_sync;
/*!Definition of marshalled messages used by Account Key Sync. */
const marshal_member_descriptor_t account_key_sync_req_member_descriptors[] =
{
    MAKE_MARSHAL_MEMBER_ARRAY(account_key_sync_req_t, uint16, account_key_index, ACCOUNT_KEY_INDEX_LEN),
    MAKE_MARSHAL_MEMBER_ARRAY(account_key_sync_req_t, uint16, account_keys, ACCOUNT_KEY_DATA_LEN),
};

const marshal_type_descriptor_t marshal_type_descriptor_account_key_sync_req_t =
    MAKE_MARSHAL_TYPE_DEFINITION(account_key_sync_req_t, account_key_sync_req_member_descriptors);

const marshal_type_descriptor_t marshal_type_descriptor_account_key_sync_cfm_t =
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(sizeof(account_key_sync_cfm_t));

/*! X-Macro generate account key sync marshal type descriptor set that can be passed to a (un)marshaller to initialise it.
 */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *)&marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const account_key_sync_marshal_type_descriptors[NUMBER_OF_MARSHAL_OBJECT_TYPES] = {
    MARSHAL_COMMON_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    MARSHAL_TYPES_TABLE_ACCOUNT_KEY_SYNC(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION

/*! \brief Send the marshalled data to the peer
 */
static void accountKeySync_SendMarshalledData(account_key_sync_req_t *sync_data)
{
    bdaddr peer_addr;

    if(appDeviceGetPeerBdAddr(&peer_addr))
    {
        DEBUG_LOG_V_VERBOSE("accountKeySync_SendMarshalledData. Account Key Index Info");
        for(uint8 index = 0; index < MAX_NUM_ACCOUNT_KEYS; index++)
        {
            uint32 temp = sync_data->account_key_index[index];
            DEBUG_LOG_V_VERBOSE("%04x", temp);
        }
        DEBUG_LOG_V_VERBOSE("accountKeySync_SendMarshalledData : Account Keys Info");
        for(uint8 account_key =0; account_key < ACCOUNT_KEY_DATA_LEN; account_key++)
        {
            uint16 temp = sync_data->account_keys[account_key];
            DEBUG_LOG_V_VERBOSE("%02x", temp);
        }
        
        DEBUG_LOG_DEBUG("accountKeySync_SendMarshalledData. Send Marshalled Data to the peer.");
        /*! send the account key index and account keys to counterpart on other earbud */
        appPeerSigMarshalledMsgChannelTx(accountKeySync_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_FP_ACCOUNT_KEY_SYNC,
                                         sync_data, MARSHAL_TYPE_account_key_sync_req_t);
    }
    else
    {
        DEBUG_LOG_DEBUG("accountKeySync_SendMarshalledData. No Peer to send to.");
    }
}

/*! \brief Send the confirmation of synchronization to primary device
 */
static void accountKeySync_SendConfirmation(bool synced)
{
    if (appPeerSigIsConnected())
    {
        account_key_sync_cfm_t* cfm = PanicUnlessMalloc(sizeof(account_key_sync_cfm_t));
        cfm->synced = synced;
        DEBUG_LOG("accountKeySync_SendConfirmation. Send confirmation to the peer.");
        /*! send confirmation of account key received */
        appPeerSigMarshalledMsgChannelTx(accountKeySync_GetTask(),
                                         PEER_SIG_MSG_CHANNEL_FP_ACCOUNT_KEY_SYNC,
                                         cfm, MARSHAL_TYPE_account_key_sync_cfm_t);
    }
    else
    {
        DEBUG_LOG("accountKeySync_SendConfirmation. No Peer to send to.");
    }
}

/*! \brief Handle confirmation of transmission of a marshalled message.
 */
static void accountKeySync_HandleMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    DEBUG_LOG("accountKeySync_HandleMarshalledMsgChannelTxCfm channel %u status %u", cfm->channel, cfm->status);
}

/*! \brief Handle incoming marshalled messages from peer account key sync component.
 */
static void accountKeySync_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        case MARSHAL_TYPE_account_key_sync_req_t:
        {
            bool synced;
            account_key_sync_req_t* req = (account_key_sync_req_t*)ind->msg;
            DEBUG_LOG("accountKeySync_HandleMarshalledMsgChannelRxInd RX Account Key ");
            /*! Store the Account keys and send the confirmation to the peer */
            synced = UserAccounts_StoreAllAccountKeys(req);
            accountKeySync_SendConfirmation(synced);
            free(req);
        }
        break;

        case MARSHAL_TYPE_account_key_sync_cfm_t:
        {
            account_key_sync_cfm_t *cfm = (account_key_sync_cfm_t*)ind->msg;
            if(!cfm->synced)
            {
                DEBUG_LOG("accountKeySync_HandleMarshalledMsgChannelRxInd. Failed to Synchronize.");
            }
            else
            {
                DEBUG_LOG("accountKeySync_HandleMarshalledMsgChannelRxInd. Synchronized successfully.");
            }
            free(cfm);
        }
        break;

        default:
            break;
    }
}

/*! \brief Account Key Sync Message Handler
 */
static void accountKeySync_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
            /* marshalled messaging */
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            DEBUG_LOG("accountKeySync_HandleMessage. PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND");
            accountKeySync_HandleMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T*)message);
            break;
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            DEBUG_LOG("accountKeySync_HandleMessage. PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM");
            accountKeySync_HandleMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T*)message);
            break;

        default:
            break;
    }
}

/*! \brief Account Key Sync Initialization
 */
void AccountKeySync_Init(void)
{
    DEBUG_LOG("AccountKeySync_Init");
    account_key_sync_task_data_t *key_sync = accountKeySync_GetTaskData();

    /* Initialize component task data */
    memset(key_sync, 0, sizeof(*key_sync));
    key_sync->task.handler = accountKeySync_HandleMessage;

    /* Register with peer signalling to use the account key sync msg channel */
    appPeerSigMarshalledMsgChannelTaskRegister(accountKeySync_GetTask(),
                                               PEER_SIG_MSG_CHANNEL_FP_ACCOUNT_KEY_SYNC,
                                               account_key_sync_marshal_type_descriptors,
                                               NUMBER_OF_MARSHAL_OBJECT_TYPES);
    DEBUG_LOG("accountKeySync_Init. Initialized successfully. ");
}

/*! \brief Fast Pair Account Key Synchronization API
 */
void AccountKeySync_Sync(void)
{
    DEBUG_LOG("AcountKeySync_Sync. Synchronization starts.");
    device_t my_device = BtDevice_GetSelfDevice();
    void *account_key_index_value = NULL;
    void *account_keys_value = NULL;
    size_t account_key_index_size = 0;
    size_t account_keys_size = 0;

    if(my_device)
    {
        if(appPeerSigIsConnected())
        {
            if(Device_GetProperty(my_device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
                Device_GetProperty(my_device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
            {
                account_key_sync_req_t *sync_data = PanicUnlessMalloc(sizeof(account_key_sync_req_t));
                uint32 * temp = (uint32 *)account_key_index_value;
                for(uint8 i = 0; i < MAX_NUM_ACCOUNT_KEYS; i++)
                {
                    sync_data->account_key_index[i] = temp[i];
                }
                memcpy(sync_data->account_keys, account_keys_value, sizeof(sync_data->account_keys));
                accountKeySync_SendMarshalledData(sync_data);
            }
            else
            {
                DEBUG_LOG("AccountKeySync_Sync. Should not reach here.Unexpected Data.");
            }
        }
        else
        {
            DEBUG_LOG("AccountKeySync_Sync. Peer signaling not conencted");
        }
    }
    else
    {
        DEBUG_LOG("AccountKeySync_Sync. SELF device does not exist.");
    }
}
