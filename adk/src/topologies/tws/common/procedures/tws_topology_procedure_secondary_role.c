/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Script to transition to secondary. Assumes system is already idle.
*/

#include "script_engine.h"

#include "tws_topology_procedure_set_address.h"

#define SECONDARY_ROLE_SCRIPT(ENTRY) \
    ENTRY(proc_set_address_fns, PROC_SET_ADDRESS_TYPE_DATA_SECONDARY),

/* Define the secondary_role_script */
DEFINE_TOPOLOGY_SCRIPT(secondary_role, SECONDARY_ROLE_SCRIPT);

