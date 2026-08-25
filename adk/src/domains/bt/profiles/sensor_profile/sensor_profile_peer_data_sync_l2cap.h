/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       sensor_profile_peer_data_sync_l2cap.h
\defgroup   sensor_profile Sensor Profile
\ingroup    profiles
\brief      Sensor Profile L2cap Channel creation for sensor data synchronisation
*/

#ifdef INCLUDE_SENSOR_PROFILE

#ifndef SENSOR_PROFILE_PEER_DATA_SYNC_L2CAP_H_
#define SENSOR_PROFILE_PEER_DATA_SYNC_L2CAP_H_

#include <message_.h>
#include <bdaddr_.h>

/*! \brief Called within Sensor Profile when the application registers its task.

    \param client_task The task of the application/module using this profile.

    \return Success or failure as a boolean.
*/
bool SensorProfile_L2capManagerRegister(Task client_task);

/*! \brief Called within Sensor Profile upon a connecting request to establish an L2CAP link.

    A SENSOR_PROFILE_CONNECTED message will be sent to the registered application
    task to inform about the outcome of this request.

    \param app_task Task to send a connect confirmation message back.
    \param peer_addr Secondary device Bluetooth address.

    \return Success or failure as a boolean.
*/
bool SensorProfile_L2capConnect(Task app_task, const bdaddr *peer_addr);

/*! \brief Called within Sensor Profile upon a disconnecting request to tear down the L2CAP link.

    A SENSOR_PROFILE_DISCONNECTED message will be sent to the registered application
    task to inform about the outcome of this request.

    \param app_task Task to send a disconnect confirmation message back.

    \return Success or failure as a boolean.
*/
bool SensorProfile_L2capDisconnect(Task app_task);

#endif /* SENSOR_PROFILE_PEER_DATA_SYNC_L2CAP_H_ */

#endif /* INCLUDE_SENSOR_PROFILE */
