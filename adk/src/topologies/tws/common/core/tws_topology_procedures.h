/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Definition of TWS topology procedures.
*/

#ifndef TWS_TOPOLOGY_PROCEDURES_H
#define TWS_TOPOLOGY_PROCEDURES_H

#include "procedures.h"


/*! Definition of TWS Topology procedures. 

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
    /*! Procedure to pair with peer Earbud. */
    tws_topology_procedure_pair_peer = 1,

    /*! Procedure to determine Primary/Secondary role of Earbud. */
    tws_topology_procedure_find_role,

    /*! Procedure to connect BREDR ACL to Primary Earbud. */
    tws_topology_procedure_sec_connect_peer,

    /*! Procedure to connect BREDR profiles to Secondary Earbud. */
    tws_topology_procedure_pri_connect_peer_profiles,

    /*! Procedure to enable page scan for Secondary to establish BREDR ACL to Primary Earbud. */
    tws_topology_procedure_enable_connectable_peer,

    /*! Procedure to become idle */
    tws_topology_procedure_no_role_idle,

    /*! Procedure to disconnect all handsets */
    tws_topology_procedure_disconnect_handset,

    /*! Procedure to allow devices other than the peer to connect over BR/EDR */
    tws_topology_procedure_enable_connectable_handset,

    /*! Permit BT activities */
    tws_topology_procedure_permit_bt,

    /*! Set the Bluetooth address of the earbud */
    tws_topology_procedure_set_address,

    /*! Become the primary earbud */
    tws_topology_procedure_become_primary,

    /*! Become the secondary earbud */
    tws_topology_procedure_become_secondary,

    /*! Become the acting-primary earbud */
    tws_topology_procedure_become_acting_primary,

    /*! Cancel attempt to find earbud role */
    tws_topology_procedure_cancel_find_role,

    /*! Allow LE connections */
    tws_topology_procedure_allow_connection_over_le,

    /*! Allow BR/EDR connections */
    tws_topology_procedure_allow_connection_over_bredr,

    /*! Pair with the peer earbud */
    tws_topology_procedure_pair_peer_script,

    /*! Allow handsets to connect to the earbud */
    tws_topology_procedure_allow_handset_connection,

    /*! Disconnect all inks */
    tws_topology_procedure_clean_connections,

    /*! Procedure to release the lock on ACL to the peer and
        (potentially) start closing the connection */
    tws_topology_procedure_release_peer,

    /*! Procedure for preparing for dynamic handover */
    tws_topology_procedure_dynamic_handover_prepare,

    /*! Procedure for performing dynamic handover  */
    tws_topology_procedure_dynamic_handover,

    /*! Procedure for un-doing preparations for dynamic */
    tws_topology_procedure_dynamic_handover_undo_prepare,

    /*! Enabling LE connectable */
    tws_topology_procedure_enable_le_connectable_handset,

    /*! Disconnect all LE connections */
    tws_topology_procedure_disconnect_le_connections,

    /*! Noify role change clients of pending role change */
    tws_topology_procedure_notify_role_change_clients,

    /*! Procedure that sends a message to the topology task */
    tws_topology_procedure_send_message_to_topology,

    /*! Procedure to stop LE broadcast */
    tws_topology_proc_stop_le_broadcast,

    /*! Procedure to stop handset reconnection */
    tws_topology_procedure_stop_handset_reconnect,

} tws_topology_procedure;


#endif /* TWS_TOPOLOGY_PROCEDURES_H */
