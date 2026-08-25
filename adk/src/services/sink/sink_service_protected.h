/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Sink service types and data to be used within sink_service only
*/

#ifndef SINK_SERVICE_PROTECTED_H
#define SINK_SERVICE_PROTECTED_H

#include "sink_service.h"

#include <device.h>
#include <task_list.h>

/*! \brief Send a SINK_SERVICE_CONNECTED_CFM to registered clients.

    \param device Device that represents the sink
*/
void SinkService_SendConnectedCfm(device_t device, sink_service_status_t status);

/*! \brief Send a SINK_SERVICE_DISCONNECTED_CFM to registered clients.

    \param device Device that represents the sink
*/
void SinkService_SendDisconnectedCfm(device_t device);

/*! \brief Send a SINK_SERVICE_FIRST_PROFILE_CONNECTED_IND to registered clients.

    \param device Device that represents the sink
*/
void SinkService_SendFirstProfileConnectedIndNotification(device_t device);

/*! \brief Checks whether pairing is enabled for the sink service.
 *  \note By default pairing is enabled.

    \return TRUE if the sink service pairing was enabled
*/
bool SinkService_IsPairingEnabled(void);

/*! \brief Checks whether the sink service is enabled
 *  \note By default the think sink service is enabled.

    \return TRUE if the sink service is enabled.
*/
bool SinkService_IsEnabled(void);

/*! \brief Inform the sink service context provider that it should
 *         update its context.
 *  \note  This is normally triggered by a state change within a sink service instance.
*/
void SinkService_UpdateUi(void);

#endif /* SINK_SERVICE_PROTECTED_H */
