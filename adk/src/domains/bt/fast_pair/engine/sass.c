/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       sass.c
\brief      Component handling GFP Smart Audio Source Switching.
*/
#include <logging.h>
#include <panic.h>
#include <device_list.h>
#include <av_instance.h>
#include <profile_manager.h>
#include <ui.h>
#include <handset_context.h>

#include "av.h"
#include "audio_sources.h"
#include "sass.h"
#include "fast_pair_msg_stream.h"
#include "fast_pair.h"
#include "fast_pair_adv_sass.h"
#include "cryptoalgo.h"
#include "user_accounts.h"
#include "context_framework.h"
#include "handset_service.h"
#include "ui.h"
#include "bt_device.h"
#include "focus_device.h"
#include "device_properties.h"
#include "fast_pair_pname_state.h"
#include "focus_select.h"
#include "device_db_serialiser.h"
#include "dfu.h"
#include "connection_manager.h"

#define SASS_CONNECTION_STATE_MASK 0x0F
#define SASS_CONNECTION_FLAG_MASK  0xF0

#define SASS_GET_CONNECTION_STATE(pSassTaskData) ((pSassTaskData->con_status.connection_state & SASS_CONNECTION_STATE_MASK))
#define SASS_SET_CONNECTION_STATE(pSassTaskData, newState) (pSassTaskData->con_status.connection_state = \
                                                 ((pSassTaskData->con_status.connection_state & SASS_CONNECTION_FLAG_MASK) | (newState)))

/*! SASS task data */
sassTaskData sass_task_data;

static sass_connection_status_change_callback sass_conn_status_change_cb = NULL;

extern fast_pair_adv_sass_t fast_pair_adv_sass;

const message_group_t sass_ui_inputs[] =
{
    UI_INPUTS_HANDSET_MESSAGE_GROUP
};

/*! \brief Get pointer to SASS task data structure.
\param void
\return sassTaskData pointer to SASS task data structure
*/
static sassTaskData *sass_GetTaskData(void)
{
    return (&sass_task_data);
}

/*! \brief Get pointer to SASS task handler.
\param void
\return Task SASS task handler
*/
static Task sass_GetTask(void)
{
    return &sass_task_data.task;
}

static bool sass_GetDeviceForUiInput(ui_input_t ui_input, device_t * device);

static const focus_device_t override_fns_for_device =
{
    .for_context = NULL,
    .for_ui_input = sass_GetDeviceForUiInput,
    .focus = NULL
};

/*! \brief Check if remove device is valid which sends SASS message.
\param remote device id
\return TRUE if remote device id is valid, FALSE otherwise.
*/
static bool sass_IsValidRemoteDevice(uint8 remote_device_id)
{
    if (fastPair_MsgStreamIsConnected(remote_device_id))
    {
        return TRUE;
    }

    return FALSE;
}

static void sass_UpdateActiveHandset(device_t active_handset, uint8 switch_event_reason)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    bdaddr new_active_handset_addr = DeviceProperties_GetBdAddr(active_handset);
    /* If there was no active handset, then no need to send notification */
    bool send_notification = !BdaddrIsZero(&sass_task_ptr->current_active_handset_addr);

    DEBUG_LOG("sass_UpdateActiveHandset : Switch reason = %d", switch_event_reason);
    if(!BdaddrIsSame(&sass_task_ptr->current_active_handset_addr, &new_active_handset_addr))
    {
        /* Update the inuse account key when active source changes */
        FastPair_SASSUpdateInUseAccountKeyForMruDevice(&new_active_handset_addr);
        /* Update the current active handset BD addr */
        sass_task_ptr->current_active_handset_addr = new_active_handset_addr;
        if(send_notification && BtDevice_GetNumberOfHandsetsConnectedOverBredr() > 1)
        {
            Sass_NotifyMultipointSwitchEvent(switch_event_reason);
        }

        /* When there is a change in active seeker after executing switch active audio source event,
           reset switch active audio source related flags and disable a2dp barge-in for SASS.
           We shall enable it when the SASS seeker sends a new switch active audio source message. */
        if(sass_task_ptr->switch_active_audio_source_event.is_switch_active_audio_src_pending)
        {
            if(BdaddrIsSame(&sass_task_ptr->current_active_handset_addr, &sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr))
            {
                Sass_ResetSwitchActiveAudioSourceStates();
                FocusSelect_EnableA2dpBargeInForSASS(FALSE);
            }
            else
            {
                DEBUG_LOG("sass_UpdateActiveHandset: Could not reset Switch Active Audio Source states");
            }
        }
    }
}

static bool sass_ContextHandlePhyState(void)
{
    context_physical_state_t phy_state = 0xFF;
    bool valid_context = FALSE;
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    DEBUG_LOG("sass_ContextHandlePhyState");
    if(ContextFramework_GetContextItem(context_physical_state, (unsigned *)&phy_state, sizeof(context_physical_state_t)))
    {
        if(phy_state == on_head)
        {
            sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state | SASS_AD_ON_HEAD_DETECTION;
        }
        else
        {
            sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state & ~(SASS_AD_ON_HEAD_DETECTION);
        }
        valid_context = TRUE;
    }
    return valid_context;
}

static bool sass_ContextHandleActiveSourceInfo(void)
{
    context_active_source_info_t source_info;
    bool valid_context = FALSE;
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    DEBUG_LOG("sass_ContextHandleActiveSourceInfo");

    if (Sass_IsConnectionSwitchDisabled() || 
        (SASS_GET_CONNECTION_STATE(sass_task_ptr) == SASS_NON_AUDIO_DATA))
    {
        DEBUG_LOG("sass_ContextHandleActiveSourceInfo: Connection Switch Disabled.");
        return FALSE;
    }

    if(ContextFramework_GetContextItem(context_active_source_info, (unsigned *)&source_info, sizeof(context_active_source_info_t)))
    {
        if(source_info.handset != NULL)
        {
            if(source_info.active_source.type == source_type_audio)
            {
                switch(source_info.active_source.u.audio)
                {
                    case audio_source_none:
                    {
                        sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_CONNECTED_NO_DATA;
                    }
                    break;

                    case audio_source_a2dp_1:
                    case audio_source_a2dp_2:
                    {
                        if(source_info.has_control_channel == TRUE)
                        {
                            sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_A2DP_STREAMING_WITH_AVRCP;
                        }
                        else
                        {
                            sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_A2DP_STREAMING_ONLY;
                        }
                        sass_UpdateActiveHandset(source_info.handset, SASS_NOTIFY_MP_SWITCH_EVENT_REASON_A2DP_STREAMING);
                    }
                    break;

                    default:
                        DEBUG_LOG("sass_ContextHandleActiveSourceInfo. Unsupported audio source.");
                        break;
                }
            }
            else if(source_info.active_source.type == source_type_voice)
            {
                switch(source_info.active_source.u.voice)
                {
                    case voice_source_none:
                    {
                        sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_CONNECTED_NO_DATA;
                    }
                    break;

                    case voice_source_hfp_1:
                    case voice_source_hfp_2:
                    {
                        sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_HFP_CALL;
                        sass_UpdateActiveHandset(source_info.handset, SASS_NOTIFY_MP_SWITCH_EVENT_REASON_HFP);
                    }
                    break;

                    default:
                        DEBUG_LOG("sass_ContextHandleActiveSourceInfo. Unsupported voice source.");
                        break;
                }
            }
            else
            {
                DEBUG_LOG("sass_ContextHandleActiveSourceInfo. Unidentifiable source type.");
                sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_CONNECTED_NO_DATA;
            }
        }
        else
        {
            DEBUG_LOG("sass_ContextHandleActiveSourceInfo. Connected but no active source.");
            sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_CONNECTED_NO_DATA;
        }
        valid_context = TRUE;
    }
    return valid_context;
}

static bool sass_ContextHandleConnectedHandsetsInfo(void)
{
    context_connected_handsets_info_t handset_info;
    bool valid_context = FALSE;
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    DEBUG_LOG("sass_ContextHandleConnectedHandsetsInfo");
    if(ContextFramework_GetContextItem(context_connected_handsets_info, (unsigned *)&handset_info, sizeof(context_connected_handsets_info_t)))
    {
        device_t *devices = NULL;
        uint8 index = 0;
        uint8 handset_index = 0;
        uint8 con_devices_bitmap = 0;
        unsigned num_connected_bredr_handsets = BtDevice_GetConnectedBredrHandsets(&devices);

        /* Update the connected device bitmap based on BR/EDR ACL connection status */
        if(num_connected_bredr_handsets > 0)
        {
            for(index = 0; index < num_connected_bredr_handsets; index++)
            {
                if(devices[index] != NULL)
                {
                  handset_index = DeviceList_GetIndexOfDevice(devices[index]);
                  con_devices_bitmap = con_devices_bitmap | (1 << handset_index);
                }
            }
        }
        free(devices);

        DEBUG_LOG("sass_ContextHandleConnectedHandsetsInfo: connected device new bitmap is 0x%x, cur bitmap is 0x%x, conn state 0x%x",
                   con_devices_bitmap, sass_task_ptr->con_status.connected_devices_bitmap, SASS_GET_CONNECTION_STATE(sass_task_ptr));
        if(sass_task_ptr->con_status.connected_devices_bitmap != con_devices_bitmap)
        {
            sass_task_ptr->con_status.connected_devices_bitmap = con_devices_bitmap;
            valid_context = TRUE;
        }

        if (SASS_GET_CONNECTION_STATE(sass_task_ptr) != SASS_NO_CONNECTION && 
            num_connected_bredr_handsets == 0)
        {
            valid_context = TRUE;
        }

        /* Update the connection availability flag bit upon getting the device bitmap update */
        if(valid_context)
        {
            if(num_connected_bredr_handsets < HandsetService_GetMaxNumberOfConnectedBredrHandsets())
            {
                sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state | SASS_AD_CONNECTION_AVAILABILITY;
            }
            else
            {
                sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state & ~(SASS_AD_CONNECTION_AVAILABILITY);
            }

            if (Sass_IsConnectionSwitchDisabled() ||
                (SASS_GET_CONNECTION_STATE(sass_task_ptr) == SASS_NON_AUDIO_DATA))
            {
                DEBUG_LOG("sass_ContextHandleActiveSourceInfo: Connection Switch Disabled.");
                return valid_context;
            }

            /* When there is no connected handset, move the connection state to NO_CONNECTION */
            if(num_connected_bredr_handsets == 0)
            {
                DEBUG_LOG("sass_ContextHandleConnectedHandsetsInfo. No handset connected.");
                sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_NO_CONNECTION;
            }

            /* When device/s is/are connected but the connection state is NO_CONNECTION, update it to CONNECTED_NO_DATA state */
            if(num_connected_bredr_handsets != 0 && ((sass_task_ptr->con_status.connection_state & 0x0F) == SASS_NO_CONNECTION))
            {
                DEBUG_LOG("sass_ContextHandleConnectedHandsetsInfo. Handset/s is/are connected connected but no streaming/call.");
                sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_CONNECTED_NO_DATA;
            }
        }

    }
    return valid_context;
}

