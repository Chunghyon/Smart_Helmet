/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "le_advertising_manager_aggregator_group.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_aggregator.h"

#include <logging.h>
#include <app/bluestack/dm_prim.h>


#define MAX_ADVERTISING_GROUPS  3
le_adv_item_group_t groups[MAX_ADVERTISING_GROUPS];
uint32 number_of_groups = 0;


static bool leAdvertisingManager_HasSidBeenSet(uint16 sid)
{
    return (sid && sid != DM_ULP_EXT_ADV_SID_INVALID);
}

static unsigned leAdvertisingManager_GetNewSid(le_adv_data_type_t type)
{
    uint16 sid = DM_ULP_EXT_ADV_SID_INVALID;
    if(leAdvertisingManager_IsAdvertisingTypeExtended(type))
    {
        sid = DM_ULP_EXT_ADV_SID_ASSIGNED_BY_STACK;
    }
    return sid;
}

static bool leAdvertisingManager_DoParamsMatch(le_adv_item_params_t * params1, le_adv_item_params_t * params2)
{
    bool result = FALSE;

    DEBUG_LOG_VERBOSE("LEAM Min Interval Params1 0x%x, Params2 0x%x]",  params1->primary_adv_interval_min, params2->primary_adv_interval_min);
    DEBUG_LOG_VERBOSE("LEAM Max Interval Params1 0x%x, Params2 0x%x]",  params1->primary_adv_interval_max, params2->primary_adv_interval_max);

    if((params1->primary_adv_interval_min == params2->primary_adv_interval_min)&&
       (params1->primary_adv_interval_max == params2->primary_adv_interval_max))
    {
        DEBUG_LOG_VERBOSE("LEAM Params Match!");
        result = TRUE;
    }

    return result;
}

/*! \brief Check if an item info is compatible with an existing item info.

    This is used when matching an advertising item with the group it will be put
    into.

    \note This function does not compare the placement type. This is done on
          purpose so that matching advert items and scan response items will be
          put into the same group.

    \param info1
    \param info2
*/
static bool leAdvertisingManager_DoesInfoMatch(le_adv_item_info_t * info1, le_adv_item_info_t * info2)
{
    bool result = FALSE;

    DEBUG_LOG_VERBOSE("LEAM Override Info1 0x%x, Info2 0x%x]",  info1->override_connectable_state, info2->override_connectable_state);

    if(info1->override_connectable_state == info2->override_connectable_state
            && info1->type == info2->type
            && !info1->needs_own_set && !info2->needs_own_set)
    {
        DEBUG_LOG_VERBOSE("LEAM Info Match!");
        result = TRUE;
    }

    return result;
}

le_adv_item_group_t * leAdvertisingManager_GetGroupForSet(le_adv_set_t * set)
{
    le_adv_item_group_t * group = NULL;
    for(uint32 i=0; (i < number_of_groups) && (group == NULL); i++)
    {
        if(groups[i].number_of_sets)
        {
            le_adv_set_list_t * set_to_check = groups[i].set_list;
            while(set_to_check != NULL && group == NULL)
            {
                if(set == set_to_check->set_handle)
                {
                    group = &groups[i];
                    break;
                }
                set_to_check = set_to_check->next;
            }
        }

        if (set == groups[i].scan_resp_set)
        {
            group = &groups[i];
            break;
        }
    }
    return group;
}

le_adv_item_group_t * leAdvertisingManager_GetGroupLinkedToItem(le_adv_item_handle handle)
{
    DEBUG_LOG_VERBOSE("LEAM Is Item [0x%x] Linked to Any Group", handle);

    le_adv_item_group_t * group_found = NULL;

    for(uint32 i=0;i<number_of_groups;i++)
    {
        DEBUG_LOG_VERBOSE("LEAM [Group 0x%x ]", &groups[i]);

        le_adv_item_list_t * head = groups[i].item_handles;
        while(head != NULL)
        {
            DEBUG_LOG_VERBOSE("LEAM [Item Handles 0x%x ]", head->handle );
            if(handle == head->handle)
            {
                DEBUG_LOG_VERBOSE("LEAM Item Found in Group [0x%x]" , &groups[i]);
                group_found = &groups[i];
                break;
            }

            head = head->next;
        }

        if(group_found)
        {
            break;
        }
    }
    return group_found;
}

le_adv_item_group_t * leAdvertisingManager_GetGroupForParams(le_adv_item_info_t * item_info, le_adv_item_params_t * item_params)
{
    le_adv_item_group_t * group = NULL;

    for(uint32 i=0;i<number_of_groups;i++)
    {
        DEBUG_LOG_VERBOSE("LEAM [Number of Groups %d ]", number_of_groups);

        if(leAdvertisingManager_DoParamsMatch(item_params, &groups[i].params)
                && leAdvertisingManager_DoesInfoMatch(item_info, &groups[i].info))
        {
            DEBUG_LOG_VERBOSE("LEAM Group Found with Matching Params");
            group = &groups[i];
            break;
        }
    }

    return group;
}

