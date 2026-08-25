/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_profile.c
\brief  Implementation of the profile interface for Amazon AVS
*/

#ifdef INCLUDE_AMA
#include <profile_manager.h>
#include <logging.h>
#include <csrtypes.h>
#include <stdlib.h>
#include <panic.h>
#include <task_list.h>
#include <device_properties.h>

#include "ama_transport_profile.h"
#include "bt_device.h"

static void amaProfile_ProfileManagerMessageHandler(Task task, MessageId id, Message message);
static const TaskData profile_manager_task = { amaProfile_ProfileManagerMessageHandler };
static disconnect_callback_t disconnect_callback = NULL;
static bool is_disconnect_required = FALSE;

/*! List of tasks requiring confirmation of AMA disconnect requests */
static task_list_with_data_t disconnect_request_clients;

static bool amaProfile_IsDisconnectRequired(void)
{
    return is_disconnect_required;
}

static void amaProfile_DisconnectHandler(bdaddr* bd_addr)
{
    DEBUG_LOG("amaProfile_DisconnectHandler");
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddToNotifyList(TaskList_GetBaseTaskList(&disconnect_request_clients), device);
        if (amaProfile_IsDisconnectRequired())
        {
            PanicNull((void *)disconnect_callback);
            disconnect_callback();
        }
        else
        {
            DEBUG_LOG("amaProfile_DisconnectHandler: Already disconnected, send cfm to profile_manager");
            ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                              bd_addr, profile_manager_success,
                                              profile_manager_ama_profile,
                                              profile_manager_disconnect);
        }
    }
}

/* On Android, the Alexa app doesn't send a disconnect request when the disconnection is triggered
   from the BT device menu so we piggy back off the A2DP profile disconnect.
   The AMA assistant is useless without an A2DP profile active. */
static void amaProfile_HandleDisconnectedProfileInd(DISCONNECTED_PROFILE_IND_T *ind)
{
    if (ind->profile == DEVICE_PROFILE_A2DP)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(ind->device);

        if (appDeviceTypeIsHandset(&addr))
        {
            DEBUG_LOG("amaProfile_HandleDisconnectedProfileInd: a2dp with %04x %02x %06x", addr.nap, addr.uap, addr.lap);
            if (amaProfile_IsDisconnectRequired())
            {
                PanicNull((void *)disconnect_callback);
                disconnect_callback();
            }
        }
    }
}

static void amaProfile_ProfileManagerMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case DISCONNECTED_PROFILE_IND:
            amaProfile_HandleDisconnectedProfileInd((DISCONNECTED_PROFILE_IND_T *) message);
            break;

        default:
            break;
    }
}

void AmaTransport_ProfileInit(void)
{
    DEBUG_LOG("AmaProfile_Init");
    TaskList_WithDataInitialise(&disconnect_request_clients);
    ProfileManager_RegisterProfile(profile_manager_ama_profile, NULL, amaProfile_DisconnectHandler);
    ProfileManager_ClientRegister((Task) &profile_manager_task);
}

void AmaTransport_InternalSetProfileDisconnectRequired(bool disconnect_required)
{
    is_disconnect_required = disconnect_required;
}

void AmaTransport_RegisterProfileClient(disconnect_callback_t callback)
{
    disconnect_callback = callback;
}

void AmaTransport_SendProfileConnectedInd(const bdaddr * bd_addr)
{
    DEBUG_LOG("AmaProfile_SendConnectedInd");
    ProfileManager_GenericConnectedInd(profile_manager_ama_profile, bd_addr);
}

void AmaTransport_SendProfileDisconnectedInd(const bdaddr * bd_addr)
{
    DEBUG_LOG("AmaProfile_SendDisconnectedInd");
    if (TaskList_Size(TaskList_GetBaseTaskList(&disconnect_request_clients)) != 0)
    {
        ProfileManager_NotifyConfirmation(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                          bd_addr,
                                          profile_manager_success,
                                          profile_manager_ama_profile,
                                          profile_manager_disconnect);
    }
    ProfileManager_GenericDisconnectedInd(profile_manager_ama_profile, bd_addr, 0);
}

#endif /* INCLUDE_AMA */

