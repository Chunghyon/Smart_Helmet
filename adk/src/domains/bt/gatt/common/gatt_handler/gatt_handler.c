/*!
\copyright  Copyright (c) 2015 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Application support for GATT, GATT Server and GAP Server
*/

#include "gatt_handler.h"
#include "local_name.h"
#include "gatt_handler_db_if.h"
#include "adk_log.h"
#include "system_state.h"

#include <bdaddr.h>
#include <gatt.h>
#include <gatt_manager.h>
#include <panic.h>
#include <l2cap_prim.h>

/*!< App GATT component task */
gattTaskData    app_gatt;

/*! Earbud GATT database, for the required GATT and GAP servers. */
extern const uint16 gattDatabase[];


bool GattHandlerInit(Task init_task)
{
    gattTaskData *gatt = GattGetTaskData();

    UNUSED(init_task);

    if (!GattManagerRegisterConstDB(&gattDatabase[0], GattGetDatabaseSize()/sizeof(uint16)))
    {
        DEBUG_LOG("appGattInit. Failed to register GATT database");
        Panic();
    }

    memset(gatt,0,sizeof(*gatt));

    return TRUE;
}


bool GattHandler_GetPublicAddressFromConnectId(uint16 cid, bdaddr *public_addr)
{
    tp_bdaddr client_tpaddr;
    tp_bdaddr public_tpaddr;
    bool addr_found = FALSE;
    
    if (VmGetBdAddrtFromCid(cid, &client_tpaddr))
    {
        if (client_tpaddr.taddr.type == TYPED_BDADDR_RANDOM)
        {
            if (!VmGetPublicAddress(&client_tpaddr, &public_tpaddr))
            {
                return FALSE;
            }
        }
        else
        {
            public_tpaddr = client_tpaddr;
        }
        if (!BdaddrIsZero(&public_tpaddr.taddr.addr))
        {
            *public_addr = public_tpaddr.taddr.addr;
            addr_found = TRUE;
        }
    }
    
    return addr_found;
}
