/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "context_framework_test.h"
#include "context_framework.h"

#include <logging.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

bool ContextFrameworkTest_GetPhysicalState(void)
{
    context_physical_state_t physical_state = 0xFF;
    bool item_valid = ContextFramework_GetContextItem(context_physical_state, (unsigned *)&physical_state, sizeof(context_physical_state_t));
    DEBUG_LOG_ALWAYS("ContextFrameworkTest_GetPhysicalState enum:context_physical_state_t:%d", physical_state);
    return item_valid;
}

bool ContextFrameworkTest_GetMaximumHandsets(void)
{
    context_maximum_connected_handsets_t maximum_handsets = 0xFF;
    bool item_valid = ContextFramework_GetContextItem(context_maximum_connected_handsets, (unsigned *)&maximum_handsets, sizeof(context_maximum_connected_handsets_t));
    DEBUG_LOG_ALWAYS("ContextFrameworkTest_GetMaximumHandsets %d", maximum_handsets);
    return item_valid;
}

bool ContextFrameworkTest_GetConnectedHandsetsInfo(void)
{
    context_connected_handsets_info_t handset_info;
    bool item_valid = ContextFramework_GetContextItem(context_connected_handsets_info, (unsigned *)&handset_info, sizeof(context_connected_handsets_info_t));
    DEBUG_LOG_ALWAYS("ContextFrameworkTest_GetConnectedHandsetsInfo handsets=%d", handset_info.number_of_connected_handsets);
    for(unsigned i=0; i<handset_info.number_of_connected_handsets; i++)
    {
        DEBUG_LOG_ALWAYS("Device=%p BREDR=%d LE=%d", handset_info.connected_handsets[i].handset,
                                handset_info.connected_handsets[i].is_bredr_connected,
                                handset_info.connected_handsets[i].is_le_connected);
    }
    return item_valid;
}

bool ContextFrameworkTest_GetActiveSourceInfo(void)
{
    context_active_source_info_t source_info;
    bool item_valid = ContextFramework_GetContextItem(context_active_source_info, (unsigned *)&source_info, sizeof(context_active_source_info_t));
    DEBUG_LOG_ALWAYS("ContextFrameworkTest_GetActiveSourceInfo handset=%p type=enum:source_type_t:%d source=%d has_control_channel=%d",
                                            source_info.handset,
                                            source_info.active_source.type,
                                            source_info.active_source.u.audio,
                                            source_info.has_control_channel);
    return item_valid;
}
