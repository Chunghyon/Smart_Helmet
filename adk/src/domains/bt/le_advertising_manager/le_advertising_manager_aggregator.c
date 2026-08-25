/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation file for LE advertising manager aggregator, which is responsible for collecting individual advertising items and creating advertising sets
*/

#include "le_advertising_manager_aggregator.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_new_api.h"
#include "le_advertising_manager_advertising_item_database.h"
#include "le_advertising_manager_set_sm.h"
#include "le_advertising_manager_private.h"
#include "le_advertising_manager.h"
#include "le_advertising_manager_aggregator_group.h"
#include "le_advertising_manager_aggregator_set.h"
#include "le_advertising_manager_aggregator_types.h"

#include <panic.h>
#include <logging.h>
#include <app/bluestack/dm_prim.h>

#define MAX_LEGACY_DATA_SET_SIZE_IN_OCTETS      31
#define MAX_EXTENDED_DATA_SET_SIZE_IN_OCTETS    255

#define ADV_EVENT_PROPERTIES_CONNECTABLE   (1 << 0)

/*! \brief Internal task data for the aggregator. */
typedef struct {
    /*! Task to send and process internal messages. */
    TaskData task_data;

    /*! Lock object to serialise internal messages. */
    uint16 lock;

    /*! Callback to use after completing refresh operation. */
    void (*refresh_callback)(void);

} le_advertising_manager_aggregator_t;

static le_advertising_manager_aggregator_t aggregator;

#define LeAdvertisingManager_AggregatorGetTask() (&(aggregator).task_data)
#define LeAdvertisingManager_AggregatorGetTaskData() (&aggregator)
#define LeAdvertisingManager_AggregatorGetLock() (&aggregator.lock)

/*! \brief Internal messages for the aggregator. */
typedef enum {
    LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE,
    LE_ADVERTISING_MANAGER_INTERNAL_AGGREGATOR_MSG_CHECK_REFRESH_LOCK,
    LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE,
} le_advertising_manager_internal_aggregator_msg_t;

/*! \brief Internal aggregator message */
typedef struct {
    le_adv_refresh_control_t control;
} LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE_T;

typedef struct
{
    le_adv_item_handle item_handle;
} LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE_T;


#ifdef DEBUG_GROUPS_ENABLED
static void leAdvertisingManager_LoggingGroupsSetsItems(void)
{
    DEBUG_LOG_VERBOSE("LEAM print state:", number_of_groups);
    DEBUG_LOG_VERBOSE("Groups: %d", number_of_groups);

    for(uint32 i=0;i<number_of_groups;i++)
    {
        le_adv_item_group_t *group = &groups[i];

        LEAM_DEBUG_LOG("   Group[%d]:", i);
        LEAM_DEBUG_LOG("      num sets [%d] set_list [%p]", group->number_of_sets, group->set_list);

        uint32 set_number = 0;
        for(le_adv_set_list_t * set = group->set_list; set != NULL; set = set->next)
        {
            LEAM_DEBUG_LOG("         Set[%d]: [%p] active [%d] num items [%d] item_handles [%p]", set_number, set->set_handle,
                                                set->set_handle->active, set->set_handle->number_of_items, set->set_handle->item_list);
            set_number++;
        }
        if (group->number_of_sets != set_number)
        {
            DEBUG_LOG_ERROR("LEAM ERROR number_of_sets [%u] != set_number [%u]",
                                    group->number_of_sets, set_number);
        }

        LEAM_DEBUG_LOG("      num items [%d] item_handles [%p]", group->number_of_items, group->item_handles);
        uint32 item_number = 0;
        for(le_adv_item_list_t * item_in_group = group->item_handles; item_in_group != NULL; item_in_group = item_in_group->next)
        {
            unsigned found_in_set = 0;
            for(le_adv_set_list_t * set = group->set_list; set != NULL; set = set->next)
            {
                for(le_adv_item_list_t * item_in_set = set->set_handle->item_list; item_in_set != NULL; item_in_set = item_in_set->next)
                {
                    if(item_in_set->handle == item_in_group->handle)
                    {
                        LEAM_DEBUG_LOG("         Item[%d]: [%p] size [%d] present in set [%p]", item_number, item_in_set->handle, item_in_set->size, set->set_handle );
                        found_in_set++;
                        break;
                    }
                }
            }

            if (group->scan_resp_set)
            {
                le_adv_set_t * set = group->scan_resp_set;

                for(le_adv_item_list_t * item_in_set = set->item_list; item_in_set != NULL; item_in_set = item_in_set->next)
                {
                    if(item_in_set->handle == item_in_group->handle)
                    {
                        LEAM_DEBUG_LOG("         Item[%d]: [%p] size [%d] present in scan resp set [%p]", item_number, item_in_set->handle, item_in_set->size, set);
                        found_in_set++;
                        break;
                    }
                }
            }

            if(found_in_set == 0)
            {
                LEAM_DEBUG_LOG("         Item [%d]: [%p] has no set", item_number, item_in_group->handle );

            }
            else if(found_in_set > 1)
            {
                DEBUG_LOG_ERROR("LEAM ERROR item [%d] [%p] present in more than one set",
                                        item_number, item_in_group->handle);
            }
            item_number++;
        }

        if(group->number_of_items != item_number)
        {
            DEBUG_LOG_ERROR("LEAM ERROR number_of_items [%u] != item_number [%u]",
                    group->number_of_items, item_number);
        }

        if (!group->set_list && group->scan_resp_set)
        {
            DEBUG_LOG_WARN("LEAM WARN group has scan response items but no advert items");
        }
    }
}
#else
#define leAdvertisingManager_LoggingGroupsSetsItems()
#endif

