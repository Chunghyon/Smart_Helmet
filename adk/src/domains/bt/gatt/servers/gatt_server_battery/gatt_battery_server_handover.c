/*
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gatt_battery_server_handover.c
\brief      GATT Battery Server Handover functions are defined
*/

#ifdef INCLUDE_GATT_BATTERY_SERVER

#include <panic.h>
#include <stdlib.h>
#include <sink.h>
#include <stream.h>
#include <source.h>
#include "marshal.h"
#include "handover_if.h"
#include "logging.h"
#include "gatt_server_battery_marshal_desc.h"
#include "gatt_server_battery.h"
#include "gatt.h"
#include "bt_types.h"

/*! Use this flag to clean unmarshalled data, if any, during handover abort phase */
static bool unmarshalled = FALSE;

static void gattBatteryServer_ResetContext(void)
{
    int battery_type;
    int client_count;
    gattServerBatteryInstanceInfo *batt_instance;

    for (battery_type = 0; battery_type < NUMBER_BATTERY_SERVERS; battery_type++)
    {
        batt_instance = GetGattServerBatteryInstance(battery_type);

        for (client_count = 0; client_count < NUMBER_BATTERY_CLIENTS; client_count++)
        {
            memset(&batt_instance->client_data[client_count], 0, sizeof(gattServerBatteryClientData));
        }
    }
}

/*! \brief Battery server has no conditions to check and veto */
static bool gattBatteryServer_Veto(void)
{
    return FALSE;
}

/*! \brief Find the battery client configuration and marshal if any exists */
static bool gattBatteryServer_Marshal(const tp_bdaddr *tp_bd_addr,
                                      uint8 *buf,
                                      uint16 length,
                                      uint16 *written)
{
    int battery_type;
    int client_count, client_index = 0;
    bool marshalled;
    gatt_server_battery_marshal_data_t obj;
    gattServerBatteryInstanceInfo *batt_instance;
    gatt_cid_t cid_to_marshal = GattConnect_GetConnectionIdFromTpaddr(tp_bd_addr);

    if (cid_to_marshal != INVALID_CID)
    {
        obj.cid = cid_to_marshal;
        for (battery_type = 0; battery_type < NUMBER_BATTERY_SERVERS; battery_type++)
        {
            batt_instance = GetGattServerBatteryInstance(battery_type);

            for (client_count = 0; client_count < NUMBER_BATTERY_CLIENTS; client_count++)
            {
                if (cid_to_marshal == batt_instance->client_data[client_count].cid)
                {
                    obj.client_config[client_index] = batt_instance->client_data[client_count].config;
                    obj.sent_battery_level[client_index] = batt_instance->client_data[client_count].sent_battery_level;
                    client_index++;
                    break;
                }
            }
        }

        DEBUG_LOG("gattBatteryServer_Marshal: Marshalling for addr[0x%06x]", tp_bd_addr->taddr.addr.lap);
        marshaller_t marshaller = MarshalInit(mtdesc_gatt_battery_mgr, GATT_SERVER_BATTERY_MARSHAL_OBJ_TYPE_COUNT);
        MarshalSetBuffer(marshaller, (void*)buf, length);
        marshalled = Marshal(marshaller, &obj, MARSHAL_TYPE(gatt_server_battery_marshal_data_t));
        *written = marshalled? MarshalProduced(marshaller): 0;
        MarshalDestroy(marshaller, FALSE);
        return marshalled;
    }

    *written = 0;
    return TRUE;
}

/*! \brief Unmarshal and fill the battery client config data */
static bool gattBatteryServer_Unmarshal(const tp_bdaddr *tp_bd_addr,
                                        const uint8 *buf,
                                        uint16 length,
                                        uint16 *consumed)
{
    UNUSED(tp_bd_addr);
    marshal_type_t unmarshalled_type;
    int battery_type;
    int client_count, client_index = 0;
    gatt_server_battery_marshal_data_t *data;
    gattServerBatteryInstanceInfo *batt_instance;
    gatt_cid_t cid_to_unmarshal;

    DEBUG_LOG("gattBatteryServer_Unmarshal");

    unmarshaller_t unmarshaller = UnmarshalInit(mtdesc_gatt_battery_mgr, GATT_SERVER_BATTERY_MARSHAL_OBJ_TYPE_COUNT);
    UnmarshalSetBuffer(unmarshaller, (void *)buf, length);

    if (Unmarshal(unmarshaller, (void **)&data, &unmarshalled_type))
    {
        PanicFalse(unmarshalled_type == MARSHAL_TYPE(gatt_server_battery_marshal_data_t));
        PanicNull(data);

        cid_to_unmarshal = data->cid;
        PanicFalse(cid_to_unmarshal != INVALID_CID);

        for (battery_type = 0; battery_type < NUMBER_BATTERY_SERVERS; battery_type++)
        {
            batt_instance = GetGattServerBatteryInstance(battery_type);

            for (client_count = 0; client_count < NUMBER_BATTERY_CLIENTS; client_count++)
            {
                if (batt_instance->client_data[client_count].cid == 0)
                {
                    batt_instance->client_data[client_count].cid = cid_to_unmarshal;
                    batt_instance->client_data[client_count].config = data->client_config[client_index];
                    batt_instance->client_data[client_count].sent_battery_level = data->sent_battery_level[client_index];
                    client_index++;
                    break;
                }
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
        DEBUG_LOG("gattBatteryServer_Unmarshal: failed unmarshal");
        UnmarshalDestroy(unmarshaller, TRUE);
        return FALSE;
    }
}

/*! \brief Handle commit operation on primary */
static void gattBatteryServer_HandleCommitForPrimary(void)
{
    DEBUG_LOG("gattBatteryServer_HandoverCommit For Primary");
}

/*! \brief When GATT link is transferred, the battery context will be cleaned */
static void gattBatteryServer_HandleCommitForSecondary(void)
{
    DEBUG_LOG("gattBatteryServer_HandoverCommit For Secondary");
}

/*! \brief Handle commit for Unicast manager */
static void gattBatteryServer_HandoverCommit(const tp_bdaddr *tp_bd_addr, const bool is_primary)
{
    UNUSED(tp_bd_addr) ;

    if (is_primary)
    {
        gattBatteryServer_HandleCommitForPrimary();
    }
    else
    {
        gattBatteryServer_HandleCommitForSecondary();
    }
}

/*! \brief Handle handover complete for Unicast manager */
static void gattBatteryServer_HandoverComplete(const bool is_primary )
{
    UNUSED(is_primary);
    /* mark complete of unmarshalled data */
    unmarshalled = FALSE;
}

/*! \brief On abort, reset the unicast manager context in the secondary */
static void gattBatteryServer_HandoverAbort(void)
{
    DEBUG_LOG("gattBatteryServer_HandoverAbort");

    if (unmarshalled)
    {
        unmarshalled = FALSE;
        gattBatteryServer_ResetContext();
    }
}

/*! \brief On abort, Unicast manager handover interfaces */
const handover_interface gatt_battery_server_handover_if =
        MAKE_BLE_HANDOVER_IF(&gattBatteryServer_Veto,
                             &gattBatteryServer_Marshal,
                             &gattBatteryServer_Unmarshal,
                             &gattBatteryServer_HandoverCommit,
                             &gattBatteryServer_HandoverComplete,
                             &gattBatteryServer_HandoverAbort);

#endif /* INCLUDE_GATT_BATTERY_SERVER */