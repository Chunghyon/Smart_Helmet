/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sensor profile channel for sending signalling messages between Primary & Secondary.
*/

#ifdef INCLUDE_SENSOR_PROFILE

#include "sensor_profile_signalling.h"
#include "sensor_profile_private.h"
#include "sensor_profile_typedef.h"
#include "sensor_profile_marshal_typedef.h"

#include <panic.h>
#include <logging.h>
#include <message.h>

#define peerSigTx(message, type) appPeerSigMarshalledMsgChannelTx(\
    SensorProfile_GetTask(), \
    PEER_SIG_MSG_CHANNEL_SENSOR_PROFILE, \
    (message), MARSHAL_TYPE(type))


/*! \brief Handler for receiving configuration peer signalling messages in the sensor_profile channel
           from Primary to Secondary.
*/
static void sensorProfile_HandleConfigureInd(Task app_task, const sensor_profile_control_peer_ind_t* msg)
{
    DEBUG_LOG_DEBUG("sensorProfile_HandleConfigureInd, "
                    "Enable %d, Sensor interval: %d; Processing source: %d; report_id: %d",
                    msg->enable_peer, msg->sensor_interval, msg->processing_source,
                    msg->report_id);

    if(app_task != NULL)
    {
        if (msg->enable_peer)
        {
            MESSAGE_MAKE(message, SENSOR_PROFILE_ENABLE_PEER_REQ_T);
            message->sensor_interval = msg->sensor_interval;
            message->processing_source = msg->processing_source;
            message->report_id = msg->report_id;
            MessageSend(app_task, SENSOR_PROFILE_ENABLE_PEER_REQ, message);
        }
        else
        {
            MessageSend(app_task, SENSOR_PROFILE_DISABLE_PEER_REQ, NULL);
        }

    }
}

/*! \brief Handler for receiving peer signalling messages in the sensor_profile channel
           for status confirmation from Secondary to Primary.
*/
static void sensorProfile_HandleStatusCfm(Task app_task, const sensor_profile_peer_status_cfm_t* msg)
{
    DEBUG_LOG_DEBUG("sensorProfile_HandleStatusCfm, "
                    "Enable %d, Sensor interval: %d; Processing source: %d ",
                    msg->peer_enabled, msg->sensor_interval, msg->processing_source);

    if(app_task != NULL)
    {
        if (msg->peer_enabled)
        {
            MESSAGE_MAKE(message, SENSOR_PROFILE_PEER_ENABLED_T);
            message->sensor_interval = msg->sensor_interval;
            message->processing_source = msg->processing_source;
            MessageSend(app_task, SENSOR_PROFILE_PEER_ENABLED, message);
        }
        else
        {
            MessageSend(app_task, SENSOR_PROFILE_PEER_DISABLED, NULL);
        }
    }
}

/* Send configuration Sensor Profile from the Primary earbud to the Secondary.*/
bool SensorProfile_SendConfigurationToSecondary(bool enable,
                                                uint16 sensor_interval,
                                                sensor_profile_processing_source_t source,
                                                uint16 report_id)
{
    DEBUG_LOG_DEBUG("SensorProfile_SendConfigurationToSecondary");
    if (SensorProfile_IsRolePrimary())
    {
        sensor_profile_control_peer_ind_t* msg = PanicUnlessMalloc(sizeof(*msg));
        msg->enable_peer = enable;
        msg->sensor_interval = sensor_interval;
        msg->processing_source = source;
        msg->report_id = report_id;
        peerSigTx(msg, sensor_profile_control_peer_ind_t);
        return TRUE;
    }
    return FALSE;
}

/* Send status from the Primary earbud to the Secondary.*/
bool SensorProfile_SendStatusToPrimary(bool enable,
                                       uint16 sensor_interval,
                                       sensor_profile_processing_source_t source)
{
    DEBUG_LOG_DEBUG("SensorProfile_SendStatusToPrimary");
    if (!SensorProfile_IsRolePrimary())
    {
        sensor_profile_peer_status_cfm_t* msg = PanicUnlessMalloc(sizeof(*msg));
        msg->peer_enabled = enable;
        msg->sensor_interval = sensor_interval;
        msg->processing_source = source;
        peerSigTx(msg, sensor_profile_peer_status_cfm_t);
        return TRUE;
    }
    return FALSE;
}

/* Handles PEER_SIG_CONNECTION_IND */
void SensorProfile_HandlePeerSignallingConnectionInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    if (ind->status != peerSigStatusConnected)
    {
        DEBUG_LOG_DEBUG("sensorProfile_HandlePeerSignallingConnectionInd - disconnected %d", ind->status);
    }
    else
    {
        DEBUG_LOG_DEBUG("sensorProfile_HandlePeerSignallingConnectionInd - Peer signalling Connected");
    }
}

/* Handles PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND */
void SensorProfile_HandlePeerSignallingMessage(Task app_task,
                                               const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind)
{
    DEBUG_LOG_DEBUG("SensorProfile_HandlePeerSignallingMessage. Channel 0x%x, type %d",
                    ind->channel, ind->type);
    switch (ind->type)
    {
    case MARSHAL_TYPE(sensor_profile_control_peer_ind_t):
        {
            sensorProfile_HandleConfigureInd(app_task, (const sensor_profile_control_peer_ind_t*)ind->msg);
        }
        break;
    case MARSHAL_TYPE(sensor_profile_peer_status_cfm_t):
        {
            sensorProfile_HandleStatusCfm(app_task, (const sensor_profile_peer_status_cfm_t*)ind->msg);
        }
        break;
    default:
        DEBUG_LOG_DEBUG("SensorProfile_HandlePeerSignallingMessage unhandled type 0x%x", ind->type);
        break;
    }

    free(ind->msg);
}

/* Handles PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM */
void SensorProfile_HandlePeerSignallingMessageTxConfirm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm)
{
    DEBUG_LOG_DEBUG("SensorProfile_HandlePeerSignallingMessageTxConfirm. Channel 0x%x, status %d, type %d",
                    cfm->channel, cfm->status, cfm->type);
    if (cfm->status != peerSigStatusSuccess)
    {
        DEBUG_LOG_WARN("SensorProfile_HandlePeerSignallingMessageTxConfirm - There is some problem "
                       "queueing the signalling message!");
    }
}

#endif /* INCLUDE_SENSOR_PROFILE */
