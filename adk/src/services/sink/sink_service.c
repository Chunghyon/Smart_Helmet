/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sink service implementation
*/

/* local logging */
#include "sink_service_logging.h"

/* local includes */
#include "sink_service.h"
#include "sink_service_protected.h"
#include "sink_service_private.h"
#include "sink_service_config.h"
#include "sink_service_sm.h"


/* framework includes */
#include <bt_device.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <device_properties.h>
#include <task_list.h>
#include <ui.h>
#include <unexpected_message.h>

/* system includes */
#include <bdaddr.h>
#include <stdlib.h>
#include <panic.h>
#include <message.h>

/* Definition of sink_service log level */
DEBUG_LOG_DEFINE_LEVEL_VAR

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(sink_service_msg_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(SINK_SERVICE, SINK_SERVICE_MESSAGE_END)


#define SinkServiceCommonSm_ConnectRequest(sm)          SinkServiceSm_ConnectRequest(sm)
#define SinkServiceCommonSm_DisconnectRequest(sm)       SinkServiceSm_DisconnectRequest(sm)
#define SinkServiceCommonSm_PairRequest(sm)             SinkServiceSm_PairRequest(sm)
#define SinkServiceCommonSm_EnableAll()                 SinkServiceSm_EnableAll()
#define SinkServiceCommonSm_DisableAll()                SinkServiceSm_DisableAll()


/*! Handset Service module data. */
sink_service_data_t sink_service_data;

/*! \brief UI Inputs the Sink Service is interested in. */
const message_group_t sink_service_ui_inputs[] =
{
    UI_INPUTS_SINK_MESSAGE_GROUP,
};


/*! \brief Handle a CON_MANAGER_TP_CONNECT_IND for BR/EDR and BLE connections */
static void sinkService_HandleConManagerTpConnectInd(const CON_MANAGER_TP_CONNECT_IND_T *ind)
{
    TRANSPORT_T transport = ind->tpaddr.transport;
    const typed_bdaddr* taddr = &ind->tpaddr.taddr;

    DEBUG_LOG_DEBUG("sinkService_HandleConManagerTpConnectInd type[%d] addr [%04x,%02x,%06lx] incoming [%d]",
           taddr->type, taddr->addr.nap, taddr->addr.uap, taddr->addr.lap, ind->incoming);

    if (ind->incoming)
    {
        if(transport == TRANSPORT_BREDR_ACL)
        {
            device_t device = BtDevice_GetDeviceForBdAddr(&taddr->addr);

            if(!device)
            {
                device = PanicNull(BtDevice_GetDeviceCreateIfNew(&taddr->addr, DEVICE_TYPE_SINK));
                DEBUG_LOG_DEBUG("sinkService_HandleConManagerTpConnectInd Create new sink device %p", device);
                PanicFalse(BtDevice_SetDefaultProperties(device));

                BtDevice_SetFlags(device, DEVICE_FLAGS_NOT_PAIRED, DEVICE_FLAGS_NOT_PAIRED);
            }

            sink_service_state_machine_t *sm = sinkServiceSm_FindOrCreateSm(&ind->tpaddr);
            DEBUG_LOG_DEBUG("sinkService_HandleConManagerTpConnectInd received for BR/EDR sink %p", sm);

            if(sm)
            {
                if(!BtDevice_DeviceIsValid(sm->sink_device))
                {
                    SinkServiceSm_SetDevice(sm, device);
                }

                /* As sink just connected it cannot have profile connections, so clear flags */
                BtDevice_SetConnectedProfiles(sm->sink_device, 0);

                /* Forward the connection to the state machine. */
                SinkServiceSm_HandleConManagerBredrTpConnectInd(sm, ind);
            }
            else
            {
                DEBUG_LOG_WARN("SinkService: All instances busy, ignoring incoming connection.");
            }
        }
    }
}

static void sinkService_HandleUiInput(MessageId ui_input)
{
    DEBUG_LOG_FN_ENTRY("sinkService_HandleUiInput enum:UI_INPUT_ENUM:%d", ui_input);

    switch (ui_input)
    {
        case ui_input_pair_sink:
            FOR_EACH_SINK_SM(sm)
            {
                /* If any state machine instance is already active then it will disconnect and start pairing */
                if (sm->state > SINK_SERVICE_STATE_DISCONNECTED)
                {
                    DEBUG_LOG_INFO("SinkService: Disconnecting from active device. Pairing request pending.");
                    SinkService_DisconnectAll();
                    SinkService_GetTaskData()->pairing_request_pending = TRUE;
                    return;
                }
            }

            /* No instances are in use. Use the first instance in the list for this request */
            SinkServiceCommonSm_PairRequest(SinkService_GetTaskData()->state_machines);
            break;

        case ui_input_connect_sink:
            SinkService_Connect();
            break;
        case ui_input_disconnect_sink:
            SinkService_DisconnectAll();
            break;
        default:
            break;
    }
}

static void sinkService_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    if (SinkService_IsEnabled())
    {
        if (isMessageUiInput(id))
        {
           sinkService_HandleUiInput(id);
           return;
        }
        switch (id)
        {
        case CON_MANAGER_TP_CONNECT_IND:
            sinkService_HandleConManagerTpConnectInd((const CON_MANAGER_TP_CONNECT_IND_T *) message);
            break;

        default:
            UnexpectedMessage_HandleMessage(id);
            break;

        }
    } else
    {
        DEBUG_LOG_DEBUG("sinkService_HandleMessage: Not allowed. Sink service is disabled. ");
    }
}

