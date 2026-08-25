/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef STEREO_TOPOLOGY_PROC_CONNECT_HANDSET_T
#define STEREO_TOPOLOGY_PROC_CONNECT_HANDSET_T

#include "stereo_topology_procedures.h"

/*! Structure definining the parameters for the procedure to (re)connect a handset

    This should normally be used via the predefined constant structures
    PROC_CONNECT_HANDSET_LINK_LOSS and PROC_CONNECT_HANDSET_NORMAL
*/
typedef struct
{
    bool link_loss;
} CONNECT_HANDSET_PARAMS_T;

extern const procedure_fns_t stereo_proc_connect_handset_fns;

/*! Parameter definition for link loss reconnection */
extern const CONNECT_HANDSET_PARAMS_T stereo_topology_procedure_connect_handset_link_loss;
#define PROC_CONNECT_HANDSET_LINK_LOSS  ((Message)&stereo_topology_procedure_connect_handset_link_loss)

/*! Parameter definition for normal handset connection */
extern const CONNECT_HANDSET_PARAMS_T stereo_topology_procedure_connect_handset_normal;
#define PROC_CONNECT_HANDSET_NORMAL  ((Message)&stereo_topology_procedure_connect_handset_normal)

#endif /* STEREO_TOPOLOGY_PROC_CONNECT_HANDSET_T */