#ifdef DEBUG_GROUPS_ENABLED
void leAdvertisingManager_LoggingItemData(le_adv_item_handle item)
{
    LEAM_DEBUG_LOG("LEAM Log Item [0x%x]", item);

    le_adv_item_data_t data = {.data=NULL, .size=0};

    if(item && item->callback)
    {
        if(item->callback->GetItemData)
        {
            item->callback->GetItemData(&data);

            LEAM_DEBUG_LOG("LEAM Log Item Data [0x%x] Size [%d]", data.data, data.size);
        }

        if(item->callback->ReleaseItemData)
        {
            item->callback->ReleaseItemData();
        }
    }
    else
    {
        LEAM_DEBUG_LOG("LEAM Log Item Data invalid");
    }
}
#endif

static void leAdvertisingManager_GetItemParams(le_adv_item_handle handle, le_adv_item_params_t * item_params)
{
    LeAdvertisingManager_PopulateDefaultAdvertisingParams(item_params);
    if(handle && handle->callback && handle->callback->GetItemParameters)
    {
        PanicFalse(handle->callback->GetItemParameters(item_params));
    }
}

static void leAdvertisingManager_GetItemInfo(le_adv_item_handle handle, le_adv_item_info_t * item_info)
{
    if(handle && handle->callback && handle->callback->GetItemInfo)
    {
        PanicFalse(handle->callback->GetItemInfo(item_info));
    }
}

bool leAdvertisingManager_IsAdvertisingTypeExtended(le_adv_data_type_t type)
{
    return (type == le_adv_type_extended_connectable || type == le_adv_type_extended_scannable || type == le_adv_type_extended_anonymous);
}

bool leAdvertisingManager_IsAdvertisingTypeConnectable(le_adv_data_type_t type)
{
    bool is_connectable_type = FALSE;
    switch(type)
    {
        case le_adv_type_legacy_connectable_scannable:
        case le_adv_type_legacy_directed:
        case le_adv_type_extended_connectable:
            is_connectable_type = TRUE;
            break;
        case le_adv_type_legacy_non_connectable_scannable:
        case le_adv_type_legacy_non_connectable_non_scannable:
        case le_adv_type_legacy_anonymous:
        case le_adv_type_extended_scannable:
        case le_adv_type_extended_anonymous:
            break;
        default:
            Panic();
            break;
    }
    return is_connectable_type;
}

static unsigned leAdvertisingManager_GetDataPacketSizeForAdvertisingType(le_adv_data_type_t type)
{
    return (leAdvertisingManager_IsAdvertisingTypeExtended(type) ? MAX_EXTENDED_DATA_SET_SIZE_IN_OCTETS : MAX_LEGACY_DATA_SET_SIZE_IN_OCTETS);
}

static le_adv_set_t * leAdvertisingManager_GetAdvertisingSetForSm(le_advertising_manager_set_state_machine_t * sm)
{
    bool found = FALSE;
    le_adv_set_t * set = NULL;

    for(uint32 i=0;i<number_of_groups;i++)
    {
        le_adv_set_list_t * head_set = groups[i].set_list;

        while(head_set != NULL)
        {
            if(sm == head_set->set_handle->sm)
            {
                found = TRUE;
                set = head_set->set_handle;
                break;
            }

            head_set = head_set->next;

        }

        if(found)
        {
            break;
        }
    }

    return set;
}

