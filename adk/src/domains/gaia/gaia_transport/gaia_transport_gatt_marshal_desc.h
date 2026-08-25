/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_transport_gatt_marshal_desc.h

DESCRIPTION
    Creates tables of marshal type descriptors for gata transport gatt type
*/

#ifndef GAIA_TRANSPORT_GATT_MARSHAL_DESC_H
#define GAIA_TRANSPORT_GATT_MARSHAL_DESC_H


#include "marshal_common_desc.h"
#include "gatt_connect.h"
#include "gaia.h"

/* Data struture to be marshalled/unmarshalled by gaia transport gatt */
typedef struct
{
    uint32 cid;
    uint8 response_data_flags;               /*!< response notifications/indicattions enabled on response/data endpoint. */
    uint8 size_response;
    uint8 data_endpoint_mode;                /*!< Current mode of data endpoint */
} gaia_transport_gatt_marshal_data_t;

#define GAIA_TRANSPORT_GATT_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(gaia_transport_gatt_marshal_data_t)

/* Use xmacro to expand type table as enumeration of marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    GAIA_TRANSPORT_GATT_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    GAIA_TRANSPORT_GATT_MARSHAL_OBJ_TYPE_COUNT
};
#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const  mtd_gaia_transport_gatt_client[];

#endif // GAIA_TRANSPORT_GATT_MARSHAL_DESC_H
