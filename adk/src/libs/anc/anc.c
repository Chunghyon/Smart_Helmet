/*******************************************************************************
Copyright (c) 2015-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc.c

DESCRIPTION
    ANC VM Library API functions.
*/

#include "anc.h"
#include "anc_sm.h"
#include "anc_data.h"
#include "anc_debug.h"
#include "anc_config_read.h"

#include <stdlib.h>

/******************************************************************************/
static bool ancDisable(void)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_disable;
        break;

        case anc_parallel_filter_topology:
            event.id = anc_state_event_disable_parallel_filter;
        break;

        default:
            event.id = anc_state_event_disable;
        break;

    }
    event.args = NULL;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/

static bool ancSetModeWithConfig(anc_mode_t mode, bool update_filter_coefficients, bool enable_coarse_gains, bool enable_fine_gains)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_set_mode_args_t args;
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_set_mode;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_set_parallel_mode;
        break;

        default:
            event.id =anc_state_event_set_mode;
        break;
    }

    args.mode = mode;
    args.update_filter_coefficients = update_filter_coefficients;
    args.enable_coarse_gains = enable_coarse_gains;
    args.enable_fine_gains = enable_fine_gains;

    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/

device_ps_key_config_t device_ps_key_config;

bool AncInit(anc_mic_params_t *mic_params, anc_mode_t init_mode)
{
    anc_state_event_initialise_args_t args;
    anc_state_event_t event = {anc_state_event_initialise, NULL};

    args.mic_params = mic_params;
    args.mode = init_mode;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}
/******************************************************************************/
bool AncSetTopology(anc_filter_topology_t anc_topology)
{
    anc_state_event_t event ={anc_state_event_set_topology,NULL};
    anc_state_event_set_topology_args_t args;

    args.anc_topology=anc_topology;
    event.args=&args;

    return ancStateMachineHandleEvent(event);

}
/******************************************************************************/
#ifdef HOSTED_TEST_ENVIRONMENT
bool AncLibraryTestReset(void)
{
    return ancDataDeinitialise();
}
#endif

/******************************************************************************/
bool AncEnable(bool enable)
{
    bool status = FALSE;

    if (enable)
    {
        status = AncEnableWithConfig(TRUE, FALSE);
    }
    else
    {
        status = ancDisable();
    }

    return status;
}

/******************************************************************************/
bool AncEnableWithUserGain(anc_user_gain_config_t *left_channel_gain, anc_user_gain_config_t *right_channel_gain)
{
    anc_state_event_enable_with_user_gain_args_t args;
    anc_state_event_t event = {anc_state_event_enable_with_user_gain, NULL};

    args.gain_config_left = left_channel_gain;
    args.gain_config_right = right_channel_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncEnableWithMutePathGains(void)
{
    return AncEnableWithConfig(TRUE, TRUE);
}

/******************************************************************************/
bool AncEnableWithConfig(bool update_filter_coefficients, bool mute_path_gains)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_enable_args_t args;
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_enable;
        break;

        case anc_parallel_filter_topology:
            event.id = anc_state_event_enable_parallel_filter;
        break;

        default:
            event.id = anc_state_event_enable;
        break;

    }

    args.update_filter_coefficients = update_filter_coefficients;
    args.mute_path_gains = mute_path_gains;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}


/******************************************************************************/
bool AncSetMode(anc_mode_t mode)
{
    return ancSetModeWithConfig(mode, TRUE, TRUE, TRUE);
}

/******************************************************************************/
bool AncSetModeFilterCoefficients(anc_mode_t mode)
{
    return ancSetModeWithConfig(mode, TRUE, FALSE, FALSE);
}

/******************************************************************************/
bool AncSetModeWithSelectedGains(anc_mode_t mode, bool enable_coarse_gains, bool enable_fine_gains)
{
    return ancSetModeWithConfig(mode, TRUE, enable_coarse_gains, enable_fine_gains);
}

/******************************************************************************/
bool AncSetModeWithConfig(anc_mode_t mode, bool update_filter_coefficients, bool enable_gains)
{
    return ancSetModeWithConfig(mode, update_filter_coefficients, enable_gains, enable_gains);
}

