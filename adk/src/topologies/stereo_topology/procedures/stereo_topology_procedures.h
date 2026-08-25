/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Definition of STEREO topology specific procedures.
*/

#ifndef STEREO_TOPOLOGY_PROCEDURES_H
#define STEREO_TOPOLOGY_PROCEDURES_H

#include "procedures.h"


/*! Definition of STEREO Topology procedures. 

    A naming convention is followed for important procedures. 
    Following the convention and using in the recommended order
    reduces the possibility of unexpected behaviour.

    The convention applies to functions that enable or disable 
    activity.
    \li allow changes the response if an event happens.
    \li permit starts or stops an activity temporarily
    \li enable Starts or stops an activity permanently

    If several of these functions are called with DISABLE parameters
    it is recommended that they are called in the order 
    allow - permit - enable.
*/

typedef enum
{
    stereo_topology_procedure_enable_connectable_handset = 1,

    stereo_topology_procedure_allow_handset_connection,

    stereo_topology_procedure_connect_handset,

    stereo_topology_procedure_disconnect_handset,

    stereo_topology_procedure_allow_le_connection,

    stereo_topology_procedure_system_stop,

    stereo_topology_procedure_start_stop_script,

    stereo_topology_procedure_send_message_to_topology,

    stereo_topology_procedure_disconnect_le,

    stereo_topology_procedure_disconnect_lru_handset,
} stereo_topology_procedure;


#endif /* STEREO_TOPOLOGY_PROCEDURES_H */
