/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for the statistics framework plugin handler
*/

#include "statistics_gaia_plugin_handler_manager.h"
#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#if defined(INCLUDE_STATISTICS)

/*! \brief Handler list node */
struct handler_list_item
{
    /*! Category ID of the plugin to be registered */
    statistics_gaia_plugin_category_id_t category_id;

    /*! Command handler of the category handler to be registered */
    const statistics_gaia_plugin_handler_functions_t *functions;
};

static struct handler_list_item *handler_list = NULL;
static size_t handler_list_count = 0;

void StatisticsGaiaPluginHandlerManager_Init(void)
{
    DEBUG_LOG_INFO("StatisticsGaiaPluginHandler_Init");

    /* Globals should be zero initialised, catch case where we are called
     * a second time */
    PanicFalse(!handler_list);
    PanicFalse(handler_list_count == 0);
}

void StatisticsGaiaPluginHandlerManager_Reset(void)
{
    DEBUG_LOG_INFO("StatisticsGaiaPluginHandlerManager_Reset");

    if (handler_list)
    {
        free(handler_list);
        handler_list = NULL;
        handler_list_count = 0;
    }
}

bool StatisticsGaiaPluginHandlerManager_AddToList(statistics_gaia_plugin_handler_category_ids_t category_id,
                                           const statistics_gaia_plugin_handler_functions_t *functions)
{
    if (category_id == 0)
    {
        DEBUG_LOG_ERROR("StatisticsGaiaPluginHandler_AddToList, category_id %u not added as category_id zero", category_id);
        return FALSE;
    }

    if (functions == NULL ||
            functions->get_statistic_value_handler == NULL ||
            functions->get_supported_statistic_ids_handler == NULL)
    {
        DEBUG_LOG_ERROR("StatisticsGaiaPluginHandler_AddToList, category_id %u not added as functions NULL", category_id);
        return FALSE;
    }

    if ((handler_list_count > 0) &&
        (handler_list[handler_list_count - 1].category_id >= category_id))
    {
        return FALSE;
    }

    size_t new_size = sizeof(struct handler_list_item) * (handler_list_count + 1);
    handler_list = PanicNull(realloc(handler_list, new_size));

    struct handler_list_item *new_entry = &handler_list[handler_list_count];
    new_entry->category_id = category_id;
    new_entry->functions = functions;
    handler_list_count++;

    return TRUE;
}

const statistics_gaia_plugin_handler_functions_t* StatisticsGaiaPluginHandlerManager_GetHandler(statistics_gaia_plugin_handler_category_ids_t category_id)
{
    const statistics_gaia_plugin_handler_functions_t * handler = NULL;
    if (handler_list)
    {
        for(int i=0; i < handler_list_count; i++)
        {
            if(handler_list[i].category_id == category_id)
            {
                handler = handler_list[i].functions;
                break;
            }
        }
    }
    return handler;
}

size_t StatisticsGaiaPluginHandlerManager_GetRegisteredIDs(statistics_gaia_plugin_category_id_t *list, size_t max_length)
{
    size_t length = MIN(handler_list_count, max_length);
    if (handler_list)
    {
        for(int i=0; i < length; i++)
        {
            list[i] = handler_list[i].category_id;
        }
    }

    return length;
}

size_t StatisticsGaiaPluginHandlerManager_GetNumberOfRegisteredHandlers(void)
{
    return handler_list_count;
}

#endif // INCLUDE_STATISTICS
