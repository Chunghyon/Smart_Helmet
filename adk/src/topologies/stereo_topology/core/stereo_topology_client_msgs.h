/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to STEREO Topology utility functions for sending messages to clients.
*/

#ifndef STEREO_TOPOLOGY_CLIENT_MSGS_H
#define STEREO_TOPOLOGY_CLIENT_MSGS_H
#include <handset_service.h>
#include "stereo_topology.h"

/*! \brief Send indication to registered clients that handset disconnected goal has been reached.
    \param[in] Handset service disconnect status message.
*/
void StereoTopology_SendHandsetDisconnectedIndication(const handset_service_status_t status);


/*! \brief Send confirmation message to the task which called #StereoTopology_Stop().
    \param[in] Status Status of the start operation.

    \note It is expected that the task will be the application SM task.
*/
void StereoTopology_SendStopCfm(stereo_topology_status_t status);

#endif /* STEREO_TOPOLOGY_CLIENT_MSGS_H */
