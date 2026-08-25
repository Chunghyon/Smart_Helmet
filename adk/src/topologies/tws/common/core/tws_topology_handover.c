/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       tws_topology_handover.c
\brief      TWS Topology Handover interfaces

*/
#ifdef INCLUDE_MIRRORING
#include "app_handover_if.h"
#include "tws_topology_goals.h"
#include "tws_topology_private.h"
#include "tws_topology_sm.h"

#include <panic.h>
#include <logging.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool twsTopology_Veto(void);
static void twsTopology_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/


REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(TWS_TOPOLOGY, twsTopology_Veto, twsTopology_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*! 
    \brief Handle Veto check during handover
    \return bool
*/
static bool twsTopology_Veto(void)
{
    bool veto = FALSE;

    /* Veto if there are pending goals */
    if (TwsTopology_IsAnyGoalPending())
    {
        veto = TRUE;
        DEBUG_LOG("twsTopology_Veto, Pending goals");
    }
    
    return veto;
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void twsTopology_Commit(bool is_primary)
{
    if (is_primary)
    {
        DEBUG_LOG("twsTopology_Commit, primary");
        twsTopology_SmHandleHandoverToPrimary(TwsTopology_GetSm());
    }
    else
    {
        DEBUG_LOG("twsTopology_Commit secondary");
    }
}

#endif /* INCLUDE_MIRRORING */