/*! \brief Notify our SASS capability to connected Seeker when Seeker requests for our SASS capability.
 */
static sass_message_rsp sass_NotifyCapabilityToSeeker(uint8 remote_device_id)
{
    DEBUG_LOG("sass_NotifyCapabilityToSeeker: Notify sass capability to remote device id: %d", remote_device_id);

    uint8 sass_notify_capability_data[SASS_NOTIFY_CAPABILITY_ADD_DATA_LEN] = {0};
    uint8 sass_capability_flag = 0;
    context_physical_state_t phy_state = 0xFF;
    sass_message_rsp msg_response = sass_no_ack_message;

    /* Check multipoint current state */
    if(HandsetService_IsBrEdrMultipointEnabled())
    {
        DEBUG_LOG("sass_NotifyCapabilityToSeeker: MP enabled");
        sass_capability_flag = sass_capability_flag | SASS_MP_CURRENT_STATE;
    }
    else
    {
        DEBUG_LOG("sass_NotifyCapabilityToSeeker: MP disabled");
        sass_capability_flag = sass_capability_flag & ~(SASS_MP_CURRENT_STATE);
    }

    /* Check on-head detection current state */
    if(ContextFramework_GetContextItem(context_physical_state, (unsigned *)&phy_state, sizeof(context_physical_state_t)))
    {
        DEBUG_LOG("sass_NotifyCapabilityToSeeker: current phy state is %d", phy_state);
        if(phy_state == on_head)
        {
            sass_capability_flag = sass_capability_flag | SASS_ON_HEAD_DETECTION_CURRENT_STATE;
        }
        else
        {
            sass_capability_flag = sass_capability_flag & ~(SASS_ON_HEAD_DETECTION_CURRENT_STATE);
        }
    }

    /* SASS capabilities of provider */
    sass_capability_flag = sass_capability_flag | SASS_STATE | SASS_ON_HEAD_DETECTION | SASS_MP_CONFIG;

    DEBUG_LOG("sass_NotifyCapabilityToSeeker: sass_capability_flag : 0x%x", sass_capability_flag);

    sass_notify_capability_data[0] = (uint8)((SASS_VERSION_CODE >> 8) & 0xFF);
    sass_notify_capability_data[1] = (uint8)((SASS_VERSION_CODE & 0xFF));
    sass_notify_capability_data[2] = sass_capability_flag;
    sass_notify_capability_data[3] = 0x00;
    
    fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                               SASS_NOTIFY_CAPABILITY_EVENT,
                               sass_notify_capability_data,
                               SASS_NOTIFY_CAPABILITY_ADD_DATA_LEN,
                               remote_device_id);

    return msg_response;
}

/*! \brief Handle Notify Capability of SASS message from Seeker. As per spec, additional data in this message
           is not processed.
 */
static sass_message_rsp sass_HandleNotifyCapabilityFromSeeker(uint8 remote_device_id)
{
    DEBUG_LOG("sass_HandleNotifyCapabilityFromSeeker: Received from remote device id: %d", remote_device_id);
    sass_message_rsp msg_response = sass_ack_message;

    return msg_response;
}

/*! \brief Construct Encrypted Connection Status additional data for Notify Connection Status SASS message.
 */
static void sass_ConstructNotifyConnectionStatusData(uint8 remote_device_id)
{
    DEBUG_LOG("sass_ConstructNotifyConnectionStatusData: for remote_device_id: %d", remote_device_id);

    uint8* session_nonce = NULL;
    /* Total size of connection status data will be 4 bytes including 1 byte padding for AES CTR encryption */
    uint8 conn_status_raw_data[SASS_CONN_STATUS_DATA_LEN + 1] = {0};
    uint8* iv = NULL;
    uint8  data_sz_words = (SASS_CONN_STATUS_DATA_LEN + 1)/2;
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    /* Handle cross over scenario. For example, when we wait for AES CTR cfm for previous connection status change and
       we get a new connection status change update. */
    if(sass_task_ptr->conn_status_enc_pending.remote_device_id != 0xFF)
    {
        DEBUG_LOG("sass_ConstructNotifyConnectionStatusData: cannot process this request");
        return;
    }

    iv = (uint8*)PanicUnlessMalloc(SASS_IV_LEN);

    conn_status_raw_data[0] = Sass_GetConnectionState();
    conn_status_raw_data[1] = Sass_GetCustomData();
    conn_status_raw_data[2] = Sass_GetConnectedDeviceBitMap();
    /* Extra byte for padding */
    conn_status_raw_data[3] = 0x00;

    DEBUG_LOG_V_VERBOSE("sass_ConstructNotifyConnectionStatusData: conn status raw data");

    for(int i=0; i<SASS_CONN_STATUS_DATA_LEN; i++)
    {
        DEBUG_LOG_V_VERBOSE("0x%x", conn_status_raw_data[i]);
    }

    /* Get session nonce for current connection */
    session_nonce = FastPair_MsgStream_GetSessionNonce(remote_device_id);

    if(session_nonce != NULL)
    {
        uint8 *little_endian_key = PanicUnlessMalloc(SASS_INUSE_ACCOUNT_KEY_LEN);
        uint8 *little_endian_iv =  PanicUnlessMalloc(SASS_IV_LEN);
        uint8 *little_endian_conn_status_data = PanicUnlessMalloc(SASS_CONN_STATUS_DATA_LEN + 1);
        uint8* in_use_account_key = NULL;
        bool is_active_connection = TRUE;

        DEBUG_LOG_V_VERBOSE("sass_ConstructNotifyConnectionStatusData: session nonce");
        for(int i=0; i<SASS_SESSION_NONCE_LEN; i++)
        {
            DEBUG_LOG_V_VERBOSE("0x%x", *(session_nonce + i));
        }

        /* Store 8 byte message nonce which has to be sent later in Notify Connection Status SASS message */
        sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status = fastPair_MsgStream_GenerateNonce();
        /* Store this remote device id in sass task structure which will be reset after sending the notify connection
           status message for this remote device */
        sass_task_ptr->conn_status_enc_pending.remote_device_id = remote_device_id;

        DEBUG_LOG_V_VERBOSE("sass_ConstructNotifyConnectionStatusData: Generate Message Nonce");
        for(int i=0; i<SASS_MSG_NOUNCE_LEN; i++)
        {
            DEBUG_LOG_V_VERBOSE("0x%x",*(sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status + i));
        }

        /* Generate initial vector which is concatenation of session nonce and message nonce */
        memcpy(iv, session_nonce, SASS_SESSION_NONCE_LEN);
        memcpy(iv + SASS_SESSION_NONCE_LEN, sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status, SASS_MSG_NOUNCE_LEN);

        DEBUG_LOG_V_VERBOSE("sass_ConstructNotifyConnectionStatusData: Generated IV");
        for(int i=0; i< SASS_IV_LEN; i++)
        {
            DEBUG_LOG_V_VERBOSE("0x%x", *(iv+i));
        }

        in_use_account_key = fastPair_SASSGetInUseAccountKey(&is_active_connection);

        if(in_use_account_key != NULL)
        {
            DEBUG_LOG_V_VERBOSE("sass_ConstructNotifyConnectionStatusData: in use acconut key");
            for(int i=0; i<SASS_INUSE_ACCOUNT_KEY_LEN; i++)
            {
                DEBUG_LOG_V_VERBOSE("0x%x", *(in_use_account_key + i));
            }
            fastPair_ConvertEndiannessFormat(in_use_account_key, SASS_INUSE_ACCOUNT_KEY_LEN, little_endian_key);
        }
        fastPair_ConvertEndiannessFormat(iv, SASS_IV_LEN, little_endian_iv);
        fastPair_ConvertEndiannessFormat(conn_status_raw_data, (SASS_CONN_STATUS_DATA_LEN + 1), little_endian_conn_status_data);

        ConnectionEncryptDecryptAesCtrReq(&(sass_task_ptr->task), 0, cl_aes_ctr_big_endian, 
        (uint16 *)little_endian_key, (uint16 *)little_endian_iv, data_sz_words, (uint16 *)little_endian_conn_status_data);

        free(little_endian_iv);
        free(little_endian_key);
    }

    free(iv);
}

/*! \brief Kick start to construct the required data for encrypting connection status data for a remote device.
 */
static void sass_TriggerNotifyConnectionStatus(void)
{
    DEBUG_LOG("sass_TriggerNotifyConnectionStatus");

    sassTaskData *sass_task_ptr = sass_GetTaskData();
    uint8 remote_device_id = 0xFF;

    for(remote_device_id = 1; remote_device_id <= SASS_MAX_CONNECTIONS; remote_device_id++)
    {
        if(sass_task_ptr->notify_conn_status_pending[remote_device_id-1] && sass_IsValidRemoteDevice(remote_device_id))
        {
            DEBUG_LOG("sass_TriggerNotifyConnectionStatus: to remote device id: %d", remote_device_id);
            sass_ConstructNotifyConnectionStatusData(remote_device_id);
            break;
        }
    }
}

