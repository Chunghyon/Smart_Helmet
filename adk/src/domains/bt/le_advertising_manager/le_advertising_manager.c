/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Management of Bluetooth Low Energy advertising
*/

#include "le_advertising_manager.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_advertising_item_database.h"
#include "le_advertising_manager_aggregator.h"
#include "le_advertising_manager_set_sm.h"
#include "le_advertising_manager_private.h"

#include <message.h>
#include <panic.h>
#include <logging.h>

#define MSEC_TO_LE_TIMESLOT(x)  ((x)*1000/625)

#define DEFAULT_ADV_EVENT_PROPERTIES 0x13
#define DEFAULT_ADV_INTERVAL_MIN MSEC_TO_LE_TIMESLOT(100)
#define DEFAULT_ADV_INTERVAL_MAX MSEC_TO_LE_TIMESLOT(120)
#define DEFAULT_PRIMARY_ADV_CHANNEL_MAP 0x7
#define DEFAULT_OWN_ADDR_TYPE 0x1
#define DEFAULT_PEER_TPADDR (typed_bdaddr){0xFF, { 0x00000000, 0x00, 0x0000 } }
#define DEFAULT_ADV_FILTER_POLICY 0x0
#define DEFAULT_PRIMARY_ADV_PHY 0x1
#define DEFAULT_SECONDARY_ADV_MAX_SKIP 0x0
#define DEFAULT_SECONDARY_ADV_PHY 0x1
#define DEFAULT_SID 0x100

static bool allowed =  FALSE;
static bool connectable =  FALSE;
static Task allow_task = NULL;

bool LeAdvertisingManager_IsAdvertisingAllowed(void)
{
    return allowed;
}

bool LeAdvertisingManager_IsConnectableAdvertisingEnabled(void)
{
    return connectable;
}

bool LeAdvertisingManager_Init(Task init_task)
{
    DEBUG_LOG_VERBOSE("LEAM Init");

    UNUSED(init_task);

    allowed = FALSE;
    connectable = FALSE;
    allow_task = NULL;

    LeAdvertisingManager_InitAdvertisingItemDatabase();
    LeAdvertisingManager_SetSmInit();
    LeAdvertisingManager_InitAggregator();

    return TRUE;
}

bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable)
{
    DEBUG_LOG_VERBOSE("LEAM EnableConnectableAdvertising [%d]", enable);

    UNUSED(task);

    connectable = enable;

    le_adv_refresh_control_t control = { .advertising_state_update_callback = NULL };
    LeAdvertisingManager_QueueAdvertisingStateUpdate(&control);
    return TRUE;
}

static void leAdvertisingManager_AllowAdvertisingSendCfmMessage(bool allow)
{
    if(allow_task)
    {
        MESSAGE_MAKE(msg, LE_ADV_MGR_ALLOW_ADVERTISING_CFM_T);
        msg->allow = allow;
        msg->status = le_adv_mgr_status_success;
        MessageSend(allow_task, LE_ADV_MGR_ALLOW_ADVERTISING_CFM, msg);
    }
}

static void leAdvertisingManager_AllowAdvertisingRefreshCallback(void)
{
    DEBUG_LOG_VERBOSE("LEAM Refresh Callback Existing State [%d]", allowed);

    leAdvertisingManager_AllowAdvertisingSendCfmMessage(allowed);
}

bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow)
{
    DEBUG_LOG_VERBOSE("LEAM AllowAdvertising Task [0x%x] Allow [%d]", task, allow);

    allow_task = task;

    if (allowed != allow)
    {
        DEBUG_LOG_VERBOSE("LEAM AllowAdvertising Existing State [%d]", allowed);

        allowed = allow;

        le_adv_refresh_control_t control = { .advertising_state_update_callback = leAdvertisingManager_AllowAdvertisingRefreshCallback };
        LeAdvertisingManager_QueueAdvertisingStateUpdate(&control);
    }
    else
    {
        leAdvertisingManager_AllowAdvertisingSendCfmMessage(allow);
    }

    return TRUE;
}

bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params)
{
    UNUSED(params);
    return TRUE;
}

bool LeAdvertisingManager_ParametersSelect(uint8 index)
{
    UNUSED(index);
    return TRUE;
}

bool LeAdvertisingManager_UpdateAdvertisingItem(const le_adv_item_handle handle)
{
    DEBUG_LOG_VERBOSE("LEAM UpdateAdvertisingItem with Handle [0x%x]", handle);
    LeAdvertisingManager_QueueClientDataUpdate(handle);

    return TRUE;
}

bool LeAdvertisingManager_PopulateDefaultAdvertisingParams(le_adv_item_params_t * params_to_populate)
{
    PanicNull(params_to_populate);
    *params_to_populate = (le_adv_item_params_t)
    {
        .primary_adv_interval_min = DEFAULT_ADV_INTERVAL_MIN,
        .primary_adv_interval_max = DEFAULT_ADV_INTERVAL_MAX,
        . primary_adv_channel_map = DEFAULT_PRIMARY_ADV_CHANNEL_MAP,
        .own_addr_type = DEFAULT_OWN_ADDR_TYPE,
        .peer_tpaddr = DEFAULT_PEER_TPADDR,
        .adv_filter_policy = DEFAULT_ADV_FILTER_POLICY,
        .primary_adv_phy = DEFAULT_PRIMARY_ADV_PHY,
        .secondary_adv_max_skip = DEFAULT_SECONDARY_ADV_MAX_SKIP,
        .secondary_adv_phy = DEFAULT_SECONDARY_ADV_PHY,
        .adv_sid = DEFAULT_SID
    };
    return TRUE;
}

#else

#include <adc.h>
#include <panic.h>
#include <stdlib.h>

#include "connection_abstraction.h"
#include <task_list.h>
#include <connection_manager.h>

#include "local_addr.h"
#include "local_name.h"

#include "le_advertising_manager_sm.h"
#include "le_advertising_manager_clients.h"
#include "le_advertising_manager_data_common.h"
#include "le_advertising_manager_private.h"
#include "le_advertising_manager_select_common.h"
#include "le_advertising_manager_select_extended.h"
#include "le_advertising_manager_select_legacy.h"
#if defined(USE_NEW_SM_FOR_EXTENDED_ADVERTISING) || defined(USE_NEW_SM_FOR_LEGACY_ADVERTISING)
#include "le_advertising_manager_set_sm_msg_handler.h"
#endif
#include "le_advertising_manager_utils.h"

