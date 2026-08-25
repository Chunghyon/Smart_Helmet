/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_transport_gatt_marshal_desc.c

DESCRIPTION
    Creates tables of marshal type descriptors for gata transport gatt type
*/

#include "gaia_transport_gatt_marshal_desc.h"

static const marshal_member_descriptor_t gaia_transport_gatt_client_data[] =
{
    MAKE_MARSHAL_MEMBER(gaia_transport_gatt_marshal_data_t, uint32, cid),
    MAKE_MARSHAL_MEMBER(gaia_transport_gatt_marshal_data_t, uint8, response_data_flags),
    MAKE_MARSHAL_MEMBER(gaia_transport_gatt_marshal_data_t, uint8, size_response),
    MAKE_MARSHAL_MEMBER(gaia_transport_gatt_marshal_data_t, uint8, data_endpoint_mode),
};

static const marshal_type_descriptor_t mtd_gaia_transport_gatt_marshal_data_t =
        MAKE_MARSHAL_TYPE_DEFINITION(gaia_transport_gatt_marshal_data_t, gaia_transport_gatt_client_data);

/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *) &mtd_##type,
const marshal_type_descriptor_t * const  mtd_gaia_transport_gatt_client[] =
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    GAIA_TRANSPORT_GATT_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION
