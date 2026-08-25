/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for LE advertising manager aggregator set.
*/

#include "le_advertising_manager_aggregator_set.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager.h"
#include "le_advertising_manager_advertising_item_database.h"

#include <logging.h>



uint32 leAdvertisingManager_GetItemSize(le_adv_item_handle item)
{
    PanicNull(item);
    le_adv_item_info_t item_info = { .data_size = 0 };
    item->callback->GetItemInfo(&item_info);
    return item_info.data_size;
}

void leAdvertisingManager_SetAdvertisingSetBusyLock(le_adv_set_t * set)
{
    DEBUG_LOG_FN_ENTRY("LEAM Set Busy Lock for Set [0x%x]", set);

    set->lock = 1;
}

void leAdvertisingManager_ReleaseAdvertisingSetBusyLock(le_adv_set_t * set)
{
    DEBUG_LOG_FN_ENTRY("LEAM Release Busy Lock for Set [0x%x]", set);

    set->lock = 0;
}

le_adv_set_t * leAdvertisingManager_GetSetWithFreeSpace(le_adv_set_list_t * set_list, uint32 free_space_size)
{
    le_adv_set_t * set_handle = NULL;
    for(le_adv_set_list_t * set = set_list; set != NULL; set = set->next)
    {
        if(set->set_handle->space >= free_space_size)
        {
            set_handle = set->set_handle;
            break;
        }
    }
    return set_handle;
}

void leAdvertisingManager_AddItemToSet(le_adv_set_t * set, le_adv_item_handle handle, uint32 size)
{
    DEBUG_LOG_VERBOSE("LEAM Add item %p of size %d to set %p", handle, size, set);
    PanicNull(handle);
    PanicNull(set);
    PanicFalse(size);
    PanicFalse(size <= set->space);

    le_adv_item_list_t * new_item = PanicUnlessMalloc(sizeof(le_adv_item_list_t));
    new_item->handle = handle;
    new_item->next = NULL;
    new_item->size = size;

    if(set->item_list == NULL)
    {
        set->item_list = new_item;
    }
    else
    {
        for(le_adv_item_list_t * item = set->item_list; item != NULL; item = item->next)
        {
            leAdvertisingManager_LoggingItemData(item->handle);
            if(item->next == NULL)
            {
                item->next = new_item;
                break;
            }
        }
    }
    set->space -= new_item->size;
    set->number_of_items++;
}


#endif /* LE_ADVERTISING_MANAGER_NEW_API */