static void leAdvertisingManager_UpdateParamsForAdvertisingSet(le_adv_set_t * set)
{
    LEAM_DEBUG_LOG("LEAM Update Params for Set [0x%x]", set);

    le_adv_item_group_t * group = PanicNull(leAdvertisingManager_GetGroupForSet(set));
    le_advertising_manager_set_params_t params =
    {
        .adv_event_properties = group->info.type,
        .primary_adv_interval_min = group->params.primary_adv_interval_min,
        .primary_adv_interval_max = group->params.primary_adv_interval_max,
        .primary_adv_channel_map = group->params.primary_adv_channel_map,
        .adv_filter_policy = group->params.adv_filter_policy,
        .primary_adv_phy = group->params.primary_adv_phy,
        .secondary_adv_max_skip = group->params.secondary_adv_max_skip,
        .secondary_adv_phy = group->params.secondary_adv_phy,
        .adv_sid = group->params.adv_sid,
    };

    LeAdvertisingManager_SetSmUpdateParams(set->sm, &params);
}

static void leAdvertisingManager_AggregatorRegisterCfm(le_advertising_manager_set_state_machine_t *sm, bool success)
{
    LEAM_DEBUG_LOG("LEAM AggregatorRegisterCfm adv_handle %u success %u",
                      LeAdvertisingManager_SetSmGetAdvHandle(sm), success);

    PanicNull(sm);
    PanicFalse(success);

    le_adv_set_t * set = PanicNull(leAdvertisingManager_GetAdvertisingSetForSm(sm));

    set->needs_registering = FALSE;

    if(set->needs_params_update)
    {
        leAdvertisingManager_UpdateParamsForAdvertisingSet(set);
    }
    else
    {
        leAdvertisingManager_ReleaseAdvertisingSetBusyLock(set);
    }
}

static le_advertising_manager_data_packet_t *leAdvertisingManager_CreateDataPacketForSet(le_adv_set_t *set)
{
    le_advertising_manager_data_packet_t* data_packet = NULL;
    le_adv_item_group_t * group = PanicNull(leAdvertisingManager_GetGroupForSet(set));
    le_adv_item_list_t * head = set->item_list;

    while(head != NULL)
    {
        leAdvertisingManager_LoggingItemData(head->handle);

        le_adv_item_data_t data;

        if (   head->handle->callback->GetItemData(&data)
            && (data.size != 0))
        {
            DEBUG_LOG_VERBOSE("LEAM Update Data for Item [0x%x] with Size [%d]", head->handle, data.size);

            if(!data_packet)
            {
                data_packet = LeAdvertisingManager_DataPacketCreateDataPacket(leAdvertisingManager_GetDataPacketSizeForAdvertisingType(group->info.type));
            }

            if(!LeAdvertisingManager_DataPacketAddDataItem(data_packet, &data))
            {
                if(data.size != head->size)
                {
                    // If client data size has changed since allocating the item to a set, assume the client
                    // update is currently queued so will be processed shortly 
                    DEBUG_LOG_VERBOSE("LEAM Item Data size %d does not match size in set %d", data.size, head->size);
                }
                else
                {
                    Panic();
                }
            }

            if(head->handle->callback->ReleaseItemData)
            {
                head->handle->callback->ReleaseItemData();
            }
        }
        else
        {
            DEBUG_LOG_VERBOSE("LEAM Update Data for Item [0x%x] with Size 0", head->handle);
        }

        head = head->next;
    }

    return data_packet;
}

/*! \brief Update the advert data in the controller for the given set.

    This function will update both the advert and scan response data for the
    given set.

    \note The scan response data for each set in a group is the same.

    \param set Advertising set to update the data for.
*/
static void leAdvertisingManager_UpdateDataForAdvertisingSet(le_adv_set_t * set)
{
    le_advertising_manager_data_packet_t* adv_packet = NULL;
    le_advertising_manager_data_packet_t* scan_resp_packet = NULL;
    le_adv_item_group_t * group = PanicNull(leAdvertisingManager_GetGroupForSet(set));

    PanicNull(set);

    LEAM_DEBUG_LOG("LEAM Update Set [0x%x] Space Left [%d]", set, set->space);

    adv_packet = leAdvertisingManager_CreateDataPacketForSet(set);

    if (group->scan_resp_set)
    {
        scan_resp_packet = leAdvertisingManager_CreateDataPacketForSet(group->scan_resp_set);
    }

    DEBUG_LOG_VERBOSE("LEAM Update Data with Adv Packet %p Scan Resp Packet %p", adv_packet, scan_resp_packet);

    le_advertising_manager_set_adv_data_t adv_data = {0};

    if(adv_packet)
    {
        adv_data.adv_data = *adv_packet;
    }

    if (scan_resp_packet)
    {
        adv_data.scan_resp_data = *scan_resp_packet;
    }

    LeAdvertisingManager_SetSmUpdateData(set->sm, &adv_data);

    /* The firmware has taken ownership of the buffer(s) in the data packets so
       reset the buffers in the packets before destroying the packets */
    if (adv_packet)
    {
        LeAdvertisingManager_DataPacketReset(adv_packet);
        LeAdvertisingManager_DataPacketDestroy(adv_packet);
        adv_packet = NULL;
    }

    if (scan_resp_packet)
    {
        LeAdvertisingManager_DataPacketReset(scan_resp_packet);
        LeAdvertisingManager_DataPacketDestroy(scan_resp_packet);
        scan_resp_packet = NULL;
    }
}

