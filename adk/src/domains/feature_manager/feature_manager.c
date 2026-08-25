/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation for Feature Manager APIs
*/

#include "feature_manager.h"

#include <logging.h>
#include <panic.h>

#define INDEX_NOT_FOUND (0xFF)

typedef struct {
    feature_id_t id;
    const feature_interface_t * interface;
} feature_manager_client_t;

static const feature_manager_priority_lists_t * priority_lists = NULL;
static feature_manager_handle_t feature_manager_handles[feature_id_max];

static unsigned featureManager_GetIndexForFeatureInPriorityList(unsigned list_index, feature_id_t id);


static bool featureManager_IsHighestPriorityFeatureOnAllLists(feature_id_t id)
{
    bool is_highest_priority_feature = TRUE;

    for(unsigned list_index=0; list_index<priority_lists->number_of_lists; list_index++)
    {
        unsigned feature_index = featureManager_GetIndexForFeatureInPriorityList(list_index, id);

        if (   (INDEX_NOT_FOUND != feature_index)
            && (feature_index > 0))
        {
            is_highest_priority_feature = FALSE;
            break;
        }
    }

    return is_highest_priority_feature;
}

static void featureManager_VerifyClient(feature_manager_client_t * client)
{
    PanicNull((void *)client);
    PanicNull((void *)client->interface);
    PanicNull((void *)client->interface->GetState);

    if(!featureManager_IsHighestPriorityFeatureOnAllLists(client->id))
    {
        PanicNull((void *)client->interface->Suspend);
        PanicNull((void *)client->interface->Resume);
    }
}

static unsigned featureManager_GetIndexForFeatureInPriorityList(unsigned list_index, feature_id_t id)
{
    unsigned index = INDEX_NOT_FOUND;

    for(unsigned i=0; i<priority_lists->list[list_index]->number_of_features; i++)
    {
        if(priority_lists->list[list_index]->id[i] == id)
        {
            index = i;
            break;
        }
    }

    return index;
}

static feature_manager_client_t * featureManager_GetClientFromId(feature_id_t id)
{
    feature_manager_client_t * client = NULL;

    for(unsigned i=0; i<feature_id_max; i++)
    {
        feature_manager_client_t * current_client = (feature_manager_client_t *)feature_manager_handles[i];
        if(current_client && current_client->id == id)
        {
            client = current_client;
        }
    }

    return client;
}

static bool featureManager_IsHigherPriorityFeatureRunning(feature_manager_client_t * client_requesting_to_start)
{
    bool higher_priority_feature_running = FALSE;

    for(unsigned list_index=0; list_index<priority_lists->number_of_lists; list_index++)
    {
        unsigned index_for_feature_in_priority_list = featureManager_GetIndexForFeatureInPriorityList(list_index, client_requesting_to_start->id);

        if(index_for_feature_in_priority_list != INDEX_NOT_FOUND)
        {
            for(int index = index_for_feature_in_priority_list-1; index >= 0; index--)
            {
                feature_manager_client_t * client = featureManager_GetClientFromId(priority_lists->list[list_index]->id[index]);

                if(client && client->interface->GetState() == feature_state_running)
                {
                    DEBUG_LOG("featureManager_IsHigherPriorityFeatureRunning enum:feature_id_t:%d is running", priority_lists->list[list_index]->id[index]);
                    higher_priority_feature_running = TRUE;
                    break;
                }
            }
        }
    }

    return higher_priority_feature_running;
}

static void featureManager_SuspendClient(feature_manager_client_t * client_to_suspend)
{
    feature_state_t state = client_to_suspend->interface->GetState();

    if(client_to_suspend->interface->Suspend)
    {
        if(state == feature_state_running)
        {
            DEBUG_LOG("featureManager_SuspendClient suspending enum:feature_id_t:%d", client_to_suspend->id);
            client_to_suspend->interface->Suspend();
        }
        else
        {
            DEBUG_LOG("featureManager_SuspendClient enum:feature_id_t:%d was not running so no need to suspend", client_to_suspend->id);
        }
    }
}

