/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the statistics streaming stats handler
*/

#ifndef STATISTICS_GAIA_PLUGIN_STREAMING_H
#define STATISTICS_GAIA_PLUGIN_STREAMING_H

#if defined(INCLUDE_STATISTICS)

typedef enum
{
    statistics_gaia_plugin_handlers_streaming_statistic_ids_codec_type = 0x01,
    statistics_gaia_plugin_handlers_streaming_statistic_ids_lossless_enabled = 0x02,
    statistics_gaia_plugin_handlers_streaming_statistic_ids_bitrate = 0x03,
    statistics_gaia_plugin_handlers_streaming_statistic_ids_primary_rssi = 0x04,
    statistics_gaia_plugin_handlers_streaming_statistic_ids_primary_link_quality = 0x05,
    statistics_gaia_plugin_handlers_streaming_statistic_ids_number_of_commands = 5,
} statistics_gaia_plugin_handlers_streaming_statistic_ids_t;

typedef enum
{
    statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_unknown,
    statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_sbc,
    statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aac,
    statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aptx,
    statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aptx_hd,
    statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_aptx_adaptive,
} statistics_gaia_plugin_handlers_streaming_statistic_a2dp_codec_t;

/*! \brief Registers the streaming statistics handler

    \param  init_task       Task passed on initialisation by earbud_init/headset_init.
                            Currently unused.
*/
bool StatisticsGaiaPluginHandlersStreaming_Init(Task init_task);

#endif //INCLUDE_STATISTICS
#endif // STATISTICS_GAIA_PLUGIN_STREAMING_H
