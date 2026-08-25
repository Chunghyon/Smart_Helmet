/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for LE advertising manager aggregator set.

An aggregator set is the advertising items that will be included in a single OTA advertising set.
*/

#ifndef LE_ADVERTISING_MANAGER_AGGREGATOR_SET_H
#define LE_ADVERTISING_MANAGER_AGGREGATOR_SET_H

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_aggregator_types.h"

uint32 leAdvertisingManager_GetItemSize(le_adv_item_handle item);

void leAdvertisingManager_SetAdvertisingSetBusyLock(le_adv_set_t *set);

void leAdvertisingManager_ReleaseAdvertisingSetBusyLock(le_adv_set_t *set);

le_adv_set_t * leAdvertisingManager_GetSetWithFreeSpace(le_adv_set_list_t * set_list, uint32 free_space_size);

void leAdvertisingManager_AddItemToSet(le_adv_set_t * set, le_adv_item_handle handle, uint32 size);

#endif /* LE_ADVERTISING_MANAGER_NEW_API */

#endif // LE_ADVERTISING_MANAGER_AGGREGATOR_SET_H
