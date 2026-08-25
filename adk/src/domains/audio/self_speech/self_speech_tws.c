/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     Only primary EB will support the Self speech detection,  provides TWS support for Self Speech use cases to identify the primary
*/

#ifdef ENABLE_SELF_SPEECH

#include "self_speech_tws.h"
#include "self_speech.h"
#include "bt_device.h"
#include "tws_topology_role_change_client_if.h"

#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stdio.h>

typedef struct
{
    Task serverTask;
    int32 min_reconnection_delay;
    tws_topology_role current_role;
} self_speech_tws_data_t;

static self_speech_tws_data_t self_speech_tws_data =
{
    .serverTask = NULL,
    .min_reconnection_delay = D_SEC(2),
    .current_role = tws_topology_role_none
};

static void selfSpeechTws_SendRoleChangePrepareResponse(void)
{
    DEBUG_LOG("selfSpeechTws_SendRoleChangePrepareResponse");
    MessageSend(PanicNull(self_speech_tws_data.serverTask), TWS_ROLE_CHANGE_PREPARATION_CFM, NULL);
}

static void selfSpeechTws_Enable(void)
{
    DEBUG_LOG("selfSpeechTws_Enable current_role=enum:tws_topology_role:%d", self_speech_tws_data.current_role);
    if(self_speech_tws_data.current_role == tws_topology_role_primary)
    {
        Self_Speech_Enable();
    }
}

static void selfSpeechTws_Disable(void)
{
    DEBUG_LOG("selfSpeechTws_Disable current_role=enum:tws_topology_role:%d", self_speech_tws_data.current_role);
    Self_Speech_Disable();
}

static void selfSpeechTws_SendRoleChangeRequestResponse(void)
{
    DEBUG_LOG("selfSpeechTws_SendRoleChangeRequestResponse");
    MAKE_TWS_ROLE_CHANGE_ACCEPTANCE_MESSAGE(TWS_ROLE_CHANGE_ACCEPTANCE_CFM);
    message->role_change_accepted = TRUE;

    MessageSend(PanicNull(self_speech_tws_data.serverTask), TWS_ROLE_CHANGE_ACCEPTANCE_CFM, message);
}

static void selfSpeechTws_Initialise(Task server, int32_t reconnect_delay)
{
    DEBUG_LOG("selfSpeechTws_Initialise server, reconnect delay =%d",reconnect_delay);
    self_speech_tws_data.min_reconnection_delay = reconnect_delay;
    self_speech_tws_data.serverTask = server;
}

static void selfSpeechTws_RoleChangeIndication(tws_topology_role new_role)
{
    tws_topology_role old_role = self_speech_tws_data.current_role;
    self_speech_tws_data.current_role = new_role;
    
    DEBUG_LOG("selfSpeechTws_RoleChangeIndication role=enum:tws_topology_role:%d", new_role);

    if(old_role == tws_topology_role_primary && new_role != tws_topology_role_primary)
    {
        selfSpeechTws_Disable();
    }
    else if(old_role != tws_topology_role_primary && new_role == tws_topology_role_primary)
    {
        selfSpeechTws_Enable();
    }
}

static void selfSpeechTws_ProposeRoleChange(void)
{
    DEBUG_LOG("selfSpeechTws_ProposeRoleChange");
    selfSpeechTws_SendRoleChangeRequestResponse();
}

static void selfSpeechTws_ForceRoleChange(void)
{
    DEBUG_LOG("selfSpeechTws_ForceRoleChange");
}

static void selfSpeechTws_PrepareRoleChange(void)
{
    DEBUG_LOG("selfSpeechTws_PrepareRoleChange");
    selfSpeechTws_SendRoleChangePrepareResponse();
}

static void selfSpeechTws_CancelRoleChange(void)
{
    DEBUG_LOG("selfSpeechTws_CancelRoleChange");
}

bool SelfSpeechTws_IsPrimary(void)
{
    return (self_speech_tws_data.current_role == tws_topology_role_primary);
}

TWS_ROLE_CHANGE_CLIENT_REGISTRATION_MAKE(selfSpeechTws,
                                         selfSpeechTws_Initialise,
                                         selfSpeechTws_RoleChangeIndication,
                                         selfSpeechTws_ProposeRoleChange,
                                         selfSpeechTws_ForceRoleChange,
                                         selfSpeechTws_PrepareRoleChange,
                                         selfSpeechTws_CancelRoleChange);

#endif /*ENABLE_SELF_SPEECH*/