/*! \brief This function will check if an account key associated with a device is matching with
           in-use account key.
 */
static bool sass_IsAccountKeyMatchingWithInUseAccountKey(device_t device)
{
    DEBUG_LOG("sass_IsAccountKeyMatchingWithInUseAccountKey");

    bool is_acc_key_matching_with_inuse_acc_key = FALSE;

    /* Check if the device is associated with an account key. */
    if(Sass_IsAccountKeyAssociatedWithDevice(device))
    {
        bdaddr device_addr = DeviceProperties_GetBdAddr(device);
        uint8* account_key = NULL;
        uint8* in_use_account_key = NULL;
        bool   is_handset_connected_and_active = FALSE;

        in_use_account_key = fastPair_SASSGetInUseAccountKey(&is_handset_connected_and_active);
        account_key = UserAccounts_GetAccountKeyWithHandset(SASS_ACCOUNT_KEY_LEN, &device_addr);

        /* Compare the account key fetched from User Account for a device matches with in use account key */
        if((in_use_account_key != NULL && account_key != NULL) &&
            (memcmp(account_key, in_use_account_key, SASS_ACCOUNT_KEY_LEN) == 0) )
        {
            is_acc_key_matching_with_inuse_acc_key = TRUE;
        }

        if(account_key != NULL)
        {
            free(account_key);
        }
    }

    return is_acc_key_matching_with_inuse_acc_key;
}

/*! \brief Evaluate to which SASS seekers the Notify Connection Status Message has to be sent.
 */
static void sass_EvaluateSendNotifyConnStatus(void)
{
    unsigned num_connected_handsets;
    unsigned num_connected_sass_seekers = 0;
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    uint8 remote_device_id = 0xFF;

    device_t *devices_list = NULL;
    num_connected_handsets = BtDevice_GetConnectedBredrHandsets(&devices_list);

    /* Find out the number of connected SASS seekers out of the total connected handsets */
    for(unsigned count = 0; count < num_connected_handsets; count++)
    {
        if(sass_IsAccountKeyMatchingWithInUseAccountKey(*(devices_list + count)))
        {
             bdaddr handset_bdaddr = DeviceProperties_GetBdAddr(*(devices_list+count));
            /* Get remote device id associated with handset bd addr */
            remote_device_id = FastPair_MsgStreamGetRemoteDeviceId(&handset_bdaddr);

            sass_task_ptr->notify_conn_status_pending[remote_device_id-1] = TRUE;
            num_connected_sass_seekers++;
        }
    }

    DEBUG_LOG("sass_EvaluateSendNotifyConnStatus: Notify Connection Status flags: %d %d, Connected Seekers: %d",
              sass_task_ptr->notify_conn_status_pending[0],
              sass_task_ptr->notify_conn_status_pending[1],
              sass_task_ptr->number_of_connected_seekers);

    sass_task_ptr->number_of_connected_seekers = num_connected_sass_seekers;

    sass_TriggerNotifyConnectionStatus();
    free(devices_list);
}

/*! \brief Send Notify Connection Status SASS message to connected seeker.
 */
static void sass_SendNotifyConnectionStatus(uint8* enc_conn_status, uint8 enc_conn_status_size)
{
    uint8 active_device_flag = 0x00;
    uint8* notify_conn_status_add_data = NULL;
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    uint8 remote_device_id = 0xFF;
    bdaddr handset_bdaddr;

    remote_device_id = sass_task_ptr->conn_status_enc_pending.remote_device_id;
    handset_bdaddr = fastPair_MsgStreamGetDeviceAddr(remote_device_id);

    DEBUG_LOG("sass_SendNotifyConnectionStatus: Send Message to handset [%04x,%02x,%06lx] with remote device id %d",
               handset_bdaddr.nap,
               handset_bdaddr.uap,
               handset_bdaddr.lap,
               remote_device_id);

    if(sass_IsValidRemoteDevice(remote_device_id))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(&handset_bdaddr);
        if(Sass_IsAccountKeyAssociatedWithDevice(device) &&
           Focus_GetFocusForDevice(device) == focus_foreground)
        {
            /* this seeker is the active device */
            active_device_flag = 0x01;
        }

        notify_conn_status_add_data = (uint8*)PanicUnlessMalloc(SASS_NOTIFY_CONN_STATUS_ADD_DATA_LEN);

        /* construct the notify connection status additional data. */
        *notify_conn_status_add_data = active_device_flag;
        memcpy(notify_conn_status_add_data + SASS_ACTIVE_DEVICE_FLAG, enc_conn_status, enc_conn_status_size);
        memcpy(notify_conn_status_add_data + SASS_ACTIVE_DEVICE_FLAG + enc_conn_status_size, 
               sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status,
               SASS_MSG_NOUNCE_LEN);

        /* Send Notify Connection Status SASS message to connected Seeker */
        fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                                   SASS_NOTIFY_CONNECTION_STATUS_EVENT,
                                   notify_conn_status_add_data,
                                   SASS_NOTIFY_CONN_STATUS_ADD_DATA_LEN,
                                   remote_device_id);

        free(notify_conn_status_add_data);
    }

    free(sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status);
    sass_task_ptr->notify_conn_status_pending[remote_device_id-1] = FALSE;
    sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status = NULL;
    sass_task_ptr->conn_status_enc_pending.remote_device_id = 0xFF;

   /* Evaluate again to check if we need to send the message to other seeker */
   sass_TriggerNotifyConnectionStatus();
}

/* Handle Get Connection Status SASS message */
static sass_message_rsp sass_HandleGetConnectionStatus(uint8 remote_device_id)
{
    sass_message_rsp msg_response = sass_no_ack_message;
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    DEBUG_LOG("sass_HandleGetConnectionStatus: Received from remote_device_id: %d", remote_device_id);

    sass_task_ptr->notify_conn_status_pending[remote_device_id-1] = TRUE;
    sass_ConstructNotifyConnectionStatusData(remote_device_id);

   return msg_response;
}

/*! \brief Send the received custom data from connected Seeker to SASS advertising manager.
 */
static sass_message_rsp sass_HandleCustomDataMessage(uint8 custom_data, uint8 remote_device_id)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_message_rsp msg_response = sass_ack_message;

   DEBUG_LOG("sass_HandleCustomDataMessage: custom data is 0x%x from remote device %d", custom_data, remote_device_id);

    if(sass_task_ptr->con_status.custom_data != custom_data)
    {
        Sass_SetCustomData(custom_data);
        sass_EvaluateSendNotifyConnStatus();
        sass_conn_status_change_cb();
    }

    return msg_response;
}

/*! \brief Notify our SASS capability to connected Seeker when Seeker requests for our SASS capability.
 */
static sass_message_rsp sass_HandleNotifySassInitiatedConnection(uint8 sass_initiated_conn_ind, uint8 remote_device_id)
{
    sass_message_rsp msg_response = sass_ack_message;
    DEBUG_LOG("sass_NotifySassInitiatedConnection: is connection triggered by SASS: %d from remote device %d", sass_initiated_conn_ind, remote_device_id);

   return msg_response;
}

/*! \brief Handle In Use Account Key message from the seeker.
 */
static sass_message_rsp sass_HandleInUseAccountKeyMessage(const uint8* additional_data, uint8 remote_device_id, bdaddr device_addr)
{
    DEBUG_LOG("sass_Handle_InUseAccountKeyMessage: Received from remote device: %d", remote_device_id);

    const uint8 in_use_string_utf8[SASS_INUSE_ACC_KEY_STRING_LEN] = {0x69, 0x6e, 0x2d, 0x75, 0x73, 0x65};
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_message_rsp msg_response = sass_ack_message;

    if (memcmp(in_use_string_utf8, additional_data, SASS_INUSE_ACC_KEY_STRING_LEN) == 0)
    {
        memcpy(&sass_task_ptr->mru_handset_bd_addr, &device_addr, sizeof(bdaddr));
        /* If in-use account key is not updated as part of mru device property update, we shall set the in-use account key when sass seeker
           sends the in-use account key message. */
        FastPair_SASSUpdateInUseAccountKeyForMruDevice(&sass_task_ptr->mru_handset_bd_addr);
        sass_conn_status_change_cb();
    }
    else
    {
        DEBUG_LOG("sass_Handle_InUseAccountKeyMessage: invalid additional data received");
        msg_response = sass_nak_message;
    }

    return msg_response;
}

/*! \brief Handle Switch Back events from Seeker.
 */
static sass_message_rsp sass_HandleSwitchBackEvent(uint8 switch_back_event, uint8 remote_device_id)
{
    DEBUG_LOG("sass_HandleSwitchBackEvent: Received 0x%x from remote device: %d", switch_back_event, remote_device_id);
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_message_rsp msg_response = sass_ack_message;

    if(switch_back_event == SASS_SWITCH_BACK_AND_AUDIO_RESUME)
    {
        /* Set the audio resume flag which needs to be checked before audio resumption */
        sass_task_ptr->switch_back_audio_resume = TRUE;
    }

    if(switch_back_event == SASS_SWITCH_BACK || switch_back_event == SASS_SWITCH_BACK_AND_AUDIO_RESUME)
    {
        bdaddr device_addr = fastPair_MsgStreamGetDeviceAddr(remote_device_id);
        if(!BdaddrIsZero(&device_addr))
        {
            /* Disconnect the handset device which has triggered the switch back */
            HandsetService_DisconnectRequest(&sass_task_ptr->task, &device_addr, 0);
            sass_task_ptr->switch_back_is_initiated = TRUE;
        }
        else
        {
            DEBUG_LOG("sass_HandleSwitchBackEvent : Not a valid device address.");
        }
    }
    else
    {
        DEBUG_LOG("sass_HandleSwitchBackEvent: Unknown switch back event message received");
        msg_response = sass_nak_message;
    }

    return msg_response;
}

/*! \brief Handle Set Multipoint State event from Seeker. Users on Handset may turn on/off multipoint functionality
 */
static sass_message_rsp sass_HandleSetMultipointStateEvent(uint8 multipoint_switch_event, uint8 remote_device_id)
{
    DEBUG_LOG("sass_HandleSetMultipointStateEvent: Received 0x%x from remote device: %d", multipoint_switch_event, remote_device_id);

    sass_message_rsp msg_response = sass_ack_message;

    if(multipoint_switch_event == SASS_SWITCH_OFF_MULTIPOINT)
    {
        DEBUG_LOG("sass_HandleSetMultipointStateEvent: Switch off multipoint");
        Ui_InjectUiInput(ui_input_disable_multipoint);
    }
    else if(multipoint_switch_event == SASS_SWITCH_ON_MULTIPOINT)
    {
        DEBUG_LOG("sass_HandleSetMultipointStateEvent: Switch on multipoint");
        Ui_InjectUiInput(ui_input_enable_multipoint);
    }
    else
    {
        DEBUG_LOG("sass_HandleSetMultipointStateEvent: Unkown multipoint switch event message received");
        msg_response = sass_nak_message;
    }

    return msg_response;
}

bdaddr* Sass_GetSwitchToDeviceBdAddr(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    return &sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr;
}

bool Sass_ResumePlayingOnSwitchToDevice(void)
{
     sassTaskData *sass_task_ptr = sass_GetTaskData();

     return sass_task_ptr->switch_active_audio_source_event.resume_playing_on_switch_to_device;
}

void Sass_ResetResumePlayingFlag(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_task_ptr->switch_active_audio_source_event.resume_playing_on_switch_to_device = FALSE;
}

void Sass_ResetSwitchActiveAudioSourceStates(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    DEBUG_LOG("Sass_ResetSwitchActiveAudioSourceStates");

    Sass_ResetSwitchToDeviceBdAddr();
    Sass_ResetSwitchAwayDeviceBdAddr();
    sass_task_ptr->switch_active_audio_source_event.is_switch_active_audio_src_pending = FALSE;
}

void Sass_ResetSwitchToDeviceBdAddr(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    BdaddrSetZero(&sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr);
}

void Sass_ResetSwitchAwayDeviceBdAddr(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    BdaddrSetZero(&sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr);
}

void Sass_NotifyMultipointSwitchEvent(uint8 switch_event_reason)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    uint8 num_connected_handsets;
    uint8 num_connected_sass_seekers = 0;
    device_t *devices = NULL;

    DEBUG_LOG("Sass_NotifyMultipointSwitchEvent : Switch Event Reason = %d", switch_event_reason);

    num_connected_handsets = BtDevice_GetConnectedBredrHandsets(&devices);
    for(uint8 count = 0; count < num_connected_handsets; count++)
    {
        if(Sass_IsAccountKeyAssociatedWithDevice(*(devices + count)))
        {
            num_connected_sass_seekers++;
        }
    }

    /* Notify MultiPoint SASS Switch Event to all the connected SASS seekers */
    for(uint8 count = 0; count < num_connected_sass_seekers; count++)
    {
        bdaddr device_bd_addr = DeviceProperties_GetBdAddr(*(devices + count));
        uint8 remote_device_id = FastPair_MsgStreamGetRemoteDeviceId(&device_bd_addr);

        if(sass_IsValidRemoteDevice(remote_device_id))
        {
            uint8 pname[FAST_PAIR_PNAME_DATA_LEN];
            uint8 pname_len;
            uint8 sass_notify_mp_switch_data_len;
            device_t current_active_device = BtDevice_GetDeviceForBdAddr(&sass_task_ptr->current_active_handset_addr);

            if((TRUE == fastPair_GetPName(pname,&pname_len)) && (Sass_IsAccountKeyAssociatedWithDevice(current_active_device)))
            {
                /* If Personalized name length is more than 64 octets, just restrict it to 64 octets */
                if(pname_len > FAST_PAIR_PNAME_DATA_LEN)
                {
                    pname_len = FAST_PAIR_PNAME_DATA_LEN;
                }
                sass_notify_mp_switch_data_len = SASS_NOTIFY_MP_SWITCH_REASON_FLAG_LEN + SASS_NOTIFY_MP_SWITCH_TARGET_DEVICE_FLAG_LEN + pname_len;
            }
            else
            {
                sass_notify_mp_switch_data_len = SASS_NOTIFY_MP_SWITCH_EVENT_ADD_DATA_LEN_DEFAULT;
            }

            uint8 sass_notify_mp_switch_data[sass_notify_mp_switch_data_len];
            if(sass_notify_mp_switch_data_len != SASS_NOTIFY_MP_SWITCH_EVENT_ADD_DATA_LEN_DEFAULT)
            {
                /* PName stored at provider is in little endian format, copy it in big endian format */
                for(uint8 i=0; i < pname_len; i++)
                {
                    sass_notify_mp_switch_data[SASS_NOTIFY_MP_SWITCH_REASON_FLAG_LEN + SASS_NOTIFY_MP_SWITCH_TARGET_DEVICE_FLAG_LEN + i] = pname[pname_len - i -1];
                }
            }
            else
            {
                /* Copy last 2 bytes of the target device BD address in case of SASS to non-SASS switch event */
                sass_notify_mp_switch_data[2] = (uint8)(sass_task_ptr->current_active_handset_addr.lap >> 8) & 0xFF;
                sass_notify_mp_switch_data[3] = (uint8)(sass_task_ptr->current_active_handset_addr.lap) & 0xFF;
            }

            sass_notify_mp_switch_data[0] = switch_event_reason;

            /* If current active device bd address matches with this device, then set the target device flag to this device, else another device */
            if(BdaddrIsSame(&sass_task_ptr->current_active_handset_addr, &device_bd_addr))
            {
                sass_notify_mp_switch_data[1] = SASS_NOTIFY_MP_SWITCH_EVENT_TO_THIS_DEVICE;
            }
            else
            {
                sass_notify_mp_switch_data[1] = SASS_NOTIFY_MP_SWITCH_EVENT_TO_ANOTHER_DEVICE;
            }

            /* Notify multipoint switch event to the seeker */
            fastPair_MsgStreamSendData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                                       SASS_NOTIFY_MP_SWITCH_EVENT,
                                       sass_notify_mp_switch_data,
                                       sass_notify_mp_switch_data_len,
                                       remote_device_id);
        }
    }
    free(devices);
}

static sass_message_rsp sass_HandleSwitchActiveAudioSource(uint8 switch_active_audio_source_event, uint8 remote_device_id)
{
    DEBUG_LOG("sass_HandleSwitchActiveAudioSource: Received 0x%x from remote device id: %d",
              switch_active_audio_source_event, remote_device_id);

    sass_message_rsp msg_response = sass_ack_message;
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    device_t device = NULL;
    uint8 other_remote_device_id = (remote_device_id == 1) ? 2 : 1;

    /* Switch to device associated with remote device id */
    if(switch_active_audio_source_event & SASS_SWITCH_TO_THIS_DEVICE)
    {
        sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr = fastPair_MsgStreamGetDeviceAddr(remote_device_id);
        sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr = fastPair_MsgStreamGetDeviceAddr(other_remote_device_id);
    }
    /* Switch to other connected device */
    else
    {
        sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr = fastPair_MsgStreamGetDeviceAddr(other_remote_device_id);
        sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr = fastPair_MsgStreamGetDeviceAddr(remote_device_id);
    }

    device = BtDevice_GetDeviceForBdAddr(&sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr);


    /* Switch to device is a non-SASS seeker, just process the disconnection request from the current device */
    if (device == NULL && (switch_active_audio_source_event & SASS_DISCONNECT_BLUETOOTH) == SASS_DISCONNECT_BLUETOOTH)
    {
        if(!BdaddrIsZero(&sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr))
        {
            HandsetService_DisconnectRequest(&sass_task_ptr->task, &sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr, 0);
            Sass_ResetSwitchActiveAudioSourceStates();

            return msg_response;
        }
    }

    /* Send NAK if device pointer to switch to device is null or switch active audio source request is for active device or 
       switch active audio src event is reject SCO on switched away device and disconnect BT is not set */
    if((device == NULL) || Focus_GetFocusForDevice(device) == focus_foreground || 
       ((switch_active_audio_source_event & SASS_REJECT_SCO) == SASS_REJECT_SCO &&
        (switch_active_audio_source_event & SASS_DISCONNECT_BLUETOOTH) != SASS_DISCONNECT_BLUETOOTH))
    {
        msg_response = sass_nak_message;
        /* Reset switch active audio source event states */
        Sass_ResetSwitchActiveAudioSourceStates();
        return msg_response;
    }
    /* Perform switch active audio source event*/
    else
    {
        sass_task_ptr->switch_active_audio_source_event.is_switch_active_audio_src_pending = TRUE;
        FocusSelect_EnableA2dpBargeInForSASS(TRUE);
        Ui_InjectUiInput(ui_input_switch_active_audio_source);

        /* Update the active handset upon receiving switch active audio source event
           when none of the connected handsets streaming audio */
        if(SASS_GET_CONNECTION_STATE(sass_task_ptr) == SASS_CONNECTED_NO_DATA)
        {
            sass_UpdateActiveHandset(device, SASS_NOTIFY_MP_SWITCH_EVENT_REASON_UNSPECIFIED);
        }
    }

    /* After switching active audio source via Switch Active Audio Source SASS message, the newly active audio source device shall 
       show 'Switch Back to last device' notification to the user to switch back to previous active audio source device. 
       Upon tapping that notification, Seeker would again send Switch Active Audio Source SASS message to switch back to to the
       previous active audio source device. */
    if(switch_active_audio_source_event & SASS_RESUME_PLAY_ON_SWITCH_TO_DEVICE)
    {
        sass_task_ptr->switch_active_audio_source_event.resume_playing_on_switch_to_device = TRUE;
    }
    else
    {
        sass_task_ptr->switch_active_audio_source_event.resume_playing_on_switch_to_device = FALSE;
    }

    /* Disconnect Bluetooth on switched away device */
    if(switch_active_audio_source_event & SASS_DISCONNECT_BLUETOOTH)
    {
        if(!BdaddrIsZero(&sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr))
        {
            HandsetService_DisconnectRequest(&sass_task_ptr->task, &sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr, 0);
        }
        else
        {
            DEBUG_LOG("sass_HandleSwitchActiveAudioSource : Not a valid device address.");
        }
    }

    return msg_response;
}

