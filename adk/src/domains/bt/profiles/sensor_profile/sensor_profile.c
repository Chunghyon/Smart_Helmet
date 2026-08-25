/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Sensor Profile.
*/
#ifdef INCLUDE_SENSOR_PROFILE

#include "sensor_profile.h"
#include "sensor_profile_private.h"
#include "sensor_profile_peer_data_sync_l2cap.h"
#include "sensor_profile_signalling.h"
#include "sensor_profile_marshal_typedef.h"

#include <stdlib.h>
#include <logging.h>
#include <panic.h>

sensor_profile_task_data_t sensor_profile;

/*! \brief Main message Handler for the Sensor Profile.*/
static void sensorProfile_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
    /* Peer Signalling messages */
    case PEER_SIG_CONNECTION_IND:
        SensorProfile_HandlePeerSignallingConnectionInd((const PEER_SIG_CONNECTION_IND_T *)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
        SensorProfile_HandlePeerSignallingMessage(SensorProfile_Get()->reg_task,(const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message);
        break;

    case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
        SensorProfile_HandlePeerSignallingMessageTxConfirm((const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
        break;

    default:
        DEBUG_LOG_DEBUG("sensorProfile_MessageHandler: Unhandled id MESSAGE:sensor_profile_internal_msg_t:0x%x", id);
        break;
    }
}

bool SensorProfile_Init(Task init_task)
{
    memset(SensorProfile_Get(), 0, sizeof(*SensorProfile_Get()));
    SensorProfile_Get()->task_data.handler = sensorProfile_MessageHandler;

    /* Register a channel for peer signalling */
    appPeerSigMarshalledMsgChannelTaskRegister(SensorProfile_GetTask(),
                                            PEER_SIG_MSG_CHANNEL_SENSOR_PROFILE,
                                            sensor_profile_marshal_type_descriptors,
                                            NUMBER_OF_SENSOR_PROFILE_MARSHAL_TYPES);

    /* Register for peer signaling notifications */
    appPeerSigClientRegister(SensorProfile_GetTask());

    /* At the moment Sensor_profile initialisation is not really asynchronous
    so returning just TRUE should be sufficient. Sending SENSOR_PROFILE_INIT_CFM
    to #task here. */
    MessageSend(init_task, SENSOR_PROFILE_INIT_CFM, NULL);
    return TRUE;
}

bool SensorProfile_Register(Task client_task)
{
    SensorProfile_Get()->reg_task = client_task;
    return SensorProfile_L2capManagerRegister(client_task);
}

void SensorProfile_Connect(Task app_task,const bdaddr *peer_addr)
{
    DEBUG_LOG_INFO("SensorProfile_Connect");

    if(!SensorProfile_Get()->reg_task)
    {
        // No need to connect this profile as there is not registered client
        // We should probably never be here, but this could happen during manual testing
        // if SensorProfile_Register(reg_Task) has not been invoked.
        DEBUG_LOG_DEBUG("SensorProfile_Connect - There is not a registered client");
        return;
    }

    if(!peer_addr)
    {
        DEBUG_LOG_ERROR("SensorProfile_Connect - Peer address is NULL");
        Panic();
    }

    if (!SensorProfile_L2capConnect(app_task, peer_addr))
    {
        DEBUG_LOG_WARN("SensorProfile_L2capConnect failed or already connected!");
    }
}

void SensorProfile_Disconnect(Task app_task)
{
    DEBUG_LOG_INFO("SensorProfile_Disconnect");
    if(!SensorProfile_Get()->reg_task)
    {
        // No need to connect this profile as there is not registered client
        // We should probably never be here, but this could happen during manual testing
        // if SensorProfile_Register(reg_Task) has not been invoked.
        DEBUG_LOG_DEBUG("SensorProfile_Disconnect - There is not a registered client");
        return;
    }
    if (!SensorProfile_L2capDisconnect(app_task))
    {
        DEBUG_LOG_WARN("SensorProfile_L2cap_Disconnect failed or already disconnected!");
    }
}

bool SensorProfile_EnablePeer(uint16 interval,
                              sensor_profile_processing_source_t source,
                              uint16 tx_value)
{
    DEBUG_LOG_DEBUG("SensorProfile_EnablePeer");
    if (!SensorProfile_SendConfigurationToSecondary(TRUE, interval, source, tx_value))
    {
        DEBUG_LOG_WARN("SensorProfile_EnablePeer failed!");
        return FALSE;
    }
    return TRUE;
}

bool SensorProfile_DisablePeer(void)
{
    DEBUG_LOG_DEBUG("SensorProfile_DisablePeer");
    if (!SensorProfile_SendConfigurationToSecondary(FALSE, 0,
                                                    SENSOR_PROFILE_PROCESSING_INVALID,0 ))
    {
        DEBUG_LOG_WARN("SensorProfile_DisablePeer failed!");
        return FALSE;
    }
    return TRUE;
}

bool SensorProfile_PeerEnabled(uint16 sensor_interval,
                               sensor_profile_processing_source_t processing_src)
{
    DEBUG_LOG_DEBUG("SensorProfile_PeerEnabled");
    if (!SensorProfile_SendStatusToPrimary(TRUE, sensor_interval, processing_src))
    {
        DEBUG_LOG_WARN("SensorProfile_PeerEnabled failed!");
        return FALSE;
    }
    return TRUE;

}

bool SensorProfile_PeerDisabled(void)
{
    DEBUG_LOG_DEBUG("SensorProfile_PeerDisabled");
    if (!SensorProfile_SendStatusToPrimary(FALSE, 0, SENSOR_PROFILE_PROCESSING_INVALID))
    {
        DEBUG_LOG_WARN("SensorProfile_PeerDisabled failed!");
        return FALSE;
    }
    return TRUE;
}

void SensorProfile_SetRole(bool primary)
{
    SensorProfile_Get()->is_primary = primary;
}

bool SensorProfile_IsRolePrimary(void)
{
    return SensorProfile_Get()->is_primary;
}

#endif /* INCLUDE_SENSOR_PROFILE */
