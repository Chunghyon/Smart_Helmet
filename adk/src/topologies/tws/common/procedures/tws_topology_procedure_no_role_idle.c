/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#include "script_engine.h"

#include "tws_topology_procedure_allow_handset_connect.h"
#include "tws_topology_procedure_enable_connectable_handset.h"
#include "tws_topology_procedure_enable_connectable_peer.h"
#include "tws_topology_procedure_disconnect_handset.h"
#include "tws_topology_procedure_release_peer.h"
#include "tws_topology_procedure_cancel_find_role.h"
#include "tws_topology_procedure_set_address.h"
#include "tws_topology_procedure_permit_bt.h"
#include "tws_topology_procedure_allow_connection_over_le.h"
#include "tws_topology_procedure_allow_connection_over_bredr.h"
#include "tws_topology_procedure_clean_connections.h"
#include "tws_topology_procedure_enable_le_connectable_handset.h"
#include "tws_topology_procedure_stop_le_broadcast.h"
#include "tws_topology_procedure_stop_handset_reconnect.h"

#include <logging.h>

#ifdef LE_ADVERTISING_MANAGER_NEW_API
#define PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)
#else
#define PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)   ENTRY(proc_enable_le_connectable_handset_fns, PROC_ENABLE_LE_CONNECTABLE_PARAMS_DISABLE),
#endif


#define NO_ROLE_IDLE_SCRIPT(ENTRY) \
    ENTRY(proc_stop_handset_reconnect_fns, NO_DATA), \
    ENTRY(proc_allow_connection_over_le_fns, PROC_ALLOW_CONNECTION_OVER_LE_DISABLE), \
    ENTRY(proc_allow_connection_over_bredr_fns, PROC_ALLOW_CONNECTION_OVER_BREDR_DISABLE), \
    ENTRY(proc_allow_handset_connect_fns, PROC_ALLOW_HANDSET_CONNECT_DATA_DISABLE), \
    ENTRY(proc_permit_bt_fns, PROC_PERMIT_BT_DISABLE), \
    ENTRY(proc_enable_connectable_handset_fns, PROC_ENABLE_CONNECTABLE_HANDSET_DATA_DISABLE), \
    ENTRY(proc_enable_connectable_peer_fns, PROC_ENABLE_CONNECTABLE_PEER_DATA_DISABLE), \
    PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY) \
    ENTRY(proc_disconnect_handset_fns, NO_DATA), \
    ENTRY(proc_release_peer_fns, NO_DATA), \
    ENTRY(proc_cancel_find_role_fns, NO_DATA), \
    ENTRY(proc_clean_connections_fns, NO_DATA), \
    ENTRY(proc_stop_le_broadcast_fns, NO_DATA), \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_PRIMARY), \
    ENTRY(proc_permit_bt_fns, PROC_PERMIT_BT_ENABLE), \
    ENTRY(proc_allow_connection_over_le_fns, PROC_ALLOW_CONNECTION_OVER_LE_ENABLE), \
    ENTRY(proc_allow_connection_over_bredr_fns, PROC_ALLOW_CONNECTION_OVER_BREDR_ENABLE)

/* Define the no_role_idle_script */
DEFINE_TOPOLOGY_SCRIPT(no_role_idle, NO_ROLE_IDLE_SCRIPT);