static void sass_AesCtrCfm(CL_CRYPTO_ENCRYPT_DECRYPT_AES_CTR_CFM_T *cfm)
{
    DEBUG_LOG("sass_AesCtrCfm");
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    if(cfm->status == success)
    {
        uint8 enc_data_sz_bytes = 0;
        uint8 *enc_conn_status = NULL;

        enc_data_sz_bytes = cfm->data_len * 2;

        enc_conn_status = PanicUnlessMalloc(enc_data_sz_bytes);

        /* Connection status raw data is appended with 0x00 and converted to little endian format for AES-CTR.
           When AES-CTR cfm is handled, convert the connection status raw data from little endian format to big
           endian format and exclude the last byte as it is not needed to be sent to Seeker. */
        fastPair_ConvertEndiannessFormat((uint8 *)cfm->data, enc_data_sz_bytes, enc_conn_status);

        DEBUG_LOG_DEBUG("sass_AesCtrCfm: Encrypted connection status ");
        for(uint8 count = 0; count < enc_data_sz_bytes; count++)
        {
            DEBUG_LOG_DEBUG("%02x", enc_conn_status[count]);
        }

        sass_SendNotifyConnectionStatus(enc_conn_status, enc_data_sz_bytes-1);

        free(enc_conn_status);
    }
    else
    {
        DEBUG_LOG("sass_AesCtrCfm: Requested operation has failed");

        free(sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status);
        sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status = NULL;
        sass_task_ptr->conn_status_enc_pending.remote_device_id = 0xFF;
    }
}

/*! \brief Verify MAC and if MAC is correct, Provider shall follow the instruction of SASS message.
 */
static bool sass_VerifyMac(const uint8 *mac, const uint8 *sass_add_data, 
                               uint16 sass_data_len, const uint8* msg_nonce, uint8 remote_device_id)
{
    uint8* session_nonce = NULL;
    uint8* in_use_account_key = NULL;
    uint8* hmac_sha256_out = NULL;
    uint8* nonce_data = NULL;
    bool   is_mac_valid = FALSE;
    bdaddr device_addr = {0};

    DEBUG_LOG("sass_VerifyMac");

    /* Get session nonce of current connection */
    session_nonce = FastPair_MsgStream_GetSessionNonce(remote_device_id);

    if(session_nonce != NULL)
    {
        /* Get In Use Account Key of the current connection */
        device_addr = fastPair_MsgStreamGetDeviceAddr(remote_device_id);

        in_use_account_key = UserAccounts_GetAccountKeyWithHandset(SASS_INUSE_ACCOUNT_KEY_LEN,&device_addr);

        if(in_use_account_key != NULL)
        {
            /* Nonce data is session nonce concatenated with mac*/
            nonce_data = (uint8*)PanicUnlessMalloc(SASS_NONCE_LEN);
            hmac_sha256_out = (uint8*)PanicUnlessMalloc(SHA256_DIGEST_SIZE);

            /* Concatenate session nonce and message nonce */
            memcpy(nonce_data, session_nonce, SASS_SESSION_NONCE_LEN);
            memcpy(nonce_data + SASS_SESSION_NONCE_LEN, msg_nonce, SASS_MSG_NOUNCE_LEN);

            hmac_sha256((uint8*)sass_add_data, sass_data_len, hmac_sha256_out, in_use_account_key,
                        nonce_data, SASS_NONCE_LEN);

            /* Verify incoming mac with first 8 bytes of hmac_sha256_out. */
            if(memcmp(mac, hmac_sha256_out, SASS_MAC_LEN)!=0)
            {
                DEBUG_LOG_ERROR("sass_VerifyMac: HMAC sha256 of decoded data does not match with the one in input data.");
                is_mac_valid = FALSE;
            }
            else
            {
                DEBUG_LOG("sass_VerifyMac: Hmac sha256 output is matching with input data");
                is_mac_valid = TRUE;
            }

            free(in_use_account_key);
            free(hmac_sha256_out);
            free(nonce_data);
        }
    }

    return is_mac_valid;
}

static void sass_AudioConnectMessage(audio_source_t source)
{
    avInstanceTaskData *av_instance = Av_GetInstanceForHandsetSource(source);
    device_t handset_device = Av_GetDeviceForInstance(av_instance);
    audio_status_t audio_status = audio_status_disconnected;

    if(handset_device == NULL)
    {
        DEBUG_LOG("sass_AudioConnectMessage: handset device handle is NULL");
        return;
    }
    DEBUG_LOG("sass_AudioConnectMessage source=%d", source);
    audio_status = DeviceProperties_GetAudioStatus(handset_device);

    /* Update the audio status to connected if it's not already connected */
    if(audio_status != audio_status_connected)
    {
        DeviceProperties_SetAudioStatus(handset_device, audio_status_connected);
    }
}

static void sass_AudioDisconnectMessage(audio_source_t source)
{
    DEBUG_LOG("sass_AudioDisconnectMessage source=%d", source);

    avInstanceTaskData *av_instance = Av_GetInstanceForHandsetSource(source);
    device_t handset_device = Av_GetDeviceForInstance(av_instance);

    uint8 is_disconnected_for_sass_barge_in = TRUE;
    device_t disconnected_device_for_sass_barge_in = DeviceList_GetFirstDeviceWithPropertyValue(device_property_disconnected_for_sass_barge_in,
            &is_disconnected_for_sass_barge_in, sizeof(uint8));

    /* Check if Audio Disconnection is due to SASS Switching */
    if(disconnected_device_for_sass_barge_in == handset_device)
    {
        audio_status_t audio_status = DeviceProperties_GetAudioStatus(handset_device);

        /* If the current audio status is playing, set it as interrupted 
            so that the audio can be resumed upon switch back. */
        if(audio_status == audio_status_playing)
        {
            DeviceProperties_SetAudioStatus(handset_device, audio_status_interrupted);
        }
    }
    else
    {
        /* Audio Disconnection is not due to SASS Switching */
        DeviceProperties_SetAudioStatus(handset_device, audio_status_disconnected);
    }

}

static void sass_AvrcpPlayStatusPlayingMessage(avInstanceTaskData *av_instance)
{
    DEBUG_LOG("sass_AvrcpPlayStatusPlayingMessage");
    device_t handset_device = Av_GetDeviceForInstance(av_instance);
    DeviceProperties_SetAudioStatus(handset_device, audio_status_playing);
}

static void sass_AvrcpPlayStatusNotPlayingMessage(avInstanceTaskData *av_instance)
{
    DEBUG_LOG("sass_AvrcpPlayStatusNotPlayingMessage");
    device_t handset_device = Av_GetDeviceForInstance(av_instance);
    DeviceProperties_SetAudioStatus(handset_device, audio_status_paused);
}

static void sass_HandleHandsetDisconnectCfm(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    DEBUG_LOG("sass_HandleHandsetDisconnectCfm : status = %d", cfm->status);

    if(sass_task_ptr->switch_back_is_initiated)
    {
        /*
        Device which initiated switch back is now disconnected.
        Connect the previous disconnected device.
        */
        uint8 is_disconnected_for_sass_barge_in = TRUE;
        device_t disconnected_device_for_sass_barge_in = DeviceList_GetFirstDeviceWithPropertyValue(device_property_disconnected_for_sass_barge_in,
                                                                                           &is_disconnected_for_sass_barge_in, sizeof(uint8));

        /* Reset switch back initiated flag now */
        sass_task_ptr->switch_back_is_initiated = FALSE;

        /* Check if the device is valid before doing any further operation*/
        if(BtDevice_DeviceIsValid(disconnected_device_for_sass_barge_in))
        {
            bdaddr most_recently_disconnected_bd_addr = DeviceProperties_GetBdAddr(disconnected_device_for_sass_barge_in);
            uint32 profiles_to_connect = BtDevice_GetSupportedProfilesNotConnected(disconnected_device_for_sass_barge_in);

            HandsetService_ConnectAddressRequest(&sass_task_ptr->task, &most_recently_disconnected_bd_addr, profiles_to_connect);
        }
        /* This is the case when we have not done any SASS switching so far */
        else
        {
            DEBUG_LOG("sass_HandleHandsetDisconnectCfm: Not a valid device to switch back to");
        }
    }

    /* As part of switch active audio source procedure, remote device asked us to disconnect link 
     * if media need not be resumed on other handset, then the procedure is complete at this point.
     */
    if (sass_task_ptr->switch_active_audio_source_event.is_switch_active_audio_src_pending && 
        !sass_task_ptr->switch_active_audio_source_event.resume_playing_on_switch_to_device)
    {
        /* check address of disconnected device */
        if (BdaddrIsSame(&sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr, &cfm->addr))
        {
            Sass_ResetSwitchActiveAudioSourceStates();
            FocusSelect_EnableA2dpBargeInForSASS(FALSE);
        }
    }
}

static void sass_HandleHandsetDisconnectedInd(bdaddr *addr)
{
    /* If the disconnected indication is for the active handset, then reset the current active handset BD addr */
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    if(BdaddrIsSame(&sass_task_ptr->current_active_handset_addr, addr))
    {
        DEBUG_LOG("sass_HandleHandsetDisconnectedInd : Disconnected Indication is for the active handset");
        BdaddrSetZero(&sass_task_ptr->current_active_handset_addr);
    }

    if(BdaddrIsSame(&sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr, addr))
    {
        Sass_ResetSwitchActiveAudioSourceStates();
        FocusSelect_EnableA2dpBargeInForSASS(FALSE);
    }

    if(BdaddrIsSame(&fast_pair_adv_sass.key_data.device_addr, addr))
    {
        fast_pair_adv_sass.key_data.in_use_account_key_handset_connected_and_active = FALSE;

        /* Custom data is not longer valid */
        Sass_SetCustomData(0);
    }
}

