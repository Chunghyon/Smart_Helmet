/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_enabled_state.c

DESCRIPTION
    Event handling functions for the ANC Enabled State.
*/

#include "anc_enabled_state.h"
#include "anc_common_state.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_configure.h"
#include "anc_config_write.h"
#include "anc_configure_coefficients.h"

static bool disableAncEventHandler(void)
{
    bool disabled = FALSE;
    if(ancConfigureDisableAnc())
    {
        ancDataSetState(anc_state_disabled);
        disabled = TRUE;
    }
    return disabled;
}

static bool disableParallelAncEventHandler(void)
{
    bool disabled =FALSE;
    if(ancConfigureDisableParallelAnc())
    {
        ancDataSetState(anc_state_disabled);
        disabled = TRUE;
    }
    return disabled;
}

static bool setModeEventHandler(anc_state_event_set_mode_args_t* args)
{
    return ancConfigureAfterModeChange(args->update_filter_coefficients, args->enable_coarse_gains, args->enable_fine_gains);
}

static bool setParallelModeEventHandler(anc_state_event_set_mode_args_t* args)
{
    return ancConfigureParallelFilterAfterModeChange(args->update_filter_coefficients, args->enable_coarse_gains, args->enable_fine_gains);
}

static bool setFilterPathGainEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancConfigureFilterPathGain(((anc_state_event_set_path_gain_args_t *)event.args)->instance,
                                          ((anc_state_event_set_path_gain_args_t *)event.args)->path,
                                          ((anc_state_event_set_path_gain_args_t *)event.args)->gain);

    return gain_set;
}

static bool setParallelFilterPathGainEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancConfigureParallelFilterPathGain(((anc_state_event_set_parallel_filter_path_gain_args_t *)event.args)->path,
                                                  ((anc_state_event_set_parallel_filter_path_gain_args_t *)event.args)->instance_0_gain,
                                                  ((anc_state_event_set_parallel_filter_path_gain_args_t *)event.args)->instance_1_gain);
    return gain_set;
}

static bool writeFineGainToPsEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancWriteFineGain(ancDataGetMode(),
                                          ((anc_state_event_write_gain_args_t *)event.args)->path,
                                          ((anc_state_event_write_gain_args_t *)event.args)->gain);

    return gain_set;
}

static bool writeFineGainToPsDualEventHandler(anc_state_event_t event)
{
    bool gain_set = FALSE;

    gain_set = ancWriteFineGainDual(ancDataGetMode(),
                                            ((anc_state_event_write_gain_dual_args_t  *)event.args)->path,
                                            ((anc_state_event_write_gain_dual_args_t  *)event.args)->instance_0_gain,
                                            ((anc_state_event_write_gain_dual_args_t  *)event.args)->instance_1_gain);

    return gain_set;
}

static bool configureLpfCoefficientsEventHandler(anc_state_event_configure_lpf_coefficients_args_t* args)
{
    return ancConfigureLpfCoefficients(args->instance, args->path, args->lpf_shift_1, args->lpf_shift_2);
}

static bool configureDcFilterCoefficientsEventHandler(anc_state_event_configure_dc_filter_coefficients_args_t* args)
{
    return ancConfigureDcFilterCoefficients(args->instance, args->path, args->filter_shift, args->filter_enable);
}

static bool configureSmallLpfCoefficientsEventHandler(anc_state_event_configure_small_lpf_coefficients_args_t* args)
{
    return ancConfigureSmLPFCoefficients(args->instance, args->filter_shift, args->filter_enable);
}


/******************************************************************************/
bool ancStateEnabledHandleEvent(anc_state_event_t event)
{
    bool success = FALSE;

    switch (event.id)
    {
        case anc_state_event_disable:
        {
            success = disableAncEventHandler();
        }
        break;

        case anc_state_event_disable_parallel_filter:
        {
            success =disableParallelAncEventHandler();
        }
        break;

        case anc_state_event_set_mode:
        {
            if (ancCommonStateHandleSetMode(event))
            {
                success = setModeEventHandler((anc_state_event_set_mode_args_t *)event.args);
            }
        }
        break;

        case anc_state_event_set_parallel_mode:
        {
            if(ancCommonStateHandleSetMode(event))
            {
                success = setParallelModeEventHandler((anc_state_event_set_mode_args_t *)event.args);
            }

        }
        break;

        case anc_state_event_set_single_filter_path_gain:
        {
            success = setFilterPathGainEventHandler(event);
        }
        break;
               
        case anc_state_event_write_fine_gain:
        {
            success = writeFineGainToPsEventHandler(event);
        }
        break;
        
        case anc_state_event_write_fine_gain_dual:
        {
            success = writeFineGainToPsDualEventHandler(event);
        }
        break;

        case anc_state_event_set_all_single_filter_path_gains:
        {
            ancConfigureFilterPathGains();
#ifdef ANC_UPGRADE_FILTER
            setRxMixGains(anc_single_filter_topology);
#endif
            success = TRUE;
        }
        break;

        case anc_state_event_set_parallel_filter_path_gain:
        {
            success = setParallelFilterPathGainEventHandler(event);
        }
        break;

        case anc_state_event_set_parallel_filter_path_gains:
        {
             ancConfigureParallelFilterPathGains(TRUE, TRUE);
#ifdef ANC_UPGRADE_FILTER
            setRxMixGains(anc_parallel_filter_topology);
#endif
            success = TRUE;
        }
        break;

        case anc_state_event_configure_lpf_coefficients:
        {
            success = configureLpfCoefficientsEventHandler((anc_state_event_configure_lpf_coefficients_args_t *)event.args);
        }
        break;

        case anc_state_event_configure_dc_filter_coefficients:
        {
            success = configureDcFilterCoefficientsEventHandler((anc_state_event_configure_dc_filter_coefficients_args_t *)event.args);
        }
        break;

        case anc_state_event_configure_small_lpf_coefficients:
        {
            success = configureSmallLpfCoefficientsEventHandler((anc_state_event_configure_small_lpf_coefficients_args_t *)event.args);
        }
        break;

        case anc_state_event_set_rxmix_enables:
        {
#ifdef ANC_UPGRADE_FILTER
            setRxMixEnables(ancDataGetTopology());
            success = TRUE;
#endif
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
