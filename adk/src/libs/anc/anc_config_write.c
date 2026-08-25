/*******************************************************************************
Copyright (c) 2019  Qualcomm Technologies International, Ltd.


FILE NAME
    anc_config_write.c

DESCRIPTION
    Write the gain parameter to USR13 PS key
*/

#include <stdlib.h>
#include <string.h>
#include "anc.h"
#include "anc_debug.h"
#include "anc_config_data.h"
#include "anc_data.h"
#include "anc_config_read.h"
#include "anc_tuning_data.h"
#include "anc_config_write.h"
#include <ps.h>

extern device_ps_key_config_t device_ps_key_config;

/*! Derive the Delta gain value (difference between the write fine gain and golden config) for the current mode and gain path specified.
    Update the Delta gain value in the device_ps_key. This gain will be used during the ANC initialisation and/or mode change.
*/
bool ancWriteFineGain(anc_mode_t mode, audio_anc_path_id gain_path, uint16 gain)
{
    /* Maximum Gain should be 255 */
    if(gain > MAXIMUM_FINE_GAIN)
    {
        ANC_DEBUG_INFO(("ancWriteFineGain: Invalid gain [%d]", gain));
        Panic();
    }
    bool status = FALSE;
    uint8 golden_gain = 0;
    uint16 fine_gain_tune[FINE_GAIN_TUNE_DATA_ENTRIES] = {0};
    status = TRUE;

    PsRetrieve(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);

    if(ancReadFineGain(mode, gain_path, &golden_gain))
    {
        if(golden_gain == 0)
        {
            golden_gain = 128;
        }
        int16 golden_gain_q_format = convertGainTo16BitQFormat((uint16)golden_gain);
        int16 fine_gain_q_format   = convertGainTo16BitQFormat(gain);
        int16 gain_difference_q_format = 0;

        gain_difference_q_format = (fine_gain_q_format - golden_gain_q_format);
        fine_gain_tune[gain_path - 1] = gain_difference_q_format;

        /* Store delta fine gain in 16-bit fixed-point format(Q6.9) in PS*/
        status = PsStore(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE);
    }
    else
    {
        status = FALSE;
    }

    return status;
}

/*! Write fine gain to the Audio PS key for the current mode and gain path specified 
*/
bool ancWriteFineGainDual(anc_mode_t mode, audio_anc_path_id gain_path, uint16 instance_0_gain, uint16 instance_1_gain)
{
    ANC_DEBUG_INFO(("ancWriteFineGainDual: called"));
    /* Maximum Gain should be 255 */
    if(instance_0_gain > MAXIMUM_FINE_GAIN || instance_1_gain > MAXIMUM_FINE_GAIN)
    {
        ANC_DEBUG_INFO(("ancWriteFineGainDual: Invalid gain [%d, %d]", instance_0_gain, instance_1_gain));
        Panic();
    }
    bool status = FALSE;
    uint8 golden_gain_0 = 0, golden_gain_1 = 0;
    uint16 fine_gain_tune[FINE_GAIN_TUNE_DATA_ENTRIES_HS] = {0};
    status = TRUE;

    PsRetrieve(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE_HS);

    if(ancReadFineGainDual(mode, gain_path, &golden_gain_0, &golden_gain_1))
    {
        if(golden_gain_0 == 0)
        {
            golden_gain_0 = 128;
        }
        int16 golden_gain_q_format = convertGainTo16BitQFormat((uint16)golden_gain_0);
        int16 fine_gain_q_format   = convertGainTo16BitQFormat(instance_0_gain);
        int16 gain_difference_q_format = 0;

        gain_difference_q_format = (fine_gain_q_format - golden_gain_q_format);
        fine_gain_tune[gain_path - 1] = gain_difference_q_format;

        if(golden_gain_1 == 0)
        {
            golden_gain_1 = 128;
        }
        golden_gain_q_format = convertGainTo16BitQFormat((uint16)golden_gain_1);
        fine_gain_q_format   = convertGainTo16BitQFormat(instance_1_gain);
        gain_difference_q_format = 0;

        gain_difference_q_format = (fine_gain_q_format - golden_gain_q_format);
        fine_gain_tune[ANC_INSTANCE_1_INDEX_OFFSET + gain_path - 1] = gain_difference_q_format;

        /* Store delta fine gain in 16-bit fixed-point format(Q6.9) in PS*/
        status = PsStore(device_ps_key_config.device_ps_key, fine_gain_tune, FINE_GAIN_TUNE_DATA_SIZE_HS);

        ANC_DEBUG_INFO(("ancWriteFineGainDual: written [%d, %d, %d, %d, %d, %d]", fine_gain_tune[0], fine_gain_tune[1],
        fine_gain_tune[2], fine_gain_tune[3],
        fine_gain_tune[4], fine_gain_tune[5]));

    }
    else
    {
        status = FALSE;
    }

    return status;
}