static bool sass_HandleConnectedProfileInd(const CONNECTED_PROFILE_IND_T *ind)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    bool context_update = FALSE;

    DEBUG_LOG("sass_HandleConnectedProfileInd Profile = %d device_ptr is 0x%x", ind->profile, ind->device);

    /* Until all br/edr profiles are connected, Context Framework will not indicate SASS about the connected handset info. 
       Hence SASS could update the connected handset info when there is a br/edr profile connect indication.
    */
    context_update = sass_ContextHandleConnectedHandsetsInfo();

    if(ind->profile == DEVICE_PROFILE_AVRCP)
    {
        device_t device = ind->device;
        bdaddr handset_addr = DeviceProperties_GetBdAddr(device);

        /* If this device got connected due to SASS switch back message, we might have to resume 
         * the media.
         */
        if(sass_task_ptr->switch_back_audio_resume == TRUE)
        {
            audio_status_t audio_status = DeviceProperties_GetAudioStatus(device);

            if(audio_status != audio_status_disconnected && audio_status != audio_status_playing)
            {
               DEBUG_LOG("sass_HandleConnectedProfileInd resume media on this device");

               /* Reset the audio resume flag now */
               sass_task_ptr->switch_back_audio_resume = FALSE;

               /* Request the handset to play the media */
               Ui_InjectUiInput(ui_input_play);
            }
        }

        /* Update SASS state from streaming to streaming with AVRCP state in case 
         * streaming has already started and AVCRP profile connected with some delay.
         */
        if( SASS_GET_CONNECTION_STATE(sass_task_ptr) == SASS_A2DP_STREAMING_ONLY &&
            BdaddrIsSame(&sass_task_ptr->current_active_handset_addr, &handset_addr))
        {
            SASS_SET_CONNECTION_STATE(sass_task_ptr, SASS_A2DP_STREAMING_WITH_AVRCP);

            return TRUE;
        }
    }

    return context_update;
}


#ifdef INCLUDE_DFU
/*! \brief Update connection status to Non-audio data transferring when firmware upgrade is started
    \param None
    \return None
*/
static void sass_SetDfuState(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    DEBUG_LOG("sass_SetDfuState");

    if(SASS_GET_CONNECTION_STATE(sass_task_ptr) != SASS_NON_AUDIO_DATA)
    {
        SASS_SET_CONNECTION_STATE(sass_task_ptr, SASS_NON_AUDIO_DATA);
        sass_EvaluateSendNotifyConnStatus();
        sass_conn_status_change_cb();
    }
}

/*! \brief Reset the connection status of SASS back to it's original connection state
    \param None
    \return None
*/
static void sass_ResetDfuState(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    DEBUG_LOG("sass_ResetDfuState");

    if(SASS_GET_CONNECTION_STATE(sass_task_ptr) == SASS_NON_AUDIO_DATA)
    {
        SASS_SET_CONNECTION_STATE(sass_task_ptr, SASS_NO_CONNECTION);
        sass_ContextHandleActiveSourceInfo();
        sass_ContextHandleConnectedHandsetsInfo();
        sass_EvaluateSendNotifyConnStatus();
        sass_conn_status_change_cb();
    }
}
#endif /* INCLUDE_DFU */

/*! \brief Handle messages from other components
 */
static void sass_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    bool context_update = FALSE;
    DEBUG_LOG("sass_HandleMessage message id, MSG:0x%x", id);

    switch(id)
    {
        case CL_CRYPTO_ENCRYPT_DECRYPT_AES_CTR_CFM:
            sass_AesCtrCfm((CL_CRYPTO_ENCRYPT_DECRYPT_AES_CTR_CFM_T *)message);
            break;

        case AV_A2DP_AUDIO_CONNECTED:
            sass_AudioConnectMessage(((AV_A2DP_AUDIO_CONNECT_MESSAGE_T *)message)->audio_source);
            break;

        case AV_A2DP_AUDIO_DISCONNECTED:
            sass_AudioDisconnectMessage(((AV_A2DP_AUDIO_DISCONNECT_MESSAGE_T *)message)->audio_source);
            break;

        case AV_AVRCP_PLAY_STATUS_PLAYING_IND:
            sass_AvrcpPlayStatusPlayingMessage(((AV_AVRCP_PLAY_STATUS_PLAYING_IND_T *)message)->av_instance);
            break;

        case AV_AVRCP_PLAY_STATUS_NOT_PLAYING_IND:
            sass_AvrcpPlayStatusNotPlayingMessage(((AV_AVRCP_PLAY_STATUS_NOT_PLAYING_IND_T *)message)->av_instance);
            break;

        case HANDSET_SERVICE_DISCONNECT_CFM:
            sass_HandleHandsetDisconnectCfm((const HANDSET_SERVICE_DISCONNECT_CFM_T *)message);
            break;

        case HANDSET_SERVICE_DISCONNECTED_IND:
        {
            const HANDSET_SERVICE_DISCONNECTED_IND_T *ind = (const HANDSET_SERVICE_DISCONNECTED_IND_T *)message;
            DEBUG_LOG("Handset Disconnected Ind: status = %d", ind->status);
            sass_HandleHandsetDisconnectedInd(&ind->addr);
            break;
        }

        case CONNECTED_PROFILE_IND:
            context_update = sass_HandleConnectedProfileInd((const CONNECTED_PROFILE_IND_T *) message);
            break;

        case ui_input_disable_sass_connection_switch:
            Sass_DisableConnectionSwitch();
            break;

        case ui_input_enable_sass_connection_switch:
            Sass_EnableConnectionSwitch();
            break;

        /* Context Handling */
        case context_physical_state:
            context_update = sass_ContextHandlePhyState();
            break;

        case context_active_source_info:
            context_update = sass_ContextHandleActiveSourceInfo();
            break;

        case context_connected_handsets_info:
            context_update = sass_ContextHandleConnectedHandsetsInfo();
            break;

#ifdef INCLUDE_DFU
        case DFU_PRE_START:
        case DFU_STARTED:
            sass_SetDfuState();
            break;
        case DFU_COMPLETED:
        case DFU_CLEANUP_ON_ABORT:
        case DFU_ABORTED:
            sass_ResetDfuState();
            break;
#endif /* INCLUDE_DFU */

        case CON_MANAGER_TP_DISCONNECT_IND:
        {
            CON_MANAGER_TP_DISCONNECT_IND_T *ind = (CON_MANAGER_TP_DISCONNECT_IND_T *)message;
            if(ind->tpaddr.transport == TRANSPORT_BREDR_ACL)
            {
                sass_HandleHandsetDisconnectedInd(&ind->tpaddr.taddr.addr);
                context_update = sass_ContextHandleConnectedHandsetsInfo();
            }
            break;
        }

         default:
            DEBUG_LOG("sass_HandleMessage. Unhandled MessageID = %d", id);
            break;
   }

   if(context_update)
   {
      sass_EvaluateSendNotifyConnStatus();
      sass_conn_status_change_cb();
   }
}

/*! \brief Handle incoming SASS message sent by Seeker.
 */
