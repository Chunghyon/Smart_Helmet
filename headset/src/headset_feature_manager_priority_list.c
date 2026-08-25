/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the feature manager priority list for the Headset application.
*/

#include "headset_feature_manager_priority_list.h"

#define SIZE_OF_LIST(arr)       ARRAY_DIM(arr) 

/* We want to add analog audio v/s VA priority because both use the same ADC as source */
#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
static const feature_id_t feature_id_for_analog_audio_over_va[] = { feature_id_wired_analog_audio, feature_id_va };
static const feature_manager_priority_list_t feature_priority_for_analog_over_va = { feature_id_for_analog_audio_over_va, SIZE_OF_LIST(feature_id_for_analog_audio_over_va) };
#endif

/* List 1 */
static const feature_id_t feature_id_for_sco_over_va[] = { feature_id_sco, feature_id_va };
static const feature_manager_priority_list_t feature_priority_for_sco_over_va = { feature_id_for_sco_over_va, SIZE_OF_LIST(feature_id_for_sco_over_va) };

/* List of lists */
static const feature_manager_priority_list_t * priority_lists[] = { 
        &feature_priority_for_sco_over_va
#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
        , &feature_priority_for_analog_over_va
#endif
        };
static const feature_manager_priority_lists_t feature_manager_priority_lists = { priority_lists, SIZE_OF_LIST(priority_lists) };
const feature_manager_priority_lists_t * Headset_GetFeatureManagerPriorityLists(void)
{
    return &feature_manager_priority_lists;
}
