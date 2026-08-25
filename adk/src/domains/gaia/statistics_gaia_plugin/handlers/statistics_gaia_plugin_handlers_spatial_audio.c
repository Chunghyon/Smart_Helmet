/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the statistics framework spatial audio stats handler
*/

#include "statistics_gaia_plugin_handlers_spatial_audio.h"
#include "statistics_gaia_plugin_utils.h"
#include "statistics_gaia_plugin.h"

#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#if defined(INCLUDE_STATISTICS) && defined(INCLUDE_SPATIAL_AUDIO) && defined(INCLUDE_SPATIAL_DATA) && defined(INCLUDE_ATTITUDE_FILTER)

#include "spatial_data.h"
#include "context_framework.h"

static const statistics_gaia_plugin_statistic_id_t
    statistics_gaia_plugin_handlers_spatial_audio_supported[4] =
{    statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_w,
     statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_x,
     statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_y,
     statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_z
};

static size_t statisticsGaiaPluginHandlersSpatialAudio_GetSupportedStatisticIDs(statistics_gaia_plugin_statistic_id_t *ids, size_t max_length)
{
    size_t to_copy = MIN(max_length, (size_t)sizeof (statistics_gaia_plugin_handlers_spatial_audio_supported));
    memcpy(ids, statistics_gaia_plugin_handlers_spatial_audio_supported, to_copy);
    return to_copy;
}

static size_t statisticsGaiaPluginHandlersSpatialAudio_GetStatisticValue(statistics_gaia_plugin_statistic_id_t statistic_id, uint8* value, size_t max_length)
{
    context_spatial_audio_info_t status = {0};
    if (!ContextFramework_GetContextItem(context_spatial_audio_info, (void *)&status, sizeof (context_spatial_audio_info_t)))
    {
        DEBUG_LOG_INFO("statisticsGaiaPluginHandlersSpatialAudio_GetStatisticValue id %d Context framework return false", statistic_id);
        return 0;
    }

    DEBUG_LOG_VERBOSE("statisticsGaiaPluginHandlersSpatialAudio_GetStatisticValue id:%d enum:spatial_data_report_id_t:0x%x", statistic_id, status.report_id);

    quaternion_data_t *quaternions = NULL;

    switch (status.report_id)
    {
#ifndef INCLUDE_3DAMP
    case spatial_data_report_1:
        quaternions = &(status.report_data.quat_data.quaternion);
        break;
#else
    case spatial_data_report_1bt:
        quaternions = &(status.report_data.quat_data.quaternion);
        break;
    case spatial_data_report_debug:
        quaternions = &(status.report_data.debug_data.quaternion);
        break;
#endif
    default:
        break;
    }

    if (!quaternions)
    {
        DEBUG_LOG_INFO("statisticsGaiaPluginHandlersSpatialAudio_GetStatisticValue id %d quaternions not set", statistic_id);
        return 0;
    }

    size_t return_length = 0;

    switch (statistic_id)
    {
    case statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_w:
        return_length = StatisticsGaiaPluginUtils_ReturnInt16(quaternions->w, value, max_length);
        break;
    case statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_x:
        return_length = StatisticsGaiaPluginUtils_ReturnInt16(quaternions->x, value, max_length);
        break;
    case statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_y:
        return_length = StatisticsGaiaPluginUtils_ReturnInt16(quaternions->y, value, max_length);
        break;
    case statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_z:
        return_length = StatisticsGaiaPluginUtils_ReturnInt16(quaternions->z, value, max_length);
        break;
    default:
        break;
    }

    return return_length;
}

bool StatisticsGaiaPluginHandlersSpatialAudio_Init(Task init_task)
{
    UNUSED(init_task);
    static const statistics_gaia_plugin_handler_functions_t functions =
    {
        .get_supported_statistic_ids_handler = statisticsGaiaPluginHandlersSpatialAudio_GetSupportedStatisticIDs,
        .get_statistic_value_handler = statisticsGaiaPluginHandlersSpatialAudio_GetStatisticValue,
    };

    DEBUG_LOG_VERBOSE("StatisticsGaiaPluginHandlersSpatialAudio_Init");

    return StatisticsGaiaPlugin_RegisterCategoryHandler(statistics_gaia_plugin_handler_ids_spatial_audio, &functions);
}

#endif