le_adv_item_group_t * leAdvertisingManager_CreateNewGroup(le_adv_item_info_t * info, le_adv_item_params_t * params)
{
    DEBUG_LOG_VERBOSE("LEAM CreateNewGroup");

    PanicNull(params);
    PanicNull(info);
    PanicFalse(number_of_groups < MAX_ADVERTISING_GROUPS);

    le_adv_item_group_t *new_group = &groups[number_of_groups++];
    memset(new_group, 0, sizeof(*new_group));

    DEBUG_LOG_VERBOSE("LEAM Set Params for Group [0x%x]", new_group);

    new_group->params = *params;
    new_group->info = *info;

    if(!leAdvertisingManager_HasSidBeenSet(new_group->params.adv_sid))
    {
        new_group->params.adv_sid = leAdvertisingManager_GetNewSid(new_group->info.type);
    }

    DEBUG_LOG_VERBOSE("LEAM CreateNewGroup End, Groups [0x%x] Group [0x%x] Number of Groups [%d]", &groups, new_group, number_of_groups);

    return new_group;
}

void leAdvertisingManager_AddItemToGroup(le_adv_item_group_t * group, le_adv_item_handle handle)
{
    DEBUG_LOG_FN_ENTRY("LEAM AddItemToGroup group [%p] handle [%p]", group, handle);

    le_adv_item_list_t * new_item = PanicUnlessMalloc(sizeof(le_adv_item_list_t));
    memset(new_item, 0, sizeof(*new_item));

    new_item->handle = handle;
    new_item->next = NULL;

    if(group->item_handles == NULL)
    {
        group->item_handles = new_item;
    }
    else
    {
        for(le_adv_item_list_t * item = group->item_handles; item != NULL; item = item->next)
        {
            if(item->next == NULL)
            {
                item->next = new_item;
                break;
            }
        }
    }
    group->number_of_items++;
}

void leAdvertisingManager_RemoveItemFromGroup(le_adv_item_group_t * group, le_adv_item_handle handle)
{
    DEBUG_LOG_VERBOSE("LEAM Removing item %p from group %p", handle, group);
    PanicNull(handle);
    PanicFalse(group->number_of_items);

    le_adv_item_list_t * item = group->item_handles;
    le_adv_item_list_t * previous_item = group->item_handles;

    while(item != NULL && item->handle != handle)
    {
        previous_item = item;
        item = item->next;
    }

    if(item)
    {
        previous_item->next = item->next;
        free(item);
        group->number_of_items--;
    }
    else
    {
        DEBUG_LOG_VERBOSE("LEAM Item %p not found in group %p", handle, group);
        Panic();
    }

    if (group->always_include_item_handle == handle)
    {
        group->always_include_item_handle = NULL;
    }
}

void leAdvertisingManager_AddSetToGroup(le_adv_item_group_t * group, le_adv_set_t * set_to_add)
{
    DEBUG_LOG_VERBOSE("LEAM Add set %p to group %p", set_to_add, group);
    PanicNull(set_to_add);
    PanicNull(group);

    le_adv_set_list_t * new_set = PanicUnlessMalloc(sizeof(le_adv_set_list_t));
    new_set->set_handle = set_to_add;
    new_set->next = NULL;

    if(group->set_list == NULL)
    {
        group->set_list = new_set;
    }
    else
    {
        for(le_adv_set_list_t * set = group->set_list; set != NULL; set = set->next)
        {
            DEBUG_LOG_VERBOSE("    set %p", set->set_handle);
            if(set->next == NULL)
            {
                set->next = new_set;
                break;
            }
        }
    }
    group->number_of_sets++;
}

void leAdvertisingManager_RemoveSetFromGroup(le_adv_item_group_t * group, le_adv_set_t * set_to_remove)
{
    DEBUG_LOG_VERBOSE("LEAM Remove set %p from group %p", set_to_remove, group);
    PanicNull(set_to_remove);
    PanicNull(group);
    PanicFalse(group->number_of_sets);

    le_adv_set_list_t * previous_set = group->set_list;
    for(le_adv_set_list_t * set = group->set_list; set != NULL; set = set->next)
    {
        if(set->set_handle == set_to_remove)
        {
            DEBUG_LOG_VERBOSE("  removing set_handle %p", set->set_handle);
            previous_set->next = set->next;
            free(set);
            group->number_of_sets--;
            break;
        }
        previous_set = set;
    }
}

bool leAdvertisingManager_DoesGroupPassAdvertisingCriteria(le_adv_item_group_t * group)
{
    DEBUG_LOG_VERBOSE("LEAM Does Group [0x%x] Pass Advertising Criteria", group);

    bool result = FALSE;

    if(LeAdvertisingManager_IsAdvertisingAllowed())
    {
        DEBUG_LOG_VERBOSE("LEAM Advertising Allowed");

        if(group->info.override_connectable_state)
        {
            DEBUG_LOG_VERBOSE("LEAM Advertising Group Override Logic");
            result = TRUE;
        }
        else if(leAdvertisingManager_IsAdvertisingTypeConnectable(group->info.type))
        {
            if(LeAdvertisingManager_IsConnectableAdvertisingEnabled())
            {
                DEBUG_LOG_VERBOSE("LEAM Group Connectable Advertising");
                result = TRUE;
            }
        }
        else
        {
            DEBUG_LOG_VERBOSE("LEAM Group non-connectable Advertising");
            result = TRUE;
        }
    }

    return result;
}

bool leAdvertisingManager_IsGroupToBeRefreshed(le_adv_item_group_t * group)
{
    return group->needs_refresh;
}

void leAdvertisingManager_MarkGroupToBeRefreshed(le_adv_item_group_t * group, bool refresh)
{
    group->needs_refresh = refresh;
}

#endif /* LE_ADVERTISING_MANAGER_NEW_API */