static void leAdvertisingManager_AggregatorUpdateParamsCfm(le_advertising_manager_set_state_machine_t *sm, bool success)
{
    LEAM_DEBUG_LOG("LEAM AggregatorUpdateParamsCfm adv_handle %u success %u",
                      LeAdvertisingManager_SetSmGetAdvHandle(sm), success);

    PanicNull(sm);
    PanicFalse(success);

    le_adv_set_t * set = leAdvertisingManager_GetAdvertisingSetForSm(sm);

    PanicNull(set);

    set->needs_params_update = FALSE;

    if(set->needs_data_update)
    {
        leAdvertisingManager_UpdateDataForAdvertisingSet(set);
    }
    else
    {
        leAdvertisingManager_ReleaseAdvertisingSetBusyLock(set);
    }
}

static void leAdvertisingManager_AggregatorUpdateDataCfm(le_advertising_manager_set_state_machine_t *sm, bool success)
{
    LEAM_DEBUG_LOG("LEAM AggregatorUpdateDataCfm adv_handle %u success %u",
                      LeAdvertisingManager_SetSmGetAdvHandle(sm), success);

    PanicNull(sm);
    PanicFalse(success);

    le_adv_set_t * set = leAdvertisingManager_GetAdvertisingSetForSm(sm);

    PanicNull(set);

    set->needs_data_update = FALSE;

    if((set->needs_enabling) && (set->needs_disabling))
    {
        Panic();
    }

    if(set->needs_enabling)
    {
        if(!set->active)
        {
            LeAdvertisingManager_SetSmEnable(sm);
        }
    }
    else if(set->needs_disabling)
    {
        if(set->active)
        {
            LeAdvertisingManager_SetSmDisable(sm);
        }
    }
    else
    {
        leAdvertisingManager_ReleaseAdvertisingSetBusyLock(set);
    }
}

static void leAdvertisingManager_AggregatorEnableCfm(le_advertising_manager_set_state_machine_t *sm, bool success)
{
    LEAM_DEBUG_LOG("LEAM AggregatorEnableCfm adv_handle %u success %u",
                      LeAdvertisingManager_SetSmGetAdvHandle(sm), success);

    PanicNull(sm);
    PanicFalse(success);

    le_adv_set_t * set = leAdvertisingManager_GetAdvertisingSetForSm(sm);

    PanicNull(set);

    set->needs_enabling = FALSE;
    set->active = TRUE;
    leAdvertisingManager_ReleaseAdvertisingSetBusyLock(set);
}

static void leAdvertisingManager_AggregatorDisableCfm(le_advertising_manager_set_state_machine_t *sm, bool success)
{
    LEAM_DEBUG_LOG("LEAM AggregatorDisableCfm adv_handle %u success %u",
                      LeAdvertisingManager_SetSmGetAdvHandle(sm), success);

    PanicNull(sm);
    PanicFalse(success);

    le_adv_set_t * set = leAdvertisingManager_GetAdvertisingSetForSm(sm);

    PanicNull(set);

    set->needs_disabling = FALSE;
    set->active = FALSE;
    leAdvertisingManager_ReleaseAdvertisingSetBusyLock(set);
}

static le_advertising_manager_set_client_interface_t aggregator_adv_set_client_interface = {
    .RegisterCfm = leAdvertisingManager_AggregatorRegisterCfm,
    .UpdateParamsCfm = leAdvertisingManager_AggregatorUpdateParamsCfm,
    .UpdateDataCfm = leAdvertisingManager_AggregatorUpdateDataCfm,
    .EnableCfm = leAdvertisingManager_AggregatorEnableCfm,
    .DisableCfm = leAdvertisingManager_AggregatorDisableCfm
};

static unsigned leAdvertisingManager_GetNumberOfSetsInUse(void)
{
    LEAM_DEBUG_LOG("LEAM Find Sets in Groups");

    unsigned result = 0;

    for(uint32 i=0;i<number_of_groups;i++)
    {
        result += groups[i].number_of_sets;
    }
    return result;
}


