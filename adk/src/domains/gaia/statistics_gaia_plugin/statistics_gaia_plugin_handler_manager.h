/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the statistics framework plugin handler
*/

#ifndef STATISTICS_GAIA_PLUGIN_HANDLER_H
#define STATISTICS_GAIA_PLUGIN_HANDLER_H

#include <gaia_features.h>
#include <gaia_framework.h>

typedef enum
{
    statistics_gaia_plugin_handler_ids_streaming = 0x0001,                /*<! Streaming Statistics Handler */
    statistics_gaia_plugin_handler_ids_spatial_audio = 0x0002,            /*<! Spatial Audio Statistics Handler */
} statistics_gaia_plugin_handler_category_ids_t;


typedef uint8 statistics_gaia_plugin_statistic_id_t;
typedef uint16 statistics_gaia_plugin_category_id_t;

typedef struct
{
    /*! \brief Fetches a list of supported statistic ids.

        \param  ids              Buffer for list of supported ids
        \param  max_length       Maximum number of bytes that ids param can accommodate

        \return The number of bytes used in ids buffer.
    */
    size_t (*get_supported_statistic_ids_handler)(statistics_gaia_plugin_statistic_id_t *ids, size_t max_length);
    /*! \brief Fetches the value for a statistic

        \param  statistic_id     Statistic ID of the requested statistic
        \param  value            Buffer for value of statistic
        \param  max_length       Maximum number of bytes that value param can accommodate

        \return The number of bytes used in value buffer. 0 if value is unavailable/unsupported etc
    */
    size_t (*get_statistic_value_handler)(statistics_gaia_plugin_statistic_id_t statistic_id, uint8* value, size_t max_length );
} statistics_gaia_plugin_handler_functions_t;

#if defined(INCLUDE_STATISTICS)

/*! \brief Initialisation function for the handlers list
*/
void StatisticsGaiaPluginHandlerManager_Init(void);

/*! \brief Removes all registered handlers. Provided for testing
*/
void StatisticsGaiaPluginHandlerManager_Reset(void);

/*! \brief Adds a new handler to the handlers list. Handlers must be added in increasing order of category ID.

    \param  handler_id          Handler(Category) ID of the handler to be registered
    \param  functions           Command handlers of the handler to be registered

    \return True if successful 
*/
bool StatisticsGaiaPluginHandlerManager_AddToList(statistics_gaia_plugin_handler_category_ids_t category_id,
                                                  const statistics_gaia_plugin_handler_functions_t *functions);

/*! \brief Returns command functions for a category if registered

    \param  handler_id          Handler(Category) ID of the handler to be registered

    \return The registered handler or NULL if none registered
*/
const statistics_gaia_plugin_handler_functions_t* StatisticsGaiaPluginHandlerManager_GetHandler(statistics_gaia_plugin_handler_category_ids_t category_id);

/*! \brief Returns list of registered category ids

    \param  list          The list of category ids that will be filled on return
    \param  max_length    The maximum size of the list of category ids that will be filled on return


    \return The size of the list returned
*/
size_t StatisticsGaiaPluginHandlerManager_GetRegisteredIDs(statistics_gaia_plugin_category_id_t *list, size_t max_length);

/*! \brief Returns the current number of handlers registered

    \return The size of the list returned
*/
size_t StatisticsGaiaPluginHandlerManager_GetNumberOfRegisteredHandlers(void);

#endif // INCLUDE_STATISTICS
#endif // STATISTICS_GAIA_PLUGIN_HANDLER_H