#include "logging.h"
#include "hydra_macros.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(le_adv_mgr_message_id_t)
LOGGING_PRESERVE_MESSAGE_TYPE(adv_mgr_internal_messages_t)

#ifndef HOSTED_TEST_ENVIRONMENT
/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */

ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(ADV_MANAGER, ADV_MANAGER_MESSAGE_END)

#endif

#define LE_EXT_ADV_SCAN_ENABLE_MASK        2

/*!< Task information for the advertising manager */
adv_mgr_task_data_t app_adv_manager;

#if !defined(USE_NEW_SM_FOR_LEGACY_ADVERTISING)
static le_adv_mgr_state_machine_t * sm = NULL;
#endif
static Task task_allow_all;
static Task task_enable_connectable;

static void leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally(Task task, le_adv_mgr_status_t status);
static void leAdvertisingManager_SetAllowAdvertising(bool allow, le_adv_advertising_blocks_t flag);
static bool leAdvertisingManager_ScheduleAllowDisallowMessages(bool allow, le_adv_mgr_status_t status);
static void leAdvertisingManager_EnableAdvertising(bool enable);
static bool leAdvertisingManager_ScheduleEnableDisableConnectableMessages(bool enable, le_adv_mgr_status_t status);
#if !(defined(USE_NEW_SM_FOR_EXTENDED_ADVERTISING) || defined(USE_NEW_SM_FOR_LEGACY_ADVERTISING))
static void leAdvertisingManager_ScheduleInternalRestartAdvertising(void);
#endif
static void leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(bool action);
static void leAdvertisingManager_HandleInternalParametersUpdateRequest(void);
static void leAdvertisingManager_HandleInternalReleaseDatasetRequest(const LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T * msg);
static void leAdvertisingManager_HandleInternalAllowAdvertisingRequest(const LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T * message);
static void leAdvertisingManager_HandleInternalEnableConnectableRequest(const LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T * message);
static bool leAdvertisingManager_UpdateParameters(uint8);
static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message);
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void);
static void leAdvertisingManager_HandleInternalRestartAdvertising(void);
static void leAdvertisingManager_HandleInternalUnblocked(void);
static void leAdvertisingManager_HandleInternalRestartCheckIfStopped(void);


static void leAdvertisingManager_HandleConManagerTpConnectInd(const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    PanicNull((CON_MANAGER_TP_CONNECT_IND_T*)ind);

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleConManagerTpConnectInd enum:le_adv_mgr_state_t:%d %06x Incoming:%d",
        LeAdvertisingManagerSm_GetState(),
        ind->tpaddr.taddr.addr.lap,
        ind->incoming);
}

static void handleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG_LEVEL_1("LEAM handle_message MESSAGE:adv_mgr_internal_messages_t:0x%x",id);

    switch (id)
    {
        case LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING:
            leAdvertisingManager_HandleEnableAdvertising((const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T*)message);
            return;

        case LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE:
            leAdvertisingManager_HandleNotifyRpaAddressChange();
            break;
        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM, (const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T*)message, FALSE);
            break;

        case CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM, (const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T*)message, FALSE);
            break;

        case CL_DM_BLE_SET_EXT_ADV_DATA_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_EXT_ADV_DATA_CFM, (const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T*)message, FALSE);
            break;

        case CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM, (const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T*)message, FALSE);
            break;

        case CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM, (const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T*)message, FALSE);
            break;

        case CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM, (const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T*)message, FALSE);
            break;
        case CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM, (const CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T*)message, FALSE);
            break;
        case CL_DM_BLE_EXT_ADV_TERMINATED_IND:
            LeAdvertisingManager_HandleConnectionLibraryMessages(CL_DM_BLE_EXT_ADV_TERMINATED_IND, (const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T*)message, FALSE);
            break;
        case LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER:
            leAdvertisingManager_HandleInternalIntervalSwitchover();
            break;

        case LE_ADV_MGR_INTERNAL_DATA_UPDATE:
            leAdvertisingManager_HandleInternalDataUpdateRequest();
            break;

        case LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE:
            leAdvertisingManager_HandleInternalEnableConnectableRequest((const LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T *) message);
            break;

        case LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING:
            leAdvertisingManager_HandleInternalAllowAdvertisingRequest((const LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T *) message);
            break;

        case LE_ADV_MGR_INTERNAL_RESTART_ADVERTISING:
            leAdvertisingManager_HandleInternalRestartAdvertising();
            break;

        case LE_ADV_MGR_INTERNAL_RELEASE_DATASET:
            if(leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_none))
            {
                /* No blocking condition, process the release dataset request */
                leAdvertisingManager_HandleInternalReleaseDatasetRequest((const LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T *) message);
            }
            else
            {
                /* Blocking condition exists, repost the release dataset request */
                MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_RELEASE_DATASET, message, &leAdvertisingManager_GetBlockingCondition());
            }
            break;

        case LE_ADV_MGR_INTERNAL_PARAMS_UPDATE:
            leAdvertisingManager_HandleInternalParametersUpdateRequest();
            break;

        case LE_ADV_MGR_INTERNAL_RESTART_CHECK_STOPPED:
            leAdvertisingManager_HandleInternalRestartCheckIfStopped();
            break;

        case LE_ADV_MGR_INTERNAL_UNBLOCKED:
            leAdvertisingManager_HandleInternalUnblocked();
            break;

        case CON_MANAGER_TP_CONNECT_IND:
            leAdvertisingManager_HandleConManagerTpConnectInd((const CON_MANAGER_TP_CONNECT_IND_T *) message);
            break;

        case CON_MANAGER_TP_DISCONNECT_IND:
        case CON_MANAGER_TP_DISCONNECT_REQUESTED_IND:
        case CON_MANAGER_BLE_PARAMS_UPDATE_IND:
            /* These messages are sent when registered with connection manager.
               They are not needed by the LE Advertising manager. */
            break;

        default:
            Panic();
            break;
    }

}