static unsigned sinkService_GetCurrentContext(void)
{
    DEBUG_LOG_FN_ENTRY("sinkService_GetCurrentContext");

    sink_service_context_t current_ctxt = context_sink_disconnected;

    FOR_EACH_SINK_SM(sm)
    {
        switch (sm->state)
        {
            case SINK_SERVICE_STATE_PAIRING:
            {
                return context_sink_pairing;
            }
            case SINK_SERVICE_STATE_CONNECTING_BREDR_ACL:
            case SINK_SERVICE_STATE_CONNECTING_PROFILES:
            {
                return context_sink_connecting;
            }
            case SINK_SERVICE_STATE_CONNECTED:
            {
                current_ctxt = context_sink_connected;
            }
            break;
            case SINK_SERVICE_STATE_DISABLED:
            {
                return context_sink_disabled;
            }

        default:
            break;
        }
    }
    return (unsigned)current_ctxt;
}


/*
    Public functions
*/

bool SinkService_Init(Task task)
{
    UNUSED(task);
    DEBUG_LOG_FN_ENTRY("SinkService_Init");

    SinkService_GetTask()->handler = sinkService_HandleMessage;
    SinkService_GetTaskData()->disable_request_pending = FALSE;
    SinkService_GetTaskData()->pairing_enabled = TRUE;
    SinkService_GetTaskData()->pairing_request_pending = FALSE;
    ConManager_SetPageTimeout(MS_TO_BT_SLOTS(SinkServiceConfig_GetConfig()->page_timeout * MS_PER_SEC));

    /* Initialise all state machine instances */
    FOR_EACH_SINK_SM(sm)
    {
        SinkServiceSm_ClearInstance(sm);
    }

    /* register with connection manager to get notification of (dis)connections*/
    ConManagerRegisterTpConnectionsObserver(cm_transport_all, SinkService_GetTask());
    TaskList_InitialiseWithCapacity(SinkService_GetClientList(),SINK_SERVICE_CLIENT_TASKS_LIST_INIT_CAPACITY);
    Ui_RegisterUiProvider(ui_provider_sink_service, sinkService_GetCurrentContext);

    /* Register SM as a UI input consumer */
    Ui_RegisterUiInputConsumer(SinkService_GetTask(), sink_service_ui_inputs, ARRAY_DIM(sink_service_ui_inputs));

    return TRUE;
}

void SinkService_SendConnectedCfm(device_t device, sink_service_status_t status)
{
    MESSAGE_MAKE(message, SINK_SERVICE_CONNECTED_CFM_T);
    message->device = device;
    message->status = status;
    TaskList_MessageSend(&SinkService_GetTaskData()->clients, SINK_SERVICE_CONNECTED_CFM, message);
}

