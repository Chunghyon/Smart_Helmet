/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    gatt_server_battery_marshal_desc.c

DESCRIPTION
    Creates tables of marshal type descriptors gatt server battery data types

*/

#include "gatt_server_battery_marshal_desc.h"

static const marshal_member_descriptor_t mmd_gatt_server_battery_data[] =
{
    MAKE_MARSHAL_MEMBER_ARRAY(gatt_server_battery_marshal_data_t, uint16, client_config, NUMBER_BATTERY_SERVERS),
    MAKE_MARSHAL_MEMBER_ARRAY(gatt_server_battery_marshal_data_t, uint16, sent_battery_level, NUMBER_BATTERY_SERVERS),
    MAKE_MARSHAL_MEMBER(gatt_server_battery_marshal_data_t, uint32, cid),
};

static const marshal_type_descriptor_t marshal_type_descriptor_gatt_server_battery_marshal_data_t =
        MAKE_MARSHAL_TYPE_DEFINITION(gatt_server_battery_marshal_data_t, mmd_gatt_server_battery_data);

/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *) &marshal_type_descriptor_##type,
const marshal_type_descriptor_t * const  mtdesc_gatt_battery_mgr[GATT_SERVER_BATTERY_MARSHAL_OBJ_TYPE_COUNT] =
{
    GATT_SERVER_BATTERY_COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    GATT_SERVER_BATTERY_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION

