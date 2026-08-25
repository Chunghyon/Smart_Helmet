/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       battery_gaia_plugin.h
\brief      Header file for the gaia battery plugin that implements the battery GAIA feature.
            That feature enables a GAIA client to fetch the battery level(s) of a device
            and its dependant device(s) - if any.
*/

#ifndef BATTERY_GAIA_PLUGIN_H_
#define BATTERY_GAIA_PLUGIN_H_

#ifdef INCLUDE_GAIA
#include <gaia_features.h>
#include <gaia_framework.h>


/*! \brief Gaia battery plugin version
*/
#define BATTERY_GAIA_PLUGIN_VERSION (1)


/*! \brief IDs for the commands of the GAIA battery feature.
*/
typedef enum
{
    /*! Finds out all the batteries that are supported */
    get_supported_batteries = 0,
    /*! Gets the levels of the batteries passed as parameters */
    get_battery_levels = 1,
    /*! Total number of commands */
    number_of_battery_commands = 2
} battery_plugin_pdu_ids_t;

/*! \brief Battery types that are supported by this plugin.
*/
typedef enum
{
    single_device_battery = 0,
    left_device_battery = 1,
    right_device_battery = 2,
    charger_case_battery = 3,
    number_of_battery_types = 4
} battery_plugin_battery_types_t;

/*! \brief 'Get Battery levels' response format.
           The response contains a list of pairs of (battery, level in percent).
           Response Payload Format:
           0         1         2         3         4 ... (Byte offset)
           +---------+---------+---------+---------+---
           |      (pair 1)     | (optional pair 2) | ...
           | batt ID | level % | batt ID | level % |
           +---------+---------+---------+---------+---
*/
typedef enum
{
    get_battery_levels_response_battery_pair_offset = 0,
    get_battery_levels_response_level_pair_offset = 1,
    get_battery_levels_response_pair_length = 2
} battery_plugin_get_battery_levels_response_t;

/*! \brief Maximum length the response to 'Get Battery Levels' can be */
#define get_battery_levels_response_length_max (number_of_battery_types * get_battery_levels_response_pair_length)

/*! \brief Battery level value to return when a battery type is supported and cannot
           be determined.
*/
#define BATTERY_LEVEL_UNKNOWN     (0xFF)


/*! \brief GAIA battery plugin initialisation function.
           This function registers the command handler for the GAIA Battery feature.
           The Application that uses this Feature must call this function before
           using any functions of the Feature.
*/
bool BatteryGaiaPlugin_Init(Task init_task);


#endif /* INCLUDE_GAIA */

#endif /* BATTERY_GAIA_PLUGIN_H_ */
