    /*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Stereo Topology component core.
*/

#include "stereo_topology.h"
#include "stereo_topology_private.h"
#include "stereo_topology_config.h"
#include "stereo_topology_rules.h"
#include "stereo_topology_goals.h"
#include "stereo_topology_client_msgs.h"
#include "stereo_topology_private.h"
#include "stereo_topology_procedure_system_stop.h"


#include "stereo_topology_rules.h"

#include <logging.h>

#include <handset_service.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <power_manager.h>
#include <pairing.h>
#include <panic.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(stereo_topology_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(stereo_topology_internal_message_t)
LOGGING_PRESERVE_MESSAGE_ENUM(stereo_topology_goals)

/*! Instance of the Stereo Topology. */
stereo_topology_task_data_t stereo_topology = {0};

static void stereoTopology_HandlePairingActivity(const PAIRING_ACTIVITY_T *message)
{
    UNUSED(message);
    DEBUG_LOG_VERBOSE("stereoTopology_HandlePairingActivity status=enum:pairingActivityStatus:%d",
                        message->status);
}

/*! \brief Take action following power's indication of imminent shutdown.*/
static void stereoTopology_HandlePowerShutdownPrepareInd(void)
{
    stereo_topology_task_data_t *stereo_taskdata = StereoTopologyGetTaskData();

    DEBUG_LOG_VERBOSE("stereoTopology_HandlePowerShutdownPrepareInd");
    /* Stereo should stop being connectable during shutdown. */
    stereo_taskdata->shutdown_in_progress = TRUE;
    appPowerShutdownPrepareResponse(StereoTopologyGetTask());
}

/*! \brief Generate handset related disconnection events . */
static void stereoTopology_HandleHandsetServiceDisconnectedInd(const HANDSET_SERVICE_DISCONNECTED_IND_T* ind)
{
    DEBUG_LOG_VERBOSE("stereoTopology_HandleHandsetServiceDisconnectedInd %04x,%02x,%06lx status %u", ind->addr.nap,
                                                                                               ind->addr.uap,
                                                                                               ind->addr.lap,
                                                                                               ind->status);

    if(ind->status == handset_service_status_link_loss)
    {
        StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_HANDSET_LINKLOSS);
    }
}

/*! \brief Print bluetooth address of the handset. */
static void stereoTopology_PrintBdaddr(const bdaddr addr)
{
    DEBUG_LOG_VERBOSE("stereoTopology_printbdaddr %04x,%02x,%06lx", addr.nap,
                                                                     addr.uap,
                                                                     addr.lap);
}

static void stereoTopology_HandleStopTimeout(void)
{
    DEBUG_LOG_FN_ENTRY("stereoTopology_HandleStopTimeout");

    StereoTopology_SendStopCfm(stereo_topology_status_fail);
    StereoTopology_SetState(STEREO_TOPOLOGY_STATE_STOPPED);
}

static void stereoTopology_HandleStopCompletion(void)
{
    if (StereoTopology_IsStateStopping())
    {
        DEBUG_LOG_FN_ENTRY("stereoTopology_HandleStopCompletion");

        /* Send the stop message before clearing the app task below */
        StereoTopology_SendStopCfm(stereo_topology_status_success);
        StereoTopology_SetState(STEREO_TOPOLOGY_STATE_STOPPED);
    }
}


/*! \brief Stereo Topology message handler.
 */
static void stereoTopology_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_VERBOSE("stereoTopology_HandleMessage. message id MESSAGE:stereo_topology_internal_message_t:0x%x",id);

    /* handle messages */
    switch (id)
    {
        case PAIRING_ACTIVITY:
            stereoTopology_HandlePairingActivity(message);
            break;

        case HANDSET_SERVICE_CONNECTED_IND:
            {
                HANDSET_SERVICE_CONNECTED_IND_T *ind = (HANDSET_SERVICE_CONNECTED_IND_T *)message;
                DEBUG_LOG_INFO("stereoTopology_HandleMessage: HANDSET_SERVICE_CONNECTED_IND profiles_connected = %d",
                              ind->profiles_connected);
                stereoTopology_PrintBdaddr(ind->addr);
            }
            break;

        case HANDSET_SERVICE_DISCONNECTED_IND:
            DEBUG_LOG_DEBUG("stereoTopology_HandleMessage: HANDSET_SERVICE_DISCONNECTED_IND");
            stereoTopology_HandleHandsetServiceDisconnectedInd((HANDSET_SERVICE_DISCONNECTED_IND_T*)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            {
                CON_MANAGER_CONNECTION_IND_T *ind = (CON_MANAGER_CONNECTION_IND_T *)message;
                DEBUG_LOG_DEBUG("stereoTopology_HandleMessage: CON_MANAGER_CONNECTION_IND Connected = %d, Transport BLE = %d",
                                 ind->connected, ind->ble);
                stereoTopology_PrintBdaddr(ind->bd_addr);
            }
            break;

        /* Power indications */
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            stereoTopology_HandlePowerShutdownPrepareInd();
            DEBUG_LOG_VERBOSE("stereoTopology_HandleMessage: APP_POWER_SHUTDOWN_PREPARE_IND");
            break;

        case STEREOTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP:
            stereoTopology_HandleStopTimeout();
            break;

        case PROC_SEND_STEREO_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED:
            stereoTopology_HandleStopCompletion();
            break;

        default:
            DEBUG_LOG_VERBOSE("stereoTopology_HandleMessage: Unhandled message MESSAGE:stereo_topology_internal_message_t:0x%x", id);
            break;
      }
}