static void featureManager_ResumeClient(feature_manager_client_t * client_to_resume)
{
    feature_state_t state = client_to_resume->interface->GetState();

    if(client_to_resume->interface->Resume)
    {
        if(state == feature_state_suspended)
        {
            if(featureManager_IsHigherPriorityFeatureRunning(client_to_resume))
            {
                DEBUG_LOG("featureManager_ResumeClient enum:feature_id_t:%d cannot resume as a higher priority feature is running", client_to_resume->id);
            }
            else
            {
                DEBUG_LOG("featureManager_ResumeClient resuming enum:feature_id_t:%d", client_to_resume->id);
                client_to_resume->interface->Resume();
            }
        }
        else
        {
            DEBUG_LOG("featureManager_ResumeClient enum:feature_id_t:%d was not suspended so no need to resume", client_to_resume->id);
        }
    }
}

static void featureManager_PerformActionOnLowerPriorityFeatures(feature_manager_client_t * client_requesting_to_start, void(*action)(feature_manager_client_t * client))
{
    PanicNull((void *)action);

    for(unsigned list_index=0; list_index<priority_lists->number_of_lists; list_index++)
    {
        unsigned index_for_feature_in_priority_list = featureManager_GetIndexForFeatureInPriorityList(list_index, client_requesting_to_start->id);

        if(index_for_feature_in_priority_list != INDEX_NOT_FOUND)
        {
            for(unsigned index = index_for_feature_in_priority_list+1; index < priority_lists->list[list_index]->number_of_features; index++)
            {
                feature_manager_client_t * client = featureManager_GetClientFromId(priority_lists->list[list_index]->id[index]);
                if (client)
                {
                    action(client);
                }
            }
        }
    }
}

void FeatureManager_SetPriorities(const feature_manager_priority_lists_t * priority_list)
{
    priority_lists = priority_list;
}

feature_manager_handle_t FeatureManager_Register(feature_id_t feature_id, const feature_interface_t * feature_interface)
{
    DEBUG_LOG_FN_ENTRY("FeatureManager_Register enum:feature_id_t:%d", feature_id);

    PanicNull((void *)priority_lists);
    PanicNotNull((void*) feature_manager_handles[feature_id]);

    feature_manager_client_t * handle = (feature_manager_client_t *)PanicUnlessMalloc(sizeof(feature_manager_client_t));
    handle->id = feature_id;
    handle->interface = feature_interface;

    featureManager_VerifyClient(handle);
    feature_manager_handles[feature_id] = (feature_manager_handle_t)handle;

    return feature_manager_handles[feature_id];
}

bool FeatureManager_StartFeatureRequest(feature_manager_handle_t handle)
{
    bool can_start = TRUE;
    feature_manager_client_t * client_requesting_to_start = (feature_manager_client_t *) handle;

    PanicNull((void *)priority_lists);
    PanicNull((void *)client_requesting_to_start);

    DEBUG_LOG_FN_ENTRY("FeatureManager_StartFeatureRequest enum:feature_id_t:%d", client_requesting_to_start->id);

    if(featureManager_IsHigherPriorityFeatureRunning(client_requesting_to_start))
    {
        can_start = FALSE;
    }
    else
    {
        featureManager_PerformActionOnLowerPriorityFeatures(client_requesting_to_start, featureManager_SuspendClient);
    }

    return can_start;
}

void FeatureManager_StopFeatureIndication(feature_manager_handle_t handle)
{
    feature_manager_client_t * client_which_stopped = (feature_manager_client_t *) handle;

    PanicNull((void *)priority_lists);
    PanicNull((void *)client_which_stopped);

    DEBUG_LOG_FN_ENTRY("FeatureManager_StopFeatureIndication enum:feature_id_t:%d", client_which_stopped->id);
    featureManager_PerformActionOnLowerPriorityFeatures(client_which_stopped, featureManager_ResumeClient);
}

bool FeatureManager_CanFeatureStart(feature_manager_handle_t handle)
{
    feature_manager_client_t * client = (feature_manager_client_t *) handle;

    PanicNull((void *)priority_lists);
    PanicNull((void *)client);

    return !featureManager_IsHigherPriorityFeatureRunning(client);
}

#ifdef HOSTED_TEST_ENVIRONMENT
#include <stdlib.h>
void FeatureManager_Reset(void)
{
    priority_lists = NULL;

    for(unsigned i=0; i<feature_id_max; i++)
    {
        free(feature_manager_handles[i]);
        feature_manager_handles[i] = NULL;
    }
}
#endif /* HOSTED_TEST_ENVIRONMENT */
