/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the statistics framework spatial audio stats handler
*/

#ifndef STATISTICS_GAIA_PLUGIN_SPATIAL_AUDIO_H
#define STATISTICS_GAIA_PLUGIN_SPATIAL_AUDIO_H

#if defined(INCLUDE_STATISTICS) && defined(INCLUDE_SPATIAL_AUDIO) && defined(INCLUDE_SPATIAL_DATA) && defined(INCLUDE_ATTITUDE_FILTER)

typedef enum
{
    statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_w = 0x01,
    statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_x = 0x02,
    statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_y = 0x03,
    statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_quaternion_z = 0x04,

    statistics_gaia_plugin_handlers_spatial_audio_statistic_ids_number_of_commands = 4,
} statistics_gaia_plugin_handlers_spatial_audio_ids_t;

/*! \brief Registers the spatial audio statistics handler

    \param  init_task       Task passed on initialisation by earbud_init/headset_init.
                            Currently unused.
*/
bool StatisticsGaiaPluginHandlersSpatialAudio_Init(Task init_task);

#endif //INCLUDE_STATISTICS INCLUDE_SPATIAL_AUDIO INCLUDE_SPATIAL_DATA

#endif // STATISTICS_GAIA_PLUGIN_SPATIAL_AUDIO_H