/* Local function to enable/disable connectable advertising */
static void leAdvertisingManager_InternalEnableConnectableAdvertising(Task task, bool enable)
{
    task_enable_connectable = task;

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_InternalEnableConnectableAdvertising enable %d", enable);

    leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(enable);

    leAdvertisingManager_EnableAdvertising(enable);

    leAdvertisingManager_ScheduleEnableDisableConnectableMessages(enable, le_adv_mgr_status_success);
}

/* Local function to send an internal message to schedule connectable advertising */
static void leAdvertisingManager_ScheduleInternalEnableConnectableAdvertising(Task task, bool enable)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalEnableConnectableAdvertising, Send message LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE on blocking condition %d", adv_task_data->blockingCondition);

    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE);
    message->enable = enable;
    message->task = task;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE, message, &adv_task_data->blockingCondition );

}

/* Local function to handle internal connectable advertising request */
static void leAdvertisingManager_HandleInternalEnableConnectableRequest(const LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T * msg)
{
    leAdvertisingManager_InternalEnableConnectableAdvertising(msg->task, msg->enable);

}

/* Local function to allow/disallow advertising */
static void leAdvertisingManager_InternalAllowAdvertising(Task task, bool allow)
{
    task_allow_all = task;

    leAdvertisingManager_SetAllowAdvertising(allow, le_adv_block_advertising_topology);

    leAdvertisingManager_EnableAdvertising(allow);

    leAdvertisingManager_ScheduleAllowDisallowMessages(allow, le_adv_mgr_status_success);

}

/* Local function to send an internal message to schedule allow/disallow advertising */
static void leAdvertisingManager_ScheduleInternalAllowAdvertising(Task task, bool allow)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalAllowAdvertising, Send message LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING on blocking condition %d", adv_task_data->blockingCondition);

    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING);
    message->allow = allow;
    message->task = task;
    MessageSendConditionally(AdvManagerGetTask(),
                             LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING, message,
                             &leAdvertisingManager_GetBlockingCondition());
}

/* Local function to send an internal message to restart advertising */
static void leAdvertisingManager_ScheduleInternalRestartAdvertising(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalRestartAdvertising, Send message LE_ADV_MGR_INTERNAL_RESTART_ADVERTISING on blocking condition %d", adv_task_data->blockingCondition);

    MessageCancelFirst(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_RESTART_ADVERTISING);
    MessageSendConditionally(AdvManagerGetTask(),
                             LE_ADV_MGR_INTERNAL_RESTART_ADVERTISING, NULL,
                             &leAdvertisingManager_GetBlockingCondition());
}

/* Local function to handle internal allow/disallow advertising request */
static void leAdvertisingManager_HandleInternalAllowAdvertisingRequest(const LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T * msg)
{
    leAdvertisingManager_InternalAllowAdvertising(msg->task, msg->allow);
}

static void leAdvertisingManager_HandleInternalRestartAdvertising(void)
{
    leAdvertisingManager_SetAllowAdvertising(FALSE, le_adv_block_advertising_restart);
    leAdvertisingManager_EnableAdvertising(FALSE);

    MessageCancelFirst(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_RESTART_CHECK_STOPPED);
    MessageSendConditionally(AdvManagerGetTask(),
                             LE_ADV_MGR_INTERNAL_RESTART_CHECK_STOPPED, NULL,
                             &leAdvertisingManager_GetBlockingCondition());

    MessageCancelFirst(AdvManagerGetTask(),LE_ADV_MGR_INTERNAL_UNBLOCKED);
    MessageSendConditionally(AdvManagerGetTask(),
                             LE_ADV_MGR_INTERNAL_UNBLOCKED, NULL,
                             &app_adv_manager.is_advertising_blocked);
}

/*  Blocking conditions have been cleared, Restart advertising from
    suspended */
static void leAdvertisingManager_HandleInternalUnblocked(void)
{
    leAdvertisingManager_EnableAdvertising(TRUE);
}


static void leAdvertisingManager_HandleInternalRestartCheckIfStopped(void)
{
    DEBUG_LOG("leAdvertisingManager_HandleInternalRestartCheckIfStopped adv state idle:%d susp:%d extended:%d",
                LeAdvertisingManagerSm_IsInitialised(),
                LeAdvertisingManagerSm_IsSuspended(),
                LeAdvertisingManager_IsExtendedAdvertisingInactive());

    leAdvertisingManager_SetAllowAdvertising(TRUE, le_adv_block_advertising_restart);
}


/* Local function to release dataset */
static bool leAdvertisingManager_InternalReleaseDataset(le_adv_data_set_handle handle)
{
    le_adv_data_set_t start_params_set;

    leAdvertisingManager_EnableAdvertising(FALSE);

    leAdvertisingManager_SetDataSetSelectBitmask(handle->set, FALSE);

    leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally(leAdvertisingManager_GetTaskForDataSet(handle->set), le_adv_mgr_status_success);

    MessageCancelAll(AdvManagerGetLegacyTask(), LE_ADV_MGR_INTERNAL_START);
    MessageCancelAll(AdvManagerGetExtendedTask(), LE_ADV_MGR_INTERNAL_START);

    leAdvertisingManager_FreeHandleForDataSet(handle->set);

    start_params_set = leAdvertisingManager_GetDataSetSelected();

    if(start_params_set)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Local start parameters contain a valid set, reschedule advertising start with the set %x", leAdvertisingManager_GetDataSetSelected());

        leAdvertisingManager_SetDataUpdateRequired(start_params_set, TRUE);

        leAdvertisingManager_SetDataSetSelectMessageStatusAfterRelease(start_params_set);

        LeAdvertisingManager_ScheduleAdvertisingStart(start_params_set);
    }

    return TRUE;
}

/* Local function to send an internal message to schedule release dataset */
static void leAdvertisingManager_ScheduleInternalReleaseDataset(le_adv_data_set_handle handle)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalReleaseDataset, Send message LE_ADV_MGR_INTERNAL_RELEASE_DATASET on blocking condition %d", adv_task_data->blockingCondition);

    MAKE_MESSAGE(LE_ADV_MGR_INTERNAL_RELEASE_DATASET);
    message->handle = handle;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_RELEASE_DATASET, message, &adv_task_data->blockingCondition );
}

