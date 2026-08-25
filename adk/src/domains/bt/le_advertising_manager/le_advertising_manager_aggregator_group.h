/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for LE advertising manager aggregator group.

An aggregator group is a collection of afvertising items that have compatible advertising parameters.
*/

#ifndef LE_ADVERTISING_MANAGER_AGGERGATOR_GROUP_H
#define LE_ADVERTISING_MANAGER_AGGERGATOR_GROUP_H

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager_aggregator_types.h"


extern le_adv_item_group_t groups[];
extern uint32 number_of_groups;


le_adv_item_group_t * leAdvertisingManager_GetGroupForSet(le_adv_set_t * set);

le_adv_item_group_t * leAdvertisingManager_GetGroupLinkedToItem(le_adv_item_handle handle);

le_adv_item_group_t * leAdvertisingManager_GetGroupForParams(le_adv_item_info_t * item_info, le_adv_item_params_t * item_params);

le_adv_item_group_t * leAdvertisingManager_CreateNewGroup(le_adv_item_info_t * info, le_adv_item_params_t * params);

void leAdvertisingManager_AddItemToGroup(le_adv_item_group_t * group, le_adv_item_handle handle);

void leAdvertisingManager_RemoveItemFromGroup(le_adv_item_group_t * group, le_adv_item_handle handle);

void leAdvertisingManager_AddSetToGroup(le_adv_item_group_t * group, le_adv_set_t * set_to_add);

void leAdvertisingManager_RemoveSetFromGroup(le_adv_item_group_t * group, le_adv_set_t * set_to_remove);

bool leAdvertisingManager_DoesGroupPassAdvertisingCriteria(le_adv_item_group_t * group);

bool leAdvertisingManager_IsGroupToBeRefreshed(le_adv_item_group_t * group);

void leAdvertisingManager_MarkGroupToBeRefreshed(le_adv_item_group_t * group, bool refresh);

#endif /* LE_ADVERTISING_MANAGER_NEW_API */

#endif // LE_ADVERTISING_MANAGER_AGGERGATOR_GROUP_H
