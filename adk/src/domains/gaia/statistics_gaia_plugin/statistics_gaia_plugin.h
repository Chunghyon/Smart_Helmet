/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the statistics plugin
*/

#ifndef STATISTICS_GAIA_PLUGIN_H_
#define STATISTICS_GAIA_PLUGIN_H_
#include <gaia_features.h>
#include "gaia_framework.h"
#include "statistics_gaia_plugin_handler_manager.h"

#define STATISTICS_GAIA_PLUGIN_VERSION 1

#if defined(INCLUDE_STATISTICS)

typedef enum
{
    statistics_gaia_plugin_get_available_categories_command = 0,
    statistics_gaia_plugin_get_statistics_in_category_command,
    statistics_gaia_plugin_get_statistics_values_command,

    /*! Total number of commands.*/
    number_of_statistics_commands
} statistics_gaia_plugin_command_ids_t;

typedef enum
{
    number_of_statistics_notifications = 0
} statistics_gaia_plugin_notification_ids_t;

/*! \brief Initialises and registers the Statistics plugin with the GAIA framework.

    \param  init_task       Task passed on initialisation by earbud_init/headset_init.
                            Currently unused.
*/
bool StatisticsGaiaPlugin_Init(Task init_task);

/*! \brief Registers a handler for a given statistic category.
    \param  category_id     The category id (number) for the handler.

    \param  functions       Functions used to provide implement the handler. All functions must be implemented (non-NULL).

    \return true if successfully registered.
*/
bool StatisticsGaiaPlugin_RegisterCategoryHandler(statistics_gaia_plugin_handler_category_ids_t category_id,
                                                 const statistics_gaia_plugin_handler_functions_t *functions);

#endif // INCLUDE_STATISTICS

#endif /* STATISTICS_GAIA_PLUGIN_H_*/
