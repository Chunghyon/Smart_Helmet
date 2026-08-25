/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies, Inc. and/or its subsidiaries.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the GAIA battery plugin
*/


#ifdef INCLUDE_GAIA
#include "battery_gaia_plugin.h"

#include <battery_gaia_plugin_case.h>
#include <gaia_framework_feature.h>
#include <logging.h>
#include <multidevice.h>
#include <state_of_charge.h>
#include <state_proxy.h>


/*! \brief GAIA Battery feature command handler function.
           This handler must be registered with the GAIA framework.
           It is called when the GAIA framework receives a Battery feature command.

    \param t               GAIA transport the command was received on.
                           Allows to reply with a reponse or an error.

    \param pdu_id           ID (7 bits) of the command to handle.

    \param payload_length  Payload size in bytes.

    \param payload         Payload data.

    \return The handling status of the command.
 */
static gaia_framework_command_status_t batteryGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

/*! \brief Function to handle 'Get Supported Batteries' command.
           This function builds and sends the response payload to the command.
           The response payload contains a list of battery_plugin_battery_types_t
           represented as uint8 values.

    \param t    GAIA transport the command was received on.
                Allows to send the response.
 */
static void batteryGaiaPlugin_GetSupportedBatteries(GAIA_TRANSPORT *t);

/*! \brief Function to handle 'Get Battery levels' command.
           This function builds the response payload and sends it.
           The response payload contains a list of pair of UINT8 values. The first member
           represents the type as one of battery_plugin_battery_types_t and the second
           member represents the level in percent.
           If a battery is not supported or its level cannot be determined, the level is
           set to #BATTERY_LEVEL_UNKNOWN (0xFF).

    \param t               GAIA transport the command was received on.
                           Allows to send the response.

    \param payload_length  Payload size in bytes.

    \param payload         Payload data.
                           The payload is expected to contain a list of
                           battery_plugin_battery_types_t represented as UINT8 values.
 */
static void batteryGaiaPlugin_GetBatteryLevels(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function to retrieve the level of a battery.
           This function obtains the level from other components and converts it to a
           percent value if the obtained value was not already a percent.
           If this function cannot determine the level of the battery it returns
           BATTERY_LEVEL_UNKNOWN (0xFF).

    \param battery  The battery to get the level of.

    \return The level of the given battery, in percent, or 0xFF (BATTERY_LEVEL_UNKNOWN) if
            the battery is not determined or the level could not be determined.
 */
static uint8 batteryGaiaPlugin_GetBatteryLevelInPercent(battery_plugin_battery_types_t battery);

/*! \brief Function to wrap calls and calculation to get the level of the local battery in percent.

    \return The level of the local battery if it could be determined, or 0xFF (BATTERY_LEVEL_UNKNOWN)
            if not determined.
 */
static uint8 batteryGaiaPlugin_GetLocalBatteryLevelInPercent(void);

/*! \brief Function to wrap calls and calculation to get the level of the peer battery in percent.

    \return The level of the peer battery if it could be determined, or 0xFF (BATTERY_LEVEL_UNKNOWN)
            if not determined.
 */
static uint8 batteryGaiaPlugin_GetPeerBatteryLevelInPercent(void);

/*! \brief Function to wrap calls and calculation to get the level of the case battery in percent.

    \return The level of the case battery if it could be determined, or 0xFF (BATTERY_LEVEL_UNKNOWN)
            if not determined.
 */
static uint8 batteryGaiaPlugin_GetCaseBatteryLevelInPercent(void);


/* Core functions of the Battery plugin */

bool BatteryGaiaPlugin_Init(Task init_task)
{
    DEBUG_LOG_INFO("BatteryGaiaPlugin_Init");

    UNUSED(init_task);

    /* Registering */
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = batteryGaiaPlugin_MainHandler,
    };

    GaiaFramework_RegisterFeature(GAIA_BATTERY_FEATURE_ID, BATTERY_GAIA_PLUGIN_VERSION, &functions);

    return TRUE;
}

static gaia_framework_command_status_t batteryGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_INFO("batteryGaiaPlugin_MainHandler, called for command=%u", pdu_id);

    switch (pdu_id)
    {
        case get_supported_batteries:
            batteryGaiaPlugin_GetSupportedBatteries(t);
            break;

        case get_battery_levels:
            batteryGaiaPlugin_GetBatteryLevels(t, payload_length, payload);
            break;

        default:
            DEBUG_LOG_ERROR("batteryGaiaPlugin_MainHandler, unhandled call for command=%u", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}


/* Wrapping functions of GAIA framework functions for BATTERY feature */

/*! \brief Calls GaiaFramework_SendResponse with the GAIA Battery feature ID.

    \param transport  Transport to send response on.

    \param pdu_id     PDU command ID for the response.

    \param length     Length of the payload.

    \param payload    Payload data.
*/
static void batteryGaiaPlugin_SendResponse(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 length, const uint8 *payload)
{
    GaiaFramework_SendResponse(t, GAIA_BATTERY_FEATURE_ID, pdu_id, length, payload);
}

/*! \brief Calls GaiaFramework_SendError with the GAIA Battery feature ID.

    \param transport    Transport to send response on.

    \param pdu_id       PDU command ID for the error.

    \param status_code  The status to send.
*/
static void batteryGaiaPlugin_SendError(GAIA_TRANSPORT *t, uint8 pdu_id, uint8 status_code)
{
    GaiaFramework_SendError(t, GAIA_BATTERY_FEATURE_ID, pdu_id, status_code);
}


/* Functions to handle commands */

static void batteryGaiaPlugin_GetSupportedBatteries(GAIA_TRANSPORT *t)
{
    const multidevice_type_t type = Multidevice_GetType();
    const bool hasCase = batteryGaiaPluginCase_IsCaseBatterySupported();

    uint8 result[number_of_battery_types];
    uint16 result_size = 1                                /* local battery */
            + ((type == multidevice_type_pair) ? 1 : 0)   /* peer battery if pair of devices */
            + (hasCase ? 1 : 0);                          /* case battery if case is present */

    PanicFalse(result_size <= number_of_battery_types);

    /* add audio device battery/ies */
    if (type == multidevice_type_pair)
    {
        result[0] = (uint8) left_device_battery;
        result[1] = (uint8) right_device_battery;
    }
    else
    {
        result[0] = (uint8) single_device_battery;
    }

    /* add support of case battery if present */
    if (hasCase)
    {
        result[result_size - 1] = (uint8) charger_case_battery;
    }

    DEBUG_LOG_INFO("batteryGaiaPlugin_GetSupportedBatteries, size=%u, type=%u, case=%u", result_size, type, hasCase);

    batteryGaiaPlugin_SendResponse(t, get_supported_batteries, result_size, result);
}

static void batteryGaiaPlugin_GetBatteryLevels(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    uint16 result_size;
    uint8 result[get_battery_levels_response_length_max];
    uint16 payload_max_length;
    uint32 transport_payload_size;
    uint16 batteries_count = payload_length;

    if (batteries_count == 0)
    {
        DEBUG_LOG_ERROR("batteryGaiaPlugin_GetBatteryLevels, sending 'invalid_parameter': payload is empty");
        batteryGaiaPlugin_SendError(t, get_battery_levels, invalid_parameter);
        return;
    }

    if (batteries_count > number_of_battery_types)
    {
        DEBUG_LOG_ERROR("batteryGaiaPlugin_GetBatteryLevels, sending 'invalid_parameter': too many arguments");
        batteryGaiaPlugin_SendError(t, get_battery_levels, invalid_parameter);
        return;
    }

    DEBUG_LOG_INFO("batteryGaiaPlugin_GetBatteryLevels, called for batteries_count=%u", batteries_count);

    result_size = batteries_count * get_battery_levels_response_pair_length;
    payload_max_length = GaiaFramework_GetPacketSpace(t);

    if (!Gaia_TransportGetInfo(t, GAIA_TRANSPORT_PAYLOAD_SIZE, &transport_payload_size))
    {
        DEBUG_LOG_ERROR("batteryGaiaPlugin_GetBatteryLevels, sending 'incorrect_state': getting transport payload size returned FALSE.");
        batteryGaiaPlugin_SendError(t, get_battery_levels, incorrect_state);
        return;
    }

    payload_max_length = MIN(payload_max_length, ((uint16)transport_payload_size));

    if (payload_max_length < result_size)
    {
        DEBUG_LOG_ERROR("batteryGaiaPlugin_GetBatteryLevels, sending 'invalid_parameter': response cannot fit in a payload: batteries_count=%u, result_size=%u, max_length=%u", batteries_count, result_size, payload_max_length);
        batteryGaiaPlugin_SendError(t, get_battery_levels, invalid_parameter);
        return;
    }

    for (int i = 0; i < batteries_count; i++)
    {
        const battery_plugin_battery_types_t battery = payload[i];
        uint8 level_percent = batteryGaiaPlugin_GetBatteryLevelInPercent(battery);
        uint16 offset = get_battery_levels_response_pair_length * i;

        result[offset + get_battery_levels_response_battery_pair_offset] = (uint8) battery;
        result[offset + get_battery_levels_response_level_pair_offset] = level_percent;
    }

    batteryGaiaPlugin_SendResponse(t, get_battery_levels, result_size, result);
}


/* Other functions */

static uint8 batteryGaiaPlugin_GetBatteryLevelInPercent(battery_plugin_battery_types_t battery)
{
    const multidevice_side_t side = Multidevice_GetSide();
    uint8 level_percent;

    if ((battery == single_device_battery && side == multidevice_side_both)
            || (battery == left_device_battery && side == multidevice_side_left)
            || (battery == right_device_battery && side == multidevice_side_right))
    {
        /* level of local battery */
        level_percent = batteryGaiaPlugin_GetLocalBatteryLevelInPercent();
    }

    else if ((battery == left_device_battery && side == multidevice_side_right)
             || (battery == right_device_battery && side == multidevice_side_left))
    {
        /* level of peer battery */
        level_percent = batteryGaiaPlugin_GetPeerBatteryLevelInPercent();
    }
    else if (battery == charger_case_battery && batteryGaiaPluginCase_IsCaseBatterySupported())
	{
        /* level of case battery */
        level_percent = batteryGaiaPlugin_GetCaseBatteryLevelInPercent();
    }
    else
    {
        /* battery is not supported -> level is unknown */
        level_percent = BATTERY_LEVEL_UNKNOWN;
        DEBUG_LOG_VERBOSE("batteryGaiaPlugin_GetBatteryLevelInPercent, battery=%u not supported", battery);
    }

    DEBUG_LOG_VERBOSE("batteryGaiaPlugin_GetBatteryLevelInPercent, battery level: %u%%", level_percent);

    return level_percent;
}

static uint8 batteryGaiaPlugin_GetLocalBatteryLevelInPercent(void)
{
    uint8 level_percent = Soc_GetBatterySoc();
    level_percent = (level_percent <= 100) ? level_percent : BATTERY_LEVEL_UNKNOWN;

    return level_percent;
}

static uint8 batteryGaiaPlugin_GetPeerBatteryLevelInPercent(void)
{
    uint8 level_percent;
    uint16 local_battery_level;
    uint16 peer_battery_level;

    StateProxy_GetLocalAndRemoteBatteryLevels(&local_battery_level, &peer_battery_level);

    level_percent = Soc_ConvertLevelToPercentage(peer_battery_level);
    level_percent = (level_percent <= 100) ? level_percent : BATTERY_LEVEL_UNKNOWN;

    return level_percent;
}

static uint8 batteryGaiaPlugin_GetCaseBatteryLevelInPercent(void)
{
    uint8 level_percent;
    const uint8 battery_state = batteryGaiaPluginCase_GetCaseBatteryState();
    const uint8 level_mask = 0x7f;

    level_percent = battery_state == BATTERY_STATUS_UNKNOWN ? BATTERY_LEVEL_UNKNOWN
                                                            : battery_state & level_mask;
    level_percent = (level_percent <= 100) ? level_percent : BATTERY_LEVEL_UNKNOWN;

    return level_percent;
}


#endif /* INCLUDE_GAIA */
