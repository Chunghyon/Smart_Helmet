/*
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       iap2_peer_sig_descriptor.h
\brief      IAP2 peer signalling descriptor
*/

#ifndef IAP2_PEER_SIG_DATA_STATIC_DESCRIPTOR_H
#define IAP2_PEER_SIG_DATA_STATIC_DESCRIPTOR_H

#include <marshal_common.h>
#include <marshal_common_desc.h>
#include <marshal.h>

typedef struct
{
    uint32 data_addr;
    uint8  data_length;
    uint8  data[1];
} peer_sig_static_data_t;

/* Create base list of marshal types the key sync will use. */
#define MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(peer_sig_static_data)

/* X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum MARSHAL_TYPES
{
    /* common types must be placed at the start of the enum */
    DUMMY = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    /* now expand the marshal types specific to this component. */
    MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MARSHAL_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION


#endif // IAP2_PEER_SIG_DATA_STATIC_DESCRIPTOR_H