static void sass_HandleIncomingData(uint8 remote_device_id, const uint8* msg_data, uint16 len)
{
    uint16 additional_data_len = 0;
    uint16 additional_data_offset = 0;
    uint16 mac_offset = 0;
    uint16 msg_nounce_offset = 0;
    uint16 sass_data_len = 0;
    uint8  msg_code = 0;
    bdaddr device_addr = {0};
    sass_message_rsp sass_message_response = sass_nak_message;

    if ((msg_data == NULL) || (len < SASS_ADD_DATA_INDEX))
    {
        DEBUG_LOG_ERROR("sass_HandleIncomingData: UNEXPECTED ERROR - Length is %d is less than minimum of %d or data is NULL",
                        len,SASS_ADD_DATA_INDEX);
        return;
    }

    additional_data_len = (msg_data[SASS_ADD_DATA_LEN_UPPER_INDEX]<<8) + 
                           msg_data[SASS_ADD_DATA_LEN_LOWER_INDEX];

    DEBUG_LOG("sass_HandleIncomingData: Additional data len:%d, received data len:%d",
                         additional_data_len,
                         len);

    if((SASS_ADD_DATA_INDEX+additional_data_len) != len)
    {
        DEBUG_LOG_ERROR("sass_HandleIncomingData: UNEXPECTED length ERROR Length is %d. Should be %d",
                        len,(SASS_ADD_DATA_INDEX+additional_data_len));
        return;
    }

    if (!sass_IsValidRemoteDevice(remote_device_id))
    {
        return;
    }

    msg_code = msg_data[SASS_MESSAGE_CODE_INDEX];
    device_addr = fastPair_MsgStreamGetDeviceAddr(remote_device_id);
    DEBUG_LOG("sass_HandleIncomingData: Received SASS message code is 0x%x from handset [%04x,%02x,%06lx] with remote device id %d",
               msg_code,
               device_addr.nap,
               device_addr.uap,
               device_addr.lap,
               remote_device_id);

    /* If additional data is present verify MAC and send ACK message to Seeker if MAC is correct */
    if (additional_data_len > 0)
    {
        additional_data_offset = SASS_ADD_DATA_INDEX;
        mac_offset = len - SASS_MAC_LEN;
        msg_nounce_offset = mac_offset - SASS_MSG_NOUNCE_LEN;

        /* SASS additional data length excluding message nonce len and MAC len */
        sass_data_len = msg_nounce_offset - additional_data_offset;
        bool is_mac_valid = FALSE;

        DEBUG_LOG("sass_HandleIncomingData: additional_data_offset: 0x%04x, mac_offset: 0x%04x, msg_nounce_offset: 0x%04x sass_data_len: 0x%04x",
                  additional_data_offset, mac_offset, msg_nounce_offset, sass_data_len);

        is_mac_valid = sass_VerifyMac(&msg_data[mac_offset], &msg_data[additional_data_offset],
                                      sass_data_len, &msg_data[msg_nounce_offset], remote_device_id);

        /*If MAC is incorrect, send NAK with error reason as incorrect mac. Do not process the SASS message further */
        if(!is_mac_valid)
        {
            DEBUG_LOG("sass_HandleIncomingData: send NAK due to incorrect MAC");
            fastPair_MsgStreamSendNAK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                                      msg_data[SASS_MESSAGE_CODE_INDEX],
                                      FASTPAIR_MESSAGESTREAM_NAK_REASON_INCORRECT_MAC,
                                      remote_device_id);
            return;
        }
    }

    /* Take action based on Message Code present in SASS message */
    switch(msg_code)
    {
        case SASS_GET_CAPABILITY_EVENT:
        {
            sass_message_response = sass_NotifyCapabilityToSeeker(remote_device_id);
        }
        break;
        case SASS_NOTIFY_CAPABILITY_EVENT:
        {
            sass_message_response = sass_HandleNotifyCapabilityFromSeeker(remote_device_id);
        }
        break;
        case SASS_SET_MP_STATE_EVENT:
        {
            sass_message_response = sass_HandleSetMultipointStateEvent(msg_data[additional_data_offset], remote_device_id);
        }
        break;
        case SASS_SET_SWITCHING_PREFERENCE_EVENT:
        {
        }
        break;
        case SASS_GET_SWITCHING_PREFERENCE_EVENT:
        {
        }
        break;
        case SASS_NOTIFY_SWITCHING_PREFERENCE_EVENT:
        {
        }
        break;
        case SASS_SWITCH_AUDIO_SOURCE_EVENT:
        {
            sass_message_response = sass_HandleSwitchActiveAudioSource(msg_data[additional_data_offset], remote_device_id);
        }
        break;
        case SASS_SWITCH_BACK_EVENT:
        {
            sass_message_response = sass_HandleSwitchBackEvent(msg_data[additional_data_offset], remote_device_id);
        }
        break;
        case SASS_GET_CONNECTION_STATUS_EVENT:
        {
           sass_message_response = sass_HandleGetConnectionStatus(remote_device_id);
        }
        break;
        case SASS_NOTIFY_SASS_INITIATED_CONN_EVENT:
        {
            sass_message_response = sass_HandleNotifySassInitiatedConnection(msg_data[additional_data_offset], remote_device_id);
        }
        break;
        case SASS_INUSE_ACC_KEY_IND_EVENT:
        {
            sass_message_response = sass_HandleInUseAccountKeyMessage(&msg_data[additional_data_offset], remote_device_id, device_addr);
        }
        break;
        case SASS_SEND_CUSTOM_DATA_EVENT:
        {
            sass_message_response = sass_HandleCustomDataMessage(msg_data[additional_data_offset], remote_device_id);
        }
        break;
        case SASS_SET_DROP_CONN_TARGET_EVENT:
        {
        }
        break;
        default:
        break;
    }

    /* Send ACK for the SASS message if the incoming sass message contains right additional data */
    if(sass_message_response == sass_ack_message)
    {
        DEBUG_LOG("sass_HandleIncomingData: send ACK for the received message code:0x%x", msg_data[SASS_MESSAGE_CODE_INDEX]);
        /* Send ACK for the incoming SASS message if it has additional data and mac is correct. */
        fastPair_MsgStreamSendACKWithAdditionalData(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                                                    msg_data[SASS_MESSAGE_CODE_INDEX],
                                                    (uint8*)&msg_data[additional_data_offset],
                                                    sass_data_len,
                                                    remote_device_id);
    }
    /* Send NAK if the additional data in the sass message is incorrect */
    if(sass_message_response == sass_nak_message)
    {
        fastPair_MsgStreamSendNAK(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                                  msg_code,
                                  FASTPAIR_MESSAGESTREAM_NAK_REASON_NOT_SUPORTED,
                                  remote_device_id);
    }
}

/*! \brief Handle incoming SASS message sent by Seeker.
 */
static void sass_MessageHandler(FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE msg_type,uint8 remote_device_id,
                                      const uint8 *msg_data, uint16 len)
{
    switch(msg_type)
    {
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND:
            DEBUG_LOG("sass_MessageHandler: FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_CONNECT_IND");
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM:
            DEBUG_LOG("sass_MessageHandler: FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_SERVER_CONNECT_CFM");
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA:
            DEBUG_LOG("sass_MessageHandler: FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_INCOMING_DATA");
            sass_HandleIncomingData(remote_device_id ,msg_data, len); 
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND:
            DEBUG_LOG("sass_MessageHandler: FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND");
            break;
        case FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_CFM:
            DEBUG_LOG("sass_MessageHandler: FASTPAIR_MESSAGE_STREAM_MESSAGE_TYPE_DISCONNECT_IND");
            break;
        default:
            DEBUG_LOG("sass_MessageHandler: unknown message=%x", msg_type);
            break;
    }
}

static void sass_InitializeSassTaskData(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    device_t my_device = BtDevice_GetSelfDevice();
    uint8 sass_switch_disabled = 0;

    memset(&sass_task_ptr->mru_handset_bd_addr, 0x0, sizeof(bdaddr));
    memset(&sass_task_ptr->con_status, 0x0, sizeof(sass_connection_status_field_t));
    sass_task_ptr->con_status.custom_data = 0x0;
    sass_task_ptr->switch_back_audio_resume = FALSE;
    BdaddrSetZero(&sass_task_ptr->switch_active_audio_source_event.switch_to_device_bdaddr);
    BdaddrSetZero(&sass_task_ptr->switch_active_audio_source_event.switch_away_device_bdaddr);
    sass_task_ptr->switch_active_audio_source_event.resume_playing_on_switch_to_device = FALSE;
    sass_task_ptr->switch_active_audio_source_event.is_switch_active_audio_src_pending = FALSE;
    /* At the time of initialization the connection availability flag bit should be set as there is no handset connected */
    sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state | SASS_AD_CONNECTION_AVAILABILITY;

    if(my_device != NULL)
    {
        Device_GetPropertyU8(my_device, device_property_sass_switch_disabled, &sass_switch_disabled);
    }
    DEBUG_LOG("sass_switch_disabled: %d", sass_switch_disabled);
    if(sass_switch_disabled)
    {
        sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state | SASS_DISABLE_CONNECTION_SWITCH;
    }
    else
    {
        /* At the time of initialization the connection state should be NO_CONNECTION */
        sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & 0xF0) | SASS_NO_CONNECTION;
    }
    /* At the time of initialization, sass switch barge-in flag should be set to FALSE */
    sass_task_ptr->is_bargein_for_sass_switch = FALSE;

    for(unsigned i=0; i<SASS_MAX_CONNECTIONS; i++)
    {
        sass_task_ptr->notify_conn_status_pending[i] = FALSE;
    }
    sass_task_ptr->number_of_connected_seekers = 0;
    sass_task_ptr->conn_status_enc_pending.msg_nonce_for_notify_con_status = NULL;
    sass_task_ptr->conn_status_enc_pending.remote_device_id = 0xFF;

    /* At the time of initialization, set the current active handset BD addr Zero */
    BdaddrSetZero(&sass_task_ptr->current_active_handset_addr);
    /* At the time of initialization, switch back initiated flag should be set to FALSE */
    sass_task_ptr->switch_back_is_initiated = FALSE;
}

static bdaddr sass_GetIncomingBargeInDeviceBdAddr(void)
{
    bdaddr handset_bdaddr = {0};
    uint8 context = handset_context_barge_in;

    DEBUG_LOG("sass_GetIncomingBargeInDeviceBdAddr");
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_handset_context, &context, sizeof(uint8));

    if (device)
    {
        handset_bdaddr = DeviceProperties_GetBdAddr(device);
    }

    return handset_bdaddr;
}

/*! \brief Get the handset device for the disconnection
    \param device Pointer to the device to be used which will be filled up after execution of this function

    \return TRUE if the device is found, FALSE otherwise
 */
