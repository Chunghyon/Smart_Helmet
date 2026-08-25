/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the statistics framework plugin
*/

#include "statistics_gaia_plugin.h"
#include "gaia_features.h"
#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include <byte_utils.h>

#define RESPONSE_MAX_PAYLOAD_LENGTH 48
#define RESPONSE_MIN_PAYLOAD_LENGTH 10

#define GET_AVAILABLE_CATEGORIES_COMMAND_PAYLOAD_LENGTH 2
#define GET_AVAILABLE_CATEGORIES_COMMAND_START_OFFSET 1

#define MAX_STATISTIC_ID_VALUE 255
#define GET_AVAILABLE_STATISTICS_IN_CATEGORY_COMMAND_PAYLOAD_LENGTH 3
#define GET_AVAILABLE_STATISTICS_IN_CATEGORY_COMMAND_START_OFFSET 3

#define GET_STATISTICS_VALUES_COMMAND_PAYLOAD_ENTRY_LENGTH 3


#define STATISTICS_VALUES_RESPONSE_CATEGORY_ID_OFFSET 0
#define STATISTICS_VALUES_RESPONSE_FLAGS_OFFSET 1

#define STATISTICS_VALUE_MAX_VALUE_LENGTH 4

#define STATISTICS_VALUE_CATEGORY_ID_OFFSET 0
#define STATISTICS_VALUE_CATEGORY_ID_SIZE 2

#define STATISTICS_VALUE_STATISTIC_ID_OFFSET_NO_CATEGORY 0
#define STATISTICS_VALUE_FLAGS_OFFSET_NO_CATEGORY 1
#define STATISTICS_VALUE_LENGTH_OFFSET_NO_CATEGORY 2
#define STATISTICS_VALUE_VALUE_OFFSET_NO_CATEGORY 3

#define STATISTICS_VALUE_BASE_SIZE_NO_CATEGORY 3
#define STATISTICS_VALUE_BASE_SIZE_WITH_CATEGORY 5

#if defined(INCLUDE_STATISTICS)

static void statisticsGaiaPlugin_SendResponse(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 length, const uint8 *payload)
{
    GaiaFramework_SendResponse(t, GAIA_STATISTICS_FEATURE_ID, pdu_id, length, payload);
}

static void statisticsGaiaPlugin_SendError(GAIA_TRANSPORT *t, uint8 pdu_id, uint8 status_code)
{
    GaiaFramework_SendError(t, GAIA_STATISTICS_FEATURE_ID, pdu_id, status_code);
}

static uint16 statisticsGaiaPlugin_GetMaxPayloadSize(GAIA_TRANSPORT *t)
{
    uint16 size = GaiaFramework_GetPacketSpace(t);
    return MIN(RESPONSE_MAX_PAYLOAD_LENGTH, size);
}

static void statisticsGaiaPlugin_Write2Bytes(uint8 *dst, uint16 offset ,uint16 value)
{
    dst[offset] = value >> 8;
    dst[offset + 1] = value;
}

static void statisticsGaiaPlugin_WriteStatisticValueEntry(uint8 *dst,
                                                          uint16 offset,
                                                          statistics_gaia_plugin_category_id_t category_id,
                                                          statistics_gaia_plugin_statistic_id_t statistic_id,
                                                          uint8 flags,
                                                          uint8 value_length,
                                                          uint8 *value)
{
    uint16 startOffset = offset;
    if (category_id != 0)
    {
         statisticsGaiaPlugin_Write2Bytes(dst, offset + STATISTICS_VALUE_CATEGORY_ID_OFFSET, category_id);
         startOffset += STATISTICS_VALUE_CATEGORY_ID_SIZE;
    }

    dst[startOffset + STATISTICS_VALUE_STATISTIC_ID_OFFSET_NO_CATEGORY] = statistic_id;
    dst[startOffset + STATISTICS_VALUE_FLAGS_OFFSET_NO_CATEGORY]= flags;
    dst[startOffset + STATISTICS_VALUE_LENGTH_OFFSET_NO_CATEGORY] = value_length;
    if (value_length > 0)
    {
        memcpy(dst + startOffset + STATISTICS_VALUE_VALUE_OFFSET_NO_CATEGORY, value, value_length);
    }
}

static void statisticsGaiaPlugin_HandleGetStatisticsValuesCommand(GAIA_TRANSPORT *t ,uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("statisticsGaiaPlugin_HandleGetStatisticsValuesCommand");
    if (payload_length%GET_STATISTICS_VALUES_COMMAND_PAYLOAD_ENTRY_LENGTH != 0)
    {
        DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetStatisticsValuesCommand, payload incorrect length");
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_statistics_values_command, invalid_parameter);
        return;
    }

    uint16 max_response_payload_size = statisticsGaiaPlugin_GetMaxPayloadSize(t);
    if (max_response_payload_size < RESPONSE_MIN_PAYLOAD_LENGTH)
    {
        DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetStatisticsValuesCommand, payload length insufficient");
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_statistics_values_command, failed_insufficient_resources);
        return;
    }

    uint8 *response = PanicUnlessMalloc(max_response_payload_size * sizeof (uint8));
    uint16 response_size = 1;

    uint16 payload_index = 0;
    bool response_too_big = FALSE;

    uint8 statistic_value[STATISTICS_VALUE_MAX_VALUE_LENGTH];
    size_t value_size = 0;

    while (payload_index < payload_length && !response_too_big)
    {
        statistics_gaia_plugin_handler_category_ids_t category = ByteUtilsGet2BytesFromStream(payload + payload_index);
        statistics_gaia_plugin_statistic_id_t statistics_id = payload[payload_index + 2];
        payload_index += 3;

        const statistics_gaia_plugin_handler_functions_t *functions = StatisticsGaiaPluginHandlerManager_GetHandler(category);

        if (functions == NULL ||
                functions->get_statistic_value_handler == NULL ||
                functions->get_supported_statistic_ids_handler == NULL)
        {
            free(response);
            DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetStatisticsValuesCommand, not registered");
            statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_statistics_values_command, invalid_parameter);
            return;
        }

        // Fetch value and create entry
        value_size = functions->get_statistic_value_handler(statistics_id, statistic_value, sizeof (statistic_value));

        if (response_size + value_size + STATISTICS_VALUE_BASE_SIZE_WITH_CATEGORY > max_response_payload_size)
        {
            response_too_big = TRUE;
        }
        else
        {
            statisticsGaiaPlugin_WriteStatisticValueEntry(response,
                                                          response_size,
                                                          category,
                                                          statistics_id,
                                                          0,
                                                          value_size,
                                                          statistic_value);

            response_size += value_size + STATISTICS_VALUE_BASE_SIZE_WITH_CATEGORY;
        }

    }

    response[0] = response_too_big ? 0x01 : 0x00;

    statisticsGaiaPlugin_SendResponse(t, statistics_gaia_plugin_get_statistics_values_command, response_size, response);
    free(response);
}

static void statisticsGaiaPlugin_HandleGetStatisticsInCategoryCommand(GAIA_TRANSPORT *t ,uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("statisticsGaiaPlugin_HandleGetStatisticsInCategoryCommand");
    if (payload_length != GET_AVAILABLE_STATISTICS_IN_CATEGORY_COMMAND_PAYLOAD_LENGTH)
    {
        DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetStatisticsInCategoryCommand, payload incorrect length");
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_statistics_in_category_command, invalid_parameter);
        return;
    }

    statistics_gaia_plugin_handler_category_ids_t category = ByteUtilsGet2BytesFromStream(payload);
    statistics_gaia_plugin_statistic_id_t last_statistic_id = payload[2];

    const statistics_gaia_plugin_handler_functions_t *functions = StatisticsGaiaPluginHandlerManager_GetHandler(category);

    if (functions == NULL ||
            functions->get_statistic_value_handler == NULL ||
            functions->get_supported_statistic_ids_handler == NULL)
    {
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_statistics_in_category_command, invalid_parameter);
        return;
    }

    uint16 max_response_payload_size = statisticsGaiaPlugin_GetMaxPayloadSize(t);
    if (max_response_payload_size < RESPONSE_MIN_PAYLOAD_LENGTH)
    {
        DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetStatisticsInCategoryCommand, payload length insufficient");
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_statistics_in_category_command, failed_insufficient_resources);
        return;
    }
    uint8 *response = PanicUnlessMalloc(max_response_payload_size * sizeof (uint8));

    statistics_gaia_plugin_statistic_id_t statistic_ids[MAX_STATISTIC_ID_VALUE + 1];
    size_t list_size = functions->get_supported_statistic_ids_handler(statistic_ids, sizeof (statistic_ids));

    statisticsGaiaPlugin_Write2Bytes(response, 1, category);

    uint16 response_size = GET_AVAILABLE_STATISTICS_IN_CATEGORY_COMMAND_START_OFFSET;
    bool more = FALSE;

    if (list_size > 0)
    {
        uint8 statistic_value[STATISTICS_VALUE_MAX_VALUE_LENGTH];
        size_t value_size = 0;
        for (uint16 index = 0; index < list_size; index ++)
        {
            if (statistic_ids[index] > last_statistic_id)
            {
                // Fetch value and create entry
                value_size = functions->get_statistic_value_handler(statistic_ids[index], statistic_value, sizeof (statistic_value));

                if (response_size + value_size + STATISTICS_VALUE_BASE_SIZE_NO_CATEGORY > max_response_payload_size)
                {
                    more = TRUE;
                    break;
                }

                statisticsGaiaPlugin_WriteStatisticValueEntry(response,
                                                              response_size,
                                                              0,
                                                              statistic_ids[index],
                                                              0,
                                                              value_size,
                                                              statistic_value);

                response_size += value_size + STATISTICS_VALUE_BASE_SIZE_NO_CATEGORY;
            }
        }
    }

    response[0] = more ? 0x01 : 0x00;

    statisticsGaiaPlugin_SendResponse(t, statistics_gaia_plugin_get_statistics_in_category_command, response_size, response);
    free(response);
}

static void statisticsGaiaPlugin_HandleGetAvailableCategoriesCommand(GAIA_TRANSPORT *t ,uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_VERBOSE("statisticsGaiaPlugin_HandleGetAvailableCategoriesCommand");
    if (payload_length != GET_AVAILABLE_CATEGORIES_COMMAND_PAYLOAD_LENGTH)
    {
        DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetAvailableCategoriesCommand, payload incorrect length: %u", payload_length);
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_available_categories_command, invalid_parameter);
        return;
    }

    statistics_gaia_plugin_category_id_t last_category_id = ByteUtilsGet2BytesFromStream(payload);
    uint16 max_response_payload_size = statisticsGaiaPlugin_GetMaxPayloadSize(t);
    if (max_response_payload_size < RESPONSE_MIN_PAYLOAD_LENGTH)
    {
        DEBUG_LOG_ERROR("statisticsGaiaPlugin_HandleGetAvailableCategoriesCommand, payload length insufficient");
        statisticsGaiaPlugin_SendError(t, statistics_gaia_plugin_get_available_categories_command, failed_insufficient_resources);
        return;
    }

    uint8 *response = PanicUnlessMalloc(max_response_payload_size * sizeof (uint8));
    size_t list_size = StatisticsGaiaPluginHandlerManager_GetNumberOfRegisteredHandlers();

    uint16 response_size = GET_AVAILABLE_CATEGORIES_COMMAND_START_OFFSET;
    bool more = FALSE;

    if (list_size > 0)
    {
        statistics_gaia_plugin_category_id_t ids[list_size];

        size_t fetched_size = StatisticsGaiaPluginHandlerManager_GetRegisteredIDs(ids, list_size);

        for (uint16 index = 0; index < fetched_size; index ++)
        {
            if (ids[index] > last_category_id)
            {
                if (response_size + 2 > max_response_payload_size)
                {
                    more = TRUE;
                    break;
                }

                statisticsGaiaPlugin_Write2Bytes(response, response_size, ids[index]);
                response_size += 2;
            }
        }
    }

    response[0] = more ? 0x01 : 0x00;

    statisticsGaiaPlugin_SendResponse(t, statistics_gaia_plugin_get_available_categories_command, response_size, response);
    free(response);
}

static gaia_framework_command_status_t statisticsGaiaPlugin_CommandHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    switch(pdu_id)
    {
    case statistics_gaia_plugin_get_available_categories_command:
        statisticsGaiaPlugin_HandleGetAvailableCategoriesCommand(t, payload_length, payload);
        break;
    case statistics_gaia_plugin_get_statistics_in_category_command:
        statisticsGaiaPlugin_HandleGetStatisticsInCategoryCommand(t, payload_length, payload);
        break;
    case statistics_gaia_plugin_get_statistics_values_command:
        statisticsGaiaPlugin_HandleGetStatisticsValuesCommand(t, payload_length, payload);
        break;
    default:
        DEBUG_LOG_WARN("statisticsGaiaPlugin_CommandHandler Invalid command ID enum:ui_user_config_plugin_pdu_ids_t:%d", pdu_id);
        return command_not_handled;
    }
    return command_handled;
}

bool StatisticsGaiaPlugin_RegisterCategoryHandler(statistics_gaia_plugin_handler_category_ids_t category_id,
                                                 const statistics_gaia_plugin_handler_functions_t *functions)
{
    return StatisticsGaiaPluginHandlerManager_AddToList(category_id, functions);
}

bool StatisticsGaiaPlugin_Init(Task init_task)
{
    UNUSED(init_task);
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler        = statisticsGaiaPlugin_CommandHandler,
        .send_all_notifications = NULL,
        .transport_connect      = NULL,
        .transport_disconnect   = NULL,
        .handover_veto          = NULL,
        .handover_abort         = NULL,
        .handover_complete      = NULL
    };

    DEBUG_LOG_VERBOSE("StatisticsGaiaPlugin_Init");

    GaiaFramework_RegisterFeature(GAIA_STATISTICS_FEATURE_ID, STATISTICS_GAIA_PLUGIN_VERSION, &functions);

    StatisticsGaiaPluginHandlerManager_Init();

    return TRUE;
}

#endif // INCLUDE_STATISTICS
