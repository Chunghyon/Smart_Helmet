/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to TWS Topology utility functions for sending messages to clients.
*/

#ifndef TWS_TOPOLOGY_CLIENT_MSGS_H
#define TWS_TOPOLOGY_CLIENT_MSGS_H

#include "tws_topology.h"

/*! \brief Send indication to registered clients of the new Earbud role.
    \param[in] role New Earbud role.
*/
void TwsTopology_SendRoleChangedInd(tws_topology_role role);

#endif /* TWS_TOPOLOGY_CLIENT_MSGS_H */
