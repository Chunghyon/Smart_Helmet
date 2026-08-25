/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Context types exposed by the context framework
*/

#ifndef CONTEXT_TYPES_H_
#define CONTEXT_TYPES_H_

#include <device.h>
#include "source_param_types.h"
#if defined(INCLUDE_SPATIAL_AUDIO) && defined (INCLUDE_SPATIAL_DATA)
#include "spatial_data.h"
#endif

typedef enum
{
    context_physical_state,
    context_maximum_connected_handsets,
    context_connected_handsets_info,
    context_active_source_info,
    context_streaming_info,
#if defined(INCLUDE_SPATIAL_AUDIO) && defined (INCLUDE_SPATIAL_DATA)
    context_spatial_audio_info,
#endif
    max_context_items,
} context_item_t;

typedef enum
{
    in_case,
    out_of_case,
    on_head,
} context_physical_state_t;

typedef unsigned context_maximum_connected_handsets_t;

typedef struct
{
    device_t handset;
    bool is_bredr_connected;
    bool is_le_connected;
} connected_handset_info_t;

#define CONTEXT_INFO_MAX_HANDSETS     4

typedef struct
{
    unsigned number_of_connected_handsets;
    connected_handset_info_t connected_handsets[CONTEXT_INFO_MAX_HANDSETS];
} context_connected_handsets_info_t;

typedef struct
{
    device_t handset;
    generic_source_t active_source;
    bool has_control_channel;
} context_active_source_info_t;

typedef struct
{
    uint8 seid;
    bool is_lossless;
    bool is_adaptive;
    uint32 bitrate;
    uint16 primary_rssi;
    int16 primary_link_quality;
} context_streaming_info_t;

#if defined(INCLUDE_SPATIAL_AUDIO) && defined (INCLUDE_SPATIAL_DATA)
typedef SPATIAL_DATA_REPORT_DATA_IND_T context_spatial_audio_info_t;
#endif

#endif /* CONTEXT_TYPES_H_ */

