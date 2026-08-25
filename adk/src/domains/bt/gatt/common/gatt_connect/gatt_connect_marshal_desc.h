/****************************************************************************
Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.


FILE NAME
    gatt_connect_marshal_desc.h

DESCRIPTION
    Creates tables of marshal type descriptors for Gatt Connect data types

*/

#ifndef GATT_CONNECT_MARSHAL_DESC_H
#define GATT_CONNECT_MARSHAL_DESC_H

#include "marshal_common_desc.h"
#include "app/marshal/marshal_if.h"
#include "gatt_connect.h"
#include "gatt_connect_list.h"
#include <marshal_common.h>

/* Data struture to be marshalled/unmarshalled by gatt connect */
typedef struct{
    gatt_connection_t connections;
}gatt_connect_marshal_data;


#define GATT_CONNECT_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(gatt_connect_marshal_data) \
    ENTRY(gatt_connection_t)


/* Use xmacro to expand type table as enumeration of marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum
{
    /*! common types must be placed at the start of the enum */
    DUMMY_GATT_CONNECT = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    GATT_CONNECT_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    GATT_CONNECT_MARSHAL_OBJ_TYPE_COUNT
};
#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const  mtd_gatt_connect[];
#endif // GATT_CONNECT_MARSHAL_DESC_H
