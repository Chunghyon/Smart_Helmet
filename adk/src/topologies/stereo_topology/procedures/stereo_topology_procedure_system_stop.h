/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef STEREO_TOPOLOGY_PROC_SYSTEM_STOP_H
#define STEREO_TOPOLOGY_PROC_SYSTEM_STOP_H

#include "script_engine.h"
#include "stereo_topology_private.h"
#include "stereo_topology_procedures.h"
#include "stereo_topology_private.h"

typedef enum
{
    PROC_SEND_STEREO_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED = STEREOTOP_INTERNAL_PROCEDURE_RESULTS_MSG_BASE | stereo_topology_procedure_system_stop,
} stereo_proc_system_stop_messages_t;


extern const procedure_script_t stereo_system_stop_script;

#endif /* STEREO_TOPOLOGY_PROC_SYSTEM_STOP_H */