static unsigned leAdvertisingManager_GetDataSetSizeForAdvertisingType(le_adv_data_type_t type)
{
    return (leAdvertisingManager_IsAdvertisingTypeExtended(type) ? MAX_EXTENDED_DATA_SET_SIZE_IN_OCTETS : MAX_LEGACY_DATA_SET_SIZE_IN_OCTETS);
}

/*! \brief Create a aggregator set to hold advert or scan response data items.

    \param type The type advert this data is for, e.g. legacy, extended, connectable.
    \param assign_adv_handle TRUE if this set represents data that should be
                             assigned to its own advertising set in the controller.
                             FALSE if this set is only used internally in the
                             aggregator to group related items together, for
                             example for scan response data.
*/
static le_adv_set_t * leAdvertisingManager_CreateNewAdvertisingSet(le_adv_data_type_t type, bool assign_adv_handle)
{
    le_adv_set_t * set = NULL;

    set = PanicUnlessMalloc((sizeof(le_adv_set_t)));
    memset(set, 0, sizeof(*set));

    LEAM_DEBUG_LOG("LEAM Create New Set type enum:le_adv_data_type_t:%u assign_adv_handle [%d]", type, assign_adv_handle);

    if (assign_adv_handle)
    {
        unsigned number_of_sets = leAdvertisingManager_GetNumberOfSetsInUse();

        set->sm = LeAdvertisingManager_SetSmCreate(&aggregator_adv_set_client_interface, number_of_sets + 1);
        PanicNull(set->sm);
    }

    set->item_list = NULL;
    set->number_of_items = 0;
    set->needs_registering = TRUE;
    set->needs_params_update = TRUE;
    set->needs_data_update = TRUE;
    set->needs_enabling = TRUE;
    set->needs_disabling = FALSE;
    set->active = FALSE;
    set->lock = 0;
    set->space = leAdvertisingManager_GetDataSetSizeForAdvertisingType(type);

    LEAM_DEBUG_LOG("LEAM Set [%x] Created with Sm [%x]", set, set->sm );

    return set;
}

static le_adv_set_t * leAdvertisingManager_CreateNewAdvertisingSetForGroup(le_adv_item_group_t * group)
{
    le_adv_set_t * new_set = leAdvertisingManager_CreateNewAdvertisingSet(group->info.type, TRUE);
    leAdvertisingManager_AddSetToGroup(group, new_set);
    return new_set;
}

static inline void leAdvertisingManager_ClearSetOfItems(le_adv_set_t * set, uint32 set_size)
{
    le_adv_item_list_t * item = set->item_list;
    le_adv_item_list_t * next = NULL;
    while(item != NULL)
    {
        next = item->next;
        free(item);
        item = next;
    }
    set->item_list = NULL;
    set->number_of_items = 0;
    set->space = set_size;
}

static inline void leAdvertisingManager_ClearAllSetsOfItems(le_adv_item_group_t * group)
{
    uint32 set_size = leAdvertisingManager_GetDataSetSizeForAdvertisingType(group->info.type);
    for(le_adv_set_list_t * set = group->set_list; set != NULL; set = set->next)
    {
        leAdvertisingManager_ClearSetOfItems(set->set_handle, set_size);

        /* Re-add the item that should always be in all sets in this group */
        if (group->always_include_item_handle)
        {
            le_adv_item_info_t always_include_item_info = {0};
            leAdvertisingManager_GetItemInfo(group->always_include_item_handle, &always_include_item_info);

            DEBUG_LOG("LEAM re-adding item [%p] to set [%p]", group->always_include_item_handle, set->set_handle);

            leAdvertisingManager_AddItemToSet(set->set_handle,
                                              group->always_include_item_handle,
                                              always_include_item_info.data_size);
        }
    }

    if (group->scan_resp_set)
    {
        leAdvertisingManager_ClearSetOfItems(group->scan_resp_set, set_size);
    }
}