bool StereoTopology_Init(Task init_task)
{
    UNUSED(init_task);

    stereo_topology_task_data_t *stereotop_taskdata = StereoTopologyGetTaskData();
    stereotop_taskdata->task.handler = stereoTopology_HandleMessage;
    stereotop_taskdata->goal_task.handler = StereoTopology_HandleGoalDecision;
    stereotop_taskdata->prohibit_connect_to_handset = FALSE;
    stereotop_taskdata->shutdown_in_progress = FALSE;
    StereoTopology_SetState(STEREO_TOPOLOGY_STATE_STOPPED);

    /*Initialize Stereo topology's goals and rules */
    
    StereoTopologyRules_Init(StereoTopologyGetGoalTask());
    StereoTopology_GoalsInit();

    /* Register with power to receive shutdown messages. */
    appPowerClientRegister(StereoTopologyGetTask());
    /* Allow topology to sleep */
    appPowerClientAllowSleep(StereoTopologyGetTask());

    /* register with handset service as we need disconnect and connect notification */
    HandsetService_ClientRegister(StereoTopologyGetTask());
    ConManagerRegisterConnectionsClient(StereoTopologyGetTask());
    Pairing_ActivityClientRegister(StereoTopologyGetTask());
    BredrScanManager_PageScanParametersRegister(&stereo_page_scan_params);
    BredrScanManager_InquiryScanParametersRegister(&stereo_inquiry_scan_params);

    TaskList_InitialiseWithCapacity(StereoTopologyGetMessageClientTasks(), MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY);

    return TRUE;
}


bool StereoTopology_Start(Task requesting_task)
{
    UNUSED(requesting_task);

    if (StereoTopology_IsStateStopped())
    {
        DEBUG_LOG("StereoTopology_Starting (normal start)");
        StereoTopology_SetState(STEREO_TOPOLOGY_STATE_STARTING);
    }
    else
    {
        DEBUG_LOG("StereoTopology_Start:Topology already started or is in process of stopping,topology state MESSAGE:stereo_topology_sm_t:0x%x", StereoTopology_GetState());
    }

    return TRUE;
}


void StereoTopology_RegisterMessageClient(Task client_task)
{
   TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(StereoTopologyGetMessageClientTasks()), client_task);
}


void StereoTopology_UnRegisterMessageClient(Task client_task)
{
   TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(StereoTopologyGetMessageClientTasks()), client_task);
}


void StereoTopology_ProhibitHandsetConnection(bool prohibit)
{
    StereoTopologyGetTaskData()->prohibit_connect_to_handset = prohibit;

    if(prohibit)
    {
        StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET);
    }
    return;
}

bool StereoTopology_Stop(Task requesting_task)
{
    stereo_topology_task_data_t *stereo_top = StereoTopologyGetTaskData();
    stereo_top->app_task = requesting_task;

    DEBUG_LOG_WARN("StereoTopology_Stop - topology state:0x%x", StereoTopology_GetState());

    if(StereoTopology_IsStateStarted())
    {
        StereoTopology_SetState(STEREO_TOPOLOGY_STATE_STOPPING);
    }
    else
    {
        if(StereoTopology_IsStateStopped())
        {
            DEBUG_LOG_WARN("StereoTopology_Stop - already stopped");
            StereoTopology_SendStopCfm(stereo_topology_status_success);
        }
        else
        {
            DEBUG_LOG("StereoTopology_Stop -- already stopping");
        }
    }

    return TRUE;
}

bool stereoTopology_IsRunning(void)
{
    stereo_topology_task_data_t *stereotop = StereoTopologyGetTaskData();

    return stereotop->app_task && (!StereoTopology_IsStateStopped());
}

void StereoTopology_ConnectMruHandset(void)
{
    DEBUG_LOG("StereoTopology_ConnectMruHandset");
    StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_USER_REQUEST_CONNECT_HANDSET);
}

