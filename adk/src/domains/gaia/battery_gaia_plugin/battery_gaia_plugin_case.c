/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies, Inc. and/or its subsidiaries.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file to help the battery plugin to get information on the case battery.
*/


#ifdef INCLUDE_GAIA
#include "battery_gaia_plugin_case.h"

bool batteryGaiaPluginCase_IsCaseBatterySupported(void)
{
#if defined(INCLUDE_CASE_COMMS) && defined(HAVE_CC_MODE_EARBUDS)
    return TRUE;
#else /* INCLUDE_CASE_COMMS && HAVE_CC_MODE_EARBUDS */
    return FALSE;
#endif /* INCLUDE_CASE_COMMS && HAVE_CC_MODE_EARBUDS */
}

uint8 batteryGaiaPluginCase_GetCaseBatteryState(void)
{
    return CcWithCase_GetCaseBatteryState();
}

#endif /* INCLUDE_GAIA */