static void leAdvertisingManager_RefreshSetsForGroup(le_adv_item_group_t * group)
{
    LEAM_DEBUG_LOG("LEAM refreshing group %p", group);

    leAdvertisingManager_LoggingGroupsSetsItems();

    // In the current implementation by this point we've lost knowledge of which item the update request 
    // came from so remove all items from all sets and recreate
    leAdvertisingManager_ClearAllSetsOfItems(group);

    for(le_adv_item_list_t * item = group->item_handles; item != NULL; item = item->next)
    {
        le_adv_item_info_t item_info = {0};
        leAdvertisingManager_GetItemInfo(item->handle, &item_info);
        PanicFalse(item_info.data_size <= leAdvertisingManager_GetDataSetSizeForAdvertisingType(group->info.type));

        if(item_info.data_size)
        {
            DEBUG_LOG_VERBOSE("     item[%p] placement [enum:le_adv_item_data_placement_t:%u]", item->handle, item_info.placement);

            if (le_adv_item_data_placement_advert == item_info.placement)
            {
                le_adv_set_t * set = leAdvertisingManager_GetSetWithFreeSpace(group->set_list, item_info.data_size);
                if(!set)
                {
                    set = leAdvertisingManager_CreateNewAdvertisingSetForGroup(group);

                    DEBUG_LOG("     always_inlcude_item_handle %p", group->always_include_item_handle);

                    if (group->always_include_item_handle)
                    {
                        le_adv_item_info_t always_include_item_info = {0};
                        leAdvertisingManager_GetItemInfo(group->always_include_item_handle, &always_include_item_info);

                        DEBUG_LOG("     adding always_include_item_handle %p", group->always_include_item_handle);

                        leAdvertisingManager_AddItemToSet(set,
                                                          group->always_include_item_handle,
                                                          always_include_item_info.data_size);
                    }
                }

                if (item->handle != group->always_include_item_handle)
                {
                    leAdvertisingManager_AddItemToSet(set, item->handle, item_info.data_size);
                }
                else
                {
                    DEBUG_LOG("     Skipping item %p because it should already have been added", item->handle);
                }

                set->needs_data_update = TRUE;
            }
            else if (le_adv_item_data_placement_scan_response == item_info.placement)
            {
                if (!group->scan_resp_set)
                {
                    group->scan_resp_set = leAdvertisingManager_CreateNewAdvertisingSet(group->info.type, FALSE);
                }

                leAdvertisingManager_AddItemToSet(group->scan_resp_set, item->handle, item_info.data_size);
            }
        }
    }
    leAdvertisingManager_LoggingGroupsSetsItems();
}

static void leAdvertisingManager_EnableAdvertisingForGroup(le_adv_item_group_t * group, bool enable)
{
    for(le_adv_set_list_t * set = group->set_list; set != NULL; set = set->next)
    {
        if(set->set_handle->number_of_items && enable)
        {
            if(!set->set_handle->active)
            {
                set->set_handle->needs_enabling = TRUE;
            }
        }
        else if(set->set_handle->active)
        {
            set->set_handle->needs_disabling = TRUE;
            if(!set->set_handle->number_of_items)
            {
                set->set_handle->needs_destroying = TRUE;
            }
        }
    }
}

static void LeAdvertisingManager_RegroupAdvertisingItems(void)
{
    LEAM_DEBUG_LOG("LEAM RegroupAdvertisingItems Number of Groups [%d]", number_of_groups);

    leAdvertisingManager_LoggingGroupsSetsItems();

    for(uint32 i=0;i<number_of_groups;i++)
    {
        bool enable_advertising = leAdvertisingManager_DoesGroupPassAdvertisingCriteria(&groups[i]);

        if(enable_advertising && leAdvertisingManager_IsGroupToBeRefreshed(&groups[i]))
        {
            leAdvertisingManager_RefreshSetsForGroup(&groups[i]);
            leAdvertisingManager_MarkGroupToBeRefreshed(&groups[i], FALSE);
        }
        leAdvertisingManager_EnableAdvertisingForGroup(&groups[i], enable_advertising);
    }

    leAdvertisingManager_LoggingGroupsSetsItems();
}

static void leAdvertisingManager_UpdateAdvertisingItem(le_adv_item_handle handle)
{
    le_adv_item_params_t item_params;
    le_adv_item_info_t item_info;
    leAdvertisingManager_GetItemParams(handle, &item_params);
    leAdvertisingManager_GetItemInfo(handle, &item_info);
    le_adv_item_group_t * new_group = leAdvertisingManager_GetGroupForParams(&item_info, &item_params);
    le_adv_item_group_t * current_group = leAdvertisingManager_GetGroupLinkedToItem(handle);
    LEAM_DEBUG_LOG("leAdvertisingManager_UpdateAdvertisingItem current %p new %p", current_group, new_group);
    if(current_group != new_group || !new_group)
    {
        if(current_group)
        {
            leAdvertisingManager_RemoveItemFromGroup(current_group, handle);
            leAdvertisingManager_MarkGroupToBeRefreshed(current_group, TRUE);

            /* If this item was also the 'always include' item for this group remove it. */
            if (current_group->always_include_item_handle == handle)
            {
                current_group->always_include_item_handle = NULL;
            }
        }
        if(!new_group)
        {
            LEAM_DEBUG_LOG("LEAM No Groups with Matching Params, Create New One");
            new_group = PanicNull(leAdvertisingManager_CreateNewGroup(&item_info, &item_params));
        }
        leAdvertisingManager_AddItemToGroup(new_group, handle);

        /* If this item should go in all sets, store it for later reference. */
        if (   item_info.include_if_connectable
            && leAdvertisingManager_DataTypeIsConnectable(item_info.type))
        {
            DEBUG_LOG("LEAM setting always_include_item %p for group %p", handle, new_group);
            new_group->always_include_item_handle = handle;
        }
    }
    leAdvertisingManager_MarkGroupToBeRefreshed(new_group, TRUE);
}

