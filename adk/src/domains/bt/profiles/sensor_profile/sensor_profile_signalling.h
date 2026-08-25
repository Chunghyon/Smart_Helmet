/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sensor Profile channel for sending signalling messages between Primary & Secondary.
*/
#ifdef INCLUDE_SENSOR_PROFILE

#ifndef SENSOR_PROFILE_SIGNALLING_H_
#define SENSOR_PROFILE_SIGNALLING_H_

#include "peer_signalling.h"
#include "sensor_profile.h"

#include <message_.h>

/*! \brief Send spatial audio configuration from the Primary earbud to the peer Secondary earbud.

    Called by the Primary earbud to forward Enabling/Disabling sensor data
    synchronization commands to the Secondary earbud with the required configuration.

    \param enable Sensor streaming enable command sent to Secondary peer.
                  TRUE if peer is to be Enabled, FALSE if it is to be Disabled.
    \param sensor_interval Sensor interval used to configure sensor application on enabling (in hz).
                           Not relevant on disabling.
    \param processing_source Sensor data processing source on enabling.
                             Not relevant on disabling.
    \param report_id Actual transmition value to remote device on enabling.
                     Not relevant on disabling.

    \return TRUE if sent successfully; FALSE if it failed.
*/
bool SensorProfile_SendConfigurationToSecondary(bool enable,
                                                uint16 sensor_interval,
                                                sensor_profile_processing_source_t processing_source,
                                                uint16 report_id);

/*! \brief Send status of spatial audio configuration from the peer Secondary earbud to the Primary.
 *
    Called by the Secondary earbud to forward status updates to the Primary earbud

    \param status Whether or not sensor streaming is enabled in the peer Secondary earbud.
    \param sensor_interval Sensor interval used to configure sensor application on Enabling (in hz).
                           Not relevant if status is FALSE.
    \param processing_source Sensor data processing source on Enabling.
                             Not relevant if status is FALSE.

    \return TRUE if sent successfully; FALSE if it failed.
*/
bool SensorProfile_SendStatusToPrimary(bool status,
                                       uint16 sensor_interval,
                                       sensor_profile_processing_source_t source);

/*! \brief Handle PEER_SIG_CONNECTION_IND

    Called in both Primary and Secondary earbud when the peer signalling SensorProfile
    channel is connected or disconnected.

    \param ind The indication of connection/disconnection of the SensorProfile peer signalling channel.
*/
void SensorProfile_HandlePeerSignallingConnectionInd(const PEER_SIG_CONNECTION_IND_T *ind);

/*! \brief Handle PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND

    Called in Secondary earbud to deliver signalling information sent from Primary
    earbud, i.e. Sensor enable/disable control messages.

    \param appTask This is the appTask for the client registered with the SensorProfile.
    \param ind The indication of incoming marshalled message via the peer signalling channel.

*/
void SensorProfile_HandlePeerSignallingMessage(Task appTask,
                                               const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind);

/*! \brief Handle PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM

    Called in Primary to confirm the sent message was acknowledged by the Secondary peer.

    \param cfm The confirmation of the marshalled message via the peer signalling channel.
*/
void SensorProfile_HandlePeerSignallingMessageTxConfirm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm);

#endif /* SENSOR_PROFILE_SIGNALLING_H_ */

#endif /* INCLUDE_SENSOR_PROFILE */
