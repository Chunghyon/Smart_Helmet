/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Data types used by the LE advertising aggregator.
*/

#ifndef LE_ADVERTISING_MANAGER_AGGREGATOR_TYPES_H
#define LE_ADVERTISING_MANAGER_AGGREGATOR_TYPES_H

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager.h"
#include "le_advertising_manager_data_packet.h"
#include "le_advertising_manager_set_sm.h"


typedef struct __advertising_item
{
    le_adv_item_handle handle;
    uint32 size;
    struct __advertising_item *next;
} le_adv_item_list_t;

typedef struct
{
    bool needs_registering:1;
    bool needs_params_update:1;
    bool needs_data_update:1;
    bool needs_enabling:1;
    bool needs_disabling:1;
    bool needs_destroying:1;
    bool active:1;

    uint32 space;
    uint16 lock;

    le_advertising_manager_set_state_machine_t * sm;
    uint32 number_of_items;
    le_adv_item_list_t * item_list;

} le_adv_set_t;

typedef struct __advertising_set
{
    le_adv_set_t *set_handle;
    struct __advertising_set *next;
} le_adv_set_list_t;

typedef struct
{
    /*! Linked list of all advertising items in this group. */
    le_adv_item_list_t * item_handles;

    /*! Handle to item that should be included in all sets in this group. */
    le_adv_item_handle always_include_item_handle;

    /*! Total number of advertising items. */
    uint32 number_of_items;

    /*! Advertising parameters for this group. */
    le_adv_item_params_t params;

    /*! Advertising info for this group. */
    le_adv_item_info_t info;

    /*! Does this group need to be refreshed. */
    bool needs_refresh;

    /*! Total number of advertising sets in this group. */
    uint32 number_of_sets;

    /*! linked list of advertising sets in this group. */
    le_adv_set_list_t * set_list;

    /*! A single set for the scan response data items in this group.
        All advertising sets in the group use this when setting their
        scan response data. */
    le_adv_set_t * scan_resp_set;

} le_adv_item_group_t;

#if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_V_VERBOSE)
#define DEBUG_GROUPS_ENABLED
#endif

#define LEAM_DEBUG_LOG  DEBUG_LOG_VERBOSE

#ifdef DEBUG_GROUPS_ENABLED
void leAdvertisingManager_LoggingItemData(le_adv_item_handle item);
#else
#define leAdvertisingManager_LoggingItemData(x) UNUSED(x)
#endif

/*! \brief  */
bool leAdvertisingManager_IsAdvertisingTypeExtended(le_adv_data_type_t type);

/*! \brief  */
bool leAdvertisingManager_IsAdvertisingTypeConnectable(le_adv_data_type_t type);

#endif /* LE_ADVERTISING_MANAGER_NEW_API */

#endif // LE_ADVERTISING_MANAGER_AGGREGATOR_TYPES_H
