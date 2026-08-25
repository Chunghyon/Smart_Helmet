/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the statistics framework streaming stats handler
*/

#include "statistics_gaia_plugin_handlers_streaming.h"
#include "statistics_gaia_plugin.h"
#include "statistics_gaia_plugin_utils.h"
#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>
#include "context_framework.h"
#include "av_seids.h"

#if defined(INCLUDE_STATISTICS)

static const statistics_gaia_plugin_statistic_id_t
    statistics_gaia_plugin_handlers_streaming_supported[statistics_gaia_plugin_handlers_streaming_statistic_ids_number_of_commands] =
{    statistics_gaia_plugin_handlers_streaming_statistic_ids_codec_type,
     statistics_gaia_plugin_handlers_streaming_statistic_ids_lossless_enabled,
     statistics_gaia_plugin_handlers_streaming_statistic_ids_bitrate,
     statistics_gaia_plugin_handlers_streaming_statistic_ids_primary_rssi,
     statistics_gaia_plugin_handlers_streaming_statistic_ids_primary_link_quality
};


static statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_t statisticsGaiaPluginHandlersStreaming_ConvertSeidToCodec(uint8 seid)
{
    switch(seid)
    {
    case AV_SEID_SBC_SRC:
    case AV_SEID_SBC_SNK:
    case AV_SEID_SBC_MONO_TWS_SNK:
        return statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_sbc;

    case AV_SEID_AAC_SNK:
    case AV_SEID_AAC_STEREO_TWS_SNK:
        return statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aac;

    case AV_SEID_APTX_SNK:
    case AV_SEID_APTX_MONO_TWS_SNK:
        return statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aptx;

    case AV_SEID_APTXHD_SNK:
        return statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aptx_hd;

    case AV_SEID_APTX_ADAPTIVE_SNK:
    case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
        return statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aptx_adaptive;
    default:
        break;
    }

    return statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_unknown;
}

static size_t statisticsGaiaPluginHandlersStreaming_GetSupportedStatisticIDs(statistics_gaia_plugin_statistic_id_t *ids, size_t max_length)
{
    size_t to_copy = MIN(max_length, (size_t)sizeof (statistics_gaia_plugin_handlers_streaming_supported));
    memcpy(ids, statistics_gaia_plugin_handlers_streaming_supported, to_copy);
    return to_copy;
}

static size_t statisticsGaiaPluginHandlersStreaming_GetStatisticValue(statistics_gaia_plugin_statistic_id_t statistic_id, uint8* value, size_t max_length)
{
    DEBUG_LOG_VERBOSE("statisticsGaiaPluginHandlersStreaming_GetStatisticValue id %d", statistic_id);

    context_streaming_info_t status = {0};
    if (!ContextFramework_GetContextItem(context_streaming_info, (void *)&status, sizeof (context_streaming_info_t)))
    {
        return 0;
    }

    size_t return_length = 0;

    switch (statistic_id)
    {
    case statistics_gaia_plugin_handlers_streaming_statistic_ids_codec_type:
        if (status.seid != AV_SEID_INVALID)
        {
            uint8 codec_type = statisticsGaiaPluginHandlersStreaming_ConvertSeidToCodec(status.seid);
            return_length = StatisticsGaiaPluginUtils_ReturnUInt8(codec_type, value, max_length);
        }
        break;
    case statistics_gaia_plugin_handlers_streaming_statistic_ids_lossless_enabled:
        if (status.is_adaptive)
        {
            return_length = StatisticsGaiaPluginUtils_ReturnUInt8(status.is_lossless, value, max_length);
        }
        break;
    case statistics_gaia_plugin_handlers_streaming_statistic_ids_bitrate:
        if (status.bitrate != 0)
        {
            return_length = StatisticsGaiaPluginUtils_ReturnUInt32(status.bitrate, value, max_length);
        }
        break;
    case statistics_gaia_plugin_handlers_streaming_statistic_ids_primary_rssi:
        if (status.primary_rssi != 0)
        {
            return_length = StatisticsGaiaPluginUtils_ReturnInt16(status.primary_rssi, value, max_length);
        }
        break;
    case statistics_gaia_plugin_handlers_streaming_statistic_ids_primary_link_quality:
        if (status.primary_link_quality != 0)
        {
            return_length = StatisticsGaiaPluginUtils_ReturnUInt16(status.primary_link_quality, value, max_length);
        }
        break;
    default:
        break;
    }

    return return_length;
}

bool StatisticsGaiaPluginHandlersStreaming_Init(Task init_task)
{
    UNUSED(init_task);
    static const statistics_gaia_plugin_handler_functions_t functions =
    {
        .get_supported_statistic_ids_handler = statisticsGaiaPluginHandlersStreaming_GetSupportedStatisticIDs,
        .get_statistic_value_handler = statisticsGaiaPluginHandlersStreaming_GetStatisticValue,
    };

    DEBUG_LOG_VERBOSE("StatisticsGaiaPluginHandlersStreaming_Init");

    return StatisticsGaiaPlugin_RegisterCategoryHandler(statistics_gaia_plugin_handler_ids_streaming, &functions);
}

#endif
