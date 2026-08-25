/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_disabled_state.c

DESCRIPTION
    Event handling functions for the ANC Disabled State.
*/

#include "anc_disabled_state.h"
#include "anc_common_state.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_configure.h"

static bool enableAncWithUserGainEventHandler(anc_state_event_enable_with_user_gain_args_t * args)
{
    bool enabled = FALSE;

    if(args)
        ancDataSetUserGainConfig(args->gain_config_left, args->gain_config_right);

    if(ancConfigureEnableAnc(TRUE, FALSE))
    {
        ancDataSetState(anc_state_enabled);
        enabled = TRUE;
    }

    return enabled;
}

static bool enableAncEventHandler(anc_state_event_enable_args_t * args)
{
    return ancConfigureEnableAnc(args->update_filter_coefficients, args->mute_path_gains);
}

static bool enableParallelAncEventHandler(anc_state_event_enable_args_t * args)
{
    return ancConfigureEnableParallelAnc(args->update_filter_coefficients, args->mute_path_gains);
}

/******************************************************************************/
bool ancStateDisabledHandleEvent(anc_state_event_t event)
{
    /* Assume failure until proven otherwise */
    bool success = FALSE;

    switch (event.id)
    {
        case anc_state_event_enable:
        {
            if (enableAncEventHandler((anc_state_event_enable_args_t *)event.args))
            {
                success = TRUE;
                ancDataSetState(anc_state_enabled);
            }
        }
        break;

        case anc_state_event_enable_parallel_filter:
        {
            if (enableParallelAncEventHandler((anc_state_event_enable_args_t *)event.args))
            {
                success = TRUE;
                ancDataSetState(anc_state_enabled);
            }
        }
        break;

        case anc_state_event_enable_with_user_gain:
        {
            success = enableAncWithUserGainEventHandler((anc_state_event_enable_with_user_gain_args_t *)event.args);
        }
        break;

        case anc_state_event_set_topology:
        {
            ancDataSetTopology(((anc_state_event_set_topology_args_t *)event.args)->anc_topology);
            success = TRUE;
        }
        break;

        case anc_state_event_set_mode:
        case anc_state_event_set_parallel_mode:
        {
            /* Common processing for setting the mode */
            success = ancCommonStateHandleSetMode(event);
        }
        break;

        default:
        {
            ANC_DEBUG_INFO(("Unhandled event [%d]\n", event.id));
            ANC_PANIC();
        }
        break;
    }
    return success;
}