void StereoTopology_DisconnectLruHandset(void)
{
    DEBUG_LOG("StereoTopology_DisconnectLruHandset");
    StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_LRU_HANDSET);
}

void StereoTopology_DisconnectAllHandsets(void)
{
    DEBUG_LOG("StereoTopology_DisconnectAllHandsets");
    StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_USER_REQUEST_DISCONNECT_ALL_HANDSETS);
}

/*! On State Exit Funcitons */

static void stereoTopologyExitStoppedState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyExitStoppedState");
}

static void stereoTopologyExitStoppingState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyExitStoppingState");
}

static void stereoTopologyExitStartedState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyExitStartedState");
}

static void stereoTopologyExitStartingState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyExitStartingState");
}

/*! On State Enter Funcitons */

static void stereoTopologyEnterStoppedState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyEnterStoppedState : STEREO_TOPOLOGY_STATE_STOPPED");

    stereo_topology_task_data_t *stereotop_taskdata = StereoTopologyGetTaskData();
    stereotop_taskdata->app_task = NULL;
}

static void stereoTopologyEnterStoppingState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyEnterStoppingState : STEREO_TOPOLOGY_STATE_STOPPING");

    uint32 timeout_ms = D_SEC(StereoTopologyConfig_StereoTopologyStopTimeoutS());
    DEBUG_LOG_DEBUG("StereoTopology_Stop(). Timeout:%u", timeout_ms);

    if (timeout_ms)
    {
        MessageSendLater(StereoTopologyGetTask(),
                         STEREOTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP, NULL,
                         timeout_ms);
    }
    StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_STOP);
}

static void stereoTopologyEnterStartedState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyEnterStartedState : STEREO_TOPOLOGY_STATE_STARTED");
}

static void stereoTopologyEnterStartingState(void)
{
    DEBUG_LOG_DEBUG("stereoTopologyEnterStartingState : STEREO_TOPOLOGY_STATE_STARTING");

    stereo_topology_task_data_t *stereotop_taskdata = StereoTopologyGetTaskData();
    stereotop_taskdata->prohibit_connect_to_handset = FALSE;
    stereotop_taskdata->shutdown_in_progress = FALSE;
    StereoTopologyRules_ResetEvent(RULE_EVENT_ALL_EVENTS_MASK);
    /* Set the rule to get the stereo rolling (EnableConnectable, AllowHandsetConnect) */
    StereoTopologyRules_SetEvent(STEREOTOP_RULE_EVENT_START);
}

stereo_topology_sm_t StereoTopology_GetState(void)
{
    return StereoTopologyGetTaskData()->stereo_topology_state;
}

void StereoTopology_SetState(stereo_topology_sm_t new_state)
{
    stereo_topology_task_data_t* sm = StereoTopologyGetTaskData();
    stereo_topology_sm_t previous_state = StereoTopologyGetTaskData()->stereo_topology_state;

    DEBUG_LOG_STATE("StereoTopology_SetState, state 0x%02x to 0x%02x", previous_state, new_state);

    /* Handle state exit functions */
    switch (previous_state)
    {
        case STEREO_TOPOLOGY_STATE_STOPPED:
            stereoTopologyExitStoppedState();
            break;

        case STEREO_TOPOLOGY_STATE_STOPPING:
            stereoTopologyExitStoppingState();
            break;

        case STEREO_TOPOLOGY_STATE_STARTED:
            stereoTopologyExitStartedState();
            break;

        case STEREO_TOPOLOGY_STATE_STARTING:
            stereoTopologyExitStartingState();
            break;

        default:
            DEBUG_LOG_ERROR("Attempted to exit unsupported state 0x%02x", StereoTopologyGetTaskData()->stereo_topology_state);
            Panic();
            break;
    }
    /* Set new state */
    StereoTopologyGetTaskData()->stereo_topology_state = new_state;
    /* Handle state entry functions */
    switch (new_state)
    {
        case STEREO_TOPOLOGY_STATE_STOPPED:
            stereoTopologyEnterStoppedState();
            break;

        case STEREO_TOPOLOGY_STATE_STOPPING:
            stereoTopologyEnterStoppingState();
            break;

        case STEREO_TOPOLOGY_STATE_STARTED:
            stereoTopologyEnterStartedState();
            break;

        case STEREO_TOPOLOGY_STATE_STARTING:
            stereoTopologyEnterStartingState();
            break;

        default:
            DEBUG_LOG_ERROR("Attempted to enter unsupported state 0x%02x", new_state);
            Panic();
            break;
    }

    DEBUG_LOG_VERBOSE("StereoTopology_SetState, new state 0x%02x", sm->stereo_topology_state);
}
