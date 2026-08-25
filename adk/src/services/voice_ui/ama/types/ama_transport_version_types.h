/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_version_types.h
\brief      Types relevant for transport version
*/

#ifndef AMA_TRANSPORT_VERSION_TYPES_H
#define AMA_TRANSPORT_VERSION_TYPES_H

typedef enum {
    ama_stream_control = 0,
    ama_stream_voice = 1,
    max_ama_streams
}ama_stream_type_t;

/* This is a subset - refer to AMA spec. */
typedef enum {
    ama_rx_code_success = 0x00,
    ama_rx_code_max_value
}ama_rx_code_t;

#endif // AMA_TRANSPORT_VERSION_TYPES_H