/******************************************************************************/
bool AncSetRxMixEnables(void)
{
    anc_state_event_t event;

    event.id = anc_state_event_set_rxmix_enables;
    event.args = NULL;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncSetCurrentFilterPathGains(void)
{
    anc_filter_topology_t anc_topology = ancDataGetTopology();
    anc_state_event_t event;

    switch(anc_topology)
    {
        case anc_single_filter_topology:
            event.id = anc_state_event_set_all_single_filter_path_gains;
        break;

        case anc_parallel_filter_topology:
            event.id =anc_state_event_set_parallel_filter_path_gains;
        break;

        default:
            event.id =anc_state_event_set_all_single_filter_path_gains;
        break;
    }

    event.args=NULL;
    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncIsEnabled(void)
{
    /* Get current state to determine if ANC is enabled */
    anc_state state = ancDataGetState();

    /* If library has not been initialised then it is invalid to call this function */
    ANC_ASSERT(state != anc_state_uninitialised);

    /* ANC is enabled in any state greater than anc_state_disabled, which allows
       the above assert to be compiled out if needed and this function still 
       behave as expected. */
    return (state > anc_state_disabled) ? TRUE : FALSE;
}

/******************************************************************************/

anc_mic_params_t * AncGetAncMicParams(void)
{
    return ancDataGetMicParams();
}
/******************************************************************************/
bool AncConfigureFFAPathGain(audio_anc_instance instance, uint8 gain)
{
    anc_state_event_set_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_single_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.instance = instance;
    args.path = AUDIO_ANC_PATH_ID_FFA;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureFFBPathGain(audio_anc_instance instance, uint8 gain)
{
    anc_state_event_set_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_single_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.instance = instance;
    args.path = AUDIO_ANC_PATH_ID_FFB;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureFBPathGain(audio_anc_instance instance, uint8 gain)
{
    anc_state_event_set_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_single_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.instance = instance;
    args.path = AUDIO_ANC_PATH_ID_FB;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncReadFineGain(audio_anc_path_id gain_path, uint8 *gain)
{
    return ancReadFineGain(ancDataGetMode(), gain_path, gain);
}

/******************************************************************************/
bool AncReadFineGainDual(audio_anc_path_id gain_path, uint8 *instance_0_gain, uint8 *instance_1_gain)
{
    return ancReadFineGainDual(ancDataGetMode(), gain_path, instance_0_gain, instance_1_gain);
}

/******************************************************************************/
bool AncWriteFineGain(audio_anc_path_id gain_path, uint8 gain)
{
    anc_state_event_write_gain_args_t args;
    anc_state_event_t event = {anc_state_event_write_fine_gain, NULL};

    /* Assign args to the event */
    args.path = gain_path;
    args.gain = gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

bool AncWriteFineGainDual(audio_anc_path_id gain_path, uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_write_gain_dual_args_t args;
    anc_state_event_t event = {anc_state_event_write_fine_gain_dual, NULL};

    /* Assign args to the event */
    args.path = gain_path;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
void AncReadModelCoefficients(audio_anc_instance inst, audio_anc_path_id path, uint32 *denominator, uint32* numerator)
{
    if(denominator && numerator)
    {
        ancReadModelCoefficients(inst, path, denominator, numerator);
    }
}

/******************************************************************************/
bool AncReadLpfCoefficients(audio_anc_instance inst, audio_anc_path_id path, uint8* lpf_shift_1, uint8* lpf_shift_2)
{
    bool success = FALSE;

    if (lpf_shift_1 && lpf_shift_2)
    {
        success = ancReadLpfCoefficients(inst, path, lpf_shift_1, lpf_shift_2);
    }

    return success;
}

/******************************************************************************/
bool AncReadDcFilterCoefficients(audio_anc_instance inst, audio_anc_path_id path, uint8* filter_shift, uint8* filter_enable)
{
    bool success = FALSE;

    if (filter_shift && filter_enable)
    {
        success = ancReadDcFilterCoefficients(inst, path, filter_shift, filter_enable);
    }

    return success;
}

/******************************************************************************/
bool AncReadSmallLpfCoefficients(audio_anc_instance inst, uint8* filter_shift, uint8* filter_enable)
{
    bool success = FALSE;

    if (filter_shift && filter_enable)
    {
        success = ancReadSmallLpfCoefficients(inst, filter_shift, filter_enable);
    }

    return success;
}

/******************************************************************************/
void AncReadCoarseGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint16 *gain)
{
    if (gain != NULL)
    {
        ancReadCoarseGainFromInst(inst, gain_path, gain);
    }
}

/******************************************************************************/
void AncReadFineGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint8 *gain)
{
    if (gain != NULL)
    {
        ancReadFineGainFromInst(inst, gain_path, gain);
    }
}

/******************************************************************************/
void AncReadRxMixCoarseGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint16 *gain)
{
    if (gain != NULL)
    {
#ifdef ANC_UPGRADE_FILTER
        ancReadRxMixCoarseGainFromInst(inst, gain_path, gain);
#else
        UNUSED(inst);
        UNUSED(gain_path);
        *gain = 0;
#endif
    }
}

/******************************************************************************/
void AncReadRxMixFineGainFromInstance(audio_anc_instance inst,audio_anc_path_id gain_path, uint8 *gain)
{
    if (gain != NULL)
    {
#ifdef ANC_UPGRADE_FILTER
        ancReadRxMixFineGainFromInst(inst, gain_path, gain);
#else
        UNUSED(inst);
        UNUSED(gain_path);
        *gain = 0;
#endif
    }
}

/******************************************************************************/
bool AncConfigureParallelFilterFFAPathGain(uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_set_parallel_filter_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_parallel_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.path = AUDIO_ANC_PATH_ID_FFA;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}
/******************************************************************************/
bool AncConfigureParallelFilterFFBPathGain(uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_set_parallel_filter_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_parallel_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.path = AUDIO_ANC_PATH_ID_FFB;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}
/******************************************************************************/

bool AncConfigureParallelFilterFBPathGain(uint8 instance_0_gain, uint8 instance_1_gain)
{
    anc_state_event_set_parallel_filter_path_gain_args_t args;
    anc_state_event_t event = {anc_state_event_set_parallel_filter_path_gain, NULL};

    /* Assign args to the set filter path gain event */
    args.path = AUDIO_ANC_PATH_ID_FB;
    args.instance_0_gain = instance_0_gain;
    args.instance_1_gain  = instance_1_gain;
    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureLPFCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8 lpf_shift_1, uint8 lpf_shift_2)
{
    anc_state_event_configure_lpf_coefficients_args_t args;
    anc_state_event_t event = {anc_state_event_configure_lpf_coefficients, NULL};

    /* Assign args to the configure LPF coefficients event */
    args.instance = instance;
    args.path = path;
    args.lpf_shift_1 = lpf_shift_1;
    args.lpf_shift_2  = lpf_shift_2;

    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureDcFilterCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8 filter_shift, uint8 filter_enable)
{
    anc_state_event_configure_dc_filter_coefficients_args_t args;
    anc_state_event_t event = {anc_state_event_configure_dc_filter_coefficients, NULL};

    /* Assign args to the configure DC Filter coefficients event */
    args.instance = instance;
    args.path = path;
    args.filter_shift = filter_shift;
    args.filter_enable  = filter_enable;

    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
bool AncConfigureSmLPFCoefficients(audio_anc_instance instance, uint8 filter_shift, uint8 filter_enable)
{
    anc_state_event_configure_small_lpf_coefficients_args_t args;
    anc_state_event_t event = {anc_state_event_configure_small_lpf_coefficients, NULL};

    /* Assign args to the configure Small LPF coefficients event */
    args.instance = instance;
    args.filter_shift = filter_shift;
    args.filter_enable  = filter_enable;

    event.args = &args;

    return ancStateMachineHandleEvent(event);
}

/******************************************************************************/
uint32 AncReadNumOfDenominatorCoefficients(void)
{
    return NUMBER_OF_IIR_COEFF_DENOMINATORS;
}

/******************************************************************************/
uint32 AncReadNumOfNumeratorCoefficients(void)
{
    return NUMBER_OF_IIR_COEFF_NUMERATORS;
}
/******************************************************************************/

void AncSetDevicePsKey(uint16 device_ps_key)
{
    device_ps_key_config.device_ps_key = device_ps_key;
}

/******************************************************************************/
int16 AncConvertGainTo16BitQFormat(uint16 gain)
{
    return convertGainTo16BitQFormat(gain);
}

/******************************************************************************/
uint16 AncConvert16BitQFormatToGain(int16 q_format)
{
    return convert16BitQFormatToGain(q_format);
}

/******************************************************************************/