/* Local function to handle internal release dataset request */
static void leAdvertisingManager_HandleInternalReleaseDatasetRequest(const LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T * msg)
{
    leAdvertisingManager_InternalReleaseDataset(msg->handle);

}

/* Local function to send an internal message to schedule parameters update */
static void leAdvertisingManager_ScheduleInternalParametersUpdate(void)
{
    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    DEBUG_LOG_LEVEL_1("leAdvertisingManager_ScheduleInternalParametersUpdate, Send message LE_ADV_MGR_INTERNAL_PARAMS_UPDATE on blocking condition %d", adv_task_data->blockingCondition);

    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_MGR_INTERNAL_PARAMS_UPDATE, NULL, &adv_task_data->blockingCondition );
}

/* Local function to handle internal parameters update request */
static void leAdvertisingManager_HandleInternalParametersUpdateRequest(void)
{
    leAdvertisingManager_SetupAdvertParams();
}

/* Local Function to Handle Internal Enable Advertising Message */
static void leAdvertisingManager_HandleEnableAdvertising(const LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T * message)
{
    if(TRUE == message->action)
    {
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
        le_advertising_manager_set_state_machine_t *sm = LeAdvertisingManager_SetSmGetByAdvHandle(LEGACY_ADV_HANDLE_APP_SET);
        LeAdvertisingManager_SetSmEnable(sm);
#else
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);
#endif
    }
    else
    {
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
        le_advertising_manager_set_state_machine_t *sm = LeAdvertisingManager_SetSmGetByAdvHandle(LEGACY_ADV_HANDLE_APP_SET);
        LeAdvertisingManager_SetSmDisable(sm);
#else
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
#endif
    }

#ifndef USE_NEW_SM_FOR_LEGACY_ADVERTISING
    leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
#endif

}

/* Local Function to Handle Internal Notify Rpa Address Change Message */
static void leAdvertisingManager_HandleNotifyRpaAddressChange(void)
{
    le_adv_mgr_client_iterator_t iterator;
    le_adv_mgr_register_handle client_handle = leAdvertisingManager_HeadClient(&iterator);

    while(client_handle)
    {
        if(client_handle->task)
        {
            MessageSend(client_handle->task, LE_ADV_MGR_RPA_TIMEOUT_IND, NULL);
        }

        client_handle = leAdvertisingManager_NextClient(&iterator);
    }

    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
    MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));
}

/* Local Function to Cancel Scheduled Enable/Disable Connectable Confirmation Messages */
static bool leAdvertisingManager_CancelPendingEnableDisableConnectableMessages(void)
{
    MessageCancelAll(task_enable_connectable, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);

    return TRUE;
}

/* Local Function to Schedule Enable/Disable Connectable Confirmation Messages */
static bool leAdvertisingManager_ScheduleEnableDisableConnectableMessages(bool enable, le_adv_mgr_status_t status)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    MAKE_MESSAGE(LE_ADV_MGR_ENABLE_CONNECTABLE_CFM);
    message->enable = enable;
    message->status = status;

    MessageSendConditionally(task_enable_connectable, LE_ADV_MGR_ENABLE_CONNECTABLE_CFM, message, &advMan->blockingCondition);

    return TRUE;
}

/* Local Function to Cancel Scheduled Allow/Disallow Messages */
static bool leAdvertisingManager_CancelPendingAllowDisallowMessages(void)
{
    MessageCancelAll(task_allow_all, LE_ADV_MGR_ALLOW_ADVERTISING_CFM);

    return TRUE;
}

/* Local Function to Schedule Allow/Disallow Confirmation Messages */
static bool leAdvertisingManager_ScheduleAllowDisallowMessages(bool allow, le_adv_mgr_status_t status)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    MAKE_MESSAGE(LE_ADV_MGR_ALLOW_ADVERTISING_CFM);
    message->allow = allow;
    message->status = status;

    MessageSendConditionally(task_allow_all, LE_ADV_MGR_ALLOW_ADVERTISING_CFM, message, &advMan->blockingCondition);

    return TRUE;
}

/* Local Function to Schedule an Internal Advertising Enable/Disable Message */
static void leAdvertisingManager_ScheduleInternalEnableMessage(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    MAKE_MESSAGE(LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING);
    message->action = action;
    MessageSendConditionally(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING, message, &advMan->blockingCondition );
}

/* Local Function to Return the State of Connectable LE Advertising Being Enabled/Disabled */
bool leAdvertisingManager_IsConnectableAdvertisingEnabled(void)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    const unsigned bitmask_connectable_events = le_adv_event_type_connectable_general | le_adv_event_type_connectable_directed;

    return (bitmask_connectable_events == advMan->mask_enabled_events);
}

/* Prevent advertising if any below conditions are not met */
static bool leAdvertisingManager_IsAdvertisingPossible(void)
{
    DEBUG_LOG("leAdvertisingManager_IsAdvertisingPossible  Enabled:%d Allowed:%d DataSet:%d",
            leAdvertisingManager_IsConnectableAdvertisingEnabled(),
            leAdvertisingManager_IsAdvertisingAllowed(),
            leAdvertisingManager_IsDataSetSelected());

    if(!leAdvertisingManager_IsConnectableAdvertisingEnabled())
    {
        return FALSE;
    }
    if(!leAdvertisingManager_IsAdvertisingAllowed())
    {
        return FALSE;
    }
    if(!leAdvertisingManager_IsDataSetSelected())
    {
        return FALSE;
    }
    return TRUE;
}

/* Cancel pending messages and restart enable/disable */
static void leAdvertisingManager_RestartEnableAdvertising(bool enable)
{
    leAdvertisingManager_CancelPendingAllowDisallowMessages();
    leAdvertisingManager_CancelPendingEnableDisableConnectableMessages();
    leAdvertisingManager_ScheduleInternalEnableMessage(enable);
}