static void leAdvertisingManager_SetAggregatorRefreshLock(void)
{
    LEAM_DEBUG_LOG("LEAM Set Aggregator Refresh Lock");

    LeAdvertisingManager_AggregatorGetTaskData()->lock = 1;
}

static void leAdvertisingManager_ReleaseAggregatorRefreshLock(void)
{
    LEAM_DEBUG_LOG("LEAM Release Aggregator Refresh Lock");

    LeAdvertisingManager_AggregatorGetTaskData()->lock = 0;
}

void LeAdvertisingManager_QueueAdvertisingStateUpdate(le_adv_refresh_control_t * control)
{
    LEAM_DEBUG_LOG("LEAM Refresh Advertising Sets, Current Lock[%d]", LeAdvertisingManager_AggregatorGetTaskData()->lock );
    le_adv_refresh_control_t ctrl = { .advertising_state_update_callback = NULL };
    if(control)
    {
        ctrl.advertising_state_update_callback = control->advertising_state_update_callback;
    }
    MESSAGE_MAKE(msg, LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE_T);
    msg->control = ctrl;
    MessageSendConditionally(LeAdvertisingManager_AggregatorGetTask(), LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE, msg, &LeAdvertisingManager_AggregatorGetTaskData()->lock);
}

void LeAdvertisingManager_QueueClientDataUpdate(le_adv_item_handle item_handle)
{
    LEAM_DEBUG_LOG("LeAdvertisingManager_QueueClientDataUpdate, handle %p Current Lock[%d]", item_handle, LeAdvertisingManager_AggregatorGetTaskData()->lock );
    MESSAGE_MAKE(msg, LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE_T);
    msg->item_handle = item_handle;
    MessageSendConditionally(LeAdvertisingManager_AggregatorGetTask(), LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE, msg, &LeAdvertisingManager_AggregatorGetTaskData()->lock);
}

static void leAdvertisingManager_SendInternalMessageRefreshLockCheck(void)
{
    MessageCancelFirst(LeAdvertisingManager_AggregatorGetTask(), LE_ADVERTISING_MANAGER_INTERNAL_AGGREGATOR_MSG_CHECK_REFRESH_LOCK);
    MessageSendLater(LeAdvertisingManager_AggregatorGetTask(), LE_ADVERTISING_MANAGER_INTERNAL_AGGREGATOR_MSG_CHECK_REFRESH_LOCK, NULL, 50);
}

static bool IsAnySetBusy(void)
{
    LEAM_DEBUG_LOG("LEAM Is Any Set Busy");

    bool found = FALSE;
    bool result = FALSE;

    for(uint32 i=0;i<number_of_groups;i++)
    {
        le_adv_set_list_t * head_set = groups[i].set_list;

        while(head_set != NULL)
        {
            DEBUG_LOG_VERBOSE("LEAM Is Set [0x%x] Busy[%d]", head_set->set_handle, head_set->set_handle->lock );

            if(head_set->set_handle->lock)
            {
                DEBUG_LOG_VERBOSE("LEAM Found Busy Set [0x%x]", head_set->set_handle);

                found = TRUE;
                break;
            }

            head_set = head_set->next;
        }

        if(found)
        {
            result = TRUE;
            break;
        }
    }

    return result;
}

static void leAdvertisingManager_CheckReleaseRefreshLock(void)
{
    if(!IsAnySetBusy())
    {
        leAdvertisingManager_ReleaseAggregatorRefreshLock();
        if(LeAdvertisingManager_AggregatorGetTaskData()->refresh_callback)
        {
            LEAM_DEBUG_LOG("LEAM Release Refresh Client Callback [0x%x]", LeAdvertisingManager_AggregatorGetTaskData()->refresh_callback );

            LeAdvertisingManager_AggregatorGetTaskData()->refresh_callback();
            LeAdvertisingManager_AggregatorGetTaskData()->refresh_callback = NULL;
        }

    }
    else
    {
        leAdvertisingManager_SetAggregatorRefreshLock();
        leAdvertisingManager_SendInternalMessageRefreshLockCheck();
    }
}

