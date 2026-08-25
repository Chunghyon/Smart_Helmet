/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_sm_types.h
\brief      Definition of state machine action parameter types for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_SM_TYPES_H
#define DFU_PROTOCOL_SM_TYPES_H

#include <upgrade.h>

typedef struct
{
    uint32 dfu_file_size;
    upgrade_context_t context;
} dfu_protocol_start_params_t;

typedef struct
{
    upgrade_context_t context;
} dfu_protocol_abort_params_t;

typedef struct
{
    bool is_upgrade_successful;
} dfu_protocol_transport_disconnected_params_t;

#endif // DFU_PROTOCOL_SM_TYPES_H
