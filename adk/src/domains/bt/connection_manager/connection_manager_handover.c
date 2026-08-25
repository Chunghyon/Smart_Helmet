/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       connection_manager_handover.c
\brief      Connection Manager Handover related interfaces

*/
#ifdef INCLUDE_MIRRORING
#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "connection_manager_list.h"
#include "bt_device.h"
#include <panic.h>
#include <logging.h>
#include <stdlib.h>


/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool connectionManager_VetoLink(const bdaddr *bd_addr);

static bool connectionManager_Marshal(const bdaddr *bd_addr, 
                                      marshal_type_t type,
                                      void **marshal_obj);

static app_unmarshal_status_t connectionManager_Unmarshal(const bdaddr *bd_addr, 
                                        marshal_type_t type,
                                        void *unmarshal_obj);


static void connectionManager_Commit(bool is_primary);


/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t connection_manager_marshal_types[] = {
    MARSHAL_TYPE_INFO(cm_connection_t, MARSHAL_TYPE_CATEGORY_PER_INSTANCE),
};

const marshal_type_list_t connection_manager_marshal_types_list = {connection_manager_marshal_types, ARRAY_DIM(connection_manager_marshal_types)};

/* Register the handover interface functions.
   Connection Manager has no non-link specific reasons to veto, so registers NULL
   for the VETO entry of the macro, and only registers per-link veto callbacks.
*/
REGISTER_HANDOVER_INTERFACE_VPL(CONNECTION_MANAGER, &connection_manager_marshal_types_list, NULL, connectionManager_VetoLink, connectionManager_Marshal, connectionManager_Unmarshal, connectionManager_Commit);


/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*! \brief Common logic for BREDR and LE link veto based on link state.
    \param conn Pointer to connection, may be NULL.
    \return bool TRUE veto handover, FALSE permit handover.
*/
static bool connectionManager_VetoForLinkState(cm_connection_t* conn)
{
    bool veto = FALSE;
    cm_connection_state_t connection_state;

    if (conn)
    {
        connection_state = conManagerGetConnectionState(conn);
        
        switch (connection_state)
        {
            case ACL_CONNECTING:
                DEBUG_LOG_INFO("connectionManager_Veto, ACL_CONNECTING");
                veto = TRUE;
            break;

            case ACL_CONNECTING_PENDING_PAUSE:
                DEBUG_LOG_INFO("connectionManager_Veto, ACL_CONNECTING_PENDING_PAUSE");
                veto = TRUE;
            break;

            default:
            break;
        }
    }

    return veto;
}

/*! 
    \brief Check for need to veto handover of a specific BREDR ACL. 
    \param bd_addr Address of the link to check for veto.
    \return bool TRUE veto the handover, FALSE permit the handover.
*/
static bool connectionManager_VetoLink(const bdaddr *bd_addr)
{
    tp_bdaddr tpaddr;
    cm_connection_t* conn;

    BdaddrTpFromBredrBdaddr(&tpaddr, bd_addr);
    conn = ConManagerFindConnectionFromBdAddr(&tpaddr);

    return connectionManager_VetoForLinkState(conn);
}


/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled
                            \ref bdaddr
    \param[in] type         Type of the data to be marshalled \ref marshal_type_t
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool connectionManager_Marshal(const bdaddr *bd_addr, 
                                      marshal_type_t type, 
                                      void **marshal_obj)
{
    bool status = FALSE;
    *marshal_obj = NULL;

    switch (type)
    {
        case MARSHAL_TYPE(cm_connection_t):
        {
            tp_bdaddr tpaddr;

            BdaddrTpFromBredrBdaddr(&tpaddr, bd_addr);

            *marshal_obj = PanicNull(ConManagerFindConnectionFromBdAddr(&tpaddr));

            status = TRUE;
            break;
        }
        default:
            Panic();
            break;
    }

    return status;
}

/*! 
    \brief The function shall copy the unmarshal_obj associated to specific 
            marshal type \ref marshal_type_t

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled
                            \ref bdaddr
    \param[in] type         Type of the unmarshalled data \ref marshal_type_t
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.

*/
static app_unmarshal_status_t connectionManager_Unmarshal(const bdaddr *bd_addr, 
                                        marshal_type_t type, 
                                        void *unmarshal_obj)
{
    tp_bdaddr tpaddr;
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;
    
    BdaddrTpFromBredrBdaddr(&tpaddr, bd_addr);

    switch (type)
    {
        case MARSHAL_TYPE(cm_connection_t):
        {
            cm_connection_t* new_connection;
            tp_bdaddr empty;

            BdaddrTpSetEmpty(&empty);
            new_connection = ConManagerFindConnectionFromBdAddr(&empty);
            PanicNull(new_connection);
            ConManagerConnectionCopy(new_connection, (cm_connection_t*)unmarshal_obj);
            result = UNMARSHAL_SUCCESS_FREE_OBJECT;
            break;
        }

        default:
            /* Do nothing */
            break;
    }

    return result;
}


/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void connectionManager_Commit(bool is_primary)
{
    cm_list_iterator_t iterator;
    cm_connection_t* connection = ConManagerListHeadConnection(&iterator);

    while (connection)
    {
        const tp_bdaddr *addr = ConManagerGetConnectionTpAddr(connection);

        if (is_primary)
        {
            if (appDeviceIsPrimary(&addr->taddr.addr))
            {
                /* The new primary needs to swap the address of the peer */
                const tp_bdaddr secondary_addr = *addr;
                PanicFalse(appDeviceGetSecondaryBdAddr(&secondary_addr.taddr.addr));
                ConManagerSetConnectionTpAddr(connection, &secondary_addr);
            }
        }
        else
        {
            if (appDeviceIsSecondary(&addr->taddr.addr))
            {
                /* The new secondary needs to swap the address of the peer */
                const tp_bdaddr primary_addr = *addr;
                PanicFalse(appDeviceGetPrimaryBdAddr(&primary_addr.taddr.addr));
                ConManagerSetConnectionTpAddr(connection, &primary_addr);
            }
            else
            {
                /* Except old Secondary(new Primary) connection, rest all must be
                    * handover device connections(handset-BREDR, handset-LE etc.,) which shall be removed */
                if(!appDeviceIsPrimary(&addr->taddr.addr))
                {
                    conManagerRemoveConnection(connection);
                }
            }
        }
        connection = ConManagerListNextConnection(&iterator);
    }
}


#endif /* INCLUDE_MIRRORING */
