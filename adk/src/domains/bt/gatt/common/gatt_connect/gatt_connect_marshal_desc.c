/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    gatt_connect_marshal_desc.c

DESCRIPTION
    Creates tables of marshal type descriptors for Gatt Connect data types

*/

#include "gatt_connect_marshal_desc.h"

static const marshal_type_descriptor_t mtd_gatt_connection_t =
        MAKE_MARSHAL_TYPE_DEFINITION_BASIC(gatt_connect_marshal_data);

static const marshal_member_descriptor_t mmd_gatt_connect_data[] =
{
    MAKE_MARSHAL_MEMBER(gatt_connect_marshal_data, gatt_connection_t, connections)
};

static const marshal_type_descriptor_t mtd_gatt_connect_marshal_data =
        MAKE_MARSHAL_TYPE_DEFINITION(gatt_connect_marshal_data, mmd_gatt_connect_data);


/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *) &mtd_##type,
const marshal_type_descriptor_t * const  mtd_gatt_connect[] =
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    GATT_CONNECT_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION
