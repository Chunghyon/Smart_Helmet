/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       battery_gaia_plugin_case.h
\brief      Header file to help the battery GAIA plugin to get information about the case
            battery if supported.
*/

#ifndef BATTERY_GAIA_PLUGIN_CHARGER_CASE_H_
#define BATTERY_GAIA_PLUGIN_CHARGER_CASE_H_

#ifdef INCLUDE_GAIA
#include <cc_with_case.h>

/*! \brief Function that determines if the case battery is supported by the device.

    \return TRUE if the device has access to a case battery information, FALSE otherwise.
*/
bool batteryGaiaPluginCase_IsCaseBatterySupported(void);

/*! \brief Get the state of the battery state by wrapping the call to #CcWithCase_GetCaseBatteryState
           For more information, see: #CcWithCase_GetCaseBatteryState


    \return The state of the battery as a combination of the battery level in percent and if the device
            is charging, examples:
                0b10010100 (0x94) -> level is  20% and charging.
                0b01100100 (0x64) -> level is 100% and not charging.

            It may return #BATTERY_STATUS_UNKNOWN if the state cannot be determined or the case
            battery is not supported.
*/
uint8 batteryGaiaPluginCase_GetCaseBatteryState(void);

#endif /* INCLUDE_GAIA */

#endif /* BATTERY_GAIA_PLUGIN_CHARGER_CASE_H_ */