static bool sass_HandsetDeviceForDisconnection(device_t *device)
{
    bool device_found = FALSE;
    bool fallback_on_focus_select_to_find_device = FALSE;
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    uint8 *incoming_handset_account_key = NULL;
    uint8 *in_use_account_key = NULL;
    bool is_handset_connected_and_active = FALSE;
    bdaddr incoming_handset_bd_addr = sass_GetIncomingBargeInDeviceBdAddr();

    sass_task_ptr->is_bargein_for_sass_switch= FALSE;

    DEBUG_LOG("sass_HandsetDeviceForDisconnection");
    if(!BdaddrIsZero(&incoming_handset_bd_addr))
    {
        device_t handset_device = BtDevice_GetDeviceForBdAddr(&incoming_handset_bd_addr);
        incoming_handset_account_key = UserAccounts_GetAccountKeyWithHandset(SASS_ACCOUNT_KEY_LEN, &incoming_handset_bd_addr);

        /* Reset the handset context after getting the associated account key with the incoming barge-in handset */
        if(DeviceProperties_GetHandsetContext(handset_device) == handset_context_barge_in)
        {
            DeviceProperties_SetHandsetContext(handset_device, handset_context_none);
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    in_use_account_key = fastPair_SASSGetInUseAccountKey(&is_handset_connected_and_active);

    /* If account key associated with incoming handset bd address is matching with the account key associated with MRU handset, it's a SASS switching */
    if((incoming_handset_account_key != NULL) && (in_use_account_key != NULL) && memcmp(incoming_handset_account_key, in_use_account_key, SASS_ACCOUNT_KEY_LEN) == 0)
    {
        unsigned num_connected_handsets;
        device_t *devices = NULL;

        num_connected_handsets = BtDevice_GetConnectedBredrHandsets(&devices);

        if(num_connected_handsets == 1)
        {
            DEBUG_LOG("sass_HandsetDeviceForDisconnection. Single Point use-case");
            *device = devices[0];
            device_found = TRUE;
        }
        else
        {
            uint8 num_connected_sass_seekers = 0;

            DEBUG_LOG("sass_HandsetDeviceForDisconnection. Multi Point use-case");
            for(unsigned index = 0; index < num_connected_handsets; index++)
            {
                if(Sass_IsAccountKeyAssociatedWithDevice(*(devices + index)))
                {
                    num_connected_sass_seekers++;
                }
            }

            /* Both  are non-SASS Seekers */
            if(num_connected_sass_seekers == 0)
            {
                DEBUG_LOG("sass_HandsetDeviceForDisconnection. Both are non-SASS Seekers.");
                device_found = FALSE;
            }

            /* One is SASS Seeker, other is non-SASS Seeker */
            if(num_connected_sass_seekers == 1)
            {
                for(unsigned index = 0; index < num_connected_handsets; index++)
                {
                    if(Sass_IsAccountKeyAssociatedWithDevice(*(devices + index)))
                    {
                        DEBUG_LOG("sass_HandsetDeviceForDisconnection. Disconnect SASS Seeker");
                        *device = devices[index];
                        device_found = TRUE;
                    }
                }
            }

            /* Both are SASS Seekers */
            if(num_connected_sass_seekers == 2)
            {
                uint8 num_handsets_associated_with_in_use_acc_key = 0;

                for(unsigned seeker_index = 0; seeker_index < num_connected_sass_seekers; seeker_index++)
                {
                    uint8 *account_key_associated_with_handset = NULL;
                    bdaddr device_addr = DeviceProperties_GetBdAddr(*(devices + seeker_index));
                    account_key_associated_with_handset = UserAccounts_GetAccountKeyWithHandset(SASS_ACCOUNT_KEY_LEN, &device_addr);
                    if((account_key_associated_with_handset != NULL) && 
                    memcmp(account_key_associated_with_handset, in_use_account_key, SASS_ACCOUNT_KEY_LEN) == 0)
                    {
                        num_handsets_associated_with_in_use_acc_key++;
                    }

                    if(account_key_associated_with_handset != NULL)
                    {
                        free(account_key_associated_with_handset);
                    }
                }

                if(num_handsets_associated_with_in_use_acc_key == 1)
                {
                    for(unsigned seeker_index = 0; seeker_index < num_connected_sass_seekers; seeker_index++)
                    {
                        bdaddr device_addr = DeviceProperties_GetBdAddr(*(devices + seeker_index));
                        if(Sass_GetHandsetAddressAssociatedWithInUseAccountKey(&device_addr))
                        {
                            DEBUG_LOG("sass_HandsetDeviceForDisconnection. Disconnect SASS Seeker which indicates the in-use account key.");
                            *device = devices[seeker_index];
                            device_found = TRUE;
                        }
                        break;
                    }
                }
                else
                {
                    DEBUG_LOG("sass_HandsetDeviceForDisconnection. Disconnect LRU SASS Seeker. Fallback on focus_select to find a device for disconnection.");
                    /* It's a SASS switch only, but fallback on the existing implementation of focus select to get a device for disconnection */
                    fallback_on_focus_select_to_find_device = TRUE;
                    device_found = FALSE;
                }
            }
        }

        free(devices);
    }

    if(incoming_handset_account_key != NULL)
    {
        free(incoming_handset_account_key);
    }

    /* Set the sass switch flag after finding a handset device for disconnection */
    if(device_found || fallback_on_focus_select_to_find_device)
    {
        sass_task_ptr->is_bargein_for_sass_switch= TRUE;
    }
    return device_found;
}

/*! \brief Get the device to use when handling a UI input

    \param ui_input - The ui input to find a device for (Currently SASS is only interested in ui_input_disconnect_lru_handset).
    \param device - Pointer to the device to use
    \return TRUE if a device was found, otherwise FALSE
*/
static bool sass_GetDeviceForUiInput(ui_input_t ui_input, device_t * device)
{
    bool device_found = FALSE;
    switch (ui_input)
    {
        case ui_input_disconnect_lru_handset:
            DEBUG_LOG("sass_GetDeviceForUiInput enum:ui_input_t:%d", ui_input);
            device_found = sass_HandsetDeviceForDisconnection(device);
            break;

        default:
            DEBUG_LOG("sass_GetDeviceForUiInput enum:ui_input_t:%d not supported", ui_input);
            break;
    }
    return device_found;
}

bool Sass_GetHandsetAddressAssociatedWithInUseAccountKey(bdaddr *bd_addr)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    memcpy(bd_addr, &sass_task_ptr->mru_handset_bd_addr, sizeof(bdaddr));

    if(!BdaddrIsZero(bd_addr))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

bool Sass_IsAccountKeyAssociatedWithDevice(device_t device)
{
    bdaddr device_bd_addr = DeviceProperties_GetBdAddr(device);
    bool status = FALSE;
    uint8 *account_key = NULL;

    account_key = UserAccounts_GetAccountKeyWithHandset(SASS_ACCOUNT_KEY_LEN, &device_bd_addr);

    /* Assign the status as TRUE if the account key is valid */
    if(account_key != NULL)
    {
        status = TRUE;
        free(account_key);
    }
    return status;
}

bool Sass_IsConnectedDeviceForSassSwitch(device_t device)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    bool status = FALSE;

    if(device != NULL && 
       SASS_GET_CONNECTION_STATE(sass_task_ptr) != SASS_DISABLE_CONNECTION_SWITCH && 
       Sass_IsAccountKeyAssociatedWithDevice(device))
    {
        DEBUG_LOG("Sass_IsConnectedDeviceForSassSwitch. Connected device for SASS switching");
        status = TRUE;
    }
    return status;
}

void Sass_SetConnectionStateExceptOnHeadFlag(uint8 connection_state)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_task_ptr->con_status.connection_state = (sass_task_ptr->con_status.connection_state & SASS_AD_ON_HEAD_DETECTION);
    sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state | (connection_state & ~(SASS_AD_ON_HEAD_DETECTION));
}

uint8 Sass_GetConnectionState(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    return sass_task_ptr->con_status.connection_state;
}

void Sass_SetCustomData(uint8 custom_data)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_task_ptr->con_status.custom_data = custom_data;
}

uint8 Sass_GetCustomData(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    return sass_task_ptr->con_status.custom_data;
}

void Sass_SetConnectedDeviceBitMap(uint8 connected_devices_bitmap)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_task_ptr->con_status.connected_devices_bitmap = connected_devices_bitmap;
}

uint8 Sass_GetConnectedDeviceBitMap(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    return sass_task_ptr->con_status.connected_devices_bitmap;
}

void Sass_RegisterForConnectionStatusChange(sass_connection_status_change_callback callBack)
{
    sass_conn_status_change_cb = callBack;
}

bool Sass_IsBargeInForSassSwitch(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    return sass_task_ptr->is_bargein_for_sass_switch;
}

void Sass_DisableConnectionSwitch(void)
{
    device_t my_device = BtDevice_GetSelfDevice();
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    DEBUG_LOG("Sass_DisableConnectionSwitch");

    if(my_device == NULL)
    {
        DEBUG_LOG_ERROR("Sass_DisableConnectionSwitch : Self device is NULL");
        return;
    }

    sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state | SASS_DISABLE_CONNECTION_SWITCH;
    /* Set connection switch state to disabled */
    Device_SetPropertyU8(my_device, device_property_sass_switch_disabled, TRUE);
    DeviceDbSerialiser_Serialise();
    sass_EvaluateSendNotifyConnStatus();
    sass_conn_status_change_cb();
}

void Sass_EnableConnectionSwitch(void)
{
    device_t my_device = BtDevice_GetSelfDevice();
    sassTaskData *sass_task_ptr = sass_GetTaskData();
    DEBUG_LOG("Sass_EnableConnectionSwitch");

    if(my_device == NULL)
    {
        DEBUG_LOG_ERROR("Sass_EnableConnectionSwitch : Self device is NULL");
        return;
    }

    sass_task_ptr->con_status.connection_state = sass_task_ptr->con_status.connection_state & ~SASS_DISABLE_CONNECTION_SWITCH;
    sass_ContextHandleActiveSourceInfo();
    sass_ContextHandleConnectedHandsetsInfo();
    /* Set connection switch state to enabled */
    Device_SetPropertyU8(my_device, device_property_sass_switch_disabled, FALSE);
    DeviceDbSerialiser_Serialise();
    sass_EvaluateSendNotifyConnStatus();
    sass_conn_status_change_cb();
}

bool Sass_IsConnectionSwitchDisabled(void)
{
    sassTaskData *sass_task_ptr = sass_GetTaskData();

    if ((sass_task_ptr->con_status.connection_state & SASS_DISABLE_CONNECTION_SWITCH) == SASS_DISABLE_CONNECTION_SWITCH)
    {
         return TRUE;
    }

    return FALSE;
}

void Sass_Init(void)
{
    DEBUG_LOG("Sass_Init");

    /* Register SASS message handler callback with Fast Pair Message Stream */
    fastPair_MsgStreamRegisterGroupMessages(FASTPAIR_MESSAGESTREAM_MESSAGE_GROUP_SASS_EVENT,
                                            sass_MessageHandler);

    sassTaskData *sass_task_ptr = sass_GetTaskData();
    sass_task_ptr->task.handler = sass_HandleMessage;

    sass_InitializeSassTaskData();

    /* Register with Context Framework to get context updates */
    ContextFramework_RegisterContextConsumer(context_physical_state, &sass_task_ptr->task);
    ContextFramework_RegisterContextConsumer(context_connected_handsets_info, &sass_task_ptr->task);
    ContextFramework_RegisterContextConsumer(context_active_source_info, &sass_task_ptr->task);

    /* Register with AV to receive AV status messages */
    appAvStatusClientRegister(&sass_task_ptr->task);
    /* Register with handset service to receive the handset service messages */
    HandsetService_ClientRegister(&sass_task_ptr->task);
    /* Register with profile manager to get the profile level  messages */
    ProfileManager_ClientRegister(&sass_task_ptr->task);
    /* Configure the device with SASS callback to receive the ui input events */
    Focus_ConfigureDeviceOverride(&override_fns_for_device);
    /* Register with UI component to get sass related UI inputs */
    Ui_RegisterUiInputConsumer(sass_GetTask(), sass_ui_inputs, ARRAY_DIM(sass_ui_inputs));
#ifdef INCLUDE_DFU
    Dfu_ClientRegister(sass_GetTask());
#endif
    ConManagerRegisterTpConnectionsObserver(cm_transport_bredr, sass_GetTask());
}