static void leAdvertisingManager_RefreshAdvertising(void)
{
    LEAM_DEBUG_LOG("LEAM Handle Internal Refresh Message");
    LeAdvertisingManager_RegroupAdvertisingItems();

    for(uint32 i=0;i<number_of_groups;i++)
    {
        le_adv_set_list_t * head_set = groups[i].set_list;

        while(head_set != NULL)
        {
            LEAM_DEBUG_LOG("LEAM Refresh Group[0x%x] Set[0x%x]", &groups[i], head_set->set_handle);

            if(head_set->set_handle->needs_registering)
            {
                LEAM_DEBUG_LOG("LEAM Set Needs Registering [%x]", head_set->set_handle);

                leAdvertisingManager_SetAdvertisingSetBusyLock(head_set->set_handle);
                LeAdvertisingManager_SetSmRegister(head_set->set_handle->sm);
            }
            else if(head_set->set_handle->needs_data_update)
            {
                LEAM_DEBUG_LOG("LEAM Set Needs Data Update [%x]", head_set->set_handle);

                leAdvertisingManager_SetAdvertisingSetBusyLock(head_set->set_handle);
                leAdvertisingManager_UpdateDataForAdvertisingSet(head_set->set_handle);
            }
            else if(head_set->set_handle->needs_disabling)
            {
                LEAM_DEBUG_LOG("LEAM Set Needs Disabling [%x]", head_set->set_handle);

                if(head_set->set_handle->active)
                {
                    leAdvertisingManager_SetAdvertisingSetBusyLock(head_set->set_handle);
                    LeAdvertisingManager_SetSmDisable(head_set->set_handle->sm);
                }
                else
                {
                    head_set->set_handle->needs_disabling = FALSE;
                }
            }
            else if(head_set->set_handle->needs_enabling)
            {
                LEAM_DEBUG_LOG("LEAM Set Needs Enabling [%x]", head_set->set_handle);

                if(!head_set->set_handle->active)
                {
                    leAdvertisingManager_SetAdvertisingSetBusyLock(head_set->set_handle);
                    LeAdvertisingManager_SetSmEnable(head_set->set_handle->sm);
                }
                else
                {
                    head_set->set_handle->needs_enabling = FALSE;
                }
            }
            head_set = head_set->next;
        }
    }

    leAdvertisingManager_CheckReleaseRefreshLock();

}

static void leAdvertisingManager_AggregatorHandleAdvertisingStateUpdate(const LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE_T * message)
{
    LeAdvertisingManager_AggregatorGetTaskData()->refresh_callback = message->control.advertising_state_update_callback;
    leAdvertisingManager_RefreshAdvertising();
}

static void leAdvertisingManager_AggregatorHandleInternalMsgCheckRefreshLock(void)
{
    leAdvertisingManager_CheckReleaseRefreshLock();
}

static void leAdvertisingManager_AggregatorHandleClientDataUpdate(const LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE_T * message)
{
    leAdvertisingManager_UpdateAdvertisingItem(message->item_handle);
    leAdvertisingManager_RefreshAdvertising();
}

static void LeAdvertisingManager_AggregatorMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    LEAM_DEBUG_LOG("LeAdvertisingManager_AggregatorMessageHandler id enum:le_advertising_manager_internal_aggregator_msg_t:%u", id);

    switch (id)
    {
    case LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE:
        leAdvertisingManager_AggregatorHandleAdvertisingStateUpdate((const LE_ADVERTISING_MANAGER_ADVERTISING_STATE_UPDATE_T *)message);
        break;

    case LE_ADVERTISING_MANAGER_INTERNAL_AGGREGATOR_MSG_CHECK_REFRESH_LOCK:
        leAdvertisingManager_AggregatorHandleInternalMsgCheckRefreshLock();
        break;

    case LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE:
        leAdvertisingManager_AggregatorHandleClientDataUpdate((const LE_ADVERTISING_MANAGER_CLIENT_DATA_UPDATE_T *)message);
        break;

    default:
       break;
    }
}

void LeAdvertisingManager_InitAggregator(void)
{
    number_of_groups = 0;
    aggregator.task_data.handler = LeAdvertisingManager_AggregatorMessageHandler;
    leAdvertisingManager_ReleaseAggregatorRefreshLock();
    aggregator.refresh_callback = NULL;
}

#endif /* LE_ADVERTISING_MANAGER_NEW_API */