void SinkService_SendDisconnectedCfm(device_t device)
{
    MESSAGE_MAKE(message, SINK_SERVICE_DISCONNECTED_CFM_T);
    message->device = device;
    TaskList_MessageSend(&SinkService_GetTaskData()->clients, SINK_SERVICE_DISCONNECTED_CFM, message);
}

void SinkService_SendFirstProfileConnectedIndNotification(device_t device)
{
    MESSAGE_MAKE(message, SINK_SERVICE_FIRST_PROFILE_CONNECTED_IND_T);
    message->device = device;
    TaskList_MessageSend(&SinkService_GetTaskData()->clients, SINK_SERVICE_FIRST_PROFILE_CONNECTED_IND, message);
}

void SinkService_ClientRegister(Task client_task)
{
    DEBUG_LOG_FN_ENTRY("SinkService_ClientRegister");

    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(SinkService_GetClientList()), client_task);
}

void SinkService_ClientUnregister(Task client_task)
{
    DEBUG_LOG_FN_ENTRY("SinkService_ClientUnregister");

    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(SinkService_GetClientList()), client_task);
}

void SinkService_EnablePairing(bool enable)
{
    DEBUG_LOG_FN_ENTRY("SinkService_EnablePairing: enable %d", enable);
    SinkService_GetTaskData()->pairing_enabled = enable;
}

bool SinkService_Connect(void)
{
    DEBUG_LOG_FN_ENTRY("SinkService_Connect");

    /* Connect requests denied when disabled */
    if ( SinkService_IsEnabled() )
    {
        /* iterate through all the Sinks, only allow one connection at a time */
        FOR_EACH_SINK_SM(sm)
        {
            /* If any state machine instance is not disconnected then this call can be ignored */
            if (sm->state > SINK_SERVICE_STATE_DISCONNECTED)
            {
                DEBUG_LOG_INFO("SinkService: Already connected");
                return FALSE;
            }
        }

        /* No instances are in use. Use the first instance in the list for this request */
        return SinkServiceCommonSm_ConnectRequest(SinkService_GetTaskData()->state_machines);
    } else
    {
        DEBUG_LOG_INFO("SinkService_Connect: Not allowed. Sink service is disabled.");
        return FALSE;
    }
}

void SinkService_ConnectableEnableBredr(bool enable)
{
    if(enable)
    {
        BredrScanManager_PageScanRequest(SinkService_GetTask(), SCAN_MAN_PARAMS_TYPE_FAST);
    }
    else
    {
        BredrScanManager_PageScanRelease(SinkService_GetTask());
    }
}

void SinkService_DisconnectAll(void)
{
    DEBUG_LOG_FN_ENTRY("SinkService_DisconnectAll");

    /* Request disconnection in all instances */
    FOR_EACH_SINK_SM(sm)
    {
        SinkServiceCommonSm_DisconnectRequest(sm);
    }
}

void SinkService_Disable(void)
{
    /*
     * Initiate the request to Disable the Sink Service, request to disconnect
     * from all devices. All instances will be disabled once they have disconnected.
     */
    SinkService_GetTaskData()->disable_request_pending = TRUE;
    SinkService_DisconnectAll();

    /* Disable any instances which are already Disconnected. */
    SinkServiceCommonSm_DisableAll();
}

void SinkService_Enable(void)
{
    SinkService_GetTaskData()->disable_request_pending = FALSE;
    SinkServiceCommonSm_EnableAll();
}

bool SinkService_IsPairingEnabled(void)
{
    return SinkService_GetTaskData()->pairing_enabled;
}

bool SinkService_IsEnabled(void)
{
    return !SinkService_GetTaskData()->disable_request_pending;
}

void SinkService_UpdateUi(void)
{
    Ui_InformContextChange(ui_provider_sink_service,
                           sinkService_GetCurrentContext());
}

bool SinkService_IsConnected(void)
{
    FOR_EACH_SINK_SM(sm)
    {
        if (sm->state == SINK_SERVICE_STATE_CONNECTED)
        {
            return TRUE;
        }
    }
    return FALSE;
}
