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
#include "tws_topology_procedure_enable_le_connectable_handset.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API
#define PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)
#else
#define PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY)   ENTRY(proc_enable_le_connectable_handset_fns, PROC_ENABLE_LE_CONNECTABLE_PARAMS_ENABLE),
#endif

#define PRIMARY_ROLE_SCRIPT(ENTRY) \
    ENTRY(proc_allow_handset_connect_fns, PROC_ALLOW_HANDSET_CONNECT_DATA_ENABLE), \
    PROC_ENABLE_LE_CONNECTABLE_HANDSET_FNS(ENTRY) \
    ENTRY(proc_enable_connectable_handset_fns, PROC_ENABLE_CONNECTABLE_HANDSET_DATA_ENABLE), \

/* Define the primary_role_script */
DEFINE_TOPOLOGY_SCRIPT(primary_role, PRIMARY_ROLE_SCRIPT);
