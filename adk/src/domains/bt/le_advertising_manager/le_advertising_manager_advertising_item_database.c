/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation for LE advertising manager functionality associated with advertising item database
*/

#include "le_advertising_manager_new_api.h"
#include "le_advertising_manager_advertising_item_database.h"
#include "le_advertising_manager_aggregator.h"

#include "logging.h"

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#define MAX_NUMBER_OF_ADVERTISING_ITEM_ENTRIES 20

static struct _le_adv_item database[MAX_NUMBER_OF_ADVERTISING_ITEM_ENTRIES];

void LeAdvertisingManager_InitAdvertisingItemDatabase(void)
{
	for(int i=0;i<MAX_NUMBER_OF_ADVERTISING_ITEM_ENTRIES;i++)
	{
		database[i].task = NULL;
		database[i].callback = NULL;
	}
}

le_adv_item_handle LeAdvertisingManager_RegisterAdvertisingItemCallback(Task task, const le_adv_item_callback_t * callback)
{
    DEBUG_LOG_ALWAYS("LeAdvertisingManager_RegisterAdvertisingItemCallback with Handle [0x%x]", callback);

	le_adv_item_handle handle = NULL;
	
    if(callback)
	{    
        for(int i=0;i<MAX_NUMBER_OF_ADVERTISING_ITEM_ENTRIES;i++)
		{
			if(database[i].callback == NULL)
			{
				database[i].task = task;
				database[i].callback = callback;
				handle = &database[i];
                LeAdvertisingManager_QueueClientDataUpdate(handle);
				break;
			}
        }
	}
	
	return handle;
}

uint32 LeAdvertisingManager_GetAdvertisingItemDatabaseMaxSize(void)
{
    return MAX_NUMBER_OF_ADVERTISING_ITEM_ENTRIES;
}

le_adv_item_handle LeAdvertisingManager_HeadItem(le_adv_item_iterator_t * iterator)
{
    le_adv_item_handle handle = NULL;
    
    if(iterator)
    {
        handle = &database[0];
        iterator->handle = handle;
    }
    
    return handle;
}

le_adv_item_handle LeAdvertisingManager_NextItem(le_adv_item_iterator_t * iterator)
{
    le_adv_item_handle handle = NULL;
    
    if(iterator)
    {
        iterator->handle++;

        if(iterator->handle < &database[MAX_NUMBER_OF_ADVERTISING_ITEM_ENTRIES])
        {
            handle = iterator->handle;

        }
    }
        
    return handle;
}

#endif /* LE_ADVERTISING_MANAGER_NEW_API */
