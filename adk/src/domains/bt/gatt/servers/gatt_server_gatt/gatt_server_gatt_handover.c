/*
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gatt_server_gatt_handover.c
\brief      GATT Server Handover functions are defined
*/

#include <panic.h>
#include <stdlib.h>
#include <sink.h>
#include <stream.h>
#include <source.h>
#include "marshal.h"
#include "handover_if.h"
#include "logging.h"
#include "gatt_server_gatt_marshal_desc.h"
#include "gatt_server_gatt.h"
#include "gatt.h"
#include "bt_types.h"
#include "gatt_connect.h"

/*! Use this flag to clean unmarshalled data, if any, during handover abort phase */
static bool unmarshalled = FALSE;

static void gattServer_ResetContext(void)
{
    gattServerGattData *gatt_instance = GetGattServerInstance();

    memset(gatt_instance->client_data, 0, sizeof(gattServerGattClientData) * NUMBER_GATT_CLIENTS);
}

/*! \brief GATT server has no conditions to check and veto */
static bool gattServer_Veto(void)
{
    return FALSE;
}

/*! \brief Find the gatt client configuration and marshal if any exists */
static bool gattServer_Marshal(const tp_bdaddr *tp_bd_addr,
                               uint8 *buf,
                               uint16 length,
                               uint16 *written)
{
    int client_count;
    bool marshalled;
    gatt_server_gatt_marshal_data_t obj;
    gattServerGattData *gatt_instance = GetGattServerInstance();
    gatt_cid_t cid_to_marshal = GattConnect_GetConnectionIdFromTpaddr(tp_bd_addr);

    if (cid_to_marshal != INVALID_CID)
    {
        for (client_count = 0; client_count < NUMBER_GATT_CLIENTS; client_count++)
        {
            if (cid_to_marshal == gatt_instance->client_data[client_count].cid)
            {
                obj.cid = cid_to_marshal;
                obj.client_config = gatt_instance->client_data[client_count].config;
                break;
            }
        }

        DEBUG_LOG("gattServer_Marshal: Marshalling for addr[0x%06x]", tp_bd_addr->taddr.addr.lap);
        marshaller_t marshaller = MarshalInit(mtdesc_gatt_server_mgr, GATT_SERVER_GATT_MARSHAL_OBJ_TYPE_COUNT);
        MarshalSetBuffer(marshaller, (void*)buf, length);
        marshalled = Marshal(marshaller, &obj, MARSHAL_TYPE(gatt_server_gatt_marshal_data_t));
        *written = marshalled? MarshalProduced(marshaller): 0;
        MarshalDestroy(marshaller, FALSE);
        return marshalled;
    }

    *written = 0;
    return TRUE;
}

/*! \brief Unmarshal and fill the gatt client config data */
static bool gattServer_Unmarshal(const tp_bdaddr *tp_bd_addr,
                                 const uint8 *buf,
                                 uint16 length,
                                 uint16 *consumed)
{
    UNUSED(tp_bd_addr);
    marshal_type_t unmarshalled_type;
    int client_count;
    gatt_server_gatt_marshal_data_t *data;
    gattServerGattData *gatt_instance = GetGattServerInstance();

    DEBUG_LOG("gattServer_Unmarshal");

    unmarshaller_t unmarshaller = UnmarshalInit(mtdesc_gatt_server_mgr, GATT_SERVER_GATT_MARSHAL_OBJ_TYPE_COUNT);
    UnmarshalSetBuffer(unmarshaller, (void *)buf, length);

    if (Unmarshal(unmarshaller, (void **)&data, &unmarshalled_type))
    {
        PanicFalse(unmarshalled_type == MARSHAL_TYPE(gatt_server_gatt_marshal_data_t));
        PanicNull(data);

        for (client_count = 0; client_count < NUMBER_GATT_CLIENTS; client_count++)
        {
            if (gatt_instance->client_data[client_count].cid == 0)
            {
                gatt_instance->client_data[client_count].cid = data->cid;
                gatt_instance->client_data[client_count].config = data->client_config;
                break;
            }
        }

        unmarshalled = TRUE;
        *consumed = UnmarshalConsumed(unmarshaller);
        UnmarshalDestroy(unmarshaller, TRUE);
        return TRUE;
    }
    else
    {
        *consumed = 0;
        DEBUG_LOG("gattServer_Unmarshal: failed unmarshal");
        UnmarshalDestroy(unmarshaller, TRUE);
        return FALSE;
    }
}

/*! \brief Handle commit operation on primary */
static void gattServer_HandleCommitForPrimary(void)
{
    DEBUG_LOG("gattServer_HandoverCommit For Primary");
}

/*! \brief When GATT link is transferred, the gatt server context will be cleaned */
static void gattServer_HandleCommitForSecondary(void)
{
    DEBUG_LOG("gattServer_HandoverCommit For Secondary");
}

/*! \brief Handle commit for Unicast manager */
static void gattServer_HandoverCommit(const tp_bdaddr *tp_bd_addr, const bool is_primary)
{
    UNUSED(tp_bd_addr) ;

    if (is_primary)
    {
        gattServer_HandleCommitForPrimary();
    }
    else
    {
        gattServer_HandleCommitForSecondary();
    }
}

/*! \brief Handle handover complete for gatt server */
static void gattServer_HandoverComplete(const bool is_primary )
{
    UNUSED(is_primary);
    /* mark complete of unmarshalled data */
    unmarshalled = FALSE;
}

/*! \brief On abort, reset the gatt server context */
static void gattServer_HandoverAbort(void)
{
    DEBUG_LOG("gattServer_HandoverAbort");

    if (unmarshalled)
    {
        unmarshalled = FALSE;
        gattServer_ResetContext();
    }
}

/*! \brief On abort, Unicast manager handover interfaces */
const handover_interface gatt_server_handover_if =
        MAKE_BLE_HANDOVER_IF(&gattServer_Veto,
                             &gattServer_Marshal,
                             &gattServer_Unmarshal,
                             &gattServer_HandoverCommit,
                             &gattServer_HandoverComplete,
                             &gattServer_HandoverAbort);