/* Local Function to Decide whether to Suspend or Resume Advertising and act as decided */
static void leAdvertisingManager_EnableAdvertising(bool enable)
{
    if(enable)
    {
        if(leAdvertisingManager_IsAdvertisingPossible())
        {
            if(LeAdvertisingManagerSm_IsSuspended())
            {
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
                le_advertising_manager_set_state_machine_t *sm = LeAdvertisingManager_SetSmGetByAdvHandle(LEGACY_ADV_HANDLE_APP_SET);
                LeAdvertisingManager_SetSmEnable(sm);
#else
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_starting);

                leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
#endif
            }
            else if(LeAdvertisingManagerSm_IsSuspending())
            {
                leAdvertisingManager_RestartEnableAdvertising(enable);
            }
            else if (leAdvertisingManager_EnableExtendedAdvertising(enable))
            {
                /* Nothing more to do as extended advertising will be in progress */
            }
        }
    }
    else
    {
        if(LeAdvertisingManagerSm_IsAdvertisingStarting())
        {
            leAdvertisingManager_RestartEnableAdvertising(enable);
        }
        else if(LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
            le_advertising_manager_set_state_machine_t *sm = LeAdvertisingManager_SetSmGetByAdvHandle(LEGACY_ADV_HANDLE_APP_SET);
            LeAdvertisingManager_SetSmDisable(sm);
#else
            LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspending);
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_enable_cfm);
#endif
        }
        else if (leAdvertisingManager_EnableExtendedAdvertising(enable))
        {
            /* Nothing more to do as extended advertising will be in progress */
        }
    }
}

/* Local Function to Set/Reset Bitmask for Connectable Advertising Event Types */
static void leAdvertisingManager_SetAllowedAdvertisingBitmaskConnectable(bool action)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();

    advMan->mask_enabled_events |= le_adv_event_type_connectable_general;
    advMan->mask_enabled_events |= le_adv_event_type_connectable_directed;

    if(FALSE == action)
    {
        advMan->mask_enabled_events &= ~le_adv_event_type_connectable_general;
        advMan->mask_enabled_events &= ~le_adv_event_type_connectable_directed;;
    }
}

/* Local Function to Set the Allow/Disallow Flag for All Advertising Event Types */
static void leAdvertisingManager_SetAllowAdvertising(bool allow, le_adv_advertising_blocks_t flag)
{
    adv_mgr_task_data_t *advMan = AdvManagerGetTaskData();
    if (allow)
    {
        advMan->is_advertising_blocked &= ~(uint16)flag;
    }
    else
    {
        advMan->is_advertising_blocked |= (uint16)flag;
    }
}

/* Local Function to Handle Connection Library Advertise Enable Fail Response */
static void leAdvertisingManager_HandleLegacySetAdvertisingEnableCfmFailure(void)
{
    if(leAdvertisingManager_CancelPendingAllowDisallowMessages())
    {
        bool adv_allowed = leAdvertisingManager_IsAdvertisingAllowed();
        leAdvertisingManager_ScheduleAllowDisallowMessages(adv_allowed, le_adv_mgr_status_error_unknown);
    }

    if(leAdvertisingManager_CancelPendingEnableDisableConnectableMessages())
    {
        bool connectable = leAdvertisingManager_IsConnectableAdvertisingEnabled();
        leAdvertisingManager_ScheduleEnableDisableConnectableMessages(connectable, le_adv_mgr_status_error_unknown);
    }

#ifndef USE_NEW_SM_FOR_LEGACY_ADVERTISING
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
#endif

    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);
}

/* Local function to set the state machine to suspended state and cancel RPA notify messages */
static void leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages(void)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages");

#ifndef USE_NEW_SM_FOR_LEGACY_ADVERTISING
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_suspended);
#endif

    MessageCancelAll(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE);

}

/* Function to handle CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM message */
void leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm(uint16 status)
{
    bool enable_success = FALSE;
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm status enum:hci_status:%u", status);

#if defined(USE_NEW_SM_FOR_LEGACY_ADVERTISING)
    if (TRUE)
#else
    if (leAdvertisingManager_CheckBlockingCondition(le_adv_blocking_condition_enable_cfm))
#endif
    {
        if(hci_success == status)
        {
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
            if(   LeAdvertisingManagerSm_IsSuspending()
               || LeAdvertisingManagerSm_IsSuspended())
#else
            if(LeAdvertisingManagerSm_IsSuspending())
#endif
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Info, State machine is in suspending state");

                leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages();
                leAdvertisingManager_CancelMessageParameterSwitchover();

                enable_success = TRUE;
            }
#ifdef USE_NEW_SM_FOR_LEGACY_ADVERTISING
            else if(   LeAdvertisingManagerSm_IsAdvertisingStarting()
                    || LeAdvertisingManagerSm_IsAdvertisingStarted())
#else
            else if(LeAdvertisingManagerSm_IsAdvertisingStarting())
#endif
            {
                DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Info, State machine is in starting state");

#ifndef USE_NEW_SM_FOR_LEGACY_ADVERTISING
                LeAdvertisingManagerSm_SetState(le_adv_mgr_state_started);
#endif
                MessageSendLater(AdvManagerGetTask(), LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE, NULL, D_SEC(BLE_RPA_TIMEOUT_DEFAULT));
                leAdvertisingManager_SendMessageParameterSwitchover();
                enable_success = TRUE;
            }
        }
        else if( (hci_error_command_disallowed == status) && LeAdvertisingManagerSm_IsSuspending() )
        {
            DEBUG_LOG_LEVEL_2("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Info, State machine is in suspending state, encountered an expected command disallowed error, treated as success, HCI status is %x", status);

            leAdvertisingManager_SetSuspendedStateAndCancelRpaNotifyMessages();
            leAdvertisingManager_CancelMessageParameterSwitchover();
            enable_success = TRUE;
        }
        else
        {
            DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Failure, CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM received with failure, HCI status is enum:hci_status:%u", status);

            leAdvertisingManager_HandleLegacySetAdvertisingEnableCfmFailure();
        }

        if (!enable_success || !leAdvertisingManager_EnableExtendedAdvertising(LeAdvertisingManagerSm_IsAdvertisingStarted()))
        {
            leAdvertisingManager_SetBlockingCondition(le_adv_blocking_condition_none);
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm Failure, Message Received in Unexpected Blocking Condition %x",
                          leAdvertisingManager_GetBlockingCondition());

        Panic();
    }
}

