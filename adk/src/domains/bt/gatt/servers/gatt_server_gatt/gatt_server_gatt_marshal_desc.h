/****************************************************************************
Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.


FILE NAME
    gatt_server_gatt_marshal_desc.h

DESCRIPTION
    Creates tables of marshal type descriptors for GATT server data types

*/

#ifndef GATT_SERVER_GATT_MARSHAL_DESC_H
#define GATT_SERVER_GATT_MARSHAL_DESC_H

#include "app/marshal/marshal_if.h"
#include <marshal_common.h>
#include "gatt_server_gatt.h"

#define NUMBER_OF_GATT_SERVER_GATT_COMMON_MARSHAL_OBJECT_TYPES   (3)

/* Data struture to be marshalled/unmarshalled by GATT server */
typedef struct
{
    uint16   client_config;
    uint32   cid;
} gatt_server_gatt_marshal_data_t;

#define GATT_SERVER_GATT_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(gatt_server_gatt_marshal_data_t)

#define GATT_SERVER_GATT_COMMON_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(uint8) \
    ENTRY(uint16) \
    ENTRY(uint32)

/* Use xmacro to expand type table as enumeration of marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum
{
    /* common types must be placed at the start of the enum */
    DUMMY = NUMBER_OF_GATT_SERVER_GATT_COMMON_MARSHAL_OBJECT_TYPES-1,
    GATT_SERVER_GATT_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    GATT_SERVER_GATT_MARSHAL_OBJ_TYPE_COUNT
};
#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const  mtdesc_gatt_server_mgr[];

#endif /* GATT_SERVER_GATT_MARSHAL_DESC_H */