/* Local function to prepare and send the conditional confirmation messages */
static void leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally(Task task, le_adv_mgr_status_t status)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    MAKE_MESSAGE(LE_ADV_MGR_RELEASE_DATASET_CFM);
    message->status = status;

    DEBUG_LOG_LEVEL_2("leAdvertisingManager_SendReleaseDataSetCfmMessageConditionally Info, Task is %x, status is %x, on blocking condition %d", task, status, adv_task_data->blockingCondition);

    MessageSendConditionally(task, LE_ADV_MGR_RELEASE_DATASET_CFM, message, &adv_task_data->blockingCondition );
}

/* Local function to check if parameter fallback mechanism needs to be activated */
static bool leAdvertisingManager_IsFallbackNeeded(void)
{
    DEBUG_LOG("leAdvertisingManager_IsFallbackNeeded");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    le_adv_parameters_config_table_t * table = adv_task_data->params_handle->config_table;
    uint8 index = adv_task_data->params_handle->index_active_config_table_entry;
    uint16 timeout = table->row[index].timeout_fallback_in_seconds;

    if(timeout)
    {
        DEBUG_LOG("leAdvertisingManager_IsFallbackNeeded, Fallback is needed with timeout %d seconds", timeout);

        return TRUE;
    }

    return FALSE;

}

/* Local function to update advertising parameters in use */
static bool leAdvertisingManager_UpdateParameters(uint8 index)
{
    DEBUG_LOG("leAdvertisingManager_UpdateParameters, Parameter Set Index is %d", index);

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Not Initialised");
        return FALSE;
    }

    if((le_adv_preset_advertising_interval_slow != index) && (le_adv_preset_advertising_interval_fast != index))
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Invalid Index");
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(NULL == adv_task_data->params_handle)
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Invalid Parameter");
        return FALSE;
    }

    if((index != adv_task_data->params_handle->active_params_set))
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Index Different, Change Parameter Set Immediately");

        adv_task_data->params_handle->active_params_set = index;

        leAdvertisingManager_ScheduleInternalParametersUpdate();
    }
    else if((leAdvertisingManager_IsFallbackNeeded()))
    {
        DEBUG_LOG("leAdvertisingManager_UpdateParameters, Fallback Needed, Change Parameter Set after Timeout");

        leAdvertisingManager_SendMessageParameterSwitchover();
    }

    return TRUE;
}

/* API Function to Register Callback Functions For LE Advertising Data */
le_adv_mgr_register_handle LeAdvertisingManager_Register(Task task, const le_adv_data_callback_t * const callback)
{
    DEBUG_LOG_VERBOSE("LeAdvertisingManager_Register");

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return NULL;

    return LeAdvertisingManager_NewClient(task, callback);
}

static void leAdvertisingManager_GetAdvScanCapabilities(void)
{
    DEBUG_LOG("leAdvertisingManager_GetAdvScanCapabilities");
    ConnectionDmBleGetAdvScanCapabilitiesReq(AdvManagerGetTask());
}

static bool leAdvertisingManager_IfExtendedAdvertisementAndScanningEnabled(uint8 available_api)
{
    return (available_api & LE_EXT_ADV_SCAN_ENABLE_MASK);
}

static void leAdvertisingManager_HandleGetAdvScanCapabilitiesCfm(const CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T* cfm)
{
    PanicNull((CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T*)cfm);

    DEBUG_LOG("leAdvertisingManager_HandleGetAdvScanCapabilitiesCfm status=%d available_api=%x",
                                                                    cfm->status,cfm->available_api);

    if(cfm->status != success)
    {
        return;
    }

    if(leAdvertisingManager_IfExtendedAdvertisementAndScanningEnabled(cfm->available_api))
    {
        adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
        adv_task_data->is_extended_advertising_and_scanning_enabled = TRUE;
    }
}
/* API Function to Initialise LE Advertising Manager */
bool LeAdvertisingManager_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("LeAdvertisingManager_Init");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    memset(adv_task_data, 0, sizeof(*adv_task_data));
    adv_task_data->task.handler = handleMessage;

    leAdvertisingManager_DataInit();

#if defined (USE_NEW_SM_FOR_LEGACY_ADVERTISING) || defined(USE_NEW_SM_FOR_EXTENDED_ADVERTISING)
    LeAdvertisingManager_SetSmInit();
#endif

    leAdvertisingManager_SelectLegacyAdvertisingInit();
    leAdvertisingManager_SelectExtendedAdvertisingInit();

    leAdvertisingManager_ClearDataSetSelectBitmask();
    leAdvertisingManager_ClearDataSetMessageStatusBitmask();

    adv_task_data->dataset_handset_handle = NULL;
    adv_task_data->dataset_peer_handle = NULL;
    adv_task_data->dataset_extended_handset_handle = NULL;
    adv_task_data->params_handle = NULL;
    adv_task_data->is_params_update_required = TRUE;
    adv_task_data->is_advertising_blocked = 0x1;

#if !defined(USE_NEW_SM_FOR_LEGACY_ADVERTISING)
    sm = LeAdvertisingManagerSm_Init();
    PanicNull(sm);
    LeAdvertisingManagerSm_SetState(le_adv_mgr_state_initialised);
#endif /* !USE_NEW_SM_FOR_LEGACY_ADVERTISING */

    leAdvertisingManager_ClientsInit();

    leAdvertisingManager_SetDataSetEventType(le_adv_event_type_connectable_general);
    leAdvertisingManager_GetAdvScanCapabilities();

    task_allow_all = 0;
    task_enable_connectable = 0;

    return TRUE;
}

/* API Function to De-Initialise LE Advertising Manager */
bool LeAdvertisingManager_DeInit(void)
{
    DEBUG_LOG("LeAdvertisingManager_DeInit");

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(NULL != adv_task_data->params_handle)
    {
        free(adv_task_data->params_handle);
        adv_task_data->params_handle = NULL;
    }

    if(NULL != adv_task_data->dataset_handset_handle)
    {
        free(adv_task_data->dataset_handset_handle);
        adv_task_data->dataset_handset_handle = NULL;
    }

    if(NULL != adv_task_data->dataset_peer_handle)
    {
        free(adv_task_data->dataset_peer_handle);
        adv_task_data->dataset_peer_handle = NULL;
    }

    if(NULL != adv_task_data->dataset_extended_handset_handle)
    {
        free(adv_task_data->dataset_extended_handset_handle);
        adv_task_data->dataset_extended_handset_handle = NULL;
    }

    memset(adv_task_data, 0, sizeof(adv_mgr_task_data_t));

#if defined(USE_NEW_SM_FOR_LEGACY_ADVERTISING)
    le_advertising_manager_set_state_machine_t *sm = LeAdvertisingManager_SetSmGetByAdvHandle(LEGACY_ADV_HANDLE_APP_SET);
    if (sm)
    {
        LeAdvertisingManager_SetSmDestroy(sm);
    }
#else
    if(NULL != sm)
    {
        LeAdvertisingManagerSm_SetState(le_adv_mgr_state_uninitialised);

    }
#endif

    return TRUE;
}

/* API Function to enable/disable connectable LE advertising */
bool LeAdvertisingManager_EnableConnectableAdvertising(Task task, bool enable)
{
    DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising enable %d", enable);

    if(NULL == task)
    {
        DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising, Task Input is Null");
        return FALSE;
    }

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising, LE adv. mgr. Uninitialised");
        return FALSE;
    }

    if((task_enable_connectable != task) && (task_enable_connectable))
    {
        DEBUG_LOG("LeAdvertisingManager_EnableConnectableAdvertising, Task Input is Invalid");
        return FALSE;
    }

    leAdvertisingManager_ScheduleInternalEnableConnectableAdvertising(task, enable);

    return TRUE;
}

/* API Function to enable/disable all LE advertising */
bool LeAdvertisingManager_AllowAdvertising(Task task, bool allow)
{
    bool response = FALSE;

    if (task && LeAdvertisingManagerSm_IsInitialised())
    {
        response = TRUE;
        leAdvertisingManager_ScheduleInternalAllowAdvertising(task, allow);
    }

    DEBUG_LOG("LeAdvertisingManager_AllowAdvertising. allow:%d response:%d",
                allow, response);

    return response;
}

static void leAdvertisingManager_HandleExtAdvTerminatedIndication(const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T *ind)
{
    DEBUG_LOG_LEVEL_1("leAdvertisingManager_HandleExtAdvTerminatedIndication enum:le_adv_mgr_state_t:%d %06x Reason:%d 0x%x",
        LeAdvertisingManagerSm_GetState(),
        ind->taddr.addr.lap,
        ind->reason,
        ind->adv_bits);

    leAdvertisingManager_ScheduleInternalRestartAdvertising();
}

/* API function to use as the handler for connection library messages */
bool LeAdvertisingManager_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    DEBUG_LOG_V_VERBOSE("LeAdvertisingManager_HandleConnectionLibraryMessages MESSAGE:0x%x", id);

    switch (id)
    {
        /* Legacy advertising */
        case CL_DM_BLE_SET_ADVERTISING_DATA_CFM:
            leAdvertisingManager_HandleSetLegacyAdvertisingDataCfm(((const CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T *)message)->status);
            return TRUE;

        case CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM:
            leAdvertisingManager_HandleLegacySetScanResponseDataCfm(((const CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T *)message)->status);
            return TRUE;

        case CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM:
            leAdvertisingManager_HandleLegacySetAdvertisingParamCfm(((const CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T *)message)->status);
            return TRUE;

        case CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM:
            leAdvertisingManager_HandleLegacySetAdvertisingEnableCfm(((const CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T *)message)->status);
            return TRUE;

        /* Extended advertising */
        case CL_DM_BLE_SET_EXT_ADV_DATA_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingDataCfm((const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM:
            leAdvertisingManager_HandleExtendedSetScanResponseDataCfm((const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingParamCfm((const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM:
            leAdvertisingManager_HandleExtendedSetAdvertisingEnableCfm((const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM:
            leAdvertisingManager_HandleExtendedAdvertisingRegisterCfm((const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM:
            leAdvertisingManager_HandleGetAdvScanCapabilitiesCfm((const CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T *)message);
            return TRUE;

        case CL_DM_BLE_EXT_ADV_TERMINATED_IND:
            leAdvertisingManager_HandleExtAdvTerminatedIndication((const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T *)message);
            return TRUE;

        default:
            return already_handled;
    }
}

/* API function to select the data set for undirected advertising */
le_adv_data_set_handle LeAdvertisingManager_SelectAdvertisingDataSet(Task task, const le_adv_select_params_t * params)
{
    DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet");

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, State Machine is not Initialised");

        return NULL;
    }

    if( (NULL == params) || (NULL == task) )
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, Invalid Input Arguments");

        return NULL;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_SelectAdvertisingDataSet Info, Task is %x Selected Data Set is %x" , task, params->set);
    }

    if(leAdvertisingManager_CheckIfHandleExists(params->set))
    {
        DEBUG_LOG("LeAdvertisingManager_SelectAdvertisingDataSet Failure, Dataset Handle Already Exists");

        return NULL;

    }
    else
    {
        leAdvertisingManager_SetDataUpdateRequired(params->set, TRUE);

        le_adv_data_set_handle handle = leAdvertisingManager_CreateNewDataSetHandle(params->set);

        handle->task = task;

        leAdvertisingManager_SetDataSetSelectBitmask(params->set, TRUE);

        leAdvertisingManager_SetDataSetSelectMessageStatusBitmask(params->set, TRUE);

        if(leAdvertisingManager_IsLegacySet(params->set))
        {
            MessageCancelAll(AdvManagerGetLegacyTask(), LE_ADV_MGR_INTERNAL_START);
        }
        else
        {
            MessageCancelAll(AdvManagerGetExtendedTask(), LE_ADV_MGR_INTERNAL_START);
        }

        LeAdvertisingManager_ScheduleAdvertisingStart(params->set);

        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_SelectAdvertisingDataSet Info, Handle does not exist, create new handle, handle->task is %x, handle->set is %x", handle->task, handle->set);

        return handle;
    }
}

/* API function to release the data set for undirected advertising */
bool LeAdvertisingManager_ReleaseAdvertisingDataSet(le_adv_data_set_handle handle)
{
    DEBUG_LOG("LeAdvertisingManager_ReleaseAdvertisingDataSet");

    if(NULL == handle)
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_ReleaseAdvertisingDataSet Failure, Invalid data set handle");

        return FALSE;
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_ReleaseAdvertisingDataSet Info, Data set handle is %x", handle);
    }

    leAdvertisingManager_ScheduleInternalReleaseDataset(handle);

    return TRUE;
}

/* API function to notify a change in the data */
bool LeAdvertisingManager_NotifyDataChange(Task task, const le_adv_mgr_register_handle handle)
{
    DEBUG_LOG("LeAdvertisingManager_NotifyDataChange");

    if(!leAdvertisingManager_ClientHandleIsValid(handle))
    {
        DEBUG_LOG_LEVEL_1("LeAdvertisingManager_NotifyDataChange Failure, Invalid Handle");
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    leAdvertisingManager_SetDataUpdateRequired(LE_ADV_MGR_ADVERTISING_SET_LEGACY, TRUE);

    if(adv_task_data->keep_advertising_on_notify)
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Keep advertising without restarting");

        if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Advertising in progress, schedule data update without suspending ongoing advertising");
            leAdvertisingManager_ScheduleInternalDataUpdate();
        }
    }
    else
    {
        DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Restart advertising is needed");
        if(LeAdvertisingManagerSm_IsAdvertisingStarting() || LeAdvertisingManagerSm_IsAdvertisingStarted())
        {
            DEBUG_LOG_LEVEL_2("LeAdvertisingManager_NotifyDataChange Info, Advertising in progress, suspend and reschedule advertising");
            LeAdvertisingManager_ScheduleAdvertisingStart(leAdvertisingManager_GetDataSetSelected());
        }
    }

    MAKE_MESSAGE(LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM);
    message->status = le_adv_mgr_status_success;

    MessageSend(task, LE_ADV_MGR_NOTIFY_DATA_CHANGE_CFM, message);
    return TRUE;
}

/* API function to configure LE advertising behavior when LeAdvertisingManager_NotifyDataChange() API is called */
bool LeAdvertisingManager_ConfigureAdvertisingOnNotifyDataChange(const le_adv_config_notify_t config)
{
    DEBUG_LOG("LeAdvertisingManager_ConfigureAdvertisingOnNotifyDataChange");

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;

    if((config != le_adv_config_notify_keep_advertising) && (config != le_adv_config_notify_restart_advertising))
        return FALSE;

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if(config == le_adv_config_notify_keep_advertising)
    {
        adv_task_data->keep_advertising_on_notify = TRUE;
    }
    else if(config == le_adv_config_notify_restart_advertising)
    {
        adv_task_data->keep_advertising_on_notify = FALSE;
    }

    return TRUE;
}

/* API function to register LE advertising parameter sets */
bool LeAdvertisingManager_ParametersRegister(const le_adv_parameters_t *params)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersRegister");

    if(NULL == params)
        return FALSE;

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
        return FALSE;

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    adv_task_data->params_handle =  PanicUnlessMalloc(sizeof(struct _le_adv_params_set));
    memset(adv_task_data->params_handle, 0, sizeof(struct _le_adv_params_set));

    adv_task_data->params_handle->params_set = (le_adv_parameters_set_t *)params->sets;
    adv_task_data->params_handle->config_table = (le_adv_parameters_config_table_t *)params->table;
    adv_task_data->params_handle->active_params_set = le_adv_preset_advertising_interval_invalid;
    adv_task_data->params_handle->index_active_config_table_entry = 0;

    return TRUE;

}

/* API function to select an LE advertising parameter set config table entry */
bool LeAdvertisingManager_ParametersSelect(uint8 index)
{
    DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Config Table Index is %d", index);

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Uninitialised");
        return FALSE;
    }

    if(index > le_adv_advertising_config_set_max)
    {
        DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Invalid Table Index");
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();

    if((adv_task_data->params_handle) && (adv_task_data->params_handle->config_table))
    {
        le_adv_parameters_set_t * set = adv_task_data->params_handle->params_set;
        le_adv_parameters_config_table_t * table = adv_task_data->params_handle->config_table;

        if((NULL == adv_task_data->params_handle) \
           || (NULL == set)
           || (NULL == table))
        {
            DEBUG_LOG("LeAdvertisingManager_ParametersSelect, Invalid Parameter");
            return FALSE;
        }

        leAdvertisingManager_CancelMessageParameterSwitchover();
        adv_task_data->params_handle->index_active_config_table_entry = index;

        return leAdvertisingManager_UpdateParameters(table->row[index].set_default);

    }

    return FALSE;

}

/* API function to retrieve LE advertising interval minimum/maximum value pair */
bool LeAdvertisingManager_GetAdvertisingInterval(le_adv_common_parameters_t * interval)
{
    DEBUG_LOG("LeAdvertisingManager_GetAdvertisingInterval");

    if(NULL == interval)
    {
        return FALSE;
    }

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }

    adv_mgr_task_data_t *adv_task_data = AdvManagerGetTaskData();
    le_adv_params_set_handle handle = adv_task_data->params_handle;

    if(NULL == adv_task_data->params_handle)
    {
        leAdvertisingManager_GetDefaultAdvertisingIntervalParams(interval);

        return TRUE;
    }

    if(le_adv_preset_advertising_interval_max >= adv_task_data->params_handle->active_params_set)
    {
        interval->le_adv_interval_min = handle->params_set->set_type[handle->active_params_set].le_adv_interval_min;
        interval->le_adv_interval_max = handle->params_set->set_type[handle->active_params_set].le_adv_interval_max;
    }

    return TRUE;
}


/* API function to retrieve LE advertising own address configuration */
bool LeAdvertisingManager_GetOwnAddressConfig(le_adv_own_addr_config_t * own_address_config)
{
    DEBUG_LOG("LeAdvertisingManager_GetOwnAddressConfig");

    if(FALSE == LeAdvertisingManagerSm_IsInitialised())
    {
        return FALSE;
    }

    own_address_config->own_address_type = LocalAddr_GetBleType();
    own_address_config->timeout = BLE_RPA_TIMEOUT_DEFAULT;

    return TRUE;
}
#endif
